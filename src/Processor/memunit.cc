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

#include <sys/time.h>
#include <limits.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/ubuf.h"
}                    

#include "Processor/procstate.h"
#include "Processor/exec.h"
#include "Processor/memunit.h"
#include "Processor/memq.h"
#include "Processor/mainsim.h"
#include "Processor/branchpred.h"
#include "Processor/simio.h"
#include "Processor/fastnews.h"
#include "Processor/memunit.hh"
#include "Processor/tagcvt.hh"
#include "Processor/stallq.hh"
#include "Processor/active.hh"
#include "Processor/procstate.hh"


#ifdef sgi
#define LLONG_MAX LONGLONG_MAX
#endif

#if !defined(LLONG_MAX)
#  define LLONG_MAX 9223372036854775807LL
#endif

static int  MemMatch       (instance * ld, instance * st);
static void IssueOp        (instance * memop, ProcState * proc);
static void IssueOpMessage (ProcState *proc, instance *memop);


int INSTANT_ADDRESS_GENERATE = 0;


/*
 * Overlap: returns 1 if there is overlap between addresse
 *        : accessed by references a and b.
 */
#define overlap(a,b) ((a->finish_addr >= b->finish_addr && \
                       b->finish_addr >= a->addr) || \
                      (b->finish_addr >= a->finish_addr && \
                       a->finish_addr >= b->addr))

/* 
 * What inst->memprogress means
 *    2            flushed
 *    1            completed
 *    0            unissued
 *    -1           issued
 *    -3 to -inf   forwarded
 */



/*#######################################################################*/

#ifdef STORE_ORDERING

/**************************************************************************/
/* IssueMem  : Get elements from memory queue and check for dependences,  */
/*           : resource availability and issue it                         */
/**************************************************************************/

int IssueMem(ProcState * proc)
{                       
  instance *memop;
  MemQLink<instance *> *index, *confindex;
  int ctr = 0, readctr = 0;

  proc->prefs = 0;     // zero prefetches so far this cycle
 
  index = proc->MemQueue.GetNext(NULL);

  do
    { 
      ctr++;
      memop = index->d;
 
      if ((IsLoad(memop) || IsRMW(memop)) && 
          (memop->code.instruction != iPREFETCH))
	readctr++;

      if (memop->memprogress)
	{   // already issued or dead
	  if (!Speculative_Loads)
            break;
	  else
            continue;
	}
 
      /* address has to be ready before going to memory unit */
      if (!memop->addr_ready || memop->truedep)
	continue;
 
      if (IsStore(memop))
	{
	  /* treat stores a certain way -- need to be at head of active list.
	     make sure that not only is st_ready set, but also store 
	     should be at top of memory unit. */

	  if (memop->mem_ready && ctr == 1)
	    {
	      if (L1Q_FULL[proc->proc_id]) 
		break;
	      else
		{
		  if (simulate_ilp && Processor_Consistency)
		    proc->ReadyUnissuedStores--;
		  IssueOp(memop, proc);
		  if (CanPrefetch(proc, memop))
		    proc->prefrdy[proc->prefs++] = memop;    
		}
	    }
	  
	  if (!Speculative_Loads)
            break;
	}
      else
	{ // this is where we handle loads 
	  if (memop->code.instruction == iPREFETCH)
	    {
	      /* software prefetch */
	      if (L1Q_FULL[proc->proc_id])
		break;
	      else
		{
		  // do this because we're going to be removing this entry
		  // in the MemQueue in the IssueOp() function. */
		  index = proc->MemQueue.GetPrev(index);
		  ctr--;
		  IssueOp(memop, proc);
		  continue;
		}
	    }
 
	  if (!(Speculative_Loads || (Processor_Consistency && readctr == 1)))
	    {
	      if (ctr == 1)
		{
		  if (L1Q_FULL[proc->proc_id])
		    break;
		  else
		    {
		      IssueOp(memop, proc);
		      if (CanPrefetch(proc, memop))
			proc->prefrdy[proc->prefs++] = memop;
		    }
		}
	      else
		break;
	    }
	  else
	    {
	      // in this case, we do have speculative load execution or this is
	      // processor consistency but there are only writes ahead of us.
 
	      int canissue = 1;
	      int specing = 0;
	      unsigned instaddr;
	      unsigned confaddr;
	      instance *conf;
	      confindex = NULL;
 
	      confindex = index;
	      while (confindex = proc->MemQueue.GetPrev(confindex))
		{
		  conf = confindex->d;
		  if (IsLoad(conf))
		    continue;
 
		  if (conf->addrdep || !conf->addr_ready)
		    {
		      specing = 1;
		      continue;
		    }
 
		  /* either stall or have a chance for a quick forwarding */
		  if (overlap(conf, memop))
		    {
		      confaddr = conf->addr;
		      instaddr = memop->addr;
		      if (confaddr == instaddr && conf->truedep == 0 && 
			  MemMatch(memop, conf))
			{
			  // varies from -3 on down!
			  memop->memprogress = -3 - conf->tag;    
			  canissue = 1; 
			}
		      else if (conf->code.instruction == iSTDF &&
			       memop->code.instruction == iLDUW &&
			       conf->truedep == 0 &&
			       ((instaddr == confaddr) ||
				(instaddr == confaddr + sizeof(int))))
			{
			  /* here's a case where we can forward just like the
			     regular case, even if it's a partial overlap. */
			  if (instaddr == confaddr)
			    {
			      unsigned *p = (unsigned*)&(conf->rs1valf);
			      memop->rdvali = *p;
			    }
			  else // (instaddr == confaddr + sizeof(int))
			    {
			      unsigned int *p = (unsigned*)&(conf->rs1valf);
			      memop->rdvali = *(p + 1);
			    }
 
			  memop->memprogress = -3 - conf->tag;
			  canissue = 1;   // we can definitely use it
			}
		      else
			{       
			  // we would be expecting a partial forward at the cache
			  memop->partial_overlap = 1;
			  memop->memprogress = 0;
			  canissue = 0;
			}
		      break;
		    }
		}
 
	      if (canissue && memop->memprogress > -3)
		{
		  if (L1Q_FULL[proc->proc_id])
		    {
		      memop->memprogress = 0;
		      canissue = 0;
		      break;
		    }
		}
 
	      if (canissue)
		{
		  proc->ldissues++;
		  if (specing)
		    proc->ldspecs++;
		  if (memop->memprogress < 0)
		    { // we got a quick forward
		      memop->issuetime = proc->curr_cycle;
		      proc->MemDoneHeap.insert(proc->curr_cycle + 1, memop, 
					       memop->tag);
		    }
		  else
		    {
		      IssueOp(memop, proc);
		      if (CanPrefetch(proc, memop))
			proc->prefrdy[proc->prefs++] = memop;
		    }
		}
	    }
	}  /* load */
    }
  while ((index = proc->MemQueue.GetNext(index)) && 
	 proc->UnitsFree[uMEM]);
 
  if (Prefetch) 
    StartPrefetch(proc);
 
  return 0;
}

