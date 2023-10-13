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
#include <sys/time.h>

extern "C"
{
#include "sim_main/simsys.h"
}


#include "Processor/procstate.h"
#include "Processor/mainsim.h"
#include "Processor/memunit.h"
#include "Processor/exec.h"
#include "Processor/simio.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/tagcvt.hh"
#include "Processor/stallq.hh"
#include "Processor/memunit.hh"
#include "Processor/procstate.hh"
#include "Processor/active.hh"


#ifdef sgi
#define LLONG_MAX LONGLONG_MAX
#endif

#if !defined(LLONG_MAX)
#  define LLONG_MAX 9223372036854775807LL
#endif

/*
 * mark_stores_ready : This function makes sure that stores are marked
 * ready to issue only when they are ready to graduate in the next
 * cycle -- namely, the store must be one of the first 4 (or whatever
 * the graduation rate is) instructions in the active list and each
 * instructions before it must have completed without excepting. In
 * cases with non-blocking stores (RC, PC, SC with the "-N" option),
 * stores can leave the active list after being marked ready.
 */

void activelist::mark_memops_ready(long long cycle, ProcState * proc)
{
  activelistelement *ptr, *ptr2;
  int numelts;
  int ind, actind;

  numelts = q->NumInQueue();
 
  // abort loop when it finds one of the following
  // 1) an instruction that isn't done in the active list
  //    (or, with non-blocking writes, a store that doesn't have it's
  //    address ready)
  // 2) an instruction that has its exception code set
  // 3) a mispredicted branch whose following hasn't yet been flushed
  //    (in our case, no such thing exists, so we don't do this check)
 
 
  for (ind = 0, actind = 0;
       (ind < proc->graduate_rate) && (actind < numelts);
       ind++, actind += 2)
    // 2 accounts for two active list "elements" for each entry
    {
#ifdef DEBUG
      if (!q->PeekElt(ptr, actind))
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "PeekElt gave invalid value in mark_memops_ready.\n");
#else
      q->PeekElt(ptr, actind);
#endif
      
      instance *tmpinst = GetTagCvtByPosn(ptr->tag, ind, proc);


      //---------------------------------------------------------------------
      // detect stores

      if (tmpinst->unit_type == uMEM &&
          IsStore(tmpinst) &&
          !tmpinst->mem_ready &&
          !tmpinst->stallqs &&
          tmpinst->strucdep == 0 &&
          tmpinst->addr_ready &&
	  proc->ReadyUnissuedStores < MAX_STORE_BUF)
        // no true dependence is possible; only struct dep can be a problem
        {
          tmpinst->mem_ready = 1;
          proc->ReadyUnissuedStores++;

#ifdef COREFILE
          if (YS__Simtime > DEBUG_TIME)
            fprintf(corefile,"Marking store %lld as ready to issue\n",
		    tmpinst->tag);
#endif

          if (!IsRMW(tmpinst)
#ifdef STORE_ORDERING
              && (Processor_Consistency || SC_NBWrites)
#endif
              )
            {
              if (tmpinst->exception_code == OK)
                {
                  tmpinst->in_memunit = 0;
		  instance *memop = NewInstance(tmpinst, proc);
                  memop->mem_ready = 1;
#ifndef STORE_ORDERING
		  proc->StoreQueue.Replace(tmpinst, memop);
#else
		  proc->MemQueue.Replace(tmpinst, memop);
#endif

		  q->PeekElt(ptr2, actind+1);
 
		  mark_done_in_active_list(ptr, ptr2,
					   tmpinst->exception_code, cycle);
                }
              else
                {
                  tmpinst->in_memunit = 0;
                  proc->ReadyUnissuedStores--;
#ifndef STORE_ORDERING
                  proc->StoreQueue.Remove(tmpinst);
                  proc->st_tags.Remove(tmpinst->tag);
#else
                  proc->MemQueue.Remove(tmpinst);
#endif
                }

 
              // a STORE in RC, PC, or SC w/non-blocking writes is
              // actually "done" when its address is ready, but we
              // should be careful in PC or SC w/non-blocking writes
              // not to let such a store issue unless it is at the top
              // of the memory unit, even if mem_ready is set.
            }
 
        }
 
 
 
      //---------------------------------------------------------------------
      // mark uncached loads

      if (tmpinst->unit_type == uMEM &&
          !IsStore(tmpinst) &&
          IsUncached(tmpinst) &&
          !tmpinst->mem_ready &&
          !tmpinst->stallqs &&
          tmpinst->strucdep == 0 &&
          tmpinst->addr_ready)
        // no true dependence is possible; only struct dep can be a problem
        {
          tmpinst->mem_ready = 1;
 
#ifdef COREFILE
          if (YS__Simtime > DEBUG_TIME)
            fprintf(corefile,"Marking uncached load %lld as ready to issue\n",
		    tmpinst->tag);
#endif
 
          if (tmpinst->exception_code == OK)
            {
              tmpinst->in_memunit = 0;
	      tmpinst->mem_ready = 1;
            }
          else
            {
              tmpinst->in_memunit = 0;
#ifndef STORE_ORDERING
              proc->LoadQueue.Remove(tmpinst);
              proc->st_tags.Remove(tmpinst->tag);
#else
              proc->MemQueue.Remove(tmpinst);
#endif
            }
 
 
          // a STORE in RC, PC, or SC w/non-blocking writes is
          // actually "done" when its address is ready, but we
          // should be careful in PC or SC w/non-blocking writes
          // not to let such a store issue unless it is at the top
          // of the memory unit, even if mem_ready is set.
        }

      if (ptr->done == 0 || ptr->exception != OK)
        break;

    }
}




