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

/*
 * globals.c
 * 
 * This file contains some global declarations which are used throught the
 * memory system simulator, particularly with regard to network statistics. 
 */


#include <stdio.h>
#include "sim_main/simsys.h"
#include "sim_main/pool.h"

int MemsimStatOn = 1;


/******************************** Global Variables **************************
**                                                                         **
**  All global variabes, except those that are are static and only accessed**
**  in a single file, are defined here.  There is a corresponding extern   **
**  declaration of these variables in the file simsys.h. Declarations that **
**  start with YS__ should not visible to the user.                        **
****************************************************************************/

POOL YS__MsgPool;                      /* Pool of MESSAGE descriptors      */
POOL YS__EventPool;                    /* Pool of EVENT descriptors        */
POOL YS__QueuePool;                    /* Pool of QUEUE descriptors        */
POOL YS__SemPool;                      /* Pool of SEMAPHORE descriptors    */
POOL YS__QelemPool;                    /* Pool of QELEM descriptors        */
POOL YS__StatPool;                     /* Pool of STATREC descriptors      */
POOL YS__ReqPool;                      /* Pool of REQ descriptors          */
POOL YS__ScsiReqPool;                  /* Pool of SCSI bus requests        */

double YS__Simtime;           /* The current simulation time               */
EVENT *YS__ActEvnt;           /* Pointer to the currently active event     */

int  YS__idctr;               /* Used to generate unique ids for objects   */
char YS__prbpkt[1024];        /* Buffer for probe packets                  */
int  YS__msgcounter = 0;      /* System defined unique message ID          */
int  YS__interactive = 0;     /* Flag, set if for viewsim or shsim         */

int YS__Cycles = 0;           /* Count of accumulated profiling cycles     */
double YS__CycleTime = 1.0;   /* Cycle Time                                */
int YS__NetCycleTime = 1.0;   /* Network cycle time (in clock cycles)      */


/**************** Global variables visible to the user *********************/

int TraceLevel = 0;           /* Controls the amount of trace information  */
int TraceIDs = 1;             /* If != 0, show object ids in trace output  */





