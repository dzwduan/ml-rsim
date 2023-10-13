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

#ifndef __RSIM_CACHE_H__
#define __RSIM_CACHE_H__

#include "sim_main/stat.h"
#include "Caches/req.h"
#include "Caches/lqueue.h"
#include "Caches/cache_param.h"
#include "Caches/cache_stat.h"


/*************************************************************************/ 
/********************** Cache data structures ****************************/
/*************************************************************************/

/* 
 * Cache line states
 */ 
typedef enum
{
  BAD_LINE_STATE = 0,
  INVALID,
  PR_CL,            /* private clean */
  SH_CL,            /* shared clean */
  PR_DY,            /* private dirty */
  NUM_CACHE_LINE_STATES
} cline_state_t;



/*
 * Cache line data structure  
 */
typedef struct CacheLine
{
  int             index;
  unsigned        tag;           /* tag of cache line */
  unsigned        vaddr;         /* virtual address of cache line */
  cline_state_t   state;         /* state of cache line */
  unsigned short  age;           /* age of access -- used to implement LRU */
  char            mshr_out;      /* upgrade is going on */
  char            pref;          /* used for prefetch stats */
  unsigned        pref_tag_repl; /* used for prefetch stats */
  double          pref_time;     /* used for prefetch stats */
} cline_t;



/*
 * MSHR (miss status hold register) data structure
 */
typedef struct _mshr_
{
  int      valid;               /* Is this MSHR entry valid                  */
  int      setnum;              /* Corresponding set number for the MSHR     */
  REQ     *mainreq;             /* The first request that was sent out       */
  unsigned blocknum;            /* the address tag of mainreq                */
  cline_t *cline;               /* cache line this MSHR is working on        */
  int      misscache;           /* cache hit status                          */

  double   demand;              /* time of first demand access               */
  REQ     *pending_cohe;        /* The first request that was sent out       */
  unsigned stall_WAR:1,         /* A write after read stall in effect        */
           only_prefs:1,        /* Does this MSHR include only prefetches?   */
           has_writes:1,        /* Need to go to PR_DY or PR_CL              */
           releasing:1,         /* being released                            */
           unused:20,
           pend_opers:8;        /* pending flushes or purges                 */

  REQ     *coal_req[MAX_MAX_COALS]; /* Array of coalesced accesses           */
  short    counter;             /* The number of coalesced accesses          */
  short    next_move;
  struct _mshr_ *next;
} MSHR;




/*
 * The following are the return values from Cache_check_mshr rountines.
 */
typedef enum
{
  NOMSHR,               /* no MSHR involved or needed                        */
  NOMSHR_FWD,           /* does not need an MSHR, send it down               */
  NOMSHR_STALL,         /* stall because no MSHRs left                       */
  NOMSHR_STALL_COHE,    /* can't be serviced because cohe_pend bit is set    */

  MSHR_COAL,            /* coalesce into an old MSHR COHES                   */
  MSHR_NEW,             /* get and need a new MSHR                           */
  MSHR_STALL_WAR,       /* stall because of a WAR                            */
  MSHR_STALL_COHE,      /* stall because of a merged MSHR cohe               */
  MSHR_STALL_COAL,      /* stall because of excessive coalescing             */
  MSHR_STALL_FLUSH,     /* stall because pending flush/purge requests        */
  MSHR_STALL_RELEASE,   /* stall because the MSHR is being released          */
  MSHR_USELESS_FETCH,   /* prefetch useless; line fetch already in progress  */
  MAX_MSHR_TYPES
} MSHR_Response;




/*
 * Data structure for a cache.
 */
