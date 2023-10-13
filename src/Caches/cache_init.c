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
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include "Processor/simio.h"
#include "Processor/capconf.h"
#include "Processor/predecode.h"
#include "Processor/procstate.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"
#include "Bus/bus.h"
#include "IO/io_generic.h"


CACHE   **L1ICaches;
CACHE   **L1DCaches;
CACHE   **L2Caches;
WBUFFER **WBuffers;

int      *L1IQ_FULL;
int      *L1DQ_FULL;
int      *UBQ_FULL;

cache_param_t cparam;


/*
 * Default setting
 */
int ARCH_cacsz1i  = L1I_DEFAULT_SIZE;
int ARCH_setsz1i  = L1I_DEFAULT_SET_SIZE;
int ARCH_linesz1i = L1I_DEFAULT_LINE_SIZE;

int ARCH_cacsz1d  = L1D_DEFAULT_SIZE;
int ARCH_setsz1d  = L1D_DEFAULT_SET_SIZE;
int ARCH_linesz1d = L1D_DEFAULT_LINE_SIZE;

int ARCH_cacsz2   = L2_DEFAULT_SIZE;
int ARCH_setsz2   = L2_DEFAULT_SET_SIZE;
int ARCH_linesz2  = L2_DEFAULT_LINE_SIZE;

int L1I_NUM_PORTS = L1I_DEFAULT_NUM_PORTS;
int L1D_NUM_PORTS = L1D_DEFAULT_NUM_PORTS;
int L2_NUM_PORTS  = L2_DEFAULT_NUM_PORTS;

int L1I_NUM_MSHRS = 8;    /* default number of MSHRs                         */
int L1D_NUM_MSHRS = 8;    /* default number of MSHRs                         */
int L2_NUM_MSHRS  = 8;    /* default MSHR count                              */

int MAX_COALS     = MAX_MAX_COALS;

int L1I_TAG_PIPES  = 3;   /* number of ports to L1 Tag: REQUEST, REPLY, COHE */
int L1I_TAG_DELAY  = 1;   /* time spent in TAG-RAM pipe                      */
int L1I_TAG_REPEAT = 1;   /* repeat rate of L1 TAG-RAM                       */
int L1I_TAG_PORTS[10] =   /* width of individual pipes                       */
    { L1I_DEFAULT_NUM_PORTS, 1, 1 };

int L1D_TAG_PIPES  = 3;   /* number of ports to L1 Tag: REQUEST, REPLY, COHE */
int L1D_TAG_DELAY  = 1;   /* time spent in TAG-RAM pipe                      */
int L1D_TAG_REPEAT = 1;   /* repeat rate of L1 TAG-RAM                       */
int L1D_TAG_PORTS[10] =   /* width of individual pipes                       */
    { L1D_DEFAULT_NUM_PORTS, 1, 1 };

int L2_TAG_PIPES  = 3;    /* number of ports to L2 Tag: REQUEST, REPLY, COHE */
int L2_TAG_DELAY  = 3;    /* time spent in TAG-RAM pipe                      */
int L2_TAG_REPEAT = 1;    /* repeat rate of L2 tag pipeline                  */
int L2_TAG_PORTS[10] =    /* width of each individual pipe                   */
    { 1, 1, L2_DEFAULT_NUM_PORTS };
int L2_DATA_PIPES  = 1;   /* number of ports to L2 Data                      */
int L2_DATA_DELAY  = 5;   /* time spent in DATA-RAM pipe                     */
int L2_DATA_REPEAT = 1;   /* repeat rate of L2 data pipeline                 */
int L2_DATA_PORTS[10] = { 1 };

int REQUEST_BUFFER_SIZE    = 4;
int OUT_BUFFER_SIZE        = 5;
int MAX_ARB_WAITERS        = 16;

int L1I_REQUEST_QUEUE_SIZE = 1;
int L1I_REPLY_QUEUE_SIZE   = 8;
int L1I_COHE_QUEUE_SIZE    = 16;

