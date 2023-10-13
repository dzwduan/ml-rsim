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

/***************************************************************************/
/*                                                                         */
/* Generic SCSI controller module: contains system and SCSI bus interface  */
/* Provides callback routines for the generic I/O device module, callback  */
/* routines for the SCSI bus and a PCI mapping callback. Callbacks are     */
/* forwarded to the specific SCSI adapter module through a set of routines */
/* described in a structure.                                               */
/*                                                                         */
/***************************************************************************/


#include <string.h>
#include <malloc.h>
#include <locale.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/evlst.h"
#include "Processor/simio.h"
#include "Processor/procstate.h"
#include "Processor/pagetable.h"
#include "Caches/syscontrol.h"
#include "Bus/bus.h"
#include "IO/addr_map.h"
#include "IO/io_generic.h"
#include "IO/pci.h"
#include "IO/ahc.h"
#include "IO/scsi_bus.h"
#include "IO/scsi_controller.h"
#include "IO/scsi_disk.h"
#include "IO/byteswap.h"

#include "../../lamix/mm/mm.h"
#include "../../lamix/kernel/syscontrol.h"
#include "../../lamix/dev/pci/pcireg.h"
#include "../../lamix/dev/pci/pcidevs.h"
#include "../../lamix/dev/ic/aic7xxxreg.h"



struct SCSI_CONTROLLER *SCSI_CONTROLLERs;

int ARCH_scsi_cntrs = 1;
int ARCH_disks      = 1;
int first_scsi_cntr = 0;


void     SCSI_cntl_sequencer   (void);

int      SCSI_cntl_host_read   (REQ*);
int      SCSI_cntl_host_write  (REQ*);
int      SCSI_cntl_host_reply  (REQ*);

void     SCSI_cntl_io_wonbus   (void*, SCSI_REQ*);
void     SCSI_cntl_io_request  (void*, SCSI_REQ*);
void     SCSI_cntl_io_response (void*, SCSI_REQ*);

void     SCSI_cntl_pci_map     (unsigned, int, int, int, int,
				unsigned*, unsigned*);



/*=========================================================================*/
/* Initialize SCSI controller:                                             */
/* Create generic I/O bus interface, initialize SCSI controller structure, */
/* attach to the PCI bridge and setup PCI configuration space, create SCSI */
/* bus and attach to bus as a device, call controller specific init.       */
/*=========================================================================*/

void SCSI_cntl_init(void)
{
  int              i, n, d;
  SCSI_CONTROLLER *pscsi;


  get_parameter("NUMscsi", &ARCH_scsi_cntrs, PARAM_INT);
  get_parameter("NUMdisk", &ARCH_disks, PARAM_INT);

  if (ARCH_scsi_cntrs == 0)
    return;
  
  SCSI_CONTROLLERs = RSIM_CALLOC(SCSI_CONTROLLER,
				 ARCH_numnodes * ARCH_scsi_cntrs);
  if ((!SCSI_CONTROLLERs) && (ARCH_numnodes * ARCH_scsi_cntrs > 0))
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  /*-----------------------------------------------------------------------*/

  first_scsi_cntr = ARCH_cpus + ARCH_ios;

  for (n = 0; n < ARCH_scsi_cntrs; n++)
    {
      IOGeneric_init(SCSI_cntl_host_read,
		     SCSI_cntl_host_write,
		     SCSI_cntl_host_reply);

      for (i = 0; i < ARCH_numnodes; i++)
	{
	  pscsi = PID2SCSI(i, n + first_scsi_cntr);

	  pscsi->nodeid = i;
	  pscsi->mid    = first_scsi_cntr + ARCH_ios;
	  
	  pscsi->pci_me = PCI_attach(i, first_scsi_cntr + ARCH_ios,
				     SCSI_cntl_pci_map);

	  pscsi->pci_me[0].vendor_id      = swap_short(PCI_VENDOR_ADP);
	  pscsi->pci_me[0].device_id      = swap_short(PCI_PRODUCT_ADP_2945U2W);
	  pscsi->pci_me[0].command        = 0x0000;
	  pscsi->pci_me[0].class_revision = swap_word(
	    (PCI_CLASS_MASS_STORAGE << PCI_CLASS_SHIFT) |
	    (PCI_SUBCLASS_MASS_STORAGE_SCSI << PCI_SUBCLASS_SHIFT));
	  pscsi->pci_me[0].header_type    = PCI_HDR_DEVICE;
	  pscsi->pci_me[0].interrupt_pin  = 1;


	  /* create and initialize bus interface --------------------------*/

          lqueue_init(&(pscsi->reply_queue),     BUS_TOTAL_REQUESTS);
          lqueue_init(&(pscsi->dma_queue),       BUS_TOTAL_REQUESTS);
          lqueue_init(&(pscsi->interrupt_queue), 1);

	  pscsi->bus_interface = NewEvent("SCSI Bus Interface",
					  SCSI_cntl_bus_interface, NODELETE,0);
	  EventSetArg(pscsi->bus_interface, pscsi, sizeof(pscsi));


	  /* create SCSI bus and put myself on it as device N-1 -----------*/
	  pscsi->scsi_bus = SCSI_bus_init(i, n);
	  pscsi->scsi_id  = SCSI_WIDTH * 8 - 1;
	  pscsi->scsi_me  = SCSI_device_init(pscsi->scsi_bus, pscsi->scsi_id,
					     pscsi,
					     SCSI_cntl_io_wonbus,
					     SCSI_cntl_io_request,
					     SCSI_cntl_io_response,
					     NULL, NULL, NULL, NULL);


      
	  /* create disks -------------------------------------------------*/

	  for (d = 0; d < ARCH_disks; d++)
	    {
	      SCSI_disk_init(pscsi->scsi_bus, d);
	    }

	  if (ARCH_disks > 0)
	    pscsi->scsi_max_target = ARCH_disks - 1;
	  else
	    pscsi->scsi_max_target = 0;
	  pscsi->scsi_max_lun    = 0;

	  /* create actual controller backend -----------------------------*/
	  ahc_init(pscsi, pscsi->scsi_id);
	}
      
      ARCH_ios++;
      ARCH_coh_ios++;
    }
}





