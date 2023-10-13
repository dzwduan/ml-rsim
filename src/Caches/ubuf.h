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

/*---------------------------------------------------------------------------*/
/* Uncached buffer for loads and stores with optional conditional store      */
/* buffer. Maintains queue of cacheline wide entries into which normal       */
/* loads and stores are combined, and a one-entry CSB.                       */
/*---------------------------------------------------------------------------*/

#ifndef __RSIM_UBUF_H__
#define __RSIM_UBUF_H__


#include "Caches/cache.h"
#include "Caches/req.h"


#define BT 1            /* Byte        */
#define HW 2            /* Half-word   */
#define WD 4            /* Word        */
#define DW 8            /* Double word */
#define QW 16           /* Quad word   */


/*---------------------------------------------------------------------------*/
/* uncached buffer entry                                                     */

typedef struct
{
  REQ          **reqs;          /* list of combined requests                 */
  int          fence;           /* flag a fence (do not combine across fence)*/
  unsigned     address;         /* address of pending request                */
  ReqType      type;            /* type of request (read or write)           */
  int          busy;            /* busy - do not combine into this entry     */
} UBUFITEM;





/*-------------------------------------------------------------------------*/
/* uncached buffer and CSB structure                                       */

struct UBUF
{
  int          nodeid;
  int          procid;
  
  CACHE       *above;
  CACHE       *below;
  
  unsigned     line_mask;
  int          num_entries;    /* number of entries in uncached buffer queue */
  long long    cur_reads;      /* number of reads currently in uncd. buffer  */
  UBUFITEM   **entries;        /* list of uncached buffer entries            */

  int          transaction_pending;


  /* configuration ----------------------------------------------------------*/

  int          size;           /* number of entries of uncached buffer       */
  int          flush;          /* uncached buffer flush threshold            */
  int          combine;        /* turn on combining mode                     */
  int          entrysz;        /* number of combined requests per entry      */

  /* statistics -------------------------------------------------------------*/
  
  long long    stall_ub_full;  /* # of times requests stalled because UB full*/
  long long    num_wr_coals;   /* total # of coalesced write requests        */
  long long    max_wr_coals;   /* maximum # of writes coalesced              */
  long long    num_rd_coals;   /* total # of coalesced read requests         */
  long long    max_rd_coals;   /* maximum # of reads coalesced               */

  long long    num_barriers;   /* total # of memory barriers received        */
  long long    num_barr_eff;   /* # of barriers that have marked an entry    */

  long long    num_writes;     /* # of write requests                        */
  long long    num_reads;      /* # of read requests                         */
  double       read_latency;   /* number cycles between issue and completion */
  
  STATREC     *occ;            /* uncached buffer occupancy                  */
};

typedef struct UBUF UBUF;


extern UBUF **UBuffers;

#define PID2UBUF(nid, pid) (UBuffers[nid * ARCH_cpus + pid])




/* Initialize uncached buffer */
void UBuffer_init                (void);

int  UBuffer_add_request         (REQ*);
void UBuffer_out_sim             (int);
void UBuffer_confirm_transaction (REQ*);
void UBuffer_dump                (int);

/* Statistics functions */
void UBuffer_print_params        (int, int);
void UBuffer_stat_report         (int, int);
void UBuffer_stat_clear          (int, int);
void UBuffer_stat_set            (REQ*);

#endif

