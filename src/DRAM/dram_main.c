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
#include "DRAM/cqueue.h"
#include "Memory/mmc.h"
#include "DRAM/dram_param.h"
#include "DRAM/dram.h"

/*
 * Master Memory controller uses this function to send a memory access 
 * to DRAM backend. 
 */
void  DRAM_recv_request(int nodeid, mmc_trans_t *ptrans,
			unsigned paddr, int size, int is_write)
{
  dram_info_t   *pdb = NID2DRAM(nodeid);
  dram_sa_bus_t *psabus;
  EVENT	        *pevent;
  dram_trans_t  *dtrans = DRAM_new_transaction(pdb, ptrans, paddr, size, 
					       is_write);

#ifdef TRACEDRAM
  if (dparam.trace_on)
    {
      if(!dparam.trace_max || (dram_trace_count < dparam.trace_max))
	{
	  fprintf(dram_trace_fp, "%d 0x%x %d %d %.0f\n", 
		  (ptrans->dspr) ? (ptrans->dspr->index+1): 0, paddr, 
		  (dtrans->is_write) ? 1 : 0, size, YS__Simtime);
	  dram_trace_count++;
	}
    }
#endif

   if (dparam.sim_on == 0)
     {
       DRAM_nosim_start(pdb, dtrans);
       return;
     }


   /*
    * - If SA bus is busy, push the request into the waiting queue.
    * - Otherwise, use it to send the request to the relevant bank. 
    *   Function DRAM_arrive_smc() will be activated to 
    *   receive this access after "sa_bus_cycles" cycles.
    */
   if (pdb->sa_bus.busy)
     {
       if (cbuf_full(pdb->sa_bus.waiters))
         YS__errmsg(nodeid, "SA bus's waiting queue is full.\n");
       else
         cbuf_enqueue(pdb->sa_bus.waiters, dtrans);
     }
   else
     {
       pdb->sa_bus.busy = 1;
       pevent        = pdb->sa_bus.pevent;
       pevent->uptr3 = dtrans;
       schedule_event(pevent, YS__Simtime + dparam.sa_bus_cycles);
     }
}



/*
 * A DRAM access request arrives at slave memory controller.
 */ 
void DRAM_arrive_smc(void)
{
  dram_info_t    *pdb    = YS__ActEvnt->uptr1;
  dram_sa_bus_t  *psabus = YS__ActEvnt->uptr2;
  dram_trans_t   *dtrans = YS__ActEvnt->uptr3;
  unsigned        paddr  = dtrans->paddr;
  dram_bank_t    *pbank  = DRAM_paddr_to_bank(pdb, paddr);

  
  /*
   * Collect statitics.
   */
  if (dparam.collect_stats)
    {
      psabus->count++;
      psabus->cycles += YS__Simtime - dtrans->time;
      dtrans->time = YS__Simtime;
    }

  /* 
   * - If the bank is busy, put the access into the waiting queue.
   * - otherwise, access the bank.
   */
  if (pbank->busy || pbank->chip->refresh_on)
    {
      if (Bank_queue_full(&(pbank->waiters)))
	YS__errmsg(0, "pbank->waiters are full\n");

      if (dparam.collect_stats)
	{
	  pbank->stats.queue_waits++;
	  pbank->stats.queue_len += pbank->waiters.size;
	}
      
      Bank_queue_enqueue(&(pbank->waiters), dtrans);
      pdb->total_bwaiters++;
    }
  else
    DRAM_access_bank(pdb, dtrans, YS__Simtime);

  
  /* 
   * - If there are other requests waiting on the same slave address bus,
   *   grant this slave address bus to the next one in FIFO order.
   * - Otherwise, free the slave address bus.
   */
  if (cbuf_empty(psabus->waiters))
    {
      psabus->busy = 0;
    }
  else
    {
      EVENT  *pevent = psabus->pevent;
      dtrans = (dram_trans_t *) cbuf_dequeue(psabus->waiters);
      
      if (dparam.collect_stats)
	{
	  psabus->waits++;
	  psabus->wcycles += YS__Simtime - dtrans->time;
	}

      pevent->uptr3   = dtrans;
      schedule_event(pevent, YS__Simtime + dparam.sa_bus_cycles);
    }
}



