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

extern "C"
{
#include "sim_main/util.h"
}

#include "Processor/procstate.h"
#include "Processor/memunit.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"


int   latencies[numUTYPES];
int   repeat[numUTYPES];
latfp latfuncs[numUTYPES];
repfp repfuncs[numUTYPES];
EFP   instr_func[numINSTRS];


int LAT_ALU_MUL     = 3;
int LAT_ALU_DIV     = 9;
int LAT_ALU_SHIFT   = 1;
int LAT_ALU_OTHER   = 1;
int REP_ALU_MUL     = 1;
int REP_ALU_DIV     = 1;
int REP_ALU_SHIFT   = 1;
int REP_ALU_OTHER   = 1;
int LAT_FPU_MOV     = 1;
int LAT_FPU_CONV    = 4;
int LAT_FPU_COMMON  = 3;
int LAT_FPU_DIV     = 10;
int LAT_FPU_SQRT    = 10;
int REP_FPU_MOV     = 1;
int REP_FPU_CONV    = 2;
int REP_FPU_COMMON  = 1;
int REP_FPU_DIV     = 6;
int REP_FPU_SQRT    = 6;


int alu_latency(instance * inst, ProcState * proc)
{
  int lat;
  switch (inst->code.instruction)
    {
    case iMULX:
    case iUMUL:
    case iSMUL:
    case iUMULcc:
    case iSMULcc:
      lat = LAT_ALU_MUL;
      break;

    case iUDIVX:
    case iUDIV:
    case iSDIV:
    case iUDIVcc:
    case iSDIVcc:
      lat = LAT_ALU_DIV;
      break;

    case iSLL:
    case iSRL:
    case iSRA:
      lat = LAT_ALU_SHIFT;
      break;

    default:
      lat = LAT_ALU_OTHER;
      break;
    }

  proc->Running.insert(proc->curr_cycle + (long long)lat, inst, inst->tag);
  return lat;
}


int alu_rep(instance * inst, ProcState * proc)
{
  int rep;
  switch (inst->code.instruction)
    {
    case iMULX:
    case iUMUL:
    case iSMUL:
    case iUMULcc:
    case iSMULcc:
      rep = REP_ALU_MUL;
      break;

    case iUDIVX:
    case iUDIV:
    case iSDIV:
    case iUDIVcc:
    case iSDIVcc:
      rep = REP_ALU_DIV;
      break;

    case iSLL:
    case iSRL:
    case iSRA:
      rep = REP_ALU_SHIFT;
      break;

    default:
      rep = REP_ALU_OTHER;
      break;
    }

  proc->FreeingUnits.insert(proc->curr_cycle + (long long)rep, uALU);
  return rep;
}



int fpu_latency(instance * inst, ProcState * proc)
{
  int lat;
  switch (inst->code.instruction)
    {
    case iFADDs:
    case iFADDd:
    case iFADDq:
    case iFSUBs:
    case iFSUBd:
    case iFSUBq:
    case iFMULs:
    case iFMULd:
    case iFMULq:
    case iFsMULd:
    case iFdMULq:
    case iFCMPs:
    case iFCMPd:
    case iFCMPq:
    case iFCMPEs:
    case iFCMPEd:
    case iFCMPEq:
      lat = LAT_FPU_COMMON;
      break;
      
    case iFSQRTs:
    case iFSQRTd:
    case iFSQRTq:
      lat = LAT_FPU_SQRT;
      break;
      
    case iFDIVs:
    case iFDIVd:
    case iFDIVq:
      lat = LAT_FPU_DIV;
      break;
      
    case iFsTOx:
    case iFdTOx:
    case iFqTOx:
    case iFxTOs:
    case iFxTOd:
    case iFxToq:
    case iFiTOs:
    case iFiTOd:
    case iFiTOq:
    case iFsTOi:
    case iFdTOi:
    case iFqTOi: // various i->f conversions
      lat = LAT_FPU_CONV;
      break;

    default:
      lat = LAT_FPU_MOV;        // move, neg, abs, movcc
      break;
    }

  proc->Running.insert(proc->curr_cycle + (long long)lat, inst, inst->tag);
  return lat;
}



