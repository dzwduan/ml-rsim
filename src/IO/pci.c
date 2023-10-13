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
/* PCI bridge model: includes NxM configuration spaces and some associated */
/* information to communicate with the actual device.                      */
/* Note that devices are directly connected to the system bus, the bridge  */
/* only implements the configuration feature.                              */
/* First, initialize the PCI bridge with PCI_init(). Second, register PCI  */
/* devices using PCI_attach, and then attach the bridge to the system bus  */
/* using PCI_BRIDGE_init. This is necessary since the bridge is            */
/* non-coherent but other PCI devices might be coherent and need to have   */
/* lower bus module numbers then the bridge.                               */
/*=========================================================================*/




#include <string.h>
#include <malloc.h>

#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "sim_main/evlst.h"
#include "Processor/simio.h"
#include "Processor/pagetable.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "IO/addr_map.h"
#include "IO/pci.h"
#include "IO/io_generic.h"
#include "IO/byteswap.h"
#include "../../lamix/mm/mm.h"
#include "../../lamix/dev/pci/pcireg.h"
#include "../../lamix/dev/pci/pcidevs.h"



PCI_BUS *PCI_BUSES;

int PCI_BRIDGE_read(REQ*);
int PCI_BRIDGE_write(REQ*);



/*=========================================================================*/
/* Initialize the PCI configuration space portion of the bridge.           */
/* Clear all configuration spaces, reset the vendor ID to 'invalid' and    */
/* clear the address space callback pointers.                              */
/*=========================================================================*/

void PCI_init(void)
{
  int n, m;

  PCI_BUSES = malloc(sizeof(PCI_BUS) * ARCH_numnodes);
  if (PCI_BUSES == NULL)
    YS__errmsg(0, "Malloc PCI_BUSES failed %s:%i", __FILE__, __LINE__);

  for (n = 0; n < ARCH_numnodes; n++)
    {
      PCI_BUSES[n].device_count = 0;

      memset(PCI_BUSES[n].config, 0, sizeof(PCI_BUSES[n].config));
      
      for (m = 0; m < PCI_MAX_DEVICES * PCI_MAX_FUNCTIONS; m++)
	PCI_BUSES[n].config[m].vendor_id = PCI_VENDOR_INVALID;

      for (m = 0; m < PCI_MAX_DEVICES; m++)
	PCI_BUSES[n].map_func[n] = NULL;
    }
}





/*=========================================================================*/
/* Attach a PCI device to the PCI bridge.                                  */
/* Arguments: node ID and bus module number, addres map callback function  */
/* Returns:   pointer to the configuration space array for the PCi device  */
/* PCI devices are assigned configuration spaces (slots) in the order they */
/* are attached to the bus.                                                */
/*=========================================================================*/

PCI_CONFIG *PCI_attach(int node_id, int module, map_func_t map_func)
{
  int n;
  
  if (PCI_BUSES[node_id].device_count >= PCI_MAX_DEVICES)
    YS__errmsg(node_id,
	       "Too many PCI devices attached in node %i",
	       node_id);

  n = PCI_BUSES[node_id].device_count++;

  PCI_BUSES[node_id].bus_id[n] = module;
  PCI_BUSES[node_id].map_func[n] = map_func;

  return(&PCI_BUSES[node_id].config[n * PCI_MAX_FUNCTIONS]);
}




/*=========================================================================*/
/* Finish initialization of the PCI bridge, namely attach it to the system */
/* bus and install the physical addresses of the configuration space in    */
/* the nodes simulator page table and address map.                         */
/*=========================================================================*/

