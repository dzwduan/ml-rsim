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

#include <string.h>
#include "Processor/simio.h"
#include "Processor/memunit.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"
#include "Bus/bus.h"



/*=============================================================================
 * The cache is ready to send a request out through the cluster bus. The 
 * system interface will try to grab all the resources needed to start
 * this request in bus, such as a free request number, master state, etc. 
 * If not all the necessary resources are available, the request will enter 
 * a designated waiting buffer in system interface according to its type. 
 * If even that the designated buffer is full, we have to stop cache from 
 * processing certain kinds of transactions until there is free room in 
 * system interface. For details about the control on each type of request, 
 * please refer to "MIPS R10000 Microprocessor User's Manual, Version 2.0".
 */

int Cache_start_send_to_bus(CACHE *captr, REQ *req)
{
  switch (req->type)
    {
    case REQUEST:
      req->issued = 0;
      req->bus_start_time = YS__Simtime;
      
      if ((req->req_type == READ_UC) ||
	  (req->req_type == WRITE_UC) ||
	  (req->req_type == SWAP_UC))
	{
	  /*
	   * This is an uncached I/O request. Make sure the uncached buffer has
	   * at least a free slot before calling this function. Uncached
	   * requests directly come from processor. We don't want any of 
	   * them to return back to processor due to a full uncached buffer,
	   * because it's hard to do in current model. 
	   */
	  if ((req->req_type == WRITE_UC) || (req->req_type == SWAP_UC))
	    req->bus_cycles = 1 + (req->size + BUS_WIDTH - 1) / BUS_WIDTH;
	  else
	    req->bus_cycles = 1;

	  if ((req->req_type == READ_UC) || (req->req_type == SWAP_UC))
            req->data_done = 0;
	}
      else
	{
	  /*
	   * An ordinary request will stay in the request buffer and waits for
	   * data reply either from main memory or from another processor (if
	   * data is provided in cache-to-cache transfer). Because the replies
	   * will not return in order, we have to make the request buffer a
	   * double linked queue instead of a single linked queue. 
	   */
	  if (captr->pending_request)
            return 0;

	  req->data_done = 0;
	  req->cohe_done = 0;
	  req->bus_cycles = 1;

	  /* 
	   * If the cache request buffer is full, stop processing request.
	   */
	  if (lqueue_full(&(captr->reqbuffer)))
	    {
	      captr->pending_request = req;
	      captr->stall_request++;
	      return 1;
	    }
	  else
	    dqueue_add(&(captr->reqbuffer), req, dprev, dnext,
		       REQ *, captr->nodeid);
	}
      break;


    case COHE_REPLY:
      /*
       * Cohe check hit a private dirty line. Send the dirty data to the 
       * requestor and main memory through cache-to-cache transaction. 
       * Note: there is only one entry in write-back buffer for data response.
       */
      if (captr->cohe_reply)
	YS__errmsg(captr->nodeid, "Two data responses are outstanding.");

      captr->cohe_reply = req;
      req->bus_cycles   = ARCH_linesz2 / BUS_WIDTH;
      break;

      
    case WRITEBACK:
      /*
       * Victim of a conflict or a cache flush operation.
       */
      req->bus_cycles = 1 + ARCH_linesz2 / BUS_WIDTH;
      /*      req->parent     = 0;  */
      /*
       * If there is no free slot in outgoing buffer. We have to stall
       * everything. Note: check the pending_writeback to make sure
       * it's not non-zero before entering this function.
       */
      if (lqueue_full(&(captr->outbuffer)))
	{
	  captr->stall_reply++;
	  captr->stall_cohe++;
	  captr->pending_writeback = req;
	  return(1);
	}
      else
	{
	  lqueue_add(&(captr->outbuffer), req, captr->nodeid);
	}
      break;


    case REPLY:
      /*
       * in response to a read-uncached
       */
      req->bus_cycles = (req->size + BUS_WIDTH - 1) / BUS_WIDTH;

      if (lqueue_full(&(captr->outbuffer)))
	{
	  captr->stall_reply++;
	  return(1);
	}
      else
	{
	  lqueue_add(&(captr->outbuffer), req, captr->nodeid);
	}

      break;


    default:
      YS__errmsg(captr->nodeid,
		 "Unknown req_type (%d, addr=%X) in Cache_start_send_to_bus",
		 req->type, req->paddr);
    }

  Cache_arb_for_bus(req);
  return 1;
}





