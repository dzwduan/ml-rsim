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

#ifndef __RSIM_STAT_H__
#define __RSIM_STAT_H__



/*****************************************************************************/
/* STATREC: used for calculating a variety of statistics                     */
/*****************************************************************************/

struct YS__Qe;

typedef struct YS__Stat
{
  struct YS__Qe *next;      /* Pointer to the element after this one         */
  int       type;           /* PNTSTATTYPE or INTSTATTYPE                    */
  int       id;             /* System defined unique ID                      */
  char      name[32];       /* User assignable name for dubugging            */

  /* STATRECORD Fields */
  int       maxval, minval; /* Max and min update values encountered         */
  long long samples;        /* Number of updates                             */
  int       meanflag;       /* MEANS or NOMEANS                              */
  long long sum;            /* Accumulated sum of update values              */
  long long sumsq;          /* Accumulated sum of squares of update values   */
  long long sumwt;          /* Accumulated sum of weights                    */
  long long*hist;           /* Pointer to a histogram array                  */
  int       hist_low;
  int       hist_high;
  int       hist_incr;
  int       hist_shift;
  int       hist_offset;
  int       histspecial;    /* Report only non-zero histogram bins?          */
  int       bins;           /* Number of bins of histograms                  */
  long long lastt;          /* Last interval point entered with Update()     */
  int       lastv;          /* Last interval value entered with Update()     */
  long long interval;       /* The sampling interval                         */
  int       intervalerr;    /* Nonzero => a negative interval encountered    */
} STATREC;



/* Statistics Record Characteristics */

#define NOMEANS             0           /* Means not computed                */
#define MEANS               1           /* Means are computed                */
#define HIST                2           /* Histogram is collected            */
#define NOHIST              3           /* Histogram not collected           */
#define POINT               4           /* Point statistics record type      */
#define INTERVAL            5           /* Interval statistics record type   */
#define HISTSPECIAL         6           /* Collect histograms, but only show */

STATREC   *NewStatrec(int, const char *, int, int, int, int, int, int);
void       StatrecUpdate  (STATREC *, int, long long);
void       StatrecReset   (STATREC *);       /* Resets the statrec           */
void       StatrecReport  (int, STATREC *);  /* Displays a statrec report    */
long long  StatrecSamples (STATREC *);       /* number of samples            */
double     StatrecMean    (STATREC *);       /* mean                         */
long long  StatrecSum     (STATREC *);       /* sum                          */
double     StatrecSdv     (STATREC *);       /* standard deviation           */

/*****************************************************************************/

#include "Processor/simio.h"
#include "Processor/mainsim.h"
#include "sim_main/simsys.h"
#include "sim_main/userq.h"
#include "sim_main/tr.stat.h"

#if 0
                                     /* Max value for the low histogram bin  */
#define LOWBIN   srptr->hist[srptr->bins]
                                     /* Min value for the high histogram bin */
#define HIGHBIN  srptr->hist[srptr->bins+1]
                                     /* Historgram bin size                  */
#define BININC   srptr->hist[srptr->bins+2]



static void StatrecUpdatePoint(STATREC *srptr, double v, double t)
{
  extern double   YS__Simtime;
  int i;

  if (srptr->type != PNTSTATTYPE)
    YS__errmsg("Wrong Statrec\n");

  if (srptr->samples == 0)
    {                                    /* This is the first sample         */
      srptr->maxval = v;
      srptr->minval = v;
    }
       
  if (srptr->meanflag == MEANS)
    {                                    /* Collect means                    */
      srptr->sum += v*t;                 /* Accumulate sums                  */
      srptr->sumsq += t*v*v;
      srptr->sumwt += t;
    }

  if (srptr->hist)
    {                                    /* Collect histograms               */
      for (i = 0; i < srptr->bins-1; i++)           /* Find the bin          */
	if (v < LOWBIN + i*BININC)
	  break;  
      srptr->hist[i] += t;                          /* and increment it      */
    }

  if (v > srptr->maxval)
    srptr->maxval = v;

  if (v < srptr->minval)
    srptr->minval = v;

  srptr->samples++;
  srptr->time1 = YS__Simtime;            /* New end of sampling interval     */
  TRACE_STATREC_pupdate;                 /* Updating point stats record ...  */

  return;
}




static void StatrecUpdateInterval(STATREC *srptr, double v, double t)
{
  extern double   YS__Simtime;
  int i;

  if (srptr->type != INTSTATTYPE)
    YS__errmsg("Wrong Statrec %s\n", srptr->name);
  
  if (srptr->samples == 0)
    {                                    /* This is the first sample         */
      srptr->lastt = t;                  /* Updates of interval statrecs     */
      srptr->lastv = v;                  /* require 2 values to compute an   */
                                         /* interval, so this  sample only   */
      srptr->maxval = 0.0;               /* initializes the first of them    */
      srptr->minval = 0.0;               /* Max & min refers to intervals    */
    }                                    /* not values                       */
  else
    {                                    /* This is not the first sample     */
      srptr->interval = t - srptr->lastt;          /* New interval computed  */
      if (srptr->interval < 0.0)
	srptr->intervalerr = 1;                    /* Not a valid interval   */

      if (srptr->meanflag == MEANS)                /* collect means          */
	{                                          /* accumulate sums        */
	  srptr->sum += srptr->lastv * srptr->interval;
	  srptr->sumsq += srptr->interval * srptr->lastv * srptr->lastv; 
	  srptr->sumwt += srptr->interval;                
	}

      if (srptr->hist)
	{                                          /* Collect histograms     */
	  if (srptr->interval >= 0.0)
	    {                                      /* Inteval is valid       */
	      for (i = 0; i < srptr->bins-1; i++)  /* Find the bin           */
		if (srptr->lastv < LOWBIN + i*BININC)
		  break;                           /* and increment it       */
	      srptr->hist[i] += srptr->interval;
	    }
	}

      if (v > srptr->maxval)
	srptr->maxval = v;

      if (v < srptr->minval)
	srptr->minval = v;

      srptr->lastv = v;                  /* Remember last value & time       */
                                         /* to compute next interval         */
      srptr->lastt = t;
    }

  srptr->samples++;
  srptr->time1 = YS__Simtime;            /* New end of sampling interval     */
  TRACE_STATREC_iupdate;                 /* Updating interval stats record   */
}
#endif

#endif

