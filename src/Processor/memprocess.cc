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

#include <limits.h>

#define min(a, b)               ((a) > (b) ? (b) : (a))

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/req.h"
} 

#include "Processor/procstate.h"
#include "Processor/mainsim.h"
#include "Processor/branchpred.h"
#include "Processor/simio.h"
#include "Processor/fastnews.h"
#include "Processor/tagcvt.hh"
#include "Processor/active.hh"
#include "Processor/procstate.hh"
#include "Processor/memunit.h"
#include "Processor/exec.h"
#include "Processor/branchpred.hh"
#include "Processor/pagetable.h"

#include "Processor/fetch_queue.h"
#include "Processor/fastnews.h"

#include "Processor/memunit.hh"
#include "Processor/stallq.hh"


/*
 * IssuePrefetch : Send prefetches down to the memory heirarchy
 */
int IssuePrefetch(ProcState *proc, unsigned addr, int level, int excl, 
		  long long inst_tag)
{
#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Processor %d sending out %s prefetch on address %d\n",
	    proc->proc_id, excl ? "exclusive" : "shared", addr);
#endif


  DCache_recv_addr(proc->proc_id, NULL, inst_tag, addr,
		   excl ? WRITE : READ, 4, 0, level);

  return 0;
}



/*****************************************************************************/
/* Uncached Buffer - conditional flush support functions                     */
/*****************************************************************************/
 
extern "C" int IsFlushCond(instance *inst)
{
  return (inst->code.instruction == iSWAP);
}



extern "C" int CheckFlushCond(REQ* req, int count)
{
  return(req->d.proc_data.inst->rs1vali == count);
}
 

extern "C" unsigned GetProcContext(int node, int pnum)
{
  ProcState *proc = AllProcs[node * ARCH_cpus + pnum];
  
  return(proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						   PRIV_TLB_CONTEXT)]);
}

 
extern "C" void UpdateFlushCond(REQ* req, int val)
{
  req->d.proc_data.inst->rdvali = val;

  req->hit_type = IOHIT;
  req->miss_type1 = req->miss_type2 = UNCACHED;
  req->d.proc_data.inst->global_perform = 1;
}



extern "C" void CompleteFlushCond(REQ* req, int count)
{
  req->d.proc_data.inst->rdvali = count;
  
  req->hit_type = IOHIT;
  req->miss_type1 = req->miss_type2 = UNCACHED;
  req->d.proc_data.inst->global_perform = 1;
  MemDoneHeapInsert(req, (HIT_TYPE)(req->hit_type));
}



/*****************************************************************************/
/* MemDoneHeapInsert :  Wrapper around MemDoneHeap.Insert + updates stats    */
/*****************************************************************************/

extern "C" void MemDoneHeapInsert(REQ *req, HIT_TYPE hit_type)       
{
  ProcState *proc    = AllProcs[req->d.proc_data.proc_id];
  instance  *inst    = req->d.proc_data.inst;
  long long inst_tag = req->d.proc_data.inst_tag;

  if (req->prefetch)
    return;
  
#if 0
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(simout, "RSIM processor completes address %X tag %lld "
	    "inst_tag %d @ %g\n",
	    req->address, req->tag, req->d.proc_data.inst_tag, YS__Simtime);
#endif
#endif

  if (inst->tag == inst_tag)
    {
      /* otherwise, it's been reassigned as a result of exception */
      proc->MemDoneHeap.insert(proc->curr_cycle, inst, inst_tag);
      inst->miss = hit_type;
      inst->latepf = req->prefetched_late;
      return;
    }
}



/*****************************************************************************/
/* MemDoneHeapInsertSpec :  Wrapper around MemDoneHeap.Insert (speculative)  */
/*****************************************************************************/
 
 
extern "C" void* MemDoneHeapInsertSpec(REQ *req, HIT_TYPE hit_type)
{
  ProcState *proc    = AllProcs[req->d.proc_data.proc_id];
  instance *inst     = req->d.proc_data.inst;
  instance *new_inst;
  long long inst_tag = req->d.proc_data.inst_tag;


  new_inst = new instance(inst);

  if (!req->prefetch)
    {

#if 0      
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
        fprintf(simout,
                "RSIM processor completes address %ld tag %lld inst_tag %ld @ %g\n",
                req->address,
                req->tag,
                req->d.proc_data.inst_tag,
                YS__Simtime);
#endif
#endif
 
      
      // otherwise, it's been reassigned as a result of exception, or flushed
      if (inst->tag == inst_tag)
        {
          inst->global_perform = 1;
 
          proc->MemDoneHeap.insert(proc->curr_cycle, inst, inst_tag);
 
          // if (req->handled != reqL1HIT)
	  inst->miss = hit_type;
	  inst->latepf = req->prefetched_late;
        }
    }
 
  return new_inst;
}


