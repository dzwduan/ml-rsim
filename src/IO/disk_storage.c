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
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Caches/system.h"

#include "IO/scsi_disk.h"

#include "IO/byteswap.h"


/*=========================================================================*/
/* Initialize structure for persistent disk model storage.                 */
/* Determine index and data file numbers based on node, bus and device ID. */
/* Test-open both files and create them if they don't exist. Read index    */
/* into the in-memory hash table.                                          */
/*=========================================================================*/

void DISK_storage_init(SCSI_DISK *pdisk)
{
  DISK_STORAGE_SECTOR *current, *next;
  char      prefix[PATH_MAX], path[PATH_MAX], *dir;
  int       n, file;
  
  for (n = 0; n < DISK_STORAGE_HASH; n++)         /* initialize hash table */
    {
      pdisk->storage.sectors[n].lba = -1;
      pdisk->storage.sectors[n].pba = -1;
      pdisk->storage.sectors[n].next = NULL;
    }


  /* form file names ------------------------------------------------------*/
  strcpy(prefix, "");
  get_parameter("DISK_STORAGE_PREFIX", prefix, PARAM_STRING);

  if ((strlen(prefix) > 0) && (prefix[strlen(prefix)-1] != '/'))
    strcat(prefix, "/");

  dir = getcwd(NULL, PATH_MAX);
  if (prefix[0] != '/')
    sprintf(path, "%s/%s", dir, prefix);
  else
    strcpy(path, prefix);
  free(dir);

  if (path[strlen(path)-1] != '/')
    strcat(path, "/");
  
  sprintf(pdisk->storage.index_file_name, "%sdisk_%02i_%02i_%02i.idx",
	  path,
	  pdisk->scsi_me->scsi_bus->node_id,
	  pdisk->scsi_me->scsi_bus->bus_id,
	  pdisk->scsi_me->dev_id);

  sprintf(pdisk->storage.data_file_name, "%sdisk_%02i_%02i_%02i.dat",
	  path,
	  pdisk->scsi_me->scsi_bus->node_id,
	  pdisk->scsi_me->scsi_bus->bus_id,
	  pdisk->scsi_me->dev_id);	  

  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
             "\nLoading persistent storage for disk %i, SCSI bus %i:\n  %s\n  %s\n",
             pdisk->scsi_me->dev_id, pdisk->scsi_me->scsi_bus->bus_id,
             pdisk->storage.index_file_name, pdisk->storage.data_file_name);


  /* open data file, create if it doesn't exist ---------------------------*/
  file = open(pdisk->storage.data_file_name, O_RDWR, 0x1A4);
  if (file < 0)
    {
      if (errno != ENOENT)
	{
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "DISK: Open file %s failed\n",
		     pdisk->storage.data_file_name);
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "open: %s\n",
		     YS__strerror(errno));
	  exit(1);
	}

      file = open(pdisk->storage.data_file_name, O_RDWR | O_CREAT, 0x1A4);
      if (file < 0)
	{
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "DISK: Open file %s failed\n",
		     pdisk->storage.data_file_name);
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "open: %s\n",
		     YS__strerror(errno));
	  exit(1);
	}
      
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "DISK: Creating data file %s\n",
		 pdisk->storage.data_file_name);
    }
  
  close(file);


  /* open index file, create if it doesn't exist --------------------------*/
  file = open(pdisk->storage.index_file_name, O_RDWR, 0x1A4);
  if (file < 0)
    {
      if (errno != ENOENT)
	{
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "DISK: Open file %s failed\n",
		     pdisk->storage.index_file_name);
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "open: %s\n",
		     YS__strerror(errno));
	  exit(1);
	}

      file = open(pdisk->storage.index_file_name, O_RDWR | O_CREAT, 0x1A4);
      if (file < 0)
	{
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "DISK: Open file %s failed\n",
		     pdisk->storage.index_file_name);
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "open: %s\n",
		     YS__strerror(errno));
	  exit(1);
	}
      
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "DISK: Creating index file %s\n",
		 pdisk->storage.index_file_name);
    }

  
  /* read index file into hash table --------------------------------------*/

  for (n = 0; n < DISK_STORAGE_HASH; n++)
    {
      current = &(pdisk->storage.sectors[n]);
      do
	{
	  if (read(file, current, sizeof(DISK_STORAGE_SECTOR)) <= 0)
	    break;

	  current->lba = swap_word(current->lba);
	  current->pba = swap_word(current->pba);

	  if (current->next != NULL)
	    {
	      current->next = (DISK_STORAGE_SECTOR*)malloc(sizeof(DISK_STORAGE_SECTOR));
	      current->next->lba = -1;
	      current->next->pba = -1;
	      current->next->next = NULL;
	    }
	  current = current->next;
	}
      while (current != NULL);
    }
  
  close(file);
}



/*=========================================================================*/
/* 'Perform' disk read by reading data from file into buffer.              */
/* For every block, find hash table entry and read corresponding data file */
/* block, unless no entry was found.                                       */
/*=========================================================================*/

void DISK_storage_read(SCSI_DISK *pdisk, int sector, int length, char *buf)
{
  DISK_STORAGE_SECTOR *current;
  int                  file, n;
  

  if (buf == NULL)
    return;

  while ((file = open(pdisk->storage.data_file_name, O_RDONLY)) < 0)
    {
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "DISK: Open %s failed (%s)\n",
		 pdisk->storage.data_file_name, YS__strerror(errno));
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
      sleep(FILEOP_RETRY_PERIOD);
    }

