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

#include <sys/time.h>
#include <math.h>

#ifndef NO_IEEE_FP
#  ifdef linux
#    include <ieee754.h>
#    include "Processor/linuxfp.h"
#  else
#    include <ieeefp.h>
#  endif
#endif

extern "C"
{
#include "sim_main/util.h"
}


#include "Processor/procstate.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/memunit.h"
#include "Processor/simio.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/fsr.h"
#include "Processor/tagcvt.hh"
#include "Processor/branchpred.hh"
#include "Processor/procstate.hh"
#include "Processor/active.hh"
#include "Processor/registers.h"
#include "Processor/endian_swap.h"


/* here are the functions that actually emulate the instructions */

#define icCARRY 1
#define icOVERFLOW 2
#define icZERO 4
#define icNEG 8
#define iccCARRY (v1 & icCARRY)
#define iccOVERFLOW (v1 & icOVERFLOW)
#define iccZERO (v1 & icZERO)
#define iccNEG (v1 & icNEG)

#define xcSHIFT 16
#define xccTOicc(x) ((x) >> xcSHIFT)

/* icc/xcc definitions get anded with the icc/xcc value for testing
   and ored for setting */

#define fccEQ 0
#define fccLT 1
#define fccGT 2
#define fccUO 3 /* unordered */


/* need additional indirection to get gcc to respect the volatile attribute */
volatile int *fpstatus = &fpfailed;



/* in all cases its frs1 [relop] frs2 */

inline int Xor(int a, int b)
{
  return (a||b) && (!(a&&b));
}

inline int SignExtend(int num, int width)
{
  /* So, we want to sign extend an int field of size num */
  int bit = num & (1 << (width-1));
  if (bit ==0)
    return num;
  unsigned left = ((unsigned)-1) ^ ((1<<width)-1);
  return (unsigned) num | left;
}

#define SE SignExtend





void fnRESERVED(instance *inst, ProcState *)
{
   inst->exception_code = ILLEGAL;
}


void fnILLTRAP(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL;
}



/************* Control flow instructions *************/

void fnCALL(instance *inst, ProcState *)
{
  /* a call is always properly predicted, so leave it alone */
  inst->rdvali = inst->pc;
}


void fnJMPL(instance *inst, ProcState *)
{
  int tmp;
  inst->rdvali = inst->pc;
  if (inst->code.aux1)
    tmp = inst->rs1vali + inst->code.imm;
  else
    tmp = inst->rs1vali + inst->rs2vali;
  inst->newpc = tmp;

  inst->mispredicted = (tmp != inst->branch_pred);
}


void fnRETURN(instance *inst, ProcState *)
{
  int tmp;
  if (inst->code.aux1)
    tmp = inst->rs1vali + inst->code.imm;
  else
    tmp = inst->rs1vali + inst->rs2vali;

  inst->newpc = tmp;

  /* proc->BPBComplete(inst->pc,inst->newpc); */

  inst->mispredicted = (inst->newpc != inst->branch_pred);
}


void fnTcc(instance *inst, ProcState *)
{
  except *out = &(inst->exception_code);
  int v1;
  except code;

  if (inst->code.aux1)
    inst->rdvali = inst->rs1vali + inst->code.imm;
  else
    inst->rdvali = inst->rs1vali + inst->rs2vali;

  code = (enum except)(SYSTRAP_00 + inst->rdvali);

  v1 = inst->rsccvali;
  if (inst->code.rscc == COND_XCC)
    v1 = xccTOicc(v1);
  switch (inst->code.aux2) /* these will be cases of bp_conds */
    {
    case Ab:
      *out = code;
      break;
    case Nb:
      break;
    case NEb:
      if (!iccZERO)
	*out = code;
      break;
    case Eb:
      if (iccZERO)
	*out = code;
      break;
    case Gb:
      if (!(iccZERO || Xor(iccNEG,iccOVERFLOW)))
	*out = code;
      break;
    case LEb:
      if (iccZERO || Xor(iccNEG,iccOVERFLOW))
	*out = code;
      break;
    case GEb:
      if (!Xor(iccNEG ,iccOVERFLOW))
	*out = code;
      break;
    case Lb:
      if (Xor(iccNEG,iccOVERFLOW))
	*out = code;
      break;
    case GUb:
      if (!(iccCARRY || iccZERO))
	*out = code;
      break;
    case LEUb:
      if (iccCARRY || iccZERO)
	*out = code;
      break;
    case CCb:
      if (!iccCARRY)
	*out = code;
      break;
    case CSb:
      if (iccCARRY)
	*out = code;
      break;
    case POSb:
      if (!iccNEG)
	*out = code;
      break;
    case NEGb:
      if (iccNEG)
	*out = code;
      break;
    case VCb:
      if (!iccOVERFLOW)
	*out = code;
      break;
    case VSb:
    default:
      if (iccOVERFLOW)
	*out = code;
      break;
    }
}


void fnDONERETRY(instance *inst, ProcState *proc)
{
  if (!PSTATE_GET_PRIV(proc->pstate))
    inst->exception_code = PRIVILEGED;
  else
    inst->exception_code = SERIALIZE;
}


void fnBPcc(instance *inst, ProcState *proc)
{
  int v1;
  int taken=0;

  v1 = inst->rs1vali;
  if (inst->code.rs1 == COND_XCC)
    v1 = xccTOicc(v1);

  switch (inst->code.aux1) /* these will be cases of bp_conds */
    {
    case Ab:
      taken=1;
      break;
    case Nb:
      break;
    case Eb:
      if (iccZERO)
	taken=1;
      break;
    case NEb:
      if (!iccZERO)
	taken=1;
      break;
    case Gb:
      if (!(iccZERO || Xor(iccNEG,iccOVERFLOW)))
	taken = 1;
      break;
    case LEb:
      if (iccZERO || Xor(iccNEG,iccOVERFLOW))
	taken = 1;
      break;
    case GEb:
      if (!Xor(iccNEG, iccOVERFLOW))
	taken = 1;
      break;
    case Lb:
      if (Xor(iccNEG,iccOVERFLOW))
	taken = 1;
      break;
    case GUb:
      if (!(iccCARRY || iccZERO))
	taken = 1;
      break;
    case LEUb:
      if (iccCARRY || iccZERO)
	taken = 1;
      break;
    case CCb:
      if (!iccCARRY)
	taken=1;
      break;
    case CSb:
      if (iccCARRY)
	taken=1;
      break;
    case POSb:
      if (!iccNEG)
	taken=1;
      break;
    case NEGb:
      if (iccNEG)
	taken=1;
      break;
    case VCb:
      if (!iccOVERFLOW)
	taken=1;
      break;
    case VSb:
    default:
      if (iccOVERFLOW)
	taken=1;
      break;
    }  

  if (taken)
    inst->newpc = inst->pc + (inst->code.imm * SIZE_OF_SPARC_INSTRUCTION);
  else
    inst->newpc = inst->pc + (2 * SIZE_OF_SPARC_INSTRUCTION);

  if (inst->code.aux1 != Nb) /* don't mark it for bn */
    proc->BPBComplete(inst->pc, taken, inst->code.taken);

  inst->mispredicted = (inst->taken != taken);
}

#define fnBicc fnBPcc

