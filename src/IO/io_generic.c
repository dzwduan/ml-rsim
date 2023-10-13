/*
 * Copyright (c) 2002 The Board of Trustees of the University of Illinois and
 *                    William Marsh Rice University
 * Copyright (c) 2002 The University of Utah
 * Copyright (c) 2002 The University of Notre Dame du Lac
 *
 * All rights reserved.
 *
 * Based on RSIM 1.0, developed by:
 *   Professor Sarita Adve's RSIM research group
 *   University of Illinois at Urbana-Champaign and
     William Marsh Rice University
 *   http://www.cs.uiuc.edu/rsim and http://www.ece.rice.edu/~rsim/dist.html
 * ML-RSIM/URSIM extensions by:
 *   The Impulse Research Group, University of Utah
 *   http://www.cs.utah.edu/impulse
 *   Lambert Schaelicke, University of Utah and University of Notre Dame du Lac
 *   http://www.cse.nd.edu/~lambert
 *   Mike Parker, University of Utah
 *   http://www.cs.utah.edu/~map
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal with the Software without restriction, including without
 * limitation the rights to use, copy, modify, merge, publish, distribute,
 * sublicense, and/or sell copies of the Software, and to permit persons to
 * whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimers. 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimers in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the names of Professor Sarita Adve's RSIM research group,
 *    the University of Illinois at Urbana-Champaign, William Marsh Rice
 *    University, nor the names of its contributors may be used to endorse
 *    or promote products derived from this Software without specific prior
 *    written permission. 
 * 4. Neither the names of the ML-RSIM project, the URSIM project, the
 *    Impulse research group, the University of Utah, the University of
 *    Notre Dame du Lac, nor the names of its contributors may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission. 
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE CONTRIBUTORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS WITH THE SOFTWARE. 
 */

/*****************************************************************************/
/* I/O device bus interface                                                  */
/*   Accepts uncached transactions, performs the memory operations and       */
/*   passes the request to callback functions provided by the device         */
/*   backend. Device backend is responsible for address range registration,  */
/*   memory allocation and reply initiation.                                 */
/* This does not yet handle coherent or device initiated transactions!       */
/*****************************************************************************/


#include <string.h>
#include <malloc.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/evlst.h"
#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "Processor/memunit.h"
#include "Processor/pagetable.h"
#include "Caches/system.h"
#include "Caches/cache.h"
#include "Caches/pipeline.h"
#include "IO/addr_map.h"
#include "IO/io_generic.h"
#include "Bus/bus.h"
#include "IO/byteswap.h"


IO_GENERIC *IOs = NULL;


int IO_LATENCY = 1;



/*===========================================================================*/
/* Create an I/O device on every node.                                       */
/* To create anything more useful than this generic device:                  */
/*   register address range in each node via AddrMap_insert()                */
/*   allocate host memory to back this address range via PageTable_insert()  */
/*   register read and write callback function (args: node-ID & request)     */
/*   read callback should return a reply via IO_start_send_to_bus()          */
/*   use read_int, read_char, read_bit etc. to read the value of memory-     */
/*   mapped device storage, or use write_int, write_char, write_bit etc.     */
/*   to update device storage (control registers, buffer ..)                 */
/*===========================================================================*/

