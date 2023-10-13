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

#ifndef __RSIM_INSTRUCTION_H__
#define __RSIM_INSTRUCTION_H__

/**************************************************************************/
/**** All INSTRUCTIONS enumerated -- SPARC architecture-specific **********/
/**************************************************************************/


typedef unsigned char INSTRUCTION;

#define   iRESERVED        (INSTRUCTION)0
#define   iCALL            (INSTRUCTION)1
#define   iILLTRAP         (INSTRUCTION)2
#define   iBPcc            (INSTRUCTION)3
#define   iBicc            (INSTRUCTION)4
#define   iBPr             (INSTRUCTION)5
#define   iSETHI           (INSTRUCTION)6
#define   iFBPfcc          (INSTRUCTION)7
#define   iFBfcc           (INSTRUCTION)8

#define   iADD             (INSTRUCTION)9
#define   iAND            (INSTRUCTION)10
#define   iOR             (INSTRUCTION)11
#define   iXOR            (INSTRUCTION)12
#define   iSUB            (INSTRUCTION)13
#define   iANDN           (INSTRUCTION)14
#define   iORN            (INSTRUCTION)15
#define   iXNOR           (INSTRUCTION)16
#define   iADDC           (INSTRUCTION)17
#define   iMULX           (INSTRUCTION)18
#define   iUMUL           (INSTRUCTION)19
#define   iSMUL           (INSTRUCTION)20
#define   iSUBC           (INSTRUCTION)21
#define   iUDIVX          (INSTRUCTION)22
#define   iUDIV           (INSTRUCTION)23
#define   iSDIV           (INSTRUCTION)24
#define   iADDcc          (INSTRUCTION)25
#define   iANDcc          (INSTRUCTION)26
#define   iORcc           (INSTRUCTION)27
#define   iXORcc          (INSTRUCTION)28
#define   iSUBcc          (INSTRUCTION)29
#define   iANDNcc         (INSTRUCTION)30
#define   iORNcc          (INSTRUCTION)31
#define   iXNORcc         (INSTRUCTION)32
#define   iADDCcc         (INSTRUCTION)33
#define   iUMULcc         (INSTRUCTION)34
#define   iSMULcc         (INSTRUCTION)35
#define   iSUBCcc         (INSTRUCTION)36
#define   iUDIVcc         (INSTRUCTION)37
#define   iSDIVcc         (INSTRUCTION)38
#define   iTADDcc         (INSTRUCTION)39
#define   iTSUBcc         (INSTRUCTION)40
#define   iTADDccTV       (INSTRUCTION)41
#define   iTSUBccTV       (INSTRUCTION)42
#define   iMULScc         (INSTRUCTION)43
#define   iSLL            (INSTRUCTION)44
#define   iSRL            (INSTRUCTION)45
#define   iSRA            (INSTRUCTION)46
#define   iarithSPECIAL1  (INSTRUCTION)47   /* includes RDY, RDCCR, MEMBAR, etc */
#define   iRDPR           (INSTRUCTION)48
#define   iFLUSHW         (INSTRUCTION)49
#define   iMOVcc          (INSTRUCTION)50
#define   iSDIVX          (INSTRUCTION)51
#define   iPOPC           (INSTRUCTION)52
#define   iMOVR           (INSTRUCTION)53
#define   iarithSPECIAL2  (INSTRUCTION)54  /* WRY, etc */
#define   iSAVRESTD       (INSTRUCTION)55
#define   iWRPR           (INSTRUCTION)56
#define   iIMPDEP1        (INSTRUCTION)57
#define   iIMPDEP2        (INSTRUCTION)58
#define   iJMPL           (INSTRUCTION)59
#define   iRETURN         (INSTRUCTION)60
#define   iTcc            (INSTRUCTION)61
#define   iFLUSH          (INSTRUCTION)62
#define   iSAVE           (INSTRUCTION)63
#define   iRESTORE        (INSTRUCTION)64
#define   iDONERETRY      (INSTRUCTION)65

