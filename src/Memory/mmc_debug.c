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
#include <string.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Memory/mmc.h"
#include "Bus/bus.h"

char *mmc_types[] = {
   "INVALID",
   "MMC_READ",
   "MMC_WRITE",
   "MMC_WRITEPURGE",
   "MMC_COPYOUT",
   "MMC_SYNC",
   "MMC_DPURGEALLOC",
   "MMC_DFLUSH",
   "MMC_DPURGE"
};

char *remap_name[] = {
   "AMS_TYPE_INVALID",
   "AMS_TYPE_SUPERPAGE",
   "AMS_TYPE_BASESTRIDE",
   "AMS_TYPE_TRANSPOSE",
   "AMS_TYPE_VINDIRECT",
   "AMS_TYPE_PAGECOLOR",
   "AMS_TYPE_VINDIRECT2",
   "AMS_TYPE_LASTMAP"
};

char *mtlb_state_name[] = {
   "MTLB_STATE_0",
   "MTLB_STATE_1",
   "MTLB_STATE_HT",
   "MTLB_STATE_MS",
   "MTLB_STATE_WB",
   "MTLB_STATE_DONE"
};

char *mtlb_bufstate_name[] = {
   "MTLB_INVALID",
   "MTLB_FETCHING",
   "MTLB_LOCKED",
   "MTLB_VALID"
};



/*
 * Dump MMC transaction
 */
void MMC_trans_dump(mmc_trans_t *ptrans, int flag, int nid)
{
  YS__logmsg(nid, "  0x%x:%s, sa(0x%x), time(%.0f)\n", 
	     ptrans, mmc_types[ptrans->mtype], ptrans->paddr, 
	     ptrans->time);
}



void MMC_dump(int nid)
{
  mmc_info_t *pmmc = NID2MMC(nid);
  REQ *req;
  mmc_trans_t  *ptrans;
  int  i, j, k;

  YS__logmsg(nid, "\n====== MMC ======\n");

  /*
   * Interface with bus
   */
  DumpDLinkQueue("request buffer", &(pmmc->reqqueue), dnext2, 0x61,
		 Cache_req_dump, nid);
  YS__logmsg(nid, "Arbwaiters_count = %d\n", pmmc->arbwaiters_count);
  DumpLinkQueue("arbwaiters", &(pmmc->arbwaiters), 0x62,
		Cache_req_dump, nid);

  if (mparam.sim != MMC_SIM_DETAILED)
    {
      DumpLinkQueue("wait list", &(pmmc->waitlist), 0, Cache_req_dump, nid);
      if (IsScheduled(pmmc->pevent))
	{
	  REQ *req = pmmc->pevent->uptr2;
	  
	  YS__logmsg(nid, "Pevent scheduled: YES\n");
	  YS__logmsg(nid, "  0x%x: type(%d), pref(%d), addr(0x%x)\n",
		     req, req->prcr_req_type, req->prefetch, req->paddr);
	}
      else
	YS__logmsg(nid, "Pevent scheduled: NO\n");
      return;
   }

  
  /*
   * Internal queues
   */
  YS__logmsg(nid, "\ntrans_count = %d, slave_count = %d\n", 
	     pmmc->trans_count, pmmc->slave_count);
  DumpLinkQueue("wait list", &(pmmc->waitlist), 0, MMC_trans_dump, nid);
  YS__logmsg(nid, "waittask scheduled: %s\n", 
	     IsScheduled(pmmc->waittask) ? "YES" : "NO");
}





