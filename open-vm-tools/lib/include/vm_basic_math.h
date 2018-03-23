/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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

/*
 * vm_basic_math.h --
 *
 *	Standard mathematical macros for VMware source code.
 */

#ifndef _VM_BASIC_MATH_H_
#define _VM_BASIC_MATH_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"
#include "vm_basic_types.h" // For INLINE.
#include "vm_basic_asm.h"   // For Div64...

#if defined __cplusplus
extern "C" {
#endif


static INLINE uint32
RatioOf(uint32 numer1, uint32 numer2, uint32 denom)
{
   uint64 numer = (uint64)numer1 * numer2;
   /* Calculate "(numer1 * numer2) / denom" avoiding round-off errors. */
#if defined(VMM) || !(defined(__i386__) || defined(__x86_64__))
   return numer / denom;
#else
   uint32 ratio;
   uint32 unused;
   Div643232(numer, denom, &ratio, &unused);
   return ratio;
#endif
}

static INLINE uint32
ExponentialAvg(uint32 avg, uint32 value, uint32 gainNumer, uint32 gainDenom)
{
   uint32 term1 = gainNumer * avg;
   uint32 term2 = (gainDenom - gainNumer) * value;
   return (term1 + term2) / gainDenom;
}


/*
 *-----------------------------------------------------------------------------
 *
 * IsZeroOrPowerOfTwo --
 * IsZeroOrPowerOfTwo64 --
 *
 * Results:
 *      TRUE iff the value is 0 or a power of two.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
IsZeroOrPowerOfTwo64(uint64 x)
{
   return !(x & (x - 1));
}


static INLINE Bool
IsZeroOrPowerOfTwo(uint32 x)     // IN
{
   return !(x & (x - 1));
}


static INLINE uint32
GetPowerOfTwo(uint32 x)
{
   /* Returns next-greatest power-of-two. */
   uint32 power2 = 1;
   while (x > power2) {
      power2 = power2 << 1;
   }
   return power2;
}


#if !defined(_WIN32) && !defined(_WIN64)
/*
 *-----------------------------------------------------------------------------
 *
 * RotateLeft32 --
 *
 * Results:
 *      Value rotated to the left by 'shift' bits.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint32
RotateLeft32(uint32 value, uint8 shift)
{
   return ((value << shift) | (value >> (32 - shift)));
}


/*
 *-----------------------------------------------------------------------------
 *
 * RotateRight32 --
 *
 * Results:
 *      Value rotated to the right by 'shift' bits.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint32
RotateRight32(uint32 value, uint8 shift)
{
   return ((value >> shift) | (value << (32 - shift)));
}


/*
 *-----------------------------------------------------------------------------
 *
 * RotateLeft64 --
 *
 * Results:
 *      Value rotated to the left by 'shift' bits.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint64
RotateLeft64(uint64 value, uint8 shift)
{
   return ((value << shift) | (value >> (64 - shift)));
}


/*
 *-----------------------------------------------------------------------------
 *
 * RotateRight64 --
 *
 * Results:
 *      Value rotated to the right by 'shift' bits.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint64
RotateRight64(uint64 value, uint8 shift)
{
   return ((value >> shift) | (value << (64 - shift)));
}
#endif // if !defined(_WIN32) && !defined(_WIN64)


#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _VM_BASIC_MATH_H_