void PCI_BRIDGE_init(void)
{
  int i;
  unsigned addr;
  char *memptr;
  
  IOGeneric_init(PCI_BRIDGE_read, PCI_BRIDGE_write, NULL);

  for (i = 0; i < ARCH_numnodes; i++)
    {
      AddrMap_insert(i, PCICONFIG_BASE_ADDR,
                     PCICONFIG_BASE_ADDR +
                     sizeof(PCI_CONFIG) * PCI_MAX_DEVICES * PCI_MAX_FUNCTIONS,
                     ARCH_cpus + ARCH_ios);

      for (addr = 0, memptr = (char*)PCI_BUSES[i].config;
           addr < sizeof(PCI_BUSES[i].config);
           addr += PAGE_SIZE, memptr += PAGE_SIZE)
        PageTable_insert(i, PCICONFIG_BASE_ADDR + addr, memptr);

      PCI_BUSES[i].nodeid = i;
      PCI_BUSES[i].mid = ARCH_cpus + ARCH_ios;
    }

  ARCH_ios++;
}





/*=========================================================================*/
/* PCI bridge system bus read callback - simply schedule response          */
/*=========================================================================*/

int PCI_BRIDGE_read(REQ *req)
{
  req->type     = REPLY;
  req->req_type = REPLY_UC;

  IO_start_transaction(PID2IO(req->node, req->dest_proc), req);

  return(1);
}





/*=========================================================================*/
/* PCI bridge write callback:                                              */
/* for writes to a address space base register call the devices callback   */
/* function. If a -1 was written, convert the returned address space size  */
/* into a bitmask and write it into the respective base register,          */
/* otherwise merge the address space flags into the base address.          */
/*=========================================================================*/

int PCI_BRIDGE_write(REQ *req)
{
  unsigned config_offset;
  unsigned val, newval, i, size, flags;
  int      node, dev, func, reg;

  config_offset = req->paddr & (sizeof(PCI_CONFIG) - 1);
  node = req->node;


  /* write to base address register ---------------------------------------*/
  if ((config_offset >= PCI_MAPREG_START) && (config_offset < PCI_MAPREG_END))
    {
      val = read_int(node, req->paddr);

      dev = (req->paddr - PCICONFIG_BASE_ADDR) /
        (sizeof(PCI_CONFIG) * PCI_MAX_FUNCTIONS);

      func = ((req->paddr - PCICONFIG_BASE_ADDR) %
	      (sizeof(PCI_CONFIG) * PCI_MAX_FUNCTIONS)) / sizeof(PCI_CONFIG);

      reg = (config_offset - PCI_MAPREG_START) / 4;

      if (PCI_BUSES[node].map_func[dev])
        PCI_BUSES[node].map_func[dev](val,
				      node, PCI_BUSES[node].bus_id[dev],
				      func, reg, &size, &flags);
      else
	{
	  size  = 0;
	  flags = 0;
	}


      /* prepare to inquire address space size ----------------------------*/
      if (val == 0xFFFFFFFF)
        {
          if (size == 0)                             /* no space required  */
            newval = 0x00000000;
          else                                       /* compute bit mask   */
            {
              newval = 0xFFFFFFFF;
              size += (size - 1);                    /* round up past next */
	      size >>= 1;
              i = 0;                                 /* power of 2         */
              for (i = 0; size > 0; i++, size >>= 1)
                newval &= 0xFFFFFFFE << i;
            }
	}
      else
	newval = val;

      
      if (PCI_MAPREG_TYPE(flags) == PCI_MAPREG_TYPE_IO)
	newval &= PCI_MAPREG_IO_ADDR_MASK;
      else
	newval &= PCI_MAPREG_MEM_ADDR_MASK;

      newval |= flags;
      write_int(node, req->paddr, newval);           /* write mask         */
    }

  return(1);
}




/*=========================================================================*/
/*=========================================================================*/


struct pci_knowndev
{
  unsigned short   vendor;
  unsigned short   product;
  int		   flags;
  char		  *vendorname;
  char            *productname;
};

#define	PCI_KNOWNDEV_NOPROD	0x01

#include "../../lamix/dev/pci/pcidevs_data.h"

