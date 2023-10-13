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

#ifndef __RSIM_CIRCQ_H__
#define __RSIM_CIRCQ_H__

#define TAG_CMP(a, b)  ((a)->tag - b)

/***********************************************************************/
/*********************** circq class definition ************************/
/***********************************************************************/

template <class Data> class circq       // high power FIFO, limited size
{
private:

  Data    *arr;       // data
  unsigned sz;        // max size, must be a power of 2.
  unsigned mask;      // mask = size-1
  unsigned head;      // head of FIFO
  unsigned tail;      // tail of FIFO
  int      cnt;       // Number of elements in FIFO
  int      max;       // max # of elements actually in FIFO;
                      // may differ from circq sz if non-power of 2
public:

  /* This is used only as default constructor for array cases,
     and should be immediately followed by start(int) call */
  circq() {}

  circq(int s) 
  {
    start(s);
  }

  ~circq() 
  {
    delete[] arr;
  }

  void start(int s)
  {
    max  = s;
    sz   = ToPowerOf2(s);
    mask = sz-1;
    head = tail = cnt = 0; 
    arr  = new Data[sz];
  }

  inline int Insert(Data d)
  {
    if (cnt == max) 
      return 0; 
    ++cnt; arr[tail++]=d; tail &= mask; return 1;
  }

  inline int InsertAtHead(Data d)
  {
    if (cnt == max) 
      return 0; 
    ++cnt; --head; head &= mask; arr[head] = d; return 1;
  }

  inline int Delete(Data& d)
  {
    if (cnt == 0) 
      return 0; 
    --cnt; d=arr[head]; ++head; head &= mask; return 1;
  }

  inline int DeleteFromTail(Data &d)
  {
    if (cnt == 0) 
      return 0; 
    --cnt; --tail; tail &= mask; d=arr[tail]; return 1;
  }

  /* Return head element in d * */
  inline Data PeekHead()
  { 
    if (cnt == 0) 
      return 0; 
    return arr[head];
  }

  /* Return tail element in d */
  inline Data PeekTail()
  {
    if (cnt == 0) 
      return 0; 
    return arr[(tail-1) & mask];
  }

  /* Return element ind */
  inline int PeekElt(Data &d, int ind)
  {
    if (cnt <= ind) 
      return 0; 
    d = arr[(head+ind) & mask];
    return 1;
  } 

  /* Modify element d */ 
  inline int SetElt(Data d, int ind)
  {
    if (cnt <= ind) 
      return 0; 
    arr[(head+ind) & mask] = d;
    return 1;
  } 

  /* Return number of elements */
  inline int NumInQueue() const
  {
    return cnt;
  }

  inline void reset() 
  {
    cnt = head = tail = 0;
  }

  /* All of the folling search functions perform binary search. */
  int Search(const Data& key, Data& out) const;
  int Search(long long key, Data& out) const;
  int Search2(const long long key, Data& out1, Data& out2) const;
};

/*************************************************************************/
/*********************** circq class implementation **********************/
/*************************************************************************/

template <class Data> inline int 
circq<Data>::Search(const Data& key, Data& out) const
{

#if 0  
  int lo, lbnd, hi, hbnd, mid;
  long long res;

  lo = lbnd = head;
  hi = hbnd = tail-1;
  if (hi < lo)
    {
      hi += sz;
      hbnd += sz;
    }

  while (lo <= hi)
    {
      mid = (lo+hi)/2;
      res = TAG_CMP(arr[mid&mask], key->tag);

      if (res == 0)
	{
	  out = arr[mid&mask];
	  return 1;
	}
      else if (res < 0) 
	lo = mid+1;
      else 
	hi = mid-1;
    }

  return 0;
#endif
}


template <class Data> inline int 
circq<Data>::Search(long long key, Data& out) const
{
  int lo, hi, hbnd, mid;
  long long res;

  lo = head;
  hi = hbnd = tail-1;
  if (hi < lo)
    {
      hi += sz;
      hbnd += sz;
    }

  while (lo <= hi)
    {
      mid = (lo+hi)/2;
      res = TAG_CMP(arr[mid&mask], key);

      if (res == 0)
	{
	  out  = arr[mid&mask];
	  return 1;
	}
      else if (res < 0) 
	lo = mid+1;
      else 
	hi = mid-1;
    }

  return 0;
}


/*
 * Search2() is specfically written for activelist search.
 */
template <class Data> inline int 
circq<Data>::Search2(const long long key, Data& out1, Data& out2) const
{
  int lo, lbnd, hi, hbnd, mid;
  long long res;

  lo = lbnd = head;
  hi = hbnd = tail-1;
  if (hi < lo)
    {
      hi += sz;
      hbnd += sz;
    }

  while (lo <= hi)
    {
      mid = (lo+hi)/2;
      res = TAG_CMP(arr[mid&mask], key);

      if (res == 0)
	{
	  out1 = arr[mid&mask];
	  int t = mid-1;
	  if (t >= lbnd && t <= hbnd)
	    {
	      Data o2 = arr[t&mask];
	      if (TAG_CMP(o2, key) == 0)
		{
		  out2 = o2;
		  return 1;
		}
	    }
	  t = mid+1;
	  if (t >= lbnd && t <= hbnd)
	    {
	      Data o2 = arr[t&mask];
	      if (TAG_CMP(o2, key) == 0)
		{
		  out2 = o2;
		  return 1;
		}
	    }
	  return 0;
	}

      if (res < 0)
	lo = mid+1;
      else
	hi = mid-1;
    }
  return 0;
}

#endif
