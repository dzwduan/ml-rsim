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

// make sure this won't cause problems w/ linux!
#define _FILE_OFFSET_BITS 64

#ifdef linux
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <utime.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

#ifndef linux
#include <sys/ioccom.h>
#include <sys/systeminfo.h>
#include <sys/dirent.h>
#else
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/dirent.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/utsname.h>
typedef struct timespec timespec_t;
_syscall3(int, getdents, uint, fd, struct dirent *, buf, uint, size);
#endif

#include <sys/param.h>


extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "sim_main/util.h"
#include "sim_main/invoke_debugger.h"
}

#include "Processor/procstate.h"
#include "Processor/pagetable.h"
#include "Processor/userstat.h"
#include "Processor/exec.h"
#include "Processor/memunit.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/filedesc.h"
#include "Processor/tagcvt.hh"
#include "Processor/procstate.hh"
#include "Processor/active.hh"
#include "Processor/endian_swap.h"

#include "../lamix/interrupts/traps.h"
#include "../lamix/sys/userstat.h"
#include "../lamix/lib_src/hostent.h"

#ifndef _ST_FSTYPSZ
#  define _ST_FSTYPSZ 16
#endif

#ifndef _SOCKLEN_T
#define _SOCKLEN_T
typedef unsigned int socklen_t;
#endif

static void UserStatAllocHandler  (instance *, ProcState *);
static void UserStatSampleHandler (instance *, ProcState *);
static void WritelogHandler       (instance *, ProcState *);

static void DCreatHandler         (instance *, ProcState *);
static void DOpenHandler          (instance *, ProcState *);
static void PRead64Handler        (instance *, ProcState *);
static void PWrite64Handler       (instance *, ProcState *);
static void Lseek64Handler        (instance *, ProcState *);
static void ReadlinkHandler       (instance *, ProcState *);
static void CloseHandler          (instance *, ProcState *);
static void DUnlinkHandler        (instance *, ProcState *);
static void DAccessHandler        (instance *, ProcState *);
static void IoctlHandler          (instance *, ProcState *);
static void FcntlHandler          (instance *, ProcState *);
static void Fstat64Handler        (instance *, ProcState *);
static void DFStat64Handler       (instance *, ProcState *);
static void Lstat64Handler        (instance *, ProcState *);
static void Fstatvfs64Handler     (instance *, ProcState *);
static void ChmodHandler          (instance *, ProcState *);
static void FchmodHandler         (instance *, ProcState *);
static void ChownHandler          (instance *, ProcState *);
static void FchownHandler         (instance *, ProcState *);
static void LchownHandler         (instance *, ProcState *);
static void RenameHandler         (instance *, ProcState *);
static void UtimeHandler          (instance *, ProcState *);

static void DMkdirHandler         (instance *, ProcState *);
static void DRmdirHandler         (instance *, ProcState *);
static void Getdents64Handler     (instance *, ProcState *);

static void LinkHandler           (instance *, ProcState *);
static void SymlinkHandler        (instance *, ProcState *);

static void SocketHandler         (instance *, ProcState *);
static void BindHandler           (instance *, ProcState *);
static void ListenHandler         (instance *, ProcState *);
static void AcceptHandler         (instance *, ProcState *);
static void ConnectHandler        (instance *, ProcState *);
static void GetSockOptHandler     (instance *, ProcState *);
static void SetSockOptHandler     (instance *, ProcState *);
static void ShutdownHandler       (instance *, ProcState *);
static void CloseSocketHandler    (instance *, ProcState *);
static void GetSockNameHandler    (instance *, ProcState *);
static void GetPeerNameHandler    (instance *, ProcState *);
static void SendMsgHandler        (instance *, ProcState *);
static void RecvMsgHandler        (instance *, ProcState *);
static void GetHostByNameHandler  (instance *, ProcState *);
static void GetHostByAddrHandler  (instance *, ProcState *);

static void SysInfoHandler        (instance *, ProcState *);
static void PathConfHandler       (instance *, ProcState *);
static void GetUIDHandler         (instance *, ProcState *);
static void GetGIDHandler         (instance *, ProcState *);
static void GetgroupsHandler      (instance *, ProcState *);
static void GetStdioHandler       (instance *, ProcState *);

static void GetSegmentInfoHandler (instance *, ProcState *);
 
 
 
extern char fnstdin[MAXPATHLEN], fnstdout[MAXPATHLEN], fnstderr[MAXPATHLEN];
extern char fnsimmount[MAXPATHLEN], fncwd[MAXPATHLEN];

#define NET_MSG_DELAY 30       // wait N seconds before printing message
                               // that simulation is blocked on INET socket




/**************************************************************************/
/* SimTrapHandle : Handle special operating system and RSIM traps         */
/**************************************************************************/

int SimTrapHandle(instance *inst, ProcState *proc)
{
  int type, pr;
  char *phys_mem;
  ProcState *np;
  unsigned addr;
  
  // trap type is in %g1 - register 1
  // all other arguments are in %o0 - %o5
  type = proc->phy_int_reg_file[proc->intmapper[arch_to_log(proc,
							   proc->cwp,
								   1)]];
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,
            "traps.cc: In SysTrapHandler with code %d\n",
            type);
#endif

  if (type >= SIM_TRAP_GET_SEGMENT_INFO)
    {
      if (!PSTATE_GET_PRIV(proc->pstate))
        {
          inst->exception_code = PRIVILEGED;
          return(0);
        }
    }

 
  //=========================================================================
  switch (type)
    {
    case SIM_TRAP_EXIT:                                // exit
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
 
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Processor %d exiting with code %d\n",
		 proc->proc_id,
		 proc->phy_int_reg_file[pr]);
      proc->exit = proc->phy_int_reg_file[pr] ?
        (proc->phy_int_reg_file[pr] - 128) :
          1;
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_NEWPHASE:               // newphase
      proc->newphase(inst->rs1vali);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_ENDPHASE:               // endphase
      proc->endphase(proc->proc_id / ARCH_cpus);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_HALT:                  // fullstop -- force everyone to exit!
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
 
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Processor %d forcing halt with code %d\n",
		 proc->proc_id,
		 proc->phy_int_reg_file[pr]);
      exit(1);     // abort all processors immediately, without graceful exit
 
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_RSIM_OFF:              // RSIM OFF
      proc->decode_rate = 1;
      proc->graduate_rate = 1;
      proc->max_active_list = 2;
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_RSIM_ON:               // RSIM ON
      proc->decode_rate = DECODES_PER_CYCLE;
      proc->graduate_rate = GRADUATES_PER_CYCLE;
      proc->max_active_list = MAX_ACTIVE_NUMBER;
      break;
 
      
      //=====================================================================

    case SIM_TRAP_CLEAR_STAT:            // clear stats
      StatClear(proc->proc_id / ARCH_cpus);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_REPORT_STAT:           // report stats
      StatReport(proc->proc_id / ARCH_cpus);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_USERSTAT_ALLOC:
      UserStatAllocHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_USERSTAT_SAMPLE:
      UserStatSampleHandler(inst, proc);
      break;

      
    case SIM_TRAP_LOG_SIMTIME:
      YS__logmsg(proc->proc_id / ARCH_cpus, "Current simtime: %.0f\n",
		 YS__Simtime);
      break;


    case SIM_TRAP_INVOKE_DEBUGGER:
      fprintf(stderr, "Hit INVOKE_DEBUGGER trap at simtime: %.0f\n",
	      YS__Simtime); 
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Hit INVOKE_DEBUGGER trap at simtime: %.0f\n",
		 YS__Simtime); 
#ifdef DEBUG_PROC
      invoke_debugger(realsimpath);
#else
      fprintf(stderr, "This trap can only be used with the debugging version"
	      " of mlrsim\n");
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "This trap can only be used with the debugging version"
		 "of mlrsim\n");
#endif
      break;


      //=====================================================================
      
      //---------------------------------------------------------------------
    case SIM_TRAP_GET_SEGMENT_INFO:         // get segment start/end address
      GetSegmentInfoHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_ALLOC_PAGE:               // allocate physical memory
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
      addr = (unsigned)proc->phy_int_reg_file[pr];

      phys_mem = proc->PageTable->lookup(addr);
      if (!phys_mem)
	{
#ifdef TRACE
	  if (YS__Simtime > TRACE)
	    YS__logmsg(proc->proc_id / ARCH_cpus,
		       "%.0f: Installing Mapping: %08X\n",
		       YS__Simtime, addr);
#endif
          phys_mem = (char*)malloc(PAGE_SIZE);
	  if (phys_mem == NULL)
	    YS__errmsg(proc->proc_id / ARCH_cpus,
		       "SimAllocPage: Malloc failed: %s\n",
		       YS__strerror(errno));
	  memset(phys_mem, 0, PAGE_SIZE);
	  proc->PageTable->insert(addr, phys_mem);
        }
#ifdef TRACE
      else
	if (YS__Simtime > TRACE)
	  YS__logmsg(proc->proc_id / ARCH_cpus,
		     "Installing Mapping: %08X - exists!\n",
		     addr);
#endif
      break;
      
 
      //=====================================================================
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_WRITELOG:              // write simulator log
      WritelogHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_DCREAT:                // creat system call
      DCreatHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_DOPEN:                 // open system call
      DOpenHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_PREAD64:               // pread system call
      PRead64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_PWRITE64:              // pwrite system call
      PWrite64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_LSEEK64:               // lseek system call
      Lseek64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_READLINK:              // readlink system call
      ReadlinkHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_CLOSE:                 // close system call
      CloseHandler(inst, proc);
      break;
 

      //---------------------------------------------------------------------
    case SIM_TRAP_DUNLINK:               // unlink system call
      DUnlinkHandler(inst, proc);
      break;
 

      //---------------------------------------------------------------------
    case SIM_TRAP_DACCESS:               // access system call
      DAccessHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_IOCTL:                 // ioctl system call
      IoctlHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_FCNTL:                 // fcntl system call
      FcntlHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_FSTAT64:               // fstat system call
      Fstat64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_DFSTAT64:              // stat system call
      DFStat64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_LSTAT64:               // lstat system call
      Lstat64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_CHMOD:                 // chmod system call 
      ChmodHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_FCHMOD:                // fchmod system call
      FchmodHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_CHOWN:                 // chown system call 
      ChownHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_FCHOWN:                // fchown system call
      FchownHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_LCHOWN:                // lchown system call 
      LchownHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_RENAME:                // rename system call 
      RenameHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_UTIME:                 // utime system call 
      UtimeHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_DMKDIR:                // mkdir system call
      DMkdirHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_DRMDIR:                // rmdir system call
      DRmdirHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETDENTS64:            // getdents system call
      Getdents64Handler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_LINK:                  // link system call
      LinkHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_SYMLINK:               // symlink system call
      SymlinkHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_FSTATVFS64:            // fstatvfs system call
      Fstatvfs64Handler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETUID:                // getuid system call
      GetUIDHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETGID:                // getgid system call
      GetGIDHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETGROUPS:             // getgroups system call
      GetgroupsHandler(inst, proc);
      break;
 
      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETSTDIO:              // getstdio system call
      GetStdioHandler(inst, proc);
      break;
 
      
      
      //=====================================================================

    case SIM_TRAP_SOCKET:
      SocketHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_BIND:
      BindHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_LISTEN:
      ListenHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_ACCEPT:
      AcceptHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_CONNECT:
      ConnectHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_GETSOCKOPT:
      GetSockOptHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_SETSOCKOPT:
      SetSockOptHandler(inst, proc);
      break;

      
      //---------------------------------------------------------------------
    case SIM_TRAP_SHUTDOWN:
      ShutdownHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_CLOSESOCKET:
      CloseSocketHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_GETSOCKNAME:
      GetSockNameHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_GETPEERNAME:
      GetPeerNameHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_SENDMSG:
      SendMsgHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_RECVMSG:
      RecvMsgHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_GETHOSTBYNAME:
      GetHostByNameHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_GETHOSTBYADDR:
      GetHostByAddrHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_SYSINFO:
      SysInfoHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    case SIM_TRAP_PATHCONF:
      PathConfHandler(inst, proc);
      break;


      //---------------------------------------------------------------------
    default:
      return(0);
    }
 
  return(1);
}
 
 
 
