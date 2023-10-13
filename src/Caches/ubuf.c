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

#include <stdlib.h>
#include <string.h>

#include "Processor/simio.h"
#include "Processor/tlb.h"
#include "Processor/memunit.h"
#include "sim_main/simsys.h"
#include "sim_main/stat.h"
#include "Caches/req.h"
#include "Caches/cache.h"
#include "Caches/ubuf.h"
#include "IO/io_generic.h"




UBUF **UBuffers;



int   MaxCoalesce         (UBUF*, int);



/*
#define UBUF_TRACE 
*/


/*===========================================================================*/
/* Add a request to the uncached buffer. Called by cached module.            */
/* Returns 1 if request was consumed, 0 if not. Removes request from         */
/* caches incoming request queue (above->request_queue) upon success, and    */
/* frees the request structure if appropriate.                               */
/*===========================================================================*/

int UBuffer_add_request(REQ *req)
{
  UBUF *ubufptr = PID2UBUF(req->node, req->src_proc);
  int  top, n;


#ifdef UBUF_TRACE
  YS__logmsg(req->node,
	     "### %i %s Request at Uncached Buffer 0x%08X (%i) %.0f\n",
	     req->type, ReqName[req->prcr_req_type],
	     req->paddr,
	     req->size,
	     YS__Simtime);
#endif

  
  /*=========================================================================*/
  /* memory barrier: mark in latest buffer entry if present                  */
  /*=========================================================================*/

  if (req->type == BARRIER)
    {
      if (ubufptr->num_entries > 0)
	{
	  ubufptr->entries[ubufptr->num_entries - 1]->fence = 1;

	  ubufptr->cur_reads++;
	  ubufptr->num_barr_eff++;

#ifdef UBUF_TRACE
	  YS__logmsg(req->node,
		     "### Inserting Memory Barrier at Uncached Buffer %.0f\n",
		     YS__Simtime);
#endif
	}	  

      lqueue_remove(&(ubufptr->above->request_queue));

      YS__PoolReturnObj(&YS__ReqPool, req);

      ubufptr->num_barriers++;

      return(1);
    }



      
  /*=========================================================================*/
  /* access to combining address space                                       */
  /*=========================================================================*/



  if ((req->type == REQUEST) && (req->prcr_req_type == RMW))
    YS__errmsg(req->node,
	       "RMW Request not supported by uncached buffer\n");

      
      
  /*=========================================================================*/
  /* non-accelerated requests                                                */
  /*=========================================================================*/
      
  top = ubufptr->num_entries;

  if ((ubufptr->combine) &&
      (ubufptr->num_entries > 0) &&
      ((req->size == WD) || (req->size == DW)))
    {
      for (n = ubufptr->num_entries-1; n >= 0; n--)
	{
	  if ((ubufptr->entries[n]->type != req->prcr_req_type) ||
	      (ubufptr->entries[n]->fence) ||
	      (ubufptr->entries[n]->busy))
	    break;

	  if (ubufptr->entries[n]->address ==
	      (req->paddr & ubufptr->line_mask))
	    {
	      top = n;
	      break;
	    }
	}
    }

      
  /* buffer full and combining failed - abort -------------------------------*/

  if ((top == ubufptr->num_entries) &&
      (ubufptr->num_entries >= ubufptr->size))
    {
      ubufptr->stall_ub_full++;
      return(0);
    }

	  
  /* allocate new entry if cannot (or may not) combine ----------------------*/
      
  if (top == ubufptr->num_entries)
    {
      memset(ubufptr->entries[top]->reqs,
	     0,
	     ubufptr->entrysz * sizeof(REQ*));
      ubufptr->num_entries++;
    }
      
#ifdef UBUF_TRACE
  else
    YS__logmsg(req->node,
	       "### Combining %x with %x in %i\n",
	       ubufptr->entries[top]->address,
	       req->paddr,
	       top);
#endif

  
  /* insert entry into corresponding bucket ---------------------------------*/

  req->hit_type = IOHIT;

  /* overwriting existing entry: mark as done (without perform) and free ----*/
  if (ubufptr->entries[top]->reqs[(unsigned)(req->paddr / 4) %
				 (unsigned)(ubufptr->entrysz)] != NULL)
    {
      MemDoneHeapInsertSpec(ubufptr->entries[top]->
			    reqs[(unsigned)(req->paddr / 4) %
				(unsigned)(ubufptr->entrysz)], IOHIT);
      ReturnMemopSpec(ubufptr->entries[top]->
		      reqs[(unsigned)(req->paddr / 4) %
			  (unsigned)(ubufptr->entrysz)]);
      YS__PoolReturnObj(&YS__ReqPool,
			ubufptr->entries[top]->
			reqs[(unsigned)(req->paddr / 4) %
			    (unsigned)(ubufptr->entrysz)]);
    }


  ubufptr->entries[top]->
    reqs[(unsigned)(req->paddr / 4) %
	(unsigned)(ubufptr->entrysz)] = req;
  ubufptr->entries[top]->fence   = 0;
  ubufptr->entries[top]->address = req->paddr & ubufptr->line_mask;
  ubufptr->entries[top]->type    = req->prcr_req_type;
  ubufptr->entries[top]->busy    = 0;
  
  if (req->prcr_req_type == READ)                 /* change request type     */
    {
      req->req_type = READ_UC;                    /* to `uncached'           */
      ubufptr->cur_reads++;
    }
  else
    req->req_type = WRITE_UC;

      
#ifdef UBUF_TRACE
  YS__logmsg(req->node,
	     "### Inserting %s (%x) into slot %i.%i\n",
	     ReqName[ubufptr->entries[top]->type],
	     req->paddr,
	     top,
	     (unsigned)(req->paddr / 4) % (unsigned)(ubufptr->entrysz));
#endif

  lqueue_remove(&(ubufptr->above->request_queue));
  FreeAMemUnit(req->d.proc_data.proc_id, req->d.proc_data.inst_tag);
  req->progress = 0;

  StatrecUpdate(ubufptr->occ,
		ubufptr->num_entries,
		(long long)YS__Simtime);

  return(1);
}





  
/*===========================================================================*/
/* Try to send a request to the bus interface. Called by main simulator      */
/* routine if regular uncached buffer is not empty, or CSB has a complete    */
/* request. Set flag 'transaction_pending' if something has been send to     */
/* the bus interface (below->..), the flag will be cleared by the bus        */
/* interface when the transaction is ready to issue (next in queue,          */
/* starts driving bus, completed on bus ..., depending on policy)            */
/*===========================================================================*/