void IOGeneric_init(int (*read_func)(REQ*),
		    int (*write_func)(REQ*),
		    int (*reply_func)(REQ*))
{
  int i;
  IO_GENERIC *pio;

  get_parameter("BUS_frequency",      &BUS_FREQUENCY,      PARAM_INT);
  get_parameter("BUS_total_requests", &BUS_TOTAL_REQUESTS, PARAM_INT);
  get_parameter("BUS_io_requests",    &BUS_IO_REQUESTS,    PARAM_INT);
  get_parameter("IO_latency",         &IO_LATENCY,         PARAM_INT);

  if (IOs == NULL)
    IOs = (IO_GENERIC*)malloc(sizeof(IO_GENERIC) * ARCH_numnodes);
  else
    IOs = (IO_GENERIC*)realloc(IOs, sizeof(IO_GENERIC) *
			       ARCH_numnodes * (ARCH_ios+1));
  if (!IOs)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);

  /*-------------------------------------------------------------------------*/

  for (i = 0; i < ARCH_numnodes; i++)
    {
      pio = PID2IO(i, ARCH_cpus + ARCH_ios);

      pio->nodeid = i;
      pio->mid    = ARCH_cpus + ARCH_ios;
      
      lqueue_init(&(pio->arbwaiters), IO_MAX_TRANS);
      pio->arbwaiters_count = 0;

      pio->read_func = read_func;
      pio->write_func = write_func;
      pio->reply_func = reply_func;

      lqueue_init(&(pio->cohqueue), BUS_TOTAL_REQUESTS);

      lqueue_init(&(pio->inqueue), IO_MAX_TRANS);    /*@@@ flow control ! @@@*/
      pio->inpipe = NewPipeline(1, IO_LATENCY * BUS_FREQUENCY, 1,
				IO_LATENCY * BUS_FREQUENCY);
      pio->inq_event  = NewEvent("I/O Device request input",
				 IO_handle_inqueue, NODELETE, 0);
      EventSetArg(pio->inq_event, (char*)IO_INDEX(i, ARCH_cpus + ARCH_ios),
						  sizeof(int));
      
      lqueue_init(&(pio->outqueue), IO_MAX_TRANS);

      pio->outpipe = NewPipeline(1, IO_LATENCY * BUS_FREQUENCY, 1,
				 IO_LATENCY * BUS_FREQUENCY);
      pio->outq_event = NewEvent("I/O Device request output",
				 IO_handle_outqueue, NODELETE, 0);
      EventSetArg(pio->outq_event, (char*)IO_INDEX(i, ARCH_cpus + ARCH_ios),
		  sizeof(int));

      pio->writebacks = NULL;

      IO_scoreboard_init(pio);
    }
}




/*===========================================================================*/
/* broadcast coherent request to all I/O devices                             */
/*===========================================================================*/

void IO_get_cohe_request(REQ *req)
{
  int i;

  for (i = 0; i < ARCH_coh_ios; i++)
    IO_get_request(PID2IO(req->node, i + ARCH_cpus), req);
}



/*===========================================================================*/
/* IO is receiving a request from the bus. Called by bus module.             */
/* Put request in pipeline and wake up other end, unless the pipeline or     */
/* queue were not empty, in which case the handler is already awake.         */
/*===========================================================================*/

void IO_get_request(IO_GENERIC *pio, REQ *req)
{
  if ((req->type == REQUEST) &&
      ((req->req_type == READ_UC) ||
       (req->req_type == WRITE_UC) ||
       (req->req_type == SWAP_UC)))
    {
      if (!AddToPipe(pio->inpipe, req))
	YS__errmsg(req->node, "I/O device %i input pipe full", pio->mid);
    }
  else
    {
      if (lqueue_full(&(pio->cohqueue)))
	YS__errmsg(req->node, "I/O device %i coherency queue full", pio->mid);

      lqueue_add(&(pio->cohqueue), req, req->node);
    }

  if (IsNotScheduled(pio->inq_event))
    schedule_event(pio->inq_event, YS__Simtime + BUS_FREQUENCY);
}



/*===========================================================================*/
/* Woken up when the input pipeline or queue is not empty. Take request off  */
/* the queue and handle it by calling 'GlobalPerform'. Writes requests are   */
/* returned to the pool, if the callback function succedded. For reads it    */
/* generates a generic response, if no callback function is specified.       */
/* Upon success, the request is removed from the queue.                      */
/* Check if pipeline contains an element that needs to go into the queue,    */
/* and reschedule if either pipeline or queue are not empty.                 */
/* Note that this routine can be called when there are no requests in the    */
/* queue yet, in this case it may move a request from the pipeline into the  */
/* queue, or just reschedule itself if the pipeline is deep.                 */
/*===========================================================================*/
  
