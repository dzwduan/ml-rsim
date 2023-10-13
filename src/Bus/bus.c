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
#include <malloc.h>
#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "IO/addr_map.h"
#include "IO/io_generic.h"
#include "Bus/bus.h"
#include "Memory/mmc.h"


BUS *AllBusses;
int  BUS_WIDTH          = 8;
int  BUS_FREQUENCY      = 1;
int  BUS_TURNAROUND     = 1;
int  BUS_ARBDELAY       = 1;
int  BUS_MINDELAY       = 0;
int  BUS_TOTAL_REQUESTS = 8;
int  BUS_CPU_REQUESTS   = 4;
int  BUS_IO_REQUESTS    = 4;
int  BUS_TRACE_ENABLE   = 0;
int  BUS_CRITICAL_WORD  = 1;



int MEM_CNTL;
int NUM_MODULES;


const char *RName[] =
{
  "Request",
  "Reply",
  "Coherency",
  "Coh Reply",
  "Writeback",
  "Writepurge",
  "Barrier"
};


/*=============================================================================
 * Initialize bus module.
 */

void Bus_init(void)
{
  BUS *pbus;
  int i, nid;
  char name[PATH_MAX];

  NUM_MODULES = ARCH_cpus + ARCH_ios;  /* does not include memory controller */
  MEM_CNTL = ARCH_cpus + ARCH_ios;

  /*
   * Bus configuration.
   * Default arbitration delay: two delay cycles + 1 dead cycle when 
   * mastership of the system interface changes. 
   */
  get_parameter("BUS_width",          &BUS_WIDTH,          PARAM_INT);
  get_parameter("BUS_arbdelay",       &BUS_ARBDELAY,       PARAM_INT);
  get_parameter("BUS_frequency",      &BUS_FREQUENCY,      PARAM_INT);
  get_parameter("BUS_total_requests", &BUS_TOTAL_REQUESTS, PARAM_INT);
  get_parameter("BUS_turnaround",     &BUS_TURNAROUND,     PARAM_INT);
  get_parameter("BUS_mindelay",       &BUS_MINDELAY,       PARAM_INT);
  get_parameter("BUS_critical",       &BUS_CRITICAL_WORD,  PARAM_INT);


  AllBusses = RSIM_CALLOC(BUS, ARCH_numnodes);
  if (!AllBusses)
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);
  
  for (nid = 0; nid < ARCH_numnodes; nid++)
    {
      pbus = PID2BUS(nid);

      pbus->nodeid  = nid;
      pbus->pstates = RSIM_CALLOC(int, NUM_MODULES + 1);
      if (!(pbus->pstates))
	YS__errmsg(nid, "Malloc failed at %s:%i", __FILE__, __LINE__);

      pbus->arbwaiters = RSIM_CALLOC(ARBWAITER, NUM_MODULES + 1);
      if (!(pbus->arbwaiters))
	YS__errmsg(nid, "malloc failed at %s:%i", __FILE__, __LINE__);

      pbus->cur_req    = NULL;
      pbus->state      = BUS_IDLE;
      pbus->busy_until = 0;
      pbus->req_count  = 0;

      pbus->req_queue = RSIM_CALLOC(REQ*, BUS_TOTAL_REQUESTS);
      if (!(pbus->req_queue))
	YS__errmsg(nid, "Malloc failed at %s:%i", __FILE__, __LINE__);
      for (i = 0; i < BUS_TOTAL_REQUESTS; i++)
	pbus->req_queue[i] = NULL;
      
      /* 
       * Arbitration related variables. Set the current winner to processor 0 
       * because processor 0 is likely going to be the first bus module to 
       * access the bus. The processors take turns in round-robin fashion.
       */
      for (i = 0; i < NUM_MODULES+1; i++)
	{
	  pbus->arbwaiters[i].req = 0;
	  pbus->pstates[i] = SLAVE_STATE;
	}

      pbus->arbwaiter_count = 0;
      pbus->cur_winner      = 0; 
      pbus->pstates[0]      = MASTER_STATE;
      pbus->rr_index        = MIN(1, NUM_MODULES - 1);
      pbus->next_winner     = -1; 

      pbus->write_flowcontrol = 0;
      
      pbus->last_start_time = 0.0;
      pbus->last_end_time   = 0.0;

      /*
       * Create the arbitrator to call BusMain at the appropriate time.
       * "issuer" is used to perform actual transferring on the bus.
       */
      pbus->arbitrator = NewEvent("bus_arbitrator", Bus_main, NODELETE, 0);
      pbus->arbitrator->uptr1 = (void *) pbus;

      pbus->issuer = NewEvent("bus_issuer", Bus_issuer, NODELETE, 0);
      pbus->issuer->uptr1 = (void *) pbus;

      pbus->performer = NewEvent("bus_perform", Bus_perform, NODELETE, 0);
      pbus->performer->uptr1 = (void *) pbus;

      pbus->num_trans    = RSIM_CALLOC(trans_count_t, NUM_MODULES+1);
      if (!(pbus->num_trans))
	YS__errmsg(nid, "Malloc failed at %s:%i", __FILE__, __LINE__);
      pbus->lat_trans    = RSIM_CALLOC(trans_latency_t, NUM_MODULES+1);
      if (!(pbus->lat_trans))
	YS__errmsg(nid, "Malloc failed at %s:%i", __FILE__, __LINE__);
      pbus->num_subtrans = RSIM_CALLOC(subtrans_count_t, NUM_MODULES+1);
      if (!(pbus->num_subtrans))
	YS__errmsg(nid, "Malloc failed at %s:%i", __FILE__, __LINE__);
      Bus_stat_clear(nid);
      
      if (BUS_TRACE_ENABLE)
	{
	  sprintf(name, "%s_bus.%02d", trace_dir, nid);
	  pbus->busfile = fopen(name, "w");
	  if (pbus->busfile == NULL)
	    YS__errmsg(nid, "Cannot open bus trace file %s\n", name);
	}
      else
	pbus->busfile = NULL;
    }
}



