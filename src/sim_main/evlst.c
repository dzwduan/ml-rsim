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
 * evlst.c
 *
 * This file contains functions associated with maintaining
 * the simulator event list.
 */

#include <stdio.h>
#include <string.h>
#include "sim_main/simsys.h"
#include "sim_main/tr.evlst.h"
#include "sim_main/tr.act.h"



/*****************************************************************************/
/* EVENT LIST Operations: There are two implementations of the eventlist:    */
/* a calendar queue and a simple linear list.  The choice of which list      */
/* to use is determined by a command line argument or by the operation       */
/* EventListSelect().  The implementaton of the calendar queue algorithm     */
/* follows the description in "Calendar Queues: A Fast O(1) Priority         */
/* Queue Implementation for the Simulation Event Set Problem," by Randy      */
/* Brown, Comm. ACM, Oct. 1988, pp. 1220-1227.                               */
/*****************************************************************************/

STATREC *lengthstat = 0;           /* For collecting queue length statistics */



/* 
 * Variables used when the event List is implemented as a
 * simple linear list 
 */
EVENT *head;                  /* Pointer to the first element of the queue   */
EVENT *tail;                  /* Pointer to the last element of the queue    */
int   size;                   /* Number of elements in the queue             */


/* 
 * Creates & returns pointer to an event 
 */
EVENT *NewEvent(const char *ename, func bodyname, int dflag, int etype)
{
  EVENT *eptr;

  PSDELAY;

  /* Get the activity descriptor */
  eptr             = (EVENT*) YS__PoolGetObj(&YS__EventPool);     
  eptr->id         = YS__idctr++;         /* System assigned unique ID       */
  strncpy(eptr->name, ename, 31);         /* Copy the name                   */
  eptr->name[31]   = '\0';                /* Limited to 31 characters        */

  eptr->next       = NULL;
  eptr->argptr     = 0;
  eptr->argsize    = 0;
  eptr->time       = 0.0;
  eptr->status     = LIMBO;
  eptr->scheduled  = UNSCHEDULED;
  eptr->blkflg     = INDEPENDENT;
  eptr->timeleft   = 0.0;
  eptr->enterque   = 0.0;

  eptr->body       = bodyname;
  eptr->deleteflag = dflag;
  eptr->state      = 0;
  
  TRACE_EVENT_event;                      /* Creating event ...              */
  return eptr;
}




/*
 * Initializes the event list
 */
void YS__EventListInit(void)
{
  TRACE_EVLST_init2;
  head = 0;
  tail = 0;
  size = 0;
}




/* 
 * Returns the system defined ID or 0 if TraceIDs is 0 
 */
int YS__EventId(EVENT *vptr)      
{
  if (TraceIDs)
    {  /* TraceIDs must be set to get IDs printed in traces */
      if (vptr)
	return vptr->id;       /* vptr points to something */
      else if (YS__ActEvnt)
	return YS__ActEvnt->id;/* Called from an active event */
      else
	YS__errmsg(0, "Null Event Referenced");

      return 0;
    }
  else
    return 0;
}




/*
 * Inserts an element into the event list
 */
void YS__EventListInsert(EVENT *vptr)
{
  int i;
  EVENT *tp1;
  EVENT *tp2;

  TRACE_EVLST_insert;

  size++;
  if (head == 0)
    {       /* The queue is empty */
      head = vptr;
      tail = vptr;
      vptr->next = 0;
    }
  else
    {
      if (vptr->time < head->time)
	{
	  /* Put the new element at head of queue */
	  vptr->next = (QE *) head;
	  head = vptr;
	}
      else
	{
	  if (vptr->time >= tail->time)
	    {
	      /* Put new element at tail of queue */
	      tail->next = (QE *) vptr;
	      vptr->next = 0;
	      tail = vptr;
	    }
	  else
	    {
	      /* put element in middle of queue */
	      tp1 = head;
	      /* Find position */
	      while (vptr->time >= ((EVENT *) (tp1->next))->time)
		tp1 = (EVENT *) (tp1->next);
	      
	      vptr->next = tp1->next;
	      tp1->next = (QE *) vptr;
	    }
	}
    }


#ifdef DEBUG_EVENT
  if (lengthstat)   /* Queue length statistics collected */
    StatrecUpdate(lengthstat, (double) size, 1.0);
#endif

  TRACE_EVLST_show;
}




/*
 * Removes an element from the event list
 */
void YS__EventListRemove(EVENT *vptr)
{
  EVENT *e, *p;

  if (head == vptr)
    {
      head = (EVENT*)vptr->next;
    }
  else
    {
      e = head;
      while ((e != NULL) &&
	     (e->next != NULL) &&
	     (e->next != (QE*)vptr))
	e = (EVENT*)e->next;

      e->next = vptr->next;

      if (tail == vptr)
	tail = e;
    }
}




/*
 * Returns the head of the event list
 */
