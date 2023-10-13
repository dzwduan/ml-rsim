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
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/utsname.h>

int getrusage(int who, struct rusage *rusage);

extern "C"
{
#include "sim_main/util.h"
#include "sim_main/simsys.h"
#include "sim_main/evlst.h"
#include "Caches/system.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"
#include "Bus/bus.h"

}


#include "Processor/multiprocessor.h"
#include "Processor/predecode.h"
#include "Processor/procstate.h"
#include "Processor/memunit.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"
#include "Processor/exec.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/procstate.hh"
#include "Processor/tagcvt.hh"
#include "Processor/memunit.hh"
#include "Processor/exec.hh"
#include "Processor/stallq.hh"
#include "Processor/active.hh"

#include "../../lamix/machine/intr.h"



/***********************************************************************/
/********** system configuration variable declarations *****************/
/***********************************************************************/

int MAX_ACTIVE_NUMBER        = 128; /* this means 64 instructions */
int MAX_ACTIVE_INSTS         = 64;
int MAX_SPEC                 = 8;
int FETCH_QUEUE_SIZE         = 8;
int FETCHES_PER_CYCLE        = 4;
int DECODES_PER_CYCLE        = 4;
int GRADUATES_PER_CYCLE      = 4;
int EXCEPT_FLUSHES_PER_CYCLE = 4;
int MAX_MEM_OPS              = DEFMAX_MEM_OPS; /* defined in state.h = 32 */
int MAX_ALU_OPS              = DEFMAX_ALU_OPS;
int MAX_FPU_OPS              = DEFMAX_FPU_OPS;
int MAX_STORE_BUF            = DEFMAX_STORE_BUF;
int ALU_UNITS                = 2;
int FPU_UNITS                = 2;
int ADDR_UNITS               = 1;
int MEM_UNITS                = L1D_DEFAULT_NUM_PORTS;

int partial_stats_time = 3600;

int  DEBUG_TIME = 0;
FILE *corefile;


/* Redirection file names */
char *mailto   = NULL;
char *subject  = NULL;
const char *configfile = "rsim_params";


int  statfile[MAX_NODES];
int  logfile[MAX_NODES];

char trace_dir[MAXPATHLEN];
char fnstdin[MAXPATHLEN], fnstdout[MAXPATHLEN], fnstderr[MAXPATHLEN];
char fnsimmount[MAXPATHLEN];
char fncwd[MAXPATHLEN];
char fnstat[MAXPATHLEN], fnlog[MAXPATHLEN];
char fnkernel[MAXPATHLEN];
char realsimpath[MAXPATHLEN];

long long reset_instruction = 0;     // when to reset statistics
long long stop_instruction  = 0;     // when to record stats. and abort sim.


int  StartStopInit();
int  WatchDogInit();
void GetRusage(int);
void PrintHelpInformation(char *);



/***********************************************************************/
/* UserMain    : the main procedure (in YACSIM programs); mainly sets  */
/*             : variables, parses the command line, and calls         */
/*             : initialization functions                              */
/***********************************************************************/

