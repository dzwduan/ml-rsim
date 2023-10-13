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
#include <malloc.h>
#include <string.h>


extern "C"
{
#include "sim_main/simsys.h"
}

#include "Processor/procstate.h"
#include "Processor/exec.h"
#include "Processor/mainsim.h"
#include "Processor/branchpred.h"
#include "Processor/simio.h"
#include "Processor/fastnews.h"
#include "Processor/active.hh"
#include "Processor/procstate.hh"
#include "Processor/memq.h"
#include "Processor/memunit.h"
#include "Processor/memunit.hh"
#include "Processor/tagcvt.hh"
#include "Processor/instance.h"
#include "Processor/pagetable.h"
#include "Processor/stallq.hh"
#include "Processor/tlb.h"


enum tlb_type DTLB_TYPE          = TLB_DIRECT_MAPPED;
int           DTLB_SIZE          = 128;
int           DTLB_ASSOCIATIVITY = 1;
int           DTLB_HARDWARE_FILL = 0;
int           DTLB_TAGGED        = 0;

enum tlb_type ITLB_TYPE          = TLB_DIRECT_MAPPED;
int           ITLB_SIZE          = 128;
int           ITLB_ASSOCIATIVITY = 1;
int           ITLB_HARDWARE_FILL = 0;
int           ITLB_TAGGED        = 0;

int           TLB_UNIFIED        = 0;



unsigned int tlb_read_word(ProcState *, long long, unsigned int);




//=============================================================================
// TLB Constructor: allocate memory and clear all entries
//=============================================================================

TLB::TLB (ProcState *p, enum tlb_type tp, int sz, int assoc, int tgd)
{
  int n;


  proc          = p;
  type          = tp;
  size          = sz;
  associativity = assoc;
  tagged        = tgd;
  
  entries = (struct tlb_entry*)malloc(sizeof(struct tlb_entry) * sz);
  if (entries == (struct tlb_entry*)NULL)
    YS__errmsg(0, "TLB Init: Malloc failed at %s:%i", __FILE__, __LINE__);

  for (n = 0; n < sz; n++)
    {
      entries[n].tag     = 0;
      entries[n].data    = 0;
      entries[n].context = 0;
      entries[n].age     = 0;
    }

  index = 0;
}



//=============================================================================
// Alternate constructor: clone existing TLB and share array of entries
//=============================================================================

TLB::TLB (TLB *tlb)
{
  int n;

  type           = tlb->type;
  size           = tlb->size;
  associativity  = tlb->associativity;
  tagged         = tlb->tagged;
  
  entries        = tlb->entries;
  proc           = tlb->proc;

  index = 0;
}




//=============================================================================
// TLB Destructor: free memory
//=============================================================================

TLB::~TLB()
{
  if (entries)
    free(entries);

  entries = NULL;
  size = 0;
}




//=============================================================================
// Probe TLB entry - return entry number that matches tag, or set highest bit
// if no matching entry is found
//=============================================================================

void TLB::ProbeEntry(unsigned int tag, int *entry)
{
  int n;

  for (n = 0; n < size; n++)
    if ((tlb_tag(entries[n].tag) == tlb_tag(tag)) &&
	tlb_valid(entries[n].tag))
      {
	*entry = n;
	return;
      }

  *entry = 0x80000000;                            // no match found
}



//=============================================================================
// Flush TLB - clear all entries
//=============================================================================

void TLB::Flush()
{
  int n;

  if (tagged)
    return;

  for (n = 0; n < size; n++)
    entries[n].tag  = 0;
}



//=============================================================================
// Read TLB entry
// return tag and data portion of TLB entry pointed to by 'entry'
//=============================================================================

void TLB::ReadEntry(int entry, unsigned int *tag, unsigned int *data,
		    unsigned int *ctx)
{
  if ((entry >= size) || (entry < 0))
    YS__errmsg(0, "Read from invalid TLB entry %i", entry);

  *tag  = entries[entry].tag;
  *data = entries[entry].data;
  *ctx  = entries[entry].context;
}




