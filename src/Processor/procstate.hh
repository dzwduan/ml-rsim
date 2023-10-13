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

#ifndef __RSIM_PROCSTATE_HH__
#define __RSIM_PROCSTATE_HH__


/*
 * convert architectural register to logical register (SPARC_specific)
 *  WINDOW STRUCTURE IS AS FOLLOWS (i:input, l:local, o:output)
 *  (this shows a *  4 window system)
 *          i     o
 *     WP3  l
 *          o i
 *     WP2    l
 *            o i
 *     WP1      l
 *      	o i
 *     WP0        l
 */
inline short arch_to_log(ProcState *proc, int cwp, int iarch)
{
  if (iarch < END_WINDOWED)
    return proc->log_int_reg_map[cwp * 64 +
				PSTATE_GET_AG(proc->pstate) * 4 + iarch];
  else
    return proc->log_state_reg_map[proc->tl * ToPowerOf2(END_OF_REGISTERS - END_WINDOWED) + iarch - END_WINDOWED];
}



inline void setup_arch_to_log_table(ProcState *proc)
{
  proc->log_int_reg_map = (short*)malloc(MAX_NUM_WINS * 32 * 2*sizeof(short));

  for (int ag = 0; ag < 2; ag++)
    for (int cwp = 0; cwp < MAX_NUM_WINS; cwp++)
      for (int iarch = 0; iarch < END_WINDOWED; iarch++)
	{
	  int pr;

	  pr = cwp * 64 + ag * 32 + iarch;

	  if (iarch < END_GLOBALS)
	    if (iarch == 0)
	      proc->log_int_reg_map[pr] = (short)0;
	    else
	      proc->log_int_reg_map[pr] = (short)(iarch + 8 * ag);

	  else if (iarch >= 16)
	    proc->log_int_reg_map[pr] =
	      (short)(WINSTART+16*(cwp % NUM_WINS)+(iarch-16));  // i7-i0,l7-l0

	  else                                           // It's an "o" reg
	    proc->log_int_reg_map[pr] =
	      (short)(WINSTART+16*((cwp+1) % NUM_WINS)+(iarch)); // o7-o0
	}

  

  proc->log_state_reg_map = (short*)malloc(MAX_NUM_TRAPS * ToPowerOf2(END_OF_REGISTERS - END_WINDOWED) * sizeof(short));
  for (int tl = 0; tl < MAX_NUM_TRAPS; tl++)
    for (int reg = END_WINDOWED; reg < END_OF_REGISTERS; reg++)
      {
	int iarch, pr;

	pr = tl * ToPowerOf2(END_OF_REGISTERS - END_WINDOWED) + reg - END_WINDOWED;
	iarch = reg;

	if (iarch == COND_XCC)
	  iarch  = COND_ICC;

	if ((iarch >= PRIV_TPC) && (iarch <= PRIV_TT))
	  {
	    proc->log_state_reg_map[pr] = (short)(iarch + 12 + tl * 4);
	    continue;
	  }

	if (iarch > PRIV_TT)
	  iarch -= 4;

	proc->log_state_reg_map[pr] = (short)iarch-16;
      }
}






inline unsigned DOWN_TO_PAGE(unsigned i)
{
  return i-i%PAGE_SIZE;
}
 
inline unsigned UP_TO_PAGE(unsigned i)
{
  unsigned j = i+PAGE_SIZE-1;
  return j-j%PAGE_SIZE;
}


/*************************************************************************/
/* GetMap : gets the UNIX address of the address accessed by memory      */
/*          instruction. If no translation is found, and the it is not   */
/*          a prefetch, and no TLB is simulated (size=0), mark it as     */
/*          segmentation fault.                                          */
/*************************************************************************/

inline char* GetMap(instance *inst, ProcState *proc)
{
  char *pa;

  pa = proc->PageTable->lookup(inst->addr);

  return pa;
}




inline void ProcState::ComputeAvail()
{
  int avails = active_list.NumAvail();

  if (avails < decode_rate)
    {
      if (in_exception != NULL)     // credit all the losses to exception
	avail_active_full_losses[lEXCEPT] += decode_rate - avails;
      else
	{     // find out why
	  instance *avloss = GetHeadInst(this);
	  if (avloss)
            avail_active_full_losses[lattype[avloss->code.instruction]] +=
	      decode_rate - avails;
	}
    }
  else
    avails = decode_rate;

  stalledeff += avails;
  avail_fetch_slots += avails;
}

#endif