extern "C" void UserMain(int argc, char **argv, char **env)
{
  extern char    *optarg;
  extern int      optind;
  int             c1, done;
  char            c, unit;
  char            execname[MAXPATHLEN];
  char            simpath[MAXPATHLEN];
  char           *rsim_dirname = NULL;
  char            name[64];
  int             fd, i;
  char           *startdir;
  char          **sim_args, **sim_exec = NULL;
  int             mp = 1;
  int             k;
  struct utsname  s_uname;


  uname(&s_uname);
  
  if (argc == 1)
    {
      PrintHelpInformation(argv[0]);
      return;
    }

  fnstdin[0] = fnstdout[0] = fnstderr[0] = fnstat[0] = fnlog[0] = 0;

  
  /* Parse command line and initialize variables */

  done = 0;
  while ((!done) &&
	 ((c1 = getopt(argc,
		       argv,
		       "D:F:S:X"
		       "de:hm:nr:s:t:z:")) != -1))
    {
      c = c1;
      switch (c)
	{
	case 'D': // D and S are used to redirect all files
	  rsim_dirname = optarg;
	  break;

	case 'S':
	  subject = optarg;
	  break;

	case 'z': // redirect simin separately from stdin
	  configfile = optarg;
	  break;

	case 'e':
	  mailto = optarg;
	  break;

        case 'd': // debug/dump bus trace and/or waveform
	  BUS_TRACE_ENABLE = 1;
          break;

	case 'm':
	  mp = atoi(optarg);
	  if (mp == 0)
	    {
	      fprintf(stderr, "Illegal value %s for '-m' flag - defaulting to 1\n", optarg);
	      mp = 1;
	    }
	  break;
	  
	case 'n':
	  errno = 0;
	  if ((nice(19) == -1) && (errno != 0))
	    fprintf(stderr, "Cannot 'nice' simulation: %s\n",
		    YS__strerror(errno));
	  break;

	case 'r':
	  unit = ' ';
	  sscanf(optarg, "%lld%c", &reset_instruction, &unit);
	  if ((unit == 'm') || (unit == 'M'))
	    reset_instruction *= 1000000ll;
	  if ((unit == 'b') || (unit == 'B'))
	    reset_instruction *= 1000000000ll;
	  break;
	  
	case 's':
	  unit = ' ';
	  sscanf(optarg, "%lld%c", &stop_instruction, &unit);
	  if ((unit == 'm') || (unit == 'M'))
	    stop_instruction *= 1000000ll;
	  if ((unit == 'b') || (unit == 'B'))
	    stop_instruction *= 1000000000ll;
	  break;
	  
	case 't':
	  DEBUG_TIME = atoi(optarg);
	  break;

	case 'F': // executable to interpret
	  done = 1;
	  break;

	case 'X': // static scheduling -- supported only with RC,
//	  STALL_ON_FULL = 1;     // set in proc_config.h
	  MAX_ALU_OPS = 1;       // stall as soon as first one cann't issue */
	  MAX_FPU_OPS = 1;       // stall as soon as first one cann't issue */
#if 0
	  stat_sched = 1; */     // static scheduling is like STALL_ON_FULL,
	                         // except that address generation is also
	                         // counted with the ALU/FPU counter
#endif
	  break;

	case 'h':
	default:
	  PrintHelpInformation(argv[0]);
	  return;
	}
    }

  
  //-------------------------------------------------------------------------
  // find pointers to simulated executable name and arguments

  sim_args = argv + optind - 1;

  i = 0;
  while ((i < argc - optind + 1) && (sim_args[i][0] == '-'))
    i++;

  if (i < argc - optind + 1)
    sim_exec = sim_args + i;
  else
    {
      PrintHelpInformation(argv[0]);
      return;      
    }


  if ((*sim_exec)[0] == '/')
    strcpy(execname, *sim_exec);
  else
    {
      startdir = getcwd(NULL, MAXPATHLEN);
      sprintf(execname, "%s/%s", startdir, *sim_exec);
      free(startdir);
    }

  *sim_exec = execname;

  if (!subject)
    {
      subject = strrchr(execname, '/');
      subject++;
    }


  //-------------------------------------------------------------------------
  // attempt to find simulator executable directory and use it to derive
  // the kernel file path.
  // first try to remove the last three components of the name
  // (bin/$ARCH/rsim) and replace them with lamix/lamix
  // next, scan the search path for a component that points to a simulator
  // executable, and replace the last three components with usr/lib/lamix


  if (argv[0][0] == '/')
    strcpy(simpath, argv[0]);
  else
    {
      getcwd(simpath, sizeof(simpath));
      strcat(simpath, "/");
      strcat(simpath, argv[0]);
    }

  realpath(simpath, fnkernel);
  realpath(simpath, realsimpath);

  char *p;
  if (p = strrchr(fnkernel, '/'))
    *p = 0;
  else
    fnkernel[0] = 0;

  if (p = strrchr(fnkernel, '/'))
    *p = 0;
  else
    fnkernel[0] = 0;

  if (p = strrchr(fnkernel, '/'))
    *p = 0;
  else
    fnkernel[0] = 0;

  if (fnkernel[0] == 0)
    {
      char *exe;
      if (p = strrchr(argv[0], '/'))
	exe = p;
      else
	exe = argv[0];

      p = getenv("PATH");
      if (!p)
	fprintf(stderr, "Unable to obtain environment variable PATH");

      while (strtok(p, ":"))
	{
	  sprintf(fnkernel, "%s/%s", p, exe);
	  if (access(fnkernel, X_OK) == 0)
	    break;
	}

      if (p = strrchr(fnkernel, '/'))
	*p = 0;
      else
	fnkernel[0] = 0;

      if (p = strrchr(fnkernel, '/'))
	*p = 0;
      else
	fnkernel[0] = 0;

      if (p = strrchr(fnkernel, '/'))
	*p = 0;
      else
	fnkernel[0] = 0;
    }

  if (fnkernel[0] != 0)
    {
      strcpy(fnsimmount, fnkernel);
      strcat(fnsimmount, "/sim_mounts/sim_mount");
      strcat(fnkernel, "/lamix/lamix");
    }
  else
    {
      fprintf(stderr, "Can not find kernel image\n");
      exit(1);
    }

  if (getcwd(fncwd, sizeof(fncwd)) == NULL)
    {
      fprintf(stderr, "Can not determine current working directory\n");
      exit(1);
    }
    
		


  
  //-------------------------------------------------------------------------
  // read processor related configuration

  ParseConfigFile();

  
  //-------------------------------------------------------------------------
  // parameter cleanup and consistency checks
  
  MAX_ACTIVE_NUMBER *= 2;
  MAX_ACTIVE_INSTS = MAX_ACTIVE_NUMBER / 2;
  
  if (!simulate_ilp)
    { // if ILP simulation is turned off
      MAX_ACTIVE_NUMBER        = 2;    // -a1
      FETCHES_PER_CYCLE        = 1;    // -i1
      DECODES_PER_CYCLE        = 1;    // -i1
      GRADUATES_PER_CYCLE      = 1;    // -g1
      INSTANT_ADDRESS_GENERATE = 1;    // also give this the benefit of
                                       // instant address generation
      MAX_MEM_OPS              = 1;    // should show these effects
       
      ALU_UNITS = FPU_UNITS = ADDR_UNITS = 1;
      L1I_NUM_PORTS    = 1;
      L1I_TAG_PORTS[0] = 1;
      L1D_NUM_PORTS    = 1;
      L1D_TAG_PORTS[0] = 1;
      MEM_UNITS        = 1;
    }

  if (MAX_ACTIVE_NUMBER > MAX_MAX_ACTIVE_NUMBER)
    {
      fprintf(stderr,
	      "Too many active instructions; going to size %d",
	      MAX_MAX_ACTIVE_NUMBER / 2);
      MAX_ACTIVE_NUMBER = MAX_MAX_ACTIVE_NUMBER;
    }
  
  MAX_ACTIVE_INSTS = MAX_ACTIVE_NUMBER / 2;

  if (GRADUATES_PER_CYCLE == 0)
    GRADUATES_PER_CYCLE = MAX_ACTIVE_NUMBER;
  else if (GRADUATES_PER_CYCLE == -1)
    GRADUATES_PER_CYCLE = DECODES_PER_CYCLE;

  if (EXCEPT_FLUSHES_PER_CYCLE == -1)
    EXCEPT_FLUSHES_PER_CYCLE = GRADUATES_PER_CYCLE;


  
  //-------------------------------------------------------------------------
  // determine stdin/stdout/stderr filenames and log/stat filenames
  
  startdir = getcwd(NULL, MAXPATHLEN);

  if (rsim_dirname && rsim_dirname[0] == '/')
    {
      strcpy(fnstdout, rsim_dirname);
      rsim_dirname = NULL;
    }
  else
    {
      strcpy(fnstdout, startdir);
    }

  if (fnstdout[strlen(fnstdout)-1] != '/')
    strcat(fnstdout,  "/");

  if (rsim_dirname)
    {
      strcat(fnstdout, rsim_dirname);
      if (fnstdout[strlen(fnstdout)-1] != '/')
	strcat(fnstdout,  "/");
    }

  strcat(fnstdout,  subject);

  strcpy(trace_dir, fnstdout);
  strcpy(fnstat,    fnstdout);
  strcpy(fnlog,     fnstdout);
  strcpy(fnstderr,  fnstdout);
  strcpy(fnstdin,   fnstdout);

  strcat(fnstderr, ".stderr");
  strcat(fnstdin,  ".stdin");
  strcat(fnstdout, ".stdout");
  strcat(fnstat,   ".stat");
  strcat(fnlog,    ".log");

  if ((fd = open(fnstdin, O_RDONLY)) < 0)           // check if stdin exists
    if (errno == ENOENT)
      strcpy(fnstdin, "/dev/null");
  else
    close(fd);


  //-------------------------------------------------------------------------



  //-------------------------------------------------------------------------

  StartStopInit();
  WatchDogInit();
  
  DoMP(mp);

  
  //-------------------------------------------------------------------------
  // create a logfile for every node

  for (k = 0; k < ARCH_numnodes; k++)
    {
      logfile[k] = 0;
      statfile[k] = 0;
    }
  
  if (fnlog[0])
    {
      char *fn;

      fn = (char*)malloc(MAX(strlen(fnlog), strlen(fnstat)) + 4);
      if (fn == NULL)
	{
	  fprintf(stderr, "Malloc failed at %s:%i", __FILE__, __LINE__);
	  exit(1);
	}

      for (k = 0; k < ARCH_numnodes; k++)
	{
	  if (ARCH_numnodes == 1)
	    strcpy(fn, fnlog);
	  else if (ARCH_numnodes <= 10)
	    sprintf(fn, "%s%i", fnlog, k);
	  else
	    sprintf(fn, "%s%02i", fnlog, k);

	  logfile[k] = open(fn, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	  if (logfile[k] < 0)
	    fprintf(stderr,
		    "Opening logfile %s failed: %s\n", fn,
		    YS__strerror(errno));

	  if ((k < ARCH_firstnode) || (k >= ARCH_firstnode + ARCH_mynodes))
	    continue;
	  
	  if (ARCH_numnodes == 1)
	    strcpy(fn, fnstat);
	  else if (ARCH_numnodes <= 10)
	    sprintf(fn, "%s%i", fnstat, k);
	  else
	    sprintf(fn, "%s%02i", fnstat, k);
	  
	  statfile[k] = open(fn,
			     O_WRONLY | O_CREAT | O_TRUNC,
			     S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	  if (statfile[k] < 0)
	    fprintf(stderr,
		    "Opening statfile %s failed: %s\n", fn,
		    YS__strerror(errno));
	}

      free(fn);
    }
    


  //-------------------------------------------------------------------------

  for (k = ARCH_firstnode;
       k < ARCH_firstnode + ARCH_mynodes;
       k++)
    {
      int ac;
      YS__logmsg(k, "RSIM command line: ");
      for (ac = 0; ac < argc; ac++)
	YS__logmsg(k, "%s ", argv[ac]);
      YS__logmsg(k, "\n");

      YS__logmsg(k, "\nRunning simulation on %s (%s/%s)\n",
	      s_uname.nodename, s_uname.sysname, s_uname.machine);      

      YS__statmsg(k, "RSIM command line: ");
      for (ac = 0; ac < argc; ac++)
	YS__statmsg(k, "%s ", argv[ac]);
      YS__statmsg(k, "\n");

      YS__statmsg(k, "RSIM:\t Active list:      %3d\n", MAX_ACTIVE_INSTS);
      YS__statmsg(k, "\t Speculations:     %3d\n", MAX_SPEC);
      YS__statmsg(k, "\t Fetch rate:       %3d\n", FETCHES_PER_CYCLE);
      YS__statmsg(k, "\t Decode rate:      %3d\n", DECODES_PER_CYCLE);
      YS__statmsg(k, "\t Graduation rate:  %3d\n", GRADUATES_PER_CYCLE);
      if (STALL_ON_FULL)
	{
	  YS__statmsg(k, "\t ALU queue:        %3d\n", MAX_ALU_OPS);
	  YS__statmsg(k, "\t FPU queue:        %3d\n", MAX_FPU_OPS);
	  YS__statmsg(k, "\t Memory queue:     %3d\n", MAX_MEM_OPS);
	}
    }

  fflush(NULL);
  

  //-------------------------------------------------------------------------
  // Set signal handler, setup various tables

  SetSignalHandler();
  PredecodeTableSetup();
  UnitArraySetup();
  FuncTableSetup();

  
  //-------------------------------------------------------------------------
  // Initialize the system architecture

  SystemInit();

  AllProcs = RSIM_CALLOC(ProcState*, ARCH_numnodes * ARCH_cpus);
  if (!AllProcs)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);

  for (i = ARCH_cpus * ARCH_firstnode;
       i < ARCH_cpus * (ARCH_firstnode + ARCH_mynodes);
       i += ARCH_cpus)
    {
      AllProcs[i]  = new ProcState(i);
      if (startup(fnkernel, sim_args, env, AllProcs[i]) == -1)
	YS__errmsg(i / ARCH_cpus,
		   "Error loading kernel file %s",
		   fnkernel);

      for (int n = 1; n < ARCH_cpus; n++)
	{
	  AllProcs[i + n] = new ProcState(i + n);
	  AllProcs[i + n]->pc       = AllProcs[i]->pc;
	  AllProcs[i + n]->npc      = AllProcs[i]->npc;
	  AllProcs[i + n]->fetch_pc = AllProcs[i]->fetch_pc;
	  AllProcs[i + n]->kdata_segment_low = AllProcs[i]->kdata_segment_low;
	  AllProcs[i + n]->kdata_segment_high = AllProcs[i]->kdata_segment_high;
	  AllProcs[i + n]->ktext_segment_low = AllProcs[i]->ktext_segment_low;
	  AllProcs[i + n]->ktext_segment_high = AllProcs[i]->ktext_segment_high;
	}
    }


  //-------------------------------------------------------------------------
  // start simulation driver

  EVENT *rsim_event = NewEvent("RSIM Process - all processors",
			       RSIM_EVENT, NODELETE, 0);
  schedule_event(rsim_event, YS__Simtime + 0.5);

  // This is started at 0.5 to make sure that the smnet requests for this
  // time are handled _before_ the processor is handled. This is to avoid
  // spurious cases when an extra cycle is added in just for the processor
  // to see the smnet request

  /* start the YACSIM driver */
  double walltime = time(0);

  DriverRun();

#if defined(USESIGNAL)
  signal(SIGALRM, SIG_IGN);
#else
  sigset(SIGALRM, SIG_IGN);
#endif


  //-------------------------------------------------------------------------
  // simulation finished - clean up and print runtime
  
  chdir(startdir);
  free(startdir);

  walltime = time(0) - walltime;
  int wallhour = (int) (walltime / 3600);
  int wallmin  = (int) (walltime - wallhour * 3600)/60;
  int wallsec  = (int) walltime % 60;

  
  for (k = ARCH_firstnode;
       k < ARCH_firstnode + ARCH_mynodes;
       k++)
    {
      YS__statmsg(k,
		  "\n------------------------------------------------------------------------\n\n"); 
      YS__statmsg(k,
		  "\nSIMULATOR STATISTICS\n\n");
      YS__statmsg(k,
		  "Simulation Host                 : %s (%s/%s)\n",
	      s_uname.nodename, s_uname.sysname, s_uname.machine);
      YS__statmsg(k,
		  "Processors                      : %i\n\n",
		  total_processes);
      YS__statmsg(k,
		  "Elapsed cycles for simulation   : %.0f\n",
		  GetSimTime());
      YS__statmsg(k,
		  "Elapsed wall time of simulation : %d:%02d:%02d\n",
		  wallhour, wallmin, wallsec);
      YS__statmsg(k,
		  "Cycles / second                 : %.2f\n\n",
		  GetSimTime() / walltime);

      GetRusage(k);
  
      YS__statmsg(k, "\n\n");
    }

  
  return;
}




/*=========================================================================*/
/*                                                                         */
/* The main process event; gets called every cycle performs the main       */
/* processor functions. The main loop calls RSIM_EVENT for each            */
/* processor every cycle (cycle-by-cycle processor simulation stage).      */
/*                                                                         */
/*=========================================================================*/

extern "C" void RSIM_EVENT()
{
  int i;
  
  /*
   * Loop through each processor and advance simulation by a cycle
   */
  for (i = ARCH_cpus * ARCH_firstnode;
       i < ARCH_cpus * (ARCH_firstnode + ARCH_mynodes);
       i++)
    {
      /*
       * Handle requests in the pipelines of the L1 cache, L1 write buffer,
       * and L2 cache.
       */
      if (L1ICaches[i]->num_in_pipes)
	L1ICacheOutSim(i);

      if (L1DCaches[i]->num_in_pipes)
	L1DCacheOutSim(i);

      if (WBuffers[i]->num_in_pipes || WBuffers[i]->inqueue.size)
	L1DCacheWBufferSim(i);


      if (UBuffers[i]->num_entries)
	UBuffer_out_sim(i);

      if (L2Caches[i]->num_in_pipes)
	L2CacheOutSim(i);

      //---------------------------------------------------------------------

      ProcState *proc = AllProcs[i];
      if ((proc) && (!proc->halt))
	{
	  proc->curr_cycle = (long long) YS__Simtime;
#ifdef COREFILE
	  corefile = proc->corefile;
#endif
	  proc->DELAY--;
	  if (proc->DELAY <= 0 && !proc->exit)
	    {
	      if (proc->in_exception != NULL)
		{
		  proc->ComputeAvail();
		  PreExceptionHandler(proc->in_exception, proc);
		}

#ifdef COREFILE
	      if (proc->curr_cycle > DEBUG_TIME)
		fprintf(corefile, "Completion cycle %d \n", proc->curr_cycle);
#endif

	      /* Completion stage of the pipeline */
	      CompleteMemQueue(proc);
	      CompleteQueues(proc);

	      if (proc->in_exception == NULL && !proc->exit)
		/* Main processor pipeline */
		maindecode(proc);
	  
	      if (proc->exit)
		{
		  aliveprocs--;

		  DoExit();
		  
		  if (aliveprocs == 0)
		    /* otherwise, just keep running, since caches might still
		       need to service INVL requests, etc. */
		    {
		      EXIT = 1;
		      return;
		    }
		}

	      if (proc->DELAY <= 0)
		{
		  //	      if (proc->ReadyQueue_count > 0)
		  IssueQueues(proc);       /* Issue to queues */

#ifdef STORE_ORDERING
		  if (proc->MemQueue.NumItems() && proc->UnitsFree[uMEM])
		    IssueMem(proc);
#else
		  if ((proc->LoadQueue.NumItems() ||
		       proc->StoreQueue.NumItems()) &&
		      proc->UnitsFree[uMEM])
		    IssueMem(proc);
#endif	      
		  proc->DELAY = 1;
		}

#ifndef NOSTAT
	      StatrecUpdate(proc->SpecStats,
			    proc->branchq.NumItems(),
			    1);

	      for (int ctrfu = 0; ctrfu < numUTYPES; ctrfu++)
		StatrecUpdate(proc->FUUsage[ctrfu],
			      proc->MaxUnits[ctrfu]-proc->UnitsFree[ctrfu],
			      1);

#ifndef STORE_ORDERING
	      StatrecUpdate(proc->VSB, proc->StoresToMem, 1);
	      StatrecUpdate(proc->LoadQueueSize,
			    proc->LoadQueue.NumItems(), 1);
#else
	      StatrecUpdate(proc->MemQueueSize,
			    proc->MemQueue.NumItems(), 1);
#endif
	      StatrecUpdate(proc->FetchQueueStats,
				 proc->fetch_queue->NumItems(), 1);
	      StatrecUpdate(proc->ActiveListStats,
			    proc->active_list.NumElements(), 1);
#endif
	    }
	}

      
       /*
       * Handle requests coming into L1 cache and L2 cache
       */
      if (!(L1ICaches[i]->inq_empty))
	L1ICacheInSim(i);

      if (!(L1DCaches[i]->inq_empty))
	L1DCacheInSim(i);

      if (!(L2Caches[i]->inq_empty))
	L2CacheInSim(i);
    }

  
  //-------------------------------------------------------------------------
  // Schedule the main processorloop for next cycle

  schedule_event(YS__ActEvnt, YS__Simtime + 1.0);
}



/***********************************************************************/
/* Watchdog - detects stalled CPUs and suspends simulation.            */
/***********************************************************************/

#define WATCHDOG_PERIOD 10000.0 * 1000000.0 / CPU_CLK_PERIOD

long long *WatchDogCounts;


extern "C" void WatchDog()
{
  int n;
  EVENT *ev = (EVENT*)EventGetArg(NULL);
  
  if (EXIT)
    return;
  
  for (n = ARCH_cpus * ARCH_firstnode;
       n < ARCH_cpus * (ARCH_firstnode + ARCH_mynodes);
       n++)
    {
      if (WatchDogCounts[n] == AllProcs[n]->graduates)
        {
          YS__logmsg(n / ARCH_cpus,
                     "Watchdog: CPU %i node %i appears stalled\n",
                     n % ARCH_cpus, n / ARCH_cpus);
          YS__logmsg(n / ARCH_cpus,
                     "Suspending process\n");
          YS__logmsg(n / ARCH_cpus,
                     "Resume simulation by sending SIGCONT (%i) to process %i\n"
,
                     SIGCONT, getpid());
          kill(getpid(), SIGSTOP);
        }
      
      WatchDogCounts[n] = AllProcs[n]->graduates;
    }

  schedule_event(ev, YS__Simtime + WATCHDOG_PERIOD);
}


int WatchDogInit()
{
  EVENT *ev;
  int    n;

  WatchDogCounts = (long long*)malloc(sizeof(long long) *
                                      ARCH_numnodes * ARCH_cpus);
  if (WatchDogCounts == NULL)
    YS__errmsg(0, "Failed to allocated watchdog counts\n");

  for (n = 0; n < ARCH_numnodes * ARCH_cpus; n++)
    WatchDogCounts[n] = 0;

  ev = NewEvent("Processor Watchdog", WatchDog, NODELETE, 0);

  EventSetArg(ev, ev, sizeof(ev));
  schedule_event(ev, YS__Simtime + WATCHDOG_PERIOD);

  return(0);
}





/***********************************************************************/
/* Handle start/stop event: scheduled at the earliest time graduation  */
/* count may reach desired start or stop count. Find maximum           */
/* graduation count among all processors. If start/reset count is      */
/* given, check if reached and then reset all statistics, otherwise    */
/* reschedule for next earliest time. If stop count is given, check    */
/* if reached, then dump statistics and interrupt all processors,      */
/* otherwise reschedule.                                               */
/***********************************************************************/

extern "C" void StartStopHandle()
{
  int n;
  long long max_inst = 0;
  long long del;
  EVENT *ev = (EVENT*)EventGetArg(NULL);

  
  // simulation exiting, don't bother
  if (EXIT)
    return;
  
  // find highest graduated instruction count
  for (n = 0; n < ARCH_numnodes * ARCH_cpus; n++)
    if (AllProcs[n]->graduates > max_inst)
      max_inst = AllProcs[n]->graduates;


  // still waiting to reset statistics
  if (reset_instruction != 0)
    {
      // reset instruction reached: reset statistics
      if (max_inst >= reset_instruction)
	{
	  for (n = ARCH_firstnode;
	       n < ARCH_firstnode + ARCH_mynodes;
	       n++)
	    {
	      YS__logmsg(n,
			 "Reset instruction %lld reached - reset statistics\n",
			 reset_instruction);
	      StatClear(n);
	    }

	  reset_instruction = 0;
          max_inst = 0;
	}
      // start instruction not reached: reschedule
      else
	{
          del = (reset_instruction - max_inst) / GRADUATES_PER_CYCLE;
          if (del <= 0)
            del = 1;
	  schedule_event(ev, YS__Simtime + del);
#ifdef DEBUG
	  YS__logmsg(0,
		     "%.0f: Reset instr. not reached (%lld > %lld) - reschedule for %.0f\n",
		     YS__Simtime, reset_instruction, max_inst,
		     YS__Simtime + del);
#endif
	}
    }

  
  // waiting to reach stop instruction
  if ((reset_instruction == 0) && (stop_instruction != 0))
    {
      if (max_inst >= stop_instruction)
	{
	  for (n = ARCH_firstnode;
	       n < ARCH_firstnode + ARCH_mynodes;
	       n++)
	    {
	      YS__logmsg(n,
			 "Stop instruction %lld reached - write statistics\n",
			 stop_instruction);
	      StatReport(n);
	    }

	  for (n = ARCH_cpus * ARCH_firstnode;
	       n < ARCH_cpus * (ARCH_firstnode + ARCH_mynodes);
	       n += ARCH_cpus)
	    ExternalInterrupt(n, IPL_PFAIL);

	  stop_instruction = 0;
	}
      else
	{
          del = (stop_instruction - max_inst) / GRADUATES_PER_CYCLE;
          if (del <= 0)
            del = 1;
	  schedule_event(ev, YS__Simtime + del);
#ifdef DEBUG
	  YS__logmsg(0,
		     "%.0f: Stop instr. not reached (%lld > %lld) - reschedule for %.0f\n",
		     YS__Simtime, stop_instruction, max_inst,
		     YS__Simtime + del);
#endif
	}
    }
}



/***********************************************************************/
/* Initialize 'start/stop' feature. If start and/or stop instruction   */
/* is specified, create event and schedule accordingly.                */
/* Schedule time is estimated by assuming maximum graduation rate per  */
/* cycle and remaining instructions to go. When scheduled, handler     */
/* checks actual graduation count and reschedules if needed.           */
/***********************************************************************/

int StartStopInit()
{
  EVENT *ev;
  
  // skip if neither start nor stop is specified
  if ((reset_instruction == 0) && (stop_instruction == 0))
    return(0);

  // check that stop is past start, unless stop is not specified
  if ((reset_instruction >= stop_instruction) &&
      (stop_instruction != 0))
    {
      fprintf(stderr,
	      "Reset instruction count (%lld) greater than stop instruction count (%lld)\n",
	      reset_instruction, stop_instruction);
      exit(1);
    }


  ev = NewEvent("Start/Stop Statistics & Simulation",
		StartStopHandle, NODELETE, 0);

  EventSetArg(ev, ev, sizeof(ev));
  if (reset_instruction > 0)
    {
      schedule_event(ev, reset_instruction / GRADUATES_PER_CYCLE);
    }
  else
    {
      schedule_event(ev, stop_instruction / GRADUATES_PER_CYCLE);
    }
}




/***********************************************************************/

void GetRusage(int node)
{
  unsigned int  user_sec, user_min, user_hour;
  unsigned int  sys_sec, sys_min, sys_hour;
  unsigned int  total_sec, total_min, total_hour;
  int           width, wu, ws, n;
  struct rusage Rusage;


  getrusage(RUSAGE_SELF, &Rusage);
  user_sec = Rusage.ru_utime.tv_sec;
  sys_sec = Rusage.ru_stime.tv_sec;

  total_hour = (user_sec + sys_sec) / 3600;
  total_min  = ((user_sec + sys_sec) % 3600) / 60;
  total_sec  = (user_sec + sys_sec) % 60;
  user_hour  = user_sec / 3600;
  user_min   = (user_sec % 3600) / 60;
  user_sec   = user_sec % 60;
  sys_hour   = sys_sec / 3600;
  sys_min    = (sys_sec % 3600) / 60;
  sys_sec    = sys_sec % 60;

  width = total_hour > 0 ? (int)log10(double(total_hour)) : 0;
  ws    = sys_hour   > 0 ? (int)log10(double(sys_hour))   : 0;
  wu    = user_hour  > 0 ? (int)log10(double(user_hour))  : 0;

  YS__statmsg(node,
	      "Resource Usage\n");
  YS__statmsg(node,
	      "  total cpu time                : %d:%02d:%02d\n",
	      total_hour, total_min, total_sec);

  YS__statmsg(node,
	      "  system time                   : ");
  for (n = 0; n < width - ws; n++)
    YS__statmsg(node, " ");
  YS__statmsg(node,
	      "%d:%02d:%02d\n",
	      sys_hour, sys_min, sys_sec);

  YS__statmsg(node,
	      "  user time                     : ");
  for (n = 0; n < width - wu; n++)
    YS__statmsg(node, " ");
  YS__statmsg(node,
	      "%d:%02d:%02d\n",
	      user_hour, user_min, user_sec);

  YS__statmsg(node,
	      "  major page faults             : %d\n",
	      Rusage.ru_majflt);
  YS__statmsg(node,
	      "  minor page faults             : %d\n",
	      Rusage.ru_minflt);
  YS__statmsg(node,
	      "  voluntary context switches    : %d\n",
	      Rusage.ru_nvcsw);
  YS__statmsg(node,
	      "  involuntary context switches  : %d\n",
	      Rusage.ru_nivcsw);
}


/***********************************************************************/

void PrintHelpInformation(char *progname)
{
  printf("usage: %s [simulator-options] -F [application-options]\n\n", 
	   progname);

  puts("simulator options are:");
  puts("\t-d         - turn on bus trace");
  puts("\t-e eaddr   - Send an email notification to the specified");
  puts("\t             address upon completion of this simulation");
  puts("\t-m cpus    - parallel simulation on up to M processors");
  puts("\t-n         - lower simulator priority (nice)");
  puts("\t-r icount  - reset statistics when icount instructions are graduated");
  puts("\t-s icount  - write statistics and abort simulation when icount instructions are graduated");
  puts("\t-t time    - print debugging output after time T");
  puts("\t-z file    - Configuration file (default: rsim_params)");
  puts("\t-D dir     - Directory for output file");
  puts("\t-S subj    - Subject to use in output filenames");
  puts("\t-X         - Static scheduling (partially supported)");

  puts("\n[Application options] depend on specific applications");
  puts("For more information, please refer to the RSIM manual for a");
  puts("detailed description of the RSIM command line options.");
}
