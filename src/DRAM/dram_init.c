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
#include <string.h>
#include <strings.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "Memory/mmc.h"
#include "DRAM/cqueue.h"
#include "DRAM/dram_param.h"
#include "DRAM/dram.h"


/* 
 * Define global variables here. 
 */
dram_info_t  *DBs;      /* Per processor information of DRAM-backend */
dram_param_t  dparam;   /* DRAM-backend parameters */


#ifdef TRACEDRAM
 char     dram_trace_file[128];
 FILE    *dram_trace_fp;
 unsigned dram_trace_count = 0;
#endif



/*
 * Read DRAM-backend-related parameters from the designated parameter file.
 */
void DRAM_read_params(void)
{
  char  tmpbuf[256];

  /* 
   * Switches for debugging/tracing support.
   */
  dparam.sim_on         = 1;
  dparam.latency        = 18;
  dparam.frequency      = 1;
  dparam.scheduler_on   = 1;
  dparam.debug_on       = 0;
  dparam.collect_stats  = 1;
  dparam.trace_on       = 0;
  dparam.trace_max      = 0;

  get_parameter("DRAM_sim_on",        &dparam.sim_on,        PARAM_INT);
  get_parameter("DRAM_latency",       &dparam.latency,       PARAM_INT);
  get_parameter("DRAM_frequency",     &dparam.frequency,     PARAM_INT);
  get_parameter("DRAM_scheduler",     &dparam.scheduler_on,  PARAM_INT);
  get_parameter("DRAM_debug",         &dparam.debug_on,      PARAM_INT);
  get_parameter("DRAM_collect_stats", &dparam.collect_stats, PARAM_INT);
  get_parameter("DRAM_trace_on",      &dparam.trace_on,      PARAM_INT);
  get_parameter("DRAM_trace_max",     &dparam.trace_max,     PARAM_INT);
  
  if (MMC_sim_on() == 0)
    {
      dparam.sim_on = 0;
      dparam.collect_stats = 0;
    }

#ifdef TRACEDRAM
  if (dparam.trace_on)
    {
      if (!get_parameter("DRAM_trace_file", dram_trace_file, PARAM_STRING))
	strcpy(dram_trace_file, "dram_trace");

      if (!(dram_trace_fp = fopen(dram_trace_file, "w")))
	YS__errmsg(0, "Couldn't open dram trace file %s\n", dram_trace_file);
    }
#endif

  /* 
   * DRAM backend organization.
   */
  dparam.sa_bus_cycles  = DRAM_SA_BUS_CYCLES;
  dparam.sd_bus_cycles  = DRAM_SD_BUS_CYCLES;
  dparam.sd_bus_width   = DRAM_SD_BUS_WIDTH;
  dparam.rd_busses      = DRAM_RD_BUSSES;
  dparam.num_databufs   = DRAM_NUM_DATABUFS;
  dparam.critical_word  = DRAM_CRITICAL_WORD;
  dparam.banks_per_chip = DRAM_BANKS_PER_CHIP;
  dparam.num_banks      = DRAM_NUM_BANKS;
  dparam.bank_depth     = DRAM_BANK_DEPTH;
  dparam.interleaving   = 0;
  dparam.max_bwaiters   = dparam.bank_depth * dparam.num_banks;

  get_parameter("DRAM_sa_bus_cycles",  &dparam.sa_bus_cycles,  PARAM_INT);
  get_parameter("DRAM_sd_bus_cycles",  &dparam.sd_bus_cycles,  PARAM_INT);
  get_parameter("DRAM_sd_bus_width",   &dparam.sd_bus_width,   PARAM_INT);
  get_parameter("DRAM_num_smcs",       &dparam.rd_busses,      PARAM_INT);
  get_parameter("DRAM_num_databufs",   &dparam.num_databufs,   PARAM_INT);
  get_parameter("DRAM_critical_word",  &dparam.critical_word,  PARAM_INT);
  get_parameter("DRAM_num_banks",      &dparam.num_banks,      PARAM_INT);
  get_parameter("DRAM_banks_per_chip", &dparam.banks_per_chip, PARAM_INT);
  get_parameter("DRAM_bank_depth",     &dparam.bank_depth,     PARAM_INT);
  get_parameter("DRAM_interleaving",   &dparam.interleaving,   PARAM_INT);
  get_parameter("DRAM_max_bwaiters",   &dparam.max_bwaiters,   PARAM_INT);

  if (dparam.rd_busses > DRAM_MAX_RD_BUSSES)
    YS__errmsg(0, "Too many smcs: %d (DRAM_init)\n", dparam.rd_busses);

  if (dparam.num_databufs > DRAM_MAX_DATABUFS)
    YS__errmsg(0, "Too many data buffers: %d (DRAM_init)\n", dparam.num_databufs);

  if (dparam.num_banks > DRAM_MAX_BANKS)
    YS__errmsg(0, "Too many banks: %d (DRAM_init)\n", dparam.num_banks);

  if (dparam.num_banks % dparam.rd_busses)
    YS__errmsg(0, "num_banks(%d) %% num_smcs(%d) != 0 (DRAM_init)\n",
	       dparam.num_banks, dparam.rd_busses);

  /* 
   * DRAM bank parameters 
   */
  dparam.dram_type      = SDRAM;
  dparam.hot_row_policy = 0;
  dparam.block_size     = DRAM_BLOCK_SIZE;
  dparam.mini_access    = DRAM_MINI_ACCESS;
  dparam.width          = DRAM_WIDTH;
  dparam.row_size       = DRAM_ROW_SIZE;
  dparam.row_hold_time  = DRAM_ROW_HOLD_TIME;
  dparam.refresh_delay  = DRAM_REFRESH_CYCLES;
  dparam.refresh_period = DRAM_REFRESH_PERIOD;
  
  if (get_parameter("DRAM_type", tmpbuf, PARAM_STRING))
    {
      if (!strcmp(tmpbuf, "SDRAM"))
	dparam.dram_type = SDRAM;
      else if (!strcmp(tmpbuf, "RDRAM"))
	dparam.dram_type = RDRAM;
      else
	YS__errmsg(0, "Unknown DRAM type %s\n", tmpbuf);
    }
  
  get_parameter("DRAM_hot_row_policy",   &dparam.hot_row_policy, PARAM_INT);
  get_parameter("DRAM_width",            &dparam.width,          PARAM_INT);
  get_parameter("DRAM_mini_access",      &dparam.mini_access,    PARAM_INT);
  get_parameter("DRAM_block_size",       &dparam.block_size,     PARAM_INT);

  if (dparam.dram_type == SDRAM)
    {
      dparam.dtime.s.CCD    = SDRAM_tCCD;
      dparam.dtime.s.RRD    = SDRAM_tRRD;
      dparam.dtime.s.RP     = SDRAM_tRP;
      dparam.dtime.s.RAS    = SDRAM_tRAS;
      dparam.dtime.s.RCD    = SDRAM_tRCD;
      dparam.dtime.s.AA     = SDRAM_tAA;
      dparam.dtime.s.DAL    = SDRAM_tDAL;
      dparam.dtime.s.DPL    = SDRAM_tDPL;
      dparam.dtime.s.PACKET = SDRAM_tPACKET;
      
      get_parameter("SDRAM_tCCD",    &dparam.dtime.s.CCD,    PARAM_INT);
      get_parameter("SDRAM_tRRD",    &dparam.dtime.s.RRD,    PARAM_INT);
      get_parameter("SDRAM_tRP",     &dparam.dtime.s.RP,     PARAM_INT);
      get_parameter("SDRAM_tRAS",    &dparam.dtime.s.RAS,    PARAM_INT);
      get_parameter("SDRAM_tRCD",    &dparam.dtime.s.RCD,    PARAM_INT);
      get_parameter("SDRAM_tAA",     &dparam.dtime.s.AA,     PARAM_INT);
      get_parameter("SDRAM_tDAL",    &dparam.dtime.s.DAL,    PARAM_INT);
      get_parameter("SDRAM_tDPL",    &dparam.dtime.s.DPL,    PARAM_INT);
      get_parameter("SDRAM_tPACKET", &dparam.dtime.s.PACKET, PARAM_INT);

      get_parameter("SDRAM_row_size",      &dparam.row_size,       PARAM_INT);
      get_parameter("SDRAM_row_hold_time", &dparam.row_hold_time,  PARAM_INT);
      get_parameter("SDRAM_refresh_delay", &dparam.refresh_delay,  PARAM_INT);
      get_parameter("SDRAM_refresh_period",&dparam.refresh_period, PARAM_INT);
    }
  else
    {
      dparam.dtime.r.PACKET = RDRAM_tPACKET;
      dparam.dtime.r.RC     = RDRAM_tRC;
      dparam.dtime.r.RR     = RDRAM_tRR;
      dparam.dtime.r.RP     = RDRAM_tRP;
      dparam.dtime.r.CBUB1  = RDRAM_tCBUB1;
      dparam.dtime.r.CBUB2  = RDRAM_tCBUB2;
      dparam.dtime.r.RCD    = RDRAM_tRCD;
      dparam.dtime.r.CAC    = RDRAM_tCAC;
      dparam.dtime.r.CWD    = RDRAM_tCWD;

      get_parameter("RDRAM_tPACKET", &dparam.dtime.r.PACKET, PARAM_INT);
      get_parameter("RDRAM_tRC",     &dparam.dtime.r.RC,     PARAM_INT);
      get_parameter("RDRAM_tRR",     &dparam.dtime.r.RR,     PARAM_INT);
      get_parameter("RDRAM_tRP",     &dparam.dtime.r.RP,     PARAM_INT);
      get_parameter("RDRAM_tCBUB1",  &dparam.dtime.r.CBUB1,  PARAM_INT);
      get_parameter("RDRAM_tCBUB2",  &dparam.dtime.r.CBUB2,  PARAM_INT);
      get_parameter("RDRAM_tRCD",    &dparam.dtime.r.RCD,    PARAM_INT);
      get_parameter("RDRAM_tCAC",    &dparam.dtime.r.CAC,    PARAM_INT);
      get_parameter("RDRAM_tCWD",    &dparam.dtime.r.CWD,    PARAM_INT);

      get_parameter("RDRAM_row_size",      &dparam.row_size,       PARAM_INT);
      get_parameter("RDRAM_row_hold_time", &dparam.row_hold_time,  PARAM_INT);
      get_parameter("RDRAM_refresh_delay", &dparam.refresh_delay,  PARAM_INT);
      get_parameter("RDRAM_refresh_period",&dparam.refresh_period, PARAM_INT);
    }

  
  /* 
   * The following variables are used to simplify arithmetic operations
   * into bit operations. 
   */
  dparam.num_chips    = dparam.num_banks / dparam.banks_per_chip;
  dparam.width_shift  = NumOfBits(dparam.width, 1);
  dparam.row_shift    = NumOfBits(dparam.row_size * dparam.num_banks, 1);
  dparam.sd_bus_shift = NumOfBits(dparam.sd_bus_width, 1);

  if (!(dparam.interleaving & 1)) /* 0 or 2*/
    dparam.bank_shift = NumOfBits(dparam.block_size, 1);
  else /* 1 or 3 */
    dparam.bank_shift = NumOfBits(dparam.row_size, 1);

  dparam.bank_mask     = dparam.num_banks - 1;
  dparam.chip_shift    = NumOfBits(dparam.banks_per_chip, 1);
  dparam.chip_mask     = dparam.num_chips - 1;
  dparam.rd_bus_shift  = NumOfBits(dparam.num_banks / dparam.rd_busses, 1);
  dparam.rd_bus_mask   = dparam.rd_busses - 1;
  dparam.databuf_shift = NumOfBits(dparam.num_banks / dparam.num_databufs, 1);
  dparam.databuf_mask  = dparam.num_databufs - 1;
  
  /*
   * Convert DRAM cycles into simulation cycles.
   */

#if 0
  dparam.sa_bus_cycles  *= dparam.frequency;
  dparam.sd_bus_cycles  *= dparam.frequency;
#endif

  dparam.row_hold_time  *= dparam.frequency;
  dparam.refresh_delay  *= dparam.frequency;
  dparam.refresh_period *= dparam.frequency;

  if (dparam.dram_type == SDRAM)
    {
      dparam.dtime.s.CCD    *= dparam.frequency;
      dparam.dtime.s.RRD    *= dparam.frequency;
      dparam.dtime.s.RP     *= dparam.frequency;
      dparam.dtime.s.RAS    *= dparam.frequency;
      dparam.dtime.s.RCD    *= dparam.frequency;
      dparam.dtime.s.AA     *= dparam.frequency;
      dparam.dtime.s.DAL    *= dparam.frequency;
      dparam.dtime.s.DPL    *= dparam.frequency;
      dparam.dtime.s.PACKET *= dparam.frequency;

      dparam.co2d_cycles = dparam.dtime.s.CCD + dparam.dtime.s.RAS + 
	dparam.dtime.s.RCD + dparam.dtime.s.AA + 
	dparam.dtime.s.PACKET;
    }
  else
    {
      dparam.dtime.r.PACKET *= dparam.frequency;
      dparam.dtime.r.RC     *= dparam.frequency;
      dparam.dtime.r.RR     *= dparam.frequency;
      dparam.dtime.r.RP     *= dparam.frequency;
      dparam.dtime.r.CBUB1  *= dparam.frequency;
      dparam.dtime.r.CBUB2  *= dparam.frequency;
      dparam.dtime.r.RCD    *= dparam.frequency;
      dparam.dtime.r.CAC    *= dparam.frequency;
      dparam.dtime.r.CWD    *= dparam.frequency;

      dparam.co2d_cycles = dparam.dtime.r.RCD + dparam.dtime.r.PACKET + 
	dparam.dtime.r.CAC + dparam.dtime.r.RP;
    }
}



