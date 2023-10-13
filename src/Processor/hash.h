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

#ifndef __RSIM_HASH_H__
#define __RSIM_HASH_H__


extern "C"
{
#include "sim_main/simsys.h"
}


/* uses templates for generic hashing the hash table setup provided is a
   double-hashing table The user has to pass in 2 hash functions. He is
   guaranteed that the size will be a power of 2. It is the users
   responsibility to make sure that the second one always returns an odd */

enum BucketState { BUCKET_EMPTY, BUCKET_IGNORE, BUCKET_FULL };

const unsigned minSize = 4;

template <class KeyType, class InfoType> class HashTable;


/**************************************************************************/
/***************** Bucket class definition ********************************/
/**************************************************************************/

template <class KeyType, class InfoType> class Bucket
{
  friend class HashTable<KeyType, InfoType>;

private:

  BucketState state;
  KeyType     key;
  InfoType    info;

  inline Bucket(): state(BUCKET_EMPTY), key(0), info(0)
  {}

  inline Bucket(KeyType k, InfoType i): state(BUCKET_FULL), key(k), info(i) 
  {}

  inline ~Bucket() 
  {}

  inline void Set(KeyType k, InfoType i) 
  { key = k; info = i; state = BUCKET_FULL; }

  inline BucketState State() const 
  { return state; }

  inline BucketState State(BucketState b) 
  { return state = b; }

  inline KeyType Key() const 
  { return key; }

  inline KeyType Key(KeyType k) 
  { return key = k; }

  inline InfoType Info() const 
  { return info; }

  inline InfoType Info(InfoType i) 
  { return info = i; }

  inline int matches(const KeyType& k) const 
  { return key == k; }
};


/**************************************************************************/
/************************* HashTable class definition *********************/
/**************************************************************************/

template <class KeyType, class InfoType> class HashTable
{
private:
  unsigned size;
  unsigned used;
  unsigned ignores;
  unsigned empty;

  unsigned shiftr;
  unsigned shiftl;
  unsigned szmask;
   
  unsigned iterator;

  Bucket<KeyType, InfoType> *buckets;
   
  void startout(unsigned sz);

  inline void kill() 
  {
    if (buckets != NULL)
      delete[] buckets;
    buckets = NULL;
  }

  inline void rehash(); 
  
  inline void pack() 
  {
    if (ignores > (size >> 2))
      rehash();
  }
  
  inline void loosen() 
  {
    if (empty < (size >> 2))
      rehash();
  }

  
public:

  HashTable(int shift)
  {
    shiftl = shift; 
    shiftr = (unsigned)sizeof(unsigned) * 8 - shiftl; 
    startout(minSize);
  }

  ~HashTable() 
  { kill(); }
   
  inline int insert(KeyType, InfoType);        // returns 0 on failure
  inline int lookup(KeyType, InfoType*) const; // puts value into i on success
  inline int assign(KeyType, InfoType);
  inline int remove(KeyType);

  inline void restart() 
  { startout(minSize); }

  inline int NumElts() const 
  { return used; }

  void seek();

  /* following member functions are used to traverse the hashtable */

  unsigned startIterator();
  InfoType operator *(); // a destructive dereference

  unsigned operator ++() 
  { seek(); return (iterator < size); }
};


#define HASH1(k, szmask) (k & szmask)
#define HASH2(k, szmask) (((((k << shiftl) | (k >> shiftr)) & szmask) << 1) + 1)

/**************************************************************************/
/************* HashTable member functions definitions *********************/
/**************************************************************************/

/***************************** startup ************************************/

template <class KeyType, class InfoType>
inline void HashTable<KeyType, InfoType>::startout(unsigned sz)
{
  sz = ToPowerOf2(sz); // to make sure it is a power of 2
  size = sz;
  szmask = size - 1;
  buckets = new Bucket<KeyType, InfoType>[sz];

  ignores = 0;
  empty = sz;
  used = 0;
}

/************************* Insertion ***********************************/
template <class KeyType, class InfoType> inline 
int HashTable<KeyType, InfoType>::insert(KeyType k, InfoType info)
{
  unsigned hashpos = HASH1(k, szmask);
  unsigned hashadv = HASH2(k, szmask);
  unsigned attempts = 0;

  while ((attempts < size) && (buckets[hashpos].State() == BUCKET_FULL))
    {
      hashpos += hashadv;
      hashpos &= size-1;
      attempts++;
    }

  if (attempts == size)
    {
      rehash();
      return insert(k,info);
    }

  if (buckets[hashpos].State() == BUCKET_EMPTY)
    {
      buckets[hashpos].Set(k, info);
      used++;
      empty--;
      loosen();
      return 1;
    }
   
  // otherwise we have hit an ignoreme
  unsigned firstignore = hashpos;

  while ((attempts < size) && (buckets[hashpos].State() != BUCKET_EMPTY))
    {
      hashpos += hashadv;
      hashpos &= size-1;
      attempts++;
    }
   
  if (attempts == size)
    {
      buckets[firstignore].Set(k, info);
      used++;
      ignores--;
      return 1;
    }
  else
    { // we have hit an empty
      buckets[hashpos].Set(k, info);
      used++;
      empty--;
      loosen();
      return 1;
    }
}