#endif  /* StoreOrdering ##################################################*/




/*
 * Check for match between load and store and also forward values where 
 * possible. Returns 1 on success, 0 on failure call this function after 
 * matching load and store's address.
 */

static int MemMatch(instance * ld, instance * st) 
{
  INSTRUCTION load = ld->code.instruction;
  INSTRUCTION store = st->code.instruction;

  switch (store)
    {
    case iSTW:
      if ((load == iLDUW) || (load == iLDSW) || (load == iLDUWA))
	{
	  if (load == iLDSW)
	    ld->rdvali = (int)st->rs1vali;
	  else
	    ld->rdvali = (unsigned)st->rs1vali;

	  return 1;
	}
      else if (load == iLDF)
	{
	  int *addr = (int *) &(st->rs1vali);
	  *((int*)&(ld->rdvalfh)) = *addr;
	  return 1;
	}
      return 0;


    case iSTD:
      if (load == iLDD)
	{
	  ld->rdvalipair.a = st->rs1valipair.a;
	  ld->rdvalipair.b = st->rs1valipair.b;
	  return 1;
	}
      else
	return 0;


   case iSTB:
     if ((load == iLDUB) || (load == iLDSB))
       {
	 if (load == iLDSB)
	   ld->rdvali=(char)st->rs1vali;
	 else
	   ld->rdvali=(unsigned char)st->rs1vali;
	 
	 return 1;
       }
     else
       return 0;

     
   case iLDSTUB:
     if (load == iLDUB)
       {
         ld->rdvali = 255;
         return 1;
       }
     else
       return 0;

     
    case iSTH:
      if ((load == iLDUH) || (load == iLDSH))
	{
          if (load == iLDSH)
            ld->rdvali=(short)st->rs1vali;
          else
            ld->rdvali=(unsigned short)st->rs1vali;

	  return 1;
	}
      else
	return 0;

      
   case iSTDF:
      if (load == iLDDF)
	{
	  ld->rdvalf = st->rs1valf;
	  return 1;
	}
      else
	return 0;

      
   case iSTF:
      if (load == iLDF)
	{
	  ld->rdvalfh = st->rs1valfh;
	  return 1;
	}
      else if (load == iLDSW || load == iLDUW)
	{
	  int *p = (int*)&(st->rs1valfh);
          if (load == iLDSW)
            ld->rdvali = *p;
          else
            ld->rdvali = (unsigned int)*p;

	  return 1;
	}
      else
	return 0;

      
    default:
      break;
    }
  
  return 0;
}



/*************************************************************************/
/* IssueOp : Issue a memory operation to the cache or the perfect memory */
/*         : system                                                      */
/*************************************************************************/

static void IssueOp(instance * memop, ProcState * proc)
{
  if (memop->vsbfwd < 0)
    proc->vsbfwds++;

  proc->UnitsFree[uMEM]--;

  memop->memprogress = -1;
  memop->issuetime   = proc->curr_cycle;
  memop->time_issued = YS__Simtime;

#ifdef COREFILE
  IssueOpMessage(proc, memop);
#endif

  memory_latency(memop, proc);
}



struct MemoryRequestState
{
  instance *inst;
  ProcState*proc;

  MemoryRequestState(instance * i, ProcState * p):inst(i), proc(p) {}
};




/*************************************************************************/
/* memory_latency : models the effect of latency spent on memory issue   */
/*                : use StartUpMemReference to interface with MemSys     */
/*************************************************************************/ 

int memory_latency(instance * inst, ProcState * proc)
{
  /* just to see if we should execute it at all -- or does it 
     give a seg fault */

  if (inst->exception_code == ITLB_MISS ||
      inst->exception_code == DTLB_MISS ||
      inst->exception_code == INSTR_FAULT ||
      inst->exception_code == DATA_FAULT ||
      inst->exception_code == BUSERR ||
      inst->exception_code == PRIVILEGED ||
      (!simulate_ilp && IsStore(inst) && inst->exception_code != OK))
    {
      // the last condition takes care of places that mark_stores_ready would
      // have handled for an ILP processor
      proc->FreeingUnits.insert(proc->curr_cycle, uMEM);

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "Freeing memunit for tag %lld\n", inst->tag);
#endif

#ifndef STORE_ORDERING
      if (!simulate_ilp && IsStore(inst) && !IsRMW(inst))
	{
	  proc->active_list.mark_done_in_active_list(inst->tag, 
						     inst->exception_code,
						     proc->curr_cycle - 1);
	  return 0;
	}
#endif

      proc->MemDoneHeap.insert(proc->curr_cycle + 1, inst, inst->tag);
      return 0;
    }

#ifndef STORE_ORDERING
  if (!simulate_ilp && IsStore(inst) && !IsRMW(inst))
    {
      inst->in_memunit = 0;
      instance *memop = new instance(inst);
      proc->StoreQueue.Replace(inst, memop);
      inst = memop;
    }
#endif

  if (inst->code.instruction == iPREFETCH)
    { // send out a prefetch
      int excl  = inst->code.aux2 == PREF_1WT || inst->code.aux2 == PREF_NWT;
      int level = 1 + (Prefetch == 2 || inst->code.aux2 == PREF_1WT || 
                       inst->code.aux2 == PREF_1RD);

      if (!drop_all_sprefs)
	IssuePrefetch(proc, inst->addr, level, excl, inst->tag);

      // the prefetch instruction itself should return instantly a pref isn't
      // like other memops; it can return even without paying cache time

      inst->memprogress = 1;
      PerformMemOp(inst, proc);

#if 0
#ifdef COREFILE
      OpCompletionMessage(inst, proc);
#endif
#endif

      proc->DoneHeap.insert(proc->curr_cycle + 1, inst, inst->tag);
      return 0;
    }

  if (StartUpMemRef(proc, inst) == -1)
    /* not accepted -- should never happen */
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "StartUpMemRef rejected an operation.\n");

  return 0;
}



/*===========================================================================
 * Takes entries off the MemDoneHeap and if they are valid, inserts them 
 * on the running queue called by RSIM_EVENT().
 */
void CompleteMemQueue(ProcState * proc)
{
  instance *inst;
  long long inst_tag;

  if ((proc->MemDoneHeap.num() == 0) ||
      (proc->MemDoneHeap.PeekMin() > proc->curr_cycle))
    return;

  do
    {
      proc->MemDoneHeap.GetMin(inst, inst_tag);
 
      if (inst->tag == inst_tag)             // hasn't been flushed
        CompleteMemOp(inst, proc);           // this will insert completions
                                             // on running q

#ifdef COREFILE
      else
        {
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile,
		    "Got nonmatching tag %lld off memheap\n",
		    inst_tag);
        }
#endif
 
    }
  while (proc->MemDoneHeap.num() != 0 &&
         proc->MemDoneHeap.PeekMin() <= proc->curr_cycle);
}



