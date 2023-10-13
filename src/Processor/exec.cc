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
}                    

#include "Processor/procstate.h"
#include "Processor/branchpred.h"
#include "Processor/memunit.h"
#include "Processor/mainsim.h"
#include "Processor/exec.h"
#include "Processor/fetch_queue.h"
#include "Processor/simio.h"
#include "Processor/fastnews.h"
#include "Processor/fsr.h"
#include "Processor/tagcvt.hh"
#include "Processor/active.hh"
#include "Processor/procstate.hh"
#include "Processor/memunit.hh"
#include "Processor/stallq.hh"
#include "Processor/exec.hh"


#ifdef sgi
#define LLONG_MAX LONGLONG_MAX
#endif

#if !defined(LLONG_MAX)
#  define LLONG_MAX 9223372036854775807LL
#endif

#ifdef sparc
#pragma align 8 (TheBadPC)
#endif

instr *TheBadPC = new instr(iSETHI);

#ifdef sgi
#pragma align_symbol (TheBadPC, 8)
#endif

#define min(a, b)               ((a) > (b) ? (b) : (a))



/***************************************************************************/
/***************************************************************************/

int fetch_cycle(ProcState * proc)
{
  instr                    *instrn;
  int                       rc, attributes, count;
  unsigned                  phys_pc;
  Queue<fetch_queue_entry> *fetch_queue = proc->fetch_queue;
  fetch_queue_entry         fqe;


  if ((proc->interrupt_pending) &&
      (PSTATE_GET_IE(proc->pstate) &&
       (fetch_queue->NumItems() < proc->fetch_queue_size)))
    {
      int n = 31;
      while (((proc->interrupt_pending & (0x00000001 << n)) == 0) &&
	     (n > 0))
	n--;

      if (n > proc->pil)
	{
	  fqe.exception_code = (enum except)(INTERRUPT_00 - n);

	  fqe.inst = NewInstance(TheBadPC, proc); // TLB miss or prot. fault
	  fqe.pc   = proc->fetch_pc;         // insert NOP with exception set

	  fetch_queue->Enqueue(fqe);
	  return(0);
	}
    }

  
  if ((!proc->fetch_done) || (L1IQ_FULL[proc->proc_id]))
    return(0);

  count = min(proc->fetch_rate,
	      (ARCH_linesz1i - (proc->fetch_pc % ARCH_linesz1i)) /
	      SIZE_OF_SPARC_INSTRUCTION);


  //-------------------------------------------------------------------------
  // do one TLB access if I-TLB is enabled

  phys_pc = proc->fetch_pc;
  attributes = 0;
  if (PSTATE_GET_ITE(proc->pstate))
    {
      rc = proc->itlb->
	LookUp(&phys_pc, &attributes,
	       proc->log_int_reg_file[arch_to_log(proc,
						  proc->cwp,
						  PRIV_TLB_CONTEXT)],
	       0, PSTATE_GET_PRIV(proc->pstate), LLONG_MAX);

      if (rc != TLB_HIT)
	{
	  if (fetch_queue->NumItems() < proc->fetch_queue_size)
	    {
	      if (rc == TLB_MISS)
		fqe.exception_code = ITLB_MISS;

	      if (rc == TLB_FAULT)
		fqe.exception_code = INSTR_FAULT;

	      fqe.inst = NewInstance(TheBadPC, proc);
	      fqe.pc   = proc->fetch_pc;       // insert NOP with exception set

	      fetch_queue->Enqueue(fqe);
	    }
	  
	  return(0);
	}
    }

  ICache_recv_addr(proc->proc_id, proc->fetch_pc, phys_pc,
		   count, attributes,
		   proc->pstate);

  proc->fetch_done = 0;
  
  return 0;
}




/*************************************************************************/
/* decode_cycle :  brings new instructions into active list              */
/*************************************************************************/ 
/* decode_cycle() --  The decode routine called every cycle              */
/* This is organized into two main parts -                               */
/* 1. Check the stall queue for instructions stalled from previous       */
/*    cycles.                                                            */
/* 2. Check the instructions for this cycle.                             */
/*************************************************************************/

int decode_cycle(ProcState * proc)
{
#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "ENTERING DECODE CYCLE-CYCLE NUMBER %d. UnitsFree: "
	    "%d %d %d %d\n", proc->curr_cycle, proc->UnitsFree[uALU], 
	    proc->UnitsFree[uFP], proc->UnitsFree[uADDR], 
	    proc->UnitsFree[uMEM]);
#endif

  instance         *inst;
  Queue<fetch_queue_entry> *fetch_queue = proc->fetch_queue;
  fetch_queue_entry fqe;
  int brk        = 0;
  int badpc      = 0;
  int count      = 0;
  int decoderate = proc->decode_rate;

  
  /* DECODE THIS CYCLE'S INSTRUCTIONS. */

  if (proc->inst_save)
    {
      count++;
      if (check_dependencies(proc->inst_save, proc) != 0)
	brk = 1;
      else
	proc->inst_save = NULL;
    }


  if (proc->stall_the_rest)
    brk = 1;

  if (proc->stall_the_rest == -1ll)
    unstall_the_rest(proc);



#ifdef DEBUG
  if (brk)
    return(0);
  
  if (proc->inst_save)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "proc->inst_save == %lld\n", proc->inst_save->tag);
#endif


  /* Read in more instructions only if there are no branch stalls (or if 
     there is a branch stall and the delay slot has to be read in) */

  while ((count < decoderate) && (brk == 0) && !proc->sync)
    {
      /* get the instruction from the fetch queue */
      if (fetch_queue->Empty())
	{
	  if (proc->fetch_pc != proc->pc)
	    {
	      proc->fetch_pc = proc->pc;
              proc->fetch_done = 1;
	    }

	  return -1;
	}

#if 0
#ifdef COREFILE
      instrn->print();
#endif
#endif

      fetch_queue->GetHead(fqe);
      fetch_queue->Dequeue();

      inst = fqe.inst;
      if (fqe.pc != proc->pc)
	{
	  DeleteInstance(inst, proc);
	  continue;
	}

      count++;

      inst->decode_instruction(proc);
      AddtoTagConverter(inst->tag, inst, proc);

      if (fqe.exception_code != OK)
      	{
	  inst->exception_code = fqe.exception_code;
      	  proc->stall_the_rest = inst->tag;
      	  proc->type_of_stall_rest = eBADBR;
      	}

      
      /* If there are dependencies, put it in the stall queue.
         check_dependencies takes care of issuing it if there are no
         dependencies */
      if (check_dependencies(inst, proc) != 0)
	{
          proc->inst_save = inst;
          brk = 1;
	}

      /* If a pipeline bubble has to be inserted, then... */
      if (proc->stall_the_rest)
        brk = 1;

      if (proc->stall_the_rest == -1ll)
        unstall_the_rest(proc); // this is just a xfer, not a real stall
    }
  
  return 0;
}



