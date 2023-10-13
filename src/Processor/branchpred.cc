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

extern "C"
{
#include "sim_main/simsys.h"
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
#include "Processor/stallq.hh"
#include "Processor/branchpred.hh"
#include "Processor/memunit.hh"



bptype BPB_TYPE  = TWOBIT;
int    BPB_SIZE  = 512;
int    RAS_STKSZ = 4;


/*
 * decode_branch_instruction : called the first time a branch instruction 
 *                           : is encountered, sets all the relevant fields
 */
int decode_branch_instruction(instr * instrn,
			      instance * inst,
			      ProcState * proc)
{
  // Translate this branch instruction, sets inst->branch_pred & inst->taken
  StartCtlXfer(inst, proc);    

  if (instrn->uncond_branch)
    {
      // This is an unconditional branch.  For unconditional branches,
      // if Annul bit is 0, Instruction in delay slot is executed always;
      // if Annul bit is 1, Instruction in delay slot is not executed.

      if (instrn->annul == 0)
	{
	  // Non anulled unconditional branches
	  switch (inst->taken)
	    {
	    case 1:                             // speculatively taken
	      proc->copymappernext = 1;
	      // NO break here

	    case 2:     // non-speculatively taken; no need to copy mappers
	      proc->pc  = proc->npc;
	      proc->npc = inst->branch_pred;
              break;

	    case 3:     // non-predictable, so not speculated
	      proc->pc = proc->npc;
	      proc->npc = inst->branch_pred;      // will be -1
	      inst->branchdep = 1;
	      proc->unpredbranch = 1;
	      return 1;

	    case 0:     // speculatively not taken

	    default:
	      YS__errmsg(proc->proc_id / ARCH_cpus,
			 "decode_branch_instr: bad case %d for uncond branch"
			 " P %d, tag %lld, time %d\n",
			 inst->taken,
			 proc->proc_id, 
			 inst->tag,
			 proc->curr_cycle);
	    }
	}
      else
	{
	  // Anulled unconditional branches
	  switch (inst->taken)
	    {
	    case 1:     // speculatively taken
	      inst->branchdep = 2;        // shadow mapping again
	      // Note no break

	    case 2:     // non-speculatively taken
	      proc->pc  = inst->branch_pred;
	      proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
	      break;

	    case 3:     // I do not know what to do
	      proc->pc = inst->branch_pred;    // it will be -1
	      inst->branchdep = 1;
	      proc->unpredbranch = 1;
	      break;

	    case 0:     // speculatively not taken

	    default:
	      YS__errmsg(proc->proc_id / ARCH_cpus,
			 "decode_branch_instr: bad case %d for uncond branch"
			 " P %d, tag %lld, time %d\n",
			 inst->taken,
			 proc->proc_id, 
			 inst->tag,
			 proc->curr_cycle);
	    }
	}
    }
  else
    {
      // This is a conditional branch
      // If annul bit is 0, instruction in delay slot executed always.
      // If annul bit is 1, delay slot not executed unless branch is taken.

      if (instrn->annul == 0)
	{
	  // Non annulled conditional branches
	  // The shadow mapping will have to be done in the next instruction
	  // ie, at the delay slot. lets make a note of it.

	  proc->copymappernext = 1;
	  proc->pc = proc->npc; 

	  switch (inst->taken)
	    {
	    case 0:     // speculatively not-taken
	      proc->npc = proc->npc + SIZE_OF_SPARC_INSTRUCTION;
	      break;

	    case 1:     // speculatively taken
	      proc->npc = inst->branch_pred;
	      break;

	    case 2:     // This should not occur here.
	                // This means taken for sure 

	    case 3:     // This should not occur here. This means unpredicted

	    default:
	      YS__errmsg(proc->proc_id / ARCH_cpus,
			 "decode_branch_instr: case %d for cond branch "
			 " P%d, tag %lld, time %d\n",
			 inst->taken,
			 proc->proc_id, 
			 inst->tag,
			 proc->curr_cycle);
	    }
	}
      else
	{
	  // Anulled conditional branches
	  switch (inst->taken)
	    {
	    case 0:     // Speculatively not taken
	      proc->pc = proc->npc + SIZE_OF_SPARC_INSTRUCTION;
	      // delay slot not taken
	      proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
	      inst->branchdep = 2;
	      break;

	    case 1:     // Speculatively taken
	      proc->pc = proc->npc;
	      proc->npc = inst->branch_pred;
	      // We copy the shadow map here because of the possibility of 
	      // the delay slot getting squashed
	      inst->branchdep = 2;        
	      break;

	    case 2:  // This should not occur here. This means taken for sure
	    case 3:  // This should not occur here. This means unpredicted

	    default:
	      YS__errmsg(proc->proc_id / ARCH_cpus,
			 "decode_branch_instr: case %d for cond branch P %d,"
			 " tag %lld, time %d\n",
			 inst->taken,
			 proc->proc_id, 
			 inst->tag,
			 proc->curr_cycle);
	    }              // End of switch
	}                  // End of is annulled or not
    }                      // End of is unconditional or not?

  return 0;
}




/*
 * GoodPrediction : Free up shadow mapper and update processor speculation 
 *                : level once we know that the prediction was correct      
 */
void GoodPrediction(instance * inst, ProcState * proc)
{
  if (inst->code.uncond_branch == 4)                  // if it's a RAS access
    proc->ras_good_predicts++;
  else
    proc->bpb_good_predicts++;

  long long tag_to_use = inst->tag;
  instance *inst2;

  if (inst->code.annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;

  if (tag_to_use == GetTailInst(proc)->tag + 1)
    {
      // We havent issued the delay slot yet!
      proc->copymappernext = 0;  // We dont need to copy the mappers now
      return;
    }

  // Remove the entry from the Branch Q
  if (RemoveFromBranchQ(tag_to_use, proc) != 0)
    {
      if (inst2->branchdep == 2)
	{
#ifdef DEBUG
	  if (proc->stall_the_rest != inst2->tag)
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "BRANCH STALLING ERROR P%d (%lld/0x%08X) != (%lld)!!", 
                       proc->proc_id,
		       inst->tag, inst->pc,
		       proc->stall_the_rest);
#endif
	  
	  if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	    // we might have had later memq full problems if stall_on_full
	    unstall_the_rest(proc);     

	  inst2->branchdep = 0;
	  instance *inst3;
	  inst3 = proc->BranchDepQ.GetNext(proc); // this should be inst2 itself

#ifdef DEBUG
	  if (inst3 && inst3->tag != inst2->tag)
	    // it wouldn't have been in the branchdepq if it stalled in rename
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "BRANCH STALLING ERROR P%d(%lld/%d) %s:%d!!", 
                       proc->proc_id, inst->tag, inst->pc, __FILE__, __LINE__);
#endif

	  if (inst3)     /* if it was in the branch depq */
            inst3->stallqs--;
	}
    }
}





