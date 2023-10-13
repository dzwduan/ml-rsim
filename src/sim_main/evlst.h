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

#ifndef __RSIM_EVLST_H__
#define __RSIM_EVLST_H__

#include "sim_main/util.h"
#include "sim_main/stat.h"
#include "sim_main/userq.h"


/*****************************************************************************/
/* Event States                                                              */
/*****************************************************************************/

#define LIMBO             0          /* Just created                         */
#define READY             1          /* In the ready list                    */
#define DELAYED           2          /* In the event list                    */
#define SCHEDULED         8
#define UNSCHEDULED       9
#define WAIT_SEM          3          /* In a semaphore's queue               */
#define WAIT_FLG          4          /* In a flag's queue                    */
#define WAIT_CON          5          /* In a conditions                      */
#define WAIT_RES          6          /* In a resorce's queue                 */
#define USING_RES         7          /* In the event getting resource        */



/*****************************************************************************/
/* Event, resource, process, and processor States                            */
/*****************************************************************************/

#define RUNNING            8         /* Executing the body function          */
#define BLOCKED            9         /* Due to a blocking schedule           */
#define WAIT_JOIN          10        /* Due to a ProcessJoin                 */
#define WAIT_MSG           11        /* Due to a blocking ReceiveMsg         */
#define WAIT_BAR           12        /* Due to a BarrierSync()               */



/*****************************************************************************/
/* Blocking Actions                                                          */
/*****************************************************************************/

#define INDEPENDENT         0        /* Unrelated child scheduled            */
#define NOBLOCK             0        /* Nonblocking MessageReceive           */
#define BLOCK               1        /* Blocking Schedule or Message Receive */
#define FORK                2        /* Forking Schedule                     */



/*****************************************************************************/
/* Event Characteristics                                                     */
/*****************************************************************************/

#define DELETE              1        /* Event deleted at termination         */
#define NODELETE            0        /* Event not deleted at termination     */



/*****************************************************************************/
/* Event Queue Statistics                                                    */
/*****************************************************************************/

#define TIME                1
#define UTIL                2
#define LENGTH              3
#define BINS                4
#define BINWIDTH            5
#define EMPTYBINS           6



/*****************************************************************************/
/* Argument and Buffer Size and misc.                                        */
/*****************************************************************************/

#define UNKNOWN             -1             /* Size not specified             */
#define DEFAULTSTK          0              /* Default stack size             */
#define ME                  YS__ActEvnt



/*****************************************************************************/
/* Event: the basic unit of scheduling in the YACSIM driver as used by RSIM  */
/*****************************************************************************/

typedef struct YS__Event
{
  QE        *next;                /* Pointer to the element after this one   */
  int        id;                  /* System defined unique ID                */
  char       name[32];            /* User assignable name for dubugging      */

  /* STATQE fields */
  double     enterque;            /* Time the event enters a queue           */

  /* EVENT fields */
  char      *argptr;              /* arguments or data for this activity     */
  int        argsize;             /* Size of argument structure              */
  double     time;                /* time to order priority queues           */
  int        status;              /* Status (e.g., ready, waiting,...)       */
  int        scheduled;           /* is the activity being scheduled         */
  int        blkflg;              /* INDEPENDENT, BLOCK, or FORK             */
  STATREC   *statptr;             /* Statistics record for status            */
  double     priority;            /* Priority for resource scheduling        */
  double     timeleft;            /* for RR resources and time slicing       */
  func       body;                /* Defining function for the event         */
  int        state;               /* Save return point after reschedule      */
  int        deleteflag;          /* DELETE or NODELETE                      */
  int        evtype;              /* User defined event type                 */
  void      *uptr1;
  void      *uptr2;
  void      *uptr3;
  int        ival1;
  int        ival2;
} EVENT;


extern EVENT  *head;
extern EVENT  *tail;
extern int     size;
extern int     cqsize;
extern double  lastprio;



/*****************************************************************************/
/* Event list operations                                                     */
/*****************************************************************************/

EVENT  *NewEvent             (const char *, void (*)(), int, int);
void    YS__EventListInit    (void);         /* Initializes the event list   */
void    YS__EventListInsert  (EVENT *);
void    YS__EventListRemove  (EVENT *);
EVENT  *YS__EventListGetHead (void);
int     EventGetState        (void);
void    EventSetState        (EVENT *, int);
double  YS__EventListHeadval (void);
void    EventSchedTime       (EVENT *, double, int);
void    EventSetArg          (EVENT *, void *, int);
char   *EventGetArg          (EVENT *);
void    Evlst_dump           (int);



/*****************************************************************************/
/* Driver operations                                                         */
/*****************************************************************************/

void YS__RdyListAppend(EVENT *);
void DriverRun();       /* Activates the simulation driver; returns no value */



/*****************************************************************************/
/* inline functions (macros)                                                 */
/*****************************************************************************/

#define IsNotScheduled(evt)     (evt->scheduled == UNSCHEDULED)
#define IsScheduled(evt)        (evt->scheduled == SCHEDULED)

#define YS__EventListHeadval()  ((head)   ? head->time : -1.0)


#define schedule_event(vptr, abstime)  	  \
        { 				  \
           vptr->scheduled = SCHEDULED;   \
           vptr->time = abstime; 	  \
           YS__EventListInsert(vptr); 	  \
        }

#define deschedule_event(vptr)  	  \
        { 				  \
           vptr->scheduled = UNSCHEDULED; \
           YS__EventListRemove(vptr); 	  \
        }


#if 0
#define YS__EventListGetHead(actptr)\
        {\
	   if (head != 0) {\
	      size--;\
	      actptr = head;\
	      head   = (EVENT *) (actptr->next);\
	      actptr->next = 0;\
              actptr->scheduled = UNSCHEDULED; \
	      if (head == 0)\
		 tail = 0;\
	   }\
	   else \
	      actptr = 0;\
        }

#define YS__EventListInsert(aptr) \
        { \
	   if (head == 0) {       /* The queue is empty */ \
	      head = (EVENT *)aptr; \
	      tail = (EVENT *)aptr; \
	      aptr->next = 0; \
	   } \
	   else { \
	      if (aptr->time < head->time) { /* head */ \
		 aptr->next = (QE *) head; \
		 head = (EVENT *)aptr; \
	      } \
	      else if (aptr->time >= ((tail))->time) { /* tail */ \
		 tail->next = (QE *) aptr; \
		 aptr->next = 0; \
		 tail = (EVENT *) aptr; \
	      } \
	      else { /* middle */ \
                 EVENT *tp1 = head; \
		 while (aptr->time >= ((EVENT *) (tp1->next))->time) \
		    tp1 = (EVENT *) (tp1->next); \
		 aptr->next = tp1->next; \
		 tp1->next = (QE *) aptr; \
	      } \
	   } \
	   size++; \
        }
#endif

#endif