/*===========================================================================*/
/* Host Bus Read Callback Function - forward individual requests to          */
/* specific controller and generate reply transaction                        */
/*===========================================================================*/

int SCSI_cntl_host_read(REQ* req)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(req->node, req->dest_proc);
  REQ             *nreq;

  nreq = req;
  while (nreq != NULL)
    {
      if (pscsi->contr_spec->host_read)
	if (pscsi->contr_spec->host_read(pscsi->controller,
					 nreq->paddr,
					 nreq->size) == 0)
	  return(0);
      nreq = nreq->parent;
    }

  req->type = REPLY;
  req->req_type = REPLY_UC;

  if (lqueue_full(&(pscsi->reply_queue)))
    YS__errmsg(pscsi->nodeid, "SCSI Reply queue full!\n");

  lqueue_add(&(pscsi->reply_queue), req, pscsi->nodeid);

  if (IsNotScheduled(pscsi->bus_interface))
    schedule_event(pscsi->bus_interface, YS__Simtime + BUS_FREQUENCY);

  return(1);
}




/*===========================================================================*/
/* Host Bus Write Callback Function                                          */
/* forward individual requests to specific controller                        */
/*===========================================================================*/

int SCSI_cntl_host_write(REQ* req)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(req->node, req->dest_proc);
  
  while (req != NULL)
    {
      if (pscsi->contr_spec->host_write)
	if (pscsi->contr_spec->host_write(pscsi->controller,
					  req->paddr,
					  req->size) == 0)
	  return(0);
      req = req->parent;
    }

  return(1);
}



/*===========================================================================*/
/* Host Bus Reply Callback function - do nothing (yet)                       */
/*===========================================================================*/

int SCSI_cntl_host_reply(REQ *req)
{
  return(1);
}




/*===========================================================================*/
/* PCI Map callback function - forward to controller-specific callback       */
/*===========================================================================*/

void SCSI_cntl_pci_map(unsigned address, int node, int module,
		       int function, int reg,
		       unsigned *size, unsigned *flags)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(node, module);

  *size = 0;
  *flags = 0;

  if (pscsi->contr_spec->pci_map)
    pscsi->contr_spec->pci_map(pscsi->controller, address,
			       node, module, function, reg,
			       size, flags);
}





/*===========================================================================*/
/* Generate host bus transaction. Caller has allocated request structure and */
/* filled in address, size, prcr_req_type and completion callbacks. This     */
/* routine adds source and destination module ID, request type and sends it  */
/* to the generic I/O module bus interface.                                  */
/*===========================================================================*/