/*
 * Access to specific bank. 
 */
void  DRAM_access_bank(dram_info_t *pdb, dram_trans_t *dtrans,
		       rsim_time_t starttime)
{
  dram_bank_t  *pbank = DRAM_paddr_to_bank(pdb, dtrans->paddr);

  /*
   * Collect statistics.
   */
  if (dparam.collect_stats)
    {
      if (dtrans->is_write) 
	pbank->stats.writes++;
      else 
	pbank->stats.reads++;
    }

  /* 
   * Access specified DRAMs. 
   */
  if (dparam.scheduler_on)
    {
      switch (dparam.dram_type)
	{
	case SDRAM:
	  DRAM_access_SDRAM_bank(pdb, dtrans, starttime);
	  break;
	case RDRAM:
	  DRAM_access_RDRAM_bank(pdb, dtrans, starttime);
	  break;
	default:
	  YS__errmsg(pdb->nodeid, "Unkown DRAM type %d\n", dparam.dram_type);
	}
    }
  else
    DRAM_access_bank_aux1(pdb, dtrans, starttime);

  /*
   * Remember the type of last access. It's used for Precharge command.
   */
  pbank->last_type = dtrans->is_write;
}



/*
 * Called when the scheduler is NOT on. In this case, each bank access
 * is a constant delay.
 */
void  DRAM_access_bank_aux1(dram_info_t *pdb, dram_trans_t *dtrans,
			    rsim_time_t starttime)
{
  unsigned      paddr  = dtrans->paddr;
  mmc_trans_t  *ptrans = dtrans->ptrans;
  int           size   = dtrans->size;
  dram_bank_t  *pbank  = DRAM_paddr_to_bank(pdb, paddr);
  EVENT        *pevent = pbank->pevent;
  rsim_time_t   nxttime;
   
  /*
   * Call DRAM_bank_done when the access is done.
   */
  if (dparam.dram_type == RDRAM) 
    nxttime = YS__Simtime + dparam.co2d_cycles + 
      dparam.dtime.r.PACKET * (size >> dparam.width_shift);
  else
    nxttime = YS__Simtime + dparam.co2d_cycles +  
      dparam.dtime.s.PACKET * (size >> dparam.width_shift);

  pbank->busy = dtrans;
  schedule_event(pevent, nxttime);
}



/*
 * Access Synchronous DRAM. Please see SDRAM documents for the details 
 * on SDRAM timing. The basic idea here is to find the earliest 
 * possible time to start the access.
 */