int L1D_REQUEST_QUEUE_SIZE = 1;
int L1D_REPLY_QUEUE_SIZE   = 8;
int L1D_COHE_QUEUE_SIZE    = 16;

int L2_REQUEST_QUEUE_SIZE  = 8;
int L2_REPLY_QUEUE_SIZE    = 8;
int L2_COHE_QUEUE_SIZE     = 16;



/*
 * For quick mapping for addresses to block numbers.
 */
int block_mask1i;
int block_mask1d;
int block_mask2;
int block_shift1i;
int block_shift1d;
int block_shift2;


/*
 * Static functions.
 */
static void Cache_read_params (void);
static void L1ICache_init     (CACHE *captr, int nodeid, int pid);
static void L1DCache_init     (CACHE *captr, int nodeid, int pid);
static void L2Cache_init      (CACHE *captr, int nodeid, int pid);

#define min(a, b)               ((a) > (b) ? (b) : (a))
#define max(a, b)               ((a) < (b) ? (b) : (a))


/*=============================================================================
 * Initializes the cache hierarchy, which includes a write-through L1 cache
 * with a write buffer and a write-back, write-allocate L2 cache. This
 * cache hierarchy maintains inclusion property: everything in L1 cache must
 * also be in L2 cache.
 */


void Cache_init(void)
{
  CACHE     *l1icaches;
  CACHE     *l1dcaches;
  CACHE     *l2caches;
  WBUFFER   *wbuffers;
  CacheStat *cachestats;
  int        nodeid, procid, i;

  Cache_read_params();

  /*
   * Sanity check on cache configuration
   */
  if (ARCH_linesz1i < MIN_CACHE_LINESZ)
    {
      YS__warnmsg(0,
		  "L1 I-Cache line sizes must be > %d.",
		  MIN_CACHE_LINESZ);
      ARCH_linesz1i = MIN_CACHE_LINESZ;
    }
  
  if (ARCH_linesz1d < MIN_CACHE_LINESZ)
    {
      YS__warnmsg(0,
		  "L1 D-Cache line sizes must be > %d.",
		  MIN_CACHE_LINESZ);
      ARCH_linesz1d = MIN_CACHE_LINESZ;
    }
  
  if ((ARCH_linesz2 < ARCH_linesz1i) || (ARCH_linesz2 < ARCH_linesz1d))
    {
      YS__warnmsg(0,
		  "L2 Cache line sizes must be > %d/%d.",
		  ARCH_linesz1i, ARCH_linesz1d);
      ARCH_linesz2 = max(ARCH_linesz1i, ARCH_linesz1d);
    }

  if (MAX_COALS > MAX_MAX_COALS)
    {
      YS__warnmsg(0,
		  "Cache_mshr_coal (%i) cannot be greater than MAX_MAX_COAL (%i)\n",
		  MAX_COALS, MAX_MAX_COALS);
      MAX_COALS = MAX_MAX_COALS;
    }

  L1ICaches  = RSIM_CALLOC(CACHE*, ARCH_numnodes * ARCH_cpus);
  L1DCaches  = RSIM_CALLOC(CACHE*, ARCH_numnodes * ARCH_cpus);
  L2Caches   = RSIM_CALLOC(CACHE*, ARCH_numnodes * ARCH_cpus);
  
  l1icaches  = RSIM_CALLOC(CACHE, ARCH_numnodes * ARCH_cpus);
  l1dcaches  = RSIM_CALLOC(CACHE, ARCH_numnodes * ARCH_cpus);
  l2caches   = RSIM_CALLOC(CACHE, ARCH_numnodes * ARCH_cpus);

  WBuffers = RSIM_CALLOC(WBUFFER*, ARCH_numnodes * ARCH_cpus);
  wbuffers = RSIM_CALLOC(WBUFFER, ARCH_numnodes * ARCH_cpus);


  L1IQ_FULL  = RSIM_CALLOC(int, ARCH_numnodes * ARCH_cpus);
  L1DQ_FULL  = RSIM_CALLOC(int, ARCH_numnodes * ARCH_cpus);
  UBQ_FULL   = RSIM_CALLOC(int, ARCH_numnodes * ARCH_cpus);

  cachestats = RSIM_CALLOC(CacheStat, ARCH_numnodes * ARCH_cpus);

  if (!L1ICaches || !L1DCaches || !L2Caches || !WBuffers ||
      !l1icaches || !l1dcaches || !l2caches ||
      !L1IQ_FULL || !L1DQ_FULL || !UBQ_FULL ||
      !cachestats)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);


  for (nodeid = 0; nodeid < ARCH_numnodes; nodeid++)
    {
      for (procid = 0; procid < ARCH_cpus; procid++)
	{
	  i = nodeid * ARCH_cpus + procid;

	  L1ICache_init(&(l1icaches[i]), nodeid, procid);
	  PID2L1I(nodeid, procid) = &(l1icaches[i]);
	  PID2L1I(nodeid, procid)->pstats = &(cachestats[i]);

	  L1DCache_init(&(l1dcaches[i]), nodeid, procid);
	  PID2L1D(nodeid, procid) = &(l1dcaches[i]);
	  PID2L1D(nodeid, procid)->pstats = &(cachestats[i]);

	  L2Cache_init(&(l2caches[i]), nodeid, procid);
	  PID2L2C(nodeid, procid) = &(l2caches[i]);
	  PID2L2C(nodeid, procid)->pstats = &(cachestats[i]);

	  L1DCache_wbuffer_init(&(wbuffers[i]), nodeid, procid);
	  PID2WBUF(nodeid, procid) = &(wbuffers[i]);

	  PID2L2C(nodeid, procid)->l1i_partner = &(l1icaches[i]);
	  PID2L2C(nodeid, procid)->l1d_partner = &(l1dcaches[i]);
	  PID2L1I(nodeid, procid)->l2_partner = &(l2caches[i]);
	  PID2L1D(nodeid, procid)->l2_partner = &(l2caches[i]);

	  L1IQ_FULL[i] = 0;
	  L1DQ_FULL[i] = 0;
	  UBQ_FULL[i]  = 0;
	}
    }

  block_mask1i  = ~(l1icaches[0].block_mask);
  block_shift1i = l1icaches[0].block_shift;

  block_mask1d  = ~(l1dcaches[0].block_mask);
  block_shift1d = l1dcaches[0].block_shift;

  block_mask2  = ~(l2caches[0].block_mask);
  block_shift2 = l2caches[0].block_shift;
}