/**************************************************************************/
/* CompleteMemOp : Perform the operations on completion of a memory instr */
/*               : and add to DoneHeap                                    */
/**************************************************************************/

int CompleteMemOp(instance * inst, ProcState * proc)
{
#ifdef DEBUG
  if (inst->memprogress == 2)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Flushed operation should not come to CompleteMemOp.");
#endif

   if (IsStore(inst))
     {
       //--------------------------------------------------------------------
       // STORE: Completes perfectly. Mark it done in some way, and if it's
       // the first instruction in the store q, mark all in-order dones
       // possible
       // -- Note that in the SC world, all Stores are like RMWs

#ifdef DEBUG
      if (inst->memprogress == 2)               // it has been flushed
        YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Flushed operation %s (tag=%lld) should never come to CompleteMemOp.\n",
		   inames[inst->code.instruction], inst->tag);
#endif
      
      inst->memprogress = 1;
      
#ifdef DEBUG
      if (!inst->global_perform && inst->exception_code == OK)
        YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Tag %lld (0x%08X) Processor %d was never globally performed! (addr 0x%08X)\n",
		   inst->tag, inst->pc, proc->proc_id, inst->addr);
#endif
 
      // we need to move this to the issue stage,
      // to handle proper simulation of RAW, etc
 
      if (IsRMW(inst)
#ifdef STORE_ORDERING
          || (!SC_NBWrites && !Processor_Consistency)
#endif
          )
        {
          if (inst->memprogress <= 0)
            {
              inst->issuetime = LLONG_MAX;
              return 0; // not yet accepted
            }

          PerformMemOp(inst, proc);
          // use this so that it's clearly done in the memory unit

#if 0 
#ifdef COREFILE
          OpCompletionMessage(inst,proc);
#endif
#endif
          proc->DoneHeap.insert(proc->curr_cycle, inst, inst->tag);
        }
 
      else                  // an ordinary write in RC or PC or SC w/nb writes
        {
#ifndef STORE_ORDERING
          proc->StoresToMem--;
#endif
          PerformMemOp(inst, proc);
          // use this so that it's clearly done in the memory unit

#if 0 
#ifdef COREFILE
          OpCompletionMessage(inst, proc);
#endif
#endif

      	  if (inst->inuse == 2)
	    {
	      inst->inuse = 0;
	      proc->meminstances.Putback(inst);
	    }
	  else
	    DeleteInstance(inst, proc);
	}      
      return 0; 
    }
 
 
  
  else
    {
      //---------------------------------------------------------------------
      // LOAD: On completion, first see if there are any items in
      // store q with lower tag that has same addr or unknown addr. If
      // so, we may need to redo operation. If not, mark it done and
      // if first instr in store q, mark all in-order completes possible
#ifdef DEBUG
      if (inst->memprogress == 2)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Flushed operation should not come to CompleteMemOp.\n");
      else
#endif

      if (inst->code.instruction == iPREFETCH)
        {
          YS__logmsg(proc->proc_id / ARCH_cpus,
		     "Why did we enter Complete Mem Op on a prefetch???\n");
          inst->memprogress = 1;     // don't check limbos, spec_stores, etc.
          PerformMemOp(inst, proc);
        }
 
      else
        {
          if (inst->exception_code == SOFT_SL_COHE ||
              inst->exception_code == SOFT_SL_REPL ||
              inst->exception_code == SOFT_LIMBO)
            // footnote 5 implementation -- we'll need to redo it.
            {
              if (inst->exception_code == SOFT_LIMBO)
                proc->redos++;
 
              inst->prefetched = 0;
              inst->memprogress = 0;
              inst->vsbfwd = 0;
              inst->global_perform = 0;
              inst->exception_code = OK;
              inst->issuetime = LLONG_MAX;
              return 0;
            }
          
          int redo = 0;
          
          instance *sts;
          long long lowambig;
          if (inst->kill == 1)
            {
              redo = 1;
              inst->kill = 0;
            }

          if (spec_stores == SPEC_LIMBO &&
              proc->ambig_st_tags.GetMin(lowambig) &&
              lowambig < inst->tag)      // ambiguous store before this load...
            {
              proc->limbos++;
              inst->limbo++;  // Set the limbo bit on to remember to check for this
            }

          // if so, this is all taken care of before Issue
          if (spec_stores != SPEC_STALL)
            {
              MemQLink<instance *> *stindex = NULL;

#ifndef STORE_ORDERING
              while ((stindex = proc->StoreQueue.GetNext(stindex)) != NULL)
#else
	      while ((stindex = proc->MemQueue.GetNext(stindex)) != NULL)
#endif
                {
                  sts = stindex->d;
                  if (sts->tag > inst->tag)
                    break;

#ifdef STORE_ORDERING
                  if (!IsStore(sts))
                    continue;
#endif

                  // if we got a forward, then we can ignore the other cases here
                  if (inst->memprogress <= -sts->tag-3)
                    // we got a forward from an op with greater tag
                    continue;

                  if (!sts->addr_ready && spec_stores == SPEC_EXCEPT)
                    {
		      if (inst->limbo == 0)
			proc->limbos++;
		      inst->limbo++;

#ifdef COREFILE
                      if (proc->curr_cycle > DEBUG_TIME)
                        fprintf(corefile,
                                "P%d,%lld Gambling to avoid a limbo on %lld\n",
                                proc->proc_id,
                                inst->tag,
                                sts->tag);
#endif
                      continue;
                    }

                  if (overlap(sts, inst) &&
		      (inst->issuetime < sts->issuetime))
                    {
                      proc->redos++;
                      redo = 1;
                      break;
                    }
		}
	    }
          
	  if (redo)
	    {
	      inst->memprogress = 0;         // in other words, start all over
	      inst->vsbfwd = 0;
	      inst->issuetime = LLONG_MAX;
	      inst->limbo--;
	      return 0;
	    }
 
          else
	    {
#ifdef DEBUG
	      if (inst->memprogress == -1 )         // non-forwarded
		{
		  if (!inst->global_perform && inst->exception_code == OK)
		    YS__errmsg(proc->proc_id / ARCH_cpus,
			       "Tag %lld (0x%08X) Processor %d was never globally performed (addr 0x%08X)!\n",
			       inst->tag,
			       inst->pc, 
			       proc->proc_id,
			       inst->addr);
		}
	      
              if (inst->memprogress == 0)             // should not ever happen
		YS__errmsg(proc->proc_id / ARCH_cpus,
			   "How is memprogress 0 at Complete?\n");
#endif
	      
	      if (!inst->limbo || spec_stores == SPEC_EXCEPT)     // only_specs
		{
		  if (!inst->limbo)
		    inst->memprogress = 1;
 
		  PerformMemOp(inst, proc);
		  if (!inst->limbo)

		    {
#if 0
#ifdef COREFILE
		      OpCompletionMessage(inst, proc);
#endif
#endif
		      proc->DoneHeap.insert(proc->curr_cycle, inst, inst->tag);
		    }
		}
	    }
	}

      return 0;
    }
}




