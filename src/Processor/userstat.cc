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
/* User statistics package: allows applications to collect timing data     */
/* with very low overhead. Statistics structures are allocated through a   */
/* simulator trap with a statistics type and unique name as arguments. If  */
/* the name matches an already existing structure, that handle is          */
/* returned, otherwise a new structure is allocated.                       */
/* Statistics are collected by a second simulator trap, semantics depend   */
/* on the particular statistics type.                                      */
/*   USER_STAT_INTERVAL: record intervals as pairs of simulator traps      */
/*                       collect total, minimum, maximum, average and      */
/*                       number of intervals, report in cycles and wall    */
/*                       time                                              */
/*   USER_STAT_TRACE:    prints current time, its name and argument upon   */
/*                       every invocation                                  */
/*   USER_STAT_POINT:    records minimum, maximum, average and number of   */
/*                       samples                                           */
/* Statistics structures are managed as an array of pointers to the        */
/* individual objects. The array grows as new structures are allocated.    */
/* Upon reset or print, the respective routines for all objects are called.*/
/***************************************************************************/


#include <string.h>
#include <malloc.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "sim_main/util.h"
}

#include "Processor/simio.h"
#include "Processor/procstate.h"
#include "Processor/userstat.h"

#include "../lamix/sys/userstat.h"


user_stats **UserStats;

#ifdef sgi
#define LLONG_MAX LONGLONG_MAX
#endif

#if !defined(LLONG_MAX)
#  define LLONG_MAX 9223372036854775807LL
#endif

/*=========================================================================*/
/* Initialize per-node user statistics:                                    */
/* Allocate array of per-node statistics objects and initialize them.      */
/*=========================================================================*/

extern "C" void UserStats_init(void)
{
  int i;

  UserStats = (user_stats**)malloc(ARCH_numnodes *
				   sizeof(user_stats*));
 
  if (!UserStats)
    YS__errmsg(0, "Malloc failed at %s:%s\n", __FILE__, __LINE__);

  for (i = 0; i < ARCH_numnodes; i++)
    UserStats[i] = new user_stats(i);
}



/*=========================================================================*/
/* Report user statistics for one node - C wrapper around class function   */
/*=========================================================================*/

extern "C" void UserStats_report(int node)
{
  UserStats[node]->print(statfile[node]);
}



/*=========================================================================*/
/* Reset statistics - C wrapper around the class function                  */
/*=========================================================================*/

extern "C" void UserStats_clear(int node)
{
  UserStats[node]->reset();
}










/*=========================================================================*/
/* Interval statistics class - derived from basic statistics class.        */
/* Records intervals as time between subsequent calls to 'sample' and      */
/* collects maximum, minimum, total, average and number of intervals.      */
/*=========================================================================*/

class user_stat_interval : public user_stat
{
private:
  long long last;
  long long min;
  long long max;
  long long total;
  long long samples;

public:
  user_stat_interval(char *na, int no) : user_stat(na, no)
  {
    reset();
  };

  ~user_stat_interval()
  {};

  void reset();
  void print(int);
  void sample(int);
};




/*=========================================================================*/
/* Reset interval statistics class                                         */
/*=========================================================================*/

void user_stat_interval::reset()
{
  last    = 0;
  min     = LLONG_MAX;
  max     = 0;
  total   = 0;
  samples = 0;
}



/*=========================================================================*/
/* Print interval statistics. If at least one sample exists, report total, */
/* average, minimum and maximum in cycles and wall time.                   */
/*=========================================================================*/