/*=============================================================================
 * Called by "pbus->arbitrator" whenever there is something that should be
 * taken care of by the bus.
 */

void Bus_main(void)
{
  BUS *pbus = (BUS*)YS__ActEvnt->uptr1;

  switch (pbus->state)
    {
    case BUS_ARBITRATING:
      Bus_arbitrator(pbus);
      break;

    case BUS_WAKINGUP:
      Bus_wakeup_winner(pbus);
      break;

    default:
      YS__errmsg(pbus->nodeid,
		 "How did we get into bus state %d?",
		 pbus->state);
    }
}


/*=============================================================================
 * Called by a processor which has something to send out through the bus
 * and which is not in master state. Bus receives only one arbitration request
 * from one bus module at one time. The bus module has to buffer the 
 * arbitration requests on its own when more than one arb_req is outstanding.
 */

void Bus_recv_arb_req(int nid, int pid, REQ *req, double start_time)
{
  BUS *pbus = (BUS*)PID2BUS(nid);
  long long start = (long long)(start_time + 0.99);
  
  if (pbus->arbwaiters[pid].req)
    YS__errmsg(pbus->nodeid,
	       "module %d has already an arbitration waiter.",
	       pid);

  /* synchronize start time to bus cycle */
  while (start % BUS_FREQUENCY)
    start++;
  
  pbus->arbwaiters[pid].req = req;
  pbus->arbwaiters[pid].time = (double)start;
  pbus->arbwaiter_count++;

  req->arb_start_time = YS__Simtime;
  
  if (IsNotScheduled(pbus->arbitrator))
    {
      pbus->state = BUS_ARBITRATING;
      schedule_event(pbus->arbitrator, MAX(YS__Simtime, pbus->busy_until));
    }
}



/*=============================================================================
 * Determine which module will drive the bus after current bus owner (i.e. the
 * one in master state.)
 */