/****************************************************************************
 * This functions reads in an instruction and converts to an instance. It
 * converts the static instruction to the dynamic instance.  Initializes the
 * elements of instance and also does a preliminary check for hazards.  
 ***************************************************************************/

int instance::decode_instruction(ProcState * proc)
{
  /* INITIALIZE DATA ELEMENTS OF INST */

  tag = proc->instruction_count;   /* Our instruction id henceforth! */
  proc->instruction_count++;       /* Increment Global variable */

  unit_type        = unit[code.instruction];
  win_num          = proc->cwp; /* before any change in this instruction. */

  addr             = 0;
  addr_ready       = 0;
  mem_ready        = 0;
  stallqs          = 0;
  busybits         = 0;
  in_memunit       = 0;
  partial_overlap  = 0;
  limbo            = 0;
  kill             = 0;
  vsbfwd           = 0;
  miss             = L1DHIT;
  latepf           = 0;

  rs1valf          = rs2valf = 0;
  rsccvali         = 0;
  rsdvalf          = 0;
  rccvali          = rdvali = 0;

  time_active_list = YS__Simtime;
  issuetime        = LLONG_MAX;  // start it out as high as possible
  addrissuetime    = LLONG_MAX;  // used only in static sched; start out high

  /* Set up default dependency values */
  truedep          = 1;
  addrdep          = 1;
  strucdep         = 0;
  branchdep        = 0;

  /* Make all other values zero */
  completion       = 0;
  newpc            = 0;
  exception_code   = OK;
  mispredicted     = 0;
  taken            = 0;
  memprogress      = 0;

  proc->stall_the_rest     = 0;
  proc->type_of_stall_rest = eNOEFF_LOSS;

  
  /* This is the first time the instruction is being processed, we will have
     to convert the window pointer, register number combination to a logical
     number and then allocate physical registers, etc.. */

  
  /* Source register 1 */
  switch (code.rs1_regtype)
    {
    case REG_FSR:
    case REG_INT:
    case REG_INT64:
      lrs1 = arch_to_log(proc, win_num, code.rs1);
      prs1 = proc->intmapper[lrs1];
      break;
      
    case REG_FP:
      lrs1 = code.rs1;
      prs1 = proc->fpmapper[lrs1];
      break;

    case REG_FPHALF:
      lrs1 = code.rs1;
      prs1 = proc->fpmapper[unsigned (lrs1) & ~1U];
      break;

    case REG_INTPAIR:
      if (code.rs1 & 1)
	{    /* odd source reg. */
	  exception_code = ILLEGAL;
	  lrs1 = prs1 = prs1p = 0;
	}
      else
	{
	  lrs1  = arch_to_log(proc, win_num, code.rs1);
	  prs1  = proc->intmapper[lrs1];
	  prs1p = proc->intmapper[lrs1 + 1];
	}
      break;

    default:
      break;
    }

  /* Source register 2 */
  switch (code.rs2_regtype)
    {
    case REG_INT:
    case REG_INT64:
      lrs2 = arch_to_log(proc, win_num, code.rs2);
      prs2 = proc->intmapper[lrs2];
      break;

    case REG_FP:
      lrs2 = code.rs2;
      prs2 = proc->fpmapper[lrs2];
      break;

    case REG_FPHALF:
      lrs2 = code.rs2;
      prs2 = proc->fpmapper[unsigned (lrs2) & ~1U];
      break;

    default:
      break;
    }

  /* Conditon code as a source register rscc */
  lrscc = arch_to_log(proc, win_num, code.rscc);
  prscc = proc->intmapper[lrscc];

  if (code.rd_regtype == REG_FPHALF)
    {
      /* Since FPs are mapped/renamed by doubles, we need to make the 
	 destination register as source also in this case */
      lrsd = code.rd;
      prsd = proc->fpmapper[unsigned (lrsd) & ~1U];
    }

  /*************************************************************/
  /***************** Register window operations ****************/
  /*************************************************************/
  
  // Lets us NOW bump up or bring down the cwp if its a save/restore
  // instruction. Note that the source registers do not see the effect 
  // of the change in the cwp while the destination registers do
  
  int dests = 1;
  if (code.wpchange != WPC_NONE)
    {
      if (code.wpchange == WPC_SAVE)
	{
	  if (proc->cansave)
	    {
	      if (proc->cleanwin - proc->canrestore != 0)
		win_num = unsigned (win_num + 1) % NUM_WINS;
	      else
		{
		  exception_code = CLEANWINDOW;
		  dests = 0;
		  lrd = lrcc = 0;
		  strucdep = 2;
		}
	    }
	  else
	    { // SAVE EXCEPTION. Do not let this instruction act properly.
	      exception_code = WINOVERFLOW;
	      dests = 0;
	      lrd = lrcc = 0;
	      strucdep = 2;
	    }
	}

      else if (code.wpchange == WPC_RESTORE)
	{
	  if (proc->canrestore)
	    win_num = unsigned(win_num + NUM_WINS - 1) % NUM_WINS;
	  else 
	    {
	      // RESTORE EXCEPTION.
	      exception_code = WINUNDERFLOW;
	      dests = 0;
	      lrd = lrcc = 0;
	      strucdep = 2;
	    }
	}

      else if ((code.wpchange == WPC_FLUSHW) &&
	       (proc->cansave != NUM_WINS-2))
	{
	  exception_code = WINOVERFLOW;
	  dests = 0;
	  lrd = lrcc = 0;
	  strucdep = 2;
	}
    }
	    

  if (dests)
    {  // ALWAYS, except for win exceptions
      /* Convert the destination condition code register */
      lrcc = arch_to_log(proc, win_num, code.rcc);

      /* Convert the Destination Register */
      if (code.rd_regtype == REG_FP || code.rd_regtype == REG_FPHALF)
	{
	  lrd = code.rd;
	  strucdep = 1;  /* To go to the right place in check_dependencies */
	}
      else
	{
	  lrd = arch_to_log(proc, win_num, code.rd);
	  strucdep = 2;  /* To go to the right place in check_dependencies */
	}
    }

  /***************************************************************/
  /******************* Memory barrier instructions ***************/
  /***************************************************************/

#ifndef STORE_ORDERING
  if (code.instruction == iMEMBAR && code.rs1 == STATE_MEMBAR)
    {
      // decode a membar and put it in its queue
      MembarInfo mb;
      mb.tag = tag;
      mb.SS       = (code.aux2 & MB_StoreStore) != 0;
      mb.LS       = (code.aux2 & MB_LoadStore) != 0;
      mb.SL       = (code.aux2 & MB_StoreLoad) != 0;
      mb.LL       = (code.aux2 & MB_LoadLoad) != 0;
      mb.MEMISSUE = (code.aux1 & MB_MEMISSUE) != 0;
      mb.SYNC     = (code.aux1 & MB_SYNC) != 0;
      ComputeMembarInfo(proc, mb);
      proc->membar_tags.Insert(mb);
    }
#endif

  // Check if we are a delay slot
  if (proc->copymappernext == 1)
    {
      /* There is a message for me by the previous branch to take care of
         copying the shadow mapping, Let me store this information. */
      branchdep = 2;
      proc->copymappernext = 0;     /* We don't want the next instruction
                                       to do the same thing */
    }

  pc  = proc->pc;
  npc = proc->npc;
  int tmppc = proc->pc; /* We need this to check if we are having a
			   jump in the instruction addressing */

  // Check if we are getting into a branch instruction. At the end of this
  // and the next stage, we would have got the pc and npc for the next
  // instruction, and also identified if there are any branch dependencies

  if (code.cond_branch || code.uncond_branch)
    decode_branch_instruction(&code, this, proc);
  else
    { /* For all non control transfer instructions */
      proc->pc  = proc->npc;
      proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
    }

  if ((proc->pc != tmppc + SIZE_OF_SPARC_INSTRUCTION &&
       proc->pc != tmppc + 2 * SIZE_OF_SPARC_INSTRUCTION) &&
      !proc->stall_the_rest)
    { // not taken branch with no DELAYs
      /* We are doing a jump at this stage to a different portion of the
         address space, next fetch should begin only in the next cycle, so
         stall the rest of fetches this cycle */
      proc->stall_the_rest = -1;
      proc->type_of_stall_rest = eBR;
    }

  // Note that if we are starting a new cycle, this will get reset anyway, so
  // we needn't bother about this instruction being the last one of a cycle!

  return (0);
}




