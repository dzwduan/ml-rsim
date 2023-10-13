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
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"

/*
 * Assign REAL names to some integer values so that the debugger won't have
 * to look everywhere for those variables' meaning.
 */
static char *ReqTypeName[] =
{
  "BAD",
  "READ",
  "WRITE",
  "RMW",
  "FLUSH",
  "PURGE",
  "RWRITE",
  "WBACK",
  "READ_SH",
  "READ_OWN",
  "READ_CURRENT",
  "UPGRADE",
  "READ_UC",
  "WRITE_UC",
  "SWAP_UC",
  "REPLY_EXCL",
  "REPLY_SH",
  "REPLY_UPGRADE",
  "REPLY_UC",
  "INVALIDATE",
  "SHARED"
};


static char *TypeName[] =
{
  "REQUEST",
  "REPLY",
  "COHE",
  "COHE_REPLY",
  "WRITEBACK",
  "WRITEPURGE",
  "BARRIER"
};


static char *HitTypeNames[] =
{
  "L1HIT",
  "L2HIT",
  "MEM",
  "IO",
  "UNKOWN"
};


/*
 * Dump a request. "flag" indicates where the request locates, therefore
 * deciding what information shoud be printed out.
 */
void Cache_req_dump(REQ *req, int flag, int node)
{
  YS__logmsg(node,
	     "  0x%08x: (%s), pref(%d), vaddr(0x%x), paddr(0x%x), attr(0x%x), time(%.0f)\n",
	     req, ReqTypeName[req->prcr_req_type], req->prefetch,
	     req->vaddr, req->paddr, req->memattributes, req->issue_time);

  switch (flag)
    {
    case 0x01: /* pipeline */
      YS__logmsg(node,
		"              progress(%d))\n",
		req->progress);
      break;

    case 0x11: /* L1 request queue */
      break;
      
    case 0x12: /* L1 reply queue */
      YS__logmsg(node,
		 "              l1mshr(0x%x), handled(%s)\n",
		 req->l1mshr, HitTypeNames[req->hit_type]);
      break;
      
    case 0x13: /* L1 cohe queue */
      break;

    case 0x31: /* wbuffer input queue */
      break;
      
    case 0x32: /* wbuffer write queue */
      YS__logmsg(node,
		 "              wb_cohe_pending(%d)\n",
		 req->wb_cohe_pending);
      break;

    case 0x21: /* L2 request queue */
      break;
      
    case 0x22: /* L2 reply queue */
      YS__logmsg(node,
		 "              l2mshr(0x%x), handled(%s)\n",
		 req->l2mshr, HitTypeNames[req->hit_type]);
      break;
      
    case 0x23: /* L2 cohe queue */
      YS__logmsg(node,
		 "              src_node(%d)\n",
		 req->src_proc);
      break;
    
    case 0x41: /* request buffer */
      YS__logmsg(node,
		 "              cohe_count(%d), data_done(%d), cohe_done(%d), proc(%d), dest(%d)\n", 
		 req->cohe_count, req->data_done, req->cohe_done,
		 req->src_proc, req->dest_proc);
      break;

    case 0x42: /* writeback buffer */
      YS__logmsg(node,
		 "              parent(0x%x), req_type(%s)\n", 
		 req->parent, ReqTypeName[req->req_type]);
      break;
     
    case 0x43: /* arbitration waiter */
      YS__logmsg(node,
		 "              type(%s), reqnum(%d), proc(%d), dest(%d)\n", 
		 TypeName[req->type],
		 (req->type == REQUEST) ? req->reqnum : -1,
		 req->src_proc, req->dest_proc);
      break;

    case 0x51: /* bus request buffer */
    case 0x52: /* request number wait */
    case 0x53: /* bus arbitration waiters */
      YS__logmsg(node,
		 "              type(%s), proc(%d), dest(%d)\n", 
		 TypeName[req->type], req->src_proc, req->dest_proc);
      break;

    case 0x61: /* mmc request buffer */
    case 0x62: /* mmc arbitration buffer */
    case 0x63: /* mmc current request */
      YS__logmsg(node,
		 "              type(%s), proc(%d)\n", 
		 TypeName[req->type], req->src_proc);
      if (req->type == REQUEST)
	YS__logmsg(node,
		   " cohe_count(%d), c2c_copy(%d), data_rtn(%d)\n",
		   req->cohe_count, req->c2c_copy, req->data_rtn);
      break;

    case 0x71: /* I/O request */
      YS__logmsg(node,
		 "              type(%s), proc(%d), dest(%d)\n", 
		 TypeName[req->type], req->src_proc, req->dest_proc);
      if (req->type == REQUEST)
	YS__logmsg(node,
		   " cohe_count(%d), c2c_copy(%d), data_rtn(%d)\n",
		   req->cohe_count, req->c2c_copy, req->data_rtn);
      break;
    default:
      break;
    }
}