void IO_handle_inqueue(void)
{
  REQ *req, *creq, *nreq;
  IO_GENERIC *pio = &(IOs[(int)EventGetArg(NULL)]);
  int rc = 0, index;


  /*-------------------------------------------------------------------------*/
  req = lqueue_head(&pio->cohqueue);

  if (req != NULL)
    {
      /* insert our own requests into the scoreboard ------------------------*/
      if ((req->src_proc == pio->mid) &&
	  (req->type == REQUEST) &&
	  (req->req_type != READ_UC) &&
	  (req->req_type != WRITE_UC) &&
	  (req->req_type != SWAP_UC))
	IO_scoreboard_insert(pio, req);
	
      creq = IO_scoreboard_lookup(pio, req);
      rc = 0;

      
      /*---------------------------------------------------------------------*/
      /* snoop all coherent requests:
	 our own requests                 -> EXCL (don't care)
	 snoop miss                       -> EXCL
	 hit READ_CURRENT                 -> EXCL
	 READ_SH hits a pending READ_SH   -> SH
	 READ_SH hits pending WRITEBACK   -> EXCL & upgrade WB to C2C_copy
	 READ_OWN hits pending WRITEBACK  -> EXCL & upgrade WB to C2C_copy
	 
	 stall all snoop hits that are ambiguous:
	 read_shared hits read_own
	 upgrade hits read_own/shared
	 read_own hits read_own/shared
	 any hits to replies in flight, hits to upgraded WRITEBACKS
      */

      /* our own request, or no hit: exclusive ------------------------------*/
      if ((req->src_proc == pio->mid) || (creq == NULL))
	{
          Bus_send_cohe_response(req, REPLY_EXCL);
	  rc = 1;
	}

      /* hit and not our own request: depends on request type ---------------*/
      else
	{
	  /* read_current does not transfer ownership */
	  if ((creq->type == REQUEST) && (creq->req_type == READ_CURRENT))
	    {
	      Bus_send_cohe_response(req, REPLY_EXCL);
	      rc = 1;
	    }
	  
	  /* snoop READ_SHARED ----------------------------------------------*/
	  if ((req->type == REQUEST) && (req->req_type == READ_SH))
	    {
	      /* read_shared hits read_shared: remains shared */
	      if ((creq->type == REQUEST) && (creq->req_type == READ_SH))
		{
		  Bus_send_cohe_response(req, REPLY_SH);
		  rc = 1;
		}

	      /* read_shared hits pending writeback: stall response */
	      if (creq->type == WRITEBACK)
		{
		  rc = 0;
		  /* old code - didn't work
		  Bus_send_cohe_response(req, REPLY_EXCL);
		  req->c2c_copy = 1;
		  creq->type = COHE_REPLY;
		  creq->req_type = REPLY_EXCL;
		  creq->dest_proc = req->src_proc;
		  creq->parent = req;
		  rc = 1;
		  */
		}
	    }

	  
	  /* snoop READ_OWN -------------------------------------------------*/
	  if ((req->type == REQUEST) && (req->req_type == READ_OWN))
	    {
	      /* read_own hits pending writeback: stall response */
	      if (creq->type == WRITEBACK)
		{
		  rc = 0;
		  /* old code - didn't work !
		  Bus_send_cohe_response(req, REPLY_EXCL);
		  req->c2c_copy = 1;
		  creq->type = COHE_REPLY;
		  creq->req_type = REPLY_EXCL;
		  creq->dest_proc = req->src_proc;
		  creq->parent = req;
		  rc = 1;
		  */
		}
	    }

	  /* snoop WRITEPURGE: always OK, but invalidate data */
	  if (req->type == WRITEPURGE)
	    {
	      Bus_send_cohe_response(req, REPLY_EXCL);
	      /* read_shared hits pending writeback: remove pending request */
	      if (creq->type == WRITEBACK)
		{
		  /* remove from outgoing queue */
		}
	    }
	}

      if (rc)
	lqueue_remove(&(pio->cohqueue));
    }

  
  /*-------------------------------------------------------------------------*/
  /* handle requests (uncached only) from inqueue, and move requests from    */
  /* delay pipeline to queue                                                 */
  
  req = lqueue_head(&pio->inqueue);

  
  /*-------------------------------------------------------------------------*/

  if ((req != NULL) && (req->type == REQUEST) && (req->prcr_req_type == WRITE))
    {
      creq = req;
      while (creq)
	{
	  req->perform(creq);
	  creq = creq->parent;
	}

      rc = 1;
      if (pio->write_func)
	rc = pio->write_func(req);

      if (rc)
	{
	  /* return instances to CPU and free request objects ---------------*/
	  creq = req;
	  while (creq)
	    {
	      nreq = creq->parent;
	      if ((req->req_type != WRITE_UC) ||
		  (!tlb_uc_csb(req->memattributes)))
		req->complete(creq, req->hit_type);

	      YS__PoolReturnObj(&YS__ReqPool, creq);
	      creq = nreq;
	    }
	}
    }

  
  /*-------------------------------------------------------------------------*/

  if ((req != NULL) && (req->type == REQUEST) && (req->prcr_req_type == READ))
    {
      creq = req;
      while (creq)
	{
	  req->perform(creq);
	  creq = creq->parent;
	}

      rc = 1;
      if (pio->read_func)
	rc = pio->read_func(req);

      /* perform read, but only if no read callback is registered */
      if (!pio->read_func)
	{
	  req->type = REPLY;
	  req->req_type = REPLY_UC;

	  IO_start_transaction(pio, req);
	}
    }



  /*-------------------------------------------------------------------------*/

  if ((req != NULL) && (req->type == REQUEST) && (req->prcr_req_type == RMW))
    {
      creq = req->parent;
      while (creq)
	{
	  creq->perform(creq);
	  creq = creq->parent;
	}

      rc = 1;
      if (pio->write_func)
	rc = pio->write_func(req);

      /* perform write, but only if no read callback is registered */
      if (!pio->write_func)
	{
	  req->type = REPLY;
	  req->req_type = REPLY_UC;

	  IO_start_transaction(pio, req);
	}
    }

  if ((req != NULL) && rc)
    lqueue_remove(&(pio->inqueue));


  /*-------------------------------------------------------------------------*/

  GetPipeElt(req, pio->inpipe);
  if (req)
    {
      ClearPipeElt(pio->inpipe);
      lqueue_add(&(pio->inqueue), req, pio->nodeid);
    }

  if ((!lqueue_empty(&(pio->inqueue))) ||
      (!lqueue_empty(&(pio->cohqueue))) ||
      (!PipeEmpty(pio->inpipe)))
    schedule_event(pio->inq_event, YS__Simtime + BUS_FREQUENCY);
}




