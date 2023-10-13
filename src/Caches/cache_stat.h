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

#ifndef __RSIM_CACHE_STAT_H__
#define __RSIM_CACHE_STAT_H__


/* 
 * access type: where was an access actually served.
 */
typedef enum
{
  L1IHIT,                                   /* hit in L1 cache             */
  L1DHIT,                                   /* hit in L1 cache             */
  L2HIT,                                    /* hit in L2 cache             */
  MEMHIT,                                   /* satisfied in local memory   */
  IOHIT,                                    /* satisfied by I/O device     */
  UNKHIT                                    /* unknown type                */
} HIT_TYPE;



/*
 * cache miss type
 */
typedef enum
{
  CACHE_MISS_NONSPECIFIC,                   /* unclassified cache miss     */
  CACHE_MISS_COLD,                          /* cold miss                   */
  CACHE_MISS_CONF,                          /* conflict miss               */
  CACHE_MISS_CAP,                           /* capacity miss               */
  CACHE_MISS_COHE,                          /* coherence-type miss         */
  UNCACHED,                                 /* not cached                  */
  NUM_CACHE_MISS_TYPES                      /* NOTE: THIS MUST BE LAST!    */
} MISS_TYPE;

extern char *MissTypeNames[];               /* names for miss types        */



/*
 * statistics for each type of reference.
 */
typedef struct _refstat_
{
  long long  count;
  long long  hits[UNKHIT];
  double     hitcycles[UNKHIT];
  long long  l1imisses[NUM_CACHE_MISS_TYPES];
  long long  l1dmisses[NUM_CACHE_MISS_TYPES];
  long long  l2misses[NUM_CACHE_MISS_TYPES];
  double mlatency;
} ref_stat_t;



/* 
 * prefetch statistics 
 */
typedef struct
{
  long long total;
  long long dropped;       /* prefetches dropped due to no resource        */
  long long unnecessary;   /* line already in cache                        */
  long long issued;        /* issued to memory                             */
  long long useless;       /* if line gets replaced before demand access   */
  long long useful;        /* the prefetched line is actually demanded     */
  long long damaging;      /* replaced line reused before the prefetched   */
  long long early;         /* how many early prefetches                    */
  long long late;          /* if others coalesce into MSHR                 */
  double earliness;
  double lateness;
} prefetch_stat_t;



/*
 * Classification of MSHR stalls
 */
typedef struct
{
  long long war;           /* stalls that arise from write after read      */
  long long full;          /* stalls that arise from FULL MSHRs            */
  long long cohe;          /* stalls from pending COHE messages            */
  long long coal;          /* stalls from excessive coalescing             */
  long long release;       /* stalls from releasing MSHR                   */
  long long flush;         /* stall from pending flush/purge               */
} stall_stat_t;



/*
 * statistics for the cache 
 */
typedef struct CacheStat
{
  ref_stat_t ifetch;       /* instruction fetch                            */
  ref_stat_t read;         /* read                                         */
  ref_stat_t write;        /* write                                        */
  ref_stat_t rmw;          /* read-modify-write                            */
  ref_stat_t pread;        /* read prefetch                                */
  ref_stat_t pwrite;       /* write prefetch                               */
  ref_stat_t flush;        /* flush                                        */
  ref_stat_t purge;        /* purge                                        */
  ref_stat_t ioread;       /* I/O (uncached) read                          */
  ref_stat_t iowrite;      /* I/O (uncached) write                         */

  long long snoop_requests;

  /* victims of conflicts */
  long long shcl_victims1i, shcl_victims1d, shcl_victims2;
  long long prcl_victims1i, prcl_victims1d, prcl_victims2;
  long long prdy_victims1i, prdy_victims1d, prdy_victims2;

  /* prefetch statistics */
  prefetch_stat_t  sl1;    /* speculative load                             */
  prefetch_stat_t  sl2;    /* speculative load                             */
  prefetch_stat_t  l1ip;   /* L1 prefetch                                  */
  prefetch_stat_t  l1dp;   /* L1 prefetch                                  */
  prefetch_stat_t  l2p;    /* L2 prefetch                                  */

  /* stalls */
  stall_stat_t    l1istall;
  stall_stat_t    l1dstall;
  stall_stat_t    l2stall;
} CacheStat;

#endif