void  DRAM_access_SDRAM_bank(dram_info_t *pdb, dram_trans_t *dtrans, 
			     rsim_time_t starttime)
{
  mmc_trans_t   *ptrans    = dtrans->ptrans;
  unsigned       paddr     = dtrans->paddr;
  unsigned       size      = dtrans->size;
  int            row_num   = paddr >> dparam.row_shift;
  dram_bank_t   *pbank     = DRAM_paddr_to_bank(pdb, paddr);
  dram_rd_bus_t *prdbus    = DRAM_bankid_to_rd_bus(pdb, pbank->id);
  dram_chip_t   *pchip     = pbank->chip;
  EVENT         *pevent    = pbank->pevent;
  int            cas_count = size >> dparam.width_shift;
  rsim_time_t    bus_busy, start_time, wcycles, delay;
  int            hit_row;
   
  /*
   * First, find out if it hits/misses on the hot row.
   */
  if (row_num == pbank->hot_row && pbank->expire > starttime ) 
    hit_row = 1;
  else
    hit_row = 0;

  pbank->hitmiss = (pbank->hitmiss << 1) | (row_num == pbank->hot_row);

  /*
   * Add command2command delay.
   */
  if (pchip->last_bank == pbank->id)
    {
      prdbus->busy_until += dparam.dtime.s.CCD;
      pchip->ras_busy    += dparam.dtime.s.CCD;
    }
  else
    {
      prdbus->busy_until += dparam.dtime.s.RRD;
      pchip->ras_busy    += dparam.dtime.s.RRD;
    }

  
  /* 
   * Find the earliest time that the access can be initiated, for
   * the RD bus concern only. 
   */
  if (hit_row)
    bus_busy = prdbus->busy_until;
  else
    bus_busy = prdbus->busy_until - 
      (dparam.dtime.s.RAS + dparam.dtime.s.RCD);

  if (!dtrans->is_write)
    bus_busy -= dparam.dtime.s.AA;

  start_time = MAX(MAX(pchip->ras_busy, bus_busy), starttime);

  /*
   * Collect statistics.
   */
  if (dparam.collect_stats)
    {
      prdbus->count++;
      prdbus->cycles += dparam.dtime.s.PACKET * cas_count;
      if (start_time == bus_busy)
	{
	  wcycles = bus_busy - MAX(pchip->ras_busy, starttime);
	  if (wcycles > 0)
	    {
	      prdbus->waits++;
	      prdbus->wcycles += wcycles;
	      prdbus->cycles  += wcycles;
	    }
	}
      
      if (hit_row)
	{
	  if (dtrans->is_write)
	    pbank->stats.write_hits++;
	  else  
	    pbank->stats.read_hits++;
	}
      else if (pbank->expire > starttime)
	{
	  if (dtrans->is_write)
	    pbank->stats.write_misses++;
	  else  
	    pbank->stats.read_misses++;
	}
      
      pbank->stats.queue_cycles += start_time - dtrans->time;
      dtrans->time = start_time;
      if (start_time < prdbus->busy_until)
	pbank->stats.overlap += prdbus->busy_until - start_time;
    }

  
  /*
   * Close the current hot row if it exists. 
   */
  if (!hit_row && pbank->expire > starttime)
    {
      start_time += dparam.dtime.s.RP;
      if (pbank->last_type == DRAM_WRITE)
	start_time += dparam.dtime.s.DAL;
    }

  
  /*
   * Update the states of control signals (RAS/CAS) and data signals.
   * 
   */
  if (hit_row)
    {
      pchip->ras_busy    = start_time + dparam.dtime.s.PACKET * cas_count;
      prdbus->busy_until = start_time + dparam.dtime.s.PACKET * cas_count;
    }
  else
    {
      pchip->ras_busy    = start_time + dparam.dtime.s.RAS + 
	dparam.dtime.s.RCD + 
	dparam.dtime.s.PACKET * cas_count;
      prdbus->busy_until = start_time + dparam.dtime.s.RAS + 
	dparam.dtime.s.RCD + 
	dparam.dtime.s.PACKET * cas_count;
    }
  
  if (!dtrans->is_write)
    prdbus->busy_until += dparam.dtime.s.AA;

  
  /* 
   * - If the hot row is remained open after this access, remember
   *   the active row.
   * - Otherwise, enable auto precharge operation.
   */
  pbank->hot_row = row_num;
  if (dtrans->open_row)
    {
      pbank->expire  = start_time + dparam.row_hold_time;
    }
  else
    {
      if (dtrans->is_write)
	{
	  pchip->ras_busy    += dparam.dtime.s.DPL + dparam.dtime.s.RP;
	  prdbus->busy_until += dparam.dtime.s.DPL + dparam.dtime.s.RP;
	}
      else
	{
	  delay = dparam.dtime.s.RP - cas_count;
	  if (delay < 0)
            delay = 0;
	  pchip->ras_busy    += dparam.dtime.s.DPL + delay;
	  prdbus->busy_until += dparam.dtime.s.DPL + delay;
	}
      
      pbank->expire = 0;
    }

  pchip->last_bank = pbank->id;
  pchip->last_type = dtrans->is_write;
  
  /*
   * Schedule the event to call DRAM_bank_done when this access is done.
   */
  pbank->busy = dtrans;
  schedule_event(pevent, prdbus->busy_until);
}



/*
 * Direct Rambus DRAM. Please consult RDRAM documents for the 
 * details on RDRAM timing.
 */
