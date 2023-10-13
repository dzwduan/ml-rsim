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

#ifndef __RSIM_POOL_H__
#define __RSIM_POOL_H__

/*
 * Pool declarations -- for faster memory allocation. Used to minimize the 
 * use of malloc.
 */
typedef struct YS__Pool
{ 
  char   name[32];              /* User defined name                       */
  char **ptrs;
  int    elements;
  int    elem_size;
  int    current;
} POOL;

void  YS__PoolInit      (POOL * pptr, char *name, int objs, int objsz);



#include <malloc.h>
#include <string.h>
#include "Processor/simio.h"
#include "sim_main/tr.pool.h"



/****************************************************************************
 * YS__PoolGetObj:
 * Return a pointer to an object from the pool. If there are no free objects
 * at the time, expand the arry of object pointers, allocate a new chunk of
 * objects, zero them out and insert them into the array.
 ***************************************************************************/

static char *YS__PoolGetObj(POOL *pptr)
{
  char *ptr;

#ifdef DEBUG_POOL

  ptr = (char *) malloc(pptr->elem_size);
  if (ptr == NULL)
    YS__errmsg(0, "Malloc failed in %s:%s\n", __FILE__, __LINE__);

  memset(ptr, 0, pptr->elem_size);   /* no need to assign pnext and pfnext */

  return(ptr);

#else /* Regular pool operation */

  int n;

  TRACE_POOL_getobj1;                /* Getting object from pool           */

  if (pptr->current == 0)
    {
      pptr->ptrs = (char**)realloc(pptr->ptrs,
				   sizeof(char*) * pptr->elements * 2);

      ptr = (char*)malloc(pptr->elements * pptr->elem_size);
      if (ptr == NULL)
	YS__errmsg(0, "Malloc failed in %s:%s\n", __FILE__, __LINE__);

      memset(ptr, 0, pptr->elements * pptr->elem_size);

      for (n = 0; n < pptr->elements; n++, ptr += pptr->elem_size)
	pptr->ptrs[n] = ptr;

      pptr->current = pptr->elements;
      pptr->elements *= 2;
    }

  return(pptr->ptrs[--pptr->current]);
#endif
}



/****************************************************************************
 * YS__PoolReturnObj: Put an object back into its pool
 ***************************************************************************/
extern double YS__Simtime;
static void YS__PoolReturnObj(POOL *pptr, void *optr)
{
#ifdef DEBUG_POOL

  free(optr);
  
#else

  TRACE_POOL_retobj;                           /* Returning object to pool */

  if (pptr->current < pptr->elements)
    pptr->ptrs[pptr->current++] = (char*)optr;

#endif
}


#endif