/*
 * Every bank is associated with a prioritized waiting queue, and a worker 
 * event which schedules transactions in waiting queue.
 */
void DRAM_bank_init(dram_info_t *pdb, int bid)
{
  dram_bank_t      *pbank = &(pdb->banks[bid]);
  bank_queue_t     *dq    = &(pbank->waiters);
  bank_queue_elm_t *qelms;
  int               i;

  pbank->id        = bid;
  pbank->busy	    = 0;
  pbank->expire    = 0;
  pbank->hitmiss   = 0;
  pbank->last_type = DRAM_READ;
  pbank->chip      = DRAM_bankid_to_chip(pdb, bid);

  /*
   * Initialize the waiting queue.
   */
  if (!(qelms = RSIM_CALLOC(bank_queue_elm_t, dparam.bank_depth)))
    YS__errmsg(0, "malloc failed at %s:%i", __FILE__, __LINE__);

  dq->max  = dparam.bank_depth;
  dq->size = 0;
  dq->head = dq->tail = 0;
  dq->free = 0;
  for (i = 0; i < dq->max; i++)
    {
      qelms[i].next = dq->free;
      dq->free = &(qelms[i]);
    }

  pbank->pevent        = NewEvent("bank", DRAM_bank_done, NODELETE, 0);
  pbank->pevent->uptr1 = pdb;
  pbank->pevent->uptr2 = pbank;
  pbank->pevent->uptr3 = DRAM_bankid_to_databuf(pdb, bid);
}



