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

#ifndef __RSIM_FASTNEWS_HH__
#define __RSIM_FASTNEWS_HH__ 

inline void *operator new(unsigned, void *ptr)
{
  return(ptr);
}


/***************************************************************************/
/*  NewInstance: Returns a pointer to an instance from the instances pool  */
/***************************************************************************/

inline instance *NewInstance(instance *i, ProcState *proc)
{
  instance *in = proc->meminstances.Get();

#ifdef DEBUG
  if (in == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "NewInstance returns NULL!");
  else if (in->inuse)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Re-allocating an inuse instance element (0x%x)!!!", in);
#endif

  i->copy(in);
  in->inuse = 2;
  return in;
}


inline instance *NewInstance(instr *i, ProcState *proc)
{
  instance *in = proc->instances.Get();
  
#ifdef DEBUG
  if (in == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "NewInstance returns NULL!");
  else if (in->inuse)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Re-allocating an inuse instance element (0x%x)!!!", in);
#endif

  i->copy(&in->code);
  in->inuse = 1;   /* mark the instance as being in use */  
  return in;
}



/***************************************************************************/
/*  DeleteInstance: Returns an instance pointer back to the instances pool */
/***************************************************************************/

inline void DeleteInstance(instance *inst, ProcState *proc)
{
#ifdef DEBUG
  if (!inst->inuse)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "De-allocating an already free instance (0x%p)!!!\n",
	       inst);
#endif

  //  if (inst->inuse
  
  inst->inuse = 0;    /* mark the instance as not in use */
  inst->tag = -1;                  
  inst->instance::~instance();
  proc->instances.Putback(inst);

  return;
}



/***************************************************************************/
/*  NewBranchQElement: Returns a pointer to a new branch queue element     */
/***************************************************************************/

inline BranchQElement *NewBranchQElement(long long tg,
					 MapTable *map,
					 ProcState *proc)
{
  BranchQElement *in = proc->bqes.Get();

  return new (in) BranchQElement(tg, map);
}



/***************************************************************************/
/*  DeleteBranchQElement: Returns a branch queue element pointer back to   */
/*                        the branch queue element pool                    */
/*****************************************************************************/

inline void DeleteBranchQElement(BranchQElement *bqe, ProcState *proc)
{
  proc->bqes.Putback(bqe);
}



/***************************************************************************/
/* NewMapTable  : return a pointer to a new MapTable element               */
/***************************************************************************/

inline MapTable *NewMapTable(ProcState *proc)
{
  MapTable *in = proc->mappers.Get();

  return new (in) MapTable;
}



/***************************************************************************/
/* DeleteMapTable : return a MapTable pointer back to the processor's pool */
/*                : of MapTable elements                                   */
/***************************************************************************/

inline void DeleteMapTable(MapTable *map, ProcState *proc)
{
  proc->mappers.Putback(map);
}



/***************************************************************************/
/* NewMiniStallQElt : return a pointer to a new MiniStallQ element         */
/***************************************************************************/

inline MiniStallQElt *NewMiniStallQElt(instance *i, int b, ProcState *proc)
{
  MiniStallQElt *in = proc->ministallqs.Get();

  return new (in) MiniStallQElt(i, b);
}



/***************************************************************************/
/* DeleteMiniStallQElt : return a MiniStallQ element back to the           */
/*                       processor's pool of MiniStallQ elements           */
/***************************************************************************/

inline void DeleteMiniStallQElt(MiniStallQElt *msqe, ProcState *proc)
{
  proc->ministallqs.Putback(msqe);
}




/***************************************************************************/
/* Newactivelistelement : return a pointer to the an new active list       */
/*                        element                                          */
/***************************************************************************/

inline activelistelement *Newactivelistelement(instance *i,
					       long long t,
					       int lr,
					       int pr, 
					       REGTYPE type,
					       ProcState *proc)
{
  activelistelement *in = proc->actives.Get();

  return new (in) activelistelement(i, t, lr, pr, type);
}



/***************************************************************************/
/* Deleteactivelistelement : return an active list element back to the     */
/*                         : processor's active list elements pool         */
/***************************************************************************/

inline void Deleteactivelistelement(activelistelement *ale, ProcState *proc)
{
  proc->actives.Putback(ale);
}


/***************************************************************************/
/* NewTagtoInst : return a pointer to a new TagtoInst element              */
/***************************************************************************/

inline TagtoInst *NewTagtoInst(long long tg, instance *inst, ProcState *proc)
{
  TagtoInst *in = proc->tagcvts.Get();

  return new (in) TagtoInst(tg, inst);
}



/***************************************************************************/
/* DeleteTagtoInst : return a TagToInst pointer back to the processor's    */
/*                 : TagtoInst pool                                        */
/***************************************************************************/

inline void DeleteTagtoInst(TagtoInst *tcvt,ProcState *proc)
{
  proc->tagcvts.Putback(tcvt);
}

#endif






