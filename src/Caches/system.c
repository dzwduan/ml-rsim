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
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>

#include "Processor/simio.h"
#include "Processor/pagetable.h"
#include "Processor/filedesc.h"
#include "Processor/userstat.h"
#include "sim_main/simsys.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Caches/cache_stat.h"
#include "Caches/ubuf.h"
#include "Caches/syscontrol.h"
#include "IO/addr_map.h"
#include "IO/io_generic.h"
#include "IO/pci.h"
#include "IO/scsi_bus.h"
#include "IO/scsi_controller.h"


#include "IO/realtime_clock.h"
#include "Bus/bus.h"
#include "Memory/mmc.h"
#include "DRAM/dram.h"


int ARCH_numnodes   = 1;
int ARCH_mynodes    = 1;
int ARCH_firstnode  = 0;
int ARCH_cpus       = 1;
int ARCH_ios        = 0;
int ARCH_coh_ios    = 0;

int CPU_CLK_PERIOD  = 5000;                                   /* 200 MHz   */
unsigned PHYSICAL_MEMORY = 512 * 1024 * 1024;                 /* 512 MByte */

int EXIT = 0;


const char *ReqName[REQ_TYPE_MAX] =
{ "",
  "READ        ",
  "WRITE       ",
  "RMW         ",
  "FLUSHC      ",
  "PURGEC      ",
  "RWRITE      ",
  "WRITEBACK   ",
  "READ_SH     ",
  "READ_OWN    ",
  "READ_CURRENT",
  "UPGRADE     ",
  "READ_UC     ",
  "WRITE_UC    ",
  "SWAP_UC     ",
  "REPLY_EXCL  ",
  "REPLY_SHARED",
  "REPLY_UPGRD ",
  "REPLY_UC    ",
  "INVALIDATE  ",
  "SHARED      "
};



/*=========================================================================*/
/* Called by UserMain() to initialize the system. In the current           */
/* simulator, the memory system is composed of L1 cache with write buffer, */
/* L2 cache, system bus (i.e. cluster bus), main memory controller,        */
/* DRAM backend, PCI bridge, zero or more Adaptec PCI SCSI controllers,    */
/* and a realtime clock.                                                   */
/*=========================================================================*/

void SystemInit()
{
  char   phys_mem[50];
  char  *p;
  int    k;

  if (ARCH_numnodes > MAX_NODES)
    YS__errmsg(0,
	       "Too many nodes: %d, only %d supported",
	       ARCH_numnodes, MAX_NODES);

  if (ARCH_cpus > MAX_MEMSYS_PROCS)
    YS__errmsg(0,
	       "Too many CPUs: %d, only %d supported",
	       ARCH_cpus, MAX_MEMSYS_PROCS);

  if (ARCH_cpus != ToPowerOf2(ARCH_cpus))
    {
      YS__warnmsg(0,
		  "ARCH_cpus (%d) is not a power of 2. Use %d instead.",
		  ARCH_cpus ,
		  ARCH_cpus = ToPowerOf2(ARCH_cpus));
      ARCH_cpus = ToPowerOf2(ARCH_cpus);
    }


  memset(phys_mem, 0, sizeof(phys_mem));
  get_parameter("CLKPERIOD", &CPU_CLK_PERIOD,  PARAM_INT);
  get_parameter("MEMORY",    phys_mem, PARAM_STRING);

  PHYSICAL_MEMORY = atoi(phys_mem);
  p = phys_mem + strspn(phys_mem, "0123456789");
  if ((*p == 'k') || (*p == 'K'))
    PHYSICAL_MEMORY *= 1024;
  else if ((*p == 'm') || (*p == 'M'))
    PHYSICAL_MEMORY *= (1024 * 1024);
  else if ((*p == 'g') || (*p == 'G'))
    PHYSICAL_MEMORY *= (1024 * 1024 * 1024);

  if (PHYSICAL_MEMORY == 0)
    PHYSICAL_MEMORY = 512 * 1024 * 1024;
  
  PHYSICAL_MEMORY &= ~(PAGE_SIZE -1);

  AddrMap_init();
  PageTable_init();
  FD_init();
  UserStats_init();

  Cache_init();
  UBuffer_init();
  SysControl_init();

  PCI_init();
  SCSI_init();
  SCSI_cntl_init();

  
  RTC_init();
  PCI_BRIDGE_init();
  
  Bus_init();

  MMC_init();
  DRAM_init();

  if (ARCH_cpus + ARCH_coh_ios > MAX_MEMSYS_SNOOPERS)
    YS__errmsg(0,
	       "Too many coherent bus modules: %d, only %d supported",
	       ARCH_cpus + ARCH_coh_ios, MAX_MEMSYS_SNOOPERS);
}



