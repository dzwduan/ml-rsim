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

#ifndef __RSIM_DRAM_H__
#define __RSIM_DRAM_H__

#include "DRAM/cqueue.h"




/* Definition of a DRAM transaction, one for each DRAM access */
typedef struct dram_trans
{
  mmc_trans_t	    *ptrans;   /* the memory transaction spawned this access */
  rsim_time_t        time;     /* time entering a stage (e.g. SA/bank/RD/SD) */
  rsim_time_t        etime;    /* time being initiated                       */
  unsigned	     paddr;    /* physical address                           */
  short	             size;     /* size of this access                        */
  unsigned char	
    locked       : 1,          /* is locked at the head of waiting queue?    */
    open_row     : 1,          /* leave row open after this access?          */
    is_write     : 1;          /* 0 -- read access; 1 -- write access        */
} dram_trans_t;




/* Slave Address bus */
typedef struct
{
  circbuf_t    *waiters;       /* waiters' fifo queue                        */
  EVENT        *pevent;        /* worker task to control the waiters         */
  int 		busy;          /* is the bus being used?                     */
   
  long long     count;         /* accesses having used this bus              */
  rsim_time_t   cycles;        /* total cycles being used                    */
  long long     waits;         /* accesses having been in waiter queue       */
  rsim_time_t   wcycles;       /* total cycles spent on waiter queue         */
} dram_sa_bus_t; 



/* Slave Data bus */
typedef struct
{
  rsim_time_t  busy_until;     /* when the SD bus will be free               */
  long long    count;          /* accesses having used this bus              */
  rsim_time_t  cycles;         /* total cycles being used                    */
  long long    waits;          /* accesses having waited for the bus         */
  rsim_time_t  wcycles;        /* total waiting cycles                       */
} dram_sd_bus_t; 




/* RD_bus (one for each Slave Memory Controller) */
typedef struct _rd_bus_
{
  rsim_time_t  busy_until;     /* when the RD bus will be free               */
  long long    count;          /* accesses having used this bus              */
  rsim_time_t  cycles;         /* total cycles being used                    */
  long long    waits;          /* accesses having waited for the bus         */
  rsim_time_t  wcycles;        /* total waiting cycles                       */
} dram_rd_bus_t;




/*
 * Databuffer (Accumulate/Mux chip) connects several RD busses to 
 * several SD busses via one MD bus.
 */
typedef struct _databuf_
{
  circbuf_t	*waiters;     /* waiters waiting on RD busses                */
  EVENT         *cevent;      /* worker task to dispatch waiters             */
  EVENT         *devent;      /* worker task to dispatch waiters             */
  void		*busy;        /* transaction that's using the data buffer    */
} dram_databuf_t;



typedef struct _chip_
{
  int           id;
  rsim_time_t	cas_busy;                  /* CAS bus busy until             */
  rsim_time_t	ras_busy;                  /* RAS bus busy until             */
  int           last_type;                 /* Is last access a write/read?   */
  int           last_bank;                 /* the bank the last access goes  */

  EVENT        *refresh_event;             /* refreshing task                */
  int 	        refresh_needed;            /* need refresh                   */
  int 		refresh_on;                /* refresh operation is underway  */
} dram_chip_t;




/* Definition of bank waiting queue elements */
typedef struct _bank_queue_elm_
{
  struct _bank_queue_elm_ *next;
  struct _bank_queue_elm_ *prev;
  dram_trans_t            *data;
} bank_queue_elm_t;




/* Definition of bank waiting queue. It's a double-linked queue */
typedef struct
{
  int               max;
  int               size;

  bank_queue_elm_t *free;
  bank_queue_elm_t *head;
  bank_queue_elm_t *tail;
} bank_queue_t;




/* Bank-related statistics. One for each DRAM bank */
typedef struct
{
  long long  readwrites;           /* total reads/writes                     */
  long long  reads;                /* total reads                            */
  long long  writes;               /* total writes                           */
  long long  read_hits;            /* reads hit on the hot row.              */
  long long  write_hits;           /* writes hit on the hot row.             */
  long long  read_misses;          /* reads missed on the hot row.           */
  long long  write_misses;         /* writes missed on the hot row.          */
  long long  queue_len;            /* waiting queue length                   */
  long long  queue_waits;          /* accesses having been in waiting queue  */
  double     queue_cycles;         /* time spent on the waiting queue        */
  double     access_cycles;        /* sum of the latency of all accesses     */
  long long  overlap;              /* total overlapped cycles                */
} dbank_stat_t;





/* DRAM bank. Each bank has a worker event(task), and a queue of
 * transanctions waiting to be processed.
 */
typedef	struct _bank_
{
  char		 id;               /* bank id                                */
  dram_chip_t   *chip;             /* the chip that the bank resides in      */
  void		*busy;             /* the access that's using the bank       */
  bank_queue_t	 waiters;          /* waiting queue                          */
  EVENT	*pevent;                   /* event to control the waiting queue     */

  u_int          hot_row;          /* the hot row                            */
  rsim_time_t    expire;           /* by when the hot row is expired         */
  char		 hitmiss;          /* hot row hit/miss history               */
  int            last_type;        /* Is last access a write/read?           */

  dbank_stat_t   stats;            /* statistics                             */
} dram_bank_t;





