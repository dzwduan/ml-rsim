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

#include <string.h>
#include <malloc.h>
#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Caches/pipeline.h"


/* Private functions */
void Cache_print_one_stat      (int nid, char *name, ref_stat_t *oneref,
			        ref_stat_t *total);
void Cache_print_prefetch_stat (int nid, prefetch_stat_t *pp);


/* Conditional print: print only if the "var" is not zero */
#define CondPrintOne(nid, format, var) 	\
	if (var > 0)			\
	   YS__statmsg(nid, format, var);




/*===========================================================================
 * Collect statistics for each request. It's called when memory system
 * finishes a request and is releasing the control to processor. Note 
 * that the "hit_type" and "miss_type" should have been set with correct 
 * values before this function.
 */ 

void Cache_stat_set(CACHE *captr, REQ *req)
{
  ref_stat_t *rstat;

  /*
   * Use a slight different collecting method for uncached requests.
   */
  if (tlb_uncached(req->memattributes))
    {
      if (req->prcr_req_type == WRITE)
	{
	  captr->pstats->iowrite.count++;
	  captr->pstats->iowrite.mlatency += YS__Simtime - req->issue_time;
	}
      else
	{
	  captr->pstats->ioread.count++;
	  captr->pstats->ioread.mlatency += YS__Simtime - req->issue_time;
	}
      return;
    }

  
  if (req->hit_type == UNKHIT)
    YS__errmsg(captr->nodeid,
	       "Cache_stat_set: Unknow hit type %i for a retired request 0x%08X.", req->hit_type, req->paddr);


  /*
   * Prefetch statistics collection is dispersed among functions in
   * l1cache.c l2cache.c.  
   */
  if (req->prefetch)
    return;

  switch (req->prcr_req_type)
    {
    case READ:
      if (req->ifetch)
	rstat = &(captr->pstats->ifetch);
      else
	rstat = &(captr->pstats->read);
      break;

    case WRITE:
      rstat = &(captr->pstats->write);
      break;

    case RMW:
      rstat = &(captr->pstats->rmw);
      break;

    case FLUSHC:
      /*
       * Flush and purge don't care about cache misses.
       */
      rstat = &(captr->pstats->flush);
      rstat->count++;
      rstat->hits[req->hit_type]++;
      rstat->hitcycles[req->hit_type] += YS__Simtime - req->issue_time;
      return;

    case PURGEC:
      rstat = &(captr->pstats->purge);
      rstat->count++;
      rstat->hits[req->hit_type]++;
      rstat->hitcycles[req->hit_type] += YS__Simtime - req->issue_time;
      return;

    default:
      YS__errmsg(captr->nodeid, "Cache_stat_set(): unknown req_type %d.", req->prcr_req_type);
    }

  rstat->count++;
  rstat->hits[req->hit_type]++;
  rstat->hitcycles[req->hit_type] += YS__Simtime - req->issue_time;


  /*
   * If it's an L1 cache miss
   */
  if (req->hit_type >= L2HIT)
    if (req->ifetch)
      rstat->l1imisses[req->miss_type1]++;
    else
      rstat->l1dmisses[req->miss_type1]++;


  /*
   * If it's an L2 cache miss, count the memory latency.
   */
  if (req->hit_type >= MEMHIT)
    {
      rstat->l2misses[req->miss_type2]++;
      rstat->mlatency += req->bus_return_time - req->bus_start_time;
    }
}



/*===========================================================================
 * Report statistics for a cache module. L1 and L2 cache share
 * the same statistics structure.
 */

