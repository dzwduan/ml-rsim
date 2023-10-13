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
#include <values.h>
#include <math.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/evlst.h"
#include "Processor/simio.h"
#include "Caches/system.h"
#include "Caches/lqueue.h"

#include "IO/scsi_bus.h"
#include "IO/scsi_disk.h"
#include "IO/pci.h"

#ifndef linux
typedef unsigned char u_int8_t;
#endif

#include "../../lamix/dev/scsipi/scsipi_all.h"


#define min(a, b)               ((a) > (b) ? (b) : (a))


void      DISK_request_handler    ();
void      DISK_seek_handler       ();
void      DISK_sector_handler     ();

int       DISK_issue_request      (SCSI_DISK *pdisk, SCSI_REQ* req);

void      DISK_wonbus             (void*, SCSI_REQ*);
void      DISK_request            (void*, SCSI_REQ*);
void      DISK_response           (void*, SCSI_REQ*);
void      DISK_perform            (void*, SCSI_REQ*);
void      DISK_stat_report        (void*);
void      DISK_stat_clear         (void*);
void      DISK_dump               (void*);



/*=========================================================================*/
/* Create and configure a SCSI disk module                                 */
/* Allocate and initialize a new disk drive control structure. Redirect    */
/* configuration file if a separate disk-config file is specified, and     */
/* read parameters. Restore original configuration file name. Allocate and */
/* initialize queues, cache and persistent storage. Attach to SCSI bus.    */
/*=========================================================================*/

void SCSI_disk_init(SCSI_BUS *psbus, int dev_id)
{
  SCSI_DISK   *pdisk;
  char         seek_method[128];
  char         disk_configfile[PATH_MAX];
  char        *old_configfile;
  extern FILE *pfp;
  extern char *configfile;
  int          n;

  pdisk = (SCSI_DISK*)malloc(sizeof(SCSI_DISK));
  if (pdisk == NULL)
    YS__errmsg(psbus->node_id, "Malloc failed at %s:%i", __FILE__, __LINE__);

  pdisk->dev_id = dev_id;

  pdisk->current_cylinder = 0;
  pdisk->current_head     = 0;

  /*-----------------------------------------------------------------------*/

  strcpy(pdisk->name, "IBM/Ultrastar_9LP");
  pdisk->seek_single          =  0.7;
  pdisk->seek_avg             =  6.5;
  pdisk->seek_full            = 14.0;
  pdisk->seek_method          = DISK_SEEK_CURVE;
  pdisk->write_settle         = 1.3;
  pdisk->head_switch          = 0.85;
  pdisk->cntl_overhead        = 40;
  pdisk->rpm                  = 7200;
  pdisk->cylinders            = 8420;
  pdisk->heads                = 10;
  pdisk->sectors              = 209;
  pdisk->cylinder_skew        = 20;
  pdisk->track_skew           = 35;
  pdisk->request_queue_size   = 32;
  pdisk->response_queue_size  = 32;
  pdisk->cache_size           = 1024;
  pdisk->cache_segments       = 16;
  pdisk->cache_write_segments = 2;
  pdisk->prefetch             = 1;
  pdisk->fast_writes          = 0;
  pdisk->buffer_full_ratio    = 0.75;
  pdisk->buffer_empty_ratio   = 0.75;
  pdisk->ticks_per_ms         = 1000000000 / CPU_CLK_PERIOD;


  /*-----------------------------------------------------------------------*/

  pdisk->scsi_me = SCSI_device_init(psbus, dev_id, pdisk,
				    DISK_wonbus,
				    DISK_request,
				    DISK_response,
				    DISK_perform,
				    DISK_stat_report,
				    DISK_stat_clear,
				    DISK_dump);

  /*-----------------------------------------------------------------------*/

  strcpy(disk_configfile, "");
  get_parameter("DISK_params", disk_configfile, PARAM_STRING);
  if (strlen(disk_configfile) > 0)
    {
      fclose(pfp);
      pfp = NULL;
      old_configfile = configfile;
      configfile = disk_configfile;
    }

  get_parameter("DISK_name",         &(pdisk->name),           PARAM_STRING);

  get_parameter("DISK_seek_single",  &(pdisk->seek_single),    PARAM_DOUBLE);
  get_parameter("DISK_seek_av",      &(pdisk->seek_avg),       PARAM_DOUBLE);
  get_parameter("DISK_seek_full",    &(pdisk->seek_full),      PARAM_DOUBLE);

  strcpy(seek_method, "");
  get_parameter("DISK_seek_method",  seek_method,              PARAM_STRING);

  if (strncasecmp("DISK_SEEK_NONE",  seek_method, 14) == 0)
    pdisk->seek_method = DISK_SEEK_NONE;
  if (strncasecmp("DISK_SEEK_CONST", seek_method, 15) == 0)
    pdisk->seek_method = DISK_SEEK_CONST;
  if (strncasecmp("DISK_SEEK_LINE",  seek_method, 14) == 0)
    pdisk->seek_method = DISK_SEEK_LINE;
  if (strncasecmp("DISK_SEEK_CURVE", seek_method, 15) == 0)
    pdisk->seek_method = DISK_SEEK_CURVE;

  get_parameter("DISK_write_settle",  &(pdisk->write_settle),  PARAM_DOUBLE);
  get_parameter("DISK_head_switch",   &(pdisk->head_switch),   PARAM_DOUBLE);
  get_parameter("DISK_cntl_ov",       &(pdisk->cntl_overhead), PARAM_DOUBLE);

  get_parameter("DISK_rpm",           &(pdisk->rpm),           PARAM_INT);
  get_parameter("DISK_cyl",           &(pdisk->cylinders),     PARAM_INT);
  get_parameter("DISK_heads",         &(pdisk->heads),         PARAM_INT);
  get_parameter("DISK_sect",          &(pdisk->sectors),       PARAM_INT);
  get_parameter("DISK_cylinder_skew", &(pdisk->cylinder_skew), PARAM_INT);
  get_parameter("DISK_track_skew",    &(pdisk->track_skew),    PARAM_INT);

  get_parameter("DISK_request_q",   &(pdisk->request_queue_size), PARAM_INT);
  get_parameter("DISK_response_q",  &(pdisk->response_queue_size),PARAM_INT);
  get_parameter("DISK_cache_size",  &(pdisk->cache_size),         PARAM_INT);
  get_parameter("DISK_cache_seg",   &(pdisk->cache_segments),     PARAM_INT);
  get_parameter("DISK_cache_write_seg", &(pdisk->cache_write_segments), PARAM_INT);
  get_parameter("DISK_prefetch"  ,  &(pdisk->prefetch)   ,        PARAM_INT);
  get_parameter("DISK_fast_write",  &(pdisk->fast_writes),        PARAM_INT);

  get_parameter("DISK_buffer_full", &(pdisk->buffer_full_ratio),
		PARAM_DOUBLE);
  get_parameter("DISK_buffer_empty",&(pdisk->buffer_empty_ratio),
		PARAM_DOUBLE);

  if (pdisk->cache_size != ToPowerOf2(pdisk->cache_size))
    YS__errmsg(psbus->node_id,
	       "DISK: Size of disk cache must be power of two");

  if (pdisk->cache_segments != ToPowerOf2(pdisk->cache_segments))
    YS__errmsg(psbus->node_id,
	       "DISK: Number of disksegments must be power of two");

  if (strlen(disk_configfile) > 0)
    {
      fclose(pfp);
      pfp = NULL;
      configfile = old_configfile;
    }

  /*-----------------------------------------------------------------------*/

  lqueue_init(&(pdisk->inqueue), pdisk->request_queue_size);
  lqueue_init(&(pdisk->outqueue), pdisk->response_queue_size);

  pdisk->state        = DISK_IDLE;
  pdisk->current_req  = NULL;
  pdisk->start_offset =
    (pdisk->scsi_me->scsi_bus->node_id * SCSI_WIDTH * 8 * PCI_MAX_DEVICES) +
    (pdisk->scsi_me->scsi_bus->bus_id * SCSI_WIDTH * 8) +
    pdisk->scsi_me->dev_id;
  pdisk->start_offset %= pdisk->sectors;

  pdisk->request_event = NewEvent("Disk Request",
				  DISK_request_handler, NODELETE, 0);
  EventSetArg(pdisk->request_event, (char*)pdisk, sizeof(pdisk));

  pdisk->seek_event = NewEvent("Disk Sector Reached",
			       DISK_seek_handler, NODELETE, 0);
  EventSetArg(pdisk->seek_event, (char*)pdisk, sizeof(pdisk));

  pdisk->sector_event = NewEvent("Disk Sector Transferred",
				 DISK_sector_handler, NODELETE, 0);
  EventSetArg(pdisk->sector_event, (char*)pdisk, sizeof(pdisk));

  DISK_storage_init(pdisk);
  
  DISK_cache_init(pdisk);

  DISK_stat_clear(pdisk);

  /*-----------------------------------------------------------------------*/
  /* initialize inquiry data command return structure                      */

  memset(&pdisk->inquiry_data, 0, sizeof(pdisk->inquiry_data));
  pdisk->inquiry_data.type                 = SCSI_DEVICE_DISK;
  pdisk->inquiry_data.qualifier            = 0;
  pdisk->inquiry_data.version              = 2;
  pdisk->inquiry_data.response_format      = 0;
  pdisk->inquiry_data.additional_length    = sizeof(SCSI_INQUIRY_DATA) - 4;
  pdisk->inquiry_data.flags                =
    SID_CmdQue | SID_Linked | SID_WBus16 | SID_WBus32;
  n = strcspn(pdisk->name, "/");
  if ((n > sizeof(pdisk->inquiry_data.vendor) - 1) || (n == 0))
    strcpy(pdisk->inquiry_data.vendor, "Unknown");
  else
    strncpy(pdisk->inquiry_data.vendor, pdisk->name, n);
  n += strspn(pdisk->name + n, "/");
  strncpy(pdisk->inquiry_data.product, pdisk->name + n,
	  min(sizeof(pdisk->inquiry_data.product) - 1,
	      strlen(pdisk->name + n)));
  strcpy(pdisk->inquiry_data.revision, "1.0");


  /*-----------------------------------------------------------------------*/
  /* initialize mode sense command return structure                        */
  
  memset(&pdisk->mode_sense_data, 0, sizeof(pdisk->mode_sense_data));
  pdisk->mode_sense_data.data_length     = sizeof(pdisk->mode_sense_data) - 1;
  pdisk->mode_sense_data.medium_type     = 0;
  pdisk->mode_sense_data.blk_desc_length = 8;
  pdisk->mode_sense_data.density         = 0;
  n = pdisk->cylinders * pdisk->heads * pdisk->sectors;
  pdisk->mode_sense_data.nblocks[0]      = (n >> 16) & 0xFF;
  pdisk->mode_sense_data.nblocks[1]      = (n >> 8) & 0xFF;
  pdisk->mode_sense_data.nblocks[2]      = n & 0xFF;
  pdisk->mode_sense_data.blklen[0]       = (SCSI_BLOCK_SIZE >> 16) & 0xFF;
  pdisk->mode_sense_data.blklen[1]       = (SCSI_BLOCK_SIZE >> 8) & 0xFF;
  pdisk->mode_sense_data.blklen[2]       = SCSI_BLOCK_SIZE & 0xFF;

  pdisk->mode_sense_data.page_code       = 4;
  pdisk->mode_sense_data.page_length     = 22;
  
  pdisk->mode_sense_data.disk_pages.rigid_geometry.ncyl[0] =
    (pdisk->cylinders >> 16) & 0xFF;
  pdisk->mode_sense_data.disk_pages.rigid_geometry.ncyl[1] =
    (pdisk->cylinders >> 8) & 0xFF;
  pdisk->mode_sense_data.disk_pages.rigid_geometry.ncyl[2] =
    pdisk->cylinders & 0xFF;
  pdisk->mode_sense_data.disk_pages.rigid_geometry.nheads = pdisk->heads;
  pdisk->mode_sense_data.disk_pages.rigid_geometry.rpm[0] =
    (pdisk->rpm >> 8) & 0xFF;
  pdisk->mode_sense_data.disk_pages.rigid_geometry.rpm[1] =
    pdisk->rpm & 0xFF;


  /*-----------------------------------------------------------------------*/
  /* allocate sense data return structure array - one per initiator        */

  pdisk->sense_data = (SCSI_EXT_SENSE_DATA*)malloc(sizeof(SCSI_EXT_SENSE_DATA) * SCSI_WIDTH * 8);
}







