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
#include <math.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Caches/system.h"

#include "IO/scsi_disk.h"



int  DISK_cache_newsegment    (SCSI_DISK*);
void DISK_cache_touch_segment (SCSI_DISK*, int);



/*===========================================================================*/
/* Create and initialize disk cache. Allocate N segments and reset start/end */
/* address, LRU counter and write flags.                                     */
/*===========================================================================*/

void DISK_cache_init(SCSI_DISK *pdisk)
{
  int n;

  pdisk->cache = (DISK_CACHE_SEGMENT*)malloc(sizeof(DISK_CACHE_SEGMENT) *
					     pdisk->cache_segments);

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      pdisk->cache[n].start_block = 0;
      pdisk->cache[n].end_block   = 0;
      pdisk->cache[n].lru         = 0;
      pdisk->cache[n].write       = 0;
      pdisk->cache[n].committed   = 0;
    }
}



/*===========================================================================*/
/* Return segment with the highest sector number in the range start_block:   */
/* start_block + length. For reads look only through read segments. Possibly */
/* return segment that does not match but to which the range can be          */
/* appended. For writes, check only committed write segments that overlap    */
/* with the range. If none is found, get a new segment.                      */
/*===========================================================================*/

int DISK_cache_getsegment(SCSI_DISK *pdisk, int start_block,
			  int length, int write)
{
  int n;
  int segment_size = (pdisk->cache_size * 1024) /
    (pdisk->cache_segments * SCSI_BLOCK_SIZE);


  /* read request can match a segment that ends at start_block-1 ------------*/
  if (!write)
    for (n = 0; n < pdisk->cache_segments; n++)
      {
	if ((start_block >= pdisk->cache[n].start_block) &&
	    (start_block < pdisk->cache[n].end_block) &&
	    (!pdisk->cache[n].write))
	  {
	    DISK_cache_touch_segment(pdisk, n);
	    return(n);
	  }

	if ((start_block >= pdisk->cache[n].start_block) &&
	    (start_block <= pdisk->cache[n].end_block) &&
	    (pdisk->cache[n].end_block - pdisk->cache[n].start_block <
	     segment_size) &&
	    (!pdisk->cache[n].write))
	  {
	    DISK_cache_touch_segment(pdisk, n);
	    return(n);
	  }

	if ((start_block >= pdisk->cache[n].start_block) &&
	    (start_block < pdisk->cache[n].end_block) &&
	    (start_block + length > pdisk->cache[n].end_block) &&
	    (!pdisk->cache[n].write))
	  {
	    DISK_cache_touch_segment(pdisk, n);
	    length -= (pdisk->cache[n].end_block - start_block);
	    start_block = pdisk->cache[n].end_block;
	    n = -1;
	    continue;	    
	  }

      }
  else
    /* write request can match only overlapping, committed write segments ---*/
    for (n = 0; n < pdisk->cache_segments; n++)
      {
	if ((start_block >= pdisk->cache[n].start_block) &&
	    (start_block < pdisk->cache[n].end_block) &&
	    (pdisk->cache[n].write && pdisk->cache[n].committed))
	  {
	    DISK_cache_touch_segment(pdisk, n);
	    return(n);
	  }
      }

  n = DISK_cache_newsegment(pdisk);       /* no segment found: get a new one */
  DISK_cache_touch_segment(pdisk, n);     /* make new segment the youngest   */

  return(n);
}





/*===========================================================================*/
/* Get a new segment: look for oldest non-write segment and return its ID    */
/*===========================================================================*/

int DISK_cache_newsegment(SCSI_DISK *pdisk)
{
  int n, age = 0;

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      if ((age == pdisk->cache[n].lru) &&
	  (pdisk->cache[n].write))
	{
	  age++;
	  n = -1;
	  continue;
	}

      if (age == pdisk->cache[n].lru)
	{
	  pdisk->cache[n].write       = 0;
	  pdisk->cache[n].committed   = 0;
	  pdisk->cache[n].start_block = 0;
	  pdisk->cache[n].end_block   = 0;
	  return(n);
	}
    }

  return(-1);     /* shouldn't get here */
}



/*===========================================================================*/
/* Determine cache hit for a read:                                           */
/* look for segment that contains start_block, return full_hit if range is   */
/* is completely within that segment, otherwise adjust start_block and       */
/* length and search again. Return partial hit if a segment contains only    */
/* a subset of the block range, or return miss otherwise.                    */
/*===========================================================================*/