/*===========================================================================*/
/* Called by I/O module to issue a bus transaction. Request is inserted      */
/* into the delay pipeline. Schedules the output routine if either pipeline  */
/* or output queue are not empty.                                            */
/*===========================================================================*/

int IO_start_transaction(IO_GENERIC *pio, REQ *req)
{
  if (!AddToPipe(pio->outpipe, req))
    return(0);

  if (IsNotScheduled(pio->outq_event))
    schedule_event(pio->outq_event, YS__Simtime + (double)BUS_FREQUENCY);

  return(1);
}



/*===========================================================================*/
/* Called when output pipeline or queues are not empty. Try to send request  */
/* from output queue to the bus interface, remove it from the queue only     */
/* if request has been processed (is issued to bus, or has arbitrated, or    */
/* is in the arb-waiters queue). Then, move entry from delay pipeline into   */
/* host-side output queue, move entry from device-side output queue to delay */
/* pipeline, and reschedule itself if either pipeline of queues are not      */
/* empty.                                                                    */
/*===========================================================================*/

void IO_handle_outqueue(void)
{
  REQ        *req;
  IO_GENERIC *pio = &(IOs[(int)EventGetArg(NULL)]);
  int         index;


  /*-------------------------------------------------------------------------*/

  req = pio->writebacks;
  if (req != NULL)
    {
      if (IO_start_send_to_bus(pio, req))
	{
	  pio->writebacks = req->dnext;
	  req->dnext = NULL;
	}
    }

  
  /*-------------------------------------------------------------------------*/
  /* take entry off host-side output queue and send to bus interface         */

  req = (REQ *)lqueue_head(&pio->outqueue);
  if (req)
    {
      if (IO_start_send_to_bus(pio, req))
	lqueue_remove(&(pio->outqueue));
    }

  
  /*-------------------------------------------------------------------------*/
  /* take entry off pipeline and enter into host-side output queue           */
  
  GetPipeElt(req, pio->outpipe);
  if ((req != NULL) && (!lqueue_full(&(pio->outqueue))))
    {
      ClearPipeElt(pio->outpipe);

      lqueue_add(&(pio->outqueue), req, pio->nodeid);
    }


  /*-------------------------------------------------------------------------*/
  
  if (((!lqueue_empty(&(pio->outqueue))) ||
       (!PipeEmpty(pio->outpipe))) &&
      IsNotScheduled(pio->outq_event))
    schedule_event(pio->outq_event, YS__Simtime + BUS_FREQUENCY);  
}




