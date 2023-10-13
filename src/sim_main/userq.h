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

#ifndef __RSIM_USERQ_H__
#define __RSIM_USERQ_H__

/*****************************************************************************/
/* Object types                                                              */
/*****************************************************************************/

#define UNTYPED           0
#define QETYPE            1000          /* Queue elements                    */
#define QUETYPE           1100          /* Queues                            */
#define SYNCQTYPE         1110          /* Synchronization Queueus           */
#define SEMTYPE           1111          /* Semaphores                        */
#define FLGTYPE           1112          /* Flages                            */
#define BARTYPE           1113          /* Barriers                          */
#define RESTYPE           1120          /* Resources                         */
#define STATQETYPE        1200          /* Statistical Queue Elements        */
#define ACTTYPE           1210          /* Activities                        */
#define PROCTYPE          1211          /* Processes                         */
#define EVTYPE            1212          /* Events                            */
#define MSGTYPE           1220          /* Messages                          */
#define STVARTYPE         1300          /* State Variables                   */
#define IVARTYPE          1310          /* Integer Valued State Variable     */
#define FVARTYPE          1320          /* Real Valued State Variable        */
#define STRECTYPE         1400          /* Statistics Records                */
#define PNTSTATTYPE       1410          /* Point Statistics Records          */
#define INTSTATTYPE       1420          /* Interval STatistics Records       */



/* PARCSIM Objects */
#define PROCRTYPE         3001          /* Processors                        */
#define PRCSTYPE          3002          /* Processes                         */



/*****************************************************************************/
/* Queue element declaration                                                 */
/*****************************************************************************/

typedef struct YS__Qelem
{                                /* Used only with state variables and pools */
  struct YS__Qelem *next;        /* Pointer to the next Qelem in the list    */
  char             *optr;        /* Pointer to an object                     */
} QELEM;



/*****************************************************************************/
/* Queue declarations                                                        */
/*****************************************************************************/

typedef struct YS__Qe
{                               /* Queue element; base of most other objects */
  struct YS__Qe *next;          /* Pointer to the element after this one     */
  int            type;          /* PROCTYPE or EVTYPE                        */
  int            id;            /* System defined unique ID                  */
  char           name[32];      /* User assignable name for dubugging        */
} QE;



/*****************************************************************************/
/* Statistical queue -- the base of activities and messages                  */
/*****************************************************************************/

/* Statistical queue element; base of activities & msgs */
typedef struct YS__StatQe
{
  QE    *next;                      /* Pointer to the element after this one */
  int    type;                      /* PROCTYPE or EVTYPE                    */
  int    id;                        /* System defined unique ID              */
  char   name[32];                  /* User assignable name for dubugging    */

  /* STATQE fields */
  double enterque;                  /* Time the activity enters a queue      */
} STATQE;



/*****************************************************************************/
/* Basic queue of QEs; derived from QE                                       */
/*****************************************************************************/

typedef struct YS__Queue
{
  QE   *next;                       /* Pointer to the element after this one */
  int   type;                       /* Identifies the type of queue element  */
  int   id;                         /* System defined unique ID              */
  char  name[32];                   /* User assignable name for dubugging    */

  /* QUEUE fields */
  QE   *head;                       /* the first element of the queue        */
  QE   *tail;                       /* the last element of the queue         */
  int   size;                       /* Number of elements in the queue       */
} QUEUE;



/*****************************************************************************/
/* Synchronization queue -- the base of semaphore and other YACSIM objects   */
/*****************************************************************************/

typedef struct YS__SyncQue
{
  QE      *next;                /* Pointer to the element after this one     */
  int      type;                /* Identifies the type of queue element      */
  int      id;                  /* System defined unique ID                  */
  char     name[32];            /* User assignable name for dubugging        */

  /* QUEUE fields */
  QE      *head;                /* Pointer to the first element of the queue */
  QE      *tail;                /* Pointer to the last element of the queue  */
  int      size;                /* Number of elements in the queue           */

  /* SYNCQUE fields */
  STATREC *lengthstat;          /* Queue length                              */
  STATREC *timestat;            /* Time in the queue                         */
} SYNCQUE;



/* Create and return a pointer to a new queue */
QUEUE  *YS__NewQueue(const char *);

/* Puts a queue element at the tail of a queue */
void   YS__QueuePutTail(QUEUE*, QE*);

/* the system defined ID or 0 if TrID is 0 */
int    YS__QeId(QE*);

#endif
