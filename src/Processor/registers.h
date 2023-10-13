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

#ifndef __RSIM_REGISTERS_H__
#define __RSIM_REGISTERS_H__



#define PSTATE_GET_PRIV(a) (a &   0x00000001)    // privileged state
#define PSTATE_SET_PRIV(a) (a |=  0x00000001)
#define PSTATE_CLR_PRIV(a) (a &= ~0x00000001)

#define PSTATE_GET_ITE(a)  (a &   0x00000002)    // instr. address translation
#define PSTATE_SET_ITE(a)  (a |=  0x00000002)
#define PSTATE_CLR_ITE(a)  (a &= ~0x00000002)

#define PSTATE_GET_DTE(a)  (a &   0x00000004)    // data address translation
#define PSTATE_SET_DTE(a)  (a |=  0x00000004)
#define PSTATE_CLR_DTE(a)  (a &= ~0x00000004)

#define PSTATE_GET_AG(a)   (a &   0x00000008)    // alternate globals
#define PSTATE_SET_AG(a)   (a |=  0x00000008)
#define PSTATE_CLR_AG(a)   (a &= ~0x00000008)

#define PSTATE_GET_IE(a)   (a &   0x00000010)    // interrupt enable
#define PSTATE_SET_IE(a)   (a |=  0x00000010)
#define PSTATE_CLR_IE(a)   (a &= ~0x00000010)

#define PSTATE_GET_FPE(a)  (a &   0x00000020)    // FP unit enable
#define PSTATE_SET_FPE(a)  (a |=  0x00000020)
#define PSTATE_CLR_FPE(a)  (a &= ~0x00000020)

 


/*****************************************************************************/
/******************* SPARC architecture specific *****************************/
/*****************************************************************************/

/* Architected Register Layout                Implementation
    0-7         %g0 - %g7                      r0-r7 / r8-r15
    8-15        %o0 - %o7                      r68 + MAX_TL*4 + winnum*16 + reg
   16-23        %l0 - %l7                      
   24-31        %i0 - %i7
   32-39        cc regs                        r16-r23
   40-55        user-level state               r24-r39
   56-59        priv. regs. w/ stack           r68 + TL*4 + reg
   60-87        priv. registers                r40-r67
   88           end of registers               r68 + MAX_TL*4 (winstart)
   */


/*  
 * COND REGISTERS ARE LAID OUT AS FOLLOWS:
 *    32 -- fcc0
 *    33 -- fcc1
 *    34 -- fcc2
 *    35 -- fcc3
 *    36 -- icc   32-bit
 *    38 -- xcc   64-bit 
 *    37,39: reserved 
 *  (note: this gets renamed/mapped as the same reg as the icc, but has a 
 *   different set of values)
 */

#define ZEROREG         0                        /* hardwire register 0 to 0 */
#define REG_REGISTERS   0                                 /* starting point */
#define END_GLOBALS     8
#define END_WINDOWED    32
#define COND_REGISTERS  32
 
 
/* COND REGISTERS ARE LAID OUT AS FOLLOWS:
   32 -- fcc0
   33 -- fcc1
   34 -- fcc2
   35 -- fcc3
   36 -- icc
   38 -- xcc                (note: this gets renamed/mapped as the same reg as
                                   the icc, but has a different set of values)
   37,39: reserved */
 
 
#define COND_FCC(x) (COND_REGISTERS+(x))
#define COND_ICC    (COND_REGISTERS+4)
#define COND_XCC    (COND_REGISTERS+6)


#define STATE_REGISTERS  40                      /* starting point is Y reg */
#define STATE_Y      (STATE_REGISTERS+0)
#define STATE_CCR    (STATE_REGISTERS+2)  /* this can be statically converted
                                             to COND_ICC -- no need to do it
                                             dynamically */
