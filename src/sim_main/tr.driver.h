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
 * tr.driver.h
 *
 * Tracing macros for event-driven simulation library driver
 */
#ifndef __TR_DRIVER_H__
#define __TR_DRIVER_H__

#ifdef DEBUG_TRACE

/*
 * DRIVER tracing statements
 */

#define TRACE_DRIVER_run  \
   if (TraceLevel >= MAXDBLEVEL-3) { \
      sprintf(YS__prbpkt,"User activating Simulation Driver\n"); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_reset  \
    if (TraceLevel >= MAXDBLEVEL-3)  { \
       sprintf(YS__prbpkt,"\nDRIVER RESET --------------------------------");\
       sprintf(YS__prbpkt+strlen(YS__prbpkt),"<<Simulation Time: %g>>\n\n", \
               YS__Simtime);  \
      YS__SendPrbPkt("times",YS__prbpkt); \
    }

#define TRACE_DRIVER_body1  \
   if (TraceLevel >= MAXDBLEVEL-3)  { \
      sprintf(YS__prbpkt,"Transferring to process %s[%d]\n", \
         actptr->name,YS__EventId(actptr));  \
      YS__SendPrbPkt(actptr->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_evterminate  \
   if (TraceLevel >= MAXDBLEVEL-3)  { \
      sprintf(YS__prbpkt,"Event %s[%d] terminating\n", \
         YS__ActEvnt->name,YS__EventId(YS__ActEvnt)); \
      YS__SendPrbPkt(YS__ActEvnt->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_evdelete  \
   if (TraceLevel >= MAXDBLEVEL-1)  { \
      sprintf(YS__prbpkt,"        Deleting process %s[%d]\n", \
         actptr->name,YS__EventId(YS__ActEvnt)); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_terminate  \
   if (TraceLevel >= MAXDBLEVEL-3 && actptr->id != 0)  { \
      sprintf(YS__prbpkt,"Process %s[%d] terminating\n", \
         actptr->name,YS__EventId(actptr)); \
      YS__SendPrbPkt(actptr->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_prdelete  \
   if (TraceLevel >= MAXDBLEVEL-1)  { \
      sprintf(YS__prbpkt,"        Deleting process %s[%d]\n", \
         actptr->name,YS__EventId(actptr)); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_join1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt, \
              "    All child activities complete, process"\
              " %s[%d] released from Join\n", \
              parptr->name,YS__EventId(parptr)); \
      YS__SendPrbPkt(parptr->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_interrupt  \
   if (TraceLevel >= MAXDBLEVEL-3)  { \
      sprintf(YS__prbpkt,"Driver interrupt occurs, returning to user\n"); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_interrupt1  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Driver interrupt flag set\n"); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_empty  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    ------ Ready list empty, "); \
      sprintf(YS__prbpkt+strlen(YS__prbpkt), \
         "checking for pending resources\n");  \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_body3  \
   if (TraceLevel >= MAXDBLEVEL-3)  { \
      sprintf(YS__prbpkt,"\nTIME STEP COMPLETED: Time = %g\n\n",YS__Simtime); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_body4  \
   if (TraceLevel >= MAXDBLEVEL-4) { \
      sprintf(YS__prbpkt, \
              "\nREADY & EVENT LISTS EMPTY: Time = %g\n\n",YS__Simtime); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_simtime  \
    if (TraceLevel >= MAXDBLEVEL-3)  { \
       sprintf(YS__prbpkt, \
               "---------------------------------------------------"); \
       sprintf(YS__prbpkt+strlen(YS__prbpkt),"<<Simulation Time: %g>>\n", \
               YS__Simtime);  \
      YS__SendPrbPkt("times",YS__prbpkt); \
    }

#define TRACE_DRIVER_body2  \
   if (TraceLevel >= MAXDBLEVEL-3)  { \
      sprintf(YS__prbpkt,"Initiating event %s[%d]\n",  \
              YS__ActEvnt->name,YS__EventId(YS__ActEvnt)); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_body6  \
   if (TraceLevel >= MAXDBLEVEL-1) { \
      sprintf(YS__prbpkt,"        Deleting event %s[%d]\n",  \
              YS__ActEvnt->name,YS__EventId(YS__ActEvnt)); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#define TRACE_DRIVER_join2  \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
     sprintf(YS__prbpkt, \
             "    All child activities complete, process"\
             " %s[%d] released from Join\n", \
             YS__ActEvnt->parent->name,YS__EventId(YS__ActEvnt->parent)); \
     YS__SendPrbPkt(YS__ActEvnt->parent->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_condrelease  \
   if (TraceLevel >= MAXDBLEVEL-2) { \
      sprintf(YS__prbpkt,\
              "    - Event %s[%d] released from condition %s[%d]\n", \
              actptr->name,YS__EventId(actptr),conptr1->name,YS__EventId(conptr1));\
      YS__SendPrbPkt(actptr->name,YS__prbpkt); \
   }

#define TRACE_DRIVER_evalcond   \
   if (TraceLevel >= MAXDBLEVEL-2)  { \
      sprintf(YS__prbpkt,"    Evaluating condition %s[%d]; new state = %d\n", \
              conptr1->name,YS__QeId(conptr1),conptr1->state); \
      YS__SendPrbPkt("Driver",YS__prbpkt); \
   }

#else /* DEBUG_TRACE */

#define TRACE_DRIVER_run ;
#define TRACE_DRIVER_reset ;
#define TRACE_DRIVER_body1 ;
#define TRACE_DRIVER_terminate ;
#define TRACE_DRIVER_evterminate ;
#define TRACE_DRIVER_prdelete ;
#define TRACE_DRIVER_evdelete ;
#define TRACE_DRIVER_join1 ;
#define TRACE_DRIVER_interrupt ;
#define TRACE_DRIVER_interrupt1 ;
#define TRACE_DRIVER_empty ;
#define TRACE_DRIVER_body3 ;
#define TRACE_DRIVER_body4 ;
#define TRACE_DRIVER_simtime ;
#define TRACE_DRIVER_body6 ;
#define TRACE_DRIVER_body2 ;
#define TRACE_DRIVER_join2 ;
#define TRACE_DRIVER_flgrelease ;
#define TRACE_DRIVER_condrelease ;
#define TRACE_DRIVER_evalcond ;

#endif

#endif
