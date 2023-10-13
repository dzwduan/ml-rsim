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

#include <string.h>
#include <malloc.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Bus/bus.h"


static char *BStateName[] =
{
  "IDLE",
  "ARBITRATING",
  "WAKINGUP"
};


extern int NUM_MODULES;
extern int MEM_CNTL;


/*=============================================================================
 * Print out everything the bus has. Need finer control?
 */

void Bus_dump(int nid)
{
  BUS *pbus = PID2BUS(nid);
  REQ *req;
  int  i;

  YS__logmsg(pbus->nodeid, "\n========= BUS MODULE =========\n");
  YS__logmsg(pbus->nodeid, "State(%s), busy_until(%.0f), write_flowcontrol(%d)\n", 
	     BStateName[pbus->state], pbus->busy_until,
	     pbus->write_flowcontrol);

  /*
   * Request number management, including all outstanding requests.
   */
  YS__logmsg(pbus->nodeid,
	     "request buffer: size = %d\n", pbus->req_count);
  for (i = 0; i < BUS_TOTAL_REQUESTS; i++)
    {
      if (pbus->req_queue[i])
	{
	  YS__logmsg(pbus->nodeid, "  %d:\n", i);
          Cache_req_dump(pbus->req_queue[i], 0x51, nid);
	}
    }
  
  /*
   * Arbitration waiters.
   */
  YS__logmsg(pbus->nodeid,
	     "Arbwaiters: size = %d\n", pbus->arbwaiter_count);
  for (i = 0; i < NUM_MODULES+1; i++)
    {
      if (pbus->arbwaiters[i].req)
	{
	  YS__logmsg(pbus->nodeid, "  %d:\n", i);
	  Cache_req_dump(pbus->arbwaiters[i].req, 0x53, nid);
	}
    }

  
  /*
   * States of all bus modules.
   */
  YS__logmsg(pbus->nodeid, "Current bus owner: %d.", pbus->cur_winner);
  for (i = 0; i < NUM_MODULES; i++)
    YS__logmsg(pbus->nodeid, "  Module(%d):%d,", i, pbus->pstates[i]);

  YS__logmsg(pbus->nodeid, " MMC:%d\n", pbus->pstates[i]);


  /*
   * Current bus state: 
   */
  if (pbus->next_winner != -1)
    {
      YS__logmsg(pbus->nodeid,
		 "Next winner: %d, start = %.0f\n", pbus->next_winner, 
		 pbus->next_start_time);
      Cache_req_dump(pbus->next_req, 0x53, nid);
    }

  YS__logmsg(pbus->nodeid,
	     "Arbitrator status: %sSCHEDULED\n", 
	     IsNotScheduled(pbus->arbitrator) ? "UN" : " ");

  YS__logmsg(pbus->nodeid,
	     "Issuer status: %sSCHEDULED\n", 
	     IsNotScheduled(pbus->issuer) ? "UN" : " ");

  YS__logmsg(pbus->nodeid,
	     "Performer status: %sSCHEDULED\n", 
	     IsNotScheduled(pbus->performer) ? "UN" : " ");

  if (IsScheduled(pbus->issuer))
    {
      YS__logmsg(pbus->nodeid,
		 "issuing_req: %s\n", 
		 (pbus->cur_req_owner == PROCESSOR) ? "PROCESSOR" : "MMC");
      Cache_req_dump(pbus->cur_req, 0x53, nid);
    }
}