void Bus_arbitrator(BUS *pbus)
{
  int        i, idx;
  ARBWAITER *parb;
  REQ       *next_req;
  int        next_winner = -1;
  double     start_time = 1e20;
  double     end_time  = 1.0e20; /* big enough? */
  long long  next_cycle;
  

  if (pbus->arbwaiter_count == 0)
    return;

  next_cycle = (long long)(YS__Simtime + 0.99);
  while (next_cycle % BUS_FREQUENCY)
    next_cycle++;
  
  /* find next winner, determine request start time -------------------------*/

  for (i = pbus->rr_index; i < NUM_MODULES; i++)
    {
      parb = &(pbus->arbwaiters[i]);
      if (parb->req && parb->time < start_time)
	{
	  if ((pbus->req_count == BUS_TOTAL_REQUESTS) &&
	      (parb->req->type == REQUEST) &&
	      (parb->req->req_type != WRITE_UC))
	    continue;

	  if ((pbus->write_flowcontrol) &&
	      ((parb->req->type == COHE_REPLY) ||
	       (parb->req->type == WRITEBACK) ||
	       (parb->req->type == WRITEPURGE)))
	    continue;
	  
	  next_winner = i;
	  start_time = parb->time;
	}
    }

  for (i = 0; i < pbus->rr_index; i++)
    {
      parb = &(pbus->arbwaiters[i]);
      if (parb->req && parb->time < start_time)
	{
	  if ((pbus->req_count == BUS_TOTAL_REQUESTS) &&
	      (parb->req->type == REQUEST) &&
	      (parb->req->req_type != WRITE_UC))
	    continue;

	  if ((pbus->write_flowcontrol) &&
	      ((parb->req->type == COHE_REPLY) ||
	       (parb->req->type == WRITEBACK) ||
	       (parb->req->type == WRITEPURGE)))
	    continue;
	  
	  next_winner = i;
	  start_time = parb->time;
	}
    }



  /*
   * If the MMC wants to use the bus, it has highest priority.
   */
  if (pbus->arbwaiters[MEM_CNTL].req && 
      pbus->arbwaiters[MEM_CNTL].time <= start_time)
    {
      next_winner = MEM_CNTL;
      start_time  = pbus->arbwaiters[MEM_CNTL].time;
    }
  else
    {
      /* 
       * Bus is going to be used by a processor. Advance round-robin index.
       */
      if (++pbus->rr_index == NUM_MODULES)
	pbus->rr_index = 0;
    }

  if (next_winner == -1)
    {
      schedule_event(pbus->arbitrator, YS__Simtime + BUS_FREQUENCY);
      return;
    }

  pbus->arbwaiter_count--;
  pbus->next_winner = next_winner;
  next_req = pbus->arbwaiters[next_winner].req;
  pbus->arbwaiters[next_winner].req = NULL;
  pbus->next_req = next_req;

  
  /*
   * 1. Driver does not change:
   *    start driving at end of current transaction
   *    at least BUS_MINDELAY cycles after last transaction start
   *    synchronize to bus cycle
   * 2. Driver changes:
   *    start driving BUS_TURNAROUND cycles after end of transaction
   *    at least BUS_ARBDELAY cycles after arbitration request
   *    at least BUS_MINDELAY cycles after last transaction start
   *    synchronize to bus cycle
   */
  if (pbus->next_winner == pbus->cur_winner)
    pbus->next_start_time =
      MAX(MAX(pbus->last_start_time + BUS_MINDELAY * BUS_FREQUENCY,
	      pbus->last_end_time),
	  (double)next_cycle);
  else
    pbus->next_start_time =
      MAX(MAX(pbus->last_start_time + BUS_MINDELAY * BUS_FREQUENCY,
	      pbus->last_end_time + BUS_TURNAROUND * BUS_FREQUENCY),
	  MAX(start_time + BUS_ARBDELAY * BUS_FREQUENCY,
	      (double)next_cycle));

  
  if (pbus->next_start_time < pbus->busy_until)
    pbus->next_start_time = pbus->busy_until;

  pbus->last_start_time = pbus->next_start_time;
  pbus->last_end_time   = pbus->next_start_time +
    pbus->next_req->bus_cycles * BUS_FREQUENCY;

  /*
   * Try to wakeup the next winner at appropriate time.
   */
  pbus->state = BUS_WAKINGUP;
  schedule_event(pbus->arbitrator, pbus->next_start_time);
}



/*=============================================================================
 * Wake up the bus module who starts owning the bus at this cycle.
 */

