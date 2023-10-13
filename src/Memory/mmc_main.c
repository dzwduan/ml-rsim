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

#include <stdio.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Memory/mmc.h"
#include "Bus/bus.h"
#include "DRAM/cqueue.h"
#include "DRAM/dram.h"

static void MMC_trans_enqueue(mmc_info_t *pmmc, mmc_trans_t *ptrans);

/*
 * MMC receives a transaction. The first thing MMC does is to check the
 * cache. If the data is in cache, bingo! Otherwise, access the DRAMs.
 */
void MMC_recv_trans(mmc_info_t *pmmc, REQ *req)
{
  mmc_trans_t *ptrans;

  ptrans = MMC_get_free_trans(pmmc);
  ptrans->issued      = YS__Simtime;
  ptrans->paddr       = req->paddr;
  ptrans->size        = mparam.cache_line_size;
  ptrans->c2c_copy    = 0;

  req->memtrans = ptrans;

  switch (req->type)
    {
    case REQUEST:
      pmmc->stats.reads++;
      ptrans->mtype = MMC_READ;
      ptrans->req   = req;
      break;

    case WRITEBACK:
    case WRITEPURGE:
      pmmc->stats.writes++;
      ptrans->mtype = MMC_WRITE;
      ptrans->req   = 0;
      if (req->type == WRITEBACK)
	YS__PoolReturnObj(&YS__ReqPool, req);
      break;

    case COHE_REPLY:
      pmmc->stats.copyouts++;
      ptrans->mtype = MMC_WRITE;
      ptrans->req   = 0;
      YS__PoolReturnObj(&YS__ReqPool, req);
      break;
  
    default:
      YS__errmsg(pmmc->nodeid,
		 "MMC_recv_trans: Unknow request type %d", req->type);
    }

  if (ptrans->mtype == MMC_WRITE)
    {
      pmmc->wb_count++;
      if (pmmc->wb_count == mparam.max_writeback_count - 1)
	PID2BUS(pmmc->nodeid)->write_flowcontrol++;
    }
  
  if (mparam.sim == MMC_SIM_DETAILED)
    MMC_trans_enqueue(pmmc, ptrans);
  else
    MMC_nosim_start(pmmc, ptrans);
}




/*
 * First step a memory transaction need go through.
 */
static void MMC_trans_enqueue(mmc_info_t *pmmc, mmc_trans_t *ptrans)
{
  ptrans->time = YS__Simtime;

  lqueue_add(&(pmmc->waitlist), ptrans, pmmc->nodeid);

  if (IsNotScheduled(pmmc->waittask))
    {
      if (pmmc->waitlist.size != 1)
	YS__errmsg(pmmc->nodeid,
		   "MMCGetRequest: no event scheduled for waiting req");

      schedule_event(pmmc->waittask, ptrans->time);
    }
}



/*
 * This functions is activated by event "waittask". Try to process one
 * transaction from the wait list or the high priority list.
 */
void MMC_process_waiter(void)
{
  mmc_info_t  *pmmc = (mmc_info_t *) YS__ActEvnt->uptr1;
  mmc_trans_t *ptrans = 0;

#ifdef DEBUG
  if (pmmc->waitlist.size == 0)
    {
      YS__warnmsg(pmmc->nodeid, "MMC_process_waiter: nothing to be processed");
      return;
    }
#endif

  /*
   * Check the wait list. 
   */
  if (pmmc->slave_count < 8 && pmmc->waitlist.size > 0 && 
	   ((mmc_trans_t*)(lqueue_head(&pmmc->waitlist)))->time <= YS__Simtime)
    {
      lqueue_get(&(pmmc->waitlist), ptrans);
    }

  if (ptrans)
    {
      pmmc->slave_count++;
      DRAM_recv_request(pmmc->nodeid, ptrans, ptrans->paddr, 
			mparam.cache_line_size, 
			(ptrans->mtype != MMC_READ));
    }
  else if (pmmc->slave_count >= 8)
    {
      schedule_event(pmmc->waittask, YS__Simtime + 1);
      return;
    }

  /*
   * Schedule for next transaction
   */
  ptrans = lqueue_head(&pmmc->waitlist);
  if (ptrans)
    {
      rsim_time_t nexttime = 1.0e20;
      if (ptrans)
	nexttime = MIN(nexttime, ptrans->time);
   
      schedule_event(pmmc->waittask, MAX(YS__Simtime, nexttime));
    }
}




/*
 * Called by DRAM module when a DRAM starts delivering data.
 */