/*****************************************************************************/
/* ReturnMemopSpec :  Return instance copy made by 'MemDoneHeapInsertSpec'   */
/*****************************************************************************/
 
 
extern "C" void ReturnMemopSpec(REQ *req)
{
  instance *inst = req->d.proc_data.inst;

  delete inst;
}




/*****************************************************************************/
/* GlobalPerform : Make the effect of reads and writes visible to the        */
/*               : simulator (read/write the unix address space)             */
/*****************************************************************************/

extern "C" void PerformData(REQ * req)
{
  instance *inst = req->d.proc_data.inst;

  if (inst == NULL ||
      //      inst->global_perform ||
      req->prefetch)
    return;

  if (req->d.proc_data.inst_tag != inst->tag)
    return;

  // to handle times when reused by exception, prediction, etc
  if (inst->vsbfwd)
    {
      if (inst->memprogress == -1)
	{  // standard for when issued
	  inst->memprogress = req->d.proc_data.inst->vsbfwd;
	  inst->vsbfwd = 0;
	  inst->global_perform = 1;
	}
      else
	{ // if instruction got killed, don't reset memprogress
	  inst->vsbfwd = 0;
	  inst->global_perform = 1;
	}
    }
  else     /* Call the instruction function in funcs.cc */
    DoMemFunc(inst, AllProcs[req->d.proc_data.proc_id]);
}




/*****************************************************************************/
/*****************************************************************************/


extern "C" void PerformIFetch(REQ * req)
{
  int        count, max_count;
  unsigned   pc;
  ProcState *proc = AllProcs[req->d.proc_instruction.proc_id];
  fetch_queue_entry fqe;
  instr     *instruction;


  if (proc->pstate != req->d.proc_instruction.pstate)
    {
      proc->fetch_done = 1;
      return;
    }

  if (proc->fetch_pc != req->vaddr)
    return;

  instruction = (instr*)req->d.proc_instruction.instructions;
  max_count = min(req->d.proc_instruction.count,
		  proc->fetch_queue_size - proc->fetch_queue->NumItems());

  count = 0;
  pc    = req->vaddr;
  while (count < max_count)
    {
      fqe.inst           = NewInstance(instruction, proc);
      fqe.exception_code = OK;
      fqe.pc             = pc;
      proc->fetch_queue->Enqueue(fqe);

      count ++;
      instruction ++;
      pc += SIZE_OF_SPARC_INSTRUCTION;
    }

  proc->fetch_done = 1;
  proc->fetch_pc   = pc;
}



/*****************************************************************************/
/* SpecLoadBufCohe: Called in systems with speculative loads whenever        */
/* a coherence comes to the L1 cache (whether an invalidation or a           */
/* replacement from L2). This function checks the coherence message          */
/* against outstanding speculative loads to determine if any needs to        */
/* be rolled back.                                                           */
/*****************************************************************************/

