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

#include <string.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/system.h"

#include "Caches/ubuf.h"
#include "Bus/bus.h"
} 

#include "Processor/mainsim.h"
#include "Processor/procstate.h"
#include "Processor/simio.h"
#include "Processor/branchpred.h"
#include "Processor/tlb.h"

static void ConfigureInt       (void *, char *);
static void ConfigureStr       (void *, char *);
static void ConfigureIntKB     (void *, char *);
static void ConfigureDoubleInt (void *, char *);
static void ConfigureBPBType   (void *, char *);
static void ConfigureTLBType   (void *, char *);
static void ConfigureTLBFill   (void *, char *);
static void ConfigureUBufType  (void *, char *);



extern char *configfile;
FILE *pfp = 0;


/************************************************************************/
/* ParseConfigFile: parse the input file passed in for each of the      */
/* parameter types and call the function specified to determine how     */
/* the variable in question should be set                               */
/************************************************************************/

void ParseConfigFile()
{
  static struct config_param 
  {
    const char *key;              // the name of the parameter
    void *dataptr;                // a pointer to the "actual" variable 
    void (*func)(void *, char *); // a function to convert a configuration
                                  // file parameter to an actual value.
                                  // The most common is ConfigureInt, which
                                  // merely calls "atoi" on the configuration
  }
  configparams[] =
  {
    { "activelist",      &MAX_ACTIVE_NUMBER,        ConfigureInt      },
    { "bpbtype",         &BPB_TYPE,                 ConfigureBPBType  },
    { "bpbsize",         &BPB_SIZE,                 ConfigureInt      },
    { "decoderate",      &DECODES_PER_CYCLE,        ConfigureInt      },
    { "fetchqueue",      &FETCH_QUEUE_SIZE,         ConfigureInt      },
    { "fetchrate",       &FETCHES_PER_CYCLE,        ConfigureInt      },
    { "flushrate",       &EXCEPT_FLUSHES_PER_CYCLE, ConfigureInt      },
    { "graduationrate",  &GRADUATES_PER_CYCLE,      ConfigureInt      },
    { "kernel",          &fnkernel,                 ConfigureStr      },
    { "latdiv",          &LAT_ALU_DIV,              ConfigureInt      },
    { "latfconv",        &LAT_FPU_CONV,             ConfigureInt      },
    { "latfdiv",         &LAT_FPU_DIV,              ConfigureInt      },
    { "latflt",          &LAT_FPU_COMMON,           ConfigureInt      },
    { "latfmov",         &LAT_FPU_MOV,              ConfigureInt      },
    { "latfsqrt",        &LAT_FPU_SQRT,             ConfigureInt      },
    { "latint",          &LAT_ALU_OTHER,            ConfigureInt      },
    { "latmul",          &LAT_ALU_MUL,              ConfigureInt      },
    { "latshift",        &LAT_ALU_SHIFT,            ConfigureInt      },
    { "maxaluops",       &MAX_ALU_OPS,              ConfigureInt      },
    { "maxfpuops",       &MAX_FPU_OPS,              ConfigureInt      },
    { "maxmemops",       &MAX_MEM_OPS,              ConfigureInt      },
    { "numaddrs",        &ADDR_UNITS,               ConfigureInt      },
    { "numalus",         &ALU_UNITS,                ConfigureInt      },
    { "numfpus",         &FPU_UNITS,                ConfigureInt      },
    { "nummems",         &MEM_UNITS,                ConfigureInt      },
    { "numnodes",        &ARCH_numnodes,            ConfigureInt      },
    { "numcpus",         &ARCH_cpus,                ConfigureInt      },
    { "rassize",         &RAS_STKSZ,                ConfigureInt      },
    { "repdiv",          &REP_ALU_DIV,              ConfigureInt      },
    { "repflt",          &REP_FPU_COMMON,           ConfigureInt      },
    { "repfmov",         &REP_FPU_MOV,              ConfigureInt      },
    { "repfconv",        &REP_FPU_CONV,             ConfigureInt      },
    { "repfdiv",         &REP_FPU_DIV,              ConfigureInt      },
    { "repfsqrt",        &REP_FPU_SQRT,             ConfigureInt      },
    { "repint",          &REP_ALU_OTHER,            ConfigureInt      },
    { "repmul",          &REP_ALU_MUL,              ConfigureInt      },
    { "repshift",        &REP_ALU_SHIFT,            ConfigureInt      },
    { "shadowmappers",   &MAX_SPEC,                 ConfigureInt      },
    { "storebuffer",     &MAX_STORE_BUF,            ConfigureInt      },
    { "itlbassoc",       &ITLB_ASSOCIATIVITY,       ConfigureInt      },
    { "itlbsize",        &ITLB_SIZE,                ConfigureInt      },
    { "itlbtype",        &ITLB_TYPE,                ConfigureTLBType  },
    { "itlbfill",        &ITLB_HARDWARE_FILL,       ConfigureTLBFill  },
    { "itlbtag",         &ITLB_TAGGED,              ConfigureInt      },
    { "dtlbassoc",       &DTLB_ASSOCIATIVITY,       ConfigureInt      },
    { "dtlbsize",        &DTLB_SIZE,                ConfigureInt      },
    { "dtlbtype",        &DTLB_TYPE,                ConfigureTLBType  },
    { "dtlbfill",        &DTLB_HARDWARE_FILL,       ConfigureTLBFill  },
    { "dtlbtag",         &DTLB_TAGGED,              ConfigureInt      }
  };

  char   buf[1024], *bp;
  char   buf1[1000], buf2[1000];
  int    num_entries;
  int    i;

  if (strcmp(configfile, "/dev/null") == 0)
    return;

  if (pfp == 0)
    {
      if (!(pfp = fopen(configfile, "r")))
	{
	  fprintf(stderr,
		  "Couldn't open configuration file %s. Use default: %s.\n",
		  configfile, "rsim_params");
	  
	  if (!(pfp = fopen("rsim_params", "r")))
	    {
	      fprintf(stderr, "Couldn't open rsim_params either.\n");
	      return;
	    }
	  else
            strcpy(configfile, "rsim_params");
	}
    }
  else
    rewind(pfp);

  num_entries = sizeof(configparams) / sizeof(struct config_param);

  while ((bp = fgets(buf, 1024, pfp)) != NULL)
    {
      while (*bp == ' ' || *bp == '\t')
	bp++;

      if (*bp == '\n' || *bp == '#')
	continue;

      if (sscanf(bp, "%s %s", buf1, buf2) == 2)
	{
	  for (i = 0; i < num_entries; i++)
	    {
	      if (strcasecmp(configparams[i].key, buf1) == 0)
		{
		  (*(configparams[i].func)) (configparams[i].dataptr, buf2);
		  break;
		}
	    }
	  /*
	  if (i == num_entries)
            fprintf(stderr, "Unknown configuration option %s\n", buf1);
	    */
	}
      else
	fprintf(stderr, "ill-formatted line(%s) in configuration file (%s).", 
		bp, configfile);
    }
}


  
static void ConfigureInt(void *dp, char *s)
{
  *((int *)dp) = atoi(s);
}
 
 
static void ConfigureStr(void *dp, char *s)
{
  strcpy((char*)dp, s);
}
 
 
 