void Bus_wakeup_winner(BUS *pbus)
{
  if (pbus->busy_until > YS__Simtime)
    YS__errmsg(pbus->nodeid,
	       "How can busy_until > YS__Simtime happen? (%f > %f)",
	       pbus->busy_until, YS__Simtime);

  pbus->pstates[pbus->cur_winner]  = SLAVE_STATE;
  pbus->pstates[pbus->next_winner] = MASTER_STATE;
  pbus->cur_winner = pbus->next_winner;
  pbus->next_winner = -1;
  pbus->busy_until = YS__Simtime +
    (pbus->next_req->bus_cycles * BUS_FREQUENCY);

  if (pbus->cur_winner == MEM_CNTL)
    MMC_in_master_state(pbus->next_req);
  else if (pbus->cur_winner < ARCH_cpus)
    Cache_in_master_state(pbus->next_req);
  else
    IO_in_master_state(pbus->next_req);

  /*-------------------------------------------------------------------------*/
  /* 
   * MMC_in_master_state and Cache_in_master_state should set busy_until 
   * with an appropriate value bigger than YS__Simtime.
   */
  if (IsNotScheduled(pbus->arbitrator))
    {
      if (pbus->arbwaiter_count > 0)
	{
	  pbus->state = BUS_ARBITRATING;
	  schedule_event(pbus->arbitrator, pbus->busy_until);
	}
      else
	pbus->state = BUS_IDLE;
    }
}




/*===========================================================================*/

void Bus_start(REQ *req, int owner)
{
  BUS *pbus = PID2BUS(req->node);


  if (IsScheduled(pbus->issuer))
    YS__errmsg(pbus->nodeid,
	       "IOGetMasterState: scheduling scheduled bus issuer");

  pbus->cur_req       = req;
  pbus->cur_req_owner = owner;
  pbus->busy_until = YS__Simtime + (req->bus_cycles * BUS_FREQUENCY);
  
  if ((req->type == REQUEST) &&
      (req->req_type != WRITE_UC))
    pbus->req_count++;
      
  if ((req->type == REPLY) ||
      (req->type == COHE_REPLY))
    pbus->req_count--;

  Bus_start_trans(pbus, req);

  if ((BUS_CRITICAL_WORD) &&
      ((req->type == REPLY) || (req->type == COHE_REPLY)))
    {
      schedule_event(pbus->performer, YS__Simtime + BUS_FREQUENCY);
    }
  else
    {
      schedule_event(pbus->performer, pbus->busy_until - 0.1);
    }
    
  schedule_event(pbus->issuer, pbus->busy_until);

  if ((req->type == REQUEST) || (req->type == COHE_REPLY))
    pbus->num_subtrans[req->src_proc][req->req_type]++;
  if (req->type == REPLY)
    {
      pbus->num_subtrans[req->dest_proc][req->req_type]++;
      pbus->num_trans[req->dest_proc][req->type]++;
      pbus->lat_trans[req->dest_proc][req->type]+=req->bus_cycles;
    }
  else
    {
      pbus->num_trans[req->src_proc][req->type]++;
      pbus->lat_trans[req->src_proc][req->type]+=req->bus_cycles;
    }
}



  
/*=============================================================================
 * Send request on the bus (and of transaction).
 */

void Bus_issuer(void)
{
  BUS *pbus = (BUS *) YS__ActEvnt->uptr1;
  REQ *req = pbus->cur_req;

  if (req == NULL)
    YS__errmsg(pbus->nodeid,
	       "Bus_issuer(): Nothing to be sent on bus, why are we here?");


  pbus->arb_delay += (YS__Simtime -
		      req->bus_cycles*BUS_FREQUENCY -
		      req->arb_start_time);
  
  /*-------------------------------------------------------------------------*/

  Bus_finish_trans(pbus, pbus->cur_req);

  pbus->cur_req = NULL;
}




/*=============================================================================
 * Send request on the bus.
 */

void Bus_perform(void)
{
  BUS *pbus = (BUS *) YS__ActEvnt->uptr1;

  if (pbus->cur_req == NULL)
    YS__errmsg(pbus->nodeid,
	       "Bus_perform(): Nothing to be sent on bus, why are we here?");

  if (pbus->cur_req_owner == PROCESSOR)
    Cache_send_on_bus(pbus->cur_req);
  
  if (pbus->cur_req_owner == IO_MODULE)
    IO_send_on_bus(pbus->cur_req);
  
  if (pbus->cur_req_owner == COORDINATOR)
    MMC_send_on_bus(pbus->cur_req);
}



/*=============================================================================
 * Send an I/O request to the specified I/O port.
 */