/*=========================================================================*/
/* Event handler for incoming requests: takes requests off the input       */
/* queue, checks for cache hits, attaches a cache segment to the request   */
/* and starts a seek operation if necessary.                               */
/* Event is scheduled by 'disk_request' when a new request enters an empty */
/* qeueue and the disk is idle, by itself when a request is completed, by  */
/* the seek event handler when a seek request is completed or by the       */
/* sector event all sectors are transferred.                               */
/*=========================================================================*/

void DISK_request_handler()
{
  SCSI_DISK *pdisk = (SCSI_DISK*)EventGetArg(NULL);
  SCSI_REQ  *req;
  int        start_block;
  int        length, segment;
  int        hit_type;


  /*=======================================================================*/
  /* if queue is empty and fast writes are pending create a write request  */

  start_block = 0;
  length  = MAXINT;
  segment = DISK_cache_getwsegment(pdisk, &start_block, &length);
  if (!lqueue_empty(&pdisk->inqueue))
    req = lqueue_head(&pdisk->inqueue);

  if ((pdisk->fast_writes) &&
      (pdisk->current_req == NULL) &&
      (segment >= 0) &&
      ((lqueue_empty(&pdisk->inqueue)) ||
       ((!lqueue_empty(&pdisk->inqueue)) &&
	(req->orig_request == SCSI_REQ_WRITE) &&
	(req->transferred == 0))))
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i] %0.f: Start Cache Writeback Segment %i\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 YS__Simtime, segment);
#endif
      req = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
      req->orig_request   = SCSI_REQ_WRITE;
      req->start_block    = start_block;
      req->length         = length;
      req->current_length = 0;
      req->transferred    = length;
      req->cache_segment  = segment;
      req->imm_flag       = SCSI_FLAG_TRUE;
      req->buscycles      = 0;
      pdisk->current_req  = req;
    }



  /*=======================================================================*/
  /* no current request (could be write-back or prefetch)                  */
  /* get new request from queue                                            */

  if ((lqueue_empty(&(pdisk->inqueue))) &&
      (pdisk->current_req == NULL))
    return;

  if (pdisk->current_req == NULL)
    {
      lqueue_get(&(pdisk->inqueue), req);
      pdisk->current_req = req;
    }
  else
    req = pdisk->current_req;

#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i]  %.0f Handle %s Sector %i %p\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     YS__Simtime,
	     SCSI_ReqName[req->orig_request], req->start_block, req);
