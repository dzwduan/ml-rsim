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

#ifndef __RSIM_QUEUE_H__
#define __RSIM_QUEUE_H__

/*************************************************************************/
/***************** Individual elements of Queue **************************/
/*************************************************************************/

template <class Data> struct QueueLink
{
  Data d;
  QueueLink<Data> *next;

  QueueLink(Data x):d(x) 
  {
    next = NULL;
  }
};




/*************************************************************************/
/***************** Queue class definition ********************************/
/*************************************************************************/

template <class Data> class Queue
{
private:
  QueueLink<Data> *head;
  QueueLink<Data> *tail;
  QueueLink<Data> *free;
  int items;

public:
  Queue()	                                /* constructor */
  {
    head  = NULL;
    tail  = NULL;
    items = 0;
    free  = NULL;
  }

  inline void restart()
  {	 	                                /* destructor  */
    QueueLink<Data> *old;
    while (head != NULL) 
      {
	old = head;
	head = head->next;
	delete old;
      }

    while (free != NULL) 
      {
	old = free;
	free = free->next;
	delete old;
      }
    
    head = tail = free = NULL;
    items = 0;
  }

  inline void Enqueue(Data d);		       /* insert element at tail   */

  inline int  Dequeue();		       /* delete element from head */

  inline int  GetHead(Data &d)
  {
    if (!items)
      return 0;
    d = head->d;
    return 1;
  }

  inline int  NumItems() const		       /* get number of items      */
  {
    return items;
  }

  inline int  Empty() const
  {
    if (items)
      return 0;
    else
      return 1;
  }
};



/*************************************************************************/
/***************** Queue class implementation ****************************/
/*************************************************************************/

/********************* Enqueue operation *********************************/

template<class Data> inline void Queue<Data>::Enqueue(Data d)
{
  QueueLink<Data> *item, *next;
  items++;

  if (free)
    {
      next = free->next;
      item = new(free) QueueLink<Data>(d);
      free = next;
    }
  else
    item = new QueueLink<Data>(d);

  if (tail == NULL)
    {
      head = tail = item;
    }
  else
    {
      tail->next = item;
      tail = item;
    }
}



/**************************** Delete operations ***************************/

template <class Data> inline int  Queue<Data>::Dequeue()
{
  if (head == NULL)
    {
      return 0;
    }
  else
    {
      items--;
      QueueLink<Data> *old = head;
      head = head->next;
      if (head == NULL)
	tail = NULL;

#ifdef DEBUG_POOL
      delete old;
#else
      old->next = free;
      free = old;
#endif
    }

  return 1;
}

#endif