/*===========================================================================
 * check_dependencies() implements the dependecy checking logic of the
 * processor.
 */
int check_dependencies(instance * inst, ProcState * proc)
{
  short next_free_phys_reg = 0;

  proc->intmapper[ZEROREG]    = 0;
  proc->intregbusy[proc->intmapper[ZEROREG]]   = 0;


  // NOTE: INT_PAIR takes the standard INT route except that it maps
  // the second dest register where other instructions would map
  // "rcc".  This is OK in the SPARC since the only instructions (LDD,
  // LDDA) with an INT_PAIR destination do not have a CC destination



  // in stat_sched, addr. gen is also lumped in with these
  if (STALL_ON_FULL && inst->strucdep)
    {
      if (stat_sched ||
	  (proc->active_instr[inst->unit_type] >=
	   proc->max_active_instr[inst->unit_type]))
	{
	  // Make sure there's space in the issue queue before
	  // even trying to get renaming registers, etc.
	  proc->stall_the_rest = inst->tag;
	  proc->type_of_stall_rest = eISSUEQFULL;
	  return 11;
	}

    }
 
  
  /*************************************/
  /* Check for structural stalls first */
  /*************************************/

  if (inst->strucdep != 0)
    {
      switch(inst->strucdep)
        {
        case 1:                 // FP destination register
          
          // No free memory for rd fp
          next_free_phys_reg = proc->free_fp_list->getfreereg();
          if (next_free_phys_reg == -1)
            {
              proc->stall_the_rest = inst->tag;
              proc->type_of_stall_rest = eRENAME;
              inst->strucdep = 1;
              return (1);
            }
          inst->prd = next_free_phys_reg;
          // Note: no break here, we have to do the next step too

	  
        case 3:
          // Copy old mapping to active list
          if (proc->active_list.NumEntries() == proc->max_active_list)
            {
              // No space in active list for fp
              inst->strucdep = 1;
              proc->free_fp_list->addfreereg(inst->prd);
              inst->prd = 0;
              proc->stall_the_rest = inst->tag;
              proc->type_of_stall_rest = eNOEFF_LOSS;
              return (3);
            }

          // NOTE: This code is used for both FP and FPHALF
          if (inst->code.rd_regtype == REG_FP)
            proc->active_list.add_to_active_list(inst,
						 inst->lrd,
						 proc->fpmapper[inst->lrd],
						 REG_FP,
						 proc);
          else                  // FPHALF
            proc->active_list.add_to_active_list(inst,
						 unsigned(inst->lrd)&~1U,
						 proc->fpmapper[unsigned(inst->lrd)&~1U],
						 REG_FP,proc);
            
          // NOW, change the mapper to point to the new mapping
          proc->cwp = inst->win_num;
 
          if (((inst->code.wpchange == WPC_SAVE) ||
	       (inst->code.wpchange == WPC_RESTORE)) &&
              (inst->exception_code != WINOVERFLOW) &&
              (inst->exception_code != WINUNDERFLOW))
            {
              proc->cansave -= inst->code.wpchange;
              proc->canrestore += inst->code.wpchange;
 
#ifdef COREFILE
              if (YS__Simtime > DEBUG_TIME)
                fprintf(corefile,
                        "Changing cwp. Now CANSAVE %d, CANRESTORE %d\n",
                        proc->cansave,
                        proc->canrestore);
#endif
            }
 
 
          if (inst->code.rd_regtype == REG_FP)
            proc->fpmapper[inst->lrd] = inst->prd;
          else                                      // REG_FPHALF
            proc->fpmapper[unsigned(inst->lrd)&~1U] = inst->prd;
          
          // Update busy register table
          proc->fpregbusy[inst->prd] = 1;
 
          inst->strucdep = 5;
          break;
 
          
        case 2:                        // Int destination register
          if (inst->lrd == ZEROREG)
            {
              inst->strucdep = 4;
              inst->prd = proc->intmapper[ZEROREG];
            }
          else
            {
              // No free memory for rd int
              next_free_phys_reg = proc->free_int_list->getfreereg();
              if (next_free_phys_reg == -1)
                {
                  proc->stall_the_rest = inst->tag;
                  proc->type_of_stall_rest = eRENAME;
                  inst->strucdep = 2;
                  return (2);
                }
              inst->prd = next_free_phys_reg;
              // Note: no break
	    }

          
        case 4:
          // Copy old mapping to active list
          if (proc->active_list.NumEntries() == proc->max_active_list)
            {
              if (inst->prd != proc->intmapper[ZEROREG])
                {
                  inst->strucdep = 2;
                  proc->free_int_list->addfreereg(inst->prd);
                  inst->prd = proc->intmapper[ZEROREG];
                }
              proc->stall_the_rest = inst->tag;
              proc->type_of_stall_rest = eNOEFF_LOSS;
              return (4);
            }

          proc->active_list.add_to_active_list(inst,
					       inst->lrd,
					       proc->intmapper[inst->lrd],
					       REG_INT, proc);

          // NOW, change the mapper to point to the new mapping
          proc->cwp = inst->win_num;
          if (((inst->code.wpchange == WPC_SAVE) ||
	       (inst->code.wpchange == WPC_RESTORE)) &&
              (inst->exception_code != WINOVERFLOW) &&
              (inst->exception_code != WINUNDERFLOW))
            {
              proc->cansave -= inst->code.wpchange;
              proc->canrestore += inst->code.wpchange;

#ifdef COREFILE
              if (YS__Simtime > DEBUG_TIME)
                fprintf(corefile,
                        "Changing cwp. Now CANSAVE %d, CANRESTORE %d\n",
                        proc->cansave,
                        proc->canrestore);
#endif
            }

          proc->intmapper[inst->lrd] = inst->prd;
 
          
          // Update busy register table
          if (inst->prd != proc->intmapper[ZEROREG])
            proc->intregbusy[inst->prd] = 1;

          inst->strucdep = 5;
          break ;

          
        default:
          break;
        }
 
 
      
      switch(inst->strucdep)
        {
        case 5:
          if (inst->code.rd_regtype == REG_INTPAIR)
            {
              // in this case, we need to map the second register
              // of the pair rather than the cc register
              inst->prcc = proc->intmapper[ZEROREG];         // cc not used
              if (inst->code.rd & 1)              // odd destination register
                {
                  inst->exception_code = ILLEGAL;
                  inst->prdp = 0;
                }
              else
                {
                  next_free_phys_reg = proc->free_int_list->getfreereg();
                  if (next_free_phys_reg == -1)
                    {
                      proc->stall_the_rest = inst->tag;
                      proc->type_of_stall_rest = eRENAME;
                      inst->strucdep = 5;
                      return (5);
                    }
                  inst->prdp = next_free_phys_reg;
                }
              // No break here as we have to do the next part anyway
            }
          else
            {
              inst->prdp = proc->intmapper[ZEROREG];       // pair not used
              if (inst->lrcc == ZEROREG)
                {
                  inst->strucdep = 6;
                  inst->prcc = proc->intmapper[ZEROREG];
                }
              else
                {                                 // rcc no free list register
                  next_free_phys_reg = proc->free_int_list->getfreereg();
                  if (next_free_phys_reg == -1)
                    {
                      proc->stall_the_rest = inst->tag;
                      proc->type_of_stall_rest = eRENAME;
                      inst->strucdep = 5;
                      return (5);
                    }
                  inst->prcc = next_free_phys_reg;
                  // No break here as we have to do the next part anyway
                }
            }
 
 
        case 6:
          // active list full
#ifdef DEBUG
          // Copy old mapping to active list
          if (proc->active_list.NumEntries() == proc->max_active_list)
            {
              // THIS CASE SHOULD NEVER HAPPEN SINCE ACTIVE LIST IS IN PAIRS
              YS__errmsg(proc->proc_id / ARCH_cpus,
			 "Tag %lld: Active list full in case 6 -- that should never happen.",
			 inst->tag);
              
              inst->strucdep = 6;
              proc->stall_the_rest = inst->tag;   // rest of the instructions should
                                                  // not be decoded
              proc->type_of_stall_rest = eNOEFF_LOSS;
              return (6);
            }
#endif
	
          if (inst->code.rd_regtype == REG_INTPAIR)
            {
              proc->active_list.add_to_active_list(inst,
						   inst->lrd + 1,
						   proc->intmapper[inst->lrd + 1],
						   REG_INT,
						   proc);
 
              // NOW, change the mapper to point to the new mapping
              proc->intmapper[inst->lrd+1] = inst->prdp;
              
              // Update busy register table
              proc->intregbusy[inst->prdp] = 1;
            }
          else
            {
              proc->active_list.add_to_active_list(inst,
						   inst->lrcc,
						   proc->intmapper[inst->lrcc],
						   REG_INT,
						   proc);
              
              // NOW, change the mapper to point to the new mapping
              proc->intmapper[inst->lrcc] = inst->prcc;

              // Update busy register table
              if (inst->prcc != proc->intmapper[ZEROREG])
                proc->intregbusy[inst->prcc] = 1;
            }

          proc->stalledeff--;          // it made it into active list, so it's
                                       // no longer an eff problem!  
          
#ifdef COREFILE
          if (proc->stalledeff < 0)
            YS__errmsg(proc->proc_id / ARCH_cpus,
		       "STALLED EFFICIENCY DROPS BELOW 0\n");
#endif
          
          inst->strucdep = (inst->unit_type == uMEM) ? 10 : 0;
          break;
  
        default :
          break;
        }
    }
 
  
  //-------------------------------------------------------------------------
  // Check for branch dependencies here
  // If branchdep = 1, then that means we do not know what to
  // fetch next(maybe other than the delay slot, things will
  // stall automatically after the branch as the next pc will
  // be -1 till this gets done, so nothing we need to do.
 
  if (STALL_ON_FULL)
    {
      proc->active_instr[inst->unit_type]++;
    }
 

  if (proc->stall_the_rest == inst->tag)
    {
      // we don't call unstall the rest, because we don't want
      // stalledeff getting set to 0 for no obvious reason, as we are
      // still fetching instructions
      proc->stall_the_rest = 0;
      proc->type_of_stall_rest = eNOEFF_LOSS;
    }
 
  
  // CHECK FOR BRANCH STRUCTURAL DEPENDENCIES HERE
  if (inst->branchdep == 2)
    {
      // This means we are in a place where state has to
      // be saved, either at a branch or at a delay slot.
      // (That work has been done by decode_instruction already!)
      // So all we have to do is to save the shadow mapper

      // Try to copy into mappers
      if (AddBranchQ(inst->tag, proc) != 0)
        { 
         // out of speculations ! stall future instructions until
          // we get some speculations freed
          inst->stallqs++;
          proc->stall_the_rest = inst->tag;
          proc->type_of_stall_rest = eSHADOW;

#ifdef COREFILE
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile,"Branch %d failed to get shadow mapper\n",inst->tag);
#endif

          inst->branchdep = 2;
          proc->BranchDepQ.AddElt(inst,proc);
          // Don't return, go ahead and issue it, we will
          // take care of the shadow mapping in the update cycle.
        }
      else
        {
          // successful!!
 
#ifdef COREFILE
          if(proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile,"Branch %d got shadow mapper\n",inst->tag);
#endif
 
          inst->branchdep = 0;
        }
    }
 
 

  if (proc->unpredbranch && ((inst->branchdep != 1) || inst->code.annul))
    {
      // the processor has some branch that could not be predicted, and
      // either we are that annulled branch itself or we are the
      // delay slot of the delayed branch. Now we need to do a stall the
      // rest. BTW, this _really_ will not like a branch in a delay slot.
      // We should probable serialize in such a case.
 
      proc->stall_the_rest = inst->tag;
      proc->type_of_stall_rest = eBADBR;
 
#ifdef COREFILE
      if(proc->curr_cycle > DEBUG_TIME)
        fprintf(corefile,"Stalling the rest for unpredicted branch at %d\n",inst->tag);
#endif
    }
 
  
  int oldaddrdep = inst->addrdep;                    // Used for disambiguate
 
 
  
  /************************************/
  /* check for data dependencies next */
  /************************************/

  // check RAW (true) dependencies for rs1 and rs2
  if (inst->truedep == 1)
    {
      inst->truedep = 0;
      inst->addrdep = 0;
      
      // check for rs1
      if (inst->code.rs1_regtype == REG_INT ||
	  inst->code.rs1_regtype == REG_INT64)
        {
          if (proc->intregbusy[inst->prs1] == 1)
            {
              inst->busybits |= BUSY_SETRS1;
              proc->dist_stallq_int[inst->prs1].AddElt(inst, proc, BUSY_CLEARRS1);
              inst->truedep = 1;
            }
        }
 
      else if (inst->code.rs1_regtype == REG_FP ||
	       inst->code.rs1_regtype == REG_FPHALF)
        {
          if (proc->fpregbusy[inst->prs1] == 1)
            {
              inst->busybits |= BUSY_SETRS1;
              proc->dist_stallq_fp[inst->prs1].AddElt(inst, proc, BUSY_CLEARRS1);
              inst->truedep = 1;
            }
        }
 
      else if (inst->code.rs1_regtype == REG_INTPAIR)
        {
          if (proc->intregbusy[inst->prs1] == 1)
            {
              inst->busybits |= BUSY_SETRS1;
              proc->dist_stallq_int[inst->prs1].AddElt(inst, proc, BUSY_CLEARRS1);
              inst->truedep = 1;
            }
 
          if (proc->intregbusy[inst->prs1p] == 1)
            {
              inst->busybits |= BUSY_SETRS1P;
              proc->dist_stallq_int[inst->prs1p].AddElt(inst, proc, BUSY_CLEARRS1P);
              inst->truedep = 1;
            }
        }
 
      
      // check for rs2
      if (inst->code.rs2_regtype == REG_INT ||
	  inst->code.rs2_regtype == REG_INT64)
        {
          if (proc->intregbusy[inst->prs2] == 1)
            {
              inst->busybits |= BUSY_SETRS2;
              proc->dist_stallq_int[inst->prs2].AddElt(inst, proc, BUSY_CLEARRS2);
              inst->truedep = 1;
              inst->addrdep = 1;
            }
        }
 
      else if (inst->code.rs2_regtype == REG_FP ||
	       inst->code.rs2_regtype == REG_FPHALF)
        {
          if (proc->fpregbusy[inst->prs2] == 1)
            {
              inst->busybits |= BUSY_SETRS2;
              proc->dist_stallq_fp[inst->prs2].AddElt(inst,proc,BUSY_CLEARRS2);
              inst->truedep = 1;
              inst->addrdep = 1;
            }
        }
 
      
      // check for rscc
      if (proc->intregbusy[inst->prscc] == 1)
        {
          inst->busybits |= BUSY_SETRSCC;
          proc->dist_stallq_int[inst->prscc].AddElt(inst,proc,BUSY_CLEARRSCC);
          inst->truedep = 1;
          inst->addrdep = 1;
        }

      // If dest. is a FPHALF, then writing dest. register is effectively an
      // RMW. In this case, we need to make sure that register is also available
      if (inst->code.rd_regtype == REG_FPHALF &&
	  proc->fpregbusy[inst->prsd] == 1)
        {
          inst->busybits |= BUSY_SETRSD;
          proc->dist_stallq_fp[inst->prsd].AddElt(inst, proc, BUSY_CLEARRSD);
          inst->truedep = 1;
        }
    }
  
 
 
  /**************************************/
  /* check for address dependences next */
  /**************************************/

  if (inst->addrdep == 0 && inst->unit_type == uMEM)
    {
      inst->rs2vali = proc->phy_int_reg_file[inst->prs2];
      inst->rsccvali = proc->phy_int_reg_file[inst->prscc];

      if (oldaddrdep && inst->strucdep == 0)
	// already in memory system, but ambiguous
        CalculateAddress(inst, proc);
    }

  if (inst->strucdep == 10)                      // not yet in memory system
    {
//      if (NumInMemorySystem(proc) < MAX_MEM_OPS)
        {
          AddToMemorySystem(inst, proc);
          inst->strucdep = 0;
        }
/* @@     else
        {
          proc->UnitQ[uMEM].AddElt(inst, proc);
          inst->stallqs++;
          inst->strucdep = 10;

          if (STALL_ON_FULL)
            {
              // in this type of processor (probably more realistic),
              // the processor fetch/etc. stalls when the memory queue
              // is filled up (by default, we'll keep fetching later
              // instructions and just make this one wait for its resource
              // (hold it in its active list space)
 
              proc->stall_the_rest = inst->tag;
              proc->type_of_stall_rest = eMEMQFULL;
            }
        } */
    }
 

  proc->sync = inst->code.sync;

  if (inst->truedep == 0)
    // Now we are ready to issue, but do we have the resources?
    SendToFU(inst, proc);

  return (0);
} /*** End of check_dependencies ***/





