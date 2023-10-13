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
  stat.c

  This file provides functions related to the YACSIM "Statrec" statistics
  collection library.
  */


#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/tr.stat.h"
#include <malloc.h>
#include <string.h>
#include <math.h>



#define IsPowerOf2(x) ((((x)-1) & (x)) == 0)


/***************************************************************************/
/* STATREC Operations: Statistics records are used to collect and          */
/* display statistics for a simulation.  Each STATREC can compute a        */
/* running mean, standard deviation, max, mean and histogram.  Collected   */
/* statistics may be displayed in a standard format, or they can be        */
/* constructed in any user defined format using operations that return     */
/* all relevant STATREC information                                        */
/***************************************************************************/
/* Creates and returns a pointer to a new statistics record                */


STATREC *NewStatrec(int node, const char *srname, int typ, int meanflg,
		    int histflg, int nbins, int lowbin, int highbin)
{
  STATREC *srptr;
  int i;
  double d;

  PSDELAY;

  srptr = (STATREC*)YS__PoolGetObj(&YS__StatPool);
  srptr->id = YS__idctr++;
  strncpy(srptr->name, srname, 31);                       /* copy its name */
  srptr->name[31]    = '\0';
  srptr->meanflag    = meanflg;
  srptr->sum         = 0;
  srptr->sumsq       = 0;
  srptr->sumwt       = 0;
  srptr->samples     = 0;
  srptr->interval    = 0;              /* Interval of last interval staterec
					  update                           */
  srptr->intervalerr = 0;              /* Set if interval of interval
					  statrec is ever neg              */

  if (typ == POINT)
    srptr->type = PNTSTATTYPE;
  else if (typ == INTERVAL)
    srptr->type = INTSTATTYPE;
  else
    YS__errmsg(node, "Invalid statistics record type, use POINT or INTERVAL");

  if (histflg == HIST || histflg == HISTSPECIAL)
    {                                  /* Histograms will be collected     */
      srptr->histspecial = (histflg == HISTSPECIAL);

      d = (double)(highbin - lowbin) / (double)nbins;
      i = (int)d;
      while (((double)rint(d) != d) || (!IsPowerOf2(i)))
	{
	  highbin++;
	  
	  d = (double)(highbin - lowbin) / (double)nbins;
	  i = (int)d;
	}

      srptr->bins = nbins+1;

      srptr->hist = (long long*)malloc((srptr->bins)*sizeof(long long)); 
      if (srptr->hist == NULL)
	YS__errmsg(node, "Malloc fails in NewStatrec");

      for (i = 0; i < srptr->bins; i++)
	srptr->hist[i] = 0;                            /* Clear all bins   */

      if (nbins > 0)
	srptr->hist_incr = (highbin - lowbin) / nbins;         /* Bin size */
      else
	srptr->hist_incr = 0;

      srptr->hist_shift = NumOfBits((highbin - lowbin) / nbins, 1);
      srptr->hist_offset = srptr->hist_incr - 1 - srptr->hist_low;
      srptr->hist_low  = lowbin;
      srptr->hist_high = highbin;
      TRACE_STATREC_new2;               /* Creating statistics record ...  */
    }
  else if (histflg == NOHIST)
    {                                   /* Histograms not collected        */
      srptr->hist = NULL;
      srptr->bins = 0;
      TRACE_STATREC_new1;               /* Creating Statistics record ...  */
    } 
  else
    YS__errmsg(node,
	       "Invalid histogram flag, use HIST, NOHIST, or HISTSPECIAL");

  return srptr;
}



/***************************************************************************/

int YS__StatrecId(STATREC *srptr)      /* Returns the system defined ID or 0
			                  if TrID is 0                     */
{
  if (TraceIDs)
    return srptr->id;
  else
    return 0;
}


/***************************************************************************/

void StatrecReset(STATREC *srptr)        /* Resets the statrec             */
{
  int i;

  PSDELAY;

  if (srptr->hist)                       /* Clear all histogram bins       */
    for (i = 0; i < srptr->bins; i++)
      *(srptr->hist+i) = 0;

  if (srptr->meanflag == MEANS)
    {                                    /* Reset all sum-related fields   */
      srptr->sum = 0;
      srptr->sumsq = 0;
      srptr->sumwt = 0;
    }

  srptr->samples = 0;                     /* Clear sample count            */
  srptr->intervalerr = 0;        /* Set if interval of interval statrec is
				    ever neg                               */
  srptr->interval = 0;           /* Interval of last interval update       */
  TRACE_STATREC_reset;           /* Resetting statistics records ...       */
}




/***************************************************************************/




