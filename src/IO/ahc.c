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
/*=========================================================================*/


#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <values.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/evlst.h"

#include "Processor/pagetable.h"
#include "Caches/req.h"
#include "Bus/bus.h"
#include "IO/addr_map.h"
#include "IO/ahc.h"
#include "IO/scsi_bus.h"
#include "IO/scsi_controller.h"
#include "IO/io_generic.h"

#include "../../lamix/dev/ic/aic7xxxreg.h"
#include "../../lamix/dev/pci/pcireg.h"


typedef unsigned char u_int8_t;

#include "../../lamix/dev/scsipi/scsi_all.h"
#include "../../lamix/dev/scsipi/scsipi_all.h"
#include "../../lamix/dev/scsipi/scsi_disk.h"
#include "../../lamix/dev/scsipi/scsipi_disk.h"


#define min(a, b)               ((a) > (b) ? (b) : (a))



/*=========================================================================*/
/* This structure contains the handles for the generic SCSI controller to  */
/* communicate with the Adaptec model.                                     */

SCSI_CONTROLLER_SPEC ahc_spec =
{
  ahc_host_read,
  ahc_host_write,
  ahc_pci_map,

  ahc_scsi_wonbus,
  ahc_scsi_request,
  ahc_scsi_response,

  ahc_print_params,
  ahc_stat_report,
  ahc_stat_clear,
  ahc_dump
};




/*=========================================================================*/
/* Initialize Adaptec SCSI Controller: allocate control structure, allocate*/
/* control registers and SCB array, create sequencer event and initialize. */
/*=========================================================================*/

void ahc_init(SCSI_CONTROLLER *pscsi, int scsi_id)
{
  ahc_t *ahc;
  int    scbs;

  scbs = MAX_SCBS_DEFAULT;  
  get_parameter("AHC_scbs", &scbs, PARAM_INT);


  /* allocate internal data structures ------------------------------------*/

  ahc = malloc(sizeof(ahc_t));
  if (ahc == NULL)
    YS__errmsg(pscsi->nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);
  memset(ahc, 0, sizeof(ahc_t));

  ahc->scsi_me      = pscsi;
  ahc->scsi_id      = scsi_id;
  ahc->max_scbs     = scbs;
  ahc->base_addr    = 0;
  pscsi->contr_spec = &ahc_spec;
  pscsi->controller = ahc;

  ahc->regs = malloc(AHC_REG_END);
  if (ahc->regs == NULL)
    YS__errmsg(pscsi->nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);
  memset(ahc->regs, 0, AHC_REG_END);

  ahc->scbs = malloc(sizeof(ahc_scb_t) * ahc->max_scbs);
  if (ahc->scbs == NULL)
    YS__errmsg(pscsi->nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);
  memset(ahc->scbs, 0, sizeof(ahc_scb_t) * ahc->max_scbs);

  ahc->pending = malloc(sizeof(ahc_scb_t*) * SCSI_WIDTH * 8);
  if (ahc->pending == NULL)
    YS__errmsg(pscsi->nodeid, "Malloc failed at %s:%i", __FILE__, __LINE__);
  memset(ahc->pending, 0, sizeof(ahc_scb_t*) * SCSI_WIDTH * 8);


  /* create sequencer event -----------------------------------------------*/

  ahc->seq_cycle_read =
    ((1000000/SCSI_FREQUENCY) / CPU_CLK_PERIOD) * (ARCH_linesz2/SCSI_WIDTH);

  ahc->seq_cycle_write = (int)((double)ahc->seq_cycle_read*0.98);

  ahc->seq_cycle_fast = BUS_FREQUENCY * 2;

  ahc->seq_state = SEQ_IDLE;

  ahc->sequencer = NewEvent("AHC SCSI Sequencer", ahc_sequencer,
			    NODELETE, 0);
  EventSetArg(ahc->sequencer, ahc, sizeof(ahc));


  /* initialize (reset) control registers and statistics ------------------*/
  
  ahc_reset(ahc);
  
  ahc_stat_clear(ahc);
}





/*=========================================================================*/
/* Reset internal data structures. Called during initialization and upon   */
/* software reset (write to reset register).                               */
/* Clear all registers, return sequencer to idle state and reset SCB       */
/* queues. Probably not safe during normal operation.                      */
/*=========================================================================*/

void ahc_reset(ahc_t *ahc)
{
  memset(ahc->regs, 0, AHC_REG_END);
  ahc->seq_state         = SEQ_IDLE;
  ahc->seq_pause         = 1;
  ahc->seq_scb           = 0;

  if (IsScheduled(ahc->sequencer))
    deschedule_event(ahc->sequencer);

  ahc_queue_init(ahc->qin_fifo);
  ahc_queue_init(ahc->qout_fifo);
  ahc_queue_init(ahc->reconnect_scbs);

  ahc->regs[SCSIID]             = ahc->scsi_id;
  ahc->regs[SCSICONF]           = ahc->scsi_id;
  ahc->regs[SCSICONF+1]         = ahc->scsi_id;
  ahc->regs[DISC_DSB_A]         = 0x00;
  ahc->regs[DISC_DSB_B]         = 0x00;
  ahc->regs[WAITING_SCBH]       = SCB_LIST_NULL;
  ahc->regs[TARG_SCRATCH+0x10]  = ahc->scsi_me->scsi_max_target;
  ahc->regs[TARG_SCRATCH+0x11]  = ahc->scsi_me->scsi_max_lun;

  if (SCSI_WIDTH > 1)
    ahc->regs[SBLKCTL] = SELWIDE;
  else
    ahc->regs[SBLKCTL] = 0;
  ahc->regs[HCNTRL]  = PAUSE | CHIPRSTACK;
}





/*=========================================================================*/
/* Callback routine for host bus reads. Shift SCB output queue upon read   */
/* from head and collect statistics for interrupt status reads.            */
/*=========================================================================*/

int ahc_host_read(void *controller, unsigned address, unsigned length)
{
  ahc_t         *ahc = (ahc_t*)controller;
  unsigned char *regs, data;
  unsigned       offset;
  double         lat;

  if ((address < ahc->base_addr) || (address >= ahc->base_addr + AHC_REG_END))
    {
      YS__warnmsg(ahc->scsi_me->nodeid,
		  "AHC: Read from invalid address 0x%08X\n",
		  address);
      return(1);
    }
  
  offset = address - ahc->base_addr;
  regs = ahc->regs;

  
  /* handle each byte that is read by the request individually ------------*/
  
  for (;
       length > 0;
       offset++, length--)
    {
#ifdef AHC_TRACE
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i] AHC Read 0x%03X %i -> 0x%02X\n",
		 ahc->scsi_me->mid, offset, length, ahc->regs[offset]);
#endif

      switch(offset)
	{
	  /*---------------------------------------------------------------*/
	  /* read from SCB output queue: shift queue and adjust counter    */
	case QOUTFIFO:
          ahc_queue_shiftout(ahc->qout_fifo, data);

	  regs[QOUTCNT]  = ahc_queue_size(ahc->qout_fifo);
	  regs[QOUTFIFO] = ahc_queue_head(ahc->qout_fifo);

          lat = YS__Simtime - ahc->scbs[data].start_time;
	  if (lat > ahc->request_total_time_max)
	    ahc->request_total_time_max = lat;
	  if (lat < ahc->request_total_time_min)
	    ahc->request_total_time_min = lat;
	  ahc->request_total_time_avg += lat;

          break;


	  /*---------------------------------------------------------------*/
	  /* read from interrupt status: record latency for first read     */
	case INTSTAT:
	  if (!ahc->intr_seq_lat_done)
	    {
	      lat = YS__Simtime - ahc->intr_seq_start;
	      ahc->intr_seq_lat_avg += lat;
	      if (lat > ahc->intr_seq_lat_max)
		ahc->intr_seq_lat_max = lat;
	      if (lat < ahc->intr_seq_lat_min)
		ahc->intr_seq_lat_min = lat;
	      ahc->intr_seq_lat_done = 1;
	    }

	  if (!ahc->intr_cmpl_lat_done)
	    {
	      lat = YS__Simtime - ahc->intr_cmpl_start;
	      ahc->intr_cmpl_lat_avg += lat;
	      if (lat > ahc->intr_cmpl_lat_max)
		ahc->intr_cmpl_lat_max = lat;
	      if (lat < ahc->intr_cmpl_lat_min)
		ahc->intr_cmpl_lat_min = lat;
	      ahc->intr_cmpl_lat_done = 1;
	    }

	  if (!ahc->intr_scsi_lat_done)
	    {
	      lat = YS__Simtime - ahc->intr_scsi_start;
	      ahc->intr_scsi_lat_avg += lat;
	      if (lat > ahc->intr_scsi_lat_max)
		ahc->intr_scsi_lat_max = lat;
	      if (lat < ahc->intr_scsi_lat_min)
		ahc->intr_scsi_lat_min = lat;
	      ahc->intr_scsi_lat_done = 1;
	    }
	  break;
	}
    }

  return(1);
}






/*=========================================================================*/
/* Callback routine for host bus writes.                                   */
/* Handle side-effects and statistics.                                     */
/*=========================================================================*/