void UBuffer_out_sim(int gid)
{
  REQ     *req, *nreq, *creq;
  UBUF    *ubufptr = UBuffers[gid];
  int      n, i, j, num_coals, size;
  int      max_wr_coals, max_rd_coals;



  if (ubufptr->transaction_pending)
    return;



  /*=========================================================================*/
  /* other uncached loads/stores                                             */
  
  max_wr_coals = 0;
  max_rd_coals = 0;

  if ((ubufptr->num_entries >= ubufptr->flush) ||
      (ubufptr->cur_reads > 0))
    {
      
      /*---------------------------------------------------------------------*/
      /* find first request within oldest entry                              */
      i = 0;
      while ((i < ubufptr->entrysz) &&
	     ((req = ubufptr->entries[0]->reqs[i]) == NULL))
	i++;

      
      if (i >= ubufptr->entrysz)
	YS__errmsg(gid / ARCH_cpus, "Empty entry in Uncached Buffer\n");


      /* check if can send down */

      
      /*---------------------------------------------------------------------*/
      /* if output port not full: possibly coalesce and send out             */

      
      num_coals = MaxCoalesce(ubufptr, i);         /* get number of coals    */
      creq = req;

      ubufptr->entries[0]->reqs[i] = NULL;
      j = i + req->size / 4;                       /* skip to next request   */
                                                   /* walk through request   */
      for (j = i + req->size / 4, n = req->size / 4;               
	   n < num_coals;                          /* array in size-steps    */
	   n += size / 4, j += size / 4)
	{
	  nreq = ubufptr->entries[0]->reqs[j];     /* get next request       */
	  size = nreq->size;
	  req->size += nreq->size;                 /* increment size         */
	      	      
	  creq->parent = nreq;
	  creq = nreq;
	  
          ubufptr->entries[0]->reqs[j] = NULL;

          /*-----------------------------------------------------------------*/
	  if (req->prcr_req_type == READ)
	    {
	      ubufptr->cur_reads--;
	      max_rd_coals += size;
	      ubufptr->num_rd_coals++;
	    }
	  else
	    {
	      max_wr_coals += size;
	      ubufptr->num_wr_coals++;
	    }
	}                                                  /* end for loop   */

      /*---------------------------------------------------------------------*/

      if (req->req_type == READ_UC)
	ubufptr->cur_reads--;

      ubufptr->transaction_pending = 1;
      Cache_start_send_to_bus(ubufptr->below, req);

#ifdef UBUF_TRACE
      YS__logmsg(gid / ARCH_cpus,
	      "### Forwarding %s request %x %i %.2f  -->  %i\n",
	      ReqName[req->req_type],
	      req->paddr,
	      req->size,
	      YS__Simtime,
	      ubufptr->num_entries);
#endif
      
      /* statistics ---------------------------------------------------------*/

      if ((max_wr_coals > 0) && (ubufptr->max_wr_coals < req->size))
	ubufptr->max_wr_coals = req->size;
      if ((max_rd_coals > 0) && (ubufptr->max_rd_coals < req->size))
	ubufptr->max_rd_coals = req->size;

      n = i + num_coals;                               /* check if rest      */
      while ((n < ubufptr->entrysz) &&                 /* of entry is empty  */
	     (ubufptr->entries[0]->reqs[n] == NULL))
	n++;

      if (n >= ubufptr->entrysz)                       /* if empty: shift    */
	{
	  UBUFITEM *old;
	  
	  if (ubufptr->entries[0]->fence)
	    ubufptr->cur_reads--;

	  old = ubufptr->entries[0];
	  
	  for (n = 1; n < ubufptr->num_entries; n++)   /* shift all entries  */
	    ubufptr->entries[n-1] = ubufptr->entries[n];
	  ubufptr->entries[ubufptr->num_entries-1] = old;
	  /* clear old top entry  */
	      
	  ubufptr->num_entries--;

	  StatrecUpdate(ubufptr->occ,
			ubufptr->num_entries,
			YS__Simtime);
	}
      else
	ubufptr->entries[0]->busy = 1;
    }
}