/*
 * Perform exhaustive printouts on the current state of L1 cache.
 */
void L1ICache_dump(int proc_id)
{
  CACHE *captr = L1ICaches[proc_id];
  REQ   *req;
  int    i, j, index;

  YS__logmsg(proc_id / ARCH_cpus, "\n====== L1 I-CACHE ======\n");

  /*
   * Input queues 
   */
  YS__logmsg(proc_id / ARCH_cpus,
	     "inq_empty = %d\n",
	     captr->inq_empty);
  if (captr->request_queue.size)
    DumpLinkQueue("request queue", &captr->request_queue, 0x11,
		  Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->reply_queue.size)
    DumpLinkQueue("reply queue",   &captr->reply_queue, 0x12,
		  Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->cohe_queue.size) 
    DumpLinkQueue("cohe queue",    &captr->cohe_queue, 0x13,
		  Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * Pipelines
   */
  YS__logmsg(proc_id / ARCH_cpus,
	  "num_in_pipes = %d\n",
	  captr->num_in_pipes);
  if (captr->tag_pipe[2]->count)
    DumpPipeline("request pipeline", captr->tag_pipe[2], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->tag_pipe[1]->count)
    DumpPipeline("reply pipeline", captr->tag_pipe[1], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->tag_pipe[0]->count)
    DumpPipeline("cohe pipeline", captr->tag_pipe[0], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * MSHRs
   */
  for (i = 0; i < captr->max_mshrs; i++)
    {
      MSHR *pmshr = &(captr->mshrs[i]);
      if (pmshr->valid != 1)
	continue;
      YS__logmsg(proc_id / ARCH_cpus,
	      "MSHR(%d): size = %d\n",
	      i, pmshr->counter + 1);
      Cache_req_dump(pmshr->mainreq, 0, proc_id / ARCH_cpus);
      for (j = 0; j < pmshr->counter; j++)
	Cache_req_dump(pmshr->coal_req[j], 0, proc_id / ARCH_cpus);
    }

  /*
   * What else?
   */
}




/*
 * Perform exhaustive printouts on the current state of L1 cache.
 */
void L1DCache_dump(int proc_id)
{
  CACHE *captr = L1DCaches[proc_id];
  REQ   *req;
  int    i, j, index;

  YS__logmsg(proc_id / ARCH_cpus, "\n====== L1 D-CACHE ======\n");

  /*
   * Input queues 
   */
  YS__logmsg(proc_id / ARCH_cpus,
	  "inq_empty = %d\n",
	  captr->inq_empty);
  if (captr->request_queue.size)
    DumpLinkQueue("request queue", &captr->request_queue, 0x11,
		  Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->reply_queue.size)
    DumpLinkQueue("reply queue",   &captr->reply_queue, 0x12,
		  Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->cohe_queue.size) 
    DumpLinkQueue("cohe queue",    &captr->cohe_queue, 0x13,
		  Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * Pipelines
   */
  YS__logmsg(proc_id / ARCH_cpus,
	  "num_in_pipes = %d\n",
	  captr->num_in_pipes);
  if (captr->tag_pipe[2]->count)
    DumpPipeline("request pipeline", captr->tag_pipe[2], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->tag_pipe[1]->count)
    DumpPipeline("reply pipeline", captr->tag_pipe[1], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);
  if (captr->tag_pipe[0]->count)
    DumpPipeline("cohe pipeline", captr->tag_pipe[0], 0x01,
		 Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * MSHRs
   */
  for (i = 0; i < captr->max_mshrs; i++)
    {
      MSHR *pmshr = &(captr->mshrs[i]);
      if (pmshr->valid != 1)
	continue;
      YS__logmsg(proc_id / ARCH_cpus,
	      "MSHR(%d): size = %d\n",
	      i, pmshr->counter + 1);
      Cache_req_dump(pmshr->mainreq, 0, proc_id / ARCH_cpus);
      for (j = 0; j < pmshr->counter; j++)
	Cache_req_dump(pmshr->coal_req[j], 0, proc_id / ARCH_cpus);
    }

  /*
   * What else?
   */
}




/*
 * Perform exhaustive printouts on the current state of L2 cache.
 */
void L2Cache_dump(int proc_id)
{
  CACHE *captr = L2Caches[proc_id];
  REQ   *req;
  int    i, j, index;

  YS__logmsg(proc_id / ARCH_cpus, "\n====== L2CACHE ======\n");

  /*
   * Input queues
   */
  YS__logmsg(proc_id / ARCH_cpus,
	     "inq_empty = %d\n",
	     captr->inq_empty);
  DumpLinkQueue("request queue", &captr->request_queue, 0x21,
		Cache_req_dump, proc_id / ARCH_cpus);
  DumpLinkQueue("reply queue",   &captr->reply_queue, 0x22,
		Cache_req_dump, proc_id / ARCH_cpus);
  DumpLinkQueue("cohe queue",    &captr->cohe_queue, 0x23,
		Cache_req_dump, proc_id / ARCH_cpus);
  DumpLinkQueue("noncohe queue", &captr->noncohe_queue, 0x23,
		Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * Pipelines
   */
  YS__logmsg(proc_id / ARCH_cpus,
	  "num_in_pipes = %d\n",
	  captr->num_in_pipes);
  DumpPipeline("data pipeline",    captr->data_pipe[0], 0x01,
	       Cache_req_dump, proc_id / ARCH_cpus);
  DumpPipeline("request pipeline", captr->tag_pipe[0], 0x01,
	       Cache_req_dump, proc_id / ARCH_cpus);
  DumpPipeline("reply pipeline",   captr->tag_pipe[1], 0x01,
	       Cache_req_dump, proc_id / ARCH_cpus);
  DumpPipeline("cohe pipeline",    captr->tag_pipe[2], 0x01,
	       Cache_req_dump, proc_id / ARCH_cpus);

  /*
   * MSHRs
   */
  for (i = 0; i < captr->max_mshrs; i++)
    {
      MSHR *pmshr = &(captr->mshrs[i]);
      if (pmshr->valid != 1)
	continue;
      
      YS__logmsg(proc_id / ARCH_cpus,
		 "MSHR(%d): size = %d\n",
		 i, pmshr->counter + 1);
      Cache_req_dump(pmshr->mainreq, 0, proc_id / ARCH_cpus);
      for (j = 0; j < pmshr->counter; j++)
	Cache_req_dump(pmshr->coal_req[j], 0, proc_id / ARCH_cpus);
    }

  /*
   * Control/status variables
   */
  YS__logmsg(proc_id / ARCH_cpus, "Others:");
  YS__logmsg(proc_id / ARCH_cpus,
	     "  stall_request: %d, stall_cohe: %d, stall_reply: %d\n",
	     captr->stall_request, captr->stall_cohe, captr->stall_reply);

  /*
   * system interface
   */
  YS__logmsg(proc_id / ARCH_cpus, "\n====== System Interface ======\n");
  DumpDLinkQueue("request buffer", &captr->reqbuffer,  dnext, 0x41,
		 Cache_req_dump, proc_id / ARCH_cpus);
  DumpLinkQueue("out buffer",     &captr->outbuffer,  0x42,
		Cache_req_dump, proc_id / ARCH_cpus);
  DumpLinkQueue("arb waiters",    &captr->arbwaiters, 0x43,
		Cache_req_dump, proc_id / ARCH_cpus);
  YS__logmsg(proc_id / ARCH_cpus,
	  "Arbwaiters_count = %d\n",
	  captr->arbwaiters_count);

  if (captr->pending_request)
    {
      YS__logmsg(proc_id / ARCH_cpus, "pending_request:\n");
      Cache_req_dump(captr->pending_request, 0, proc_id / ARCH_cpus);
    }
  
  if (captr->cohe_reply)
    {
      YS__logmsg(proc_id / ARCH_cpus, "cohe_reply:\n");
      Cache_req_dump(captr->cohe_reply, 0, proc_id / ARCH_cpus);
    }
  
  if (captr->data_cohe_pending)
    {
      YS__logmsg(proc_id / ARCH_cpus, "data_cohe_pending:\n");
      Cache_req_dump(captr->data_cohe_pending, 0, proc_id / ARCH_cpus);
    }
  
  if (captr->pending_writeback)
    {
      YS__logmsg(proc_id / ARCH_cpus, "pending_writeback:\n");
      Cache_req_dump(captr->pending_writeback, 0, proc_id / ARCH_cpus);
    }
}


/*
 * Cache module contains L1 cache, L1 write buffer, and L2 cache.
 */
void Cache_dump(int proc_id)
{
  WBUFFER *pwbuf = WBuffers[proc_id];
  int i;
  REQ *req;

  YS__logmsg(proc_id / ARCH_cpus,
	     "\nTotal simulated cycles: %.0f\n",
	     YS__Simtime);

  /*
   * L1 cache and write buffer
   */
  L1ICache_dump(proc_id);
  L1DCache_dump(proc_id);
  UBuffer_dump(proc_id);
  
  YS__logmsg(proc_id / ARCH_cpus, "\n====== WBuffer ======\n");
  if (pwbuf->inqueue.size == 0)
    YS__logmsg(proc_id / ARCH_cpus, "inq_empty = 1\n"); 
  else
    DumpLinkQueue("input queue", &pwbuf->inqueue, 0x31,
		  Cache_req_dump, proc_id / ARCH_cpus);

  YS__logmsg(proc_id / ARCH_cpus,
	      "num_in_pipes = %d\n",
	      pwbuf->num_in_pipes);
  DumpLinkQueue("write queue", &pwbuf->write_queue, 0x32,
		Cache_req_dump, proc_id / ARCH_cpus);
  
  /*
   * L2 cache 
   */
  L2Cache_dump(proc_id);
}
