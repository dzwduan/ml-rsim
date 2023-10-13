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

#ifndef __RSIM_PROCSTATE_H__
#define __RSIM_PROCSTATE_H__


#define SIZE_OF_SPARC_INSTRUCTION       4

#undef PAGE_SIZE
#define PAGE_SIZE                    0x00001000


#ifdef __cplusplus

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>

#include "Processor/proc_config.h"
#include "Processor/registers.h"
#include "Processor/funcunits.h"
#include "Processor/instruction.h"
#include "Processor/instance.h"
#include "Processor/heap.h"
#include "Processor/hash.h"
#include "Processor/allocator.h"
#include "Processor/memq.h"
#include "Processor/fetch_queue.h"
#include "Processor/queue.h"
#include "Processor/stallq.h"
#include "Processor/tagcvt.h"
#include "Processor/circq.h"
#include "Processor/active.h"
#include "Processor/freelist.h"
#include "Processor/tlb.h"
#include "Processor/pagetable.h"

extern "C"
{
#include "Caches/system.h"
#include "Caches/req.h"
#include "Caches/cache.h"
} 



struct MapTable;
struct BranchQElement;

extern int  DEBUG_TIME;  /* time to enable debugging on */

extern ProcState **AllProcs;
extern int       aliveprocs;


extern int MAX_ACTIVE_NUMBER; // Maximum number of elements in the active list
extern int MAX_ACTIVE_INSTS;

#define MAX_MAX_ACTIVE_NUMBER 4096 

extern int FETCH_QUEUE_SIZE;    // 8

extern int FETCHES_PER_CYCLE;   // 4
extern int DECODES_PER_CYCLE;   // 4
extern int GRADUATES_PER_CYCLE; // 4
extern int EXCEPT_FLUSHES_PER_CYCLE;

extern int MAX_SPEC;            // Maximum number of speculations allowed

extern int MAX_ALU_OPS;         // max unissued integer ops in proc at a time 
extern int MAX_FPU_OPS;         // max unissued FP ops in proc at a time 
extern int MAX_MEM_OPS;         // max mem ops in mem queue 
extern int MAX_STORE_BUF;       // max number of unissued stores


/* number of functional units of each type */
extern int ALU_UNITS, FPU_UNITS, MEM_UNITS, ADDR_UNITS;


#define DEFMAX_ALU_OPS        16
#define DEFMAX_FPU_OPS        16
#define DEFMAX_MEM_OPS 	      16
#define DEFMAX_STORE_BUF      16

#define MAX_NUM_WINS 32
#define MIN_NUM_WINS 4
#define MAX_NUM_TRAPS 16


#define NO_OF_LOG_INT_REGS (MAX_NUM_WINS*16 + MAX_NUM_TRAPS*4 + END_OF_REGISTERS - 20)
#define NO_OF_LOG_FP_REGS  32 
#define NO_OF_PHY_INT_REGS (NO_OF_LOG_INT_REGS + MAX_ACTIVE_NUMBER)
#define NO_OF_PHY_FP_REGS  (NO_OF_LOG_FP_REGS + MAX_ACTIVE_NUMBER)



/******************************************************************/
/****************** MembarInfo structure definition ***************/
/******************************************************************/
struct MembarInfo
{
  long long tag;        /* instruction tag                        */
  int SS;               /* store store membar?                    */
  int LS;               /* load store membar?                     */
  int SL;               /* store load membar?                     */
  int LL;               /* load load membar?                      */
  int MEMISSUE;         /* blocks all memory issue                */
  int SYNC;             /* blocks instruction initiation (decode) */
  
  int operator == (struct MembarInfo x)
  {
    return tag == x.tag;
  } 

  int operator <= (struct MembarInfo x)
  {
    return tag <= x.tag;
  }
  
  int operator >= (struct MembarInfo x)
  {
    return tag >= x.tag;
  }
  
  int operator < (struct MembarInfo x)
  {
    return tag < x.tag;
  }
  
  int operator > (struct MembarInfo x)
  {
    return tag > x.tag;
  }
  
  int operator != (struct MembarInfo x)
  {
    return tag != x.tag;
  }
};


/******************************************************************/
/****************** Some useful structure definition **************/
/******************************************************************/

/*
 * tagged_inst structure definition.
 * Used for detecting changes to instance tag 
 */
struct tagged_inst
{
  instance *inst;
  long long inst_tag;

  tagged_inst() {}

  tagged_inst(instance * i):inst(i), inst_tag(i->tag) {}

  int ok() const 
  {
    return inst_tag == inst->tag;
  }
};


