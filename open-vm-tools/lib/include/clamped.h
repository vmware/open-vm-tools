/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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
 * clamped.h --
 *
 *      Clamped arithmetic. This header file provides inline
 *      arithmetic operations that don't overflow. Instead, they
 *      saturate at the data type's max or min value.
 */

#ifndef _CLAMPED_H_
#define _CLAMPED_H_

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "vm_basic_defs.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_U64To32 --
 *
 *      Convert unsigned 64-bit to 32-bit, clamping instead of truncating.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_U64To32(uint32 *out,  // OUT
                uint64 a)     // IN
{
   uint32 clamped = (uint32)a;

   if (UNLIKELY(a != clamped)) {
      *out = MAX_UINT32;
      return FALSE;
   }

   *out = clamped;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_S64To32 --
 *
 *      Convert signed 64-bit to 32-bit, clamping instead of truncating.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_INT32 or MIN_INT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_S64To32(int32 *out,  // OUT
                int64 a)     // IN
{
   int32 clamped = (int32)a;

   if (UNLIKELY(a != clamped)) {
      *out = a < 0 ? MIN_INT32 : MAX_INT32;
      return FALSE;
   }

   *out = clamped;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_S32To16 --
 *
 *      Convert signed 32-bit to 16-bit, clamping instead of truncating.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_INT16 or MIN_INT16, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_S32To16(int16 *out,  // OUT
                int32 a)     // IN
{
   int16 clamped = (int16)a;

   if (UNLIKELY(a != clamped)) {
      *out = a < 0 ? MIN_INT16 : MAX_INT16;
      return FALSE;
   }

   *out = clamped;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Clamped_SAdd32 --
 *
 *      Signed 32-bit addition.
 *
 *      Add two integers, clamping the result to MAX_INT32 or
 *      MIN_INT32 if it would have overflowed.
 *
 * Results:
 *      On success, returns TRUE.
 *      If the result would have overflowed and we clamped it, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Clamped_SAdd32(int32 *out,  // OUT
               int32 a,     // IN
               int32 b)     // IN
{
   return Clamped_S64To32(out, (int64)a + b);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_UMul32 --
 *
 *      Unsigned 32-bit multiplication.
 *
 *      We're abusing the fact that 32x32-bit multiplication always
 *      returns a 64-bit result on x86 anyway, and that the compiler
 *      should be smart enough to optimize the code here into a
 *      32x32-bit multiply.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_UMul32(uint32 *out,  // OUT
               uint32 a,     // IN
               uint32 b)     // IN
{
   return Clamped_U64To32(out, (uint64)a * b);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_SMul32 --
 *
 *      Signed 32-bit multiplication.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_INT32 or MIN_INT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_SMul32(int32 *out,  // OUT
               int32 a,     // IN
               int32 b)     // IN
{
   return Clamped_S64To32(out, (int64)a * b);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_UAdd32 --
 *
 *      Unsigned 32-bit addition.
 *
 *      This is a utility function for 32-bit unsigned addition,
 *      in which the result is clamped to MAX_UINT32 on overflow.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_UAdd32(uint32 *out,  // OUT
               uint32 a,     // IN
               uint32 b)     // IN
{
   uint32 c = a + b;

   /*
    * Checking against one src operand is sufficient.
    */
   if (UNLIKELY(c < a)) {
      *out = MAX_UINT32;
      return FALSE;
   }

   *out = c;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_UAdd64 --
 *
 *      Unsigned 64-bit addition.
 *
 *      This is a utility function for 64-bit unsigned addition,
 *      in which the result is clamped to MAX_UINT64 on overflow.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT64, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_UAdd64(uint64 *out,   // OUT
               uint64 a,      // IN
               uint64 b)      // IN
{
   uint64 c = a + b;

   /*
    * Checking against one src operand is sufficient.
    */
   if (UNLIKELY(c < a)) {
      *out = MAX_UINT64;
      return FALSE;
   }

   *out = c;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_URoundUpBits32 --
 *
 *      Round up an unsigned 32-bit number by the specified number
 *      of bits.  Clamp to MAX_UINT32 on overflow.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT32, returns FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_URoundUpBits32(uint32 *out,  // OUT
                       uint32 x,     // IN
                       uint32 bits)  // IN
{
   uint32 mask = (1 << bits) - 1;
   uint32 c = (x + mask) & ~mask;

   ASSERT(bits < sizeof(uint32) * 8);

   if (UNLIKELY(x + mask < x)) {
      *out = MAX_UINT32;
      return FALSE;
   }

   *out = c;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_UMul64 --
 *
 *      Unsigned 64-bit multiplication.
 *
 * Results:
 *      On success, returns TRUE. If the result would have overflowed
 *      and we clamped it to MAX_UINT64, returns FALSE.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_UMul64(uint64 *out,  // OUT
               uint64 a,     // IN
               uint64 b)     // IN
{
   uint32 aL = a & MASK64(32);
   uint32 aH = a >> 32;
   uint32 bL = b & MASK64(32);
   uint32 bH = b >> 32;

   ASSERT(out != NULL);

   if (UNLIKELY(aH > 0 && bH > 0)) {
      *out = MAX_UINT64;
      return FALSE;
   } else {
      uint64 s1 = (aH * (uint64)bL) << 32;
      uint64 s2 = (aL * (uint64)bH) << 32;
      uint64 s3 = (aL * (uint64)bL);
      uint64 sum;
      Bool clamped;

      ASSERT(s1 == 0 || s2 == 0);
      sum = s1 + s2;
      clamped = !Clamped_UAdd64(&sum, sum, s3);

      *out = sum;
      return !clamped;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Clamped_USub64 --
 *
 *      Unsigned 64-bit subtraction.
 *      Compute (a - b), and clamp to 0 if the result would have underflowed.
 *
 * Results:
 *      On success, returns TRUE. If the result would have underflowed
 *      and we clamped it to 0, returns FALSE.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Clamped_USub64(uint64 *out,  // OUT
               uint64 a,     // IN
               uint64 b)     // IN
{
   ASSERT(out != NULL);

   if (UNLIKELY(b > a)) {
      *out = 0;
      return FALSE;
   } else {
      *out = a - b;
      return TRUE;
   }
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _CLAMPED_H_
