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

#include <stdio.h>
#include "Memory/mmc.h"
#include "DRAM/cqueue.h"
#include "DRAM/dram_param.h"
#include "DRAM/dram.h"

/*
 * Start the refresh task(event) for a chip. They start in staggered order
 * after the first operation per chip to avoid having them all go
 * off on the same cycle.
 */
void DRAM_start_refresh(dram_info_t *pdb, int chipid)
{
  dram_chip_t *pchip = &(pdb->chips[chipid]);
  EVENT       *pevent = pchip->refresh_event;

#ifdef TRACEDRAM
  if (dparam.debug_on)
    YS__logmsg(logfile, "Refresh is on its way.\n");
#endif

  pevent->body = DRAM_refresh;
  schedule_event(pevent, YS__Simtime + dparam.refresh_period);
}


/*
 * Start refreshing this chip.
 */
void DRAM_refresh(void)
{
  dram_info_t *pdb   = YS__ActEvnt->uptr1;
  dram_chip_t *pchip = YS__ActEvnt->uptr2;
  dram_bank_t *pbank;
  int          i;

#ifdef TRACEDRAM
  if (dparam.debug_on)
    fprintf(logfile, "DRAM_refresh at %.0f cycles\n", pevent->time);
#endif

  if (pchip->refresh_on)
    YS__errmsg(pdb->nodeid, "The chip is busy, can not refresh\n");
 
   /*
    * - If the chip is free, start the refresh operation and label the 
    *   chip as busy.
    * - If the chip is being used, the refresh task(event) will be activated
    *   right after the ongoing transactions finish.
    */
  for (i = 0; i < dparam.banks_per_chip; i++ )
    {
      pbank = &(pdb->banks[pchip->id + dparam.num_chips * i]);
      if (pbank->busy)
	{
	  pchip->refresh_needed = 1;
	  break;
	}
    }
  
  if (i == dparam.banks_per_chip)
    {
      pchip->refresh_needed = 0;
      pchip->refresh_on     = 1;
      
      YS__ActEvnt->body = DRAM_refresh_done;
      schedule_event(YS__ActEvnt, YS__ActEvnt->time + dparam.refresh_delay);
    }
}


/*
 * Refresh is done. Start up the next DRAM access if there is one 
 * in the waiting queue.
 */
void DRAM_refresh_done(void)
{
  dram_info_t *pdb   = YS__ActEvnt->uptr1;
  dram_chip_t *pchip = YS__ActEvnt->uptr2;
  dram_bank_t *pbank;
  int          i;

#ifdef TRACEDRAM
  if (dparam.debug_on)
    fprintf(logfile, "DRAM_refresh done at %.0f cycles\n", YS__ActEvnt->time);
#endif

  pchip->refresh_on = 0;
  
  for (i = 0; i < dparam.banks_per_chip; i++ )
    {
      pbank = &(pdb->banks[pchip->id + dparam.num_chips * i]);
      pbank->expire = 0;
    }
  
  /*
   * Do next memory access for the bank.
   */
  for (i = 0; i < dparam.banks_per_chip; i++ )
    {
      pbank = &(pdb->banks[pchip->id + dparam.num_chips * i]);

      if (pbank->waiters.size > 0)
	pbank->busy = 0;
      else
	{
	  dram_trans_t *dtrans = Bank_queue_dequeue(pbank);

	  pdb->total_bwaiters--;

	  if (dparam.collect_stats)
	    {
	      pbank->stats.queue_cycles += YS__Simtime - dtrans->time;
	      dtrans->time = YS__Simtime;
	    }
	  
	  DRAM_access_bank(pdb, dtrans, YS__Simtime);
	  break;
	}
    }

  /*
   * Schedule another refresh operation.
   */
  YS__ActEvnt->body = DRAM_refresh;
  schedule_event(YS__ActEvnt, YS__ActEvnt->time + dparam.refresh_period);
}


/*
 * Start a refresh operation.
 */
int DRAM_do_refresh(dram_info_t *pdb, int chipid)
{
  dram_chip_t *pchip  = &(pdb->chips[chipid]);
  EVENT       *pevent = pchip->refresh_event;

  pchip->refresh_on = 1;

  pevent->body = DRAM_refresh_done;
  schedule_event(pevent, YS__Simtime + dparam.refresh_delay);

  return 0;
}
