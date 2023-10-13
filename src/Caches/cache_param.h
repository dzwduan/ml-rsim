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

#ifndef __RSIM_CACHE_PARAM_H__
#define __RSIM_CACHE_PARAM_H__

/*************************************************************************/ 
/************ Default setting for cache module ***************************/
/*************************************************************************/

#define L1I_DEFAULT_SIZE	32
#define L1I_DEFAULT_SET_SIZE	 1
#define L1I_DEFAULT_LINE_SIZE	32
#define L1I_DEFAULT_NUM_PORTS    1

#define L1D_DEFAULT_SIZE	32
#define L1D_DEFAULT_SET_SIZE 	 1
#define L1D_DEFAULT_LINE_SIZE	32
#define L1D_DEFAULT_NUM_PORTS    1 

#define L2_DEFAULT_SIZE	       256
#define L2_DEFAULT_SET_SIZE	 4
#define L2_DEFAULT_LINE_SIZE   128
#define L2_DEFAULT_NUM_PORTS     1

/*
 * Note: for other default settings, please check cache_init.c.
 */

#define MIN_CACHE_LINESZ       	16   /* minimum acceptable line size */
#define L1CACHE 	       	1    /* First level caches */
#define L2CACHE 	       	2    /* Second-level cache */
#define FULL_ASS   	       	-1   /* full associativity */
#define LRU 		       	1    /* LRU replacement policy */
#define MAX_MAX_COALS		32   /* maximum # reqs coalesced in an MSHR */
#define NO_REPLACE 	       	2    /* Couldn't find a victim */
#define INFINITE               	-1   /* infinite cache */



/*************************************************************************/ 
/*********************** Parameter structure *****************************/
/*************************************************************************/

/* comments? later */

typedef struct {
   /* 
    * general switches
    */
   unsigned frequency;
   unsigned collect_stats;
   unsigned L1I_prefetch;
   unsigned L1D_prefetch;
   unsigned L1I_perfect;
   unsigned L1D_perfect;
   unsigned L2_prefetch;
   unsigned L2_perfect;

   /* 
    * L1 I-cache 
    */
   unsigned L1I_size;
   unsigned L1I_set_size;
   unsigned L1I_line_size;
   unsigned L1I_mshr_num;
   unsigned L1I_tag_delay;
   unsigned L1I_tag_repeat;
   unsigned L1I_tag_pipes;
   unsigned L1I_port_num;
   unsigned L1I_rep_queue;
   unsigned L1I_req_queue;
   unsigned L1I_cohe_queue;

   /* 
    * L1 D-cache 
    */
   int L1D_writeback;
   unsigned L1D_size;
   unsigned L1D_set_size;
   unsigned L1D_line_size;
   unsigned L1D_wb_size;
   unsigned L1D_mshr_num;
   unsigned L1D_tag_delay;
   unsigned L1D_tag_repeat;
   unsigned L1D_tag_pipes;
   unsigned L1D_port_num;
   unsigned L1D_rep_queue;
   unsigned L1D_req_queue;
   unsigned L1D_cohe_queue;

   /* 
    * L2 cache 
    */
   unsigned L2_size;
   unsigned L2_line_size;
   unsigned L2_set_size;
   unsigned L2_mshr_num;
   unsigned L2_tag_delay;
   unsigned L2_tag_repeat;
   unsigned L2_tag_pipes;
   unsigned L2_data_delay;
   unsigned L2_data_repeat;
   unsigned L2_data_pipes;
   unsigned L2_port_num;
   unsigned L2_rep_queue;
   unsigned L2_req_queue;
   unsigned L2_cohe_queue;

#if 0
   unsigned L1_line_mask;
   unsigned L1_line_shift;
   unsigned L1_vindx_mask;          /* L1C is virtually-indexed */
   unsigned L1_read_allocate;
   unsigned L1_write_allocate;
   unsigned L2_assocshift;
   unsigned L2_line_mask;
   unsigned L2_line_shift;
   unsigned L2_pindxmask;           /* L2C is physically-indexed */
   unsigned L2_wb_size;
   unsigned L2_wb_thresh;
   unsigned L2_to_L1_factor;
#endif
} cache_param_t;

extern cache_param_t cparam;

#endif
 