void PCI_print_config(int node, PCI_CONFIG *pdev)
{
  int             n, m;
  unsigned short  vendor_id, device_id;
  unsigned int    class_rev;
  struct pci_knowndev *kdp;


  YS__statmsg(node, "PCI Configuration\n");
  for (n = 0; n < PCI_MAX_FUNCTIONS; n++)
    {
      vendor_id = swap_short(pdev[n].vendor_id);
      device_id = swap_short(pdev[n].device_id);
      class_rev = swap_word(pdev[n].class_revision);

      if (vendor_id == PCI_VENDOR_INVALID)
	continue;

      YS__statmsg(node,
		  "  PCI Function %i\n", n);

      /* search for vendor ID in list of vendors/devices */
      kdp = pci_knowndevs;
      while (kdp->vendorname != NULL)
	{	/* all have vendor name */
	  if (kdp->vendor == vendor_id)
	    break;  
	  kdp++;
	}

      YS__statmsg(node,
		  "      Vendor ID : 0x%04X   %s\n",
		  vendor_id, kdp->vendorname ? kdp->vendorname : "unknown");

      /* search for device in list of vendors/devices */
      /* not all devices of all vendors are listed explicitly, for some */
      /* vendors the PCI_KNOWNDEV_NOPROD is set to indicate the absence */
      /* of any device names */
      kdp = pci_knowndevs;
      while (kdp->vendorname != NULL)
	{	/* all have vendor name */
	  if (kdp->vendor == vendor_id &&
	      (kdp->product == device_id ||
	       (kdp->flags & PCI_KNOWNDEV_NOPROD) != 0))
	    break;
	  kdp++;
	}

      YS__statmsg(node,
		  "      Device ID : 0x%04X   %s\n",
		  device_id,
		  kdp->vendorname == NULL ? "unknown" :
		  (kdp->flags & PCI_KNOWNDEV_NOPROD) == 0 ? kdp->productname : "unknown");

      YS__statmsg(node, "      Class     : ");

      switch (PCI_CLASS(class_rev))
	{
	case PCI_CLASS_PREHISTORIC:
	  YS__statmsg(node, "Prehistoric/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_PREHISTORIC_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    case PCI_SUBCLASS_PREHISTORIC_VGA:
	      YS__statmsg(node, "VGA");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;


	case PCI_CLASS_MASS_STORAGE:
	  YS__statmsg(node, "Mass Storage/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_MASS_STORAGE_SCSI:
	      YS__statmsg(node, "SCSI");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_IDE:
	      YS__statmsg(node, "IDE");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_FLOPPY:
	      YS__statmsg(node, "Floppy");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_IPI:
	      YS__statmsg(node, "IPI");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_RAID:
	      YS__statmsg(node, "RAID");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_ATA:
	      YS__statmsg(node, "ATA");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_SATA:
	      YS__statmsg(node, "SATA");
	      break;
	    case PCI_SUBCLASS_MASS_STORAGE_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;


	case PCI_CLASS_NETWORK:
	  YS__statmsg(node, "Network/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_NETWORK_ETHERNET:
	      YS__statmsg(node, "Ethernet");
	      break;
	    case PCI_SUBCLASS_NETWORK_TOKENRING:
	      YS__statmsg(node, "Tokenring");
	      break;
	    case PCI_SUBCLASS_NETWORK_FDDI:
	      YS__statmsg(node, "FDDI");
	      break;
	    case PCI_SUBCLASS_NETWORK_ATM:
	      YS__statmsg(node, "ATM");
	      break;
	    case PCI_SUBCLASS_NETWORK_ISDN:
	      YS__statmsg(node, "ISDN");
	      break;
	    case PCI_SUBCLASS_NETWORK_WORLDFIP:
	      YS__statmsg(node, "WORLDFIP");
	      break;
	    case PCI_SUBCLASS_NETWORK_PCIMGMULTICOMP:
	      YS__statmsg(node, "PCIMGMULTICOMP");
	      break;
	    case PCI_SUBCLASS_NETWORK_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;


	case PCI_CLASS_DISPLAY:
	  YS__statmsg(node, "Display/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_DISPLAY_VGA:
	      YS__statmsg(node, "VGA");
	      break;
	    case PCI_SUBCLASS_DISPLAY_XGA:
	      YS__statmsg(node, "XGA");
	      break;
	    case PCI_SUBCLASS_DISPLAY_3D:
	      YS__statmsg(node, "3D");
	      break;
	    case PCI_SUBCLASS_DISPLAY_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_MULTIMEDIA:
	  YS__statmsg(node, "Multimedia/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_MULTIMEDIA_VIDEO:
	      YS__statmsg(node, "Video");
	      break;
	    case PCI_SUBCLASS_MULTIMEDIA_AUDIO:
	      YS__statmsg(node, "Audio");
	      break;
	    case PCI_SUBCLASS_MULTIMEDIA_TELEPHONE:
	      YS__statmsg(node, "Telephone");
	      break;
	    case PCI_SUBCLASS_MULTIMEDIA_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_MEMORY:
	  YS__statmsg(node, "Memory/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_MEMORY_RAM:
	      YS__statmsg(node, "RAM");
	      break;
	    case PCI_SUBCLASS_MEMORY_FLASH:
	      YS__statmsg(node, "Flash");
	      break;
	    case PCI_SUBCLASS_MEMORY_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_BRIDGE:
	  YS__statmsg(node, "Bridge/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_BRIDGE_HOST:
	      YS__statmsg(node, "Host");
	      break;
	    case PCI_SUBCLASS_BRIDGE_ISA:
	      YS__statmsg(node, "ISA");
	      break;
	    case PCI_SUBCLASS_BRIDGE_EISA:
	      YS__statmsg(node, "EISA");
	      break;
	    case PCI_SUBCLASS_BRIDGE_MC:
	      YS__statmsg(node, "MC");
	      break;
	    case PCI_SUBCLASS_BRIDGE_PCI:
	      YS__statmsg(node, "PCI");
	      break;
	    case PCI_SUBCLASS_BRIDGE_PCMCIA:
	      YS__statmsg(node, "PCMCIA");
	      break;
	    case PCI_SUBCLASS_BRIDGE_NUBUS:
	      YS__statmsg(node, "NuBus");
	      break;
	    case PCI_SUBCLASS_BRIDGE_CARDBUS:
	      YS__statmsg(node, "CardBus");
	      break;
	    case PCI_SUBCLASS_BRIDGE_RACEWAY:
	      YS__statmsg(node, "RACEway");
	      break;
	    case PCI_SUBCLASS_BRIDGE_STPCI:
	      YS__statmsg(node, "PCI semitransparant");
	      break;
	    case PCI_SUBCLASS_BRIDGE_INFINIBAND:
	      YS__statmsg(node, "InfiniBand");
	      break;
	    case PCI_SUBCLASS_BRIDGE_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_COMMUNICATIONS:
	  YS__statmsg(node, "Communciations/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_COMMUNICATIONS_SERIAL:
	      YS__statmsg(node, "Serial");
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_PARALLEL:
	      YS__statmsg(node, "Parallel");
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_MPSERIAL:
	      YS__statmsg(node, "Serial multiport");
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_MODEM:
	      YS__statmsg(node, "Modem")   ;
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_GPIB:
	      YS__statmsg(node, "Gigabit InfiniBand")   ;
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_SMARTCARD:
	      YS__statmsg(node, "SmartCard")   ;
	      break;
	    case PCI_SUBCLASS_COMMUNICATIONS_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_SYSTEM:
	  YS__statmsg(node, "System/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_SYSTEM_PIC:
	      YS__statmsg(node, "PIC");
	      break;
	    case PCI_SUBCLASS_SYSTEM_DMA:
	      YS__statmsg(node, "DMA");
	      break;
	    case PCI_SUBCLASS_SYSTEM_TIMER:
	      YS__statmsg(node, "Timer");
	      break;
	    case PCI_SUBCLASS_SYSTEM_RTC:
	      YS__statmsg(node, "Realtime Clock");
	      break;
	    case PCI_SUBCLASS_SYSTEM_PCIHOTPLUG:
	      YS__statmsg(node, "PCI Hotplug");
	      break;
	    case PCI_SUBCLASS_SYSTEM_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_INPUT:
	  YS__statmsg(node, "Input/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_INPUT_KEYBOARD:
	      YS__statmsg(node, "Keyboard");
	      break;
	    case PCI_SUBCLASS_INPUT_DIGITIZER:
	      YS__statmsg(node, "Digitizer");
	      break;
	    case PCI_SUBCLASS_INPUT_MOUSE:
	      YS__statmsg(node, "Mouse");
	      break;
	    case PCI_SUBCLASS_INPUT_SCANNER:
	      YS__statmsg(node, "Scanner");
	      break;
	    case PCI_SUBCLASS_INPUT_GAMEPORT:
	      YS__statmsg(node, "Gameport");
	      break;
	    case PCI_SUBCLASS_INPUT_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_DOCK:
	  YS__statmsg(node, "Dock/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_DOCK_GENERIC:
	      YS__statmsg(node, "Generic");
	      break;
	    case PCI_SUBCLASS_DOCK_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_PROCESSOR:
	  YS__statmsg(node, "Processor/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_PROCESSOR_386:
	      YS__statmsg(node, "i386");
	      break;
	    case PCI_SUBCLASS_PROCESSOR_486:
	      YS__statmsg(node, "i486");
	      break;
	    case PCI_SUBCLASS_PROCESSOR_PENTIUM:
	      YS__statmsg(node, "Pentium");
	      break;
	    case PCI_SUBCLASS_PROCESSOR_ALPHA:
	      YS__statmsg(node, "Alpha");
	      break;
	    case PCI_SUBCLASS_PROCESSOR_POWERPC:
	      YS__statmsg(node, "PowerPC");
	      break;
	    case PCI_SUBCLASS_PROCESSOR_MIPS:
	      YS__statmsg(node, "MIPS");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_SERIALBUS:
	  YS__statmsg(node, "Serial Bus/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_SERIALBUS_FIREWIRE:
	      YS__statmsg(node, "Firewire");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_ACCESS:
	      YS__statmsg(node, "ACCESS.bus");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_SSA:
	      YS__statmsg(node, "SSA");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_USB:
	      YS__statmsg(node, "USB");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_FIBER:
	      YS__statmsg(node, "FibreChannel");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_SMBUS:
	      YS__statmsg(node, "SM Bus");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_INFINIBAND:
	      YS__statmsg(node, "InfiniBand");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_IPMI:
	      YS__statmsg(node, "IPMI");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_SERCOS:
	      YS__statmsg(node, "Sercos");
	      break;
	    case PCI_SUBCLASS_SERIALBUS_CANBUS:
	      YS__statmsg(node, "CAN Bus");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_WIRELESS:
	  YS__statmsg(node, "Wireless/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_WIRELESS_IRDA:
	      YS__statmsg(node, "iRDA");
	      break;
	    case PCI_SUBCLASS_WIRELESS_CONSUMERIR:
	      YS__statmsg(node, "InfraRed");
	      break;
	    case PCI_SUBCLASS_WIRELESS_RF:
	      YS__statmsg(node, "RF");
	      break;
	    case PCI_SUBCLASS_WIRELESS_BLUETOOTH:
	      YS__statmsg(node, "Bluetooth");
	      break;
	    case PCI_SUBCLASS_WIRELESS_BROADBAND:
	      YS__statmsg(node, "Broadband");
	      break;
	    case PCI_SUBCLASS_WIRELESS_802_11A:
	      YS__statmsg(node, "IEEE 802.11A");
	      break;
	    case PCI_SUBCLASS_WIRELESS_802_11B:
	      YS__statmsg(node, "IEEE 802.11B");
	      break;
	    case PCI_SUBCLASS_WIRELESS_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_INTELLIO:
	  YS__statmsg(node, "Intelligent IO/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_I2O_STANDARD:
	      YS__statmsg(node, "I2O");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;
	  

	case PCI_CLASS_SATELLITE:
	  YS__statmsg(node, "Satellite/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_SATELLITE_TV:
	      YS__statmsg(node, "TV");
	      break;
	    case PCI_SUBCLASS_SATELLITE_AUDIO:
	      YS__statmsg(node, "Audio");
	      break;
	    case PCI_SUBCLASS_SATELLITE_VOICE:
	      YS__statmsg(node, "Voice");
	      break;
	    case PCI_SUBCLASS_SATELLITE_DATA:
	      YS__statmsg(node, "Data");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	case PCI_CLASS_CRYPT:
	  YS__statmsg(node, "Encryption/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_CRYPT_NETCOMP:
	      YS__statmsg(node, "Network");
	      break;
	    case PCI_SUBCLASS_CRYPT_ENTERTAINMENT:
	      YS__statmsg(node, "Entertainment");
	      break;
	    case PCI_SUBCLASS_CRYPT_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }	  
	  break;

	  
	case PCI_CLASS_DATA:
	  YS__statmsg(node, "Data Acquisition/");

	  switch (PCI_SUBCLASS(class_rev))
	    {
	    case PCI_SUBCLASS_DATA_DPIO:
	      YS__statmsg(node, "DPIO");
	      break;
	    case PCI_SUBCLASS_DASP_TIMEFREQ:
	      YS__statmsg(node, "Timefreq");
	      break;
	    case PCI_SUBCLASS_DASP_SYNC:
	      YS__statmsg(node, "Sync");
	      break;
	    case PCI_SUBCLASS_DASP_MGMT:
	      YS__statmsg(node, "Management");
	      break;
	    case PCI_SUBCLASS_DATA_MISC:
	      YS__statmsg(node, "Miscellaneous");
	      break;
	    default:
	      YS__statmsg(node, "Unknown");
	      break;
	    }
	  break;

	  
	default:
	  YS__statmsg(node, "Undefined");
	  break;
	}

      YS__statmsg(node, "\n");
      
      if (pdev[n].interrupt_pin)
	{
	  if ((pdev[n].interrupt_line >> 4) == 0x0F)
	    YS__statmsg(node, "      Interrupt : all CPUs");
	  else
	    YS__statmsg(node, "      Interrupt : CPU %i",
		    pdev[n].interrupt_line >> 4);
      
	  YS__statmsg(node, " on IRQ %i\n", pdev[n].interrupt_line & 0x0F);
	}
      else
	YS__statmsg(node, "      No interrupt\n");


      YS__statmsg(node, "      Address Regions\n");
      for (m = 0;
	   m < (PCI_MAPREG_END - PCI_MAPREG_START) / sizeof(unsigned);
	   m++)
	{
	  unsigned base_addr = swap_word(pdev[n].base_addr[m]);

	  if ((PCI_MAPREG_MEM_ADDR(base_addr) == 0xFFFFFFFF) ||
	      (PCI_MAPREG_MEM_ADDR(base_addr) == 0x00000000))
	    continue;
	  
	  if (base_addr & PCI_MAPREG_TYPE_IO)
	    YS__statmsg(node,
		    "          %i: 0x%08X (I/O)\n",
		    m,
		    PCI_MAPREG_IO_ADDR(base_addr));
	  else
	    YS__statmsg(node,
		    "          %i: 0x%08X (Memory;%scacheable)\n",
		    m,
		    PCI_MAPREG_MEM_ADDR(base_addr),
		    PCI_MAPREG_MEM_CACHEABLE(base_addr) ? " " : " non-");
	      
	}

      YS__statmsg(node, "\n");
    }
}



/*=========================================================================*/
/*=========================================================================*/

void PCI_BRIDGE_dump(int nid)
{
  PCI_BUS *pci = &PCI_BUSES[nid];
  IO_GENERIC *pio = PID2IO(nid, pci->mid);
  
  YS__logmsg(nid, "\n=============== PCI BRIDGE =================\n");
  YS__logmsg(nid, "device_count(%d), module(%d)\n",
	     pci->device_count, pci->mid);
  IO_dump(pio);
}