/*===========================================================================*/
/* determine maximum number of request that can be combined, beginning at    */
/* 'start'. The size of a combined request must be a power of two, and the   */
/* start address must be aligned to this size. Returns size of request.      */
/*===========================================================================*/

int MaxCoalesce(UBUF *ubufptr, int start)
{
  int align, max, n;

  if (ubufptr->entries[0]->reqs[start]->size < WD)
    return(0);


  /* count consecutive requests and sum up total request size -------------*/
  for (n = start, max = 0;
       (n < ubufptr->entrysz) &&
	 (ubufptr->entries[0]->reqs[n] != NULL);
	 max += ubufptr->entries[0]->reqs[n]->size / 4,
       n += ubufptr->entries[0]->reqs[n]->size / 4);


  /* reduce size of request until alignment is OK -------------------------*/
  align = ubufptr->entrysz;
  while ((align > 1) && ((start % align) != 0))
    align /= 2;


  /* reduce size further until less than or equal to uncached buffer size -*/
  for (n = ubufptr->entrysz;
       n > max;
       n /= 2);
  max = n;

#ifdef UBUF_TRACE
  for (n = 0; n < ubufptr->entrysz; n++)
    YS__logmsg(ubufptr->nodeid,
	       "%s ", ubufptr->entries[0]->reqs[n] == NULL ? "_" : "X");
  YS__logmsg(ubufptr->nodeid,
	     " -> %i (%i %i)\n", max >= align ? align : max, align, max);
#endif
  
  if (max >= align)
    return align;
  else
    return max;
}



/*=========================================================================*/
/* pending uncached buffer transaction is accepted by bus interface, and   */
/* ordering constraints are satisfied -> next request can be sent.         */
/*=========================================================================*/

void UBuffer_confirm_transaction(REQ* req)
{
  PID2UBUF(req->node, req->src_proc)->transaction_pending = 0;
}



/*===========================================================================*/
/* UBuffer_init: creates and initializes all UBuffer modules                 */
/*===========================================================================*/