void fnBPr(instance *inst, ProcState *proc)
{
  int v1;
  int taken=0;

  v1 = inst->rs1vali;
  switch (inst->code.aux1) /* these will be cases of bp_conds */
    {
    case 1:
      if (v1 == 0)
	taken = 1;
      break;
    case 2:
      if (v1 <= 0)
	taken = 1;
      break;
    case 3:
      if (v1 < 0)
	taken = 1;
      break;
    case 5:
      if (v1 != 0)
	taken = 1;
      break;
    case 6:
      if (v1 > 0)
	taken = 1;
      break;
    case 7:
      if (v1 >= 0)
	taken = 1;
      break;
    case 0:
    case 4:
    default:
      break;
    }  
  
  if (taken)
    inst->newpc = inst->pc + (inst->code.imm * SIZE_OF_SPARC_INSTRUCTION);
  else
    inst->newpc = inst->pc + (2 * SIZE_OF_SPARC_INSTRUCTION);

  proc->BPBComplete(inst->pc, taken, inst->code.taken);
  
  inst->mispredicted = (inst->taken != taken);
}


void fnFBPfcc(instance *inst, ProcState *proc)
{
  double v1;
  int taken=0;

  v1 = inst->rs1vali;
  
#define E (v1 == fccEQ)
#define L (v1 == fccLT)
#define G (v1 == fccGT)
#define U (v1 == fccUO)
  
  switch (inst->code.aux1)
    {
    case Af:
      taken=1;
      break;
    case Uf:
      if (U)
	taken=1;
      break;
    case Gf:
      if (G)
	taken=1;
      break;
    case UGf:
      if (U || G)
	taken=1;
      break;
    case Lf:
      if (L)
	taken=1;
      break;
    case ULf:
      if (U || L)
	taken=1;
      break;
    case LGf:
      if (L || G)
	taken=1;
      break;
    case NEf:
      if (L || G || U)
	taken=1;
      break;
    case Nf:
      break;
    case Ef:
      if (E)
	taken=1;
      break;
    case UEf:
      if (E || U)
	taken=1;
      break;
    case GEf:
      if (G || E)
	taken=1;
      break;
    case UGEf:
      if (U || G || E)
	taken=1;
      break;
    case LEf:
      if (L || E)
	taken=1;
      break;
    case ULEf:
      if (U||L||E)
	taken=1;
      break;
    case Of:
      if (E||L||G)
	taken=1;
      break;
    }
  
  
  if (taken)
    inst->newpc = inst->pc + (inst->code.imm * SIZE_OF_SPARC_INSTRUCTION);
  else
    inst->newpc = inst->pc + (2 * SIZE_OF_SPARC_INSTRUCTION);

  if (inst->code.aux1 != Nf) /* don't mark it for bn */
    proc->BPBComplete(inst->pc, taken, inst->code.taken);
  
  inst->mispredicted = (inst->taken != taken);
}

#define fnFBfcc fnFBPfcc

/************* Arithmetic instructions *************/

void fnSETHI(instance *inst, ProcState *)
{
  unsigned tmp = inst->code.imm;
  inst->rdvali=tmp << 10;
}


void fnADD(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali + inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali + inst->rs2vali;
    }
}


void fnAND(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali & inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali & inst->rs2vali;
    }
}


void fnOR(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali | inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali | inst->rs2vali;
    }
}


void fnXOR(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali ^ inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali ^ inst->rs2vali;
    }
}


void fnSUB(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali - inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali - inst->rs2vali;
    }
}


void fnANDN(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali & (~inst->code.imm);
    }
  else
    {
      inst->rdvali=inst->rs1vali & (~inst->rs2vali);
    }
}


void fnORN(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali | (~ inst->code.imm);
    }
  else
    {
      inst->rdvali=inst->rs1vali | (~ inst->rs2vali);
    }
}


void fnXNOR(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=~(inst->rs1vali ^ inst->code.imm);
    }
  else
    {
      inst->rdvali=~(inst->rs1vali ^ inst->rs2vali);
    }
}


void fnADDC(instance *inst, ProcState *)
{
  int vcc = inst->rsccvali;
  if (inst->code.rscc == COND_XCC)
    vcc = xccTOicc(vcc);
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali + inst->code.imm + (vcc&icCARRY);
    }
  else
    {
      inst->rdvali=inst->rs1vali + inst->rs2vali + (vcc&icCARRY);
    }
}


void fnMULX(instance *inst, ProcState *)
{
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali * inst->code.imm;
    }
  else
    {
      inst->rdvali=inst->rs1vali * inst->rs2vali;
    }
}



void fnUMUL(instance *inst, ProcState *)
{
  UINT64 destval;
  
  if (inst->code.aux1)
    {
      destval=UINT64((unsigned)inst->rs1vali)*UINT64((unsigned)inst->code.imm);
      //destval=UINT64(inst->rs1vali)*UINT64(inst->code.imm);
    }
  else
    {
      destval=UINT64((unsigned)inst->rs1vali)*UINT64((unsigned)inst->rs2vali);
      //destval=UINT64(inst->rs1vali)*UINT64(inst->rs2vali);
    }
  inst->rdvali = destval; /* WRITE ALL 64 bits when supported... */
  inst->rccvali = (destval >> 32); /* Y reg gets 32 MSBs */
}


void fnSMUL(instance *inst, ProcState *)
{
  INT64 destval;
  
  if (inst->code.aux1)
    {
      destval=INT64(inst->rs1vali) * INT64(inst->code.imm);
    }
  else
    {
      destval=INT64(inst->rs1vali) * INT64(inst->rs2vali);
    }
	
  inst->rdvali = destval; /* WRITE ALL 64 bits when supported... */
  inst->rccvali = (UINT64(destval) >> 32); /* Y reg gets 32 MSBs */
}



void fnUDIVX(instance *inst, ProcState *)
{
  unsigned v1 = inst->rs1vali;
  unsigned v2;
  v2 = (inst->code.aux1) ? inst->code.imm : inst->rs2vali;

  if (v2 == 0) /* divide by 0 */
    inst->exception_code = DIV0;
  else
    inst->rdvali=v1/v2;
}