#endif

  req->current_length = 0;



  /*=======================================================================*/
  /* read or prefetch (both requested and triggered by a read)             */
  /* check cache: full hit - update cache LRU bits and return data         */
  /* partial hit - update cache LRU bits and start disk seek               */
  /* miss - start disk seek                                                */

  if ((req->orig_request == SCSI_REQ_READ) ||
      (req->orig_request == SCSI_REQ_PREFETCH))
    {
      /* abort read-induced prefetch if more requests are waiting ---------*/
      if ((req->orig_request == SCSI_REQ_PREFETCH) &&
	  (req->request_type == SCSI_REQ_READ) &&
	  (!lqueue_empty(&(pdisk->inqueue))))
	{
	  YS__PoolReturnObj(&YS__ScsiReqPool, req);
	  pdisk->current_req = NULL;

	  if (IsNotScheduled(pdisk->request_event))
	    schedule_event(pdisk->request_event,
			   YS__Simtime + pdisk->cntl_overhead*SCSI_FREQ_RATIO);
	}


      /* check cache ------------------------------------------------------*/

      hit_type = DISK_cache_hit(pdisk, req->start_block, req->length);
      req->cache_segment = DISK_cache_getsegment(pdisk, req->start_block,
						 req->length, 0);

      if (hit_type == DISK_CACHE_HIT_FULL)
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Full Cache Hit [%i]\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     req->cache_segment);
#endif
          DISK_cache_touch(pdisk, req->start_block, req->length);

	  if ((req->orig_request == SCSI_REQ_READ) ||
	      ((req->orig_request == SCSI_REQ_PREFETCH) &&
	       (req->request_type == SCSI_REQ_PREFETCH) &&
	       (!req->imm_flag)))
	    {
	      req->transferred       = req->length;
	      req->buscycles         =
		req->length * SCSI_BLOCK_SIZE / SCSI_WIDTH;
	      req->current_data_size = req->buscycles * SCSI_WIDTH;
	      req->request_type      = SCSI_REQ_RECONNECT;
	      req->reply_type        = SCSI_REP_COMPLETE;

	      DISK_issue_request(pdisk, pdisk->current_req);
	    }

	  pdisk->current_req = NULL;
	  pdisk->state = DISK_IDLE;

	  if (IsNotScheduled(pdisk->request_event))
	    schedule_event(pdisk->request_event,
			   YS__Simtime + pdisk->cntl_overhead*SCSI_FREQ_RATIO);

	  if (req->orig_request == SCSI_REQ_READ)
	    pdisk->cache_hits_full++;

	  return;
	}


      if (hit_type == DISK_CACHE_HIT_PARTIAL)
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Partial Cache Hit [%i] (%i-%i) %i\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     req->cache_segment,
		     pdisk->cache[req->cache_segment].start_block,
		     pdisk->cache[req->cache_segment].end_block,
		     req->start_block);
#endif
          DISK_cache_touch(pdisk, req->start_block,
			   pdisk->cache[req->cache_segment].end_block -
			   req->length);

          req->current_length = pdisk->cache[req->cache_segment].end_block -
	    req->start_block;

          DISK_do_seek(pdisk, req->start_block + req->current_length, 0);

	  if (req->orig_request == SCSI_REQ_READ)
	    pdisk->cache_hits_partial++;
	}


      if (hit_type == DISK_CACHE_MISS)
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Cache Miss\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif
          DISK_do_seek(pdisk, req->start_block, 0);
	}
    }


  
  /*=======================================================================*/
  /* seek request - initiate disk seek                                     */
  
  if (req->orig_request == SCSI_REQ_SEEK)
    {
      DISK_do_seek(pdisk, req->start_block, 0);
    }


  
  /*=======================================================================*/

  if (req->orig_request == SCSI_REQ_WRITE)
    {
      DISK_cache_commit_write(pdisk, req->start_block, req->length);

      if ((pdisk->fast_writes) &&
	  (req->imm_flag) &&
	  (req->length == req->transferred))
	{
	  SCSI_REQ *qreq = lqueue_empty(&pdisk->inqueue) ?
	    NULL : lqueue_head(&pdisk->inqueue);
	  SCSI_REQ *sreq;

	  if (req->buscycles > 0)
	    {
	      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
	      memcpy(sreq, req, sizeof(SCSI_REQ));
	    }
	  else
	    sreq = req;


#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Committing Fast Write\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif

	  if ((pdisk->state == DISK_IDLE) &&
	      ((lqueue_empty(&pdisk->inqueue)) ||
	       ((qreq != NULL) && (qreq->orig_request == SCSI_REQ_WRITE) &&
		(qreq->transferred == 0))))
	    {
	      sreq->start_block = 0;
	      sreq->length = INT_MAX;
              sreq->cache_segment = DISK_cache_getwsegment(pdisk,
							  &(sreq->start_block),
							  &(sreq->length));
#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i]  Start Fast Write %i\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 sreq->cache_segment);
#endif

	      if (sreq != req)
		sreq->transferred = sreq->length;
	      
	      DISK_do_seek(pdisk, sreq->start_block, 1);
	      pdisk->current_req = sreq;
	    }
	  else
	    {
	      YS__PoolReturnObj(&YS__ScsiReqPool, sreq);
	      if (pdisk->state == DISK_IDLE)
		{
#ifdef SCSI_DISK_TRACE
		  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			     "[%i:%i]  Next Request\n",
			     pdisk->scsi_me->scsi_bus->bus_id+1,
			     pdisk->scsi_me->dev_id);
#endif
		  if (IsNotScheduled(pdisk->request_event))
		    schedule_event(pdisk->request_event,
				   YS__Simtime);
		  pdisk->current_req = NULL;
		}
	    }
	}

      else
	{
	  if (req->transferred == 0)
	    {
	      SCSI_REQ* sreq;

	      req->transferred = DISK_cache_write(pdisk, req->start_block,
						  req->length);

	      DISK_cache_commit_write(pdisk, req->start_block, req->length);

	      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
	      memcpy(sreq, req, sizeof(SCSI_REQ));
	      sreq->buscycles         =
		req->transferred * SCSI_BLOCK_SIZE / SCSI_WIDTH;
              sreq->current_data_size = sreq->buscycles * SCSI_WIDTH;
              sreq->request_type      = SCSI_REQ_RECONNECT;

	      if (sreq->transferred == req->length)
		sreq->reply_type = SCSI_REP_COMPLETE;
	      else
		sreq->reply_type = SCSI_REP_SAVE_DATA_POINTER;

              DISK_issue_request(pdisk, sreq);
	    }

          req->cache_segment = DISK_cache_getsegment(pdisk,
						     req->start_block, 0, 1);
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Start Write Segment %i\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     req->cache_segment);
#endif
	  DISK_do_seek(pdisk, req->start_block, 1);
	}
    }

  

  
  /*=======================================================================*/
  /* sync-cache request:                                                   */

  if (req->orig_request == SCSI_REQ_SYNC_CACHE)
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i]  Handle SYNC %i %i\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 req->start_block, req->length);
#endif

      if (req->length > 0)
	req->length += (pdisk->cache_size * 1024 / SCSI_BLOCK_SIZE) /
	  pdisk->cache_segments;
      else
	req->length = INT_MAX;

      req->cache_segment = DISK_cache_getwsegment(pdisk,
						  &(req->start_block),
						  &(req->length));
      if (req->cache_segment < 0)
	{
	  if (!req->imm_flag)
	    {
	      req->request_type      = SCSI_REQ_RECONNECT;
	      req->reply_type        = SCSI_REP_COMPLETE;
	      req->buscycles         = 0;
	      req->current_data_size = 0;	      
	      DISK_issue_request(pdisk, req);
	    }
	  else
	    YS__PoolReturnObj(&YS__ScsiReqPool, req);

	  pdisk->current_req = NULL;
	  pdisk->state = DISK_IDLE;
	  
	  if (IsNotScheduled(pdisk->request_event))
	    schedule_event(pdisk->request_event,
			   YS__Simtime + pdisk->cntl_overhead*SCSI_FREQ_RATIO);
	  return;
	}
      
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i]  Flush %i %i\n", req->start_block,
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 req->length);
#endif
      
      req->current_length = 0;
      DISK_do_seek(pdisk, req->start_block, 1);
    }


  

  /*=======================================================================*/
  /* format request, if imm-flag is not set report completion              */

  if (req->orig_request == SCSI_REQ_FORMAT)
    {
      if (!req->imm_flag)
	{
	  req->request_type      = SCSI_REQ_RECONNECT;
	  req->reply_type        = SCSI_REP_COMPLETE;
	  req->buscycles         = 0;
	  req->current_data_size = 0;
	  DISK_issue_request(pdisk, req);
	}
      else
	YS__PoolReturnObj(&YS__ScsiReqPool, req);

      pdisk->current_req = NULL;
      pdisk->state = DISK_IDLE;

      if (IsNotScheduled(pdisk->request_event))
	schedule_event(pdisk->request_event,
		       YS__Simtime + pdisk->cntl_overhead * SCSI_FREQ_RATIO);
      return;
    }
  
}





