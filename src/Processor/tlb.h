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

#ifndef __RSIM_TLB_H__
#define __RSIM_TLB_H__

#ifdef __cplusplus
extern "C"
{
#endif
#include "Caches/req.h"
#include "../lamix/mm/mm.h"
#ifdef __cplusplus
}
#endif


enum tlb_type
{
  TLB_FULLY_ASSOC,
  TLB_SET_ASSOC,
  TLB_DIRECT_MAPPED,
  TLB_PERFECT
};


extern enum tlb_type DTLB_TYPE;
extern int           DTLB_SIZE;
extern int           DTLB_ASSOCIATIVITY;
extern int           DTLB_HARDWARE_FILL;
extern int           DTLB_TAGGED;

extern enum tlb_type ITLB_TYPE;
extern int           ITLB_SIZE;
extern int           ITLB_ASSOCIATIVITY;
extern int           ITLB_HARDWARE_FILL;
extern int           ITLB_TAGGED;

extern int           TLB_UNIFIED;


#define ITLB_CMD_PROBE  0x0001
#define ITLB_CMD_FLUSH  0x0002

#define DTLB_CMD_PROBE  0x0010
#define DTLB_CMD_FLUSH  0x0020




/* TLB Tag Portion : tag<0:20> unused<21:30> valid                           */
/* TLB Data Portion: data<0:20> unused<>                                     */
/*                   non_coherent uc_acc uncached rd-only mapping-valid      */

#define tlb_valid(a)    (unsigned int)(a & 0x00000001)
#define tlb_tag(a)      (unsigned int)(a & ~(PAGE_SIZE-1))

#define tlb_mvalid(b)   (unsigned int)(b & MAP_VALID)
#define tlb_rdonly(b)   (unsigned int)(b & MAP_RDONLY)
#define tlb_uncached(b) (unsigned int)(b & MAP_UNCACHED)
#define tlb_uc_csb(b)   (unsigned int)(b & MAP_UC_CSB)
#define tlb_non_coh(b)  (unsigned int)(b & MAP_NON_COHERENT)
#define tlb_priv(b)     (unsigned int)(b & MAP_PRIVILEGED)
#define tlb_attr(b)     (unsigned int)(b & (PAGE_SIZE-1))

#define tlb_data(b)     (unsigned int)(b & ~(PAGE_SIZE-1))
#define tlb_offset(c)   (unsigned int)(c & (PAGE_SIZE-1))


/*---------------------------------------------------------------------------*/

#ifdef __cplusplus


#include "Processor/procstate.h"
#include "Processor/instance.h"

#define TLB_HIT    0
#define TLB_MISS   1
#define TLB_FAULT  2


struct tlb_entry
{
  unsigned int tag;
  unsigned int data;
  unsigned int context;
  int          age;
};


class TLB
{
public:
  TLB  (ProcState*, enum tlb_type, int, int, int);
  TLB  (TLB*);
  ~TLB ();

  void            ProbeEntry   (unsigned int, int*);
  void            ReadEntry    (int, unsigned int*, unsigned int*,
				unsigned int*);
  void            WriteEntry   (int, unsigned int, unsigned int, unsigned int);
  int             MissEntry    (unsigned int);
  void            Flush        ();
  int             LookUp       (unsigned int*, int*, unsigned int, int, int,
				long long);
  void            Fill         (unsigned int, int, unsigned int, long long);
  void            Perform_Fill (REQ*);



private:
  int                 size;
  int                 associativity;
  enum tlb_type       type;
  int                 tagged;
  struct tlb_entry   *entries;
  int                 index;
  ProcState          *proc;
  unsigned int        fill_addr;
  unsigned int        fill_data;
  int                 fill_index;
  long long           fill_tag;
  unsigned int        fill_context;
};

#endif

#endif