void MMC_dram_data_ready(int nodeid, mmc_trans_t *ptrans)
{
  mmc_info_t *pmmc = NID2MMC(nodeid);

  /*
   * The transaction has gathered all the data it needs. Consider it done.
   */
  pmmc->slave_count--;
  if (ptrans->mtype == MMC_READ)
    {
      if (!ptrans->c2c_copy)
	MMC_data_fetch_done(pmmc, ptrans->req);
    }
}




/*
 * Called by DRAM module when a DRAM access completes.
 * A transaction is done. Put it back to freelist.
 */
void MMC_dram_done(int nodeid, mmc_trans_t *ptrans)
{
  mmc_info_t *pmmc = NID2MMC(nodeid);

  if (ptrans->mtype == MMC_READ)
    {
      pmmc->stats.load_cycles += YS__Simtime - ptrans->issued;
      pmmc->stats.load_count++;
    }


  if (ptrans->mtype == MMC_WRITE)
    {
      pmmc->wb_count--;
      if (pmmc->wb_count == mparam.max_writeback_count - 2)
	PID2BUS(pmmc->nodeid)->write_flowcontrol--;
    }

  
  /*
   * Place the transaction back on the free list.
   */
  lqueue_add(&(pmmc->freelist), ptrans, pmmc->nodeid);

  pmmc->trans_count--;
  if (pmmc->trans_count == 0)
    {
      pmmc->free_start = YS__Simtime;
      pmmc->stats.busy_cycles += YS__Simtime - pmmc->busy_start;
    }

}



/*
 * Reserve a new MMC transaction for an incoming request.
 */
mmc_trans_t *MMC_get_free_trans(mmc_info_t *pmmc)
{
  mmc_trans_t *ptrans;

  if (pmmc->freelist.size == 0)
    YS__errmsg(pmmc->nodeid, "MMC_get_free_trans: Freelist empty");

  if (pmmc->trans_count == 0)
    {
      pmmc->busy_start = YS__Simtime;
      pmmc->stats.free_cycles += YS__Simtime - pmmc->free_start;
    }

  pmmc->trans_count++;

  lqueue_get(&(pmmc->freelist), ptrans);
  return ptrans;
}




/*
 * Used the the MMC simulator is turned off. In this case, each memory
 * access is serviced in a fixed latency (specified by mparam.latency in
 * terms of frequency cycles) and in FIFO order.
 */
void MMC_nosim_start(mmc_info_t *pmmc, mmc_trans_t *ptrans)
{
  if (pmmc->trans_count++ == 0)
    {
      pmmc->busy_start = YS__Simtime;
      pmmc->stats.free_cycles += YS__Simtime - pmmc->free_start;
    }

  if (IsNotScheduled(pmmc->pevent))
    {
      pmmc->pevent->uptr2 = ptrans;
      schedule_event(pmmc->pevent,
		     YS__Simtime + (mparam.latency * mparam.frequency));
    }
   else 
     lqueue_add(&(pmmc->waitlist), ptrans, pmmc->nodeid);
}




/*
 * Used when MMC simulator is turned off.
 */
void MMC_nosim_done(void)
{
  mmc_info_t   *pmmc   = YS__ActEvnt->uptr1;
  mmc_trans_t  *ptrans = YS__ActEvnt->uptr2;
  REQ          *req    = ptrans->req;
  rsim_time_t   delay;
  

  if (--pmmc->trans_count == 0)
    {
      pmmc->free_start = YS__Simtime;
      pmmc->stats.busy_cycles += YS__Simtime - pmmc->busy_start;
    }

  if (req)
    {
      if (req->type == WRITEPURGE)
	{
	  YS__PoolReturnObj(&YS__ReqPool, req);
	}
      else
	{
          if (!ptrans->c2c_copy)
   	    MMC_data_fetch_done(pmmc, req);
	}
    }

  if (pmmc->waitlist.size > 0)
    {
      mmc_trans_t *new_trans;
      lqueue_get(&(pmmc->waitlist), new_trans);
      pmmc->pevent->uptr2 = new_trans;

      if (mparam.sim == MMC_SIM_FIXED)
	delay = (mparam.latency * mparam.frequency);

      if (mparam.sim == MMC_SIM_PIPELINED)
	delay = new_trans->issued - YS__Simtime +
	  (mparam.latency * mparam.frequency);
      schedule_event(pmmc->pevent, YS__Simtime + delay);
    }

  if (ptrans)
    MMC_dram_done(pmmc->nodeid, ptrans);
}



/*
 * Used by DRAM backend. 
 */
int MMC_sim_on(void)
{
  return (mparam.sim == MMC_SIM_DETAILED);
}
