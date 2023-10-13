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
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Memory/mmc.h"
#include "DRAM/cqueue.h"
#include "DRAM/dram_param.h"
#include "DRAM/dram.h"


void DRAM_cbuf_dump(char *name, circbuf_t *q, int flag, int node);
void DRAM_trans_dump(dram_trans_t *dtrans, int flag, int node);

/*
 * Called upon the exit of simulator.
 */ 
void DRAM_exit(void)
{
  int   i, j, k;
  dram_info_t  *pdb;

  for (i = 0; i < ARCH_numnodes; i++)
    {
      dram_info_t *pdb = NID2DRAM(i);
      
      for (k = 0; k < dparam.num_banks; k++)
	{
	  dram_bank_t *pbank = &(pdb->banks[k]);
	  if (pbank->chip->refresh_on)
	    /* cancel_event(pbank->chip->refresh_event); */
	    pbank->chip->refresh_on = 0;
	}
    }
}


/*
 * Dump the current state of DRAM backend, including all the waiting 
 * queues in SA busses, SD busses, RD busses, and DRAM banks. The output
 * seems a little bit messy, put finer control later.
 */
void DRAM_dump(int nodeid)
{
  dram_info_t  *pdb = &(DBs[nodeid]);
  dram_trans_t *dtrans;
  int k;

  YS__logmsg(nodeid, "\n====== DRAM Scheduler ======\n");
 
  if (dparam.sim_on == 0)
    {
      YS__logmsg(nodeid,
		 "Total transactions: %d\n",
		 lqueue_check(&pdb->freedramlist));

      YS__logmsg(nodeid, "wait list: size = %d\n", pdb->waitlist.size); 

      for (k = 0; k < pdb->waitlist.size; k++)
	{
	  lqueue_elem(&pdb->waitlist, dtrans, k, nodeid);
	  DRAM_trans_dump(dtrans, 0, nodeid);
	}

      if (IsScheduled(pdb->pevent))
	{
	  dtrans = pdb->pevent->uptr2;
	  YS__logmsg(nodeid, "Pevent scheduled: YES\n");                    
	  DRAM_trans_dump(dtrans, 0, nodeid);
	}                                                          
      else                                                       
	YS__logmsg(nodeid, "Pevent scheduled: NO\n");                     

      return;
    }

  YS__logmsg(nodeid,
	     "Total transactions: %d\n",
	     lqueue_check(&pdb->freedramlist));
  YS__logmsg(nodeid,
	     "Total transactions in bank queue: %d\n",
	     pdb->total_bwaiters);
  
  if (!(pdb->sa_bus.busy == 0 && pdb->sa_bus.waiters->size == 0))
    {
      YS__logmsg(nodeid,
		 "SA bus busy(%d), pevent(%d)\n", pdb->sa_bus.busy,
		 IsScheduled(pdb->sa_bus.pevent) ? "SCHEDULED" : "UNSCHEDULED");
      DRAM_cbuf_dump("  waiters", pdb->sa_bus.waiters, 0, nodeid);
    }

  YS__logmsg(nodeid,
	     "SD bus busy_until(%.0f)\n",
	     pdb->sd_bus.busy_until);

  for (k = 0; k < dparam.rd_busses; k++)
    {
      dram_rd_bus_t *prdbus = &(pdb->rd_busses[k]);
      YS__logmsg(nodeid,
		 "RD bus %d, busy_until(%.0f)\n",
		 k, prdbus->busy_until);
    }

  for (k = 0; k < dparam.num_chips; k++)
    {
      dram_chip_t *pchip = &(pdb->chips[k]);
      YS__logmsg(nodeid,
		 "Chip:%d cas(%.0f) ras(%.0f) lastwrite(%d) lastbank(%d)\n",
		 k, pchip->cas_busy, pchip->ras_busy, pchip->last_type, 
		 pchip->last_bank);
    }

  for (k = 0; k < dparam.num_banks; k++)
    {
      dram_bank_t *pbank = &(pdb->banks[k]);
      if (IsNotScheduled(pbank->pevent) && pbank->busy == 0 &&
          pbank->waiters.size == 0)
	continue;
      YS__logmsg(nodeid,
		 "Bank %d: hotrow(%d) expire(%.0f) lastwrite(%d) event(%s)\n",
		 k, pbank->hot_row, pbank->expire, pbank->last_type,
		 IsScheduled(pbank->pevent) ? "SCHEDULED" : "UNSCHEDULED");

      if (pbank->busy)
	{
	  YS__logmsg(nodeid, "  The cur_trans:"); 
	  DRAM_trans_dump(pbank->busy, 0, nodeid);
	}
      
      if (pbank->waiters.size > 0)
	{
	  int m;
	  bank_queue_elm_t *bq = pbank->waiters.head;
	  YS__logmsg(nodeid,
		     "  waiters queue: size = %d\n",
		     pbank->waiters.size);
	  for (m = 0; m < pbank->waiters.size; m++)
	    {
	      DRAM_trans_dump(bq->data, 0, nodeid);
	      bq = bq->next;
	    }
	}
    }

  for (k = 0; k < dparam.num_databufs; k++)
    {
      dram_databuf_t *pdatabuf = &(pdb->databufs[k]);
      if (pdatabuf->busy == 0 && IsNotScheduled(pdatabuf->cevent) && 
          cbuf_empty(pdatabuf->waiters))
	continue;
      YS__logmsg(nodeid,
		 "Data buffer %d: busy(%d), cevent(%s)\n",
		 k, pdatabuf->busy ? 1 : 0, 
		 IsScheduled(pdatabuf->cevent) ? "SCHEDULED" : "UNSCHEDULED");
      if (pdatabuf->busy)
	DRAM_trans_dump(pdatabuf->busy, 0, nodeid);
      if (!cbuf_empty(pdatabuf->waiters)) 
	DRAM_cbuf_dump("  waiters", pdatabuf->waiters, 0, nodeid);
    }
}


/*
 * Dump a circular buffer used in DRAM backend. 
 */
void DRAM_cbuf_dump(char *name, circbuf_t *q, int flag, int node)
{
  dram_trans_t *dtrans;

  if (cbuf_inuse(q) == 0)
    return;

  YS__logmsg(node,
	     "%s: size = %d\n", name, cbuf_inuse(q));
  cbuf_iterate(q, dtrans, dram_trans_t *)
    DRAM_trans_dump(dtrans, flag, node);
}


/*
 * Dump a dram transaction. (see dram.h for definition of dram_trans_t.)
 */
void DRAM_trans_dump(dram_trans_t *dtrans, int flag, int node)
{
  YS__logmsg(node,
	     "  0x%x: ptrans(0x%x) pa(0x%x) sz(%d) W(%d)\n",
	     dtrans, dtrans->ptrans, dtrans->paddr, dtrans->size,
	     dtrans->is_write);
  YS__logmsg(node,
	     "              %s, time(%.0f) etime(%.0f)\n",
	     dtrans->is_write ? "WRITE" : "READ", dtrans->time, dtrans->etime);
}