#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] %.0f: DISK Storage Read %s %i %i\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     YS__Simtime,
	     pdisk->storage.data_file_name,
	     sector,
	     length);
#endif

  for (n = 0; n < length; n++, buf += SCSI_BLOCK_SIZE, sector++)
    {
      current = &(pdisk->storage.sectors[sector % DISK_STORAGE_HASH]);
      do
	{
	  if (current->lba == sector)
	    break;
	  current = current->next;
	}
      while (current != NULL);

      memset(buf, 0, SCSI_BLOCK_SIZE);
      if (current != NULL)
	{
	  while (pread(file, buf, SCSI_BLOCK_SIZE, current->pba) < 0)
	    {
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "DISK: Read file %s failed\n",
			 pdisk->storage.data_file_name);
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
              sleep(FILEOP_RETRY_PERIOD);
	    }
	}
#ifdef SCSI_DISK_TRACE
      else
	YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		   "[%i:%i] Sector %i not found - returning zeros\n",
		   pdisk->scsi_me->scsi_bus->bus_id+1,
		   pdisk->scsi_me->dev_id,
		   sector);
#endif
    }

  close(file);
}


int block_empty(char *buf)
{
  int n;

  for (n = 0; n < SCSI_BLOCK_SIZE; n++)
    if (buf[n] != 0)
      return(0);

  return(1);
}


/*=========================================================================*/
/* 'Perform' disk write by writing data from buffer into the file.         */
/* For every block, find file block in hash table or allocate new block    */
/* and write data. Always write new hashtable to file.                     */
/*=========================================================================*/

void DISK_storage_write(SCSI_DISK *pdisk, int sector, int length, char *buf)
{
  DISK_STORAGE_SECTOR *current, *next;
  struct stat          stat_buf;
  int                  file, n;


  if (buf == NULL)
    return;

  while ((file = open(pdisk->storage.data_file_name, O_RDWR)) < 0)
    {
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "DISK: Open %s failed (%s)\n",
		 pdisk->storage.data_file_name, YS__strerror(errno));
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
      sleep(FILEOP_RETRY_PERIOD);
    }

#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] %.0f: DISK Storage Write %s %i %i\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     YS__Simtime,
	     pdisk->storage.data_file_name,
	     sector,
	     length);
#endif

  for (n = 0; n < length; n++, buf += SCSI_BLOCK_SIZE, sector++)
    {
      next = &(pdisk->storage.sectors[sector % DISK_STORAGE_HASH]);
      do
	{
	  current = next;
	  if (current->lba == sector)
	    break;
	  next = current->next;
	}
      while (next != NULL);

      if (current->lba == -1)                   /* first element is unused */
	{
	  if (block_empty(buf))
#ifdef SCSI_DISK_TRACE
	    {
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i] Sector %i is all zeros, not stored\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 sector);
	      continue;
	    }
#else
	  continue;
#endif

#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] Sector %i not found - create it\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     sector);
#endif
	  fstat(file, &stat_buf);
	  current->lba = sector;
	  current->pba = stat_buf.st_size;
	  current->next = NULL;
	}
      else if (next == NULL)
	{
	  if (block_empty(buf))
#ifdef SCSI_DISK_TRACE
	    {
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i] Sector %i is all zeros, not stored\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 sector);
	      continue;
	    }
#else
	  continue;
#endif

#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] Sector %i not found - create it\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     sector);
#endif
	  current->next = (DISK_STORAGE_SECTOR*)malloc(sizeof(DISK_STORAGE_SECTOR));
	  fstat(file, &stat_buf);
	  current = current->next;
	  current->lba = sector;
	  current->pba = stat_buf.st_size;
	  current->next = NULL;
	}

      while (pwrite(file, buf, SCSI_BLOCK_SIZE, current->pba) < 0)
	{
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "DISK: Write file %s failed (%s)\n",
		     pdisk->storage.data_file_name, YS__strerror(errno));
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
          sleep(FILEOP_RETRY_PERIOD);
	}
    }

  close(file);


  
  /* write new hash table -------------------------------------------------*/

  while ((file = open(pdisk->storage.index_file_name, O_RDWR)) < 0)
    {
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "DISK: Open %s failed (%s)\n",
		 pdisk->storage.index_file_name, YS__strerror(errno));
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
      sleep(FILEOP_RETRY_PERIOD);
    }

  for (n = 0; n < DISK_STORAGE_HASH; n++)
    {
      current = &(pdisk->storage.sectors[n]);
      do
	{
	  /* switch byte order for external, platform-independent storage */
	  current->lba = swap_word(current->lba);
	  current->pba = swap_word(current->pba);

	  while (write(file, current, sizeof(DISK_STORAGE_SECTOR)) < 0)
	    {
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "DISK: Write file %s failed (%s)\n",
			 pdisk->storage.index_file_name, YS__strerror(errno));
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "Retry in %i secs ...\n", FILEOP_RETRY_PERIOD);
              sleep(FILEOP_RETRY_PERIOD);
	    }

	  /* switch byte order back for internal storage */
	  current->lba = swap_word(current->lba);
	  current->pba = swap_word(current->pba);

	  current = current->next;
	}
      while (current != NULL);
    }
  
  close(file);
}
