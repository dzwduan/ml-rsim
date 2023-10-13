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

#ifndef __RSIM_HEAP_H__
#define __RSIM_HEAP_H__

extern "C"
{
}


#define MINHEAPSZ	8


/***************************************************************************/
/************************** Heap class definition **************************/
/***************************************************************************/

#define  lchild(node)  ((node << 1) + 1)
#define  rchild(node)  ((node << 1) + 2)
#define  parent(node)  ((node-1) >> 1)

template<class T> class Heap
{
private:
  T         *obj;
  long long *ord;
  int        size;
  int        used;

public:

  Heap(int sz): size(sz), used(0) 
  {
    obj = new T[sz];
    ord = new long long[sz];
  }

  ~Heap()
  {
    delete[] obj;
    delete[] ord;
  }

  inline int insert(long long ts, T o); // Insert element o with timestamp ts
  inline long long GetMin(T& min);      // return min object & timestamp
  
  inline int num() const                // return number of elements in heap
  {
    return used;
  }

  inline long long PeekMin() const      // return min timestamp
  {
    return ord[0];
  }
};


/***************************************************************************/
/****************** Heap class member function definitions *****************/
/***************************************************************************/

template<class T> inline int Heap<T>::insert(long long ts, T o)
{
  if (used >= size)
    return 0;
  
  obj[used] = o;
  ord[used] = ts;
  
  int step = used++;
  int par = parent(step);

  while (step >0 && (ts < ord[par]))
    {
      ord[step] = ord[par];
      obj[step] = obj[par];
      obj[par] = o;
      ord[par] = ts;

      step = par;
      par = parent(par);
    }

  return 1;
}


template<class T> inline long long Heap<T>::GetMin(T& min)
{
  if (used == 0)
    return -1;
   
  long long ts, ans;
  int l, r, done, posn;

  ans = ord[0];
  min = obj[0];
  --used;

  if (used > 0)
    {
      T   prop = obj[0] = obj[used];
      ts = ord[0] = ord[used];
      posn = 0;
      done = 0;
      while (!done)
	{
	  l = lchild(posn);
	  r = rchild(posn);
	  if (l >= used) // means that we are at a leaf
	    break;
	  if (r >= used)
	    { // no right child 
	      ord[r] = ord[posn] ; // so we can handle it all together
	      obj[r] = obj[posn] ; 
	    }
	 
	  int lts = ord[l], rts = ord[r];
	  int lcomp = (lts < ts), rcomp = (rts < ts), lrcomp = (lts < rts);

	  switch (lcomp * 4 + rcomp * 2 + lrcomp)
	    {
	    case 0: // parent is least
	    case 1:
	      done = 1;
	      break;

	    case 2: // right is least, left greatest
	    case 3:
	    case 6: // right least, parent greatest
	      ord[posn] = rts;
	      obj[posn] = obj[r];
	      ord[r] = ts;
	      obj[r] = prop;
	      posn = r;
	      break;

	    case 4: // left is least, right greatest
	    case 5:
	    case 7: // left the lowest, parent greatest
	      ord[posn] = lts;
	      obj[posn] = obj[l];
	      ord[l] = ts;
	      obj[l] = prop;
	      posn = l;
	      break;

	    default: // can't happen
	      break;
	    }
	}
    }
  return ans;
}



/**************************************************************************/
/******************* InstHeap class defintion *****************************/
/**************************************************************************/
/* The InstHeap is similar to the ordinary heap of heap.h, with a         */
/* slightly different definition of "less".  Just like any other heap, an */
/* inst heap also requires parent to be less than both children.          */
/*                                                                        */
/* However, "less" here is defined as having a lower timestamp or         */
/* having an equal timestamp and a lesser tag (only the former is         */
/* considered in ordinary heaps)                                          */
/**************************************************************************/



struct instance;

class InstHeap
{
  typedef instance *instptr;
  
private:
  instptr   *obj;
  long long *tags;
  long long *ord;
  int        size;
  int        used;
   
public:
  InstHeap():size(MINHEAPSZ),used(0)
  {
    obj = new instptr[size];
    tags = new long long[size];
    ord = new long long[size];
  }


