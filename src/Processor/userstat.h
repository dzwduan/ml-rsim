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

/***************************************************************************/
/* User statistics package: allows applications to collect timing data     */
/* with very low overhead. Statistics structures are allocated through a   */
/* simulator trap with a statistics type and unique name as arguments. If  */
/* the name matches an already existing structure, that handle is          */
/* returned, otherwise a new structure is allocated.                       */
/* Statistics are collected by a second simulator trap, semantics depend   */
/* on the particular statistics type.                                      */
/* Statistics structures are managed as a list of pointers to the          */
/* individual objects. The array grows as new structures are allocated.    */
/* Upon reset or print, the respective routines for all objects are called.*/
/***************************************************************************/


#ifndef __RSIM_USERSTAT_H__
#define __RSIM_USERSTAT_H__


/* standard C definitions - for initialization ----------------------------*/
#ifndef __cplusplus


void UserStats_init   (void);
void UserStats_report (int);
void UserStats_clear  (int);


/* C++ definitions --------------------------------------------------------*/
#else


/*-------------------------------------------------------------------------*/
/* base class for user statistics objects: implements only name and empty  */
/* member functions, needs to be overloaded by specific implementations.   */
/*-------------------------------------------------------------------------*/

class user_stat
{
protected:
  char *name;
  int   node;

public:
  user_stat(char *na, int no)
  {
    name = strdup(na);
    node = no;
  };
  
  virtual ~user_stat(void)
  {};
  
  char *getname(void)
  {
    return name;
  };

  virtual void reset(void)
  {};
  
  virtual void print(int)
  {};
  
  virtual void sample(int)
  {};
};




/*-------------------------------------------------------------------------*/
/* per-node user statistics object                                         */
/*-------------------------------------------------------------------------*/

class user_stats
{
private:
  int         node;
  int         num_elemns;
  user_stat **elemns;

public:
  user_stats  (int);
  ~user_stats ();

  void reset  ();
  void print  (int);
  int  alloc  (int, char *);
  void sample (int, int);
};



extern user_stats **UserStats;  /* global array of user statistics objects */

#endif


#endif
