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

#ifndef __RSIM_TAGCVT_HH__
#define __RSIM_TAGCVT_HH__

/*************************************************************************/
/* AddtoTagConverter : initializes the tag-to-instance lookup table for  */
/*                   : a new instance                                    */
/*************************************************************************/
inline int AddtoTagConverter(long long taag, instance * instt,
			     ProcState * proc)
{
#ifdef DEBUG
  int res = proc->tag_cvt.Insert(NewTagtoInst(taag, instt, proc));

  if (res == 0)
    YS__logmsg(proc->proc_id / ARCH_cpus,
	    "FAILED TO ADD TAG %d to tag converter\n", taag);

  return res;
#else
  return proc->tag_cvt.Insert(NewTagtoInst(taag, instt, proc));
#endif
}


/*************************************************************************/
/* convert_tag_to_inst: finds the instance corresponding to a given tag  */
/*************************************************************************/
inline instance *convert_tag_to_inst(long long tag, ProcState * proc)
{
  TagtoInst *tmpptr;

  if (proc->tag_cvt.Search(tag, tmpptr))
    return (tmpptr->inst);
  else
    return (NULL);
}



/*************************************************************************/
/* UpdateTaghead: Increment the counter for the head element of the tag  */
/*                converter                                              */
/*************************************************************************/
inline int UpdateTaghead(long long tag, ProcState * proc)
{
  TagtoInst *junk = proc->tag_cvt.PeekHead();

#ifdef DEBUG
  if (junk == 0 || junk->tag != tag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "UTh: Something stuck in the tag converter, tag %d, "
	       "wanted tag %d\n", junk->tag, tag);
#endif

  return ++junk->count;
}


/*************************************************************************/
/* UpdateTagtail: Increment the counter at the end of the tag converter  */
/*************************************************************************/
inline int UpdateTagtail(long long tag, ProcState * proc)
{
  TagtoInst *junk = proc->tag_cvt.PeekTail();

#ifdef DEBUG
  if (junk == 0 || junk->tag != tag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "UTt: Something extra in the tag converter, tag %d, "
	       "wanted tag %d\n", junk->tag, tag);
#endif

  return ++junk->count;
}


/*************************************************************************/
/* TagCvtHead: Get instance at the head of the tag converter             */
/*************************************************************************/
inline instance *TagCvtHead(long long taag, ProcState * proc)
{
  TagtoInst *junk = proc->tag_cvt.PeekHead();

#ifdef DEBUG
  if (junk == 0 || junk->tag != taag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "TCH: Something stuck in the tag converter, tag %d, "
	       "wanted tag %d\n", junk->tag, taag);
#endif

  return junk->inst;
}


/*************************************************************************/
/* GetTagCvtByPosn: Get the instance at a specific index into the        */
/*                  tag converter                                        */
/*************************************************************************/
inline instance *GetTagCvtByPosn(long long taag, int index, ProcState * proc)
{
  TagtoInst *junk;

#ifdef DEBUG
  if (!proc->tag_cvt.PeekElt(junk, index) || junk->tag != taag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "GTCBP: Something incorrect in the tag converter, "
	       "tag %d, wanted tag %d\n", junk->tag, taag);
#else
  proc->tag_cvt.PeekElt(junk, index);
#endif

  return junk->inst;
}


/*************************************************************************/
/* GetHeadInst: Get instance at head of tag converter                    */
/*************************************************************************/
inline instance *GetHeadInst(ProcState * proc)
{
  TagtoInst *junk;

  if (junk = proc->tag_cvt.PeekHead())
    return junk->inst;

  return NULL;
}


/*************************************************************************/
/* GetTailInst: Get instance at tail of tag converter                    */
/*************************************************************************/
inline instance *GetTailInst(ProcState * proc)
{
  TagtoInst *junk;

  if (junk = proc->tag_cvt.PeekTail())
    return junk->inst;

  return NULL;
}



/*************************************************************************/
/* TagCvtTail: Get instance at tail of tag converter                     */
/*************************************************************************/
inline instance *TagCvtTail(long long taag, ProcState * proc)
{
  TagtoInst *junk = proc->tag_cvt.PeekTail();

#ifdef DEBUG
  if (!junk || junk->tag != taag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "TCT: Something extra in the tag converter, tag %d, "
	       "wanted tag %d\n", junk->tag, taag);
#endif

  return junk->inst;
}


/*************************************************************************/
/* GraduateTagConverter: Remove an element from head of tag converter    */
/*************************************************************************/
inline void GraduateTagConverter(long long taag, ProcState * proc)
{
  TagtoInst *junk;

#ifdef DEBUG
  if (!proc->tag_cvt.Delete(junk) || junk->tag != taag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "GTC: Something stuck in the tag converter, tag %d, "
              "wanted tag %d\n", junk->tag, taag);
#else
  proc->tag_cvt.Delete(junk);
#endif

  DeleteTagtoInst(junk, proc);
}


/*************************************************************************/
/* FlushTagConverter: Remove an element from end of tag converter        */
/*************************************************************************/
inline void FlushTagConverter(long long taag, ProcState * proc)
{
  TagtoInst *junk;

#ifdef DEBUG
  if (!proc->tag_cvt.DeleteFromTail(junk) || junk->tag != taag)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "FTC: Something extra in the tag converter, tag %d, "
	       "wanted tag %d\n", junk->tag, taag);
#else
  proc->tag_cvt.DeleteFromTail(junk);
#endif

  DeleteTagtoInst(junk, proc);
}

#endif
