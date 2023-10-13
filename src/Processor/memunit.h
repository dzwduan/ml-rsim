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

#ifndef __RSIM_MEMUNIT_H__
#define __RSIM_MEMUNIT_H__

#include "Caches/cache_stat.h"


struct ProcState;
struct instance;

#ifdef __cplusplus

/* 
 * Functions
 */
struct MembarInfo;

extern int  IssueMem          (ProcState *);
extern int  memory_latency    (instance *, ProcState *);
extern void CompleteMemQueue  (ProcState *);
extern int  CompleteMemOp     (instance *, ProcState *);
extern void PerformMemOp      (instance *, ProcState *);
extern void FlushMems         (long long, ProcState *);
extern void Disambiguate      (instance *, ProcState *);
extern void ComputeMembarQueue(ProcState *);
extern void ComputeMembarInfo (ProcState *, const MembarInfo&);
extern int  IssuePrefetch     (ProcState *, unsigned, int,
			       int, long long inst_tag = 0);

/* 
 * some convenient inline functions 
 */
inline int  IsLoad                (instance *inst);
inline int  IsStore               (instance *inst);
inline int  IsRMW                 (instance *inst);
inline int  IsUncached            (instance *inst);
inline int  StartUpMemRef         (ProcState *proc, instance *inst);
inline void AddToMemorySystem     (instance * inst, ProcState * proc);
inline int  NumInMemorySystem     (ProcState *);
inline void CalculateAddress      (instance * inst, ProcState * proc);
inline unsigned GetAddr           (instance * memop, ProcState *proc);
inline void GenerateAddress       (instance * inst, ProcState * proc);
inline void StartPrefetch         (ProcState * proc);
inline void DoMemFunc             (instance * inst, ProcState * proc);
inline int  CanPrefetch           (ProcState * proc, instance * memop);
inline void AddFullToMemorySystem (ProcState * proc);

extern ReqType mem_acctype[];          /* memory access type */
extern int  mem_length[];              /* memory access length */
extern int  INSTANT_ADDRESS_GENERATE;  /* skip address generation stage */

extern "C"
{
#endif

  struct _req_;
  void      MemRefProcess         ();
  void      MemDoneHeapInsert     (struct _req_ *req, HIT_TYPE);
  void     *MemDoneHeapInsertSpec (struct _req_ *req, HIT_TYPE);
  void      ReturnMemopSpec       (struct _req_ *req);
  unsigned  GetProcContext        (int, int);

  void      PerformData           (struct _req_ *req);
  void      PerformIFetch         (struct _req_ *req);
  
  enum      SLBFailType { SLB_Repl, SLB_Cohe };
  int       SpecLoadBufCohe       (int, int, enum SLBFailType);
  void      FreeAMemUnit          (int proc_id, long long);
  void      AckWriteToWBUF        (struct instance *, int proc_id);

  int       IsFlushCond           (struct instance *inst);
  int       CheckFlushCond        (struct _req_ *req, int count);
  void      UpdateFlushCond       (struct _req_ *req, int val);
  void      CompleteFlushCond     (struct _req_ *req, int val);
  
  void      ReplaceAddr           (struct instance *inst, unsigned addr);
  void      ExternalInterrupt     (int, int);
  void      ProcessorHalt         (int, int);
  
#ifdef __cplusplus
}
#endif

#endif