#define   iFMOVs          (INSTRUCTION)66
#define   iFMOVd          (INSTRUCTION)67
#define   iFMOVq          (INSTRUCTION)68
#define   iFNEGs          (INSTRUCTION)69
#define   iFNEGd          (INSTRUCTION)70
#define   iFNEGq          (INSTRUCTION)71
#define   iFABSs          (INSTRUCTION)72
#define   iFABSd          (INSTRUCTION)73 
#define   iFABSq          (INSTRUCTION)74
#define   iFSQRTs         (INSTRUCTION)75
#define   iFSQRTd         (INSTRUCTION)76
#define   iFSQRTq         (INSTRUCTION)77
#define   iFADDs          (INSTRUCTION)78
#define   iFADDd          (INSTRUCTION)79
#define   iFADDq          (INSTRUCTION)80
#define   iFSUBs          (INSTRUCTION)81 
#define   iFSUBd          (INSTRUCTION)82
#define   iFSUBq          (INSTRUCTION)83
#define   iFMULs          (INSTRUCTION)84
#define   iFMULd          (INSTRUCTION)85
#define   iFMULq          (INSTRUCTION)86
#define   iFDIVs          (INSTRUCTION)87
#define   iFDIVd          (INSTRUCTION)88
#define   iFDIVq          (INSTRUCTION)89 
#define   iFsMULd         (INSTRUCTION)90
#define   iFdMULq         (INSTRUCTION)91
#define   iFsTOx          (INSTRUCTION)92
#define   iFdTOx          (INSTRUCTION)93
#define   iFqTOx          (INSTRUCTION)94
#define   iFxTOs          (INSTRUCTION)95
#define   iFxTOd          (INSTRUCTION)96
#define   iFxToq          (INSTRUCTION)97 
#define   iFiTOs          (INSTRUCTION)98
#define   iFdTOs          (INSTRUCTION)99
#define   iFqTOs         (INSTRUCTION)100
#define   iFiTOd         (INSTRUCTION)101
#define   iFsTOd         (INSTRUCTION)102
#define   iFqTOd         (INSTRUCTION)103
#define   iFiTOq         (INSTRUCTION)104
#define   iFsTOq         (INSTRUCTION)105 
#define   iFdTOq         (INSTRUCTION)106
#define   iFsTOi         (INSTRUCTION)107
#define   iFdTOi         (INSTRUCTION)108
#define   iFqTOi         (INSTRUCTION)109
#define   iFMOVs0        (INSTRUCTION)110
#define   iFMOVd0        (INSTRUCTION)111
#define   iFMOVq0        (INSTRUCTION)112
#define   iFMOVs1        (INSTRUCTION)113 
#define   iFMOVd1        (INSTRUCTION)114
#define   iFMOVq1        (INSTRUCTION)115
#define   iFMOVs2        (INSTRUCTION)116
#define   iFMOVd2        (INSTRUCTION)117
#define   iFMOVq2        (INSTRUCTION)118
#define   iFMOVs3        (INSTRUCTION)119
#define   iFMOVd3        (INSTRUCTION)120
#define   iFMOVq3        (INSTRUCTION)121 
#define   iFMOVsi        (INSTRUCTION)122
#define   iFMOVdi        (INSTRUCTION)123
#define   iFMOVqi        (INSTRUCTION)124
#define   iFMOVsx        (INSTRUCTION)125
#define   iFMOVdx        (INSTRUCTION)126
#define   iFMOVqx        (INSTRUCTION)127
#define   iFCMPs         (INSTRUCTION)128
#define   iFCMPd         (INSTRUCTION)129
#define   iFCMPq         (INSTRUCTION)130
#define   iFCMPEs        (INSTRUCTION)131
#define   iFCMPEd        (INSTRUCTION)132
#define   iFCMPEq        (INSTRUCTION)133
#define   iFMOVRsZ       (INSTRUCTION)134
#define   iFMOVRdZ       (INSTRUCTION)135
#define   iFMOVRqZ       (INSTRUCTION)136
#define   iFMOVRsLEZ     (INSTRUCTION)137
#define   iFMOVRdLEZ     (INSTRUCTION)138
#define   iFMOVRqLEZ     (INSTRUCTION)139
#define   iFMOVRsLZ      (INSTRUCTION)140
#define   iFMOVRdLZ      (INSTRUCTION)141
#define   iFMOVRqLZ      (INSTRUCTION)142
#define   iFMOVRsNZ      (INSTRUCTION)143
#define   iFMOVRdNZ      (INSTRUCTION)144
#define   iFMOVRqNZ      (INSTRUCTION)145
#define   iFMOVRsGZ      (INSTRUCTION)146
#define   iFMOVRdGZ      (INSTRUCTION)147
#define   iFMOVRqGZ      (INSTRUCTION)148
#define   iFMOVRsGEZ     (INSTRUCTION)149
#define   iFMOVRdGEZ     (INSTRUCTION)150
#define   iFMOVRqGEZ     (INSTRUCTION)151