/*
 * BadPrediction : Steps to be taken on event of a branch misprediction 
 */
void BadPrediction(instance * inst, ProcState * proc)
{
  // we assume that proc->pc and proc->npc have been set properly before
  // calling BadPrediction

  if (inst->code.uncond_branch == 4)  // if it's a RAS access
    proc->ras_bad_predicts++;
  else
    proc->bpb_bad_predicts++;

  long long tag_to_use = inst->tag;
  instance *inst2;

  if (inst->code.annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;

  if (tag_to_use == GetTailInst(proc)->tag + 1)
    {
      // The delay slot (and things after it) have not been issued yet!
      proc->copymappernext = 0; // We are saved the touble of saving mappers
      return;
    }

  // to indicate that the next instruction after this one is the point from
  // which the processor will be fetching after this misprediction
  inst2->npc = proc->pc;       

  if (inst2->branchdep == 2)
    {
#ifdef DEBUG
      if (proc->stall_the_rest != inst2->tag)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "BRANCH STALLING ERROR P%d(%lld/%d) %s:%d!!", 
		   proc->proc_id, inst->tag, inst->pc, __FILE__, __LINE__);
#endif
      
      if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	unstall_the_rest(proc);

      inst2->branchdep = 0;
      instance *inst3 = proc->BranchDepQ.GetNext(proc);   

#ifdef DEBUG
      // result should be inst2 itself, if anything
      if (inst3 && inst3->tag != inst2->tag)
	/* wouldn't have been in branchq on renaming stall */
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "BRANCH STALLING ERROR P%d(%lld/%d) %s:%d!!", 
		   proc->proc_id, inst->tag, inst->pc, __FILE__, __LINE__);
#endif
      
      if (inst3)
	inst3->stallqs--;
    }
  else
    // Copy the corresponding mapper entry to the original mappers
    // Also copy the busy state of the integers at that instant
    CopyBranchQ(tag_to_use, proc);

  // Flush the mapper tables to remove this and later branches if any
  FlushBranchQ(tag_to_use, proc);

  // We should flush the memory queue too so that the loads and stores dont
  // keep waiting to verify that the branch is done or not...
  FlushMems(tag_to_use, proc);

  // Flush all entries in the stallQ which were sent in after the branch.
  // Must be flushed before active list since some entries in stallQ are not
  // in active list.
  FlushStallQ(tag_to_use, proc);

  FlushFetchQ(proc);
  
  // Flush the active list to remove any entries after the branch 
  // While flushing, put back the registers in the free list

  int pre = proc->active_list.NumElements();
  FlushActiveList(tag_to_use, proc);

  int post = proc->active_list.NumElements();

