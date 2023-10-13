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
#include "Processor/memunit.h"
#include "Processor/tlb.h"
#include "Processor/simio.h"
#include "Processor/procstate.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Caches/syscontrol.h"
#include "IO/addr_map.h"



/*=============================================================================
 * This function is called by the processor module to initiate a data memory 
 * access. Note the request queue of L1 cache is the communication channel
 * between the processor and memory system. L1Q_FULL indicates whether
 * or not the request queue is full.
 */
void DCache_recv_addr(int proc_id,
		      struct instance *inst,
		      long long inst_tag, 
		      unsigned paddr,
		      int memacctype,
		      int memaccsize,
		      int memattribute,
		      int preflevel)
{
  CACHE  *captr;
  REQ    *req   = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  captr = L1DCaches[proc_id];

  /*
   * General information
   */
  req->type              = REQUEST;
  req->prefetch          = preflevel;
  req->node              = proc_id / ARCH_cpus;
  req->src_proc          = proc_id % ARCH_cpus;
  req->prcr_req_type     = (ReqType)memacctype;
  req->req_type          = BAD_REQ_TYPE;
  req->progress          = 0;
  req->ifetch            = 0;
  
  req->parent            = NULL;
  req->paddr             = paddr;
  req->vaddr             = paddr;
  req->memattributes     = memattribute;
  req->size              = memaccsize;
  req->l1mshr            = 0;
  req->l2mshr            = 0;
  req->cohe_count        = 0;

  req->dest_proc         = AddrMap_lookup(req->node, paddr);

  /* information to identify this REQUEST to the processor simulator */
  req->d.proc_data.inst      = inst;
  req->d.proc_data.inst_tag  = inst_tag;
  req->d.proc_data.proc_id   = proc_id;

  req->perform  = PerformData;
  req->complete = MemDoneHeapInsert;


  /* for statistics */
  req->issue_time        = YS__Simtime;
  req->hit_type          = UNKHIT;
  req->line_cold         = 0;

  if (preflevel == 1)
    captr->pstats->sl1.total++;
  else if (preflevel == 2)
    captr->pstats->sl2.total++;

  /*
   * Add it to REQUEST input queue of L1 cache, which must then know that 
   * there is something on its input queue so as to be activated by 
   * cycle-by-cycle simulator. If this access fills up the port, the 
   * L1Q_FULL variable is set so that the processor does not issue any 
   * more memory references until the port clears up. 
   */
  lqueue_add(&(captr->request_queue), req, proc_id / ARCH_cpus);
  captr->inq_empty  = 0;
  L1DQ_FULL[proc_id] = lqueue_full(&(captr->request_queue));
}



extern void IO_empty_func (REQ*, ...);


/*=============================================================================
 * This function is called by the processor TLB to initiate a data memory 
 * access to satisfy a TLB miss.
 */
void DCache_recv_tlbfill(int proc_id,
			 void (*perform)(REQ*),
			 unsigned char *buf, 
			 unsigned paddr, void *tlb)
{
  CACHE  *captr;
  REQ    *req   = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  captr = L1DCaches[proc_id];

  /*
   * General information
   */
  req->type              = REQUEST;
  req->prefetch          = 0;
  req->node              = proc_id / ARCH_cpus;
  req->src_proc          = proc_id % ARCH_cpus;
  req->prcr_req_type     = READ;
  req->req_type          = BAD_REQ_TYPE;
  req->progress          = 0;
  req->ifetch            = 0;
  
  req->parent            = NULL;
  req->paddr             = paddr;
  req->vaddr             = paddr;
  req->memattributes     = 0;
  req->size              = sizeof(unsigned int);
  req->l1mshr            = 0;
  req->l2mshr            = 0;
  req->cohe_count        = 0;

  req->dest_proc         = AddrMap_lookup(req->node, paddr);

  /* information to identify this REQUEST to the processor simulator */
  req->d.mem.buf   = buf;
  req->d.mem.aux   = tlb;
  req->d.mem.count = sizeof(unsigned int);

  req->perform  = perform;
  req->complete = (void(*)(REQ*, HIT_TYPE))IO_empty_func;


  /* for statistics */
  req->issue_time        = YS__Simtime;
  req->hit_type          = UNKHIT;
  req->line_cold         = 0;

  /*
   * Add it to REQUEST input queue of L1 cache, which must then know that 
   * there is something on its input queue so as to be activated by 
   * cycle-by-cycle simulator. If this access fills up the port, the 
   * L1Q_FULL variable is set so that the processor does not issue any 
   * more memory references until the port clears up. 
   */
  lqueue_add(&(captr->request_queue), req, proc_id / ARCH_cpus);
  captr->inq_empty  = 0;
  L1DQ_FULL[proc_id] = lqueue_full(&(captr->request_queue));
}




