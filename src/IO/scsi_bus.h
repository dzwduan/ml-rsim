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

#ifndef __RSIM_SCSI_BUS_H__
#define __RSIM_SCSI_BUS_H__


#include "sim_main/evlst.h"


/*---------------------------------------------------------------------------*/
/* SCSI request and response types                                           */

typedef enum
{
  SCSI_REQ_MISC,                     /* miscellaneous commands               */
  SCSI_REQ_INQUIRY,                  /* read device identification           */
  SCSI_REQ_MODE_SENSE,               /* read device parameters               */
  SCSI_REQ_READ_CAPACITY,            /* get capacity information             */
  SCSI_REQ_REQUEST_SENSE,            /* get sense data                       */
  SCSI_REQ_READ,                     /* read N blocks                        */
  SCSI_REQ_WRITE,                    /* write N blocks                       */
  SCSI_REQ_PREFETCH,                 /* prefetch N blocks into device cache  */
  SCSI_REQ_SEEK,                     /* perform a seek operation             */
  SCSI_REQ_FORMAT,                   /* format medium                        */
  SCSI_REQ_SYNC_CACHE,               /* synchronize device cache             */
  SCSI_REQ_RECONNECT,                /* reconnection after disconnect        */
  
  SCSI_REQ_MAX
} SCSI_REQUEST_TYPE;


typedef enum
{
  SCSI_REP_COMPLETE,                 /* successful completion of command     */
  SCSI_REP_CONNECT,                  /* command accepted, start data transfer*/
  SCSI_REP_DISCONNECT,               /* 'split-transaction'                  */
  SCSI_REP_SAVE_DATA_POINTER,        /* disconnect after partial data xfer   */
  SCSI_REP_BUSY,                     /* can not accept command at this time  */
  SCSI_REP_REJECT,                   /* illegal command                      */
  SCSI_REP_TIMEOUT,                  /* target does not respond              */

  SCSI_REP_MAX
} SCSI_REPLY_TYPE;


extern const char *SCSI_ReqName[];
extern const char *SCSI_RepName[];


/*---------------------------------------------------------------------------*/
/* SCSI request queueing flags                                               */

typedef enum
{
  NO_QUEUE,                          /* don't queue request                  */
  SIMPLE_QUEUE,                      /* put request at end of queue          */
  ORDER_QUEUE,                       /* see above, don't bypass this request */
  HEAD_OF_QUEUE                      /* put request at head of queue         */
} SCSI_QUEUE_MSG;



/*---------------------------------------------------------------------------*/
/* SCSI request structure                                                    */

struct SCSI_DEV;

struct SCSI_REQ
{
  struct SCSI_REQ   *parent;         /* pointer to related request           */
  
  SCSI_REQUEST_TYPE  request_type;   /* request          type                */
  SCSI_REPLY_TYPE    reply_type;     /* request          type                */

  int                initiator;      /* request initiator/response target    */
  int                target;         /* request target/response initiator    */

  int                lun;            /* logical unit number                  */
  int                lba;            /* logical block address                */
  int                length;         /* length of request (blocks or bytes)  */
  int                imm_flag;       /* enable immediate status report       */
  
  SCSI_QUEUE_MSG     queue_msg;      /* queueing flag                        */
  int                queue_tag;      /* unique queue tag                     */
  int                aux;
  
  
  /* non-standard fields used for processing requests -----------------------*/
  SCSI_REQUEST_TYPE  orig_request;   /* original request type                */
  char              *buf;            /* pointer to data                      */
  
  int                start_block;    /* physical start block                 */
  int                current_length; /* blocks already read from device      */
  int                transferred;    /* number of blocks already transferred */
  int                cache_segment;  /* cache segment attached to request    */
  int                current_data_size;      /* bytes in current transfer    */
  
  int                buscycles;      /* number of cycles (use for reconnect) */
};

typedef struct SCSI_REQ SCSI_REQ;