/***************************************************************************/
 
static void UserStatAllocHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile, "traps.cc: In UserStatAlloc Handler\n");
#endif

  int   pr;
  char  buffer[MAX_USERSTAT_NAME];
  int   type;
  int   return_value;
  int   lreturn_register;
  int   return_register;
  char *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  type = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < sizeof(buffer)-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (buffer[i] == 0)
	break;
      inst->addr++;
    }
  

  return_value = UserStats[proc->proc_id / ARCH_cpus]->alloc(type, buffer);

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}



/***************************************************************************/


static void UserStatSampleHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile, "traps.cc: In UserStatSample Handler\n");
#endif

  int   pr;
  int   id;
  int   val;
 
  pr  = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  id  = proc->phy_int_reg_file[pr];
 
  pr  = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  val = proc->phy_int_reg_file[pr];
 
  UserStats[proc->proc_id / ARCH_cpus]->sample(id, val);
}




/***************************************************************************/
/* WritelogHandler : Simulator exception for writing log file              */
/***************************************************************************/

static void WritelogHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Writelog Handler\n");
#endif

  int   number_of_items;
  char *buffer;
  int   pr, fd;
  int   count = 0;
  int   lreturn_register;
  int   return_register;
  int   retval, done;

  fd = logfile[proc->proc_id / ARCH_cpus];
  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  number_of_items = proc->phy_int_reg_file[pr];
 
  int st = inst->addr & (PAGE_SIZE - 1);

  done = 0;
  if (number_of_items > PAGE_SIZE - st)
    {
      buffer = GetMap(inst, proc);
      while ((retval = write(fd, buffer, PAGE_SIZE-st)) < 0)
	{
	  printf("Write to file failed (%s)",
		 YS__strerror(errno));
	  printf("  Retry in %i secs ...\n",
		 FILEOP_RETRY_PERIOD);
	  fflush(stdout);
	  sleep(FILEOP_RETRY_PERIOD);
	}

      count += retval;
      inst->addr += retval;
      number_of_items -= retval;
 
      if (retval != PAGE_SIZE-st)
        done = 1;

      while (number_of_items >= PAGE_SIZE)
        {
          buffer = GetMap(inst, proc);

	  while ((retval = write(fd, buffer, PAGE_SIZE)) < 0)
	    {
	      printf("Write to file failed (%s)",
		     YS__strerror(errno));
	      printf("  Retry in %i secs ...\n",
		     FILEOP_RETRY_PERIOD);
	      fflush(stdout);
	      sleep(FILEOP_RETRY_PERIOD);
	    }
	  
          if (retval < 0)             /* some sort of error other than EOF */
            count = retval;
          else
            {
              count += retval;
              number_of_items -= retval;
              inst->addr += retval;
            }
          
          if (retval != PAGE_SIZE)
            {
              done = 1;
              break;
            }
        }
    }

  buffer = GetMap(inst, proc);
  if (!done)
    {
      while ((retval = write(fd, buffer, number_of_items)) < 0)
	{
	  printf("Write to file failed (%s)",
		 YS__strerror(errno));
	  printf("  Retry in %i secs ...\n",
		 FILEOP_RETRY_PERIOD);
	  fflush(stdout);
	  sleep(FILEOP_RETRY_PERIOD);
	}

      if (retval < 0)                 /* some sort of error other than EOF */
        count = retval;
      else
        count += retval;
    }
 
  if (count < 0)
    count = -1 * errno;
  fflush(NULL);

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = count;
}
 
 
 


/***************************************************************************/


static void DCreatHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Creat Handler\n");
#endif
 
  int  pr;
  char buffer[PATH_MAX]; 
  int  mode, fd;
  int  return_value;
  int  return_register;
  int  lreturn_register;
  char *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  mode = proc->phy_int_reg_file[pr];

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = FD_create(buffer, O_WRONLY | O_CREAT | O_TRUNC, mode);
  
  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/

static void DOpenHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Open Handler\n");
#endif

  int  pr;
  char buffer[PATH_MAX]; 
  int  oflag;
  int  mode, fd;
  int  return_value;
  int  lreturn_register;
  int  return_register;
  char *pa;
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  oflag = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  mode = proc->phy_int_reg_file[pr];

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = FD_create(buffer, oflag, mode);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/
/* PReadHandler : Simulator exception routine that handles read            */
/***************************************************************************/
 
static void PRead64Handler(instance *inst, ProcState *proc)
{
 
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In PRead Handler\n");
#endif
  
  int       fd, number_of_items;
  char     *buffer;
  int       pr;
  int       count = 0;
  int       lreturn_register;
  int       return_register;
  int       retval, done, v0, v1;
  long long offset;
  /* read the parameters */
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  number_of_items = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  v0 = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 12)];
  v1 = proc->phy_int_reg_file[pr];

  offset = (long long)v0 << 32 | (long long)v1;

  /* do one character at a time - can be optimized */

  int st = inst->addr & (PAGE_SIZE - 1);

  done = 0;
  fd = FD_open(fd);
  if (fd < 0)
    {
      count = -1;
      done = 1;
    }
  
  if ((done == 0) && (number_of_items > PAGE_SIZE - st))
    {
      buffer = GetMap(inst, proc);
      retval = (int)pread64(fd, buffer, PAGE_SIZE-st, offset);
      if (retval < 0)
	count = retval;
      else
	{
	  count += retval;
	  number_of_items -= retval;
	  inst->addr += retval;
	  offset += retval;
	}
 
      if (retval != PAGE_SIZE - st)
        done = 1;
      
      while (!done && (number_of_items >= PAGE_SIZE))
        {
          buffer = GetMap(inst, proc);
          retval = (int)pread64(fd, buffer, PAGE_SIZE, offset);
	  
          if (retval < 0)             /* some sort of error other than EOF */
            count = retval;
          else
            {
              count += retval;
              number_of_items -= retval;
              inst->addr += retval;
	      offset += retval;
            }

          if (retval != PAGE_SIZE)
            {
              done = 1;
              break;
            }
        }
    }
      
  buffer = GetMap(inst, proc);
 
  if (!done)
    {
      retval = (int)pread64(fd, buffer, number_of_items, offset);
      if (retval < 0)                 /* some sort of error other than EOF */
        count = retval;
      else
        count += retval;
    }

  close(fd);

  if (count < 0)
    count = -1 * errno;

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = count;
}
 
 
 
/***************************************************************************/
/* PWriteHandler : Simulator exception routine that handles write          */
/***************************************************************************/

static void PWrite64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In PWrite Handler\n");
#endif

  int       fd, number_of_items;
  char     *buffer;
  int       pr;
  int       count = 0;
  int       lreturn_register;
  int       return_register;
  int       retval, done, v0, v1;
  long long offset;

  /*read the parameters */
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  number_of_items = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  v0 = proc->phy_int_reg_file[pr];
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 12)];
  v1 = proc->phy_int_reg_file[pr];

  offset = (long long)v0 << 32 | (long long)v1;
  
  int st = inst->addr & (PAGE_SIZE - 1);

  done = 0;
  fd = FD_open(fd);
  if (fd < 0)
    {
      count = -1;
      done = 1;
    }
  
  if ((done == 0) && (number_of_items > PAGE_SIZE - st))
    {
      buffer = GetMap(inst, proc);

      retval = (int)pwrite64(fd, buffer, PAGE_SIZE-st, offset);
      count += retval;
      inst->addr += retval;
      number_of_items -= retval;
      offset += retval;
 
      if (retval != PAGE_SIZE-st)
        done = 1;

      while (number_of_items >= PAGE_SIZE)
        {
          buffer = GetMap(inst, proc);

          retval = (int)pwrite64(fd, buffer, PAGE_SIZE, offset);
          if (retval < 0)             /* some sort of error other than EOF */
            count = retval;
          else
            {
              count += retval;
              number_of_items -= retval;
              inst->addr += retval;
	      offset += retval;
            }
          
          if (retval != PAGE_SIZE)
            {
              done = 1;
              break;
            }
        }
    }
      
  buffer = GetMap(inst, proc);
  
  if (!done)
    {
      retval = (int)pwrite64(fd, buffer, number_of_items, offset);

      if (retval < 0)                 /* some sort of error other than EOF */
        count = retval;
      else
        count += retval;
    }
 
  if (count < 0)
    count = -1 * errno;
  fflush(NULL);
  close(fd);

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = count;
}
 
 



/***************************************************************************/
/* LseekHandler : Simulator exception routine that handles lseek           */
/***************************************************************************/
 
static void Lseek64Handler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Lseek Handler\n");
#endif
  
  int       fd, whence, v0, v1;
  long long position;
  int       lreturn_register0, lreturn_register1;
  int       return_register0, return_register1;
  long long return_value;
  int       pr;
  
  /* read parameters */
  lreturn_register0 = arch_to_log(proc, proc->cwp, 8);
  lreturn_register1 = arch_to_log(proc, proc->cwp, 9);
  return_register0 = proc->intmapper[lreturn_register0];
  return_register1 = proc->intmapper[lreturn_register1];
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  v0 = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  v1 = proc->phy_int_reg_file[pr];

  position = (long long)v0 << 32 | (long long)v1;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  whence = proc->phy_int_reg_file[pr];

  return_value = FD_lseek(fd, position, whence);

  if (return_value < 0)
    {
      proc->phy_int_reg_file[return_register0] =
	proc->log_int_reg_file[lreturn_register0] = -errno;
      proc->phy_int_reg_file[return_register1] =
	proc->log_int_reg_file[lreturn_register1] = 0;
    }
  else
    {
      proc->phy_int_reg_file[return_register0] =
	proc->log_int_reg_file[lreturn_register0] = return_value >> 32;
      proc->phy_int_reg_file[return_register1] =
	proc->log_int_reg_file[lreturn_register1] = return_value;
    }
}
 
 
 
 
/***************************************************************************/
/* ReadlinkHandler : Simulator exception routine that handles readlink     */
/***************************************************************************/
 
