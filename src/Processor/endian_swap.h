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

inline char endian_swap(char arg) {
  return arg;
}

inline unsigned char endian_swap(unsigned char arg) {
  return arg;
}

inline unsigned short endian_swap(unsigned short arg) {
#ifdef ENDIAN_SWAP
  return (arg << 8) & 0xff00 |
         (arg >> 8) & 0x00ff;
#else
  return arg;
#endif
}

inline short endian_swap(short arg) {
  return (short) endian_swap((unsigned short) arg);
}

inline unsigned long endian_swap(unsigned long arg) {
#ifdef ENDIAN_SWAP
  return (arg << 24) & 0xff000000 |
         (arg <<  8) & 0x00ff0000 |
         (arg >>  8) & 0x0000ff00 |
         (arg >> 24) & 0x000000ff;
#else
  return arg;
#endif
}

inline int endian_swap(int arg) {
  return (int) endian_swap((unsigned long) arg);
}

inline unsigned int endian_swap(unsigned int arg) {
  return (unsigned int) endian_swap((unsigned long) arg);
}

inline void* endian_swap(void* arg) {
  return (void*) endian_swap((unsigned long) arg);
}

inline int* endian_swap(int* arg) {
  return (int*) endian_swap((unsigned long) arg);
}

inline unsigned int* endian_swap(unsigned int* arg) {
  return (unsigned int*) endian_swap((unsigned long) arg);
}

inline char* endian_swap(char* arg) {
  return (char*) endian_swap((unsigned long) arg);
}

inline long endian_swap(long arg) {
  return (long) endian_swap((unsigned long) arg);
}

inline float endian_swap(float arg) {
  unsigned long tmp;
  tmp = * (unsigned long *) &arg;
  tmp = endian_swap(tmp);
  return *(float *) &tmp;
}

inline unsigned long long endian_swap(unsigned long long arg) {
#ifdef ENDIAN_SWAP
  return (arg << 56) & 0xff00000000000000ull |
         (arg << 40) & 0x00ff000000000000ull |
         (arg << 24) & 0x0000ff0000000000ull |
         (arg <<  8) & 0x000000ff00000000ull |
         (arg >>  8) & 0x00000000ff000000ull |
         (arg >> 24) & 0x0000000000ff0000ull |
         (arg >> 40) & 0x000000000000ff00ull |
         (arg >> 56) & 0x00000000000000ffull;
#else
  return arg;
#endif
}

inline long long endian_swap(long long arg) {
  return (long long) endian_swap((unsigned long long) arg);
}

inline double endian_swap(double arg) {
  unsigned long long tmp;
  tmp = * (unsigned long long *) &arg;
  tmp = endian_swap(tmp);
  return *(double *) &tmp;
}