int ahc_host_write(void *controller, unsigned address, unsigned length)
{
  unsigned char  data, *regs;
  unsigned       olength = length;
  ahc_t         *ahc = (ahc_t*)controller;
  unsigned       offset;
  double         lat;

  
  if ((address < ahc->base_addr) ||
      (address >= ahc->base_addr + AHC_REG_END))
    {
      YS__warnmsg(ahc->scsi_me->nodeid,
		  "AHC: Read from invalid address 0x%08X\n", address);
      return(1);
    }
  
  offset = address - ahc->base_addr;
  regs = ahc->regs;

  
  /* handle each byte that is affected by the request individually --------*/
  
  for (;
       length > 0;
       offset++, length--)
    {
      data = regs[offset];

#ifdef AHC_TRACE
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i] AHC Write 0x%03X %i -> 0x%02X\n",
		 ahc->scsi_me->mid, offset, length, data);
#endif

      switch (offset)
	{
	  /*---------------------------------------------------------------*/
	  /* change SCB pointer: copy new SCB entry into register set      */
	case SCBPTR:
	  data %= ahc->max_scbs;
	  memcpy(regs + SCBARRAY,
		 &ahc->scbs[data],
		 AHC_REG_END - SCBARRAY);
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i]  Change SCB Pointer to %i\n",
		     ahc->scsi_me->mid, data);
#endif
	  regs[SCBPTR] = data;
	  break;


	  /*---------------------------------------------------------------*/
	  /* host control write: start or stop sequencer, perform reset    */
	case HCNTRL:
	  if (data & PAUSE)               /* pause bit set: halt sequencer */
	    {
	      ahc->seq_pause = 1;
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
			 "[%i]  Pause Sequencer\n",
			 ahc->scsi_me->mid);
#endif
	    }
	  
	  if (!(data & PAUSE) &&          /* pause bit cleared: start seq. */
	      (ahc->seq_pause))
	    {
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
			 "[%i]  UnPause Sequencer\n",
			 ahc->scsi_me->mid);
#endif
	      if (IsNotScheduled(ahc->sequencer))
		schedule_event(ahc->sequencer,
			       YS__Simtime + ahc->seq_cycle_fast);
	      ahc->seq_pause = 0;
	    }
	  
	  if (data & CHIPRST)             /* reset bit set: perform reset  */
	    {
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
			 "[%i]  Reset AHC\n", ahc->scsi_me->mid);
#endif
	      ahc_reset(ahc);
	      regs[HCNTRL] = CHIPRSTACK;
	    }

	  break;

	  
	  /*---------------------------------------------------------------*/
	  /* clear SCSI interrupt status 0 (write only)                    */
	case CLRSINT0:
	  regs[CLRSINT0] &= ~data;        /* clear register                */
	  regs[SSTAT0] &= ~data;          /* clear interrupt status bits   */

	  if (!ahc->intr_scsi_clr_done)   /* compute interrupt latency     */
	    {
	      lat = YS__Simtime - ahc->intr_scsi_start;
	      ahc->intr_scsi_clr_avg += lat;
	      if (lat > ahc->intr_scsi_clr_max)
		ahc->intr_scsi_clr_max = lat;
	      if (lat < ahc->intr_scsi_clr_min)
		ahc->intr_scsi_clr_min = lat;
	      ahc->intr_scsi_clr_done = 1;
	    }
	  
	  break;
	  


	  /*---------------------------------------------------------------*/
	  /* clear SCSI interrupt status 1 (write only)                    */
	case CLRSINT1:
	  regs[CLRSINT1] &= ~data;        /* clear register                */
	  regs[SSTAT1] &= ~data;          /* clear interrupt status bits   */

	  if (!ahc->intr_scsi_clr_done)   /* compute interrupt latency     */
	    {
	      lat = YS__Simtime - ahc->intr_scsi_start;
	      ahc->intr_scsi_clr_avg += lat;
	      if (lat > ahc->intr_scsi_clr_max)
		ahc->intr_scsi_clr_max = lat;
	      if (lat < ahc->intr_scsi_clr_min)
		ahc->intr_scsi_clr_min = lat;
	      ahc->intr_scsi_clr_done = 1;
	    }
	  
	  break;
	  


	  /*---------------------------------------------------------------*/
	  /* clear host interrupt (write only)                             */
	case CLRINT:
	  regs[CLRINT] &= ~data;          /* clear register                */
	  regs[INTSTAT] &= ~data;         /* clear interrupt status bits   */

	  if (!ahc->intr_seq_clr_done)    /* compute interrupt latency     */
	    {
	      lat = YS__Simtime - ahc->intr_seq_start;
	      ahc->intr_seq_clr_avg += lat;
	      if (lat > ahc->intr_seq_clr_max)
		ahc->intr_seq_clr_max = lat;
	      if (lat < ahc->intr_seq_clr_min)
		ahc->intr_seq_clr_min = lat;
	      ahc->intr_seq_clr_done = 1;
	    }


	  if (!ahc->intr_cmpl_clr_done)   /* compute interrupt latency     */
	    {
	      lat = YS__Simtime - ahc->intr_cmpl_start;
	      ahc->intr_cmpl_clr_avg += lat;
	      if (lat > ahc->intr_cmpl_clr_max)
		ahc->intr_cmpl_clr_max = lat;
	      if (lat < ahc->intr_cmpl_clr_min)
		ahc->intr_cmpl_clr_min = lat;
	      ahc->intr_cmpl_clr_done = 1;
	    }
	  
	  break;
	  

	  /*---------------------------------------------------------------*/
	  /* write to SCB input queue: load queue, adjust counter,         */
	  /* start seqencer                                                */
	case QINFIFO:
	  ahc_queue_shiftin(ahc->qin_fifo, data);
	  regs[QINCNT] = ahc_queue_size(ahc->qin_fifo);

#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "  Load QIN -> %i\n", regs[QINCNT]);
#endif
	  
	  if (!(regs[HCNTRL] & PAUSE) &&
	      (IsNotScheduled(ahc->sequencer)))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);

	  ahc->scbs[data].start_time = YS__Simtime;
	  ahc->request_count++;

	  break;


	  /*---------------------------------------------------------------*/
	  /* write to any SCB register: copy new byte into current SCB     */
	  /* entry and add SCB index to offset if in auto-increment mode   */
	case SCB_CONTROL:
	case SCB_TCL:
	case SCB_TARGET_STATUS:
	case SCB_SGCOUNT:
	case SCB_SGPTR0:
	case SCB_SGPTR1:
	case SCB_SGPTR2:
	case SCB_SGPTR3:
	case SCB_RESID_SGCNT:
	case SCB_RESID_DCNT0:
	case SCB_RESID_DCNT1:
	case SCB_RESID_DCNT2:
	case SCB_DATAPTR0:
	case SCB_DATAPTR1:
	case SCB_DATAPTR2:
	case SCB_DATAPTR3:
	case SCB_DATACNT0:
	case SCB_DATACNT1:
	case SCB_DATACNT2:
	case SCB_CMDPTR0:
	case SCB_CMDPTR1:
	case SCB_CMDPTR2:
	case SCB_CMDPTR3:
	case SCB_CMDLEN:
	case SCB_TAG:
	case SCB_NEXT:
	case SCB_PREV:
	  {
	    unsigned char *p;
	    p = (unsigned char*)&(ahc->scbs[regs[SCBPTR]]);
	    p += offset - SCBARRAY;
	    
	    if (regs[SCBCNT] & SCBAUTO)
	      p += (regs[SCBCNT] & SCBCNT_MASK);

	    *p = data;

	    break;
	  }

	  
	  /*---------------------------------------------------------------*/
	default:
	  break;
	}
    }

  /* end of request per-byte loop */

  
  /*-----------------------------------------------------------------------*/
  /* for writes to a SCB register, increment SCB index if auto-increment   */
  
  if ((offset >= SCBARRAY) &&
      (offset <= AHC_REG_END) &&
      (regs[SCBCNT] & SCBAUTO))
    {
      memcpy(regs + SCBARRAY,
	     &ahc->scbs[regs[SCBPTR]],
	     AHC_REG_END - SCBARRAY);
      regs[SCBCNT] =
	SCBAUTO | (((regs[SCBCNT] & SCBCNT_MASK) + olength) & SCBCNT_MASK);
    }

  return(1);
}





/*=========================================================================*/
/* PCI Mapping callback routine: called when host writes to PCI address    */
/* space register. If new value is -1 simply return required address space */
/* and flag bits, otherwise map control registers at new base address.     */
/*=========================================================================*/

void ahc_pci_map(void *controller, unsigned address,
		 int node, int module, int function, int reg,
		 unsigned *size, unsigned *flags)
{
  ahc_t *ahc = (ahc_t*)controller;


  *flags = PCI_MAPREG_TYPE_IO;              /* require I/O space           */

  if (address == 0xFFFFFFFF)                /* return size for function 0  */
    {                                       /* and register 0, otherwise   */
      if ((function == 0) && (reg == 0))    /* return 0 (unused)           */
	*size = AHC_REG_END;
      else
	*size = 0;

      return;
    }
  else                                      /* map control registers       */
    {
      if (ahc->base_addr != 0)
	{
	  AddrMap_remove(node, ahc->base_addr, ahc->base_addr + AHC_REG_END,
			 module);
	  PageTable_remove(node, ahc->base_addr);
	}

      address = PCI_MAPREG_IO_ADDR(address);
      AddrMap_insert(node, address, address + AHC_REG_END, module);
      PageTable_insert(node, address, (char*)ahc->regs);
      ahc->base_addr = address;
    }
}








/*=========================================================================*/
/* SCSI Controller Sequencer state machine                                 */
/* Controls processing of requests, SCSI bus transactions and host bus DMA */
/*=========================================================================*/