int SCSI_cntl_host_issue(SCSI_CONTROLLER *pscsi, REQ *req)
{
  req->node      = pscsi->nodeid;
  req->src_proc  = pscsi->mid;
  req->dest_proc = AddrMap_lookup(req->node, req->paddr);

  req->type = REQUEST;
  if (req->prcr_req_type == WRITE)
    {
      if (((req->paddr % ARCH_linesz2) == 0) &&
	  (req->d.mem.count == ARCH_linesz2))
	req->type = WRITEPURGE;
      else
	req->req_type = READ_OWN;
    }
  else
    {
      req->req_type = READ_CURRENT;
    }

  if (lqueue_full(&(pscsi->dma_queue)))
    return(0);

  lqueue_add(&(pscsi->dma_queue), req, pscsi->nodeid);
  if (IsNotScheduled(pscsi->bus_interface))
    schedule_event(pscsi->bus_interface, YS__Simtime + BUS_FREQUENCY);

  return(1);
}




/*===========================================================================*/
/* Generate interrupt transaction.                                           */
/* Check PCI configuration space settings, setup interrupt transaction       */
/* and issue it on system bus.                                               */
/*===========================================================================*/

int SCSI_cntl_host_interrupt(SCSI_CONTROLLER *pscsi)
{
  REQ      *req;
  int       target;
  unsigned  addr;

  if (lqueue_full(&(pscsi->interrupt_queue)))
    return(0);

#ifdef SCSI_CNTL_TRACE
  YS__logmsg(pscsi->nodeid,
	     "%.0f: SCSI Controller Interrupt Host\n", YS__Simtime);
#endif

  pscsi->intr_vector = pscsi->pci_me[0].interrupt_line & 0x0F;
  target = pscsi->pci_me[0].interrupt_line >> 4;
  
  req = (REQ *) YS__PoolGetObj(&YS__ReqPool);  

  if (target != 0xFF)                 /* single-CPU interrupt */
    addr = SYSCONTROL_THIS_LOW(target) + SC_INTERRUPT;
  else
    addr = SYSCONTROL_LOCAL_LOW + SC_INTERRUPT;

  req->vaddr = addr;
  req->paddr = addr;

  req->size  = 4;
  
  req->d.mem.buf = (unsigned char*)&(pscsi->intr_vector);
  req->perform  = IO_write_word;
  req->complete = (void(*)(REQ*, HIT_TYPE))IO_empty_func;

  req->node      = pscsi->nodeid;
  req->src_proc  = pscsi->mid;
  req->dest_proc = target;

  req->type          = REQUEST;
  req->req_type      = WRITE_UC;
  req->prcr_req_type = WRITE;
  req->prefetch      = 0;
  req->ifetch        = 0;
  
  req->parent        = NULL;

  lqueue_add(&(pscsi->interrupt_queue), req, pscsi->nodeid);
  if (IsNotScheduled(pscsi->bus_interface))
    schedule_event(pscsi->bus_interface, YS__Simtime + BUS_FREQUENCY);

  return(1);
}



/*===========================================================================*/
/* SCSI Controller Host Bus Interface: multiplex reply queue, interrupt      */
/* queue and DMA queue (in this order/priority) onto generic I/O interface.  */
/*===========================================================================*/

void SCSI_cntl_bus_interface()
{
  SCSI_CONTROLLER  *pscsi = (SCSI_CONTROLLER*)EventGetArg(NULL);
  LinkQueue       *lq;
  REQ             *req;


  if (!lqueue_empty(&(pscsi->reply_queue)))
    lq = &(pscsi->reply_queue);
  else if (!lqueue_empty(&(pscsi->interrupt_queue)))
    lq = &(pscsi->interrupt_queue);
  else if (!lqueue_empty(&(pscsi->dma_queue)))
    lq = &(pscsi->dma_queue);
  else
    {
      YS__warnmsg(pscsi->nodeid, "Spurious bus interface wakeup %i:%i\n",
                  pscsi->nodeid, pscsi->mid);
      return;
    }

  req = lqueue_head(lq);

  if (IO_start_transaction(PID2IO(pscsi->nodeid, pscsi->mid), req))
    {
#ifdef SCSI_CNTL_TRACE
      YS__logmsg(pscsi->nodeid,
		 "[%i] SCSI Issue %s 0x%08X\n",
		 pscsi->mid, ReqName[req->req_type], req->paddr);
#endif
      lqueue_remove(lq);
    }
  
  if ((!lqueue_empty(&(pscsi->reply_queue))) ||
      (!lqueue_empty(&(pscsi->interrupt_queue))) ||
      (!lqueue_empty(&(pscsi->dma_queue))))
    schedule_event(pscsi->bus_interface, YS__Simtime + BUS_FREQUENCY);
}