/*=============================================================================
 * Called after having successfully grabbed a request number. Now, it's time
 * to arbitrate for the bus.
 */

void Cache_arb_for_bus(REQ *req)
{
  CACHE *captr = PID2L2C(req->node, req->src_proc);
  BUS   *pbus  = PID2BUS(req->node);
  REQ   *oreq;

  
  /* 
   * Put it into arbitration waiters queue, if somebody is already there.
   */
  if (captr->arbwaiters_count > 0)
    {
      oreq = pbus->arbwaiters[req->src_proc].req;
      
      /* C2C-copies bypass stalled arbitration requests to avoid deadlocks */
      if ((req->type == COHE_REPLY) && (oreq != NULL))
	{
          lqueue_add_head(&(captr->arbwaiters), oreq, captr->nodeid);
	  pbus->arbwaiters[req->src_proc].req = req;
	}
      else
	{
	  lqueue_add(&(captr->arbwaiters), req, captr->nodeid);
	}

      captr->arbwaiters_count++;
      return;
    }

  /*
   * Send an arbitration request to the arbitrator (currently implemented
   * in bus module).
   */

  captr->arbwaiters_count = 1;
  Bus_recv_arb_req(req->node, req->src_proc, req, YS__Simtime);
  return;
}





/*=============================================================================
 * Gained the control of the bus at this cycle. Start sending the pending 
 * request to the destination over the bus. It schedules the bus issuer to
 * call cache module back when the request is coming off the bus.
 */

void Cache_in_master_state(REQ *req)
{
  CACHE *captr;

  
  if (req->type == REPLY)
    captr = PID2L2C(req->node, req->dest_proc);
  else
    captr = PID2L2C(req->node, req->src_proc);  
  
  /*
   * Schedule the bus issuer to call Cache_send_on_bus, which will
   * perform the actual request transfer, at the time when the transaction 
   * comes off the bus.
   */ 

  Bus_start(req, PROCESSOR);
  

  /*-------------------------------------------------------------------------*/

  if (req->req_type == WRITE_UC)
    UBuffer_confirm_transaction(req);


  /* More arbitration waiters? ----------------------------------------------*/
  if (captr->arbwaiters_count > 0)
    {
      if (--captr->arbwaiters_count > 0)
	{
	  REQ *nreq;
	  lqueue_get(&(captr->arbwaiters), nreq);
	  Bus_recv_arb_req(nreq->node, nreq->src_proc, nreq, YS__Simtime);
	}
    }
}




/*=============================================================================
 * Called at the last cycle of a transaction transferring. Move the request
 * into destination(s), and release some holding resources if possible. 
 */

void Cache_send_on_bus(REQ *req)
{
  CACHE *captr;

  if (req->type == REPLY)
    captr = PID2L2C(req->node, req->dest_proc);
  else
    captr = PID2L2C(req->node, req->src_proc);

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


    case COHE_REPLY:
      Bus_send_cohe_data_response(req);
      captr->cohe_reply = 0;
      break;

      
    case WRITEBACK:
      lqueue_remove(&(captr->outbuffer));
      if (req->parent)
	{
	  /* 
	   * It's been changed to data response because hit by a cohe request. 
	   */
	  req->type = COHE_REPLY;
	  Bus_send_cohe_data_response(req);
	}
      else
	Bus_send_writeback(req);

      /* 
       * If there is a pending writeback. Push it into the outgoing buffer and
       * unstall the reply queue and cohe queue processing.
       */
      if (captr->pending_writeback)
	{
	  REQ *nreq = captr->pending_writeback;
	  captr->stall_reply--;
	  captr->stall_cohe--;
	  captr->pending_writeback = 0;
	  Cache_start_send_to_bus(PID2L2C(req->node, nreq->src_proc),
				  nreq); 
	}
      break;


    case REPLY:
      {
	lqueue_remove(&(captr->outbuffer));

	if (req->req_type == REPLY_UC)
	  Bus_send_IO_reply(req);
	else
	  Bus_send_reply(req);
	break;
      }

      
    default:
      YS__errmsg(captr->nodeid,
		 "Unknown request type (%d, addr=%X) in Cache_send_on_bus",
		 req->type, req->paddr);
    }
}