/*=============================================================================
 * This function is called by the processor module to signal a completed
 * memory barrier to the uncached buffer
 */
void DCache_recv_barrier(int proc_id)
{
  CACHE  *captr;
  REQ    *req   = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  
  captr = L1DCaches[proc_id];

  /* General information */
  req->type              = BARRIER;
  req->prefetch          = 0;
  req->node              = proc_id / ARCH_cpus;
  req->src_proc          = proc_id % ARCH_cpus;
  req->dest_proc         = req->src_proc;
  req->prcr_req_type     = WRITE;
  req->paddr             = 0;
  req->vaddr             = 0;
  req->memattributes     = tlb_uncached(0xFFFFFFFF);
  req->size              = 0;
  req->l1mshr            = 0;
  req->l2mshr            = 0;
  req->req_type          = BAD_REQ_TYPE;
  req->progress          = 0;

  req->parent            = NULL;
  req->cohe_count        = 0;
  
  /* information to identify this REQUEST to the processor simulator */
  req->d.proc_data.inst     = NULL;
  req->d.proc_data.inst_tag = -1;
  req->d.proc_data.proc_id  = proc_id;

  req->perform  = PerformData;
  req->complete = MemDoneHeapInsert;


  /*
   * for statistics
   */
  req->issue_time        = YS__Simtime;
  req->hit_type          = UNKHIT;
  req->line_cold         = 0;


  /*
   * Add it to REQUEST input queue of L1 cache, which must then know that 
   * there is something on its input queue so as to be activated by 
   * cycle-by-cycle simulator. If this access fills up the port, the 
   * L1Q_FULL variable is set so that the processor does not issue any 
   * more memory references until the port clears up. 
   */
  lqueue_add(&(captr->request_queue), req, proc_id / ARCH_cpus);
  captr->inq_empty  = 0;
  L1DQ_FULL[proc_id] = lqueue_full(&(captr->request_queue));
}




/*=============================================================================
 * This function is called by the processor module to initiate a memory 
 * access. Note the request queue of L1 cache is the communication channel
 * between the processor and memory system. L1Q_FULL indicates whether
 * or not the request queue is full.
 */
void ICache_recv_addr(int      proc_id,
		      unsigned vaddr,
		      unsigned paddr,
		      int      count,
		      int      memattribute,
		      unsigned pstate)
{
  CACHE  *captr;
  REQ    *req   = (REQ *) YS__PoolGetObj(&YS__ReqPool);  


  captr = L1ICaches[proc_id];
  
  req->node              = proc_id / ARCH_cpus;
  req->src_proc          = proc_id % ARCH_cpus;
  req->dest_proc         = AddrMap_lookup(req->node, paddr);
  req->vaddr             = vaddr;
  req->paddr             = paddr;
  req->memattributes     = memattribute;
  req->size              = count * SIZE_OF_SPARC_INSTRUCTION;
  req->issue_time        = YS__Simtime;

  /* information to identify this REQUEST to the processor simulator */
  req->d.proc_instruction.count   = count;
  req->d.proc_instruction.proc_id = proc_id;
  req->d.proc_instruction.pstate  = pstate;
  
  req->type              = REQUEST;
  req->req_type          = BAD_REQ_TYPE;
  req->prcr_req_type     = READ;
  req->ifetch            = 1;
  req->prefetch          = 0;
  req->progress          = 0;

  req->parent            = NULL;
  req->l1mshr            = 0;
  req->l2mshr            = 0;
  req->cohe_count        = 0;

  req->hit_type          = UNKHIT;
  req->line_cold         = 0;
  
  
  /*
   * General information, skip assigning this if we got an old IFetch req.
   */
  
  req->perform  = PerformIFetch;
  req->complete = (void (*)(REQ*, HIT_TYPE))IO_empty_func;


  /*
   * Add it to REQUEST input queue of L1 cache, which must then know that 
   * there is something on its input queue so as to be activated by 
   * cycle-by-cycle simulator. If this access fills up the port, the 
   * L1Q_FULL variable is set so that the processor does not issue any 
   * more memory references until the port clears up. 
   */
  lqueue_add(&(captr->request_queue), req, proc_id / ARCH_cpus);
  captr->inq_empty  = 0;
  L1IQ_FULL[proc_id] = lqueue_full(&(captr->request_queue));
}





/*=============================================================================
 * This function is called on REPLYs.  Collect statistics, add the request 
 * to processor MemDoneHeap, and free the REQ data structures if needed.
 */
void Cache_global_perform(CACHE *captr, REQ *req, int release_req)
{
  Cache_stat_set(captr, req);

  req->perform(req);
  req->complete(req, req->hit_type);

  if ((release_req) && (!req->cohe_count))
    YS__PoolReturnObj(&YS__ReqPool, req);
}
