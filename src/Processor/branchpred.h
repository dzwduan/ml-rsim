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

#ifndef __RSIM_BRANCHPRED_H__
#define __RSIM_BRANCHPRED_H__

/*************************************************************************/
/********************** MapTable class definition ************************/
/*************************************************************************/

/*
 * The implementation of the MapTable closely models the shadow mappers in
 * the MIPS R10000 processor implementation. For more details, please refer
 * to the MIPS R10000 user manual 
 */

struct MapTable
{
  unsigned short *imap;		       /* integer register mappings        */
  unsigned short *fmap;		       /* floating point register mappings */
  int inited;		               /* are the mappers initialized?     */

  MapTable()
  {
    if (!inited)
      {
	imap = (unsigned short*)malloc(NO_OF_LOG_INT_REGS * sizeof(unsigned short));
	fmap = (unsigned short*)malloc(NO_OF_LOG_FP_REGS * sizeof(unsigned short));
	inited = 1;
      }
  }
   
  ~MapTable() {}
};


/***************************************************************************/
/********************* BranchQElement class definition *********************/
/***************************************************************************/

struct BranchQElement
{
  long long tag;	               /* tag of branch/delay slot         */
  MapTable *specmap;	               /* shadow mappers with branch       */
  int       done;	               /* is branch done?                  */

  BranchQElement(long long tg, MapTable *map) 
  {
    tag = tg;
    specmap = map;
    done = 0;
  }

  ~BranchQElement() {}
};


/***************************************************************************/
/***************** Branch predictor configuration **************************/
/***************************************************************************/

/*
 * Branch predictor enumerated type definition
 */
enum bptype
{
  TWOBIT,                    /* Two-bit hardware bimodal prediction scheme */
  TWOBITAGREE,               /* Two-bit hardware agree prediction scheme   */
  STATIC                     /* static prediction scheme                   */
};


extern bptype BPB_TYPE;
extern int    BPB_SIZE;                    /* size of Branch Pred buffer   */
extern int    RAS_STKSZ;                   /* size of return address stack */

/***************************************************************************/
/********************* Branch function definitions *************************/
/***************************************************************************/


struct ProcState;
struct instance;
struct instr;


inline void StartCtlXfer              (instance *, ProcState *);
extern int  decode_branch_instruction (instr *, instance *, ProcState *);
extern int  AddBranchQ                (long long, ProcState *);
extern int  RemoveFromBranchQ         (long long, ProcState *);
extern void BadPrediction             (instance *, ProcState *);
extern void GoodPrediction            (instance *, ProcState *);
extern void HandleUnPredicted         (instance *, ProcState *);
extern int  CopyBranchQ               (long long, ProcState *);
extern void FlushBranchQ              (long long, ProcState *);
extern int  FlushStallQ               (long long, ProcState *);
extern void FlushFetchQ               (ProcState *);

#endif