/************************ Lookup operation *******************************/

template <class KeyType, class InfoType> inline 
int HashTable<KeyType, InfoType>::lookup(KeyType k, InfoType *info) const
{
  unsigned hashpos = HASH1(k, szmask);
  unsigned hashadv = HASH2(k, szmask);
  unsigned attempts = 0;

  while (attempts < size)
    {
      switch (buckets[hashpos].State())
	{
	case BUCKET_EMPTY:
	  return 0;

	case BUCKET_FULL:
	  if (buckets[hashpos].matches(k))
	    {
	      *info = buckets[hashpos].Info();
	      return 1;
	    }
	  break;

	default: // ignore me
	  break;
	}
    
      hashpos += hashadv;
      hashpos &= size-1;
      attempts++;
    }
  return 0;
}


/************************** Assignment operation ***************************/

template <class KeyType, class InfoType>
inline int HashTable<KeyType, InfoType>::assign(KeyType k, InfoType info)
{
  unsigned hashpos = HASH1(k,szmask);
  unsigned hashadv = HASH2(k, szmask);
  unsigned attempts = 0;

  while (attempts < size)
    {
      switch (buckets[hashpos].State())
	{
	case BUCKET_EMPTY:
	  return 0;

	case BUCKET_FULL:	
	  if (buckets[hashpos].matches(k))
	    {
	      buckets[hashpos].Info(info);
	      return 1;
	    } 
	  break;
	  
	default: // ignore me
	  break;
	}
      
      hashpos += hashadv;
      hashpos &= size-1;
      attempts++;
    }
  return 0;
}


/*********************** Deletion operation *******************************/

template <class KeyType, class InfoType>
inline int HashTable<KeyType, InfoType>::remove(KeyType k)
{
  unsigned hashpos = HASH1(k,szmask);
  unsigned hashadv = HASH2(k,szmask);
  unsigned attempts = 0;
  
  while (attempts < size)
    {
      switch (buckets[hashpos].State())
	{
	case BUCKET_EMPTY:
	  return 0;
	  
	case BUCKET_FULL:
	  if (buckets[hashpos].matches(k))
	    {
	      buckets[hashpos].State(BUCKET_IGNORE);
	      ignores++;
	      used--;
	      pack(); // we don't want a table full of ignores
	      return 1;
	    } 
	  break;
	  
	default: // ignore me
	  break;
	}
    
      hashpos += hashadv;
      hashpos &= size-1;
      attempts++;
    }
  return 0;
}


/************************** Rehash operation ******************************/

template <class KeyType, class InfoType>
inline void HashTable<KeyType, InfoType>::rehash()
{
  Bucket<KeyType,InfoType> *old = new Bucket<KeyType,InfoType>[size];
  unsigned oldSize = size;
  unsigned i;

  for (i=0; i< oldSize; i++)
    old[i] = buckets[i];
  
  kill();
  unsigned nu = ToPowerOf2(2*used);
  unsigned ResultSize = (nu > minSize) ? nu : minSize;

  startout(ResultSize);
  
  for (i = 0; i< oldSize; i++)
    {
      if (old[i].State() == BUCKET_FULL)
	insert(old[i].Key(),old[i].Info());
    }
  
  delete[] old;
}



/****************** Miscellaneous HashTable operations *******************/

template <class KeyType, class InfoType>
inline void HashTable<KeyType, InfoType>::seek()
{
  iterator++;
  while (iterator < size)
    {
      if (buckets[iterator].State() == BUCKET_FULL)
	return;
      iterator++;
    }
}


template <class KeyType, class InfoType>
inline unsigned HashTable<KeyType, InfoType>::startIterator()
{
  pack();
  iterator = 0;
  if (buckets[0].State() != BUCKET_FULL)
    seek();

  return (iterator < size);
}


template <class KeyType, class InfoType>
inline InfoType HashTable<KeyType, InfoType>::operator *()
{
  buckets[iterator].State(BUCKET_IGNORE);
  ignores++;
  used--;
  return buckets[iterator].Info();
}

#endif
