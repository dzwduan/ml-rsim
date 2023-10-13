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

/*****************************************************************************/
/* Generic SCSI controller module: contains system and SCSI bus interface    */
/* Provides callback routines for the generic I/O device module, callback    */
/* routines for the SCSI bus and a PCI mapping callback. Callbacks are       */
/* forwarded to the specific SCSI adapter module through a set of routines   */
/* described in a structure.                                                 */
/*****************************************************************************/

#ifndef __RSIM_SCSI_CONTROLLER_H__
#define __RSIM_SCSI_CONTROLLER_H__


#include "sim_main/evlst.h"
#include "Caches/req.h"
#include "Caches/lqueue.h"


/* uncomment to compile SCSI controller with debugging output */
/*
#define SCSI_CNTL_TRACE
*/

struct PCI_CONFIG;
struct SCSI_BUS;
struct SCSI_DEV;
struct SCSI_REQ;



/*---------------------------------------------------------------------------*/
/* This structure is the interface to the specific SCSI adapter.             */
/* It contains pointers to callback routines for the system interface,       */
/* SCSI bus and statistics. SCSI adapters should fill in this structure      */
/* and set the pointer in the generic SCSI controller to it.                 */
/*---------------------------------------------------------------------------*/

struct SCSI_CONTROLLER_SPEC
{
  int  (*host_read)     (void*, unsigned, unsigned);
  int  (*host_write)    (void*, unsigned, unsigned);
  void (*pci_map)       (void *, unsigned, int, int, int, int,
			 unsigned*, unsigned*);

  void (*scsi_wonbus)   (void*, struct SCSI_REQ*);
  void (*scsi_request)  (void*, struct SCSI_REQ*);
  void (*scsi_response) (void*, struct SCSI_REQ*);

  void (*print_params)  (void*);
  void (*stat_report)   (void*);
  void (*stat_clear)    (void*);
  void (*dump)          (void*);
};

typedef struct SCSI_CONTROLLER_SPEC SCSI_CONTROLLER_SPEC;




/*---------------------------------------------------------------------------*/
/* Generic SCSI controller module: contains system and SCSI bus interface.   */
/* Provides a set of callback routines for both bus interfaces and forwards  */
/* events to the specific adapter through the callback routines it provides. */
/*---------------------------------------------------------------------------*/

struct SCSI_CONTROLLER
{
  int                    nodeid;            /* identify module in system     */
  int                    mid;
  unsigned               intr_vector;
  
  struct PCI_CONFIG     *pci_me;            /* my PCI configuration space    */

  struct SCSI_BUS       *scsi_bus;          /* SCSI bus module attached      */
  struct SCSI_DEV       *scsi_me;           /* controller is a SCSI device   */
  int                    scsi_id;           /* controllers SCSI bus ID       */
  int                    scsi_max_target;   /* last target on SCSI bus       */
  int                    scsi_max_lun;      /* last lun in any target        */

  LinkQueue              reply_queue;
  LinkQueue              dma_queue;
  LinkQueue              interrupt_queue;
  EVENT                 *bus_interface;

  void                  *controller;        /* controller-specific data      */
  SCSI_CONTROLLER_SPEC  *contr_spec;        /* controller specific callbacks */
};

typedef struct SCSI_CONTROLLER SCSI_CONTROLLER;



/*---------------------------------------------------------------------------*/

extern struct SCSI_CONTROLLER *SCSI_CONTROLLERs;

extern int ARCH_scsi_cntrs;
extern int ARCH_disks;
extern int first_scsi_cntr;

#define PID2SCSI(nid, pid) &(SCSI_CONTROLLERs[(pid-first_scsi_cntr) + (nid * ARCH_scsi_cntrs)])

void SCSI_cntl_init           (void);

void SCSI_cntl_bus_interface  ();
int  SCSI_cntl_host_issue     (SCSI_CONTROLLER*, REQ*);
int  SCSI_cntl_host_interrupt (SCSI_CONTROLLER*);

void SCSI_cntl_io_issue       (SCSI_CONTROLLER*);

void SCSI_cntl_print_params   (int, int);
void SCSI_cntl_stat_report    (int, int);
void SCSI_cntl_stat_clear     (int, int);

void SCSI_cntl_dump           (int, int);

#endif