void ahc_sequencer(void)
{
  ahc_t           *ahc = (ahc_t*)EventGetArg(NULL);
  unsigned char   *regs  = ahc->regs;
  unsigned char    data;
  ahc_scb_t       *scb;
  double           lat;
  int              n;

  scb = &ahc->scbs[ahc->seq_scb];

  switch (ahc->seq_state)
    {
      /*===================================================================*/
      /* Idle state: return immediately if sequencer is halted, check for  */
      /* reconnect-SCB, otherwise take SCB off the input queue. Check if   */
      /* new request is a second non-queued request to a target.           */
      
    case SEQ_IDLE:
      if (ahc->seq_pause)
	return;

      if (regs[WAITING_SCBH] != SCB_LIST_NULL)
	{
	  data = regs[WAITING_SCBH];
	  ahc->regs[QINCNT] = ahc_queue_size(ahc->qin_fifo);
	  if ((ahc_queue_size(ahc->qin_fifo) == 0) ||
	      ((ahc_queue_size(ahc->qin_fifo) > 0) &&
	       (ahc_queue_head(ahc->qin_fifo) != data)))
	    {
	      ahc_queue_shiftin_head(ahc->qin_fifo, data);
	    }
	  regs[WAITING_SCBH] = ahc->scbs[data].next;

#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Taking entry %i off selection queue; next = %i\n",
		     ahc->scsi_me->mid, YS__Simtime, data, regs[WAITING_SCBH]);
#endif
	}

      if (ahc_queue_size(ahc->reconnect_scbs) > 0)   /* reconnect SCB ?    */
	{
	  data = ahc_queue_head(ahc->reconnect_scbs); /* take 1. elem. from Q*/
	  ahc->seq_scb = data;
	  scb = &(ahc->scbs[data]);

	  ahc_queue_shiftout(ahc->reconnect_scbs, data);

	  regs[SCBPTR]  = ahc->seq_scb;
          memcpy(regs + SCBARRAY, scb, AHC_REG_END - SCBARRAY);

#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Taking entry %i off reconnect queue; %i waiting\n",
		     ahc->scsi_me->mid, YS__Simtime, data,
		     ahc_queue_size(ahc->reconnect_scbs));
#endif

	  ahc->seq_state = SEQ_DATA;
	  schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}
      else                                       /* else check input queue */
	{
	  if (ahc_queue_size(ahc->qin_fifo) == 0)
	    return;

	  data = ahc_queue_head(ahc->qin_fifo);  /* take first elem. from Q*/
	  ahc->seq_scb = data;
	  scb = &(ahc->scbs[data]);

#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Taking entry %i off command queue; %i waiting\n",
		     ahc->scsi_me->mid, YS__Simtime, data,
		     ahc_queue_size(ahc->qin_fifo)-1);
#endif

	  if ((ahc->pending[scb->tcl >> 4] != NULL) &&
	      (!(scb->control & TAG_ENB)))       /* stall second non-queued*/
	    {                                    /* request to a target    */
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
			 "[%i] %.0f: AHC Stall additional non-queued request\n",
			 ahc->scsi_me->mid, YS__Simtime);
#endif
	      return;
	    }


	  if (regs[WAITING_SCBH] == SCB_LIST_NULL)
	    regs[WAITING_SCBH] = data;
	  else
	    {
	      unsigned char p;

	      p = regs[WAITING_SCBH];
	      while (ahc->scbs[p].next != SCB_LIST_NULL)
		p = ahc->scbs[p].next;

	      ahc->scbs[p].next = data; 
	    }

	  scb->next = SCB_LIST_NULL;
	  regs[SCBPTR]  = ahc->seq_scb;
          memcpy(regs + SCBARRAY, scb, AHC_REG_END - SCBARRAY);
	}


#ifdef AHC_TRACE
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i] SCB[%i]\n", ahc->scsi_me->mid, data);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Control  = 0x%02X\n", scb->control);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Target   = 0x%02X\n", scb->tcl);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      SegCount = 0x%02X\n", scb->sg_segment_count);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Segment  = 0x%02X%02X%02X%02X\n",
		 scb->sg_segment_ptr3, scb->sg_segment_ptr2, 
		 scb->sg_segment_ptr1, scb->sg_segment_ptr0);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Data     = 0x%02X%02X%02X%02X\n",
		 scb->data_ptr3, scb->data_ptr2, 
		 scb->data_ptr1, scb->data_ptr0);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      DataLen  = 0x00%02X%02X%02X\n",
		 scb->data_count2, scb->data_count1, 
		 scb->data_count0);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Command  = 0x%02X%02X%02X%02X\n",
		 scb->scsi_cmd_ptr3, scb->scsi_cmd_ptr2, 
		 scb->scsi_cmd_ptr1, scb->scsi_cmd_ptr0);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      CmdLen   = 0x%02X\n", scb->scsi_cmd_len);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Tag      = 0x%02X\n", scb->tag);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Next     = 0x%02X\n", scb->next);
      YS__logmsg(ahc->scsi_me->nodeid,
		 "      Prev     = 0x%02X\n", scb->prev);
#endif

      
      /* prepare for command DMA ------------------------------------------*/

      scb->dma_addr =
	(scb->scsi_cmd_ptr3 << 24) | (scb->scsi_cmd_ptr2 << 16) |
	(scb->scsi_cmd_ptr1 << 8) | scb->scsi_cmd_ptr0;
      scb->dma_length      = scb->scsi_cmd_len;
      scb->dma_sg_length   = scb->scsi_cmd_len;
      scb->dma_length_done = scb->scsi_cmd_len;
      scb->dma_buffer      = scb->command;

      if (scb->dma_length > sizeof(scb->command))
	YS__errmsg(ahc->scsi_me->nodeid,
		   "AHC[%i]: SCSI Command too long (0x%02X)",
		   ahc->scsi_me->mid, scb->dma_length);

      ahc->seq_state = SEQ_COMMAND;

      scb->status = SCB_ACTIVE;
      scb->write  = 0;

      
      
      /*===================================================================*/
      /* read SCSI command into internal buffer via DMA                    */
      /* issue host bus transactions until dma_length is 0, then wait for  */
      /* all data returns.                                                 */

    case SEQ_COMMAND:
      if (scb->dma_length > 0)                    /* need more data        */
	ahc_dma(ahc, scb);

      if (scb->dma_length_done > 0)               /* wait for data returns */
	{
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}

#ifdef AHC_TRACE
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i] %.0f: Command DMA Done\n",
		 ahc->scsi_me->mid, YS__Simtime);
#endif

      /* prepare for DMA of S/G vector if it has not been read ------------*/
      
      if ((scb->dma_segments == NULL) && (scb->sg_segment_count > 0))
	{
	  scb->dma_segments = malloc(scb->sg_segment_count *
				     sizeof(ahc_dmaseg_t));
	  if (scb->dma_segments == NULL)
	    YS__errmsg(ahc->scsi_me->nodeid,
		       "Malloc failed in %s:%i", __FILE__, __LINE__);

	  scb->dma_addr =
	    (scb->sg_segment_ptr3 << 24) | (scb->sg_segment_ptr2 << 16) |
	    (scb->sg_segment_ptr1 << 8) | scb->sg_segment_ptr0;
	  scb->dma_length      = scb->sg_segment_count * sizeof(ahc_dmaseg_t);
	  scb->dma_sg_length   = scb->sg_segment_count * sizeof(ahc_dmaseg_t);
	  scb->dma_length_done = scb->dma_length;
	  scb->dma_buffer      = (unsigned char*)scb->dma_segments;
	}
      else
	{
	  scb->dma_length      = 0;
	  scb->dma_sg_length   = 0;
	  scb->dma_length_done = 0;
	}

      ahc->seq_state = SEQ_DMA_VECTOR;

      
      
      /*===================================================================*/
      /* read DMA S/G vector via DMA                                       */
      /* issue host bus transactions until remaining length is 0, then     */
      /* wait for all data returns. If a reconnect request arrived in the  */
      /* meantime drop current request and return to idle, otherwise       */
      /* issue request on SCSI bus and remove from input queue.            */
      
    case SEQ_DMA_VECTOR:
      if (scb->dma_length > 0)                    /* need more data        */
	ahc_dma(ahc, scb);

      if (scb->dma_length_done > 0)               /* wait for data returns */
	{
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}

#ifdef AHC_TRACE
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i] %.0f: Vector DMA Done\n",
		 ahc->scsi_me->mid, YS__Simtime);

      for (n = 0; n < scb->sg_segment_count; n++)
      YS__logmsg(ahc->scsi_me->nodeid,
		 "[%i]   Segment %i: 0x%02X%02X%02X%02X 0x%02X%02X%02X%02X\n",
		 ahc->scsi_me->mid,
		 n,
		 scb->dma_segments[n].addr3,
		 scb->dma_segments[n].addr2,
		 scb->dma_segments[n].addr1,
		 scb->dma_segments[n].addr0,
		 scb->dma_segments[n].length3,
		 scb->dma_segments[n].length2,
		 scb->dma_segments[n].length1,
		 scb->dma_segments[n].length0);
#endif

      
      /* reconnect request arrived: drop current request and return to idle*/

      if (ahc_queue_size(ahc->reconnect_scbs) > 0)
	{
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: Reconnect while processing command\n",
		     ahc->scsi_me->mid, YS__Simtime);
