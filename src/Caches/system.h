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

#ifndef __RSIM_SYSTEM_H__
#define __RSIM_SYSTEM_H__

#include <limits.h>


/* set to 1 if all processors have finished executing - used to signal to
   other modules not to reschedule periodic events (e.g. RTC) */
extern int EXIT;

/*
 * Statistics or tracing switches.
 */
#define TURN_ON_STATS       MemsimStatOn = ON;
#define TURN_OFF_STATS      MemsimStatOn = OFF;


#define MAX_NODES           32        /* maximum number of nodes             */
#define MAX_MEMSYS_PROCS    16        /* maximum number of CPUs per node     */
#define MAX_MEMSYS_SNOOPERS 24        /* maximum number of snoopers per node */


/*
 * Architecture-specific parameters
 */
extern int ARCH_numnodes;  /* total number of nodes in system                */
extern int ARCH_mynodes;   /* number of nodes local to this simulator        */
extern int ARCH_firstnode; /* first node in this simulator                   */
extern int ARCH_cpus;      /* number of cpus per node                        */
extern int ARCH_ios;       /* number of I/O (other) bus modules per node     */
extern int ARCH_coh_ios;   /* number of coherent I/O modules per node        */
extern int ARCH_cacsz1i;   /* L1 I-cache size                                */
extern int ARCH_setsz1i;   /* L1 I-cache associativity                       */
extern int ARCH_linesz1i;  /* L1 I-cache line size                           */
extern int ARCH_cacsz1d;   /* L1 D-cache size                                */
extern int ARCH_setsz1d;   /* L1 D-cache associativity                       */
extern int ARCH_linesz1d;  /* L1 D-cache line size                           */
extern int ARCH_wbufsz;    /* L1 write-buffer size                           */
extern int ARCH_cacsz2;    /* L2 cache size                                  */
extern int ARCH_setsz2;    /* L2 cache associativity                         */
extern int ARCH_linesz2;   /* L2 cache line size                             */

extern int CPU_CLK_PERIOD;
extern unsigned int PHYSICAL_MEMORY;

extern char trace_dir[PATH_MAX];

/* Memory system initialization function call and statistics routines */
extern void SystemInit ();
extern void StatReport (int);
extern void StatClear  (int);
extern void print_stat (int, char *fmt, ...);


/* Used by get_parameter */
#define PARAM_INT       1
#define PARAM_FLOAT     2
#define PARAM_DOUBLE    3
#define PARAM_STRING    4

int get_parameter(char *pname, void *value, int type);

#endif





