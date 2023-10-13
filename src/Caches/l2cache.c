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
#include <malloc.h>
#include "Processor/capconf.h"
#include "Processor/simio.h"
#include "Processor/memunit.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"
#include "Caches/syscontrol.h"
#include "IO/addr_map.h"
#include "Bus/bus.h"


/*
 * Macros which specify which pipeline each type of access belongs to.
 *
 * A separate COHE tag pipe is added to avoid deadlock. Such a port may be
 * considered excessive in a real system; thus, it may be advisable to
 * reserve a certain number of MSHR-like buffers for incoming COHE messages
 * and simply withhold processing of additional COHEs until space opens up
 * in one of these buffers.
 */
#define L2ReqDATAPIPE 		0
#define L2ReplyTAGPIPE 		0
#define L2CoheTAGPIPE 		1
#define L2RequestTAGPIPE 	2



/*
 * Functions to process request
 */
static int  L2ProcessDataReq            (CACHE*, REQ*);
static int  L2ProcessTagRequest         (CACHE*, REQ*);
static int  L2ProcessTagReply           (CACHE*, REQ*);
static int  L2ProcessTagCohe            (CACHE*, REQ*);
static int  L2ProcessTagCoheReply       (CACHE*, REQ*);
static int  L2ProcessFlushPurge         (CACHE*, REQ*);
static REQ *Cache_make_wb_req           (CACHE*, REQ*, ReqType);
static REQ *Cache_make_cohe_to_l1       (CACHE*, REQ*, int state);
static int  Cache_add_repl_to_l1        (CACHE*, REQ*);
static MSHR_Response L2Cache_check_mshr (CACHE*, REQ*);
static void L2Cache_start_prefetch      (CACHE*, REQ*);
static int  L2Cache_uncoalesce_mshr     (CACHE*, MSHR*);




/*===========================================================================
 * L2CacheInSim: function that brings new transactions from the ports into
 * the pipelines associated with the various cache parts. An operation can
 * be removed from its port as soon as pipeline space opens up for it.
 * Called whenever there may be something on an input port.
 */
void L2CacheInSim(int gid)
{
  CACHE *captr = L2Caches[gid];
  REQ   *req;
  int    nothing_on_ports = 1;


  /* First check if there is a pending cache-to-cache copy ================*/
  if (captr->data_cohe_pending)
    if (AddToPipe(captr->data_pipe[L2ReqDATAPIPE], captr->data_cohe_pending))
      {
        captr->data_cohe_pending = 0;
        captr->num_in_pipes++;
      }
	
  /* Coherence queue first because somebody else is waiting for you =======*/
  while (req = lqueue_head(&captr->cohe_queue))
    {
      if (AddToPipe(captr->tag_pipe[L2CoheTAGPIPE], req))
	{
	  req->progress = 0;
	  lqueue_remove(&(captr->cohe_queue));
	  captr->num_in_pipes++;
	}
      else
	{
	  nothing_on_ports = 0;
	  break;
	}
    }


  req = lqueue_head(&captr->noncohe_queue);
  if (req != NULL)
    {
      if ((IsSysControl_external(req, gid % ARCH_cpus)) ||
	  (IsSysControl_local(req)))
	{
	  if (SysControl_external_request(gid, req))
	    {
	      lqueue_remove(&(captr->noncohe_queue));
	    }
	  else
	    nothing_on_ports = 0;
	}   
      else
	YS__errmsg(captr->nodeid,
		   "Unknown request type in noncoherent queue\n");
    }

  
  /* Reply queue is always handled before request queue ===================*/
  while (req = lqueue_head(&captr->reply_queue))
    {
      if (AddToPipe(captr->tag_pipe[L2ReplyTAGPIPE], req))
	{
	  req->progress = 0;
	  lqueue_remove(&(captr->reply_queue));
	  captr->num_in_pipes++;
	}
      else
	{
	  nothing_on_ports = 0;
	  break;
	}
    }

  
  /* Request queue ========================================================*/
  while (req = lqueue_head(&captr->request_queue))
    {
      if (AddToPipe(captr->tag_pipe[L2RequestTAGPIPE], req))
	{
	  req->progress = 0;
	  lqueue_remove(&(captr->request_queue));
	  captr->num_in_pipes++;
	}
      else
	{
	  nothing_on_ports = 0;
	  break;
	}
    }

  if (nothing_on_ports)
    captr->inq_empty = 1;
}



/*===========================================================================
 * L2CacheOutSim: initiates actual processing of various REQ types
 * Called each cycle that there is something in pipes to be processed
 */

void L2CacheOutSim(int gid)
{
  CACHE    *captr = L2Caches[gid];
  Pipeline *pipe;
  REQ      *req;
  int       pipenum, ctr, index, result;

  /*
   * NOTE: L2 is not a monolithic piece of SRAM like the L1. The L2 is split
   * into a "tag SRAM" array and a "data SRAM" array. So, this function needs
   * to look at both tag pipes and data pipes. It looks at data pipes first,
   * as tag pipes might push something into data pipes, but never the other
   * way around.
   */
  for (pipenum = 0; pipenum < L2_DATA_PIPES; pipenum++)
    {
      pipe = captr->data_pipe[pipenum];

      for (ctr = 0; ctr < pipe->ports; ctr++)
	{
	  GetPipeElt(req, pipe);
	  if (req == 0)
	    break;

	  result = L2ProcessDataReq(captr, req);
	  if (result == 1)
	    {
	      ClearPipeElt(pipe);
	      captr->num_in_pipes--;
	    }
	  else if (result == -1)
	    {
	      SetPipeElt(req->invl_req, pipe);
	      req->invl_req = 0;
	    }
	}
    }

  
  /*-------------------------------------------------------------------------
   * Process tag pipelines. First, process reply queue; then, process
   * coherence request queue; Finally, it's request queue's turn.
   */
  if (!captr->stall_reply)
    {
      pipe = captr->tag_pipe[L2ReplyTAGPIPE];

      for (ctr = 0; ctr < pipe->ports; ctr++)
	{
	  GetPipeElt(req, pipe);
	  if (req == 0)
	    break;

	  result = L2ProcessTagReply(captr, req);
          if (result == 1)
	    {
	      ClearPipeElt(pipe);
	      captr->num_in_pipes--;
	    }
	  else if (result == -1)
	    {
	      SetPipeElt(req->invl_req, pipe);
	      req->invl_req = 0;
	    }
	}
    }

  if (!captr->stall_cohe && !captr->cohe_reply)
    {
      pipe = captr->tag_pipe[L2CoheTAGPIPE];

      for (ctr = 0; ctr < pipe->ports; ctr++)
	{
	  GetPipeElt(req, pipe);
	  if (req == 0)
	    break;

	  if (L2ProcessTagCohe(captr, req))
	    {
	      ClearPipeElt(pipe);
	      captr->num_in_pipes--;
	      captr->pstats->snoop_requests++;
	    }
	}
    }

  /*-----------------------------------------------------------------------*/
  
  if (!captr->stall_request)
    {
      pipe = captr->tag_pipe[L2RequestTAGPIPE];

      for (ctr = 0; ctr < pipe->ports; ctr++)
	{
	  GetPipeElt(req, pipe);
	  if (req == 0)
	    break;

          result = L2ProcessTagRequest(captr, req);
          if (result == 0)  /* ?? */
	    break;

	  if (result == 1)
	    {
	      ClearPipeElt(pipe);
	      captr->num_in_pipes--;
	    }
	  else if (result == -1)
	    {
	      SetPipeElt(req->invl_req, pipe);
	      req->invl_req = 0;
	    }
	}
    }
}





