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

#ifndef __RSIM_REQ_H__
#define __RSIM_REQ_H__

#include "Caches/system.h"
#include "Caches/cache_stat.h"


/*****************************************************************************/
/* ReqType: type of transaction carried in a given REQ                       */
/*****************************************************************************/

typedef enum
{
  BAD_REQ_TYPE = 0,

  /* the processor-only request types                                        */
  READ,                                 /* read                              */
  WRITE,                                /* write                             */
  RMW,                                  /* read-modify-write                 */
  FLUSHC,                               /* flush                             */
  PURGEC,                               /* purge                             */
  RWRITE,                               /* remote write                      */
  WBACK,
  
  /* System-visible transaction requests                                     */
  READ_SH,                              /* read to share                     */
  READ_OWN,                             /* read to own                       */
  READ_CURRENT,                         /* read coherent w/o state change    */
  UPGRADE,                              /* upgrade from shared to exclusive  */
  READ_UC,                              /* uncached read                     */
  WRITE_UC,                             /* uncached write                    */
  SWAP_UC,                              /* uncached write with data return   */
  
  /* reply types for lines                                                   */
  REPLY_EXCL,                           /* sending private data              */
  REPLY_SH,                             /* sending share data                */
  REPLY_UPGRADE,                        /* allow upgrade                     */
  REPLY_UC,                             /* send uncached data                */

  /* cohe types from L2 cache to L1 cache                                    */
  INVALIDATE,                           /* invalidate the line               */
  SHARED,                               /* change state to shared            */

  REQ_TYPE_MAX
} ReqType;


extern const char *ReqName[];


/*****************************************************************************/
/* Codes used in req->type to indicate type of transaction/response          */
/*****************************************************************************/

typedef enum
{
  REQUEST,                 /* indicate a new request for data or ownership   */
  REPLY,                   /* indicate a response to a REQUEST               */
  COHE,                    /* indicate a coherence transaction               */
  COHE_REPLY,              /* indicate a response to a coherence transaction */
  WRITEBACK,               /* writing back dirty private data                */
  WRITEPURGE,              /* writeback w/ cache invalidation                */
  BARRIER                  /* memory barrier                                 */
} RType;



/*****************************************************************************/
/* The REQ data structure: This contains a variety of fields used in each    */
/* of the modules of the memory system simulator                             */
/*****************************************************************************/

typedef struct _req_
{
  /*
   * Links used to build linked-lists. A req may be in several different
   * link queue at the same time, so we need multiple links.
   */
  struct _req_ 
    *dnext,                            /* used in CPU request buffer         */
    *dprev,                            /* used in CPU request buffer         */
    *dnext2,                           /* used in MMC request buffer         */
    *dprev2;                           /* used in MMC request buffer         */

  /*
   * Identify itself in the whole system.
   */
  RType    type;                       /* REQUEST, COHE, REPLY ...           */
  short    prefetch;                   /* prefetch access?                   */
  short    reqnum;                     /* request number                     */
  short    node;                       /* node ID                            */
  short    src_proc;                   /* source module of request           */
  short    dest_proc;                  /* destination module of request      */
  ReqType  prcr_req_type;              /* processor request type             */
  ReqType  req_type;                   /* transaction type                   */
  ReqType  cohe_type;                  /* result of coherence check          */
  unsigned vaddr;                      /* virtual address requested          */
  unsigned paddr;                      /* physical address requested         */
  int      ifetch;                     /* distinguish data/instructions      */
  int      size;                       /* size of data requested             */
  int      memattributes;              /* page attributes                    */

  /* Variables recording the status/progress                                 */
  short    issued;        /* REQUEST: issued on the bus?                     */
  short    data_done;     /* REQUEST: has data returned?                     */
  short    cohe_done;     /* REQUEST: is coherence check done?               */
  short    cohe_count;    /* REQUEST: how many cohe reports are expected?    */
  short    c2c_copy;      /* REQUEST: data provided by cache-to-cache copy?  */
  short    data_rtn;      /* REQUEST: has data returned from memory          */
  short    bus_cycles;    /* how many bus cycles this req needs              */
  short    progress;      /* Indicates the stage of progress for this REQUEST 
			     while being processed in a cache array          */
   short   wb_count;      /* used by L2 to count dirty lines in L1           */

  /*
   * Used by REPLY to indicate whether this reply needs an invalidation
   * to L1 cache or a victim write-back to memory.
   */
  struct _req_ *invl_req;              /* invalidation request to L1 cache   */
  struct _req_ *wrb_req;               /* write victim back to memory        */

  /*
   * Save some values to avoid re-searching.
   */
  struct _mshr_  *l1mshr;              /* L1 cache mshr used by REQUEST      */
  struct _mshr_  *l2mshr;              /* L2 cache mshr used by REQUEST      */

  /* Used by cache-to-cache req to identify the source req                   */
  struct _req_ *parent; 

  /* Used by wbuffer item to indicate if some coherence requests are
   * pending on that write request                                           */
  int   wb_cohe_pending;  

  struct mmc_trans *memtrans;
  
  /* Store all the MSHRs which are hit by the coherence requests spawned 
   * by this request                                                         */
  struct _mshr_  *stalled_mshrs[MAX_MEMSYS_SNOOPERS]; 

  /* fields used to identify this REQ to the processor simulator             */
  union
  {
    struct                             /* used for processor Ifetches        */
    {
      int   count;                     /* number of instructions to fetch    */
      int   proc_id;                   /* processor ID                       */
      unsigned pstate;                 /* Processor status word              */
      char *instructions;              /* pointer to predecoded instructions */
    } proc_instruction;
    
    struct                             /* used for processor data accesses   */
    {
      struct instance *inst;           /* instance executing the access      */
      long long        inst_tag;       /* copy of instance tag               */
      int              proc_id;        /* processor ID                       */
    } proc_data;

    struct                             /* used by other bus masters          */
    {
      unsigned char *buf;              /* internal data buffer (src or sink) */
      void          *aux;              /* other per-request information      */
      int            count;            /* number of bytes to transfer        */
    } mem;
  } d;

  void (*perform)(struct _req_*);
  void (*complete)(struct _req_*, HIT_TYPE);
  
  /* 
   * Statistics for latencies calculated from different points in progress
   * of this request.
   */
  double mem_start_time;
  double issue_time;
  double arb_start_time;
  double bus_start_time;
  double bus_return_time;
  unsigned   
    unused:16,
    hit_type:4,             /* where the request is satisfied?               */
    miss_type1:4,           /* what type of miss, if any?                    */
    miss_type2:4,           /* what type of miss, if any?                    */
    prefetched_late:2,      /* was this an access that to a late prefetch    */
    line_cold:2;            /* Is this the first access to line? (cold miss) */
} REQ;

#endif