/*===========================================================================*/
/* Device won arbitration on SCSI bus. Forward to specific controller.       */
/*===========================================================================*/

void SCSI_cntl_io_wonbus(void *ptr, SCSI_REQ * req)
{
  SCSI_CONTROLLER *pscsi = (SCSI_CONTROLLER*)ptr;

  if (pscsi->contr_spec->scsi_wonbus)
    pscsi->contr_spec->scsi_wonbus(pscsi->controller, req);
}




/*===========================================================================*/
/* SCSI request arrived for the device - forward to specific controller.     */
/*===========================================================================*/

void SCSI_cntl_io_request(void *ptr, SCSI_REQ * req)
{
  SCSI_CONTROLLER *pscsi = (SCSI_CONTROLLER*)ptr;

  if (pscsi->contr_spec->scsi_request)
    pscsi->contr_spec->scsi_request(pscsi->controller, req);  
}




/*===========================================================================*/
/* SCSI Response arrived for device - forward to specific controller.        */
/*===========================================================================*/

void SCSI_cntl_io_response(void *ptr, SCSI_REQ* req)
{
  SCSI_CONTROLLER *pscsi = (SCSI_CONTROLLER*)ptr;

  if (pscsi->contr_spec->scsi_response)
    pscsi->contr_spec->scsi_response(pscsi->controller, req);  
}




/*===========================================================================*/
/* Print configuration parameters                                            */
/* call controller-specific print-routine and then SCSI bus print routine.   */
/*===========================================================================*/

void SCSI_cntl_print_params(int nid, int mid)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(nid, mid + first_scsi_cntr);
  
  PCI_print_config(nid, pscsi->pci_me);
  
  if (pscsi->contr_spec->print_params)
    pscsi->contr_spec->print_params(pscsi->controller);

  SCSI_bus_print_params(nid);
}



/*===========================================================================*/
/* Report statistics: first call controller-specific report routine, then    */
/* the SCSI bus report routine (which in turn calls the report routines for  */
/* all attached devices)                                                     */
/*===========================================================================*/

void SCSI_cntl_stat_report(int nid, int mid)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(nid, mid + first_scsi_cntr);

  if (pscsi->contr_spec->stat_report)
    {
      YS__statmsg(nid, "SCSI Controller %i Statistics\n", mid);
      pscsi->contr_spec->stat_report(pscsi->controller);
    }

  YS__statmsg(nid, "SCSI Bus %i Statistics\n", mid);
  SCSI_bus_stat_report(pscsi->scsi_bus);
}




/*===========================================================================*/
/* Clear statistics: for each node, call controller-specific clear routine   */
/* and SCSI bus clear routine.                                               */
/*===========================================================================*/

void SCSI_cntl_stat_clear(int nid, int mid)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(nid, mid + first_scsi_cntr);

  if (pscsi->contr_spec->stat_clear)
    pscsi->contr_spec->stat_clear(pscsi->controller);

  SCSI_bus_stat_clear(pscsi->scsi_bus);
}



/*===========================================================================*/
/* Dump debug information about controller                                   */
/*===========================================================================*/

void SCSI_cntl_dump(int nid, int mid)
{
  SCSI_CONTROLLER *pscsi = PID2SCSI(nid, mid + first_scsi_cntr);
  IO_GENERIC      *pio   = PID2IO(nid, pscsi->mid);
  REQ *req;

  YS__logmsg(nid, "\n============== SCSI CONTROLLER %i =============\n", mid);
  YS__logmsg(nid, "scsi_id(%d), scsi_max_target(%d), scsi_max_lun(%d)\n",
	     pscsi->scsi_id, pscsi->scsi_max_target, pscsi->scsi_max_lun);
  DumpLinkQueue("reply_queue", &(pscsi->reply_queue), 0x71,
		Cache_req_dump, nid);
  DumpLinkQueue("dma_queue", &(pscsi->dma_queue), 0x71, Cache_req_dump, nid);
  DumpLinkQueue("interrupt_queue", &(pscsi->interrupt_queue), 0x71,
		Cache_req_dump, nid);
  YS__logmsg(nid, "bus_interface scheduled: %s\n",
	     IsScheduled(pscsi->bus_interface) ? "yes" : "no");
  IO_dump(pio);

  if (pscsi->contr_spec->dump)
    pscsi->contr_spec->dump(pscsi->controller);
  
  SCSI_bus_dump(pscsi->scsi_bus);
}