/*=========================================================================*/
/* Disk seek operation complete: finish a seek request, or start           */
/* transferring data. Scheduled by the request-event handler or the sector */
/* event handler whenever a seek is necessary.                             */
/*=========================================================================*/

void DISK_seek_handler()
{
  SCSI_DISK *pdisk = (SCSI_DISK*)EventGetArg(NULL);
  SCSI_REQ  *req = pdisk->current_req;


  if (pdisk->current_req == NULL)
    return;

#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] Seek %.1f to %i\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     YS__Simtime,
	     DISK_sector_at_time(pdisk, pdisk->current_head,
				 pdisk->current_cylinder, YS__Simtime));
#endif


  /* seek request is complete at this point -------------------------------*/

  if (req->orig_request == SCSI_REQ_SEEK)
    {
      if (!req->imm_flag)
	{
	  req->request_type      = SCSI_REQ_RECONNECT;
	  req->reply_type        = SCSI_REP_COMPLETE;
	  req->buscycles         = 0;
	  req->current_data_size = 0;
	  DISK_issue_request(pdisk, req);
	}
      
      pdisk->state        = DISK_IDLE;
      pdisk->current_req  = NULL;

      if (IsNotScheduled(pdisk->request_event))
	schedule_event(pdisk->request_event, YS__Simtime);
    }

  
  /* schedule sector event for other requests -----------------------------*/
  else
    {
      pdisk->state = DISK_TRANSFER;

      if (IsNotScheduled(pdisk->request_event))
	schedule_event(pdisk->sector_event,
		       YS__Simtime +
		       rint(DISK_rotation_time(pdisk, 1)*pdisk->ticks_per_ms));
    }
}






/*=========================================================================*/
/* sector event handler: complete sector has been transferred.             */
/* Insert data into cache for reads, get new segment unless it is a        */
/* prefetch. Make segment clean if all data has been written, get new      */
/* write segment. Schedule new sector or seek event if more data needs to  */
/* be transferred, or request-event if current request is completed.       */
/*=========================================================================*/

void DISK_sector_handler()
{
  SCSI_DISK *pdisk = (SCSI_DISK*)EventGetArg(NULL);
  SCSI_REQ  *req = pdisk->current_req, *sreq;
  int        start_block, next_cylinder, next_head, length;


  if (req == NULL)
    return;
  

  req->current_length++;
  
  pdisk->transfer_time += DISK_rotation_time(pdisk, 1);

  
  /*=======================================================================*/
  /* Handle reads and prefetches (both requested and triggered by a read)  */
  
  if ((req->orig_request == SCSI_REQ_READ) ||
      (req->orig_request == SCSI_REQ_PREFETCH))
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i] Read %.1f  Sector %i  Segment %i\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 YS__Simtime,
		 req->start_block + req->current_length - 1,
		 req->cache_segment);
#endif

      pdisk->blocks_read_media++;

      
      /* insert sector in cache -------------------------------------------*/

      DISK_cache_insert(pdisk, req->start_block + req->current_length - 1,
			1, req->cache_segment);

      
      /* read-induced prefetch reaches end of segment - abort -------------*/

      if ((DISK_cache_segment_full(pdisk, req->cache_segment) ||
	   (!lqueue_empty(&(pdisk->inqueue)))) &&
	  (req->orig_request == SCSI_REQ_PREFETCH) &&
	  (req->request_type == SCSI_REQ_READ))
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Abort Prefetch\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif
	  
	  pdisk->state = DISK_IDLE;
	  pdisk->current_req = NULL;
	  YS__PoolReturnObj(&YS__ScsiReqPool, req);
	  
	  if (IsNotScheduled(pdisk->request_event))
	    schedule_event(pdisk->request_event, YS__Simtime);
	  return;
	}

      

      /* requested prefetch or read fills up segment - get new segment ----*/

      if ((DISK_cache_segment_full(pdisk, req->cache_segment)) &&
	  (req->current_length < req->length))
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] Segment full\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif

          req->cache_segment =
	    DISK_cache_getsegment(pdisk,
				  req->start_block + req->current_length,
				  req->length - req->current_length - 1, 0);
	}


      
      /* return data if threshold reached ---------------------------------*/
      if (((double)(req->current_length - req->transferred) >=
	   (double)(req->length-req->transferred)*pdisk->buffer_full_ratio) &&
	  (req->length - req->transferred > 0) &&
	  (req->orig_request == SCSI_REQ_READ))
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  Return Data %f > %f ?",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     DISK_estimate_access_time(pdisk,
					       req->start_block+req->current_length,
					       req->length - req->current_length),
		     (double)((req->length - req->transferred) * SCSI_BLOCK_SIZE) / (double)(SCSI_WIDTH * SCSI_FREQUENCY * 1000.0));
#endif

	  if (DISK_estimate_access_time(pdisk,
					req->start_block + req->current_length,
					req->length - req->current_length) <
	      (double)((req->length - req->transferred) * SCSI_BLOCK_SIZE) /
	      (double)(SCSI_WIDTH * SCSI_FREQUENCY * 1000.0))
	    {
	      req->request_type  = SCSI_REQ_RECONNECT;
	      req->reply_type    = SCSI_REP_COMPLETE;
	      req->buscycles     = (req->length - req->transferred) *
		SCSI_BLOCK_SIZE / SCSI_WIDTH;
	      req->current_data_size = req->buscycles * SCSI_WIDTH;
	      req->transferred   = req->length;
	      DISK_issue_request(pdisk, req);

#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 " %s (%i)\n",
			 SCSI_RepName[req->reply_type], req->buscycles);
#endif
	    }
	  else
	    {
	      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
	      memcpy(sreq, req, sizeof(SCSI_REQ));
	      sreq->request_type      = SCSI_REQ_RECONNECT;
	      sreq->reply_type        = SCSI_REP_SAVE_DATA_POINTER;
	      sreq->buscycles         =
		(req->current_length - req->transferred) *
		SCSI_BLOCK_SIZE / SCSI_WIDTH;
	      sreq->current_data_size = sreq->buscycles * SCSI_WIDTH;
	      sreq->parent            = req;
	      DISK_issue_request(pdisk, sreq);

	      req->transferred       = req->current_length;

#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 " %s (%i)\n",
			 SCSI_RepName[sreq->reply_type], sreq->buscycles);
#endif
	    }
	}
    }



  
  /*=======================================================================*/
  /* handle write request: mark segment as clean if completely written     */

  if (req->orig_request == SCSI_REQ_WRITE)
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i] Wrote %.1f  Sector %i  Segment %i\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 YS__Simtime,
		 req->start_block + req->current_length - 1,
		 req->cache_segment);
