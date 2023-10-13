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

/*
 * pool.c
 *
 * Provides essential functions for faster memory allocation of small
 * objects used throughout the memory system simulator.
 */


#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include "sim_main/simsys.h"
#include "Processor/simio.h"
#include "sim_main/tr.pool.h"



/***************************************************************************/
/* POOL Operations: used to manage the allocation of memory used for       */
/* object descriptors.  These reduce the number of calls to malloc by      */
/* maintaining a list of descriptors that can be allocated for new         */
/* objects and then returned to the pool for reuse when the object is      */
/* deleted or the simulation reset.  Pools use malloc to obtain large      */
/* blocks of memory consisting of several objects and then parcels         */
/* them out in response to the "new" object operation.  Pools never        */
/* return memory to the system, so the size of the pool only increases.    */
/* In order to use a pool, the first two elements of each structure must   */
/* be char pointers "pnxt" and "pfnxt", which maintain the pool lists      */
/***************************************************************************/

/*
 * YS__PoolInit: start out a new pool of objects, specifying the number to
 * allocate with each increment and the size of each object
 */
void YS__PoolInit(POOL *pptr, char *name, int objs, int objsz)
{
#ifndef DEBUG_POOL
  int n;
  char *elemns;
  
  pptr->ptrs = (char**)malloc(objs * sizeof(char*));
  if (pptr->ptrs == NULL)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);
  
  elemns = (char*)malloc(objs * objsz);
  if (elemns == NULL)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);

  memset(elemns, 0, objs * objsz);
  
  for (n = 0; n < objs; n++, elemns += objsz)
    pptr->ptrs[n] = elemns;

#endif

  pptr->elements  = objs;
  pptr->elem_size = objsz;
  pptr->current   = objs;
  
  strncpy(pptr->name, name, 31);    /* copy the name in                 */
  pptr->name[31] = '\0';
}