//=============================================================================
// Write TLB entry with tag and data
// clear reference count (LRU) for set associative TLB
//=============================================================================

void TLB::WriteEntry(int entry, unsigned int tag, unsigned int context,
		     unsigned int data)
{
  entry = entry % size;
  entries[entry].tag     = tag;
  entries[entry].data    = data;
  entries[entry].context = context;
}



//=============================================================================
// Return oldest (or invalid) entry in a set specified by tag
//=============================================================================

int TLB::MissEntry(unsigned int addr)
{
  int n, base, invalid;


  if (type == TLB_FULLY_ASSOC)
    return(0);

  if (type == TLB_DIRECT_MAPPED)
    {
      n    = PAGE_SIZE;
      base = addr;
      while (n != 1)
	{
	  n = n >> 1;
	  base = base >> 1;
	}
      base = base % size;
      return(base);
    }

  
  // set associative TLB ------------------------------------------------------
  // need to find invalid or oldest entry in set
  if (type == TLB_SET_ASSOC)
    {
      n    = PAGE_SIZE;
      base = addr;
      while (n != 1)
        {
          n = n >> 1;
          base = base >> 1;
        }
      base = base % (size / associativity);
      base *= associativity;

      invalid = -1;                               // first look for invalid
      for (n = 0; n < associativity; n++)         // entry in set
        if (!tlb_valid(entries[base+n].tag))
          {
            invalid = base+n;
            break;
          }
      
      if (invalid == -1)                          // no invalid entry found
        for (n = 0; n < associativity; n++)       // look for oldest entry
          if (entries[base+n].age == 0)
            invalid = base+n;

      return(invalid);
    }

  return(0);
}

  
//=============================================================================
// Perform TLB Lookup: find entry whose tag portion matches the virtual page
//   number of the virtual address in instance
//   hit & mapping is valid               : translate address
//   hit & mapping invalid                : segmentation fault
//   hit & store to rdonly                : segmentation fault
//   hit & user access to privileged page : segmentation fault
//   miss                                 : TLB miss
//=============================================================================

