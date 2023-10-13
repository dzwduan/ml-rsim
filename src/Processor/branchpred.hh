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

#ifndef __RSIM_BRANCHPRED_HH__
#define __RSIM_BRANCHPRED_HH__

/*
 * Predict the direction of control transfer for branch.
 * this function is called when we first see that an instruction is a
 * transfer of control. The return value is whether or not the branch is
 * taken:
 *   0 = not taken;
 *   1 = taken;
 *   2 = taken without speculation (ie unconditional + know addr.);
 *   3 = don't know yet.
 */
inline void StartCtlXfer(instance * inst, ProcState * proc)
{
  int result = 0;

  if (inst->code.uncond_branch == 2 || inst->code.uncond_branch == 3)
    {
      // in this case, we know the address we want to go to.
      // There is no question of speculating
      inst->branch_pred = inst->newpc =
        proc->pc + (inst->code.imm * SIZE_OF_SPARC_INSTRUCTION);
      inst->taken = 2;

      if (inst->code.uncond_branch == 3)                     // It's a call
        proc->RASInsert(inst->pc + (2*SIZE_OF_SPARC_INSTRUCTION));

      return;
    }

  
  // Now we are in the case where we have to speculate. Either we don't know
  // the address we want to go to, or we don't know whether or not we want to
  // go there at all. In this case we should start speculating

  if (inst->code.uncond_branch == 4)
    { // it's a return
      // predict based on the return address stack
      result = 1;                      // taken speculatively
      inst->branch_pred = proc->RASPredict();
    }
  
  else if (inst->code.uncond_branch)
    { // it's a random jump
      // CURRENTLY, WE DON'T PREDICT THIS SORT OF ACCESS
      result = 3;
      inst->branch_pred = (unsigned) -1;

    }
  else
    { // a conditional branch
      if (inst->code.cond_branch && inst->code.aux1 == 0)
        // it's a "bn" -- "instruction prefetch" so intentionally mispredict
        result = 1;
      else
        result = proc->BPBPredict(inst->pc, inst->code.taken);

      if (result)
        inst->branch_pred = inst->pc +
          (inst->code.imm * SIZE_OF_SPARC_INSTRUCTION);
      else
        inst->branch_pred = inst->pc + (2 * SIZE_OF_SPARC_INSTRUCTION);
    }

  inst->taken = result;
}



/*************************************************************************/
/* Functions about branch predictor                                      */
/*************************************************************************/
/* Prediction uses the 2 bit history scheme                              */
/*                                                                       */
/*            nt                                                         */
/*         <--------                                                     */
/*      10           11                                                  */
/*      |  -------->  ^                                                  */
/*      |      t      |                                                  */
/*   nt |             | t                                                */
/*      |             |                                                  */
/*      |      nt     |                                                  */
/*      v  <--------  |                                                  */
/*      00           01                                                  */
/*         -------->                                                     */
/*             t                                                         */
/*                                                                       */
/* Above, the first bit is the prediction bit                            */
/*                                                                       */
/* In our system the first bit is represented by BranchPred, and the     */
/* second by PrevPred                                                    */
/*                                                                       */
/* We support either regular 2-bit prediction or 2-bit agree prediction  */
/* (for agree, replace T/NT with A/NA)                                   */
/*************************************************************************/

/*
 * BPBSetup : Intialize the branch predictor
 */
inline void ProcState::BPBSetup()
{
  if (BPB_SIZE > 0)
    {
      if (BPB_TYPE == TWOBIT || BPB_TYPE == TWOBITAGREE)
	{
	  BranchPred = new int[BPB_SIZE];
	  PrevPred   = new int[BPB_SIZE];

	  for (int i = 0; i < BPB_SIZE; i++)
	    {
	      BranchPred[i] = 1; // initialize it to metastable and taken/agree
	      PrevPred[i] = 0;
	    }
	}
    }
  else
    {
      if (BPB_TYPE != STATIC)
	{
	  YS__logmsg(proc_id / ARCH_cpus,
		  "Cannot simulate dynamic branch prediction with "
		  "buffer size %d. Reverting to static.\n", BPB_SIZE);
	  BPB_TYPE = STATIC;
	}
    }
}



/*
 * BPBPredict : Based on current PC (and, possibly static prediction),
 *              predict whether branch taken or not.
 *              returns prediction: 1 for taken, 0 for not taken 
 */
inline int ProcState::BPBPredict(unsigned bpc, int statpred)
{
  if (BPB_TYPE == TWOBIT)
    return BranchPred[(bpc / SIZE_OF_SPARC_INSTRUCTION) & (BPB_SIZE - 1)];
  else if (BPB_TYPE == TWOBITAGREE)
    // this is an XNOR of (dynamic) agree prediction & static prediction
    return (BranchPred[(bpc / SIZE_OF_SPARC_INSTRUCTION) & (BPB_SIZE - 1)] == statpred);      
  else   // right now, the only other type is static
    return statpred;
}



/*
 * BPBComplete: called when branch speculation resolved. Set actual
 *              history here
 */
inline void ProcState::BPBComplete(unsigned bpc, int taken, int statpred)
{
  int inbit;

  if (BPB_TYPE == TWOBIT)
    inbit = taken;
  else if (BPB_TYPE == TWOBITAGREE)
    inbit = (taken == statpred);               // agree/disagree
  else                                         // currently this means static
    return;                                    // static has no BPB

  int posn = (bpc / SIZE_OF_SPARC_INSTRUCTION) & (BPB_SIZE - 1);

  if (PrevPred[posn] == BranchPred[posn])      // either 00 or 11
    PrevPred[posn] = inbit;
  else                  // in this case, either 01 or 10, so set both = inbit
    BranchPred[posn] = PrevPred[posn] = inbit;
}



/*************************************************************************/
/* Return Address stack functions.                                       */
/* Operations are insert and destructive predict                         */
/*************************************************************************/

inline void ProcState::RASSetup()
{
  if (RAS_STKSZ > 0)
    {
      ReturnAddressStack = new unsigned[RAS_STKSZ];
      for (int i = 0; i < RAS_STKSZ; i++)
	ReturnAddressStack[i] = 0;  
    }

  rascnt = 0;
  rasptr = 0;
}



inline void ProcState::RASInsert (unsigned retpc)
{
  if (RAS_STKSZ > 0)
    {
      ReturnAddressStack[rasptr++] = retpc;
      if (rascnt == RAS_STKSZ)
	ras_overflows++;
      else
	rascnt++;
      rasptr = (rasptr % RAS_STKSZ);
    }
}



inline unsigned ProcState::RASPredict()
{
  if (RAS_STKSZ > 0)
    {
      if (rascnt == 0)
	ras_underflows++;
      else
	rascnt--;
      rasptr = unsigned ((rasptr + RAS_STKSZ - 1) % RAS_STKSZ);
      return ReturnAddressStack[rasptr];
    }
  else
    return 0;
}

#endif
