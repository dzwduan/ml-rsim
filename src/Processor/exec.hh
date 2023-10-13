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

#ifndef __RSIM_EXEC_HH__
#define __RSIM_EXEC_HH__

/*
 * The maindecode function is called every cycle, and is used to call,
 * successively, the stages related to pipeline-processing.
 *
 * The decode_cycle function brings new instructions into the active list and
 * calls other functions to handle them appropriately. The function
 * instance::decode_instruction sets up important fields related to each new
 * instruction brought in.
 *
 * check_dependencies determines if a new instruction depends on the values
 * produced by previous instructions, and if so, puts the instruction in stall
 * queues associated with the needed registers.
 *
 * If there are no dependencies, the SendToFU and issue functions process the
 * instruction.
 *
 * At the end of each cycle update_cycle marks the registers associated with
 * newly computed results as ready and tries to start instructions that depend
 * on those registers.  
 */

inline int maindecode(ProcState * proc)
{
  int old_sync = proc->sync;

  /* First update data structures on the basis of what happened in the
     previous cycle  */

  update_cycle(proc);

  graduate_cycle(proc);

  proc->intregbusy[proc->intmapper[ZEROREG]] = 0;

  /* perform the decoding associated with this cycle */
  if (proc->exit)
    return 1;

  proc->ComputeAvail();

  if (!proc->in_exception && !old_sync)
    // we won't take new ops if so
    {
      decode_cycle(proc);
      fetch_cycle(proc);
    }

  /* Go on to the execution unit */
  return (0);
}

 


/**************************************************************************/
/* graduate_cycle : remove elements in-order from the active list, handle */
/*                : any exceptions that are flagged                       */
/**************************************************************************/
  
inline void graduate_cycle(ProcState * proc)
{
  instance *rettagval;


  // decrement random register, skip wired entries ----------------------------

  proc->itlb_random--;
  if (proc->itlb_random < proc->itlb_wired)
    proc->itlb_random = ITLB_SIZE - 1;

  proc->dtlb_random--;
  if (proc->dtlb_random < proc->dtlb_wired)
    proc->dtlb_random = DTLB_SIZE - 1;


  //---------------------------------------------------------------------------
  /* If we are at the head of the active list, and no excepns, graduate,
     release into free list too. Takes care of freeing and writing back 
     and exceptions. */

  
  rettagval = proc->active_list.remove_from_active_list(proc->curr_cycle,
							proc);

  if (rettagval != NULL)
    {
      // Exception at the tag returned !
      
      // Exception Handler returns -1 if non returnable exception,
      // otherwise returns 0,
         
      // it also flushes ALL relevant queues and resets all relevant
      // lists and restarts execution from the instruction which
      // caused the exception
      proc->start_cycle[rettagval->exception_code] = (long long)YS__Simtime;
      proc->start_graduated[rettagval->exception_code] = proc->graduates;

      if (!PSTATE_GET_PRIV(proc->pstate))
	{
	  proc->cycles[OK] += (long long)YS__Simtime - proc->start_cycle[OK];
	  proc->graduated[OK] += proc->graduates - proc->start_graduated[OK];
	  proc->start_graduated[OK] = 0;
	}
      
      proc->time_pre_exception = proc->curr_cycle;
      PreExceptionHandler(rettagval, proc);
    }


  if (simulate_ilp)
    proc->active_list.mark_memops_ready(proc->curr_cycle, proc);
}




/*
 * Free up one instance of a functional unit and wake up someone who 
 * is waiting for it.
 */
inline void LetOneUnitStalledGuyGo(ProcState * proc, UTYPE func_unit)
{
  proc->UnitsFree[func_unit]++;        /* free up the unit */

  if (func_unit != uMEM)
    {
      instance *inst;
      inst = proc->UnitQ[func_unit].GetNext(proc);

      if (inst != NULL)
	{
	  inst->stallqs--;
	  issue(inst, proc);
	}
    }
}



inline void CompleteFreeingUnit(ProcState *proc)
{
  UTYPE func_unit;

  while (proc->FreeingUnits.num() != 0 &&
	 proc->FreeingUnits.PeekMin() <= proc->curr_cycle)
    {
      proc->FreeingUnits.GetMin(func_unit);
      LetOneUnitStalledGuyGo(proc, func_unit);
    }
}

#endif
