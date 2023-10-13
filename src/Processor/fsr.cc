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
/* Floating-point status register routines, mainly for marshalling and       */
/* unmarshalling various bit fields in the FSR                               */
/*---------------------------------------------------------------------------*/


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
#include "sim_main/simsys.h"
}                    

#include "Processor/procstate.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/tagcvt.hh"
#include "Processor/active.hh"
#include "Processor/registers.h"
#include "Processor/instruction.h"
#include "Processor/instance.h"
#include "Processor/fsr.h"
#include "Processor/procstate.hh"


volatile int fpfailed = 0;


/*---------------------------------------------------------------------------*/
/* set FSR with value from instances rdvali or rdvalipair, depending on      */
/* instruction type                                                          */
/*---------------------------------------------------------------------------*/

void set_fsr(ProcState *proc, instance *inst)
{
  if (inst->code.rd_regtype == REG_INT)
    {
      unsigned value = unsigned(inst->rdvali);
  
#ifndef NO_IEEE_FP
      fpsetround(fp_rnd((value >> 30) & 0x03));
#endif

      proc->fp754_trap_mask = (value >> 23) & 0x1F;

      proc->fp_trap_type    = (value >> 14) & 0x07;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						COND_FCC(0))] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  COND_FCC(0))]] =
	(value >> 10) & 0x03;
      
      proc->fp754_aexc      = (value >> 5) & 0x1F;
      proc->fp754_cexc      = (value >> 0) & 0x1F;
    }
  else if (inst->code.rd_regtype == REG_INT64)
    {
      unsigned value0 = unsigned(inst->rdvalipair.a);
      unsigned value1 = unsigned(inst->rdvalipair.b);

  
      // even part - upper 32 bits --------------------------------------------
  
      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						COND_FCC(1))] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  COND_FCC(1))]] =
	(value0 >> 0) & 0x03;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						COND_FCC(2))] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  COND_FCC(2))]] =
	(value0 >> 2) & 0x03;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						COND_FCC(3))] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  COND_FCC(3))]] =
	(value0 >> 4) & 0x03;

      
      // odd part - lower 32 bits ---------------------------------------------
  
#ifndef NO_IEEE_FP
      fpsetround(fp_rnd((value1 >> 30) & 0x03));
#endif

      proc->fp754_trap_mask = (value1 >> 23) & 0x1F;

      proc->fp_trap_type    = (value1 >> 14) & 0x07;

      proc->log_int_reg_file[arch_to_log(proc, proc->cwp,
						COND_FCC(0))] =
	proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc, proc->cwp,
						  COND_FCC(0))]] =
	(value1 >> 10) & 0x03;

      proc->fp754_aexc      = (value1 >> 5) & 0x1F;
      proc->fp754_cexc      = (value1 >> 0) & 0x1F;
    }
  else
    YS__warnmsg(proc->proc_id / ARCH_cpus,
		"Invalid register type %i in 'set_fsr' (%s 0x%08X)",
		inst->code.rd_regtype,
		inames[inst->code.instruction],
		inst->pc);
}





/*---------------------------------------------------------------------------*/
/* copy FSR value into instructions rs1vali/rs1valipair.                     */
/*---------------------------------------------------------------------------*/

void get_fsr(ProcState *proc, instance *inst)
{
#ifndef NO_IEEE_FP
  unsigned RD = ((unsigned)fpgetround())&3;
  // for now, RSIM and host have same rounding direction
#else
  unsigned RD = 0;
#endif

  unsigned TEM  = proc->fp754_trap_mask;     // IEEE FP exception mask
  unsigned NS   = 0;                         // non-standard FP impl.
  unsigned VER  = 0;                         // FP version number
  unsigned FTT  = proc->fp_trap_type;        // trap #
  unsigned QNE  = 0;                         // deferred trap queue
  unsigned fcc0 =                            // 2 bits
    proc->log_int_reg_file[arch_to_log(proc, inst->win_num,
					      COND_FCC(0))];
  unsigned AEXC = proc->fp754_aexc;          // accumulated exception, 5 bits
  unsigned CEXC = proc->fp754_cexc;          // current exception, 5 bits

  unsigned fsr  = (RD<<30) | (TEM<<23) | (NS<<22) | (VER<<17) |
    (FTT<<14) | (QNE<<13) | (fcc0<<10) | (AEXC<<5) | (CEXC<<0);

  
  if (inst->code.instruction == iSTFSR)
    inst->rs1vali = fsr;
  else
    {
      inst->rs1valipair.a =
	(((proc->
	   log_int_reg_file[arch_to_log(proc,
					       inst->win_num,
					       COND_FCC(3))]) << 4) |
	 ((proc->
	   log_int_reg_file[arch_to_log(proc,
					       inst->win_num,
					       COND_FCC(2))]) << 2) |
	 ((proc->
	   log_int_reg_file[arch_to_log(proc,
					       inst->win_num,
					       COND_FCC(1))]) << 4));
      inst->rs1valipair.b = fsr;
    }
}