/*=============================================================================
 * Getting a coherence request. Just put it into the coherence queue of 
 * every L2 cache in the node, including the one that started this request.
 */

void Cache_get_cohe_request(REQ *req)
{
  CACHE  *captr;
  int     i;

  for (i = 0; i < ARCH_cpus; i++)
    {
      captr = PID2L2C(req->node, i);
      lqueue_add(&(captr->cohe_queue), req, captr->nodeid);
      captr->inq_empty = 0;
    }
}



void Cache_get_noncoh_request(REQ *req)
{
  CACHE  *captr;

  captr = PID2L2C(req->node, req->dest_proc);
  lqueue_add(&(captr->noncohe_queue), req, captr->nodeid);
  captr->inq_empty = 0;    
}




/*=============================================================================
 * Getting a cache-to-cache copy. A request can't retire from cached request
 * buffer until it receives CoheCompletion, which should be sent out by the 
 * MMC AFTER this.
 */

void Cache_get_data_response(REQ *req)
{
  req->parent->data_done = 1;
  YS__PoolReturnObj(&YS__ReqPool, req);
}




/*=============================================================================
 * Received data from main memory. This function also serves as a 
 * CoheCompletion notice.
 */

void Cache_get_reply(REQ *req)
{
  CACHE  *captr = PID2L2C(req->node, req->src_proc);
  REQ    *tmpreq;
  MSHR   *pmshr;
  int     i;

  /*
   * Then, release all the MSHRs which are stalling after serving
   * coherence checks for this request.
   */
  for (i = 0; i < ARCH_cpus; i++)
    {
      pmshr = req->stalled_mshrs[i];
      if (pmshr && pmshr->pending_cohe == req)
	{
	  pmshr->pending_cohe = NULL;
	  req->stalled_mshrs[i] = NULL;
	}
    }

  /*
   * Remove the original request from the request buffer, put it into
   * the reply queue of L2 cache.
   */
  req->bus_return_time = YS__Simtime;
  req->type = REPLY;
  dqueue_remove(&(captr->reqbuffer), req, dprev, dnext, REQ *);

  if (lqueue_full(&(captr->reply_queue)))
    YS__errmsg(captr->nodeid,
	       "Cache_handle_reply: no free slot in reply_queue!");

  
  lqueue_add(&(captr->reply_queue), req, captr->nodeid);
  captr->inq_empty = 0;

  /*
   * Put the waiting request into the request buffer, if exists. 
   */
  if (captr->pending_request)
    {
      REQ *nreq = captr->pending_request;
      captr->pending_request = 0;
      captr->stall_request--;

      dqueue_add(&(captr->reqbuffer), nreq, dprev, dnext,
		 REQ *, captr->nodeid);
      Cache_arb_for_bus(nreq);
    }
}




/*=============================================================================
 * Getting the reply for an uncached READ request.
 */

void Cache_get_IO_reply(REQ *req)
{
  CACHE *captr = PID2L2C(req->node, req->src_proc);
  REQ   *creq, *nreq;

  req->data_done = 1;

  creq = req->parent;
  while (creq)
    {
      UBuffer_stat_set(creq);
      if (creq->type == REPLY)
	MemDoneHeapInsert(creq, req->hit_type);
      nreq = creq->parent;
      YS__PoolReturnObj(&YS__ReqPool, creq);
      creq = nreq;
    }

  UBuffer_stat_set(req);
  MemDoneHeapInsert(req, req->hit_type);
  YS__PoolReturnObj(&YS__ReqPool, req);  

  UBuffer_confirm_transaction(req);
}



/*=============================================================================
 * Search the outgoing buffer for a coherence request. If a coherence request
 * hits an entry in the outgoing buffer, this entry will be converted into
 * a data response.
 */
REQ *Cache_check_outbuffer(CACHE *captr, REQ *req)
{
  LinkQueue *lq       = &(captr->outbuffer);
  unsigned   blocknum = req->paddr >> captr->block_shift;
  REQ       *treq;
  int        i;

  for (i = 0; i < lq->size; i++)
    {
      lqueue_elem(lq, treq, i, captr->nodeid);
      if (blocknum == (treq->paddr >> captr->block_shift))
	return treq;
    }

  if (captr->pending_writeback)
    {
      if (blocknum == (captr->pending_writeback->paddr >> captr->block_shift))
	return captr->pending_writeback;
    }

  return 0;
}