static void ReadlinkHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In ReadLink Handler\n");
#endif
 
  int   fd, length;
  int   lreturn_register;
  int   return_register;
  int   return_value;
  int   pr, i;
  char  path[PATH_MAX], *buf, *pa;
 
  /* read parameters */
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
   
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
  memset(path, 0, sizeof(path));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      path[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  length = proc->phy_int_reg_file[pr];

  buf = (char*)malloc(length);
  if (buf == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "SimReadLink: Malloc failed: %s\n",
	       YS__strerror(errno));
  memset(buf, 0, length);

  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = readlink(path, buf, length);

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  inst->addr = proc->phy_int_reg_file[pr];

  for (i = 0; i < length; i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = buf[i];
      inst->addr++;
    }
  free(buf);
  
  if (return_value < 0)
    return_value = errno * -1;
  
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;
}


/***************************************************************************/
/* CloseHandler : Simulator exception routine that handles close           */
/***************************************************************************/

static void CloseHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Close Handler\n");
#endif
  
  int fd;
  int lr;
  int pr;
  int return_value;
 
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  fd = proc->phy_int_reg_file[pr];

  return_value = FD_remove(fd);
  if (return_value < 0)
    return_value = errno * -1;

  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}
 
 
/***************************************************************************/


static void DUnlinkHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Unlink Handler\n");
#endif

  int  pr, fd;
  char buffer[PATH_MAX]; 
  int  return_value;
  int  lreturn_register;
  int  return_register;
  char *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = unlink(buffer);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}
 
 
/***************************************************************************/


static void DAccessHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Access Handler\n");
#endif
 
  int  pr;
  char buffer[PATH_MAX]; 
  int  mode, fd;
  int  return_value;
  int  lreturn_register;
  int  return_register;
  char *pa;
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  mode = proc->phy_int_reg_file[pr];

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = access(buffer, mode);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}
 
 
/***************************************************************************/


static void IoctlHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Ioctl Handler\n");
#endif
 
  int   pr;
  int   fd, request, arg, size;
  int   return_value = -1;
  int   lreturn_register;
  int   return_register;
  char *buf = NULL, *pa;
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  request = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  arg = proc->phy_int_reg_file[pr];

  size = (request >> 16) & 0xFF;

  if (request & IOC_INOUT)
    {
      buf = (char*)malloc(size);
      if (buf == NULL)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimIoctl: Malloc failed: %s\n",
		   YS__strerror(errno));
      memset(buf, 0, size);
    }

  if (request & IOC_IN)      // copy third argument
    {
      inst->addr = arg;
      for (int i = 0; i < size; i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  buf[i] = *pa;
	  inst->addr++;
	}
    }

  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = ioctl(fd, request, buf);
      close(fd);
    }

  if (request & IOC_OUT)      // copy result from buffer
    {
      inst->addr = arg;
      for (int i = 0; i < size; i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *pa = buf[i];
	  inst->addr++;
	}
    }


  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;

  if (buf != NULL)
    free(buf);
}

/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*                                                                          */
/*                  We haven't finished fcntl for linux yet!                */
/*                                                                          */
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
/*!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/

/***************************************************************************/

typedef struct solaris_flock
{
  short         l_type;
  short         l_whence;
  unsigned long l_start;
  unsigned long l_len;
  long          l_sysid;
  unsigned long l_pid;
} solaris_flock_t;


typedef struct solaris_flock64
{
  short         l_type;
  short         l_whence;
  long long     l_start;
  long long     l_len;
  long          l_sysid;
  unsigned long l_pid;
} solaris_flock64_t;

static void fcntl_panic(int proc_id, int request)
{
  YS__errmsg(proc_id / ARCH_cpus,
		   "SimFcntl: unknown request: %d\n",
		   request);
}

#define SOL_F_DUPFD		0
#define SOL_F_GETFD		1
#define SOL_F_SETFD		2
#define SOL_F_GETFL		3
#define SOL_F_SETFL		4
#define SOL_F_SETLK		6
#define SOL_F_SETLKW		7
#define SOL_F_CHKFL		8
#define SOL_F_DUP2FD		9
#define SOL_F_ALLOCSP		10
#define SOL_F_FREESP		11
#define SOL_F_ISSTREAM		13
#define SOL_F_GETLK		14
#define SOL_F_PRIV		15
#define SOL_F_NPRIV		16
#define SOL_F_QUOTACTL		17
#define SOL_F_BLOCKS		18
#define SOL_F_BLKSIZE		19
#define SOL_F_RSETLK		20
#define SOL_F_RGETLK		21
#define SOL_F_RSETLKW		22
#define SOL_F_GETOWN		23
#define SOL_F_SETOWN		24
#define SOL_F_REVOKE		25
#define SOL_F_HASREMOTELOCKS	26
#define SOL_F_FREESP64		27
#define SOL_F_GETLK64		33
#define SOL_F_SETLK64		34
#define SOL_F_SETLKW64		35
#define SOL_F_SHARE		40
#define SOL_F_UNSHARE		41

static void FcntlHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Fcntl Handler\n");
#endif
 
  int                    pr;
  int                    fd, request, arg;
  int                    return_value = -1;
  int                    lreturn_register;
  int                    return_register;
  char                  *buf = NULL, *pa;
  struct solaris_flock   sfl;
  struct solaris_flock64 sfl64;
  long long		 l_start;
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  request = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  arg = proc->phy_int_reg_file[pr];

  // case request -> host_request
  //              -> function
  //              -> copy out?
  //              -> 64 vs. 32 bit flock
  //              -> other structures

  // It is believed that the only request fcntl is used for anymore is freesp!
  // For now, let's only handle that case.  If it turns out not to be true, we
  // can go back and deal w/ it.
  if (request != SOL_F_FREESP && request != SOL_F_FREESP64)
    fcntl_panic(proc->proc_id, request);

  if (request == SOL_F_FREESP)
    {  
      buf = (char*)malloc(sizeof(struct solaris_flock));
      if (buf == NULL)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: Malloc Failed: %s\n",
		   YS__strerror(errno));
      memset(buf, 0, sizeof(struct solaris_flock));

      inst->addr = arg;
      for (int i = 0; i < sizeof(struct solaris_flock); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  buf[i] = *pa;
	  inst->addr++;
	}

      if (endian_swap(((struct solaris_flock*)buf)->l_whence) != SEEK_SET)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: F_FREESP: l_whence != SEEK_SET!\n");

      if (endian_swap(((struct solaris_flock*)buf)->l_len) != 0)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: F_FREESP: l_len != 0!\n");

      l_start = endian_swap(((struct solaris_flock*)buf)->l_start);
    }

  if (request == SOL_F_FREESP64)
    {
      buf = (char*)malloc(sizeof(struct solaris_flock64));
      if (buf == NULL)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: Malloc Failed: %s\n",
		   YS__strerror(errno));
      memset(buf, 0, sizeof(struct solaris_flock64));

      inst->addr = arg;
      for (int i = 0; i < sizeof(struct solaris_flock64); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  buf[i] = *pa;
	  inst->addr++;
	}

      if (endian_swap(((struct solaris_flock64*)buf)->l_whence) != SEEK_SET)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: F_FREESP: l_whence != SEEK_SET!\n");

      if (endian_swap(((struct solaris_flock64*)buf)->l_len) != 0)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "SimFcntl: F_FREESP: l_len != 0!\n");

      l_start = endian_swap(((struct solaris_flock64*)buf)->l_start);
    }


  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = ftruncate(fd, l_start);
      close(fd);
    }

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;

  if (buf)
    free(buf);
}




/***************************************************************************/

// Operating systems other than Solaris may have a different layout of the
// stat structure. Thus, we copy only the elements needed for the Solaris
// version from the native structure. Reference native elements by name,
// simulated structure (RSIM/Solaris) by offset/length

// Solaris 2.5 definition of the user stat structure

struct solaris_stat64
{
  unsigned long         st_dev;                  // 32 bit
  long                  st_pad1[3];              // 3 x 32 bit
  unsigned long long    st_ino;                  // 64 bit
  mode_t                st_mode;                 // 32 bit
  nlink_t               st_nlink;                // 32 bit
  uid_t                 st_uid;                  // 32 bit
  gid_t                 st_gid;                  // 32 bit
  unsigned long         st_rdev;                 // 32 bit
  long                  st_pad2[2];              // 2 x 32 bit
#ifdef PAD_STAT64
  long                  st_pad3;                 // 32 bit
#endif
  long long             st_size;                 // 64 bit
  timespec_t            st_atim;                 // 2 x 32 bit
  timespec_t            st_mtim;                 // 2 x 32 bit
  timespec_t            st_ctim;                 // 2 x 32 bit
  long                  st_blksize;              // 32 bit
#ifdef PAD_STAT64
  long                  st_pad5;                 // 32 bit
#endif
  long long             st_blocks;               // 64 bit
  char                  st_fstype[_ST_FSTYPSZ];  // 16 x 8 bit
  long                  st_pad4[8];              // 8 x 32 bit
};


static void Fstat64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In FStat Handler\n");
#endif
 
  int                    pr;
  int                    fd, i;
  struct stat64          sbuf;
  struct solaris_stat64  solbuf;
  int                    return_value = -1;
  int                    lreturn_register;
  int                    return_register;
  char                  *pa;
  char                   buf[sizeof(struct solaris_stat64)];
  unsigned               base;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  base = proc->phy_int_reg_file[pr];

  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = fstat64(fd, &sbuf);
      close(fd);
    }
  if (return_value < 0)
    return_value = errno * -1;

  memset(&solbuf, 0, sizeof(solbuf));
  solbuf.st_dev          = endian_swap((unsigned int)sbuf.st_dev);
  solbuf.st_ino          = endian_swap((long long)sbuf.st_ino);
  solbuf.st_mode         = endian_swap(sbuf.st_mode);
  solbuf.st_nlink        = endian_swap(sbuf.st_nlink);
  solbuf.st_uid          = endian_swap(sbuf.st_uid);
  solbuf.st_gid          = endian_swap(sbuf.st_gid);
  solbuf.st_rdev         = endian_swap((unsigned int)sbuf.st_rdev);
  solbuf.st_size         = endian_swap(sbuf.st_size);
#ifdef linux
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atime);
  solbuf.st_atim.tv_nsec = 0;
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtime);
  solbuf.st_mtim.tv_nsec = 0;
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctime);
  solbuf.st_ctim.tv_nsec = 0;