/*************************************************************************/
/* PerformMemOp : Possibly remove memory operation from queue and free   */
/*              : up slot in memory system. If aggressive use of load    */
/*              : values supported past ambiguous stores, update         */
/*              : register values, but keep load in queue                */
/*************************************************************************/

void PerformMemOp(instance * inst, ProcState * proc)
{
#ifdef STORE_ORDERING
  instance *i;
#endif

#ifndef STORE_ORDERING
  if (IsRMW(inst))                 // RMW -----------------------------------
    {
      proc->rmw_tags.Remove(inst->tag);
      proc->StoreQueue.Remove(inst);
      inst->in_memunit = 0;
    }

  else if (IsStore(inst))          // stores --------------------------------
    {
      proc->st_tags.Remove(inst->tag);
      proc->StoreQueue.Remove(inst);

      // Don't add anything to memory system here because this wasn't still
      // sitting in the "real" memory queue in RC
    }
  else                             // loads ---------------------------------
    {
      if (inst->code.instruction == iPREFETCH ||
	  (!inst->limbo && (!Speculative_Loads ||
			    (!(proc->minstore < proc->SLtag &&
			       inst->tag > proc->SLtag) &&
			     !(proc->minload < proc->LLtag &&
			       inst->tag > proc->LLtag)))))
	// we enter here ordinarily, for loads that are only preceded by
	// disambiguated stores and which are not speculative
	{
          proc->LoadQueue.Remove(inst);
	  inst->in_memunit = 0;
	}

#else /* Is STORE_ORDERING! */

  if (IsStore(inst))
    { // RMW is nothing special here....
      // In sequential consistency, memory operations are not removed
      // from the memory queue until the operations above them have
      // all finished.

      int keepgoing = proc->MemQueue.GetMin(i);
 
      while (keepgoing)          // find other ones to remove also
	{
	  if (i->memprogress > 0)
	    {
	      proc->MemQueue.Remove(i);
              
	      if (simulate_ilp && !Processor_Consistency && IsStore(i))
		proc->ReadyUnissuedStores--;
 
	      i->in_memunit = 0;
	    }
	  else
	    break;
              
	  keepgoing = proc->MemQueue.GetMin(i);
	}
    }

  //-------------------------------------------------------------------------
  // Load Instruction 
  else
    {
      if (!inst->limbo || inst->code.instruction == iPREFETCH)
	{
	  // we enter here ordinarily, for accesses past
	  // disambiguated stores

	  MemQLink<instance *> *stepper = NULL, *oldstepper;
	  i = stepper->d;

	  while (stepper = proc->MemQueue.GetNext(stepper))
	    {
	      if (i->memprogress > 0)
		{
		  if (simulate_ilp && !Processor_Consistency && IsStore(i))
		    proc->ReadyUnissuedStores--;

		  oldstepper = stepper;
		  stepper = proc->MemQueue.GetPrev(stepper);
		  proc->MemQueue.Remove(oldstepper);
		  i->in_memunit = 0;
		}
	      else if (!(Processor_Consistency && IsStore(i) && !IsRMW(i)))
		break;
 
	      if ((stepper=proc->MemQueue.GetNext(stepper)) != NULL)
		i = stepper->d;
	    }
	}

#endif // STORE_ORDERING

      else
	{
	  // when we do have a limbo, we need to just pass the value on, by
	  // setting the dest reg val. The instance remains in the MemQueus

#ifdef DEBUG
	  if (inst->limbo && spec_stores != SPEC_EXCEPT)
	    /* We come here only for SPEC_EXCEPT */
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "%s:%d -- should only be entered with SPEC_EXCEPT."
		       " P%d,%lld\n",
		       __FILE__,
		       __LINE__,
		       proc->proc_id,
		       inst->tag);
#endif
	  
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile, "Sending value down from %s-load tag %lld\n",
		    inst->limbo ? "limbo" : "spec", inst->tag);
#endif
	  switch (inst->code.rd_regtype)
	    {
	    case REG_INT: 
	    case REG_INT64:
	      if (inst->prd != 0)
		proc->phy_int_reg_file[inst->prd] = inst->rdvali;
	      proc->intregbusy[inst->prd] = 0;
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	      break;

	    case REG_FP:
	      proc->phy_fp_reg_file[inst->prd] = inst->rdvalf;
	      proc->fpregbusy[inst->prd] = 0;
	      proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	      break;

	    case REG_FPHALF: 
	      {
		proc->phy_fp_reg_file[inst->prd] = inst->rsdvalf;
		float *address = (float *) (&proc->phy_fp_reg_file[inst->prd]);

		if (inst->code.rd & 1) /* the odd half */
		  address += 1;

		*address = inst->rdvalfh;
		proc->fpregbusy[inst->prd] = 0;
		proc->dist_stallq_fp[inst->prd].ClearAll(proc);
		break;
	      }

	    case REG_INTPAIR:
	      if (inst->prd != 0)
		proc->phy_int_reg_file[inst->prd] = inst->rdvalipair.a;
		  
	      proc->phy_int_reg_file[inst->prdp] = inst->rdvalipair.b;
	      proc->intregbusy[inst->prd] = 0;
	      proc->intregbusy[inst->prdp] = 0;
	      proc->dist_stallq_int[inst->prd].ClearAll(proc);
	      proc->dist_stallq_int[inst->prdp].ClearAll(proc);
	      break;

	    default:
	      break;
	    }

	  /* Do the same for rcc too. */
	  if (inst->prcc != 0)
	    proc->phy_int_reg_file[inst->prcc] = inst->rccvali;
	      
	  proc->intregbusy[inst->prcc] = 0;
	  proc->dist_stallq_int[inst->prcc].ClearAll(proc);
	}
    }
}

 
 
/**************************************************************************/
/* FlushMems  : Flush the memory queues in the event of a branch mispredn */
/**************************************************************************/