#endif
	  
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
          ahc->seq_state = SEQ_IDLE;
	  return;
	}


      /* otherwise: issue request on SCSI bus and remove from input queue  */

      if (!ahc_scsi_command(ahc, scb))
	YS__errmsg(ahc->scsi_me->nodeid,
		   "AHC[%i]: SCSI Command failed", ahc->scsi_me->mid);

      if ((ahc_queue_size(ahc->qin_fifo) > 0) &&
	  (ahc_queue_head(ahc->qin_fifo) == ahc->seq_scb))
	{
	  ahc_queue_shiftout(ahc->qin_fifo, data);
	  ahc->regs[QINCNT] = ahc_queue_size(ahc->qin_fifo);
	  memcpy(regs + SCBARRAY,
		 &ahc->scbs[data],
		 AHC_REG_END - SCBARRAY);

	  if (!(scb->control & TAG_ENB))
	    ahc->pending[scb->tcl >> 4] = scb;

	  scb->queue_time = YS__Simtime - scb->start_time;
	}


      scb->current_segment = 0;
      scb->dma_length = scb->dma_length_done = 0;
      regs[SCSIID] |= (scb->tcl & 0xF0);


      /* prepare for data transfer, start at current S/G segment ----------*/

      ahc->seq_state = SEQ_RESPONSE;

      if (scb->sg_segment_count > 0)
	{
	  scb->dma_addr =
	    (scb->dma_segments[scb->current_segment].addr3 << 24) |
	    (scb->dma_segments[scb->current_segment].addr2 << 16) |
	    (scb->dma_segments[scb->current_segment].addr1 << 8) |
	    scb->dma_segments[scb->current_segment].addr0;

	  scb->dma_sg_length =
	    (scb->dma_segments[scb->current_segment].length3 << 24) |
	    (scb->dma_segments[scb->current_segment].length2 << 16) |
	    (scb->dma_segments[scb->current_segment].length1 << 8) |
	    scb->dma_segments[scb->current_segment].length0;

	  scb->dma_buffer = scb->data;
	}
      else
	{
	  scb->data          = NULL;
	  scb->dma_buffer    = NULL;
	  scb->dma_addr      = 0;
	  scb->dma_sg_length = 0;
	}



      /*===================================================================*/
      /* Wait for SCSI device response.                                    */

    case SEQ_RESPONSE:

      if (scb->status == SCB_ACTIVE)     /* keep polling while no response */
	{
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}

      lat = YS__Simtime - scb->start_time;
      if (lat > ahc->request_connect_time_max)
	ahc->request_connect_time_max = lat;
      if (lat < ahc->request_connect_time_min)
	ahc->request_connect_time_min = lat;
      ahc->request_connect_time_avg += lat;

      regs[WAITING_SCBH] = ahc->scbs[regs[WAITING_SCBH]].next;

      if (scb->status == SCB_ERROR)      /* SCSI error (timeout): finish   */
	{                                /* request and return to idle     */
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Command Error\n",
		     ahc->scsi_me->mid, YS__Simtime);
#endif
	  ahc_scsi_complete(ahc, scb);
	  ahc->seq_state = SEQ_IDLE;
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}


      
      if (scb->status == SCB_BUSY)       /* device busy: finish request    */
	{                                /* and return to idle             */
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Command Busy\n",
		     ahc->scsi_me->mid, YS__Simtime);
#endif
	  ahc_scsi_complete(ahc, scb);
	  ahc->seq_state = SEQ_IDLE;
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + ahc->seq_cycle_read * 50);
	  return;	  
	}      



      if (scb->status == SCB_DISCONNECT) /* SCSI disconnect: return to idle*/
	{
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Command Disconnect\n",
		     ahc->scsi_me->mid, YS__Simtime);
#endif
	  ahc->seq_state = SEQ_IDLE;
	  scb->control |= 0x04;
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + ahc->seq_cycle_fast);
	  ahc->request_disconnect_count++;
	  return;
	}    


      if (scb->status == SCB_CONNECT)    /* SCSI connect: start data xfer  */
	{
	  ahc->seq_state = SEQ_DATA;
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + (scb->write ?
			   ahc->seq_cycle_read :
			   ahc->seq_cycle_write));
	  return;	  
	}
      

      if (scb->status == SCB_COMPLETE)  /* request complete: finish it and */
	{                               /* return to idle                  */
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "%.0f: AHC Command Complete %i (no DMA)\n",
		     YS__Simtime, scb->status);
#endif
	  ahc_scsi_complete(ahc, scb);
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + (scb->write ?
			   ahc->seq_cycle_read :
			   ahc->seq_cycle_write));
	  ahc->seq_state = SEQ_IDLE;
	  return;	  
	}      


      
      /*===================================================================*/
      /* data transfer: dma_length is set by connect/reconnect request,    */
      /* issue host transactions until length is 0, if at end of current   */
      /* segment move to next segment, check status at end of transfer.    */
      
    case SEQ_DATA:

      if (scb->dma_length > 0)
	ahc_dma(ahc, scb);


      /* at end of segment, more data and more segments: go to next segment*/
      if ((scb->dma_sg_length <= 0) &&
	  (scb->current_segment + 1 < scb->sg_segment_count))
	{
	  scb->current_segment++;
          scb->dma_sg_length =
	    (scb->dma_segments[scb->current_segment].length3 << 24) |
	    (scb->dma_segments[scb->current_segment].length2 << 16) |
	    (scb->dma_segments[scb->current_segment].length1 << 8) |
	    scb->dma_segments[scb->current_segment].length0;

	  scb->dma_addr =
	    (scb->dma_segments[scb->current_segment].addr3 << 24) |
	    (scb->dma_segments[scb->current_segment].addr2 << 16) |
	    (scb->dma_segments[scb->current_segment].addr1 << 8) |
	    scb->dma_segments[scb->current_segment].addr0;
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Next Segment %i of %i -> 0x%08X 0x%08X\n",
		     ahc->scsi_me->mid,
		     YS__Simtime, scb->current_segment, scb->sg_segment_count,
		     scb->dma_addr, scb->dma_sg_length);
#endif
	}


      if (scb->dma_length_done > 0)       /* wait for completion           */
	{
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + (scb->write ?
			   ahc->seq_cycle_read :
			   ahc->seq_cycle_write));
	  return;
	}


      /* request is complete: finish it and return to idle ----------------*/

      if (scb->status == SCB_COMPLETE)
	{
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Command Complete %i %i\n",
		     ahc->scsi_me->mid, YS__Simtime, scb->status, ahc_queue_size(ahc->reconnect_scbs));
#endif
	  ahc_scsi_complete(ahc, scb);
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer,
			   YS__Simtime + (scb->write ?
			   ahc->seq_cycle_read :
			   ahc->seq_cycle_write));
	  ahc->seq_state = SEQ_IDLE;
	  return;	  
	}


      if (scb->status == SCB_DISCONNECT)
	{
	  ahc->seq_state = SEQ_IDLE;
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}
	

      
      /* no request status (neither complete nor save_data_ptr): poll -----*/
      
      if (scb->status == SCB_CONNECT)
	{
	  if (IsNotScheduled(ahc->sequencer))
	    schedule_event(ahc->sequencer, YS__Simtime + ahc->seq_cycle_fast);
	  return;
	}
	

      break;
      

      
      /*-------------------------------------------------------------------*/
    default:
      YS__errmsg(ahc->scsi_me->nodeid,
		 "AHC[%i]: Unknown SCSI Sequencer State %i\n",
		 ahc->scsi_me->mid, ahc->seq_state);
    }
}





/*=========================================================================*/
/* Perform a pending DMA transaction                                       */
/* copy data between internal buffer and main memory.                      */
/* Called when request is completed at caches/memory.                      */
/*=========================================================================*/

static void ahc_perform(REQ *req)
{
  ahc_scb_t *scb = (ahc_scb_t*)req->d.mem.aux;
  char *addr;
  int   n;

  addr = PageTable_lookup(req->node, req->paddr);
  if (addr == NULL)
    YS__errmsg(req->node,
	       "AHC: DMA to/from non-existing memory location 0x%08X\n",
	       req->paddr);

  if (req->prcr_req_type == WRITE)
    memcpy(addr, req->d.mem.buf, req->d.mem.count);
  else
    memcpy(req->d.mem.buf, addr, req->d.mem.count);

  scb->dma_length_done -= req->d.mem.count;
}


void ahc_complete(REQ *req, HIT_TYPE ht)
{

}





/*=========================================================================*/
/* Issue a host bus DMA transaction: Determine size based on remaining     */
/* length and address aligmnent, determine read/write, set physical address*/
/* and completion callback routines. Issue to generic SCSI controller.     */
/* Adjust DMA address, buffer pointer and length before returning.         */
/*=========================================================================*/