#else
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atim.tv_sec);
  solbuf.st_atim.tv_nsec = endian_swap(sbuf.st_atim.tv_nsec);
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtim.tv_sec);
  solbuf.st_mtim.tv_nsec = endian_swap(sbuf.st_mtim.tv_nsec);
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctim.tv_sec);
  solbuf.st_ctim.tv_nsec = endian_swap(sbuf.st_ctim.tv_nsec);
#endif
  solbuf.st_blksize      = endian_swap(sbuf.st_blksize);
  solbuf.st_blocks       = endian_swap(sbuf.st_blocks);
#ifndef linux
  memcpy(solbuf.st_fstype, sbuf.st_fstype, sizeof(sbuf.st_fstype));
#else
  solbuf.st_fstype[0] = '\0';
#endif

  memcpy(buf, &solbuf, sizeof(solbuf));

  inst->addr = base;
  for (i = 0; i < sizeof(struct solaris_stat64); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = buf[i];
      inst->addr++;
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/


static void DFStat64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Stat Handler\n");
#endif
 
  int                    pr, i, fd;
  char                   buffer[PATH_MAX]; 
  struct stat64          sbuf;
  struct solaris_stat64  solbuf;
  int                    return_value = -1;
  int                    lreturn_register;
  int                    return_register;
  char                  *pa;
  char                   buf[sizeof(struct solaris_stat64)];
  unsigned               base;
 
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  base = proc->phy_int_reg_file[pr];
  inst->addr = base;

  memset(buffer, 0, sizeof(buffer));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  base = proc->phy_int_reg_file[pr];

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = lstat64(buffer, &sbuf);
  if (return_value < 0)
    return_value = errno * -1;

  memset(&solbuf, 0, sizeof(solbuf));
  solbuf.st_dev          = endian_swap((unsigned int)sbuf.st_dev);
  solbuf.st_ino          = endian_swap((long long)sbuf.st_ino);
  solbuf.st_mode         = endian_swap(sbuf.st_mode);
  solbuf.st_nlink        = endian_swap(sbuf.st_nlink);
  solbuf.st_uid          = endian_swap(sbuf.st_uid);
  solbuf.st_gid          = endian_swap(sbuf.st_gid);
  solbuf.st_rdev         = endian_swap((unsigned int)sbuf.st_rdev);
  solbuf.st_size         = endian_swap(sbuf.st_size);
#ifdef linux
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atime);
  solbuf.st_atim.tv_nsec = 0;
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtime);
  solbuf.st_mtim.tv_nsec = 0;
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctime);
  solbuf.st_ctim.tv_nsec = 0;
#else
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atim.tv_sec);
  solbuf.st_atim.tv_nsec = endian_swap(sbuf.st_atim.tv_nsec);
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtim.tv_sec);
  solbuf.st_mtim.tv_nsec = endian_swap(sbuf.st_mtim.tv_nsec);
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctim.tv_sec);
  solbuf.st_ctim.tv_nsec = endian_swap(sbuf.st_ctim.tv_nsec);
#endif
  solbuf.st_blksize      = endian_swap(sbuf.st_blksize);
  solbuf.st_blocks       = endian_swap(sbuf.st_blocks);
#ifndef linux
  memcpy(solbuf.st_fstype, sbuf.st_fstype, sizeof(sbuf.st_fstype));
#else
  solbuf.st_fstype[0] = '\0';
#endif

  memcpy(buf, &solbuf, sizeof(solbuf));

  inst->addr = base;
  for (i = 0; i < sizeof(struct solaris_stat64); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = buf[i];
      inst->addr++;
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}
 
 
/***************************************************************************/


static void Lstat64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In LStat Handler\n");
#endif
 
  int                    pr, i;
  char                   buffer[PATH_MAX]; 
  struct stat64          sbuf;
  struct solaris_stat64  solbuf;
  int                    return_value;
  int                    lreturn_register;
  int                    return_register;
  char                  *pa;
  char                   buf[sizeof(struct solaris_stat64)];
  unsigned               base; 

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  base = proc->phy_int_reg_file[pr];
  inst->addr = base;

  memset(buffer, 0, sizeof(buffer));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  base = proc->phy_int_reg_file[pr];

  return_value = lstat64(buffer, &sbuf);

  if (return_value < 0)
    return_value = errno * -1;

  memset(&solbuf, 0, sizeof(solbuf));
  solbuf.st_dev          = endian_swap((unsigned int)sbuf.st_dev);
  solbuf.st_ino          = endian_swap((long long)sbuf.st_ino);
  solbuf.st_mode         = endian_swap(sbuf.st_mode);
  solbuf.st_nlink        = endian_swap(sbuf.st_nlink);
  solbuf.st_uid          = endian_swap(sbuf.st_uid);
  solbuf.st_gid          = endian_swap(sbuf.st_gid);
  solbuf.st_rdev         = endian_swap((unsigned int)sbuf.st_rdev);
  solbuf.st_size         = endian_swap(sbuf.st_size);
#ifdef linux
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atime);
  solbuf.st_atim.tv_nsec = 0;
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtime);
  solbuf.st_mtim.tv_nsec = 0;
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctime);
  solbuf.st_ctim.tv_nsec = 0;
#else
  solbuf.st_atim.tv_sec  = endian_swap(sbuf.st_atim.tv_sec);
  solbuf.st_atim.tv_nsec = endian_swap(sbuf.st_atim.tv_nsec);
  solbuf.st_mtim.tv_sec  = endian_swap(sbuf.st_mtim.tv_sec);
  solbuf.st_mtim.tv_nsec = endian_swap(sbuf.st_mtim.tv_nsec);
  solbuf.st_ctim.tv_sec  = endian_swap(sbuf.st_ctim.tv_sec);
  solbuf.st_ctim.tv_nsec = endian_swap(sbuf.st_ctim.tv_nsec);
#endif
  solbuf.st_blksize      = endian_swap(sbuf.st_blksize);
  solbuf.st_blocks       = endian_swap(sbuf.st_blocks);
#ifndef linux
  memcpy(solbuf.st_fstype, sbuf.st_fstype, sizeof(sbuf.st_fstype));
#else
  solbuf.st_fstype[0] = '\0';
#endif

  memcpy(buf, &solbuf, sizeof(solbuf));

  inst->addr = base;
  for (i = 0; i < sizeof(struct solaris_stat64); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = buf[i];
      inst->addr++;
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}



/***************************************************************************/
/* ChmodHandler : Simulator exception routine that handles chmod           */
/***************************************************************************/

static void ChmodHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Chmod Handler\n");
#endif
  
  int   fd, mode;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  mode = proc->phy_int_reg_file[pr];
 
  return_value = chmod(buffer, mode);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* FchmodHandler : Simulator exception routine that handles fchmod         */
/***************************************************************************/

static void FchmodHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Fchmod Handler\n");
#endif
  
  int fd, mode;
  int lr;
  int pr;
  int return_value = -1;
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  mode = proc->phy_int_reg_file[pr];

  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = fchmod(fd, mode);
      close(fd);
    }
  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}
 



/***************************************************************************/
/* ChownHandler : Simulator exception routine that handles chown           */
/***************************************************************************/

static void ChownHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Chown Handler\n");
#endif
  
  int   fd, uid, gid;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  uid = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  gid = proc->phy_int_reg_file[pr];
 
  return_value = chown(buffer, uid, gid);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* FchownHandler : Simulator exception routine that handles fchown         */
/***************************************************************************/

static void FchownHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Fchown Handler\n");
#endif
  
  int fd, uid, gid;
  int lr;
  int pr;
  int return_value = -1;
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  uid = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  gid = proc->phy_int_reg_file[pr];

  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = fchown(fd, uid, gid);
      close(fd);
    }
  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}
 



/***************************************************************************/
/* LchownHandler : Simulator exception routine that handles lchown         */
/***************************************************************************/

static void LchownHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Lchown Handler\n");
#endif

  int   fd, uid, gid;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  uid = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  gid = proc->phy_int_reg_file[pr];
 
  return_value = lchown(buffer, uid, gid);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* RenameHandler : Simulator exception routine that handles rename         */
/***************************************************************************/

static void RenameHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Rename Handler\n");
#endif

  int   i;
  int   lr;
  int   pr;
  int   return_value;
  char  oldpath[PATH_MAX], newpath[PATH_MAX], *pa;

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];
  memset(oldpath, 0, sizeof(oldpath));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      oldpath[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
  memset(newpath, 0, sizeof(newpath));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      newpath[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  return_value = rename(oldpath, newpath);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* UtimeHandler : Simulator exception routine that handles utime           */
/***************************************************************************/

static void UtimeHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Utime Handler\n");
#endif

  int             i;
  int             lr;
  int             pr;
  int             return_value;
  char            path[PATH_MAX], *pa;
  struct utimbuf  timebuf;

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];
  memset(path, 0, sizeof(path));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      path[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  timebuf.actime = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  timebuf.modtime = proc->phy_int_reg_file[pr];

  return_value = utime(path, &timebuf);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* DMkdirHandler : Simulator exception routine that handles mkdir          */
/***************************************************************************/

static void DMkdirHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Mkdir Handler\n");
#endif

  int   mode, fd;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  mode = proc->phy_int_reg_file[pr];

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = mkdir(buffer, mode);

  if (return_value < 0)
    return_value = errno * -1;

  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* DRmdirHandler : Simulator exception routine that handles rmdir          */
/***************************************************************************/

static void DRmdirHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Rmdir Handler\n");
#endif

  int   fd;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer, 0, sizeof(buffer));
  for (int i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  return_value = 0;
  if (fd != -1)
    return_value = FD_fchdir(fd);
  if (return_value == 0)
    return_value = rmdir(buffer);
  
  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}



/***************************************************************************/
/* GetdentsHandler : Simulator exception routine that handles getdents     */
/***************************************************************************/

typedef struct solaris_dirent64
{
  unsigned long long d_ino;
  unsigned long long d_off;
  unsigned short     d_reclen;
  char               d_name[1];
} solaris_dirent_t;



static void Getdents64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Getdents Handler\n");
#endif
  int                      fd, fd1, size;
  int                      lr;
  int                      pr;
  int                      return_value = -1, count;
  char                    *buf, *obuf, *pa;
  off_t                    next_offset;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  size = proc->phy_int_reg_file[pr];

  obuf = (char*)malloc(size);
  if (obuf == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "SimGetDents: Malloc failed: %s\n",
	       YS__strerror(errno));
  memset(obuf, 0, size);
  
  buf = (char*)malloc(size);
  if (buf == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "SimGetDents: Malloc failed: %s\n",
	       YS__strerror(errno));
  memset(buf, 0, size);

  fd1 = FD_open(fd);
  if (fd1 < 0)
    count = -1;
  else
    count = getdents(fd1, (struct dirent*)buf, size);  

  if (count < 0)
    return_value = errno * -1;
  else
    {
      int bufcount = 0;
      return_value = 0;
      dirent *src;
      solaris_dirent64 *dest;


      // @@@@ buffer overflow ??
      while ((bufcount < count) &&
	     ((((sizeof(dest->d_ino) + sizeof(dest->d_off) + sizeof(dest->d_reclen) +
		 strlen(((dirent *)(buf+bufcount))->d_name) + 1 + 7) & 0xfff8) +
	       return_value < size)))
        {
          src = (dirent *)(buf + bufcount);
          dest = (solaris_dirent64 *)(obuf + return_value);
	  next_offset = src->d_off;

          // copy a directory entry
          dest->d_ino = endian_swap((unsigned long long)src->d_ino);
          dest->d_off = endian_swap((unsigned long long)src->d_off);
          dest->d_reclen = endian_swap((unsigned short)
	    ((sizeof(dest->d_ino) +
	      sizeof(dest->d_off) +
	      sizeof(dest->d_reclen) +
	      strlen(src->d_name) + 1 +
	      sizeof(dest->d_ino) - 1) &
	     0xfff8));                   // force alignment
          strcpy(dest->d_name, src->d_name);

          // point buf to the next entry
          bufcount += src->d_reclen;

          // point obuf to the next entry
          return_value += endian_swap(dest->d_reclen);
        }

      // If we weren't able to copy all the buf entries into obuf,
      // we'd better seek the fd back to the offset of the next
      // directory entry, so we will start w/ it on the next call!
      if (bufcount != count)
        lseek(fd1, next_offset, SEEK_SET);

      for (int i = 0; i < size; i++)
        {
          pa = (char *)GetMap(inst, proc);
          *pa = obuf[i];
          inst->addr++;
        }
    }

  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;

  FD_lseek(fd, lseek(fd1, 0, SEEK_CUR), SEEK_SET);
  close(fd1);
  free(buf);
  free(obuf);
}




