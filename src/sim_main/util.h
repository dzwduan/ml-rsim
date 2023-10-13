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

#ifndef __RSIM_UTIL_H__
#define __RSIM_UTIL_H__

#include <stdio.h>
#include <limits.h>

/*********************************************************************/
/* Some typedefs that will make typing and modification easier.      */
/*********************************************************************/

#if !defined(sgi) && !defined(hp800) && !defined(HP_UX) && !defined(sparc) && !defined(linux)
typedef unsigned long   u_long;
typedef unsigned int    u_int;
typedef unsigned short  u_short;
typedef unsigned char   u_char;
#else
#include <sys/types.h>
#endif

#if !defined(HP_UX) && !defined(sparc)
#endif
typedef double          	rsim_time_t;
typedef unsigned 		UINT32;
typedef unsigned long long 	UINT64;
typedef long long 		INT64;
typedef int 			INT32;

#ifndef UINT32_MAX
#define UINT32_MAX UINT_MAX
#endif
#ifndef INT32_MAX
#define INT32_MAX INT_MAX
#define INT32_MIN INT_MIN
#endif

typedef void (*func) ();
typedef int  (*cond) ();



/*********************************************************************/
/* MACROS or DEFINITIONS                                             */
/*********************************************************************/

#define RSIM_CALLOC(type, count)  ((type *) calloc(sizeof(type), count))

#ifndef FALSE
#define FALSE                    0
#endif
#ifndef TRUE
#define TRUE                     1
#endif

#undef PAGEOFFSET
#define PAGEOFFSET(addr)        (((unsigned) addr) &  0xFFF)
#define PAGETOBYTES(pfn)        (pfn << 12)
#define SAMEPAGE(a1, a2)        (((a1) >> 12) == ((a2) >> 12))

#define WORDSZ 4     /* word size =  4 bytes */

#if defined(sgi) || defined(linux)
#include <sys/param.h>
#endif

#ifndef MAX
#define MAX(a,b)        (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b)        (((a) < (b)) ? (a) : (b))
#endif

#define PSDELAY



/*****************************************************************************/
/* Utility operations -- provide some useful functions                       */
/*****************************************************************************/

#define FILEOP_RETRY_PERIOD 300

void YS__errmsg  (int node, const char *fmt, ...); /* Fatal error message    */
void YS__warnmsg (int node, const char *fmt, ...); /* Prints warning message */
void YS__statmsg (int node, const char *fmt, ...); /* Prints debug message   */
void YS__logmsg  (int node, const char *fmt, ...); /* Prints debug message   */

void YS__fmsg    (int file, const char *fmt, ...); /* Prints msg to file     */

char *YS__strerror (int);


double GetSimTime  ();                /* Returns the current simulation time */

int NumOfBits  (int a, int power_of_2);
static int ToPowerOf2 (int n)
{
  int t = 0;

  if (n <= 0)
    return 0;

  n += n - 1;
  while (n >>= 1)
    t++;
  return (1 << t);
}


void PrintTime(double, int);

#endif
