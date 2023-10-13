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
#include "Processor/proc_config.h"
#include "Processor/memunit.h"
#include "Processor/capconf.h"
#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"
#include "Caches/syscontrol.h"


/* 
 * Macros which specify which pipeline each type of access belongs to.  
 * Note: we have added a separate COHE tag pipe in order to avoid deadlock.
 * Such a port may be considered excessive in a real system; thus, it may 
 * be advisable to reserve a certain number of MSHR-like buffers for incoming
 * COHE messages and simply withhold processing of additional COHEs until 
 * space opens up in one of these buffers. 
 */
#define L1ReqTAGPIPE 		0
#define L1ReplyTAGPIPE 		1
#define L1CoheTAGPIPE 		2    


/* 
 * Functions that actually process transactions 
 */
static int           L1DProcessTagRequest     (CACHE*, REQ*);
static int           L1DProcessTagReply       (CACHE*, REQ*);
static int           L1DProcessTagCohe        (CACHE*, REQ*);
static void          L1DProcessFlushPurge     (CACHE*, REQ*);
static MSHR_Response L1DCache_check_mshr      (CACHE*, REQ*);
static void          L1DCache_start_prefetch  (CACHE*, REQ*);
static int           L1DCache_uncoalesce_mshr (CACHE*, MSHR*);
extern int           L2ProcessL1Reply         (CACHE*, REQ*);



/*=============================================================================
 * L1CacheInSim: function that brings new transactions from the ports into
 * the pipelines associated with the various cache parts. An operation can
 * be removed from its port as soon as pipeline space opens up for it.
 * Called whenever there may be something on an input port (indicated by
 * "inq_empty").
 */