void Bus_send_IO_request(REQ *req)
{
  int n;
  REQ *new_req;

  if (req->dest_proc < ARCH_cpus)
    {
      if (req->dest_proc == TARGET_ALL_CPUS)
	{                         /* @@@ copy instance and code @@@ */
	  for (n = 0; n < ARCH_cpus - 1; n++)
	    {
	      new_req = (REQ*)YS__PoolGetObj(&YS__ReqPool);
	      memcpy(new_req, req, sizeof(REQ));
	      new_req->dest_proc = n;
	      Cache_get_noncoh_request(new_req);
	    }
	  req->dest_proc = n;
	  Cache_get_noncoh_request(req);
	}
      else
	Cache_get_noncoh_request(req);
    }
  else
    IO_get_request(PID2IO(req->node, req->dest_proc), req);
}




/*=============================================================================
 * Send a coherence request on bus. The coherence request will be taken
 * as external request by each bus module.
 */

void Bus_send_request(REQ *req)
{
  BUS *pbus = PID2BUS(req->node);
  int n;
  
  if (!tlb_non_coh(req->memattributes))
    {
      Cache_get_cohe_request(req);

      IO_get_cohe_request(req);
    }

  MMC_recv_request(req);
}



/*=============================================================================
 * Send a coherence request on bus. The coherence request will be taken
 * as external request by each bus module.
 */

void Bus_send_writepurge(REQ *req)
{
  BUS *pbus = PID2BUS(req->node);
  int n;
  
  if (!tlb_non_coh(req->memattributes))
    {
      Cache_get_cohe_request(req);

      IO_get_cohe_request(req);
    }

  MMC_recv_request(req);
}



/*=============================================================================
 * Send a reply for an uncached (I/O) read back to the requester.
 */

void Bus_send_IO_reply(REQ *req)
{
  Cache_get_IO_reply(req);
}




/*=============================================================================
 * Send a reply back to the requestor. Used by the coordinator (i.e.
 * the main memory controller.
 */

void Bus_send_reply(REQ *req)
{
  BUS *pbus = PID2BUS(req->node);

  req->cohe_done = 1;
  req->data_done = 1;

  if (req->src_proc < ARCH_cpus)
    Cache_get_reply(req);
  else
    IO_get_reply(req);
}




/*=============================================================================
 * Send a write back to the memory. Used by the processor when
 * dirty data is casted out the cache.
 */

void Bus_send_writeback(REQ *req)
{
  BUS *pbus = PID2BUS(req->node);

  MMC_recv_write(req);
}




/*=============================================================================
 * Send a coherence report to cluster coordinator. Since there are dedicated
 * lines between processor and cluster coordinator, a processor can send
 * the coherence response at any time. 
 */

void Bus_send_cohe_response(REQ *req, int state)
{
  MMC_recv_cohe_response(req, state);
}





/*=============================================================================
 * Send a coherence data response. It's used to send out dirty data
 * directly to the requester and the coordinator.
 */

void Bus_send_cohe_data_response(REQ *req)
{
  BUS *pbus = PID2BUS(req->node);
  REQ *nreq;
  
  /*
   * First, send dirty data to the requester.
   * Second, send dirty data to the main memory controller. Dirty
   * data is written back to memory so that the requester can mark 
   * the data clean instead of dirty to avoid further writebacks.
   */

  nreq = (REQ *)YS__PoolGetObj(&YS__ReqPool);
  memcpy(nreq, req, sizeof(REQ));

  if (req->dest_proc < ARCH_cpus)
    Cache_get_data_response(req);
  else
    IO_get_data_response(req);
  MMC_recv_data_response(nreq);
}





/*=============================================================================
 * Send a coherence completion. Used by the coordinator to complete
 * a request which has got the required data through cache-to-cache
 * transfer.
 */

void Bus_send_cohe_completion(REQ *req)
{
  req->cohe_done = 1;

  if (req->src_proc < ARCH_cpus)
    Cache_get_reply(req);
  else
    IO_get_reply(req);
}



