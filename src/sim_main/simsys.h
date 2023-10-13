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

#ifndef __RSIM_SIMSYS_H__
#define __RSIM_SIMSYS_H__

#include "sim_main/util.h"
#include "sim_main/pool.h"
#include "sim_main/stat.h"
#include "sim_main/userq.h"
#include "sim_main/evlst.h"

#define MAXDBLEVEL 5              /* The number of trace levels              */
extern int TraceIDs;              /* If != 0, show object ids in trace output*/
extern int TraceLevel;            /* Controls the amount of trace information*/
extern int DEBUG_TIME;

extern int      YS__interactive;  /* set if running under viewsim or dbsim   */
extern EVENT   *YS__ActEvnt;      /* the currently occurring event           */
extern int      YS__idctr;        /* Used to generate unique ids for objects */
extern char     YS__prbpkt[];     /* Buffer for probe packets                */
extern int      YS__msgcounter;   /* System defined unique message ID        */
extern QUEUE   *YS__PendRes;      /* Queue of Resources to be evaluated      */
extern double   YS__Simtime;      /* The current simulation time             */
extern int      YS__Cycles;       /* Count of profiling cycles accumulated   */
extern double   YS__CycleTime;    /* Cycle time                              */

extern POOL YS__MsgPool;          /* Pool of MESSAGE descriptors             */
extern POOL YS__EventPool;        /* Pool of EVENT descriptors               */
extern POOL YS__QueuePool;        /* Pool of QUEUE descriptor                */
extern POOL YS__SemPool;          /* Pool of SEMAPHORE descriptors           */
extern POOL YS__QelemPool;        /* Pool of QELEM descriptors               */
extern POOL YS__StatPool;         /* Pool of STATREC descriptors             */
extern POOL YS__ReqPool;          /* Pool of REQ descriptors                 */
extern POOL YS__ScsiReqPool;      /* Pool of SCSI bus requests               */

#endif
