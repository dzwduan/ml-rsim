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

#include <signal.h>
#include <unistd.h>
#include <sys/time.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/ubuf.h"
}                    

#include "Processor/procstate.h"
#include "Processor/memunit.h"
#include "Processor/branchpred.h"
#include "Processor/traps.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"
#include "Processor/fsr.h"
#include "Processor/multiprocessor.h"
#include "Processor/procstate.hh"
#include "Processor/fastnews.h"
#include "Processor/active.hh"
#include "Processor/tagcvt.hh"
#include "Processor/stallq.hh"
#include "Processor/memunit.hh"


static void SaveCPUState(instance *, ProcState *);
static int  ProcessSerializedInstruction(instance *, ProcState *);
static void FatalException(instance *, ProcState *);

#define FAULT_COUNT_MAX 1024
static int FaultCount = 0;


void PrintDataFault(instance *inst, ProcState *proc)
{
  YS__logmsg(proc->proc_id / ARCH_cpus,
	  "DATA FAULT [%i] %.0f PC: 0x%X  NPC: 0x%X  Addr: 0x%08X   PState: 0x%08X\n",
	  proc->proc_id,
	  YS__Simtime,
	  inst->pc,
	  inst->npc,
	  inst->addr,
	  proc->pstate);
  
  if (FaultCount++ > FAULT_COUNT_MAX)
    {
      FaultCount = 0;
      YS__logmsg(proc->proc_id / ARCH_cpus,
	      "Too many faults encountered - suspending process\n");
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Resume simulation by sending SIGCONT (%i) to process %i\n",
		 SIGCONT, getpid());
      kill(getpid(), SIGSTOP);
    }
}



void PrintInstrFault(instance *inst, ProcState *proc)
{
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "INSTR FAULT [%i] %.0f PC: 0x%X  NPC: 0x%X  Addr: 0x%08X   PState: 0x%08X\n",
	     proc->proc_id,
	     YS__Simtime,
	     inst->pc,
	     inst->npc,
	     inst->pc,
	     proc->pstate);

  if (FaultCount++ > FAULT_COUNT_MAX)
    {
      FaultCount = 0;
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Too many faults encountered - suspending process\n");
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Resume simulation by sending SIGCONT (%i) to process %i\n",
		 SIGCONT, getpid());
      kill(getpid(), SIGSTOP);
    }
}


 
 
const char *enames[] =
{
  "OK               ",
  "Serialize Instr. ",
  "ITLB Miss        ",
  "unused - inline  ",
  "unused - inline  ",
  "unused - inline  ",
  "DTLB Miss        ",
  "unused - inline  ",
  "unused - inline  ",
  "unused - inline  ",
  "unused           ",
  "Privileged Instr.",
  "Instruction Fault",
  "Data Fault       ",
  "Bus Error        ",
  "Illegal Instr.   ",
  "Clean Window     ",
  "unused - inline  ",
  "unused - inline  ",
  "unused - inline  ",
  "Window Overflow  ",
  "unused - inline  ",
  "unused - inline  ",
  "unused - inline  ",
  "Window Underflow ",
  "unused - inline  ",
  "unused - inline  ",
  "unused - inline  ",
  "Soft: Limbo      ",
  "Soft: Coherency  ",
  "Soft: Replacement",
  "FP Disabled      ",
  "FP Error         ",
  "Div by Zero      ",
  "System Trap 00   ",
  "System Trap 01   ",
  "Simulator Trap   ",
  "System Trap 03   ",
  "System Trap 04   ",
  "System Trap 05   ",
  "System Trap 06   ",
  "System Trap 07   ",
  "System Trap 08   ",
  "System Trap 09   ",
  "System Trap 0A   ",
  "System Trap 0B   ",
  "System Trap 0C   ",
  "System Trap 0D   ",
  "System Trap 0E   ",
  "System Trap 0F   ",
  "System Trap 10   ",
  "System Trap 11   ",
  "System Trap 12   ",
  "System Trap 13   ",
  "System Trap 14   ",
  "System Trap 15   ",
  "System Trap 16   ",
  "System Trap 17   ",
  "System Trap 18   ",
  "System Trap 19   ",
  "System Trap 1A   ",
  "System Trap 1B   ",
  "System Trap 1C   ",
  "System Trap 1D   ",
  "System Trap 1E   ",
  "System Trap 1F   ",
  "System Trap 20   ",
  "System Trap 21   ",
  "System Trap 22   ",
  "System Trap 23   ",
  "System Trap 24   ",
  "System Trap 25   ",
  "System Trap 26   ",
  "System Trap 27   ",
  "System Trap 28   ",
  "System Trap 29   ",
  "System Trap 2A   ",
  "System Trap 2B   ",
  "System Trap 2C   ",
  "System Trap 2D   ",
  "System Trap 2E   ",
  "System Trap 2F   ",
  "Interrupt 0F     ",
  "Interrupt 0E     ",
  "Interrupt 0D     ",
  "Interrupt 0C     ",
  "Interrupt 0B     ",
  "Interrupt 0A     ",
  "Interrupt 09     ",
  "Interrupt 08     ",
  "Interrupt 07     ",
  "Interrupt 06     ",
  "Interrupt 05     ",
  "Interrupt 04     ",
  "Interrupt 03     ",
  "Interrupt 02     ",
  "Interrupt 01     ",
  "Interrupt 00     "
};