#endif

      pdisk->blocks_written_media++;

      /*-------------------------------------------------------------------*/
      /* segment completely written: mark as clean                         */

      if ((req->current_length % ((pdisk->cache_size * 1024) /
	   (pdisk->cache_segments * SCSI_BLOCK_SIZE)) == 0) ||
	  (req->current_length == req->length))
	{
	  DISK_cache_complete_write(pdisk, req->cache_segment);


	  /*---------------------------------------------------------------*/
	  /* need to get more data from initiator                          */
	  if (req->transferred < req->length)
	    {
	      SCSI_REQ *sreq;
	      
	      length = DISK_cache_write(pdisk,
					req->start_block + req->transferred,
					req->length - req->transferred);

#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i]  Get More Data %i %i %i %i\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 req->length,
			 req->current_length,
			 req->start_block + req->transferred,
			 length);
#endif
	      
	      DISK_cache_commit_write(pdisk,
				      req->start_block + req->transferred,
				      req->length - req->transferred);

	      req->buscycles         = length * SCSI_BLOCK_SIZE / SCSI_WIDTH;
	      req->current_data_size = req->buscycles * SCSI_WIDTH;
	      req->transferred      += length;
	      req->request_type      = SCSI_REQ_RECONNECT;
	      if (req->transferred < req->length)
		req->reply_type      = SCSI_REP_SAVE_DATA_POINTER;
	      else
		req->reply_type      = SCSI_REP_COMPLETE;

	      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
	      memcpy(sreq, req, sizeof(SCSI_REQ));
	      DISK_issue_request(pdisk, sreq);
	    }


	  if ((pdisk->fast_writes) && (req->imm_flag) && (req->request_type != SCSI_REQ_RECONNECT))
	    {
	      req->start_block = 0;
	      req->length = MAXINT;
	      req->cache_segment =
		DISK_cache_getwsegment(pdisk,
				       &(req->start_block),
				       &(req->length));
#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i]   New Segment Fast %i\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 req->cache_segment);
#endif

	      if (req->cache_segment < 0)
		{
		  pdisk->state = DISK_IDLE;
		  pdisk->current_req = NULL;
		  YS__PoolReturnObj(&YS__ScsiReqPool, req);

		  if (IsNotScheduled(pdisk->request_event))
		    schedule_event(pdisk->request_event, YS__Simtime);
		  return;
		}

	      req->current_length = 0;
	      req->transferred = req->length;
	      req->lba = req->start_block;
	    }

	  else if (req->current_length < req->length)
	    {
	      req->cache_segment =
		DISK_cache_getsegment(pdisk, 
				      req->start_block + req->current_length,
				      1, 1);

#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i]   New Segment %i %i -> %i\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 req->start_block + req->current_length,
			 req->length - req->current_length,
			 req->cache_segment);
#endif
	    }
	}
    }


  
  
  /*=======================================================================*/
  /* handle sync request: mark segment as clean if completely written      */

  if (req->orig_request == SCSI_REQ_SYNC_CACHE)
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i] Synced %.1f  Sector %i  Segment %i\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id,
		 YS__Simtime,
		 req->start_block + req->current_length - 1,
		 req->cache_segment);
#endif

      pdisk->blocks_written_media++;

      if (req->current_length == req->length)
	{
	  DISK_cache_complete_write(pdisk, req->cache_segment);

	  req->start_block = 0;                   /* not exactly correct @@@ */
	  req->length = MAXINT;
	  req->cache_segment = DISK_cache_getwsegment(pdisk,
						      &(req->start_block),
						      &(req->length));
	  req->current_length = 0;
	  if (req->cache_segment < 0)
	    {
	      if (!req->imm_flag)
		{
		  req->request_type      = SCSI_REQ_RECONNECT;
		  req->reply_type        = SCSI_REP_COMPLETE;
		  req->buscycles         = 0;
		  req->current_data_size = 0;
		  DISK_issue_request(pdisk, req);
		}
	      else
		YS__PoolReturnObj(&YS__ScsiReqPool, req);

	      pdisk->current_req = NULL;
	      pdisk->state       = DISK_IDLE;

	      if (IsNotScheduled(pdisk->request_event))
		schedule_event(pdisk->request_event,
			       YS__Simtime+pdisk->cntl_overhead*SCSI_FREQ_RATIO);
	      return;
	    }
	}
    }
	  


  
  /*=======================================================================*/
  /* transfer next sector: wait for rotation if on same track, or seek     */

  if (req->current_length < req->length)
    {
      next_cylinder = DISK_sector_to_cylinder(pdisk, req->start_block +
					      req->current_length);
      next_head     = DISK_sector_to_head(pdisk, req->start_block +
					  req->current_length);

      if ((next_cylinder == pdisk->current_cylinder) &&
	  (next_head == pdisk->current_head) &&
	  ((req->start_block + req->current_length ==
	    DISK_sector_at_time(pdisk, next_head,
				next_cylinder, YS__Simtime)) ||
	   (req->start_block + req->current_length + 1 ==
	    DISK_sector_at_time(pdisk, next_head,
				next_cylinder, YS__Simtime))))
	   
	{
	  if (IsNotScheduled(pdisk->sector_event))
	    schedule_event(pdisk->sector_event,
			   YS__Simtime +
			   floor(DISK_rotation_time(pdisk, 1) *
				 pdisk->ticks_per_ms));

	  pdisk->seek_target_sector = req->start_block + req->current_length;
	  

#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  More Transfer %i %i (estimated %f ms %f) -> %.0f\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     req->current_length, req->length,
		     DISK_estimate_access_time(pdisk, req->start_block+req->current_length,
					       req->length - req->current_length),
		     DISK_estimate_access_time(pdisk, req->start_block+req->current_length,
					       req->length - req->current_length) * pdisk->ticks_per_ms,
		     YS__Simtime + rint(DISK_rotation_time(pdisk, 1) *
					pdisk->ticks_per_ms));
#endif
	}
      else
	{
	  DISK_do_seek(pdisk, req->start_block + req->current_length,
		       req->orig_request == SCSI_REQ_WRITE);

#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]  More Seek\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif
	}
    }


  
  /* all blocks transferred -----------------------------------------------*/
  else
    {
      /* prefetch is complete ---------------------------------------------*/
      if ((req->orig_request == SCSI_REQ_PREFETCH) ||
	  (req->request_type == SCSI_REQ_PREFETCH))
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i] %.0f: DISK Prefetch %s %s %i\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id,
		     YS__Simtime,
		     SCSI_ReqName[req->request_type],
		     SCSI_ReqName[req->orig_request],
		     req->imm_flag);
#endif
	  
	  if ((req->orig_request == SCSI_REQ_PREFETCH) && (!req->imm_flag))
	    {
	      req->request_type      = SCSI_REQ_RECONNECT;
	      req->reply_type        = SCSI_REP_COMPLETE;
	      req->buscycles         = 0;
	      req->current_data_size = 0;
	      DISK_issue_request(pdisk, req);
	    }
	  else
	    YS__PoolReturnObj(&YS__ScsiReqPool, req);

	  pdisk->current_req = NULL;
	  pdisk->state = DISK_IDLE;
	  if (IsNotScheduled(pdisk->request_event))
	    schedule_event(pdisk->request_event, YS__Simtime);
	}

      
      /* read complete: start prefetch if queue is empty ------------------*/
      if ((req->orig_request == SCSI_REQ_READ) &&
	  (req->request_type != SCSI_REQ_PREFETCH))
	if ((lqueue_empty(&(pdisk->inqueue))) &&
	    (!DISK_cache_segment_full(pdisk, req->cache_segment)) &&
	    (pdisk->prefetch))
	  {
            sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
	    memcpy(sreq, req, sizeof(SCSI_REQ));
	    sreq->orig_request  = SCSI_REQ_PREFETCH;
	    sreq->request_type  = SCSI_REQ_READ;
	    sreq->start_block   = req->start_block + req->length;
	    sreq->length        = req->length;      /* configure length @@ */
	    sreq->imm_flag      = 1;

#ifdef SCSI_DISK_TRACE
	    YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		       "[%i:%i]   Start Prefetch %p %i\n",
		       pdisk->scsi_me->scsi_bus->bus_id+1,
		       pdisk->scsi_me->dev_id,
		       sreq,
		       sreq->start_block);
#endif
	    
	    pdisk->current_req  = sreq;
	    pdisk->state        = DISK_IDLE;
	  }
	else
	  {
	    pdisk->state        = DISK_IDLE;
	    pdisk->current_req  = NULL;
	  }

      
      /* write complete: issue response if not fast-write -----------------*/
      if (req->orig_request == SCSI_REQ_WRITE)
	{
	  if ((!pdisk->fast_writes) || (!req->imm_flag))
	    {
	      req->request_type      = SCSI_REQ_RECONNECT;
	      req->reply_type        = SCSI_REP_COMPLETE;
	      req->buscycles         = 0;
	      req->current_data_size = 0;
	      DISK_issue_request(pdisk, req);
	    }

	  pdisk->state = DISK_IDLE;
	  pdisk->current_req = NULL;

	}