#ifndef NOSTAT
  StatrecUpdate(proc->BadPredFlushes, pre - post, 1);
#endif
  
  return;
}


/*
 * we assume that proc->pc and proc->npc have been set properly before
 * calling HandleUnPredicted 
 */
void HandleUnPredicted(instance * inst, ProcState * proc)
{
  long long tag_to_use = inst->tag;
  instance *inst2;

  if (inst->code.annul == 0)
    {
      tag_to_use = inst->tag + 1;
      inst2 = convert_tag_to_inst(tag_to_use, proc);
    }
  else
    inst2 = inst;

  if (tag_to_use == proc->instruction_count)
    // The delay slot has not been issued yet!
    return;

  // to indicate that the next instruction after this one is the point from
  // which the processor will be fetching after this misprediction
  inst2->npc = proc->pc;
  inst2->branchdep = 0;
}



/*
 * FlushBranchQ : Flush all elements (shadow mappers) in the branchq
 */
void FlushBranchQ(long long tag, ProcState *proc)
{
  BranchQElement *junk;

  while (proc->branchq.NumItems())
    {
      proc->branchq.GetTail(junk);
      if (junk->tag >= tag)
	{
	  proc->branchq.RemoveTail();
	  DeleteMapTable(junk->specmap, proc);
	  DeleteBranchQElement(junk, proc);
	}
      else
	break;
    }
}




void FlushFetchQ(ProcState *proc)
{
  Queue<fetch_queue_entry> *fetch_queue = proc->fetch_queue;
  fetch_queue_entry fqe;

  while (!fetch_queue->Empty())
    {
      fetch_queue->GetHead(fqe);
      DeleteInstance(fqe.inst, proc);
      fetch_queue->Dequeue();
    }
}



/*
 * GetElement : Get branch queue element that matches given tag 
 */
BranchQElement *GetElement(long long tag, ProcState * proc)
{
  MemQLink<BranchQElement *> *stepper = NULL;
  BranchQElement *junk = NULL;

  while ((stepper = proc->branchq.GetNext(stepper)) != NULL)
    {
      junk = stepper->d;
      if (junk->tag == tag)
	return junk;
    }

  return NULL;
}



/*
 * RemoveFromBranchQ : Remove entry specified by tag from branch queue 
 *                     for successful predictions
 */
int RemoveFromBranchQ(long long tag, ProcState * proc)        
{
  // this part is like the way we handle memory system
  BranchQElement *junk = GetElement(tag, proc);

  if (junk == NULL)
    return -1;

  junk->done = 1;
  proc->branchq.Remove(junk);
  DeleteMapTable(junk->specmap, proc);
  DeleteBranchQElement(junk, proc);
  
  instance *i = proc->BranchDepQ.GetNext(proc);

  if (i != NULL)
    {
      i->stallqs--;
      if (AddBranchQ(i->tag, proc) != 0)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SERIOUS ERROR IN REVIVING A SLEPT BRANCH!!!"
		   " P%d(%lld/%d) %s:%d!!\n", proc->proc_id, i->tag, 
		   i->pc, __FILE__, __LINE__);

      i->branchdep = 0;
#ifdef COREFILE
      if (YS__Simtime > DEBUG_TIME)
	fprintf(corefile, "shadow mapper allocated for tag %lld\n", i->tag);
#endif
      
#ifdef DEBUG
      if (proc->stall_the_rest != i->tag)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SERIOUS ERROR IN REVIVING A SLEPT BRANCH!!!"
		   " P%d(%lld/%d) %s:%d!!\n", proc->proc_id, i->tag, 
		   i->pc, __FILE__, __LINE__);