/* Use an array of structures to hold per-processor DRAM-backend information */
typedef struct
{
  char	          nodeid;	       /* processor ID                       */
  LinkQueue       freedramlist;        /* free dram transactions             */
  dram_sa_bus_t   sa_bus;              /* Slave Address busses               */
  dram_sd_bus_t   sd_bus;              /* Slave Data busses                  */
  dram_rd_bus_t  *rd_busses;           /* RD bus (Correspendent to SMC)      */
  dram_databuf_t *databufs;            /* Data buffers (Accumulate/Mux chips)*/
  dram_chip_t    *chips;               /* Chips                              */
  dram_bank_t    *banks;               /* DRAM banks                         */
  int             total_bwaiters;      /* total DRAM accesses on the backend */
  long long       total_count;         /* statistics for the average cycles  */
  rsim_time_t     total_cycles;        /*   of a DRAM access.                */
  long long       sgle_count;          /* statistics for the average cycles  */
  rsim_time_t     sgle_cycles;         /*   of a DRAM access.                */

  LinkQueue       waitlist;
  EVENT          *pevent;
} dram_info_t;



extern dram_info_t *DBs;

#define NID2DRAM(nid)	&(DBs[(nid)])




/**************************************************************************/
/********************* Useful macros **************************************/
/**************************************************************************/

/* Find the associated bank for a specified physical address */
#define DRAM_paddr_to_bank(pdb, paddr)			\
           (&(pdb->banks[(paddr >> dparam.bank_shift) & dparam.bank_mask]))

/* Find the associated chip for a bank */
#define DRAM_bankid_to_chip(pdb, bankid)			\
                ((dparam.interleaving & 2) ?			\
                 &pdb->chips[bankid >> dparam.chip_shift] :	\
                 &pdb->chips[bankid & dparam.chip_mask])

/* Find the associated databuf for a bank */
#define DRAM_bankid_to_databuf(pdb, bankid)			\
                ((dparam.interleaving & 2) ?			\
                 &pdb->databufs[bankid >> dparam.databuf_shift] :\
                 &pdb->databufs[bankid & dparam.databuf_mask])

/* Find the associated RD_BUS for a bank */
#define DRAM_bankid_to_rd_bus(pdb, bankid)			   \
                ((dparam.interleaving & 2) ?			   \
                 &pdb->rd_busses[bankid >> dparam.rd_bus_shift] : \
                 &pdb->rd_busses[bankid & dparam.rd_bus_mask])

/* To check if two physical addresses access the same row */
#define DRAM_in_same_row(paddr1, paddr2)                \
                ((paddr1 >> dparam.row_shift) ==       \
                 (paddr2 >> dparam.row_shift))

/* To check if there is a data dependence hazard between two DRAM accesses */
#define DRAM_check_data_dependence(dtrans1, dtrans2)                    \
          ((dtrans1->is_write || dtrans2->is_write) &&                  \
           ((dtrans1->paddr >= dtrans2->paddr &&                        \
             dtrans1->paddr < (dtrans2->paddr + dtrans2->size)) ||      \
            (dtrans2->paddr >= dtrans1->paddr &&                        \
             dtrans2->paddr < (dtrans1->paddr + dtrans1->size))))               

/*
 * To see if there are too many DRAM accesses in the DRAM backend.
 * If yes, the MMC will stop generate more DRAM accesses.
 */
#define DRAM_bankqueue_criticalfull(nid)                             \
        ((NID2DRAM(nid))->total_bwaiters >= dparam.max_bwaiters);

/* Is the bank waiting queue full? */
#define Bank_queue_full(bq) ((bq)->size == (bq)->max)



/**************************************************************************/
/********************* Functions definitions ******************************/
/**************************************************************************/

/* dram_init.c dram_main.c */
void          DRAM_init              (void);
void          DRAM_read_params       (void);
void          DRAM_recv_request      (int nodeid, mmc_trans_t *,
				      unsigned paddr, int size,
				      int is_write);
void          DRAM_arrive_smc        (void);
void          DRAM_access_bank       (dram_info_t *, dram_trans_t *,
				      rsim_time_t);
void          DRAM_access_bank_aux1  (dram_info_t *, dram_trans_t *,
				      rsim_time_t);
void          DRAM_access_SDRAM_bank (dram_info_t *, dram_trans_t *,
				      rsim_time_t);
void          DRAM_access_RDRAM_bank (dram_info_t *, dram_trans_t *,
				      rsim_time_t);
void          DRAM_bank_done         (void);
void          DRAM_databuf_data_ready (void);
void          DRAM_databuf_done       (void);
void          DRAM_sd_bus_schedule   (dram_info_t *, dram_databuf_t *,
				      dram_trans_t *,int );
void          Bank_queue_enqueue     (bank_queue_t *, dram_trans_t *);
dram_trans_t *Bank_queue_dequeue     (dram_bank_t *pbank);
dram_trans_t *DRAM_new_transaction   (dram_info_t *, mmc_trans_t *, unsigned, 
                                      int, int );
void          DRAM_end_transaction   (dram_info_t *, dram_trans_t *);
void          DRAM_nosim_start       (dram_info_t *, dram_trans_t *);
void          DRAM_nosim_done        (void);



/* dram_refresh.c */
void          DRAM_refresh           (void);
void          DRAM_refresh_done      (void);
void          DRAM_start_refresh     (dram_info_t *pdb, int bank);
int           DRAM_do_refresh        (dram_info_t *pdb, int bank);



/* dram_stat.c, dram_debug.c */
void         DRAM_print_params       (int);
void         DRAM_stat_report        (int);
void         DRAM_stat_clear         (int);
void         DRAM_exit               (void);
void         DRAM_dump               (int nodeid);

#endif