#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i]  Done\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id);
#endif

      if (IsNotScheduled(pdisk->request_event))
	schedule_event(pdisk->request_event, YS__Simtime);
    }
}






/*=========================================================================*/
/* Issue a SCSI reply transaction on the bus. Arbitrate for bus if output  */
/* queue is empty, otherwise just put it in the queue.                     */
/*=========================================================================*/

int DISK_issue_request(SCSI_DISK *pdisk, SCSI_REQ* req)
{
  if (lqueue_full(&(pdisk->outqueue)))
    return(0);

  if (lqueue_empty(&(pdisk->outqueue)))
    {
      if (SCSI_device_request(pdisk->scsi_me, req) == 1)
	return(1);
    }

  lqueue_add(&(pdisk->outqueue), req, pdisk->scsi_me->scsi_bus->node_id);
  return(1);
}





/*=========================================================================*/
/* Callback function for when the disk wins the bus. Arbitrate again if    */
/* more requests are pending in the outqueue.                              */
/*=========================================================================*/

void DISK_wonbus(void *ptr, SCSI_REQ *req)
{
  SCSI_DISK *pdisk = (SCSI_DISK*)ptr;
  SCSI_REQ  *next_req;

  if (!lqueue_empty(&(pdisk->outqueue)))
    {
      lqueue_get(&(pdisk->outqueue), next_req);
      SCSI_device_request(pdisk->scsi_me, next_req);
    }
}





/*=========================================================================*/
/* Callback function for when the disk receives a request. Check validity. */
/* Reject if invalid block number, or non-queue request and queue is full. */
/* Enqueue reads and disconnect. Enqueue writes, transfer data until write */
/* segments are full and return disconnect, save_data_ptr, busy or         */
/* complete. Enqueue other requests. Fill in return data structure for     */
/* inquiry and read_capacity requests.                                     */
/* Schedule request-event if disk is idle and event is not yet scheduled.  */
/*=========================================================================*/

void DISK_request(void *ptr, SCSI_REQ *req)
{
  SCSI_DISK            *pdisk = (SCSI_DISK*)ptr;
  SCSI_REQ             *sreq;
  int                   rc;


  req->orig_request  = req->request_type;
  req->start_block   = req->lba;
  req->transferred   = 0;
  req->buscycles     = pdisk->cntl_overhead;

#ifdef SCSI_DISK_TRACE
  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "[%i:%i] DISK %.1f Request %s Block %i Length %i\n",
	     pdisk->scsi_me->scsi_bus->bus_id+1,
	     pdisk->scsi_me->dev_id,
	     YS__Simtime,
	     SCSI_ReqName[req->request_type],
	     req->start_block,
	     req->length);
#endif

  /*-----------------------------------------------------------------------*/
  /* check block number and length                                         */

  if ((req->request_type == SCSI_REQ_READ) ||
      (req->request_type == SCSI_REQ_WRITE) ||
      (req->request_type == SCSI_REQ_PREFETCH) ||
      (req->request_type == SCSI_REQ_SEEK))
    {
      if ((req->start_block < 0) ||
	  (req->start_block >=
	   pdisk->cylinders*pdisk->heads*pdisk->sectors) ||
	  (req->start_block + req->length < 0) ||
	  (req->start_block + req->length >
	   pdisk->cylinders*pdisk->heads*pdisk->sectors))
	{
	  req->reply_type = SCSI_REP_REJECT;
	  
	  pdisk->sense_data[req->initiator].error      = 0x70;
	  pdisk->sense_data[req->initiator].segment    = 0;
	  pdisk->sense_data[req->initiator].key        = SCSI_SENSE_ILLEGAL_REQUEST;
	  pdisk->sense_data[req->initiator].info0      = 0;
	  pdisk->sense_data[req->initiator].info1      = 0;
	  pdisk->sense_data[req->initiator].info2      = 0;
	  pdisk->sense_data[req->initiator].info3      = 0;
	  pdisk->sense_data[req->initiator].add_length = 0;
	  
	  return;
	}
    }


  if ((req->request_type != SCSI_REQ_INQUIRY) &&
      (req->request_type != SCSI_REQ_MISC) &&
      (req->lun != 0))
    {
      req->reply_type = SCSI_REP_REJECT;
      
      pdisk->sense_data[req->initiator].error      = 0x70;
      pdisk->sense_data[req->initiator].segment    = 0;
      pdisk->sense_data[req->initiator].key        = SCSI_SENSE_ILLEGAL_REQUEST;
      pdisk->sense_data[req->initiator].info0      = 0;
      pdisk->sense_data[req->initiator].info1      = 0;
      pdisk->sense_data[req->initiator].info2      = 0;
      pdisk->sense_data[req->initiator].info3      = 0;
      pdisk->sense_data[req->initiator].add_length = 0;
	  
      return;
    }


  if (req->request_type != SCSI_REQ_REQUEST_SENSE)
    {
      pdisk->sense_data[req->initiator].error      = 0x70;
      pdisk->sense_data[req->initiator].segment    = 0;
      pdisk->sense_data[req->initiator].key        = SCSI_SENSE_NONE;
      pdisk->sense_data[req->initiator].info0      = 0;
      pdisk->sense_data[req->initiator].info1      = 0;
      pdisk->sense_data[req->initiator].info2      = 0;
      pdisk->sense_data[req->initiator].info3      = 0;
      pdisk->sense_data[req->initiator].add_length = 0;
    }



  /*-----------------------------------------------------------------------*/
  /* simple (non-queue) request and queue is not empty - reject            */

  if ((req->queue_msg == NO_QUEUE) &&
      ((!lqueue_empty(&(pdisk->inqueue))) ||
       (pdisk->current_req != NULL)))
    {
      if ((pdisk->current_req != NULL) &&
	  (((pdisk->current_req->orig_request == SCSI_REQ_PREFETCH) &&
	    (pdisk->current_req->request_type == SCSI_REQ_READ)) ||
	   ((pdisk->current_req->orig_request == SCSI_REQ_WRITE) &&
	    (pdisk->current_req->imm_flag) &&
	    (pdisk->fast_writes))))
	{
	  if (!((pdisk->current_req->orig_request == SCSI_REQ_WRITE) &&
		((req->request_type == SCSI_REQ_SYNC_CACHE) ||
		 (req->request_type == SCSI_REQ_WRITE))))
	    {
#ifdef SCSI_DISK_TRACE
	      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
			 "[%i:%i]   Abort Current Request %s\n",
			 pdisk->scsi_me->scsi_bus->bus_id+1,
			 pdisk->scsi_me->dev_id,
			 SCSI_ReqName[pdisk->current_req->orig_request]);
#endif
	      YS__PoolReturnObj(&YS__ScsiReqPool, pdisk->current_req);
	      pdisk->current_req = NULL;
	      pdisk->state = DISK_IDLE;
	    }
	}
      else
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]   Queue not empty\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
	  if (pdisk->current_req != NULL)
	    YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		       "[%i:%i]   Current: %s %s %i %p\n",
		       pdisk->scsi_me->scsi_bus->bus_id+1,
		       pdisk->scsi_me->dev_id,
		       SCSI_ReqName[pdisk->current_req->request_type],
		       SCSI_ReqName[pdisk->current_req->orig_request],
		       pdisk->current_req->start_block, pdisk->current_req);
	  else
	    YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		       "[%i:%i]   Size: %i\n",
		       pdisk->scsi_me->scsi_bus->bus_id+1,
		       pdisk->scsi_me->dev_id,
		       pdisk->inqueue.size);
#endif
	  
          req->reply_type = SCSI_REP_BUSY;
	  return;
	}
    }

  

  /*-----------------------------------------------------------------------*/
  /* queue request and queue is full - reject                              */
  if ((req->queue_msg != NO_QUEUE) &&
      (lqueue_full(&(pdisk->inqueue))))
    {
#ifdef SCSI_DISK_TRACE
      YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		 "[%i:%i]   Queue full\n",
		 pdisk->scsi_me->scsi_bus->bus_id+1,
		 pdisk->scsi_me->dev_id);