/*===========================================================================
 * L2ProcessDataReq: simulates accesses to the L2 data SRAM array,
 * Returns 0 on failure; 1 on success. No equivalent function in L1.
 */

int L2ProcessDataReq(CACHE *captr, REQ * req)
{
  switch (req->type)
    {
    case REQUEST:
      /*
       * A cache hit that gets sent back to the L1. Change it to REPLY.
       */
      if (req->req_type == WBACK)
	{
	  YS__PoolReturnObj(&YS__ReqPool, req);
	  return 1;
	}

      if (req->progress == 0)
	{
	  req->progress = 1;
	  req->type     = REPLY;
	}
      
      /*
       * Try to send it up to L1 cache. Cache_add_repl_to_l1 returns 1 on
       * success, and 0 on failure. This request will be awoken in REPLY case,
       * which just seeks to send up access also.
       */
      return Cache_add_repl_to_l1(captr, req);

      
    case REPLY:
      if (req->l2mshr == 0)
	{
	  /*
	   * It's a stalled L2 hit (came from REQUEST).
	   */
	  return Cache_add_repl_to_l1(captr, req);
	}
      else
	{
	  MSHR *pmshr = req->l2mshr;
	  REQ  *tmpreq;

	  /*
	   * It was an L2 miss. We have to release all the coalesced requests
	   * one by one; and the flush/purge request if exists.
	   */
	  if (Cache_add_repl_to_l1(captr, req) == 0)
	    return 0;

	  pmshr->next_move++;

	  for (; pmshr->next_move < pmshr->counter; pmshr->next_move++)
	    {
	      tmpreq = pmshr->coal_req[pmshr->next_move];
	      if (Cache_add_repl_to_l1(captr, tmpreq) == 0)
		{
		  tmpreq->l2mshr = pmshr;
		  req->invl_req = tmpreq;
		  return -1;
		}
	    }
	  
	  /*
	   * Take care of pending flush/purge requests. Generate a write-back
	   * if flush hits a private dirty line.
	   */
	  tmpreq = NULL;
	  if (pmshr->pend_opers)
	    {
	      cline_t *cline = pmshr->cline;
	      if (pmshr->pend_opers == FLUSHC && cline->state == PR_DY)
		{
		  tmpreq = Cache_make_req(captr, cline, REPLY_EXCL);
		  tmpreq->type   = WRITEBACK;
		  tmpreq->parent = 0; /* not associated with any cohe */
		}
	      
	      if (cline->state != INVALID)
		{
		  cline->state = INVALID;
		  cline->tag = -1;
		  if (cline->pref == 2)
		    captr->pstats->sl2.useless++;
		  else if (cline->pref == 8)
		    captr->pstats->l2p.useless++;
		}
	    }
	  
          /*
	   * All coalesced requests either have been sent back to L1 cache, or
	   * have performed GlobalPerform. Release the MSHR now.
	   */
	  Cache_free_mshr(captr, pmshr);

	  if (tmpreq)
	    {
	      if (!lqueue_full(&(captr->outbuffer)) ||
		  !captr->pending_writeback)
		{
		  Cache_start_send_to_bus(captr, tmpreq);
		  return 1;
		}
	      else
		{
		  req->invl_req = tmpreq;
		  return -1;
		}
	    }
	}
      return 1;

      
    case COHE:
      /*
       * This is a cache-to-cache copy. Only one cache-to-cache copy
       * is allowed at one time.
       */
      if (captr->cohe_reply)
	return 0;
      else
	{
	  req->type = COHE_REPLY;
	  Cache_start_send_to_bus(captr, req);
	  return 1;
	}


    case WRITEBACK:
      /*
       * If the outgoing buffer is full (indicated by pending_writeback),
       * stall until there is a free slot in outgoing buffer.
       */
      if (captr->pending_writeback)
	return 0;
      else
	{
	  Cache_start_send_to_bus(captr, req);
	  return 1;
	}

    case COHE_REPLY:
      YS__errmsg(captr->nodeid, "L2 cache should never recieve COHE_REPLY.");

    default:
      break;
    }

  YS__errmsg(captr->nodeid, "Invalid case in L2ProcessDataReq()");
  return 0;
}



/*===========================================================================
 * L2ProcessTagRequest: simulates access requests to the L2 cache tag array.
 *   progress = 0: initial state
 *   progress = 1: hit,  send it to data array
 *   progress = 2: miss, send it down to memory
 * Returns 0 on failure; 1 on success.
 */