EVENT *YS__EventListGetHead(void)
{
  EVENT *retptr;

  if (head != 0)
    {     /* Queue not empty */
      size--;                          /* One fewer element in the queue */
      retptr = head;                   /* Get the head element */
      head = (EVENT *) (retptr->next);
      retptr->next = 0;         /* Clear next pointer of removed element */
      retptr->scheduled = UNSCHEDULED;
      if (head == 0)
	tail = 0;               /* Queue now empty */

#ifdef DEBUG_EVENT 
      if (lengthstat)           /* Queue length statistics collectd */
	StatrecUpdate(lengthstat, (double) size, 1.0);
#endif
      
      TRACE_EVLST_gethead2;
    }
  else
    { /* Queue empty */
      retptr = 0; /* Return NULL */
      TRACE_EVLST_gethead1;
    }
  TRACE_EVLST_show;

  return retptr;
}




/* 
 * Schedules an activity in the future 
 */
void EventSchedTime(EVENT *actptr, double timeinc, int bflg)        
{
  EVENT *vptr;

  PSDELAY;
  
  if (actptr)
    vptr = actptr;    /* actptr points to an activity, use it */
  else if (YS__ActEvnt)
    {      /* or an event is rescheduling itself */
      if (YS__ActEvnt->deleteflag == DELETE)
	YS__errmsg(0, "Can not reschedule a deleting event %s", actptr->name);
      vptr = (EVENT *) YS__ActEvnt;
    }
  else
    YS__errmsg(0, "Null Event Referenced");   /* or there is a mistake */

  TRACE_EVENT_schedule1;    /* Scheduling activity to occur in "timeinc"
			       time units */
  vptr->blkflg = bflg;
  
  if (vptr->status != RUNNING && vptr->status != LIMBO)
    YS__errmsg(0, "Can not reschedule a pending event %s", vptr->name);

#ifdef DEBUG
  if (vptr->blkflg != INDEPENDENT)
    YS__errmsg(0,
	       "Only Independent block-flag supported for "
	       "EventSchedTime in this version");
#endif
  
  if (timeinc == 0.0)
    {        /* Schedule immediately */
      vptr->time = YS__Simtime;
      YS__RdyListAppend(vptr);
    }
  else if (timeinc > 0.0)
    {    /* Schedule in the future */
      vptr->time = YS__Simtime + timeinc;
      vptr->status = DELAYED;
      YS__EventListInsert(vptr);
    }
  else
    YS__errmsg(0,
	       "Can not schedule an activity to occur in the past %s",
	       vptr->name);
}




/* 
 * Sets the argument pointer of an activity 
 */
void EventSetArg(EVENT *vptr, void *arptr, int arsize)     
{
  PSDELAY;

  if (vptr)
    {          /* vptr points to an activity */
      TRACE_EVENT_setarg;    /* Setting argument for activity ... */
      vptr->argptr = arptr;     /* Set its argument */
      vptr->argsize = arsize;
    }
   else if (YS__ActEvnt)
     {          /* Called from an active event */
       YS__ActEvnt->argptr = arptr;  /* It is setting its own argument */
       YS__ActEvnt->argsize = arsize;
     }
  else
    YS__errmsg(0, "Null Event Referenced");
}




/* 
 * Returns the argument pointer of an activity 
 */
char *EventGetArg(EVENT *vptr) 
{
  PSDELAY;

  if (vptr)
    {          /* vptr points to an activity */
      TRACE_EVENT_getarg;    /* Getting argument from activity ... */
      return vptr->argptr;
    }
  else if (YS__ActEvnt)
    return YS__ActEvnt->argptr;       /* Active event or process */
  else
    YS__errmsg(0, "Null Event Referenced");

  return NULL;
}




/* 
 * Returns the state set by EventSetState() 
 */
int EventGetState(void)
{                       
  PSDELAY;

  if (YS__ActEvnt)
    return YS__ActEvnt->state;
  else
    YS__errmsg(0,
	       "EventGetState() must be called from within an active event");
  return 0;
}




/* 
 * Sets state used to designate a return point 
 */
void EventSetState(EVENT *eptr, int st)         
{
  PSDELAY;

  if (eptr == NULL)    /* Called from within the event */
    if (YS__ActEvnt)
      YS__ActEvnt->state = st;
    else
      YS__errmsg(0,
		 "EventSetState() has NULL pointer, but not called from "
		 "an event");
  else
    eptr->state = st;
}



#if 0
/*
 * Returns the time value of the event list head
 */
double YS__EventListHeadval(void)
{
  double retval;

  if (head != 0)
    retval = (head)->time;
  else
    retval = -1.0;

  TRACE_EVLST_headval;
  return retval;
}
#endif



void Evlst_dump(int nid)
{
  EVENT *ev;
  YS__logmsg(nid, "\n====== EVENT LIST ======\n");

  for (ev = head; ev != NULL; ev = (EVENT*)ev->next)
    {
      YS__logmsg(nid, "Event %i: %s\n", ev->id, ev->name);
      YS__logmsg(nid, "  enterque(%lf), time(%lf), status(%d), scheduled(%d)\n",
		 ev->enterque, ev->time, ev->status, ev->scheduled);
    }
       
}

