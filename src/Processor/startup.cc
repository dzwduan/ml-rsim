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
#include <time.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <malloc.h>
#include <errno.h>
#include <locale.h>

#ifndef UNELF
#  ifdef linux
#    include <libelf/libelf.h>
#  else
#    include <libelf.h>
#  endif
#else 
#  include "predecode/unelf.h"
#endif

extern "C"
{
#include "sim_main/simsys.h"
}

#include "Processor/procstate.h"
#include "Processor/pagetable.h"
#include "Processor/mainsim.h"
#include "Processor/simio.h"
#include "Processor/branchpred.h"
#include "Processor/fastnews.h"
#include "Processor/active.hh"
#include "Processor/tagcvt.hh"
#include "Processor/procstate.hh"
#include "Processor/endian_swap.h"

#include "../lamix/mm/mm.h"


/* 
 * In the SPARC, the min stack frame is 16*4 for spills (will this become
 * 16*8 with 64-bit int regs?) plus 1*4 for aggregate return pointer plus
 * 6*4 for callee save space plus 1*4 for an additional save pointer 
 * Total = 16 * 4 + 1 * 4 + 6 * 4 + 1 * 4 = 0x60
 */
#define MINIMUM_STACK_FRAME_SIZE 0x40



/***************************************************************************/
/* startup : startup procedure which reads the application binary and      */
/*         : suitably initializes the stack, heap and data segments        */
/*         : machine-format specific                                       */
/***************************************************************************/

int startup(char *kernel, char **args, char **env, ProcState *proc)
{
  int           fd;
  char         *cp;
  char        **cpp;
  int           i, n, state;
  long          argcount, envcount, cnt;
  char         *text_chunk, *data_chunk, *stack_chunk, *args_chunk;
  char         *chunk;
  unsigned long text_start, data_start, args_start, start;
  unsigned      text_size, data_size, args_size;
  unsigned      old_size, size, pg;
  struct stat   sbuf;
  struct tm    *t;

#ifndef UNELF
  int         datacnt;
  Elf        *elf;
  Elf32_Ehdr *ehdr;
  Elf32_Phdr *phdr;
  Elf32_Shdr *shdr;
  Elf_Scn    *scn;
  Elf_Data   *data;

  if (stat(kernel, &sbuf) != 0)
    YS__errmsg(proc->proc_id / ARCH_cpus,
               "Error accessing kernel image %s: %s\n",
               kernel, strerror(errno));

  cp = getcwd(NULL, MAXPATHLEN);
  YS__statmsg(proc->proc_id / ARCH_cpus,
              "\nKernel image: %s%s\n",
              kernel[0] == '/' ? "" : cp, kernel);
  YS__logmsg(proc->proc_id / ARCH_cpus,
             "\nLoading kernel image:\n  %s%s\n",
             kernel[0] == '/' ? "" : cp, kernel);
   free(cp);

  setlocale(LC_TIME, "C");
  t = localtime(&sbuf.st_ctime);
  YS__statmsg(proc->proc_id / ARCH_cpus,
              "  Built on %02i/%02i/%02i at %02i:%02i:%02i\n",
              t->tm_mon+1, t->tm_mday, 1900 + t->tm_year,
              t->tm_hour, t->tm_min, t->tm_sec);
  YS__statmsg(proc->proc_id / ARCH_cpus,
              "  Size: %i bytes\n", sbuf.st_size);

  if ((fd = open(kernel, O_RDONLY)) < 0)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Error in startup: couldn't open file %s: %s", kernel,
               strerror(errno));
  
  if (elf_version(EV_CURRENT) == EV_NONE)
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "ELF out of date"); /* library out of date */


  elf = elf_begin(fd, ELF_C_READ, NULL);
  ehdr = elf32_getehdr(elf);
  phdr = elf32_getphdr(elf);

  if ((elf == NULL) ||
      (ehdr == NULL) ||
      (phdr == NULL))     // failure to read elf info
    YS__errmsg(proc->proc_id / ARCH_cpus,
	       "Error reading elf headers from %s", kernel);


  text_chunk = data_chunk = NULL;
  state = 0;

  
  //-------------------------------------------------------------------------
  // we want to find out where the elf header will be located

  start = DOWN_TO_PAGE(phdr->p_vaddr);
  old_size = 0;
  size = PAGE_SIZE;             // start small and build it up with reallocs
  chunk = (char *) malloc(size);
  memset(chunk, 0, size);

  for (cnt = 1, scn = NULL; (scn = elf_nextscn(elf, scn)) != NULL; cnt++)
    {
      if ((shdr = elf32_getshdr(scn)) == NULL)
	YS__errmsg(proc->proc_id / ARCH_cpus,
		   "ELF prob reading section %d of file %s", cnt, kernel);

      if ((void *) shdr->sh_addr == NULL)
	continue;


      if (!(shdr->sh_flags & SHF_EXECINSTR) &&
	  (shdr->sh_flags & SHF_WRITE) &&
	  (state == 0))
	{
	  text_start = UP_TO_PAGE(start);
	  text_size  = UP_TO_PAGE(size);
	  text_chunk = chunk;

	  old_size = 0;
	  start = DOWN_TO_PAGE(shdr->sh_addr);
	  size = PAGE_SIZE;
	  chunk = (char*)malloc(size);
          memset(chunk, 0, size);

	  state = 1;
	}

      for (datacnt = 0, data = NULL; 
	   (data = elf_getdata(scn, data)) != NULL; datacnt++)
	{
	  if ((shdr->sh_addr + data->d_off - start + data->d_size) > size)
	    {
	      old_size = size;
	      size = UP_TO_PAGE((int)(shdr->sh_addr + data->d_off - 
				      start + data->d_size));
	      chunk = (char*)realloc(chunk, size);
	      memset(chunk + old_size, 0, size - old_size);
	    }

	  if (data->d_buf)
	    {
#if 0
#ifdef COREFILE
	      fprintf(corefile,
		      "Startup: Copying %d bytes from elf section %d,"
		      " data chunk %d\n", data->d_size, cnt, datacnt);
#endif
#endif
	      memcpy(chunk + shdr->sh_addr + data->d_off - start,
		     data->d_buf, data->d_size);
	    }
	  else
	    {
#if 0
#ifdef COREFILE
	      fprintf(corefile,
		      "Startup: Zeroing %d bytes of elf section %d,"
		      " data chunk %d\n", data->d_size, cnt, datacnt);
#endif
#endif
	      memset(chunk + shdr->sh_addr + data->d_off - start, 
		     0, data->d_size);
	    }
	}
    }
  


  data_start = UP_TO_PAGE(start);
  data_size  = UP_TO_PAGE(size);
  data_chunk = chunk;
  
  
  /* start out at the entry point */
  proc->pc  = (unsigned)ehdr->e_entry;
  proc->npc = proc->pc + 4;
  proc->fetch_pc  = proc->pc;


  elf_end(elf);
  close(fd);   /* we've read it all now */


  //=========================================================================

