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

/*****************************************************************************/
/*  Per-node address map. Indicates which bus module responds to which       */
/*  addresses. Modules are required to register address ranges during        */
/*  initialization via AddrMap_insert(). Bus masters can find the            */
/*  destination module before starting a bus transaction, e.g. in            */
/*  Cache_receive_addr()                                                     */
/*****************************************************************************/

#ifndef __RSIM_ADDR_MAP_H__
#define __RSIM_ADDR_MAP_H__

#include <stdio.h>


#define TARGET_ALL_CPUS     -1
#define TARGET_ALL_IOS      -2
#define TARGET_ALL_MODULES  -3



/* a single address map entry -----------------------------------------------*/
typedef struct
{
  unsigned low;
  unsigned high;
  int      module;
} ADDR_MAP_ENTRY;



/* per-node address map structure -------------------------------------------*/
typedef struct
{
  ADDR_MAP_ENTRY *map;
  int num_entries;
  int max_entries;
} ADDR_MAP;



/* Exported global variables: list of all address maps ----------------------*/
extern ADDR_MAP *AllAddrMaps;


void AddrMap_init(void);
void AddrMap_insert(int, unsigned, unsigned, int);
void AddrMap_remove(int, unsigned, unsigned, int);

/*===========================================================================*/
/* Find the module in node 'gid' that responds to 'addr'.                    */
/*===========================================================================*/

static int AddrMap_lookup(int gid, unsigned addr)
{
  ADDR_MAP *pmap = &(AllAddrMaps[gid]);
  int n;

  for (n = 0; n < pmap->num_entries; n++)
    if ((addr >= pmap->map[n].low) && (addr < pmap->map[n].high))
      return(pmap->map[n].module);

  YS__errmsg(gid, "Unable to determine target module for address 0x%x", addr);
  return(-1);
}



#endif