extern "C" int SpecLoadBufCohe(int proc_id, int paddr, SLBFailType fail)
{
  ProcState *proc = AllProcs[proc_id];
  except e_code;
  MemQLink < instance * >*slbindex;
  extern int block_mask;

  if (fail == SLB_Cohe)
    e_code = SOFT_SL_COHE;
  else if (fail == SLB_Repl)
    e_code = SOFT_SL_REPL;
  else
    YS__errmsg(proc_id / ARCH_cpus, "Unknown speculative load type");

#ifdef STORE_ORDERING

  // skip the first element, since this is the active one by SC/PC standards
  slbindex = proc->MemQueue.GetNext(NULL);

  /* this indicates whether the operations being investigated are speculative
     or not. In the case of SC, all reads other than the head of the queue
     are speculative. In PC, all reads after some other read are speculative */

  int specstart;    
  if (Processor_Consistency)
    {
      specstart = 0;
      if (slbindex && (!IsStore(slbindex->d) || IsRMW(slbindex->d)))
	specstart = 1;
    }
  else// SC
    specstart = 1;

  while ((slbindex = proc->MemQueue.GetNext(slbindex)) != NULL)
    {
      instance *d = slbindex->d;
      if (d->memprogress && specstart &&
          paddr == (d->addr & block_mask1))
	{
	  if (d->exception_code == OK || d->exception_code == SERIALIZE)
            d->exception_code = e_code;

	  d->exception_code = e_code;
	  proc->active_list.flag_exception(d->tag, e_code);
	}

      if (Processor_Consistency && !specstart && (!IsStore(d) || IsRMW(d)))
	specstart = 1;
    }

#else // RC

  // in RC you might need to even look at the first load, since you
  // might just be waiting on previous stores
  slbindex = NULL;

  long long minspec = -1;
  if (proc->LLtag >= 0)
    minspec = proc->LLtag;
  else if (proc->SLtag >= 0 && proc->SLtag < minspec)
    minspec = proc->SLtag;

  if (minspec == -1)   /* no way we're speculating */
    return 0;

  while ((slbindex = proc->LoadQueue.GetNext(slbindex)) != NULL)
    {
      instance *d = slbindex->d;
      if (d->memprogress && (d->tag > minspec) &&
          paddr == (d->addr & block_mask1d))
	{
	  if (d->exception_code == OK || d->exception_code == SERIALIZE)
            d->exception_code = e_code;

	  d->exception_code = e_code;
	  proc->active_list.flag_exception(d->tag, e_code);
	}
    }

#endif

  return 0;
}



/*****************************************************************************/
/* Wrapper functions to interface to the various MemSys routines             */
/*****************************************************************************/

extern "C" void FreeAMemUnit(int proc_id, long long tag)
{
  ProcState *proc = AllProcs[proc_id];

  // do this when you commit a request from the L1 ports
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(proc->corefile, "Freeing memunit for tag %lld\n", tag);
#endif

  proc->FreeingUnits.insert(proc->curr_cycle, uMEM);
}




extern "C" void AckWriteToWBUF(instance *inst, int proc_id)
{
  ProcState *proc = AllProcs[proc_id];

  proc->active_list.mark_done_in_active_list(inst->tag, 
					     inst->exception_code,
					     proc->curr_cycle - 1);
}



/*****************************************************************************/

extern "C" void ReplaceAddr(instance *inst, unsigned addr)
{
  inst->addr = addr;
}


/*****************************************************************************/
/* Signal I/O interrupt to processor: set pending interrupt bit and clear    */
/* halt-flag. Also, subtract the halted time from the cycle count for the    */
/* current exception level, and add halted time to total time CPU was halted.*/
/*****************************************************************************/

extern "C" void ExternalInterrupt(int proc_id, int int_num)
{
  ProcState *proc = AllProcs[proc_id];
  int n;
  long long halted;
  
  if (proc == NULL)
    return;

  if (int_num < 0 || int_num > 15)
    YS__errmsg(proc_id / ARCH_cpus, "Invalid Interrupt Type %i\n", int_num);

  proc->interrupt_pending |= 0x00000001 << int_num;
  YS__logmsg(proc_id / ARCH_cpus,
	     "%.0f Interrupt %i CPU %i\n",
	     YS__Simtime, int_num, proc_id % ARCH_cpus);

  if (proc->halt != 0)
    {
      halted = (long long)YS__Simtime - proc->start_halted;
      for (n = 0; n < MAX_EXCEPT; n++)
	if (proc->start_graduated[n] != 0)
	  proc->start_cycle[n] += halted;

      proc->total_halted += halted;
      proc->halt = 0;
    }
}


/*****************************************************************************/
/* Halt processor: set PIL to specified value and set halt-flag.             */
/* Remember current cycle for acconting.                                     */
/*****************************************************************************/

extern "C" void ProcessorHalt(int proc_id, int pil)
{
  ProcState *proc = AllProcs[proc_id];

  if (proc == NULL)
    return;

  proc->halt = 1;
  proc->start_halted = (long long)YS__Simtime;

  proc->pil = pil;
  proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_PIL)] =
    proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						       PRIV_PIL)]] = pil;
}