static int L2ProcessTagRequest(CACHE * captr, REQ * req)
{
  MSHR_Response  mshr_status;
  cline_t       *cline;

  if (cparam.L2_perfect)
    req->progress = 1;

  if (req->req_type == WBACK)
    {
      if (AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req))
	{
	  captr->num_in_pipes++;
	  req->progress = 0;
	  return 1;
	}
      else
	return 0;
    }

  
  if (req->prcr_req_type == FLUSHC || req->prcr_req_type == PURGEC)
    return L2ProcessFlushPurge(captr, req);

  
  if (req->progress == 0)
    {
      Cache_search(captr, req->vaddr, req->paddr, &cline);
      mshr_status = L2Cache_check_mshr(captr, req);

      switch (mshr_status)
	{
	case NOMSHR:
	  if (cline->state == PR_CL || cline->state == PR_DY)
	    req->req_type = REPLY_EXCL;
	  else
	    req->req_type = REPLY_SH;

	  if (req->prefetch == 2)
	    captr->pstats->sl2.unnecessary++;
	  else if (req->prefetch == 8)
	    captr->pstats->l2p.unnecessary++;

	  req->progress = 1;
	  break;

	case MSHR_COAL:
	  if (req->prefetch == 2)
	    captr->pstats->sl2.unnecessary++;

	  return 1;   /* success if coalesced */

	case MSHR_NEW:
	  if (req->prefetch == 2)
	    captr->pstats->sl2.issued++;
	  else if (req->prefetch == 8)
	    captr->pstats->l2p.issued++;

	  if (cparam.L2_prefetch && req->prefetch == 0)
            L2Cache_start_prefetch(captr, req);

	  req->progress = 2;  /* this indicates that cache need to send
				something out. */
	  break;

	case MSHR_USELESS_FETCH:
	  /*
	   * a prefetch to a line that already has an outstanding fetch.
	   * Drop this prefetch even without discriminate prefetch on.
	   */
	  if (req->prefetch == 2)
	    captr->pstats->sl2.unnecessary++;
	  else if (req->prefetch == 8)
	    captr->pstats->l2p.unnecessary++;

	  YS__PoolReturnObj(&YS__ReqPool, req);
	  return 1;

	case MSHR_STALL_WAR:
	case MSHR_STALL_COHE:
	case MSHR_STALL_COAL:
	case MSHR_STALL_RELEASE:
	case NOMSHR_STALL:
	case NOMSHR_STALL_COHE:
	case MSHR_STALL_FLUSH:
	  return 0;

	default:
	  YS__errmsg(captr->nodeid,
		     "Default case in L2 mshr_misscache %d", mshr_status);
	}
    }

  /*
   * Going to Data RAM in this case. note that this isn't considered
   * progress because no resource has been booked, nor has any visible
   * state been changed (although LRU bits may have been).  NOTE: hits
   * must be sent to the data array before being sent to L1 cache
   */

  if (req->progress == 1)
    {
      if (AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req))
	{
	  captr->num_in_pipes++;
	  req->progress = 0;
	  if ((req->hit_type != L1IHIT) && (req->hit_type != L1DHIT))
	    req->hit_type = L2HIT;
	  return 1;
	}
      else
	return 0;
    }

  return Cache_start_send_to_bus(captr, req);
}



/*===========================================================================
 * Simulate cache flush/purge operations. Unlike write-through L1 cache,
 * L2 cache may generate write-back transactions for flush. Since L1
 * has been checked before this function is called, we don't need
 * invalidation requests to L1 cache.
 */

static int L2ProcessFlushPurge(CACHE *captr, REQ *req)
{
  int      i;
  cline_t *cline;
  REQ     *ireq = NULL;

  if (cparam.L2_perfect)
    return 1;

  if (req->progress == 0)
    {
      /*
       * First, tries to merge with MSHR. Then, search the cache.
       */
      req->wrb_req = 0;
      req->invl_req = 0;

      if (captr->mshr_count > 0)
	{
	  unsigned blocknum = ADDR2BNUM2(req->paddr);
	  for (i = 0; i < captr->max_mshrs; i++)
	    {
	      if (captr->mshrs[i].valid &&
		  blocknum == captr->mshrs[i].blocknum)
		{
		  captr->mshrs[i].pend_opers = req->prcr_req_type;
		  req->hit_type = MEMHIT;
		  Cache_global_perform(captr, req, 1);
		  return 1;
		}
	    }
	}

      if (Cache_search(captr, req->vaddr, req->paddr, &cline) == 0)
	{
	  if (cline->state == PR_DY && req->prcr_req_type == FLUSHC)
	    {
	      /*
	       * Need write back when flushing dirty private data.
	       */
	      req->wrb_req = Cache_make_req(captr, cline, REPLY_EXCL);
	      req->wrb_req->type   = WRITEBACK;
	      req->wrb_req->parent = 0; /* not associated with any cohe */
	    }

	  if (req->prcr_req_type == FLUSHC && 
	      cparam.L1D_writeback)
	    {
	      req->invl_req = Cache_make_req(captr, cline, INVALIDATE);
	      req->invl_req->prcr_req_type = FLUSHC;
	      req->invl_req->type          = COHE;
	      req->invl_req->l2mshr        = (MSHR *)cline;
	      req->invl_req->wrb_req       = req->wrb_req;
	      req->invl_req->wb_count      = 0;
	      req->invl_req->parent        = req;
	    }
	  
	  ireq = Cache_make_req(captr, cline, INVALIDATE);
	  ireq->type = COHE;
	  ireq->parent = req;

	  if (cline->pref == 2)
	    captr->pstats->sl2.useless++;
	  else if (cline->pref == 8)
	    captr->pstats->l2p.useless++;

	  cline->state = INVALID;
	  cline->tag = -1;
	  req->hit_type = L2HIT;
	}
      else
	{
	  req->hit_type = MEMHIT;
	}

      req->progress = 1;
    }


  if (((req->invl_req) && (lqueue_full(&(captr->l1d_partner->cohe_queue)))) ||
      ((ireq) && (lqueue_full(&(captr->l1i_partner->cohe_queue)))))
    {
      if (ireq)
	YS__PoolReturnObj(&YS__ReqPool, ireq);

      return 0;
    }

  if (ireq)
    {
      lqueue_add(&(captr->l1i_partner->cohe_queue), ireq, captr->nodeid);
      captr->l1i_partner->inq_empty = 0;
    }

  if (req->invl_req)
    {
      lqueue_add(&(captr->l1d_partner->cohe_queue), req->invl_req,
		 captr->nodeid);
      captr->l1d_partner->inq_empty = 0;
      return 1;
    }

  if (req->wrb_req)
    {
      if (!AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req->wrb_req))
	return 0;
      
      req->wrb_req->progress = 0;
      captr->num_in_pipes++;
    }

  Cache_global_perform(captr, req, 1);
  return 1;
}