void fnUDIV(instance *inst, ProcState *)
{
  UINT64 v1 = (UINT64(UINT32(inst->rsccvali)) << 32) | UINT32(inst->rs1vali);
  /* Y ## lower 32 of rs1 */
  UINT64 quot;
  UINT32 v2;
  v2 = (inst->code.aux1) ? UINT32(inst->code.imm) : UINT32(inst->rs2vali);

  if (inst->code.instruction == iUDIVcc)
    {
      inst->rccvali = 0;
    }
  if (v2 == 0) /* divide by 0 */
    inst->exception_code = DIV0;
  else
    {
      quot = v1/UINT64(v2);
      if (quot > UINT64(UINT32_MAX))
	{
	  inst->rdvali = UINT32_MAX;
	  if (inst->code.instruction == iUDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else
	{
	  inst->rdvali=quot;
	}
      if (inst->code.instruction == iUDIVcc)
	{
	  if (INT32(inst->rdvali) == 0)
	    inst->rccvali |= iccZERO;
	  if (INT32(inst->rdvali) < 0) /* if bit 31 is 1 */
	    inst->rccvali |= iccNEG;
	}
    }
}



void fnSDIV(instance *inst, ProcState *proc)
{
  INT64 v1 = INT64((UINT64(UINT32(inst->rsccvali)) << 32) | UINT32(inst->rs1vali));
  /* Y ## lower 32 of rs1 */
  INT64 quot;
  INT32 v2;
  v2 = (inst->code.aux1) ? INT32(inst->code.imm) : INT32(inst->rs2vali);

  if (inst->code.instruction == iSDIVcc)
    {
      inst->rccvali = 0;
    }

  if (v2 == 0) /* divide by 0 */
    inst->exception_code = DIV0;
  else
    {
      quot = v1 / INT64(v2);

      if (quot < INT64(INT32_MIN))
	{
	  inst->rdvali = INT32_MIN;
	  if (inst->code.instruction == iSDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else if (quot > INT64(INT32_MAX))
	{
	  inst->rdvali = INT32_MAX;
	  if (inst->code.instruction == iSDIVcc)
	    inst->rccvali |= iccOVERFLOW;
	}
      else
	{
	  inst->rdvali = quot;
	}
      
      if (inst->code.instruction == iSDIVcc)
	{
	  if (INT32(inst->rdvali) == 0)
	    inst->rccvali |= iccZERO;
	  if (INT32(inst->rdvali) < 0) /* if bit 31 is 1 */
	    inst->rccvali |= iccNEG;
	}
    }
}



void fnSUBC(instance *inst, ProcState *)
{
  int vcc = inst->rsccvali;
  if (inst->code.rscc == COND_XCC)
    vcc = xccTOicc(vcc);
  if (inst->code.aux1)
    {
      inst->rdvali=inst->rs1vali - inst->code.imm - (vcc&icCARRY);
    }
  else
    {
      inst->rdvali=inst->rs1vali - inst->rs2vali - (vcc&icCARRY);
    }
}


void fnADDcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1+v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 

  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 >= 0 && tmp < 0) || (v1<0 && v2<0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  
  if ((v1 < 0 && v2 < 0) || (tmp >= 0 && (v1 < 0 || v2 < 0)))
    inst->rccvali |= icCARRY;
}


void fnADDCcc(instance *inst, ProcState *)
{
  int v1,v2,v3,tmp;
  v1=inst->rs1vali;
  
  int vcc=inst->rsccvali;
  if (inst->code.rscc == COND_XCC)
    vcc = xccTOicc(vcc);

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp = v1 + v2 + ( vcc & icCARRY);
  inst->rdvali = tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 

  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 >= 0 && tmp < 0) || (v1 < 0 && v2 < 0 && tmp >= 0))
    inst->rccvali |= icOVERFLOW;

  if ((v1 < 0 && v2 < 0) || (tmp >= 0 && (v1 < 0 || v2 < 0)))
    inst->rccvali |= icCARRY;
}


void fnANDcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 & v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnORcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 | v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnXORcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 ^ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnSUBcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 - v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 < 0 && tmp < 0) || (v1<0 && v2>=0 && tmp>=0))
    inst->rccvali |= icOVERFLOW;
  if ((unsigned)v1 < (unsigned) v2)
    inst->rccvali |= icCARRY;
}


void fnSUBCcc(instance *inst, ProcState *)
{
  int v1, v2, tmp, icbit;

  v1 = inst->rs1vali;

  int vcc=inst->rsccvali;
  if (inst->code.rscc == COND_XCC)
    vcc = xccTOicc(vcc);

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  icbit = vcc & icCARRY;
  tmp = v1 - v2 - icbit; 
  inst->rdvali = tmp;

   
  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG;
 
  if (tmp == 0)
    inst->rccvali |= icZERO;

  if ((v1 >= 0 && v2 < 0 && tmp < 0) || (v1 < 0 && v2 >= 0 && tmp >= 0))
    inst->rccvali |= icOVERFLOW;

  if (((unsigned) v1 < (unsigned) v2) || (v1 == v2 && icbit))
    inst->rccvali |= icCARRY;
}


void fnANDNcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 & ~ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnORNcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=v1 |~ v2;
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnXNORcc(instance *inst, ProcState *)
{
  int v1,v2,tmp;
  v1=inst->rs1vali;

  if (inst->code.aux1)
    {
      v2 = inst->code.imm;
    }
  else
    {
      v2 = inst->rs2vali;
    }

  tmp=~(v1 ^ v2);
  inst->rdvali=tmp;

  inst->rccvali = 0;
  if (tmp < 0)
    inst->rccvali |= icNEG; 
  if (tmp == 0)
    inst->rccvali |= icZERO;
}


void fnUMULcc(instance *inst, ProcState *)
{
  inst->exception_code = SERIALIZE;
}


void fnSMULcc(instance *inst, ProcState *)
{
  inst->exception_code = SERIALIZE;
}


void fnMULScc(instance *inst, ProcState *)
{
  inst->exception_code = SERIALIZE;
}


void fnUMULcc_serialized(instance *inst, ProcState *proc)
{
  fnUMUL(inst,proc);  /* this will set dest val in rdvali, Y val in rccvali */

  proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
    proc->log_int_reg_file[inst->lrd] = inst->rdvali;

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, STATE_Y)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, STATE_Y)] =
    inst->rccvali;

  int vcc = 0;

  if (INT32(inst->rdvali) < 0)
    vcc |= icNEG;

  if (INT32(inst->rdvali) == 0)
    vcc |= icZERO;

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, COND_ICC)] =
    vcc;
}


void fnSMULcc_serialized(instance *inst, ProcState *proc)
{
  fnSMUL(inst,proc); /* this will set dest val in rdvali, Y val in rccvali */

  proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
    proc->log_int_reg_file[inst->lrd] =
    inst->rdvali;

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, STATE_Y)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, STATE_Y)] =
    inst->rccvali;

  int vcc = 0;

  if (INT32(inst->rdvali) < 0)
    vcc |= icNEG;

  if (INT32(inst->rdvali) == 0)
    vcc |= icZERO;

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, COND_ICC)] =
    vcc;
}


void fnMULScc_serialized(instance *inst, ProcState *proc)
{
  /* this is a very messed up instruction. In this
     instruction, Y and ICC are both inputs and outputs */
  
  /* First, form a 32-bit value by shifting rs1 right by
     one bit and shifting in N^V as the high bit */
  INT32 vcc = proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
							COND_ICC)], vy =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, STATE_Y)];

  int neg = (vcc & icNEG), ovf = (vcc & icOVERFLOW);
  INT32 nxorv = (neg && !ovf) || (!neg && ovf);
  INT32 tmpval = (UINT32(inst->rs1vali) >> 1) | (nxorv<< 31);
  INT32 multiplicand, oldtmpval;

  if (inst->code.aux1)
    multiplicand = INT32(inst->code.imm);
  else
    multiplicand = INT32(inst->rs2vali);
  
  /* Now, iff LSB of Y is 1, multiplicand is added to tmpval;
     otherwise 0 is added; set ICC based on addition */
  
  vcc = 0;
  oldtmpval = tmpval;
  if (vy & 1)
    {
      tmpval += multiplicand;
      if ((oldtmpval < 0 && multiplicand < 0 && tmpval >= 0) ||
	  (oldtmpval >= 0 && multiplicand >= 0 && tmpval < 0))
	vcc |= icOVERFLOW;
    }
  
  if (tmpval < 0)
    vcc |= icNEG;

  if (tmpval == 0)
    vcc |= icZERO;
  
  /* put those sums/conditions in the dest and ICC */
  proc->phy_int_reg_file[proc->intmapper[inst->lrd]] =
    proc->log_int_reg_file[inst->lrd] = tmpval;

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, COND_ICC)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, COND_ICC)] =
    vcc;

  /* the Y reg is shifted right one bit, with the old LSB of rs1 moving
     into the MSB */
  vy = (UINT32(vy) >> 1) | (UINT32(inst->rs1vali & 1) << 31);

  proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp, STATE_Y)]] =
    proc->log_int_reg_file[arch_to_log(proc, proc->cwp, STATE_Y)] =
    vy;
}


