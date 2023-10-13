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
#include "sim_main/simsys.h"
#include "sim_main/util.h"
#include "Processor/simio.h"
#include "Processor/pagetable.h"
#include "Processor/procstate.h"
#include "Processor/memunit.h"
#include "Processor/tlb.h"
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/syscontrol.h"
#include "IO/addr_map.h"

#include "../../lamix/mm/mm.h"
#include "../../lamix/kernel/syscontrol.h"
#include "../../lamix/machine/intr.h"



SYS_CONTROL **SysControls;



/*=========================================================================*/
/*=========================================================================*/

void SysControl_init(void)
{
  SYS_CONTROL *scp;
  int i, k;
  unsigned base_addr;


  SysControls = RSIM_CALLOC(SYS_CONTROL*, ARCH_numnodes * ARCH_cpus);
  if (!SysControls)
    YS__errmsg(0, "Malloc failed in %s:%i", __FILE__, __LINE__);

  /*-----------------------------------------------------------------------*/

  for (i = 0; i < ARCH_numnodes; i++)
    {
      AddrMap_insert(i,
		     SYSCONTROL_LOCAL_LOW,
		     SYSCONTROL_LOCAL_LOW + PAGE_SIZE, TARGET_ALL_CPUS);

      for (k = 0; k < ARCH_cpus; k++)
	{
	  scp = RSIM_CALLOC(SYS_CONTROL, 1);
	  if (!scp)
	    YS__errmsg(i, "Malloc failed in %s:%i", __FILE__, __LINE__);
 
	  SysControls[i * ARCH_cpus + k] = scp;
 
	  scp->above = PID2L1D(i, k);

	  scp->below = PID2L2C(i, k);

	  base_addr = SYSCONTROL_THIS_LOW(k);

	  AddrMap_insert(i, base_addr, base_addr + PAGE_SIZE, k);
	  PageTable_insert_alloc(i, base_addr);

	  write_int(i, base_addr + SC_CPU_ID,     k);
	  write_int(i, base_addr + SC_NODE_ID,    i);
	  write_int(i, base_addr + SC_CPU_COUNT,  ARCH_cpus);
	  write_int(i, base_addr + SC_NODE_COUNT, ARCH_numnodes);

	  write_int(i, base_addr + SC_L1I_SIZE,      ARCH_cacsz1i);
	  write_int(i, base_addr + SC_L1I_BLOCK,     ARCH_linesz1i);
	  write_int(i, base_addr + SC_L1I_ASSOC,     ARCH_setsz1i);
	  write_int(i, base_addr + SC_L1I_WRITEBACK, 0);

	  write_int(i, base_addr + SC_L1D_SIZE,      ARCH_cacsz1d);
	  write_int(i, base_addr + SC_L1D_BLOCK,     ARCH_linesz1d);
	  write_int(i, base_addr + SC_L1D_ASSOC,     ARCH_setsz1d);
	  write_int(i, base_addr + SC_L1D_WRITEBACK, cparam.L1D_writeback);

	  write_int(i, base_addr + SC_L2_SIZE,       ARCH_cacsz2);
	  write_int(i, base_addr + SC_L2_BLOCK,      ARCH_linesz2);
	  write_int(i, base_addr + SC_L2_ASSOC,      ARCH_setsz2);
	  write_int(i, base_addr + SC_L2_WRITEBACK,  1);
	  
	  write_int(i, base_addr + SC_ITLB_TYPE,     ITLB_TYPE);
	  write_int(i, base_addr + SC_ITLB_SIZE,     ITLB_SIZE);

	  write_int(i, base_addr + SC_DTLB_TYPE,     DTLB_TYPE);
	  write_int(i, base_addr + SC_DTLB_SIZE,     DTLB_SIZE);

	  write_int(i, base_addr + SC_CLK_PERIOD,    CPU_CLK_PERIOD);
	  write_int(i, base_addr + SC_PHYS_MEM,      PHYSICAL_MEMORY);

	}
    }
}


/*=========================================================================*/


int IsSysControl_local(REQ *req)
{
  return((req->paddr >= SYSCONTROL_LOCAL_LOW) &&
	 (req->paddr <  SYSCONTROL_LOCAL_LOW + PAGE_SIZE));
}


/*=========================================================================*/


int IsSysControl_external(REQ *req, int pid)
{
  return((req->paddr >= SYSCONTROL_THIS_LOW(pid)) &&
	 (req->paddr <  SYSCONTROL_THIS_HIGH(pid)));
}


/*=========================================================================*/