void FlushMems(long long tag, ProcState * proc)
{
  instance *junk;
  long long tl_tag;

  while (proc->ambig_st_tags.GetTail(tl_tag) &&
	 tl_tag > tag)
    proc->ambig_st_tags.RemoveTail();

  
#ifdef STORE_ORDERING
  while (proc->MemQueue.NumItems())
    {
      proc->MemQueue.GetTail(junk);
      if (junk->tag > tag)
        {
          proc->MemQueue.RemoveTail(); // DeleteFromTail(junk);
 
          if (junk->mem_ready)
            {
              // This can happen because of non-blocking writes
              // of an ordinary load that was marked ready past a done load
              // that is stuck in the memunit (in which case we rely on
              // the "ctr==0" requirement to stall issue)
 
              if (simulate_ilp)
                proc->ReadyUnissuedStores--;
            }
          
          if (junk->in_memunit == 0)
            {
#ifdef DEBUG
              if (!IsStore(junk) || IsRMW(junk))
		YS__errmsg("How can tag %lld be flushed from the memunit, even after it has already been graduated?\n", junk->tag);
                  // stores can come out of the memunit by being marked ready,
                  // and that's ok even with NB writes
#endif
	      
#ifdef COREFILE
              if (YS__Simtime > DEBUG_TIME)
                fprintf(corefile,
                        "Deleting memop for tag %lld which is not in_memunit.\n",
                        junk->tag);
#endif

              if (junk->inuse == 2)
		{
		  junk->inuse = 0;
		  proc->meminstances.Putback(junk);
		}
	      else
		DeleteInstance(junk, proc);
            }
          
          junk->memprogress = 2;
        }
      else
        break;
    }
 
  //-------------------------------------------------------------------------
  // ifdef STORE_ORDERING
#else

  
  while (proc->StoreQueue.NumItems())
    {
      proc->StoreQueue.GetTail(junk);
 
      if (junk->tag > tag)
        {
          proc->StoreQueue.RemoveTail();         // DeleteFromTail(junk);
          junk->memprogress = 2;
        }
      else
        break;
    }
 
  
  while (proc->LoadQueue.NumItems())
    {
      proc->LoadQueue.GetTail(junk);
      if (junk->tag > tag)
        {
          proc->LoadQueue.RemoveTail();          // DeleteFromTail(junk);
          junk->memprogress = 2;                 // mark this for done ones
        }
      else
        break;
    }
 
  
  while (proc->st_tags.GetTail(tl_tag) && tl_tag > tag)
    proc->st_tags.RemoveTail();
 
  
  while (proc->rmw_tags.GetTail(tl_tag) && tl_tag > tag)
    proc->rmw_tags.RemoveTail();
 
  
  int mbchg = 0;
  MembarInfo mb;
 
  
  while (proc->membar_tags.GetTail(mb) && mb.tag > tag)
    {
      if (mb.SYNC)
	proc->sync = 0;

      proc->membar_tags.RemoveTail();
      mbchg = 1;
 
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
        fprintf(corefile,"Flushing out membar %lld\n", mb.tag);
#endif
 
    }
  
  if (mbchg)
    ComputeMembarQueue(proc);
#endif
}




/*************************************************************************/
/* Disambiguate : Called when the address is newly generated for an      */
/*              : instruction. Checks if newly disambiguated stores      */
/*              : conflict with previously issued loads                  */
/*************************************************************************/

void Disambiguate(instance * inst, ProcState * proc)
{
  inst->addr_ready = 1;
  inst->time_addr_ready = YS__Simtime;

  if (!IsStore(inst))
    return;

  if (!simulate_ilp)
    inst->mem_ready = 1;

  instance *conf;
  MemQLink<instance *> *ldindex = NULL;      
  long long lowambig = LLONG_MAX;


  proc->ambig_st_tags.Remove(inst->tag);
  proc->ambig_st_tags.GetMin(lowambig);

  proc->LoadQueue.GetMin(conf);
  

#ifndef STORE_ORDERING
  while ((ldindex = proc->LoadQueue.GetNext(ldindex)) != NULL)
#else
  while ((ldindex = proc->MemQueue.GetNext(ldindex)) != NULL)
#endif
    {
      conf = ldindex->d;

#ifdef STORE_ORDERING
      if (IsStore(conf))
	continue;
#endif

      if (conf->tag >= inst->tag)
	{
	  if (conf->limbo)
	    {
	      if (overlap(conf, inst))
		{
		  conf->limbo--;

		  if (spec_stores == SPEC_EXCEPT)
		    {
		      if (conf->exception_code == OK ||
			  conf->exception_code == SERIALIZE)
			{
			  // mark this a soft exception
			  conf->exception_code = SOFT_LIMBO;
			  
			  proc->active_list.flag_exception(conf->tag,
							   SOFT_LIMBO);
			}
 
		      // the following needs to be done regardless
		      // of the exception code (ie, you may have
		      // already had an exception caused by
		      // SOFT_SL)
                          
		      // mark this as being "done" so that way it
		      // actually graduates
 
		      MemQLink<instance *> *confindex = ldindex;

#ifndef STORE_ORDERING
		      ldindex = proc->LoadQueue.GetPrev(ldindex);
		      proc->LoadQueue.Remove(confindex);
#else
		      ldindex = proc->MemQueue.GetPrev(ldindex);
		      proc->MemQueue.Remove(confindex);
#endif
 
		      conf->in_memunit = 0;
 
		      // We can bring something into the memqueue
		      // right now, but we don't have to, since
		      // that's definitely going to get flushed
 
		      proc->DoneHeap.insert(proc->curr_cycle, conf, conf->tag);
		    }
		  
		  else if (spec_stores == SPEC_LIMBO)
		    {
                      conf->memprogress = 0;
		      conf->vsbfwd = 0;
		      proc->redos++;               // going to have to redo it
		      conf->issuetime = LLONG_MAX;
		    }

#ifdef DEBUG
		  else
		    YS__errmsg(proc->proc_id / ARCH_cpus,
			       "Disambiguate limbo redos should not be seen with SPEC_STALL?\n");
#endif
		}    // if (overlap(inst, conf))

	      else if (lowambig > conf->tag)
		{
		  proc->unlimbos++;
		  conf->limbo--;
		  
		  conf->memprogress = 1;
		  if (!conf->limbo)
		    {
#ifndef STORE_ORDERING
		      ldindex = proc->LoadQueue.GetPrev(ldindex);
#else
		      ldindex = proc->MemQueue.GetPrev(ldindex);
#endif
		      proc->DoneHeap.insert(proc->curr_cycle, conf, conf->tag);
		    }

		  PerformMemOp(conf, proc);
		}
	      
	      else
		{
		  conf->limbo --;

		  if (!conf->limbo)
		    {
		      conf->memprogress = 1;
		      conf->in_memunit = 0;
#ifndef STORE_ORDERING
		      ldindex = proc->LoadQueue.GetPrev(ldindex);
		      proc->LoadQueue.Remove(conf);
#else
		      ldindex = proc->MemQueue.GetPrev(ldindex);
		      proc->MemQueue.Remove(conf);
#endif
		      proc->DoneHeap.insert(proc->curr_cycle, conf, conf->tag);
		    }
		}
	    }

	  else if ((conf->memprogress < 0) &&
		   overlap(conf, inst))
	    {
	      proc->kills++;
	      if (spec_stores == SPEC_EXCEPT)
		{
		  if (conf->exception_code == OK ||
		      conf->exception_code == SERIALIZE)
		    {
		      conf->exception_code = SOFT_LIMBO;
		      proc->
			active_list.flag_exception(conf->tag, SOFT_LIMBO);
		    }
		}
	      else if (spec_stores == SPEC_LIMBO)
		conf->kill = 1;
#ifdef DEBUG
	      else
		YS__errmsg(proc->proc_id / ARCH_cpus,
			   "Should never get a disambiguate kill on a SPEC_STALL?\n");
#endif
	    }
	}
    }
}




