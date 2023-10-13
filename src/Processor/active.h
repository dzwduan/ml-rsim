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

#ifndef __RSIM_ACTIVE_H__
#define __RSIM_ACTIVE_H__

struct ProcState;
struct instance;

/************************************************************************/
/****************** activelistelement class definition ******************/
/************************************************************************/

class activelistelement
{
public:
  long long tag;	  // instruction tag
  int       logicalreg;   // stores the old mapping from the logical register
                          // to the physical register; this is because the
                          // logical register is now renamed
  int       phyreg;	  // physical register after renaming
  REGTYPE   regtype;      // register type
  int       done;         // is instruction done?
  long long cycledone;    // cycle when the instruction completed
  int       exception;    // exception status of intruction
  instance *inst;

  inline activelistelement(instance* i, long long t, int lr, int pr,
			   REGTYPE type, int dn = 0, int exc = 0)
  {
    inst       = i;
    tag        = t;
    logicalreg = lr;
    phyreg     = pr;
    regtype    = type;
    done       = dn;
    exception  = exc;
    cycledone  = -1;
  }

  inline ~activelistelement() {}
};


/************************************************************************/
/********************** activelist class definition ******************* */
/************************************************************************/

/* 
 * The active list is implemented as a circular queue of active list
 * entries. Note that there are two entries associated with each
 * instruction -- one for the dest. register and one for the dest.
 * conditon code register 
 */

class activelist
{
  circq<activelistelement *> *q;
  int mx;     /* max elements in active list */

public:
  /* Member functions */
  inline activelist(int maxelements)
  {
    mx = maxelements;
    q = new circq<activelistelement *>(ToPowerOf2(mx));
  }
  inline ~activelist() {}

  inline int NumElements() const;
  inline int NumEntries() const; 
  inline int NumAvail() const; 
  inline int full() const;
  inline int add_to_active_list(instance *, int, int, REGTYPE, ProcState *);
  inline int mark_done_in_active_list(long long tagnum, int exception,
				      long long cycle);
  inline int mark_done_in_active_list(activelistelement *, 
				      activelistelement *,
				      int exception,
				      long long cycle);
  inline int flag_exception(long long tagnum, int exception);

  void mark_memops_ready            (long long cycle, ProcState *proc);
  instance *remove_from_active_list (long long cycle, ProcState *proc);
  int flush_active_list             (long long tag, ProcState *proc);
  int fp_ahead                      (long long tag, ProcState * proc);
};

#endif
