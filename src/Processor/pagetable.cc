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
/* Per-node physical page table                                            */
/* Maps physical (simulated) addresses to memory on the host machine.      */
/***************************************************************************/

#include <string.h>
#include <malloc.h>

extern "C"
{
#include "sim_main/simsys.h"
#include "sim_main/util.h"
}

#include "Processor/simio.h"
#include "Processor/procstate.h"
#include "Processor/pagetable.h"
#include "Processor/endian_swap.h"


page_table **PageTables;



/*=========================================================================*/
/* Constructor: initialize array of anchor elements.                       */
/*=========================================================================*/

page_table::page_table()
{
  for (int n = 0; n < PAGETABLE_SIZE; n++)
    {
      data[n].phys_addr = 0;
      data[n].sim_addr  = NULL;
      data[n].next      = NULL;
    }
}



/*=========================================================================*/
/* Destructor: free all elements.                                          */
/*=========================================================================*/

page_table::~page_table()
{
  struct page_table_entry *pte, *npte;

  for (int n = 0; n < PAGETABLE_SIZE; n++)
    {
      pte = data[n].next;
      while (pte)
	{
	  npte = pte->next;
	  delete pte;
	  pte = npte;
	}
    }
}




/*=========================================================================*/
/* Insert: insert an entry into the hash table. Hash physical page number  */
/* to find anchor element, scan linked list of elements until either an    */
/* unused element (sim_addr == NULL) or the end of the list is found (in   */
/* which case a new element is allocated and appended) and fill in the     */
/* found empty element.                                                    */
/*=========================================================================*/

void page_table::insert(unsigned phys_addr, char *sim_addr)
{
  unsigned index;
  page_table_entry *pte;

  index = (phys_addr / PAGE_SIZE) & PAGETABLE_HASH;
  pte = &data[index];

  while (pte->sim_addr != NULL)
    {
      if (pte->next == NULL)
	pte->next = new page_table_entry();

      pte = pte->next;
    }

  pte->phys_addr = phys_addr / PAGE_SIZE;
  pte->sim_addr  = sim_addr;
}




/*=========================================================================*/
/* Remove: hash physical page number to find anchor element. If anchor is  */
/* to be removed, copy next element into anchor and remove next element    */
/* from list, or just clear anchor element if no next element is found.    */
/* If anchor is not the desired element, scan list until element is found, */
/* keep track of previous element and simply remove desired element.       */
/*=========================================================================*/

void page_table::remove(unsigned phys_addr)
{
  unsigned          index;
  page_table_entry *pte, *opte;

  index = (phys_addr / PAGE_SIZE) & PAGETABLE_HASH;
  pte = &data[index];

  if (pte->phys_addr == phys_addr / PAGE_SIZE)  // anchor needs to be removed
    {
      if (pte->next != NULL)                      // has next element
	{
	  pte->phys_addr = pte->next->phys_addr;  // copy element
	  pte->sim_addr  = pte->next->sim_addr;
	  opte = pte->next;                       // save pointer to it
	  pte->next      = pte->next->next;       // remove from linked list
	  delete opte;                            // delete it
	}
      else                                        // anchor has no next elem.
	{
	  pte->phys_addr = 0;                     // clear it
	  pte->sim_addr  = NULL;
	  pte->next      = NULL;
	}

      return;
    }
  

  opte = pte;                                     // anchor is not desired elm.
  pte = pte->next;
  while (pte)                                     // look for match
    {
      if (pte->phys_addr == phys_addr / PAGE_SIZE)
	{
	  opte->next = pte->next;                 // remove element from list
	  delete pte;                             // delete it
	  return;
	}

      opte = pte;
      pte = pte->next;
    }
}





/*=========================================================================*/
/* Initialize per-node page table                                          */
/*=========================================================================*/

extern "C" void PageTable_init(void)
{
  int i;

  PageTables = (page_table**)malloc(ARCH_numnodes *
				    sizeof(page_table*));
 
  if (!PageTables)
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);
   
  for (i = 0; i < ARCH_numnodes; i++)
    PageTables[i] = new page_table();
}



/*=========================================================================*/
/* Insert a mapping for 1 page (4K) into the page table for this node, in  */
/* addition allocate physical (host) memory.                               */
/*=========================================================================*/