void DRAM_access_RDRAM_bank(dram_info_t *pdb, dram_trans_t *dtrans,
			    rsim_time_t starttime)
{
  mmc_trans_t   *ptrans    = dtrans->ptrans;
  unsigned       paddr     = dtrans->paddr;
  unsigned       size      = dtrans->size;
  int            row_num   = paddr >> dparam.row_shift;
  dram_bank_t   *pbank     = DRAM_paddr_to_bank(pdb, paddr);
  dram_rd_bus_t *prdbus    = DRAM_bankid_to_rd_bus(pdb, pbank->id);
  dram_chip_t   *pchip     = pbank->chip;
  EVENT         *pevent    = pbank->pevent;
  int            cas_count = size >> dparam.width_shift;
  int            hit_row;
  rsim_time_t    cas_busy, bus_busy, bank_start, start_time, wcycles;
   
  /*
   * First, find out if it hits/misses on the hot row.
   */
  if (row_num == pbank->hot_row && pbank->expire > starttime) 
    hit_row = 1;
  else
    hit_row = 0;

  pbank->hitmiss = (pbank->hitmiss << 1) | (row_num == pbank->hot_row);

  /* 
   * Find the earliest free time for RAS bus. (Figure 13/14, p 28,29.)
   */
  if (pchip->last_bank == pbank->id)
    {
      pchip->ras_busy += dparam.dtime.r.RC - dparam.dtime.r.PACKET;
    }
  else
    {
      pchip->ras_busy += dparam.dtime.r.RR;
    }
  
  /* Need precharge */ 
  if ((hit_row == 0) && (pbank->expire > starttime))
    {
      if (pchip->ras_busy < starttime + dparam.dtime.r.RP)
	pchip->ras_busy = starttime + dparam.dtime.r.RP;
    }

  
  /* 
   * Find the earliest free time for CAS/DATA bus. (Figure 19/20)
   */
  if (pchip->last_type == DRAM_READ && dtrans->is_write)
    {
      pchip->cas_busy    += dparam.dtime.r.CBUB1;
      prdbus->busy_until += dparam.dtime.r.CBUB1;
    }
  else if (pchip->last_type == DRAM_WRITE &&
	   !dtrans->is_write &&
	   pchip->last_bank == pbank->id)
    {
      pchip->cas_busy    += dparam.dtime.r.CBUB2;
      prdbus->busy_until += dparam.dtime.r.CBUB2;
    }

  /* 
   * Find the earliest time to start the access.
   */
  if (hit_row)
    {
      bus_busy = prdbus->busy_until - dparam.dtime.r.PACKET; 
      bank_start = MAX(pchip->cas_busy, starttime);
    }
  else
    {
      cas_busy = pchip->cas_busy - dparam.dtime.r.RCD;
      bus_busy = prdbus->busy_until - 
	(dparam.dtime.r.RCD + dparam.dtime.r.PACKET);
      bank_start = MAX(MAX(pchip->ras_busy, cas_busy), starttime);
    }
  
  if (dtrans->is_write)
    bus_busy -= dparam.dtime.r.CWD;
  else
    bus_busy -= dparam.dtime.r.CAC;

  start_time = MAX(bank_start, bus_busy);

  /*
   * Collect statistics.
   */
  if (dparam.collect_stats)
    {
      prdbus->count++;
      prdbus->cycles += dparam.dtime.r.PACKET * cas_count;
      wcycles = bus_busy - bank_start;
      if (start_time == bus_busy && wcycles > 0)
	{
	  prdbus->waits++;
	  prdbus->wcycles += wcycles;
	  prdbus->cycles  += wcycles;
	}
      
      if (hit_row)
	{
	  if (dtrans->is_write)
	    pbank->stats.write_hits++;
	  else  
	    pbank->stats.read_hits++;
	}
      else if (pbank->expire > starttime)
	{
	  if (dtrans->is_write)
	    pbank->stats.write_misses++;
	  else  
	    pbank->stats.read_misses++;
	}

      pbank->stats.queue_cycles += start_time - dtrans->time;
      dtrans->time = start_time;
      if (start_time < prdbus->busy_until)
	pbank->stats.overlap += prdbus->busy_until - start_time;
    }

  /*
   * Update the states of RAS/CAS/DATA busses.
   * 
   */
  if (hit_row)
    {
      pchip->cas_busy    = start_time + dparam.dtime.r.PACKET * cas_count;
      prdbus->busy_until = pchip->cas_busy + dparam.dtime.r.PACKET;
    }
  else
    {
      pchip->ras_busy    = start_time + dparam.dtime.r.PACKET;
      pchip->cas_busy    = start_time + dparam.dtime.r.RCD +
	dparam.dtime.r.PACKET * cas_count;
      prdbus->busy_until = pchip->cas_busy + dparam.dtime.r.PACKET;
    }
  
  if (dtrans->is_write)
    prdbus->busy_until += dparam.dtime.r.CWD;
  else
    prdbus->busy_until += dparam.dtime.r.CAC;

  /*   
   * Use RDA/WRA/RD-PRES command to avoid PRE command. (Figure 20)
   * So the timing for closing the hot row and leaving it open
   * is the same. Just record the hot row state.
   */
  pbank->hot_row = row_num;
  if (dtrans->open_row)
    {
      pbank->expire  = starttime + dparam.row_hold_time;
    }
  else
    {
      pbank->expire  = 0;
    }

  pchip->last_bank = pbank->id;
  pchip->last_type = dtrans->is_write;

  /*
   * Schedule the event to call DRAM_bank_done when the access is done.
   */
  pbank->busy = dtrans;
  schedule_event(pevent, prdbus->busy_until);
}