#endif
      
      req->reply_type = SCSI_REP_BUSY;
      return;
    }

  

  /*-----------------------------------------------------------------------*/
  /* read request: disconnect, put in queue and schedule handler           */

  if (req->request_type == SCSI_REQ_READ)
    {
      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
      memcpy(sreq, req, sizeof(SCSI_REQ));

      if (sreq->queue_msg == HEAD_OF_QUEUE)
	{
	  lqueue_add_head(&(pdisk->inqueue), sreq,
			  pdisk->scsi_me->scsi_bus->node_id);
	}
      else
	{
	  lqueue_add(&(pdisk->inqueue), sreq,
		     pdisk->scsi_me->scsi_bus->node_id);
	}

      if (IsNotScheduled(pdisk->request_event))
	schedule_event(pdisk->request_event, YS__Simtime +
	               0 );
		       /* pdisk->cntl_overhead * SCSI_FREQ_RATIO); */
  
      req->reply_type = SCSI_REP_DISCONNECT;

      pdisk->requests_read++;
      pdisk->blocks_read += req->length;

      return;
    }

  

  /*-----------------------------------------------------------------------*/
  /* write request: disconnect, put in cache and in queue, schedule handler*/
  
  if (req->request_type == SCSI_REQ_WRITE)
    {
      rc = DISK_cache_write(pdisk, req->start_block, req->length);

      if (rc == 0)
	{
#ifdef SCSI_DISK_TRACE
	  YS__logmsg(pdisk->scsi_me->scsi_bus->node_id,
		     "[%i:%i]   No write segment\n",
		     pdisk->scsi_me->scsi_bus->bus_id+1,
		     pdisk->scsi_me->dev_id);
#endif
	  
          if ((!req->imm_flag) || (!pdisk->fast_writes))
	    {
	      req->reply_type = SCSI_REP_BUSY;
	      return;
	    }
	}

      req->transferred += rc;
      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
      memcpy(sreq, req, sizeof(SCSI_REQ));
      req->buscycles = sreq->buscycles = rc * SCSI_BLOCK_SIZE / SCSI_WIDTH;
      req->current_data_size = rc * SCSI_BLOCK_SIZE;

      if (rc == 0)
	req->reply_type = SCSI_REP_DISCONNECT;
      else if ((pdisk->fast_writes) &&
	       (req->imm_flag) &&
	       (req->transferred == req->length))
	req->reply_type = SCSI_REP_COMPLETE;
      else
	req->reply_type = SCSI_REP_SAVE_DATA_POINTER;

      if (sreq->queue_msg == HEAD_OF_QUEUE)
	{
	  lqueue_add_head(&(pdisk->inqueue), sreq,
			  pdisk->scsi_me->scsi_bus->node_id);
	}
      else
	{
	  lqueue_add(&(pdisk->inqueue), sreq,
		     pdisk->scsi_me->scsi_bus->node_id);
	}

      
      if ((pdisk->state == DISK_IDLE) &&
	  (IsNotScheduled(pdisk->request_event)))
	schedule_event(pdisk->request_event, YS__Simtime +
		       pdisk->cntl_overhead * SCSI_FREQ_RATIO);

      pdisk->requests_write++;
      pdisk->blocks_written += sreq->length;

      return;
    }


  
  /*-----------------------------------------------------------------------*/
  /* other requests: put in queue and return immediately                   */

  if ((req->request_type == SCSI_REQ_PREFETCH) ||
      (req->request_type == SCSI_REQ_SEEK) ||
      (req->request_type == SCSI_REQ_FORMAT) ||
      (req->request_type == SCSI_REQ_SYNC_CACHE))
    {
      sreq = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
      memcpy(sreq, req, sizeof(SCSI_REQ));

      req->current_data_size = 0;
      if (req->imm_flag)
	req->reply_type = SCSI_REP_COMPLETE;
      else
	req->reply_type = SCSI_REP_DISCONNECT;
      
      if (sreq->queue_msg == HEAD_OF_QUEUE)
	{
	  lqueue_add_head(&(pdisk->inqueue), sreq,
			  pdisk->scsi_me->scsi_bus->node_id);
	}
      else
	{
	  lqueue_add(&(pdisk->inqueue), sreq,
		     pdisk->scsi_me->scsi_bus->node_id);
	}

      if ((pdisk->state == DISK_IDLE) &&
	  (IsNotScheduled(pdisk->request_event)))
	schedule_event(pdisk->request_event, YS__Simtime +
		       pdisk->cntl_overhead * SCSI_FREQ_RATIO);

      pdisk->requests_other++;

      return;
    }


  
  /*-----------------------------------------------------------------------*/
  /* test command: return status only                                      */

  if (req->request_type == SCSI_REQ_MISC)
    {
      req->current_data_size = 0;
      req->reply_type = SCSI_REP_COMPLETE;

      pdisk->requests_other++;

      return;
    }

  
  
  /*-----------------------------------------------------------------------*/
  /* inquiry/capacity command: fill in return buffer                       */
  
  if (req->request_type == SCSI_REQ_INQUIRY)
    {
      if (req->buf != NULL)
	{
	  memcpy(req->buf, &pdisk->inquiry_data,
		 min(req->length, sizeof(pdisk->inquiry_data)));

	  if (req->lun != 0)
	    req->buf[0] = 0x7F;
	}
      
      req->reply_type        = SCSI_REP_COMPLETE;
      req->buscycles        += req->length / SCSI_WIDTH;
      req->current_data_size = req->length;

      pdisk->requests_other++;

      return;
    }

  
  if (req->request_type == SCSI_REQ_MODE_SENSE)
    {
      if (req->aux != 4)
	YS__warnmsg(pdisk->scsi_me->scsi_bus->node_id,
		    "Mode Sense Page %i not supported!", req->aux);
      
      if (req->buf != NULL)
	memcpy(req->buf, &pdisk->mode_sense_data,
	       min(req->length, sizeof(pdisk->mode_sense_data)));

      req->reply_type        = SCSI_REP_COMPLETE;
      req->buscycles        += req->length / SCSI_WIDTH;
      req->current_data_size = req->length;

      pdisk->requests_other++;

      return;
    }

  
  if (req->request_type == SCSI_REQ_READ_CAPACITY)
    {
      if (req->buf != NULL)
	{
	  int n = pdisk->cylinders*pdisk->heads*pdisk->sectors - 1;
	  req->buf[0] = (n >> 24) & 0xFF;
	  req->buf[1] = (n >> 16) & 0xFF;
	  req->buf[2] = (n >>  8) & 0xFF;
	  req->buf[3] = (n >>  0) & 0xFF;
	  req->buf[4] = (SCSI_BLOCK_SIZE >> 24) & 0xFF;
	  req->buf[5] = (SCSI_BLOCK_SIZE >> 16) & 0xFF;
	  req->buf[6] = (SCSI_BLOCK_SIZE >>  8) & 0xFF;
	  req->buf[7] = (SCSI_BLOCK_SIZE >>  0) & 0xFF;
	}

      req->reply_type        = SCSI_REP_COMPLETE;
      req->buscycles        += req->length / SCSI_WIDTH;
      req->current_data_size = req->length;

      pdisk->requests_other++;

      return;      
    }


  if (req->request_type == SCSI_REQ_REQUEST_SENSE)
    {
      if (req->buf != NULL)
	memcpy(req->buf, &pdisk->sense_data[req->initiator],
	       min(req->length, sizeof(pdisk->mode_sense_data)));

      req->reply_type        = SCSI_REP_COMPLETE;
      req->buscycles        += req->length / SCSI_WIDTH;
      req->current_data_size = req->length;

      pdisk->requests_other++;

      pdisk->sense_data[req->initiator].error      = 0x70;
      pdisk->sense_data[req->initiator].segment    = 0;
      pdisk->sense_data[req->initiator].key        = SCSI_SENSE_NONE;
      pdisk->sense_data[req->initiator].info0      = 0;
      pdisk->sense_data[req->initiator].info1      = 0;
      pdisk->sense_data[req->initiator].info2      = 0;
      pdisk->sense_data[req->initiator].info3      = 0;
      pdisk->sense_data[req->initiator].add_length = 0;

      return;      
    }
}