#define STATE_ASI         (STATE_REGISTERS+3)
#define STATE_TICK        (STATE_REGISTERS+4)
#define STATE_PC          (STATE_REGISTERS+5)
#define STATE_FPRS        (STATE_REGISTERS+6)
#define STATE_MEMBAR      (STATE_REGISTERS+15)
#define STATE_SIR         (STATE_REGISTERS+15) 

#if defined(sparc)
#include <sys/fsr.h>
#else
#define FPRS_DL            0x1     /* dirty lower */
#define FPRS_DU            0x2     /* dirty upper */
#define FPRS_FEF           0x4     /* enable fp */
#endif

#define PRIV_REGISTERS    (STATE_REGISTERS+16)
#define PRIV_TPC          (PRIV_REGISTERS+0)
#define PRIV_TNPC         (PRIV_REGISTERS+1)
#define PRIV_TSTATE       (PRIV_REGISTERS+2)
#define PRIV_TT           (PRIV_REGISTERS+3)
#define PRIV_TICK         (PRIV_REGISTERS+4)
#define PRIV_TBA          (PRIV_REGISTERS+5)
#define PRIV_PSTATE       (PRIV_REGISTERS+6)
#define PRIV_TL           (PRIV_REGISTERS+7)
#define PRIV_PIL          (PRIV_REGISTERS+8)
#define PRIV_CWP          (PRIV_REGISTERS+9)
#define PRIV_CANSAVE      (PRIV_REGISTERS+10)
#define PRIV_CANRESTORE   (PRIV_REGISTERS+11)
#define PRIV_CLEANWIN     (PRIV_REGISTERS+12)
#define PRIV_OTHERWIN     (PRIV_REGISTERS+13)
#define PRIV_WSTATE       (PRIV_REGISTERS+14)
#define PRIV_FQ           (PRIV_REGISTERS+15)

#define PRIV_TLB_CONTEXT  (PRIV_REGISTERS+16)
#define PRIV_TLB_INDEX    (PRIV_REGISTERS+17)
#define PRIV_TLB_BADADDR  (PRIV_REGISTERS+18)
#define PRIV_TLB_TAG      (PRIV_REGISTERS+19)
#define PRIV_ITLB_RANDOM  (PRIV_REGISTERS+20)
#define PRIV_ITLB_WIRED   (PRIV_REGISTERS+21)
#define PRIV_ITLB_DATA    (PRIV_REGISTERS+22)
#define PRIV_DTLB_RANDOM  (PRIV_REGISTERS+23)
#define PRIV_DTLB_WIRED   (PRIV_REGISTERS+24)
#define PRIV_DTLB_DATA    (PRIV_REGISTERS+25)
#define PRIV_TLB_COMMAND  (PRIV_REGISTERS+26)

#define PRIV_VER          (PRIV_REGISTERS+31)

#define END_OF_REGISTERS  (PRIV_REGISTERS+32)

/* Offset of register windows in expanded (windows unrolled)
   logical reg file */
#define WINSTART (END_OF_REGISTERS - 16 + 4 * NUM_TRAPS)



/*****************************************************************************/
/************* REGTYPE enumnerated type definition ***************************/
/*****************************************************************************/


typedef char REGTYPE;

#define REG_INT     0
#define REG_FP      1
#define REG_INTPAIR 2
#define REG_FPHALF  3
#define REG_FPPAIR  4
#define REG_FSR     5
#define REG_INT64   6


#if 0
enum REGTYPE
{
  REG_INT,         /* integer register                                       */
  REG_FP,          /* floating point double-precision register               */
  REG_INTPAIR,     /* pair of adjacent integer registers                     */
  REG_FPHALF,      /* floating point single-prec. register (half a double)   */
  REG_FPPAIR,      /* pair of adjacent double-precision FP registers --      */
                   /* used in quadruple precision instructions.              */
                   /* For future expansion                                   */
  REG_FSR,         /* FP status reg., gets special treatment in issue stage  */
  REG_INT64        /* 64 bit integer type -- for future expansion            */
};
#endif
#endif