/***************************************************************************/
/* remove_from_active_list : handles removal of instructions from active   */
/*                           list.                                         */
/*                           Also computes components of execution time.   */
/***************************************************************************/

/* 
 * Remove tagged elements from the active list if exception status is OK. 
 *   return 0 on success, 
 *   return tag causing problems, otherwise.
 */

instance *activelist::remove_from_active_list(long long cycle,
					      ProcState * proc)
{
  activelistelement *ptr;
  instance *tmpinst;
  int gradded = 0, busy = 0;

  
  ptr = q->PeekHead();

  if ((ptr == NULL) ||
      ((!ptr->done) && (!ptr->exception)) ||
      (ptr->cycledone + simulate_ilp > cycle))
    return(NULL);

  do
    {
      tmpinst = TagCvtHead(ptr->tag, proc);

#ifdef DEBUG
      if (tmpinst == NULL)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Something is wrong: no translation for this TAG %lld!\n",
		   ptr->tag);
#endif
      

      if (tmpinst->in_memunit && tmpinst->exception_code == OK)
         break;

      q->Delete(ptr);

      if (ptr->exception)
        {
          // Exception is set, we are in trouble
          UpdateTaghead(ptr->tag, proc);
          // THIS WILL BE CAUGHT THE FIRST TIME
          Deleteactivelistelement(ptr, proc);
 
          // cwp, etc. will get set through flushactivelist, etc.
          return tmpinst;
        }
 
              
      // No exception, we can free the active list entry
      // Also free the old physical register for later use
 
      if (ptr->regtype == REG_FP)
        proc->free_fp_list->addfreereg(ptr->phyreg);
      else
        if (ptr->phyreg != 0)
          proc->free_int_list->addfreereg(ptr->phyreg);
 
 
      UpdateTaghead(ptr->tag, proc);
      
      if (tmpinst->code.sync && proc->sync)
	{
	  if (tmpinst->code.instruction == iMEMBAR)
	    {
	      proc->membar_tags.RemoveTail();
	      ComputeMembarQueue(proc);
	    }

	  proc->sync = 0;
	}

          
      // We should also check to make sure that destinations
      // are indeed not busy (this is an easy mistake to make
      // when modifying code). At the same time, we'll now
      // update the logical register file (simulator abstraction)

      if (tmpinst->code.rd_regtype == REG_FP ||
	  tmpinst->code.rd_regtype == REG_FPHALF)
	{
	  int logreg;
 
	  if (tmpinst->code.rd_regtype == REG_FP)
	    logreg = tmpinst->lrd;
	  else if (tmpinst->code.rd_regtype == REG_FPHALF)
	    logreg = unsigned(tmpinst->lrd)&~1U;
 
	  // the logical register is really the double, not the half...
 
	  proc->log_fp_reg_file[logreg] =
	    proc->phy_fp_reg_file[tmpinst->prd];

#ifdef DEBUG
	  if (proc->fpregbusy[tmpinst->prd])
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "BUSY FP DESTINATION REGISTER AT GRAD!!\n");
#endif
	}
      else
	{
	  proc->log_int_reg_file[tmpinst->lrd] =
	    proc->phy_int_reg_file[tmpinst->prd];
 
	  if (tmpinst->code.rd_regtype == REG_INTPAIR)
	    proc->log_int_reg_file[tmpinst->lrd+1] =
	      proc->phy_int_reg_file[tmpinst->prdp];

#ifdef DEBUG
	  if (proc->intregbusy[tmpinst->prdp])
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "BUSY INT DESTINATION REGISTER AT GRAD!!\n");
#endif
	}
 
      proc->log_int_reg_file[tmpinst->lrcc] =
	proc->phy_int_reg_file[tmpinst->prcc];