int TLB::LookUp(unsigned int *address, int *attributes, unsigned int context,
		int write, int priv, long long instr_tag)
{
  unsigned int i, n, base, old_age, addr, data, tag, ctx;
  struct tlb_entry *e, k;

  *attributes = 0;
  addr = *address;


  
  switch (type)
    {
      //-----------------------------------------------------------------------
      // perfect TLB, modelled as fully associative TLB with instantaneous
      // fills; search through entire TLB, if not found, directly access
      // physical memory to perform 2-level page table walk

    case TLB_PERFECT:
      n = 0;
      do
	{
	  tag  = entries[index].tag;
	  data = entries[index].data;
	  ctx  = entries[index].context;
	  
	  if ((tlb_tag(tag) == tlb_tag(addr)) &&
	      tlb_valid(tag) &&
	      ((tagged && ctx == context) || !tagged))
	    {
	      *attributes = tlb_attr(data);

	      if (write && tlb_rdonly(data))
		return(TLB_FAULT);

	      if (!priv && tlb_priv(data))
		return(TLB_FAULT);
	      
              if (tlb_mvalid(data))
		*address = tlb_data(data) | tlb_offset(addr);
	      else
		return(TLB_FAULT);

	      return(TLB_HIT);
	    }
	  
	  n++;
	  index = (index + 1) % size;
	}
      while (n < size);

      //-----------------------------------------------------------------------
      // not found:
      // read L0 page table entry (context + upper 10 address bits)
      data = tlb_read_word(proc, instr_tag,
			   proc->log_int_reg_file[arch_to_log(proc,
							      proc->cwp,
							      PRIV_TLB_CONTEXT)] +
			   (addr >> 20) & ~3);
      if (!tlb_mvalid(data))           // L0 entry not valid: fault
	return(TLB_FAULT);

      // read L1 entry (L0 + middle 10 address bits)
      data = tlb_read_word(proc, instr_tag,
			   (data & ~3) | ((addr >> 10) & 0xFFC));
      if (!tlb_mvalid(data))           // L1 not valid: fault
	return(TLB_FAULT);

      entries[index].tag     = (addr & ~(PAGE_SIZE-1)) | 1;
      entries[index].data    = data;
      entries[index].context = context;

      if (write && tlb_rdonly(data))
	return(TLB_FAULT);

      if (!priv && tlb_priv(data))
	return(TLB_FAULT);

      *attributes = tlb_attr(data);
      *address = tlb_data(data) | tlb_offset(addr);

      return(TLB_HIT);


      
      //-----------------------------------------------------------------------
      // fully associative TLB
      // search through entire TLB

    case TLB_FULLY_ASSOC:
      n = 0;
      do
	{
	  tag  = entries[index].tag;
	  data = entries[index].data;
	  ctx  = entries[index].context;

	  if ((tlb_tag(tag) == tlb_tag(addr)) &&
	      tlb_valid(tag) &&
	      ((tagged && ctx == context) || !tagged))
	    {
	      *attributes = tlb_attr(data);

	      if (write && tlb_rdonly(data))
		return(TLB_FAULT);

	      if (!priv && tlb_priv(data))
		return(TLB_FAULT);

              if (tlb_mvalid(data))
		*address = tlb_data(data) | tlb_offset(addr);
	      else
		return(TLB_FAULT);

	      return(TLB_HIT);
	    }
	  
	  n++;
	  index = (index + 1) % size;
	}
      while (n < size);
      
      return(TLB_MISS);


      
      //-----------------------------------------------------------------------
      // set associative TLB
      // compute start index as page-number modulo TLB size
      // round down to associativity
      // look for match within set
      // upon hit: set hit entry to associativity-1 (youngest entry)
      //           decrement every age above the hit entries original age
      //           leave rest unchanged
      
    case TLB_SET_ASSOC:
      n    = PAGE_SIZE;
      base = addr;
      while (n != 1)
	{
	  n = n >> 1;
	  base = base >> 1;
	}
      base = base % (size / associativity);
      base *= associativity;

      n = 0;
      do
	{
	  tag  = entries[base + n].tag;
	  data = entries[base + n].data;
	  ctx  = entries[base + n].context;

	  if ((tlb_tag(tag) == tlb_tag(addr)) &&
	      tlb_valid(tag) &&
	      ((tagged && ctx == context) || (!tagged)))
	    {
	      *attributes = tlb_attr(data);

	      if (write && tlb_rdonly(data))
		return(TLB_FAULT);
	      
	      if (!priv && tlb_priv(data))
		return(TLB_FAULT);
	      
	      if (tlb_mvalid(data))
		*address = tlb_data(data) | tlb_offset(addr);
	      else
		return(TLB_FAULT);
	      
	      old_age = entries[base+n].age;
	      entries[base+n].age = associativity-1;
	      for (i = 0; i < associativity; i++)
		{
		  if (i == n)
		    continue;
		  if (entries[base+i].age > old_age)
		    entries[base+i].age--;
		}
	      
	      return(TLB_HIT);
	    }
	  n++;
	}
      while (n < associativity);

      return(TLB_MISS);


      
      //-----------------------------------------------------------------------
      // direct mapped TLB
      // compute index as page-number modulo TLB-size
  
    case TLB_DIRECT_MAPPED:
      n    = PAGE_SIZE;
      base = addr;
      while (n != 1)
	{
	  n = n >> 1;
	  base = base >> 1;
	}
      base = base % size;

      if ((tlb_tag(entries[base].tag) == tlb_tag(addr)) &&
	  (tlb_valid(entries[base].tag)) &&
	  ((tagged && entries[base].context == context) || (!tagged)))
	{
	  *attributes = tlb_attr(entries[base].data);

	  if (write && tlb_rdonly(entries[base].data))
	    return(TLB_FAULT);
	      
	  if (!priv && tlb_priv(entries[base].data))
	    return(TLB_FAULT);

	  if (tlb_mvalid(entries[base].data))
	    *address = tlb_data(entries[base].data) | tlb_offset(addr);
	  else
	    return(TLB_FAULT);
	  
	  return(TLB_HIT);
	}

      return(TLB_MISS);


    default:
      return(TLB_FAULT);
    }
}