int L2ProcessTagFlushReply(CACHE *captr, REQ *req)
{
  if (req->wrb_req == 0 && req->wb_count)
    {
      req->wrb_req = (REQ *) YS__PoolGetObj(&YS__ReqPool);
      req->wrb_req->paddr     = req->parent->paddr & block_mask2;
      req->wrb_req->vaddr     = req->parent->vaddr & block_mask2;
      req->wrb_req->req_type  = REPLY_EXCL;
      req->wrb_req->type      = WRITEBACK;
      req->wrb_req->node      = captr->nodeid;
      req->wrb_req->src_proc  = captr->procid;
      req->wrb_req->ifetch    = 0;
      req->wrb_req->dest_proc = AddrMap_lookup(req->node, req->paddr);
      req->wrb_req->parent    = 0;
      req->memattributes      = req->memattributes;
    }
 
  if (req->wrb_req)
    {
      if (!AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req->wrb_req))
	return 0;
      req->wrb_req->progress = 0;
      captr->num_in_pipes++;
    }

  Cache_global_perform(captr, req->parent, 1);
  if (!req->cohe_count)
    YS__PoolReturnObj(&YS__ReqPool, req);

  return 1;
}



/*===========================================================================
 * L2Cache_check_mshr: This function is called for all incoming REQUESTs,
 * to check whether the incoming transaction matches an outstanding
 * transaction in an MSHR, whether it hits in the cache, and whether it
 * needs an MSHR).
 */

static MSHR_Response L2Cache_check_mshr(CACHE * captr, REQ * req)
{
  MSHR_Response  response;
  MSHR          *pmshr;
  cline_t       *cline;
  int            misscache, i;

  /*
   * First determines whether the incoming transaction matches any MSHR.
   */
  pmshr = 0;
  if (captr->mshr_count > 0)
    {
      unsigned blocknum = ADDR2BNUM2(req->paddr);
      for (i = 0; i < captr->max_mshrs; i++)
	{
	  if (captr->mshrs[i].valid && blocknum == captr->mshrs[i].blocknum)
	    {
	      pmshr = &(captr->mshrs[i]);
	      break;
	    }
	}
    }

  /*
   * Determine if the desired line is available in the cache.
   */
  misscache = Cache_search(captr, req->vaddr, req->paddr, &cline);

  /*
   * If it's an L2 prefetch, we will drop it no matter what as long as it
   * matches an MSHR or a cache line.
   */
  if ((req->prefetch == 8) && (pmshr || misscache == 0))
    return MSHR_USELESS_FETCH;

  if (pmshr == 0)
    {  /* the line is not present in any MSHR */
      if (misscache == 0)
	{ /* the line is in cache */
	  if (Cache_hit_update(captr, cline, req))
	    { /* update LRU ages */
	      if (cparam.L2_prefetch & 2)
		L2Cache_start_prefetch(captr, req);
	    }

	  if (req->prcr_req_type == READ)
            return NOMSHR;
	  else
	    { /* it's a write or rmw */
	      if (cline->state != SH_CL)
		{
		  /*
		   * The access is either a wirte or a rmw; and the cache line
		   * is in private state.
		   */
		  if (cline->state == PR_CL)
		    cline->state = PR_DY;
		  return NOMSHR;
		}
	      /* Otherwise (share state), we will need upgrade. */
	    }
	}

      /* The line is either not in l2 cache; or the request is a write and
       * the line is in l2 cache but with share state. */

      if (captr->mshr_count == captr->max_mshrs)
	{
	  /*
	   * None are available, the value "NOMSHR_STALL" is returned.
	   */
	  captr->pstats->l2stall.full++;
	  return NOMSHR_STALL;
	}

      /*
       * Find the first free MSHR
       */
      for (i = 0; i < captr->max_mshrs; i++)
	{
	  if (captr->mshrs[i].valid != 1)
	    {
	      pmshr = &(captr->mshrs[i]);
	      break;
	    }
	}

      req->l2mshr         = pmshr;
      pmshr->valid        = 1;
      pmshr->mainreq      = req;
      pmshr->setnum       = cline->index >> captr->idx_shift;
      pmshr->blocknum     = req->paddr >> captr->block_shift;
      pmshr->counter      = 0;
      pmshr->pending_cohe = 0;
      pmshr->stall_WAR    = 0;
      pmshr->demand       = -1.0;
      pmshr->cline        = cline;
      pmshr->misscache    = misscache;
      pmshr->pend_opers   = 0;
      pmshr->only_prefs   = (req->prefetch & 10) ? 1 : 0;
      pmshr->has_writes   = (req->prcr_req_type == READ) ? 0 : 1;
      pmshr->releasing    = 0;

      captr->mshr_count++;
      captr->reqmshr_count++;
      captr->reqs_at_mshr_count++;
      
      if (misscache == 0)
	{ /* line present in cache -- upgrade-type access */
	  /*
	   * In the case of upgrades, the line is locked into cache by
	   * setting "mshr_out"; this guarantees that the line is not
	   * victimized on a later "REPLY" before the upgrade reply returns.
	   * In all cases where the line is present in cache, the
	   * "hit_update" function is called to update the ages of the lines
	   * in the set (for LRU replacement).
	   */
	  req->req_type = UPGRADE;
	  cline->mshr_out = 1;
	}
      else
	{
	  if ((req->prcr_req_type == WRITE) || (req->prcr_req_type == RMW))
            req->req_type = READ_OWN;
	  else
            req->req_type = READ_SH;
	}

      return MSHR_NEW;
    }

  /* Matches in MSHR -- REQUEST must either be merged, dropped, forwarded
     around cache, or stalled. */

  /*
   * If there is pending flush/purge operation, or pending coherence
   * request, or the MSHR is being released, stall.
   */
  if (pmshr->pend_opers)
    {
      captr->pstats->l2stall.flush++;
      return MSHR_STALL_FLUSH;
    }
  
  if (pmshr->releasing)
    {
      captr->pstats->l2stall.release++;
      return MSHR_STALL_RELEASE;
    }

  /*
   * Now we need to consider the possibility of a "WAR" stall. This is a
   * case where an MSHR has an exclusive-mode request wants to merge with a
   * shared-mode MSHR.  Even if this is a read request to an MSHR with an
   * outstanding WAR, this request should be stalled, as otherwise the read
   * would be processed out-of-order with respect to the stalled write
   */
  if (pmshr->stall_WAR)
    {
      captr->pstats->l2stall.war++;
      return MSHR_STALL_WAR;
    }

  if ((req->prcr_req_type != READ) &&
      (pmshr->mainreq->prcr_req_type == READ))
    {
      captr->pstats->l2stall.war++;
      pmshr->stall_WAR = 1;
      return MSHR_STALL_WAR;
    }

  /*
   * Too many requests coalesced with MSHR
   */
  if (pmshr->counter == MAX_COALS-1)
    {
      captr->pstats->l2stall.coal++;
      return MSHR_STALL_COAL;
    }

  /*
   * No problems with coalescing the request, so coalesce it
   */
  pmshr->coal_req[pmshr->counter++] = req;
  if (req->req_type == WRITE || req->req_type == RMW)
    pmshr->has_writes = 1;
  else
    pmshr->has_writes = 0;

  if (pmshr->only_prefs && !(req->prefetch & 10))
    {
      /*
       * Hit a late prefetch.
       */
      if (pmshr->mainreq->prefetch == 2)
	{
	  captr->pstats->sl2.late++;
	  captr->pstats->sl2.useful++;
	}
      else if (pmshr->mainreq->prefetch == 8)
	{
	  captr->pstats->l2p.late++;
	  captr->pstats->l2p.useful++;
	}
      
      pmshr->demand = YS__Simtime; /* use this to compute lateness factor */
      pmshr->only_prefs = 0;
    }
  
  return MSHR_COAL;
}



