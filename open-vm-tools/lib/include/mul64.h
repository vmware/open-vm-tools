/*********************************************************
 * Copyright (C) 2003-2014,2017 VMware, Inc. All rights reserved.
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

/*
 * mul64.h
 *
 *      Integer by fixed point multiplication, with rounding.
 *
 *      These routines are implemented in assembly language for most
 *      supported platforms.  This file has plain C fallback versions.
 */

#ifndef _MUL64_H_
#define _MUL64_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_asm.h"

#if defined __cplusplus
extern "C" {
#endif


#ifdef MUL64_NO_ASM
/*
 *-----------------------------------------------------------------------------
 *
 * Mul64x3264 --
 *
 *    Unsigned integer by fixed point multiplication, with rounding:
 *       result = floor(multiplicand * multiplier * 2**(-shift) + 0.5)
 * 
 *       Unsigned 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         (multiplier, shift), where shift < 64.
 *
 * Result:
 *       Unsigned 64-bit integer product.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint64
Mul64x3264(uint64 multiplicand, uint32 multiplier, uint32 shift)
{
   uint64 lo, hi, lo2, hi2;
   unsigned carry;

   // ASSERT(shift >= 0 && shift < 64);

   lo = (multiplicand & 0xffffffff) * multiplier;
   hi = (multiplicand >> 32) * multiplier;

   lo2 = lo + (hi << 32);
   carry = lo2 < lo;
   hi2 = (hi >> 32) + carry;

   if (shift == 0) {
      return lo2;
   } else {
      return (lo2 >> shift) + (hi2 << (64 - shift)) +
         ((lo2 >> (shift - 1)) & 1);
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * Muls64x32s64 --
 *
 *    Signed integer by fixed point multiplication, with rounding:
 *       result = floor(multiplicand * multiplier * 2**(-shift) + 0.5)
 * 
 *       Signed 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         (multiplier, shift), where shift < 64.
 *
 * Result:
 *       Signed 64-bit integer product.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   uint64 lo, hi, lo2, hi2;
   unsigned carry;

   // ASSERT(shift >= 0 && shift < 64);

   hi = ((uint64)multiplicand >> 32) * multiplier;
   if (multiplicand < 0) {
      hi -= (uint64)multiplier << 32;
   }
   lo = ((uint64)multiplicand & 0xffffffff) * multiplier;

   lo2 = lo + (hi << 32);
   carry = lo2 < lo;
   hi2 = (((int64)hi >> 32) + carry);

   if (shift == 0) {
      return lo2;
   } else {
      return (lo2 >> shift) + (hi2 << (64 - shift)) +
         ((lo2 >> (shift - 1)) & 1);
   }
}
#endif


#if defined __cplusplus
} // extern "C"
#endif

#endif // _MUL64_NOASM_H_