void Cache_stat_report(int nid, int pid)
{
  CacheStat *pstat = PID2L2C(nid, pid)->pstats;
  WBUFFER   *pwbuf = PID2WBUF(nid, pid);
  int                issuedprefs;
  ref_stat_t         total;

  if (cparam.collect_stats == 0)
    return;

  YS__statmsg(nid, "Cache Statistics\n\n");


  if (pstat->read.count <= 0)
    return;

  /* Each type of access: READ, WRITE, RMW, FLUSH, PURGE. more? -----------*/
  memset((char *)&(total), 0, sizeof(total));
  Cache_print_one_stat(nid,"Total Instr. Fetches: ",&(pstat->ifetch),&total);
  Cache_print_one_stat(nid,"Total Reads:          ",&(pstat->read), &total);
  Cache_print_one_stat(nid,"Total Writes:         ",&(pstat->write),&total);
  Cache_print_one_stat(nid,"Total RMWs:           ",&(pstat->rmw),  &total);
  Cache_print_one_stat(nid,"Total Flushes:        ",&(pstat->flush),0);
  Cache_print_one_stat(nid,"Total Purges:         ",&(pstat->purge),0);
  Cache_print_one_stat(nid,"Total Accesses:       ",&total,         0);
  CondPrintOne(nid, "Snoop Requests: %lld\n\n", pstat->snoop_requests);

  /* I/O (i.e., uncached processor requests) */
  if (pstat->ioread.count)
    YS__statmsg(nid,
		"I/O read:   %12lld\taverage latency: %8.2f\n",
		pstat->ioread.count,
		pstat->ioread.mlatency / pstat->ioread.count);
  
  if (pstat->iowrite.count)
    YS__statmsg(nid,
		"IO write:   %12lld\taverage latency: %8.2f\n",
		pstat->iowrite.count,
		pstat->iowrite.mlatency / pstat->iowrite.count);

  /*
   * Conflicts. Pay attention to whether a two-level cache hierarchy
   * or a one-level cache hierarchy is being simulated. 
   */
  YS__statmsg(nid,
	      "L1 Instruction Total Conflicts: %12lld\n",
	      pstat->shcl_victims1i+pstat->prcl_victims1i+pstat->prdy_victims1i);
  YS__statmsg(nid,
	      "  pr_clean:                     %12lld\n",
	      pstat->prcl_victims1i);
  YS__statmsg(nid,
	      "  sh_clean:                     %12lld\n",
	      pstat->shcl_victims1i);
  YS__statmsg(nid,
	      "  pr_dirty:                     %12lld\n",
	      pstat->prdy_victims1i);

  YS__statmsg(nid,
	      "L1 Data Total Conflicts:        %12lld\n",
	      pstat->shcl_victims1d+pstat->prcl_victims1d+pstat->prdy_victims1d);
  YS__statmsg(nid,
	      "  pr_clean:                     %12lld\n",
	      pstat->prcl_victims1d);
  YS__statmsg(nid,
	      "  sh_clean:                     %12lld\n",
	      pstat->shcl_victims1d);
  YS__statmsg(nid,
	      "  pr_dirty:                     %12lld\n",
	      pstat->prdy_victims1d);

  YS__statmsg(nid,
	      "L2 Total Conflicts:             %12lld\n",
	      pstat->shcl_victims2+pstat->prcl_victims2+pstat->prdy_victims2);
  YS__statmsg(nid,
	      "  pr_clean:                     %12lld\n",
	      pstat->prcl_victims2);
  YS__statmsg(nid,
	      "  sh_clean:                     %12lld\n",
	      pstat->shcl_victims2);
  YS__statmsg(nid,
	      "  pr_dirty:                     %12lld\n",
	      pstat->prdy_victims2);

  YS__statmsg(nid, "\n");

  
  /* Statistics for speculative loads -------------------------------------*/
  if (pstat->sl1.total > 0)
    {
      YS__statmsg(nid, "L1 Speculative Load Statistics:\n");
      Cache_print_prefetch_stat(nid, &(pstat->sl1));
    }

  if (pstat->sl2.total > 0)
    {
      YS__statmsg(nid, "L2 Speculative Load Statistics:\n");
      Cache_print_prefetch_stat(nid, &(pstat->sl2));
    }

  
  /* Statistics for prefetch ----------------------------------------------*/
  if (pstat->l1ip.total > 0)
    {
      YS__statmsg(nid, "L1 Instruction Prefetch Statistics:\n");
      Cache_print_prefetch_stat(nid, &(pstat->l1ip));
    }
  
  if (pstat->l1dp.total > 0)
    {
      YS__statmsg(nid, "L1 Data Prefetch Statistics:\n");
      Cache_print_prefetch_stat(nid, &(pstat->l1dp));
    }
  
  if (pstat->l2p.total > 0)
    {
      YS__statmsg(nid, "L2 Prefetch Statistics:\n");
      Cache_print_prefetch_stat(nid, &(pstat->l2p));
    }

  /* Stalls because of contention on MSHRs --------------------------------*/
  if (cparam.collect_stats > 1)
    {
      YS__statmsg(nid, "L1 Instruction MSHR Stall:\n");
      CondPrintOne(nid, "  write after read:\t%lld",pstat->l1istall.war);
      CondPrintOne(nid, "  full mshr:       \t%lld",pstat->l1istall.full);
      CondPrintOne(nid, "  cohe pending:    \t%lld",pstat->l1istall.cohe);
      CondPrintOne(nid, "  too many coal:   \t%lld",pstat->l1istall.coal);
      CondPrintOne(nid, "  releasing:       \t%lld",pstat->l1istall.release);
      CondPrintOne(nid, "  flush/purge:     \t%lld", pstat->l1istall.flush);

      YS__statmsg(nid, "L1 Data MSHR Stall:\n");
      CondPrintOne(nid, "  write after read:\t%lld",pstat->l1dstall.war);
      CondPrintOne(nid, "  full mshr:       \t%lld",pstat->l1dstall.full);
      CondPrintOne(nid, "  cohe pending:    \t%lld",pstat->l1dstall.cohe);
      CondPrintOne(nid, "  too many coal:   \t%lld",pstat->l1dstall.coal);
      CondPrintOne(nid, "  releasing:       \t%lld",pstat->l1dstall.release);
      CondPrintOne(nid, "  flush/purge:     \t%lld",pstat->l1dstall.flush);

      YS__statmsg(nid, "\nL2 MSHR Stall:\n");      
      CondPrintOne(nid, "  write after read:\t%lld",pstat->l2stall.war);
      CondPrintOne(nid, "  full mshr:       \t%lld",pstat->l2stall.full);
      CondPrintOne(nid, "  cohe pending:    \t%lld",pstat->l2stall.cohe);
      CondPrintOne(nid, "  too many coal:   \t%lld",pstat->l2stall.coal);
      CondPrintOne(nid, "  releasing:       \t%lld",pstat->l2stall.release);
      CondPrintOne(nid, "  flush/purge:     \t%lld",pstat->l2stall.flush);
    }
  
  /*-----------------------------------------------------------------------*/
  
  if ((pwbuf->stall_wb_full) || (pwbuf->stall_match))
    YS__statmsg(nid, "\nL1 Write Buffer Statistics\n\n");
  
  if (pwbuf->stall_wb_full)
    YS__statmsg(nid, "  stall         \t%d\t",   pwbuf->stall_wb_full);
  if (pwbuf->stall_match)
    YS__statmsg(nid, "  read match    \t%d\n\n", pwbuf->stall_match);
}




  
/*===========================================================================
 * Print statistics for one type of request. (read, write, rmw, etc.)
 * A request is classified as one of the following hit types: 
 * L1 hit, L2 hit, and cache miss (memory hit). 
 * Each cache miss is further split into the following types: 
 * coherence miss, cold miss, conflict miss, and capacity miss.
 */