#else
  YS_errmsg(proc->proc_id / ARCH_cpus,
	    "Unable to load ELF files on this system");
#endif



  //=========================================================================
  // Insert the text pages into the processor hash table

  char *chunkptr   = text_chunk;
 
  for (pg = (unsigned)text_start;
       pg < text_start + text_size;
       pg += PAGE_SIZE, chunkptr += PAGE_SIZE)
    proc->PageTable->insert(pg, chunkptr);

  proc->ktext_segment_low  = (unsigned)text_start;
  proc->ktext_segment_high = (unsigned)text_start + text_size;

  
  //=========================================================================
  // Insert the data pages into the processor hash table

  chunkptr   = data_chunk;

  for (pg = (unsigned)data_start;
       pg < data_start + data_size;
       pg += PAGE_SIZE, chunkptr += PAGE_SIZE)
    proc->PageTable->insert(pg, chunkptr);


  proc->kdata_segment_low  = (unsigned)data_start;
  proc->kdata_segment_high = (unsigned)data_start + data_size;

  

  //-------------------------------------------------------------------------
  // 1 chunk for the initial stack -- it will expand as needed
  //
  //  Figure out how many bytes are needed to hold the arguments on
  //  the new stack that we are building.  Also count the number of
  //  arguments, to become the argc that the new "main" gets called with.

  YS__logmsg(proc->proc_id / ARCH_cpus, "\nStartup command line:\n  ");
  for (size = 0, i = 0; args[i] != NULL; i++)
    {
      size += strlen(args[i]) + 1;
      YS__logmsg(proc->proc_id / ARCH_cpus, "%s ", args[i]);
    }
  argcount = i;
  YS__logmsg(proc->proc_id / ARCH_cpus,"\n\n");
  
  for (i = 0; env[i] != NULL; i++)
    size += strlen(env[i]) + 1;
  envcount = i;
 
  args_size = size +
    (argcount + envcount + 3) * sizeof(char*);
  args_size = (args_size + PAGE_SIZE - 1) & ~(PAGE_SIZE-1);
  args_start = data_start + data_size;

  args_chunk = (char*)malloc(args_size);
  memset(args_chunk, 0, args_size);
  chunkptr = args_chunk;
  for (pg = (unsigned)args_start;
       pg < args_start + args_size;
       pg += PAGE_SIZE, chunkptr += PAGE_SIZE)
    proc->PageTable->insert(pg, chunkptr);

  proc->kdata_segment_high = (unsigned)args_start + args_size;
  
  //
  //  The arguments will get copied starting at "cp", and the argv
  //  pointers to the arguments (and the argc value) will get built
  //  starting at "cpp".  The value for "cpp" is computed by subtracting
  //  off space for the number of arguments (plus 2, for the terminating
  //  NULL and the argc value) times the size of each (4 bytes), and
  //  then rounding the value *down* to a double-word boundary.

  cp = (char *)(args_chunk + (argcount+envcount+2) * sizeof(char*));
  cpp = (char **)(args_chunk);

  for (i = 0; i < argcount; i++)          // copy each argument and set argv's
    {
      *cpp++ = (char*) endian_swap((unsigned long)
		        (args_start + cp - args_chunk));
      strcpy(cp, args[i]);
      cp += strlen(cp) + 1;
    }
  *cpp++ = NULL;                        // the last argv is a NULL pointer

  for (i = 0; i < envcount; i++)        // copy each env var and set envv's
    {
      *cpp++ = (char*) endian_swap((unsigned long)
                        (args_start + cp - args_chunk));
      strcpy(cp, env[i]);
      cp += strlen(cp) + 1;
    }
  *cpp++ = NULL;                        // the last envv is a NULL pointer

  cpp = (char **)(args_chunk + args_size - (sizeof(char*)));
  *cpp = (char*)endian_swap((unsigned long)args_start);
  

  return 0;
}