/***************************************************************************/
/* Link system call                                                        */
/***************************************************************************/

static void LinkHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Link Handler\n");
#endif

  int   i;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer0[PATH_MAX], buffer1[PATH_MAX], *pa;
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer0, 0, sizeof(buffer0));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer0[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer1, 0, sizeof(buffer1));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer1[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  
  return_value = link(buffer0, buffer1);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}




/***************************************************************************/
/* SymLink system call                                                     */
/***************************************************************************/

static void SymlinkHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Symlink Handler\n");
#endif

  int   i;
  int   lr;
  int   pr;
  int   return_value;
  char  buffer0[PATH_MAX], buffer1[PATH_MAX], *pa;

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer0, 0, sizeof(buffer0));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer0[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(buffer1, 0, sizeof(buffer1));
  for (i = 0; i < PATH_MAX-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      buffer1[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }

  
  return_value = symlink(buffer0, buffer1);

  if (return_value < 0)
    return_value = errno * -1;
  
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}











/***************************************************************************/
/***************************************************************************/

struct solaris_sockaddr_in
{
  unsigned short   sin_family;
  unsigned short   sin_port;
  unsigned int     sin_addr;
  char             sin_zero[8];
};

struct solaris_linger
{
  int     l_onoff;
  int     l_linger;
};

#define SOLARIS_AF_UNSPEC       0
#define SOLARIS_AF_INET         2

#define SOLARIS_SOCK_DGRAM      1
#define SOLARIS_SOCK_STREAM     2
#define SOLARIS_SOCK_RAW        4
#define SOLARIS_SOCK_RDM        5
#define SOLARIS_SOCK_SEQPACKET  6

#define SOLARIS_PF_UNSPEC       SOLARIS_AF_UNSPEC
#define SOLARIS_PF_INET         SOLARIS_AF_INET

#define SOLARIS_SO_DEBUG        0x0001
#define SOLARIS_SO_REUSEADDR    0x0004
#define SOLARIS_SO_KEEPALIVE    0x0008
#define SOLARIS_SO_DONTROUTE    0x0010
#define SOLARIS_SO_BROADCAST    0x0020
#define SOLARIS_SO_LINGER       0x0080
#define SOLARIS_SO_OOBINLINE    0x0100
#define SOLARIS_SO_SNDBUF       0x1001
#define SOLARIS_SO_RCVBUF       0x1002
#define SOLARIS_SO_DGRAM_ERRIND 0x0200
#define SOLARIS_SO_ERROR        0x1007
#define SOLARIS_SO_TYPE         0x1008




static int CopyinSockAddr(ProcState *proc, struct sockaddr_in *sockaddr,
			   unsigned paddr, int sockaddrlen)
{
  struct solaris_sockaddr_in  sol_addr;
  char                       *pa;
  int                         i;

  if (sockaddrlen != sizeof(struct solaris_sockaddr_in))
    {
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Sizeof(solaris_sockaddr_in) != namelen (%i != %i)\n",
		  sizeof(struct solaris_sockaddr_in), sockaddrlen);
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Lamix Kernel may be using a different sockaddr structure\n");
    }

  memset(&sol_addr, 0, sizeof(sol_addr));
  for (i = 0; i < sockaddrlen; i++)
    {
      pa = PageTables[proc->proc_id / ARCH_cpus]->lookup(paddr);
      if (pa == NULL)
	{
	  errno = EINVAL;
	  return(EINVAL);
	}
      *((char*)(&sol_addr) + i) = *pa;
      paddr++;
    }

  sol_addr.sin_family = endian_swap(sol_addr.sin_family);
  if ((sol_addr.sin_family != SOLARIS_AF_UNSPEC) && 
      (sol_addr.sin_family != SOLARIS_AF_INET))
    {
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "CopyinSockAddr: Unknown sockaddr.sin_family 0x%X\n",
		  sol_addr.sin_family);
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Lamix kernel may be using an unsupported protocol\n");
    }
  
  if (sol_addr.sin_family == SOLARIS_AF_UNSPEC)
    sockaddr->sin_family     = AF_UNSPEC;
  if (sol_addr.sin_family == SOLARIS_AF_INET)
    sockaddr->sin_family     = AF_INET;
  sockaddr->sin_port         = sol_addr.sin_port;
  sockaddr->sin_addr.s_addr  = sol_addr.sin_addr;

  return(0);
}


static int CopyoutSockAddr(ProcState *proc, struct sockaddr_in *sockaddr,
			   unsigned paddr)
{
  struct solaris_sockaddr_in  sol_addr;
  char                       *pa;
  int                         i;

  if ((sockaddr->sin_family != AF_UNSPEC) && 
      (sockaddr->sin_family != AF_INET))
    {
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "CopyoutSockAddr: Unknown sockaddr.sin_family 0x%X\n",
		  sockaddr->sin_family);
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Lamix kernel may be using an unsupported protocol\n");
    }
  
  if (sockaddr->sin_family == AF_UNSPEC)
    sol_addr.sin_family = endian_swap(SOLARIS_AF_UNSPEC);
  if (sockaddr->sin_family == AF_INET)
    sol_addr.sin_family = endian_swap(SOLARIS_AF_INET);
  sol_addr.sin_port     = sockaddr->sin_port;
  sol_addr.sin_addr     = sockaddr->sin_addr.s_addr;  
  
  for (i = 0; i < sizeof(struct solaris_sockaddr_in); i++)
    {
      pa = PageTables[proc->proc_id / ARCH_cpus]->lookup(paddr);
      if (pa == NULL)
	{
	  errno = EINVAL;
	  return(EINVAL);
	}
      *pa = *((char*)(&sol_addr) + i);
      paddr++;
    }

  return(0);
}


/*****************************************************************************/


static void SocketHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Socket Handler\n");
#endif
  
  int domain, type, protocol, pr;
  int return_value;
  int lreturn_register;
  int return_register;
  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  domain = proc->phy_int_reg_file[pr];
  switch (domain)
    {
    case SOLARIS_AF_UNSPEC: domain = AF_UNSPEC;      break;
    case SOLARIS_AF_INET:   domain = AF_INET;        break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Unsupported socket address family 0x%X\n", domain);
    }
  
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  type = proc->phy_int_reg_file[pr];
  switch (type)
    {
    case SOLARIS_SOCK_DGRAM     : type = SOCK_DGRAM;     break;
    case SOLARIS_SOCK_STREAM    : type = SOCK_STREAM;    break;
    case SOLARIS_SOCK_RAW       : type = SOCK_RAW;       break;
    case SOLARIS_SOCK_RDM       : type = SOCK_RDM;       break;
    case SOLARIS_SOCK_SEQPACKET : type = SOCK_SEQPACKET; break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Unsupported socket type 0x%X\n", type);
    }      

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  protocol = proc->phy_int_reg_file[pr];
  switch (protocol)
    {
    case SOLARIS_PF_UNSPEC: protocol = PF_UNSPEC;      break;
    case SOLARIS_PF_INET:   protocol = PF_INET;        break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Unsupported socket protocol 0x%X\n", protocol);      
    }

  return_value = socket(domain, type, protocol);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/

static void BindHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Bind Handler\n");
#endif
  
  int                  fd, addrlen, pr;
  unsigned int         addr;
  int                  return_value = -1;
  int                  lreturn_register;
  int                  return_register;
  struct sockaddr_in   sa;
  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  addrlen = proc->phy_int_reg_file[pr];

  return_value = CopyinSockAddr(proc, &sa, addr, addrlen);
  addrlen = sizeof(sa);
  if (return_value == 0)
    return_value = bind(fd, (struct sockaddr*)&sa, addrlen);

  if (return_value < 0)
    return_value = errno * -1;

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}

/***************************************************************************/

static void ListenHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Listen Handler\n");
#endif
  
  int fd, backlog, pr;
  int return_value = -1;
  int lreturn_register;
  int return_register;
  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  backlog = proc->phy_int_reg_file[pr];

  return_value = listen(fd, backlog);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}

/***************************************************************************/

extern "C" void net_alarm(int sig)
{
}

static void AcceptHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Accept Handler\n");
#endif

  int                 fd, pr;
#ifdef linux
  socklen_t           addrlen;
#else
  int                 addrlen;
#endif
  struct sockaddr_in  sa;
  unsigned int        addr;
  int                 return_value = -1;
  int                 lreturn_register;
  int                 return_register;
  char               *pa;
  int                 i;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];

  memset(&sa, 0, sizeof(sa));
  addrlen = sizeof(sa);

  signal(SIGALRM, net_alarm);
  alarm(NET_MSG_DELAY);
  return_value = accept(fd, (struct sockaddr*)&sa, &addrlen);
  if ((return_value == -1) && (errno == EINTR))
    {
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "SimTrapAccept: Simulation stopped waiting for INET socket connection\n");
      signal(SIGALRM, SIG_IGN);
      return_value = accept(fd, (struct sockaddr*)&sa, &addrlen);
    }
  alarm(0);

  /* copy sockaddr into simulator physical memory */
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = proc->phy_int_reg_file[pr];
  CopyoutSockAddr(proc, &sa, addr);

  /* copy addrlen into simulator physical memory */
  if (return_value < 0)
    addrlen = 0;
  else
    addrlen = endian_swap(sizeof(struct solaris_sockaddr_in));
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  inst->addr = proc->phy_int_reg_file[pr];
  for (i = 0; i < sizeof(addrlen); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *((char*)(&addrlen) + i);
      inst->addr++;
    }
  
  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/

