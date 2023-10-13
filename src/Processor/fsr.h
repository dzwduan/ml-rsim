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

/*---------------------------------------------------------------------------*/
/* Floating Point Status Register and Exception Routines                     */
/* Exceptions are detected by setting the host machines FP trap mask equal   */
/* to the simulated machines mask. If the emulated operation causes an       */
/* exception, the signal handler marks the IEEE exception type in 'fpfailed' */
/* and the emulation routine marks the exception in the instance.            */
/* The detailed exception type is stored in rccvali (condition code result,  */
/* not used for FP operations) as IEEE trap type in the upper 16 bits and    */
/* general FP trap type in the lower 16 bits.                                */
/*---------------------------------------------------------------------------*/


#ifndef __RSIM_FSR_H__
#define __RSIM_FSR_H__


/* general FP trap types in FSR */

#define FP_TRAP_IEEE754        0x01
#define FP_TRAP_UNFINISHED     0x02
#define FP_TRAP_UNIMPLEMENTED  0x03
#define FP_TRAP_SEQUENCE_ERROR 0x04
#define FP_TRAP_HARDWARE_ERROR 0x05
#define FP_TRAP_INVALID_REG    0x06


/* IEEE 754 FP trap types */

#define FP754_TRAP_INVALID     0x10
#define FP754_TRAP_OVERFLOW    0x08
#define FP754_TRAP_UNDERFLOW   0x04
#define FP754_TRAP_DIV0        0x02
#define FP754_TRAP_INEXACT     0x01


/* set host machines FP trap mask equal to simulator mask */
#ifndef NO_IEEE_FP
#  define FPSETMASK() fpsetmask(proc->fp754_trap_mask)
#else
#  define FPSETMASK()
#endif

/* clear host machines FP trap mask */
#ifndef NO_IEEE_FP
#  define FPCLRMASK() fpsetmask(0)
#else
#  define FPCLRMASK()
#endif




/*---------------------------------------------------------------------------*/
/* mark an FP exception of 'type' in instance */
/*---------------------------------------------------------------------------*/

inline void set_fp_exception(ProcState *proc, instance *inst, unsigned type)
{
  inst->exception_code = FPERR;
  inst->rccvali        = type;
}



/*---------------------------------------------------------------------------*/
/* mark IEEE trap of type 'mask' in instance, if trap is not masked by CPU   */
/*---------------------------------------------------------------------------*/

inline void set_fp754_exception(ProcState *proc, instance *inst, unsigned mask)
{
  if (proc->fp754_trap_mask & mask)
    {
      inst->rccvali        = (mask << 16) | FP_TRAP_IEEE754;
      inst->exception_code = FPERR;
    }
}



/*---------------------------------------------------------------------------*/
/* take an FP exception: copy exception type and IEEE subtype to processors  */
/* floating-point status register                                            */
/*---------------------------------------------------------------------------*/

inline void take_fp_exception(ProcState *proc, instance *inst)
{
  unsigned mask = inst->rccvali >> 16;
  unsigned type = inst->rccvali & 0x0000FFFF;
  
  proc->fp754_cexc     = mask;
  proc->fp754_aexc    |= mask;
  proc->fp_trap_type  |= type;
}


/*---------------------------------------------------------------------------*/
/* global variables used to identify errors (overflow, underflow, etc) while */
/* emulating floating point instructions. Variable is cleared before the     */
/* emulation and contains the IEEE 754 trap mask set by the signal handler   */
/* in case of an exception.                                                  */
/*---------------------------------------------------------------------------*/

extern volatile int fpfailed;


void get_fsr(ProcState*, instance *);
void set_fsr(ProcState*, instance *);

#endif