void UBuffer_init(void)
{
  int      n, c, i;
  UBUF    *ubufptr;
  char     ubstr[40];


  /*-------------------------------------------------------------------------*/

  UBuffers = RSIM_CALLOC(UBUF*, ARCH_numnodes * ARCH_cpus);
  if (!UBuffers)
    YS__errmsg(0, "Malloc failed at %s:%i", __FILE__, __LINE__);

  /*-------------------------------------------------------------------------*/

  for (n = 0; n < ARCH_numnodes; n++)
    for (c = 0; c < ARCH_cpus; c++)
    {
      ubufptr = RSIM_CALLOC(UBUF, 1);
      if (!ubufptr)
	YS__errmsg(n,
		   "Malloc failed at %s:%i",
		   __FILE__, __LINE__);

      PID2UBUF(n, c) = ubufptr;

      ubufptr->size        = 4;
      ubufptr->entrysz     = 8;
      ubufptr->combine     = 1;
      ubufptr->flush       = 1;


      get_parameter("ubufsize",      &ubufptr->size,    PARAM_INT);
      get_parameter("ubufflush",     &ubufptr->flush,   PARAM_INT);
      get_parameter("ubufentrysize", &ubufptr->entrysz, PARAM_INT);

      strcpy(ubstr, "comb");
      get_parameter("ubuftype", ubstr, PARAM_STRING);  
      if (strncasecmp(ubstr, "comb", 4) == 0)
	ubufptr->combine = 1;
      else if (strncasecmp(ubstr, "nocomb", 6) == 0)
	ubufptr->combine = 0;
      else
	{
	  fprintf(stderr, "Unknown uncached buffer type %s\n", ubstr);
	  exit(1);
	}
      

      /*---------------------------------------------------------------------*/

      ubufptr->nodeid = n;
      ubufptr->procid = c;

      ubufptr->above = PID2L1D(n, c);
      ubufptr->below = PID2L2C(n, c);

      ubufptr->num_entries = 0;
      ubufptr->line_mask   = ~(ubufptr->entrysz-1);

      ubufptr->entries     = RSIM_CALLOC(UBUFITEM*, ubufptr->size);
      if (!ubufptr->entries)
	YS__errmsg(n, "Malloc failed at %s:%i", __FILE__, __LINE__);

      for (i = 0; i < ubufptr->size; i++)
	{
	  ubufptr->entries[i] = RSIM_CALLOC(UBUFITEM, 1);
	  if (ubufptr->entries[i] == NULL)
	    YS__errmsg(n, "Malloc failed at %s:%i", __FILE__, __LINE__);

	  ubufptr->entries[i]->reqs = RSIM_CALLOC(REQ*, ubufptr->entrysz);
	  if (ubufptr->entries[i]->reqs == NULL)
	    YS__errmsg(n, "Malloc failed at %s:%i", __FILE__, __LINE__);
	  memset(ubufptr->entries[i]->reqs,
		 0,
		 ubufptr->entrysz * sizeof(REQ*));
	}

      ubufptr->cur_reads = 0;

      ubufptr->transaction_pending = 0;

      /*---------------------------------------------------------------------*/

      ubufptr->occ = NewStatrec(n,
				"UBuf occupancy",
				INTERVAL,
				MEANS,
				HIST,
				ubufptr->size,
				0,
				ubufptr->size);

      UBuffer_stat_clear(n, c);
    }
}




/*===========================================================================*/
/* UBuffer_Print_params(): Print uncached buffer configuration               */
/*===========================================================================*/


void UBuffer_print_params(int nid, int pid)
{
  UBUF *ubufptr = PID2UBUF(nid, pid);

  YS__statmsg(nid, "Uncached Buffer Configuration\n");

  YS__statmsg(nid, "  Entries: %d\t\tEntrysize: %d\n",
	      ubufptr->size, ubufptr->entrysz);
  YS__statmsg(nid, "  %s\t\tFlush at %i\n\n",
	      ubufptr->combine ? "Combine" : "Don't Combine",
	      ubufptr->flush);


  YS__statmsg(nid, "\n");
}





 
/*===========================================================================*/
/* UBuffer_stat_report: Reports statistics of a UBuffer module               */
/*===========================================================================*/
 