/*===========================================================================
 * Simulates access replies to the L2 cache tag array.
 *  progress = 0: initial state
 *  progress = 1: reply has been processed, but hasn't sent anything out.
 *  progress = 2: the invalidation request has been sent up, if any.
 *  progress = 3: the write-back request has been sent down, if any.
 * Returns 0 on failure; 1 on success.
 */

static int L2ProcessTagReply(CACHE * captr, REQ * req)
{
  int        misscache, i;
  MISS_TYPE  ccdres;
  cline_t   *cline;
  MSHR      *pmshr;
  REQ       *ireq;

  if (req->type == WRITEBACK)
    {
      if (captr->pending_writeback)
	return 0;
      else
	{
	  Cache_start_send_to_bus(captr, req);
	  return 1;
	}
    }

  if (captr->stall_reply)
    return 0;

  if (req->progress == 0)
    {
      pmshr = req->l2mshr;
      if (pmshr == 0)
	YS__errmsg(captr->nodeid,
		   "L2 Cache %i received a reply (0x%p 0x%08X) for a nonexistent MSHR\n",
		   captr->procid, req, req->paddr);

      /*
       * Collect statistics about late prefetch.
       */
      if (pmshr->demand > 0)
	{
	  if (pmshr->mainreq->prefetch == 2)
            captr->pstats->sl2.lateness += YS__Simtime - pmshr->demand;
	  else if (pmshr->mainreq->prefetch == 8)
            captr->pstats->l2p.lateness += YS__Simtime - pmshr->demand;
	  pmshr->demand = -1;
	}

      /*
       * Does the MSHR have a coherence message merged into it? This happens
       * when MSHR receives a certain type of COHE while outstanding. In
       * this case, the line must transition to a different state than the
       * REPLY itself would indicate. Note: such merging is only allowed if
       * the merge will not require a copyback of any sort. And are there
       * any writes with this MSHR? If so, make sure to transition to
       * dirty....
       */
      cline         = pmshr->cline;
      misscache     = pmshr->misscache;
      req->invl_req = NULL;
      req->wrb_req  = NULL;

      if (misscache == 0)
	{ /* this was for an upgrade */
	  cline->state    = PR_DY;
	  cline->mshr_out = 0;
	}
      else
	{
	  if (misscache == 1)
	    { /* present miss */
	      Cache_pmiss_update(captr, req, cline->index, pmshr->only_prefs);
	      req->miss_type2 = CACHE_MISS_COHE;
	    }
	  else
	    { /* misscache == 2 : total miss */
	      /*
	       * Finds a victim and updates ages. If no places are available,
	       * wait.
	       */
	      if (!Cache_miss_update(captr, req, &cline, pmshr->only_prefs))
		{
		  YS__warnmsg(captr->nodeid,
			      "No replace for L2 cache_miss, possible deadlock");
		  return 0;
		}

	      /*
	       * capacity/conflict miss detector
	       */
	      ccdres = CCD_InsertNewLine(captr->ccd, ADDR2BNUM2(req->paddr));
	      req->miss_type2 = req->line_cold ? CACHE_MISS_COLD : ccdres;
	      pmshr->cline = cline;
	    }

	  if (cline->state != INVALID)
	    {
	      /*
	       * In this case, some sort of replacement is needed. Gather all
	       * information about line being replaced
	       */
	      if (cline->state == SH_CL)
		captr->pstats->shcl_victims2++;
	      else if (cline->state == PR_CL)
		captr->pstats->prcl_victims2++;
	      else if (cline->state == PR_DY)
		captr->pstats->prdy_victims2++;

	      if (cline->state == PR_DY)
		{
		  /*
		   * Need to send the private dirty line back to main memory.
		   */
		  req->wrb_req = Cache_make_req(captr, cline, REPLY_EXCL);
		  req->wrb_req->type   = WRITEBACK;
		  req->wrb_req->parent = 0; /* not associated with any cohe*/
		}

	      /*
	       * Also an invalidation message is sent up to the L1 caches.
	       */

              req->invl_req = Cache_make_req(captr, cline, INVALIDATE);
	      req->invl_req->type = COHE;
	      req->invl_req->parent = req;

	      ireq = Cache_make_req(captr, cline, INVALIDATE);
	      ireq->type = COHE;
	      ireq->parent = req;
	    }

	  /*
	   * Set tags and states to reflect new line
	   */
	  cline->tag   = req->paddr >> captr->tag_shift;
	  cline->vaddr = req->vaddr & block_mask2;
	  if (pmshr->has_writes)
	    cline->state = PR_DY;
	  else
	    cline->state = (req->req_type == REPLY_SH) ? SH_CL : PR_CL;
	}
      req->progress = 1;
    }

  if (req->progress == 1)
    {
      if (req->invl_req)
	{
	  if ((lqueue_full(&(captr->l1i_partner->cohe_queue))) ||
	      (lqueue_full(&(captr->l1d_partner->cohe_queue))))
	    return 0;
	  else
	    {
	      ireq->prcr_req_type = 0;
	      ireq->wb_count = 0;
	      lqueue_add(&(captr->l1i_partner->cohe_queue), ireq,
			 captr->nodeid);
	      captr->l1i_partner->inq_empty = 0;

	      req->invl_req->prcr_req_type = 0;
	      req->invl_req->wb_count = 0;
	      lqueue_add(&(captr->l1d_partner->cohe_queue),
			 req->invl_req, captr->nodeid);
	      captr->l1d_partner->inq_empty = 0;
	    }
	}

      if (cparam.L1D_writeback && req->invl_req)
         req->progress = 2;
      else
         req->progress = 3;
    }

  if (req->progress == 2)
    return 0;
  
  if (req->progress == 3)
    {
      /*
       * ready to send out a WRB to Data pipe or a REPL down, if any
       */
      if (req->wrb_req)
	{
	  if (AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req->wrb_req))
	    {
	      req->wrb_req->progress = 0;
	      captr->num_in_pipes++;
	    }
	  else
	    return 0;
	}
      req->progress = 4;     /* ready to send REPLY to data pipe */
    }

  if (AddToPipe(captr->data_pipe[L2ReqDATAPIPE], req))
    {
      captr->num_in_pipes++;
      req->progress = 0;

      /*
       * Prepare release the request one by one, starting from the mainreq
       * (indicated by "next_move = -1". All the requests share the reply
       * type (REPLY_SH, REPLY_PR, or UPGRADE).
       */
      pmshr = req->l2mshr;
      pmshr->next_move = -1;
      pmshr->releasing = 1;
      for (i = 0; i < pmshr->counter; i++)
	{
	  REQ *tmpreq = pmshr->coal_req[i];
	  if ((tmpreq->hit_type != L1IHIT) && (tmpreq->hit_type != L1DHIT))
            tmpreq->hit_type = L2HIT;
	  tmpreq->req_type  = req->req_type;
	  tmpreq->line_cold = 0;
	}
      return 1;
    }

  /* couldn't go to Data pipe */
  return 0;
}



