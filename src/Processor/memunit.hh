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

#ifndef __RSIM_MEMUNIT_HH__
#define __RSIM_MEMUNIT_HH__


extern "C"
{
#include "Caches/cache.h"
#include "Caches/ubuf.h"
}


inline int IsLoad(instance *inst)
{
  int a = mem_acctype[inst->code.instruction];
  return (a == READ);
}


inline int IsStore(instance *inst)
{
  int a = mem_acctype[inst->code.instruction];
  return (a != READ);
}


inline int IsRMW(instance *inst)
{
  int a = mem_acctype[inst->code.instruction];
  return (a != READ) && (a != WRITE);
}


int IsUncached(instance *inst)
{
  return(tlb_uncached(inst->addr_attributes));
}


inline int StartUpMemRef(ProcState *proc, instance *inst)
{
  inst->global_perform = 0;    // clear this bit!!

  DCache_recv_addr(proc->proc_id, inst, inst->tag, inst->addr,
		   mem_acctype[inst->code.instruction],
		   mem_length[inst->code.instruction],
		   inst->addr_attributes,
		   0  /* no prefetch */);
  return 0;
}




/*************************************************************************/
/* AddToMemorySystem :  Handle memory operations and insert them in the  */
/*                   :  appropriate queues                               */
/*************************************************************************/
 

inline void AddToMemorySystem(instance * inst, ProcState * proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile, "Adding tag %lld to memory system\n", inst->tag);
#endif

  inst->prefetched = 0;
  inst->in_memunit = 1;
  inst->miss       = L1DHIT;
  inst->mem_ready  = 0;

#ifdef STORE_ORDERING
  inst->kill = 0;
  inst->limbo = 0;
  proc->MemQueue.Insert(inst);
#else
  if (IsStore(inst))
    {
      inst->mem_ready = 0;
      if (IsRMW(inst))
	proc->rmw_tags.Insert(inst->tag);
      else
	proc->st_tags.Insert(inst->tag);

      proc->StoreQueue.Insert(inst);
    }
  else
    {
      inst->kill = 0;
      inst->limbo = 0;
      proc->LoadQueue.Insert(inst);
    }
#endif

  inst->addr = 0;
  if ((IsStore(inst)) && (inst->addrdep))
    proc->ambig_st_tags.Insert(inst->tag);

  if (inst->addrdep == 0)
    CalculateAddress(inst, proc);
}



/***************************************************************************/
/* NumInMemorySystem :  return the number of elements in the memory system */
/***************************************************************************/
 
inline int NumInMemorySystem(ProcState * proc)
{
#ifndef STORE_ORDERING
  return proc->StoreQueue.NumItems() + proc->LoadQueue.NumItems() - proc->StoresToMem;
#else
  return proc->MemQueue.NumItems();
#endif
}


/*************************************************************************/
/* CalculateAddress : Calculate memory address and issue the memory oprn */
/*                    to the address generation unit                     */
/*************************************************************************/

inline void CalculateAddress(instance * inst, ProcState * proc)
{
  int rc;

  inst->addr = GetAddr(inst, proc);

  //---------------------------------------------------------------------------
  // TLB lookup

  if (PSTATE_GET_DTE(proc->pstate))
    {
      rc = proc->dtlb->LookUp(&inst->addr,
			      &inst->addr_attributes,
			      proc->log_int_reg_file[arch_to_log(proc,
								 proc->cwp,
								 PRIV_TLB_CONTEXT)],
			      mem_acctype[inst->code.instruction] == WRITE ||
			      mem_acctype[inst->code.instruction] == RMW,
			      PSTATE_GET_PRIV(proc->pstate), inst->tag);

      if (rc == TLB_MISS)
	{
	  inst->exception_code = DTLB_MISS;
	  proc->active_list.flag_exception(inst->tag, DTLB_MISS);
	  proc->active_list.mark_done_in_active_list(inst->tag, DTLB_MISS,
						     proc->curr_cycle);
	}
	
      if (rc == TLB_FAULT)
	{
	  inst->exception_code = DATA_FAULT;
	  proc->active_list.flag_exception(inst->tag, DATA_FAULT);
	  proc->active_list.mark_done_in_active_list(inst->tag, DATA_FAULT,
						     proc->curr_cycle);
	}

    }
  else
    inst->addr_attributes = 0;
      
  inst->finish_addr = inst->addr + mem_length[inst->code.instruction] - 1;

  //---------------------------------------------------------------------------

  GenerateAddress(inst, proc);
}