/*************************************************************************/
/* IsSoftException : returns true if exception code flags soft exception */
/*************************************************************************/
 
inline int IsSoftException(except code)
{
  return (code == SOFT_LIMBO) || (code==SOFT_SL_COHE) || (code==SOFT_SL_REPL);
}



/*************************************************************************/
/* PreExceptionHandler : Ensures precise exceptions on "hard" exceptions */
/*************************************************************************/

int PreExceptionHandler(instance * inst, ProcState * proc)
{
   proc->in_exception = inst;

  // "hard" exceptions -- that is, exceptions which trap to kernel
  // code (and which can thus possibly alter the TLB) have to at least
  // wait for all prior stores to at least _issue_. In addition, they have
  // to wait for any possible S-L constraint, since loads can go speculatively
  // and can cause exceptions detected later. Fortunately, the latter only
  // affects SC. "soft" exceptions don't have to wait for anything.

  if (IsSoftException(inst->exception_code) || proc->ReadyUnissuedStores == 0)
    {
      proc->in_exception = NULL;
      return ExceptionHandler(inst->tag, proc); // now we're for real!
    }
  else
    {
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
        fprintf(corefile,
                "Processor %d in PreExceptionHandler; waiting for %d stores to i
ssue\n",
                proc->proc_id,
                proc->ReadyUnissuedStores);
#endif
      return -1;
    }
}




/*************************************************************************/
/* ExceptionHandler : Ensures preciseness of exceptions                  */
/*************************************************************************/
 
int ExceptionHandler(long long tag, ProcState * proc)
{
  unsigned int n, i, addr;
 
  /* We have got an exception at the tag value */
  instance *inst = TagCvtHead(tag, proc);
  instance icopy = *inst;
 
#ifndef NOSTAT
  if (!IsSoftException(inst->exception_code))
    StatrecUpdate(proc->in_except,
		  proc->curr_cycle-proc->time_pre_exception,
		  1);
#endif
  
  // At this point, all instructions before this instruction
  // have completed and all instructions after this have not
  // written back -- precise interrupts
 
  // There are certain things we do irrespective of the exception,
  // they are ...
 
  FlushBranchQ(tag, proc);
  tag = tag-1;        // because we should also kill the excepting instruction
 
  FlushMems(tag, proc);
    
  FlushStallQ(tag, proc);

  int fetch_flush;
  fetch_flush = proc->fetch_queue->NumItems();
  FlushFetchQ(proc);
 
  /* This will delete all entries in the DoneHeap */
 
  UnitSetup(proc, 1);
 
  /* WE HAVE TO FLUSH STALL BEFORE ACTIVE */

  int pre = proc->active_list.NumElements();
  FlushActiveList(tag, proc);
  int post = proc->active_list.NumElements();

  int flushed = fetch_flush > (pre - post) ? fetch_flush : pre - post;
  
  if (EXCEPT_FLUSHES_PER_CYCLE != 0)
    proc->DELAY = (flushed+EXCEPT_FLUSHES_PER_CYCLE-1) / EXCEPT_FLUSHES_PER_CYCLE;

#ifndef NOSTAT
  StatrecUpdate(proc->ExceptFlushed, pre-post, 1);
#endif


#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile,
            "Tag %lld caused exception %d at time %d\n",
            icopy.tag,
            icopy.exception_code,
            proc->curr_cycle);
#endif
 
 
  
#ifdef COREFILE
  /* check to make sure that all busy bits are off */
  int bctr;
  for (bctr = 0; bctr < NO_OF_PHY_INT_REGS; bctr++)
    {
      if (proc->intregbusy[bctr])
        {
          YS__logmsg(proc->proc_id / ARCH_cpus,
		     "Busy bit #%d set at exception!\n", bctr);
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile, "Busy bit #%d set at exception!\n", bctr);
        }
    }
 
  for (bctr = 0; bctr < NO_OF_PHY_FP_REGS; bctr++)
    {
      if (proc->fpregbusy[bctr])
        {
          YS__logmsg(proc->proc_id / ARCH_cpus, "FPBusy bit #%d set at exception!\n", bctr);
          if (proc->curr_cycle > DEBUG_TIME)
            fprintf(corefile, "FPBusy bit #%d set at exception!\n", bctr);
        }
    }
#endif