/* 
 * Statistics: Efficiency characterization. For more details, refer to
 *  BennetFlynn1995  TR 
 */
enum eff_loss_stall
{
  eNOEFF_LOSS,      /* no efficiency loss                      */
  eBR,              /* branch-related efficiency losses        */
  eBADBR,           /* branch-related efficiency losses        */
  eSHADOW,          /* loss due to shadow mappers full         */
  eRENAME,          /* loss due to inadequate rename           */
  eMEMQFULL,        /* loss due to memory queue full           */
  eISSUEQFULL,      /* loss due to issue queue full            */
  eNUM_EFF_STALLS   /* number of classes of efficiency loss    */
};


/********************************************************************/
/******************* ProcState class  definition ********************/
/********************************************************************/

struct ProcState
{
  /* The ProcState class represents the state of an individual processor, 
     and is separate for the different processors in the MP case   */

  static int  numprocs;            /* number of processors                 */

  
  /****************** Configuraton parameter ********************/

  int         proc_id;             /* processor id                         */
  int         fetch_rate;          /* instruction fetches per cycle        */
  int         decode_rate;         /* decode rate of proc                  */
  int         graduate_rate;       /* graduate rate of proc                */
  int         max_active_list;     /* instruction window size              */
  FILE       *corefile;            /* corefile file pointer                */

  
  /****************** ???????????  ********************/
  
  long long   instruction_count;   /* total number of instructions         */
  long long   graduation_count;    /* number of graduated instrns          */
  long long   curr_cycle;          /* current simulated cycle              */

  page_table *PageTable;


  /******** instruction fetching, decoding, and graduation *******/

  int         halt;                /* halt processor until next interrupt  */
  
  unsigned    pc;                  /* the current pc                       */
  unsigned    npc;                 /* the next pc to fetch from            */

  unsigned    pstate;              /* copies of supervisor state registers */
  int         tl;                  /* for faster access                    */
  int         pil;
  int         itlb_random;
  int         itlb_wired;
  int         dtlb_random;
  int         dtlb_wired;

  int         cwp;                 /* current window pointer               */
  int         cansave;             /* # of reg. wins. that can be saved    */
  int         canrestore;          /* # of reg. wins. that can be restored */
  int         otherwin;            /* reserved windows                     */
  int         cleanwin;            /* unused windows                       */
  long long   tick_base;           /* cycle counter register               */
  
  unsigned    fp754_trap_mask;     /* IEEE FP trap mask                    */
  unsigned    fp754_aexc;          /* IEEE FP acrued traps                 */
  unsigned    fp754_cexc;          /* IEEE FP current traps                */
  unsigned    fp_trap_type;        /* general FP trap type                 */
  
  int         exit;                /* set by exit trap handler             */

  int         interrupt_pending;   /* indicates external interrupt         */

  class freelist   *free_fp_list;  /* Free register lists for FP           */
  class freelist   *free_int_list; /* Free register lists for integer      */
  class activelist  active_list;   /* Active_list                          */
  circq<TagtoInst*> tag_cvt;       /* tagcvt queue definition              */

  unsigned short *fpmapper;        /* fp logical-to-physical mapper        */
  unsigned short *intmapper;       /* int logical-to-physical mapper       */
  MapTable *activemaptable;        /* current mappers, to distinguish from 
			              shadow mappers */
  char     *fpregbusy;             /* busy table for fp registers          */
  char     *intregbusy;            /* busy table for int registers         */

  int                       fetch_queue_size;
  Queue<fetch_queue_entry> *fetch_queue;  
  unsigned                  fetch_pc;
  int                       fetch_done;
  instance                 *inst_save;  

  
  int            DELAY;            /* Is the processor stalling            */
  long long      stall_the_rest;   /* flag indicating processor stall      */
  int            stalledeff;           /* efficiency loss due to stall     */
  eff_loss_stall type_of_stall_rest;   /* classification of stall          */

  int     log_int_reg_file[NO_OF_LOG_INT_REGS];
  double  log_fp_reg_file[NO_OF_LOG_FP_REGS];
  int     phy_int_reg_file[NO_OF_LOG_INT_REGS + MAX_MAX_ACTIVE_NUMBER];
  double  phy_fp_reg_file[NO_OF_LOG_FP_REGS + MAX_MAX_ACTIVE_NUMBER];

  short  *log_int_reg_map;
  short  *log_state_reg_map;