void Cache_print_one_stat(int nid, char *name,
			  ref_stat_t *oneref,
			  ref_stat_t *total)
{
  static char *MissTypeNames[NUM_CACHE_MISS_TYPES] =
  {
    "generic misses  ",
    "cold misses     ",
    "conflict misses ",
    "capacity misses ",
    "coherency misses"
  };

  int i;

  if (oneref->count <= 0)
    return;

  YS__statmsg(nid, "%s %lld\n", name, oneref->count);

  if (oneref->hits[L1IHIT])
    YS__statmsg(nid,
		"  L1 I-Cache Hits:     %12lld  (%6.2f%%)   Average Cycles:  %8lld\n",
		oneref->hits[L1IHIT],
		100.0 * oneref->hits[L1IHIT]/oneref->count,
		(int)oneref->hitcycles[L1IHIT]/oneref->hits[L1IHIT]);

  if (oneref->hits[L1DHIT])
    YS__statmsg(nid,
		"  L1 D-Cache Hits:     %12lld  (%6.2f%%)   Average Cycles:  %8lld\n",
		oneref->hits[L1DHIT],
		100.0 * oneref->hits[L1DHIT]/oneref->count,
		(int)oneref->hitcycles[L1DHIT]/oneref->hits[L1DHIT]);

  if (oneref->hits[L2HIT] > 0)
    YS__statmsg(nid,
		"  L2 Hits:             %12lld  (%6.2f%%)   Average Cycles:  %8lld\n",
		oneref->hits[L2HIT],
		100.0 * oneref->hits[L2HIT]/oneref->count,
		(int)oneref->hitcycles[L2HIT]/oneref->hits[L2HIT]);

  if (oneref->hits[MEMHIT])
    {
      YS__statmsg(nid,
		  "  Misses:              %12lld  (%6.2f%%)\n",
		  oneref->hits[MEMHIT],
		  100.0 * oneref->hits[MEMHIT]/oneref->count);
      YS__statmsg(nid,
		  "  Miss Cycles:         %12lld              Average Latency: %8lld\n",
		  (int)oneref->hitcycles[MEMHIT] / oneref->hits[MEMHIT],
		  (int)oneref->mlatency / oneref->hits[MEMHIT]);
    }

  for (i = 0; i < NUM_CACHE_MISS_TYPES; i++)
    if (oneref->l1imisses[i] > 0)
      YS__statmsg(nid,
		  "  L1I %s %12lld\n",
		  MissTypeNames[i], oneref->l1imisses[i]);

  for (i = 0; i < NUM_CACHE_MISS_TYPES; i++)
    if (oneref->l1dmisses[i] > 0)
      YS__statmsg(nid,
		  "  L1D %s %12lld\n",
		  MissTypeNames[i], oneref->l1dmisses[i]);

  for (i = 0; i < NUM_CACHE_MISS_TYPES; i++)
    if (oneref->l2misses[i] > 0)
      YS__statmsg(nid,
		  "  L2 %s  %12lld\n",
		  MissTypeNames[i], oneref->l2misses[i]);

  
  /* Add to "total" if necessary ------------------------------------------*/
  if (total)
    {
      total->count += oneref->count;
      for (i = 0; i < UNKHIT; i++)
	{
	  total->hits[i] += oneref->hits[i];
	  total->hitcycles[i] += oneref->hitcycles[i];
	}
      
      for (i = 0; i < NUM_CACHE_MISS_TYPES; i++)
	{
	  total->l1imisses[i] += oneref->l1imisses[i];
	  total->l1dmisses[i] += oneref->l1dmisses[i];
	  total->l2misses[i]  += oneref->l2misses[i];
	}
      
      total->mlatency += oneref->mlatency;
    }
  
  YS__statmsg(nid, "\n");
}