/*
 * Handle issue once all dependences (other than structural dependences 
 * for functional units) are taken care of.
 */
int SendToFU(instance * inst, ProcState * proc)
{
  /* We have crossed all hazards, ISSUE IT!! */
  /* But, first copy the values of the phy regs into the instance */

#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile, "pc = %d, tag = %d, %s \n", inst->pc, 
              inst->tag, inames[inst->code.instruction]);
      fprintf(corefile, "lrs1 = %d, prs1 = %d \n", inst->lrs1, inst->prs1);
      fprintf(corefile, "lrs2 = %d, prs2 = %d \n", inst->lrs2, inst->prs2);
      fprintf(corefile, "lrd = %d, prd = %d \n", inst->lrd, inst->prd);
    }
#endif


  switch (inst->code.rs1_regtype)
    {
    case REG_FSR:
      // raise serialize exception if any FP ops are ahead of us (instruction
      // will be retried immediately), otherwise gather data to store

      if (proc->active_list.fp_ahead(inst->tag, proc))
	{
	  inst->exception_code = SERIALIZE;
          proc->active_list.flag_exception(inst->tag, SERIALIZE);
          proc->active_list.mark_done_in_active_list(inst->tag, SERIALIZE,
						     proc->curr_cycle);
	}
      else
	get_fsr(proc, inst);
      break;

      
    case REG_INT:
      inst->rs1vali = proc->phy_int_reg_file[inst->prs1];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %d, ", inst->rs1vali);
#endif
      break;

      
    case REG_INT64:
      inst->rs1valll = proc->phy_int_reg_file[inst->prs1];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %lld, ", inst->rs1valll);
#endif
      break;

      
    case REG_FP:
      inst->rs1valf = proc->phy_fp_reg_file[inst->prs1];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1f = %f, ", inst->rs1valf);
#endif
      break;

      
    case REG_FPHALF:
      {
	float *address = (float *) (&proc->phy_fp_reg_file[inst->prs1]);
#ifdef ENDIAN_SWAP
	if (!(inst->code.rs1 & 1))  /* the odd half */
	    address += 1;
#else
	if (inst->code.rs1 & 1)     /* the odd half */
	    address += 1;
#endif
	 inst->rs1valfh = *address;

#ifdef COREFILE
	 if (proc->curr_cycle > DEBUG_TIME)
	   fprintf(corefile, "rs1fh = %f, ", inst->rs1valfh);
#endif
	 break;
      }

    
    case REG_INTPAIR:
      inst->rs1valipair.a = proc->phy_int_reg_file[inst->prs1];
      inst->rs1valipair.b = proc->phy_int_reg_file[inst->prs1p];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs1i = %d/%d, ", inst->rs1valipair.a, 
		inst->rs1valipair.b);