/*=============================================================================
 * Read parameters about cache configuration from the designated parameter 
 * file. (default is "rsim_params" in current directory);
 */

void Cache_read_params(void)
{
  cparam.L1I_prefetch   = 0;
  cparam.L1D_prefetch   = 0;
  cparam.L1D_writeback  = 1;
  cparam.L2_prefetch   = 0;
  cparam.collect_stats = 1;
  cparam.frequency     = 1;

  get_parameter("Cache_collect_stats", &cparam.collect_stats, PARAM_INT);
  get_parameter("Cache_frequency",     &cparam.frequency,     PARAM_INT);
  get_parameter("Cache_mshr_coal",     &MAX_COALS,            PARAM_INT);
  get_parameter("L2C_size",            &ARCH_cacsz2,          PARAM_INT);
  get_parameter("BUS_total_requests",  &BUS_TOTAL_REQUESTS,   PARAM_INT);
  get_parameter("BUS_cpu_requests",    &BUS_CPU_REQUESTS,     PARAM_INT);

  get_parameter("L1IC_perfect",        &cparam.L1I_perfect,   PARAM_INT);
  get_parameter("L1IC_prefetch",       &cparam.L1I_prefetch,  PARAM_INT);
  get_parameter("L1IC_size",           &ARCH_cacsz1i,         PARAM_INT);
  get_parameter("L1IC_assoc",          &ARCH_setsz1i,         PARAM_INT);
  get_parameter("L1iC_line_size",      &ARCH_linesz1i,        PARAM_INT);
  get_parameter("L1IC_ports",          &L1I_NUM_PORTS,        PARAM_INT);
  get_parameter("L1IC_tag_latency",    &L1I_TAG_DELAY,        PARAM_INT);
  get_parameter("L1IC_tag_repeat",     &L1I_TAG_REPEAT,       PARAM_INT);
  get_parameter("L1IC_mshr",           &L1I_NUM_MSHRS,        PARAM_INT);
  
  get_parameter("L1DC_perfect",        &cparam.L1D_perfect,   PARAM_INT);
  get_parameter("L1DC_prefetch",       &cparam.L1D_prefetch,  PARAM_INT);
  get_parameter("L1DC_writeback",      &cparam.L1D_writeback, PARAM_INT);
  get_parameter("L1DC_size",           &ARCH_cacsz1d,         PARAM_INT);
  get_parameter("L1DC_assoc",          &ARCH_setsz1d,         PARAM_INT);
  get_parameter("L1DC_line_size",      &ARCH_linesz1d,        PARAM_INT);
  get_parameter("L1DC_ports",          &L1D_NUM_PORTS,        PARAM_INT);
  get_parameter("L1DC_tag_latency",    &L1D_TAG_DELAY,        PARAM_INT);
  get_parameter("L1DC_tag_repeat",     &L1D_TAG_REPEAT,       PARAM_INT);
  get_parameter("L1DC_wbuf_size",      &ARCH_wbufsz,          PARAM_INT);
  get_parameter("L1DC_mshr",           &L1D_NUM_MSHRS,        PARAM_INT);
  
  get_parameter("L2C_prefetch",        &cparam.L2_prefetch,   PARAM_INT);
  get_parameter("L2C_perfect",         &cparam.L2_perfect,    PARAM_INT);
  get_parameter("L2C_size",            &ARCH_cacsz2,          PARAM_INT);
  get_parameter("L2C_assoc",           &ARCH_setsz2,          PARAM_INT);
  get_parameter("L2C_line_size",       &ARCH_linesz2,         PARAM_INT);
  get_parameter("L2C_ports",           &L2_NUM_PORTS,         PARAM_INT);
  get_parameter("L2C_tag_latency",     &L2_TAG_DELAY,         PARAM_INT);
  get_parameter("L2C_tag_repeat",      &L2_TAG_REPEAT,        PARAM_INT);
  get_parameter("L2C_data_latency",    &L2_DATA_DELAY,        PARAM_INT);
  get_parameter("L2C_data_repeat",     &L2_DATA_REPEAT,       PARAM_INT);
  get_parameter("L2C_mshr",            &L2_NUM_MSHRS,         PARAM_INT);

  L1I_TAG_PORTS[0] = L1I_NUM_PORTS;
  L1D_TAG_PORTS[0] = L1D_NUM_PORTS;
  L2_TAG_PORTS[2]  = L2_NUM_PORTS;

  /*
   * I like put the whole configuration into one global structure.
   * Dumb?! Hm, You'll see when you are debugging the simulator.
   */
  cparam.L1I_size        = ARCH_cacsz1i;
  cparam.L1I_line_size   = ARCH_linesz1i;
  cparam.L1I_set_size    = ARCH_setsz1i;
  cparam.L1I_mshr_num    = L1I_NUM_MSHRS;
  cparam.L1I_tag_delay   = L1I_TAG_DELAY;
  cparam.L1I_tag_repeat  = L1I_TAG_REPEAT;
  cparam.L1I_tag_pipes   = L1I_TAG_PIPES;
  cparam.L1I_port_num    = L1I_TAG_PORTS[0];
  cparam.L1I_rep_queue   = BUS_TOTAL_REQUESTS;
  cparam.L1I_req_queue   = L1I_REQUEST_QUEUE_SIZE;
  cparam.L1I_cohe_queue  = BUS_TOTAL_REQUESTS;

  cparam.L1D_size        = ARCH_cacsz1d;
  cparam.L1D_line_size   = ARCH_linesz1d;
  cparam.L1D_set_size    = ARCH_setsz1d;
  cparam.L1D_wb_size     = ARCH_wbufsz;
  cparam.L1D_mshr_num    = L1D_NUM_MSHRS;
  cparam.L1D_tag_delay   = L1D_TAG_DELAY;
  cparam.L1D_tag_repeat  = L1D_TAG_REPEAT;
  cparam.L1D_tag_pipes   = L1D_TAG_PIPES;
  cparam.L1D_port_num    = L1D_TAG_PORTS[0];
  cparam.L1D_rep_queue   = BUS_TOTAL_REQUESTS;
  cparam.L1D_req_queue   = L1D_REQUEST_QUEUE_SIZE;
  cparam.L1D_cohe_queue  = BUS_TOTAL_REQUESTS;

  cparam.L2_size         = ARCH_cacsz2;
  cparam.L2_line_size    = ARCH_linesz2;
  cparam.L2_set_size     = ARCH_setsz2;
  cparam.L2_mshr_num     = L2_NUM_MSHRS;
  cparam.L2_tag_delay    = L2_TAG_DELAY;
  cparam.L2_tag_repeat   = L2_TAG_REPEAT;
  cparam.L2_data_pipes   = L2_DATA_PIPES;
  cparam.L2_data_delay   = L2_DATA_DELAY;
  cparam.L2_data_repeat  = L2_DATA_REPEAT;
  cparam.L2_tag_pipes    = L2_TAG_PIPES;
  cparam.L2_port_num     = L2_TAG_PORTS[2];
  cparam.L2_rep_queue    = BUS_TOTAL_REQUESTS;
  cparam.L2_req_queue    = L2_REQUEST_QUEUE_SIZE;
  cparam.L2_cohe_queue   = BUS_TOTAL_REQUESTS;
}