/*
 * Called when a dram access is done.
 */
void DRAM_bank_done(void)
{
  dram_info_t    *pdb      = YS__ActEvnt->uptr1;
  dram_bank_t    *pbank    = YS__ActEvnt->uptr2;
  dram_databuf_t *pdatabuf = YS__ActEvnt->uptr3;
  dram_trans_t   *dtrans   = pbank->busy;

  if (dparam.collect_stats)
    {
      pbank->stats.readwrites++;
      pbank->stats.access_cycles += YS__Simtime - dtrans->time;
      dtrans->time = YS__Simtime;
    }

  /*
   * The next step is going throught data buffer (Accumulate/Mux chip).
   * - If the buffer is busy, wait.
   * - otherwise, send the data back to MMC if the access is a read
   *   and terminate this transaction if the access is a write.
   */
  if (pdatabuf->busy)
    {
      if (cbuf_full(pdatabuf->waiters))
	YS__errmsg(pdb->nodeid, "pdatabuf->waiters are full\n");
      cbuf_enqueue(pdatabuf->waiters, dtrans);
    }
  else
    {
      DRAM_sd_bus_schedule(pdb, pdatabuf, dtrans, 0);
    }

  
  /*
   * Find out the next action should be taken on this bank.
   *  1. Refreshing always has the highest priority.
   *  2. If the waiting queue is not empty, grant the bank to the first
   *     transaction in the waiting queue.
   *  3. Otherwise, keep the bank idle.
   */
  if (pbank->chip->refresh_needed)
    {
      DRAM_refresh();
    }
  else if (pbank->waiters.size > 0)
    {
      dram_trans_t *dtrans = Bank_queue_dequeue(pbank);
      pdb->total_bwaiters--;
      
      DRAM_access_bank(pdb, dtrans, MAX(dtrans->time, pbank->chip->ras_busy));
    }
  else
    {
      pbank->busy = 0;
    }
}



void DRAM_sd_bus_schedule(dram_info_t *pdb, dram_databuf_t *pdatabuf, 
			  dram_trans_t *dtrans, int waited)
{
  double         data_ready;

  if (dparam.collect_stats && YS__Simtime > dtrans->time)
    {
      pdb->sd_bus.waits++;
      pdb->sd_bus.wcycles += YS__Simtime - dtrans->time;
    }

  
  /* 
   * The time on SD bus is decided by the size of the access.
   */
  if (pdb->sd_bus.busy_until < YS__Simtime)
    pdb->sd_bus.busy_until = YS__Simtime;

  
  data_ready = pdb->sd_bus.busy_until + dparam.sd_bus_cycles; 

  if (dtrans->size <= dparam.sd_bus_width)
    pdb->sd_bus.busy_until += dparam.sd_bus_cycles;
  else
    pdb->sd_bus.busy_until += dparam.sd_bus_cycles * 
      (dtrans->size >> dparam.sd_bus_shift);


  /* 
   * Call DRAM_databuf_done when the transfer on the SD bus is done.
   */
  pdatabuf->busy = dtrans;
  pdatabuf->cevent->uptr3 = &(pdb->sd_bus);
  pdatabuf->devent->uptr3 = &(pdb->sd_bus);
  if (dparam.critical_word)
    {
      schedule_event(pdatabuf->devent, data_ready);
    }
  else
    {
      schedule_event(pdatabuf->devent, pdb->sd_bus.busy_until);
    }
  schedule_event(pdatabuf->cevent, pdb->sd_bus.busy_until);
}