//=============================================================================
// Callback routine for hardware table walk, called by cache when request
// completes.
// If L0 access done: check entry and start L1 access
// If L1 access done: write TLB entry and resume processor
//=============================================================================

extern "C" void Perform_TLB_Fill(REQ *req)
{
  TLB *tlb = (TLB*)req->d.mem.aux;
  tlb->Perform_Fill(req);
}


void TLB::Perform_Fill(REQ *req)
{
  if (fill_data == 0)
    {
      fill_data = tlb_read_word(proc, fill_tag, req->paddr);

      if (!tlb_mvalid(fill_data))
	{
	  WriteEntry(fill_index, tlb_tag(fill_addr) | 1, fill_context, 0);
	  proc->DELAY = 0;
	}

      DCache_recv_tlbfill(proc->proc_id, Perform_TLB_Fill,
			  (unsigned char*)&fill_data,
			  (fill_data & ~3) | ((fill_addr >> 10) & 0xFFC),
			  this);
      
      return;
    }

  WriteEntry(fill_index,
	     tlb_tag(fill_addr) | 1,
	     fill_context,
	     fill_data);
  proc->DELAY = 0;
}



//=============================================================================
// Initiate TLB hardware fill
// Kick off L0 request to cache, using Perform_TLB_Fill as completion callback
//=============================================================================

void TLB::Fill(unsigned int addr, int idx, unsigned int ctx, long long tag)
{
  fill_addr    = addr;
  fill_index   = idx;
  fill_data    = 0;
  fill_context = ctx;
  fill_tag     = tag;

  DCache_recv_tlbfill(proc->proc_id, Perform_TLB_Fill,
		      (unsigned char*)&fill_data,
		      proc->log_int_reg_file[arch_to_log(proc,
							 proc->cwp,
							 PRIV_TLB_CONTEXT)] +
		      ((addr >> 20) & ~3), this);
}




//=============================================================================
// Access physical memory to read a word (page table entry)
// Check processor memory or store queue for older, pending stores
// Forward data if match, otherwise read physical memory
//=============================================================================

unsigned int tlb_read_word(ProcState *proc, long long tag, unsigned int addr)
{
  instance *conf;
  unsigned int data;
  int found;
  MemQLink<instance *> *ldindex = NULL;      

  found = 0;
  
#ifndef STORE_ORDERING
  //  proc->StoreQueue.GetMin(conf);
  while ((ldindex = proc->StoreQueue.GetNext(ldindex)) != NULL)
#else
    //  proc->MemQueue.GetMin(conf);
  while ((ldindex = proc->MemQueue.GetNext(ldindex)) != NULL)
#endif
    {
      conf = ldindex->d;

      if ((!IsStore(conf)) || (!conf->addr_ready) || (conf->busybits))
	continue;

      if (conf->tag >= tag)
	break;

      if ((conf->code.instruction == iSTW) && (addr == conf->addr))
	{
	  data = conf->rs1vali;
	  found = 1;
	}

      if (conf->code.instruction == iSTD)
	{
	  if (addr == conf->addr)
	    {
	      data = conf->rs1valipair.a;
	      found = 1;
	    }
	  else if (addr + sizeof(int) == conf->addr)
	    {
	      data = conf->rs1valipair.b;
	      found = 1;
	    }
	}
    }

  if (found)
    return(data);
  else
    return(read_int(proc->proc_id / ARCH_cpus, addr));
}



