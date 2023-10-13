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

#include <signal.h>
#include <unistd.h>
#ifdef linux
#  include <ieee754.h>
#  include <fpu_control.h>
#  include "Processor/linuxfp.h"
#else
#  include <ieeefp.h>
#endif
#include <ucontext.h>
#include <sys/utsname.h>

extern "C"
{
#include "sim_main/simsys.h"
}

#include "Processor/procstate.h"
#include "Processor/memunit.h"
#include "Processor/simio.h"
#include "Processor/fsr.h"

#include "../../lamix/machine/intr.h"


extern "C" void MemoryDumpAll();

extern "C" void killhandler(int)
{
  printf("Killed ...\n");
  fflush(stdout);
  exit(2);
}


extern int procgrp;

extern "C" void faulthandler(int sig, siginfo_t *si, ucontext_t *uc)
{
  int n;
  
  for (n = ARCH_firstnode; n < ARCH_firstnode + ARCH_mynodes; n++)
    {
      if (sig == SIGILL)
	YS__logmsg(n, "\nProcess received SIGILL\n");
      if (sig == SIGSEGV)
	YS__logmsg(n, "\nProcess received SIGSEGV\n");
      if (sig == SIGBUS)
	YS__logmsg(n, "\nProcess received SIGBUS\n");
      if (sig == SIGTRAP)
	YS__logmsg(n, "\nProcess received SIGTRAP\n");
#ifdef __sparc
      YS__logmsg(n, "Addr: 0x%08X  PC: 0x%08X/0x%08X\n\n",
		 si->si_addr,
		 uc->uc_mcontext.gregs[REG_PC],
		 uc->uc_mcontext.gregs[REG_nPC]);
#endif
#ifdef sgi
      YS__logmsg(n, "Addr: 0x%08X   PC: 0x%08uX\n\n",
		 si->si_addr,
		 uc->uc_mcontext.gregs[CTX_EPC]);
#endif
    }

  if (procgrp != -1)
    kill(-procgrp, SIGKILL);

  MemoryDumpAll();
  
  exit(1);  
}



int ctrl_c_count = 0;
#define MAX_CTRL_C_COUNT 20

/*=============================================================================
 * inthandler()
 *   trap ctrl-C: forward interrupt 1 to the first CPU in every node.
 *   Reinstalls itself for the first MAX_CTRL_C_COUNT signals, and prints
 *   a warning message to the logfile for the last time. The next signal
 *   will abort the simulation.
 */
extern "C" void inthandler(int)
{
  int n;
  
  if (ctrl_c_count == MAX_CTRL_C_COUNT)
    {
      for (n = ARCH_firstnode;
	   n < ARCH_firstnode + ARCH_mynodes;
	   n++)
	YS__logmsg(n,
		   "Press CTRL-C once more to forcefully abort simulation\n");
    }

  if (ctrl_c_count++ < MAX_CTRL_C_COUNT)
    signal(SIGINT, inthandler);
  else
    exit(1);

  for (n = ARCH_cpus * ARCH_firstnode;
       n < ARCH_cpus * (ARCH_firstnode + ARCH_mynodes);
       n += ARCH_cpus)
    ExternalInterrupt(n, IPL_PFAIL);
}



/*=============================================================================
 * fpfailure()
 *   save detailed IEEE exception code in 'fpfailed', which may be used during
 *   instruction emulation to detect IEEE FP traps in simulated programs.
 */
