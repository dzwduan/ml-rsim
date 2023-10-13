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

/*===========================================================================*/
/* PCI bridge model: includes NxM configuration spaces and some associated   */
/* information to communicate with the actual device.                        */
/* Note that devices are directly connected to the system bus, the bridge    */
/* only implements the configuration feature.                                */
/*===========================================================================*/



#ifndef _RSIM_PCI_H_
#define _RSIM_PCI_H_


#include "../../lamix/machine/pci_machdep.h"


struct PCI_CONFIG
{
  unsigned short  device_id;
  unsigned short  vendor_id;

  unsigned short  status;
  unsigned short  command;

  unsigned        class_revision;

  unsigned char   bist;
  unsigned char   header_type;
  unsigned char   latency_timer;
  unsigned char   cacheline_size;

  unsigned        base_addr[6];

  unsigned        cardbus_cic_ptr;

  unsigned short  subsystem_id;
  unsigned short  subsystem_vendor_id;

  unsigned        expansion_rom_addr;

  unsigned        capabilities;

  unsigned        reserved;

  unsigned char   max_lat;
  unsigned char   min_gnt;
  unsigned char   interrupt_pin;
  unsigned char   interrupt_line;

  unsigned impl_specific[48];
};

typedef struct PCI_CONFIG PCI_CONFIG;




/*===========================================================================*/
/* callback function to inquire about address spaces in device               */
/* one function per device                                                   */
/* arguments: physical address written into configuration register           */
/*            node id                                                        */
/*            bus module number                                              */
/*            function number (within device)                                */
/*            register number (within function)                              */
/*            pointer to address space size (return value)                   */
/*            pointer to address space flags (mem or I/O ...)                */
/* if address argument is -1 return required address space and flags         */
/* otherwise map address space at specified base address (and return size)   */

typedef void (*map_func_t)(unsigned, int, int, int, int,
                           unsigned*, unsigned*);




/*===========================================================================*/
/* PCI bridge control structure                                              */
typedef struct
{
  int        nodeid;
  int        mid;
  PCI_CONFIG config[PCI_MAX_DEVICES * PCI_MAX_FUNCTIONS];   /* config spaces */
  int        device_count;                /* number of PCI devices           */
  int        bus_id[PCI_MAX_DEVICES];     /* bus module number               */
  map_func_t map_func[PCI_MAX_DEVICES];   /* address space mapping functions */
} PCI_BUS;




/*===========================================================================*/
/* PCI bridge routines: call PCI_init before registering any PCI devices,    */
/* register PCI device with PCI_attach, attach the bridge to the bus with    */
/* PCI_BRIDGE_init (after registering all devices)                           */

void        PCI_init(void);
PCI_CONFIG *PCI_attach(int, int, map_func_t);
void        PCI_print_config(int, PCI_CONFIG*);

void        PCI_BRIDGE_init(void);

void        PCI_BRIDGE_dump(int);

#endif