  ~InstHeap()
  {
    delete[] obj;
    delete[] tags;
    delete[] ord;
  }

  
  inline int num() const 
  {
    return used;
  }

  /* return min timestamp */
  inline long long PeekMin() const 
  {
    return ord[0];
  } 

  /* return the head instance */
  inline instance *PeekHeadInst() const
  {
    return obj[0];
  }

  /* 
   * The following three member functions are defined below. 
   *   void resize(int);
   *   int insert(long long ts, instance *o, long long tag);
   *   int GetMin(instance *& min, long long& tag); 
   */

  inline void resize(int sz)
  {
    instance **obj2 = new instptr[sz];
    long long *tags2 = new long long[sz];
    long long *ord2  = new long long[sz];

    memcpy(obj2,  obj,  MIN(sz, size) * sizeof(instptr));
    memcpy(tags2, tags, MIN(sz, size) * sizeof(long long));
    memcpy(ord2,  ord,  MIN(sz, size) * sizeof(long long));

    delete []obj; delete []tags; delete []ord;
    
    size = sz;
    obj  = obj2;
    tags = tags2;
    ord  = ord2;
  }

  inline int insert(long long ts, instance * o, long long tag)
  {
    if (used >= size) 
      resize(2 * size);

    int step, par;

    obj[used] = o;
    tags[used] = tag;
    ord[used] = ts;
    step = used++;
    par = parent(step);

    while(step > 0 && (ts < ord[par] || (ts == ord[par] && tag<tags[par])))
      {
	ord[step] = ord[par];
	tags[step] = tags[par];
	obj[step] =  obj[par];
	obj[par] = o;
	tags[par] = tag;
	ord[par] = ts;

	step = par;
	par = parent(par);
      }
    return 1;
  }

  
  inline long long GetMin(instance * &min, long long &tag)
  {
    instance *prop;
    int  l, r, posn, done; 
    long long ptag, ts, ans;
    
    if (used == 0)
      return -1;

    min = obj[0];
    tag = tags[0];
    ans = ord[0];

    if (--used > 0)
      {
	ts   = ord[0]  = ord[used];
	prop = obj[0]  = obj[used];
	ptag = tags[0] = tags[used];

	posn = 0; done = 0;
	while (!done)
	  {
	    l = lchild(posn);
	    r = rchild(posn);
	    if (l >= used)	// means that we are at a leaf
	      break;
	    if (r >= used)
	      {       // no right child
		ord[r] = ord[posn]; // so we can handle it all together
		obj[r] = obj[posn];		
		tags[r] = tags[posn];
	      }

	    long long lts  = ord[l],  rts  = ord[r];
	    long long ltag = tags[l], rtag = tags[r];
	    int lcomp  = (lts < ts  || (lts == ts && ltag < ptag)), 
	      rcomp  = (rts < ts  || (rts == ts && rtag < ptag)),
	      lrcomp = (lts < rts || (lts == rts && ltag < rtag));

	    switch (lcomp * 4 + rcomp * 2 + lrcomp)
	      {
	      case 0:	// parent is least
	      case 1:
		done = 1;
		break;
		
	      case 2:	// right is least, left greatest
	      case 3:
	      case 6:	// right least, parent greatest
		ord[posn]  = rts; 
		obj[posn]  = obj[r]; 
		tags[posn] = rtag;
		ord[r]  = ts; 
		obj[r]  = prop; 
		tags[r] = ptag;
		posn = r;
		break;

	      case 4:	// left is least, right greatest
	      case 5:
	      case 7:	// left the lowest, parent greatest
		ord[posn] = lts; 
		obj[posn] = obj[l]; 
		tags[posn] = ltag;
		ord[l]  = ts; 
		obj[l]  = prop; 
		tags[l] = ptag;
		posn = l;
		break;

	      default:	// can't happen
		break;
	      }
	  }
      }
    return ans;
  }
};

#endif
