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
#include <strings.h>
#include <stdarg.h>
#include "Processor/simio.h"
#include "sim_main/util.h"
#include "Caches/system.h"
#include "Memory/mmc.h"
#include "Bus/bus.h"

void MMC_print_one_stat       (int, mmc_stat_t *pm);


/*=============================================================================
 * Statistics for all MMCs.
 */

void MMC_stat_report(int nid)
{
  mmc_info_t *pmmc = NID2MMC(nid);
  long long   total;

  if (mparam.collect_stats == 0 || Cache_perfect()) 
    return;
  
  YS__statmsg(nid, "Memory Controller Statistics\n");

  total        = pmmc->stats.reads + pmmc->stats.writes + pmmc->stats.copyouts;

  YS__statmsg(nid,
	      "  Total Transactions:   %12lld\n", total);

  YS__statmsg(nid,
	      "  Reads:                %12lld\n", pmmc->stats.reads);

  if (pmmc->stats.writes)
    YS__statmsg(nid,
		"  Writes:               %12lld\n", pmmc->stats.writes);

  if (pmmc->stats.upgrades)
    YS__statmsg(nid,
		"  Upgrades:             %12lld\n", pmmc->stats.upgrades);

  if (pmmc->stats.copyouts)
    YS__statmsg(nid,
		"  Copyouts:             %12lld\n", pmmc->stats.copyouts);

  YS__statmsg(nid,
	      "\n  busy_cycles:          %12lld\n", pmmc->stats.busy_cycles);

  YS__statmsg(nid,
	      "  free_cycles:          %12lld\n", pmmc->stats.free_cycles);

  YS__statmsg(nid,
	      "  total loads:          %12lld\n", pmmc->stats.load_count);

  YS__statmsg(nid,
	      "  total load cycles:    %12lld\n", pmmc->stats.load_cycles);

  YS__statmsg(nid,
	      "  load latency:         %12.2f\n\n",
	      1.0*pmmc->stats.load_cycles / pmmc->stats.load_count+2);
}



/*=============================================================================
 * MMC configuration
 */
void MMC_print_params(int nid)
{
  YS__statmsg(nid, "Memory Controller Configuration\n");

  if (mparam.sim == MMC_SIM_FIXED)
    {
      YS__statmsg(nid, "  simulator  \t Off  -  simulating fixed latency\n");
      YS__statmsg(nid, "  latency    \t%4d memory controller cycles\n",
		  mparam.latency);
      YS__statmsg(nid, "  frequency\t%4d\n",
		  mparam.frequency);
    }

  if (mparam.sim == MMC_SIM_PIPELINED)
    {
      YS__statmsg(nid, "  simulator  \t Off  -  simulating fixed, pipelined latency\n");
      YS__statmsg(nid, "  latency    \t%4d memory controller cycles\n",
		  mparam.latency);
      YS__statmsg(nid, "  frequency\t%4d\n",
		  mparam.frequency);
    }

  YS__statmsg(nid, "  statistics\t  %s \n\n",
	      mparam.collect_stats ? "On" : "Of");
}


/*=============================================================================
 * Reset statistics
 */

void MMC_stat_clear(int nid)
{
  if (mparam.collect_stats == 0 || Cache_perfect()) 
    return;

  bzero(&((NID2MMC(nid))->stats), sizeof(mmc_stat_t));
}