/*=========================================================================*/
/* Callback function for responses to the disk. Not supported!             */
/*=========================================================================*/

void DISK_response(void *ptr, SCSI_REQ *req)
{
  SCSI_DEV  *pdev  = (SCSI_DEV*)ptr;
  SCSI_DISK *pdisk = (SCSI_DISK*)pdev->device;

  YS__errmsg(pdisk->scsi_me->scsi_bus->node_id,
	     "SCSI: Response to disk not supported\n");
}




/*=========================================================================*/
/*=========================================================================*/

void DISK_perform(void *ptr, SCSI_REQ *req)
{
  SCSI_DEV  *pdev  = (SCSI_DEV*)ptr;
  SCSI_DISK *pdisk = (SCSI_DISK*)pdev->device;

  if (req->orig_request == SCSI_REQ_READ)
    DISK_storage_read(pdisk, req->start_block, req->length, req->buf);

  if (req->orig_request == SCSI_REQ_WRITE)
    DISK_storage_write(pdisk, req->start_block, req->length, req->buf);
}





/*=========================================================================*/
/* Report configuration and statistics for a specific disk.                */
/*=========================================================================*/

void DISK_stat_report(void *ptr)
{
  SCSI_DISK *pdisk = (SCSI_DISK*)ptr;
  int nid = pdisk->scsi_me->scsi_bus->node_id;

  YS__statmsg(nid,
	      "SCSI Disk %i Configuration - %s\n",
	      pdisk->dev_id, pdisk->name);
  YS__statmsg(nid,
	      "%i rpm, %i heads, %i cylinders\n",
	      pdisk->rpm, pdisk->heads, pdisk->cylinders);
  YS__statmsg(nid,
	      "  %.2f ms Avg Seek;  %.2f ms Full Seek;  %.2f ms Track-to-Track Seek\n",
	      pdisk->seek_avg, pdisk->seek_full, pdisk->seek_single);

  if (pdisk->cache_size == 0)
    YS__statmsg(nid, "  No cache\n");
  else
    YS__statmsg(nid,
		"  %i KB cache;  %i segments;  %i write segments;  fast-writes %s\n",
		pdisk->cache_size,
		pdisk->cache_segments,
		pdisk->cache_write_segments,
		pdisk->fast_writes == 1 ? "on" : "off");

  YS__statmsg(nid,
	      "  Buffer Full Ratio: %.2f;  Buffer Empty Ratio: %.2f\n",
	      pdisk->buffer_full_ratio, pdisk->buffer_empty_ratio);
   YS__statmsg(nid,
              "  Persistent disk storage:\n    %s\n    %s\n",
              pdisk->storage.index_file_name, pdisk->storage.data_file_name);
  
  YS__statmsg(nid, "\nSCSI Disk %i Statistics\n", pdisk->dev_id);

  YS__statmsg(nid,
	      "  %i requests;  %i reads;  %i writes;  %i other\n",
	      pdisk->requests_read + pdisk->requests_write + pdisk->requests_other,
	      pdisk->requests_read, pdisk->requests_write, pdisk->requests_other);
  YS__statmsg(nid,
	      "  Full cache hits:     %10i\tpartial cache hits: %10i\n",
	      pdisk->cache_hits_full, pdisk->cache_hits_partial);
  YS__statmsg(nid,
	      "  Blocks read:         %10i\tfrom media:         %10i\n",
	      pdisk->blocks_read, pdisk->blocks_read_media);
  YS__statmsg(nid,
	      "  Blocks written:      %10i\tto media:           %10i\n",
	      pdisk->blocks_written, pdisk->blocks_written_media);
  YS__statmsg(nid,
	      "  Total seek time:     %10.2f ms  (%6.2f%%)\n",
	      pdisk->seek_time,
	  pdisk->seek_time * pdisk->ticks_per_ms * 100.0 / YS__Simtime);
  YS__statmsg(nid,
	      "  Total transfer time: %10.2f ms  (%6.2f%%)\n",
	      pdisk->transfer_time,
	      pdisk->transfer_time * pdisk->ticks_per_ms * 100.0 / YS__Simtime);
  YS__statmsg(nid,
	      "  Idle time:           %10.2f ms  (%6.2f%%)\n",
	      (YS__Simtime - (pdisk->seek_time + pdisk->transfer_time) * pdisk->ticks_per_ms) / pdisk->ticks_per_ms,
	      100.0 - (pdisk->seek_time + pdisk->transfer_time) *
	      pdisk->ticks_per_ms * 100.0 / YS__Simtime);
  
  YS__statmsg(nid, "\n");
}




/*=========================================================================*/
/* Reset statistics counters for a disk.                                   */
/*=========================================================================*/

void DISK_stat_clear(void *ptr)
{
  SCSI_DISK *pdisk = (SCSI_DISK*)ptr;

  pdisk->requests_read        = 0;
  pdisk->requests_write       = 0;
  pdisk->requests_other       = 0;
  pdisk->cache_hits_partial   = 0;
  pdisk->cache_hits_full      = 0;
  pdisk->blocks_read          = 0;
  pdisk->blocks_written       = 0;
  pdisk->blocks_read_media    = 0;
  pdisk->blocks_written_media = 0;
  pdisk->seek_time            = 0.0;
  pdisk->transfer_time        = 0.0;
}


/*=========================================================================*/
/* Dump debug information                                                  */
/*=========================================================================*/

void DISK_dump(void *ptr)
{
  SCSI_DISK *pdisk = (SCSI_DISK*)ptr;
  int n, nid = pdisk->scsi_me->scsi_bus->node_id;

  YS__logmsg(nid, "\n============== SCSI DISK %i %i ==============\n",
	     pdisk->scsi_me->dev_id, pdisk->scsi_me->scsi_bus->bus_id);
  YS__logmsg(nid, "state(%d), current_cylinder(%d), current_head(%d)\n",
	     pdisk->state, pdisk->current_cylinder, pdisk->current_head);
  YS__logmsg(nid, "previous_cylinder(%d), seek_target_sector(%d)\n",
	     pdisk->previous_cylinder, pdisk->seek_target_sector);
  YS__logmsg(nid,
	     "start_offset(%d), seek_start_time(%lf), seek_end_time(%lf)\n",
	     pdisk->start_offset, pdisk->seek_start_time,
	     pdisk->seek_end_time);

  YS__logmsg(nid, "current_req:\n");
  if (pdisk->current_req)
    SCSI_req_dump(pdisk->current_req, 0, nid);
  else
    YS__logmsg(nid, "  NULL\n");

  YS__logmsg(nid, "request_event scheduled: %s\n",
	     IsScheduled(pdisk->request_event) ? "yes" : "no");
  YS__logmsg(nid, "seek_event scheduled: %s\n",
	     IsScheduled(pdisk->seek_event) ? "yes" : "no");
  YS__logmsg(nid, "sector_event scheduled: %s\n",
	     IsScheduled(pdisk->sector_event) ? "yes" : "no");

  DumpLinkQueue("inqueue", &(pdisk->inqueue), 0, SCSI_req_dump, nid);
  DumpLinkQueue("outqueue", &(pdisk->outqueue), 0, SCSI_req_dump, nid);

  for (n = 0; n < pdisk->cache_segments; n++)
    {
      YS__logmsg(nid,
		 "cache seg[%02i] start_block(%d), end_block(%d), write(%d)\n",
		 n, pdisk->cache[n].start_block, 
		 pdisk->cache[n].end_block, pdisk->cache[n].write);
      YS__logmsg(nid,
		 "              committed(%d), lru(%d)\n",
		 pdisk->cache[n].committed, pdisk->cache[n].lru);
    }
}