#define fnUDIVcc fnUDIV
#define fnSDIVcc fnSDIV

void fnSLL(instance *inst, ProcState *)
{
  int v1=inst->rs1vali;
  int v2;

  if (inst->code.aux1 & 2)
    v2=inst->code.imm;
  else
    v2=inst->rs2vali;

  inst->rdvali=v1<<v2;
}


void fnSRL(instance *inst, ProcState *)
{
  unsigned v1=inst->rs1vali;
  unsigned v2;
  if (inst->code.aux1 & 2)
    v2=inst->code.imm;
  else
    v2=inst->rs2vali;

  inst->rdvali=v1 >> v2;
}


void fnSRA(instance *inst, ProcState *)
{
  int v1=inst->rs1vali;
  int v2;
  
  if (inst->code.aux1 & 2)
    v2 = inst->code.imm;
  else
    v2 = inst->rs2vali;

  v2 &= 0x1f;

  inst->rdvali = v1 >> v2;
  if (v1 < 0)
    inst->rdvali |= (-1) << (32 - v2);
}


void fnarithSPECIAL1(instance *inst, ProcState *)
{
  if (inst->code.rs1 == STATE_MEMBAR)         // stbar/membar
    {
      // nothing to do, but make sure not illegal...
      if (inst->code.rd != 0)                 // membar shouldn't have this...
	inst->exception_code = ILLEGAL;
    }
  else
    inst->exception_code = SERIALIZE;
}



void fnRDPR(instance *inst, ProcState *proc)
{
  if (!PSTATE_GET_PRIV(proc->pstate))
    inst->exception_code = PRIVILEGED;
  else
    inst->exception_code = SERIALIZE;
}



void fnMOVcc(instance *inst, ProcState *)
{
  int v1, v2;
  int& out = inst->rdvali;

  out = inst->rs1vali;

  if (inst->code.aux1)
    v2 = inst->code.imm;
  else
    v2 = inst->rs2vali;

  v1 = inst->rsccvali;
  if (inst->code.rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code.rscc == COND_XCC)
	v1 = xccTOicc(v1);
      switch (inst->code.aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || Xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!Xor(iccNEG ,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code.aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
}


void fnSDIVX(instance *inst, ProcState *)
{
  int v1 = inst->rs1vali;
  int v2;
  v2 = (inst->code.aux1) ? inst->code.imm : inst->rs2vali;

  if (v2 == 0) /* divide by 0 */
    inst->exception_code = DIV0;
  else
    inst->rdvali=v1/v2;
}


void fnPOPC(instance *inst, ProcState *)
{
  static int POPC_arr[16]={0,1,1,2,
		       1,2,2,3,
		       1,2,2,3,
		       2,3,3,4};
  unsigned v1;
  int &out=inst->rdvali;
  if (inst->code.aux1)
    v1=inst->code.imm;
  else
    v1=inst->rs1vali;

  out =0;

  while (v1 != 0)
    {
      out += POPC_arr[v1 & 15];
      v1 >>= 4;
    }
}


void fnMOVR(instance *inst, ProcState *)
{
  int &out=inst->rdvali;
  out=inst->rs1vali;
  int v1 = inst->rsccvali;
  int v2 = (inst->code.aux1) ? inst->code.imm : inst->rs2vali;

  switch (inst->code.aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
}


void fnarithSPECIAL2(instance *inst, ProcState *proc)
{
  /* right now, only implement basic ones -- save others for later... */

  if (inst->code.rd == STATE_Y ||
      inst->code.rd == STATE_CCR ||
      inst->code.rd == COND_ICC)
    {
      // these get renamed, so they can be rewritten whenever
      if (inst->code.aux1)
	inst->rdvali = inst->rs1vali ^ inst->code.imm;
      else
	inst->rdvali = inst->rs1vali ^ inst->rs2vali;
    }

  else if (inst->code.rd == STATE_ASI ||
	   inst->code.rd == STATE_FPRS)
    // These may be things of overall system-wide consequence, so handle
    // them appropriately by serializing before writing to these registers
    inst->exception_code = SERIALIZE;
  else
    YS__logmsg(proc->proc_id / ARCH_cpus,
	       "Other cases (%i) of ArithSpecial2 (0x%08X) not yet implemented\n",
	       inst->code.rd, inst->pc);
}



void fnSAVRESTD(instance *inst, ProcState *proc)
{
  if (!PSTATE_GET_PRIV(proc->pstate))
    inst->exception_code = PRIVILEGED;
  else
    inst->exception_code = SERIALIZE;
}


void fnWRPR(instance *inst, ProcState *proc)
{
  if (!PSTATE_GET_PRIV(proc->pstate))
    inst->exception_code = PRIVILEGED;
  else
    {
      if (inst->code.rd == PRIV_VER)
	inst->exception_code = ILLEGAL;
      else
	inst->exception_code = SERIALIZE;
    }
}


void fnIMPDEP1(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}


void fnIMPDEP2(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}

#define fnSAVE    fnADD
#define fnRESTORE fnADD
#define fnFLUSHW  fnADD    // this is a no-op, %g0 + %g0 -> %g0

/************* Floating-point instructions *************/

#define FP_ENABLED 	                                                     \
(PSTATE_GET_FPE(proc->pstate) &&                                             \
 (proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,                 \
					    STATE_FPRS)]] & FPRS_FEF))

  
// Treat FP values as ints for "non" operations, to prevent side effects
// on intel processors...  Shouldn't hurt on others.
void fnFMOVs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *) &(inst->rs2valfh);
  *((int *) &(inst->rdvalfh)) = *ip;
}


void fnFMOVd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *) &(inst->rs2valf);
  *((long long *) &(inst->rdvalf)) = *ip;
}


void fnFMOVq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  // For some reason we are treating quads as doubles?  (Also elsewhere
  // in the code.)  I'm guessing quads aren't really supported, but if
  // that is the case, it seems like we should panic w/ an error message,
  // instead of fake it this way?  Unless this is a legal architectural
  // thing to do???

  long long *ip = (long long *) &(inst->rs2valf);
  *((long long *) &(inst->rdvalf)) = *ip;
}


void fnFNEGs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalfh =- inst->rs2valfh;
}


void fnFNEGd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf =- inst->rs2valf;
}


void fnFNEGq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf =- inst->rs2valf;
}


void fnFABSs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (inst->rs2valfh >= 0)
    inst->rdvalfh =  inst->rs2valfh;
  else
    inst->rdvalfh =- inst->rs2valfh;
}


void fnFABSd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (inst->rs2valf >= 0)
    inst->rdvalf =  inst->rs2valf;
  else
    inst->rdvalf =- inst->rs2valf;
}


void fnFABSq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (inst->rs2valf >= 0)
    inst->rdvalf = inst->rs2valf;
  else
    inst->rdvalf =- inst->rs2valf;
}


void fnFSQRTs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalfh = sqrt(inst->rs2valfh);

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFSQRTd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = sqrt(inst->rs2valf);

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFSQRTq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = sqrt(inst->rs2valf);

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFADDs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalfh = inst->rs1valfh + inst->rs2valfh;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFADDd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf + inst->rs2valf;
  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFADDq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf + inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFSUBs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalfh = inst->rs1valfh - inst->rs2valfh;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFSUBd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf - inst->rs2valf;
  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFSUBq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf - inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFMULs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalfh = inst->rs1valfh * inst->rs2valfh;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFMULd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;
  
  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf * inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFMULq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf * inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFDIVs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalfh = inst->rs1valfh / inst->rs2valfh;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFDIVd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();
  inst->rdvalf = inst->rs1valf / inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFDIVq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf / inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFsMULd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = double(inst->rs1valfh) * double(inst->rs2valfh);

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}


