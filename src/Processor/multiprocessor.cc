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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#ifndef linux
#include <sys/siginfo.h>
#endif

extern "C"
{
#include "sim_main/simsys.h"
#include "sim_main/evlst.h"
#include "Caches/system.h"

}


#include "Processor/simio.h"
#include "Processor/multiprocessor.h"

int              total_processes = 1;
int              my_process_id   = 0;

int              barrier_id      = -1;
lrsim_barrier_t *barrier_ptr     = NULL;
EVENT           *barrier_event;

int              procgrp = -1;



extern "C" void sigchldhandler(int sig, siginfo_t *si)
{
  int n, pid, status;

  printf("SIG CHILD\n");

  if ((si->si_code == CLD_TRAPPED) ||
      (si->si_code == CLD_STOPPED) ||
      (si->si_code == CLD_CONTINUED) ||
      (si->si_code == CLD_EXITED))
    return;

#ifdef linux
  pid    = si->si_pid;
  status = si->si_status;
#else
  pid    = si->__data.__proc.__pid;
  status = si->__data.__proc.__pdata.__cld.__status;
#endif
  
  for (n = ARCH_firstnode; n < ARCH_firstnode + ARCH_mynodes; n++)
    {
      if (si->si_code == CLD_KILLED)
	YS__logmsg(n, "\nChild %i killed with status 0x%04X\n", pid, status);
      if (si->si_code == CLD_DUMPED)
	YS__logmsg(n, "\nChild %i dumped with status 0x%04X\n", pid, status);
      YS__logmsg(n, "\nExiting ...\n\n");
    }

  if (procgrp != -1)
    kill(-procgrp, SIGKILL);
  
  exit(1);
}



void DoMP(int mp)
{
  int               rc;
  struct sigaction  sa;

  
  ARCH_firstnode = 0;
  ARCH_mynodes = ARCH_numnodes;
  

#if defined(__sparc) || defined(linux)
  total_processes = sysconf(_SC_NPROCESSORS_ONLN);
#endif
#ifdef sgi
  total_processes = sysconf(_SC_NPROC_ONLN);
#endif

  if (total_processes > ARCH_numnodes)
    total_processes = ARCH_numnodes;

  if (total_processes > mp)
    total_processes = mp;


  ARCH_firstnode = 0;
  ARCH_mynodes   = (ARCH_numnodes + total_processes - 1) / total_processes;
  total_processes = (ARCH_numnodes + ARCH_mynodes - 1) / ARCH_mynodes;

  printf("Running simulation on %i CPU%c\n",
	 total_processes, total_processes > 1 ? 's' : ' ');
  fflush(stdout);


  /*-------------------------------------------------------------------------*/
  /* create shared memory region for barrier if needed                       */

  if (total_processes > 1)
    {
      barrier_id = shmget(IPC_PRIVATE,
			  sizeof(lrsim_barrier_t) +
			  sizeof(unsigned) * ARCH_numnodes,
                          IPC_CREAT | SHM_R | SHM_W);
      if (barrier_id < 0)
        fprintf(stderr,
		"Error: ShmGet failed at %s:%i: %s\n",
		__FILE__, __LINE__, YS__strerror(errno)), exit(1);

      barrier_ptr = (lrsim_barrier_t*)shmat(barrier_id, NULL, 00400 | 00200);
      if (barrier_ptr < (lrsim_barrier_t*)NULL)
        fprintf(stderr,
		"Error: ShmAt failed at %s:%i: %s\n",
		__FILE__, __LINE__, YS__strerror(errno)), exit(1);

      barrier_ptr->lock      = 0;
      barrier_ptr->count     = 0;
      barrier_ptr->bcast     = 0;
      barrier_ptr->num_procs = ARCH_cpus;
      barrier_ptr->procs_lock = 0;

      for (int n = 0; n < ARCH_numnodes; n++)
	barrier_ptr->wakeup[n] = 0;

      barrier_event = NewEvent("RSIM Barrier", DoBarrier, NODELETE, 0);
      schedule_event(barrier_event, YS__Simtime + BARRIER_INTERVAL);
    }

  
  /*-------------------------------------------------------------------------*/
  /* fork simulation processes                                               */

  if (total_processes > 1)
    procgrp = setpgrp();

  for (my_process_id = total_processes - 1;
       my_process_id > 0;
       my_process_id--)
    {
      rc = fork();
      if (rc < 0)
	fprintf(stderr, "Error: Fork failed: %s\n", YS__strerror(errno));

      if (rc == 0)
	break;

      ARCH_firstnode += ARCH_mynodes;
    }

  if (my_process_id == 0)
    ARCH_mynodes = ARCH_numnodes - (total_processes - 1) * ARCH_mynodes;

  if (total_processes > 1)
    {
      atexit(CleanupMP);

      sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))sigchldhandler;
      sigemptyset(&sa.sa_mask);
      sa.sa_flags   = SA_SIGINFO | SA_NOCLDSTOP;
      sigaction(SIGCHLD, &sa, NULL);
    }
}



void DoExit()
{
  if (total_processes == 1)
    return;
  
  get_lock(&barrier_ptr->procs_lock);
  barrier_ptr->num_procs--;
  clr_lock(&barrier_ptr->procs_lock);
}



extern "C" void CleanupMP()
{
  int n;

  if (barrier_ptr != NULL)
    shmdt((char*)barrier_ptr);

  if (my_process_id == 0)
    {
      for (n = 1; n < total_processes; n++)
	wait(NULL);

      if (barrier_id >= 0)
	shmctl(barrier_id, IPC_RMID, NULL);
    }
}



#ifdef sparc
extern "C" void _yield();
#endif

#if defined(sgi) || defined(linux)
#include <sched.h>
#define _yield sched_yield
#endif


extern "C" void DoBarrier()
{
  int old, i;

  get_lock(&barrier_ptr->lock);

  barrier_ptr->count++;

  if (barrier_ptr->count < total_processes)
    {
      old = barrier_ptr->bcast;
      clr_lock(&barrier_ptr->lock);

      while (barrier_ptr->bcast == old)
	{
	  for (i = 0; i < 100000; i++)
	    if (barrier_ptr->bcast != old)
	      break;

	  if (barrier_ptr->bcast == old)
	    _yield();
	}
    }
  else
    {
      barrier_ptr->count = 0;
      barrier_ptr->bcast++;

      clr_lock(&barrier_ptr->lock);
    }

  if (barrier_ptr->num_procs)
    schedule_event(barrier_event, YS__Simtime + BARRIER_INTERVAL);

  for (i = ARCH_firstnode; i < ARCH_firstnode + ARCH_mynodes; i++)
    if (barrier_ptr->wakeup[i])
      {
	barrier_ptr->wakeup[i] = 0;

      }
}

