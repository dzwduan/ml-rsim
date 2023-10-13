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

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/cache.h"
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



#define min(a, b)               ((a) > (b) ? (b) : (a))



int   ProcState::numprocs = 0;
int   aliveprocs = 0;


ProcState  **AllProcs;



/* set the names for factors leading to efficiency loss (see state.h) */

const char *eff_loss_names[eNUM_EFF_STALLS] =
{
  "OK                   ",
  "Branch in cycle      ",
  "Unpredicted branch   ",
  "Shadow mappers full  ",
  "Rename registers full",
  "Memory queue full    ",
  "Issue queue full     "
};


const char *fuusage_names[numUTYPES] =
{
  "ALU utilization       ",
  "FPU utilization       ",
  "Addr. gen. utilization",
  "Cache port utilization"
};


static void ResetInst(instance * inst)
{
  inst->tag   = -1;
  inst->inuse = 0;
}




/***********************************************************************/
/* ******************** state class constructor ********************** */
/***********************************************************************/

ProcState::ProcState(int proc):
  active_list  (MAX_ACTIVE_NUMBER),
  tag_cvt      (MAX_ACTIVE_INSTS + 3),
  FreeingUnits (ALU_UNITS + FPU_UNITS + ADDR_UNITS + MEM_UNITS),
  instances    (MAX_ACTIVE_INSTS + FETCH_QUEUE_SIZE + 1, ResetInst),
  meminstances (MAX_ACTIVE_INSTS),
  bqes         (MAX_SPEC + 2),
  mappers      (MAX_SPEC + 1),
  ministallqs  ((MAX_ACTIVE_INSTS + 1) * 7),
  actives      (MAX_ACTIVE_NUMBER + 3),
  tagcvts      (MAX_ACTIVE_INSTS + 3)
{
  int i, j, n;

  proc_id = proc;
  aliveprocs++;
  halt = 0;
  
#ifdef COREFILE
  char proc_file_name[80];
  sprintf(proc_file_name, "corefile.%d", proc_id);
  corefile = fopen(proc_file_name, "w");
#else
  corefile = NULL;
#endif

  l1i_argptr = L1ICaches[proc_id];
  l1d_argptr = L1DCaches[proc_id];
  l2_argptr  = L2Caches[proc_id];
  wb_argptr  = WBuffers[proc_id];

  fetch_rate      = FETCHES_PER_CYCLE;
  decode_rate     = DECODES_PER_CYCLE;
  graduate_rate   = GRADUATES_PER_CYCLE;
  max_active_list = MAX_ACTIVE_NUMBER;

  exit = 0;                  /* don't exit yet */
  interrupt_pending = 0;     /* no interrupt   */

  PageTable = PageTables[proc_id / ARCH_cpus];

  
  // Initialize privileged state --------------------------------------------

  pc = 0;

  pstate = 0;
  PSTATE_SET_PRIV(pstate);
  PSTATE_CLR_ITE(pstate);
  PSTATE_CLR_DTE(pstate);
  PSTATE_CLR_AG(pstate);
  PSTATE_CLR_IE(pstate);
  PSTATE_SET_FPE(pstate);

  tl = 0;
  pil = 0;
  
  itlb_wired  = dtlb_wired = 0;

  if (ToPowerOf2(NUM_WINS) != NUM_WINS)
    YS__errmsg(proc_id / ARCH_cpus,
	       "NumWins (%i) must be power of 2\n", NUM_WINS);
  
  cwp = 0;
  cansave = NUM_WINS - 2;
  canrestore = 0;
  otherwin   = 0;
  cleanwin   = NUM_WINS;
  tick_base  = 0ll;

  
  // Initialize FP state ----------------------------------------------------

  fp_trap_type    = 0;
  fp754_trap_mask = 0;
  fp754_aexc      = 0;
  fp754_cexc      = 0;
  
  
  //-------------------------------------------------------------------------
  
  instruction_count = 0;
  curr_cycle        = 0;
  graduation_count  = 0;
  last_graduated    = 0;

  /* initialize the free lists and busy lists */
  //  short *origfpfree = new short[NO_OF_PHY_FP_REGS];
  //  for (i = 0, j = NO_OF_LOG_FP_REGS; j < NO_OF_PHY_FP_REGS; i++, j++)
  //    origfpfree[i] = j;

  free_fp_list = new freelist(NO_OF_PHY_FP_REGS);//, origfpfree,
  //NO_OF_PHY_FP_REGS - NO_OF_LOG_FP_REGS);

  //  short *origintfree = new short[NO_OF_PHY_INT_REGS];
  //  for (i = 0, j = NO_OF_LOG_INT_REGS; j < NO_OF_PHY_INT_REGS; i++, j++)
  //    origintfree[i] = j;

  free_int_list = new freelist(NO_OF_PHY_INT_REGS);//, origintfree,
  //			       NO_OF_PHY_INT_REGS - NO_OF_LOG_INT_REGS);
  
  intregbusy = new char[NO_OF_PHY_INT_REGS];
  fpregbusy  = new char[NO_OF_PHY_FP_REGS];
  memset(fpregbusy,
	 0,
	 NO_OF_PHY_FP_REGS * sizeof(char));
  memset(intregbusy,
	 0,
	 NO_OF_PHY_INT_REGS * sizeof(char));

  memset(log_int_reg_file,
         0,
         sizeof(log_int_reg_file));
  memset(log_fp_reg_file,
         0,
         sizeof(log_fp_reg_file));
  memset(phy_int_reg_file,
         0,
         sizeof(phy_int_reg_file));
  memset(phy_fp_reg_file,
         0,
         sizeof(phy_fp_reg_file));

  setup_arch_to_log_table(this);
  

  //---------------------------------------------------------------------------
  // setup TLBs

  if ((TLB_UNIFIED == 0) && ((ITLB_SIZE == 0) || (DTLB_SIZE == 0)))
    {
      TLB_UNIFIED = 1;
      if (DTLB_SIZE == 0)
	{
	  DTLB_TYPE          = ITLB_TYPE;
	  DTLB_SIZE          = ITLB_SIZE;
	  DTLB_ASSOCIATIVITY = ITLB_ASSOCIATIVITY;
	  DTLB_TAGGED        = ITLB_TAGGED;
	}
      else
	{
	  ITLB_TYPE          = DTLB_TYPE;
	  ITLB_SIZE          = DTLB_SIZE;
	  ITLB_ASSOCIATIVITY = DTLB_ASSOCIATIVITY;
	  ITLB_TAGGED        = DTLB_TAGGED;
	}
    }

  if (ITLB_SIZE == 0)
    {
      if (DTLB_SIZE == 0)
	YS__errmsg(proc_id / ARCH_cpus,
		   "At least one TLB size must be non-zero!");

      dtlb = new TLB(this, DTLB_TYPE, DTLB_SIZE, DTLB_ASSOCIATIVITY,
		     DTLB_TAGGED);
      itlb = new TLB(dtlb);
    }
  else
    {
      itlb = new TLB(this, ITLB_TYPE, ITLB_SIZE, ITLB_ASSOCIATIVITY,
		     ITLB_TAGGED);

      if (DTLB_SIZE == 0)
	dtlb = new TLB(itlb);
      else
	dtlb = new TLB(this, DTLB_TYPE, DTLB_SIZE, DTLB_ASSOCIATIVITY,
		       DTLB_TAGGED);
    }

  
  //------------------------------------------------------------------------
  
  /* Initialize the mappers */
  activemaptable = NewMapTable(this);
  if (activemaptable == NULL)
    YS__errmsg(proc_id / ARCH_cpus, "Got a NULL map table entry!!");

  fpmapper  = activemaptable->fmap;
  intmapper = activemaptable->imap;
  for (i = 0; i < NO_OF_LOG_FP_REGS; i++)
    fpmapper[i] = free_fp_list->getfreereg();
  //    fpmapper[i] = i;

  intmapper[0] = 0;
  for (i = 1; i < NO_OF_LOG_INT_REGS; i++)
    intmapper[i] = free_int_list->getfreereg();
  //    intmapper[i] = i;

  //---------------------------------------------------------------------------
  // initialize registers
  
  dtlb_random = DTLB_SIZE - 1;
  itlb_random = ITLB_SIZE - 1;
  
  log_int_reg_file[arch_to_log(this, cwp, PRIV_PSTATE)] = 
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_PSTATE)]] = pstate;

  log_int_reg_file[arch_to_log(this, cwp, PRIV_PIL)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_PIL)]] = pil;

  log_int_reg_file[arch_to_log(this, cwp, PRIV_TL)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_TL)]] = tl;
  
  log_int_reg_file[arch_to_log(this, cwp, PRIV_VER)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_VER)]] =
    (42 << 24) | (NUM_TRAPS << 8) | NUM_WINS-1;

  // initialize user & supervisor tick register
 
  log_int_reg_file[arch_to_log(this, cwp, PRIV_TICK)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_TICK)]] = 0;
 
  log_int_reg_file[arch_to_log(this, cwp, STATE_TICK)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, STATE_TICK)]] = 0;
  
  log_int_reg_file[arch_to_log(this, cwp, STATE_FPRS)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, STATE_FPRS)]] = FPRS_FEF;
  
  // initialize page table base, random & wired register

  log_int_reg_file[arch_to_log(this, cwp, PRIV_TLB_CONTEXT)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_TLB_CONTEXT)]] = 0;
 
  log_int_reg_file[arch_to_log(this, cwp, PRIV_ITLB_RANDOM)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_ITLB_RANDOM)]] =
    itlb_random;
 
  log_int_reg_file[arch_to_log(this, cwp, PRIV_DTLB_RANDOM)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_DTLB_RANDOM)]] =
    dtlb_random;
 
  log_int_reg_file[arch_to_log(this, cwp, PRIV_ITLB_WIRED)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_ITLB_WIRED)]] =
    itlb_wired;
 
  log_int_reg_file[arch_to_log(this, cwp, PRIV_DTLB_WIRED)] =
    phy_int_reg_file[intmapper[arch_to_log(this, cwp, PRIV_DTLB_WIRED)]] =
    dtlb_wired;

  //------------------------------------------------------------------------
  /* Initialize Ready Queues */
  for (i = 0; i < numUTYPES; i++)
    {
      ReadyQueues[i].start(MAX_ACTIVE_INSTS);
      active_instr[i] = 0;
    }

  max_active_instr[uALU] = MAX_ALU_OPS;
  max_active_instr[uFP]  = MAX_FPU_OPS;
  max_active_instr[uMEM] = MAX_MEM_OPS;

  ldissues = 0;

  DELAY = 1;
  stall_the_rest = 0;
  type_of_stall_rest = eNOEFF_LOSS;
  stalledeff = 0;

  fetch_queue_size = FETCH_QUEUE_SIZE;   // number of entries in the fetch q 

  fetch_pc   = 0;                        // the next pc to be fetched
  fetch_done = 1;
  fetch_queue = new Queue<fetch_queue_entry>;
  inst_save = NULL;

  in_exception = NULL;

  // Prediction
  copymappernext = 0;
  unpredbranch = 0;
  BPBSetup();                            // setup branch prediction buffer
  RASSetup();                            // setup RAS


  // Prefetch
  if (Prefetch)
    {
      typedef instance *instp;
      max_prefs = MEM_UNITS;
      prefrdy = new instp[MEM_UNITS];
    }

