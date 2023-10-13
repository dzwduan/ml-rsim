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

#ifndef __RSIM_INSTANCE_H__
#define __RSIM_INSTANCE_H__

extern "C"
{
#include "Caches/cache_stat.h"
}

struct ProcState;
struct instr;

/* 
 * SETRSD is used when the output register is an FPHALF, so writing the 
 * destination is effectively an RMW... 
 */

#define BUSY_SETRS1    1
#define BUSY_SETRS2    2
#define BUSY_SETRSCC   4
#define BUSY_SETRSD    8
#define BUSY_SETRS1P   16

#define BUSY_CLEARRS1  ((char)~1)
#define BUSY_CLEARRS2  ((char)~2)
#define BUSY_CLEARRSCC ((char)~4)
#define BUSY_CLEARRSD  ((char)~8)
#define BUSY_CLEARRS1P ((char)~16)

#define BUSY_ALLCLEAR  0
#define BUSY_ALLSET    (BUSY_SETRS1 | BUSY_SETRS2 | BUSY_SETRSCC | \
                        BUSY_SETRSD | BUSY_SETRS1P)

enum except {         /* exception modes supported                         */
  OK,                 /* no exception                                      */
  SERIALIZE,          /* instruction that requires serialization           */
  ITLB_MISS,          /* instruction TLB miss                              */
  UNUSED_01,          /* unused - room for inlining exception code         */
  UNUSED_02,
  UNUSED_03,
  DTLB_MISS,          /* data TLB miss                                     */
  UNUSED_11,          /* unused - room for inlining exception code         */
  UNUSED_12,
  UNUSED_13,
#ifdef UIO            /* UIO-TRAP                  (do not remove comment) */
  CSB_FLUSH,          /* UIO-TRAP CSB flush        (do not remove comment) */
#else                 /* UIO-TRAP                  (do not remove comment) */
  UNUSED,
#endif                /* UIO-TRAP                  (do not remove comment) */
  PRIVILEGED,         /* privileged instruction, requires supervisor mode  */
  INSTR_FAULT,        /* instruction fetch protection error                */
  DATA_FAULT,         /* data access protection error                      */
  BUSERR,             /* bus error, misaligned access                      */
  ILLEGAL,            /* illegal instruction                               */
  CLEANWINDOW,        /* clean register window                             */
  UNUSED_21,          /* unused - room for inlining exception code         */
  UNUSED_22,
  UNUSED_23,
  WINOVERFLOW,        /* register window overflow                          */
  UNUSED_31,          /* unused - room for inlining exception code         */
  UNUSED_32,
  UNUSED_33,
  WINUNDERFLOW,       /* register window underflow                         */
  UNUSED_41,          /* unused - room for inlining exception code         */
  UNUSED_42,
  UNUSED_43,
  SOFT_LIMBO,         /* memory disambiguation violation exception         */
  SOFT_SL_COHE,       /* speculative loads -- coherence violation          */
  SOFT_SL_REPL,       /* speculative loads -- replacement exception        */
  FPDISABLED,         /* Floating point unit is disabled                   */
  FPERR,              /* Floating point exception                          */
  DIV0,               /* Division by zero                                  */
  SYSTRAP_00,         /* used for emulating operating system traps         */
  SYSTRAP_01,         /* system call trap                                  */
  SYSTRAP_02,         /* system call trap                                  */
  SYSTRAP_03,         /* system call trap                                  */
  SYSTRAP_04,         /* system call trap                                  */
  SYSTRAP_05,         /* system call trap                                  */
  SYSTRAP_06,         /* system call trap                                  */
  SYSTRAP_07,         /* system call trap                                  */
  SYSTRAP_08,         /* system call trap                                  */
  SYSTRAP_09,         /* system call trap                                  */
  SYSTRAP_0A,         /* system call trap                                  */
  SYSTRAP_0B,         /* system call trap                                  */
  SYSTRAP_0C,         /* system call trap                                  */
  SYSTRAP_0D,         /* system call trap                                  */
  SYSTRAP_0E,         /* system call trap                                  */
  SYSTRAP_0F,         /* system call trap                                  */
  SYSTRAP_10,         /* system call trap                                  */
  SYSTRAP_11,         /* system call trap                                  */
  SYSTRAP_12,         /* system call trap                                  */
  SYSTRAP_13,         /* system call trap                                  */
  SYSTRAP_14,         /* system call trap                                  */
  SYSTRAP_15,         /* system call trap                                  */
  SYSTRAP_16,         /* system call trap                                  */
  SYSTRAP_17,         /* system call trap                                  */
  SYSTRAP_18,         /* system call trap                                  */
  SYSTRAP_19,         /* system call trap                                  */
  SYSTRAP_1A,         /* system call trap                                  */
  SYSTRAP_1B,         /* system call trap                                  */
  SYSTRAP_1C,         /* system call trap                                  */
  SYSTRAP_1D,         /* system call trap                                  */
  SYSTRAP_1E,         /* system call trap                                  */
  SYSTRAP_1F,         /* system call trap                                  */
  SYSTRAP_20,         /* system call trap                                  */
  SYSTRAP_21,         /* system call trap                                  */
  SYSTRAP_22,         /* system call trap                                  */
  SYSTRAP_23,         /* system call trap                                  */
  SYSTRAP_24,         /* system call trap                                  */
  SYSTRAP_25,         /* system call trap                                  */
  SYSTRAP_26,         /* system call trap                                  */
  SYSTRAP_27,         /* system call trap                                  */
  SYSTRAP_28,         /* system call trap                                  */
  SYSTRAP_29,         /* system call trap                                  */
  SYSTRAP_2A,         /* system call trap                                  */
  SYSTRAP_2B,         /* system call trap                                  */
  SYSTRAP_2C,         /* system call trap                                  */
  SYSTRAP_2D,         /* system call trap                                  */
  SYSTRAP_2E,         /* system call trap                                  */
  SYSTRAP_2F,         /* system call trap                                  */
  INTERRUPT_0F,       /* external interrupt                                */
  INTERRUPT_0E,
  INTERRUPT_0D,
  INTERRUPT_0C,
  INTERRUPT_0B,
  INTERRUPT_0A,
  INTERRUPT_09,
  INTERRUPT_08,
  INTERRUPT_07,
  INTERRUPT_06,
  INTERRUPT_05,
  INTERRUPT_04,
  INTERRUPT_03,
  INTERRUPT_02,
  INTERRUPT_01,
  INTERRUPT_00,
  MAX_EXCEPT
};