int L2ProcessTagInvlReply(CACHE *captr, REQ *rtn_req)
{
  REQ *req = rtn_req->parent;
 
  if (rtn_req->wb_count && req->wrb_req == 0)
    {
      req->wrb_req = Cache_make_req(captr, req->l2mshr->cline, REPLY_EXCL);
      req->wrb_req->type   = WRITEBACK;
      req->wrb_req->parent = 0; /* not associated with any cohe */
    }
  
  if (req)
    req->progress = 3;
 
  if (!req->cohe_count)
    YS__PoolReturnObj(&YS__ReqPool, rtn_req);

  return 1;
}



/*===========================================================================
 * Uncoalesce each request coalesced in the specified MSHR.
 */
static int L2Cache_uncoalesce_mshr(CACHE *captr, MSHR *pmshr)
{
  HIT_TYPE  hit_type = pmshr->mainreq->hit_type;
  int       latepf   = (pmshr->demand > 0.0);
  int       i;
  REQ       *tmpreq;

  /*
   * If this MSHR had a late prefetch, set prefetched_late field to allow
   * system to count every access coalesced into the MSHR as part of
   * "late PF" time in statistics. Is this reasonable?
   */
  pmshr->mainreq->prefetched_late = latepf;

  if (pmshr->mainreq->prefetch == 8)
    {
      if (!pmshr->mainreq->cohe_count)
	YS__PoolReturnObj(&YS__ReqPool, pmshr->mainreq);
    }
  else
    Cache_global_perform(captr, pmshr->mainreq, 1);
  
  for (i = 0; i < pmshr->counter; i++)
    {
      REQ *req = pmshr->coal_req[i];
      /*
       * In addition to latepf, also set hit_type of each coalesced REQUEST
       * to be L1HIT. This allows all of these times to be counted as the
       * appropriate component of execution time.
       */
      if (req->ifetch)
	req->hit_type = L1IHIT;
      else
	req->hit_type = L1DHIT;
      req->prefetched_late = latepf;

      if (req->prefetch == 8)
	YS__errmsg(captr->nodeid,
		   "L1Cache_uncoalesce_msrh: coalesced L1 prefetch");

      Cache_global_perform(captr, req, 1);
    }

  /*
   * Take care of pending flush/purge requests. Generate a write-back
   * if flush hits a private dirty line.
   */
  tmpreq = NULL;
  if (pmshr->pend_opers)
    {
      cline_t *cline = pmshr->cline;
      if (pmshr->pend_opers == FLUSHC && cline->state == PR_DY)
	{
	  tmpreq = Cache_make_req(captr, cline, REPLY_EXCL);
	  tmpreq->type   = WRITEBACK;
	  tmpreq->parent = 0; /* not associated with any cohe */
	}
      
      if (cline->state != INVALID)
	{
	  cline->state = INVALID;
	  cline->tag = -1;
	  if (cline->pref == 2)
	    captr->pstats->sl2.useless++;
	  else if (cline->pref == 8)
	    captr->pstats->l2p.useless++;
	}
    }

  Cache_free_mshr(captr, pmshr);

  if (tmpreq)
    {
      if (lqueue_full(&(captr->outbuffer)) && captr->pending_writeback)
	{
	  pmshr->mainreq->invl_req = tmpreq;
	  return -1;
	}
      else
	Cache_start_send_to_bus(captr, tmpreq);
    }

  return 1;
}



/*===========================================================================
 * L2ProcessTagReq: simulates accesses to the L2 cache tag array.
 * Its behavior depends on the type of message (i.e. type) received
 * This is very similar to L1ProcessTagReq, except where noted.
 * Returns 0 on failure; 1 on success.
 */
int L2ProcessTagCohePipe(CACHE *captr, REQ *req)
{
   switch (req->type)
     {
     case COHE:
     case REQUEST:
       return L2ProcessTagCohe(captr, req);
 
     case COHE_REPLY:
       if (req->prcr_req_type == FLUSHC)
         return L2ProcessTagFlushReply(captr, req);
       else if (captr->procid == req->src_proc)
         return L2ProcessTagInvlReply(captr, req);
       else
         return L2ProcessTagCoheReply(captr, req);
     }

   YS__errmsg(captr->nodeid, "L2TagProcessCohePipe: req not handled\n");
   return 0;
}
 



int L2ProcessL1Reply(CACHE *captr, REQ *req)
{
  if (req->prcr_req_type == FLUSHC)
    return L2ProcessTagFlushReply(captr, req);
  else if (captr->procid == req->src_proc)
    return L2ProcessTagInvlReply(captr, req);
  else
    return L2ProcessTagCoheReply(captr, req);
}
 


int L2ProcessTagCoheReply(CACHE *captr, REQ *req)
{
  ReqType finalstate;

  if (req->parent->prcr_req_type == READ)
    finalstate = REPLY_SH;
  else
    finalstate = REPLY_EXCL;

  /*
   * Tell MMC the coherence results. Note: do this before sending
   * cache-to-cache copy.
   */

  if ((req->type == COHE) && (req->wb_count || req->wrb_req))
    {
      REQ *blw_req = Cache_make_wb_req(captr, req->parent, finalstate);

      /*
       * cache-to-cache transaction needs to access the data SRAM array.
       */
      if (!AddToPipe(captr->data_pipe[L2ReqDATAPIPE], blw_req))
	{
	  if (captr->data_cohe_pending)
            YS__errmsg(captr->nodeid,
		       "Something is wrong: two data_cohe_pendings.");

	  captr->data_cohe_pending = blw_req;
	  captr->inq_empty = 0;
	}
      else
	captr->num_in_pipes++;
    }

  Bus_send_cohe_response(req->parent, finalstate);

  YS__PoolReturnObj(&YS__ReqPool, req);
  return 1;
}