extern int MEM_UNITS;


/*=============================================================================
 * Initialization of the L1 I-cache.
 */

static void L1ICache_init(CACHE *captr, int nodeid, int procid)
{
  int i;

  if (cparam.L1I_size <= 0)
    YS__errmsg(nodeid, "L1 I-cache size must be greater then zero");

  /*
   * Common fields: identity, configuration, etc.
   */
  captr->nodeid      = nodeid;
  captr->procid      = procid;
  captr->gid         = nodeid * ARCH_cpus + procid;
  captr->type        = L1CACHE;
  captr->size        = ARCH_cacsz1i;
  captr->linesz      = ARCH_linesz1i;
  captr->setsz       = ARCH_setsz1i;
  captr->replacement = LRU;

  
  /*
   * Initialize data array. For perfect cache, we always fake a hit
   * for each request, so no needs for data array.
   */

  Cache_init_aux(captr);

  captr->data = (char*)memalign(sizeof(long long),
				ARCH_cacsz1i *
				(1024 / SIZE_OF_SPARC_INSTRUCTION) *
				SIZEOF_INSTR);
  if (captr->data == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);
  
  /*
   * Initialize MSHRs (Miss Status Hold Register?).
   */
  captr->max_mshrs = L1I_NUM_MSHRS;
  captr->mshrs = RSIM_CALLOC(MSHR, captr->max_mshrs);
  if (captr->mshrs == NULL)
    YS__errmsg(nodeid, "Malloc failed in %s:%i", __FILE__, __LINE__);

  
  for (i = 0; i < captr->max_mshrs; i++)
    captr->mshrs[i].valid = 0;

  
  captr->mshr_count = 0;       /* counts WRBs in MSHRs, etc */
  captr->reqmshr_count = 0;    /* only counting data requests */
  captr->reqs_at_mshr_count = 0;

  
  /*
   *  Initialize input queues
   *  1. "request queue" contains request from processor.
   *  2. "reply queue" contains replies from L2 cache.
   *  3. "cohe queue" contains cohe request from other processor.
   */
  lqueue_init(&(captr->request_queue),
	      L1I_REQUEST_QUEUE_SIZE > MEM_UNITS ?
	      L1I_REQUEST_QUEUE_SIZE : MEM_UNITS);
  lqueue_init(&(captr->reply_queue), L1I_REPLY_QUEUE_SIZE);
  lqueue_init(&(captr->cohe_queue), L1I_COHE_QUEUE_SIZE);
  captr->inq_empty = 1;

  
  /*
   * Tag pipelines. Unlike L2 cache, L1 cache has only a tag array.
   */
  captr->tag_pipe = RSIM_CALLOC(Pipeline *, L1I_TAG_PIPES);
  if (captr->tag_pipe == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < L1I_TAG_PIPES; i++)
    captr->tag_pipe[i] = NewPipeline(L1I_TAG_PORTS[i], 
				     cparam.frequency * L1I_TAG_DELAY,
				     cparam.frequency * L1I_TAG_REPEAT,
				     L1I_TAG_DELAY / cparam.frequency);
  
  captr->num_in_pipes = 0;

  
  /*
   * Statistics related stuff: capacity/conflict miss detector,
   * prefetch earliness and lateness.
   */
  captr->ccd = NewCapConfDetector(captr->num_lines);
  captr->pref_lateness  = NewStatrec(nodeid,
				     "Prefetch Lateness", POINT, MEANS,
				     HIST, 20, 0, 200);
  captr->pref_earliness = NewStatrec(nodeid,
				     "Prefetch Earlyness", POINT, MEANS,
				     HIST, 20, 0, 200);
}




