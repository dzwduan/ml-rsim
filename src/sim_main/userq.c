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
 * userq.c
 *
 * This file provides the resource queues and sempahores used in various
 * places in the memory system simulator.
 */

#include <stdio.h>
#include <string.h>
#include "sim_main/simsys.h"
#include "sim_main/tr.userq.h"


/*****************************************************************************/
/* QUEUE Operations: Queues implement the basic linked lists used by         */
/* several other objects (e.g., semaphores).  They are simple single-link    */
/* lists with a pointer to the head and tail and links that point in the     */
/* direction from head to tail.  The generic element for these queues is     */
/* a QE, a structure that forms the base of several different types of       */
/* objects that can to put in queues (e.g., events, queues themselves).      */
/* There are several basic operations to add elements to and delete them     */
/* from queues.  Four special insertion operations insert events ordered by  */
/* one of two keys                                                           */
/*****************************************************************************/

/*
 * Create and return a pointer to a new queue
 */
QUEUE *YS__NewQueue(const char *qname)
{
  QUEUE *qptr;

  qptr = (QUEUE *) YS__PoolGetObj(&YS__QueuePool);

  qptr->id = YS__idctr++;      /* QE fields */
  qptr->type = QUETYPE;

  strncpy(qptr->name, qname, 31);      /* QUEUE fields */
  qptr->name[31] = '\0';
  qptr->head = NULL;
  qptr->tail = NULL;
  qptr->size = 0;

  return qptr;
}


/*
 * Puts a queue element at the tail of a queue
 */
void YS__QueuePutTail(QUEUE *qptr, QE *qeptr)
{
  SYNCQUE *sqptr = (SYNCQUE *) qptr;   /* Only cast qptr once */
  STATQE *sqeptr = (STATQE *) qeptr;   /* Only cast qeptr once */

  TRACE_QUEUE_puttail; /* Putting on the tail of queue */
  if (qptr == NULL)
    YS__errmsg(0, "Null queue pointer passed to QueuePutTail");
  
  if (qeptr == NULL)
    YS__errmsg(0, "Null queue element pointer passed to QueuePutTail");
  
  if (qptr->head == NULL)
    {    /* The queue is empty */
      qptr->head = qeptr;       /* Tail and head elements are the same */
      qeptr->next = NULL;       /* (just to be sure) */
    }
  else
    {               /* Adding to the tail of a non-empty queue */
      qptr->tail->next = qeptr; /* Set forward link of old tail element */
      qeptr->next = NULL;       /* Clear next pointer of new tail element */
    }                    /* (just to be sure) */
  
  qptr->tail = qeptr;  /* New element is now the tail */
  qptr->size++;        /* One more element in the queue */

  if (qptr->type == SYNCQTYPE)
    { /* Resource statistics done in userq.c */
      sqeptr->enterque = YS__Simtime;   /* For time in queue statistics */
      if (sqptr->lengthstat)    /* Queue length statistics collected */
	YS__warnmsg(0, "Queue statistics not supported\n");
/*	StatrecUpdate(sqptr->lengthstat, (double) qptr->size, YS__Simtime); */
    }
  TRACE_QUEUE_show(qptr);      /* Prints the contents of the queue */
}


/*
 * Returns the system defined ID or 0 if TrID is 0
 */
int YS__QeId(QE *qeptr)
{
  if (TraceIDs)
    return qeptr->id;
  else
    return 0;
}