void ahc_dma(ahc_t *ahc, ahc_scb_t *scb)
{
  REQ *req;

  req = (REQ*)YS__PoolGetObj(&YS__ReqPool);
  memset(req, 0, sizeof(REQ));

  req->vaddr = scb->dma_addr;
  req->paddr = scb->dma_addr;
  req->size  = ARCH_linesz2;
  
  req->perform     = ahc_perform;
  req->complete    = ahc_complete;
  req->d.mem.buf   = scb->dma_buffer;
  req->d.mem.aux   = scb;
  req->d.mem.count = min(min(scb->dma_length, scb->dma_sg_length),
			 ARCH_linesz2 - (scb->dma_addr % ARCH_linesz2));

  if (scb->write)
    req->prcr_req_type = WRITE;
  else
    req->prcr_req_type = READ;
  
  if (!SCSI_cntl_host_issue(ahc->scsi_me, req))
    {
      YS__PoolReturnObj(&YS__ReqPool, req);
      return;
    }

  scb->dma_addr      += req->d.mem.count;
  scb->dma_buffer    += req->d.mem.count;
  scb->dma_length    -= req->d.mem.count;
  scb->dma_sg_length -= req->d.mem.count;

  if (req->d.mem.count <= 0)
    YS__errmsg(ahc->scsi_me->nodeid,
	       "AHC[%i]: Invalid DMA Transaction size %i 0x%08X in state %i\n",
	       ahc->scsi_me->mid, req->d.mem.count, req->paddr, ahc->seq_state);
  
#ifdef AHC_TRACE
  YS__logmsg(ahc->scsi_me->nodeid,
	     "[%i] %.0f: DMA %s (%s) 0x%08X %i bytes -> %i %i %p\n",
	     ahc->scsi_me->mid, YS__Simtime,
	     scb->write ? "Write" : "Read",
	     req->type == WRITEPURGE ? "WRITE_PURGE" : ReqName[req->req_type],
	     req->paddr,
	     req->d.mem.count,
	     scb->dma_length,
	     scb->dma_sg_length, req->d.mem.buf);
#endif
}






/*=========================================================================*/
/* Assemble and issue SCSI request based on command vector. Allocate       */
/* request structure, fill in initiator, target and lun, set queue tags    */
/* and decode command, including length and block size. Also set DMA       */
/* direction and allocate data buffer.                                     */
/*=========================================================================*/

int ahc_scsi_command(ahc_t *ahc, ahc_scb_t *scb)
{
  SCSI_REQ *req;
  int       length = 0;


  if (scb->tcl & SELBUSB)
    YS__errmsg(ahc->scsi_me->nodeid,
	       "AHC[%i]: Attempt to issue SCSI command on bus B",
	       ahc->scsi_me->mid);


  req = (SCSI_REQ*)YS__PoolGetObj(&YS__ScsiReqPool);
  req->initiator    = ahc->scsi_id;
  req->target       = scb->tcl >> 4;
  req->lun          = scb->tcl & 0x07;
  req->parent       = NULL;
  
  if (scb->control & DISCENB)
    req->imm_flag   = SCSI_FLAG_TRUE;
  else
    req->imm_flag   = SCSI_FLAG_FALSE;

  if (scb->control & TAG_ENB)
    {
      if ((scb->control & SCB_TAG_TYPE) == 0x00)
	req->queue_msg = SIMPLE_QUEUE;
      if ((scb->control & SCB_TAG_TYPE) == 0x01)
	req->queue_msg = HEAD_OF_QUEUE;
      if ((scb->control & SCB_TAG_TYPE) == 0x02)
	req->queue_msg = ORDER_QUEUE;
      if ((scb->control & SCB_TAG_TYPE) == 0x03)
	YS__errmsg(ahc->scsi_me->nodeid,
		   "AHC[%i]: Invalid queue type in SCSI command",
		   ahc->scsi_me->mid);
    }
  else
    req->queue_msg = NO_QUEUE;

  req->queue_tag   = scb->tag;


  /* decode command -------------------------------------------------------*/

  switch (scb->command[0])
    {
    case TEST_UNIT_READY:
    case START_STOP:
    case PREVENT_ALLOW:
      req->request_type           = SCSI_REQ_MISC;
      req->length                 = 0;
      req->lba                    = 0;
      break;


    case REQUEST_SENSE:
      req->request_type           = SCSI_REQ_REQUEST_SENSE;
      req->length                 = scb->command[4];
      req->lba                    = 0;
      req->aux                    = 0;
      scb->write                  = 1;
      length                      = req->length;
      break;

      
    case SCSI_READ_COMMAND:
      req->request_type           = SCSI_REQ_READ;
      req->length                 = scb->command[4] > 0 ? scb->command[4]:256;
      req->lba                    =
	((scb->command[1] & 0x1F) << 16) |
	(scb->command[2] << 8) |
	scb->command[3];
      scb->write                  = 1;
      length                      = req->length * SCSI_BLOCK_SIZE;
      break;

      
    case SCSI_WRITE_COMMAND:
      req->request_type           = SCSI_REQ_WRITE;
      req->length                 = scb->command[4] > 0 ? scb->command[4]:256;
      req->lba                    =
	((scb->command[1] & 0x1F) << 16) |
	(scb->command[2] << 8) |
	scb->command[3];
      scb->write                  = 0;
      length                      = req->length * SCSI_BLOCK_SIZE;
      break;

      
    case INQUIRY:
      req->request_type           = SCSI_REQ_INQUIRY;
      req->length                 = scb->command[4];
      req->lba                    = 0;
      scb->write                  = 1;
      length                      = req->length;
      break;


    case SCSI_MODE_SENSE:
      req->request_type           = SCSI_REQ_MODE_SENSE;
      req->length                 = scb->command[4];
      req->lba                    = 0;
      req->aux                    = scb->command[2];
      scb->write                  = 1;
      length                      = req->length;
      break;

      
    case READ_CAPACITY:
      req->request_type           = SCSI_REQ_READ_CAPACITY;
      req->length                 = 8;
      req->lba                    =
	(scb->command[2] << 24) | (scb->command[3] << 16) |
	(scb->command[4] << 8) | (scb->command[5]);
      scb->write                  = 1;
      length                      = req->length;
      break;

      
    case READ_BIG:
      req->request_type           = SCSI_REQ_READ;
      req->length                 = (scb->command[7] << 8) | scb->command[8];
      req->lba                    =
	(scb->command[2] << 24) | (scb->command[3] << 16) |
	(scb->command[4] << 8) | scb->command[5];
      scb->write                  = 1;
      length                      = req->length * SCSI_BLOCK_SIZE;
      break;


    case WRITE_BIG:
      req->request_type           = SCSI_REQ_WRITE;
      req->length                 = (scb->command[7] << 8) | scb->command[8];
      req->lba                    =
	(scb->command[2] << 24) | (scb->command[3] << 16) |
	(scb->command[4] << 8) | scb->command[5];
      scb->write                  = 0;
      length                      = req->length * SCSI_BLOCK_SIZE;
      break;


    case SCSI_SYNCHRONIZE_CACHE:
      req->request_type           = SCSI_REQ_SYNC_CACHE;
      req->length                 = (scb->command[7] << 8) | scb->command[8];
      req->lba                    =
	(scb->command[2] << 24) | (scb->command[3] << 16) |
	(scb->command[4] << 8) | scb->command[5];
      req->imm_flag               = scb->command[1] & 0x02;
      scb->write                  = 0;
      length                      = 0;
      break;


    default:
      YS__errmsg(ahc->scsi_me->nodeid,
		 "AHC[%i]: Invalid SCSI command 0x%02X",
		 ahc->scsi_me->mid, scb->command[0]);
    }


  /* allocate buffer if data length greater then 0 ------------------------*/
  
  if (length > 0)
    scb->data = malloc(length);
  else
    scb->data = NULL;

  req->buf = (char*)scb->data;

#ifdef AHC_TRACE
  YS__logmsg(ahc->scsi_me->nodeid,
	     "[%i] %.0f: Start SCSI Command %s (0x%02X) Target %i Lun %i Sector %i Size %i\n",
	     ahc->scsi_me->mid, YS__Simtime,
	     SCSI_ReqName[req->request_type], req->request_type, req->target,
	     req->lun, req->lba, req->length);
#endif
  
  return(SCSI_device_request(ahc->scsi_me->scsi_me, req));
}





/*=========================================================================*/
/* SCSI adapter won bus - do nothing right now                             */
/*=========================================================================*/

void ahc_scsi_wonbus(void *controller, SCSI_REQ *req)
{
  ahc_t         *ahc = (ahc_t*)controller;
}




/*=========================================================================*/
/* Request sent to SCSI adapter - not supported                            */
/*=========================================================================*/

void ahc_scsi_request(void *controller, SCSI_REQ *req)
{
  ahc_t         *ahc = (ahc_t*)controller;

  YS__errmsg(ahc->scsi_me->nodeid,
	     "AHC[%i]: Request to SCSI controller not supported",
	     ahc->scsi_me->mid);
}




/*=========================================================================*/
/* SCSI adapter received response:                                         */
/*=========================================================================*/