void StatrecUpdate(STATREC *srptr, int v, long long t)
{
  int i;

  PSDELAY;

  if (srptr->type == PNTSTATTYPE)
    {                                    /* Its a point statistics record  */
      if (srptr->samples == 0)
	{                                /* This is the first sample       */
	  srptr->maxval = v;
	  srptr->minval = v;
	}
       
      if (srptr->meanflag == MEANS)
	{                                /* Collect means                  */
	  srptr->sum += v*t;             /* Accumulate sums                */
	  srptr->sumsq += t*v*v;
	  srptr->sumwt += t;
	}

      if (srptr->hist)                   /* Collect histograms             */
	{
	  if (v + srptr->hist_low <= srptr->hist_high)
	    srptr->hist[(v + srptr->hist_offset) >> srptr->hist_shift] += t;
	}

      if (v > srptr->maxval)
	srptr->maxval = v;

      if (v < srptr->minval)
	srptr->minval = v;

      srptr->samples++;
      TRACE_STATREC_pupdate;             /* Updating point stats record ...*/

      return;
    }


  if (srptr->type == INTSTATTYPE)
    {                                    /* This is an interval statistics
					     record                        */
      if (srptr->samples == 0)
	{                                /* This is the first sample       */
	  srptr->lastt = t;              /* Updates of interval statrecs   */
	  srptr->lastv = v;              /* require 2 values to compute an */
	                                 /* interval, so this  sample only */
	  srptr->maxval = 0;             /* initializes the first of them  */
	  srptr->minval = 0;             /* Max & min refers to intervals  */
	}                                /* not values                     */
      else
	{                                /* This is not the first sample   */
	  srptr->interval = t - srptr->lastt;      /* New interval computed*/
	  if (srptr->interval < 0)
	    srptr->intervalerr = 1;                /* Not a valid interval */

	  if (srptr->meanflag == MEANS)            /* collect means        */
	    {                                      /* accumulate sums      */
	      srptr->sum += srptr->lastv * srptr->interval;
	      srptr->sumsq += srptr->interval * srptr->lastv * srptr->lastv; 
	      srptr->sumwt += srptr->interval;                
	    }

	  if (srptr->hist)                         /* Collect histograms   */
	    if (srptr->interval >= 0.0)
	      srptr->hist[(srptr->lastv + srptr->hist_offset) >> srptr->hist_shift] +=
		srptr->interval;

	  if (v > srptr->maxval)
	    srptr->maxval = v;

	  if (v < srptr->minval)
	    srptr->minval = v;

	  srptr->lastv = v;              /* Remember last value & time     */
	                                 /* to compute next interval       */
	  srptr->lastt = t;
	}

      srptr->samples++;
      TRACE_STATREC_iupdate;             /* Updating interval stats record */

      return;
    }
}




/***************************************************************************/


/* Generates and displays a statrec report*/
void StatrecReport(int node, STATREC *srptr)
{
  int       i ,j;
  long long total = 0;
  double    stddev;
  double    x;
  
  PSDELAY;

  if (srptr->type == PNTSTATTYPE)
    {                                    /* This is a point statrec        */
      YS__statmsg(node,
		  "\nStatistics Record %s:\n",
		  srptr->name);
      YS__statmsg(node,
		  "  Number of samples = %lld,   Max Value = %i,   Min Value = %i\n",
		  srptr->samples,
		  srptr->maxval,
		  srptr->minval);

      if (srptr->sumwt != 0.0 && srptr->sumwt != 1.0)
	{                                    /* calculate standard dev.    */
	  x = (((double)srptr->sumsq-((double)srptr->sum/(double)srptr->sumwt)
		*((double)srptr->sum/(double)srptr->sumwt)*(double)srptr->sumwt)/((double)srptr->sumwt-1.0));
	  if (x > 0.0)
	    stddev = sqrt(x);
	  else 
	    stddev = 0.0;
	}
      else
	stddev = 0.0;

      
      if (stddev == -0.0)
	stddev = 0.0;

      
      if (srptr->meanflag == MEANS)     /* print out means & stddev also   */
	{
	  YS__statmsg(node,
		      "  Mean = %g,   Standard Deviation = %g\n",
		      (double)srptr->sum/(double)srptr->sumwt,
		      stddev);
	}

      
      if (srptr->hist)
	{                                           /* print out histogram */
	  for (i = 0; i < srptr->bins; i++)
	    total += srptr->hist[i];

	  if (total != 0)
	    {
	      YS__statmsg(node, "    Bin            Value\n");
	      YS__statmsg(node, "  -------        ---------\n");

	      for (i = 0; i < srptr->bins; i++)
		{
		  if (!srptr->histspecial || (srptr->hist[i] > 0))
		    {
		      YS__statmsg(node,
				  " %8.2f  %15lld (%6.2f%%) |",
				  (double)srptr->hist_low + i * (double)srptr->hist_incr,
				  srptr->hist[i],
				  (double)(((double)srptr->hist[i]/(double)total)*100.0));
		      
		      for (j = 0;
			   j < (int)(((double)srptr->hist[i]/(double)total)*40.0);
			   j++) 
			YS__statmsg(node, "%s","*");

		      YS__statmsg(node, "\n");
		    }
		}
	    }
	  else
	    YS__statmsg(node, "All histogram entries = 0");

	  if (srptr->maxval+srptr->hist_low > srptr->hist_high)
	    YS__statmsg(node,
			"Warning: maximum value exceeds histogram range (%i, %i)\n",
			srptr->hist_low,srptr->hist_high);
			
	}
    }

  
  else if (srptr->type == INTSTATTYPE)
    {                                          /* interval statistics recd.*/
      YS__statmsg(node, "\nStatistics Record %s:\n", srptr->name);

      if (srptr->intervalerr != 0)
	YS__statmsg(node, "Invalid statistics, interval error");

      YS__statmsg(node,
		  "  Number of intervals = %lld, Max Value = %i, Min Value = %i\n",
		  srptr->samples-1,
		  srptr->maxval,
		  srptr->minval);

      if (srptr->sumwt != 0 && srptr->sumwt != 1)
	{                                   /* calculate standdard deviation */
	  x = (((double)srptr->sumsq-((double)srptr->sum/(double)srptr->sumwt)
		*((double)srptr->sum/(double)srptr->sumwt)*(double)srptr->sumwt)/((double)srptr->sumwt-1.0));
	  if (x > 0.0)
	    stddev = sqrt(x);
	  else 
	    stddev = 0.0;
	}
      else
	stddev = 0.0;

      if (stddev == -0.0)
	stddev = 0.0;

      if (srptr->meanflag == MEANS)                  /* print out means also */
	YS__statmsg(node,
		    "  Mean = %g,   Standard Deviation = %g\n",
		    (double)srptr->sum/(double)srptr->sumwt,
		    stddev);

      if (srptr->hist)
	{                                        /* print out histogram bins */
	  for (i = 0; i < srptr->bins; i++)
	    total += srptr->hist[i];

	  if (total != 0.0)
	    {
	      YS__statmsg(node, "    Bin            Value\n");
	      YS__statmsg(node, "  -------        ---------\n");

	      for (i = 0; i < srptr->bins; i++)
		{
		  if (!srptr->histspecial || (srptr->hist[i] > 0))
		    {
		      YS__statmsg(node,
				  " %8.2f  %15lld (%6.2f%%) |",
				  (double)srptr->hist_low+i*(double)srptr->hist_incr,
				  srptr->hist[i],
				  (double)(((double)srptr->hist[i]/(double)total)*100.0));

		      for (j = 0;
			   j < (int)(((double)srptr->hist[i]/(double)total)*40.0);
			   j++) 
			YS__statmsg(node, "%s","*");
		      YS__statmsg(node, "\n");
		    }
		}
	    }
	  else
	    YS__statmsg(node, "All histogram entries = 0");
	}
   }
  else
    YS__statmsg(node,
		"Invalid statistic record type, use POINT or INTERVAL");

  YS__statmsg(node, "\n");
}