void fnFdMULq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  *fpstatus = 0;
  FPSETMASK();

  inst->rdvalf = inst->rs1valf * inst->rs2valf;

  if (*fpstatus)
    set_fp754_exception(proc, inst, *fpstatus);
  FPCLRMASK();
}



// conversions from floating-point to 32-bit integer --------------------------

void fnFsTOi(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rdvalfh);
  *ip = (int)inst->rs2valfh;
}


void fnFdTOi(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rdvalfh);
  *ip = (int)inst->rs2valf;
}


void fnFqTOi(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rdvalfh);
  *ip = (int) inst->rs2valf;
}


// conversions from float to 64 bit integer -----------------------------------

void fnFsTOx(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rdvalf);
  *ip = (long long) inst->rs2valfh;
}


void fnFdTOx(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rdvalf);
  *ip = (long long) inst->rs2valf;
}


void fnFqTOx(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rdvalf);
  *ip = (long long) inst->rs2valf;
}


// conversions from integer to single-precision float -------------------------

void fnFiTOs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalfh = float(res);
}


void fnFxTOs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalfh = float(res);
}


// conversions from integer to double-precision float -------------------------

void fnFiTOd(instance *inst,ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalf = double(res);
}


void fnFxTOd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalf = double(res);
}


// conversion from integer to quad-precision float ----------------------------

void fnFiTOq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  int *ip = (int *)&(inst->rs2valfh);
  int res = *ip;
  inst->rdvalf = double(res);
}


void fnFxToq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  long long *ip = (long long *)&(inst->rs2valf);
  long long res = *ip;
  inst->rdvalf = double(res);
}


// conversion from floating point to single-precision float -------------------

void fnFdTOs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalfh = float(inst->rs2valf);
}


void fnFqTOs(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalfh = float(inst->rs2valf);
}


// conversion from floating-point to double-precision float -------------------

void fnFsTOd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf=double(inst->rs2valfh);
}


void fnFqTOd(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf=inst->rs2valf;
}


// conversion from floating point to quad-precision float ---------------------

void fnFsTOq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf=double(inst->rs2valfh);
}


void fnFdTOq(instance *inst, ProcState *proc)
{
  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  inst->rdvalf=inst->rs2valf;
}



#define fnFMOVq0 fnFMOVd0

void fnFMOVd0(instance *inst, ProcState *proc)
{
// Cleanup -- see orig out should be ref, not ptr
  int v1;
  long long *v2;
  long long *out;
  out = (long long *) &(inst->rs1valf);

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  v2 = (long long *) &(inst->rs2valf);

  v1 = inst->rsccvali;
  if (inst->code.rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code.rscc == COND_XCC) /* the xcc */
	v1 = xccTOicc(v1);
      switch (inst->code.aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || Xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code.aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
  *((long long *) &(inst->rdvalf)) = *out;
}


void fnFMOVs0(instance *inst, ProcState *proc)
{
// Cleanup -- see orig out should be ref, not ptr
  int v1;
  int *v2;
  int *out;
  out = (int *) &(inst->rs1valfh);

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  v2 = (int *) &(inst->rs2valfh);

  v1 = inst->rsccvali;
  if (inst->code.rscc >= COND_ICC) /* the integer condition code register */
    {
      if (inst->code.rscc == COND_XCC) /* the xcc */
	v1 = xccTOicc(v1);
      switch (inst->code.aux2) /* these will be cases of bp_conds */
	{
	case Ab:
	  out=v2;
	  break;
	case Nb:
	  break;
	case NEb:
	  if (!iccZERO)
	    out=v2;
	  break;
	case Eb:
	  if (iccZERO)
	    out=v2;
	  break;
	case Gb:
	  if (!(iccZERO || Xor(iccNEG,iccOVERFLOW)))
	    out = v2;
	  break;
	case LEb:
	  if (iccZERO || Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GEb:
	  if (!Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case Lb:
	  if (Xor(iccNEG,iccOVERFLOW))
	    out = v2;
	  break;
	case GUb:
	  if (!(iccCARRY || iccZERO))
	    out = v2;
	  break;
	case LEUb:
	  if (iccCARRY || iccZERO)
	    out = v2;
	  break;
	case CCb:
	  if (!iccCARRY)
	    out=v2;
	  break;
	case CSb:
	  if (iccCARRY)
	    out=v2;
	  break;
	case POSb:
	  if (!iccNEG)
	    out=v2;
	  break;
	case NEGb:
	  if (iccNEG)
	    out=v2;
	  break;
	case VCb:
	  if (!iccOVERFLOW)
	    out=v2;
	  break;
	case VSb:
	default:
	  if (iccOVERFLOW)
	    out=v2;
	  break;
	}
    }
  else /* use floating point condition codes */
    {
      #define E (v1 == fccEQ)
      #define L (v1 == fccLT)
      #define G (v1 == fccGT)
      #define U (v1 == fccUO)
      
      switch(inst->code.aux2)
	{
	case Af:
	  out=v2;
	  break;
	case Nf:
	  break;
	case Uf:
	  if (U)
	    out=v2;	
	  break;
	case Gf:
	  if (G)
	    out=v2;
	  break;
	case UGf:
	  if (U || G)
	    out=v2;
	  break;
	case Lf:
	  if (L)
	    out=v2;
	  break;
	case ULf:
	  if (U || L)
	    out=v2;
	  break;
	case LGf:
	  if (L || G)
	    out=v2;
	  break;
	case NEf:
	  if (L || G || U)
	    out=v2;
	  break;
	case Ef:
	  if (E)
	    out=v2;
	  break;
	case UEf:
	  if (E || U)
	    out=v2;
	  break;
	case GEf:
	  if (G || E)
	    out=v2;
	  break;
	case UGEf:
	  if (U || G || E)
	    out=v2;
	  break;
	case LEf:
	  if (L||E)
	    out=v2;
	  break;
	case ULEf:
	  if (U||L||E)
	    out=v2;
	  break;
	case Of:
	  if (E||L||G)
	    out=v2;
	  break;
	}
    }
  *((int *) &(inst->rdvalfh)) = *out;
}

#define fnFCMPq fnFCMPd
#define fnFCMPEq fnFCMPEd

void fnFCMPd(instance *inst, ProcState *proc)
{
  double v1 = inst->rs1valf, v2=inst->rs2valf;
  int& out = inst->rdvali;

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    out=fccUO;
}


void fnFCMPs(instance *inst, ProcState *proc)
{
  float v1 = inst->rs1valfh, v2=inst->rs2valfh;
  int& out = inst->rdvali;

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    out=fccUO;
}


void fnFCMPEd(instance *inst, ProcState *proc)
{
  double v1 = inst->rs1valf, v2=inst->rs2valf;
  int& out = inst->rdvali;

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    set_fp754_exception(proc, inst, FP754_TRAP_INVALID);
}


void fnFCMPEs(instance *inst, ProcState *proc)
{
  float v1 = inst->rs1valfh, v2=inst->rs2valfh;
  int& out = inst->rdvali;

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  if (v1==v2)
    out=fccEQ;
  else if (v1 < v2)
    out=fccLT;
  else if (v1 > v2)
    out=fccGT;
  else
    set_fp754_exception(proc, inst, FP754_TRAP_INVALID);
}


#define fnFMOVRq fnFMOVRd

void fnFMOVRd(instance *inst, ProcState *proc)
{
// Cleanup -- see orig out should be ref, not ptr
  long long *out;  

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  out=(long long *) &(inst->rs1valf);
  int v1 = inst->rsccvali;
  long long *v2 = (long long *) &(inst->rs2valf);

  switch (inst->code.aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
  *((long long *) &(inst->rdvalf)) = *out;
}


void fnFMOVRs(instance *inst, ProcState *proc)
{
// Cleanup -- see orig out should be ref, not ptr
  int *out;
  out= (int *) &(inst->rs1valfh);
  int v1 = inst->rsccvali;
  int *v2 = (int *) &(inst->rs2valfh);

  if (!FP_ENABLED)
    inst->exception_code = FPDISABLED;

  switch (inst->code.aux2)
    {
    case 1:
      if (v1 == 0)
	out = v2;
      break;
    case 2:
      if (v1 <= 0)
	out = v2;
      break;
    case 3:
      if (v1 < 0)
	out = v2;
      break;
    case 5:
      if (v1 != 0)
	out = v2;
      break;
    case 6:
      if (v1 > 0)
	out = v2;
      break;
    case 7:
      if (v1 >= 0)
	out = v2;
      break;
    case 0:
    case 4:
    default:
      break;
    }
  *((int *) &(inst->rdvalfh)) = *out;
}

/************* Memory instructions *************/


/****** Template for a variety of integer loads  *********/
template <class T> void ldi(instance *inst, ProcState *proc, T t)
{
  char *pa = GetMap(inst, proc);
  T *ptr = (T *)pa; 
  if (ptr)
    {
      T result = *ptr;

      result = endian_swap(result);
      if (t)
	result = SE(result, sizeof(T)*8);
      inst->rdvali = result;
    }
  else
    inst->exception_code = BUSERR;
}


void fnLDUW(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (unsigned)0);
}


void fnLDUB(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (unsigned char)0);
}


void fnLDUH(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (unsigned short)0);
}


void fnLDSW(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (int)0);
}


