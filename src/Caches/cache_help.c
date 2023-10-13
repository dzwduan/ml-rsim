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
#include <string.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "IO/addr_map.h"


/*=============================================================================
 * This REQ should be placed in the specified link queue, if there is space 
 * available. If there is space, the function returns 1 to indicate success. 
 * If there is no space, the function returns 0 and does not add anything 
 * into the queue.
 */

int Cache_send_out_req(LinkQueue *lq, REQ * req)
{
  if (lqueue_full(lq))
    return 0;

  lqueue_add(lq, req, req->node);
  return 1;
}



/*=============================================================================
 * Update statistics and free the MSHR.
 */

int Cache_free_mshr(CACHE *captr, MSHR *pmshr)
{
  if (pmshr->demand > 0.0)         /* there was a late prefetch */
    StatrecUpdate(captr->pref_lateness,
		  (int)(YS__Simtime - pmshr->demand),
		  1);

  pmshr->valid = 0;
  pmshr->releasing = 0;
  captr->mshr_count--;

  if (pmshr->mainreq->type != COHE_REPLY)
    {
      /* We were using it as a temporary holding thing */
      captr->reqmshr_count--;
      captr->reqs_at_mshr_count -= pmshr->counter + 1;
    }

#if 0
   StatrecUpdate(captr->mshr_req_count, captr->reqs_at_mshr_count,
                 (long long)YS__Simtime);
   StatrecUpdate(captr->mshr_occ, captr->reqmshr_count,
		 (long long)YS__Simtime);
#endif

   return 0;
}



/*=============================================================================
 * Find which MSHR is being used by a current REQUEST.
 */
MSHR *Cache_find_mshr(CACHE *captr, REQ *req)
{
  int i;
  unsigned blocknum;

  if (captr->mshr_count == 0)
    return 0;

  blocknum = req->paddr >> captr->block_shift;
  for (i = 0; i < captr->max_mshrs; i++)
    {
      if (captr->mshrs[i].valid && blocknum == captr->mshrs[i].blocknum)
	return &(captr->mshrs[i]);
    }

  return 0;
}


/*=============================================================================
 * Allocate and initiate a link queue with default maximum size.
 */

LinkQueue *AllocAndInitLinkQueue(int max_size)
{
  LinkQueue *lq = (LinkQueue *) malloc(sizeof(LinkQueue));
  if (lq == NULL)
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  lq->head  = 0;
  lq->tail  = 0;
  lq->total = max_size;
  lq->size  = 0;

  return lq;
}


/*
 * Provides a replacement request of the specified type (INVL or
 * WRB, etc).
 */
REQ *Cache_make_req(CACHE *captr, cline_t *cline, ReqType req_type)
{
  REQ *req_ret;
  unsigned paddr = (cline->tag << captr->tag_shift) |
    ((cline->index >> captr->set_shift) << captr->block_shift);
 
  req_ret            = (REQ *) YS__PoolGetObj(&YS__ReqPool);
  req_ret->paddr     = paddr;
  req_ret->vaddr     = cline->vaddr;
  req_ret->req_type  = req_type;
  req_ret->node      = captr->nodeid;
  req_ret->src_proc  = captr->procid;
  req_ret->ifetch    = 0;
  req_ret->dest_proc = AddrMap_lookup(req_ret->node, req_ret->paddr);
  req_ret->memattributes = 1;
 
  return req_ret;
}