static void ConfigureIntKB(void *dp, char *s)
{
  *((int *)dp) = atoi(s) * 1024;
}
 
 
 
static void ConfigureDoubleInt(void *dp, char *s)
{
  int tmp;
  ConfigureInt(&tmp,s);
  *((double *)dp) = tmp;
}
 

static void ConfigureTLBType(void *dp, char *s)
{
  if (strncasecmp(s, "fully_assoc", 11) == 0)
    *((int *)dp) = TLB_FULLY_ASSOC;
  else if (strncasecmp(s, "set_assoc", 9) == 0)
    *((int *)dp) = TLB_SET_ASSOC;
  else if (strncasecmp(s, "direct", 6) == 0)
    *((int *)dp) = TLB_DIRECT_MAPPED;
  else if (strncasecmp(s, "perfect", 7) == 0)
    *((int *)dp) = TLB_PERFECT;
  else
    {
      fprintf(stderr, "Unknown TLB type '%s'\n", s);
      exit(1);
    }
}
 

static void ConfigureTLBFill(void *dp, char *s)
{
  if (strncasecmp(s, "hardware", 8) == 0)
    *((int *)dp) = 1;
  else
    {
      fprintf(stderr, "Unknown TLB fill type '%s'\n", s);
      exit(1);
    }
}
 

static void ConfigureBPBType(void *dp, char *s)
{
  if (strcasecmp(s, "2bit") == 0)
    *((bptype *) dp) = TWOBIT;
  else if (strcasecmp(s, "2bitagree") == 0)
    *((bptype *) dp) = TWOBITAGREE;
  else if (strcasecmp(s, "static") == 0)
    *((bptype *) dp) = STATIC;
  else
    {
      fprintf(stderr, "Unknown BPB type %s\n", s);
      exit(1);
    }
}