#endif
      break;


    default:
      YS__errmsg(proc->proc_id / ARCH_cpus,
		 "Unexpected regtype %i\n",
		 inst->code.rs1_regtype);
    }



  //-------------------------------------------------------------------------
  
  switch (inst->code.rs2_regtype)
    {
    case REG_INT:
      inst->rs2vali = proc->phy_int_reg_file[inst->prs2];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2i = %d, ", inst->rs2vali);
#endif
      break;

      
    case REG_INT64:
      inst->rs2valll = proc->phy_int_reg_file[inst->prs2];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2i = %lld, ", inst->rs2valll);
#endif
      break;


    case REG_FP:
      inst->rs2valf = proc->phy_fp_reg_file[inst->prs2];

#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rs2f = %f, ", inst->rs2valf);
#endif
      break;


    case REG_FPHALF:
      {
	float *address = (float *) (&proc->phy_fp_reg_file[inst->prs2]);
#ifdef ENDIAN_SWAP
	if (!(inst->code.rs2 & 1))   /* the odd half */
	    address += 1;
#else
	if (inst->code.rs2 & 1)      /* the odd half */
	    address += 1;
#endif
	
	inst->rs2valfh = *address;

#ifdef COREFILE
	if (proc->curr_cycle > DEBUG_TIME)
	  fprintf(corefile, "rs2fh = %f, ", inst->rs2valfh);
#endif
	break;
      }

      
    default:
      YS__errmsg(proc->proc_id / ARCH_cpus,
		 "Unexpected regtype %i",
		 inst->code.rs2_regtype);
    }


  inst->rsccvali = proc->phy_int_reg_file[inst->prscc];

  if (inst->code.rd_regtype == REG_FPHALF)
    {
      inst->rsdvalf = proc->phy_fp_reg_file[inst->prsd];
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "rsdf = %f, ", inst->rsdvalf);
#endif
    }

  