#ifndef STORE_ORDERING
  StoresToMem = 0;
  SStag = LStag = SLtag = LLtag = MEMISSUEtag = -1;
#endif
  ReadyUnissuedStores = 0;
  sync = 0;

  //-------------------------------------------------------------------------
  // Statistics

  start_time = 0;
  start_icount = 0;
  graduates = 0;
  bpb_good_predicts = 0;
  bpb_bad_predicts  = 0;
  ras_good_predicts = 0;
  ras_bad_predicts  = 0;
  ras_underflows    = 0;
  ras_overflows     = 0;

  ldspecs = 0;
  last_counted = 0;

  for (n = 0; n < MAX_EXCEPT; n++)
    {
      exceptions[n] = 0;
      graduated[n] = 0;
      cycles[n] = 0;
      start_cycle[n] = 0;
      start_graduated[n] = 0;
    }

  total_halted = 0;
  mem_refs = 0;
 
  sim_start_time = time(NULL);
  
  stats_phase = 0;     // start out in a convenient phase #0

  limbos = 0;
  unlimbos = 0;
  redos = 0;
  kills = 0;
  vsbfwds = 0;
  fwds = 0;
  partial_overlaps = 0;
  avail_fetch_slots = 0;

#ifndef NOSTAT
  // total number of instructions flushed on bad predicts
  BadPredFlushes = NewStatrec(proc_id / ARCH_cpus,
			      "Bad prediction flushes",
			      POINT, MEANS, NOHIST, 8, 0, MAX_ACTIVE_INSTS);
  
  // total number of instructions flushed on exceptions
  ExceptFlushed = NewStatrec(proc_id / ARCH_cpus,
			     "Exception flushes",
			     POINT, MEANS, NOHIST, 8, 0, MAX_ACTIVE_INSTS);

  SpecStats = NewStatrec(proc_id / ARCH_cpus,
			 "Speculation level", POINT, MEANS, NOHIST,
			 MAX_SPEC, 0, MAX_SPEC);

  FUUsage[int (uALU)] = NewStatrec(proc_id / ARCH_cpus,
				   fuusage_names[uALU],
				   POINT, MEANS, HIST, ALU_UNITS,
				   0, ALU_UNITS);
  FUUsage[int (uFP)] = NewStatrec(proc_id / ARCH_cpus,
				  fuusage_names[uFP],
				  POINT, MEANS, HIST, FPU_UNITS, 0, FPU_UNITS);
  FUUsage[int (uMEM)] = NewStatrec(proc_id / ARCH_cpus,
				   fuusage_names[uMEM],
				   POINT, MEANS, HIST, MEM_UNITS,
				   0, MEM_UNITS);
  FUUsage[int (uADDR)] = NewStatrec(proc_id / ARCH_cpus,
				    fuusage_names[uADDR],
				    POINT, MEANS, HIST, ADDR_UNITS,
				    0, ADDR_UNITS);

  
