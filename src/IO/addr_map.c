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

/***************************************************************************/
/*  Per-node address map. Indicates which bus module responds to which     */
/*  addresses. Modules are required to register address ranges during      */
/*  initialization via AddrMap_insert(). Bus masters can find the          */
/*  destination module before starting a bus transaction, e.g. in          */
/*  Cache_receive_addr()                                                   */
/***************************************************************************/



#include <string.h>
#include <malloc.h>
#include <stdlib.h>
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Caches/system.h"
#include "IO/addr_map.h"


ADDR_MAP *AllAddrMaps;

#define ADDR_MAP_INCR 32


/*=========================================================================*/
/* Initialize per-node address map. Allocate space for N pointers and      */
/* allocate N address maps with space for ADDR_MAP_INCR entries initially. */
/*=========================================================================*/

void AddrMap_init(void)
{
  ADDR_MAP *pmap;
  int i, pid;

  AllAddrMaps = RSIM_CALLOC(ADDR_MAP, ARCH_numnodes);
  if (!AllAddrMaps)
    YS__errmsg(0, "Malloc failed at  %s:%i", __FILE__, __LINE__);
  
  for (pid = 0; pid < ARCH_numnodes; pid++)
    {
      AllAddrMaps[pid].map = RSIM_CALLOC(ADDR_MAP_ENTRY, ADDR_MAP_INCR);
      if (!AllAddrMaps[pid].map)
        YS__errmsg(pid, "Malloc failed at %s:%i", __FILE__, __LINE__);

      AllAddrMaps[pid].num_entries = 0;
      AllAddrMaps[pid].max_entries = ADDR_MAP_INCR;
    }
}



int AddrMap_cmp(const void *a, const void *b)
{
  const ADDR_MAP_ENTRY *pa, *pb;

  pa = (ADDR_MAP_ENTRY*)a;
  pb = (ADDR_MAP_ENTRY*)b;

  if (pa->low < pb->low)
    return(-1);
  else
    return(1);
}


/*=========================================================================*/
/* Insert a new address range for 'module' into address map for node 'gid'.*/
/* Address regions start at 'low' and end at 'high'-1, and must not be     */
/* empty or overlap with existing regions.                                 */
/*=========================================================================*/

void AddrMap_insert(int node, unsigned low, unsigned high, int module)
{
  ADDR_MAP *pmap = &(AllAddrMaps[node]);
  int n;

  /* address map full, increase size by ADDR_MAP_INCR entries ---------------*/
  
  if (pmap->num_entries == pmap->max_entries)
    {
      pmap->max_entries += ADDR_MAP_INCR;
      pmap->map = realloc(pmap->map,
			  sizeof(ADDR_MAP_ENTRY) * pmap->max_entries);
      if (!pmap->map)
	YS__errmsg(node, "Realloc failed in %s:%i", __FILE__, __LINE__);
    }

  
  /* check if address space is empty or overlaps with existing region -------*/
  /* ignore duplicate address regions from the same module                   */

  if (high <= low)
    YS__errmsg(node,
	       "Attempt to register empty address region [0x%08X:0X%08X]",
	       low, high);

  for (n = 0; n < pmap->num_entries; n++)
    if (((low <= pmap->map[n].low)  && (high >  pmap->map[n].low)) ||
	((low <  pmap->map[n].high) && (high >= pmap->map[n].high)))
      {
	if ((low == pmap->map[n].low) && (high == pmap->map[n].high) &&
	    (module == pmap->map[n].module))
	  return;
	
	YS__errmsg(node,
		   "Module %i: Address range [0x%08X:0x%08X] overlaps existing region %i",
		   module, low, high, n);
      }

  
  /* insert entry -----------------------------------------------------------*/
  pmap->map[pmap->num_entries].low    = low;
  pmap->map[pmap->num_entries].high   = high;
  pmap->map[pmap->num_entries].module = module;
  pmap->num_entries++;

  qsort(pmap->map, pmap->num_entries, sizeof(ADDR_MAP_ENTRY),
	AddrMap_cmp);
}


  

/*===========================================================================*/
/* Remove entry from adddress map                                            */
/*===========================================================================*/

void AddrMap_remove(int gid, unsigned low, unsigned high, int module)
{
  ADDR_MAP *pmap = &(AllAddrMaps[gid]);
  int n;

  for (n = 0; n < pmap->num_entries; n++)
    if ((low == pmap->map[n].low) &&
	(high == pmap->map[n].high) &&
	(module == pmap->map[n].module))
      {
	pmap->num_entries--;
	if (n != pmap->num_entries)
	  {
	    pmap->map[n].low    = pmap->map[pmap->num_entries].low;
	    pmap->map[n].high   = pmap->map[pmap->num_entries].high;
	    pmap->map[n].module = pmap->map[pmap->num_entries].module;
	  }
	
	qsort(pmap->map, pmap->num_entries, sizeof(ADDR_MAP_ENTRY),
	      AddrMap_cmp);

	return;
      }
}
