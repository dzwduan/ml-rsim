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
 * tr.stat.h : Statistics tracing
 */
#ifndef __TR_STAT_H__
#define __TR_STAT_H__

#ifdef DEBUG_TRACE

/*
 * STATREC tracing statements 
 */

#define TRACE_STATREC_new1  \
   if (TraceLevel >= MAXDBLEVEL-2) {  \
      sprintf(YS__prbpkt,"    Creating statistics record %s[%d]\n",  \
              srptr->name,YS__StatrecId(srptr));  \
      if (meanflg)  \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "    - Means computed; no histograms computed\n");  \
      else \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "    - No Means and no histograms computed\n");  \
      YS__SendPrbPkt(srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_new2  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Creating statistics record %s[%d]\n", \
              srptr->name,YS__StatrecId(srptr));  \
      if (meanflg)  \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "    - Means and histograms computed\n");  \
      else \
         sprintf(YS__prbpkt+strlen(YS__prbpkt), \
                 "    - Histograms computed; no means computed\n");  \
      sprintf(YS__prbpkt+strlen(YS__prbpkt), \
              "    - %d bins, bin size = %g, low bin < %g, high bin >= %g\n",\
              srptr->bins,BININC,LOWBIN,HIGHBIN);  \
      YS__SendPrbPkt(srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_sethistsz  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Setting default histogram pool size to %d\n",i); \
      YS__SendPrbPkt(NULL,YS__prbpkt); \
   }

#define TRACE_STATREC_reset  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,"    Resetting statistics record %s[%d]\n",  \
              srptr->name,YS__StatrecId(srptr)); \
      YS__SendPrbPkt(srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_pupdate  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Updating point statistics record "\
              "%s[%d]; value = %g, weight = %g\n", \
              srptr->name,YS__StatrecId(srptr),v,t); \
      YS__SendPrbPkt(srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_iupdate  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    Updating interval statistics record "\
              "%s[%d]; value = %g, interval = %g\n", \
              srptr->name,YS__StatrecId(srptr),v,srptr->interval); \
      YS__SendPrbPkt(srptr->name,YS__prbpkt); \
   }
 
/*
 * Trace statements for external histograms
 */

#define TRACE_STATREC_create  \
   if (YS__interactive)  { \
      sprintf(YS__prbpkt, "%c%g:%g:%d", \
              HISTCREATE, BININC, LOWBIN, srptr->bins - 2); \
      YS__SendPrbPkt(HISTPROBE,srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_isample  \
   if (YS__interactive)  { \
      sprintf(YS__prbpkt, "%c%g:%g", \
              HISTSAMPLE, srptr->lastv, srptr->interval); \
      YS__SendPrbPkt(HISTPROBE,srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_psample  \
   if (YS__interactive)  { \
      sprintf(YS__prbpkt, "%c%g:%g", HISTSAMPLE, v, t); \
      YS__SendPrbPkt(HISTPROBE,srptr->name,YS__prbpkt); \
   }

#define TRACE_STATREC_clear  \
   if (YS__interactive)  { \
      sprintf(YS__prbpkt, "%c", HISTRESET); \
      YS__SendPrbPkt(HISTPROBE,srptr->name,YS__prbpkt); \
   }

#else  /* DEBUG_TRACE */

#define TRACE_STATREC_new1
#define TRACE_STATREC_new2
#define TRACE_STATREC_sethistsz
#define TRACE_STATREC_reset
#define TRACE_STATREC_pupdate
#define TRACE_STATREC_iupdate

#define TRACE_STATREC_create
#define TRACE_STATREC_isample
#define TRACE_STATREC_psample
#define TRACE_STATREC_clear

#endif

#endif
