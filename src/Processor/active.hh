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

#ifndef __RSIM_ACTIVE_HH__
#define __RSIM_ACTIVE_HH__


/*  Return the number of elements in active list */

inline int activelist::NumElements() const 
{
   return q->NumInQueue() >> 1;
}


/* return entries in active list */

inline int activelist::NumEntries() const 
{
   return q->NumInQueue();
}


/*  Return number of available instruction slots in window  */

inline int activelist::NumAvail() const 
{
   return (mx-q->NumInQueue()) >> 1;
}


/* is active list full */

inline int activelist::full() const			
{
   return (q->NumInQueue() == mx);
}


/* Add an old mapping into active list. */

inline int activelist::add_to_active_list(instance *inst,
					  int oldlog,
					  int oldphy, 
					  REGTYPE regtype,
					  ProcState * proc)
{
  if (q->NumInQueue() == mx)
      return (1);

  q->Insert(Newactivelistelement(inst, inst->tag, oldlog, oldphy, 
				 regtype, proc));

#ifdef COREFILE
  if (GetSimTime() > DEBUG_TIME)
    fprintf(corefile, "Add to active list : tag %lld :Total now %d\n", 
	    inst->tag, q->NumInQueue());
#endif

  return (0);
}


/* Called on instruction completion to inform graduation stage that 
 * this instruction has completed */

inline int activelist::mark_done_in_active_list(long long tagnum,
						int except,
						long long curr_cycle)
{
  activelistelement *ptr, *ptr2;

  if (q->Search2(tagnum, ptr, ptr2))
    {
      ptr->done       = 1;
      ptr->cycledone  = curr_cycle;
      ptr->exception  = except;
      ptr2->done      = 1;
      ptr2->cycledone = curr_cycle;
      ptr2->exception = except;
      return 0;
    }
  else
    {
#ifdef COREFILE
      if (GetSimTime() > DEBUG_TIME)
	fprintf(corefile, "MARK DONE FAILED FOR TAG %lld\n", tagnum);
#endif
      return 1;
    }
}


/* mark_done_in_active_list: an overloaded version of above */

inline int activelist::mark_done_in_active_list(activelistelement * ptr, 
						activelistelement * ptr2,
						int except,
						long long curr_cycle)
{
  ptr->done       = 1;
  ptr->cycledone  = curr_cycle;
  ptr->exception  = except;
  ptr2->done      = 1;
  ptr2->cycledone = curr_cycle;
  ptr2->exception = except;
  return 0;
}


/* After "completion", this instruction was discovered to have an 
   exception. Used in the case of speculative load execution. */

inline int activelist::flag_exception(long long tagnum, int except)
{
  activelistelement *ptr, *ptr2;

  if (q->Search2(tagnum, ptr, ptr2))
    {
      ptr->exception  = except;
      ptr2->exception = except;
      return 0;
    }
  else
    return 1;
}

#endif
