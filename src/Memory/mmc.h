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

#ifndef __RSIM_MMC_H__
#define __RSIM_MMC_H__

#include <string.h>
#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/lqueue.h"
#include "Memory/mmc_param.h"
#include "Memory/mmc_stat.h"



/***************************************************************************/
/******** Definition of major data structures in MMC module ****************/
/***************************************************************************/

/*
 * data structures used to identify each outstanding transaction in MMC. 
 */
typedef enum
{
  MMC_INVALID_TYPE = 0,
  MMC_READ,
  MMC_WRITE,
  MMC_WRITEPURGE,
  MMC_COPYOUT,
  MMC_SYNC,
  MMC_DPURGEALLOC,
  MMC_DFLUSH,
  MMC_DPURGE
} mtrans_type_t;


typedef struct mmc_trans
{
  mtrans_type_t     mtype;       /* MMC transaction type                     */
  char              bank;        /* Memory bank                              */
  REQ              *req;	 /* Request spawned this transaction         */
  unsigned          paddr;       /* Real physical address to access DRAM     */
  unsigned          size;        /* Access size                              */
  int               c2c_copy;    /* has been satisfied by cache-2-cache copy */
  unsigned         *pdata;       /* Write Purge Data                         */

  rsim_time_t       issued;      /* Time issued to MMC                       */
  rsim_time_t       time;        /* time to enter different stage            */
} mmc_trans_t;



/*
 * Data structure to hold information in per MMC base.
 */
typedef struct
{
  /* General internal data structures */
  int          nodeid;
  mmc_stat_t   stats;
  int          trans_count;
  rsim_time_t  busy_start;
  rsim_time_t  free_start;
  int          slave_count;
  int          wb_count;
  LinkQueue    freelist;
  LinkQueue    waitlist;
  EVENT       *waittask;

  /* Interface with bus module */
  DLinkQueue   reqqueue;
  LinkQueue    arbwaiters;
  int          arbwaiters_count;
  EVENT       *pevent;
} mmc_info_t;



/***************************************************************************/
/********* Definition of global variables and useful macros ****************/
/***************************************************************************/

extern mmc_info_t *AllMMCs;

#define NID2MMC(nid) 		&(AllMMCs[nid])
#define MAX_TRANS    		512


		  
/***************************************************************************/
/******************* Definition of Functions *******************************/
/***************************************************************************/

/* mmc_init.c */
void         MMC_init                (void);
void         MMC_read_params         (void);
int          MMC_sim_on              (void);


/* mmc_main.c */
void         MMC_recv_trans          (mmc_info_t *, REQ *req);
void         MMC_process_waiter      (void);
void         MMC_dram_data_ready     (int nodeid, mmc_trans_t *);
void         MMC_dram_done           (int nodeid, mmc_trans_t *);
mmc_trans_t *MMC_get_free_trans      (mmc_info_t *pmmc);
void         MMC_nosim_start         (mmc_info_t *, mmc_trans_t *);
void         MMC_nosim_done          (void);


/* mmc_bus.c */
void         MMC_in_master_state     (REQ *req);
void         MMC_send_on_bus         (REQ *req);
void         MMC_recv_request        (REQ *req);
void         MMC_recv_write          (REQ *req);
void         MMC_recv_cohe_response  (REQ *req, int state);
void         MMC_recv_data_response  (REQ *req);
void         MMC_data_fetch_done     (mmc_info_t *pmmc, REQ *req);
void         MMC_start_send_to_bus   (mmc_info_t *pmmc, REQ *req);
void         MMC_send_reply          (mmc_info_t *pmmc, REQ *req);


/* mmc_stat.c and mmc_debug.c */
void         MMC_count_read_hit      (mmc_info_t *pmmc, int is_shadow);
void         MMC_count_nonread_hit   (mmc_info_t *pmmc, int is_shadow);
void         MMC_print_params        (int);
void         MMC_stat_report         (int);
void         MMC_stat_clear          (int);
void         MMC_dump                (int pid);

#endif
