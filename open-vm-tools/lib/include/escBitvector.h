/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/



#ifndef _ESC_BITVECTOR_H_
#define _ESC_BITVECTOR_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE  // XXX is this true?
#include "includeCheck.h"


#ifdef __KERNEL__
#include "driver-config.h"
#include <linux/string.h>
/* Don't include these if compiling for the Solaris or Apple kernels. */
#elif !defined _KERNEL && !defined KERNEL
#include <stdlib.h>
#include <string.h>
#endif

#if defined _KERNEL && defined __FreeBSD__
# include <sys/libkern.h>
#elif defined KERNEL && defined __APPLE__
# include <string.h>
#endif

#include "vm_assert.h"



#define ESC_BITVECTOR_INDEX(_x)     ((_x)>>5)
#define ESC_BITVECTOR_MASK(_x)      (1<<((_x)&31))

#define ESC_BITVECTOR_SIZE 256 // hardwired size of the bitvector

/*
 *----------------------------------------------------------------------
 *
 * EscBitVector --
 *
 *      Taken from bitvector.h, but hard wired for use with the Escape
 *      routines, which always need a bitvector of 256 bits, are never
 *      used in the monitor, and need to work in the linux kernel. [bac]
 *
 *
 *----------------------------------------------------------------------
 */
typedef struct EscBitVector {
   uint32 vector[ESC_BITVECTOR_SIZE/32];
} EscBitVector;


/*
 *----------------------------------------------------------------------
 *
 * EscBitVector_Init --
 *
 *      Clear all the bits in this vector.
 *
 * Results:
 *      All bits are cleared
 *      
 *----------------------------------------------------------------------
 */
static INLINE void EscBitVector_Init(EscBitVector *bv)
{
   memset(bv, 0, sizeof(EscBitVector));
}

/*
 *----------------------------------------------------------------------
 *
 * EscBitVector_Set, EscBitVector_Clear, EscBitVector_Test --
 *
 *      basic operations
 *
 * Results:
 *      insertion/deletion/presence  to/from/in the set
 *      
 *      EscBitVector_Test returns non-zero if present, 0 otherwise
 *
 *
 *----------------------------------------------------------------------
 */
static INLINE void EscBitVector_Set(EscBitVector *bv,int n)
{
   ASSERT(n>=0 && n<ESC_BITVECTOR_SIZE);
#ifdef __GNUC__
   __asm__ __volatile ( "btsl %1,%0" : "=m" (bv->vector[0]) :"Ir" (n));
#else
   bv->vector[ESC_BITVECTOR_INDEX(n)] |= ESC_BITVECTOR_MASK(n);
#endif
}

static INLINE void EscBitVector_Clear(EscBitVector *bv,int n)
{
   ASSERT(n>=0 && n<ESC_BITVECTOR_SIZE);
#ifdef __GNUC__
   __asm__ __volatile ( "btrl %1,%0" : "=m" (bv->vector[0]) :"Ir" (n));
#else
   bv->vector[ESC_BITVECTOR_INDEX(n)] &= ~ESC_BITVECTOR_MASK(n);
#endif
}

static INLINE int EscBitVector_Test(EscBitVector const *bv, int n)
{
   ASSERT(n>=0 && n<ESC_BITVECTOR_SIZE);
#ifdef __GNUC__
   {
      uint32 tmp;
   __asm__ __volatile ( "btl %2,%1\n\tsbbl %0,%0" : "=r" (tmp) : "m" (bv->vector[0]),"Ir" (n));
      return tmp;
   }
#else
   return ((bv->vector[ESC_BITVECTOR_INDEX(n)] & ESC_BITVECTOR_MASK(n)) != 0);
#endif
}




#endif  /* _ESC_BITVECTOR_H_ */


