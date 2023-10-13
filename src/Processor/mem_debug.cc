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

#include <sys/time.h>
#include <limits.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "Caches/system.h"
}                    

#include "Processor/procstate.h"
#include "Processor/exec.h"
#include "Processor/memunit.h"
#include "Processor/memq.h"
#include "Processor/mainsim.h"
#include "Processor/branchpred.h"
#include "Processor/simio.h"
#include "Processor/fastnews.h"
#include "Processor/stallq.hh"
#include "Processor/tagcvt.hh"
#include "Processor/memunit.hh"
#include "Processor/active.hh"
#include "Processor/procstate.hh"



extern "C" void DumpInstance(instance *inst, int pid)
{
  YS__logmsg(pid / ARCH_cpus,
	     "instance: %s, tag(%lld), pc(0x%x), addr(0x%x)\n",
	     IsLoad(inst) ? "load" : (IsStore(inst) ? "WRITE" : "RMW"),
	     inst->tag, inst->pc, inst->addr);
  YS__logmsg(pid / ARCH_cpus,
	     "addr_attr(0x%x), addr_ready(%d), truedep(%d)\n",
	     inst->addr_attributes, inst->addr_ready, inst->truedep);
  YS__logmsg(pid / ARCH_cpus,
	     "          memprog(%lld), in_mem(%d), globalperf(%d), pref(%d)\n",
	     inst->memprogress, inst->in_memunit, inst->global_perform,
	     inst->prefetched);
  YS__logmsg(pid / ARCH_cpus,
	     "          stallqs(%d), except(%d), limbo(%d), name(%s)\n",
	     inst->stallqs, inst->exception_code, inst->limbo,
	     inames[inst->code.instruction]);
}



extern "C" void DumpProcMemQueue(int proc_id, int maxinst)
{ 
  int i = 0;

  ProcState *proc = AllProcs[proc_id];
  MemQLink<instance *> *index = NULL;

  YS__logmsg(proc_id / ARCH_cpus,
	     "\n=========== Processor Memory Unit =========\n");

#ifdef STORE_ORDERING
  YS__logmsg(proc_id / ARCH_cpus,
	     "ProcMemQueue: size = %d\n",
	     proc->MemQueue.NumItems());
  while (index = proc->MemQueue.GetNext(index))
    {
      if (++i > maxinst)
	break;
      DumpInstance(index->d, proc_id);
    }
#else
  
  YS__logmsg(proc_id / ARCH_cpus,
	     "ProcLoadQueue: size = %d\n", proc->LoadQueue.NumItems());
  while (index = proc->LoadQueue.GetNext(index))
    {
      if (++i > maxinst)
	break;
      DumpInstance(index->d, proc_id);
    }
  
  YS__logmsg(proc_id / ARCH_cpus,
	     "ProcStoreQueue: size = %d\n", proc->StoreQueue.NumItems());
  while (index = proc->StoreQueue.GetNext(index))
    {
      if (++i > maxinst)
	break;
      DumpInstance(index->d, proc_id);
    }
#endif  

   YS__logmsg(proc_id / ARCH_cpus,
	      "MemDoneHeap: size = %d\n", proc->MemDoneHeap.num());
   if (proc->MemDoneHeap.num() > 0)
     DumpInstance(proc->MemDoneHeap.PeekHeadInst(), proc_id);
}