int fpu_rep(instance * inst, ProcState * proc)
{
  int rep;

  switch (inst->code.instruction)
    {
    case iFADDs:
    case iFADDd:
    case iFADDq:
    case iFSUBs:
    case iFSUBd:
    case iFSUBq:
    case iFMULs:
    case iFMULd:
    case iFMULq:
    case iFsMULd:
    case iFdMULq:
    case iFCMPs:
    case iFCMPd:
    case iFCMPq:
    case iFCMPEs:
    case iFCMPEd:
    case iFCMPEq:
      rep = REP_FPU_COMMON;
      break;

    case iFSQRTs:
    case iFSQRTd:
    case iFSQRTq:
      rep = REP_FPU_SQRT;
      break;

    case iFDIVs:
    case iFDIVd:
    case iFDIVq:
      rep = REP_FPU_DIV;
      break;
      
    case iFsTOx:
    case iFdTOx:
    case iFqTOx:
    case iFxTOs:
    case iFxTOd:
    case iFxToq:
    case iFiTOs:
    case iFiTOd:
    case iFiTOq:
    case iFsTOi:
    case iFdTOi:
    case iFqTOi: // various i->f conversions
      rep = REP_FPU_CONV;
      break;

    default:
      rep = REP_FPU_MOV;        // move, neg, abs, movcc
      break;
    }

  proc->FreeingUnits.insert(proc->curr_cycle + (long long)rep, uFP);
  return rep;
}


/*
 * Set up tables that determine the latency of various functional 
 * units + other intialization. The function is also called during
 * flush (exception, miss predication, etc.). It will flush all 
 * the ready queues of functions units.
 */
void UnitSetup(ProcState * proc, int except)
{
  if (!FAST_UNITS)
    {
      /* note: The value 0 in the latencies or repeat array indicates that some 
         function must be called in order to calculate the actual latency or
         repeat rate for the given instruction. */
      latencies[uALU]  = 0;
      latencies[uFP]   = 0;
      latencies[uMEM]  = 0;
      latencies[uADDR] = 1;
      latfuncs[uALU]   = alu_latency;
      latfuncs[uFP]    = fpu_latency;
      latfuncs[uMEM]   = memory_latency;

      if (REP_ALU_MUL == REP_ALU_DIV &&
	  REP_ALU_MUL == REP_ALU_SHIFT && 
          REP_ALU_MUL == REP_ALU_OTHER)
	repeat[uALU] = REP_ALU_MUL;
      else
	{
	  repeat[uALU] = 0;
	  repfuncs[uALU] = alu_rep;
	}

      repeat[uFP] = 0;
      repeat[uMEM] = 0;
      repeat[uADDR] = 1;
      repfuncs[uFP] = fpu_rep;
      repfuncs[uMEM] = 0;
   }
  else
    {
      // if FAST_UNITS set, do 1 cycle ALU and FPU
      latencies[uALU] = 1;
      latencies[uFP] = 1;
      latencies[uMEM] = 0;
      latencies[uADDR] = 1;
      latfuncs[uMEM] = memory_latency;

      repeat[uALU] = 1;
      repeat[uFP] = 1;
      repeat[uMEM] = 0;
      repfuncs[uMEM] = 0;
      repeat[uADDR] = 1;
    }

  proc->UnitsFree[uALU] = proc->MaxUnits[uALU] = ALU_UNITS;
  proc->UnitsFree[uFP] = proc->MaxUnits[uFP] = FPU_UNITS;
  proc->MaxUnits[uMEM] = MEM_UNITS;

  if (!except)
    {
      /* we don't reset this on an exception since we don't 
         flush cache ports on exception */
      proc->UnitsFree[uMEM] = MEM_UNITS;        
    }

  proc->UnitsFree[uADDR] = proc->MaxUnits[uADDR] = ADDR_UNITS;


  /* also, go through and empty all heaps */
  UTYPE uj;
  while (proc->FreeingUnits.GetMin(uj) != -1)
    {
      if (uj == uMEM)
	proc->UnitsFree[uMEM]++;
    }

  for (int Ujunk = 0; Ujunk < int (numUTYPES); Ujunk++)
    {
      circq<tagged_inst> &q = proc->ReadyQueues[Ujunk];
      tagged_inst ijunktag;

      while (q.Delete(ijunktag))
	ijunktag.inst->tag = -1;
    }
}