typedef struct _cache_
{
  short      nodeid;            /* Node number where module is located       */
  short      procid;            /* processor number where module is located  */
  short      gid;               /* system global processor ID                */
  short      type;              /* either L1CACHE or L2CACHE                 */
  struct _cache_ *l2_partner;   /* for L1 caches - points to L2              */
  struct _cache_ *l1i_partner;  /* for L2 cache, points to L1 I-cache        */
  struct _cache_ *l1d_partner;  /* for L2 cache, points to L1 D-cache        */

  /* Cache configuration variables and facilitator variables ----------------*/

  cline_t *tags;                /* cache line states                         */
  char    *data;                /* data for I-Cache, array of instructions   */
  int      size;                /* cache size                                */
  int      linesz;              /* cache line size                           */
  int      setsz;               /* associativity                             */
  int      num_lines;           /* number of cache lines                     */
  int      replacement;         /* replacement policy                        */
  int      set_mask;            /* (setsz - 1)                               */
  int      set_shift;           /* NumberOfBits(setsz - 1)                   */
  int      idx_mask;            /* ((number of sets) - 1)                    */
  int      idx_shift;           /* NumofOfBits((number of sets) - 1)         */
  int      block_mask;          /* number of bits removed to generate block  */
  int      block_shift;         /* # of shift bits to specify block number   */
  int      tag_shift;           /* # of shift bits to specify tag            */
  
  /* MSHR-related data structure */

  MSHR *mshrs;                  /* MSHR structures                           */
  int   mshr_count;             /* number of MSHRs in use                    */
  int   reqmshr_count;          /* number of MSHRs used by REQUESTs          */
  int   reqs_at_mshr_count;     /* number of REQUESTs at MSHRs               */
  int   max_mshrs;              /* total number of MSHRs in cache            */

  /* Input queues */

  LinkQueue request_queue;
  LinkQueue reply_queue;
  LinkQueue cohe_queue;
  LinkQueue noncohe_queue;
  LinkQueue *cohe_reply_queue;             /* for L2 only */
  int       inq_empty;          /* any REQs on input queues? */

  /* Pipelined cache structure */

  struct YS__Pipeline **data_pipe;     /* For the data RAM array pipes       */
  struct YS__Pipeline **tag_pipe;      /* For the tag array pipes            */
  int    num_in_pipes;                 /* number of accesses in pipes        */

  /* Bus interface, (used by l2 cache only) */

  DLinkQueue reqbuffer;
  LinkQueue  outbuffer;
  LinkQueue  arbwaiters;
  short      arbwaiters_count;

  /* Control or status variables */

  short    stall_request;     /* stop processing requests                   */
  short    stall_cohe;        /* stop processing coherence requests         */
  short    stall_reply;       /* stop processing replies                    */

  REQ    *cohe_reply;         /* only one data coherence reply is allowed   */
  REQ    *data_cohe_pending;
  REQ    *pending_writeback;
  REQ    *pending_request;

  /*
   * Statistics. For one L1 processor, L1 and L2 cache share the same
   * statistics data structure. That's why we use a pointer here.
   */
  CacheStat *pstats;           /* statistics                                 */
  STATREC   *pref_lateness;    /* extent to which late prefetches are late   */
  STATREC   *pref_earliness;   /* length of time useful prefetches sit in    */
  struct CapConfDetector *ccd; /* Capacity-conflict miss detector            */
} CACHE;




/* 
 * write buffer declaration 
 */
typedef struct YS__WBuffer
{
  LinkQueue  write_queue;    /* Outstanding write requests                   */
  LinkQueue  inqueue;        /* The input queue contains requests for L1     */
  int        inq_empty;      /* Is input queue empty                         */
  int        num_in_pipes;   /* Is write_queue empty                         */

  int stall_wb_full;         /* # of times requests stalled because WB full  */
  int stall_match;           /* # of read requests matching prior writes     */
} WBUFFER;




/**************************************************************************/ 
/********************* Global (exported) variables ************************/
/**************************************************************************/


extern CACHE   **L2Caches;
extern CACHE   **L1ICaches;
extern CACHE   **L1DCaches;
extern WBUFFER **WBuffers;
extern int      *L1IQ_FULL;
extern int      *L1DQ_FULL;


extern int L1I_NUM_PORTS;
extern int L1D_NUM_PORTS;
extern int L2_NUM_PORTS;

extern int L1I_TAG_DELAY;       /* L1 I-cache tag array access times         */
extern int L1I_TAG_PIPES;       /* Number of L1 I-cache tag array pipelines  */
extern int L1I_TAG_PORTS[];     /* Number of L1 I-cache ports to tag array   */

extern int L1D_TAG_DELAY;       /* L1 D-cache tag array access times         */
extern int L1D_TAG_PIPES;       /* Number of L1 D-cache tag array pipelines  */
extern int L1D_TAG_PORTS[];     /* Number of L1 D-cache ports to tag array   */

extern int L2_TAG_DELAY;        /* L2 cache tag array access times           */
extern int L2_TAG_PIPES;        /* Number of L2 cache tag array pipelines    */
extern int L2_TAG_PORTS[];      /* Number of L2 cache ports to tag array     */
extern int L2_DATA_DELAY;       /* L2 cache data array access times          */
extern int L2_DATA_PIPES;       /* Number of L2 cache data array pipelines   */
extern int L2_DATA_PORTS[];     /* Number of L2 cache ports to data array    */