void fnLDSB(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (char)1);
}


void fnLDSH(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (short)1);
}


void fnLDD(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst, proc);
  unsigned *ptr = (unsigned *)pa;
  if (ptr)
    {
      inst->rdvalipair.a = endian_swap(*ptr);
      ptr++;
      inst->rdvalipair.b = endian_swap(*ptr);
    }
  else
    inst->exception_code = BUSERR;
}


void fnLDX(instance *inst,ProcState *proc)
{
  ldi(inst, proc, (unsigned)0);
}


/* Templates for long-long's do not seem to compile with the SC-4.0 for
   SPARC v8, so the long-long version of ldi is specified separately. */

void ldll(instance *inst, ProcState *proc, unsigned long long t)
{
  char *pa = GetMap(inst, proc);
  unsigned long long *ptr = (unsigned long long *)pa; 
  if (ptr)
    {
      unsigned long long result = *ptr;

      result = endian_swap(result);

      if (t)
	result = SE(result, sizeof(unsigned long long)*8);
      inst->rdvalll = result;
    }
  else
    inst->exception_code = BUSERR;
}


void fnLDF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  float *ptr = (float *)pa;
  if (ptr)
    {
      float result = *ptr;

      result = endian_swap(result);

      int *ip = (int *) &result;
      int *dp = (int*)&(inst->rdvalfh);
      *dp = *ip; /* single-precision */
    }
  else
    inst->exception_code = BUSERR;
}

void fnLDDF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  double *ptr = (double *)pa;
  if (ptr)
    {
      double result = *ptr;

      result = endian_swap(result);

      long long *ip = (long long *) &result;
      *((long long *) &(inst->rdvalf)) = *ip; /* double-precision */
    }
  else
    inst->exception_code = BUSERR;
}


void fnLDQF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  double *ptr = (double *)pa;
  if (ptr)
    {
      double result = *ptr;

      result = endian_swap(result);

      long long *ip = (long long *) &result;
      *((long long *) &(inst->rdvalf)) = *ip; /* double-precision */
    }
  else
    inst->exception_code = BUSERR;
}



/****** Template for a variety of integer stores  *******/
template <class T> void sti(instance *inst, ProcState *proc, T)
{
  char *pa = GetMap(inst, proc);
  
  T *ptr = (T *)pa;
  *ptr = endian_swap((T)(inst->rs1vali));
}


void fnSTW(instance *inst,ProcState *proc)
{
  sti(inst, proc, (int)0);
}


void fnSTB(instance *inst,ProcState *proc)
{
  sti(inst, proc, (char)0);
}


void fnSTH(instance *inst,ProcState *proc)
{
  sti(inst,proc,(short)0);
}


void fnSTX(instance *inst,ProcState *proc)
{
  sti(inst,proc,(unsigned)0);
}


void fnSTF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  float *ptr = (float *)pa;

  int *ip = (int*)&(inst->rs1valfh);
  *((int*)pa) = endian_swap(*ip);
}


void fnSTDF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  double *ptr = (double *)pa;

  long long *ip = (long long*)&(inst->rs1valf);
  *((long long*)pa) = endian_swap(*ip);
}


void fnSTQF(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  double *ptr = (double *)pa;

  long long *ip = (long long*)&(inst->rs1valf);
  *((long long*)pa) = endian_swap(*ip);
}


void fnSTD(instance *inst,ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  unsigned *ptr = (unsigned *)pa;

  *ptr = endian_swap(inst->rs1valipair.a);
  ptr++;
  *ptr = endian_swap(inst->rs1valipair.b);
}



#define fnSTDA fnSTD


void fnLDUWA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(unsigned)0);
}



void fnLDUBA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(unsigned char)0);
}



void fnLDUHA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(unsigned short)0);
}



void fnLDSWA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(int)0);
}



void fnLDSBA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(char)1);
}



void fnLDSHA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(short)1);
}



#define fnLDDA fnLDD



void fnLDXA(instance *inst,ProcState *proc)
{
  ldi(inst,proc,(unsigned)0);
}



#define fnLDFA  fnLDF
#define fnLDDFA fnLDDF
#define fnLDQFA fnLDQF


void fnSTWA(instance *inst,ProcState *proc)
{
  sti(inst,proc,(int)0);
}



/* functions to store destination processor for WriteSend */
void fnRWSD(instance *inst,ProcState *proc)
{
  /*  send_dest[proc->proc_id] = inst->rs1vali; */
}


void fnSTBA(instance *inst,ProcState *proc)
{
  sti(inst,proc,(char)0);
}


void fnSTHA(instance *inst,ProcState *proc)
{
  sti(inst,proc,(short)0);
}


void fnSTXA(instance *inst,ProcState *proc)
{
  sti(inst,proc,(unsigned)0);
}


#define fnSTFA  fnSTF
#define fnSTDFA fnSTDF
#define fnSTQFA fnSTQF


void fnLDSTUB(instance *inst, ProcState *proc)
{
  char *pa = GetMap(inst, proc);
  unsigned char *ptr = (unsigned char *)pa;
  if (ptr)
    {
      unsigned char result = *ptr;

      inst->rdvali = result;
      *ptr = 0xff;
    }
  else
    inst->exception_code = BUSERR;
}



void fnSWAP(instance *inst, ProcState *proc)
{
  char *pa=GetMap(inst,proc);
  int *ptr = (int *)pa;
  if (ptr)
    {
      int result = endian_swap(*ptr);

      inst->rdvali=result;
      *ptr = endian_swap(inst->rs1vali);
    }
  else
    inst->exception_code = BUSERR;
}


#define fnLDSTUBA fnLDSTUB

#define fnSWAPA fnSWAP