/*---------------------------------------------------------------------------*/
/* SCSI device type identifiers                                              */

#define SCSI_DEVICE_DISK           0x00
#define SCSI_DEVICE_TAPE           0x01
#define SCSI_DEVICE_PRINTER        0x02
#define SCSI_DEVICE_PROCESSOR      0x03
#define SCSI_DEVICE_WRITEONCE      0x04
#define SCSI_DEVICE_CDROM          0x05
#define SCSI_DEVICE_SCANNER        0x06
#define SCSI_DEVICE_OPTICAL        0x07
#define SCSI_DEVICE_CHANGER        0x08
#define SCSI_DEVICE_COMMUNIACTION  0x09
#define SCSI_DEVICE_UNKNOWN        0x1F

#define SCSI_DEVICE_VALID          0x00
#define SCSI_DEVICE_INVALID        0x03

#define SCSI_FLAG_TRUE             1
#define SCSI_FLAG_FALSE            0



/*---------------------------------------------------------------------------*/
/* inquiry return data structures                                            */

typedef struct
{
  char type;
  char qualifier;
  char version;
  char response_format;
  char additional_length;
  char rsvd0;
  char rsvd1;
  char flags;
  char vendor[8];
  char product[16];
  char revision[4];
  char extra[8];
} SCSI_INQUIRY_DATA;


typedef struct
{
  char data_length;
  char medium_type;
  char dev_spec;
  char blk_desc_length;

  char density;
  char nblocks[3];
  char rsvd0;
  char blklen[3];

  char page_code;
  char page_length;

  union
  {
    struct disk_format
    {
      char tracks[2];
      char alt_sec[2];
      char alt_trk_0[2];
      char alt_trk_v[2];
      char ph_sec_t[2];
      char bytes_s[2];
      char interleave[2];
      char trk_skew[2];
      char cyl_skew[2];
      char flags;
      char rsvd1;
      char rsvd2;
    } disk_format;

    struct rigid_geometry
    {
      char ncyl[3];
      char nheads;
      char st_cyl_wp[3];
      char st_cyl_rwc[3];
      char driv_step[2];
      char land_zone[3];
      char sp_sync_ctl;
      char rot_offset;
      char rsvd1;
      char rpm[2];
      char rsvd2;
      char rsvd3;
    } rigid_geometry;

    struct flex_geometry
    {
      char xfr_rate[2];
      char nheads;
      char ph_sec_tr;
      char bytes_s[2];
      char ncyl[2];
      char st_cyl_wp[2];
      char st_cyl_rwc[2];
      char driv_step[2];
      char driv_step_w;
      char head_settle[2];
      char motor_on;
      char motor_off;
      char flags;
      char step_p_cyl;
      char write_pre;
      char heads_load;
      char head_unload;
      char pin_34_2;
      char pin_4_1;
      char rsvd1;
      char rsvd2;
      char rsvd3;
      char rsvd4;
    } flex_geometry;
  } disk_pages;
} SCSI_MODE_SENSE_DATA;



typedef struct
{
  char error;
  char segment;
  char key;
  char info0;
  char info1;
  char info2;
  char info3;
  char add_length;
} SCSI_EXT_SENSE_DATA;


#define SCSI_SENSE_NONE            0x0
#define SCSI_SENSE_RECOVERED_ERROR 0x1
#define SCSI_SENSE_NOT_READY       0x2
#define SCSI_SENSE_MEDIUM_ERROR    0x3
#define SCSI_SENSE_HARDWARE_ERROR  0x4
#define SCSI_SENSE_ILLEGAL_REQUEST 0x5
#define SCSI_SENSE_UNIT_ATTENTION  0x6
#define SCSI_SENSE_DATA_PROTECT    0x7
#define SCSI_SENSE_BLANK_CHECK     0x8
#define SCSI_SENSE_VENDOR          0x9
#define SCSI_SENSE_COPY_ABORT      0xA
#define SCSI_SENSE_ABORT_COMMAND   0xB
#define SCSI_SENSE_EQUAL           0xC
#define SCSI_SENSE_VOLUME_OVERFLOW 0xD
#define SCSI_SENSE_MISCOMPARE      0xE