void ahc_scsi_response(void *controller, SCSI_REQ *req)
{
  ahc_t         *ahc = (ahc_t*)controller;
  ahc_scb_t     *scb;
  int            n;
  unsigned char  tcl;
  unsigned char *regs = ahc->regs;

#ifdef AHC_TRACE
  YS__logmsg(ahc->scsi_me->nodeid,
	     "[%i] %.0f AHC: Response %s %i %i (%s)\n",
	     ahc->scsi_me->mid, YS__Simtime,
	     SCSI_RepName[req->reply_type],
	     req->start_block,
	     req->current_data_size,
	     SCSI_ReqName[req->orig_request]);
#endif
  
  tcl = req->target << 4 | req->lun;

  scb = NULL;
  for (n = 0; n < ahc->max_scbs; n++)
    if (req->queue_msg == NO_QUEUE)
      {
	if ((ahc->scbs[n].status != SCB_INACTIVE) &&
	    (ahc->scbs[n].tcl == tcl))
	  {
	    scb = &ahc->scbs[n];
	    break;
	  }
      }
    else
      {
	if ((ahc->scbs[n].status != SCB_INACTIVE) &&
	    (ahc->scbs[n].tag == req->queue_tag) &&
	    (ahc->scbs[n].tcl == tcl))
	  {
	    scb = &ahc->scbs[n];
	    break;
	  }
      }

  if (scb == NULL)
    YS__errmsg(ahc->scsi_me->nodeid,
	       "AHC[%i]: SCSI Response to non-existent SCB (%02X)",
	       ahc->scsi_me->mid, tcl);


  
  switch (req->reply_type)
    {
    case SCSI_REP_COMPLETE:
      if (IsNotScheduled(ahc->sequencer))
	schedule_event(ahc->sequencer,
		       YS__Simtime + ahc->seq_cycle_fast);

      /* ahc->seq_state = SEQ_RESPONSE; */
      /*      ahc_shiftin_queue(ahc->reconnect_scbs, scb - ahc->scbs); */
      scb->status = SCB_COMPLETE;
      scb->req    = req;
      break;


    case SCSI_REP_TIMEOUT:
    case SCSI_REP_REJECT:
      scb->status = SCB_ERROR;
      scb->req    = req;
      break;


    case SCSI_REP_BUSY:
      scb->status = SCB_BUSY;
      scb->req    = req;
      break;
      

    case SCSI_REP_CONNECT:
      if (req->request_type == SCSI_REQ_RECONNECT)
	{
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: AHC Reconnect %i %i %i %i\n",
		     ahc->scsi_me->mid, YS__Simtime,
		     IsNotScheduled(ahc->sequencer),
		     ahc->seq_state, ahc->seq_scb, n);
#endif
	  if (IsNotScheduled(ahc->sequencer))
	    {
	      schedule_event(ahc->sequencer,
			     YS__Simtime + ahc->seq_cycle_fast);
	    }

	  if (!((ahc->seq_state == SEQ_DATA) && (ahc->seq_scb == n)))
            {
  	      ahc_queue_shiftin(ahc->reconnect_scbs, n);
            }
	}

      scb->status           = SCB_CONNECT;
      scb->dma_length      += req->current_data_size;
      scb->dma_length_done += req->current_data_size;
      scb->req              = req;

      if ((req->orig_request == SCSI_REQ_READ) &&
	  (req->reply_type == SCSI_REP_CONNECT))
	SCSI_bus_perform(ahc->scsi_me->scsi_bus, req);  

      break;
      
      
    case SCSI_REP_SAVE_DATA_POINTER:
    case SCSI_REP_DISCONNECT:
      scb->status = SCB_DISCONNECT;
      scb->req    = req;

      if (req->orig_request == SCSI_REQ_READ)
	SCSI_bus_perform(ahc->scsi_me->scsi_bus, req);  

      if (req->reply_type == SCSI_REP_DISCONNECT)
	{
	  YS__PoolReturnObj(&YS__ScsiReqPool, req);
	  scb->req = NULL;
	}

      break;



    default:
      YS__errmsg(ahc->scsi_me->nodeid,
		 "AHC[%i]: Unknown SCSI response %i",
		 ahc->scsi_me->mid, req->reply_type);
    }
}





/*=========================================================================*/
/* Complete a request: insert it into the output FIFO if successful,       */
/* interrupt host in any case (if interrupts are enabled) and compute      */
/* statistics. Free internal buffers and return request to pool.           */
/* If request was rejected with 'busy' status return it to the head of the */
/* input FIFO.                                                             */
/*=========================================================================*/

int ahc_scsi_complete(ahc_t *ahc, ahc_scb_t *scb)
{
  unsigned char *regs = ahc->regs;
  double lat;

  if (scb->status == SCB_COMPLETE)
    {
      if (!(regs[INTSTAT] & INT_PEND) && (regs[HCNTRL] & INTEN))
	{
	  SCSI_cntl_host_interrupt(ahc->scsi_me);
	  ahc->intr_cmpl_count++;
	  ahc->intr_cmpl_start    = YS__Simtime;
	  ahc->intr_cmpl_lat_done = 0;
	  ahc->intr_cmpl_clr_done = 0;
#ifdef AHC_TRACE
	  YS__logmsg(ahc->scsi_me->nodeid,
		     "[%i] %.0f: Complete Interrupt\n",
		     ahc->scsi_me->mid, YS__Simtime);
#endif
	}

      regs[INTSTAT] |= CMDCMPLT;

      ahc_queue_shiftin(ahc->qout_fifo, ahc->seq_scb);
      regs[QOUTCNT] = ahc_queue_size(ahc->qout_fifo);
      regs[QOUTFIFO] = ahc_queue_head(ahc->qout_fifo);

      if (scb->req->orig_request == SCSI_REQ_WRITE)
	SCSI_bus_perform(ahc->scsi_me->scsi_bus, scb->req);

      ahc->request_complete_count++;
    }



  if (scb->status == SCB_ERROR)
    {
      switch (scb->req->reply_type)
	{
	case SCSI_REP_TIMEOUT:
	  if (!(regs[INTSTAT] & INT_PEND) &&
	      (regs[HCNTRL] & INTEN) &&
	      (regs[SIMODE1] & ENSELTIMO))
	    {
	      SCSI_cntl_host_interrupt(ahc->scsi_me);
	      ahc->intr_scsi_count++;
	      ahc->intr_scsi_start    = YS__Simtime;
	      ahc->intr_scsi_lat_done = 0;
	      ahc->intr_scsi_clr_done = 0;
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
			 "[%i] %.0f: SCSI Interrupt\n",
			 ahc->scsi_me->mid, YS__Simtime);
#endif
	    }

	  regs[INTSTAT] |= SCSIINT;
	  regs[SSTAT1] |= SELTO;		
	  ahc->seq_pause = 1;
      	  break;




	case SCSI_REP_REJECT:
	  if (!(regs[INTSTAT] & INT_PEND) && (regs[HCNTRL] & INTEN))
	    {
	      SCSI_cntl_host_interrupt(ahc->scsi_me);
	      ahc->intr_seq_count++;
	      ahc->intr_seq_start    = YS__Simtime;
	      ahc->intr_seq_lat_done = 0;
	      ahc->intr_seq_clr_done = 0;
#ifdef AHC_TRACE
	      YS__logmsg(ahc->scsi_me->nodeid,
		      "[%i] %.0f: Sequencer Interrupt\n",
		      ahc->scsi_me->mid, YS__Simtime);
#endif
	    }

	  regs[INTSTAT] |= BAD_STATUS;
	  scb->target_status = SCSI_CHECK;
	  memcpy(regs + SCBARRAY, scb, AHC_REG_END - SCBARRAY);
	  ahc->seq_pause = 1;
	  break;


	default:
	  YS__errmsg(ahc->scsi_me->nodeid,
		     "AHC[%i]: Unknown SCSI Bus Reply %i\n",
		     ahc->scsi_me->mid, scb->req->reply_type);
	  break;
	}
    }


  lat = YS__Simtime - scb->start_time;
  if (lat > ahc->request_complete_time_max)
    ahc->request_complete_time_max = lat;
  if (lat < ahc->request_complete_time_min)
    ahc->request_complete_time_min = lat;
  ahc->request_complete_time_avg += lat;

  if (scb->status == SCB_BUSY)
    {
      ahc_queue_shiftin_head(ahc->qin_fifo, ahc->seq_scb);
      ahc->regs[QINCNT] = ahc_queue_size(ahc->qin_fifo);
    }
  else
    {
      if (scb->queue_time > ahc->request_queue_time_max)
	ahc->request_queue_time_max = scb->queue_time;
      if (scb->queue_time < ahc->request_queue_time_min)
	ahc->request_queue_time_min = scb->queue_time;
      ahc->request_queue_time_avg += scb->queue_time;
    }

  

  if (scb->dma_segments != NULL)
    free(scb->dma_segments);
  scb->dma_segments = NULL;

  if (scb->data != NULL)
    free(scb->data);
  scb->data = NULL;

  scb->status = SCB_INACTIVE;

  if (!(scb->control & TAG_ENB) &&
      (ahc->pending[scb->tcl >> 4] == scb))
    ahc->pending[scb->tcl >> 4] = NULL;

  YS__PoolReturnObj(&YS__ScsiReqPool, scb->req);
  scb->req = NULL;

  return(0);
}




/*=========================================================================*/
/* Print configuration of an Adaptec SCSI controller.                      */
/*=========================================================================*/

void ahc_print_params(void *controller)
{
  ahc_t *ahc = (ahc_t*)controller;
  int nid    = ahc->scsi_me->scsi_bus->node_id;

  YS__statmsg(nid, "Adaptec SCSI Controller Configuration\n");

  YS__statmsg(nid, "  Number of SCBs: %i\n", ahc->max_scbs);

  YS__statmsg(nid, "\n");
}



/*=========================================================================*/
/* Report statistics for an Adaptec SCSI controller.                       */
/*=========================================================================*/

