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

/*=========================================================================*/
/* Filedescriptor management for simulated software, used by simulator     */
/* traps. Maintain fd to filename/offset/flags mapping. Open files only    */
/* when in use. Map table grows dynamically on demand.                     */
/*=========================================================================*/


#define _FILE_OFFSET_BITS 64

#ifdef linux
#  define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "sim_main/util.h"
}

#include "Processor/simio.h"
#include "Processor/filedesc.h"


rsim_filedesc_t *FDs;                        /* dynamic filedescriptor map */
int fd_max  = FD_INCR;                       /* current size of map        */
int fd_free = 0;                             /* lowest available entry     */


/*=========================================================================*/
/* Initialize filedescriptor map with initial FD_INCR size.                */
/* One map per simulator process.                                          */
/*=========================================================================*/

extern "C" void FD_init(void)
{
  FDs = (rsim_filedesc_t*)malloc(fd_max * sizeof(rsim_filedesc_t));
  if (FDs == NULL)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);

  for (int n = 0; n < fd_max; n++)
    memset(FDs[n].name, 0, sizeof(FDs[n].name));
}



/*=========================================================================*/
/* Create and initialize a new entry.                                      */
/* Called when simulated software opens or creates a file.                 */
/* Grow map if fd_free has reached upper  bound. fd_free always points to a*/
/* free entry! If file is to be created, mock up filename as cwd+name or   */
/* just name if it is an absolute path, and open file to create and        */
/* truncate it. Close file again. Convert name into resolved absolute path */
/* and initialize fd entry. Remove 'create' and 'truncate' from flags.     */
/* Move fd_free to next free entry, or to top of map.                      */
/*=========================================================================*/

int FD_create(const char *name, int flags, mode_t mode)
{
  int fd;
  
  if (fd_free == fd_max)
    {
      fd_max += FD_INCR;
      FDs = (rsim_filedesc_t*)realloc(FDs, fd_max * sizeof(rsim_filedesc_t));
      if (FDs == NULL)
	YS__errmsg(0, "Realloc failed in %s:%i", __FILE__, __LINE__);

      for (int n = fd_free; n < fd_max; n++)
	memset(FDs[n].name, 0, sizeof(FDs[n].name));
    }

  if (flags & O_CREAT)
    {
      if (name[0] != '/')
	{
	  getcwd(FDs[fd_free].name, PATH_MAX);
	  strcat(FDs[fd_free].name, "/");
	  strcat(FDs[fd_free].name, name);
	}
      else
	strcpy(FDs[fd_free].name, name);

      do
	{
	  fd = open(FDs[fd_free].name, flags, mode);
	  if ((fd < 0) &&
	      ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT)))
	    {
	      sleep(FILEOP_RETRY_PERIOD);
	      continue;
	    }

	  if (fd < 0)
	    {
	      YS__logmsg(0, "FD Create: Create failed: %s %i\n",
			FDs[fd_free].name, errno);
	      return(-1);
	    }
	}
      while (fd < 0);
      close(fd);
    }

  if (realpath(name, FDs[fd_free].name) == NULL)
    return(-1);

  while ((fd = open(FDs[fd_free].name, flags, mode)) < 0)
    {
      if (errno == EACCES)
	return(-1);

      printf("FD Create: Open %s failed (%s)",
             FDs[fd_free].name, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
             FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  close(fd);

  FDs[fd_free].flags  = (flags & ~O_CREAT & ~O_TRUNC);
  FDs[fd_free].offset = 0LL;

  fd = fd_free;
  while ((fd_free < fd_max) && (FDs[fd_free].name[0] != '\0'))
    fd_free++;

  return(fd);
}




/*=========================================================================*/
/* Open a file for use, called when opened file is accessed.               */
/* Check if fd is valid, open file using name and flags in fd entry, seek  */
/* current offset and  return real filedescriptor.                         */
/*=========================================================================*/

int FD_open(int fd)
{
  int rfd;
  
  if ((fd < 0) || (fd >= fd_max) || (FDs[fd].name[0] == '\0'))
    return(-1);

  while ((rfd = open(FDs[fd].name, FDs[fd].flags)) < 0)
    {
      printf("FD Open: Open %s failed (%s)",
             FDs[fd].name, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
             FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  /*
  if (rfd < 0)
    {
      YS__logmsg(0, "FD Open: Open failed: %s %i\n",
                 FDs[fd].name, errno);
      return(-1);
    }
  */

#if defined(sgi)  || defined(linux)
  lseek64(rfd, FDs[fd].offset, SEEK_SET);
#else
  llseek(rfd, FDs[fd].offset, SEEK_SET);
#endif

  return(rfd);
}



/*=========================================================================*/
/* Change directory using a filedescriptor. Called by many software trap   */
/* handler that use relative pathnames.                                    */
/* Check filedescriptor and change to directory.                           */
/*=========================================================================*/

int FD_fchdir(int fd)
{
  int rc;
  
  if ((fd < 0) || (fd >= fd_max) || (FDs[fd].name[0] == '\0'))
    return(-1);

  while ((rc = chdir(FDs[fd].name)) < 0)
    if ((errno == EINTR) || (errno == EAGAIN) || (errno == ETIMEDOUT))
      {
	printf("FD FChDir: chdir %s failed (%s)",
	       FDs[fd].name, YS__strerror(errno));
	printf("  Retry in %i secs ...\n",
	       FILEOP_RETRY_PERIOD);
	sleep(FILEOP_RETRY_PERIOD);
	continue;
      }
    else
      break;

  return(rc);
}



/*=========================================================================*/
/* LSeek on filedescriptor. Called to update offset of a file.             */
/* Set offset to new value for SEEK_SET, increment or decrement offset for */
/* SEEK_CUR or obtain filesize and add/subtract offset for SEEK_END.       */
/*=========================================================================*/

long long FD_lseek(int fd, long long offset, int whence)
{
  int    rfd;
#ifdef linux
  struct stat sbuf;
#else
  struct stat64 sbuf;
#endif
  
  if ((fd < 0) || (fd >= fd_max) || (FDs[fd].name[0] == '\0'))
    return(-1);

  while ((rfd = open(FDs[fd].name, FDs[fd].flags)) < 0)
    {
      printf("FD LSeek: Open %s failed (%s)",
             FDs[fd].name, YS__strerror(errno));
      printf("  Retry in %i secs ...\n",
             FILEOP_RETRY_PERIOD);
      fflush(stdout);
      sleep(FILEOP_RETRY_PERIOD);
    }

  switch (whence)
    {
    case SEEK_SET:
      FDs[fd].offset = offset;
      break;
    case SEEK_CUR:
      FDs[fd].offset += offset;
      break;
    case SEEK_END:
#ifdef linux
      if (fstat(rfd, &sbuf) < 0)
#else
      if (fstat64(rfd, &sbuf) < 0)
#endif
	return(-1);
      FDs[fd].offset = sbuf.st_size + offset;
      break;
    default:
      errno = EINVAL;
      return(-1);
    }

  close(rfd);

  return(FDs[fd].offset);
}



/*=========================================================================*/
/* Remove filedescriptor from map. Called when a file is closed.           */
/* Mark entry as free (name empty) and adjust fd_free if new entry is lower*/
/*=========================================================================*/

int FD_remove(int fd)
{
  if ((fd < 0) || (fd >= fd_max) || (FDs[fd].name[0] == '\0'))
    return(-1);

  FDs[fd].name[0] = '\0';
  if (fd < fd_free)
    fd_free = fd;
  return(0);
}