/*---------------------------------------------------------------------------*/
/* Generic SCSI Device Structure as seen by the bus                          */

struct SCSI_BUS;

struct SCSI_DEV
{
  void             *device;               /* pointer to device-specific data */
  int               dev_id;               /* SCSI device ID                  */
  SCSI_REQ         *req;                  /* currently arbitrating request   */
                                          /* callback functions              */
  void            (*wonbus_callback)  (void*, SCSI_REQ*);
  void            (*request_callback) (void*, SCSI_REQ*);
  void            (*response_callback)(void*, SCSI_REQ*);
  void            (*perform_callback) (void*, SCSI_REQ*);
  void            (*stat_report)(void*);
  void            (*stat_clear) (void*);
  void            (*dump)(void*);

  struct SCSI_BUS  *scsi_bus;             /* pointer to SCSI bus             */
};

typedef struct SCSI_DEV SCSI_DEV;



/*---------------------------------------------------------------------------*/
/* SCSI Bus structure and states                                             */

#define SCSI_BUS_IDLE      1
#define SCSI_BUS_ARBITRATE 2
#define SCSI_BUS_BUSY      3


struct SCSI_BUS
{
  int        node_id;
  int        bus_id;

  int        state;                       /* idle or busy                    */
  
  SCSI_REQ  *current_req;                 /* request currently being handled */
  SCSI_DEV **devices;                     /* list of attached SCSI devices   */

  EVENT     *arbitrate;                   /* events for arbitration, request */
  EVENT     *send_request;                /* and response phase              */
  EVENT     *send_response;

  double     utilization;                 /* statistics                      */
  long long  num_trans;
  double     last_clear;
};

typedef struct SCSI_BUS SCSI_BUS;


/*---------------------------------------------------------------------------*/
/* global SCSI subsystem parameters                                          */

#define SCSI_BLOCK_SIZE 512

extern int SCSI_WIDTH;                    /* bus width in bytes              */
extern int SCSI_FREQUENCY;                /* bus frequency in MHz            */
extern int SCSI_FREQ_RATIO;               /* CPU to SCSI frequency ratio     */
extern int SCSI_ARB_DELAY;                /* arbitration delay in bus cycles */
extern int SCSI_BUS_FREE;                 /* bus free delay in bus cycles    */
extern int SCSI_REQ_DELAY;                /* lumped request overhead         */
extern int SCSI_SEL_TIMEOUT;              /* device select timeout in cycles */


/*---------------------------------------------------------------------------*/
/* SCSI bus functions                                                        */

void      SCSI_init(void);
SCSI_BUS *SCSI_bus_init(int, int);
void      SCSI_bus_perform(SCSI_BUS*, SCSI_REQ*);

void      SCSI_bus_print_params(int node);
void      SCSI_bus_stat_report(SCSI_BUS*);
void      SCSI_bus_stat_clear(SCSI_BUS*);

void      SCSI_req_dump(SCSI_REQ*, int, int);
void      SCSI_bus_dump(SCSI_BUS*);

/*---------------------------------------------------------------------------*/
/* SCSI generic device functions                                             */

SCSI_DEV *SCSI_device_init(SCSI_BUS*, int, void *,
			   void (*)(void*, SCSI_REQ*),
			   void (*)(void*, SCSI_REQ*),
			   void (*)(void*, SCSI_REQ*),
			   void (*)(void*, SCSI_REQ*),
			   void (*)(void*),
			   void (*)(void*),
			   void (*)(void*));
int       SCSI_device_request(SCSI_DEV*, SCSI_REQ*);

#endif