void L1DCacheInSim(int gid)
{
  CACHE *captr = L1DCaches[gid];
  int    nothing_on_ports = 1;
  REQ   *req;

  /*---------------------------------------------------------------------------
   * Process replies first so that their holding resources (cache line,
   * mshr, etc.) can be freed and available for other transactions.
   */
  while (req = lqueue_head(&captr->reply_queue))
    {
      if (AddToPipe(captr->tag_pipe[L1ReplyTAGPIPE], req))
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

  
  /*---------------------------------------------------------------------------
   * Request queue needs some special treatment because it's the
   * communication channel between the processor and memory system. 
   */
  while (req = lqueue_head(&captr->request_queue))
    {
      if (IsSysControl_local(req))
	{
	  if (SysControl_local_request(gid, req))
	    {
	      req->progress = 0;
	      L1DQ_FULL[gid] = lqueue_full(&(captr->request_queue));
	      continue;
	    }
	  else
	    {
	      nothing_on_ports = 0;
	      break;
	    }		
	}
      
      else if (tlb_uncached(req->memattributes))
	{
	  /* UBuffer_add_request removes the request from the queue and
	     frees the memory unit upon success */
	  if (UBuffer_add_request(req))
	    {
	      L1DQ_FULL[gid] = lqueue_full(&(captr->request_queue));
	      continue;
	    }
	  else
	    {
	      nothing_on_ports = 0;
	      break;
	    }
	}
      else if (AddToPipe(captr->tag_pipe[L1ReqTAGPIPE], req))
	{
          /* 
	   * It's been committed, free the corresponding mem unit if this 
	   * request originally came from the processor module.
	   */
	  if (req->prefetch != 4)
	    FreeAMemUnit(req->d.proc_data.proc_id,
			 req->d.proc_data.inst_tag);

	  L1DQ_FULL[gid] = 0;
	  captr->num_in_pipes++;
	  req->progress = 0;
	  lqueue_remove(&(captr->request_queue));
	}
      else
	{ /* couldn't add it to pipe */
	  nothing_on_ports = 0;
	  break;
	}
    }

  
  /*---------------------------------------------------------------------------
   * Coherency check queue.
   */
  while (req = lqueue_head(&captr->cohe_queue))
    {
      if (AddToPipe(captr->tag_pipe[L1CoheTAGPIPE], req))
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

  if (nothing_on_ports)        /* nothing available for processing */
    captr->inq_empty = 1;      /* All inqs are apparently empty */
}




/*=============================================================================
 * L1DCacheOutSim: initiates actual processing of various REQ types.
 * Called each cycle if there is something in pipes to be processed
 */
void L1DCacheOutSim(int gid)
{
  CACHE    *captr  = L1DCaches[gid];
  REQ      *req;
  int       pipenum, ctr, index;
  Pipeline *pipe;


  /*---------------------------------------------------------------------------
   * Always processes replies first because they are going to release the
   * resources and unstall stalled execution.
   */
  pipe = captr->tag_pipe[L1ReplyTAGPIPE];

  for (ctr = 0; ctr < pipe->ports; ctr++)
    {
      GetPipeElt(req, pipe);
      if (req == 0)
	break;

      if (L1DProcessTagReply(captr, req))
	{
	  ClearPipeElt(pipe);
	  captr->num_in_pipes--;
	}
    }

  /*-------------------------------------------------------------------------*/

  pipe = captr->tag_pipe[L1ReqTAGPIPE];

  for (ctr = 0; ctr < pipe->ports; ctr++)
    {
      GetPipeElt(req, pipe);
      if (req == 0)
	break;

      if (L1DProcessTagRequest(captr, req))
	{
	  ClearPipeElt(pipe);
	  captr->num_in_pipes--;
	}
    }

  
  /*---------------------------------------------------------------------------
   * Cohe requests are sent by L2 cache which has already sent cohe response
   * back to the requester. Since it's not in the critical path, it's
   * processed last.
   */

  pipe = captr->tag_pipe[L1CoheTAGPIPE];

  for (ctr = 0; ctr < pipe->ports; ctr++)
    {
      GetPipeElt(req, pipe);
      if (req == 0)
	break;

      if (L1DProcessTagCohe(captr, req))
	{
	  ClearPipeElt(pipe);
	  captr->num_in_pipes--;
	}
    }
}





/*=============================================================================
 * Simulates flush or purge operations to caches. In current simulator, each
 * such operation flushs/purges data in cache hierarchy, including L1 and L2.
 * In the processor's or sytem interface's point of view, each flush/purge
 * operation applies to one L2 cache line.  So for each flush/purge, L1 cache
 * has to check the physical address range contains the specified L2 cache 
 * line, which possible spans several L1 cache lines (if L1 cache line is
 * smaller than L2 cache line). In real world, this probably needs
 * (L2 cache line size / L1 cache line size) times access of L1 tag array.
 * But for simplicity, we just assume it could be done in one cycle. Since 
 * flush/purge doesn't likely happen a lot in real applications, the 
 * assumption won't affect the system performance significantly. 
 */

static void L1DProcessFlushPurge(CACHE * captr, REQ * req)
{
  int paddr = req->paddr & block_mask2;
  int eaddr = paddr + ARCH_linesz2;
  int vaddr = req->vaddr & block_mask2;
  int pbnum;
  int i;
  cline_t *cline;
  

  for (; paddr < eaddr; paddr += ARCH_linesz1i, vaddr += ARCH_linesz1i)
    {
      if (captr->mshr_count > 0)
	{
	  /*
	   * First check if there are any outstanding requests for this
	   * cache lines. If yes, mark the MSHR so that it will perform
	   * flush/purge after all coalesced requests have been served.
	   * It also stops the MSHR from coalesce more requests.
	   */
	  pbnum = paddr >> captr->block_shift;
	  for (i = 0; i < captr->max_mshrs; i++)
	    {
	      if (captr->mshrs[i].valid && pbnum == captr->mshrs[i].blocknum)
		{
		  captr->mshrs[i].pend_opers = req->prcr_req_type;
		  break;
		}
	    }

	  if (i != captr->max_mshrs)
	    {
	      req->hit_type = L1DHIT; 
	      continue; 
	    }
	}

      /*
       * Invalidate the line if it presents in cache. We don't need write
       * data back to L2 because it's a write-through cache.
       */
      if (Cache_search(captr, vaddr, paddr, &cline) == 0)
	{
	  if (cline->pref == 1)
	    captr->pstats->sl1.useless++;
	  else if (cline->pref == 4)
	    captr->pstats->l1dp.useless++;
	  cline->state = INVALID;
	  cline->tag = -1;
	} 
    }
}





/*=============================================================================
 * L1ProcessTagRequest: simulates request accesses to the L1 cache.
 * We use "req->progress" to record the processing progress of a request.
 *  progress = 0: initial value
 *  progress = 1: has been processed, but stuck because the output queue 
 *                is full. Just try to send it out if comes again.
 * Returns 0 on failure; 1 on success.
 */

static int L1DProcessTagRequest(CACHE * captr, REQ * req)
{
  int            status;
  MSHR_Response  mshr_status;
  LinkQueue      *lq;

  
  if (cparam.L1D_perfect)
    {
      req->hit_type = L1DHIT;
      Cache_global_perform(captr, req, 1);         
      return 1;
    }


   if (req->progress == 0)
     {
       if (req->prcr_req_type == FLUSHC)
	 {
	   if (!cparam.L1D_writeback)
	     L1DProcessFlushPurge(captr, req);
	   req->progress = 1;
	 }
       else if (req->prcr_req_type == PURGEC)
	 {
	   L1DProcessFlushPurge(captr, req);
	   req->progress = 1;
	 }
       else
	 {         /* READ, WRITE, or RMW */
	   mshr_status = L1DCache_check_mshr(captr, req);
	   switch (mshr_status)
	     {
	     case NOMSHR: /* a hit with no associated forward request */
	       if (req->prefetch == 4)
		 captr->pstats->l1dp.unnecessary++;
	       else if (req->prefetch == 1)
		 captr->pstats->sl1.unnecessary++;
	       else if (req->prefetch == 2)
		 captr->pstats->sl2.unnecessary++;
 
	       /*
		* send the request back to the processor, as it's done.
		* REQs also get freed up there.
		*/
	       if (req->prefetch == 4)
		 YS__PoolReturnObj(&YS__ReqPool, req);
	       else
		 Cache_global_perform(captr, req, 1);
	       return 1;
 
	     case MSHR_COAL: /* Coalesced into an MSHR. success. */
	       return 1;
 
	     case MSHR_NEW: /* need to send down some sort of miss or upgrd */
	       if (req->prefetch == 4)
		 captr->pstats->l1dp.issued++;
	       else if (req->prefetch == 1)
		 captr->pstats->sl1.issued++;
	       req->progress = 1;
 
	       if (cparam.L1D_prefetch && !req->prefetch &&
		   req->hit_type != L1DHIT)
		 L1DCache_start_prefetch(captr, req);
	       break;
 
	     case MSHR_USELESS_FETCH:
	       /*
		* a prefetch to a line that already has an outstanding fetch.
		* Drop this prefetch even without discriminate prefetch on.
		*/
	       if (req->prefetch == 4)
		 captr->pstats->l1dp.unnecessary++;
	       else if (req->prefetch == 1)
		 captr->pstats->sl1.unnecessary++;
	       else if (req->prefetch == 2)
		 captr->pstats->sl2.unnecessary++;
 
	       req->hit_type = L1DHIT;
	       if (req->prefetch == 4)
		 YS__PoolReturnObj(&YS__ReqPool, req);
	       else
		 Cache_global_perform(captr, req, 1);
	       return 1;
 
	     case NOMSHR_FWD:
	       /*
		* means REQUEST is a non-write-allocate write miss/hit or
		* an L2 prefetch. No prefetch stats here, since that will
		* be done in L2 cache.
		*/
	       if (req->hit_type == L1DHIT)        /* A write hits L1 cache. */
		 Cache_global_perform(captr, req, 0);

	       req->progress = 1;
	       break;
	       
	     case MSHR_STALL_WAR:  /* a write wants to merge with a read MSHR*/
	     case MSHR_STALL_COAL: /* too many requests coalesced to  MSHR   */
	     case MSHR_STALL_FLUSH:   /* pending flush/purge requests        */
	     case NOMSHR_STALL:       /* No MSHRs available for this request */
	       if (req->prefetch && DISCRIMINATE_PREFETCH)
		 {
		   /* just drop the prefetch here. */
		   if (req->prefetch == 1)
		     captr->pstats->sl1.dropped++;
		   else if (req->prefetch == 2)
		     captr->pstats->sl2.dropped++;
		   else if (req->prefetch == 4)
		     captr->pstats->l1dp.dropped++;
 
		   YS__PoolReturnObj(&YS__ReqPool, req);
		   return 1;
		 }
	       return 0;
 
	     default:
	       YS__errmsg(captr->nodeid, "Default case in L1D mshr_hittype: %d", mshr_status);
	     }
	 }
     }

   
   /*
    * we'll send down an upgrade, miss, or non-allocating access
    * successful if there's space in the output port; otherwise, return 0
    * and allow retry.
    */
   if (cparam.L1D_writeback)
     lq = &(captr->l2_partner->request_queue);
   else
     lq = &(PID2WBUF(0, captr->procid)->inqueue);

   if (!lqueue_full(lq))
     {
       lqueue_add(lq, req, captr->nodeid);
       if (cparam.L1D_writeback)
         captr->l2_partner->inq_empty = 0;
       return 1;
     }
 
   return 0;
}



/*=============================================================================
 * Check MSHR for a new L1 REQUEST.
 */
static MSHR_Response L1DCache_check_mshr(CACHE *captr, REQ *req)
{
  MSHR    *pmshr;
  cline_t *cline;
  int      misscache, i;

  /* 
   * First determines whether the incoming transaction matches any MSHR 
   */
  pmshr = 0; 
  if (captr->mshr_count > 0)
    {
      unsigned blocknum = ADDR2BNUM1D(req->paddr);
      for (i = 0; i < captr->max_mshrs; i++)
	{
	  if ((captr->mshrs[i].valid == 1) &&
	      (blocknum == captr->mshrs[i].blocknum))
	    {
	      pmshr = &(captr->mshrs[i]);
	      break;
	    }
	}
    }


  /* 
   * Determine if the requested line is available in the cache. 
   */
  misscache = Cache_search(captr, req->vaddr, req->paddr, &cline);

  /*
   * If it's an L1 prefetch, we will drop it no matter what as long as it
   * matches an MSHR or a cache line.
   */
  if ((req->prefetch == 4) && (pmshr || misscache == 0))
    return MSHR_USELESS_FETCH;

  if (pmshr == 0)
    {  /* the line is not present in any MSHR */
      if (misscache == 0)    /* hit */
	{
	  if (Cache_hit_update(captr, cline, req))
	    { /* update LRU ages */
	      if (cparam.L1D_prefetch & 2)
		L1DCache_start_prefetch(captr, req);
	    }
         
	  if (req->prcr_req_type == READ)
	    {
	      req->hit_type = L1DHIT;
	      return NOMSHR;
	    }
	  else
	    { /* it's a write or rmw */ 
	      if (cline->state != SH_CL)
		{ 
		  /* 
		   * The access is either a write or a rmw; and the cache line 
		   * in private state. We need forward the write to L2 cache 
		   * through write buffer. But it doesn't need anything here.
		   */
		  if (req->prefetch)
		    return MSHR_USELESS_FETCH;

		  if (cline->state == PR_CL)
		    cline->state = PR_DY;
		  req->hit_type = L1DHIT;

		  if (cparam.L1D_writeback)
		    return NOMSHR;
		  else
		    return NOMSHR_FWD;
		}
	      else
		{ /* the line is in shared state */
		  if (req->prefetch == 2)
		    {
		      /* 
		       * It is an L2 write prefetch.
		       * Nothing to do with L1 cache.
		       */
		      return NOMSHR_FWD;
		    }
		  /* Otherwise, it will allocate an L1 MSHR. */
		}
	    }
	}

      /* The line is either not in l1 cache, or in l1 cache but the cache
         line is in share state. */

      if ((req->prcr_req_type == WRITE && !misscache) ||
	  req->prefetch == 2)
	{
	  /* 
	   * A write missing in L1 cache or an L2 prefetch goes to the next 
	   * level of cache without taking an MSHR here.
	   */
	  return NOMSHR_FWD;
	}

      
      /**** Otherwise, we need to allocate an MSHR for this request ****/

      if (captr->mshr_count == captr->max_mshrs)
	{  
	  /* 
	   * None are available, the value "NOMSHR_STALL" is returned. 
	   */
	  captr->pstats->l1dstall.full++;
	  return NOMSHR_STALL;
	}

      
      /* 
       * Find the first free MSHR.
       */
      for (i = 0; i < captr->max_mshrs; i++)
	{
	  if (captr->mshrs[i].valid != 1)
	    {
	      pmshr = &(captr->mshrs[i]);
	      break;
	    }
	}

      req->l1mshr         = pmshr;
      pmshr->valid        = 1;
      pmshr->mainreq      = req;
      pmshr->setnum       = cline->index >> captr->set_shift;
      pmshr->blocknum     = req->paddr >> captr->block_shift;
      pmshr->counter      = 0;
      pmshr->pending_cohe = 0;
      pmshr->stall_WAR    = 0;
      pmshr->demand       = -1.0; 
      pmshr->only_prefs   = (req->prefetch) ? 1 : 0;
      pmshr->has_writes   = 0;
      pmshr->cline        = cline;
      pmshr->misscache    = misscache;
      pmshr->pend_opers   = 0;

      captr->mshr_count++;
      captr->reqmshr_count++;
      captr->reqs_at_mshr_count++;

      return MSHR_NEW;
    }

  
  /* Matches an MSHR. The REQUEST must be merged, dropped, forwarded
     or stalled. */

  /*
   * If there are pending flush/purge operations, stall.
   */
  if (pmshr->pend_opers)
    {
      captr->pstats->l1dstall.flush++;
      return MSHR_STALL_FLUSH;
    }
   
  /* 
   * Now, how does the MSHR handle prefetch matches? At the first level
   * cache, prefetches should either be dropped, forwarded around cache, or 
   * stalled. There is never any need to coalesce at L1, since one fetch is 
   * sufficient. At the L2, though, prefetches cannot be dropped, as they
   * might already have many requests coalesced into them and waiting for
   * them at the L1 cache. 
   */
  if (req->prefetch)
    {
      if (req->prcr_req_type == READ)
	{
          /* 
           * If a read prefetch wants to coalesce at L1 cache, drop it. 
           */
          return MSHR_USELESS_FETCH;
	}
      else
	{ /* it's a write prefetch */
          if (req->prefetch == 1)
	    {
	      /* 
	       * If an L1 write prefetch wants to coalesce at L1 cache, 
	       * send down as a NOMSHR_FWD, after transforming to an L2 
	       * write prefetch. (may cause upgrade) 
	       */
	      if (pmshr->mainreq->prcr_req_type != READ)
                return MSHR_USELESS_FETCH;
	    }
          else if (req->prefetch == 2)
	    return NOMSHR_FWD;
	}
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
      captr->pstats->l1dstall.war++;
      return MSHR_STALL_WAR;
    }

  if (req->prcr_req_type != READ && 
      pmshr->mainreq->prcr_req_type == READ)
    {
      /* 
       * Write after read -- stall system. Note: if the access is a prefetch 
       * that is going to be dropped with DISCRIMINATE prefetching, there is 
       * no reason to count this in stats or start considering this an "old" 
       * WAR.
       */
      if (!req->prefetch || !DISCRIMINATE_PREFETCH)
	{
	  captr->pstats->l1dstall.war++;
	  pmshr->stall_WAR = 1;
	}
      
      return MSHR_STALL_WAR;
    }


  /*
   * Too many requests coalesced with MSHR 
   */
  if (pmshr->counter == MAX_COALS-1)
    {  
      captr->pstats->l1dstall.coal++;
      return MSHR_STALL_COAL;
    }

  
  /* 
   * No problems with coalescing the request, so coalesce it.
   */
  pmshr->coal_req[pmshr->counter++] = req;

  if (pmshr->only_prefs && !req->prefetch)
    {
      /* 
       * Demand access coalesces with late prefetch accesses.
       */
      if (pmshr->mainreq->prefetch == 1)
	{
	  captr->pstats->sl1.late++;
	  captr->pstats->sl1.useful++;
	}
      else if (pmshr->mainreq->prefetch == 4)
	{
	  captr->pstats->l1dp.late++;
	  captr->pstats->l1dp.useful++;
	}
      pmshr->only_prefs = 0;
      pmshr->demand = YS__Simtime;
    }

  return MSHR_COAL;
}



/*=============================================================================
 * L1ProcessTagReply: simulates access replies to the L1 cache.
 * This functions always return 1 because it won't fail for a write-through
 * cache -- no write-back to lower level, no invalidation to higher level.
 */
static int L1DProcessTagReply(CACHE * captr, REQ * req)
{
  int        misscache;
  HIT_TYPE   hit_type;
  MISS_TYPE  ccdres;
  cline_t   *cline;
  MSHR      *pmshr;


  if (cparam.L1D_perfect) 
    YS__errmsg(captr->nodeid, "Perfect L1 D-cache shouldn't receive REPLY");

  if (req->progress == 0)
    {   /* no progress made so far */
      pmshr = req->l1mshr;
      if (pmshr == 0)
	 YS__errmsg(captr->nodeid,
		    "L1D Cache %i received a reply for a nonexistent MSHR\n",
		    captr->procid);

      /*
       * Collect statistics if the mainreq is a late prefetch.
       */
      if (pmshr->demand > 0)
	{
	  if (pmshr->mainreq->prefetch == 1)
            captr->pstats->sl1.lateness += YS__Simtime - pmshr->demand;
	  else if (pmshr->mainreq->prefetch == 4)
            captr->pstats->l1dp.lateness += YS__Simtime - pmshr->demand;
	  pmshr->demand = -1;
	}

      cline     = pmshr->cline;
      misscache = pmshr->misscache;
      req->wrb_req = 0;

      if (misscache == 0)     /* line present in cache -- upgrade reply */
	 cline->state = PR_DY;
      else
	{ /* line not present in cache */
	  if (misscache == 1)
	    { 
	      /* "present" miss -- COHE miss */
	      Cache_pmiss_update(captr, req, cline->index, pmshr->only_prefs);
	      CCD_InsertNewLine(captr->ccd, ADDR2BNUM1D(req->paddr));         
	      req->miss_type1 = CACHE_MISS_COHE;
	    }
	  else
	    {
	      /* total miss: find a victim and updates ages */
	      if (!Cache_miss_update(captr, req, &cline, pmshr->only_prefs))
		YS__errmsg(captr->nodeid,
			   "L1WT should never see a NO_REPLACE");

	      /* We should determine COLD/CAP/CONF */
	      ccdres = CCD_InsertNewLine(captr->ccd, ADDR2BNUM1D(req->paddr));
	      req->miss_type1 = req->line_cold ? CACHE_MISS_COLD : ccdres;
	      pmshr->cline = cline;
	    }

	  if (cline->state == SH_CL)
	    captr->pstats->shcl_victims1d++;
	  else if (cline->state == PR_CL)
	    captr->pstats->prcl_victims1d++;
	  else if (cline->state == PR_DY)
	    {
	      if (cparam.L1D_writeback)
		{
		  req->wrb_req = Cache_make_req(captr, cline, WBACK);
		  req->wrb_req->type = REQUEST;
		}
	      captr->pstats->prdy_victims1d++;
	    }


	  /* 
	   * Now fill in the information for the new line 
	   */
	  cline->tag   = req->paddr >> captr->tag_shift;
	  cline->vaddr = req->vaddr & block_mask1d;
	  if (pmshr->has_writes)
	    cline->state = PR_DY;
	  else
	    cline->state = (req->req_type == REPLY_SH) ? SH_CL : PR_CL;
	}

      req->progress = 1;
    }


  if (req->progress == 1)
    {
      if (req->wrb_req)
	{
	  LinkQueue *lq = &(captr->l2_partner->request_queue);
	  if (lqueue_full(lq))
            return 0;
	  else
	    {
	      lqueue_add(lq, req->wrb_req, captr->nodeid);
	      captr->l2_partner->inq_empty = 0;
	    }
	}
      req->progress = 2;
    }
 

   /* 
   * Now, resolve all requests coalesced into the MSHR in question. Send 
   * them back to the processor for possible further handling. And if any
   * flush/purge transaction is pending, activate it.
   */
  pmshr = req->l1mshr;
  L1DCache_uncoalesce_mshr(captr, pmshr);

  if (pmshr->pend_opers && pmshr->cline->state != INVALID)
    {
      if (pmshr->cline->pref == 1)
	captr->pstats->sl1.useless++;
      else if (pmshr->cline->pref == 4)
	captr->pstats->l1dp.useless++;
      pmshr->cline->state = INVALID;
      pmshr->cline->tag = -1;
    }

  /*  
   * free up the MSHR, or release all coalesced requests. 
   */
  Cache_free_mshr(captr, pmshr);  

  return 1;
}




/*=============================================================================
 * Uncoalesce each request coalesced in the specified MSHR.
 */
int L1DCache_uncoalesce_mshr(CACHE *captr, MSHR *pmshr)
{
  HIT_TYPE  hit_type = pmshr->mainreq->hit_type;
  int       latepf   = (pmshr->demand > 0.0);
  int       i;

  /*
   * If this MSHR had a late prefetch, set prefetched_late field to allow
   * system to count every access coalesced into the MSHR as part of "late PF"
   * time in statistics. Is this reasonable?
   */
  pmshr->mainreq->prefetched_late = latepf;

  if (pmshr->mainreq->prefetch == 4)
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
      req->hit_type = L1DHIT;
      req->prefetched_late = latepf;

      if (req->prefetch == 4)
	YS__errmsg(captr->nodeid,
		   "L1Cache_uncoalesce_msrh: coalesced L1 prefetch");
      
      Cache_global_perform(captr, req, 1);
    }

  return 0;
}




/*=============================================================================
 * L1ProcessTagCohe: simulates coherence requests or invalidation
 * request to the L1 cache. An invalidation request is sent by the L2
 * when a miss victim is casted out of L2 cache in order to ensure the 
 * "inclusion property".
 */

static int L1DProcessTagCohe(CACHE * captr, REQ * req)
{
  cline_t *cline;
  int      i, misscache;
  unsigned paddr;
  unsigned vaddr;
  unsigned eaddr;


  if (cparam.L1D_perfect)
    req->progress = 1;

  if (req->progress == 0)
    {
      paddr = req->paddr & block_mask2;
      vaddr = req->vaddr & block_mask2;
      eaddr = paddr + ARCH_linesz2;

      for (; paddr < eaddr; paddr += ARCH_linesz1d, vaddr += ARCH_linesz1d)
	{
	  misscache = Cache_search(captr, vaddr, paddr, &cline);       

	  if (misscache == 0)
	    {  /* line in cache */
	      if (req->req_type == INVALIDATE)
		{
		  if (captr->gid == req->src_proc)
		    {
		      /* 
		       * It's actually an invalidation request. Reset the tag
		       * so it won't be taken as a victim of coherence check.
		       */
		      cline->tag = -1;
		    }

		  if (cline->state == PR_DY)
		    req->wb_count++;
		  cline->state = INVALID;

		  if (cline->pref == 1)
		    captr->pstats->sl1.useless++;
		  else if (cline->pref == 4)
		    captr->pstats->l1dp.useless++;

		  /* 
		   * Invalidated; check outstanding speculative loads. 
		   */
		  if (Speculative_Loads)      
		    SpecLoadBufCohe(captr->procid, paddr, SLB_Cohe);
		}
	      else
		cline->state = SH_CL;
	    }
	}

      req->progress = 1;
    }

  if (cparam.L1D_writeback)
    return L2ProcessL1Reply(captr->l2_partner, req);
  else
    {
      if (!req->cohe_count)
	YS__PoolReturnObj(&YS__ReqPool, req);

      return 1;
    }
}




/*=============================================================================
 * Start an L1-initiated prefetch. If L1Q is full, drop the prefetch.
 * Note that prefetch can not cross page boudary and no prefetch for
 * non-cacheable address.
 */

static void L1DCache_start_prefetch(CACHE *captr, REQ *req)
{
  REQ *newreq;
  unsigned newpaddr = req->paddr + captr->linesz;

  if (!SAMEPAGE(req->paddr, newpaddr) ||
      tlb_uncached(req->memattributes))
    return;

  captr->pstats->l1dp.total++;

  if (L1DQ_FULL[captr->gid])
    {
      captr->pstats->l1dp.dropped++;
      return;
    }

  /*
   * General information
   */
  newreq = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  newreq->type          = REQUEST;
  newreq->req_type      = READ;
  newreq->prefetch      = 4;
  newreq->node          = captr->nodeid;
  newreq->src_proc      = captr->procid;
  newreq->prcr_req_type = req->prcr_req_type;
  newreq->paddr         = newpaddr;
  newreq->vaddr         = req->vaddr + captr->linesz;
  newreq->ifetch        = 0;
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
  captr->inq_empty  = 0;
  L1DQ_FULL[captr->gid] = lqueue_full(&(captr->request_queue));
}



#if 0
void L1ProcessTagPipe(CACHE *captr, int pipenum)
{
  Pipeline *pipe = captr->tag_pipe[pipenum];
  int       ctr  = 0;
  REQ       *req;
  
  while (1)
    {
      GetPipeElt(req, pipe);
      if (req == 0)
	break;

      if (L1ProcessTagReq(captr, req) == 1)
	{
	  ClearPipeElt(pipe);
	  captr->num_in_pipes--;
	}
      else
	ctr++;
    }
}



/*
 * L1ProcessTagReq: simulates accesses to the L1 cache.
 * Its behavior depends on the type of message (i.e. type) received
 * Returns 0 on failure; 1 on success.
 */
static int L1ProcessTagReq(CACHE * captr, REQ * req)
{
  switch (req->type)
    {
    case REQUEST:
      return L1ProcessTagRequest(captr, req);

    case REPLY:
      return L1ProcessTagReply(captr, req);

    case COHE:
      return L1ProcessTagCohe(captr, req);

    case COHE_REPLY:
      YS__errmsg("L1 cache should not get a COHE_REPLY!");
    }

  YS__errmsg("Unreachable code in L1ProcessTagReq()!");
  return 0;
}


/*
 * Macro that processes input queues of L1 cache. Used by L1CacheInSim.
 */
#define ProcessInputQueue(lq, pipenum)                     	\
        while (req = (REQ *) lqueue_head(lq)) {                 \
           if (AddToPipe(captr->tag_pipe[pipenum], req)) {      \
              req->progress = 0;                                \
              lqueue_remove(lq, next, REQ *);                   \
              captr->num_in_pipes++;                            \
           }                                                    \
           else {                                               \
              nothing_on_ports = 0;                             \
              break;                                            \
           }                                                    \
        }

/*
 * Macro that processes pipelines of L1 cache. Used by L1CacheOutSim.
 */
#define ProcessPipeline(pline, pfunc)           \
        pipe = pline;                           \
        ctr  = 0;                               \
        while (1) {                             \
           GetPipeElt(req, pipe, ctr);          \
           if (req == 0)                        \
              break;                            \
           if (pfunc(captr, req)) {             \
              ClearPipeElt(pipe, ctr);          \
              captr->num_in_pipes--;            \
           }                                    \
           else                                 \
              ctr++;                            \
        }
#endif