#ifdef DEBUG
      if (proc->intregbusy[tmpinst->prcc])
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "BUSY CC DESTINATION REGISTER AT GRAD (Instr: %s; PC: 0x%08X)\n",
		   inames[tmpinst->code.instruction], tmpinst->pc);
#endif


      //---------------------------------------------------------------------


      Deleteactivelistelement(ptr, proc);
      ptr = q->PeekHead();
      tmpinst = TagCvtHead(ptr->tag, proc);

      q->Delete(ptr);

      if (ptr->exception)
	{
	  // Exception is set, we are in trouble
	  UpdateTaghead(ptr->tag, proc);
	  // THIS WILL BE CAUGHT THE FIRST TIME
	  Deleteactivelistelement(ptr, proc);
 
	  // cwp, etc. will get set through flushactivelist, etc.
	  return tmpinst;
	}
 
              
      // No exception, we can free the active list entry
      // Also free the old physical register for later use
 
      if (ptr->regtype == REG_FP)
	proc->free_fp_list->addfreereg(ptr->phyreg);
      else
	if (ptr->phyreg != 0)
	  proc->free_int_list->addfreereg(ptr->phyreg);

      UpdateTaghead(ptr->tag, proc);

#ifdef TRACE
      if (YS__Simtime > TRACE)
	{
	  unsigned char *dp;
	  if (PSTATE_GET_PRIV(proc->pstate))
	    YS__logmsg(proc->proc_id / ARCH_cpus, "    ");
	  
	  if (tmpinst->unit_type == uMEM)
	    {
	      if (IsStore(tmpinst))
		{
		  YS__logmsg(proc->proc_id / ARCH_cpus,
			     "[%i] %9.2f: %5x\t%s\t[%08x]=",
			     proc->proc_id,
			     YS__Simtime,
			     tmpinst->pc,
			     inames[tmpinst->code.instruction],
			     tmpinst->addr);
		  if (tmpinst->code.rs1_regtype == REG_INT)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%08X",
			       tmpinst->rs1vali);
		  if (tmpinst->code.rs1_regtype == REG_FSR)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%08X",
			       tmpinst->rs1vali);
		  else if (tmpinst->code.rs1_regtype == REG_INTPAIR)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%08X %08X",
			       tmpinst->rs1valipair.a,
			       tmpinst->rs1valipair.b);
		  else if (tmpinst->code.rs1_regtype == REG_INT64)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%016llX",
			       tmpinst->rs1valll);
		  else if (tmpinst->code.rs1_regtype == REG_FP)
		    {
		      dp = (unsigned char*)(&tmpinst->rdvalf);
		      YS__logmsg(proc->proc_id / ARCH_cpus,
				 "%f (%02X%02X%02X%02X%02X%02X%02X%02X)",
				 tmpinst->rdvalf, *dp++, *dp++, *dp++, *dp++,
				 *dp++, *dp++, *dp++, *dp++);
		    }
		  else if (tmpinst->code.rs1_regtype == REG_FPPAIR)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%f",
			       tmpinst->rs1valf);
		  else if (tmpinst->code.rs1_regtype == REG_FPHALF)
		    {
		      dp = (unsigned char*)(&tmpinst->rdvalfh);
		      YS__logmsg(proc->proc_id / ARCH_cpus,
				 "%f (%02X%02X%02X%02X)",
				 tmpinst->rdvalfh, *dp++, *dp++, *dp++, *dp++);
		    }
		  if ((tmpinst->code.rs1_regtype == REG_INT) ||
		      (tmpinst->code.rs1_regtype == REG_INTPAIR) ||
		      (tmpinst->code.rs1_regtype == REG_INT64))
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       " (R%02i)\t",
			       tmpinst->code.rs1);
		  else
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       " (F%02i)\t",
			       tmpinst->code.rs1);
		}
	      else     // uMEM & !store => load
		{
		  unsigned char *dp;
		  YS__logmsg(proc->proc_id / ARCH_cpus,
			     "[%i] %9.2f: %5x\t%s\t[%08x]=",
			     proc->proc_id,
			     YS__Simtime,
			     tmpinst->pc,
			     inames[tmpinst->code.instruction],
			     tmpinst->addr);
		  dp = (unsigned char*)(&tmpinst->rdvalf);
		  if (tmpinst->code.rd_regtype == REG_INT)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%08X",
			       tmpinst->rdvali);
		  else if (tmpinst->code.rd_regtype == REG_INTPAIR)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%08X %08X",
			       tmpinst->rdvalipair.a,
			       tmpinst->rdvalipair.b);
		  else if (tmpinst->code.rd_regtype == REG_INT64)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%016llX",
			       tmpinst->rdvalll);
		  else if (tmpinst->code.rd_regtype == REG_FP)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%f (%02X%02X%02X%02X%02X%02X%02X%02X)",
			       tmpinst->rdvalf,
			       *dp++, *dp++, *dp++, *dp++,
			       *dp++, *dp++, *dp++, *dp++);
		  else if (tmpinst->code.rd_regtype == REG_FPPAIR)
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       "%f",
			       tmpinst->rdvalf);
		  else if (tmpinst->code.rd_regtype == REG_FPHALF)
		    {
		      dp = (unsigned char*)(&tmpinst->rdvalfh);
		      YS__logmsg(proc->proc_id / ARCH_cpus,
				 "%f (%02X%02X%02X%02X)",
				 tmpinst->rdvalfh, *dp++, *dp++, *dp++, *dp++);
		    }
		  if ((tmpinst->code.rd_regtype == REG_INT) ||
		      (tmpinst->code.rd_regtype == REG_INTPAIR) ||
		      (tmpinst->code.rd_regtype == REG_INT64))
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       " (R%02i)\t",
			       tmpinst->code.rd);
		  else
		    YS__logmsg(proc->proc_id / ARCH_cpus,
			       " (F%02i)\t",
			       tmpinst->code.rd);
		}
	    }
	  else   // !uMem => other instruction
	    {
	      YS__logmsg(proc->proc_id / ARCH_cpus,
			 "[%i] %9.2f: %5x\t%s\tR%02i = ",
			 proc->proc_id,
			 YS__Simtime,
			 tmpinst->pc,
			 inames[tmpinst->code.instruction],
			 tmpinst->code.rd);
	      dp = (unsigned char*)(&tmpinst->rdvalfh);
	      if (tmpinst->code.rd_regtype == REG_INT)
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   "%08X",
			   tmpinst->rdvali);
	      else if (tmpinst->code.rd_regtype == REG_INTPAIR)
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   "%08X %08X",
			   tmpinst->rdvalipair.a,
			   tmpinst->rdvalipair.b);
	      else if (tmpinst->code.rd_regtype == REG_INT64)
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   "%016llX",
			   tmpinst->rdvalll);
	      else if (tmpinst->code.rd_regtype == REG_FP)
		{
		  dp = (unsigned char*)(&tmpinst->rdvalf);
		  YS__logmsg(proc->proc_id / ARCH_cpus,
			     "%f (%02X%02X%02X%02X%02X%02X%02X%02X)",
			     tmpinst->rdvalf, *dp++, *dp++, *dp++, *dp++,
			     *dp++, *dp++, *dp++, *dp++);
		}
	      else if (tmpinst->code.rd_regtype == REG_FPPAIR)
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   "%f",
			   tmpinst->rdvalf);
	      else if (tmpinst->code.rd_regtype == REG_FPHALF)
		{
		  dp = (unsigned char*)(&tmpinst->rdvalfh);
		  YS__logmsg(proc->proc_id / ARCH_cpus,
			     "%f (%02X%02X%02X%02X)",
			     tmpinst->rdvalfh, *dp++, *dp++, *dp++, *dp++);
		}
	      if ((tmpinst->code.rd_regtype == REG_INT) ||
		  (tmpinst->code.rd_regtype == REG_INTPAIR) ||
		  (tmpinst->code.rd_regtype == REG_INT64))
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   " (R%02i)\t",
			   tmpinst->code.rd);
	      else
		YS__logmsg(proc->proc_id / ARCH_cpus,
			   " (F%02i)\t",
			   tmpinst->code.rd);
	    }

	  YS__logmsg(proc->proc_id / ARCH_cpus,
		     "\t%c\n",
		     proc->interrupt_pending ? '*' : ' ');

	  if (tmpinst->code.wpchange)
	    YS__logmsg(proc->proc_id / ARCH_cpus,
		       "  New CWP: %i\n", tmpinst->win_num);

  }