int  Proc_stat_report (int, int);
void Proc_stat_clear  (int, int);


/*=========================================================================*/
/* StatReport: Prints out statistics for one node collected by the various */
/* modules of the system.                                                  */
/*=========================================================================*/

void StatReport(int node)
{
  int n;

  YS__statmsg(node,
	      "\n\n\n\n========================================================================\n\n\n\n");
  
  if (ARCH_numnodes > 1)
    YS__statmsg(node,
		"STATISTICS FOR NODE %i\n\n", node);

  for (n = 0; n < ARCH_cpus; n++)
    {
      YS__statmsg(node,
		  "------------------------------------------------------------------------\n\n");
      YS__statmsg(node,
		  "PROCESSOR %i STATISTICS\n\n", n);
	  
      if (!Proc_stat_report(node, n))
	continue;

      YS__statmsg(node,
		  "------------------------------------------------------------------------\n\n");
      YS__statmsg(node,
		  "CACHE %i STATISTICS\n\n", n);
      
      Cache_print_params(node);
      Cache_stat_report(node, n);

      YS__statmsg(node,
	      "------------------------------------------------------------------------\n\n");
      YS__statmsg(node,
		  "UNCACHED BUFFER %i STATISTICS\n\n", n);

      UBuffer_print_params(node, n);
      UBuffer_stat_report(node, n);
    }

  YS__statmsg(node,
	  "------------------------------------------------------------------------\n\n");
  YS__statmsg(node,
	      "SYSTEM BUS STATISTICS\n\n");
      
  Bus_print_params(node);
  Bus_stat_report(node);

  YS__statmsg(node,
	  "------------------------------------------------------------------------\n\n");
  YS__statmsg(node,
	      "MEMORY CONTROLLER STATISTICS\n\n");
      
  MMC_print_params(node);
  MMC_stat_report(node);

  YS__statmsg(node,
	      "------------------------------------------------------------------------\n\n");
  YS__statmsg(node,
	      "DRAM STATISTICS\n\n");
      
  DRAM_print_params(node);
  DRAM_stat_report(node);

  YS__statmsg(node,
	      "------------------------------------------------------------------------\n\n");
  YS__statmsg(node,
	  "REALTIME CLOCK STATISTICS\n\n");

  RTC_print_params(node);
  RTC_stat_report(node);
      

  for (n = 0; n < ARCH_scsi_cntrs; n++)
    {
      YS__statmsg(node,
		  "------------------------------------------------------------------------\n\n");
      YS__statmsg(node,
		  "SCSI CONTROLLER %i STATISTICS\n\n", n);

      SCSI_cntl_print_params(node, n);
      SCSI_cntl_stat_report(node, n);
    }


      
  YS__statmsg(node,
	      "------------------------------------------------------------------------\n\n");
  YS__statmsg(node,
	      "USER STATISTICS\n\n");

  UserStats_report(node);
}





/*=========================================================================*/
/* StatClear: Resets collection of statistics collected by the various     */
/* modules of the system.                                                  */
/*=========================================================================*/

void StatClear(int node)
{
  int n;
  
  for (n = 0; n < ARCH_cpus; n++)
    {
      Proc_stat_clear    (node, n);
      Cache_stat_clear   (node, n);
      UBuffer_stat_clear (node, n);
    }

  Bus_stat_clear       (node);
  MMC_stat_clear       (node);
  DRAM_stat_clear      (node);
  RTC_stat_clear       (node);
  for (n = 0; n < ARCH_scsi_cntrs; n++)
    SCSI_cntl_stat_clear (node, n);


  UserStats_clear      (node);
}




/*=========================================================================*/
/* Dump the memory system for debugging experts.                           */
/*=========================================================================*/