int SysControl_local_request(int gid, REQ *req)
{
  SYS_CONTROL *syscontrol_ptr = SysControls[gid];
  unsigned old_addr;
  
  if ((req->type != REQUEST) ||
      ((req->prcr_req_type != READ) && (req->prcr_req_type != WRITE)))
    YS__errmsg(req->node,
	       "Unknown req->type %i or req->prcr_req_type %i at SysControl module\n",
	       req->type, req->prcr_req_type);

  if ((req->prcr_req_type == WRITE) &&
      (req->paddr - SYSCONTROL_THIS_LOW(gid % ARCH_cpus) < SC_RDONLY_HIGH))
    YS__errmsg(req->node,
	       "Attempted write to read-only memory in SysControl module");

  lqueue_remove(&(syscontrol_ptr->above->request_queue));
  FreeAMemUnit(req->d.proc_data.proc_id, req->d.proc_data.inst_tag);

  old_addr = req->paddr;
  req->paddr = req->paddr - SYSCONTROL_LOCAL_LOW + SYSCONTROL_THIS_LOW(gid % ARCH_cpus);
  ReplaceAddr(req->d.proc_data.inst, req->paddr);

  PerformData(req);
  MemDoneHeapInsert(req, IOHIT);

  if ((req->paddr == SYSCONTROL_THIS_LOW(gid%ARCH_cpus) + SC_HALT) &&
      (req->prcr_req_type == WRITE))
    ProcessorHalt(gid, read_int(req->node, req->paddr));


  req->paddr = old_addr;
  ReplaceAddr(req->d.proc_data.inst, req->paddr);  
  
  YS__PoolReturnObj(&YS__ReqPool, req);

  return(1);
}




/*=========================================================================*/


int SysControl_external_request(int gid, REQ *req)
{
  SYS_CONTROL *syscontrol_ptr = SysControls[gid];
  unsigned     old_addr;
  REQ         *creq;


  if ((req->type != REQUEST) ||
      ((req->req_type != READ_UC) && (req->req_type != WRITE_UC)))
    YS__errmsg(req->node,
	       "Unknown req->type %i or req->req_type %i at SysControl module\n", req->type,
	       req->req_type);


  if (!IsSysControl_local(req) && !IsSysControl_external(req, gid % ARCH_cpus))
    YS__errmsg(req->node,
	       "Illegal address 0x%08X at SysControl module\n",
	       req->paddr);


  if (IsSysControl_local(req))
    {
      req->paddr = req->paddr - SYSCONTROL_LOCAL_LOW + SYSCONTROL_THIS_LOW(gid % ARCH_cpus);
      if (req->src_proc < ARCH_cpus)
	ReplaceAddr(req->d.proc_data.inst, req->paddr);
    }


  if ((req->req_type == WRITE_UC) &&
      (req->paddr - SYSCONTROL_THIS_LOW(gid % ARCH_cpus) < SC_RDONLY_HIGH))
    YS__errmsg(req->node,
	       "Attempted write to read-only memory in SysControl module");


  /*-----------------------------------------------------------------------*/

  if (req->req_type == WRITE_UC)
    {
      creq = req;
      while (creq)
	{
	  old_addr = creq->paddr;
	  if (IsSysControl_local(creq))
	    {
	      creq->paddr = creq->paddr - SYSCONTROL_LOCAL_LOW + SYSCONTROL_THIS_LOW(gid % ARCH_cpus);
	      if (req->src_proc < ARCH_cpus)
		ReplaceAddr(creq->d.proc_data.inst, creq->paddr);
	    }

	  creq->perform(creq);

	  if (creq->paddr == SYSCONTROL_THIS_LOW(gid%ARCH_cpus) + SC_INTERRUPT)
	    ExternalInterrupt(gid, read_int(creq->node, creq->paddr));

	  
	  creq->paddr = old_addr;
	  if (req->src_proc < ARCH_cpus)
	    ReplaceAddr(creq->d.proc_data.inst, creq->paddr);
	  creq->complete(creq, IOHIT);

	  YS__PoolReturnObj(&YS__ReqPool, creq);

	  creq = creq->parent;
	}
 
      return(1);
    }

  /*-----------------------------------------------------------------------*/

  if (req->req_type == READ_UC)
    {
      if (!lqueue_full(&(syscontrol_ptr->below->outbuffer)) ||
	  !syscontrol_ptr->below->pending_writeback)
	{
	  creq = req;
	  while (creq)
	    {	  
	      creq->perform(creq);
	      creq = creq->parent;
	    }
 
	  req->type = REPLY;
	  req->req_type = REPLY_UC;
	  req->hit_type = IOHIT;

	  Cache_start_send_to_bus(syscontrol_ptr->below, req);
	}
      else
	return(0);
    }

  return(0);
}










