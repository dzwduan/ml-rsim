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

#ifndef __RSIM_SCSI_DISK_H__
#define __RSIM_SCSI_DISK_H__


/*-------------------------------------------------------------------------*/
/* SCSI disk module definitions and control structures                     */
/*-------------------------------------------------------------------------*/


#include "Caches/lqueue.h"
#include "IO/scsi_bus.h"


/* uncomment to compile disk model with debugging output */
/*
#define SCSI_DISK_TRACE
*/

/*-------------------------------------------------------------------------*/
/* disk seek time computation methods                                      */

typedef enum
{
  DISK_SEEK_NONE,                          /* instantaneous seeks          */
  DISK_SEEK_CONST,                         /* constant seek time - average */
  DISK_SEEK_LINE,                          /* 3-point line interpolation   */
  DISK_SEEK_CURVE                          /* 3-point curve interpolation  */
} DISK_SEEK_METHOD;



/*-------------------------------------------------------------------------*/
/* disk states and cache states                                            */

#define DISK_IDLE      0
#define DISK_SEEK      2
#define DISK_TRANSFER  3

#define DISK_CACHE_MISS         0
#define DISK_CACHE_HIT_PARTIAL  1
#define DISK_CACHE_HIT_FULL     2



/*-------------------------------------------------------------------------*/
/* a disk cache segment, cache is an array of these                        */

typedef struct
{
  int   start_block;                       /* start/end block              */
  int   end_block;
  int   write;                             /* is it a write-segment ?      */
  int   committed;                         /* is the write committed ?     */
  int   lru;                               /* LRU counter                  */
} DISK_CACHE_SEGMENT;



/*-------------------------------------------------------------------------*/
/* control structures for physical disk storage (file)                     */

typedef struct _disk_storage_sector_       /* a hash table entry           */
{
  int    lba;                              /* logical (disk) number        */
  int    pba;                              /* physical (file) block number */
  struct _disk_storage_sector_ *next;      /* next table entry             */
} DISK_STORAGE_SECTOR;


#define DISK_STORAGE_HASH 1024             /* storage hash table size      */

typedef struct
{
  DISK_STORAGE_SECTOR sectors[DISK_STORAGE_HASH];
  char index_file_name[PATH_MAX];
  char data_file_name[PATH_MAX];
} DISK_STORAGE;



/*-------------------------------------------------------------------------*/
/* disk control structure                                                  */

typedef struct
{
  int                  dev_id;             /* SCSI bus device ID           */
  SCSI_DEV            *scsi_me;            /* pointer to SCSI bus interface*/

  /* disk parameters ------------------------------------------------------*/

  char                 name[64];           /* disk model description       */

  double               seek_single;        /* single track seek time       */
  double               seek_avg;           /* average seek time            */
  double               seek_full;          /* full disk seek time          */
  DISK_SEEK_METHOD     seek_method;        /* method to compute seek time  */

  double               write_settle;           /* write settle time        */
  double               head_switch;            /* head switch time         */
  double               cntl_overhead;          /* per-request overhead     */

  int                  ticks_per_ms;           /* core clock ticks per ms  */
  
  int                  rpm;                    /* rotational speed         */
  int                  cylinders;              /* disk configuration       */
  int                  heads;
  int                  sectors;
  int                  cylinder_skew;
  int                  track_skew;
  
  int                  request_queue_size;     /* SCSI interface queue szs */
  int                  response_queue_size;
  int                  cache_size;             /* cache size in KBytes     */
  int                  cache_segments;         /* number of cache segments */
  int                  cache_write_segments;   /* number of write segments */

  double               buffer_full_ratio;
  double               buffer_empty_ratio;

  int                  prefetch;               /* prefetch after reads ?   */
  int                  fast_writes;            /* enable fast writes ?     */
  
  /* disk state -----------------------------------------------------------*/

  int                  state;
  int                  current_cylinder;
  int                  current_head;
  int                  previous_cylinder;
  int                  seek_target_sector;
  int                  start_offset;
  double               seek_start_time;
  double               seek_end_time;
  
  SCSI_REQ            *current_req;
  
  EVENT               *request_event;
  EVENT               *seek_event;
  EVENT               *sector_event;

  LinkQueue            inqueue;
  LinkQueue            outqueue;

  DISK_CACHE_SEGMENT  *cache;

  DISK_STORAGE         storage;

  /* statistics -----------------------------------------------------------*/

  int                  requests_read;
  int                  requests_write;
  int                  requests_other;
  int                  cache_hits_full;
  int                  cache_hits_partial;
  int                  blocks_read;
  int                  blocks_written;
  int                  blocks_read_media;
  int                  blocks_written_media;
  double               seek_time;
  double               transfer_time;

  /* disk configuration ---------------------------------------------------*/

  SCSI_INQUIRY_DATA     inquiry_data;
  SCSI_MODE_SENSE_DATA  mode_sense_data;
  SCSI_EXT_SENSE_DATA  *sense_data;
  
} SCSI_DISK;




/*-------------------------------------------------------------------------*/
/* disk model routines                                                     */

void    SCSI_disk_init            (SCSI_BUS*, int);

/* mechanical model routines */
int     DISK_sector_at_time       (SCSI_DISK*, int head, int, double);
int     DISK_sector_to_cylinder   (SCSI_DISK*, int);
int     DISK_sector_to_head       (SCSI_DISK*, int);
double  DISK_seek_time            (SCSI_DISK*, int, int, int);
int     DISK_seek_distance        (SCSI_DISK*, double, int);
double  DISK_rotation_time        (SCSI_DISK*, int);
void    DISK_do_seek              (SCSI_DISK*, int, int);
double  DISK_estimate_access_time (SCSI_DISK*, int, int);

/* cache model routines */
void    DISK_cache_init           (SCSI_DISK*);
int     DISK_cache_getsegment     (SCSI_DISK*, int, int, int);
int     DISK_cache_hit            (SCSI_DISK*, int, int);
void    DISK_cache_touch          (SCSI_DISK*, int, int);
int     DISK_cache_insert         (SCSI_DISK*, int, int, int);
int     DISK_cache_write          (SCSI_DISK*, int, int);
int     DISK_cache_getwsegment    (SCSI_DISK*, int*, int*);
void    DISK_cache_commit_write   (SCSI_DISK*, int, int);
void    DISK_cache_complete_write (SCSI_DISK*, int);
int     DISK_cache_segment_full   (SCSI_DISK*, int);

/* physical storage routines */
void    DISK_storage_init         (SCSI_DISK*);
void    DISK_storage_read         (SCSI_DISK*, int, int, char*);
void    DISK_storage_write        (SCSI_DISK*, int, int, char*);

#endif