/*************************************************************************/
/* GetAddr: calculate the simulated address for the instruction          */
/*************************************************************************/
 
inline unsigned GetAddr(instance *memop, ProcState *proc)
{
  unsigned addr;

  if (memop->addrdep != 0)
    return 0;

  //---------------------------------------------------------------------------
  // calculate address
  
  if (memop->code.instruction == iCASA || memop->code.instruction == iCASXA)
    addr = memop->rs2vali;
  else if (memop->code.aux1)
    addr = memop->rs2vali + memop->code.imm;
  else
    addr = memop->rs2vali + memop->rsccvali;


  //---------------------------------------------------------------------------
  // check alignment

  if ((addr & (mem_length[memop->code.instruction] - 1)) != 0)
    {
      memop->exception_code = BUSERR;
      proc->active_list.flag_exception(memop->tag, BUSERR);
      proc->active_list.mark_done_in_active_list(memop->tag, BUSERR,
						 proc->curr_cycle);

      // not aligned by length. mark it a bus error. We'll check
      // this later before issuing...
    }
  
  return addr;
}


/*************************************************************************/
/* GenerateAddress : Simulate the access to the address generation unit  */
/*************************************************************************/

inline void GenerateAddress(instance * inst, ProcState * proc)
{
  if (INSTANT_ADDRESS_GENERATE)
    Disambiguate(inst, proc);
  else
    {
      if (proc->UnitsFree[uADDR] == 0)
	{
	  inst->stallqs++;
	  proc->UnitQ[uADDR].AddElt(inst, proc);
	}
      else
	issue(inst, proc);     // issue takes it through address generation
    }
}



inline void StartPrefetch(ProcState * proc)
{
  /* "Prefetch" carries the default level for prefetching, as well
     as being an indication of whether prefetching is on or not. */

  for (int pref = 0; pref < proc->prefs && proc->UnitsFree[uMEM] &&
	 !L1DQ_FULL[proc->proc_id]; pref++)
    {
      instance *prefop = proc->prefrdy[pref];
      int macc  = mem_acctype[prefop->code.instruction];
      int level = Prefetch;
      prefop->prefetched = 1;

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "%s_PREFETCH tag = %lld (%s) addr = %d\n",
		macc != READ ? "EXCL" : "SH", prefop->tag,
		inames[prefop->code.instruction], prefop->addr);
#endif

      if (PrefetchWritesToL2 && macc != READ)
	level = 2;

      IssuePrefetch(proc, prefop->addr, level, macc != READ);
      proc->UnitsFree[uMEM]--;
    }
}



/*
 * Wrapper around the actual instruction execution function. 
 */
inline void DoMemFunc(instance * inst, ProcState * proc)
{
  /*
  if (inst->global_perform)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Globally performing something that's already been "
	       "globally performed!!\n");
	       */
  /* Execute the function associated with this instruction */
  (*(instr_func[inst->code.instruction])) (inst, proc);

  inst->global_perform = 1;
}




/*************************************************************************/
/* CanPrefetch: return true if a write memory operation needs to be      */
/*            : and can be prefetched                                    */
/*************************************************************************/
 
inline int CanPrefetch(ProcState * proc, instance * memop)
{
  return ((Prefetch ) &&
          proc->prefs != proc->max_prefs &&
          memop->addr_ready &&
          !IsUncached(memop) &&
          memop->prefetched == 0);
}



/*************************************************************************/
/* AddFullToMemorySystem: a space just opened up in the memory queue, so */
/* some other instruction should be filled in (if any is waiting)        */
/*************************************************************************/

inline void AddFullToMemorySystem(ProcState * proc)
{
  instance *i;

  i = proc->UnitQ[uMEM].GetNext(proc);

  if (i != NULL)
    {
      if ((STALL_ON_FULL) && (proc->stall_the_rest == i->tag))
	{
	  // we're definitely stalled for this tag however, we might not just
          // be stalled for memqfull -- we might also have branch prediction
          // problems
	  if (i->branchdep == 2)
            proc->type_of_stall_rest = eSHADOW;
	  else if (proc->unpredbranch)
            proc->type_of_stall_rest = eBADBR;
	  else
            unstall_the_rest(proc);
	}

      i->stallqs--;
      i->strucdep = 0;
      AddToMemorySystem(i, proc);
    }
}

#endif