int DISK_cache_hit(SCSI_DISK *pdisk, int start_block, int length)
{
  int n, rc = DISK_CACHE_MISS;

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      if ((pdisk->cache[n].write) && (!pdisk->cache[n].committed))
	continue;

      if ((start_block >= pdisk->cache[n].start_block) &&
	  (start_block < pdisk->cache[n].end_block) &&
	  (start_block + length > pdisk->cache[n].end_block))
	{
	  rc = DISK_CACHE_HIT_PARTIAL;
	  length -= (pdisk->cache[n].end_block - start_block);
	  start_block = pdisk->cache[n].end_block;
	  n = -1;
	  continue;
	}

      if ((start_block >= pdisk->cache[n].start_block) &&
	  (start_block < pdisk->cache[n].end_block) &&
	  (start_block + length <= pdisk->cache[n].end_block))
	return(DISK_CACHE_HIT_FULL);	
    }
  
  return(rc);
}




/*===========================================================================*/
/* Insert blocks into cache segment                                          */
/* append blocks if first block is at end of segment;                        */
/* append partial if start is within segment limits but length exceeds       */
/* last block; overwrite otherwise                                           */
/* make segment the youngest segment by setting its LRU value to the highest */
/* and decrementing the LRU value of all other segments with values higher   */
/* then the old age of that segment                                          */
/*===========================================================================*/

int DISK_cache_insert(SCSI_DISK *pdisk, int start_block, int length,
		       int segment)
{
  int n;
  int segment_size = (pdisk->cache_size * 1024) /
    (pdisk->cache_segments * SCSI_BLOCK_SIZE);
  int segment_full = pdisk->cache[segment].end_block -
    pdisk->cache[segment].start_block;

  DISK_cache_touch_segment(pdisk, segment);
  if (segment_full + length > segment_size)
    length = segment_size - segment_full;


  /* append to end of segment -----------------------------------------------*/
  if (start_block == pdisk->cache[segment].end_block)
    pdisk->cache[segment].end_block += length;

  /* overwrite end of segment -----------------------------------------------*/
  else if ((start_block >= pdisk->cache[segment].start_block) &&
	   (start_block < pdisk->cache[segment].end_block) &&
	   (start_block + length > pdisk->cache[segment].end_block))
    pdisk->cache[segment].end_block = start_block + length;

  /* overwrite entire segment -----------------------------------------------*/
  else
    {
      pdisk->cache[segment].start_block = start_block;
      pdisk->cache[segment].end_block = start_block + length;
    }

  pdisk->cache[segment].committed = 1;
  
  return(length);
}




/*===========================================================================*/
/* touch an entire block number range: look for segment containing           */
/* start_block and touch it, adjust start_block and length and repeat search */
/*===========================================================================*/

void DISK_cache_touch(SCSI_DISK *pdisk, int start_block, int length)
{
  int n;

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      if ((start_block >= pdisk->cache[n].start_block) &&
	  (start_block < pdisk->cache[n].end_block) &&
	  (start_block + length > pdisk->cache[n].end_block))
	{
	  DISK_cache_touch_segment(pdisk, n);
	  length -= (pdisk->cache[n].end_block - start_block);
	  start_block = pdisk->cache[n].end_block;
	  n = -1;
	  continue;
	}

      if ((start_block >= pdisk->cache[n].start_block) &&
	  (start_block < pdisk->cache[n].end_block) &&
	  (start_block + length <= pdisk->cache[n].end_block))
	DISK_cache_touch_segment(pdisk, n);
    }
}



/*===========================================================================*/
/* Update LRU counters so that this segment is the youngest.                 */
/* Set counter to the maximum value, and decrement all counters that are     */
/* greater than that counters old value.                                     */
/*===========================================================================*/

void DISK_cache_touch_segment(SCSI_DISK *pdisk, int segment)
{
  int n, old_age = pdisk->cache[segment].lru;

  for (n = 0; n < pdisk->cache_segments; n++)
    if (pdisk->cache[n].lru > old_age)
      pdisk->cache[n].lru--;

  pdisk->cache[segment].lru = pdisk->cache_segments - 1;
}




/*===========================================================================*/
/* Insert write data into the cache: check if number of write segments       */
/* exceed maximum number, otherwise get a new segment, insert data and set   */
/* set write flag. Repeat until all data is written or out of write segments */
/*===========================================================================*/