#endif

      gradded++;
 
      if ((PSTATE_GET_DTE(proc->pstate)) &&
	  (tmpinst->unit_type == uMEM))
	proc->mem_refs++;
         
      GraduateTagConverter(ptr->tag, proc);

      int lastcount = proc->curr_cycle-proc->last_counted;
 
      if (lastcount > 0 || proc->graduate_rate == 0)
	// how much do we need to account for this tag
	{
#ifndef NOSTAT
	  // we account in this fashion if we have
	  // infinite grad rate, single grad rate, or if
	  // we have finite multiple grads but we didn't
	  // have a completely busy cycle last time
 
	  if (tmpinst->miss != L1DHIT)
	    {
	      // if it's a miss, count it in its miss
	      // latency in addition to its regular latency
 
	      switch (lattype[tmpinst->code.instruction])
		{
		case lRD:
		  StatrecUpdate(proc->lat_contrs[lRDmiss],
				lastcount,
				1);
		  break;
		case lWT:
		  StatrecUpdate(proc->lat_contrs[lWTmiss],
				lastcount,
				1);
		  break;
		case lRMW:
		  StatrecUpdate(proc->lat_contrs[lRMWmiss],
				lastcount,
				1);
		  break;
		default:
		  // don't know why we're here
		  break;
		}
	    }
 
	  if (tmpinst->partial_overlap)
	    StatrecUpdate(proc->partial_otime, lastcount, 1);
      
	  StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code.instruction]],
			lastcount, 1);
                    
	  switch(lattype[tmpinst->code.instruction])
	    {
	    case lRD:
	      StatrecUpdate(proc->lat_contrs[lRD_L1 + (int)tmpinst->miss],
			    lastcount,
			    1);
 
	      if (tmpinst->latepf)
		{
		  StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code.instruction] + lRMW_PFlate-lRMW],
				lastcount,
				1);
		}
	      break;

		  
	    case lWT:
	      StatrecUpdate(proc->lat_contrs[lWT_L1+(int)tmpinst->miss],
			    lastcount,
			    1);
	      if (tmpinst->latepf)
		{
		  StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code.instruction]+lRMW_PFlate-lRMW],
				lastcount,
				1);
		}
	      break;

		  
	    case lRMW:
	      StatrecUpdate(proc->lat_contrs[lRMW_L1+(int)tmpinst->miss],
			    lastcount,
			    1);
	      if (tmpinst->latepf)
		{
		  StatrecUpdate(proc->lat_contrs[lattype[tmpinst->code.instruction]+lRMW_PFlate-lRMW],
				lastcount,
				1);
		}
	      break;

		  
	    default:
	      break;
	    }
