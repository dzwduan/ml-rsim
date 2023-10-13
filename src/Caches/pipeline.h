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

#ifndef __RSIM_PIPELINE_H__
#define __RSIM_PIPELINE_H__

/**************************************************************************/
/**************** Definiton of data structures ****************************/
/**************************************************************************/

typedef struct
{
  void   *elm;
  double  time;
} PipeElm;



typedef struct YS__Pipeline
{
  int      ports;           /* how many pipeline entries can be in each stage */
  int      latency;         /* how many pipeline entries can be in each stage */
  int      repeat_rate;
  int      stages;
  int      count;           /* number of entries in pipe                      */
  PipeElm *elms;
  int      head;
  int      tail;
} Pipeline;




/**************************************************************************/
/**************** Definiton of functions **********************************/
/**************************************************************************/

Pipeline *NewPipeline (int ports, int latency, int repeat_rate, int stages);
int       AddToPipe   (Pipeline *, void *);

/**************************************************************************/
/************ Macros to speed up the simulator ****************************/
/**************************************************************************/

#define PipeEmpty(pline) ((pline)->count == 0)


/*
 * Peek at an element in the head stage of the pipeline.
 */
#define GetPipeElt(req, pline) 			                           \
{					  	                           \
  if ((pline->count > 0) &&                                                \
      (pline->elms[pline->tail].time+pline->latency <= YS__Simtime))       \
    req = (REQ *) pline->elms[pline->tail].elm;                            \
  else                                                                     \
    req = 0;                                                               \
}



/*
 * Remove an element from the pipeline. 
 */
#define ClearPipeElt(pline)        		                           \
{					                	           \
  pline->count--;                                                          \
  pline->tail++;                                                           \
  if (pline->tail == pline->stages*pline->ports)                           \
    pline->tail = 0;                                                       \
}



/* 
 * Replace an element in the pipeline with a new request. 
 */
#define SetPipeElt(req, pline)			                           \
{						                           \
  pline->elms[pline->tail].elm = req;                                      \
}



/*
 * Dump requests in a pipeline.
 */
#define DumpPipeline(name, pline, flag, dump_elm, node)                    \
{                                                                          \
  if (pline->count > 0) {                                                  \
    int n, index;                                                          \
    YS__logmsg(node, "%s: size = %d\n", name, pline->count);               \
    index = pline->tail;                                                   \
    for (n = 0; n < pline->count; n++) {                                   \
       dump_elm(pline->elms[index].elm, flag, node);                       \
       index++;                                                            \
       if (index == pline->stages)                                         \
         index = 0;                                                        \
    }                                                                      \
  }                                                                        \
}



#endif