/*===========================================================================*/
/* Attempt to issue transaction on the bus. Initialize various request       */
/* fields and try to get a request number if necessary. Enqueue the request  */
/* in the arb-waiters queue if other requests are already waiting, or start  */
/* the transaction immediately if we own the bus and it is idle. Otherwise,  */
/* arbitrate for the bus.                                                    */
/*===========================================================================*/

int IO_start_send_to_bus(IO_GENERIC *pio, REQ *req)
{
  req->memattributes = 0;

  switch (req->type)
    {
    case REQUEST:
      if ((req->req_type == READ_UC) ||
	  (req->req_type == WRITE_UC) ||
	  (req->req_type == SWAP_UC))
	{
	  req->bus_start_time = YS__Simtime;
	  if ((req->req_type == WRITE_UC) || (req->req_type == SWAP_UC))
	    req->bus_cycles = 1 + (req->size + BUS_WIDTH - 1) / BUS_WIDTH;
	  else
	    req->bus_cycles = 1;

	  if ((req->req_type == READ_UC) || (req->req_type == SWAP_UC))
	    req->data_done = 0;
	}
      else
	{
          req->data_done  = 0;
          req->cohe_done  = 0;
          req->bus_cycles = 1;
          req->bus_start_time = YS__Simtime;
        }
      break;


    case COHE_REPLY:
      /*
       * Cohe check hit a private dirty line. Send the dirty data to the 
       * requestor and main memory through cache-to-cache transaction. 
       */
      req->bus_cycles   = ARCH_linesz2 / BUS_WIDTH;
      break;


    case WRITEBACK:
      req->bus_cycles = 1 + ARCH_linesz2 / BUS_WIDTH;
      req->parent     = 0;
      break;

      
    case WRITEPURGE:
      req->bus_cycles = 1 + ARCH_linesz2 / BUS_WIDTH;
      req->parent     = 0;
      req->data_done  = 1;
      req->cohe_done  = 0;      
      break;

      
    case REPLY:
      if (req->req_type == REPLY_UC)
	req->bus_cycles = (req->size + BUS_WIDTH - 1) / BUS_WIDTH;
      else
	req->bus_cycles = ARCH_linesz2 / BUS_WIDTH;
      break;
    }
  
  return(IO_arb_for_bus(req));
}


/*===========================================================================*/


