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

#ifndef __RSIM_ALLOCATOR_H__
#define __RSIM_ALLOCATOR_H__



/***************************************************************************/
/**********************  Allocator class routines  *************************/
/***************************************************************************/

template <class Data> class Allocator
{
  typedef Data *dp;

protected:
  Data **arr;
  Data **original_newed_objects; // this gets copied into arr at reset time
  int  sz;
  int  tail;
  void (*reset_func)(Data *);

  
public:
  // new the arrays, then new the original objects, then call reset
  Allocator(int s, void (*rf)(Data *) = NULL)
  {
    arr = (dp*)malloc(sizeof(dp) * s);
    original_newed_objects = (dp*)malloc(sizeof(dp) * s);

    sz = s;

    for (int i = 0; i < s; i++)
      {
	original_newed_objects[i] = (Data*)malloc(sizeof(Data));
	memset((char *)original_newed_objects[i], 0, sizeof(Data));
      }

    reset_func = rf;
    reset();
  }

  
  // delete the newed objects, delete the arrays
  ~Allocator()
  {
    for (int i = 0; i < sz; i++)
      free(original_newed_objects[i]);
    free(arr);
    free(original_newed_objects);
  }

  
  // put back into stack
  inline int Putback(Data *d)
  {
    if (tail == sz) 
      return 0; 
    arr[tail++] = d; return 1;
  }


  // get from stack
  inline Data *Get()
  {
    if (tail == 0)
      return 0;
    
    return arr[--tail];
  }

  
  // number of elements in stack
  inline int Elts() const 
  {
    return tail;
  }

  
  // reset stack
  inline void reset()
  {
    tail = sz;
    memcpy(arr, original_newed_objects, sizeof(dp) * sz);
    if (reset_func)
      {
	for (int i = 0; i < sz; i++)
	  (*reset_func)(arr[i]);
      }
  }
};




/***************************************************************************/
/******************  Dynamic Allocator class routines  *********************/
/***************************************************************************/

template <class Data> class DynAllocator
{
  typedef Data *dp;

protected:
  Data **arr;
  int  sz;
  int  tail;

  
public:
  // new the arrays, then new the original objects, then call reset
  DynAllocator(int s)
  {
    arr = (dp*)malloc(sizeof(dp) * s);
    sz = s;

    for (int i = 0; i < s; i++)
      {
	arr[i] = (Data*)malloc(sizeof(Data));
	memset((char *)arr[i], 0, sizeof(Data));
      }

    tail = sz;
  }

  
  // delete the newed objects, delete the arrays
  ~DynAllocator()
  {
    for (int i = 0; i < sz; i++)
      free(arr[i]);
    free(arr);
  }

  
  // put back into stack
  inline int Putback(Data *d)
  {
    if (tail == sz) 
      return 0; 
    arr[tail++] = d; return 1;
  }


  // get from stack
  inline Data *Get()
  {
    if (tail == 0)
      {
	arr = (Data**)realloc(arr, sizeof(Data*) * sz * 2);
	for (int i = 0; i < sz; i++)
	  {
	    arr[i] = (Data*)malloc(sizeof(Data));
	    memset((char *)arr[i], 0, sizeof(Data));
	  }
	

	tail = sz;
	sz *= 2;
      }
    
    return arr[--tail];
  }

  
  // number of elements in stack
  inline int Elts() const 
  {
    return tail;
  }
};




/***************************************************************************/
/**********************   Stack class routines  ****************************/
/***************************************************************************/

template <class Data> class Stack 
{
private:
  Data *arr;
  //  Data *origmap;
  //  unsigned origused;
  unsigned sz;
  unsigned tail;

  
public:
  Stack (int s)//(Data *omap, int used, int s)
  {
    //    origmap = omap;
    //    origused = used;
    sz = s;
    arr = new Data[sz];
    reset();
  }

  
  // delete the newed objects,delete the arrays
  ~Stack() 
  {
    delete[] arr;
  } 

  
  // push data into stack
  inline int Push(Data d)
  {
    if (tail == sz) 
      return 0; 

    arr[tail++] = d;
    return 1;
  }

  
  // pop data from stack
  inline int Pop(Data& d)
  {
    if (tail == 0) 
      return 0; 

    d = arr[--tail];
    return 1;
  }

  
  // number of elements in stack
  inline int used() const
  {
    return tail;
  }

  
  // Is stack full??
  inline int full() const 
  {
    return tail == sz;
  }

  
  // reset stack
  inline void reset()
  {
    //    tail = origused;
    //   memcpy(arr, origmap, origused*sizeof(Data));
    for (tail = 1; tail < sz; tail ++)
      arr[tail-1] = tail-1;
    tail = sz-1;
  }
};

#endif