  circq<tagged_inst> ReadyQueues[numUTYPES];   /* units are ready to issue */
  int UnitsFree[numUTYPES];        /* number of units free of each type    */
  int MaxUnits[numUTYPES];         /* maximum number of FU's per type      */
  int active_instr[numUTYPES];     /* # of unissued/active instructions    */
  int max_active_instr[numUTYPES]; /* maximum # of active instructions     */

  
  /******** instruction issue, execution, and completion *******/
   
  Heap<UTYPE> FreeingUnits;        /* units get freed                      */
  InstHeap    Running;             /* when instructions complete           */
  InstHeap    DoneHeap;            /* instructions that are done           */
  InstHeap    MemDoneHeap;         /* memory instructions that are done    */
  MiniStallQ  UnitQ[numUTYPES];    /* instructions stalled at each unit    */

  MiniStallQ  dist_stallq_int[NO_OF_LOG_INT_REGS + MAX_MAX_ACTIVE_NUMBER];
  MiniStallQ  dist_stallq_fp[NO_OF_LOG_FP_REGS + MAX_MAX_ACTIVE_NUMBER];

  instance *in_exception;          /* instance causing exception           */
  long long time_pre_exception;    /* time before exception                */

  
  /********************* Prediction **************************/

  MemQ<class BranchQElement*> branchq;   /* Branch Queue class definition  */
  int unpredbranch;                      /* unpredicted branch outstanding */
  MiniStallQ BranchDepQ;                 /* stalled branch instructions    */

  int      *BranchPred;                  /* 1st bit of 2-bit BHT           */
  int      *PrevPred;                    /* 2nd bit of 2-bit BHT           */
  unsigned *ReturnAddressStack;          /* return address predictor       */
  int       rasptr;                      /* return address stack pointer   */
  int       rascnt;                      /* return address stack counter   */
  int       copymappernext;              /* copy shadow mapper on next
					    instruction(delay slot)        */

  /*********************  Prefetch  ***************************/

  int        prefs;
  int        max_prefs;
  instance **prefrdy;                    /* prefetch slots                 */

  
  /*********************  Allocator  ***************************/

  Allocator<instance>          instances;    /* pool of instances          */
  DynAllocator<instance>       meminstances; /* pool of memop instances    */
  Allocator<BranchQElement>    bqes;         /* pool of branch queue elems */
  Allocator<MapTable>          mappers;      /* pool of shadow mappers     */
  Allocator<MiniStallQElt>     ministallqs;  /* pool of mini stall queues  */
  Allocator<activelistelement> actives;      /* pool of active list elems  */
  Allocator<TagtoInst>         tagcvts;      /* pool of tagcvt elements    */

  
  /************  Memory system related variables  **************/

  CACHE   *l1i_argptr;
  CACHE   *l1d_argptr;
  WBUFFER *wb_argptr;
  CACHE   *l2_argptr;

  TLB *itlb;
  TLB *dtlb;

#ifndef STORE_ORDERING
  MemQ<instance *> LoadQueue;            /* load queue                     */
  MemQ<instance *> StoreQueue;           /* store queue                    */
  int StoresToMem;                       /* keep track of outstanding sts  */
  MemQ<long long>  st_tags;              /* list of store tags             */
  MemQ<long long>  rmw_tags;             /* list of rmw tags               */
  MemQ<MembarInfo> membar_tags;          /* list of membar tags            */
  long long SStag, LStag, SLtag, LLtag;  /* various memory barrier tags    */
  long long MEMISSUEtag;                 /* memory issue/instruction sync. */
  long long minload, minstore;           /* last load and store instr. tags*/
#else
  MemQ<instance *> MemQueue;             /* unified memory queue           */
#endif

  int ReadyUnissuedStores;               /* ready but unissued stores      */
  MemQ<long long> ambig_st_tags;         /* ambiguous store tags           */
  int sync;


  int ktext_segment_low;
  int ktext_segment_high;
  int kdata_segment_low;
  int kdata_segment_high;

  /************************* Statistics ***************************/

  long long start_time;                  /* start time for stats collection*/
  long long start_icount;                /* start instruction count        */
  long long last_graduated;              /* last graduated instruction     */
  long long last_counted;                /* last instruction               */
  long long graduates;                   /* number of graduates            */

  time_t sim_start_time;

  long long exceptions[MAX_EXCEPT];      /* number of except. of each class*/
  long long graduated[MAX_EXCEPT];       /* # of graduates/exception class */
  long long cycles[MAX_EXCEPT];          /* # of cycles per exception class*/
  long long start_cycle[MAX_EXCEPT];     /* entry cycle count of exception */
                                         /* handler                        */
  long long start_graduated[MAX_EXCEPT]; /* entry instruction count        */