int IO_arb_for_bus(REQ *req)
{
  IO_GENERIC *pio;
  BUS        *pbus = PID2BUS(req->node);
  REQ        *oreq;


  if (req->type != REPLY)
    pio  = PID2IO(req->node, req->src_proc);
  else
    pio  = PID2IO(req->node, req->dest_proc);


  if (pio->arbwaiters_count > 0)
    {
      oreq = pbus->arbwaiters[req->src_proc].req;

      if (lqueue_full(&(pio->arbwaiters)))
	return(0);
      
      /* C2C-copies bypass stalled arbitration requests to avoid deadlocks */
      if ((req->type == COHE_REPLY) && (oreq != NULL))
        {
          lqueue_add_head(&(pio->arbwaiters), oreq, pio->nodeid);
          pbus->arbwaiters[req->src_proc].req = req;
        }
      else
        {
          lqueue_add(&(pio->arbwaiters), req, pio->nodeid);
        }

      pio->arbwaiters_count++;
      return(1);
    }

  
  if (InMasterState(pio->nodeid, pio->mid) &&
      BusIsIdleUntil(req->bus_cycles * BUS_FREQUENCY))
    {
      /*
      if ((req->type == REQUEST) &&
	  (req->req_type != WRITE_UC))
	  PID2BUS(req->node)->req_count++;
      
      if ((req->type == REPLY) ||
	  (req->type == COHE_REPLY))
	PID2BUS(req->node)->req_count--;
      */
      req->arb_start_time = YS__Simtime;
      
      
      IO_in_master_state(req);
      return(1);
    }
  

  pio->arbwaiters_count++;
  Bus_recv_arb_req(pio->nodeid, pio->mid, req, YS__Simtime);

  return(1);
}




/*===========================================================================*/
/* Start driving the bus to send out the pending request.                    */
/* Called by bus module. If more requests are waiting to arbitrate, get a    */
/* request from the arb-waiters queue and let it arbitrate.                  */
/*===========================================================================*/

void IO_in_master_state(REQ *req)
{
  IO_GENERIC *pio;

  if (req->type != REPLY)
    pio  = PID2IO(req->node, req->src_proc);
  else
    pio  = PID2IO(req->node, req->dest_proc);


  Bus_start(req, IO_MODULE);


  /*-------------------------------------------------------------------------*/

  if ((req->type == WRITEBACK) || (req->type == COHE_REPLY))
    IO_scoreboard_remove(pio, req);


  /* More arbitration waiters? ----------------------------------------------*/
  if (pio->arbwaiters_count > 0)
    {
      if (--pio->arbwaiters_count > 0)
        {
          REQ *newreq;
          lqueue_get(&(pio->arbwaiters), newreq);
          Bus_recv_arb_req(pio->nodeid, pio->mid, newreq, YS__Simtime);
        }
    }
}



/*===========================================================================*/
/* Send reply back to requestor, called by bus module at end of transaction. */
/*===========================================================================*/

void IO_send_on_bus(REQ *req)
{
  switch (req->type)
    {
    case REQUEST:
      if ((req->req_type == READ_UC) ||
	  (req->req_type == WRITE_UC) ||
	  (req->req_type == SWAP_UC))
	Bus_send_IO_request(req);
      else
	Bus_send_request(req);
      break;
      

    case REPLY:
      if (req->req_type == REPLY_UC)
	Bus_send_IO_reply(req);
      else
	Bus_send_reply(req);
      break;
      

    case COHE_REPLY:
      Bus_send_cohe_data_response(req);
      break;
      

    case WRITEBACK:
      if (req->parent != NULL)
	{
	  /* 
	   * It's been changed to data response because hit by a cohe request. 
	   */
	  req->type = COHE_REPLY;
	  req->req_type = REPLY_EXCL;
	  Bus_send_cohe_data_response(req);
	}
      else
	Bus_send_writeback(req);
      break;

      
    case WRITEPURGE:
      Bus_send_writepurge(req);
      break;

      
    default:
      YS__errmsg(req->node,
		 "Default case in IO_send_on_bus() line %i for req 0x%08X/0x%08X %i %i",
		 __LINE__, req->vaddr, req->paddr, req->type, req->req_type);
    }
}




/*===========================================================================*/
/*===========================================================================*/

