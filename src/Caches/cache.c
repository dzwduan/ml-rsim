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
#include "Processor/simio.h"
#include "Processor/capconf.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"



/*
 * Determines whether a line, addressed by virtual address "vaddr" and
 * physical address "paddr", is present in the cache or not.
 *
 * Returns: 0 -- hit
 *          1 -- present miss (tag in cache, but INVALID due to COHE
 *               or flush/purge)
 *          2 -- total miss
 * If there is a match (return 0 or 1), store the line pointer into
 * parameter "*cline".
 */
int Cache_search(CACHE *captr, unsigned vaddr, unsigned paddr, cline_t **cline)
{
  cline_t  *pline;
  unsigned  tag;
  int       i, lineidx;

  /*
   * Note: L1 cache is virtually indexed. L2 cache is physically indexed.
   */
  if (captr->type == L1CACHE)
    lineidx = ADDR2SET(vaddr);
  else
    lineidx = ADDR2SET(paddr);

  tag    = PADDR2TAG(paddr);
  *cline = &(captr->tags[lineidx]); /* default return value */

  /* 
   * loop through the set looking for a match 
   */
  for (i = 0; i < captr->setsz; i++, lineidx++)
    {
      pline = &(captr->tags[lineidx]);
      if (pline->tag == tag)
	{
	  *cline = pline;
	  if (pline->state != INVALID)
	    return 0;
	  else
	    return 1;
	}
    }

  return 2;
}



/*
 * Updates the cache ages in the set to indicate a hit on a reference;
 * age is not maintained for an infinite cache because there will be
 * no replacement.
 */
int Cache_hit_update(CACHE *captr, cline_t *pline, REQ * req)
{
  unsigned  lineidx, i, oldage;
  cline_t  *cline;
  int       hit_cpref = 0; /* hit cache-initiated prefetch? */

  /*
   * Update ages only if LRU is used and cache is not infinite.
   */
  if (captr->replacement != LRU || captr->size == INFINITE)
    return 0;

  if (pline->pref && !req->prefetch)
    {
      /*
       * It's now been demand fetched! The blame for replacing
       * a line goes to the demand access, not the prefetch.
       * Also account for the earliness factor.
       */
      switch (pline->pref)
	{
	case 4:              /* @@@ fix @@@ */
	  captr->pstats->l1dp.useful++;
	  captr->pstats->l1dp.early++;
	  captr->pstats->l1dp.earliness += YS__Simtime - pline->pref_time;
	  hit_cpref = 1;
	  break;

	case 8:
	  captr->pstats->l2p.useful++;
	  captr->pstats->l2p.early++;
	  captr->pstats->l2p.earliness += YS__Simtime - pline->pref_time;
	  hit_cpref = 1;
	  break;

	case 1:
	  captr->pstats->sl1.useful++;
	  captr->pstats->sl1.early++;
	  captr->pstats->sl1.earliness += YS__Simtime - pline->pref_time;
	  break;

	case 2:
	  captr->pstats->sl2.useful++;
	  captr->pstats->sl2.early++;
	  captr->pstats->sl2.earliness += YS__Simtime - pline->pref_time;
	  break;

	default:
	  YS__warnmsg(captr->nodeid, "Cache_hit_update: unknow prefetch type");
	  break;
	}

      pline->pref = 0;
      pline->pref_tag_repl = -1;
    }

  lineidx = pline->index & (~captr->set_mask);
  oldage  = pline->age;

  for (i = 0; i < captr->setsz; i++, lineidx++)
    {
      cline = &(captr->tags[lineidx]);
      if (cline == pline)
	cline->age = 1;  /* age of line hit is made 1 */
      else if (cline->age < oldage)
	/* age less than that the hit one is increased by one. */
	cline->age++;
    }

  return hit_cpref;
}



/*
 * Updates cache age on a present miss. We distinguish present miss
 * from other types of misses only because they are easier to handle.
 */
void Cache_pmiss_update(CACHE * captr, REQ *req, int index, int prefonly)
{
  int      lineidx, i;
  int      oldage;
  cline_t *cline;

  /*
   * update ages only if LRU and not infinite cache are true.
   */
  if (captr->replacement != LRU || captr->size == INFINITE)
    return;

  oldage  = captr->tags[index].age;
  lineidx = index & (~captr->set_mask);

  for (i = 0; i < captr->setsz; i++, lineidx++)
    {
      cline = &(captr->tags[lineidx]);
      if (lineidx == index)
	{
	  if (prefonly && req->prefetch)
	    {
	      cline->pref = req->prefetch;
	      cline->pref_time = YS__Simtime;
	    }
	  else    /* either only demand access or a late prefetch */
	    cline->pref = 0;

	  /* this happens regardless, since nothing was replaced */
	  cline->pref_tag_repl = -1;
	  cline->age = 1;
	}
      else if (cline->age < oldage)
	/* age less than that line is increased by one */
	cline->age++;
    }
}