#ifdef TRACE
  if (YS__Simtime > TRACE)
    {
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "    [%i] %s ",
		 proc->proc_id,
		 enames[icopy.exception_code]);

      if (inst->unit_type == uMEM)
	if (IsStore(inst))
	  YS__logmsg(proc->proc_id / ARCH_cpus,
		     "\t%s\t[%08x]=%08x (R%02i)\n",
		     inames[inst->code.instruction],
		     inst->addr,
		     inst->rs1vali,
		     inst->code.rs1);
	else
	  YS__logmsg(proc->proc_id / ARCH_cpus,
		     "\t%s\t[%08x]=%08x (R%02i)\n",
		     inames[inst->code.instruction],
		     inst->addr,
		     inst->rdvali,
		     inst->code.rd);
      else
	YS__logmsg(proc->proc_id / ARCH_cpus,
		   "\t%s\tR%02i = %08x\n",
		   inames[inst->code.instruction],
		   inst->code.rd,
		   proc->log_int_reg_file[inst->lrd]);
  }
#endif

  proc->exceptions[icopy.exception_code]++;
 
 
  //---------------------------------------------------------------------------
  // Let us look at the type of exception first
  switch(icopy.exception_code)
    {
 
      // ----------------------------------------------------------------------
    case OK:
 
      /* No exception, we should not have come here */
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "ERROR -- P%d(%lld) @ %lld, exception flagged when none!\n",
		 proc->proc_id, icopy.tag, proc->curr_cycle);
 
#ifdef COREFILE
      fprintf(corefile, "ERROR, exception flagged when none!\n");
