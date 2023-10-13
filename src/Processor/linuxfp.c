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

#include <fpu_control.h>
#include "Processor/linuxfp.h"

#define SPARC_FP_INV  0x10
#define SPARC_FP_OF   0x08
#define SPARC_FP_UF   0x04
#define SPARC_FP_DZ   0x02
#define SPARC_FP_PREC 0x01


/*
 *
 * On the x86, a '1' in the mask means to disable the particular exception.
 * On the SPARC, a '0' in the mask means to disable the exception.
 *
 */


fp_rnd fpsetround(fp_rnd rnd_mode)
{
  fp_rnd        ret_val;
  fpu_control_t fp_control_word;

  ret_val = fpgetround();

  _FPU_GETCW(fp_control_word);
  fp_control_word &= (~FPU_ROUND_MASK);
  fp_control_word |= (((short) rnd_mode) << FPU_ROUND_SHIFT);
  
  _FPU_SETCW(fp_control_word);

  return ret_val;
}


fp_rnd fpgetround()
{
  fpu_control_t fp_control_word;
  
  _FPU_GETCW(fp_control_word);
  return((fp_rnd)((fp_control_word >> FPU_ROUND_SHIFT) & FPU_ROUND_MASK));
}


int fpsetmask(int mask)
{
  int           ret_val;
  int           deno;
  fpu_control_t fp_control_word;

  ret_val = fpgetmask();

  FPU_CLEAR_EXCEPTIONS;

  _FPU_GETCW(fp_control_word);
  deno = fp_control_word & _FPU_MASK_DM;
  fp_control_word &= (~FPU_INT_MASK);
  fp_control_word |= deno;

  if (!(mask & SPARC_FP_PREC))  fp_control_word |= _FPU_MASK_PM;
  if (!(mask & SPARC_FP_UF))    fp_control_word |= _FPU_MASK_UM;
  if (!(mask & SPARC_FP_OF))    fp_control_word |= _FPU_MASK_OM;
  if (!(mask & SPARC_FP_DZ))    fp_control_word |= _FPU_MASK_ZM;
  if (!(mask & SPARC_FP_INV))   fp_control_word |= _FPU_MASK_IM;

  _FPU_SETCW(fp_control_word);

  return ret_val;

}

int fpgetmask()
{
  int ret_val = 0;
  fpu_control_t fp_control_word;

  _FPU_GETCW(fp_control_word);

  if (!(fp_control_word & _FPU_MASK_PM))    ret_val |= SPARC_FP_PREC;
  if (!(fp_control_word & _FPU_MASK_UM))    ret_val |= SPARC_FP_UF;
  if (!(fp_control_word & _FPU_MASK_OM))    ret_val |= SPARC_FP_OF;
  if (!(fp_control_word & _FPU_MASK_ZM))    ret_val |= SPARC_FP_DZ;
  if (!(fp_control_word & _FPU_MASK_IM))    ret_val |= SPARC_FP_INV;

  return ret_val;
}