static void ConnectHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Connect Handler\n");
#endif

  int                 fd, addrlen, pr;
  unsigned int        addr;
  struct sockaddr_in  sa;
  int                 return_value = -1;
  int                 lreturn_register;
  int                 return_register;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  addrlen = proc->phy_int_reg_file[pr];
 
  return_value = CopyinSockAddr(proc, &sa, addr, addrlen);
  addrlen = sizeof(sa);
  if (return_value == 0)
    return_value = connect(fd, (struct sockaddr*)&sa, addrlen);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}

/***************************************************************************/

static void GetSockOptHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In GetSockOpt Handler\n");
#endif

  int                    fd, level, optname, optval, pr;
#ifdef linux
  socklen_t              optlen;
#else
  int                    optlen;
#endif
  int                    return_value = -1;
  int                    lreturn_register;
  int                    return_register;
  char                  *pa, *p;
  struct solaris_linger  sol_ling;
  struct linger          ling;
  int                    i, len;


  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  level = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  optname = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  optval = proc->phy_int_reg_file[pr];


  // convert Solaris/Lamix option name into native form
  switch(optname)
    {
    case SOLARIS_SO_DEBUG:         optname = SO_DEBUG;         break;
    case SOLARIS_SO_REUSEADDR:     optname = SO_REUSEADDR;     break;
    case SOLARIS_SO_KEEPALIVE:     optname = SO_KEEPALIVE;     break;
    case SOLARIS_SO_DONTROUTE:     optname = SO_DONTROUTE;     break;
    case SOLARIS_SO_BROADCAST:     optname = SO_BROADCAST;     break;
    case SOLARIS_SO_LINGER:        optname = SO_LINGER;        break;
    case SOLARIS_SO_OOBINLINE:     optname = SO_OOBINLINE;     break;
    case SOLARIS_SO_SNDBUF:        optname = SO_SNDBUF;        break;
    case SOLARIS_SO_RCVBUF:        optname = SO_RCVBUF;        break;
#if !defined(sgi) && !defined(linux)
    case SOLARIS_SO_DGRAM_ERRIND:  optname = SO_DGRAM_ERRIND;  break;
#endif
    case SOLARIS_SO_ERROR:         optname = SO_ERROR;         break;
    case SOLARIS_SO_TYPE:          optname = SO_TYPE;          break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "GetOptName: Unknown option name %i\n", optname);
    }


  // optval specified: check optlen & set pointer to buffer
  if (optval != 0)
    {
      // copy current optlen value from simulator memory
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 12)];
      inst->addr = proc->phy_int_reg_file[pr];
      for (i = 0; i < sizeof(optlen); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *((char*)(&optlen) + i) = *pa;
	  inst->addr++;
	}
      optlen = endian_swap(optlen);

      // linger: check size and set result buffer
      if (optname == SO_LINGER)
	{
	  if (optlen != sizeof(sol_ling))
	    YS__warnmsg(proc->proc_id / ARCH_cpus,
			"GetSockOpt(SO_LINGER): optlen != sizeof(sol_linger) (%i != %i)\n",
			optlen, sizeof(sol_ling));
	  p = (char*)&ling;
	  optlen = sizeof(ling);
	}
      // other optname: check size (should be integer)
      else
	{
	  if (optlen != sizeof(optval))
	    YS__warnmsg(proc->proc_id / ARCH_cpus,
			"GetSockOpt: optlen != sizeof(int) (%i != %i)\n",
			optlen, sizeof(optval));
	  p = (char*)&optval;
	  optlen = sizeof(optval);
	}

      memset(p, 0, optlen);
    }
  else
    {
      p = NULL;
      optlen = 0;
    }
  
  return_value = getsockopt(fd, level, optname, p, &optlen);

  if (optval != 0)
    {
      // linger: convert linger stucture to Solaris/Lamix format
      if (optname == SO_LINGER)
	{
	  sol_ling.l_onoff  = endian_swap(ling.l_onoff);
	  sol_ling.l_linger = endian_swap(ling.l_linger);
	  p = (char*)&sol_ling;
	  len = sizeof(sol_ling);
	}
      // other optname: endianswap
      else
	{
	  optval = endian_swap(optval);
	  len = sizeof(optval);
	}

      // copy optval into simulator memory
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
      inst->addr = proc->phy_int_reg_file[pr];
      for (i = 0; i < len; i++)
        {
          pa = (char *)GetMap(inst, proc);
          *pa = *(p + i);
          inst->addr++;
        }

      // swap and copy result optlen into simulator memory
      optlen = endian_swap(optlen);
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 12)];
      inst->addr = proc->phy_int_reg_file[pr];
      for (i = 0; i < sizeof(optlen); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *pa = *((char*)(&optlen) + i);
	  inst->addr++;
	}
    }

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/

static void SetSockOptHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In SetSockOpt Handler\n");
#endif
  int                    fd, level, optname, optval, optlen, pr;
  int                    return_value = -1, i;
  int                    lreturn_register;
  int                    return_register;
  char                  *pa, *p;
  struct solaris_linger  sol_ling;
  struct linger          ling;

  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  level = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  optname = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  optval = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 12)];
  optlen = proc->phy_int_reg_file[pr];

  // convert Solaris/Lamix option name into native form
  switch(optname)
    {
    case SOLARIS_SO_DEBUG:         optname = SO_DEBUG;         break;
    case SOLARIS_SO_REUSEADDR:     optname = SO_REUSEADDR;     break;
    case SOLARIS_SO_KEEPALIVE:     optname = SO_KEEPALIVE;     break;
    case SOLARIS_SO_DONTROUTE:     optname = SO_DONTROUTE;     break;
    case SOLARIS_SO_BROADCAST:     optname = SO_BROADCAST;     break;
    case SOLARIS_SO_LINGER:        optname = SO_LINGER;        break;
    case SOLARIS_SO_OOBINLINE:     optname = SO_OOBINLINE;     break;
    case SOLARIS_SO_SNDBUF:        optname = SO_SNDBUF;        break;
    case SOLARIS_SO_RCVBUF:        optname = SO_RCVBUF;        break;
#if !defined(sgi) && !defined(linux)
    case SOLARIS_SO_DGRAM_ERRIND:  optname = SO_DGRAM_ERRIND;  break;
#endif
    case SOLARIS_SO_ERROR:         optname = SO_ERROR;         break;
    case SOLARIS_SO_TYPE:          optname = SO_TYPE;          break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "GetOptName: Unknown option name %i\n", optname);
    }


  // optval specified: check optlen & set pointer to buffer
  if (optval != 0)
    {
      // linger: check size and set result buffer
      if (optname == SO_LINGER)
	{
	  if (optlen != sizeof(sol_ling))
	    YS__warnmsg(proc->proc_id / ARCH_cpus,
			"GetSockOpt(SO_LINGER): optlen != sizeof(sol_linger) (%i != %i)\n",
			optlen, sizeof(sol_ling));
	  p = (char*)&sol_ling;
	  optlen = sizeof(sol_ling);
	}
      // other optname: check size (should be integer)
      else
	{
	  if (optlen != sizeof(optval))
	    YS__warnmsg(proc->proc_id / ARCH_cpus,
			"GetSockOpt: optlen != sizeof(int) (%i != %i)\n",
			optlen, sizeof(optval));
	  p = (char*)&optval;
	  optlen = sizeof(optval);
	}

      // copy optval from simulator memory
      pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
      inst->addr = proc->phy_int_reg_file[pr];
      for (i = 0; i < optlen; i++)
        {
          pa = (char *)GetMap(inst, proc);
          *(p + i) = *pa;
          inst->addr++;
        }

      // convert linger structure or integer into native format
      if (optname == SO_LINGER)
	{
	  ling.l_onoff  = endian_swap(sol_ling.l_onoff);
	  ling.l_linger = endian_swap(sol_ling.l_linger);
	  p = (char*)&ling;
	  optlen = sizeof(ling);
	}
      else
	optval = endian_swap(optval);	
    }
  else
    {
      p = NULL;
      optlen = 0;
    }
  
  return_value = setsockopt(fd, level, optname, p, optlen);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}

/***************************************************************************/

static void ShutdownHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Shutdown Handler\n");
#endif
  
  int fd, how, pr;
  int return_value = -1;
  int lreturn_register;
  int return_register;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  how = proc->phy_int_reg_file[pr];

  return_value = shutdown(fd, how);

  if (return_value < 0)
    return_value = errno * -1;

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/
/* CloseSocketHandler : Simulator exception routine that closes sockets    */
/***************************************************************************/

static void CloseSocketHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In CloseSocket Handler\n");
#endif
  
  int fd;
  int lr;
  int pr;
  int return_value;
 
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  fd = proc->phy_int_reg_file[pr];

  return_value = close(fd);
  if (return_value < 0)
    return_value = errno * -1;

  proc->phy_int_reg_file[pr] =
    proc->log_int_reg_file[lr] = return_value;
}
 
 


/***************************************************************************/

static void GetSockNameHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Connect Handler\n");
#endif

  int                 fd, pr, i;
#ifdef linux
  socklen_t           addrlen;
#else
  int                 addrlen;
#endif
  struct sockaddr_in  sa;
  int                 return_value = -1;
  int                 lreturn_register;
  int                 return_register;
  unsigned int        addr;
  char               *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  //  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  //  inst->addr = proc->phy_int_reg_file[pr];
 
  addrlen = sizeof(sa);
  memset(&sa, 0, sizeof(sa));
  return_value = getsockname(fd, (struct sockaddr*)&sa, &addrlen);

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = proc->phy_int_reg_file[pr];
  CopyoutSockAddr(proc, &sa, addr);

  if (return_value < 0)
    addrlen = 0;
  else
    addrlen = endian_swap(sizeof(struct solaris_sockaddr_in));
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  inst->addr = proc->phy_int_reg_file[pr];
  for (i = 0; i < sizeof(addrlen); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *((char*)(&addrlen) + i);
      inst->addr++;
    }
  
  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/

static void GetPeerNameHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Connect Handler\n");
#endif

  int                 fd, pr, i;
#ifdef linux
  socklen_t           addrlen;
#else
  int                 addrlen;
#endif
  struct sockaddr_in  sa;
  int                 return_value = -1;
  int                 lreturn_register;
  int                 return_register;
  unsigned int        addr;
  char               *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  //  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  //  inst->addr = proc->phy_int_reg_file[pr];
 
  addrlen = sizeof(sa);
  memset(&sa, 0, sizeof(sa));
  return_value = getpeername(fd, (struct sockaddr*)&sa, &addrlen);

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = proc->phy_int_reg_file[pr];
  CopyoutSockAddr(proc, &sa, addr);
  
  if (return_value < 0)
    addrlen = 0;
  else
    addrlen = endian_swap(sizeof(struct solaris_sockaddr_in));
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  inst->addr = proc->phy_int_reg_file[pr];
  for (i = 0; i < sizeof(addrlen); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *((char*)(&addrlen) + i);
      inst->addr++;
    }
  
  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/*****************************************************************************/