#ifndef STORE_ORDERING
  VSB = NewStatrec(proc_id / ARCH_cpus,
		   "Virtual Store Buffer size",
		   POINT, MEANS, NOHIST, 10, 0, 100);
  LoadQueueSize = NewStatrec(proc_id / ARCH_cpus,
			     "Load queue size",
			     POINT, MEANS, HIST, 8, 0, MAX_MEM_OPS);
#else
  MemQueueSize = NewStatrec(proc_id / ARCH_cpus,
			    "Mem queue size",
			    POINT, MEANS, HIST, MAX_MEM_OPS / 8,
			    0, MAX_MEM_OPS);
#endif

  // size of fetch queue
  FetchQueueStats = NewStatrec(proc_id / ARCH_cpus,
			       "Fetch queue size",
			       POINT, MEANS, HIST, min(8, fetch_queue_size), 0,
			       fetch_queue_size);

  // size of active list
  ActiveListStats = NewStatrec(proc_id / ARCH_cpus,
			       "Active list size", POINT, MEANS,
			       HIST, 8, 0, MAX_ACTIVE_INSTS);

  readacc   = NewStatrec(proc_id / ARCH_cpus,
			 "Read accesses",
			 POINT, MEANS, NOHIST, 5, 0, 10);
  writeacc  = NewStatrec(proc_id / ARCH_cpus,
			 "Write accesses",
			 POINT, MEANS, NOHIST, 5, 0,10);
  rmwacc    = NewStatrec(proc_id / ARCH_cpus,
			 "RMW accesses",
			 POINT, MEANS, NOHIST, 5, 0, 10);

  readiss   = NewStatrec(proc_id / ARCH_cpus,
			 "Read accesses (from issue)",
			 POINT, MEANS, NOHIST, 5, 0, 10);
  writeiss  = NewStatrec(proc_id / ARCH_cpus,
			 "Write accesses (from issue)",
			 POINT, MEANS, NOHIST, 5, 0, 10);
  rmwiss    = NewStatrec(proc_id / ARCH_cpus,
			 "RMW accesses (from issue)",
			 POINT, MEANS, NOHIST, 5, 0, 10);

  readact   = NewStatrec(proc_id / ARCH_cpus,
			 "Read active",
			 POINT, MEANS, NOHIST, 5, 0, 10);
  writeact  = NewStatrec(proc_id / ARCH_cpus,
			 "Write active",
			 POINT, MEANS, NOHIST, 5, 0, 10);
  rmwact    = NewStatrec(proc_id / ARCH_cpus,
			 "RMW active",
			 POINT, MEANS, NOHIST, 5, 0, 10);

  
