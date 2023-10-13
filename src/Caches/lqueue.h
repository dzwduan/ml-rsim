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

#ifndef __RSIM_LQUEUE_H__
#define __RSIM_LQUEUE_H__

/*************************************************************************/
/* Link queue data structure: for all kinds of queues in memory system.  */
/*************************************************************************/

typedef struct
{
  int    head; 	                  /* head pointer of the queue             */
  int    tail;                    /* tail pointer of the queue             */
  int    total;                   /* maximum size of the queue             */
  int    size;                    /* number of elements currently in queue */
  void **elemns;
} LinkQueue;



/*************************************************************************/
/********************* Single linked queue *******************************/
/*************************************************************************/

#define lqueue_init(q, totalsize) 	           \
{ 					           \
  (q)->head = (q)->tail = 0; 	                   \
  (q)->total = totalsize; 		           \
  (q)->size = 0; 			           \
  (q)->elemns = malloc((totalsize) * sizeof(void*)); \
}

/* Is a link queue full? */
#define lqueue_full(q) ((q)->size >= (q)->total)


/* PeekLinkQueue: Return first element in queue without removing it. */
#define lqueue_head(q)    ((q)->size > 0 ? (q)->elemns[(q)->head] : NULL)
#define lqueue_empty(q)   ((q)->size == 0)


/* CheckLinkQueue: returns the number of empty slots in the queue and. */
#define lqueue_check(q) ((q)->total - (q)->size)


/* Insert an element into the specified list. */
#define lqueue_add(lq, elm, node)	      	        \
{							\
  LinkQueue *q = lq;					\
  if (q->size < q->total)                               \
    {  				                        \
      q->elemns[q->tail] = elm;                         \
      q->tail++;                                        \
      if (q->tail == q->total)                          \
	q->tail = 0;                                    \
      q->size++;					\
    }							\
  else 						        \
    YS__errmsg(node, "AddToLinkQueue(): queue is full (%s:%i)", __FILE__, __LINE__);	\
} 



/* Insert an element into the specified list at the head. */
#define lqueue_add_head(lq, elm, node)     	        \
{							\
  LinkQueue *q = lq;					\
  if (q->size < q->total)                               \
    {  				                        \
      q->head = (q->head + (q->total-1)) % q->total;    \
      q->elemns[q->head] = elm;                         \
      q->size++;					\
    }							\
  else 						        \
    YS__errmsg(node, "AddToLinkQueueHead(): queue is full (%s:%i)", __FILE__, __LINE__);	\
} 


/* return Nth element from Queue */
#define lqueue_elem(lq, elm, idx, node)                 \
{                                                       \
  LinkQueue *q = lq;					\
  if (idx < q->size)                                    \
    {  				                        \
      elm = q->elemns[(q->head + idx) % q->total];      \
    }							\
  else 						        \
    YS__errmsg(node, "AddToLinkQueueHead(): queue is full (%s:%i)", __FILE__, __LINE__);	\
} 
  

/* Delete the head of the list. */
#define lqueue_remove(lq)			        \
{							\
  LinkQueue *q = lq;					\
  q->head++;                                            \
  if (q->head == q->total)                              \
    q->head = 0;                                        \
  q->size--;						\
}


/* Get and delete an element */
#define lqueue_get(lq, elm)			        \
{							\
  LinkQueue *q = lq;					\
  elm = q->elemns[q->head]; 			        \
  q->head++;                                            \
  if (q->head == q->total)                              \
    q->head = 0;                                        \
  q->size--;						\
}


/*
 * Dump all the requests in the specified queue. Used for debugging.
 */
#define DumpLinkQueue(name, lq, flag, dump_elm, node) 		             \
{							                     \
  int n;                                                                     \
  YS__logmsg(node, "%s: size = %d\n", name, (lq)->size);                     \
  for (n = 0; n < (lq)->size; n++)                                           \
    {				                                             \
      dump_elm(((lq)->elemns[((lq)->head + n) % (lq)->total]), flag, node);  \
    }							                     \
}




/*************************************************************************/
/******************** Double linked queue ********************************/
/*************************************************************************/

typedef struct
{
  void *head; 	/* head pointer of the queue */
  void *tail; 	/* tail pointer of the queue */
  int  total; 	/* maximum size of the queue */
  int  size;  	/* number of elements currently in queue */
} DLinkQueue;



/*************************************************************************/
/********************* Double linked queue *******************************/
/*************************************************************************/

#define dqueue_init(q, totalsize) 	\
{ 					\
  (q)->head = (q)->tail = NULL; 	\
  (q)->total = totalsize; 		\
  (q)->size = 0; 			\
}

#define dqueue_full(q) (q->size == q->total)

/*
 * Insert an element into the specified queue.
 */
#define dqueue_add(lq, elm, lprev, lnext, type, node)	      	\
        {							\
           DLinkQueue *q = lq;					\
	   if (q->size < q->total) { 				\
	      ((type)elm)->lprev = q->tail;			\
	      ((type)elm)->lnext = q->head;			\
	      if (q->head) { 					\
		 ((type)q->tail)->lnext = elm;			\
		 q->tail = elm;					\
		 ((type)q->head)->lprev = elm;			\
	      }							\
	      else {						\
		 q->head = elm;					\
		 q->tail = elm;					\
	      }							\
	      q->size++;					\
	   }							\
	   else 						\
	      YS__errmsg(node, "AddToLinkQueue(): queue is full (%s:%i)", __FILE__, __LINE__);	\
        }

/*
 * Remove an element from the double linked queue.
 */
#define dqueue_remove(lq, elm, lprev, lnext, type)		\
        {							\
           DLinkQueue *q = lq;					\
           q->size--;						\
           if (q->head == (void *)elm) {			\
              q->head = ((type)q->head)->lnext;			\
              if (q->size == 0)					\
                 q->head = q->tail = NULL;			\
              else {						\
                 ((type)q->head)->lprev = q->tail;		\
                 ((type)q->tail)->lnext = q->head;		\
              }							\
           }							\
           else if (q->tail == elm) {				\
              q->tail = ((type)q->tail)->lprev;			\
              ((type)q->head)->lprev = q->tail;			\
              ((type)q->tail)->lnext = q->head;			\
           } 					 		\
           else {						\
              ((type)elm->lprev)->lnext = elm->lnext;		\
              ((type)elm->lnext)->lprev = elm->lprev;		\
           }							\
        }

/*
 * Dump all the requests in the specified queue. Used for debugging.
 */
#define DumpDLinkQueue(name, lq, link, flag, dump_elm, node) 	            \
        {							            \
           if ((lq)->size > 0) {				            \
	      YS__logmsg(node, "%s: size = %d\n", name, (lq)->size);        \
	      req = (REQ *)((lq)->head); 			            \
	      for (i = 0; i < (lq)->size; i++) { 		            \
		 dump_elm(req, flag, node);   			            \
		 req = req->link; 				            \
	      }							            \
	   }							            \
	}

#endif






