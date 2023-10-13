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
 * file: driver.c
 * description: Functions associated with the YACSIM simulation driver.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "Processor/simio.h"
#include "sim_main/simsys.h"
#include "sim_main/evlst.h"
#include "sim_main/userq.h"
#include "sim_main/pool.h"
#include "sim_main/tr.driver.h"
#include "Caches/req.h"
#include "Caches/system.h"
#include "Caches/cache.h"
#include "IO/scsi_bus.h"


static int    YS__TrMode;   /* Parameters used for interactive control */
static double YS__TrTime;   /* with DBSIM */
static int    YS__TrIter;

void UserMain(int, char**, char**);



/*
 * main: This is where it all starts. Main initializes the simulator driver.
 * It then calls the user-supplied routine UserMain().
 */
int main(int argc, char *argv[], char *env[])
{
  YS__idctr     = 0;      /* Unique ID generator */
  YS__ActEvnt   = NULL;   /* Points to currently active event */
  YS__Simtime   = 0.0;    /* Simulation time starts at 0 */
  YS__Cycles    = 0;      /* Initialize PARCSIM delays */
  YS__CycleTime = 1.0;

  /* Initialize all the pools used by the simulator */
  YS__PoolInit(&YS__EventPool,   "EventPool",     50,  sizeof(EVENT));
  YS__PoolInit(&YS__QueuePool,   "QueuePool",     4,   sizeof(QUEUE));
  YS__PoolInit(&YS__QelemPool,   "QelemPool",     50,  sizeof(QELEM));
  YS__PoolInit(&YS__StatPool,    "StatRecPool",   50,  sizeof(STATREC));
  YS__PoolInit(&YS__ReqPool,     "ReqPool",       100, sizeof(REQ));
  YS__PoolInit(&YS__ScsiReqPool, "ScsiReqPool",   50,  sizeof(SCSI_REQ));

  /* Initialize the event list */
  YS__EventListInit();

  /* Transfer to user code */
  UserMain(argc, argv, env);

  return(0);
}




/***************************************************************************/
/* DRIVER Operations: These routines manipulate the event list and         */
/* call the user activities at the appropriate simulation times.           */
/***************************************************************************/

/*
 * Appends an activity onto the system ready list
 */
void YS__RdyListAppend(EVENT *aptr)
{
  /* schedule at the current simulation time */
  aptr->time = YS__Simtime;
  YS__EventListInsert(aptr);

  /* For statistics collection */
  aptr->status = READY;
}




/*
 * Activates the simulation driver; returns a value
 * set by DriverInterrupt or 0 for termination
 */
void DriverRun()
{
  EVENT *actptr;


  TRACE_DRIVER_run;                  /* User activating simulation drivers */

  while (1)
    {                                /* Start of main simulation loop      */
      /* The ready list consists of those activities at the head of the event
       * list that are scheuduled for the current simulation time */

      while (YS__EventListHeadval() == YS__Simtime)
	{
	  /* The ready list is not empty. Get the next activity */
	  actptr = YS__EventListGetHead();

	  /* activity must be activated by switching to it if it is a
	     process or calling it if it is an event. */

	  YS__ActEvnt = (EVENT *) actptr;         /* An event is now active  */
	  YS__ActEvnt->status = RUNNING;          /* statistics collection   */

	  (YS__ActEvnt->body) ();                 /* THE EVENT OCCURS        */

	  YS__ActEvnt->status = LIMBO;            /* statistics collection   */

	  if (YS__ActEvnt->deleteflag == DELETE)
	    {
#ifdef DEBUG_DRIVER
	      if (YS__ActEvnt->blkflg == BLOCK)
		/* Parent was blocked. Wake parent up */
		YS__errmsg("BlkFlg == BLOCK not allowed in this version");
		
	      if (YS__ActEvnt->blkflg == FORK)
		/* Parent forked this evnt. Wake parent up */
		YS__errmsg("BlkFlg == FORK not allowed in this version");
#endif
	      /* Free the event */
	      YS__PoolReturnObj(&YS__EventPool, YS__ActEvnt);
	    }

	  YS__ActEvnt = NULL;
	}


      /* Returns 0 if event list is empty */
      if (YS__EventListHeadval() >= 0)
	YS__Simtime = YS__EventListHeadval();     /* Advance simulation time */
      else                    /* Terminate DriverRun due to empty event list */
	return;
    }
}
