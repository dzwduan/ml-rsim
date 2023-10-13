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

#ifndef __RSIM_IO_GENERIC_H__
#define __RSIM_IO_GENERIC_H__


#include "sim_main/evlst.h"
#include "Caches/req.h"
#include "Caches/lqueue.h"
#include "Caches/pipeline.h"


#define IO_MAX_TRANS 8


extern int IO_LATENCY;


typedef struct
{
  REQ **reqs;
  int   req_count;
  int   req_max;
} SCOREBOARD;


typedef struct
{
  int nodeid;
  int mid;
  
  LinkQueue    arbwaiters;
  int          arbwaiters_count;

  LinkQueue    cohqueue;
  LinkQueue    inqueue;
  Pipeline    *inpipe;
  LinkQueue    outqueue;
  Pipeline    *outpipe;

  EVENT       *inq_event;
  EVENT       *outq_event;

  int (*read_func) (REQ*);
  int (*write_func)(REQ*);
  int (*reply_func)(REQ*);

  REQ         *writebacks;

  SCOREBOARD   scoreboard;
  
} IO_GENERIC;


extern IO_GENERIC *IOs;


#define IO_INDEX(nid, pid) ((pid-ARCH_cpus) * ARCH_numnodes + nid)
#define PID2IO(nid, pid) &(IOs[IO_INDEX(nid, pid)])

#define SCOREBOARD_INCREMENT 16


void IOGeneric_init         (int (*read_func) (REQ*),
		             int (*write_func)(REQ*),
		             int (*reply_func)(REQ*));

int  IO_start_transaction   (IO_GENERIC*, REQ*);

void IO_get_request         (IO_GENERIC*, REQ*);
void IO_get_cohe_request    (REQ*);
void IO_handle_inqueue      (void);
void IO_handle_outqueue     (void);
int  IO_start_send_to_bus   (IO_GENERIC*, REQ*);
int  IO_arb_for_bus         (REQ*);
void IO_in_master_state     (REQ*);
void IO_send_on_bus         (REQ*);
void IO_get_reply           (REQ*);
void IO_get_data_response   (REQ*);

void IO_scoreboard_init     (IO_GENERIC*);
void IO_scoreboard_insert   (IO_GENERIC*, REQ*);
REQ* IO_scoreboard_lookup   (IO_GENERIC*, REQ*);
void IO_scoreboard_remove   (IO_GENERIC*, REQ*);

void IO_read_byte           (REQ*);
void IO_read_word           (REQ*);
void IO_read_block          (REQ*);
void IO_write_byte          (REQ*);
void IO_write_word          (REQ*);
void IO_write_block         (REQ*);
void IO_empty_func          (REQ*, HIT_TYPE);

void IO_dump                (IO_GENERIC*);

#endif