#ifdef COREFILE



/*************************************************************************/
/* InstCompletionMessage : print out debug information at complete time  */
/*************************************************************************/

static void InstCompletionMessage(instance * inst, ProcState * proc)
{
  if (proc->curr_cycle > DEBUG_TIME)
    {
      fprintf(corefile,
	      "Completed executing tag = %d, %s, ",
              inst->tag,
	      inames[inst->code.instruction]);

      switch (inst->code.rd_regtype)
	{
	case REG_INT:
	  fprintf(corefile, "rdi = %d, ", inst->rdvali);
	  break;
	case REG_FP:
	  fprintf(corefile, "rdf = %f, ", inst->rdvalf);
	  break;
	case REG_FPHALF:
	  fprintf(corefile, "rdfh = %f, ", double (inst->rdvalfh));
	  break;
	case REG_INTPAIR:
	  fprintf(corefile, "rdp = %d/%d, ", inst->rdvalipair.a, 
		  inst->rdvalipair.b);
	  break;
	case REG_INT64:
	  fprintf(corefile, "rdll = %lld, ", inst->rdvalll);
	  break;
	default:
	  fprintf(corefile, "rdX = XXX, ");
	  break;
	}

      fprintf(corefile, "rccvali = %d \n", inst->rccvali);
    } 
}


void InstMessageToRunningQueue(ProcState *proc, instance *inst, int unit_type)
{
  if (unit_type != uADDR)
    {
      if (proc->curr_cycle > DEBUG_TIME)
	{
	  fprintf(corefile,
		  "Issue tag = %lld, %s, ",
		  inst->tag,
		  inames[inst->code.instruction]);

	  switch (inst->code.rs1_regtype)
	    {
	    case REG_INT:
	      fprintf(corefile, "rs1i = %d, ", inst->rs1vali);
	      break;
	    case REG_FP:
	      fprintf(corefile, "rs1f = %f, ", inst->rs1valf);
	      break;
	    case REG_FPHALF:
	      fprintf(corefile, "rs1fh = %f, ", double (inst->rs1valfh));
	      break;
	    case REG_INTPAIR:
	      fprintf(corefile, "rs1p = %d/%d, ", 
		      inst->rs1valipair.a, inst->rs1valipair.b);
	      break;
	    case REG_INT64:
	      fprintf(corefile, "rs1ll = %lld, ", inst->rs1valll);
	      break;
	    default:
	      fprintf(corefile, "rs1X = XXX, ");
	      break;
	    }

	  switch (inst->code.rs2_regtype)
	    {
	    case REG_INT:
	      fprintf(corefile, "rs2i = %d, ", inst->rs2vali);
	      break;
	    case REG_FP:
	      fprintf(corefile, "rs2f = %f, ", inst->rs2valf);
	      break;
	    case REG_FPHALF:
	      fprintf(corefile, "rs2fh = %f, ", double (inst->rs2valfh));
	      break;
	    case REG_INTPAIR:
	      fprintf(corefile, "rs2pair unsupported");
	      break;
	    case REG_INT64:
	      fprintf(corefile, "rs2ll = %lld, ", inst->rs2valll);
	      break;
	    default:
	      fprintf(corefile, "rs2X = XXX, ");
	      break;
	    }

	  fprintf(corefile, "rscc = %d, imm = %d\n",
		  inst->rsccvali, inst->code.imm);
	}
    }
  else
    {
      if (proc->curr_cycle > DEBUG_TIME)
	fprintf(corefile, "Issue address generation for %lld\n", inst->tag);
    }
}
#endif