#endif
      
      if (!STALL_ON_FULL || proc->type_of_stall_rest != eMEMQFULL)
	unstall_the_rest(proc);
    }

  return 0;
}



/*
 * AddBranchQ: Initialize shadow mappers and add to list of outstanding
 *             branches. return -1 if out of shadow mappers
 */
int AddBranchQ(long long tag, ProcState * proc)
{
  if (proc->branchq.NumItems() >= MAX_SPEC)
    {
      /* out of speculations */
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "Out of shadow mappers !\n");
#endif
      return (-1);
    }

  MapTable *specmap = NewMapTable(proc);

#ifdef DEBUG
  if (specmap == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Got a NULL map table entry!!");
#endif

  int n;
  long long *src, *dest;

  src  = (long long*)proc->intmapper;
  dest = (long long*)specmap->imap;
  for (n = 0; n < (sizeof(short)*NO_OF_LOG_INT_REGS) / sizeof(long long); n++)
    dest[n] = src[n];
  
  
  src  = (long long*)proc->fpmapper;
  dest = (long long*)specmap->fmap;
  for (n = 0; n < (sizeof(short)*NO_OF_LOG_FP_REGS) / sizeof(long long); n++)
    dest[n] = src[n];

  /* Add it into our Branch List */
  proc->branchq.Insert(NewBranchQElement(tag, specmap, proc));

  return (0);
}



/*
 * CopyBranchQ : Set up the state after a branch misprediction
 */
int CopyBranchQ(long long tag, ProcState * proc)
{
  /* Let us get the entry corresponding to this tag */
  BranchQElement *entry = GetElement(tag, proc);

  if (entry == NULL)
    {
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "<branchspec.cc> Error : Shadow mapper not "
		"found for tag %lld\n", tag);
#endif
      return (-1);
    }

  /* Change the mappers to point to the right ones */

  MapTable *tmpmap = proc->activemaptable;
  proc->activemaptable = entry->specmap;
  entry->specmap = tmpmap;

  proc->intmapper = proc->activemaptable->imap;
  proc->fpmapper = proc->activemaptable->fmap;

  return (0);
}



/*
 * FlushActiveList : Flush the active list on a branch misprediction
 */
int FlushActiveList(long long tag, ProcState * proc)
{
  proc->copymappernext = 0;
  proc->unpredbranch = 0;
  unstall_the_rest(proc);

  /* Flush out the active list suitable freeing the physical registers used
     too. */
  return (proc->active_list.flush_active_list(tag, proc));
}



/*
 * Flush stall queue elements on branch misprediction
 */
int FlushStallQ(long long tag, ProcState * proc)
{
  instance *tmpinst;

  if (proc->inst_save != NULL && proc->inst_save->tag > tag)
    {
      /* Check for tag > tagof our valid instructions */
      tmpinst = proc->inst_save;

      if (tmpinst->code.wpchange &&
	  !(tmpinst->strucdep > 0 && 
	    tmpinst->strucdep < 5) &&
          tmpinst->exception_code != WINOVERFLOW &&
          tmpinst->exception_code != WINUNDERFLOW &&
	  tmpinst->exception_code != CLEANWINDOW)
	{
	  if (tmpinst->code.wpchange != WPC_FLUSHW)
	    {
	      /* unupdate CWP that has been changed */
	      proc->cwp = (proc->cwp + NUM_WINS -
			   tmpinst->code.wpchange) % NUM_WINS;

	      proc->cansave += tmpinst->code.wpchange;
	      proc->canrestore -= tmpinst->code.wpchange;
	    }
	}
	  
      if (tmpinst->strucdep > 0 && tmpinst->strucdep < 7)
	{
	  // renaming only partially done
	  FlushTagConverter(tmpinst->tag, proc);
	  DeleteInstance(tmpinst, proc);
          proc->inst_save = NULL;
	}
      else
	tmpinst->tag = -1;
    }

  return (0);
}