extern "C" void fpfailure(int signo, siginfo_t *si, ucontext_t *context)
{
  switch (si->si_code)
    {
    case FPE_FLTDIV:
      fpfailed = FP754_TRAP_DIV0;
      break;
      
    case FPE_FLTOVF:
      fpfailed = FP754_TRAP_OVERFLOW;
      break;
      
    case FPE_FLTUND:
      fpfailed = FP754_TRAP_UNDERFLOW;
      break;
      
    case FPE_FLTRES:
      fpfailed = FP754_TRAP_INEXACT;
      break;
      
    case FPE_FLTINV:
      fpfailed = FP754_TRAP_INVALID;
      break;
    }

#ifdef sgi
  context->uc_mcontext.gregs[CTX_EPC] += 4;
#endif  

#ifdef linux
  context->uc_mcontext.fpregs->cw |= FPU_INT_MASK;
  FPU_CLEAR_EXCEPTIONS;

  // on some linux systems si_code is not set correctly
  // look at FPU status word instead
  if (context->uc_mcontext.fpregs->sw & _FPU_MASK_IM)
    fpfailed = FP754_TRAP_INVALID;
  if (context->uc_mcontext.fpregs->sw & _FPU_MASK_ZM)
    fpfailed = FP754_TRAP_DIV0;
  if (context->uc_mcontext.fpregs->sw & _FPU_MASK_OM)
    fpfailed = FP754_TRAP_OVERFLOW;
  if (context->uc_mcontext.fpregs->sw & _FPU_MASK_UM)
    fpfailed = FP754_TRAP_UNDERFLOW;
  if (context->uc_mcontext.fpregs->sw & _FPU_MASK_PM)
    fpfailed = FP754_TRAP_INEXACT;
#endif  
}



extern "C" void pipehandler(int signo, siginfo_t *si, void* context)
{
}

extern "C" void debughandler(int signo, siginfo_t *si, void* context)
{
  MemoryDumpAll();
}


/*=============================================================================
 * before_exit()
 *   Called before exiting; prints out an exit message and possibly mails
 *   a completion notification
 */

extern "C" void before_exit()
{
  extern char *mailto, *subject, *fnstdout, *fnstderr, *fnstat, *fnlog;
  int n;

  
  for (n = ARCH_firstnode;
       n < ARCH_firstnode + ARCH_mynodes;
       n++)
    YS__logmsg(n, "\nAbout to exit after time %.0f\n", YS__Simtime);
  
  if (mailto && fnstdout && fnstderr)
    {
      char sbuf[1024];
      if (!subject)
	subject = (char*)"L-RSIM_res";

      // mail a completion notification
#if 0
      char *out = tmpnam(NULL);
      FILE *fp = fopen((const char *) out, "w");
#endif
      char *out = (char*)"ml-rsim_mailXXXXXX";
      int outfd = mkstemp(out);
      FILE *fp  = fdopen(outfd, "w");

      struct utsname un;
      uname(&un);
      fprintf(fp, "Processor %s:\n", un.nodename);

      fprintf(fp, "RSIM %s has completed\n", subject);
      fprintf(fp, "Application output file: %s\n\n", fnstdout);
      fprintf(fp, "Simulator statistics: %s\n\n", fnstat);
      fprintf(fp, "Simulator log: %s\n\n", fnlog);
      fclose(fp);

      sprintf(sbuf, "mailx -s %s %s < %s", subject, mailto, out);
      system(sbuf);
      unlink(out);
    }
}



void SetSignalHandler()
{
  struct sigaction  sa;

  /* Set the function to be called before exit */
  atexit(before_exit);
  
  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))fpfailure;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGFPE, &sa, NULL);

#if defined(USESIGNAL)
  signal(SIGINT, inthandler);
  signal(SIGKILL, killhandler);
#else
  sigset(SIGINT, inthandler);
  sigset(SIGKILL, killhandler);
#endif

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))faulthandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGILL, &sa, NULL);

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))faulthandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGSEGV, &sa, NULL);

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))faulthandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGBUS, &sa, NULL);

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))faulthandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGTRAP, &sa, NULL);

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))pipehandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGPIPE, &sa, NULL);

  sa.sa_sigaction = (void (*)(int, siginfo_t*, void*))debughandler;
  sigfillset(&sa.sa_mask);
  sa.sa_flags   = SA_SIGINFO;
  sigaction(SIGUSR1, &sa, NULL);
}