void DRAM_databuf_data_ready(void)
{
  dram_info_t    *pdb      = YS__ActEvnt->uptr1;
  dram_databuf_t *pdatabuf = YS__ActEvnt->uptr2;
  dram_trans_t   *dtrans   = pdatabuf->busy;

  /*
   * Inform the MMC that the dram access is done.
   */

  MMC_dram_data_ready(pdb->nodeid, dtrans->ptrans);
}




/*
 * Called when a transaction comes off the SD bus, i.e data has been sent 
 * back to MMC.
 */
void DRAM_databuf_done(void)
{
  dram_info_t    *pdb      = YS__ActEvnt->uptr1;
  dram_databuf_t *pdatabuf = YS__ActEvnt->uptr2;
  dram_sd_bus_t  *psdbus   = YS__ActEvnt->uptr3;
  dram_trans_t   *dtrans   = pdatabuf->busy;

  /* 
   * At this point, a DRAM transaction is considered to be done.
   */
  DRAM_end_transaction(pdb, dtrans);

  if (dparam.collect_stats)
    {
      psdbus->count++;
      psdbus->cycles += YS__Simtime - dtrans->time;
      dtrans->time    = YS__Simtime;
    }

  /*
   * If any DRAM transactions are contenting for this SD bus, 
   * pick up the the head of the waiting queue as the next winner.
   */
  if (cbuf_empty(pdatabuf->waiters))
    pdatabuf->busy = 0;
  else
    {
      dram_trans_t *ndtrans = (dram_trans_t *)cbuf_dequeue(pdatabuf->waiters);
      DRAM_sd_bus_schedule(pdb, pdatabuf, ndtrans, 1);
    }

  /*
   * Inform the MMC that the dram access is done.
   */
  MMC_dram_done(pdb->nodeid, dtrans->ptrans);
}



/*
 * Insert a DRAM access into the bank waiting queue.
 */
void Bank_queue_enqueue(bank_queue_t *bq, dram_trans_t *dtrans)
{
  bank_queue_elm_t *qelm, *cur, *next;

  if (bq->size == bq->max)
    YS__errmsg(0, "The waiting queue on bank is full\n");

  qelm       = bq->free;
  bq->free   = qelm->next;
  qelm->data = (void *) dtrans;

  if (bq->size++ == 0)
    {
      /* 
       * The waiting queue is empty 
       */
      qelm->prev = qelm->next = qelm;
      bq->head   = bq->tail   = qelm;
    }
  else
    {
      /* 
       * Insert the transaction at the tail. 
       */
      qelm->prev     = bq->tail;  
      bq->tail->next = qelm;                                               
      qelm->next     = bq->head;
      bq->head->prev = qelm;
      bq->tail       = qelm;
    }
}



#define DRAM_STATUS_BITS 5

/*
 * Dequeue the head of the bank waiting queue. Also, find out
 * if the hot row should remain open after this access.
 */