void IO_get_reply(REQ *req)
{
  IO_GENERIC *pio = PID2IO(req->node, req->src_proc);
  MSHR *pmshr;
  REQ  *creq, *preq;
  int   i, rc;

  for (i = 0; i < ARCH_cpus; i++)
    {
      pmshr = req->stalled_mshrs[i];
      if (pmshr && pmshr->pending_cohe == req)
        {
          pmshr->pending_cohe = 0;
          req->stalled_mshrs[i] = 0;
        }
    }

  req->bus_return_time = YS__Simtime;

  /*-------------------------------------------------------------------------*/

  creq = req;
  while (creq)
    {
      req->perform(creq);
      creq = creq->parent;
    }
  
  rc = 1;
  if (pio->reply_func != NULL)
    rc = pio->reply_func(req);

  if (rc)
    {
      if (req->prcr_req_type == READ)
	{
	  IO_scoreboard_remove(pio, req);
	  creq = req;
	  while (creq)
	    {
	      preq = creq->parent;
	      if (!req->cohe_count)
		YS__PoolReturnObj(&YS__ReqPool, creq);

	      creq = preq;
	    }
	}

      if (req->prcr_req_type == WRITE)
	{
	  req->type      = WRITEBACK;
	  req->src_proc  = pio->mid;
	  req->dest_proc = ARCH_cpus + ARCH_ios;
	  creq           = req->parent;
	  while (creq)
	    {
	      preq = creq->parent;
	      if (!req->cohe_count)
		YS__PoolReturnObj(&YS__ReqPool, creq);

	      creq = preq;
	    }

	  req->parent = NULL;
	  req->dnext  = NULL;
	  
	  if (pio->writebacks == NULL)
	    pio->writebacks = req;
	  else
	    {
	      creq = pio->writebacks;
	      while (creq->dnext != NULL)
		creq = creq->dnext;
	      creq->dnext = req;
	    }

	  if (IsNotScheduled(pio->outq_event))
	    schedule_event(pio->outq_event, YS__Simtime + BUS_FREQUENCY);
	}
    }
}



/*===========================================================================*/
/* Receive a cache-to-cache copy from some other master. The parent-pointer  */
/* points to our original request. Data always arrives before the response.  */
/*===========================================================================*/

void IO_get_data_response(REQ* req)
{
  req->parent->data_done = 1;

  YS__PoolReturnObj(&YS__ReqPool, req);
}



/*===========================================================================*/
/* Initialize scoreboard - clear counters and allocate some storage          */
/*===========================================================================*/

void IO_scoreboard_init(IO_GENERIC *pio)
{
  pio->scoreboard.reqs = malloc(sizeof(REQ*) * SCOREBOARD_INCREMENT);
  pio->scoreboard.req_count = 0;
  pio->scoreboard.req_max = SCOREBOARD_INCREMENT;
}



/*===========================================================================*/
/* Insert request into the scoreboard                                        */
/*===========================================================================*/

void IO_scoreboard_insert(IO_GENERIC *pio, REQ *req)
{
  if (pio->scoreboard.req_count == pio->scoreboard.req_max)
    {
      pio->scoreboard.req_max += SCOREBOARD_INCREMENT;
      pio->scoreboard.reqs = realloc(pio->scoreboard.reqs,
				     sizeof(REQ*) * pio->scoreboard.req_max);
    }

  pio->scoreboard.reqs[pio->scoreboard.req_count++] = req;
}


/*===========================================================================*/
/* Lookup request by address - simple sequential search                      */
/*===========================================================================*/

REQ* IO_scoreboard_lookup(IO_GENERIC *pio, REQ *req)
{
  int n;

  for (n = 0; n < pio->scoreboard.req_count; n++)
    if (pio->scoreboard.reqs[n]->paddr == req->paddr)
      return(pio->scoreboard.reqs[n]);

  return(NULL);
}


/*===========================================================================*/
/* Remove entry from scoreboard, match by request structure address, move    */
/* last entry to now empty entry.                                            */
/*===========================================================================*/

