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

#ifndef __DRAM_PARAM_H__
#define __DRAM_PARAM_H__

#define  DRAM_READ	0
#define  DRAM_WRITE	1

/*
 * Memory organization 
 */
#define  DRAM_SA_BUS_CYCLES       1
#define  DRAM_SD_BUS_CYCLES       1
#define  DRAM_SD_BUS_WIDTH        32
#define  DRAM_NUM_DATABUFS    	  2
#define  DRAM_CRITICAL_WORD    	  1
#define  DRAM_BANKS_PER_CHIP   	  2
#define  DRAM_RD_BUSSES    	  4
#define  DRAM_NUM_BANKS        	  16
#define  DRAM_BANK_DEPTH          16

#define  DRAM_MAX_RD_BUSSES	  128
#define  DRAM_MAX_DATABUFS        64
#define  DRAM_MAX_BANKS           1024
#define  DRAM_MAX_FACTOR          8



/*
 * Configuration of DRAM
 */
#define  SDRAM                     1
#define  RDRAM                     2

#define  DRAM_ROW_SIZE             512
#define  DRAM_BLOCK_SIZE           128
#define  DRAM_MINI_ACCESS          16
#define  DRAM_WIDTH                16
#define  DRAM_ROW_HOLD_TIME        750000
#define  DRAM_REFRESH_CYCLES       2048
#define  DRAM_REFRESH_PERIOD       750000

#define  SDRAM_tCCD	1
#define  SDRAM_tRRD	2
#define  SDRAM_tRP	3
#define  SDRAM_tRAS	7
#define  SDRAM_tRCD	3
#define  SDRAM_tAA	3
#define  SDRAM_tDAL	5
#define  SDRAM_tDPL	2
#define  SDRAM_tPACKET	1

#define  RDRAM_tPACKET	4
#define  RDRAM_tRC	28
#define  RDRAM_tRR	8
#define  RDRAM_tRP	8
#define  RDRAM_tCBUB1	4
#define  RDRAM_tCBUB2	8
#define  RDRAM_tRCD	7
#define  RDRAM_tCAC	8
#define  RDRAM_tCWD	6



/* 
 * SDRAM timing 
 */
typedef struct
{
  int    CCD;                                   /* CAS to CAS delay time     */
  int    RRD;                                   /* bank to bank delay time   */
  int    RP;                                    /* precharge time            */
  int    RAS;                                   /* minimum bank active time  */
  int    RCD;                                   /* RAS to CAS delay          */
  int    AA;                                    /* CAS latency               */
  int    DAL;                                   /* Data in to active/Refresh */
  int    DPL;                                   /* Data in to precharge      */
  int    PACKET;                                /* length of CAS/DATA        */
} sdram_timing_t;



/* 
 * RDRAM timing 
 */
typedef struct
{
  int    PACKET;      /* length of ROWA/ROWR/COLC/COLM/COLX packet           */
  int    RC;          /* interval between ACT commands to the same bank      */
  int    RR;          /* RAS-to-RAS time to different banks of same device   */
  int    RP;          /* interval between PRER command and next ROWA command */
  int    CBUB1;       /* bubble between a RD and WR command                  */
  int    CBUB2;       /* bubble between a WR and RD to the same devcice      */
  int    RCD;         /* RAS-to-CAS delay                                    */
  int    CAC;         /* CAS-to-Q read data                                  */
  int    CWD;         /* CAS write delay                                     */
} rdram_timing_t;



typedef struct
{
  /* 
   * simulator-clock-rate / DRAM-clock-rate 
   */
  int    frequency;

  /* 
   * Switches 
   */
  int    sim_on;
  int    latency;
  int    scheduler_on;
  int    debug_on;
  int    collect_stats;
  int    trace_on;
  int    trace_max;

  /* 
   * Memory organization 
   */
  int    sa_bus_cycles;
  int    sd_bus_cycles;
  int    sd_bus_width;
  int    rd_busses;
  int    num_databufs;
  int    critical_word;
  int    banks_per_chip;
  int    num_chips;
  int    num_banks;
  int    bank_depth;
  int    interleaving;
  
  /* 
   * DRAM bank parameter 
   */
  int    hot_row_policy;
  int    row_size;
  int    block_size;
  int    mini_access;
  int    width;
  int    row_hold_time;
  int    refresh_delay;
  int    refresh_period;
  int    dram_type;
  
  union
  {
    sdram_timing_t s;
    rdram_timing_t r;
  } dtime;
  
  int    max_bwaiters;

  /* 
   * Variables to simplify computations 
   */
  int    co2d_cycles;
  int    sd_bus_shift;
  int    bank_shift;
  int    bank_mask;
  int    chip_shift;
  int    chip_mask;
  int    databuf_shift;
  int    databuf_mask;
  int    rd_bus_shift;
  int    rd_bus_mask;
  int    width_shift;
  int    row_shift;
} dram_param_t;


extern dram_param_t dparam;


#ifdef TRACEDRAM
extern FILE     *dram_trace_fp;
extern unsigned  dram_trace_count;   
extern FILE     *dram_trace_fp;                               
#endif

#endif