void Bus_start_trans(BUS* pbus, REQ* req)
{
  unsigned mask;

  if (pbus->busfile == NULL)
    return;

  if ((req->req_type != WRITE_UC) &&
      (req->req_type != READ_UC) &&
      (req->req_type != SWAP_UC) &&
      (req->req_type != REPLY_UC))
    mask = ~(ARCH_linesz2 - 1);
  else
    mask = 0xFFFFFFFF;

  fprintf(pbus->busfile, "%8.0f  Start  ",
	  YS__Simtime);

  if ((req->type == REQUEST) || (req->type == COHE_REPLY))
    {
      fprintf(pbus->busfile, "%s\t%08X\t%2d\t%i -> ",
	      ReqName[req->req_type],
	      req->paddr & mask,
	      req->bus_cycles,
	      req->src_proc);

      if (req->dest_proc == TARGET_ALL_CPUS)
	fprintf(pbus->busfile, "All CPUs\n");
      if (req->dest_proc == TARGET_ALL_IOS)
	fprintf(pbus->busfile, "All IOs\n");
      if (req->dest_proc == TARGET_ALL_MODULES)
	fprintf(pbus->busfile, "All Modules\n");
      if (req->dest_proc >= 0)
	fprintf(pbus->busfile, "%i\n", req->dest_proc);
    }

  if (req->type == WRITEBACK)
    fprintf(pbus->busfile, "WRITE_BACK\t%08X\t%2d\t%i -> %i\n",
	    req->paddr & mask,
	    req->bus_cycles,
	    req->src_proc,
	    req->dest_proc);

  if (req->type == WRITEPURGE)
    fprintf(pbus->busfile, "WRITE_PURGE\t%08X\t%2d\t%i -> %i\n",
	    req->paddr & mask,
	    req->bus_cycles,
	    req->src_proc,
	    req->dest_proc);

  if (req->type == REPLY)
    {
      fprintf(pbus->busfile, "%s\t%08X\t%2d\t%i -> ",
	      ReqName[req->req_type],
	      req->paddr & mask,
	      req->bus_cycles,
	      req->dest_proc);

      if (req->src_proc == TARGET_ALL_CPUS)
	fprintf(pbus->busfile, "All CPUs\n");
      if (req->src_proc == TARGET_ALL_IOS)
	fprintf(pbus->busfile, "All IOs\n");
      if (req->src_proc == TARGET_ALL_MODULES)
	fprintf(pbus->busfile, "All Modules\n");
      if (req->src_proc >= 0)
	fprintf(pbus->busfile, "%i\n", req->src_proc);
    }

  fflush(pbus->busfile);
}



void Bus_finish_trans(BUS* pbus, REQ* req)
{
  if (pbus->busfile == NULL)
    return;

  if (req->type == WRITEBACK)
    fprintf(pbus->busfile, "%8.0f  Finish WRITE_BACK\n\n",
	    YS__Simtime);
  else if (req->type == WRITEPURGE)
    fprintf(pbus->busfile, "%8.0f  Finish WRITE_PURGE\n\n",
	    YS__Simtime);
  else
    fprintf(pbus->busfile, "%8.0f  Finish %s\n\n",
	    YS__Simtime,
	    ReqName[req->req_type]);

  fflush(pbus->busfile);
}



/*****************************************************************************/
/* Bus statistics functions                                                  */
/*****************************************************************************/

void Bus_print_params(int nid)
{
  YS__statmsg(nid, "Bus Configuration\n");
  YS__statmsg(nid, "  width:                %3d bytes\n", BUS_WIDTH);
  YS__statmsg(nid, "  frequency:            %3d (%.3f MHz)\n",
	      BUS_FREQUENCY, 1000000.0 / ((double)CPU_CLK_PERIOD * (double)BUS_FREQUENCY));
  YS__statmsg(nid,
	      "  arbitration delay:    %3d         turnaround cycles: %3d\n",
	      BUS_ARBDELAY, BUS_TURNAROUND);
  YS__statmsg(nid,
	      "  mininum delay:        %3d  (# cycles between addresses)\n",
	      BUS_MINDELAY);
  YS__statmsg(nid,
	      "  outstanding requests: %3d;  %d per CPU;  %d per I/O device\n",
	      BUS_TOTAL_REQUESTS, BUS_CPU_REQUESTS, BUS_IO_REQUESTS);
  YS__statmsg(nid,
	      "  critical-word first :   %s\n", BUS_CRITICAL_WORD ? "on" : "off");
  YS__statmsg(nid, "\n");
}