extern const char *enames[];



/*******************************************************************/
/***** class instance defintion:a dynamic instruction **************/
/*******************************************************************/

struct instance
{
  /************************* General Variables *****************************/
  long long tag;             /* Unique tag identifier                      */

  instr     code;            /* the static instruction referenced          */

  unsigned  pc;              /* program counter                            */
  unsigned  npc;             /* next pc value                              */
  unsigned  newpc;           /* default:0, not filled in until instr compl */

  char      win_num;         /* Window number                              */
  char      inuse;           /* indicates use of instance                  */
  UTYPE     unit_type;       /* Unit type associated with instruction      */
  char      stallqs;         /* counts # of mini-stallqs                   */

  except    exception_code;  /* 0 for no exception?                        */
  HIT_TYPE  miss;            /* mark type of cache miss                    */

  
  /**************************** Memory address *****************************/

  unsigned  addr;            /* the address of the memory instruction      */
  unsigned  finish_addr;     /* the "end address" of the memory instruction*/

  long long vsbfwd;	     /* forwards from virtual store buffer         */
  

  
  /**************************** Time Variables *****************************/

  double    time_active_list;  /* time from active list                    */
  double    time_addr_ready;   /* time from address ready                  */
  double    time_issued;       /* time from issue                          */
  long long issuetime;         /* cycle # when it was issued; should be
			          initialized to MAX_INT                   */
  long long addrissuetime;     /* cycle # when sent to address generation
			          unit; used for static scheduling only    */

  /**************************** Status Variables *****************************/

  
  long long memprogress;     /* lists current state of memory instruction  */
  long long completion;      /* time for completion                        */

  int       addr_attributes;   /* page attributes from TLB                 */

  char      addr_ready;        /* has address generation happened yet?     */
  char      busybits;          /* indicate "busy'ness" of rs1, rs2 and rd  */
  char      in_memunit;        /* flag a memory operation in memory unit   */
  char      mem_ready;         /* memory op marked ready to issue?         */

  char      global_perform;    /* has mem operation been globally performed*/
  char      limbo;	       /* flag "limbo" ambiguous memory ops        */
  char      kill;	       /* kill instruction flag                    */
  char      partial_overlap;   /* indicate presense of partial overlaps
				  between memory operations                */

  char      prefetched;        /* flag a prefetch instruction              */
  char      latepf;	       /* flag a late prefetch                     */
  char      mispredicted;      /* Result of prediction, set at completion  */
  char      taken;             /* the return value of StartCtlXfer         */

  unsigned  branch_pred;       /* pc predicted, filled in at decode        */

  
  /************************** dependence records ***************************/
  char      truedep; 	       /* true dependence                          */
  char      addrdep;	       /* address dependence                       */
  char      strucdep;	       /* structural dependence                    */
  char      branchdep;	       /* control dependence                       */

  
  /************************** Registers Numbers ****************************/

  /* Logical register numbers after taking into effect of window pointer. */
  short     lrs1;
  short     lrs2;
  short     lrscc;
  short     lrd;
  short     lrcc; 
    
  /* Physical register numbers */
  short     prs1;
  short     prs2;
  short     prscc;   
  short     prd;
  short     prcc;          

  /* this is used for instructions where dest. reg is an FPHALF.  Since FPs 
     are mapped/renamed by doubles, these are effectively RMWs */
  short     lrsd;
  short     prsd; 

  /* for INT_PAIR ops */
  short     prs1p;
  short     prdp;


#ifdef PAD_INSTANCE
  int __pad__;
#endif
  /************************** Registers Values *****************************/

  union
  {        /* destination */
    int    rdvali;
    double rdvalf;
    float  rdvalfh;
    struct
    {
       int a, b;
    } rdvalipair;
    long long rdvalll;
  };
    
  union
  {	/* register source 1 */
    int    rs1vali;
    double rs1valf;
    float  rs1valfh;
    struct
    {
      int a, b;
    } rs1valipair;
    long long rs1valll;
  };
    
  union
  {	/* register source 2 */
    int     rs2vali;
    double  rs2valf;
    float   rs2valfh;
    long long rs2valll;   /* rs2 can't have an int reg pair with this ISA */
  };

  int    rsccvali;        /* value of source condition code */
  int    rccvali;         /* Only integer values */
  double rsdvalf; 	  /* the value of prsd */

public:
  int decode_instruction(ProcState *proc);   // see pipestages.cc

  inline void copy(instance *i)
  {
    long long *src, *dest;
    int n;

    src = (long long*)this;
    dest = (long long*)i;
    for (n = 0; n < sizeof(instance) / sizeof(long long); n++)
      dest[n] = src[n];    
  }

  inline instance()
  { tag = 0; }

  inline instance(instr *instrn)
  {
    instrn->copy(&code); // code = *instrn;
  }

  inline instance(instance *inst)  /* copy contructor */
  {
    inst->copy(this);
  }

  inline ~instance() 
  { tag=-1; } /* reassign tag */
};

#endif



