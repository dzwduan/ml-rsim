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

/*-------------------------------------------------------------------------*/
/* Simple model of a hypothetical Adaptec SCSI controller based on the     */
/* AIC7770 chip.  Implements basic control registers needed for normal     */
/* device driver operation, such as SCB array, host control, SCB input and */
/* output FIFO. Supports only one SCSI bus which is configured             */
/* independently from settings on the adapter.                             */
/*-------------------------------------------------------------------------*/



#ifndef __RSIM_AHC_H__
#define __RSIM_AHC_H__


/* uncomment this to compile Adaptec model with debugging output */
/*
#define AHC_TRACE
*/


/*-------------------------------------------------------------------------*/
/* A DMA Scatter/Gather vector element. Contains 4-byte start address and  */
/* 4-byte length. Note that both are in little-endian format.              */

struct ahc_dmaseg_t
{
  unsigned char addr0;
  unsigned char addr1;
  unsigned char addr2;
  unsigned char addr3;
  unsigned char length0;
  unsigned char length1;
  unsigned char length2;
  unsigned char length3;
};

typedef struct ahc_dmaseg_t ahc_dmaseg_t;



/*-------------------------------------------------------------------------*/
/* Request status. Updated by the sequencer (->active) and the response    */
/* handling routine. Sequencer polls on the status for state transitions.  */

enum scb_status_t
{
  SCB_INACTIVE,
  SCB_ACTIVE,
  SCB_CONNECT,
  SCB_BUSY,
  SCB_DISCONNECT,
  SCB_ERROR,
  SCB_COMPLETE
};

typedef enum scb_status_t scb_status_t;




/*-------------------------------------------------------------------------*/
/* Internal representation of an SCB entry. First part corresponds to the  */
/* software-visible control registers (windowed). Contains extra fields    */
/* for internal bookkeeping, pointers to buffers and statistics variables. */

struct ahc_scb_t
{
  unsigned char    control;
  unsigned char    tcl;
  unsigned char    target_status;
  unsigned char    sg_segment_count;
  unsigned char    sg_segment_ptr0;
  unsigned char    sg_segment_ptr1;
  unsigned char    sg_segment_ptr2;
  unsigned char    sg_segment_ptr3;
  unsigned char    res_sg_segment_count;
  unsigned char    res_data_count0;
  unsigned char    res_data_count1;
  unsigned char    res_data_count2;
  unsigned char    data_ptr0;
  unsigned char    data_ptr1;
  unsigned char    data_ptr2;
  unsigned char    data_ptr3;
  unsigned char    data_count0;
  unsigned char    data_count1;
  unsigned char    data_count2;
  unsigned char    rsvd0;
  unsigned char    scsi_cmd_ptr0;
  unsigned char    scsi_cmd_ptr1;
  unsigned char    scsi_cmd_ptr2;
  unsigned char    scsi_cmd_ptr3;
  unsigned char    scsi_cmd_len;
  unsigned char    tag;
  unsigned char    next;
  unsigned char    prev;
  unsigned char    rsvd1;
  unsigned char    rsvd2;
  unsigned char    rsvd3;
  unsigned char    rsvd4;
   
  unsigned char    command[16];
  ahc_dmaseg_t    *dma_segments;
  unsigned char   *data;
  int              write;
  
  scb_status_t     status;
  struct SCSI_REQ *req;
  int              current_segment;

  unsigned         dma_addr;
  unsigned         dma_length;
  unsigned         dma_sg_length;
  unsigned         dma_length_done;
  unsigned char   *dma_buffer;

  double           start_time;
  double           queue_time;
};

typedef struct ahc_scb_t ahc_scb_t;




/*-------------------------------------------------------------------------*/
/* Internal representation of the SCB input and output queue. Fixed size   */
/* array with head and tail pointer.                                       */

struct ahc_queue_t
{
  unsigned char elements[256];
  int           head;
  int           tail;
  int           size;
};

typedef struct ahc_queue_t ahc_queue_t;


#define ahc_queue_init(q)   (q).head = 0; (q).tail = 0; (q).size = 0;

#define ahc_queue_shiftin(q, e)                                             \
  if ((q).size >= 256)                                                      \
    YS__errmsg(ahc->scsi_me->nodeid,                                        \
	       "AHC[%i]: shift into full queue",                            \
	       ahc->scsi_me->mid);                                          \
  (q).elements[(q).tail] = e;                                               \
  (q).tail = ((q).tail + 1) & 0xFF;                                         \
  (q).size++;

#define ahc_queue_shiftin_head(q, e)                                        \
  if ((q).size >= 256)                                                      \
    YS__errmsg(ahc->scsi_me->nodeid,                                        \
	       "AHC[%i]: shift into full queue",                            \
	       ahc->scsi_me->mid);                                          \
  (q).head = ((q).head + 255) & 0xFF;                                       \
  (q).elements[(q).head] = e;                                               \
  (q).size++;

#define ahc_queue_shiftout(q, e)                                            \
  if ((q).size <= 0)                                                        \
    YS__errmsg(ahc->scsi_me->nodeid,                                        \
	       "AHC[%i]: shift out empty queue",                            \
	       ahc->scsi_me->mid);                                          \
  e = (q).elements[(q).head];                                               \
  (q).head = ((q).head + 1) & 0xFF;                                         \
  (q).size--;