#endif
                    
	  proc->last_counted = proc->curr_cycle;
	}

      proc->last_graduated = proc->curr_cycle;
      busy++;
            
      proc->graduation_count++;
      proc->graduates++;

      if (tmpinst->unit_type == uMEM)
	{
	  proc->active_instr[uMEM]--;
	}

      
      if (tmpinst->partial_overlap)
	proc->partial_overlaps++;
            
      DeleteInstance(tmpinst, proc);

 
      Deleteactivelistelement(ptr, proc);
    }
  while ((gradded != proc->graduate_rate) &&
         (ptr = q->PeekHead()) && 
         (ptr->done) &&
         (ptr->cycledone + simulate_ilp <= cycle));


  // meaningless for infinite (0) graduation rates
  if (busy == proc->graduate_rate && proc->graduate_rate > 0)
    {
#ifndef NOSTAT
      StatrecUpdate(proc->lat_contrs[lBUSY], 1, 1);
#endif
      proc->last_counted = proc->curr_cycle + 1;
    }
 
  return NULL;                            // in other words, no exception
}



/*************************************************************************/
/* flush_active_list: empty elements from the list on a misprediction or */
/*                  : exception. Free registers in the process.          */
/*                  : returns -1 for empty active list , 0 on success    */
/*                  : -2 if element not found in list,                   */
/*************************************************************************/