#endif
      exit(-1);
      return -1;
 
 
      // ----------------------------------------------------------------------
    case BUSERR:
      if (icopy.code.instruction == iCASA || icopy.code.instruction == iCASXA)
	addr = icopy.rs2vali;
      else if (icopy.code.aux1)
	addr = icopy.rs2vali + icopy.code.imm;
      else
	addr = icopy.rs2vali + icopy.rsccvali;
      
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_BADADDR)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_BADADDR)]] = addr;
      break;

      
      // ----------------------------------------------------------------------
    case SERIALIZE:

      proc->reset_lists();

      proc->graduates++;
      proc->graduation_count++;

      if (ProcessSerializedInstruction(&icopy, proc))
        // if it returns non-zero, then it has set the PC as it desires
        {
        }
      else
        {
          proc->pc = icopy.npc; // don't restart the instruction separately
          proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
        }

      // statistics: add cycle count and instruction count
      proc->cycles[SERIALIZE] +=
        (long long)YS__Simtime + proc->DELAY - proc->start_cycle[SERIALIZE];
      proc->graduated[SERIALIZE] +=
        proc->graduates - proc->start_graduated[SERIALIZE];
      proc->start_graduated[SERIALIZE] = 0;

      if (!PSTATE_GET_PRIV(proc->pstate))
	{
	  proc->start_graduated[OK] = proc->graduates;
	  proc->start_cycle[OK] = proc->curr_cycle;
	}
      return(0);
      break;
 

 
      // ----------------------------------------------------------------------
    case SOFT_SL_REPL:
    case SOFT_SL_COHE:
    case SOFT_LIMBO:
      
      // this is a soft exception -- an inplace flush
      // (used for speculative load exceptions, for example
 
      proc->graduates++;
      proc->graduation_count++;
 
      // statistics: add cycle count and instruction count
      proc->cycles[SOFT_LIMBO] +=
        (long long)YS__Simtime + proc->DELAY - proc->start_cycle[SOFT_LIMBO];
      proc->graduated[SOFT_LIMBO] +=
        proc->graduates - proc->start_graduated[SOFT_LIMBO];
      proc->start_graduated[SOFT_LIMBO] = 0;
 
      proc->reset_lists();
      proc->pc = icopy.pc;       // we need to restart the instruction
      proc->npc = icopy.npc; 

      if (!PSTATE_GET_PRIV(proc->pstate))
	{
	  proc->start_graduated[OK] = proc->graduates;
	  proc->start_cycle[OK] = proc->curr_cycle;
	}
      return(0);
      break;
 
 
      
      // ----------------------------------------------------------------------
      // Simulator Trap
    case SYSTRAP_02:

      proc->reset_lists();

      proc->pc = icopy.npc;
      proc->npc = proc->pc+SIZE_OF_SPARC_INSTRUCTION;
 
      if (!SimTrapHandle(&icopy, proc))
        {
          FatalException(&icopy, proc);
          return -1;
        }

      proc->graduates++;
      proc->graduation_count++;
 
      // statistics: add cycle count and instruction count
      proc->cycles[SYSTRAP_02] +=
        (long long)YS__Simtime + proc->DELAY - proc->start_cycle[SYSTRAP_02];
      proc->graduated[SYSTRAP_02] +=
        proc->graduates - proc->start_graduated[SYSTRAP_02];
      proc->start_graduated[SYSTRAP_02] = 0;
 
      if (!PSTATE_GET_PRIV(proc->pstate))
	{
	  proc->start_graduated[OK] = proc->graduates;
	  proc->start_cycle[OK] = proc->curr_cycle;
	}

      return 0;
      break;

      
      // ----------------------------------------------------------------------
      // copy offending PC into BADADDR register, copy page number of
      // offending PC into index register (not used for fully assoc. TLB)
      // and into lower bits of context register

    case ITLB_MISS:
      addr = icopy.pc;
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_BADADDR)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_BADADDR)]] =
        addr;
      
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
					 PRIV_TLB_INDEX)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_TLB_INDEX)]] =
        proc->itlb->MissEntry(addr);
 
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
					 PRIV_TLB_TAG)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_TLB_TAG)]] =
        (addr & ~(PAGE_SIZE-1)) | 0x1;
 
      break;

      
      // ----------------------------------------------------------------------
      // copy offending address into BADADDR register, copy page number of
      // offending address into index register (not used for fully assoc. TLB)
      // and into lower bits of context register

    case DTLB_MISS:
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_BADADDR)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_BADADDR)]] =
        (unsigned int)icopy.addr;
 
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_INDEX)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_INDEX)]] =
        proc->dtlb->MissEntry(icopy.addr);
 
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_TAG)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_TAG)]] =
        (icopy.addr & ~(PAGE_SIZE-1)) | 0x1;
 
      break;
      

      // ----------------------------------------------------------------------
    case INSTR_FAULT:
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_BADADDR)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_BADADDR)]] =
        icopy.pc;
      PrintInstrFault(&icopy, proc);
      break;

      
      // ----------------------------------------------------------------------
    case DATA_FAULT:
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TLB_BADADDR)] =
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TLB_BADADDR)]] =
        (unsigned int)icopy.addr;
      PrintDataFault(&icopy, proc);
      break;


      //-----------------------------------------------------------------------
    case FPERR:
      take_fp_exception(proc, inst);
      break;
      
      // external interrupt: clear interrupt bit ------------------------------
    case INTERRUPT_00:
    case INTERRUPT_01:
    case INTERRUPT_02:
    case INTERRUPT_03:
    case INTERRUPT_04:
    case INTERRUPT_05:
    case INTERRUPT_06:
    case INTERRUPT_07:
    case INTERRUPT_08:
    case INTERRUPT_09:
    case INTERRUPT_0A:
    case INTERRUPT_0B:
    case INTERRUPT_0C:
    case INTERRUPT_0D:
    case INTERRUPT_0E:
    case INTERRUPT_0F:
      proc->interrupt_pending &=
	~(0x00000001 << (INTERRUPT_00 - icopy.exception_code));
      break;
    }

  
  proc->reset_lists();
 
  SaveCPUState(&icopy, proc);

  if (icopy.exception_code == CLEANWINDOW)
    {
#ifdef TRACE
      if (YS__Simtime > TRACE)
	YS__logmsg(proc->proc_id / ARCH_cpus,
		   "    [%i] Clean Window %i - saving %i\n",
		   proc->proc_id, proc->cwp, (proc->cwp + 1) % NUM_WINS);
#endif
      proc->cwp = (proc->cwp + 1) % NUM_WINS;
    }

  if ((icopy.code.instruction == iFLUSHW) &&
      (icopy.exception_code == WINOVERFLOW))
    {
#ifdef TRACE
      if (YS__Simtime > TRACE)
	YS__logmsg(proc->proc_id / ARCH_cpus,
		   "    [%i] Flush Window - saving %i\n",
		   proc->proc_id, (proc->cwp + proc->cansave + 2) % NUM_WINS);
#endif
      proc->cwp = (proc->cwp + proc->cansave + 2) % NUM_WINS;
    }

      
  if ((icopy.code.instruction == iSAVE) &&
      (icopy.exception_code == WINOVERFLOW))
    {
#ifdef TRACE
      if (YS__Simtime > TRACE)
	YS__logmsg(proc->proc_id / ARCH_cpus,
		   "    [%i] Overflow Window %i - saving %i\n",
		   proc->proc_id, proc->cwp, (proc->cwp + 2) % NUM_WINS);
#endif
      proc->cwp = (proc->cwp + 2) % NUM_WINS;
    }

  
  if (icopy.exception_code == WINUNDERFLOW)
    {
#ifdef TRACE
      if (YS__Simtime > TRACE)
	YS__logmsg(proc->proc_id / ARCH_cpus,
		   "    [%i] Underflow Window %i - restoring %i\n",
		   proc->proc_id, proc->cwp, (proc->cwp + NUM_WINS -1) % NUM_WINS);
#endif
      proc->cwp = (proc->cwp + NUM_WINS - 1) % NUM_WINS;
    }      


  //---------------------------------------------------------------------------
  // jump to trap table

  if ((icopy.exception_code <= OK) || (icopy.exception_code >= MAX_EXCEPT))
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Invalid exception code %i at PC 0x%08X\n",
	       icopy.exception_code, icopy.pc);
  
  proc->pc = proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
								proc->cwp,
								PRIV_TBA)]] |
    icopy.exception_code << 5;
  proc->npc = proc->pc+SIZE_OF_SPARC_INSTRUCTION;
     
  return 0;
}