void UBuffer_stat_report(int nid, int pid)
{
  UBUF *ubufptr = PID2UBUF(nid, pid);

  YS__statmsg(nid, "Uncached Buffer Statistics\n");

  YS__statmsg(nid,
	      "  Stalls (Buffer Full)  : %lld cycles\n",
	      ubufptr->stall_ub_full);

  StatrecReport(nid, ubufptr->occ);

  if (ubufptr->num_writes)
    YS__statmsg(nid,
		"  Total writes    : %8lld\n",
		ubufptr->num_writes);
  
  if (ubufptr->num_wr_coals)
    YS__statmsg(nid,
		"  Writes Coalesced: %8lld\tMax writes coalesced: %8lld bytes\n",
		ubufptr->num_wr_coals,
		ubufptr->max_wr_coals);

  if (ubufptr->num_reads)
    YS__statmsg(nid,
		"  Total reads     : %8lld\tRead Latency        : %8.3f cycles\n",
		ubufptr->num_reads,
		ubufptr->read_latency / (double)ubufptr->num_reads);
  
  if (ubufptr->num_rd_coals)
    YS__statmsg(nid,
		"  Reads Coalesced : %8lld\tMax reads coalesced : %8lld bytes\n",
		ubufptr->num_rd_coals,
		ubufptr->max_rd_coals);

  if (ubufptr->num_barriers)
    YS__statmsg(nid,
		"  Memory Barriers : %8lld\teffective : %lld\n\n",
		ubufptr->num_barriers,
		ubufptr->num_barr_eff);


  
  YS__statmsg(nid, "\n");
}




/*===========================================================================*/
/* UBuffer_stat_set: set statistics for completed transaction                */
/*===========================================================================*/

void UBuffer_stat_set(REQ *req)
{
  UBUF *ubufptr = PID2UBUF(req->node, req->src_proc);
  
  if (req->prcr_req_type == READ)
    {
      ubufptr->num_reads++;
      ubufptr->read_latency += (YS__Simtime - req->issue_time);
    }

  if (req->prcr_req_type == WRITE)
    {
      ubufptr->num_writes++;
    }

}



/*===========================================================================*/
/* UBuffer_stat_clear: Resets statistics collection for a UBuffer module     */
/*===========================================================================*/
 
void UBuffer_stat_clear(int nid, int pid)
{
  UBUF *ubufptr = PID2UBUF(nid, pid);
  
  StatrecReset(ubufptr->occ);
    
  ubufptr->stall_ub_full  = 0;

  ubufptr->num_wr_coals   = 0;
  ubufptr->max_wr_coals   = 0;
  ubufptr->num_rd_coals   = 0;
  ubufptr->max_rd_coals   = 0;

  ubufptr->num_barriers   = 0;
  ubufptr->num_barr_eff   = 0;

  ubufptr->num_writes     = 0;
  ubufptr->num_reads      = 0;
  ubufptr->read_latency   = 0.0;

}



/*===========================================================================*/
/* Dump exhaustive debugging information                                     */
/*===========================================================================*/

void UBuffer_dump(int proc_id)
{
  int nid = proc_id / ARCH_cpus;
  int n, m;
  UBUF *ubufptr = PID2UBUF(nid, proc_id % ARCH_cpus);

  YS__logmsg(nid, "\n====== UNCACHED BUFFER ======\n");

  YS__logmsg(nid, "num_entries(%d), cur_reads(%d), transaction_pending(%d)\n",
	     ubufptr->num_entries, ubufptr->cur_reads,
	     ubufptr->transaction_pending);
  
  for (n = 0; n < ubufptr->num_entries; n++)
    {
      YS__logmsg(nid,
		 "item[%i]: fence(%i), address(0x%x), type(%s), busy(%s)\n",
		 n, ubufptr->entries[n]->fence, ubufptr->entries[n]->address,
		 ReqName[ubufptr->entries[n]->type],
		 ubufptr->entries[n]->busy);
      for (m = 0; m < ubufptr->entrysz; m++)
	{
	  if (ubufptr->entries[n]->reqs[m] == NULL)
	    continue;
	  Cache_req_dump(ubufptr->entries[n]->reqs[m], 0x11, nid);
	}
    }

}