void ahc_stat_report(void *controller)
{
  ahc_t *ahc = (ahc_t*)controller;
  int nid    = ahc->scsi_me->scsi_bus->node_id;

  if (ahc->intr_seq_count > 0)
    {
      ahc->intr_seq_lat_avg /= ahc->intr_seq_count;
      ahc->intr_seq_clr_avg /= ahc->intr_seq_count;

      YS__statmsg(nid, "    Sequencer Interrupt:\n");
      YS__statmsg(nid, "      %i Interrupts\n", ahc->intr_cmpl_count);

      YS__statmsg(nid, "      Interrupt handler latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_seq_lat_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_seq_lat_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_seq_lat_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");

      YS__statmsg(nid, "      Interrupt clear latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_seq_clr_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_seq_clr_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_seq_clr_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");
    }


  if (ahc->intr_cmpl_count > 0)
    {
      ahc->intr_cmpl_lat_avg /= ahc->intr_cmpl_count;
      ahc->intr_cmpl_clr_avg /= ahc->intr_cmpl_count;

      YS__statmsg(nid, "    Command Complete Interrupt:\n");
      YS__statmsg(nid, "      %i Interrupts\n", ahc->intr_cmpl_count);

      YS__statmsg(nid, "      Interrupt handler latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_cmpl_lat_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_cmpl_lat_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_cmpl_lat_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");

      YS__statmsg(nid, "      Interrupt clear latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_cmpl_clr_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_cmpl_clr_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_cmpl_clr_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");
    }


  if (ahc->intr_scsi_count > 0)
    {
      ahc->intr_scsi_lat_avg /= ahc->intr_scsi_count;
      ahc->intr_scsi_clr_avg /= ahc->intr_scsi_count;

      YS__statmsg(nid, "    SCSI Interrupt:\n");
      YS__statmsg(nid, "      %i Interrupts\n", ahc->intr_scsi_count);

      YS__statmsg(nid, "      Interrupt handler latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_scsi_lat_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_scsi_lat_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_scsi_lat_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");

      YS__statmsg(nid, "      Interrupt clear latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->intr_scsi_clr_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->intr_scsi_clr_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->intr_scsi_clr_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");
    }

  YS__statmsg(nid, "\n");

  if (ahc->request_count > 0)
    {
      ahc->request_queue_time_avg    /= ahc->request_count;
      ahc->request_connect_time_avg  /= ahc->request_count;
      ahc->request_complete_time_avg /= ahc->request_count;
      
      YS__statmsg(nid,
	      "    %i Requests;  %i disconnected;  %i completed\n",
	      ahc->request_count,
	      ahc->request_disconnect_count,
	      ahc->request_complete_count);

      YS__statmsg(nid, "      Input Queue Latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->request_queue_time_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->request_queue_time_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->request_queue_time_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");

      YS__statmsg(nid, "      Initial Connect Latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->request_connect_time_min * (double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->request_connect_time_avg * (double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->request_connect_time_max * (double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");

      YS__statmsg(nid, "      Completion Latency:\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->request_complete_time_min*(double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->request_complete_time_avg*(double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->request_complete_time_max*(double)CPU_CLK_PERIOD/1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");
    }

  if (ahc->request_complete_count > 0)
    {
      ahc->request_total_time_avg /= ahc->request_complete_count;
      
      YS__statmsg(nid,
	      "      Total Latency (including output queue time):\n");
      YS__statmsg(nid, "          Min: ");
      PrintTime(ahc->request_total_time_min * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Avg: ");
      PrintTime(ahc->request_total_time_avg * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, ";  Max: ");
      PrintTime(ahc->request_total_time_max * (double)CPU_CLK_PERIOD / 1.0e12,
		statfile[nid]);
      YS__statmsg(nid, "\n");
    }

      
  YS__statmsg(nid, "\n");
}




/*=========================================================================*/
/* Clear statistics for one Adaptec SCSI controller.                       */
/*=========================================================================*/

void ahc_stat_clear(void *controller)
{
  ahc_t *ahc = (ahc_t*)controller;

  ahc->intr_seq_count     = 0;
  ahc->intr_seq_start     = 0.0;
  ahc->intr_seq_lat_done  = 1;
  ahc->intr_seq_lat_max   = 0.0;
  ahc->intr_seq_lat_avg   = 0.0;
  ahc->intr_seq_lat_min   = MAXDOUBLE;
  ahc->intr_seq_clr_done  = 1;
  ahc->intr_seq_clr_max   = 0.0;
  ahc->intr_seq_clr_avg   = 0.0;
  ahc->intr_seq_clr_min   = MAXDOUBLE;
  
  ahc->intr_cmpl_count    = 0;
  ahc->intr_cmpl_start    = 0.0;
  ahc->intr_cmpl_lat_done = 1;
  ahc->intr_cmpl_lat_max  = 0.0;
  ahc->intr_cmpl_lat_avg  = 0.0;
  ahc->intr_cmpl_lat_min  = MAXDOUBLE;
  ahc->intr_cmpl_clr_done = 1;
  ahc->intr_cmpl_clr_max  = 0.0;
  ahc->intr_cmpl_clr_avg  = 0.0;
  ahc->intr_cmpl_clr_min  = MAXDOUBLE;
  
  ahc->intr_scsi_count    = 0;
  ahc->intr_scsi_start    = 0.0;
  ahc->intr_scsi_lat_done = 1;
  ahc->intr_scsi_lat_max  = 0.0;
  ahc->intr_scsi_lat_avg  = 0.0;
  ahc->intr_scsi_lat_min  = MAXDOUBLE;
  ahc->intr_scsi_clr_done = 1;
  ahc->intr_scsi_clr_max  = 0.0;
  ahc->intr_scsi_clr_avg  = 0.0;
  ahc->intr_scsi_clr_min  = MAXDOUBLE;

  ahc->request_count             = 0;
  ahc->request_disconnect_count  = 0;
  ahc->request_complete_count    = 0;
  ahc->request_queue_time_max    = 0.0;
  ahc->request_queue_time_avg    = 0.0;
  ahc->request_queue_time_min    = MAXDOUBLE;
  ahc->request_connect_time_max  = 0.0;
  ahc->request_connect_time_avg  = 0.0;
  ahc->request_connect_time_min  = MAXDOUBLE;
  ahc->request_complete_time_max = 0.0;
  ahc->request_complete_time_avg = 0.0;
  ahc->request_complete_time_min = MAXDOUBLE;
  ahc->request_total_time_max    = 0.0;
  ahc->request_total_time_avg    = 0.0;
  ahc->request_total_time_min    = MAXDOUBLE;
}



/*=========================================================================*/
/* Dump debug information about AHC controller                             */
/*=========================================================================*/

void AHC_dmaseg_dump(ahc_dmaseg_t *seg, int nid)
{
  YS__logmsg(nid, "    addr(0x%02x%02x%02x%02x), length(0x%02x%02x%02x%02x)\n",
	     seg->addr3, seg->addr2, seg->addr1, seg->addr0,
	     seg->length3, seg->length2, seg->length1, seg->length0);
}

void AHC_scb_dump(ahc_scb_t *scb, int nid)
{
  int n;
  
  YS__logmsg(nid, "  control(0x%02x),  tcl(0x%02x)\n",
	     scb->control, scb->tcl);
  YS__logmsg(nid, "  target_status(0x%02x),  sg_segment_count(%d)\n",
	     scb->target_status, scb->sg_segment_count);
  YS__logmsg(nid, "  sg_segment_ptr(0x%02x%02x%02x%02x)\n",
	     scb->sg_segment_ptr3, scb->sg_segment_ptr2,
	     scb->sg_segment_ptr1, scb->sg_segment_ptr0);
  
  if (scb->dma_segments)
    for (n = 0; n < scb->sg_segment_count; n++)
      AHC_dmaseg_dump(&(scb->dma_segments[n]), nid);
  
  YS__logmsg(nid,
	     "  res_sg_segment_count(%d), res_data_count(0x%02x%02x%02x)\n",
	     scb->res_sg_segment_count, scb->res_data_count2,
	     scb->res_data_count1, scb->res_data_count0);

  YS__logmsg(nid, "  data_ptr(0x%02x%02x%02x%02x)\n",
	     scb->data_ptr3, scb->data_ptr2, scb->data_ptr1, scb->data_ptr0);
  YS__logmsg(nid, "  data_count(0x%02x%02x%02x)\n",
	     scb->data_count2, scb->data_count1, scb->data_count0);

  YS__logmsg(nid, "  scsi_cmd_ptr(0x%02x%02x%02x%02x)\n",
	     scb->scsi_cmd_ptr3, scb->scsi_cmd_ptr2,
	     scb->scsi_cmd_ptr1, scb->scsi_cmd_ptr0);
  YS__logmsg(nid, "  scsi_cmd_len(%d), tag(0x%02x), next(%d), prev(%d)\n",
	     scb->scsi_cmd_len, scb->tag, scb->next, scb->prev);

  YS__logmsg(nid, "  command(0x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x)\n",
	     scb->command[0],  scb->command[1],
	     scb->command[2],  scb->command[3],
	     scb->command[4],  scb->command[5],
	     scb->command[6],  scb->command[7],
	     scb->command[8],  scb->command[9],
	     scb->command[10], scb->command[11],
	     scb->command[12], scb->command[13],
	     scb->command[14], scb->command[15]);
  YS__logmsg(nid, "  write(%d), dma_addr(0x%x), dma_length(0x%x)\n",
	     scb->write, scb->dma_addr, scb->dma_length);
  YS__logmsg(nid, "  dma_sg_length(0x%x), dma_length_done(0x%x)\n",
	     scb->dma_sg_length, scb->dma_length_done);
}


void ahc_dump(void *controller)
{
  ahc_t *ahc = (ahc_t*)controller;
  int nid    = ahc->scsi_me->scsi_bus->node_id;
  int n;

  YS__logmsg(nid, "\n============== AHC SCSI CONTROLLER ================\n");
  YS__logmsg(nid, "seq_state(%d), sequencer scheduled: %s\n",
	     ahc->seq_state, IsScheduled(ahc->sequencer) ? "yes" : "no");

  for (n = 0; n < ahc->max_scbs; n++)
    {
      YS__logmsg(nid, "scb[%i]\n", n);
      AHC_scb_dump(&(ahc->scbs[n]), nid);
    }

  YS__logmsg(nid, "qin_fifo size(%d)\n", ahc->qin_fifo.size);
  for (n = ahc->qin_fifo.head; n != ahc->qin_fifo.tail; n = (n +1) & 0xff)
    YS__logmsg(nid, "  %d ", ahc->qin_fifo.elements[n]);

  YS__logmsg(nid, "\nqout_fifo size(%d)\n", ahc->qout_fifo.size);
  for (n = ahc->qout_fifo.head; n != ahc->qout_fifo.tail; n = (n +1) & 0xff)
    YS__logmsg(nid, "  %d ", ahc->qout_fifo.elements[n]);

  YS__logmsg(nid, "\nreconnect_scbs size(%d)\n", ahc->reconnect_scbs.size);
  for (n = ahc->reconnect_scbs.head;
       n != ahc->reconnect_scbs.tail;
       n = (n +1) & 0xff)
    YS__logmsg(nid, "  %d ", ahc->reconnect_scbs.elements[n]);
  YS__logmsg(nid, "\n");
  
  for (n = 0; n < SCSI_WIDTH*8; n++)
    {
      if (ahc->pending[n] == NULL)
	continue;
      YS__logmsg(nid, "pending[%i]\n", n);
      AHC_scb_dump(ahc->pending[n], nid);
    }

  YS__logmsg(nid, "scsiseq(0x%02x)    sxfrctl0(0x%2x)\n",
	     ahc->regs[SCSISEQ], ahc->regs[SXFRCTL0]);
  YS__logmsg(nid, "sxfrctl1(0x%02x)   scsisigi(0x%02x)\n",
	     ahc->regs[SXFRCTL1], ahc->regs[SCSISIGI]);
  YS__logmsg(nid, "scsisigo(0x%02x)   scsirate(0x%02x)\n",
	     ahc->regs[SCSISIGO], ahc->regs[SCSIRATE]);
  YS__logmsg(nid, "scsiid(0x%02x)     scsidatl(0x%02x)\n",
	     ahc->regs[SCSIID], ahc->regs[SCSIDATL]);
  YS__logmsg(nid, "scsidath(0x%02x)   stcnt0(0x%02x)\n",
	     ahc->regs[SCSIDATH], ahc->regs[STCNT0]);
  YS__logmsg(nid, "stcnt1(0x%02x)     stcnt2(0x%02x)\n",
	     ahc->regs[STCNT1], ahc->regs[STCNT2]);
  YS__logmsg(nid, "clrsint0(0x%02x)   sstat0(0x%02x)\n",
	     ahc->regs[CLRSINT0], ahc->regs[SSTAT0]);
  YS__logmsg(nid, "clrsint1(0x%02x)   sstat1(0x%02x)\n",
	     ahc->regs[CLRSINT1], ahc->regs[SSTAT1]);
  YS__logmsg(nid, "simode1(0x%02x)\n",
	     ahc->regs[SIMODE1]);
  YS__logmsg(nid, "scsibusl(0x%02x)   scsibush(0x%02x)\n",
	     ahc->regs[SCSIBUSL], ahc->regs[SCSIBUSH]);
  YS__logmsg(nid, "shaddr0(0x%02x)    shaddr1(0x%02x)\n",
	     ahc->regs[SHADDR0], ahc->regs[SHADDR1]);
  YS__logmsg(nid, "shaddr2(0x%02x)    shaddr3(0x%02x)\n",
	     ahc->regs[SHADDR2], ahc->regs[SHADDR3]);
  YS__logmsg(nid, "selid(0x%02x)      sblkctl(0x%02x)\n",
	     ahc->regs[SELID], ahc->regs[SBLKCTL]);
  YS__logmsg(nid, "seqctl(0x%02x)     seqram(0x%02x)\n",
	     ahc->regs[SEQCTL], ahc->regs[SEQRAM]);
  YS__logmsg(nid, "accum(0x%02x)      sindex(0x%02x)\n",
	     ahc->regs[ACCUM], ahc->regs[SINDEX]);
  YS__logmsg(nid, "dindex(0x%02x)     allzeros(0x%02x)\n",
	     ahc->regs[DINDEX], ahc->regs[ALLZEROS]);
  YS__logmsg(nid, "sindir(0x%02x)     dindir(0x%02x)\n",
	     ahc->regs[SINDIR], ahc->regs[DINDIR]);
  YS__logmsg(nid, "function1(0x%02x)\n",
	     ahc->regs[FUNCTION1]);
  YS__logmsg(nid, "haddr0(0x%02x)     haddr1(0x%02x)\n",
	     ahc->regs[HADDR0], ahc->regs[HADDR1]);
  YS__logmsg(nid, "haddr2(0x%02x)     haddr3(0x%02x)\n",
	     ahc->regs[HADDR2], ahc->regs[HADDR3]);
  YS__logmsg(nid, "hcnt0(0x%02x)      hcnt1(0x%02x)\n",
	     ahc->regs[HCNT0], ahc->regs[HCNT1]);
  YS__logmsg(nid, "hcnt2(0x%02x)      scbptr(0x%02x)\n",
	     ahc->regs[HCNT2], ahc->regs[SCBPTR]);
  YS__logmsg(nid, "bctl(0x%02x)       dscommand(0x%02x)\n",
	     ahc->regs[BCTL], ahc->regs[DSCOMMAND]);
  YS__logmsg(nid, "bustime(0x%02x)    busspd(0x%02x)\n",
	     ahc->regs[BUSTIME], ahc->regs[BUSSPD]);
  YS__logmsg(nid, "hcntrl(0x%02x)     intstat(0x%02x)\n",
	     ahc->regs[HCNTRL], ahc->regs[INTSTAT]);
  YS__logmsg(nid, "error(0x%02x)      clrint(0x%02x)\n",
	     ahc->regs[ERROR], ahc->regs[CLRINT]);
  YS__logmsg(nid, "dfcntrl(0x%02x)    dfstatus(0x%02x)\n",
	     ahc->regs[DFCNTRL], ahc->regs[DFSTATUS]);
  YS__logmsg(nid, "dtdat(0x%02x)      scbcnt(0x%02x)\n",
	     ahc->regs[DFDAT], ahc->regs[SCBCNT]);
  YS__logmsg(nid, "qinfifo(0x%02x)    qincnt(0x%02x)\n",
	     ahc->regs[QINFIFO], ahc->regs[QINCNT]);
  YS__logmsg(nid, "qoutfifo(0x%02x)   qoutcnt(0x%02x)\n",
	     ahc->regs[QOUTFIFO], ahc->regs[QOUTCNT]);

  YS__logmsg(nid, "scb00(0x%02x)      scb01(0x%02x)\n",
	     ahc->regs[SCBARRAY+0],  ahc->regs[SCBARRAY+1]);
  YS__logmsg(nid, "scb02(0x%02x)      scb03(0x%02x)\n",
	     ahc->regs[SCBARRAY+2],  ahc->regs[SCBARRAY+3]);
  YS__logmsg(nid, "scb04(0x%02x)      scb05(0x%02x)\n",
	     ahc->regs[SCBARRAY+4],  ahc->regs[SCBARRAY+5]);
  YS__logmsg(nid, "scb06(0x%02x)      scb07(0x%02x)\n",
	     ahc->regs[SCBARRAY+6],  ahc->regs[SCBARRAY+7]);
  YS__logmsg(nid, "scb08(0x%02x)      scb09(0x%02x)\n",
	     ahc->regs[SCBARRAY+8],  ahc->regs[SCBARRAY+9]);
  YS__logmsg(nid, "scb0A(0x%02x)      scb0B(0x%02x)\n",
	     ahc->regs[SCBARRAY+10], ahc->regs[SCBARRAY+11]);
  YS__logmsg(nid, "scb0C(0x%02x)      scb0D(0x%02x)\n",
	     ahc->regs[SCBARRAY+12], ahc->regs[SCBARRAY+13]);
  YS__logmsg(nid, "scb0E(0x%02x)      scb0F(0x%02x)\n",
	     ahc->regs[SCBARRAY+14], ahc->regs[SCBARRAY+15]);
  YS__logmsg(nid, "scb10(0x%02x)      scb11(0x%02x)\n",
	     ahc->regs[SCBARRAY+16], ahc->regs[SCBARRAY+17]);
  YS__logmsg(nid, "scb12(0x%02x)      scb13(0x%02x)\n",
	     ahc->regs[SCBARRAY+18], ahc->regs[SCBARRAY+19]);
  YS__logmsg(nid, "scb14(0x%02x)      scb15(0x%02x)\n",
	     ahc->regs[SCBARRAY+20], ahc->regs[SCBARRAY+21]);
  YS__logmsg(nid, "scb16(0x%02x)      scb17(0x%02x)\n",
	     ahc->regs[SCBARRAY+22], ahc->regs[SCBARRAY+23]);
  YS__logmsg(nid, "scb18(0x%02x)      scb19(0x%02x)\n",
	     ahc->regs[SCBARRAY+24], ahc->regs[SCBARRAY+25]);
  YS__logmsg(nid, "scb1A(0x%02x)      scb1B(0x%02x)\n",
	     ahc->regs[SCBARRAY+26], ahc->regs[SCBARRAY+27]);
}