extern int L1I_NUM_MSHRS;       /* Number of MSHRs at the L1 I-cache         */
extern int L1D_NUM_MSHRS;       /* Number of MSHRs at the L1 D-cache         */
extern int L2_NUM_MSHRS;        /* Number of MSHRs at the L2 cache           */

extern int MAX_COALS;           /* max. # of request coalesced in one MSHR   */

extern int block_mask1i;
extern int block_mask1d;
extern int block_mask2;

extern int block_shift1i;
extern int block_shift1d;
extern int block_shift2;



/**************************************************************************/ 
/********************* Some useful macros *********************************/
/**************************************************************************/

#define PID2L1I(nid, pid)        (L1ICaches[nid * ARCH_cpus + pid])
#define PID2L1D(nid, pid)        (L1DCaches[nid * ARCH_cpus + pid])
#define PID2L2C(nid, pid)        (L2Caches[nid * ARCH_cpus + pid])
#define PID2WBUF(nid, pid)       (WBuffers[nid * ARCH_cpus + pid])

#define ADDR2BNUM1I(addr)        (addr >> block_shift1i)
#define ADDR2BNUM1D(addr)        (addr >> block_shift1d)
#define ADDR2BNUM2(addr)         (addr >> block_shift2)
#define ADDR2SET(addr)   	 (((addr >> captr->block_shift) & \
                                   captr->idx_mask) << captr->set_shift)
#define PADDR2TAG(paddr)     	 (paddr >> captr->tag_shift)



/**********************************************************************/ 
/* Cache-related functions -- more documentation in .c files          */
/**********************************************************************/

/* 
 * Simulation functions called by cycle-by-cycle simulator in processor
 * module (mainsim.cc).
 */
extern void L1ICacheOutSim (int gid);
extern void L1DCacheOutSim (int gid);
extern void L2CacheOutSim  (int gid);

extern void L1ICacheInSim  (int gid);
extern void L1DCacheInSim  (int gid);
extern void L2CacheInSim   (int gid);



/* Function used by processor to start up a new memory reference */
void DCache_recv_addr      (int, struct instance *, long long, unsigned,
		            int, int, int, int);
void DCache_recv_tlbfill   (int, void (*)(REQ*), unsigned char*, unsigned,
			    void*);
void DCache_recv_barrier   (int);
void ICache_recv_addr      (int, unsigned, unsigned, int, int, unsigned);


/* Cache initialization and search routines. (cache_init.c, cache.c) */
void Cache_init                (void);
void Cache_init_aux            (CACHE *);
int  Cache_search              (CACHE *, unsigned, unsigned, cline_t **);
int  Cache_hit_update          (CACHE *, cline_t *, REQ *);
void Cache_pmiss_update        (CACHE *, REQ *, int, int); 
int  Cache_miss_update         (CACHE *, REQ *, cline_t **, int);   


/* L1 write-buffer routines. (cache_wb.c) */
void L1DCache_wbuffer_init     (WBUFFER *, int nodeid, int pid);
void L1DCache_wbuffer          (int proc_id);
int  L1DCache_wbuffer_search   (WBUFFER * wbufptr, REQ *req, int cohe);
void L1DCacheWBufferSim        (int proc_id);


/* Functions interacting with bus. (cache_bus.c) */
int  Cache_start_send_to_bus   (CACHE *, REQ *);
void Cache_arb_for_bus         (REQ *);
void Cache_in_master_state     (REQ *);
void Cache_send_on_bus         (REQ *);
void Cache_get_cohe_request    (REQ *);
void Cache_get_noncoh_request  (REQ *);
void Cache_get_data_response   (REQ *);
void Cache_get_reply           (REQ *);
void Cache_get_IO_reply        (REQ *);
int  Cache_perfect             (void);
REQ *Cache_check_outbuffer     (CACHE *, REQ *);


/* mmc_stat.c */
void  Cache_stat_set           (CACHE *captr, REQ *req);
void  Cache_print_params       (int);
void  Cache_stat_report        (int, int);
void  Cache_stat_clear         (int, int);


/* Some help functions. (cache_help.c, cache_debug.c) */
REQ  *Cache_make_req           (CACHE *, cline_t *, ReqType);
int   Cache_send_out_req       (LinkQueue *, REQ *);
int   Cache_uncoalesce_mshr    (CACHE *, MSHR *);
int   Cache_free_mshr          (CACHE *captr, MSHR *pmshr);
void  Cache_global_perform     (CACHE *, REQ *, int release_req);
MSHR *Cache_find_mshr          (CACHE *captr, REQ *req);
void  Cache_req_dump           (REQ *, int flag, int);
void  Cache_dump               (int proc_id);

#endif