/*****************************************************************************/

long long StatrecSamples(STATREC *srptr) /* Returns the number of samples    */
{
  PSDELAY;

  return srptr->samples; 
}


/*****************************************************************************/

int StatrecMaxVal(STATREC *srptr)        /* Returns the maximum sample value */
{
  PSDELAY;

  return srptr->maxval; 
}


/*****************************************************************************/

int StatrecMinVal(STATREC *srptr)        /* Returns the minimum sample value */
{
  PSDELAY;

  return srptr->minval; 
}


/*****************************************************************************/

int StatrecBins(STATREC *srptr)          /* Returns the number of bins       */
{
  PSDELAY;

  if (srptr->hist)
    return srptr->bins;

  return 0;
}


/*****************************************************************************/

int StatrecLowBin(STATREC *srptr)        /* Returns the low bin upper bound  */
{
  PSDELAY;

  if (srptr->hist)
    return srptr->hist_low;

  return 0;
}


/*****************************************************************************/

int StatrecHighBin(STATREC *srptr)       /* Returns the high bin lower bound */
{
  PSDELAY;

  if (srptr->hist)
    return srptr->hist_high;

  return 0;
}


/*****************************************************************************/

int StatrecBinSize(STATREC *srptr)       /* Returns the bin size             */
{
  PSDELAY;

  if (srptr->hist)
    return srptr->hist_incr;

  return 0;
}


/*****************************************************************************/
/* Returns value of the ith histogram element */

long long StatrecHist(STATREC *srptr, int i)
{
  PSDELAY;

  if (srptr->hist)
    return srptr->hist[i];

  return 0;
}


/*****************************************************************************/


double StatrecMean(STATREC *srptr)       /* Returns the mean                 */
{
  PSDELAY;

  if (srptr->meanflag == MEANS)
    if (srptr->sumwt != 0)
      return srptr->sum/srptr->sumwt;
    else
      return 0;

  return 0.0;
}


/*****************************************************************************/


long long StatrecSum(STATREC *srptr)                      /* Returns the sum */
{
  PSDELAY;

  if (srptr->meanflag == MEANS)
    return srptr->sum;

  return 0;
}


/*****************************************************************************/

double StatrecSdv(STATREC *srptr)        /* Returns the standard deviation   */
{
  PSDELAY;

  if (srptr->meanflag == MEANS)
    return sqrt((double)(((double)srptr->sumsq-((double)srptr->sum/(double)srptr->sumwt)*
			  ((double)srptr->sum/(double)srptr->sumwt)*(double)srptr->sumwt)/((double)srptr->sumwt-1.0)));

  return 0.0;
}