/*===========================================================================
 * L2ProcessTagCohe: simulates coherence accesses to the L2 cache tag array.
 * Returns 0 on failure; 1 on success.
 */

static int L2ProcessTagCohe(CACHE * captr, REQ *req)
{
  int           i, misscache;
  cline_state_t oldstate;
  MSHR          *pmshr;
  cline_t       *cline;
  REQ           *blw_req, *abvi_req, *abvd_req;

  if (cparam.L2_perfect)
    {
      Bus_send_cohe_response(req, REPLY_EXCL);
      return 1;
    }

  if (captr->stall_cohe || captr->cohe_reply)
    return 0;

  /*
   * Was the request initiated by this processor? If yes, there is
   * nothing to do here, except setting pending_cohe. To put own requests
   * into coherence waiting queue just to keep the order of coherence
   * requests.
   */

  if (req->src_proc == captr->procid)
    {
      req->issued = 1;

      if (req->l2mshr->pending_cohe)
	return 0;
      else
	{
	  if (req->progress == 0)
	    {
	      req->l2mshr->pending_cohe = req;
	      req->stalled_mshrs[captr->procid] = req->l2mshr;
	    }

	  Bus_send_cohe_response(req, REPLY_EXCL);

	  return 1;
	}
    }


  /*
   * First, check if the line is in the L1 write back buffer. If yes,
   * stall the cohe until the write has been moved to L2 cache.
   */
   if (!cparam.L1D_writeback &&
       L1DCache_wbuffer_search(PID2WBUF(0, captr->procid), req, 1) != 0)
    {
      captr->stall_cohe++;
      return 0;
    }


  /*
   * Then, check if the line is in the outgoing buffer. If yes,
   * convert the write request to coherency data response.
   */
  if (Cache_check_outbuffer(captr, req) != NULL)
    return 0;

  
  /*
   * Determine if there is a matched MSHR.  Note: only one coherence
   * request -- the first one -- is allowed to overpass any coalesced
   * requests of an MSHR. Too strict?
   */
  pmshr = 0;

  if (captr->mshr_count > 0)
    {
      unsigned blocknum = ADDR2BNUM2(req->paddr);
      for (i = 0; i < captr->max_mshrs; i++)
	{
	  if ((captr->mshrs[i].valid) &&
	      (blocknum == captr->mshrs[i].blocknum))
	    {
	      pmshr = &(captr->mshrs[i]);
	      break;
	    }
	}
    }

  if ((pmshr != 0) && (req->progress == 0))
    {  
      if ((!pmshr->pending_cohe))
	{
	  pmshr->pending_cohe = req;
	  req->stalled_mshrs[captr->procid] = pmshr;
	}

      if (pmshr->mainreq->issued)
	return(0);
    }


  /*
   * Calls Cache_search to find out if the line is present.
   * If the line isn't present, just send the cohe response.
   * If the line is present, inform L1 caches if the line state is changed;
   * and generate a cache-to-cache copy if the line is in dirty state.
   */
  misscache = Cache_search(captr, req->vaddr, req->paddr, &cline);
  if (misscache)
    { /* missing in L2 cache implies missing in L1 cache. */
      Bus_send_cohe_response(req, REPLY_EXCL);
      return 1;
    }

  oldstate = cline->state;
  if ((req->type == REQUEST) && (req->req_type == READ_SH))
    cline->state = SH_CL;

  if (((req->type == REQUEST) &&
       ((req->req_type == READ_OWN) || req->req_type == UPGRADE)) ||
      (req->type == WRITEPURGE))
    {
      cline->state = INVALID;
      if (cline->pref == 2) 
	captr->pstats->sl2.useless++;
      else if (cline->pref == 8) 
	captr->pstats->l2p.useless++;
    }

  blw_req = NULL;
  abvi_req = NULL;
  abvd_req = NULL;

  if (cparam.L1D_writeback)
    {
      abvi_req = Cache_make_cohe_to_l1(captr, req, cline->state);
      abvi_req->parent = req;
      abvi_req->wrb_req = (REQ*)(oldstate == PR_DY);

      abvd_req = Cache_make_cohe_to_l1(captr, req, cline->state);
      abvd_req->parent = req;
      abvd_req->wrb_req = (REQ*)(oldstate == PR_DY);      
    }
  else
    {
      if ((oldstate == PR_DY) &&
	  (req->type == REQUEST) &&
	  ((req->req_type == READ_OWN) ||
	   (req->req_type == READ_SH) ||
	   (req->req_type == READ_CURRENT) ||
	   (req->req_type == UPGRADE)))
	blw_req = Cache_make_wb_req(captr, req, (cline->state == SH_CL) ?
				    REPLY_SH : REPLY_EXCL);

      if (cline->state != oldstate)
	{
	  abvi_req = Cache_make_cohe_to_l1(captr, req, cline->state);
	  abvi_req->parent = req;

	  abvd_req = Cache_make_cohe_to_l1(captr, req, cline->state);
	  abvd_req->parent = req;
	} 
    }


  /*
   * L1 cohe_queue is as big as L2 reply queue and L1 is faster than L2,
   * so, theorertically, L1 cohe_queue will never overflow.
   */
  if (abvd_req)
    {
      if ((lqueue_full(&(captr->l1i_partner->cohe_queue))) ||
	  (lqueue_full(&(captr->l1d_partner->cohe_queue))))
	YS__errmsg(captr->nodeid, "Somehow, the L1 cohe_queue is full");

      abvi_req->wb_count = 0;
      abvi_req->prcr_req_type = 0;

      abvd_req->wb_count = 0;
      abvd_req->prcr_req_type = 0;

      lqueue_add(&(captr->l1i_partner->cohe_queue), abvi_req, captr->nodeid);
      captr->l1i_partner->inq_empty = 0;

      lqueue_add(&(captr->l1d_partner->cohe_queue), abvd_req, captr->nodeid);
      captr->l1d_partner->inq_empty = 0;
    }


  /*
   * For write-back L1 cache, the cohe responese will be sent during
   * CoheReply
   */
  if (cparam.L1D_writeback)
    return 1;


  /*
   * Tell MMC the coherence results. Note: do this before sending
   * cache-to-cache copy.
   */
  if (req->req_type == READ_CURRENT)
    Bus_send_cohe_response(req, REPLY_EXCL);
  else if (cline->state == SH_CL)
    Bus_send_cohe_response(req, REPLY_SH);
  else if (cline->state == INVALID) /* && (req->type != WRITEPURGE)) */
    Bus_send_cohe_response(req, REPLY_EXCL);

  /*
   * cache-to-cache transaction needs to access the data SRAM array.
   */
  if (blw_req)
    {

      if (!AddToPipe(captr->data_pipe[L2ReqDATAPIPE], blw_req))
	{
	  if (captr->data_cohe_pending)
            YS__errmsg(captr->nodeid,
		       "Something is wrong: two data_cohe_pendings at L2 Cache #%i.",
		       captr->gid);
	  captr->data_cohe_pending = blw_req;
	  captr->inq_empty = 0;
	}
      else
	captr->num_in_pipes++;
    }

  return 1;
}