/*
 * This function occurs in the case of a total miss. The function must
 * pick a victim and then update the age accordingly. The heuristic used
 * here is as follows:
 *    1, first, an INVALID line first;
 *    2, then, the oldest SH_CL line;
 *    3, then, the oldest PR_CL line;
 *    4, finally, the oldest PR_DY line.
 * If all lines in the set have upgrades outstanding, then this function
 * cannot replace a line and thus must return 0. Otherwise, return 1 to 
 * indicate success, and put the line address into "*cline".
 */
int Cache_miss_update(CACHE *captr, REQ *req, cline_t **cline, int prefonly)
{
  cline_t *pline;
  int i, lineidx, repl_age, victim;
  int bestinv     = -1;
  int bestclean   = -1;
  int bestprivate = -1;
  int bestdirty   = -1;
  int inv_age     = -1; /* invalid cache line with the largest age */
  int clean_age   = -1; /* valid clean cache line with the largest age */
  int private_age = -1; /* valid private clean cache line with largest age */
  int dirty_age   = -1; /* dirty cache line with the largest age */
  int reqtag      = (req->paddr >> captr->tag_shift);
  int baseidx     = (*cline)->index & (~captr->set_mask);

  /*
   * Find a victim according to the heuristic rule described above.
   */
  for (lineidx = baseidx, i = 0; i < captr->setsz; i++, lineidx++)
    {
      pline = &(captr->tags[lineidx]);

      if (pline->mshr_out)
	continue;

      switch (pline->state)
	{
	case INVALID:
	  if (inv_age < pline->age)
	    {
	      inv_age = pline->age;
	      bestinv = i;
	    }
	  break;

	case SH_CL:
	  if (clean_age < pline->age)
	    {
	      clean_age = pline->age;
	      bestclean = i;
	    }
	  break;

	case PR_CL:
	  if (private_age < pline->age)
	    {
	      private_age = pline->age;
	      bestprivate = i;
	    }
	  break;

	case PR_DY:
	  if (dirty_age < pline->age)
	    {
	      dirty_age = pline->age;
	      bestdirty = i;
	    }
	  break;

	default:
	  break;
	}

      if (pline->state != INVALID && pline->pref_tag_repl == reqtag)
	{
	  pline->pref_tag_repl = -1;
	  if (pline->pref == 1)
	    captr->pstats->sl1.damaging++;
	  else if (pline->pref == 2)
	    captr->pstats->sl2.damaging++;
	  else if (pline->pref == 4)          /* @@@ fix @@@ */
	    captr->pstats->l1dp.damaging++;
	  else if (pline->pref == 8)
	    captr->pstats->l2p.damaging++;
	}
    }

  if (bestinv != -1)
    {
      victim = bestinv;
      repl_age = inv_age;
    }
  else if (bestclean != -1)
    {
      victim = bestclean;
      repl_age = clean_age;
    }
  else if (bestprivate != -1)
    {
      victim = bestprivate;
      repl_age = private_age;
    }
  else if (bestdirty != -1)
    {
      victim = bestdirty;
      repl_age = dirty_age;
    }
  else   /* all lines in set have outstanding upgrades! */
    return 0;


  /*
   * Found the victim, update ages
   */
  for (lineidx = baseidx, i = 0; i < captr->setsz; i++, lineidx++)
    {
      pline = &(captr->tags[lineidx]);
      if (i == victim) {
	if (pline->state != INVALID)
	  {
	    switch (pline->pref)
	      {
	      case 4:      /* @@@ fix @@@ */
		captr->pstats->l1dp.useless++;
		break;
	      case 8:
		captr->pstats->l2p.useless++;
		break;
	      case 1:
		captr->pstats->sl1.useless++;
		break;
	      case 2:
		captr->pstats->sl2.useless++;
		break;
	      default:
		break;
	      }
	  }
	
	/*
	 * Save the replaced line's tag so that we can check if this
	 * prefetch replaces a useful line. If the replaced line is
	 * referenced before the prefetch line, the prefetch line
	 * is counted as a "damaging" prefetch.
	 */
	if (req->prefetch && prefonly)
	  {
	    if (pline->state != INVALID)
	      pline->pref_tag_repl = pline->tag;
	    else
	      pline->pref_tag_repl = -1;

	    pline->pref = req->prefetch;
	    pline->pref_time = YS__Simtime;
	  }
	else
	  {
	    pline->pref_tag_repl = -1;
	    pline->pref = 0;
	  }
	pline->age = 1;
      }
      else if (pline->age < repl_age)
	pline->age++;
    }

  *cline = &(captr->tags[baseidx + victim]);
  return 1;
}




/*
 * Used by modules below cache system, such as bus, memory controller,
 * and DRAM backend. When perfect cache (either L1 or L2) is simulated, 
 * those modules are not necessary enabled because nothing would go there.
 */
int Cache_perfect(void)
{
  return (cparam.L1I_perfect || cparam.L1D_perfect || cparam.L2_perfect);
}