void user_stat_interval::print(int fp)
{
  if (last != 0ll)
    sample(0);

  YS__fmsg(fp, "%s (interval)\n", name);
  if (samples == 0)
    YS__fmsg(fp, "  no samples\n");
  else
    {
      YS__fmsg(fp, "  %lld sample%c\n",
	       samples, samples > 1 ? 's' : ' ');
      YS__fmsg(fp, "  Total: %12lld cycles\t",
	       total);
      PrintTime(total * (double)CPU_CLK_PERIOD / 1.0e12,
                fp);
      YS__fmsg(fp, "\n");
      YS__fmsg(fp, "  Avg:   %12lld cycles\t",
	       total / samples);
      PrintTime(total / samples * (double)CPU_CLK_PERIOD / 1.0e12,
                fp);
      YS__fmsg(fp, "\n");
      YS__fmsg(fp, "  Min:   %12lld cycles\t",
	       min);
      PrintTime(min * (double)CPU_CLK_PERIOD / 1.0e12,
                fp);
      YS__fmsg(fp, "\n");
      YS__fmsg(fp, "  Max:   %12lld cycles\t",
	       max);
      PrintTime(max * (double)CPU_CLK_PERIOD / 1.0e12,
                fp);
      YS__fmsg(fp, "\n");
    }

  YS__fmsg(fp, "\n");
}



/*=========================================================================*/
/* Sample interval statistics: if no last value exists, record current     */
/* cycle, otherwise calculate interval as difference between last sample   */
/* and current cycle and update total, maximum and minimum.                */
/*=========================================================================*/

void user_stat_interval::sample(int)
{
  long long i;

  if (last == 0ll)
    last = (long long)YS__Simtime;
  else
    {
      i = (long long)YS__Simtime - last;
      if (i > max)
	max = i;
      if (i < min)
	min = i;
      total += i;
      samples++;
      last = 0ll;
    }
}







/*=========================================================================*/
/* Trace statistics class - derived from basic statistics class.           */
/* Prints trace information at every sample. Reports total of trace points.*/
/*=========================================================================*/

class user_stat_trace : public user_stat
{
private:
  long long samples;

public:
  user_stat_trace(char *na, int no) : user_stat(na, no)
  {
    reset();
  };

  ~user_stat_trace()
  {};

  void reset();
  void print(int);
  void sample(int);
};




/*=========================================================================*/
/* Reset trace statistics class                                            */
/*=========================================================================*/

void user_stat_trace::reset()
{
  samples = 0;
}



/*=========================================================================*/
/* Print trace statistics. If at least one sample exists, report total.    */
/*=========================================================================*/

void user_stat_trace::print(int fp)
{
  YS__fmsg(fp, "%s (trace)\n", name);
  if (samples == 0)
    YS__fmsg(fp, "  no samples\n");
  else if (samples == 1)
    YS__fmsg(fp, "  1 sample\n");
  else
    YS__fmsg(fp, "  %lld samples\n", samples);
  YS__fmsg(fp, "\n");
}



/*=========================================================================*/
/* Sample trace statistics: print message and argument, increment count    */
/*=========================================================================*/

void user_stat_trace::sample(int arg)
{
  samples++;
  YS__logmsg(node, "TRACE %.0f: %s %i\n", YS__Simtime, name, arg);
}







/*=========================================================================*/
/* Point statistics class - derived from basic statistics class.           */
/* Records statistics at sample points. Reports min, max, avg of samples.  */
/*=========================================================================*/

class user_stat_point : public user_stat
{
private:
  int min;
  int max;
  long long total;
  long long samples;

public:
  user_stat_point(char *na, int no) : user_stat(na, no)
  {
    reset();
  };

  ~user_stat_point()
  {};

  void reset();
  void print(int);
  void sample(int);
};




/*=========================================================================*/
/* Reset point statistics class                                            */
/*=========================================================================*/

void user_stat_point::reset()
{
  samples = 0ll;
  min     = INT_MAX;
  max     = INT_MIN;
  total   = 0ll;
}



/*=========================================================================*/
/* Print point statistics. If at least one sample exists, report total.    */
/*=========================================================================*/

