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

#ifndef __RSIM_MEMQ_H__
#define __RSIM_MEMQ_H__

/*************************************************************************/
/***************** Individual elements of MemQ ***************************/
/*************************************************************************/

template <class Data> struct MemQLink
{
  Data d;
  MemQLink<Data> *next, *prev;

  MemQLink(Data x)
  {
    Init(x);
  }

  void Init(Data x)
  {
    d = x;
    next = prev = NULL;
  }
};


/*************************************************************************/
/***************** MemQ class definition *********************************/
/*************************************************************************/

template <class Data> class MemQ
{
private:
  MemQLink<Data> *head;
  MemQLink<Data> *tail;
  MemQLink<Data> *unused;
  int items;

public:
  MemQ(): head(NULL),tail(NULL),unused(NULL),items(0)    /* constructor */
  { }	

  inline void restart()                          /* destructor             */
  {
    MemQLink<Data> *old;
    while (head != NULL) 
      {
	old = head;
	head = head->next;

#ifdef DEBUG_POOL
	delete old;
#else
	old->next = unused;
	unused = old;
#endif
      }
    head = tail = NULL;
    items = 0;
  }

  inline void Insert(Data d);		         /* insert element         */
  inline int  Lookup(Data d);		         /* search for element     */
  inline int  Remove(Data d);		         /* delete element         */
  inline void Remove(MemQLink<Data> *stepper);   /* delete element: type 2 */
  inline int  RemoveGetPrev(Data d, Data &d2);   /* delete element: type 3 */
  inline int  RemoveGetNext(Data d, Data &d2);   /* delete element: type 4 */
  inline void RemoveTail();		         /* delete tail            */
  inline int  Replace(Data d,Data d2);	         /* replace element        */
  inline void Replace(MemQLink<Data> *stepper,Data d2); 

  inline int GetMin(Data &d)
  {
    if (!items)
      return(0);
    d = head->d;
    return 1;
  }

  inline int GetTail(Data &d)
  {
    if (!items)
      return 0;
    d = tail->d;
    return(1);
  }

  inline MemQLink<Data> *GetHead()
  {
    return(head);
  }

  inline MemQLink<Data> *GetNext(MemQLink<Data> *a)
  {
    if (a == NULL)
      return(head);
    else
      return(a->next);
  }

  inline MemQLink<Data> *GetPrev(MemQLink<Data> *a)
  {
    if (a == NULL)
      return(tail);
    else
      return(a->prev);
  }

  inline int NumItems() const 		/* get number of items */
  {
    return(items);
  }
};


/*************************************************************************/
/***************** MemQ class implementation *****************************/
/*************************************************************************/

/********************* Insert operation *********************************/

template<class Data> inline void MemQ<Data>::Insert(Data d)
{
  MemQLink<Data> *item;

  items++;
  if (unused == NULL)
    item = new MemQLink<Data>(d);
  else
    {
      item   = unused;
      unused = item->next;
      item->Init(d);
    }

  if (tail == NULL)
    head = tail = item;
  else
    {
      tail->next = item;
      item->prev = tail;
      tail = item;
    }
}


/********************* Lookup operation *********************************/

template<class Data> inline int MemQ<Data>::Lookup(Data d)
{
  MemQLink<Data> *stepper = head;

  while (stepper != NULL)
    {
      if (stepper->d == d)
	return(1);
      stepper = stepper->next;
    }
  return(0);
}


/************************* Replace operations ****************************/

template<class Data> inline void
MemQ<Data>::Replace(MemQLink<Data> *stepper, Data d2)
{
  stepper->d = d2;
}


template <class Data> inline int MemQ<Data>::Replace(Data d, Data d2)
{
  MemQLink<Data> *stepper = head;

  while (stepper != NULL)
    {
      if (stepper->d == d)
	{
	  stepper->d = d2;
	  return(1);
	}
      stepper = stepper->next;
    }
  return(0);
}


/**************************** Delete operations ***************************/

template <class Data> inline void MemQ<Data>::Remove(MemQLink<Data> *stepper)
{
  items--;
  if (stepper == tail)
    {
      if (stepper == head)
	head = tail = NULL;
      else
	{
	  tail = stepper->prev;
	  tail->next = NULL;
	}
    }
  else if (stepper == head)
    {
      head = stepper->next;
      head->prev = NULL;
    }
  else
    {
      stepper->next->prev = stepper->prev;
      stepper->prev->next = stepper->next;
    }

#ifdef DEBUG_POOL
  delete stepper;
#else
  stepper->next = unused;
  unused = stepper;
#endif
}



template <class Data> inline int MemQ<Data>::Remove(Data d)
{
  MemQLink<Data> *stepper = head;
  
  while (stepper != NULL)
    {
      if (stepper->d == d)
	  {
	    Remove(stepper);
	    return(1);
	  }
      stepper = stepper->next;
    }
  return(0);
}



template <class Data> inline int MemQ<Data>::RemoveGetPrev(Data d, Data& d2)
{
  MemQLink<Data> *prev,*stepper = head;
  
  while (stepper != NULL)
    {
      if (stepper->d == d)
	{
	  prev = stepper->prev;
	  Remove(stepper);
	  if (prev != NULL)
	    d2 = prev->d;
	  return(1);
	}
      stepper = stepper->next;
    }
  return(0);
}



template <class Data> inline int MemQ<Data>::RemoveGetNext(Data d, Data& d2)
{
  MemQLink<Data> *next, *stepper = head;
  
  while (stepper != NULL)
    {
      if (stepper->d == d)
	{
	  next = stepper->next;
	  Remove(stepper);
	  if (next != NULL)
	    d2 = next->d;
	  return(1);
	}
      stepper = stepper->next;
    }
  return(0);
}



template <class Data> inline void MemQ<Data>::RemoveTail()
{
  if (tail)
    {
      items--;
    
      if (tail == head)
	{
#ifdef DEBUG_POOl
	  delete tail;
#else
	  tail->next = unused;
	  unused = tail;
#endif
	  head = tail = NULL;
	}
      else
	{
	  register MemQLink<Data> *stepper = tail;
	  tail = tail->prev;
	  tail->next = NULL;
  
#ifdef DEBUG_POOL
	  delete stepper;
#else
	  stepper->next = unused;
	  unused = stepper;
#endif
	}
    }
}

#endif