//-----------------------------------------------------------------------------


void SaveCPUState(instance *inst, ProcState *proc)
{
#ifdef TRACE
  if (YS__Simtime > TRACE)
    {
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "    [%i] Save PC: 0x%X  NPC: 0x%X  PState: 0x%X  ICC: 0x%X\n",
		 proc->proc_id,
		 inst->pc,
		 inst->npc,
		 proc->pstate,
		 proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]]);

      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "    [%i] CWP: %i CanSave: %i CanRestore: %i Otherwin: %i Cleanwin: %i\n",
		 proc->proc_id,
		 proc->cwp,
		 proc->cansave,
		 proc->canrestore,
		 proc->otherwin,
		 proc->cleanwin);
    }
#endif
 
 
  // increment trap level
  proc->tl++;

  if (proc->tl >= NUM_TRAPS)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Maximum Trap Level %i exceeded at 0x%08X\n",
	       NUM_TRAPS, inst->pc);

  
  // save pc into tpc
  proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_TPC)] =
    proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,proc->cwp, PRIV_TPC)]] =
    inst->pc;
 
  // save npc into tnpc
  proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_TNPC)] =
    proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,proc->cwp, PRIV_TNPC)]] =
    inst->npc;

  // save pstate into tstate
  proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
					    PRIV_TSTATE)] =
    proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
					      PRIV_TSTATE)]] =
    proc->pstate | proc->
    phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							COND_ICC)]] << 24 |
    proc->cwp << 16;

  // set trap type
  proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_TT)] =
    proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, PRIV_TT)]] =
    inst->exception_code;
  
  // set pstate to privileged with alternate globals, disable interrupts
  // disable instruction TLB, disable data TLB for memory traps,
  // enable otherwise
  PSTATE_SET_PRIV(proc->pstate); 
  PSTATE_SET_AG(proc->pstate);
  PSTATE_CLR_IE(proc->pstate);
  PSTATE_CLR_ITE(proc->pstate);    

  if ((inst->exception_code == ITLB_MISS) ||
      (inst->exception_code == DTLB_MISS) ||
      (inst->exception_code == INSTR_FAULT) ||
      (inst->exception_code == DATA_FAULT))
    PSTATE_CLR_DTE(proc->pstate);
  else
    PSTATE_SET_DTE(proc->pstate);
}
 
 
 
 
/*--------------------------------------------------------------------------*/
 
 
 
static void FatalException(instance *inst, ProcState *proc)
{
 
#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, "Non returnable FATAL error!\n"); 
#endif

  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "Processor %d dying with FATAL error %i (%s) at time %g\n",
	     proc->proc_id,
	     inst->exception_code,
	     enames[inst->exception_code],
	     YS__Simtime);
    
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "pc = 0x%08X, tag = %lld, %s \n",
	     inst->pc,
	     inst->tag,
	     inames[inst->code.instruction]);
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "lrs1 = %d, prs1 = %d \n",
	     inst->lrs1,
	     inst->prs1);
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "lrs2 = %d, prs2 = %d \n",
	     inst->lrs2,
	     inst->prs2);
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "lrd = %d, prd = %d \n",
	     inst->lrd,
	     inst->prd);

  switch (inst->code.rs1_regtype)
    {
    case REG_INT:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1i = 0x%08X, ",
		 inst->rs1vali);
      break;
      
    case REG_FP:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1f = %f, ",
		 inst->rs1valf);
      break;
      
    case REG_FPHALF:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1fh = %f, ",
		 double(inst->rs1valfh));
      break;
      
    case REG_INTPAIR:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1p = 0x%08X/0x%08X, ",
		 inst->rs1valipair.a,inst->rs1valipair.b);
      break;
      
    case REG_INT64:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1ll = %016llX, ",
		 inst->rs1valll);
      break;
      
    default: 
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs1X = XXX, ");
      break;
    }
  
  switch (inst->code.rs2_regtype)
    {
    case REG_INT:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2i = 0x%08X, ",
		 inst->rs2vali);
      break;
      
    case REG_FP:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2f = %f, ",
		 inst->rs2valf);
      break;
      
    case REG_FPHALF:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2fh = %f, ",
		 double(inst->rs2valfh));
      break;
      
    case REG_INTPAIR:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2pair unsupported");
      break;
      
    case REG_INT64:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2ll = %80llX, ",
		 inst->rs2valll);
      break;
      
    default: 
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "rs2X = XXX, ");
      break;
    }
  
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "rsccvali = 0x%08X \n",
	     inst->rsccvali);
  
  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "addr = 0x%08X\n",
	     inst->addr);
  
  proc->exit = -1;

  YS__logmsg(proc->proc_id / ARCH_cpus,
	     "Processor %d forcing grinding halt with code %d\n",
	     proc->proc_id,
	     inst->rs1vali); 
  
  for (int n = 0; n < ARCH_mynodes * ARCH_cpus; n++)
    DoExit();
  
  exit(1);           // no graceful end -- abort all processors immediately
}


