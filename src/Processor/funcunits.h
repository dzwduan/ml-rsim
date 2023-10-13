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

#ifndef __RSIM_FUNCUNIT_H__
#define __RSIM_FUNCUNIT_H__

/* 
 * enumnerated type defining the type of functional units 
 */

typedef char UTYPE;

#define uALU      0
#define uFP       1
#define uADDR     2
#define uMEM      3      // this has to be the last real functional unit
#define numUTYPES 4



/* 
 * enumnerated type defining the type of memory latency 
 */
enum LATTYPE
{
  lALU  = 0,	  /* ALU operations    */
  lUSR1 = 1,	  /* for user types    */
  lUSR2 = 2,	  /* for user types    */
  lUSR3 = 3,	  /* for user types    */
  lUSR4 = 4,	  /* for user types    */
  lUSR5 = 5,	  /* for user types    */
  lUSR6 = 6,	  /* for user types    */
  lUSR7 = 7,	  /* for user types    */
  lUSR8 = 8,	  /* for user types    */
  lUSR9 = 9,	  /* for user types    */
  lBAR  = 10,	  /* barrier synch     */
  lSPIN = 11,	  /* spin synch        */
  lACQ  = 12, 	  /* acquire synch     */
  lREL  = 13,	  /* release synch     */
  lRMW, lWT, lRD, /* memory operations */
  lBRANCH,	  /* branch operations */
  lFPU,		  /* floating-point operations */
  lEXCEPT,	  /* exceptions        */
  lMEMBAR,	  /* memory barriers   */
  lBUSY,	  /* busy time         */

  /* memory miss classifications */
  lRDmiss, lWTmiss, lRMWmiss,           

  /* read classification: L1, L2, local and remote */
  lRD_L1, lRD_L2, lRD_LOC, lRD_REM,     

  /* write classification: L1, L2, local and remote */
  lWT_L1, lWT_L2, lWT_LOC, lWT_REM,     

  /* RMW   classification: L1, L2, local and remote */
  lRMW_L1, lRMW_L2, lRMW_LOC, lRMW_REM, 

  /* prefetch classifications */
  lRMW_PFlate, lWT_PFlate, lRD_PFlate,  

  /* number of latency types supported */
  lNUM_LAT_TYPES	                
};

extern const char *lattype_names[lNUM_LAT_TYPES]; /* names for the latency types */

extern int LAT_ALU_OTHER, LAT_ALU_MUL, LAT_ALU_DIV, LAT_ALU_SHIFT;
extern int REP_ALU_OTHER, REP_ALU_MUL, REP_ALU_DIV, REP_ALU_SHIFT;
extern int LAT_FPU_COMMON, LAT_FPU_MOV, LAT_FPU_CONV,LAT_FPU_DIV, LAT_FPU_SQRT;
extern int REP_FPU_COMMON, REP_FPU_MOV, REP_FPU_CONV,REP_FPU_DIV, REP_FPU_SQRT;

extern UTYPE   unit[];     /* one for each instruction */
extern LATTYPE lattype[];  /* one for each instruction */

extern void UnitArraySetup();
extern void FuncTableSetup();

#endif