#define   iLDUW          (INSTRUCTION)152
#define   iLDUB          (INSTRUCTION)153
#define   iLDUH          (INSTRUCTION)154
#define   iLDD           (INSTRUCTION)155
#define   iSTW           (INSTRUCTION)156
#define   iSTB           (INSTRUCTION)157
#define   iSTH           (INSTRUCTION)158
#define   iSTD           (INSTRUCTION)159
#define   iLDSW          (INSTRUCTION)160
#define   iLDSB          (INSTRUCTION)161
#define   iLDSH          (INSTRUCTION)162
#define   iLDX           (INSTRUCTION)163
#define   iLDSTUB        (INSTRUCTION)164
#define   iSTX           (INSTRUCTION)165
#define   iSWAP          (INSTRUCTION)166
#define   iLDUWA         (INSTRUCTION)167
#define   iLDUBA         (INSTRUCTION)168
#define   iLDUHA         (INSTRUCTION)169
#define   iLDDA          (INSTRUCTION)170
#define   iSTWA          (INSTRUCTION)171
#define   iSTBA          (INSTRUCTION)172
#define   iSTHA          (INSTRUCTION)173
#define   iSTDA          (INSTRUCTION)174
#define   iLDSWA         (INSTRUCTION)175
#define   iLDSBA         (INSTRUCTION)176
#define   iLDSHA         (INSTRUCTION)177
#define   iLDXA          (INSTRUCTION)178
#define   iLDSTUBA       (INSTRUCTION)179
#define   iSTXA          (INSTRUCTION)180
#define   iSWAPA         (INSTRUCTION)181
#define   iLDF           (INSTRUCTION)182
#define   iLDFSR         (INSTRUCTION)183
#define   iLDXFSR        (INSTRUCTION)184
#define   iLDQF          (INSTRUCTION)185
#define   iLDDF          (INSTRUCTION)186
#define   iSTF           (INSTRUCTION)187
#define   iSTFSR         (INSTRUCTION)188
#define   iSTXFSR        (INSTRUCTION)189
#define   iSTQF          (INSTRUCTION)190
#define   iSTDF          (INSTRUCTION)191
#define   iPREFETCH      (INSTRUCTION)192
#define   iLDFA          (INSTRUCTION)193
#define   iLDQFA         (INSTRUCTION)194
#define   iLDDFA         (INSTRUCTION)195
#define   iSTFA          (INSTRUCTION)196
#define   iSTQFA         (INSTRUCTION)197
#define   iSTDFA         (INSTRUCTION)198

#define   iCASA          (INSTRUCTION)199
#define   iPREFETCHA     (INSTRUCTION)200
#define   iCASXA         (INSTRUCTION)201
#define   iRWSD          (INSTRUCTION)202
#define   iRWWT_I        (INSTRUCTION)203
#define   iRWWTI_I       (INSTRUCTION)204
#define   iRWWS_I        (INSTRUCTION)205
#define   iRWWSI_I       (INSTRUCTION)206       /* various WriteThrough RW */
#define   iRWWT_F        (INSTRUCTION)207
#define   iRWWTI_F       (INSTRUCTION)208
#define   iRWWS_F        (INSTRUCTION)209
#define   iRWWSI_F       (INSTRUCTION)210       /* various WriteSend    RW */

#define   numINSTRS      (INSTRUCTION)211
/* NOTE: THIS MUST BE THE LAST ENTRY HERE --
   PUT ALL NEW INSTRUCTIONS BEFORE THIS ONE!!! */



/* instruction names array */
extern const char *inames[numINSTRS];  



/* functions to simulate the instructions */
struct ProcState;
class instance;
typedef void (*EFP)(instance *, ProcState *);
extern EFP instr_func[numINSTRS];  



/**************************************************************************/
/************* INSTRUCTIONS types -- SPARC architecture-specific **********/
/**************************************************************************/

/* 
 * First are bits 31 and 30 
 */
enum op
{
  BRANCH = 0,
  CALL   = 1,
  ARITH  = 2,
  MEM    = 3
};