/*---------------------------------------------------------------------------*/
 
 
 
 
static int ProcessSerializedInstruction(instance *inst, ProcState *proc)
{
  switch (inst->code.instruction)
    {
      //-----------------------------------------------------------------------
    case iarithSPECIAL1:               // e.g. RD from special regs.
      if (inst->code.rs1 == STATE_TICK)
	proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						  STATE_TICK)] =
	  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						    STATE_TICK)]] =
	  (unsigned int)((long long)YS__Simtime - proc->tick_base);

      if (inst->code.rs1 == STATE_PC)
	proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						  STATE_PC)] =
	  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						    STATE_PC)]] =
	  inst->pc;

      proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
	proc->log_int_reg_file[inst->lrd] =
	proc->log_int_reg_file[inst->lrs1];
      
      return(0);

      
      //-----------------------------------------------------------------------
    case iarithSPECIAL2:               // e.g. WR to special regs.
      if (inst->code.aux1)
        proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
          proc->log_int_reg_file[inst->lrd] =
          inst->rs1vali ^ inst->code.imm;
      else
        proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
          proc->log_int_reg_file[inst->lrd] =
          inst->rs1vali ^ inst->rs2vali;

      if (inst->code.rd == STATE_TICK)
	proc->tick_base =
	  (long long)YS__Simtime-(long long)proc->log_int_reg_file[inst->lrd]; 
      
      if (inst->code.rd == STATE_PC)
	{
	  proc->pc = proc->log_int_reg_file[inst->lrd];
	  proc->npc = proc->pc + SIZE_OF_SPARC_INSTRUCTION;
	}
      
      return(0);
 

      //-----------------------------------------------------------------------
    case iWRPR:
      if (inst->code.aux1)
        proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
          proc->log_int_reg_file[inst->lrd] =
          inst->rs1vali ^ inst->code.imm;
      else
        proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
          proc->log_int_reg_file[inst->lrd] =
          inst->rs1vali ^ inst->rs2vali;


      //-----------------------------------------------------------------------
      // compute new tick base upon write to tick register

      if (inst->code.rd == PRIV_TICK)
	proc->tick_base =
	  (long long)YS__Simtime-(long long)proc->log_int_reg_file[inst->lrd]; 
      

      //-----------------------------------------------------------------------
      // assign window control variables when writing to such a register

      
      if (inst->code.rd == PRIV_CANSAVE)
	proc->cansave    = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      if ((proc->cansave > NUM_WINS-1) || (proc->cansave < 0))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Illegal value 0x%X written to CANSAVE at 0x%08X\n",
		   proc->cansave, inst->pc);

      if (inst->code.rd == PRIV_CANRESTORE)
	proc->canrestore = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      if ((proc->canrestore > NUM_WINS-1) || (proc->canrestore < 0))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Illegal value 0x%X written to CANRESTORE at 0x%08X\n",
		   proc->canrestore, inst->pc);

      if (inst->code.rd == PRIV_OTHERWIN)
	proc->otherwin = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      if ((proc->otherwin > NUM_WINS-1) || (proc->otherwin < 0))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Illegal value 0x%X written to OTHERWIN at 0x%08X\n",
		   proc->otherwin, inst->pc);

      if (inst->code.rd == PRIV_CLEANWIN)
	proc->cleanwin = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      if ((proc->cleanwin > NUM_WINS) || (proc->cleanwin < 0))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Illegal value 0x%X written to CLEANWIN at 0x%08X\n",
		   proc->cleanwin, inst->pc);

      if (inst->code.rd == PRIV_CWP)
	proc->cwp = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      if ((proc->cwp > NUM_WINS-1) || (proc->cwp < 0))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Illegal value 0x%X written to CWP at 0x%08X\n",
		   proc->cwp, inst->pc);

      
      //-----------------------------------------------------------------------
      // other supervisor state registers

      if (inst->code.rd == PRIV_PSTATE)
	proc->pstate = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
	
      if (inst->code.rd == PRIV_TL)
	proc->tl = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];

      if (inst->code.rd == PRIV_PIL)
	proc->pil = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
	

      //-----------------------------------------------------------------------

      if (inst->code.rd == PRIV_ITLB_DATA)         // trigger ITLB entry write
        proc->itlb->
          WriteEntry(proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_INDEX)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_TAG)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_CONTEXT)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_ITLB_DATA)]]);


      if (inst->code.rd == PRIV_DTLB_DATA)         // trigger DTLB entry write
        proc->dtlb->
          WriteEntry(proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_INDEX)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_TAG)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_CONTEXT)]],
                     proc->
                     phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							 proc->cwp,
							 PRIV_DTLB_DATA)]]);

      
      //-----------------------------------------------------------------------

      if (inst->code.rd == PRIV_ITLB_WIRED)        // reset ITLB random reg.
	{
	  proc->itlb_wired  = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
	  proc->itlb_random = ITLB_SIZE - 1;
	}
 
      if (inst->code.rd == PRIV_DTLB_WIRED)        // reset ITLB random reg.
	{
	  proc->dtlb_wired  = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
	  proc->dtlb_random = DTLB_SIZE - 1;
	}
 
      if (inst->code.rd == PRIV_ITLB_RANDOM)
	proc->itlb_random = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];
      
      if (inst->code.rd == PRIV_DTLB_RANDOM)
	proc->dtlb_random = proc->phy_int_reg_file[proc->intmapper[inst->lrd]];


      //-----------------------------------------------------------------------
      
      if (inst->code.rd == PRIV_TLB_COMMAND)
        {
          if (proc->phy_int_reg_file[proc->intmapper[inst->lrd]] == DTLB_CMD_PROBE)
            proc->dtlb->
              ProbeEntry(proc->
                         phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						          proc->cwp,
						          PRIV_TLB_TAG)]],
                         &(proc->
                         phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						          proc->cwp,
						          PRIV_TLB_INDEX)]]));

          if (proc->phy_int_reg_file[proc->intmapper[inst->lrd]] == ITLB_CMD_PROBE)
            proc->itlb->
              ProbeEntry(proc->
                         phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						          proc->cwp,
						          PRIV_TLB_TAG)]],
                         &(proc->
                         phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						          proc->cwp,
						          PRIV_TLB_INDEX)]]));
	  
          proc->log_int_reg_file[arch_to_log(proc,
					     proc->cwp,
					     PRIV_TLB_INDEX)] =
            proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							   proc->cwp,
						           PRIV_TLB_INDEX)]];
 
          if (proc->phy_int_reg_file[proc->intmapper[inst->lrd]] == ITLB_CMD_FLUSH)
            proc->itlb->Flush();

          if (proc->phy_int_reg_file[proc->intmapper[inst->lrd]] == DTLB_CMD_FLUSH)
            proc->dtlb->Flush();
        }
      return 0;

      
      //-----------------------------------------------------------------------
    case iRDPR:
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						PRIV_TICK)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_TICK)]] =
	(unsigned int)((long long)YS__Simtime - proc->tick_base);

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_CWP)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_CWP)]] =
	proc->cwp;
      
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_CANSAVE)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_CANSAVE)]] =
	proc->cansave;
      
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_CANRESTORE)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_CANRESTORE)]] =
	proc->canrestore;
      
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_OTHERWIN)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_OTHERWIN)]] =
	proc->otherwin;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_CLEANWIN)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_CLEANWIN)]] =
	proc->cleanwin;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_PSTATE)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_PSTATE)]] =
	proc->pstate;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_TL)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_TL)]] =
	proc->tl;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_PIL)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_PIL)]] =
	proc->pil;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_ITLB_RANDOM)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_ITLB_RANDOM)]] =
	proc->itlb_random;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_ITLB_WIRED)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_ITLB_WIRED)]] =
	proc->itlb_wired;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_DTLB_RANDOM)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_DTLB_RANDOM)]] =
	proc->dtlb_random;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp, PRIV_DTLB_WIRED)] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
							   PRIV_DTLB_WIRED)]] =
	proc->dtlb_wired;


      
      proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
        proc->log_int_reg_file[inst->lrd] =
        proc->phy_int_reg_file[proc->intmapper[inst->lrs1]];
      return 0;

      
      //-----------------------------------------------------------------------
    case iMULScc:
      fnMULScc_serialized(inst, proc);
      return 0;
 
      
      //-----------------------------------------------------------------------
    case iSMULcc:
      fnSMULcc_serialized(inst, proc);
      return 0;
      
 
      //-----------------------------------------------------------------------
    case iUMULcc:
      fnUMULcc_serialized(inst, proc);
      return 0;
 
      
      //-----------------------------------------------------------------------
    case iLDFSR:
 
      //-----------------------------------------------------------------------
    case iLDXFSR:
      set_fsr(proc, inst);
      return 0;
 
      
      //-----------------------------------------------------------------------
    case iSTFSR:                    // simply retry instruction, it will
    case iSTXFSR:                   // succeed as there are no FP ops ahead
      {
	proc->pc = inst->pc;
	proc->npc = inst->npc;
	return 1;
      } 
 
      //-----------------------------------------------------------------------
    case iSAVRESTD:
      {
        if (inst->code.aux1 == 0)  // SAVE
          {
            if ((proc->cansave != 0) && (inst->code.instruction == iSAVE))
              YS__logmsg(proc->proc_id / ARCH_cpus,
			 "Why did we have a SAVED with CANSAVE != 0 ?\n");
 
            proc->cansave++;
	    if (proc->otherwin == 0)
	      proc->canrestore--;
	    else
	      proc->otherwin--;

#ifdef COREFILE
            if (YS__Simtime > DEBUG_TIME)
              fprintf(corefile,
                      "SAVED makes CANSAVE %d, CANRESTORE %d, OTHERWIN %d, CLEANWIN %d\n",
                      proc->cansave,
                      proc->canrestore,
		      proc->otherwin,
		      proc->cleanwin);
#endif
            
          }
        else                        // RESTORE
          {
            proc->canrestore++;
	    if (proc->otherwin == 0)
	      proc->cansave--;
	    else
	      proc->otherwin--;

	    if (proc->cleanwin != NUM_WINS)
	      proc->cleanwin++;
	    
#ifdef COREFILE
            if (YS__Simtime > DEBUG_TIME)
              fprintf(corefile,
                      "RESTORED makes CANSAVE %d, CANRESTORE %d, OTHERWIN %d, CLEANWIN %d\n",
                      proc->cansave,
                      proc->canrestore,
		      proc->otherwin,
		      proc->cleanwin);
#endif
            
          }
      }
      break;
 
 
      //-----------------------------------------------------------------------
    case iDONERETRY:
      {
	int tt = proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
								    proc->cwp,
								    PRIV_TT)]];
	
        // statistics: add cycle count and instruction count
        proc->cycles[tt] += (long long)YS__Simtime + proc->DELAY -
	  proc->start_cycle[tt];

        proc->graduated[tt] += proc->graduates - proc->start_graduated[tt];
	proc->start_graduated[tt] = 0;

        //---------------------------------------------------------------------
        // return from trap state

        if (inst->code.aux1 == 0)         // DONE
          {
	    proc->pc =
	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							PRIV_TNPC)]];
            proc->npc =
	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							PRIV_TNPC)]]+SIZE_OF_SPARC_INSTRUCTION;
          }
        else                               // RETRY
          {
	    proc->pc =
	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							PRIV_TPC)]];
            proc->npc =
	      proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							proc->cwp,
							PRIV_TNPC)]];
          }

	
        // common operation ---------------------------------------------------
        // restore PState & CCR, decrement TL
	proc->pstate = 
	  proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						    PRIV_PSTATE)] =
          proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						    PRIV_PSTATE)]] =
          proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						    proc->cwp,
						    PRIV_TSTATE)]] & 0x0000FFFF;

	proc->cwp =
	  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
						    proc->cwp,
						    PRIV_TSTATE)]] >> 16;
	proc->cwp &= 0x000000FF;

        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]] =
          proc->log_int_reg_file[arch_to_log(proc, proc->cwp, COND_ICC)] =
          proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, PRIV_TSTATE)]] >> 24;
          

	proc->tl--;
        proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						  PRIV_TL)] --;
        proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  PRIV_TL)]] --;
 
        if (proc->tl < 0)
	  YS__errmsg(proc->proc_id / ARCH_cpus,
		     "DoneRetry arrives at TrapLevel %i at 0x%08X\n",
		     proc->tl, inst->pc);


      if (!PSTATE_GET_PRIV(proc->pstate))
	{
	  proc->start_graduated[OK] = proc->graduates;
	  proc->start_cycle[OK] = proc->curr_cycle;
	}

	
#ifdef TRACE
	if (YS__Simtime > TRACE)
	  {
	    YS__logmsg(proc->proc_id / ARCH_cpus,
		       "    [%i] Restore PC: 0x%X  NPC: 0x%X  PState: 0x%X  ICC: 0x%X\n",
		       proc->proc_id,
		       proc->pc,
		       proc->npc,
		       proc->pstate,
		       proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]]);
 
	    YS__logmsg(proc->proc_id / ARCH_cpus,
		       "    [%i] CWP: %i CanSave: %i Canrestore: %i Otherwin: %i Cleanwin: %i\n",
		       proc->proc_id,
		       proc->cwp,
		       proc->cansave,
		       proc->canrestore,
		       proc->otherwin,
		       proc->cleanwin);
	  }
#endif
 
        return 1;
      }
      
 
      //-----------------------------------------------------------------------
    default:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Unexpected serialized instruction.\n");
      exit(-1);
      break;
    }
 
  return 0;
}