/*===========================================================================
 * Print prefetch statistics.
 */

void Cache_print_prefetch_stat(int nid, prefetch_stat_t *pp)
{
  YS__statmsg(nid, "  Total:       \t%12lld\t", pp->total);
  YS__statmsg(nid, "  dropped:     \t%12lld\n", pp->dropped);
  YS__statmsg(nid, "  unnecessary: \t%12lld\t", pp->unnecessary);
  YS__statmsg(nid, "  issued:      \t%12lld\n", pp->issued);
  YS__statmsg(nid, "  useless:     \t%12lld \t(%.2f%%)\n", 
	      pp->useless, 100.0*pp->useless / pp->issued);
  YS__statmsg(nid, "  useful:      \t%12lld \t(%.2f%%)\n", 
	      pp->useful, 100.0*pp->useful / pp->issued);
  YS__statmsg(nid, "  damaging:    \t%12lld \t(%.2f%%)\n", 
	      pp->damaging, 100.0*pp->damaging / pp->issued);

  if (pp->late)
    YS__statmsg(nid, "  late:        \t%12lld \t(Average: %.2f cycles)\n", 
		pp->late, pp->lateness / pp->late);

  if (pp->early)
    YS__statmsg(nid, "  early:       \t%12lld \t(Average: %.2f cycles)\n", 
		pp->early, pp->earliness / pp->early);
  YS__statmsg(nid, "\n");
}



/*===========================================================================
 * clear all cache stats 
 */