void user_stat_point::print(int fp)
{
  YS__fmsg(fp, "%s (point)\n", name);
  if (samples == 0)
    YS__fmsg(fp, "  no samples\n");
  else
    {
      YS__fmsg(fp, "  %lld sample%c\n", samples, samples > 1 ? 's' : ' ');
      YS__fmsg(fp, "  Total: %12lld\n", total);
      YS__fmsg(fp, "  Avg:   %12.3f\n", (double)total / (double)samples);
      YS__fmsg(fp, "  Min:   %12d\n", min);
      YS__fmsg(fp, "  Max:   %12d\n", max);
    }
  YS__fmsg(fp, "\n");
}



/*=========================================================================*/
/* Sample point statistics: record max/min, add total, increment count     */
/*=========================================================================*/

void user_stat_point::sample(int arg)
{
  samples++;
  total += arg;
  
  if (arg > max)
    max = arg;
  if (arg < min)
    min = arg;
}







/*=========================================================================*/
/* Per-node user statistics: constructor.                                  */
/*=========================================================================*/

user_stats::user_stats(int n)
{
  node       = n;
  num_elemns = 0;
  elemns     = NULL;
}



/*=========================================================================*/
/* Per-node user statistics: destructor.                                   */
/*=========================================================================*/

user_stats::~user_stats()
{
  if (elemns != NULL)
    free(elemns);
  num_elemns = 0;
  elemns = NULL;
}



/*=========================================================================*/
/* Reset user statistics - call reset routine for all statistics objects.  */
/*=========================================================================*/

void user_stats::reset()
{
  for (int n = 0; n < num_elemns; n++)
    elemns[n]->reset();
}



/*=========================================================================*/
/* Print user statistics - call print routine for all statistics objects.  */
/*=========================================================================*/

void user_stats::print(int fp)
{
  for (int n = 0; n < num_elemns; n++)
    elemns[n]->print(fp);
}



/*=========================================================================*/
/* Allocate a statistics object: scan list of existing objects, if name    */
/* exists return handle, otherwise grow array of objects and create new    */
/* object of specified type.                                               */
/*=========================================================================*/

int user_stats::alloc(int type, char *name)
{
  int n;
  user_stat ** new_elemns;

  for (n = 0; n < num_elemns; n++)
    if (strcmp(elemns[n]->getname(), name) == 0)
      break;

  if (n < num_elemns)
    return(n);

  num_elemns++;
  if (elemns == (user_stat**)NULL)
    {
      elemns = (user_stat**)malloc(sizeof(user_stat*) * num_elemns);
      if (elemns == NULL)
	YS__errmsg(0, "Malloc failed in %s:%i\n", __FILE__, __LINE__);
    }
  else
    {
      new_elemns = (user_stat**)malloc(sizeof(user_stat*) * num_elemns);
      if (new_elemns == NULL)
	YS__errmsg(0, "Malloc failed in %s:%i\n", __FILE__, __LINE__);
      memcpy(new_elemns, elemns, sizeof(user_stat*) * (num_elemns-1));
      free(elemns);
      elemns = new_elemns;
    }

  switch (type)
    {
    case USER_STAT_INTERVAL:
      elemns[n] = new user_stat_interval(name, node);
      return(n);

    case USER_STAT_TRACE:
      elemns[n] = new user_stat_trace(name, node);
      return(n);

    case USER_STAT_POINT:
      elemns[n] = new user_stat_point(name, node);
      return(n);

    default:
      YS__warnmsg(node, "Illegal UserStat type %i in user_stats::add\n", type);
      num_elemns--;
      return(-1);
    }
}



/*=========================================================================*/
/* Record user statistics sample: call corresponding routine of object.    */
/*=========================================================================*/

void user_stats::sample(int id, int val)
{
  if ((id < 0) || (id >= num_elemns))
    {
      YS__warnmsg(0, "Sample on invalid ID %i in user_stats::sample\n", id);
      return;
    }

  elemns[id]->sample(val);
}







