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
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "sim_main/util.h"

/*
 *  YS__errmsg: Prints error message & terminates simulation
 */
void YS__errmsg(int node, const char *fmt, ...)
{
  char s[1024];
  va_list ap;

  sprintf(s, "\nFatal ERROR at time %.0f:\n\t", YS__Simtime);
  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  sprintf(s, "\n\n");
  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  exit(1);
}


/*
 * Prints warning message
 */
void YS__warnmsg(int node, const char *fmt, ...)
{
  char s[1024];
  va_list ap;

  sprintf(s, "WARNING at cycle %.0f:\n", YS__Simtime);
  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }
}


/*
 * Prints statistics message
 */
void YS__statmsg(int node, const char *fmt, ...)
{
  char s[1024];
  va_list ap;

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  while (write(statfile[node], s, strlen(s)) < 0)
    {
      printf("Write to statistics file failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }
}


/*
 * Prints logfile message
 */
void YS__logmsg(int node, const char *fmt, ...)
{
  char s[1024];
  va_list ap;

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  while (write(logfile[node], s, strlen(s)) < 0)
    {
      printf("Write to logfile failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }
}




/*
 * Prints message to file
 */
void YS__fmsg(int file, const char *fmt, ...)
{
  char s[1024];
  va_list ap;

  va_start(ap, fmt);
  vsprintf(s, fmt, ap);
  va_end(ap);

  while (write(file, s, strlen(s)) < 0)
    {
      printf("Write to file failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }
}



char default_error_string[40];

char *YS__strerror(int code)
{
  char *p;

  p = strerror(code);
  if (p != NULL)
    return(p);
  
  sprintf(default_error_string, "Unknown error code %i", code);
  return(default_error_string);
}


/*
 * GetSimTime: returns simulation time
 */
double GetSimTime()
{
  return (YS__Simtime);
}



void PrintTime(double time, int fp)
{
  char s[40];
  
  if (time >= 1.0)
    sprintf(s, "%7.3f s", time);
  else if (time >= 1.0e-3)
    sprintf(s, "%7.3f ms", time * 1.0e3);
  else if (time >= 1.0e-6)
    sprintf(s, "%7.3f us", time * 1.0e6);
  else
    sprintf(s, "%7.3f ns", time * 1.0e9);

  while (write(fp, s, strlen(s)) < 0)
    {
      printf("Write failed (%i %s)",
	     errno, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
	     FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }
}



/*
 * Count the number of bits. "must_be" indicates whether "a" must
 * be a power of 2.
 */
int NumOfBits(int a, int must_be)
{
  int lastbit;
  int num_of_bits = 0;

  while (a)
    {
      lastbit = a & 1;
      if (must_be && lastbit && (a != 1))
	{
	  YS__warnmsg(0, "%d not a power of 2", a);
	  return -1;
	}
      
      a = a >> 1;
      num_of_bits++;
    }

  return MAX(num_of_bits - 1, 0);
}



/*
 * Round a number up to power of 2 
 */
/*
int ToPowerOf2(int n)
{
  int t = 0;

  if (n <= 0)
    return 0;

  n += n - 1;
  while (n >>= 1)
    t++;
  return (1 << t);
}
*/