/*#########################################################################*/


#ifndef STORE_ORDERING


/**************************************************************************/
/**************************************************************************/
 
 
int IssueBarrier(ProcState *proc)
{
  if (!L1DQ_FULL[proc->proc_id])
    {
      DCache_recv_barrier(proc->proc_id);
      return 1;
    }
  else
    return 0;     // was 0 - error
}

void IssueLoads(ProcState*);
void IssueStores(ProcState*);


/*
 * Called every cycle by IssueQueues() -- calls IssueLoads, IssueStores,
 * and IssuePrefetch. Also responsible for maintaining MEMBAR status.
 */
int IssueMem(ProcState * proc)
{                       
  instance *memop;
  long long minrmw = LLONG_MAX;
  proc->minload = LLONG_MAX, proc->minstore = LLONG_MAX;

  /* these will leave min-values alone if nothing there */
  if (proc->LoadQueue.GetMin(memop))
    proc->minload = MIN(proc->minload, memop->tag);

  proc->st_tags.GetMin(proc->minstore);        
  proc->rmw_tags.GetMin(minrmw);

  proc->minload  = MIN(proc->minload, minrmw);
  proc->minstore = MIN(proc->minstore, minrmw);

  MembarInfo mb;
  int mbchg = 0;

  while (proc->membar_tags.GetMin(mb))
    {
      if ((!(mb.LL || mb.LS) || proc->minload > mb.tag) &&
          (!(mb.SL || mb.SS) || proc->minstore > mb.tag) &&
          (!mb.MEMISSUE || (proc->minstore>mb.tag && proc->minload>mb.tag)) &&
	  (!mb.MEMISSUE || UBuffers[proc->proc_id]->num_entries == 0) &&
	  !mb.SYNC)
	{
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile, "Breaking down membar %lld\n", mb.tag);
#endif
	  if (!IssueBarrier(proc))
	    return 0;

	  mbchg = 1;
	  proc->membar_tags.Remove(mb);
	}
      else
	break;
    }

  if (mbchg)
    {
      ComputeMembarQueue(proc);

      if (Speculative_Loads)
	{
	  // We need to go through the Load Queue and find done items.
	  MemQLink<instance *> *ldindex = NULL;

	  while ((ldindex = proc->LoadQueue.GetNext(ldindex)) != NULL &&
		 (proc->minstore >= proc->SLtag || 
		  ldindex->d->tag <= proc->SLtag) &&
		 (proc->minload >= proc->LLtag || 
		  ldindex->d->tag <= proc->LLtag))
	    {
	      instance *memop2 = ldindex->d;

	      if (memop2->memprogress == 1 && !memop2->limbo)
		{
		  /* Get next one out first before this one is removed. */
		  ldindex = proc->LoadQueue.GetPrev(ldindex);      

		  /* This function will remove the access from the LoadQueue */
		  PerformMemOp(memop2, proc);      
		}
	    }
	}
    }

  proc->prefs = 0;     // zero prefetches so far this cycle

  IssueStores(proc);
  IssueLoads(proc);

  if (Prefetch) 
    StartPrefetch(proc);

  return 0;
}



/*************************************************************************/
/* ComputeMembarQueue : reset SS/LS/LL/SL tags based on membar queue     */
/*************************************************************************/

void ComputeMembarQueue(ProcState * proc)
{
  // guaranteed to be out of range
  proc->SStag =
    proc->LStag =
    proc->SLtag =
    proc->LLtag =
    proc->MEMISSUEtag =
    -1;

  MemQLink<MembarInfo> *stepper = NULL;
 
  while ((stepper = proc->membar_tags.GetNext(stepper)) != NULL)
    {
      if (stepper->d.SS && proc->SStag == -1)
        proc->SStag = stepper->d.tag;
 
      if (stepper->d.LS && proc->LStag == -1)
        proc->LStag = stepper->d.tag;
 
      if (stepper->d.SL && proc->SLtag == -1)
        proc->SLtag = stepper->d.tag;
 
      if (stepper->d.LL && proc->LLtag == -1)
        proc->LLtag = stepper->d.tag;
 
      if (stepper->d.MEMISSUE && proc->MEMISSUEtag == -1)
        proc->MEMISSUEtag = stepper->d.tag;

      if (stepper->d.SYNC && !proc->sync)
        proc->sync = 1;
    }
}


/***************************************************************************/
/* ComputeMembarInfo : On a memory barrier instruction, suitably updates   */
/*                   : the processor's memory-barrier implementation flags */
/***************************************************************************/

void ComputeMembarInfo(ProcState * proc, const MembarInfo & mb)
{
   if (mb.SS && proc->SStag == -1)
     proc->SStag = mb.tag;

   if (mb.LS && proc->LStag == -1)
     proc->LStag = mb.tag;

   if (mb.SL && proc->SLtag == -1)
     proc->SLtag = mb.tag;

   if (mb.LL && proc->LLtag == -1) 
     proc->LLtag = mb.tag;

   if (mb.MEMISSUE && proc->MEMISSUEtag == -1) 
     proc->MEMISSUEtag = mb.tag;
}



/**************************************************************************/
/* IssueStores : Get elements from store queue and check for dependences, */
/*             : resource availability and issue it                       */
/**************************************************************************/