void fnLDFSR(instance *inst, ProcState *proc)
{
  ldi(inst, proc, (unsigned)0);
  inst->exception_code = SERIALIZE;
  /* load value from memory, but also serialize it. If it turns out to be
     a limbo, then let the limbo override the serialize */
}


void fnLDXFSR(instance *inst, ProcState *proc)
{
  char *pa = GetMap(inst, proc);
  unsigned *ptr = (unsigned *)pa;
  if (ptr)
    {
      inst->rdvalipair.a = endian_swap(*ptr);
      ptr++;
      inst->rdvalipair.b = endian_swap(*ptr);

      inst->exception_code = SERIALIZE;
      /* load value from memory, but also serialize it. If it turns out to be
	 a limbo, then let the limbo override the serialize */
    }
  else
    inst->exception_code = BUSERR;
}



void fnSTFSR(instance *inst, ProcState *proc)
{
  sti(inst, proc, (unsigned)0);
}



void fnSTXFSR(instance *inst, ProcState *proc)
{
  char *pa = GetMap(inst, proc);
  unsigned *ptr = (unsigned *)pa;

  *ptr = endian_swap(inst->rs1valipair.a);
  ptr++;
  *ptr = endian_swap(inst->rs1valipair.b);
}



void fnPREFETCH(instance *, ProcState *)
{
}



void fnCASA(instance *inst, ProcState *proc)
{
  char *pa = GetMap(inst,proc);
  int *ptr = (int *)pa;
  if (ptr)
    {
      int result = endian_swap(*ptr);

      inst->rdvali=result;
      if (result == inst->rs1vali)           // put the old dest value
	*ptr = endian_swap(inst->rsccvali);  // into the memory location
    }
  else
    inst->exception_code = BUSERR;
}


#define fnPREFETCHA fnPREFETCH

/*void fnPREFETCHA(instance *inst, ProcState *proc)
{
}*/


#define fnCASXA fnCASA


/*void fnCASXA(instance *inst, ProcState *proc)
{
}*/


void fnFLUSH(instance *, ProcState *)
{
}


void fnTADDcc(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}



void fnTSUBcc(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}



void fnTADDccTV(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}



void fnTSUBccTV(instance *inst, ProcState *)
{
  inst->exception_code = ILLEGAL; /* not currently supported */
}



