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

#include <malloc.h>
#include "Processor/proc_config.h"
#include "Processor/simio.h"
#include "Processor/memunit.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"


int     ARCH_wbufsz   = 8;
int     ARCH_wbufqsz  = 32;

extern WBUFFER **WBuffers;



/*===========================================================================
 * Initializes the write-buffer.
 */
void L1DCache_wbuffer_init(WBUFFER *pwbuf, int nodeid, int pid)
{
  lqueue_init(&(pwbuf->write_queue), ARCH_wbufsz);
  pwbuf->num_in_pipes = 0;

  lqueue_init(&(pwbuf->inqueue), ARCH_wbufqsz);
  pwbuf->inq_empty = 1;

  /* Clear out relevant statistics -- explained in cache.h */
  pwbuf->stall_wb_full = 0;
  pwbuf->stall_match   = 0;
}




/*===========================================================================
 * This module simulates a write-buffer for a no-write-allocate
 * write-through primary cache.  It is connected to a non-blocking
 * write-allocate write-back secondary cache.  Although write-buffer is
 * in parallel with the L1 cache, the module sits between the two caches. 
 * In order to provide the semblance of parallel access, the write buffer 
 * has zero delay.           
 *                                                            
 * Reads are normally allowed to bypass the write-buffer, but stall in 
 * case of a match. The read cannot simply be forwarded with a        
 * write-value if available, as the read has already booked an MSHR at
 * the L1 cache and other requests may have coalesced with it        
 * already. So, we choose to stall the read until the corresponding 
 * write is issued from the write buffer.                          
 *                                                                
 * A write is sent from the write-buffer as soon as there is port space 
 * for it without interfering with a read in the same cycle.
 */

void L1DCacheWBufferSim(int gid)
{
  WBUFFER *pwbuf = WBuffers[gid];
  CACHE   *captr = L2Caches[gid];
  REQ     *req;
  int      hitwbuffer;

  /*
   * First, send one write to L2 cache if there is any.
   */
  if (pwbuf->write_queue.size > 0 && 
      !lqueue_full(&(captr->request_queue)))
    {
      lqueue_get(&(pwbuf->write_queue), req);
      pwbuf->num_in_pipes--;

      lqueue_add(&(captr->request_queue), req, gid / ARCH_cpus);
      captr->inq_empty = 0;

      if (req->wb_cohe_pending)
	{
	  captr->stall_cohe--;
	  req->wb_cohe_pending = 0;
	}
    }

  req = lqueue_head(&pwbuf->inqueue);
  if (req == NULL)
    return;

  if (req->type == REQUEST)
    {
      if ((req->prcr_req_type != WRITE && req->prcr_req_type != RMW) ||
	  req->prefetch != 0)
	{
	  hitwbuffer = L1DCache_wbuffer_search(pwbuf, req, 0);
	  if (hitwbuffer)
	    {
	      /* 
	       * A read matched a write tag! Stall until the write is done.
	       */
	      pwbuf->stall_match++;
	      return;
	    }

	  /* 
	   * Send it on to the next module -- L2 cache 
	   */
	  if (!lqueue_full(&(captr->request_queue)))
	    {
	      lqueue_remove(&(pwbuf->inqueue));
	      lqueue_add(&(captr->request_queue), req, req->node);
	      captr->inq_empty = 0;
	    }
	}
      else
	{ /* These are writes */
#ifndef STORE_ORDERING
	  /* 
	   * these processors do not commit writes until they reach the 
	   * write-buffer 
	   */
	  if (!simulate_ilp)     
            AckWriteToWBUF(req->d.proc_data.inst, req->d.proc_data.proc_id);
#endif
	  /* 
	   * Allocate a new entry in the write-buffer (if available), and 
	   * buffer the write.
	   */
	  if (!lqueue_full(&(pwbuf->write_queue)))
	    {
	      lqueue_remove(&(pwbuf->inqueue));
	      lqueue_add(&(pwbuf->write_queue), req, req->node);
	      req->wb_cohe_pending = 0; 
	      pwbuf->num_in_pipes++;
	    }
	  else
	    pwbuf->stall_wb_full++;
	}
    }
  else
    YS__errmsg(req->node, "Unknown request type(%d) in Cache_wbuffer", req->type);
}



/*===========================================================================
 * Checks if an incoming REQUEST matches the tag of an entry in the 
 * write buffer. For reads, stall until corresponding entry is freed.
 * a cohe REQUESt also has to wait if it hits in the write buffer.
 */

int L1DCache_wbuffer_search(WBUFFER * pwbuf, REQ *req, int cohe)
{
  REQ *tempreq;
  int  n;

  for (n = 0; n < pwbuf->write_queue.size; n++)
    {
      lqueue_elem(&pwbuf->write_queue, tempreq, n, tempreq->node);

      if (cohe)
	{
	  if ((req->paddr & block_mask2) == (tempreq->paddr & block_mask2))
	    {
	      tempreq->wb_cohe_pending = 1;
	      return 1;
	    }
	}
      else
	{
	  if ((req->paddr & block_mask1d) == (tempreq->paddr & block_mask1d))
            return 1;
	}
    }

  return 0;
}