/* 
 * Now, within BRANCH, let's look at field op2, bits 24-22. We'll keep
 * cond/rcond as a field in our instruction structure and do a case 
 * statement on that. 
 */
enum br_op2
{
  ILLTRAP = 0,
  BPcc    = 1,
  Bicc    = 2,
  BPr     = 3,
  SETHI   = 4, 
  FBPfcc  = 5,
  FBfcc   = 6,
  brRES   = 7
};



/* 
 * within ARITH, look at op3, which is bits 24-19 
 */
enum arith_ops
{
  ADD,
  AND,
  OR,
  XOR,
  SUB,
  ANDN,
  ORN,
  XNOR,
  ADDC, 
  MULX,
  UMUL,
  SMUL,
  SUBC,
  UDIVX,
  UDIV,
  SDIV, 
  ADDcc,
  ANDcc,
  ORcc,
  XORcc,
  SUBcc,
  ANDNcc,
  ORNcc,
  XNORcc,
  ADDCcc, 
  arithRES1,
  UMULcc,
  SMULcc,
  SUBCcc,
  arithRES2,
  UDIVcc,
  SDIVcc, 
  TADDcc,
  TSUBcc,
  TADDccTV,
  TSUBccTV,
  MULScc,
  SLL,
  SRL,
  SRA, 
  arithSPECIAL1 /* includes RDY, RDCCR, etc */, 
  arithRES3,
  RDPR,
  FLUSHW,
  MOVcc,
  SDIVX,
  POPC,
  MOVR, 
  arithSPECIAL2 /* includes WRY, etc */, 
  SAVRESTD,
  WRPR,
  arithRES4,
  FPop1,
  FPop2,
  IMPDEP1,
  IMPDEP2, 
  JMPL,
  RETURN,
  Tcc,
  FLUSH,
  SAVE,
  RESTORE,
  DONERETRY, 
  arithRES5 
};



/* 
 * Within FPop1, look at opf, bits 13-5 
 * fill in the table with reserveds, then fill in these spaces separately 
 */
enum FPop1_ops
{
  FMOVs = 0x01,
  FMOVd,
  FMOVq, 
  FNEGs = 0x05,
  FNEGd,
  FNEGq, 
  FABSs = 0x09,
  FABSd,
  FABSq, 
  FSQRTs= 0x29,
  FSQRTd,
  FSQRTq, 
  FADDs = 0x41,
  FADDd,
  FADDq, 
  FSUBs = 0x45,
  FSUBd,
  FSUBq, 
  FMULs = 0x49,
  FMULd,
  FMULq, 
  FDIVs = 0x4d,
  FDIVd,
  FDIVq, 
  FsMULd= 0x69, 
  FdMULq= 0x6e, 
  FsTOx = 0x81,
  FdTOx,
  FqTOx,
  FxTOs, 
  FxTOd = 0x88, 
  FxToq = 0x8c, 
  FiTOs = 0xc4, 
  FdTOs = 0xc6,
  FqTOs,
  FiTOd,
  FsTOd, 
  FqTOd = 0xcb,
  FiTOq,
  FsTOq,
  FdTOq, 
  FsTOi = 0xd1,
  FdTOi,
  FqTOi
};



/* 
 * Within FPop2, look at opf, bits 13-5 
 */
enum FPop2_ops
{
  FMOVs0 = 0x01,
  FMOVd0,
  FMOVq0, 
  FMOVs1 = 0x41,
  FMOVd1,
  FMOVq1, 
  FMOVs2 = 0x81,
  FMOVd2,
  FMOVq2, 
  FMOVs3 = 0xc1,
  FMOVd3,
  FMOVq3, 

  FMOVsi = 0x101,
  FMOVdi,
  FMOVqi, 
  FMOVsx = 0x181,
  FMOVdx,
  FMOVqx, 

  FCMPs  = 0x51,
  FCMPd,
  FCMPq, 
  FCMPEs = 0x55,
  FCMPEd,
  FCMPEq, 

  FMOVRsZ   = 0x25,
  FMOVRdZ,
  FMOVRqZ, 
  FMOVRsLEZ = 0x45,
  FMOVRdLEZ,
  FMOVRqLEZ, 
  FMOVRsLZ  = 0x65,
  FMOVRdLZ,
  FMOVRqLZ, 