void Cache_stat_clear(int nid, int pid)
{
  if (cparam.collect_stats == 0)
    return;

  memset((char*)PID2L2C(nid, pid)->pstats, 0, sizeof(CacheStat));

  PID2WBUF(nid, pid)->stall_wb_full = 0;
  PID2WBUF(nid, pid)->stall_match   = 0;
}




/*===========================================================================
 * Print cache configuration. (need more?)
 */

void Cache_print_params(int nid)
{
  YS__statmsg(nid, "L1 Instruction Cache Configuration\n");

  if (cparam.L1I_perfect)
    YS__statmsg(nid,
		"  size:           %4d kbytes (perfect I-cache with 100%% hit rate)\n\n", cparam.L1I_size);
  else
    {
      YS__statmsg(nid,
		  "  size:           %4d kbytes\n", cparam.L1I_size);
      YS__statmsg(nid,
		  "  line size:      %4d bytes\t", cparam.L1I_line_size);
      YS__statmsg(nid,
		  "associativity: %4d\n", cparam.L1I_set_size);
      YS__statmsg(nid,
		  "  request queue:  %4d\t\tports:         %4d\n",
		  cparam.L1I_req_queue, cparam.L1I_port_num);
      YS__statmsg(nid,
		  "  MSHR count:     %4d\n", cparam.L1I_mshr_num);
      YS__statmsg(nid,
		  "  delay:          %4d cycles\tfrequency:     %4d\n",
		  cparam.L1I_tag_delay, cparam.frequency);
      YS__statmsg(nid,
		  "  prefetch:        %s\n\n",
		  cparam.L1I_prefetch ? " on" : "off");
    }
      
  YS__statmsg(nid,
	      "L1 Data Cache Configuration\n");

  if (cparam.L1D_perfect)
    YS__statmsg(nid,
		"  size:           %4d kbytes (perfect D-cache with 100%% hit rate)\n\n", cparam.L1D_size);
  else
    {
      YS__statmsg(nid,
		  "  size:           %4d kbytes\n", cparam.L1D_size);
      YS__statmsg(nid,
		  "  line size:      %4d bytes\t", cparam.L1D_line_size);
      YS__statmsg(nid,
		  "associativity: %4d\n", cparam.L1D_set_size);
      YS__statmsg(nid,
		  "  request queue:  %4d\t\tports:         %4d\n",
		  cparam.L1D_req_queue, cparam.L1D_port_num);
      YS__statmsg(nid,
		  "  MSHR count:     %4d\n", cparam.L1D_mshr_num);
      YS__statmsg(nid,
		  "  delay:          %4d cycles\tfrequency:     %4d\n",
		  cparam.L1D_tag_delay, cparam.frequency);
      YS__statmsg(nid,
		  "  prefetch:        %s\n\n",
		  cparam.L1D_prefetch ? " on" : "off");
    }
      
  
  YS__statmsg(nid, "L2 Cache Configuration\n");
      
  if (cparam.L2_perfect)
    YS__statmsg(nid,
		"  Size:          %4d kbytes (perfect L-2 cache with 100%% hit rate)\n\n", cparam.L2_size);
  else
    {
      YS__statmsg(nid,
		  "  size:           %4d kbytes\n",
		  cparam.L2_size);
      YS__statmsg(nid,
		  "  line size:      %4d bytes\t",
		  cparam.L2_line_size);
      YS__statmsg(nid,
		  "associativity: %4d\n",
		  cparam.L2_set_size);
      YS__statmsg(nid,
		  "  request queue:  %4d\t\tports:         %4d\n",
		  cparam.L2_req_queue, cparam.L2_port_num);
      YS__statmsg(nid,
		  "  MSHR count:     %4d\n", cparam.L2_mshr_num);
      YS__statmsg(nid,
		  "  tag delay:      %4d cycles\tdata delay:    %4d\n",
		  cparam.L2_tag_delay, cparam.L2_data_delay);
      YS__statmsg(nid,
		  "  frequency:      %4d\n",
		  cparam.frequency);
      YS__statmsg(nid,
		  "  prefetch:        %s\n\n",
		  cparam.L2_prefetch ? " on" : "off");
    }
}

