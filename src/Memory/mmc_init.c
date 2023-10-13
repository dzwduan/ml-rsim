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
#include <stdlib.h>
#include "Processor/simio.h"
#include "sim_main/util.h"
#include "Memory/mmc.h"
#include "IO/addr_map.h"
#include "Bus/bus.h"
#include "../../lamix/mm/mm.h"


mmc_param_t  mparam;
mmc_info_t  *AllMMCs;




/*=============================================================================
 * Initialize the MMC module.
 */
void MMC_init(void)
{
  mmc_info_t  *pmmc;
  mmc_trans_t *ptrans;
  int      	i, j;

  MMC_read_params();

  if (!(AllMMCs = RSIM_CALLOC(mmc_info_t, ARCH_numnodes)))
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < ARCH_numnodes; i++)
    {
      /* announce address range ---------------------------------------------*/
      AddrMap_insert(i, 0, IO_SEGMENT_LOW, ARCH_cpus + ARCH_ios);

      pmmc = &(AllMMCs[i]);

      pmmc->nodeid      = i;
      pmmc->trans_count = 0;
      pmmc->busy_start  = 0;
      pmmc->free_start  = 0;
      pmmc->slave_count = 0;
      pmmc->wb_count    = 0;

      lqueue_init(&(pmmc->waitlist), MAX_TRANS + mparam.max_writeback_count);
      lqueue_init(&(pmmc->freelist), MAX_TRANS + mparam.max_writeback_count);

      if (!(ptrans = RSIM_CALLOC(mmc_trans_t, MAX_TRANS)))
	YS__errmsg(i, "malloc failed at %s:%i", __FILE__, __LINE__);

      for (j = 0; j < MAX_TRANS; j++)
	{
	  lqueue_add(&(pmmc->freelist), ptrans, i);
	  if (ptrans == NULL)
	    YS__errmsg(0, "NULL\n");
	  ptrans++;
	}
      
      if (mparam.sim != MMC_SIM_DETAILED)
	{
	  dqueue_init(&(pmmc->reqqueue), BUS_TOTAL_REQUESTS * 2);
	  lqueue_init(&(pmmc->waitlist), BUS_TOTAL_REQUESTS * 2);
	  lqueue_init(&(pmmc->arbwaiters), MAX_TRANS);
	  pmmc->arbwaiters_count = 0;
	  pmmc->pevent = NewEvent("nosim_done", MMC_nosim_done, NODELETE, 0);
	  pmmc->pevent->uptr1 = pmmc;
	  continue;
	}

      pmmc->waittask = NewEvent("mmc_access", MMC_process_waiter, NODELETE, 0);
      pmmc->waittask->uptr1 = (void *)pmmc;

      dqueue_init(&(pmmc->reqqueue), BUS_TOTAL_REQUESTS * 2);
      lqueue_init(&(pmmc->arbwaiters), MAX_TRANS);
      pmmc->arbwaiters_count = 0;
    }
}



/*=============================================================================
 * Read parameters from parameter file.
 */
void MMC_read_params(void)
{
  char buf[1024];
  int  tmp;

  /*
   * Some switches
   */
  mparam.sim                 = MMC_SIM_DETAILED;
  mparam.latency             = 20;
  mparam.frequency           = 1;
  mparam.debug               = 0;
  mparam.collect_stats       = 1;
  mparam.max_writeback_count = ARCH_cpus + ARCH_coh_ios;

  /* parameter name has been abbreviated, old name is still accepted         */
  /* also accepts old binary values (0/1) in addition to new textual values  */
  buf[0] = 0;
  get_parameter("MMC_sim", buf, PARAM_STRING);
  if (strcmp(buf, "0") == 0)
    mparam.sim = MMC_SIM_FIXED;
  else if (strcmp(buf, "1") == 0)
    mparam.sim = MMC_SIM_DETAILED;
  else if (strncasecmp(buf, "fix", 3) == 0)
    mparam.sim = MMC_SIM_FIXED;
  else if (strncasecmp(buf, "pipe", 4) == 0)
    mparam.sim = MMC_SIM_PIPELINED;
  else if (strncasecmp(buf, "detail", 6) == 0)
    mparam.sim = MMC_SIM_DETAILED;
  
  get_parameter("MMC_latency",       &mparam.latency,              PARAM_INT);
  get_parameter("MMC_frequency",     &mparam.frequency,            PARAM_INT);
  get_parameter("MMC_debug",         &mparam.debug,                PARAM_INT);
  get_parameter("MMC_collect_stats", &mparam.collect_stats,        PARAM_INT);
  get_parameter("MMC_writebacks",    &mparam.max_writeback_count,  PARAM_INT);

  if (mparam.max_writeback_count < ARCH_cpus + ARCH_coh_ios)
    YS__warnmsg(0, "Too few writebacks allowed by MC; possible deadlock!");

  mparam.cache_line_size = L2_DEFAULT_SIZE;
  get_parameter("L2C_line_size",     &mparam.cache_line_size, PARAM_INT);
  
  if (NumOfBits(mparam.cache_line_size, 1) == -1)
    YS__errmsg(0, "MMC cache line isn't a power of 2");
}