struct solaris_msghdr
{
  void         *msg_name;
  int           msg_namelen;
  struct iovec *msg_iov;
  int           msg_iovlen;
  char*         msg_accrights;
  int           msg_accrightslen;
};

struct solaris_iovec
{
  caddr_t iov_base;
  size_t  iov_len;
};


static void SendMsgHandler(instance *inst, ProcState *proc)
{
  int    fd, pr, flags, i, n;
  struct solaris_msghdr   sol_msg;
  struct msghdr           msg;
  struct solaris_iovec   *sol_iov;
  struct iovec           *iov;
  struct sockaddr_in      sa;
  int    return_value = -1;
  int    lreturn_register;
  int    return_register;
  char  *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  flags = proc->phy_int_reg_file[pr];
 
  for (i = 0; i < sizeof(sol_msg); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *((char*)(&sol_msg) + i) = *pa;
      inst->addr++;
    }

  msg.msg_name         = (caddr_t)endian_swap(sol_msg.msg_name);
  msg.msg_namelen      = endian_swap(sol_msg.msg_namelen);
  msg.msg_iovlen       = endian_swap(sol_msg.msg_iovlen);
#ifndef linux
  msg.msg_accrights    = endian_swap(sol_msg.msg_accrights);
  msg.msg_accrightslen = endian_swap(sol_msg.msg_accrightslen);
#endif
  
  if (msg.msg_namelen > 0)
    {
      msg.msg_name = (caddr_t)&sa;
      CopyinSockAddr(proc, &sa, (unsigned)endian_swap(sol_msg.msg_name),
		     msg.msg_namelen);
    }

  if (msg.msg_iovlen > 0)
    {
      msg.msg_iov = (struct iovec*)malloc(msg.msg_iovlen * sizeof(iovec));
      sol_iov = (struct solaris_iovec*)malloc(msg.msg_iovlen * sizeof(solaris_iovec));
      inst->addr = (unsigned int)endian_swap(sol_msg.msg_iov);
      for (i = 0; i < msg.msg_iovlen * sizeof(solaris_iovec); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *((char*)(sol_iov) + i) = *pa;
	  inst->addr++;
	}
    }
  else
    {
      msg.msg_iov = NULL;
      sol_iov     = NULL;
    }
    
  for (n = 0; n < msg.msg_iovlen; n++)
    {
      msg.msg_iov[n].iov_len = endian_swap(sol_iov[n].iov_len);
      msg.msg_iov[n].iov_base = (char*)malloc(msg.msg_iov[n].iov_len);
      inst->addr = (unsigned int)endian_swap(sol_iov[n].iov_base);
      for (i = 0; i < msg.msg_iov[n].iov_len; i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *((char*)(msg.msg_iov[n].iov_base) + i) = *pa;
	  inst->addr++;
	}
    }

#ifndef linux
  msg.msg_accrights    = NULL;
  msg.msg_accrightslen = 0;
#endif
  return_value = sendmsg(fd, &msg, flags);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;

  if (msg.msg_iov)
    {
      for (n = 0; n < msg.msg_iovlen; n++)
	free(msg.msg_iov[n].iov_base);
      free(msg.msg_iov);
    }
  if (sol_iov)
    free(sol_iov);
}



/*****************************************************************************/

static void RecvMsgHandler(instance *inst, ProcState *proc)
{
  int    fd, pr, flags, i, n;
  struct solaris_msghdr   sol_msg;
  struct msghdr           msg;
  struct solaris_iovec   *sol_iov;
  struct iovec           *iov;
  struct sockaddr_in      sa;
  int    return_value = -1;
  int    lreturn_register;
  int    return_register;
  char  *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  flags = proc->phy_int_reg_file[pr];
 
  for (i = 0; i < sizeof(sol_msg); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *((char*)(&sol_msg) + i) = *pa;
      inst->addr++;
    }

  msg.msg_namelen      = endian_swap(sol_msg.msg_namelen);
  msg.msg_iovlen       = endian_swap(sol_msg.msg_iovlen);
#ifndef linux
  msg.msg_accrightslen = endian_swap(sol_msg.msg_accrightslen);
#endif

  if (msg.msg_namelen > 0)
    {
      memset(&sa, 0, sizeof(sa));
      msg.msg_name = (caddr_t)&sa;
      msg.msg_namelen = sizeof(sa);
    }
      
  if (msg.msg_iovlen > 0)
    {
      msg.msg_iov = (struct iovec*)malloc(msg.msg_iovlen * sizeof(iovec));
      sol_iov = (struct solaris_iovec*)malloc(msg.msg_iovlen * sizeof(solaris_iovec));
      inst->addr = (unsigned int)endian_swap(sol_msg.msg_iov);
      for (i = 0; i < msg.msg_iovlen * sizeof(solaris_iovec); i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *((char*)(sol_iov) + i) = *pa;
	  inst->addr++;
	}
    }
  else
    {
      msg.msg_iov = NULL;
      sol_iov     = NULL;
    }
    
  for (n = 0; n < msg.msg_iovlen; n++)
    {
      msg.msg_iov[n].iov_len = endian_swap(sol_iov[n].iov_len);
      msg.msg_iov[n].iov_base = (char*)malloc(msg.msg_iov[n].iov_len);
    }

#ifndef linux
  msg.msg_accrights = NULL;
  msg.msg_accrightslen = 0;
#endif
  
  signal(SIGALRM, net_alarm);
  alarm(NET_MSG_DELAY);
  return_value = recvmsg(fd, &msg, flags);
  if ((return_value == -1) && (errno == EINTR))
    {
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "SimTrapRecvMsg: Simulation stopped waiting for INET socket data\n");
      signal(SIGALRM, SIG_IGN);
      return_value = recvmsg(fd, &msg, flags);
    }
  alarm(0);

  if (msg.msg_namelen > 0)
    CopyoutSockAddr(proc, &sa, (unsigned)endian_swap(sol_msg.msg_name));

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;

  for (n = 0; n < msg.msg_iovlen; n++)
    {
      inst->addr = (unsigned int)endian_swap(sol_iov[n].iov_base);
      for (i = 0; i < msg.msg_iov[n].iov_len; i++)
	{
	  pa = (char *)GetMap(inst, proc);
	  *pa = *((char*)(msg.msg_iov[n].iov_base) + i);
	  inst->addr++;
	}
    }

  if (msg.msg_iov)
    free(msg.msg_iov);
  if (sol_iov)
    free(sol_iov);
}




static void GetHostByNameHandler(instance *inst, ProcState *proc)
{
  int                 pr, i;
  int                 lreturn_register, return_register;  
  char               *pa, name[GETHOST_NAMESZ], *p;
  struct lib_hostent  lhe;
  struct hostent     *he;

  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(name, 0, sizeof(name));
  for (i = 0; i < sizeof(name)-1; i++)
    {
      pa = (char *)GetMap(inst, proc);
      name[i] = *pa;
      if (*pa == '\0')
        break;
      inst->addr++;
    }
  
  he = gethostbyname(name);
  if (he == NULL)
    {
      proc->phy_int_reg_file[return_register] =
	proc->log_int_reg_file[lreturn_register] = h_errno;
      return;
    }

  memset(&lhe, 0, sizeof(lhe));
  if (he->h_addrtype != AF_INET)
    YS__warnmsg(proc->proc_id / ARCH_cpus,
		"GetHostByName: address type (%i) is not AF_INET\n",
		he->h_addrtype);
  lhe.addrtype   = endian_swap(SOLARIS_AF_INET);
  lhe.addrlength = endian_swap(he->h_length);
  strncpy(lhe.names[0], he->h_name, GETHOST_NAMESZ-1);
  for (i = 1; he->h_aliases[i-1] != 0; i++)
    {
      if (i >= GETHOST_MAXNAMES)
	{
	  YS__warnmsg(proc->proc_id / ARCH_cpus,
		      "More than %i aliases provided by GetHostByName\n",
		      GETHOST_MAXNAMES-1);
	  break;
	}

      strncpy(lhe.names[i], he->h_aliases[i-1], GETHOST_NAMESZ-1);
    }

  for (i = 0; he->h_addr_list[i] != 0; i++)
    {
      if (i >= GETHOST_MAXNAMES)
	{
	  YS__warnmsg(proc->proc_id / ARCH_cpus,
		      "More than %i addresses provided by GetHostByAddr\n",
		      GETHOST_MAXNAMES);
	  break;
	}

      memcpy(&lhe.addrs[i], he->h_addr_list[i], sizeof(in_addr_t));
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];
  for (i = 0; i < sizeof(lhe); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *((char*)(&lhe) + i);
      inst->addr++;
    }
  
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = 0;
}


static void GetHostByAddrHandler(instance *inst, ProcState *proc)
{
  int                 pr, i, size, type;
  in_addr_t           addr;
  int                 lreturn_register, return_register;  
  char               *pa, *p;
  struct lib_hostent  lhe;
  struct hostent     *he;

  
  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  addr = endian_swap(proc->phy_int_reg_file[pr]);

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  size = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 11)];
  type = proc->phy_int_reg_file[pr];

  if (size != sizeof(in_addr_t))
    YS__warnmsg(proc->proc_id / ARCH_cpus,
		"GetHostByAddr: input address size (%i) != sizeof(in_addr_t) (%i)\n", size, sizeof(in_addr_t));

  if (type != SOLARIS_AF_INET)
    YS__warnmsg(proc->proc_id / ARCH_cpus,
		"GetHostByAddr: address type (%i) is not AF_INET\n", type);

  he = gethostbyaddr((char*)&addr, sizeof(in_addr_t), AF_INET);
  if (he == NULL)
    {
      proc->phy_int_reg_file[return_register] =
	proc->log_int_reg_file[lreturn_register] = h_errno;
      return;
    }

  memset(&lhe, 0, sizeof(lhe));
  if (he->h_addrtype != AF_INET)
    YS__warnmsg(proc->proc_id / ARCH_cpus,
		"GetHostByAddr: address type (%i) is not AF_INET\n",
		he->h_addrtype);
  lhe.addrtype   = endian_swap(SOLARIS_AF_INET);
  lhe.addrlength = endian_swap(he->h_length);
  strncpy(lhe.names[0], he->h_name, GETHOST_NAMESZ-1);
  for (i = 1; he->h_aliases[i-1] != 0; i++)
    {
      if (i >= GETHOST_MAXNAMES)
	{
	  YS__warnmsg(proc->proc_id / ARCH_cpus,
		      "More than %i aliases provided by GetHostByAddr\n",
		      GETHOST_MAXNAMES-1);
	  break;
	}

      strncpy(lhe.names[i], he->h_aliases[i-1], GETHOST_NAMESZ-1);
    }

  for (i = 0; he->h_addr_list[i] != 0; i++)
    {
      if (i >= GETHOST_MAXNAMES)
	{
	  YS__warnmsg(proc->proc_id / ARCH_cpus,
		      "More than %i addresses provided by GetHostByAddr\n",
		      GETHOST_MAXNAMES);
	  break;
	}

      memcpy(&lhe.addrs[i], he->h_addr_list[i], sizeof(in_addr_t));
    }

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];
  for (i = 0; i < sizeof(lhe); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *((char*)(&lhe) + i);
      inst->addr++;
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = 0;
}