  long long start_halted;                /* start of last halt period      */
  long long total_halted;                /* total time processor is halted */
  long long mem_refs;                    /* number of memory references    */

  long long bpb_good_predicts;           /* number of correct predictions  */
  long long ras_good_predicts;           /* number of correct returns      */
  long long bpb_bad_predicts;            /* number of wrong predictions    */
  long long ras_bad_predicts;            /* number of bad returns          */
  long long ras_overflows;               /* number of RAS overflows        */
  long long ras_underflows;              /* number of RAS underflows       */

  STATREC *BadPredFlushes;               /* impact of mispredictions       */
  STATREC *ExceptFlushed;                /* impact of exceptions           */
  STATREC *SpecStats;                    /* time at each spec level        */
  STATREC *FetchQueueStats;              /* size of active list            */
  STATREC *ActiveListStats;              /* size of active list            */
  STATREC *FUUsage[numUTYPES];           /* utilization of functional units*/

#ifndef STORE_ORDERING
  STATREC *VSB;                          /* avg. virtual store buffer size */
  STATREC *LoadQueueSize;                /* load queue size                */
#else
  STATREC *MemQueueSize;                 /* memory queue size              */
#endif

  STATREC *lat_contrs[lNUM_LAT_TYPES];   /* execution time components      */
  STATREC *partial_otime;                /* partial overlap times          */
  int      stats_phase;                  /* stats collection phase         */
  STATREC *in_except;                    /* time spent waiting to trap     */

  /* classify read, write, and rmw times based on different metrics */
  STATREC *readacc, *writeacc, *rmwacc;
  STATREC *readiss, *writeiss, *rmwiss;
  STATREC *readact, *writeact, *rmwact;
#if 0
  STATREC *demand_read[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_read_iss[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write_iss[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw_iss[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_read_act[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_write_act[reqNUM_REQ_STAT_TYPE];
  STATREC *demand_rmw_act[reqNUM_REQ_STAT_TYPE];
  STATREC *pref_sh[reqNUM_REQ_STAT_TYPE];
  STATREC *pref_excl[reqNUM_REQ_STAT_TYPE];
#endif

  /* classification of loads */
  long long ldissues, ldspecs, limbos, unlimbos, redos, kills;
  /* forwarding stats */
  long long vsbfwds, fwds, partial_overlaps;
  /* availability, efficiency and utility (BennetFlynn1995 TR)  */
  long long avail_fetch_slots; 
  /* number of available slots lost to each cause */
  long long avail_active_full_losses[lNUM_LAT_TYPES];        
  /* efficiency losses from each cause */
  long long eff_losses[eNUM_EFF_STALLS];     

  /************************ Functions ****************************/

  ProcState(int);
  ~ProcState()
  {
    if (corefile) fclose(corefile);
  }

  int  reset_lists();

  inline void ComputeAvail();

  /* branch prediction functions */
  inline void     BPBSetup();                 // set up BP table
  inline int      BPBPredict(unsigned bpc, int statpred);
  // returns predicted pc
  
  inline void     BPBComplete(unsigned bpc, int taken, int statpred);  

  inline void     RASSetup   ();               // set up RAS predictor
  inline void     RASInsert  (unsigned newpc); // insert on a CALL
  inline unsigned RASPredict ();
  // remove on a RETURN, this is a destructive prediction


  /* Statistics */
  void report_stats      (int);
  void reset_stats       ();
  void report_phase      ();
  void report_phase_fast (int);
  void report_phase_in   (char *);
  void report_partial    ();
  void endphase          (int);
  void newphase          (int);
};


/* Other useful function definitions (see .c files for descriptions) */


extern int  FlushActiveList     (long long tag, ProcState * proc);
extern void init_decode         (ProcState *);
extern int  ExceptionHandler    (long long, ProcState *);
extern int  PreExceptionHandler (instance *, ProcState *);
extern int  startup             (char *, char **, char **, ProcState *);

extern "C"  void RSIM_EVENT();

#define unstall_the_rest(proc) { \
   if (proc->stall_the_rest) { \
      proc->eff_losses[proc->type_of_stall_rest] += proc->stalledeff;  \
      proc->stall_the_rest = 0; \
      proc->type_of_stall_rest = eNOEFF_LOSS;  \
      proc->stalledeff = 0; \
   } \
}


inline short    arch_to_log(ProcState *proc, int cwp, int iarch);
inline void     setup_arch_to_log_table(ProcState *proc);
inline unsigned DOWN_TO_PAGE(unsigned i);
inline unsigned UP_TO_PAGE(unsigned i);
inline char*    GetMap(instance * inst, ProcState * proc);

#endif

#endif