extern "C" void PageTable_insert_alloc(int node_id, unsigned addr)
{
  char *sim_mem = 0;

  sim_mem = PageTables[node_id]->lookup(addr);
  if (!sim_mem)
    {
      sim_mem = (char*)malloc(PAGE_SIZE);
      PageTables[node_id]->insert(addr, sim_mem);
    }
}



extern "C" void PageTable_insert(int node_id, unsigned addr, char *sim_mem)
{
  char *sm;

  sm = PageTables[node_id]->lookup(addr);
  if (!sm)
    PageTables[node_id]->insert(addr, sim_mem);
}





/*=========================================================================*/
/* Lookup address in page table                                            */
/*=========================================================================*/

extern "C" char *PageTable_lookup(int node_id, unsigned addr)
{
  return PageTables[node_id]->lookup(addr);
}
  





/*=========================================================================*/
/* Remove address from page table, with or without deallocating the        */
/* simulator page.                                                         */
/*=========================================================================*/

extern "C" void PageTable_remove(int node_id, unsigned addr)
{
  PageTables[node_id]->remove(addr);
}



extern "C" void PageTable_remove_free(int node_id, unsigned addr)
{
  char *sim_mem = PageTables[node_id]->lookup(addr);
  if (sim_mem)
    free(sim_mem);

  PageTables[node_id]->remove(addr);
}





/*=========================================================================*/
/* Read integer value                                                      */
/*=========================================================================*/

extern "C" unsigned int read_int(int node_id, unsigned addr)
{
  char *pa;
  unsigned int r;

  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  r = endian_swap(*(unsigned int*)pa);
  return(r);
}



/*=========================================================================*/
/* Read character value                                                    */
/*=========================================================================*/

extern "C" unsigned char read_char(int node_id, unsigned addr)
{
  char *pa;
  unsigned char r;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  r = *(unsigned char*)pa;
  return(r);
}



/*=========================================================================*/
/* Read float value                                                        */
/*=========================================================================*/

extern "C" float read_float(int node_id, unsigned addr)
{
  char *pa;
  float r;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  r = endian_swap(*(float*)pa);
  return(r);
}




/*=========================================================================*/
/* Read double value                                                       */
/*=========================================================================*/

extern "C" double read_double(int node_id, unsigned addr)
{
  char *pa;
  double r;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  r = endian_swap(*(double*)pa);
  return(r);
}



/*=========================================================================*/
/* Read bit i within an integer                                            */
/*=========================================================================*/

extern "C" int read_bit(int node_id, unsigned addr, int i)
{
  char *pa;
  unsigned r;

  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  r = endian_swap(*(unsigned*)pa);
  return((r >> i) & 1);
}


/*=========================================================================*/
/* Write integer value                                                     */
/*=========================================================================*/

extern "C" void write_int(int node_id, unsigned addr, unsigned int v)
{
  char *pa;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  *(unsigned int*)pa = endian_swap(v);
}



/*=========================================================================*/
/* Write character value                                                   */
/*=========================================================================*/

extern "C" void write_char(int node_id, unsigned addr, unsigned char v)
{
  char *pa;

  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  *(unsigned char*)pa = v;
}



/*=========================================================================*/
/* Write float value                                                       */
/*=========================================================================*/

extern "C" void write_float(int node_id, unsigned addr, float v)
{
  char *pa;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  *(float*)pa = endian_swap(v);
}



/*=========================================================================*/
/* Write double value                                                      */
/*=========================================================================*/

extern "C" void write_double(int node_id, unsigned addr, double v)
{
  char *pa;
  
  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  *(double*)pa = endian_swap(v);
}



/*=========================================================================*/
/* Write bit 1                                                             */
/*=========================================================================*/

extern "C" void write_bit(int node_id, unsigned addr, int i, unsigned v)
{
  char *pa;
  unsigned oldval;

  pa = PageTables[node_id]->lookup(addr);
  if (!pa)
    YS__errmsg(node_id, "Attempt to access non-existing memory %x\n", addr);

  oldval = endian_swap(*(unsigned*)pa);
  if (v == 0)
    oldval &= (0xFFFFFFFE << i);
  else
    oldval |= (0x00000001 << i);
  *(unsigned*)pa = endian_swap(oldval);
}