#define ahc_queue_head(q) (q).elements[(q).head]

#define ahc_queue_size(q) (q).size




/*-------------------------------------------------------------------------*/
/* Sequencer states: used to implement the sequencer state machine.        */

enum sequencer_state_t
{
  SEQ_IDLE,
  SEQ_COMMAND,
  SEQ_DMA_VECTOR,
  SEQ_RESPONSE,
  SEQ_DATA
};

typedef enum sequencer_state_t sequencer_state_t;





/*-------------------------------------------------------------------------*/
/* SCSI adapter control structure                                          */
/*   contains control registers, SCB array, SCB input and output queues,   */
/*   sequencer control variables and statistics variables.                 */

struct ahc_t
{
  struct SCSI_CONTROLLER *scsi_me;        /* pointer to generic controller */
  int                     scsi_id;        /* my SCSI ID                    */
  unsigned                base_addr;      /* base address on system bus    */


  /* control registers and internal data structures -----------------------*/
  
  unsigned char          *regs;           /* pointer to control registers  */
  ahc_scb_t              *scbs;           /* pointer to SCB array          */

  ahc_queue_t             qin_fifo;       /* SCB input and output queue    */
  ahc_queue_t             qout_fifo;


  /* sequencer control ----------------------------------------------------*/
  
  sequencer_state_t       seq_state;      /* sequencer state variable      */
  int                     seq_cycle_fast; /* sequencer polling intervals   */
  int                     seq_cycle_read;
  int                     seq_cycle_write;
  int                     seq_pause;      /* halt sequencer                */
  int                     seq_scb;        /* current SCB                   */
  EVENT                  *sequencer;      /* event implementing sequencer  */

  ahc_queue_t             reconnect_scbs; /* SCBs that were reconnected to */
  ahc_scb_t             **pending;        /* pending non-queued requests   */


  /* configuration variables ----------------------------------------------*/
  
  int                     max_scbs;       /* number of onchip SCB entries  */


  /* statistics variables -------------------------------------------------*/
  
  int                     intr_seq_count;   /* sequencer interrupt         */
  double                  intr_seq_start;   /* count and latencies         */
  int                     intr_seq_lat_done;
  double                  intr_seq_lat_max;
  double                  intr_seq_lat_avg;
  double                  intr_seq_lat_min;
  int                     intr_seq_clr_done;
  double                  intr_seq_clr_max;
  double                  intr_seq_clr_avg;
  double                  intr_seq_clr_min;

  int                     intr_cmpl_count;  /* command-complete interrupt  */
  double                  intr_cmpl_start;  /* count and latencies         */
  int                     intr_cmpl_lat_done;
  double                  intr_cmpl_lat_max;
  double                  intr_cmpl_lat_avg;
  double                  intr_cmpl_lat_min;
  int                     intr_cmpl_clr_done;
  double                  intr_cmpl_clr_max;
  double                  intr_cmpl_clr_avg;
  double                  intr_cmpl_clr_min;

  int                     intr_scsi_count;  /* SCSI interrupt count and    */
  double                  intr_scsi_start;  /* latencies                   */
  int                     intr_scsi_lat_done;
  double                  intr_scsi_lat_max;
  double                  intr_scsi_lat_avg;
  double                  intr_scsi_lat_min;
  int                     intr_scsi_clr_done;
  double                  intr_scsi_clr_max;
  double                  intr_scsi_clr_avg;
  double                  intr_scsi_clr_min;


  int                     request_count;
  int                     request_disconnect_count;
  int                     request_complete_count;
  double                  request_queue_time_max;
  double                  request_queue_time_avg;
  double                  request_queue_time_min;
  double                  request_connect_time_max;
  double                  request_connect_time_avg;
  double                  request_connect_time_min;
  double                  request_complete_time_max;
  double                  request_complete_time_avg;
  double                  request_complete_time_min;
  double                  request_total_time_max;
  double                  request_total_time_avg;
  double                  request_total_time_min;
};

typedef struct ahc_t ahc_t;


#define MAX_SCBS_DEFAULT 32

struct SCSI_CONTROLLER;
struct SCSI_REQ;



/*-------------------------------------------------------------------------*/
/* routines implementing the SCSI adapter model                            */

void ahc_init          (struct SCSI_CONTROLLER*, int);
void ahc_reset         (ahc_t*);

int  ahc_host_write    (void*, unsigned, unsigned);
int  ahc_host_read     (void*, unsigned, unsigned);
void ahc_pci_map       (void *, unsigned, int, int, int, int,
			unsigned*, unsigned*);
void ahc_scsi_wonbus   (void*, struct SCSI_REQ*);
void ahc_scsi_request  (void*, struct SCSI_REQ*);
void ahc_scsi_response (void*, struct SCSI_REQ*);

void ahc_print_params  (void*);
void ahc_stat_report   (void*);
void ahc_stat_clear    (void*);
void ahc_dump          (void*);

void ahc_sequencer     ();
void ahc_dma           (ahc_t*, ahc_scb_t*);
int  ahc_scsi_command  (ahc_t*, ahc_scb_t*);
int  ahc_scsi_complete (ahc_t*, ahc_scb_t*);

#endif