int activelist::flush_active_list(long long tag, ProcState * proc)
{
  activelistelement *ptr;

#ifdef COREFILE
  if (proc->curr_cycle > DEBUG_TIME)
    fprintf(corefile, " FLUSH EVERYTHING AFTER %lld\n", tag);
#endif

  if (q->NumInQueue() == 0)
    return -1;

  while (q->NumInQueue())
    {
      ptr = q->PeekTail();

      if (ptr->tag <= tag)
	break;

      q->DeleteFromTail(ptr);

      /* *** THIS IS ALWAYS LAST IN TAG CONVERTER */
      instance *tmpinst = TagCvtTail(ptr->tag, proc);

      if (tmpinst == NULL)
	{
#ifdef COREFILE
	  if (proc->curr_cycle > DEBUG_TIME)
	    fprintf(corefile,
		    "Something is wrong, I dont have translation for tag %lld",
		    ptr->tag);
#endif
	  return (-1);
	}

      /* Remove entry from active list */
#ifdef COREFILE
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile,
		"Tag %lld is being flushed from active list \n", 
		ptr->tag);
#endif

      if (UpdateTagtail(ptr->tag, proc) == 2)
	{
	  FlushTagConverter(ptr->tag, proc);

	  /* We need to free the registers but only ONCE */
	  if (tmpinst->code.rd_regtype == REG_FP || 
	      tmpinst->code.rd_regtype == REG_FPHALF)
	    {
	      proc->fpregbusy[tmpinst->prd] = 0;
	      proc->free_fp_list->addfreereg(tmpinst->prd);
	      proc->dist_stallq_fp[tmpinst->prd].ClearAll(proc);
	    }
	  else
	    {	// INT, INT64, INTPAIR?
	      proc->intregbusy[tmpinst->prd] = 0;
	      if (tmpinst->prd != 0)
		proc->free_int_list->addfreereg(tmpinst->prd);
	      proc->dist_stallq_int[tmpinst->prd].ClearAll(proc);

	      proc->intregbusy[tmpinst->prdp] = 0;
	      if (tmpinst->prdp != 0)
		proc->free_int_list->addfreereg(tmpinst->prdp);
	      proc->dist_stallq_int[tmpinst->prdp].ClearAll(proc);
	    }

	  proc->intregbusy[tmpinst->prcc] = 0;
	  if (tmpinst->prcc != 0)
	    proc->free_int_list->addfreereg(tmpinst->prcc);
	  proc->dist_stallq_int[tmpinst->prcc].ClearAll(proc);

	  if (STALL_ON_FULL &&
	      ((tmpinst->unit_type != uMEM &&
		tmpinst->issuetime == LLONG_MAX) ||
	       (stat_sched && tmpinst->unit_type == uMEM && 
		tmpinst->addrissuetime == LLONG_MAX)))  // it wasn't issued
	    proc->active_instr[tmpinst->unit_type]--;
	  

	  if (STALL_ON_FULL && tmpinst->unit_type == uMEM && !stat_sched)
	    proc->active_instr[uMEM]--;
	  
	  if (tmpinst->code.sync)
	    proc->sync = 0;

          if (tmpinst->code.wpchange &&
              !(tmpinst->strucdep > 0 && tmpinst->strucdep < 5) &&
              tmpinst->exception_code != WINOVERFLOW &&
              tmpinst->exception_code != WINUNDERFLOW &&
	      tmpinst->exception_code != CLEANWINDOW)
            /* unupdate CWP that has been changed */
	    {
	      if (tmpinst->code.wpchange != WPC_FLUSHW)
		{
		  proc->cwp = unsigned (proc->cwp + NUM_WINS -
					tmpinst->code.wpchange) %
		    NUM_WINS;
		  proc->cansave += tmpinst->code.wpchange;
		  proc->canrestore -= tmpinst->code.wpchange;

#ifdef COREFILE
		  if (YS__Simtime > DEBUG_TIME)
		    fprintf(corefile,
			    "Flushing winchange instr. CANSAVE %d, CANRESTORE %d\n",
			    proc->cansave, proc->canrestore);
#endif
		}
	    }
	  DeleteInstance(tmpinst, proc);
	}
      Deleteactivelistelement(ptr, proc);
    }

  return 0;
}