void FuncTableSetup()
{
  instr_func[iRESERVED]      = fnRESERVED;
  instr_func[iILLTRAP]       = fnILLTRAP;

  instr_func[iCALL]          = fnCALL;
  instr_func[iJMPL]          = fnJMPL;
  instr_func[iRETURN]        = fnRETURN;
  instr_func[iTcc]           = fnTcc;
  instr_func[iDONERETRY]     = fnDONERETRY;

  instr_func[iBPcc]          = fnBPcc;
  instr_func[iBicc]          = fnBicc;
  instr_func[iBPr]           = fnBPr;
  instr_func[iFBPfcc]        = fnFBPfcc;
  instr_func[iFBfcc]         = fnFBfcc;
  
  instr_func[iSETHI]         = fnSETHI;
  instr_func[iADD]           = fnADD;
  instr_func[iAND]           = fnAND;
  instr_func[iOR]            = fnOR;
  instr_func[iXOR]           = fnXOR;
  instr_func[iSUB]           = fnSUB;
  instr_func[iANDN]          = fnANDN;
  instr_func[iORN]           = fnORN;
  instr_func[iXNOR]          = fnXNOR;
  instr_func[iADDC]          = fnADDC;
  
  instr_func[iMULX]          = fnMULX;
  instr_func[iUMUL]          = fnUMUL;
  instr_func[iSMUL]          = fnSMUL;
  instr_func[iUDIVX]         = fnUDIVX;
  instr_func[iUDIV]          = fnUDIV;
  instr_func[iSDIV]          = fnSDIV;
  
  instr_func[iSUBC]          = fnSUBC;
  instr_func[iADDcc]         = fnADDcc;
  instr_func[iANDcc]         = fnANDcc;
  instr_func[iORcc]          = fnORcc;
  instr_func[iXORcc]         = fnXORcc;
  
  instr_func[iSUBcc]         = fnSUBcc;
  instr_func[iANDNcc]        = fnANDNcc;
  instr_func[iORNcc]         = fnORNcc;
  instr_func[iXNORcc]        = fnXNORcc;
  instr_func[iADDCcc]        = fnADDCcc;
  
  instr_func[iUMULcc]        = fnUMULcc;
  instr_func[iSMULcc]        = fnSMULcc;
  instr_func[iUDIVcc]        = fnUDIVcc;
  instr_func[iSDIVcc]        = fnSDIVcc;
  
  instr_func[iSUBCcc]        = fnSUBCcc;
  instr_func[iTADDcc]        = fnTADDcc;
  instr_func[iTSUBcc]        = fnTSUBcc;
  instr_func[iTADDccTV]      = fnTADDccTV;
  instr_func[iTSUBccTV]      = fnTSUBccTV;
  instr_func[iMULScc]        = fnMULScc;

  instr_func[iSLL]           = fnSLL;
  instr_func[iSRL]           = fnSRL;
  instr_func[iSRA]           = fnSRA;
  instr_func[iarithSPECIAL1] = fnarithSPECIAL1;
  
  instr_func[iRDPR]          = fnRDPR;
  instr_func[iMOVcc]         = fnMOVcc;
  instr_func[iSDIVX]         = fnSDIVX;
  instr_func[iPOPC]          = fnPOPC;
  instr_func[iMOVR]          = fnMOVR;
  instr_func[iarithSPECIAL2] = fnarithSPECIAL2;
  
  instr_func[iSAVRESTD]     = fnSAVRESTD;
  instr_func[iWRPR]         = fnWRPR;
  instr_func[iIMPDEP1]      = fnIMPDEP1;
  instr_func[iIMPDEP2]      = fnIMPDEP2;
  instr_func[iSAVE]         = fnSAVE;
  instr_func[iRESTORE]      = fnRESTORE;
  
  instr_func[iFMOVs]        = fnFMOVs;
  instr_func[iFMOVd]        = fnFMOVd;
  instr_func[iFMOVq]        = fnFMOVq;

  instr_func[iFNEGs]        = fnFNEGs;
  instr_func[iFNEGd]        = fnFNEGd;
  instr_func[iFNEGq]        = fnFNEGq;
  instr_func[iFABSs]        = fnFABSs;
  instr_func[iFABSd]        = fnFABSd;
  instr_func[iFABSq]        = fnFABSq;
  instr_func[iFSQRTs]       = fnFSQRTs;
  
  instr_func[iFSQRTd]       = fnFSQRTd;
  instr_func[iFSQRTq]       = fnFSQRTq;
  instr_func[iFADDs]        = fnFADDs;
  instr_func[iFADDd]        = fnFADDd;
  instr_func[iFADDq]        = fnFADDq;
  instr_func[iFSUBs]        = fnFSUBs;
  instr_func[iFSUBd]        = fnFSUBd;
  instr_func[iFSUBq]        = fnFSUBq;
  
  instr_func[iFMULs]        = fnFMULs;
  instr_func[iFMULd]        = fnFMULd;
  instr_func[iFMULq]        = fnFMULq;
  instr_func[iFDIVs]        = fnFDIVs;
  instr_func[iFDIVd]        = fnFDIVd;
  instr_func[iFDIVq]        = fnFDIVq;
  instr_func[iFsMULd]       = fnFsMULd;
  instr_func[iFdMULq]       = fnFdMULq;
  
  instr_func[iFsTOx]        = fnFsTOx;
  instr_func[iFdTOx]        = fnFdTOx;
  instr_func[iFqTOx]        = fnFqTOx;
  instr_func[iFxTOs]        = fnFxTOs;
  instr_func[iFxTOd]        = fnFxTOd;
  instr_func[iFxToq]        = fnFxToq;
  instr_func[iFiTOs]        = fnFiTOs;
  instr_func[iFdTOs]        = fnFdTOs;
  
  instr_func[iFqTOs]        = fnFqTOs;
  instr_func[iFiTOd]        = fnFiTOd;
  instr_func[iFsTOd]        = fnFsTOd;
  instr_func[iFqTOd]        = fnFqTOd;
  instr_func[iFiTOq]        = fnFiTOq;
  instr_func[iFsTOq]        = fnFsTOq;
  instr_func[iFdTOq]        = fnFdTOq;
  instr_func[iFsTOi]        = fnFsTOi;
  
  instr_func[iFdTOi]        = fnFdTOi;
  instr_func[iFqTOi]        = fnFqTOi;
  instr_func[iFMOVs0]       = fnFMOVs0;
  instr_func[iFMOVd0]       = fnFMOVd0;
  instr_func[iFMOVq0]       = fnFMOVq0;
  instr_func[iFMOVs1]       = fnFMOVs0;
  instr_func[iFMOVd1]       = fnFMOVd0;
  instr_func[iFMOVq1]       = fnFMOVq0;
  
  instr_func[iFMOVs2]       = fnFMOVs0;
  instr_func[iFMOVd2]       = fnFMOVd0;
  instr_func[iFMOVq2]       = fnFMOVq0;
  instr_func[iFMOVs3]       = fnFMOVs0;
  instr_func[iFMOVd3]       = fnFMOVd0;
  instr_func[iFMOVq3]       = fnFMOVq0;
  instr_func[iFMOVsi]       = fnFMOVs0;
  
  instr_func[iFMOVdi]       = fnFMOVd0;
  instr_func[iFMOVqi]       = fnFMOVq0;
  instr_func[iFMOVsx]       = fnFMOVs0;
  instr_func[iFMOVdx]       = fnFMOVd0;
  instr_func[iFMOVqx]       = fnFMOVq0;
  instr_func[iFCMPs]        = fnFCMPs;
  instr_func[iFCMPd]        = fnFCMPd;
  instr_func[iFCMPq]        = fnFCMPq;
  
  instr_func[iFCMPEs]       = fnFCMPEs;
  instr_func[iFCMPEd]       = fnFCMPEd;
  instr_func[iFCMPEq]       = fnFCMPEq;
  instr_func[iFMOVRsZ]      = fnFMOVRs;
  instr_func[iFMOVRdZ]      = fnFMOVRd;
  instr_func[iFMOVRqZ]      = fnFMOVRq;
  instr_func[iFMOVRsLEZ]    = fnFMOVRs;
  
  instr_func[iFMOVRdLEZ]    = fnFMOVRd;
  instr_func[iFMOVRqLEZ]    = fnFMOVRq;
  instr_func[iFMOVRsLZ]     = fnFMOVRs;
  instr_func[iFMOVRdLZ]     = fnFMOVRd;
  instr_func[iFMOVRqLZ]     = fnFMOVRq;
  instr_func[iFMOVRsNZ]     = fnFMOVRs;
  
  instr_func[iFMOVRdNZ]     = fnFMOVRd;
  instr_func[iFMOVRqNZ]     = fnFMOVRq;
  instr_func[iFMOVRsGZ]     = fnFMOVRs;
  instr_func[iFMOVRdGZ]     = fnFMOVRd;
  instr_func[iFMOVRqGZ]     = fnFMOVRq;
  instr_func[iFMOVRsGEZ]    = fnFMOVRs;
  
  instr_func[iFMOVRdGEZ]    = fnFMOVRd;
  instr_func[iFMOVRqGEZ]    = fnFMOVRq;


  instr_func[iLDUW]         = fnLDUW;
  instr_func[iLDUB]         = fnLDUB;
  instr_func[iLDUH]         = fnLDUH;
  instr_func[iLDD]          = fnLDD;
  instr_func[iSTW]          = fnSTW;
  instr_func[iSTB]          = fnSTB;
  instr_func[iSTH]          = fnSTH;
  
  instr_func[iSTD]          = fnSTD;
  instr_func[iLDSW]         = fnLDSW;
  instr_func[iLDSB]         = fnLDSB;
  instr_func[iLDSH]         = fnLDSH;
  instr_func[iLDX]          = fnLDX;
  instr_func[iLDSTUB]       = fnLDSTUB;
  instr_func[iSTX]          = fnSTX;
  instr_func[iSWAP]         = fnSWAP;
  instr_func[iLDUWA]        = fnLDUWA;
  instr_func[iLDUBA]        = fnLDUBA;
  
  instr_func[iLDUHA]        = fnLDUHA;
  instr_func[iLDDA]         = fnLDDA;
  instr_func[iSTWA]         = fnSTWA;
  instr_func[iSTBA]         = fnSTBA;
  instr_func[iSTHA]         = fnSTHA;
  instr_func[iSTDA]         = fnSTDA;
  instr_func[iLDSWA]        = fnLDSWA;
  instr_func[iLDSBA]        = fnLDSBA;
  instr_func[iLDSHA]        = fnLDSHA;
   
  instr_func[iLDXA]         = fnLDXA;
  instr_func[iLDSTUBA]      = fnLDSTUBA;
  instr_func[iSTXA]         = fnSTXA;
  instr_func[iSWAPA]        = fnSWAPA;
  instr_func[iLDF]          = fnLDF;
  instr_func[iLDFSR]        = fnLDFSR;
  instr_func[iLDXFSR]       = fnLDXFSR;
  instr_func[iLDQF]         = fnLDQF;
  instr_func[iLDDF]         = fnLDDF;
  instr_func[iSTF]          = fnSTF;
  
  instr_func[iSTFSR]        = fnSTFSR;
  instr_func[iSTXFSR]       = fnSTXFSR;
  instr_func[iSTQF]         = fnSTQF;
  instr_func[iSTDF]         = fnSTDF;
  instr_func[iPREFETCH]     = fnPREFETCH;
  instr_func[iLDFA]         = fnLDFA;
  instr_func[iLDQFA]        = fnLDQFA;
  instr_func[iLDDFA]        = fnLDDFA;
  instr_func[iSTFA]         = fnSTFA;
  instr_func[iSTQFA]        = fnSTQFA;
  
  instr_func[iSTDFA]        = fnSTDFA;
  instr_func[iCASA]         = fnCASA;
  instr_func[iPREFETCHA]    = fnPREFETCHA;
  instr_func[iCASXA]        = fnCASXA;
  instr_func[iFLUSH]        = fnFLUSH;
  instr_func[iFLUSHW]       = fnFLUSHW;

/* REMOTE WRITE RELATED FUNCTIONS */
  instr_func[iRWWT_I]       = fnSTW;	
  instr_func[iRWWTI_I]      = fnSTW;	
  instr_func[iRWWS_I]       = fnSTW;	
  instr_func[iRWWSI_I]      = fnSTW;

  instr_func[iRWWT_F]       = fnSTDF;
  instr_func[iRWWTI_F]      = fnSTDF;
  instr_func[iRWWS_F]       = fnSTDF;
  instr_func[iRWWSI_F]      = fnSTDF;
  instr_func[iRWSD]         = fnRWSD;

}