void IO_scoreboard_remove(IO_GENERIC *pio, REQ *req)
{
  int n;

  for (n = 0; n < pio->scoreboard.req_count; n++)
    if (pio->scoreboard.reqs[n] == req)
      {
	pio->scoreboard.reqs[n] =
	  pio->scoreboard.reqs[pio->scoreboard.req_count-1];
	pio->scoreboard.req_count--;
	return;
      }
}





/*===========================================================================*/
/*===========================================================================*/

void IO_read_byte(REQ *req)
{
  char *pa;

  pa = PageTable_lookup(req->node, req->paddr);

  if (pa)
    *(char*)(req->d.mem.buf) = *pa;
}



/*===========================================================================*/
/*===========================================================================*/

void IO_read_word(REQ *req)
{
  unsigned *pa;

  pa = (unsigned*)PageTable_lookup(req->node, req->paddr);

  if (pa)
    {
      *(unsigned*)(req->d.mem.buf) = swap_word(*pa);
    }
}



/*===========================================================================*/
/*===========================================================================*/

void IO_read_block(REQ *req)
{
  char *pa;

  pa = PageTable_lookup(req->node, req->paddr);

  if (pa)
    memcpy(req->d.mem.buf, pa, req->size);
}



/*===========================================================================*/
/*===========================================================================*/

void IO_write_byte(REQ *req)
{
  char *pa;

  pa = PageTable_lookup(req->node, req->paddr);

  if (pa)
    *pa = *(char*)(req->d.mem.buf);
}



/*===========================================================================*/
/*===========================================================================*/

void IO_write_word(REQ *req)
{
  unsigned *pa;

  pa = (unsigned*)PageTable_lookup(req->node, req->paddr);
  if (pa)
    *pa = swap_word(*(unsigned*)(req->d.mem.buf));
}



/*===========================================================================*/
/*===========================================================================*/

void IO_write_block(REQ *req)
{
  char *pa;

  pa = PageTable_lookup(req->node, req->paddr);

  if (pa)
    memcpy(pa, req->d.mem.buf, req->size);
}


void IO_empty_func(REQ *req, HIT_TYPE h)
{

}


/*===========================================================================*/
/*===========================================================================*/

void IO_dump(IO_GENERIC *pio)
{
  int n, index;
  REQ *req;
  
  YS__logmsg(pio->nodeid,
	     "\n============ GENERIC I/O INTERFACE %i ============\n",
	     pio->mid);

  YS__logmsg(pio->nodeid, "arbwaiters_count = %d\n", pio->arbwaiters_count);  
  DumpLinkQueue("arbwaiters", &(pio->arbwaiters), 0x71, Cache_req_dump,
		pio->nodeid);

  DumpLinkQueue("cohqueue", &(pio->cohqueue), 0x71, Cache_req_dump,
		pio->nodeid);
  DumpPipeline("inpipe", pio->inpipe, 0x71, Cache_req_dump, pio->nodeid);
  DumpLinkQueue("inqueue", &(pio->inqueue), 0x71, Cache_req_dump, pio->nodeid);
  DumpPipeline("outpipe", pio->inpipe, 0x71, Cache_req_dump, pio->nodeid);
  DumpLinkQueue("outqueue", &(pio->outqueue), 0x71, Cache_req_dump,
		pio->nodeid);
  YS__logmsg(pio->nodeid, "inq_event scheduled: %s\n",
	     IsScheduled(pio->inq_event) ? "yes" : "no");
  YS__logmsg(pio->nodeid, "outq_event scheduled: %s\n",
	     IsScheduled(pio->outq_event) ? "yes" : "no");

  if (pio->writebacks)
    Cache_req_dump(pio->writebacks, 0x71, pio->nodeid);

  YS__logmsg(pio->nodeid, "scoreboard: req_count(%i), req_max(%d)\n",
	     pio->scoreboard.req_count, pio->scoreboard.req_max);
  for (n = 0; n < pio->scoreboard.req_count; n++)
    Cache_req_dump(pio->scoreboard.reqs[n], 0x71, pio->nodeid);
}