/*
 * This function must be called before any other functions of 
 * the DRAM module can be used.
 */
void DRAM_init(void)
{
  int           i, j, k;
  dram_info_t   *pdb;
  dram_trans_t  *dtrans;

  /*
   * Read parameters from the designated parameter file.
   */
  DRAM_read_params();

  if (!(DBs = RSIM_CALLOC(dram_info_t, ARCH_numnodes)))
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < ARCH_numnodes; i++)
    {
      pdb         = &(DBs[i]);
      pdb->nodeid = i;

      /* 
       * Queue up all the dram transactions into a free list.
       */
      if (!(dtrans = RSIM_CALLOC(dram_trans_t, MAX_TRANS * DRAM_MAX_FACTOR)))
	YS__errmsg(i, "Malloc failed at %s:%i", __FILE__, __LINE__);

      lqueue_init(&pdb->freedramlist, MAX_TRANS * DRAM_MAX_FACTOR);
      for (j = 0; j < MAX_TRANS * DRAM_MAX_FACTOR; j++)
	{
	  lqueue_add(&pdb->freedramlist, dtrans, i);
	  dtrans++;
	}

      /*
       * The DRAM backend simulator is not on, we only need a waiting list
       * and an event.
       */
      if (dparam.sim_on == 0)
	{
	  lqueue_init(&pdb->waitlist, MAX_TRANS * DRAM_MAX_FACTOR);
	  pdb->pevent = NewEvent("sabus", DRAM_nosim_done, NODELETE, 0);
	  pdb->pevent->uptr1 = pdb;
	  continue;
	}

      /*
       * Initialize the Slave Address bus.
       */
      pdb->sa_bus.pevent = NewEvent("sabus", DRAM_arrive_smc, NODELETE, 0);
      pdb->sa_bus.pevent->uptr1 = pdb;
      pdb->sa_bus.pevent->uptr2 = &(pdb->sa_bus);

      pdb->sa_bus.waiters = cbuf_alloc(1024 * DRAM_MAX_FACTOR);
      pdb->sa_bus.busy    = 0;
      pdb->sa_bus.count   = 0;
      pdb->sa_bus.waits   = 0;
      pdb->sa_bus.cycles  = 0;
      pdb->sa_bus.wcycles = 0;

      /*
       * Initialize the Slave Data bus.
       */
      pdb->sd_bus.busy_until = 0;
      pdb->sd_bus.count      = 0;
      pdb->sd_bus.waits      = 0;
      pdb->sd_bus.cycles     = 0;
      pdb->sd_bus.wcycles    = 0;
      
      /*
       * Initialize the RD busses
       */
      if (!(pdb->rd_busses = RSIM_CALLOC(dram_rd_bus_t, dparam.rd_busses)))
	YS__errmsg(i, "Malloc failed at %s:%i", __FILE__, __LINE__);

      for (j = 0; j < dparam.rd_busses; j++)
	{
	  dram_rd_bus_t *prdbus = &(pdb->rd_busses[j]);

	  prdbus->busy_until = 0;
	  prdbus->count      = 0;
	  prdbus->waits      = 0;
	  prdbus->cycles     = 0;
	  prdbus->wcycles    = 0;
	}

      /*
       * Initialize the data buffers (Accumulate/Mux Chips).
       */
      if (!(pdb->databufs = RSIM_CALLOC(dram_databuf_t, dparam.num_databufs)))
	YS__errmsg(i, "Malloc failed at %s:%i", __FILE__, __LINE__);

      for (j = 0; j < dparam.num_databufs; j++)
	{
	  dram_databuf_t *pdatabuf = &(pdb->databufs[j]);
	  
	  pdatabuf->cevent = NewEvent("data buffer done",
				     DRAM_databuf_done, NODELETE, 0);
	  pdatabuf->cevent->uptr1 = pdb;
	  pdatabuf->cevent->uptr2 = pdatabuf;

	  pdatabuf->devent = NewEvent("data buffer data ready",
				     DRAM_databuf_data_ready, NODELETE, 0);
	  pdatabuf->devent->uptr1 = pdb;
	  pdatabuf->devent->uptr2 = pdatabuf;

	  pdatabuf->waiters       = cbuf_alloc(1024);
	  pdatabuf->busy          = 0;
	}

      /*
       * Initialize the chips. Each chip has a refreshing event which 
       * efreshs the bank periodically.
       */
      if (!(pdb->chips = RSIM_CALLOC(dram_chip_t, dparam.num_chips)))
	YS__errmsg(i, "Malloc failed at %s:%i", __FILE__, __LINE__);

      for (j = 0; j < dparam.num_chips; j++)
	{
	  dram_chip_t *pchip = &(pdb->chips[j]);

	  pchip->id        = j;
	  pchip->ras_busy  = 0;
	  pchip->cas_busy  = 0;
	  pchip->last_type = DRAM_READ;
	  pchip->last_bank = 0;

	  pchip->refresh_event = NewEvent("chip", DRAM_refresh, NODELETE, 0);
	  pchip->refresh_event->uptr1 = pdb;
	  pchip->refresh_event->uptr2 = pchip;
	  pchip->refresh_event->ival1 = j;
	  pchip->refresh_on           = 0;
	  pchip->refresh_needed       = 0;
	}

      /*
       * Initialize the banks.
       */
      if (!(pdb->banks = RSIM_CALLOC(dram_bank_t, dparam.num_banks)))
	YS__errmsg(i, "Malloc failed at %s:%i", __FILE__, __LINE__);

      for (k = 0; k < dparam.num_banks; k++) 
	DRAM_bank_init(pdb, k);

      pdb->total_bwaiters = 0;
      pdb->total_count = 0;
      pdb->total_cycles = 0;
    }
}



/*
 * Circular Buffers
 */
circbuf_t *cbuf_alloc(int size)
{
  int        bytes   = sizeof(circbuf_t) + ((size - 1) * sizeof(long));
  circbuf_t *the_q   = (circbuf_t *) malloc(bytes);
  int        logsize = ToPowerOf2(size);

  if (logsize != size)
    YS__errmsg(0, "cbuf_alloc: Must be a power of 2");

  if (!the_q)
    YS__errmsg(0, "cbuf_alloc: malloc failure");

  bzero(the_q, bytes);
  the_q->mask = size - 1;
  the_q->size = size;
  return the_q;
}