  FMOVRsNZ  = 0xa5,
  FMOVRdNZ,
  FMOVRqNZ, 
  FMOVRsGZ  = 0xc5,
  FMOVRdGZ,
  FMOVRqGZ, 
  FMOVRsGEZ = 0xe5,
  FMOVRdGEZ,
  FMOVRqGEZ
};




/* within MEM, look at op3, which is bits 24-19 */
enum mem_ops
{
  LDUW,
  LDUB,
  LDUH,
  LDD,
  STW,
  STB,
  STH,
  STD, 
  LDSW,
  LDSB,
  LDSH,
  LDX,
  memRES1,
  LDSTUB,
  STX,
  SWAP, 
  LDUWA,
  LDUBA,
  LDUHA,
  LDDA,
  STWA,
  STBA,
  STHA,
  STDA, 
  LDSWA,
  LDSBA,
  LDSHA,
  LDXA,
  memRES2,
  LDSTUBA,
  STXA,
  SWAPA, 
  LDF,
  LDFSR,
  LDQF,
  LDDF,
  STF,
  STFSR,
  STQF,
  STDF, 
  memRES3,
  memRES4,
  memRES5,
  memRES6,
  memRES7, 
  PREFETCH,
  memRES8,
  memRES9, 
  LDFA,
  memRES10,
  LDQFA,
  LDDFA,
  STFA,
  memRES11,
  STQFA,
  STDFA, 
  memRES12,
  memRES13,
  memRES14,
  memRES15, 
  CASA,
  PREFETCHA,
  CASXA,
  memRES16 
};




/* 
 * cond field used in branches, bits 28-25 
 */
enum bp_conds
{
  Nb,
  Eb,
  LEb,
  Lb,
  LEUb,
  CSb,
  NEGb,
  VSb,
  Ab,
  NEb,
  Gb,
  GEb,
  GUb,
  CCb, 
  POSb,
  VCb
};



enum fp_conds
{
  Nf,
  NEf,
  LGf,
  ULf,
  Lf,
  UGf,
  Gf,
  Uf,
  Af,
  Ef,
  UEf,
  GEf,
  UGEf,
  LEf,
  ULEf,
  Of
};




/* 
 * the rcond field used in BPr, MOVr, and FMOVr 
 */
enum rcond
{
  rRES1,
  Zr,
  LEr,
  Lr,
  rRES2,
  Nr,
  Gr,
  GEr
};




/***************************************************************************/
/******************* Memory barrier related definitions ********************/
/***************************************************************************/

#define iMEMBAR 	iarithSPECIAL1
#define MB_StoreStore 	8
#define MB_LoadStore 	4
#define MB_StoreLoad 	2
#define MB_LoadLoad 	1

#define MB_LOOKASIDE    1
#define MB_MEMISSUE 	2
#define MB_SYNC         4

#define PREF_NRD 	0
#define PREF_1RD 	1
#define PREF_NWT 	2
#define PREF_1WT 	3




/***************************************************************************/
/*************** Window pointer change enumerated type defintion ***********/
/***************************************************************************/

#define WPC_RESTORE  -1
#define WPC_NONE      0
#define WPC_SAVE      1
#define WPC_FLUSHW  100

#if 0
enum WPC
{
  WPC_RESTORE = -1,          /* decrement the current window pointer (cwp) */
  WPC_NONE    = 0,           /* retain the same current window pointer     */
  WPC_SAVE    = 1,           /* increment the current window pointer       */
  WPC_FLUSHW  = 100          /* flush window - does not really change cwp  */
};
#endif


/***************************************************************************/
/********** Static instruction structure definition ************************/
/***************************************************************************/

struct instr
{
  INSTRUCTION instruction;

  /* WRITTEN REGISTERS */
  char rd;	              /* destination register                      */
  char rcc; 	              /* destination condition code register       */

  /* READ REGISTERS */
  char rs1;	              /* source register 1                         */
  char rs2;	              /* source register 2                         */
  char rscc;	              /* source condition code register            */

  /* AUXILIARY DATA */  
  char aux1;				
  char taken;	              /* taken hint for branches                   */
  char annul;	              /* annul bit for branches                    */