/*===========================================================================
 * Initialization of the L1 D-cache.
 */

static void L1DCache_init(CACHE *captr, int nodeid, int procid)
{
  int i;

  if (cparam.L1D_size == 0)
    YS__errmsg(nodeid, "L1 D-cache size must be greater then zero");

  /*
   * Common fields: identity, configuration, etc.
   */
  captr->nodeid      = nodeid;
  captr->procid      = procid;
  captr->gid         = nodeid * ARCH_cpus + procid;
  captr->type        = L1CACHE;
  captr->size        = ARCH_cacsz1d;
  captr->linesz      = ARCH_linesz1d;
  captr->setsz       = ARCH_setsz1d;
  captr->replacement = LRU;

  
  /*
   * Initialize data array. For perfect cache, we always fake a hit
   * for each request, so no needs for data array.
   */

  if (cparam.L1D_perfect == 0)
    Cache_init_aux(captr);

  captr->data = NULL;
  
  /*
   * Initialize MSHRs (Miss Status Hold Register?).
   */
  captr->max_mshrs = L1D_NUM_MSHRS;
  captr->mshrs = RSIM_CALLOC(MSHR, captr->max_mshrs);
  if (captr->mshrs == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  
  for (i = 0; i < captr->max_mshrs; i++)
    captr->mshrs[i].valid = 0;

  
  captr->mshr_count = 0;       /* counts WRBs in MSHRs, etc */
  captr->reqmshr_count = 0;    /* only counting data requests */
  captr->reqs_at_mshr_count = 0;

  
  /*
   *  Initialize input queues
   *  1. "request queue" contains request from processor.
   *  2. "reply queue" contains replies from L2 cache.
   *  3. "cohe queue" contains cohe request from other processor.
   */
  lqueue_init(&(captr->request_queue),
	      L1D_REQUEST_QUEUE_SIZE > MEM_UNITS ?
	      L1D_REQUEST_QUEUE_SIZE : MEM_UNITS);
  lqueue_init(&(captr->reply_queue), L1D_REPLY_QUEUE_SIZE);
  lqueue_init(&(captr->cohe_queue), L1D_COHE_QUEUE_SIZE);
  captr->inq_empty = 1;

  
  /*
   * Tag pipelines. Unlike L2 cache, L1 cache has only a tag array.
   */
  captr->tag_pipe = RSIM_CALLOC(Pipeline *, L1D_TAG_PIPES);
  if (captr->tag_pipe == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < L1D_TAG_PIPES; i++)
    captr->tag_pipe[i] = NewPipeline(L1D_TAG_PORTS[i], 
				     cparam.frequency * L1D_TAG_DELAY,
				     cparam.frequency * L1D_TAG_REPEAT,
				     L1D_TAG_DELAY / cparam.frequency);
  
  captr->num_in_pipes = 0;

  
  /*
   * Statistics related stuff: capacity/conflict miss detector,
   * prefetch earliness and lateness.
   */
  captr->ccd = NewCapConfDetector(captr->num_lines);
  captr->pref_lateness  = NewStatrec(nodeid,
				     "Prefetch Lateness", POINT, MEANS,
				     HIST, 20, 0, 200);
  captr->pref_earliness = NewStatrec(nodeid,
				     "Prefetch Earlyness", POINT, MEANS,
				     HIST, 20, 0, 200);
}




/*===========================================================================
 * Initialize the L2 cache
 */

static void L2Cache_init(CACHE *captr, int nodeid, int procid)
{
  int i;

  /*
   * Common field: identity, configuration
   */
  captr->nodeid      = nodeid;
  captr->procid      = procid;
  captr->gid         = nodeid * ARCH_cpus + procid;
  captr->type        = L2CACHE;
  captr->size        = ARCH_cacsz2;
  captr->linesz      = ARCH_linesz2;
  captr->setsz       = ARCH_setsz2;
  captr->replacement = LRU;

  /*
   * Initialize data array.
   */
  if (cparam.L2_perfect == 0)
    Cache_init_aux(captr);

  captr->data = 0;
  
  /*
   * Intialize MSHRs
   */
  captr->max_mshrs = L2_NUM_MSHRS;
  captr->mshrs = RSIM_CALLOC(MSHR, captr->max_mshrs);
  if (captr->mshrs == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < captr->max_mshrs; i++)
    captr->mshrs[i].valid = 0;

  captr->mshr_count = 0;
  captr->reqmshr_count = 0;
  captr->reqs_at_mshr_count = 0;

  /*
   * Initialize input queues. As in L1 cache, it has three input queues.
   */
  lqueue_init(&(captr->request_queue), L2_REQUEST_QUEUE_SIZE);
  lqueue_init(&(captr->reply_queue), L2_REPLY_QUEUE_SIZE);
  lqueue_init(&(captr->cohe_queue), L2_COHE_QUEUE_SIZE);
  lqueue_init(&(captr->noncohe_queue), IO_MAX_TRANS);
  captr->cohe_reply_queue = &(captr->cohe_queue);
  captr->inq_empty     = 1;

  /*
   * Pipelines. Unlike L1 cache, L2 cache is split into a "tag SRAM"
   * array and a "data SRAM" array.
   */
  captr->data_pipe = RSIM_CALLOC(Pipeline *, L2_DATA_PIPES);
  if (captr->data_pipe == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < L2_DATA_PIPES; i++)
    captr->data_pipe[i] = NewPipeline(L2_DATA_PORTS[i], 
				      cparam.frequency * L2_DATA_DELAY,
				      cparam.frequency * L2_DATA_REPEAT,
				      L2_DATA_REPEAT ?
				      (L2_DATA_DELAY / cparam.frequency) : 1);

  captr->tag_pipe = RSIM_CALLOC(Pipeline*, L2_TAG_PIPES);
  if (captr->tag_pipe == NULL)
    YS__errmsg(nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);

  for (i = 0; i < L2_TAG_PIPES; i++)
    captr->tag_pipe[i] = NewPipeline(L2_TAG_PORTS[i], 
				     cparam.frequency * L2_TAG_DELAY,
				     cparam.frequency * L2_TAG_REPEAT,
				     L2_TAG_DELAY / cparam.frequency);

  captr->num_in_pipes = 0;

  /*
   * Interface to bus. (L1 cache doesn't have the equivalent part.)
   */
  dqueue_init(&(captr->reqbuffer), REQUEST_BUFFER_SIZE);
  lqueue_init(&(captr->outbuffer), OUT_BUFFER_SIZE);
  lqueue_init(&(captr->arbwaiters), MAX_ARB_WAITERS);
  captr->arbwaiters_count  = 0;
  captr->stall_request     = 0;
  captr->stall_reply       = 0;
  captr->stall_cohe        = 0;
  captr->cohe_reply        = 0;
  captr->data_cohe_pending = 0;
  captr->pending_writeback = 0;
  captr->pending_request   = 0;
  
  /*
   * Statistics related stuffs: capacity/conflict miss detector; prefetch
   * earliness and lateness.
   */
  captr->ccd = NewCapConfDetector(captr->num_lines);
  captr->pref_lateness  = NewStatrec(nodeid,
				     "Prefetch Lateness",
				     POINT, MEANS,
				     HIST, 20, 0, 200);
  captr->pref_earliness = NewStatrec(nodeid,
				     "Prefetch Earlyness",
				     POINT, MEANS,
				     HIST, 20, 0, 200);
}



/*===========================================================================
 * This routine initializes the tag SRAM array of a specified cache.
 * It allocates space for each cache line. Since there is no real data
 * flowing around in the memory system, each line is represented by
 * its associated control information: state, physical tag, age, and some 
 * other information needed by the simulator to collect statistics.
 */

void Cache_init_aux(CACHE * captr)
{
  int i, j;
  int nof_sets, num_lines;
  int cachesize, blocksize, setsize;

  cachesize = captr->size;
  blocksize = captr->linesz;
  setsize   = captr->setsz;

  /*
   * Sanity check on cache configuration.
   */
  if (cachesize == INFINITE)
    YS__errmsg(captr->nodeid,
	       "No infinite cache in this version.");

  if (blocksize < WORDSZ)
    YS__errmsg(captr->nodeid,
	       "NewCache(): line size %d is too small.", blocksize);

  if (NumOfBits(blocksize, 1) < 0)
    YS__errmsg(captr->nodeid,
	       "NewCache(): line size(%d) isn't a power of 2", blocksize);

  if (setsize != FULL_ASS)
    if (NumOfBits(setsize, 1) < 0)
      YS__errmsg(captr->nodeid,
		 "NewCache(): set size(%d) isn't a power of 2", setsize);
  
  if ((cachesize * 1024) % blocksize)
    YS__errmsg(captr->nodeid,
	       "NewCache(): cache-size(%dKbytes) / line-size(%dbytes) != 0",
	       cachesize, blocksize);

  num_lines = (cachesize * 1024) / blocksize;

  if (setsize != FULL_ASS)  /* set associativity */
    {
      if (setsize > num_lines)
	YS__errmsg(captr->nodeid,
		   "setsize(%d) > num_lines(%d)", setsize, num_lines);
      nof_sets = num_lines / setsize;
    }
  else
    { /* set size is fully associative; */
      setsize      = num_lines;
      captr->setsz = setsize;
      nof_sets     = 1;
    }


  /*
   * Some facilitator variables to speed up the address manipulation
   * in cache search functions.
   */
  captr->num_lines   = num_lines;
  captr->set_mask    = setsize - 1;
  captr->set_shift   = NumOfBits(setsize, 1);
  captr->idx_mask    = nof_sets - 1;
  captr->idx_shift   = NumOfBits(nof_sets, 1);
  captr->block_mask  = blocksize - 1;
  captr->block_shift = NumOfBits(blocksize, 1);
  captr->tag_shift   = captr->block_shift + captr->idx_shift;


  /*
   * initialize the data SRAM array.
   */
  captr->tags = RSIM_CALLOC(cline_t, num_lines);
  if (captr->tags == NULL)
    YS__errmsg(captr->nodeid,
	       "NewCache(): malloc failed at %s:%i",
	       __FILE__, __LINE__);

  for (i = 0; i < num_lines; i += setsize)
    for (j = 0; j < setsize; j++)
      {
	captr->tags[i+j].index         = i+j;
	captr->tags[i+j].state         = INVALID;
	captr->tags[i+j].tag           = -1;
	captr->tags[i+j].age           = setsize;
	captr->tags[i+j].mshr_out      = 0;
	captr->tags[i+j].pref          = 0;
	captr->tags[i+j].pref_tag_repl = -1;
      }
}