/*****************************************************************************/

static void SysInfoHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In SysInfo Handler\n");
#endif
  
  int   pr, cmd, size;
  int   return_value;
  int   lreturn_register;
  int   return_register;
  char *name, *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  cmd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  size = proc->phy_int_reg_file[pr];

  name = (char*)malloc(size);
  if (name == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "SimSysInfo: Malloc failed: %s\n",
	       YS__strerror(errno));
  memset(name, 0, size);

  
#ifdef linux

  struct utsname uname_info;

  uname(&uname_info);
  return_value = 0;
  
  switch (cmd)
    {
    case 1:			// SI_SYSNAME      -- name of OS
      strncpy(name, uname_info.sysname, size);
      break;
    case 2:			// SI_HOSTNAME     -- name of node
      strncpy(name, uname_info.nodename, size);
      break;
    case 3:			// SI_RELEASE      -- release of OS
      strncpy(name, uname_info.release, size);
      break;
    case 4:                     // SI_VERSION      -- version field of utsname
      strncpy(name, uname_info.version, size);
      break;
    case 5:                     // SI_MACHINE      -- kind of machine
      strncpy(name, uname_info.machine, size);
      break;
    case 9:                     // SI_SRPC_DOMAIN  -- domain name
      strncpy(name, uname_info.domainname, size);
      break;
    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		 "SysInfo: unknown request: %d, returning empty string", cmd);
      strcpy(name, "");
      break;
    }

#else     // ifdef linux

  return_value = sysinfo(cmd, name, size);

#endif    // ifdef linux


  for (int i = 0; i < size; i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = name[i];
      inst->addr++;
    }

  free(name);
  
  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}

/***************************************************************************/

static void PathConfHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In PathConf Handler\n");
#endif
  
  int   pr, name;
  int   return_value;
  int   lreturn_register;
  int   return_register;
  char  path[PATH_MAX], *pa;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  inst->addr = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  name = proc->phy_int_reg_file[pr];

  memset(path, 0, sizeof(path));
  for (int i = 0; i < PATH_MAX; i++)
    {
      pa = (char *)GetMap(inst, proc);
      path[i] = *pa;
      if (*pa == '\0')
	break;
      inst->addr++;
    }
  
  return_value = (int)pathconf(path, name);

  if (return_value < 0)
    return_value = errno * -1;
 
  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}


/***************************************************************************/


typedef struct solaris_statvfs64
{
  unsigned long       f_bsize;        /* fundamental file system block size  */
  unsigned long       f_frsize;       /* fragment size                       */
  unsigned long long  f_blocks;       /* total blocks of f_frsize on fs      */
  unsigned long long  f_bfree;        /* total free blocks of f_frsize       */
  unsigned long long  f_bavail;       /* free blocks avail to non-superuser  */
  unsigned long long  f_files;        /* total file nodes (inodes)           */
  unsigned long long  f_ffree;        /* total free file nodes               */
  unsigned long long  f_favail;       /* free nodes avail to non-superuser   */
  unsigned long       f_fsid;         /* file system id (dev for now)        */
  char                f_basetype[16]; /* target fs type name,                */
                                      /* null-terminated                     */
  unsigned long       f_flag;         /* bit-mask of flags                   */
  unsigned long       f_namemax;      /* maximum file name length            */
  char                f_fstr[32];     /* filesystem-specific string          */
} solaris_statvfs64_t;

static void Fstatvfs64Handler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In FStat Handler\n");
#endif
 
  int                       pr;
  int                       fd, i;
  struct solaris_statvfs64  solbuf;
  struct statvfs64          buf;
  int                       return_value = -1;
  int                       lreturn_register;
  int                       return_register;
  char                     *pa;
  unsigned                  base;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  fd = FD_open(fd);
  if (fd >= 0)
    {
      return_value = fstatvfs64(fd, &buf);
      close(fd);
    }
  if (return_value < 0)
    return_value = errno * -1;

  memset(&solbuf, 0, sizeof(solbuf));
  solbuf.f_bsize       = endian_swap(buf.f_bsize);
  solbuf.f_frsize      = endian_swap(buf.f_frsize);
  solbuf.f_blocks      = endian_swap(buf.f_blocks);
  solbuf.f_bfree       = endian_swap(buf.f_bfree);
  solbuf.f_bavail      = endian_swap(buf.f_bavail);
  solbuf.f_files       = endian_swap(buf.f_files);
  solbuf.f_ffree       = endian_swap(buf.f_ffree);
  solbuf.f_favail      = endian_swap(buf.f_favail);
  solbuf.f_fsid        = endian_swap(buf.f_fsid);
#ifdef linux
  solbuf.f_basetype[0] = '\0';
#else
  int basetype_len = MIN(sizeof(solbuf.f_basetype), sizeof(buf.f_basetype));
  strncpy(solbuf.f_basetype, buf.f_basetype, basetype_len);
  solbuf.f_basetype[basetype_len - 1] = '\0';
#endif
  solbuf.f_flag        = endian_swap(buf.f_flag);
  solbuf.f_namemax     = endian_swap(buf.f_namemax);
#ifdef linux
  solbuf.f_fstr[0]     = '\0';
#else
  int fstr_len = MIN(sizeof(solbuf.f_fstr), sizeof(buf.f_fstr));
  strncpy(solbuf.f_fstr, buf.f_fstr, fstr_len);
  solbuf.f_fstr[fstr_len - 1] = '\0';
#endif

  for (i = 0; i < sizeof(solbuf); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = *(((char*)&solbuf) + i);
      inst->addr++;
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}



/***************************************************************************/


static void GetUIDHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In GetUID Handler\n");
#endif
 
  int            return_value;
  int            lreturn_register;
  int            return_register;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  return_value = getuid();

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}



/***************************************************************************/


static void GetGIDHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In GetGID Handler\n");
#endif
 
  int            return_value;
  int            lreturn_register;
  int            return_register;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  return_value = getgid();

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}



/***************************************************************************/


static void GetgroupsHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Getgroups Handler\n");
#endif
 
  int            pr;
  int            ngroups, i;
  gid_t          gids[NGROUPS_MAX];
  int            return_value;
  int            lreturn_register;
  int            return_register;
  char          *pa;
  unsigned       base;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  ngroups = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  memset(gids, 0, sizeof(gids));
  return_value = getgroups(ngroups, gids);
  if (return_value < 0)
    return_value = errno * -1;

  for (i = 0; i < ngroups; i++)
    {
      pa = (char *)GetMap(inst, proc);
      * (int *)pa = endian_swap(*((int *)&gids + i));
      inst->addr+=sizeof(int);
    }

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;    
}





/***************************************************************************/


static void GetStdioHandler(instance *inst, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In Getgroups Handler\n");
#endif
 
  int            pr;
  int            fd, i, size;
  int            lreturn_register;
  int            return_register;
  int            return_value;
  char          *pa, *buf, *fn;

  lreturn_register = arch_to_log(proc, proc->cwp, 8);
  return_register = proc->intmapper[lreturn_register];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 8)];
  fd = proc->phy_int_reg_file[pr];
 
  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 9)];
  inst->addr = proc->phy_int_reg_file[pr];

  pr = proc->intmapper[arch_to_log(proc, proc->cwp, 10)];
  size = proc->phy_int_reg_file[pr];

  return_value = 0;
  switch (fd)
    {
    case STDIO_STDIN:
      fn = fnstdin;
      break;

    case STDIO_STDOUT:
      fn = fnstdout;
      break;

    case STDIO_STDERR:
      fn = fnstderr;
      break;

    case STDIO_SIMMOUNT_BASE:
      fn = fnsimmount;
      break;

    case STDIO_CWD:
      fn = fncwd;
      break;

    case STDIO_KERNEL:
      fn = fnkernel;
      break;

    default:
      YS__logmsg(proc->proc_id / ARCH_cpus,
		 "Invalid argument %i in GetStdio", fd);
      return_value = -EINVAL;
    }
  
  buf = (char*)malloc(strlen(fn) + 4);
  if (buf == NULL)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Malloc failed in %s:%i",
	       __FILE__, __LINE__);
  memset(buf, 0, strlen(fn) + 4);
  
  if ((ARCH_numnodes == 1) ||
      (fd == STDIO_STDIN) ||
      (fd == STDIO_SIMMOUNT_BASE) ||
      (fd == STDIO_CWD) ||
      (fd == STDIO_KERNEL))
    strcpy(buf, fn);
  else if (ARCH_numnodes <= 10)
    sprintf(buf, "%s%i", fn, proc->proc_id / ARCH_cpus);
  else
    sprintf(buf, "%s%02i", fn, proc->proc_id / ARCH_cpus);

  for (i = 0; i < MIN(strlen(fn) + 4, size); i++)
    {
      pa = (char *)GetMap(inst, proc);
      *pa = buf[i];
      inst->addr++;
    }

  free(buf);

  proc->phy_int_reg_file[return_register] =
    proc->log_int_reg_file[lreturn_register] = return_value;
}





/***************************************************************************/
/* Get Segment Info Handler                                                */
/***************************************************************************/
 
static void GetSegmentInfoHandler(instance *, ProcState *proc)
{
#ifdef COREFILE
  if (YS__Simtime > DEBUG_TIME)
    fprintf(corefile,"traps.cc: In GetSegmentInfo Handler\n");
#endif
  
  int arg, pr, lr;
 
  lr = arch_to_log(proc, proc->cwp, 8);
  pr = proc->intmapper[lr];
  arg = proc->phy_int_reg_file[pr];
 
  switch (arg)
    {
    case SEG_KTEXT_LOW:
      proc->phy_int_reg_file[pr] =
        proc->log_int_reg_file[lr] = proc->ktext_segment_low;
      break;

    case SEG_KTEXT_HIGH:
      proc->phy_int_reg_file[pr] =
        proc->log_int_reg_file[lr] = proc->ktext_segment_high;
      break;
      
    case SEG_KDATA_LOW:
      proc->phy_int_reg_file[pr] =
        proc->log_int_reg_file[lr] = proc->kdata_segment_low;
      break;

    case SEG_KDATA_HIGH:
      proc->phy_int_reg_file[pr] =
        proc->log_int_reg_file[lr] = proc->kdata_segment_high;
      break;

    default:
      YS__warnmsg(proc->proc_id / ARCH_cpus,
		  "Invalid argument %i in GetSegmentInfo", arg);
    }
}