#if 0
  for (i = 0; i < reqNUM_REQ_STAT_TYPE; i++)
    {
      demand_read[i]      = NewStatrec(proc_id / ARCH_cpus,
				       "Demand read",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_write[i]     = NewStatrec(proc_id / ARCH_cpus,
				       "Demand write",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_rmw[i]       = NewStatrec(proc_id / ARCH_cpus,
				       "Demand rmw",
				       POINT, MEANS, NOHIST, 0, 0, 0);

      demand_read_iss[i]  = NewStatrec(proc_id / ARCH_cpus,
				       "Demand read (issued)",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_write_iss[i] = NewStatrec(proc_id / ARCH_cpus,
				       "Demand write (issued)",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_rmw_iss[i]   = NewStatrec(proc_id / ARCH_cpus,
				       "Demand rmw (issued)",
				       POINT, MEANS, NOHIST, 0, 0, 0);

      demand_read_act[i]  = NewStatrec(proc_id / ARCH_cpus,
				       "Demand read (active)",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_write_act[i] = NewStatrec(proc_id / ARCH_cpus,
				       "Demand write (active)",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      demand_rmw_act[i]   = NewStatrec(proc_id / ARCH_cpus,
				       "Demand rmw (active)",
				       POINT, MEANS, NOHIST, 0, 0, 0);

      pref_sh[i]          = NewStatrec(proc_id / ARCH_cpus,
				       "pref clean",
				       POINT, MEANS, NOHIST, 0, 0, 0);
      pref_excl[i]        = NewStatrec(proc_id / ARCH_cpus,
				       "pref excl",
				       POINT, MEANS, NOHIST, 0, 0, 0);
    }
#endif

  in_except = NewStatrec(proc_id / ARCH_cpus,
			 "Waiting for exceptions",
			 POINT, MEANS, NOHIST, 5, 0, 50);


  for (int lat_ctr = 0; lat_ctr < int (lNUM_LAT_TYPES); lat_ctr++)
    lat_contrs[lat_ctr] = NewStatrec(proc_id / ARCH_cpus,
				     lattype_names[lat_ctr],
				     POINT, MEANS, NOHIST, 0, 0, 0);

  partial_otime = NewStatrec(proc_id / ARCH_cpus,
			     "Partial Overlap time",
			     POINT, MEANS, NOHIST, 0, 0, 0);
#endif

  
  avail_fetch_slots = 0;
  for (i = 0; i < int (lNUM_LAT_TYPES); i++)
    avail_active_full_losses[i] = 0;

  for (i = 0; i < int (eNUM_EFF_STALLS); i++)
    eff_losses[i] = 0;

  UnitSetup(this, 0);
}




/*************************************************************************/
/* reset_lists  : Initialize the register files and the mappers          */
/*************************************************************************/
 

int ProcState::reset_lists()
{
  int i;
  /* Set busy register lists and free register lists */

  instances.reset();
 
  copymappernext = 0;
  unpredbranch   = 0;
  unstall_the_rest(this);
  
  intregbusy[intmapper[ZEROREG]] = 0;

  memset((char *)fpregbusy, 0, NO_OF_PHY_FP_REGS * sizeof(char));
  memset((char *)intregbusy, 0, NO_OF_PHY_INT_REGS * sizeof(char));
  
  // Free all the inuse registers
  free_int_list->reset();
  free_fp_list->reset();
  
  /* Initialize the mappers */
  for (i = 0; i < NO_OF_LOG_FP_REGS; i++)
    fpmapper[i] = free_fp_list->getfreereg();
  //    fpmapper[i] = i;

  intmapper[ZEROREG] = ZEROREG;
  for (i = 1; i < NO_OF_LOG_INT_REGS; i++)
    intmapper[i] = free_int_list->getfreereg();
  //    intmapper[i] = i;

  // Reallocate and initialize physical registers to logical values
  for (i = 0; i < NO_OF_LOG_FP_REGS; i++)
    phy_fp_reg_file[fpmapper[i]] = log_fp_reg_file[i];
  for (i = 0; i < NO_OF_LOG_INT_REGS; i++)
    phy_int_reg_file[intmapper[i]] = log_int_reg_file[i];
  //  memcpy(phy_fp_reg_file,
  //         log_fp_reg_file,
  //         NO_OF_LOG_FP_REGS * sizeof(double));
  //  memcpy(phy_int_reg_file,
  //         log_int_reg_file,
  //         NO_OF_LOG_INT_REGS * sizeof(int));
  
  /* Note :The CURRENT WINDOW POINTER REMAINS UNCHANGED */
  
  return 0;
}



extern "C"
{
int Proc_stat_report(int nid, int pid);
void Proc_stat_clear(int nid, int pid);

int Proc_stat_report(int nid, int pid)
{
  if (AllProcs[nid * ARCH_cpus + pid] == NULL)
    return 0;

  AllProcs[nid * ARCH_cpus + pid]->report_stats(nid);
  
  return 1;
}



void Proc_stat_clear(int nid, int pid)
{
  if (AllProcs[nid * ARCH_cpus + pid] != NULL)
    AllProcs[nid * ARCH_cpus + pid]->reset_stats();
}
}



/***************************************************************************/
/***************************************************************************/

void ProcState::report_stats(int nid)
{
  YS__statmsg(nid,
	      "Processor Clock:   %12.1f MHz (%.3f ns period)\n\n",
	      1000000.0 / (double)CPU_CLK_PERIOD,
	      (double)CPU_CLK_PERIOD / 1000.0);
  
  YS__statmsg(nid,
	      "Start cycle:            %12lld    Start instruction count: %12lld\n",
	      start_time,
	      start_icount);
  YS__statmsg(nid,
	      "End cycle:              %12lld    End instruction count:   %12lld\n",
	      curr_cycle,
	      instruction_count);
  YS__statmsg(nid,
	      "Total cycles:           %12lld    Real time:               ",
	      curr_cycle - start_time);
  PrintTime((curr_cycle - start_time) * (double)CPU_CLK_PERIOD / 1.0e12,
	    statfile[nid]);
  YS__statmsg(nid, "\n");
  YS__statmsg(nid,
	      "Non-halted cycles:      %12lld    CPU Utilization:        %8.2lf %%\n",
	      curr_cycle - start_time - total_halted,
	      100.0 * (double)(curr_cycle - start_time - total_halted) /
	      (double)(curr_cycle - start_time));
  YS__statmsg(nid,
	      "Instructions decoded:   %12lld    Decode IPC:             %8.2f\n",
	      instruction_count - start_icount,
	      (float)(instruction_count - start_icount) /
	      (float)(curr_cycle - start_time - total_halted));
  YS__statmsg(nid,
	      "Instructions graduated: %12lld    Graduate IPC:           %8.2f\n",
	      graduates,
	      (float)(graduates) / (float)(curr_cycle - start_time - total_halted));


  
  YS__statmsg(nid,
	      "\n------------------------------------------------------------------------\n");
  YS__statmsg(nid,
	      "INSTRUCTION BREAKDOWN\n\n");
  YS__statmsg(nid,
	      "Class                     Count         Cycles   Instructions       IPC\n");
  
  for (int n = 0; n < MAX_EXCEPT; n++)
    {
      if ((exceptions[n] > 0) || (n == 0))
        {
	  if (n == 0)
	    YS__statmsg(nid,
			"%s            --",
			enames[n]);
	  else
	    YS__statmsg(nid,
			"%s  %12lld",
			enames[n],
			exceptions[n]);

          if (cycles[n] > 0)
            YS__statmsg(nid,
			"   %12lld   %12lld   %8.3f\n",
			cycles[n],
			graduated[n],
			(float)((float)graduated[n]/(float)cycles[n]));
          else
            YS__statmsg(nid,
			"\n");
 
        }
    }



  YS__statmsg(nid,
	      "\n------------------------------------------------------------------------\n"); 
  YS__statmsg(nid, "TLB STATISTICS\n\n");

  if (!TLB_UNIFIED)
    {
      YS__statmsg(nid, "Instruction TLB: ");
      if (ITLB_TYPE == TLB_FULLY_ASSOC)
	YS__statmsg(nid,
		    "Fully Associative; %i entries",
		    ITLB_SIZE);
 
      if (ITLB_TYPE == TLB_SET_ASSOC)
	YS__statmsg(nid,
		    "%i Way Set Associative; %i entries",
		    ITLB_ASSOCIATIVITY, ITLB_SIZE);
 
      if (ITLB_TYPE == TLB_DIRECT_MAPPED)
	YS__statmsg(nid,
		    "Direct Mapped; %i entries",
		    ITLB_SIZE);
      
      if (ITLB_TYPE == TLB_PERFECT)
	YS__statmsg(nid,
		    "Perfect (100%% hit rate)\n\n");
      else
	{
	  if (ITLB_TAGGED)
	    YS__statmsg(nid, "; tagged");
	YS__statmsg(nid,
		    "\n\n%lld Total ITLB accesses; %lld ITLB Misses; %.4f%% ITLB Hit Rate\n\n",
		    graduates,
		    exceptions[ITLB_MISS],
		    100.0 - 100.0 * (double)exceptions[ITLB_MISS]/(double)graduates);
	}

      
      YS__statmsg(nid, "Data TLB: ");
      
      if (DTLB_TYPE == TLB_FULLY_ASSOC)
	YS__statmsg(nid,
		    "Fully Associative; %i entries",
		    DTLB_SIZE);
 
      if (DTLB_TYPE == TLB_SET_ASSOC)
	YS__statmsg(nid,
		    "%i Way Set Associative; %i entries",
		    DTLB_ASSOCIATIVITY, DTLB_SIZE);
 
      if (DTLB_TYPE == TLB_DIRECT_MAPPED)
	YS__statmsg(nid,
		    "Direct Mapped; %i entries",
		    DTLB_SIZE);
 
      if (DTLB_TYPE == TLB_PERFECT)
	YS__statmsg(nid,
		    "Perfect (100%% hit rate)\n\n");
      else
	{
	  if (DTLB_TAGGED)
	    YS__statmsg(nid, "; tagged");
	  YS__statmsg(nid,
		      "\n\n%lld Total DTLB accesses; %lld DTLB Misses; %.4f%% DTLB Hit Rate\n\n",
		      mem_refs,
		      exceptions[DTLB_MISS],
		      100.0 - 100.0 * (double)exceptions[DTLB_MISS]/(double)mem_refs);
	}
    }
  
  else       // unified TLB
    {
      YS__statmsg(nid, "Unified TLB: ");
      if (DTLB_TYPE == TLB_FULLY_ASSOC)
	YS__statmsg(nid,
		    "Fully Associative; %i entries",
		    DTLB_SIZE);
 
      if (DTLB_TYPE == TLB_SET_ASSOC)
	YS__statmsg(nid,
		    "%i Way Set Associative; %i entries",
		    DTLB_ASSOCIATIVITY, DTLB_SIZE);
 
      if (DTLB_TYPE == TLB_DIRECT_MAPPED)
	YS__statmsg(nid,
		    "Direct Mapped; %i entries",
		    DTLB_SIZE);
      
      if (DTLB_TYPE == TLB_PERFECT)
	YS__statmsg(nid,
		    "Perfect (100%% hit rate)\n\n");
      else
	{
	  if (DTLB_TAGGED)
	    YS__statmsg(nid, "; tagged");
	  YS__statmsg(nid,
		      "\n\n%lld Total ITLB accesses; %lld ITLB Misses; Miss Rate %.4f%%\n",
		      graduates,
		      exceptions[ITLB_MISS],
		      100.0 * (double)exceptions[ITLB_MISS]/(double)graduates);
      
	  YS__statmsg(nid,
		      "%lld Total DTLB accesses; %lld DTLB Misses; Miss Rate %.4f%%\n",
		      mem_refs,
		      exceptions[DTLB_MISS],
		      100.0 * (double)exceptions[DTLB_MISS]/(double)mem_refs);

	  YS__statmsg(nid,
		      "%.2lf%% Unified TLB Hit Rate\n\n",
		      100.0 - 100.0 * (double)exceptions[DTLB_MISS] /
		      (double)(graduates + mem_refs));
	}
    }




  //-------------------------------------------------------------------------

  int i;
#ifndef NOSTAT
  StatrecReport(nid, FetchQueueStats);
  StatrecReport(nid, ActiveListStats);

  StatrecReport(nid, SpecStats);
#ifndef STORE_ORDERING
  StatrecReport(nid, VSB);
  StatrecReport(nid, LoadQueueSize);
#else
  StatrecReport(nid, MemQueueSize);
#endif
#endif
  
  YS__statmsg(nid,
	      "\n------------------------------------------------------------------------\n");
  YS__statmsg(nid, "Branch Prediction Statistics\n\n");

  if (BPB_TYPE == TWOBIT)
    YS__statmsg(nid, "Two bit predictor; %i entries\n\n", BPB_SIZE);
  if (BPB_TYPE == TWOBITAGREE)
    YS__statmsg(nid, "Two bit agree predictor; %i entries\n\n", BPB_SIZE);
  if (BPB_TYPE == STATIC)
    YS__statmsg(nid, "Static predictor\n");

  YS__statmsg(nid, "%lld good predictions;  %lld bad predictions\n",
	 bpb_good_predicts, bpb_bad_predicts);
  YS__statmsg(nid, "Prediction accuracy: %.4f%%\n\n",
	 100.0 * double(bpb_good_predicts) / (bpb_good_predicts + bpb_bad_predicts));

  
  YS__statmsg(nid,
	      "\n------------------------------------------------------------------------\n");
  YS__statmsg(nid, "Return Address Stack Statistics\n\n");

  if (RAS_STKSZ > 0)
    {
      YS__statmsg(nid, "RAS size: %i\n", RAS_STKSZ);
      YS__statmsg(nid, "%lld good predictions;  %lld bad predictions\n",
		  ras_good_predicts, ras_bad_predicts);
      YS__statmsg(nid, "Prediction accuracy: %.4f%%\n",
		  100.0 * double(ras_good_predicts) / (ras_good_predicts + ras_bad_predicts));
      YS__statmsg(nid, "%lld RAS Underflows;  %lld RAS Overflows\n",
		  ras_underflows, ras_overflows);
    }
  else
    YS__statmsg(nid, "No RAS\n");
  
  
  YS__statmsg(nid,
	      "\n------------------------------------------------------------------------\n");

  YS__statmsg(nid,
	      "Loads issued: %lld, speced: %lld, limbos: %lld, unlimbos: %lld, "
	      "redos: %lld, kills: %lld\n", ldissues, ldspecs, limbos,
	      unlimbos, redos, kills);
  YS__statmsg(nid,
	      "Memory unit fwds: %lld, Virtual store buffer fwds: %lld "
	      "Partial overlaps: %lld\n", fwds, vsbfwds, partial_overlaps);

#ifndef NOSTAT
  StatrecReport(nid, BadPredFlushes);

  StatrecReport(nid, ExceptFlushed);

  for (i = 0; i < numUTYPES; i++)
    if (i != uMEM)     /* cache port utilization not really meaningul like
			  others..  */
      YS__statmsg(nid, "%s\t%12.1f%%\n", fuusage_names[i],
		  StatrecMean(FUUsage[i]) / double (MaxUnits[i]) * 100.0);


#ifdef DETAILED_STATS_LAT_CONTR
  for (i = 0; i < numUTYPES; i++)
    StatrecReport(nid, FUUsage[i]);

  for (i = 0; i < lNUM_LAT_TYPES; i++)
    StatrecReport(nid, lat_contrs[i]);

  StatrecReport(nid, partial_otime);
  StatrecReport(nid, readacc);
  StatrecReport(nid, writeacc);
  StatrecReport(nid, rmwacc);
  StatrecReport(nid, readiss);
  StatrecReport(nid, writeiss);
  StatrecReport(nid, rmwiss);
  StatrecReport(nid, readact);
  StatrecReport(nid, writeact);
  StatrecReport(nid, rmwact);
#endif

  StatrecReport(nid, in_except);

  long long drd  = StatrecSamples(readacc);
  long long dwt  = StatrecSamples(writeacc);
  long long drmw = StatrecSamples(rmwacc);

#if 0
  double dmds = drd + dwt + drmw;

  for (i = 0; i < (int) reqNUM_REQ_STAT_TYPE; i++)
    {
      long long d = StatrecSamples(demand_read[i]);
      YS__statmsg(nid,
		  "Demand read %s -- Num %lld (%.3f,%.3f) Mean %.3f/%.3f/%.3f"
		  " Stddev %.3f/%.3f/%.3f\n",
		  Req_stat_type[i],
		  d,
		  (double)d/(double)drd,
		  (double)d / (double)dmds,
		  StatrecMean(demand_read_act[i]),
		  StatrecMean(demand_read[i]),
		  StatrecMean(demand_read_iss[i]),
		  StatrecSdv(demand_read_act[i]),
		  StatrecSdv(demand_read[i]),
		  StatrecSdv(demand_read_iss[i]));
    }
  
  for (i = 0; i < (int) reqNUM_REQ_STAT_TYPE; i++)
    {
      long long d = StatrecSamples(demand_write[i]);
      YS__statmsg(nid,
		  "Demand write %s -- Num %lld (%.3f,%.3f) Mean %.3f/%.3f/%.3f"
		  " Stddev %.3f/%.3f/%.3f\n",
		  Req_stat_type[i],
		  d,
		  (double)d / (double)dwt,
		  (double)d / (double)dmds,
		  StatrecMean(demand_write_act[i]),
		  StatrecMean(demand_write[i]),
		  StatrecMean(demand_write_iss[i]),
		  StatrecSdv(demand_write_act[i]),
		  StatrecSdv(demand_write[i]),
		  StatrecSdv(demand_write_iss[i]));
    }
  
  for (i = 0; i < (int) reqNUM_REQ_STAT_TYPE; i++)
    {
      long long d = StatrecSamples(demand_rmw[i]);
      YS__statmsg(nid,
		  "Demand rmw %s -- Num %lld (%.3f,%.3f) Mean %.3f/%.3f/%.3f"
		  "Stddev %.3f/%.3f/%.3f\n",
		  Req_stat_type[i],
		  d,
		  (double)d / (double)drmw,
		  (double)d / (double)dmds,
		  StatrecMean(demand_rmw_act[i]),
		  StatrecMean(demand_rmw[i]),
		  StatrecMean(demand_rmw_iss[i]),
		  StatrecSdv(demand_rmw_act[i]),
		  StatrecSdv(demand_rmw[i]),
		  StatrecSdv(demand_rmw_iss[i]));
    }
  
  for (i = 0; i < (int) reqNUM_REQ_STAT_TYPE; i++)
    {
      long long d = StatrecSamples(pref_sh[i]);
      YS__statmsg(nid,
		  "Pref sh %s -- Num %lld Mean %.3f Stddev %.3f\n",
		  Req_stat_type[i],
		  d,
		  StatrecMean(pref_sh[i]),
		  StatrecSdv(pref_sh[i]));
    }
  
  for (i = 0; i < (int) reqNUM_REQ_STAT_TYPE; i++)
    {
      long long d = StatrecSamples(pref_excl[i]);
      YS__statmsg(nid,
		  "Pref excl %s -- Num %lld Mean %.3f Stddev %.3f\n",
		  Req_stat_type[i],
		  d,
		  StatrecMean(pref_excl[i]),
		  StatrecSdv(pref_excl[i]));
    }
#endif
#endif
  
  YS__statmsg(nid, "\n");

  for (i = 0; i < lNUM_LAT_TYPES; i++)
    YS__statmsg(nid,
		"Avail loss from %s   %12.3f\n", lattype_names[i],
		avail_active_full_losses[i] /
		(double (decode_rate) * (curr_cycle - start_time)));

  YS__statmsg(nid, "\n");

  for (i = 0; i < eNUM_EFF_STALLS; i++)   // don't count 0 since this is OK 
    YS__statmsg(nid,
		"Efficiency loss from %s   %12.3f\n", eff_loss_names[i],
		double (eff_losses[i]) / double (avail_fetch_slots));

  double ifetch = instruction_count - start_icount;

  YS__statmsg(nid, "\n");

#ifndef NOSTAT
  YS__statmsg(nid,
	      "Utility losses from misspecs  %.3f excepts: %.3f\n",
	      StatrecSum(BadPredFlushes) / ifetch,
	      StatrecSum(ExceptFlushed) / ifetch);
#endif
  
  YS__statmsg(nid,
	      "\n\n");
}




void ProcState::report_phase_fast(int pid)
{
  int nid = pid / ARCH_cpus;
  
  YS__statmsg(nid,
	      "STAT Processor: %d EndPhase: %d Issued: %lld Graduated: %lld\n",
	      proc_id,
	      stats_phase,
	      instruction_count - start_icount,
	      graduates);
  
  YS__statmsg(nid,
	      "STAT Execution time: %lld Start time: %lld Since last grad: %lld\n",
	      curr_cycle - start_time,
	      start_time,
	      curr_cycle - last_graduated);

#ifndef NOSTAT
  for (int i = 0; i < lNUM_LAT_TYPES; i++)
    YS__statmsg(nid,
		"STAT %s: Grads %lld Ratio %.4f\n",
		lattype_names[i],
		StatrecSamples(lat_contrs[i]),
		double(StatrecSamples(lat_contrs[i])) * StatrecMean(lat_contrs[i]) /
    double(curr_cycle - start_time));
#endif
	
  YS__statmsg(nid,
	      "STAT Branch prediction rate: %.4f\n",
	      double(bpb_good_predicts) / double(bpb_good_predicts + bpb_bad_predicts));
  
  YS__statmsg(nid,
	      "STAT Return prediction rate: %.4f\n",
	      double(ras_good_predicts) / double(ras_good_predicts + ras_bad_predicts));

#ifndef NOSTAT
  YS__statmsg(nid,
	      "STAT Reads Mean (ACT): %.3f Stddev: %.3f\n",
	      StatrecMean(readact),
	      StatrecSdv(readact));
  
  YS__statmsg(nid,
	      "STAT Writes Mean (ACT): %.3f Stddev: %.3f\n",
	      StatrecMean(writeact),
	      StatrecSdv(writeact));
  
  YS__statmsg(nid,
	      "STAT RMW Mean (ACT): %.3f Stddev: %.3f\n",
	      StatrecMean(rmwact),
	      StatrecSdv(rmwact));
  
  YS__statmsg(nid,
	      "STAT Reads Mean (EA): %.3f Stddev: %.3f\n",
	      StatrecMean(readacc),
	      StatrecSdv(readacc));
  
  YS__statmsg(nid,
	      "STAT Writes Mean (EA): %.3f Stddev: %.3f\n",
	      StatrecMean(writeacc),
	      StatrecSdv(writeacc));
  
  YS__statmsg(nid,
	      "STAT RMW Mean (EA): %.3f Stddev: %.3f\n",
	      StatrecMean(rmwacc),
	      StatrecSdv(rmwacc));
  
  YS__statmsg(nid,
	      "STAT Reads Mean (ISS): %.3f Stddev: %.3f\n",
	      StatrecMean(readiss),
	      StatrecSdv(readiss));
  
  YS__statmsg(nid,
	      "STAT Writes Mean (ISS): %.3f Stddev: %.3f\n",
	      StatrecMean(writeiss),
	      StatrecSdv(writeiss));
  
  YS__statmsg(nid,
	      "STAT RMW Mean (ISS): %.3f Stddev: %.3f\n",
	      StatrecMean(rmwiss),
	      StatrecSdv(rmwiss));
  
  YS__statmsg(nid,
	      "STAT ExceptionWait Mean: %.3f Stddev: %.3f\n",
	      StatrecMean(in_except),
	      StatrecSdv(in_except));
#endif
  
  double avail = double(avail_fetch_slots) /
    (double(decode_rate) * double(curr_cycle - start_time));
  
  double eff  = double(instruction_count - start_icount) / double(avail_fetch_slots);
  double util = double(graduates) / double(instruction_count - start_icount);
  
  YS__statmsg(nid,
	      "STAT Availability: %.3f Efficiency: %.3f Utility: %.3f\n",
	      avail,
	      eff,
	      util);
  
  YS__statmsg(nid, "\n");
}



/*************************************************************************/
/* state::end_phase : Full statistics dump at the end of a phase         */
/*************************************************************************/

void ProcState::endphase(int nid)
{
  report_stats(nid);
  reset_stats();
}


/*************************************************************************/
/* state::newphase: Reset phase collection statistics at new phase       */
/*************************************************************************/

void ProcState::newphase(int phase)
{
  reset_stats();       // to kill the statistics collected in the interphases
  stats_phase = phase;
}



/*************************************************************************/
/* state::reset_stats : Reset phase collection statistics                */
/*************************************************************************/

void ProcState::reset_stats()
{
  int i;

  start_time = curr_cycle;
  start_icount = instruction_count;
  graduates = 0;
  bpb_good_predicts = bpb_bad_predicts = 0;
  ras_good_predicts = ras_bad_predicts = 0;
  ras_underflows = ras_overflows = 0;

#ifndef NOSTAT
  StatrecReset(BadPredFlushes);
#endif
  
  for (int n = 0; n < MAX_EXCEPT; n++)
    {
      exceptions[n] = 0;
      graduated[n] = 0;
      cycles[n] = 0;
      start_cycle[n] = curr_cycle;
      start_graduated[n] = 0;
    }

  total_halted = 0;
  mem_refs = 0;

  ldissues = ldspecs = limbos = unlimbos = redos = 0;
  kills = vsbfwds = fwds = partial_overlaps = 0;
  avail_fetch_slots = 0;
  stats_phase = -1;

#ifndef NOSTAT
  StatrecReset(ExceptFlushed);
  StatrecReset(SpecStats);

  for (i = 0; i < numUTYPES; i++)
    StatrecReset(FUUsage[i]);

#ifndef STORE_ORDERING
  StatrecReset(VSB);
  StatrecReset(LoadQueueSize);
#else
  StatrecReset(MemQueueSize);
#endif

  StatrecReset(ActiveListStats);
  StatrecReset(FetchQueueStats);

  // size of active list
  StatrecReset(readacc);
  StatrecReset(writeacc);
  StatrecReset(rmwacc);
  StatrecReset(readact);
  StatrecReset(writeact);
  StatrecReset(rmwact);
  StatrecReset(readiss);
  StatrecReset(writeiss);
  StatrecReset(rmwiss);
  StatrecReset(in_except);

#if 0
  for (i = 0; i < reqNUM_REQ_STAT_TYPE; i++)
    {
      StatrecReset(demand_read[i]);
      StatrecReset(demand_write[i]);
      StatrecReset(demand_rmw[i]);

      StatrecReset(demand_read_act[i]);
      StatrecReset(demand_write_act[i]);
      StatrecReset(demand_rmw_act[i]);

      StatrecReset(demand_read_iss[i]);
      StatrecReset(demand_write_iss[i]);
      StatrecReset(demand_rmw_iss[i]);

      StatrecReset(pref_sh[i]);
      StatrecReset(pref_excl[i]);
   }
#endif

  for (int lat_ctr = 0; lat_ctr < int (lNUM_LAT_TYPES); lat_ctr++)
    StatrecReset(lat_contrs[lat_ctr]);

  StatrecReset(partial_otime);
#endif

  avail_fetch_slots = 0;
  
  for (i = 0; i < int (lNUM_LAT_TYPES); i++)
    avail_active_full_losses[i] = 0;

  for (i = 0; i < int (eNUM_EFF_STALLS); i++)
    eff_losses[i] = 0;
}





