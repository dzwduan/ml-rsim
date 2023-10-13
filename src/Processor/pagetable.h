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
/* Simulator Page Table for each node. Translates simulated addresses to   */
/* host addresses. Processors maintain a pointer to the page table, which  */
/* is shared among all processors within a node.  From C++ (processors),   */
/* use the normal hashtable methods for insertion and lookup, C code uses  */
/* the wrappers provided below for insertion, lookup and the actual memory */
/* access (for instance by I/O devices).                                   */
/***************************************************************************/


#ifndef __RSIM_PAGETABLE_H__
#define __RSIM_PAGETABLE_H__


/* regular C declarations, e.g. for I/O devices ---------------------------*/
#ifndef __cplusplus

void            PageTable_init         (void);
void            PageTable_insert_alloc (int, unsigned);
void            PageTable_insert       (int, unsigned, char*);
char           *PageTable_lookup       (int, unsigned);
void            PageTable_remove       (int, unsigned);
void            PageTable_remove_free  (int, unsigned);

unsigned int    read_int               (int, unsigned);
unsigned char   read_char              (int, unsigned);
float           read_float             (int, unsigned);
double          read_double            (int, unsigned);
int             read_bit               (int, unsigned, int);

void            write_int              (int, unsigned, unsigned int);
void            write_char             (int, unsigned, unsigned char);
void            write_float            (int, unsigned, float);
void            write_double           (int, unsigned, double);
void            write_bit              (int, unsigned, int, int);



/* C++ declarations, probably not used by anybody -------------------------*/
#else

extern "C"
{
  void  PageTable_init         (void);
  void  PageTable_insert_alloc (int, unsigned);
  void  PageTable_insert       (int, unsigned, char*);
  char *PageTable_lookup       (int, unsigned);
  void  PageTable_remove       (int, unsigned);
  void  PageTable_remove_free  (int, unsigned);

  unsigned int    read_int     (int, unsigned);
  unsigned char   read_char    (int, unsigned);
  float           read_float   (int, unsigned);
  double          read_double  (int, unsigned);
  int             read_bit     (int, unsigned, int);

  void            write_int    (int, unsigned, unsigned int);
  void            write_char   (int, unsigned, unsigned char);
  void            write_float  (int, unsigned, float);
  void            write_double (int, unsigned, double);
  void            write_bit    (int, unsigned, int, unsigned);
}




struct page_table_entry
{
  unsigned                 phys_addr;
  char                    *sim_addr;
  struct page_table_entry *next;

  page_table_entry()
  {
    phys_addr = 0;
    sim_addr  = NULL;
    next      = NULL;
  }
};



#define PAGETABLE_SIZE 0x10000
#define PAGETABLE_HASH (PAGETABLE_SIZE-1)


class page_table
{
private:
  struct page_table_entry data[PAGETABLE_SIZE];

public:
  page_table();
  ~page_table();
  void insert(unsigned phys_addr, char *sim_addr);
  void remove(unsigned phys_addr);
  
  inline char *lookup(unsigned phys_addr)
  {
    unsigned          index;
    page_table_entry *pte;

    index = (phys_addr / PAGE_SIZE) & PAGETABLE_HASH;
    pte = &data[index];

    while (pte)
      {
	if (pte->phys_addr == phys_addr / PAGE_SIZE)
	  return (char*)(pte->sim_addr + (phys_addr & (PAGE_SIZE-1)));
	
	pte = pte->next;
      }

    return(NULL);
  }
};


extern page_table **PageTables;


#endif

#endif