void IssueStores(ProcState * proc)
{
  /* first do loads, then stores */
  instance *memop;
  int confstore = 0;
  MemQLink<instance *> *stindex = NULL, *stindex2 = NULL;

  /* NOTE: DON'T ISSUE ANY STORES SPECULATIVELY */

  while ((proc->UnitsFree[uMEM]) &&
	 ((stindex = proc->StoreQueue.GetNext(stindex)) != NULL))
    {
      memop = stindex->d;

      /* Is the instruction already issued? */
      if (memop->memprogress)   
	continue;

      /* All memory issues are prohibited? */
      if (proc->MEMISSUEtag >= 0 &&
	  memop->tag > proc->MEMISSUEtag) 
	break;         

      if (!memop->mem_ready || confstore)
	{
	  if (CanPrefetch(proc, memop))
	    /* Prefetch subsequent ones, regardless of whether we could or 
	       couldn't for this one... */
	    proc->prefrdy[proc->prefs++] = memop;
	  
	  continue;      
	}

      if (!memop->addr_ready)
	{
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile, "store tag %lld store ready but not address "
		    "ready!\n", memop->tag);
#endif
	  /* Found the first non-read writes. We might be able to prefetch 
	     some later ones, but don't issue any demand stores */
	  confstore = 1;
	  continue;      
	}

      int canissue = 1;
      instance *conf;

      stindex2 = NULL;
      while ((stindex2 = proc->StoreQueue.GetNext(stindex2)) != NULL)
	{
	  conf = stindex2->d;
	  if (conf->tag >= memop->tag)
	    break;

	  // block this one if there is a previous unissued one with
	  // overlapping address....

	  if (conf->memprogress == 0 && overlap(conf, memop))
	    {
	      canissue = 0;
	      break;
	    }
	}

      if (canissue)
	{
	  /* the following checks to implement MEMBAR consistency */
	  if (((memop->tag > proc->SStag && proc->minstore < proc->SStag) ||
	       (memop->tag > proc->LStag && proc->minload < proc->LStag) ||
	       (IsRMW(memop) &&
		(memop->tag > proc->SLtag && proc->minstore < proc->SLtag) ||
		(memop->tag > proc->LLtag && proc->minload < proc->LLtag))))
            {
              canissue = 0;

	      if (CanPrefetch(proc, memop))
		proc->prefrdy[proc->prefs++] = memop;

	      continue;   
	    }
	}

      // Now we've passed all the semantic reasons why we can't issue it.
      // So, now we need to check for hardware constraints
 
      // check for structural hazards on input ports of cache
      if (L1DQ_FULL[proc->proc_id])
        {
          canissue = 0;
          memop->memprogress = 0;
          break;
        }

      if (canissue)
	{
	  if (!IsRMW(memop))
	    proc->StoresToMem++;
	  
	  if (simulate_ilp)
	    proc->ReadyUnissuedStores--;
	  
	  IssueOp(memop, proc);
	}
    }
}



/*************************************************************************/
/* IssueLoads : Get elements from load queue and check for forwarding    */
/*            : resource availability and issue it                       */
/*************************************************************************/