void MemoryDumpAll()
{
  int nodeid, n;

  void DumpProcMemQueue(int proc_id, int instcount);


  for (nodeid = ARCH_firstnode;
       nodeid < ARCH_firstnode + ARCH_mynodes;
       nodeid++)
    {
      YS__logmsg(nodeid, "\n================ BEGIN DEBUG INFORMATION ================\n\n");
      for (n = 0; n < ARCH_cpus; n++)
	{
	  DumpProcMemQueue(nodeid * ARCH_cpus + n, 2);
	  Cache_dump(nodeid * ARCH_cpus + n);
	}

      Bus_dump(nodeid);
      MMC_dump(nodeid);
      DRAM_dump(nodeid);
      RTC_dump(nodeid);
      PCI_BRIDGE_dump(nodeid);

      for (n = 0; n < ARCH_scsi_cntrs; n++)
	SCSI_cntl_dump(nodeid, n);
     

      Evlst_dump(nodeid);
      YS__logmsg(nodeid, "\n\n================ END DEBUG INFORMATION ================\n\n");
    }
}




/*=========================================================================*/
/* This print function tries to align "\t" at the specified column         */
/* "PRINT_STAT_ALIGNMENT", therefore making the output more beautiful.     */
/*=========================================================================*/

void print_stat(int node, char *fmt, ...)
{
  static int PRINT_ALIGNMENT[] = { 25, 33, 53, 61, 70, 79 };
  char buf[512], *bp;
  int  it, i, col, len;
  va_list ap;

  va_start(ap, fmt);
  len = vsprintf(buf, fmt, ap);
  va_end(ap);

  if (len <= 0)
    {
      YS__statmsg(node, "\n");
      return;
    }

  /*
   * Find the first '\t', align it into the specified column.
   */
  it = 0;
  bp = buf;
  col = 0;
  while (1)
    {
      for (i = 0; bp[i]; i++, col++)
	{
	  if (bp[i] == '\t')
	    break;
	}
      
      if (bp[i] == 0)
	{
	  YS__statmsg(node, "%s", bp);
	  break;
	}

      bp[i] = 0;
      YS__statmsg(node, "%s", bp);
      bp = bp + i + 1;

      for (; col < PRINT_ALIGNMENT[it]; col++)
	YS__statmsg(node, " ");
      YS__statmsg(node, " ");

      for (; *bp && *bp == ' '; bp++);
      if (*bp == 0)
	break;
      if (it < 5)
	it++;
    }

  YS__statmsg(node, "\n");
}


#define MAXBUFSIZE 1024



/*=========================================================================*/
/* Search the parameter file looking for the name/value pair. Return       */
/* 0 on failure, 1 on success. The value is returned in the address        */
/* provided. If the value is not present in the file, leave the value      */
/* alone.                                                                  */
/*=========================================================================*/

int get_parameter(char *pname, void *value, int type)
{
  extern FILE *pfp;
  extern char *configfile;
  char buf[MAXBUFSIZE], dummy[MAXBUFSIZE], *bp;

  if (strcmp(configfile, "/dev/null") == 0)
    return 0;

  if (pfp == NULL)
    {
      pfp = fopen(configfile, "r");
      if (pfp == NULL)
	{
	  YS__warnmsg(0, "Couldn't open param file %s", configfile);
	  return 0;
	}
    }

  rewind(pfp);

  /*
   * Scan the file, looking for name. Skip lines beginning with a
   * a sharp sign or a newline.
   */
  while ((bp = fgets(buf, BUFSIZ, pfp)) != NULL)
    {
      while (*bp == ' ' || *bp == '\t')
	bp++;

      if (*bp == '\n' || *bp == '#')
	continue;

      if (strncasecmp(pname, bp, strlen(pname)) == 0)
	{
	  switch (type)
	    {
	    case PARAM_INT:
	      sscanf(bp, "%s %d", dummy, (int *)value);
	      break;

	    case PARAM_FLOAT:
	      sscanf(bp, "%s %f", dummy, (float *)value);
	      break;

	    case PARAM_DOUBLE:
	      sscanf(bp, "%s %lf", dummy, (double *)value);
	      break;

	    case PARAM_STRING:
	      sscanf(bp, "%s %s", dummy, (char *) value);
	      break;

	    default:
	      YS__errmsg(0, "get_parameter: Bad type %d", type);
	      break;
	    }
	  return 1;
	}
    }

  return 0;
}



