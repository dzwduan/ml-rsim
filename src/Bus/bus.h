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

#ifndef __RSIM_BUS_H__
#define __RSIM_BUS_H__

#include <stdio.h>
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"


/*
 * bus states
 */
#define BUS_IDLE		0
#define BUS_ARBITRATING		1
#define BUS_WAKINGUP		2



/*
 * Bus modules (processors or bus coordinator ) and their states
 * In the simulator, bus coordinator is another name for MMC.
 * Each bus module can be either in slave state or in master state.
 * Only one bus module is in master state at one time.
 */
#define SLAVE_STATE		0
#define MASTER_STATE		1
#define PROCESSOR		0
#define IO_MODULE               1
#define COORDINATOR		2


/*
 * Defintion of arbitration waiter. Each processor has at most one waiter
 * at one time.
 */
typedef struct
{
  REQ     *req; /* The request that needs the bus */
  double  time; /* Time to start arbitration */
} ARBWAITER;


typedef long long trans_count_t[BARRIER+1];
typedef long long trans_latency_t[BARRIER+1];
typedef long long subtrans_count_t[REQ_TYPE_MAX];

typedef struct _bus_
{
  double     busy_until;
  int        state;
  int        nodeid;

  /* request currently using the bus */
  REQ       *cur_req;
  int        cur_req_owner;

  /* Outstanding requests. */
  REQ      **req_queue;
  int        req_count;

  /*
   * Arbitration related variables. The extra one is for main memory 
   * controller.
   */
  int       *pstates;                /* states of processor/mmc */
  ARBWAITER *arbwaiters;
  int        arbwaiter_count;
  int        rr_index;               /* to implement round-robin algorithm */
  int        cur_winner;
  int        next_winner;
  REQ       *next_req;
  double     next_start_time;

  double     last_start_time;
  double     last_end_time;

  /* Working bees to perform arbitration and transfer. */
  EVENT     *arbitrator;
  EVENT     *issuer;
  EVENT     *performer;

  int        write_flowcontrol;
  
  FILE      *busfile;

  /* Statistics */
  double            last_clear;
  double            arb_delay;
  
  trans_count_t    *num_trans;
  trans_latency_t  *lat_trans;
  subtrans_count_t *num_subtrans;
} BUS;




/*
 * Exported global variables
 */
extern BUS *AllBusses;
extern int  BUS_ARBDELAY;         /* arbitration delay                       */
extern int  BUS_MINDELAY;         /* minimum delay between address cycles    */
extern int  BUS_WIDTH;            /* bus width in bytes                      */
extern int  BUS_FREQUENCY;        /* owner of current issuing request        */
extern int  BUS_TURNAROUND;       /* turnaround cycles when master changes   */
extern int  BUS_TOTAL_REQUESTS;   /* number of outstanding requests          */
extern int  BUS_CPU_REQUESTS;     /* number of requests per CPU              */
extern int  BUS_IO_REQUESTS;      /* number of requests per IO device        */
extern int  BUS_TRACE_ENABLE;     /* turn bus trace file on/off              */
extern int  BUS_CRITICAL_WORD;    /* turn on critical word first data returns*/


#define PID2BUS(nid)		(&(AllBusses[nid]))


/*
 * Check if processor "pid" is in master state. To simplify identifying 
 * MMC, we use "ARCH_cpus" to represent MMC.
 */
#define InMasterState(nid, pid) (AllBusses[nid].pstates[pid] == MASTER_STATE)

/*
 * Check if bus is free from now to (current_sim_time + "timeinc").
 * The bus is considered free if the following conditions are met:
 *  - Nobody is using the bus at this cycle
 *  - Next bus owner has not been chosen, or
 *  - The next bus owner has been chosen, but it will not start using bus
 *    before "current_sim_time + timeinc". 
 */
#define BusIsIdleUntil(timeinc)   \
	   (IsNotScheduled(pbus->issuer) && (pbus->next_winner == -1 || \
            pbus->next_start_time > YS__Simtime + timeinc))

/*
 * Bus module function definitions
 */
void Bus_init                    (void);
void Bus_main                    (void);

void Bus_start                   (REQ *req, int owner);
void Bus_issuer                  (void);
void Bus_perform                 (void);
void Bus_recv_arb_req            (int, int, REQ *, double start_time);
void Bus_arbitrator              (BUS *pbus);
void Bus_send_writeback          (REQ *);
void Bus_send_writepurge         (REQ *);
void Bus_wakeup_winner           (BUS *pbus);

void Bus_send_request            (REQ *req);
void Bus_send_cohe_request       (REQ *req);
void Bus_send_reply              (REQ *req);

void Bus_send_cohe_response      (REQ *req, int state);
void Bus_send_cohe_completion    (REQ *req);
void Bus_send_cohe_data_response (REQ *req);

void Bus_send_IO_request         (REQ *);
void Bus_send_IO_reply           (REQ *);

void Bus_start_trans             (BUS* pbus, REQ* req);
void Bus_finish_trans            (BUS* pbus, REQ* req);

void Bus_print_params            (int);
void Bus_stat_report             (int);
void Bus_stat_clear              (int);
void Bus_dump                    (int);

#endif
