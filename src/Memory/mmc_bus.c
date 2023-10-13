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
#include <string.h>
#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Memory/mmc.h"
#include "Bus/bus.h"


extern int MEM_CNTL;


void MMC_send_reply(mmc_info_t *, REQ *req);
void MMC_start_send_to_bus(mmc_info_t *pmmc, REQ *req);
void MMC_in_master_state(REQ *req);


/*=============================================================================
 * MMC is receiving a request from the bus. Called by bus module.
 */
void MMC_recv_request(REQ *req)
{
  mmc_info_t  *pmmc = NID2MMC(req->node);

  /*
   * Set default values for these variables which should be set and
   * returned back to processor by MMC.
   */
  req->c2c_copy   = 0;
  req->hit_type   = MEMHIT;
  req->line_cold  = 0; /* support it??? */
  req->data_rtn   = 0;

  if (!tlb_non_coh(req->memattributes))
    {
      req->cohe_count = ARCH_cpus + ARCH_coh_ios;
      req->cohe_type  = REPLY_EXCL;
    }
  else
    req->cohe_count = 0;


  /*
   * Add it to request queue, which behaves like coherence scoreboard.
   * BTW, WRITEBACK doesn't need any coherence responses.
   */
  if (req->type != WRITEBACK)
    dqueue_add(&(pmmc->reqqueue), req, dprev2, dnext2, REQ *, pmmc->nodeid);


  /*
   * Upgrade should never require data from main memory. This request just
   * stays in request queue waiting for coherence reports and possible
   * dirty data from another processor.
   */
  if (req->req_type == UPGRADE)
    {
      pmmc->stats.upgrades++;
      req->data_rtn = 1;
      return;
    }

  
  /*
   * Create an internal MMC transaction and throw it into MMC timing model.
   */
  MMC_recv_trans(pmmc, req);
}





/*=============================================================================
 * MMC receives a write back. Called by bus module.
 */
void MMC_recv_write(REQ *req)
{
  mmc_info_t *pmmc = NID2MMC(req->node);

  req->cohe_count = 0;
  MMC_recv_trans(pmmc, req);
}




/*=============================================================================
 * MMC receives coherence response from a processor. Called by bus module.
 */
void MMC_recv_cohe_response(REQ *req, int state)
{
  mmc_info_t *pmmc = NID2MMC(req->node);
  MSHR       *pmshr;
  int         i;

  req->cohe_count--;

  if (req->cohe_type < state)
    req->cohe_type = state;

  /*
  if ((req->cohe_count == 0) && (req->c2c_copy) &&
      (req->req_type != UPGRADE) && (req->progress > 0))
    YS__PoolReturnObj(&YS__ReqPool, req);
  */
  
  if ((req->cohe_count == 0) && (((req->data_rtn == 1) && !req->c2c_copy) ||
				 (req->req_type == UPGRADE)))
    {
      MMC_send_reply(NID2MMC(req->node), req);
      return;
    }


  if ((req->cohe_count == 0) && (req->type == WRITEPURGE))
    {
      req->perform(req);

      dqueue_remove(&(pmmc->reqqueue), req, dprev2, dnext2, REQ *);

      for (i = 0; i < ARCH_cpus; i++)
	{
	  pmshr = req->stalled_mshrs[i];
	  if (pmshr && pmshr->pending_cohe == req)
	    {
	      pmshr->pending_cohe = NULL;
	      req->stalled_mshrs[i] = NULL;
	    }
	}

      YS__PoolReturnObj(&YS__ReqPool, req);
    }
}




/*=============================================================================
 * MMC receives a data response associated with a cohe request. 
 * Called by bus module.
 */
void MMC_recv_data_response(REQ *req)
{
  mmc_info_t *pmmc = NID2MMC(req->node);
  REQ        *par  = req->parent;

  par->c2c_copy = 1;

  if (par->cohe_type < req->req_type)
    par->cohe_type = req->req_type;

  if (par->cohe_count == 0)     /* was commented out @@@ */
    MMC_send_reply(pmmc, par);

  par->memtrans->c2c_copy = 1;

  req->type = WRITEBACK;
  MMC_recv_trans(pmmc, req);
}




/*=============================================================================
 * The requested data has been fetched from the DRAM. MMC can send
 * a reply to the requester. Called by MMC module.
 */
void MMC_data_fetch_done(mmc_info_t *pmmc, REQ *req)
{
  req->data_rtn = 1;

  if ((req->cohe_count == 0) && (!req->c2c_copy))
    MMC_send_reply(pmmc, req);
}




/*=============================================================================
 * Send reply to the requester (processor). Called by MMC module.
 */
void MMC_send_reply(mmc_info_t *pmmc, REQ *req)
{
  if (req->req_type == UPGRADE)
    {
      if (req->c2c_copy)
	{
	  req->req_type = req->cohe_type;
	  return;
	}

      PID2BUS(req->node)->req_count--;

      req->c2c_copy = 1; 
    }

  if ((req->data_rtn == 1) || (req->c2c_copy == 1))
    {
      dqueue_remove(&(pmmc->reqqueue), req, dprev2, dnext2, REQ *);
    }

  req->type = REPLY;
  req->req_type = req->cohe_type;

  if (req->c2c_copy)
    Bus_send_cohe_completion(req);
  else
    MMC_start_send_to_bus(pmmc, req);
}




/*=============================================================================
 * Called when a request should be returned back. Called by MMC module.
 */
void MMC_start_send_to_bus(mmc_info_t *pmmc, REQ *req)
{
  if (tlb_uncached(req->memattributes))
    req->bus_cycles = (req->size + BUS_WIDTH - 1) / BUS_WIDTH;
  else
    req->bus_cycles = ARCH_linesz2 / BUS_WIDTH;

  if (pmmc->arbwaiters_count > 0)
    {
      lqueue_add(&(pmmc->arbwaiters), req, pmmc->nodeid);
      pmmc->arbwaiters_count++;
    }
  else
    {
      BUS *pbus = PID2BUS(req->node);
      /*
       * The MMC is in master state; and next winner either hasn't been 
       * picked up or we have enough time to transfer this request 
       * before the next winner takes over.
       */
      pmmc->arbwaiters_count = 1;

      if (InMasterState(req->node, MEM_CNTL) &&
	  BusIsIdleUntil(req->bus_cycles * BUS_FREQUENCY))
	{
	  /*	  PID2BUS(req->node)->req_count--; */

	  req->arb_start_time = YS__Simtime;
	  MMC_in_master_state(req);
	}
      else
	Bus_recv_arb_req(pmmc->nodeid, MEM_CNTL, req, YS__Simtime);
   }
}



/*=============================================================================
 * Starting driving the bus to send out the pending request.
 * Called by bus module.
 */
void MMC_in_master_state(REQ *req)
{
  mmc_info_t *pmmc = NID2MMC(req->node);

  
  Bus_start(req, COORDINATOR);


  /* More arbitration waiters? ----------------------------------------------*/
  if (pmmc->arbwaiters_count > 0)
    {
      if (--pmmc->arbwaiters_count > 0)
	{
	  REQ *newreq;
	  lqueue_get(&(pmmc->arbwaiters), newreq);
	  Bus_recv_arb_req(pmmc->nodeid, MEM_CNTL, newreq, YS__Simtime);
	}
    }
}



/*=============================================================================
 * Called by the bus module at the last cycle of a request transferring on
 * bus. Called by bus module.
 */
void MMC_send_on_bus(REQ *req)
{
  if (tlb_uncached(req->memattributes))
    Bus_send_IO_reply(req);
  else
    Bus_send_reply(req);
}