#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "rsccvali = %d \n", inst->rsccvali);
#endif

  
  if (inst->unit_type == uMEM)
    return 0;

  if (proc->UnitsFree[inst->unit_type] == 0)
    {
      proc->UnitQ[inst->unit_type].AddElt(inst, proc);
      inst->stallqs++;
      return 7;
    }
  else
    {
      issue(inst, proc);
      return 0;
    }
}



/*
 * Handle the issue once _all_ dependences are taken care of.
 */
void issue(instance * inst, ProcState * proc)
{
  tagged_inst insttagged(inst);

  inst->strucdep = 0;
 
 
  //-------------------------------------------------------------------------
  // issue regular instructions
  
  if (inst->unit_type != uMEM)
    {                                         // uMEM doesn't use this ReadyQ
      proc->ReadyQueues[inst->unit_type].Insert(insttagged); 
 
      (proc->UnitsFree[inst->unit_type])--;   // uMEM doesn't use this here

      if (STALL_ON_FULL)
        {
	  proc->active_instr[inst->unit_type]--;
          inst->issuetime = proc->curr_cycle; 
        }
    }
 
 
  //-------------------------------------------------------------------------
  /// issue memory instruction

  else
    {
      // do it based on uADDR instead
      proc->ReadyQueues[uADDR].Insert(insttagged);
 
      (proc->UnitsFree[uADDR])--;

      if (stat_sched)                   // uADDR is also statically scheduled
        {
          inst->addrissuetime = proc->curr_cycle;
        }
    }
}






