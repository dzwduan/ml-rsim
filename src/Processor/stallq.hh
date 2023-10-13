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

#ifndef __RSIM_STALLQ_HH__
#define __RSIM_STALLQ_HH__



inline int MiniStallQElt::ClearBusy(ProcState * proc)
{
  if (inst->tag != tag)
    return 0;

  inst->busybits &= CLEAR_MASK;
  if (inst->unit_type == uMEM &&
      !(inst->busybits & (BUSY_SETRS2 | BUSY_SETRSCC)) &&
      ((CLEAR_MASK == BUSY_CLEARRS2) || (CLEAR_MASK == BUSY_CLEARRSCC)))
    {

#ifdef DEBUG
      if (inst->code.rs2_regtype == REG_INT)
	inst->rs2vali = proc->phy_int_reg_file[inst->prs2];
      else
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "Mem with rs2 not an integer??\n");
#else
      inst->rs2vali = proc->phy_int_reg_file[inst->prs2];
#endif
      
      inst->rsccvali = proc->phy_int_reg_file[inst->prscc];

      inst->addrdep = 0;
      if ((inst->strucdep == 0) && (inst->in_memunit))
	CalculateAddress(inst, proc);
    }

  if (inst->busybits == BUSY_ALLCLEAR)
    {
      inst->truedep = 0;
      SendToFU(inst, proc);
    }

  return 1;
}



inline MiniStallQElt *MiniStallQElt::GetNext(instance * &i, ProcState * proc)
{
  if (inst->tag != tag)
    {
      i = NULL;
      MiniStallQElt *nx;
      while (next != NULL)
	{  	       // DO A FLUSH!!
	  nx = next;
	  next = next->next;
	  DeleteMiniStallQElt(nx, proc);
	}
      return NULL;
    }
  i = inst;
  return(next);
}



inline void MiniStallQ::AddElt(instance * inst, ProcState * proc, char b)
{
  MiniStallQElt *elt = NewMiniStallQElt(inst, b, proc);
  if (tail == NULL)
    head = tail = elt;
  else
    {
      tail->next = elt;
      tail = elt;
    }
}



inline void MiniStallQ::ClearAll(ProcState * proc)
{
  if (head)
    {
      MiniStallQElt *old;

      while (head != NULL)
	{
	  head->ClearBusy(proc);
	  old = head;
	  head = head->next;
	  DeleteMiniStallQElt(old, proc);
	}
      head = tail = NULL;
    }
}



inline instance *MiniStallQ::GetNext(ProcState * proc)
{
  if (head == NULL)
    return NULL;
  else
    {
      MiniStallQElt *stepper = head;
      MiniStallQElt *old;

      while (stepper != NULL && (stepper->inst->tag != stepper->tag))
	{
	  old = stepper;
	  stepper = stepper->next;
	  DeleteMiniStallQElt(old, proc);
	}
      if (stepper == NULL)
	{
	  head = tail = NULL;
	  return NULL;
	}
      
      instance *res;
      res = stepper->inst;
      old = stepper;
      head = stepper->next;
      if (head == NULL)
	tail = NULL;

      DeleteMiniStallQElt(old, proc);
      return res;
    }
}

#endif



