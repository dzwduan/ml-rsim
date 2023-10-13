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

#include <stdio.h>
#include <malloc.h>
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/pipeline.h"
#include "Caches/cache.h"

/*
 * Creates a cache pipeline with the specified number of ports (width 
 * per stage) and number of stages (delay).
 */
Pipeline *NewPipeline(int ports, int latency, int repeat_rate, int stages)
{
  int i;
  Pipeline *pline;

  if (stages == 0)
    YS__errmsg(0, "NewPipeline: stages == 0");
 
  if (ports == 0)
    YS__errmsg(0, "NewPipeline: ports == 0");
 
  pline = (Pipeline *) malloc(sizeof(Pipeline));
  if (pline == 0) 
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  if (latency / repeat_rate != stages) 
    YS__warnmsg(0,
		"NewPipeline: latency(%d) %% repeat_rate(%d) != stages(%d)\n",
		latency, repeat_rate, stages);
 
  pline->ports       = ports;
  pline->latency     = latency;
  pline->repeat_rate = repeat_rate;
  pline->stages      = stages;
  pline->count       = 0;

  pline->elms = RSIM_CALLOC(PipeElm, stages * ports);
  if (pline->elms == NULL)
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);
  pline->head        = 0;
  pline->tail        = 0;

  return pline;
}


/*
 * Add a new entry to input stage of pipeline. return 1 on success,
 * and 0 on failure.
 */
int AddToPipe(Pipeline *pline, void *req)
{
  int   i;

  if (pline->count >= pline->stages * pline->ports)
    return(0);

  i = (pline->head + pline->ports - 1) % (pline->latency * pline->ports);

  if ((pline->count >= pline->ports) &&
      (pline->elms[i].time + pline->repeat_rate > YS__Simtime))
    return(0);

  pline->count++;
  pline->elms[pline->head].elm  = req;
  pline->elms[pline->head].time = YS__Simtime;
  pline->head++;
  if (pline->head == (pline->stages * pline->ports))
    pline->head = 0;
 
  return 1;
}