/*
 * Simulate the time spent in execution;move instructions from the 
 * ReadyQueues to the Running queues, and similarly, move functional 
 * units off  FreeingUnits.
 */
void IssueQueues(ProcState * proc)
{
  tagged_inst instt;
  instance *inst;
  UTYPE unit_type;

#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Issuing cycle %d\n", proc->curr_cycle);
#endif


  for (unit_type = UTYPE(0);
       unit_type < uMEM;
       unit_type = UTYPE(int(unit_type)+1))
    {
      circq<tagged_inst> *q = &(proc->ReadyQueues[unit_type]);
      while (q->NumInQueue())    // we have already checked proc->UnitsFree[unit_type]
	{
	  q->Delete(instt);
              
	  if (!instt.ok())
	    {
 
#ifdef COREFILE
	      if (proc->curr_cycle > DEBUG_TIME)
		fprintf(corefile,
			"Nonmatching entry in ReadyQueue -- was %d, now %d\n",
			instt.inst_tag,
			instt.inst->tag);
#endif
                  
	      LetOneUnitStalledGuyGo(proc, unit_type);
	      continue;
	    }
 
	  inst = instt.inst;


	  if (repeat[unit_type])
	    proc->FreeingUnits.insert(proc->curr_cycle + repeat[unit_type],unit_type);
	  else
	    (*(repfuncs[unit_type]))(inst,proc);
              
	  if (latencies[unit_type])              // deterministic latency
	    proc->Running.insert(proc->curr_cycle + latencies[unit_type],
				 inst,
				 inst->tag);
	  else
	    (*(latfuncs[unit_type]))(inst,proc);
	}
    }
}






/*
 * Move things from Running queues to Done heap, also issue instructions 
 * for units that are freed up. Take off the heap of running instructions
 * that completed in time for the next cycle. Also free up the 
 * appropriate units 
 */
void CompleteQueues(ProcState * proc)
{
  long long cycle = proc->curr_cycle;
  UTYPE     func_unit;
  instance *inst;
  long long inst_tag;

  while (proc->Running.num() != 0 && proc->Running.PeekMin() <= cycle)
    {
      proc->Running.GetMin(inst, inst_tag);
 
      // first check to see if it's been flushed
 
      if (inst_tag != inst->tag)
	{
	  // it's been flushed
 
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,
		    "Got nonmatching tag %d off Running\n",
		    inst_tag);
#endif
	  continue;
	}
 
 
      if (inst->unit_type != uMEM)
        {
          (*(instr_func[inst->code.instruction]))(inst,proc);

#if 0 
#ifdef COREFILE
          if (inst->unit_type != uMEM || !IsStore(inst) || IsRMW(inst) )
            {
              if (proc->curr_cycle > DEBUG_TIME)
                InstCompletionMessage(inst,proc);
            }
#endif
#endif
          proc->DoneHeap.insert(cycle, inst, inst->tag);
        }
 
      else
        {
          // if it's a uMEM, it must just be an address generation thing
#ifdef COREFILE
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile,"Address generated for tag %d\n",inst->tag);
#endif
          Disambiguate(inst, proc);
        }
    }
 
  while (proc->FreeingUnits.num() != 0 &&
	 proc->FreeingUnits.PeekMin() <= cycle)
    {
      proc->FreeingUnits.GetMin(func_unit);
      LetOneUnitStalledGuyGo(proc, func_unit);
    }
}





/*
 * Updates the result registers/branch prediction tables that are changed 
 * as a result of the instruction.
 */