void Bus_stat_report(int nid)
{
  BUS *pbus = PID2BUS(nid);
  int  n, i;
  long long num_trans = 0;
  long long lat_trans = 0;
  double    total_cycles = (YS__Simtime - pbus->last_clear) / BUS_FREQUENCY;
  
  YS__statmsg(nid, "Bus Statistics\n");

  for (n = 0; n < NUM_MODULES+1; n++)
    {
      for (i = 0; i < sizeof(trans_count_t) / sizeof(long long); i++)
	{
	  num_trans += pbus->num_trans[n][i];
	  lat_trans += pbus->lat_trans[n][i];
	}
    }
  
  YS__statmsg(nid, "  Avg. Arbitration Delay: %.2f processor cycles\n\n",
	      pbus->arb_delay / num_trans);
  YS__statmsg(nid, "  Transaction Breakdown\t       Count\t      Cycles (utilization)\n");
  YS__statmsg(nid, "    Total\t\t%12lld\t%12lld (%2.2lf%%)\n",
	      num_trans, lat_trans, 100.0 * ((double)lat_trans/total_cycles));

  for (i = 0; i < sizeof(trans_count_t) / sizeof(long long); i++)
    {
      num_trans = 0;
      lat_trans = 0;
      for (n = 0; n < NUM_MODULES+1; n++)
	{
	  num_trans += pbus->num_trans[n][i];
	  lat_trans += pbus->lat_trans[n][i];
	}

      if (num_trans == 0)
	continue;
      
      YS__statmsg(nid, "    %s \t\t%12lld\t%12lld (%2.2lf%%)\n",
		  RName[i], num_trans, lat_trans,
		  100.0 * ((double)lat_trans/total_cycles));

      for (n = 0; n < NUM_MODULES+1; n++)
	if (pbus->num_trans[n][i] != 0)
	  {
	    if (n < ARCH_cpus)
	      YS__statmsg(nid, "       CPU %i \t\t\t%12lld\n",
		      n, pbus->num_trans[n][i]);
	    else if (n < ARCH_cpus + ARCH_ios)
	      YS__statmsg(nid, "       IO %i \t\t\t%12lld\n",
		      n - ARCH_cpus, pbus->num_trans[n][i]);
	    else
	      YS__statmsg(nid, "       MemCntl \t\t\t%12lld\n",
		      pbus->num_trans[n][i]);
	  }
    }
  
  YS__statmsg(nid, "\n  Transaction Subtype Breakdown\n");  
  for (i = 0; i < sizeof(subtrans_count_t) / sizeof(long long); i++)
    {
      num_trans = 0;
      for (n = 0; n < NUM_MODULES+1; n++)
	num_trans += pbus->num_subtrans[n][i];

      if (num_trans == 0)
	continue;
      
      YS__statmsg(nid, "    %s \t%12lld\n",
		  ReqName[i], num_trans);

      for (n = 0; n < NUM_MODULES+1; n++)
	if (pbus->num_subtrans[n][i] != 0)
	  {
	    if (n < ARCH_cpus)
	      YS__statmsg(nid, "       CPU %i \t\t\t%12lld\n",
		      n, pbus->num_subtrans[n][i]);
	    else if (n < ARCH_cpus + ARCH_ios)
	      YS__statmsg(nid, "       IO %i \t\t\t%12lld\n",
		      n - ARCH_cpus, pbus->num_subtrans[n][i]);
	    else
	      YS__statmsg(nid, "       MemCntl \t\t\t%12lld\n",
		      pbus->num_subtrans[n][i]);
	  }
    }
  
  YS__statmsg(nid, "\n\n");
}



void Bus_stat_clear(int nid)
{
  int i, n;
  BUS *pbus = PID2BUS(nid);
  
  for (n = 0; n < NUM_MODULES+1; n++)
    {
      for (i = 0; i < sizeof(trans_count_t) / sizeof(long long); i++)
	{
	  pbus->num_trans[n][i] = 0;
	  pbus->lat_trans[n][i] = 0;
	}
      for (i = 0; i < sizeof(subtrans_count_t) / sizeof(long long); i++)
	pbus->num_subtrans[n][i] = 0;
    }
  pbus->arb_delay   = 0.0;
  pbus->last_clear  = YS__Simtime;
}