  /* BIT FIELDS */
  REGTYPE rd_regtype; 	      /* is rd a floating point?                   */
  REGTYPE rs1_regtype;	      /* is rs1 a floating point?                  */
  REGTYPE rs2_regtype;	      /* is rs2 a floating point?                  */
  char cond_branch; 	      /* indicate conditional branch               */
  char uncond_branch;         /* indicate non-conditional branch
		       	          0 for other, 
				  1 for address calc needed, 
				  2 for immediate address, 
				  3 for "call", 
				  4 for probable return (addr calc needed) */

  char wpchange; 	/* (was WPC) indicate instructions that change window
			   pointer +1 for save, -1 for restore */

  char sync;

  int  aux2;
  int  imm;		      /* immediate field                           */

  
  /* member functions */

  instr(): instruction(iRESERVED), aux1(0), aux2(0), imm(0)
  {
    rd = rcc = rs1 = rs2 = rscc = 0;
    rd_regtype = rs1_regtype = rs2_regtype = REG_INT;
    taken = annul = cond_branch = uncond_branch = 0;
    wpchange = WPC_NONE;
    sync = 0;
  }

  
  instr(INSTRUCTION i): instruction(i), aux1(0), aux2(0), imm(0)
  {
    rd = rcc = rs1 = rs2 = rscc = 0;
    rd_regtype = rs1_regtype = rs2_regtype = REG_INT;
    taken = annul = cond_branch = uncond_branch = 0;
    wpchange = WPC_NONE;
    sync = 0;
  }

  
  instr(INSTRUCTION in_instruction,
	char in_rd, char in_rcc, char in_rs1, char in_rs2, char in_rscc,
	char in_aux1, int in_aux2, int in_imm, 
	REGTYPE in_rd_regtype, REGTYPE in_rs1_regtype, REGTYPE in_rs2_regtype,
	char in_taken, char in_annul,
	char in_cond_branch, char in_uncond_branch, 
	char in_wpchange, char in_sync)
  {
    instruction = in_instruction;

    rd            = in_rd;
    rcc           = in_rcc;
    rs1           = in_rs1;
    rs2           = in_rs2;
    rscc          = in_rscc;
    aux1          = in_aux1;
    aux2          = in_aux2;
    imm           = in_imm;

    rd_regtype    = in_rd_regtype;
    rs1_regtype   = in_rs1_regtype;
    rs2_regtype   = in_rs2_regtype;
    taken         = in_taken;
    annul         = in_annul;
    cond_branch   = in_cond_branch;
    uncond_branch = in_uncond_branch;
    wpchange      = in_wpchange;
    sync          = in_sync;
  }

  
  /* writing instruction to a file */
  void output(FILE *out) 
  {
    fwrite((const char*)this, (size_t)sizeof(instr), 1, out);
  }

  
  /* reading instruction from a file */
  void input(FILE *in) 
  {
    fread((char *)this, (size_t)sizeof(instr), 1, in);
  }

  
  void copy(instr *i)
  {
    long long *src, *dest;
    int n;

    src  = (long long*)this;
    dest = (long long*)i;

    for (n = 0; n < sizeof(instr) / sizeof(long long); n++)
      dest[n] = src[n];
  }
  
  
  /* printing a file */
  void print()
  {
#if 0
#ifdef COREFILE
    extern FILE *corefile;
    if (GetSimTime() > DEBUG_TIME)
      fprintf(corefile, "%-12.11s%d\t%d\t%d\t%d\t%d\t%d\t%d\n", 
	      inames[instruction], rd, rcc, rs1, rs2, aux1, aux2, imm);
#endif
#endif
  }
};





#define Extract(v, hi, lo) ((v >> lo) & (((unsigned)-1) >> (31-hi+lo)))

typedef int  (*IMF)(instr *, unsigned); /* instruction manipulation function */
typedef void (*IEF)(instr *);           /* instruction execution function */

extern IMF toplevelop[4];        /* top level classification of instructions */
extern IMF brop2[8];             /* branch operations */
extern INSTRUCTION ibrop2[8];
extern IMF arithop[64];          /* arithmetic operations */
extern INSTRUCTION iarithop[64];
extern IMF fpop1[512];           /* floating point operations */
extern INSTRUCTION ifpop1[512];
extern IMF fpop2[512];           /* floating point operations -- (move, etc) */
extern INSTRUCTION ifpop2[512];
extern IMF memop3[64];           /* memory operations */
extern INSTRUCTION imemop3[64];

#endif