int activelist::fp_ahead(long long tag, ProcState * proc)
{
  int                index, numelts;
  activelistelement *ptr;
  
  numelts = q->NumInQueue();
  
  for (index = 0; index < numelts; index += 2)
    {
      q->PeekElt(ptr, index);
      instance *tmpinst = GetTagCvtByPosn(ptr->tag, index / 2, proc);

      if (tmpinst->tag >= tag)
	return(0);

      if (tmpinst->unit_type == uFP)
	return(1);
    }

  return(0);
}




#ifdef COREFILE
void PrintGradInstr(instance *inst)
{
  if (GetSimTime() <= DEBUG_TIME) 
    return;

  fprintf(corefile, "Graduating pc %d tag %lld: %s", inst->pc,
	  inst->tag, inames[inst->code.instruction]);

  switch (inst->code.rd_regtype)
    {
    case REG_INT:
      if (inst->lrd != ZEROREG)
	fprintf(corefile, " i%d->%d", inst->lrd, inst->rdvali);
      break;
    case REG_FP:
      fprintf(corefile, " f%d->%f", inst->lrd, inst->rdvalf);
      break;
    case REG_FPHALF:
      fprintf(corefile, " fh%d->%f", inst->lrd, inst->rdvalfh);
      break;
    case REG_INTPAIR:
      if (inst->lrd != ZEROREG)
	fprintf(corefile, " i%d->%d i%d->%d", inst->lrd, inst->rdvalipair.a, 
		inst->lrd + 1, inst->rdvalipair.b);
      else
	fprintf(corefile, " i%d->%d", inst->lrd + 1, inst->rdvalipair.b);
      break;
    case REG_INT64:
      fprintf(corefile, " ll%d->%lld", inst->lrd, inst->rdvalll);
      break;
    default:
      fprintf(corefile, " rdX = XXX");
      break;
    }

  if (inst->lrcc != ZEROREG)
    fprintf(corefile, " i%d->%d", inst->lrcc, inst->rccvali);

  if (IsStore(inst) && !IsRMW(inst))
    {
      switch (inst->code.rs1_regtype)
	{
	case REG_INT:
	  fprintf(corefile, " i%d->[%d]", inst->rs1vali, inst->addr);
	  break;
	case REG_FP:
	  fprintf(corefile, " f%f->[%d]", inst->rs1valf, inst->addr);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, " fh%f->[%d]", inst->rs1valfh, inst->addr);
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, " i%d->[%d] i%d->[%d]", 
		  inst->rs1valipair.a, inst->addr, 
		  inst->rs1valipair.b, inst->addr + 4);
	  break;
	case REG_INT64:
	  fprintf(corefile, " ll%lld->[%d]", inst->rs1valll, inst->addr);
	  break;
	default:
	  break;
	}
    }
  fprintf(corefile, "\n");
}
#endif