dram_trans_t * Bank_queue_dequeue(dram_bank_t *pbank)
{
  bank_queue_t     *bq = &(pbank->waiters);
  bank_queue_elm_t *qelm;
  dram_trans_t     *dtrans;

  if (bq->size == 0)
    YS__errmsg(0, "The waiting queue is empty\n");

  /*
   * Dequeue the head of the waiting queue.
   */
  if (--bq->size == 0)
    {
      qelm     = bq->head;
      bq->head = bq->tail = 0;
    }
  else
    {
      qelm = bq->head;

      bq->head = bq->head->next;
      bq->head->prev = bq->tail;
      bq->tail->next = bq->head;
    }
  
  dtrans     = qelm->data;
  qelm->next = bq->free;
  bq->free   = qelm;

  /*
   * Check the hot row hit/miss history to decide whether or not to leave
   * the hot row open after the access is done.
   * - hot_row_policy == 0, always close the hot row (precharge it);
   * - hot_row_policy == 1, always leave the hot row open;
   * - hot_row_policy == 2, use predictors. Please see technical report
   *   UU-CS-XXXX for details on the predicting-policy.
   *   * If the next transaction will access the same row, leave the hot row
   *     open after this access and lock the next one in the head.
   *   * Otherwise, use the selected hot-row algorithm.
   */
  switch (dparam.hot_row_policy)
    {
    case 0:
      dtrans->open_row = 0;
      break;
    case 1:
      dtrans->open_row = 1;
      break;
    case 2:
      if (bq->size && 
	  DRAM_in_same_row(dtrans->paddr, bq->head->data->paddr))
	{
	  dtrans->open_row = 1;
	  bq->head->data->locked = 1;
	}
      else
	{
	  int   i, count, history;
	  history = pbank->hitmiss;
	  for (i = count = 0; i < DRAM_STATUS_BITS; i++)
	    {
	      if (history & 1)
		count++;
	      history >>= 1;
	    }

	  dtrans->open_row = (count > (DRAM_STATUS_BITS >> 1));
	}
      break;
    default:
      YS__errmsg(0, "Wrong hot_row_policy\n");
    }

  return dtrans;
}



/*
 * Create a new DRAM transaction.
 */
dram_trans_t * DRAM_new_transaction(dram_info_t *pdb, mmc_trans_t *ptrans,
				    unsigned paddr, int size, int is_write)
{
  dram_trans_t *dtrans;

  if (size < dparam.mini_access)
    size = dparam.mini_access;

  if (pdb->freedramlist.size == 0)
    YS__errmsg(pdb->nodeid, "No free dram transaction available.\n");

  lqueue_get(&pdb->freedramlist, dtrans);

  dtrans->ptrans      = ptrans;
  dtrans->time        = YS__Simtime;
  dtrans->etime       = YS__Simtime;
  dtrans->paddr       = paddr;
  dtrans->size        = size;
  dtrans->locked      = 0;
  dtrans->is_write    = is_write;

  return dtrans;
}



/*
 * A DRAM transaction is done. Put it back to the free list.
 */
void DRAM_end_transaction(dram_info_t *pdb, dram_trans_t *dtrans)
{
  if (dparam.collect_stats)
    {
      pdb->total_count  += 1;
      pdb->total_cycles += YS__Simtime - dtrans->etime;
      pdb->sgle_count  += 1;
      pdb->sgle_cycles += YS__Simtime - dtrans->etime;
    }
  
  lqueue_add(&pdb->freedramlist, dtrans, pdb->nodeid);
}



/*
 * Used when the DRAM backend simulator is turned off. When the DRAM 
 * backend simulator is not used, each DRAM access takes a fixed latency
 * (specified by dparam.latency in terms of dparam.frequency cycles).
 */
void DRAM_nosim_start(dram_info_t *pdb, dram_trans_t *dtrans)
{
  if (IsNotScheduled(pdb->pevent))
    {
      pdb->pevent->uptr2 = dtrans;
      schedule_event(pdb->pevent,
		     YS__Simtime + (dparam.latency * dparam.frequency));
    }
  else
    {
      lqueue_add(&(pdb->waitlist), dtrans, pdb->nodeid);
    }
}



/*
 * Activated by pdb->pevent when a DRAM access has been delayed for the
 * fixed latency. Used when the DRAM backend simulator is turned off.
 */
void  DRAM_nosim_done(void)
{
  dram_info_t  *pdb    = YS__ActEvnt->uptr1;
  dram_trans_t *dtrans = YS__ActEvnt->uptr2;

  /* 
   * At this point, a DRAM transaction is considered to be done.
   */
  DRAM_end_transaction(pdb, dtrans);

  /*
   * Handle the next access, if any.
   */
  if (pdb->waitlist.size > 0)
    {
      dram_trans_t *new_dtrans;
      lqueue_get(&(pdb->waitlist), new_dtrans);

      pdb->pevent->uptr2 = new_dtrans;
      schedule_event(pdb->pevent,
		     YS__Simtime + (dparam.latency * dparam.frequency));
    }

  /*
   * Inform the MMC that the dram access is done.
   */
  MMC_dram_done(pdb->nodeid, dtrans->ptrans);
}