void IssueLoads(ProcState * proc)
{
  instance *memop;
  MemQLink<instance *> *stindex = NULL, *ldindex = NULL;

  while ((proc->UnitsFree[uMEM]) &&
	 ((ldindex = proc->LoadQueue.GetNext(ldindex)) != NULL))
    {
      memop = ldindex->d;

      if (memop->memprogress)   // instruction is already issued
	continue;

      if (proc->MEMISSUEtag >= 0 &&
	  memop->tag > proc->MEMISSUEtag)
	break;

      if ((IsUncached(memop)) &&
          (((proc->SStag > 0) && (memop->tag > proc->SStag)) ||
           ((proc->LStag > 0) && (memop->tag > proc->LStag)) ||
           ((proc->LLtag > 0) && (memop->tag > proc->LLtag))))
        break;

      if (memop->addr_ready &&
	  memop->truedep != 0)
	{
	  /* a load of an fp-half can have a true dependence on the rest of the
	     fp register. In the future, it's possible that such a load should
	     be allowed to issue but not to complete.  */

	  /* The processor should try to prefetch it, if it has that support.
	     Otherwise, this one can't issue yet. In the case of stat_sched,
	     neither can anything after it. */

	  if (CanPrefetch(proc, memop))
	    {
	      proc->prefrdy[proc->prefs++] = memop;
	      continue;   /* can issue or prefetch later loads */
	    }

	  if (stat_sched)
            break;
	}


      /*********************************************/
      /* Memory operations that are ready to issue */
      /*********************************************/
      if ((memop->truedep == 0) &&
          (memop->addr_ready) &&
          ((IsUncached(memop) && memop->mem_ready) ||
           (!IsUncached(memop))))
        {
	  int canissue = 1;
	  int specing = 0;
	  unsigned instaddr = memop->addr;
	  instance *conf;
	  unsigned confaddr;
	  stindex = NULL;


	  //-----------------------------------------------------------------
	  // prefetches
	  if (memop->code.instruction == iPREFETCH)
	    {
	      if (L1DQ_FULL[proc->proc_id])      /* ports are taken */
		break;
                // no further accesses will be allowed till ports free up 
	      else
		{
		  ldindex = proc->LoadQueue.GetPrev(ldindex);      
		  /* do this because we're going to be removing this entry 
		     in the loadqueue in the IssueOp function */
		  IssueOp(memop, proc);
		  continue;
		}
	    }

	  //-----------------------------------------------------------------
	  // check for earlier (non-issued) stores
	  while ((stindex = proc->StoreQueue.GetNext(stindex)) != NULL)
	    {
	      conf = stindex->d;

	      if (conf->tag > memop->tag)
		break;

	      if (!conf->addr_ready)
		{
		  specing = 1;

		  if (spec_stores == SPEC_STALL)
		    {
                      // now's the time to give up, if so
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      return;
		    }
		  continue;
		}

	      confaddr = conf->addr;
	      if (overlap(conf, memop))
		{
                  // ok, either we stall or we have a chance for a quick
		  // forwarding.
                  if (confaddr == instaddr &&
                      conf->truedep == 0 && 
                      MemMatch(memop, conf) && 
                      !((!Speculative_Loads &&
                         ((memop->tag > proc->SLtag && conf->tag < proc->SLtag) ||
                          (memop->tag > proc->LLtag && proc->minload < proc->LLtag)))))
		    
		    // no forward across membar unless there is specload
		    // support. note: specload membar is "speculative" across
		    // a LL or SL, but we _actually_ enforce the MemIssue ones
                    {
		      if (conf->memprogress != 0)
			{
			  // a forward from VSB of something that actually
			  // will be GloballyPerformed
			  memop->vsbfwd = -conf->tag - 3;
			  memop->memprogress = 0;
			}
		      else
			{
			  // if we accidentally counted it as a vsbfwd earlier,
			  // count it properly now
			  memop->vsbfwd = 0;         
			  // varies from -3 on down!
			  memop->memprogress = -conf->tag - 3; 
			}

		      canissue = 1;         // we can definitely use it
		      continue;
		    }
		  
                  else if (conf->code.instruction == iSTDF &&
                           memop->code.instruction == iLDUW &&
                           conf->truedep == 0 &&
                           ((memop->addr == conf->addr) ||
                            (memop->addr == conf->addr + sizeof(int))) &&
                           !((!Speculative_Loads &&
                              ((memop->tag > proc->SLtag && conf->tag < proc->SLtag) ||
                               (memop->tag > proc->LLtag && proc->minload < proc->LLtag)))))
		    {
                      // here's a case where we can forward just like the
		      // regular case, even if it's a partial overlap.
		      // I mention this case because it's
                      // so common, appearing in some libraries all the time
		      
		      if (conf->memprogress != 0)
			{
			  // a forward from VSB of something that actually will be
			  // GloballyPerformed -- only way it won't is if it gets
			  // flushed
			  memop->vsbfwd = -conf->tag - 3;
			  memop->memprogress = 0;
			}
		      else
			{
			  // if we accidentally counted it as a vsbfwd earlier,
			  // count it properly now
			  memop->vsbfwd = 0; 
			  // varies from -3 on down!
			  memop->memprogress = -conf->tag - 3; 
			}

		      if (memop->addr == conf->addr)
			{
			  int *ip = (int*)(&conf->rs1valf);
			  int *dp = &(memop->rdvali);
			  *dp = *ip;
			}
		      else if (memop->addr == conf->addr + sizeof(int))
			{
			  int *ip = (int*)(&conf->rs1valf) + 1;
			  int *dp = &(memop->rdvali);
			  *dp = *ip;
			  //			memop->rdvali = *(((int *) (&conf->rs1valf)) + 1);
			}
#ifdef DEBUG
		      else
			/* a partial overlap of STDF and LDUW that can't be
			   handled. Most probably misalignment or other problems
			   involved. Checked for this case before entering here. */
			YS__errmsg(proc->proc_id / ARCH_cpus,
				   "Partial overlap of STDF and LDUW which "
				   "couldn't be recognized: should've been checked.\n");
#endif
		      
		      canissue = 1;         // we can definitely use it
		      continue;
		    }
		  else if (conf->memprogress == 0)
		    {         // the store hasn't been issued yet
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      canissue = 0;
		      continue;
		    }
		  else if (IsRMW(conf))
		    {  
		      // you can't go in parallel with any RMW (except of
		      // course for simple LDSTUBs which you forward)
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      canissue = 0;
		      continue;
		    }
		  
                  else if (!(((memop->tag > proc->SLtag &&
                               proc->minstore < proc->SLtag) ||
                              (memop->tag > proc->LLtag &&
                               proc->minload < proc->LLtag))))
		    {
		      /* in other words, this isn't a case of trying to forward
			 across a membar for now we'll actually block out partial
			 overlaps (except for the special case we handled above
			 STDF-LDUW), and count them when they graduate */
		      
		      memop->memprogress = 0;
		      memop->vsbfwd = 0;
		      canissue = 0;
		      memop->partial_overlap = 1;
#ifdef COREFILE
		      if (YS__Simtime > DEBUG_TIME)
			fprintf(corefile, "P%d @<%d>: Partially overlapping "
				"load tag %d[%s] @(%d) conflicts with tag "
				"%d[%s] @(%d)\n", proc->proc_id, proc->curr_cycle,
				memop->tag, inames[memop->code.instruction], 
				instaddr, conf->tag, 
				inames[conf->code.instruction], confaddr);
#endif
		      continue;
		    }
		}
	    }

	  if (canissue && memop->memprogress > -3)
	    {
              if ((!Speculative_Loads &&
		   ((memop->tag > proc->SLtag && proc->minstore < proc->SLtag) ||
		    (memop->tag > proc->LLtag && proc->minload < proc->LLtag))))
                {
		  // so we're stuck at an ACQ membar well, if we have prefetch,
		  // let's go ahead and prefetch reads
		  memop->vsbfwd = 0;
		  memop->memprogress = 0;
		  canissue = 0;
		  
		  if (CanPrefetch(proc, memop))
		    {
		      proc->prefrdy[proc->prefs++] = memop;
		      continue;
		    }
		  continue;        /* in case we have some prefetches or anything
				      like that */
		}
	      
              // Check if L1 ports are busy before issuing
              if (L1DQ_FULL[proc->proc_id])
                {
                  canissue = 0;
                  memop->vsbfwd = 0;
                  memop->memprogress = 0;
                  break;
                }
	    }

	  if (canissue)
	    {
	      proc->ldissues++;
	      
	      if (specing)
		proc->ldspecs++;

	      if (memop->memprogress < 0)         // we got a quick forward
		{
		  proc->fwds++;
		  memop->issuetime = proc->curr_cycle;
		  // WE GET THE FORWARD VALUE IN THE MemMatch FUNCTION
		  proc->MemDoneHeap.insert(proc->curr_cycle + 1, memop, memop->tag);
		}
	      else
		IssueOp(memop, proc);
	    }
	}
    }
}

#endif

#ifdef COREFILE


/*************************************************************************/
/* OpCompletionMessage : print debug information at inst completion time */
/*************************************************************************/

static void OpCompletionMessage(instance * inst, ProcState * proc)
{
  if (proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile, "Completed executing tag = %lld, %s, ", 
              inst->tag, inames[inst->code.instruction]);

      switch (inst->code.rd_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rdi = %d, ", inst->rdvali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rdf = %f, ", inst->rdvalf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rdfh = %f, ", double (inst->rdvalfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rdp = %d/%d, ", inst->rdvalipair.a, 
		  inst->rdvalipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rdll = %lld, ", inst->rdvalll);
	  break;
	default:
	  fprintf(corefile, "rdX = XXX, ");
	  break;
	}

      fprintf(corefile, "rccvali = %d \n", inst->rccvali);
    }
}


static void IssueOpMessage(ProcState *proc, instance *memop)
{
  if (proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile, "Consuming memunit for tag %lld\n", memop->tag);
      fprintf(corefile, "Issue tag = %lld, %s, ", memop->tag, 
              inames[memop->code.instruction]);

      switch (memop->code.rs1_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rs1i = %d, ", memop->rs1vali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rs1f = %f, ", memop->rs1valf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rs1fh = %f, ", double (memop->rs1valfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rs1p = %d/%d, ", memop->rs1valipair.a, 
		  memop->rs1valipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rs1ll = %lld, ", memop->rs1valll);
	  break;
	default:
	  fprintf(corefile, "rs1X = XXX, ");
	  break;
	}

      switch (memop->code.rs2_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rs2i = %d, ", memop->rs2vali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rs2f = %f, ", memop->rs2valf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rs2fh = %f, ", double (memop->rs2valfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rs2pair unsupported");
	  break;
	case REG_INT64:
	  fprintf(corefile, "rs2ll = %lld, ", memop->rs2valll);
	  break;
	default:
	  fprintf(corefile, "rs2X = XXX, ");
	  break;
	}
      
      fprintf(corefile, "rscc = %d, imm = %d\n", memop->rsccvali, 
              memop->code.imm);
    }
}

#endif