int update_cycle(ProcState * proc)
{
  instance *inst;
  long long inst_tag;
 
  // Let us first check for the case when the branch was the last
  // instruction of the previous cycle, it may have completed
  // by now, so we need to look at this NOW!
    
  int over = 0;

  // Get everything corresponding to this cycle
  while (over != 1)
    {
      if ((proc->DoneHeap.num() == 0) ||
          (proc->DoneHeap.PeekMin() > proc->curr_cycle))
	return 0;


#ifdef DEBUG
      if (proc->DoneHeap.GetMin(inst,inst_tag) != proc->curr_cycle)
        {
          // don't flag it otherwise, as that would just be a flush on
	  // exception or some such....
          if (inst->tag == inst_tag)
            {

#ifdef COREFILE
              if (proc->curr_cycle > DEBUG_TIME)
                fprintf(corefile,
                        "<decode.cc>Something is wrong! We have ignored some executions \
for several cycles -- tag %d at %d!\n",
                        inst->tag,
                        proc->curr_cycle); 
#endif

              YS__errmsg(proc->proc_id / ARCH_cpus,
			 " Ignored executions in %s\n", __FILE__);
              continue;
            }
        }
#else
      proc->DoneHeap.GetMin(inst, inst_tag);
#endif

      // if inst->tag != inst_tag, then it is an instruction that has
      // already been flushed out and we can ignore it

      if (inst->tag == inst_tag)
        {
#ifdef COREFILE
          if (proc->curr_cycle > DEBUG_TIME)
            {
              fprintf(corefile, "INST %p - \n", inst);
              fprintf(corefile,"Marking tag %d as done\n", inst->tag);
            }
#endif

          if (proc->stall_the_rest == inst->tag &&
              !inst->branchdep &&
              !proc->unpredbranch)
            unstall_the_rest(proc);              // the instruction completed
          
 
          // Check if it is a branch instruction
          if (inst->code.cond_branch != 0 || inst->code.uncond_branch != 0)
            {
              if (inst->branchdep == 1)        // used for unpredicted branches
                {
                  // This is most probably an unconditional branch
                  // This is an instruction because of which the rest of
                  // the other instructions are stalled and waiting,
                  // lets update the pc
 
                  if (inst->code.annul || inst->tag+1 != proc->instruction_count)
                    // no delay slot remaining
                    {
                      proc->pc = inst->newpc;   // This would have been filled
		                                // at execution time
                      proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
                    }
                  else                    // delay slot still waiting to issue
                    {
                      proc->pc = inst->pc+SIZE_OF_SPARC_INSTRUCTION;
		      // should have already been set this way
                      proc->npc = inst->newpc;
                    }

		  
                  // Note: if there was a delay slot, we also need to fill in
                  // that one's NPC value...
 
                  HandleUnPredicted(inst, proc);
                  proc->unpredbranch = 0;         // go back to normal fetching, etc
 
                  if (proc->stall_the_rest == inst->tag ||
                      proc->type_of_stall_rest == eBADBR)
                    {
                      // either on this tag itself or on the delay slot
                      // but for this tag
                      unstall_the_rest(proc);
                    }
                }
 
 
              // Check if we did any fancy predictions
              if (inst->code.uncond_branch != 2 &&
                  inst->code.uncond_branch != 3 &&
                  inst->branchdep != 1)
                {
                  // YES
                  if (inst->mispredicted == 0)
                    {
#ifdef COREFILE
                      if (proc->curr_cycle > DEBUG_TIME)
                        fprintf(corefile,
                                "Predicted branch correctly - tag %d\n",
                                inst->tag);
#endif
                      // No error in our prediction
                      // Delete the corresponding mapper table
 
                      GoodPrediction(inst, proc);
                      
                      // Go on and graduate it as usual
                    }
 
                  else
                    {
                      // Error in prediction
 
#ifdef COREFILE
                      if (proc->curr_cycle > DEBUG_TIME)
                        fprintf(corefile,
                                "Mispredicted %s - tag %d\n",
                                (inst->code.uncond_branch == 4)? "return" : "branch",
                                inst->tag);
#endif
                      
                      int origpred = inst->taken;
              
                      // Now set pc and npc appropriately
 
                      if (proc->copymappernext && inst->tag+1==proc->instruction_count)
                        {
                          // in this case, we have decoded and
                          // completed the branch, but still haven't
                          // gotten the delay slot instruction. So, we
                          // need to set pc for the delay slot and set
                          // npc for the branch target
                          proc->pc = inst->pc+SIZE_OF_SPARC_INSTRUCTION;
			  // this should have already been that
			  // way, but anyway...
                          proc->npc = inst->newpc;
                        }
                      
                      else if (inst->code.annul &&
			       inst->code.cond_branch && !origpred)
                        {
                          // In this case, we had originally predicted
                          // an annulled branch to be not-taken, but
                          // it ended up being taken.  In this case,
                          // the upcoming pc needs to go to the delay
                          // slot, while the npc needs to point to the
                          // branch target
                          proc->pc = inst->pc + SIZE_OF_SPARC_INSTRUCTION;
                          proc->npc = inst->newpc;
                        }
                      
                      else
                        {
                          // in any other circumstance, we go to the
                          // branch target and continue execution from
                          // there
                          proc->pc = inst->newpc;
                          proc->npc = proc->pc+SIZE_OF_SPARC_INSTRUCTION;
                        }
                      
                      BadPrediction(inst, proc);
                      // continue;  // return (0);
                    }
 
                }                               // End of if for predictions
              
            }
          
	  //-----------------------------------------------------------------
	  // do not clear dependencies if instruction triggers a hard
	  // exception since the exception handler might change global state

	  if ((inst->exception_code != OK) &&
	      (inst->exception_code != SOFT_LIMBO) &&
	      (inst->exception_code != SOFT_SL_COHE) &&
	      (inst->exception_code != SOFT_SL_REPL))
	    {
	      proc->active_list.mark_done_in_active_list(inst->tag,
							 inst->exception_code,
							 proc->curr_cycle);
	      continue;
	    }

	  
          //-------------------------------------------------------------------
          // Look at destination registers
          // make them not busy
          // Update physical register file
          
          if (inst->code.rd_regtype == REG_INT ||
	      inst->code.rd_regtype == REG_INT64)
            {
              if (inst->prd != 0)
                proc->phy_int_reg_file[inst->prd] = inst->rdvali;
              proc->intregbusy[inst->prd] = 0;
              proc->dist_stallq_int[inst->prd].ClearAll(proc);
            }
 
          else if (inst->code.rd_regtype == REG_FP)
            {
              proc->phy_fp_reg_file[inst->prd] = inst->rdvalf;
              proc->fpregbusy[inst->prd] = 0;
              proc->dist_stallq_fp[inst->prd].ClearAll(proc);

	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							STATE_FPRS)]] |=
		inst->code.rd >= 32 ? FPRS_DU : FPRS_DL;
            }
          
          else if (inst->code.rd_regtype == REG_FPHALF)
            {
              proc->phy_fp_reg_file[inst->prd] = inst->rsdvalf;
              float *address = (float *) (&proc->phy_fp_reg_file[inst->prd]);
#ifdef ENDIAN_SWAP
              if (!(inst->code.rd & 1))                       // the odd half
                address += 1;
#else
              if (inst->code.rd & 1)                          // the odd half
                address += 1;
#endif      
              *address = inst->rdvalfh;
              proc->fpregbusy[inst->prd] = 0;
              proc->dist_stallq_fp[inst->prd].ClearAll(proc);
	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							STATE_FPRS)]] |=
		FPRS_DL;
	    }
 
          else if (inst->code.rd_regtype == REG_INTPAIR)
            {
              if (inst->prd != 0)
                proc->phy_int_reg_file[inst->prd] = inst->rdvalipair.a;
              proc->phy_int_reg_file[inst->prdp] = inst->rdvalipair.b;
              proc->intregbusy[inst->prd] = 0;
              proc->intregbusy[inst->prdp] = 0;
              proc->dist_stallq_int[inst->prd].ClearAll(proc);
              proc->dist_stallq_int[inst->prdp].ClearAll(proc);
            }
 

          // Do the same for rcc too.
          if (inst->prcc != 0)
            proc->phy_int_reg_file[inst->prcc] = inst->rccvali;
          proc->intregbusy[inst->prcc] = 0;
          proc->dist_stallq_int[inst->prcc].ClearAll(proc);
          
          // Update active list to show done and exception
          proc->active_list.mark_done_in_active_list(inst->tag,
						     inst->exception_code,
						     proc->curr_cycle);
        }
      
      else // the instruction has already been killed in some way
        {
 
#ifdef COREFILE
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile,"Got nonmatching tag %d off Doneheap\n",inst_tag);
#endif
 
        }
    }
  
  return 0;
}