/*===========================================================================
 * Make a cache-to-cache transfer reply.
 */

static REQ *Cache_make_wb_req(CACHE *captr, REQ *req, ReqType req_type)
{
  REQ *tmpreq;

  req->c2c_copy = 1;

  tmpreq = (REQ *) YS__PoolGetObj(&YS__ReqPool);
  tmpreq->node          = captr->nodeid;
  tmpreq->src_proc      = captr->procid;
  tmpreq->dest_proc     = req->src_proc;
  tmpreq->memattributes = req->memattributes;
  tmpreq->vaddr         = req->vaddr;
  tmpreq->paddr         = req->paddr;
  tmpreq->ifetch        = 0;
  tmpreq->parent        = req;
  tmpreq->req_type      = req_type;
  tmpreq->type          = COHE; /* This will be changed to COHE_REPLY by
			           L2ProcessDataReq */

  return tmpreq;
}



/*===========================================================================
 * Make a cohe-request sent to L1 cache.
 */

static REQ *Cache_make_cohe_to_l1(CACHE *captr, REQ *req, int state)
{
  REQ *tmpreq;
  /* @@@ fix @@@ */
  tmpreq = (REQ *) YS__PoolGetObj(&YS__ReqPool);
  tmpreq->node     = req->node;
  tmpreq->src_proc = req->src_proc;
  tmpreq->req_type = (state == SH_CL) ? SHARED : INVALIDATE;
  tmpreq->type     = req->type == WRITEPURGE ? WRITEPURGE : COHE;
  tmpreq->paddr    = req->paddr & block_mask2;
  tmpreq->vaddr    = req->vaddr & block_mask2;
  tmpreq->ifetch   = 0;
  
  return tmpreq;
}



/*===========================================================================
 * Try to handle a L2 reply classified as one of the followings:
 *  - write hit in L1 cache, nothing to do;
 *  - not L1-allocated type, do global perform here;
 *  - otherwise, put it into the reply queue of L1 cache.
 */
static int Cache_add_repl_to_l1(CACHE *captr, REQ *req)
{
  if (req->prefetch == 8)
    {
      /*
       * This is an L2 prefetch, has nothing to do with L1 cache.
       */
      if (!req->cohe_count)
	YS__PoolReturnObj(&YS__ReqPool, req);

      return 1;
    }
 
  if (req->hit_type == L1DHIT)
    {
      /*
       * This is a write hit in L1 cache. Global perform has been done in
       * l1 cache. Just release the request here.
       */
      if (!req->cohe_count)
	YS__PoolReturnObj(&YS__ReqPool, req);
      
      return 1;
    }
 
  if (req->l1mshr == 0)
    {
      /*
       * This request hasn't allocated an MSHR in L1 cache, or say
       * it is not L1-allocate type. We can just release it here.
       */
      Cache_global_perform(captr, req, 1);
      return 1;
    }

  if (req->ifetch)
    {
      if (lqueue_full(&(captr->l1i_partner->reply_queue)))
	return 0;
      else
	{
	  lqueue_add(&(captr->l1i_partner->reply_queue), req, captr->nodeid);
	  req->progress = 0;
	  captr->l1i_partner->inq_empty = 0;
	  return 1;
	}
    }
  else
    {
      if (lqueue_full(&(captr->l1d_partner->reply_queue)))
	return 0;
      else
	{
	  lqueue_add(&(captr->l1d_partner->reply_queue), req, captr->nodeid);
	  req->progress = 0;
	  captr->l1d_partner->inq_empty = 0;
	  return 1;
	}
    }
}



/*=============================================================================
 * Start a L1-initiated prefetch. If L1Q is full, drop the prefetch.
 * Note that prefetch can not cross page boundary and no prefetch for
 * non-cacheable address.
 */

static void L2Cache_start_prefetch(CACHE *captr, REQ *req)
{
  REQ *newreq;
  unsigned newpaddr = req->paddr + captr->linesz;

  
  if (!SAMEPAGE(req->paddr, newpaddr) ||
      tlb_uncached(req->memattributes))
    return;

  captr->pstats->l2p.total++;
  if (lqueue_full(&(captr->request_queue)))
    {
      captr->pstats->l2p.dropped++;
      return;
    }

  /*
   * General information
   */
  newreq = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  newreq->type          = REQUEST;
  newreq->req_type      = READ;
  newreq->prefetch      = 8;
  newreq->node          = captr->nodeid;
  newreq->src_proc      = captr->procid;
  newreq->prcr_req_type = req->prcr_req_type;
  newreq->paddr         = newpaddr;
  newreq->vaddr         = req->vaddr + captr->linesz;
  newreq->ifetch        = req->ifetch;
  newreq->l1mshr        = 0;
  newreq->l2mshr        = 0;
  newreq->memattributes = req->memattributes;
  newreq->line_cold     = 0;

  /*
   * for statistics
   */
  newreq->issue_time    = YS__Simtime;
  newreq->hit_type      = UNKHIT;
  newreq->line_cold     = 0;

  /*
   * Add it to REQUEST input queue of L1 cache, which must then know that 
   * there is something on its input queue so as to be activated by 
   * cycle-by-cycle simulator. If this access fills up the port, the 
   * L1Q_FULL variable is set so that the processor does not issue any 
   * more memory references until the port clears up. 
   */

  lqueue_add(&(captr->request_queue), newreq, captr->nodeid);
  captr->inq_empty = 0;
}