int DISK_cache_write(SCSI_DISK *pdisk, int start_block, int length)
{
  int n, writes, current_length = 0, rc;

  writes = 0;

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      if (pdisk->cache[n].write)
	writes++;
    }

  if (writes >= pdisk->cache_write_segments)
    return(0);

  do
    {
      n = DISK_cache_newsegment(pdisk);
      rc = DISK_cache_insert(pdisk, start_block, length, n);
      length -= rc;
      start_block += rc;
      current_length += rc;

      writes++;
      pdisk->cache[n].write = 1;
      pdisk->cache[n].committed = 0;
    }
  while ((writes < pdisk->cache_write_segments) && (length > 0));
  
  return(current_length);
}




/*===========================================================================*/
/* Find first committed write segment and return start block/length and      */
/* segment number.                                                           */
/*===========================================================================*/

int DISK_cache_getwsegment(SCSI_DISK *pdisk, int *start_block, int *length)
{
  int n;

  for (n = 0; n < pdisk->cache_segments; n++)
    if ((pdisk->cache[n].write == 1) &&
	(pdisk->cache[n].committed == 1) &&
	(*start_block <= pdisk->cache[n].start_block) &&
	(*start_block + *length >= pdisk->cache[n].end_block))
      {
	*start_block = pdisk->cache[n].start_block;
	*length      = pdisk->cache[n].end_block - pdisk->cache[n].start_block;
	DISK_cache_touch_segment(pdisk, n);
	return(n);
      }

  return(-1);
}




/*===========================================================================*/
/* Commit a write request: find first write segment that contains            */
/* start_block, set the committed-flag, adjust start_block and length and    */
/* repeat until length is zero. In addition, invalidate all read segments    */
/* that overlap the write request.                                           */
/*===========================================================================*/

void DISK_cache_commit_write(SCSI_DISK *pdisk, int start_block, int length)
{
  int s;

  for (s = 0; s < pdisk->cache_segments; s++)
    {
      if (pdisk->cache[s].write)
	continue;

      if ((start_block <= pdisk->cache[s].start_block) &&
	  (start_block + length > pdisk->cache[s].start_block) &&
	  (start_block + length < pdisk->cache[s].end_block))
	{
	  pdisk->cache[s].start_block = start_block + length;
	  continue;
	}

      if ((start_block > pdisk->cache[s].start_block) &&
	  (start_block + length <= pdisk->cache[s].end_block))
	{
	  pdisk->cache[s].start_block = 0;
	  pdisk->cache[s].end_block   = 0;
	  pdisk->cache[s].committed   = 0;
	  pdisk->cache[s].lru = 0;
	  continue;
	}

      if ((start_block > pdisk->cache[s].start_block) &&
	  (start_block <= pdisk->cache[s].end_block) &&
	  (start_block + length > pdisk->cache[s].start_block) &&
	  (start_block + length >= pdisk->cache[s].end_block))
	{
	  pdisk->cache[s].end_block = start_block;
	  continue;
	}

      if ((start_block <= pdisk->cache[s].start_block) &&
	  (start_block + length >= pdisk->cache[s].end_block))
	{
	  pdisk->cache[s].start_block = 0;
	  pdisk->cache[s].end_block   = 0;
	  pdisk->cache[s].committed   = 0;
	  pdisk->cache[s].lru = 0;
	  continue;
	}  
    }
  
  s = 0;
  do
    {
      if ((pdisk->cache[s].start_block == start_block) &&
	  (pdisk->cache[s].write == 1))
	{
	  pdisk->cache[s].committed = 1;
	  start_block = pdisk->cache[s].end_block;
	  length -= pdisk->cache[s].end_block - pdisk->cache[s].start_block;
	  s = 0;
	  continue;
	}
      s++;
    }
  while (s < pdisk->cache_segments);
}




/*===========================================================================*/
/* Complete a write operation by clearing the write bit.                     */
/*===========================================================================*/

void DISK_cache_complete_write(SCSI_DISK *pdisk, int segment)
{
  pdisk->cache[segment].write = 0;
}




/*===========================================================================*/
/* Check if segment is full.                                                 */
/*===========================================================================*/

int DISK_cache_segment_full(SCSI_DISK *pdisk, int segment)
{
  return(pdisk->cache[segment].end_block - pdisk->cache[segment].start_block >=
	 (pdisk->cache_size*1024) / (pdisk->cache_segments*SCSI_BLOCK_SIZE));
}
