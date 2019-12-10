/*********************************************************
 * Copyright (C) 2003-2019 VMware, Inc. All rights reserved.
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
 * vm_basic_asm.h
 *
 *	Basic asm macros
 */

#ifndef _VM_BASIC_ASM_H_
#define _VM_BASIC_ASM_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined VM_X86_64
#include "vm_basic_asm_x86_common.h"
#include "vm_basic_asm_x86_64.h"
#elif defined VM_X86_32
#include "vm_basic_asm_x86_common.h"
#include "vm_basic_asm_x86.h"
#elif defined VM_ARM_32
#include "vm_basic_asm_arm32.h"
#define MUL64_NO_ASM 1
#include "mul64.h"
#elif defined VM_ARM_64
#include "arm64_basic_defs.h"
#include "vm_basic_asm_arm64.h"
#else
#define MUL64_NO_ASM 1
#include "mul64.h"
#endif

#if defined __cplusplus
extern "C" {
#endif


/*
 * Locate most and least significant bit set functions. Use our own name
 * space to avoid namespace collisions. The new names follow a pattern,
 * <prefix><size><option>, where:
 *
 * <prefix> is [lm]ssb (least/most significant bit set)
 * <size> is size of the argument: 32 (32-bit), 64 (64-bit) or Ptr (pointer)
 * <option> is for alternative versions of the functions
 *
 * NAME        FUNCTION                    BITS     FUNC(0)
 *-----        --------                    ----     -------
 * lssb32_0    LSB set (uint32)            0..31    -1
 * mssb32_0    MSB set (uint32)            0..31    -1
 * lssb64_0    LSB set (uint64)            0..63    -1
 * mssb64_0    MSB set (uint64)            0..63    -1
 * lssbPtr_0   LSB set (uintptr_t;32-bit)  0..31    -1
 * lssbPtr_0   LSB set (uintptr_t;64-bit)  0..63    -1
 * mssbPtr_0   MSB set (uintptr_t;32-bit)  0..31    -1
 * mssbPtr_0   MSB set (uintptr_t;64-bit)  0..63    -1
 * lssbPtr     LSB set (uintptr_t;32-bit)  1..32    0
 * lssbPtr     LSB set (uintptr_t;64-bit)  1..64    0
 * mssbPtr     MSB set (uintptr_t;32-bit)  1..32    0
 * mssbPtr     MSB set (uintptr_t;64-bit)  1..64    0
 * lssb32      LSB set (uint32)            1..32    0
 * mssb32      MSB set (uint32)            1..32    0
 * lssb64      LSB set (uint64)            1..64    0
 * mssb64      MSB set (uint64)            1..64    0
 */

#ifdef _MSC_VER
static INLINE int
lssb32_0(const uint32 value)
{
   unsigned long idx;
   unsigned char ret;

   if (UNLIKELY(value == 0)) {
      return -1;
   }
   ret = _BitScanForward(&idx, (unsigned long)value);
#ifdef __analysis_assume
   __analysis_assume(ret != 0);
#endif

#pragma warning(suppress: 6001 6102) // Suppress bogus complaint that idx may be uninitialized in error case
   return idx;
}

static INLINE int
mssb32_0(const uint32 value)
{
   unsigned long idx;
   unsigned char ret;

   if (UNLIKELY(value == 0)) {
      return -1;
   }
   ret = _BitScanReverse(&idx, (unsigned long)value);
#ifdef __analysis_assume
   __analysis_assume(ret != 0);
#endif

#pragma warning(suppress: 6001 6102) // Suppress bogus complaint that idx may be uninitialized in error case
   return idx;
}

static INLINE int
lssb64_0(const uint64 value)
{
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
#ifdef VM_X86_64
      unsigned long idx;
      unsigned char ret;

      ret = _BitScanForward64(&idx, (unsigned __int64)value);
#ifdef __analysis_assume
      __analysis_assume(ret != 0);
#endif

#pragma warning(suppress: 6001 6102) // Suppress bogus complaint that idx may be uninitialized in error case
      return idx;
#else
      /* The coding was chosen to minimize conditionals and operations */
      int lowFirstBit = lssb32_0((uint32) value);
      if (lowFirstBit == -1) {
         lowFirstBit = lssb32_0((uint32) (value >> 32));
         if (lowFirstBit != -1) {
            return lowFirstBit + 32;
         }
      }
      return lowFirstBit;
#endif
   }
}

static INLINE int
mssb64_0(const uint64 value)
{
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
#ifdef VM_X86_64
      unsigned long idx;
      unsigned char ret;

      ret = _BitScanReverse64(&idx, (unsigned __int64)value);
#ifdef __analysis_assume
      __analysis_assume(ret != 0);
#endif

#pragma warning(suppress: 6001 6102) // Suppress bogus complaint that idx may be uninitialized in error case
      return idx;
#else
      /* The coding was chosen to minimize conditionals and operations */
      if (value > 0xFFFFFFFFULL) {
         return 32 + mssb32_0((uint32) (value >> 32));
      }
      return mssb32_0((uint32) value);
#endif
   }
}
#endif

#ifdef __GNUC__

#ifdef VM_X86_ANY
#define USE_ARCH_X86_CUSTOM
#endif

/* **********************************************************
 *  GCC's intrinsics for the lssb and mssb family produce sub-optimal code,
 *  so we use inline assembly to improve matters.  However, GCC cannot
 *  propagate constants through inline assembly, so we help GCC out by
 *  allowing it to use its intrinsics for compile-time constant values.
 *  Some day, GCC will make better code and these can collapse to intrinsics.
 *
 *  For example, in Decoder_AddressSize, inlined into VVT_GetVTInstrInfo:
 *  __builtin_ffs(a) compiles to:
 *  mov   $0xffffffff, %esi
 *  bsf   %eax, %eax
 *  cmovz %esi, %eax
 *  sub   $0x1, %eax
 *  and   $0x7, %eax
 *
 *  While the code below compiles to:
 *  bsf   %eax, %eax
 *  sub   $0x1, %eax
 *
 *  Ideally, GCC should have recognized non-zero input in the first case.
 *  Other instances of the intrinsic produce code like
 *  sub $1, %eax; add $1, %eax; clts
 * **********************************************************
 */

#if __GNUC__ < 4
#define FEWER_BUILTINS
#endif

static INLINE int
lssb32_0(uint32 v)
{
   int value = (int)v;
#ifdef USE_ARCH_X86_CUSTOM
   if (!__builtin_constant_p(value)) {
      if (UNLIKELY(value == 0)) {
         return -1;
      } else {
         int pos;
         __asm__ ("bsfl %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
         return pos;
      }
   }
#endif
   return __builtin_ffs(value) - 1;
}

#ifndef FEWER_BUILTINS
static INLINE int
mssb32_0(uint32 value)
{
   /*
    * We must keep the UNLIKELY(...) outside the #if defined ...
    * because __builtin_clz(0) is undefined according to gcc's
    * documentation.
    */
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
      int pos;
#ifdef USE_ARCH_X86_CUSTOM
      if (!__builtin_constant_p(value)) {
         __asm__ ("bsrl %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
         return pos;
      }
#endif
      pos = 32 - __builtin_clz(value) - 1;
      return pos;
   }
}

static INLINE int
lssb64_0(const uint64 v)
{
   int64 value = (int64)v;

#ifdef USE_ARCH_X86_CUSTOM
   if (!__builtin_constant_p(value)) {
      if (UNLIKELY(value == 0)) {
         return -1;
      } else {
         intptr_t pos;
#ifdef VM_X86_64
         __asm__ ("bsf %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
#else
         /* The coding was chosen to minimize conditionals and operations */
         pos = lssb32_0((uint32) value);
         if (pos == -1) {
            pos = lssb32_0((uint32) (value >> 32));
            if (pos != -1) {
               return pos + 32;
            }
         }
#endif
         return pos;
      }
   }
#endif
   return __builtin_ffsll(value) - 1;
}
#endif /* !FEWER_BUILTINS */

#ifdef FEWER_BUILTINS
/* GCC 3.3.x does not like __bulitin_clz or __builtin_ffsll. */
static INLINE int
mssb32_0(uint32 value)
{
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
      int pos;
      __asm__ __volatile__("bsrl %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
      return pos;
   }
}

static INLINE int
lssb64_0(const uint64 value)
{
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
      intptr_t pos;

#ifdef VM_X86_64
      __asm__ __volatile__("bsf %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
#else
      /* The coding was chosen to minimize conditionals and operations */
      pos = lssb32_0((uint32) value);
      if (pos == -1) {
         pos = lssb32_0((uint32) (value >> 32));
         if (pos != -1) {
            return pos + 32;
         }
      }
#endif /* VM_X86_64 */
      return pos;
   }
}
#endif /* FEWER_BUILTINS */


static INLINE int
mssb64_0(const uint64 value)
{
   if (UNLIKELY(value == 0)) {
      return -1;
   } else {
      intptr_t pos;

#ifdef USE_ARCH_X86_CUSTOM
#ifdef VM_X86_64
      __asm__ ("bsr %1, %0\n" : "=r" (pos) : "rm" (value) : "cc");
#else
      /* The coding was chosen to minimize conditionals and operations */
      if (value > 0xFFFFFFFFULL) {
         pos = 32 + mssb32_0((uint32) (value >> 32));
      } else {
         pos = mssb32_0((uint32) value);
      }
#endif
#else
      pos = 64 - __builtin_clzll(value) - 1;
#endif

      return pos;
   }
}

#ifdef USE_ARCH_X86_CUSTOM
#undef USE_ARCH_X86_CUSTOM
#endif

#endif // __GNUC__

static INLINE int
lssbPtr_0(const uintptr_t value)
{
#ifdef VM_64BIT
   return lssb64_0((uint64) value);
#else
   return lssb32_0((uint32) value);
#endif
}

static INLINE unsigned
lssbPtr(const uintptr_t value)
{
   return (unsigned)lssbPtr_0(value) + 1;
}

static INLINE int
mssbPtr_0(const uintptr_t value)
{
#ifdef VM_64BIT
   return mssb64_0((uint64) value);
#else
   return mssb32_0((uint32) value);
#endif
}

static INLINE unsigned
mssbPtr(const uintptr_t value)
{
   return (unsigned)mssbPtr_0(value) + 1;
}

static INLINE unsigned
lssb32(const uint32 value)
{
   return (unsigned)lssb32_0(value) + 1;
}

static INLINE unsigned
mssb32(const uint32 value)
{
   return (unsigned)mssb32_0(value) + 1;
}

static INLINE unsigned
lssb64(const uint64 value)
{
   return (unsigned)lssb64_0(value) + 1;
}

static INLINE unsigned
mssb64(const uint64 value)
{
   return (unsigned)mssb64_0(value) + 1;
}

#ifdef __GNUC__
#if defined(VM_X86_ANY) || defined(VM_ARM_ANY)

/*
 *----------------------------------------------------------------------
 *
 * uint16set --
 *
 *      memset a given address with an uint16 value, count times.
 *
 * Results:
 *      Pointer to filled memory range.
 *
 * Side effects:
 *      As with memset.
 *
 *----------------------------------------------------------------------
 */

static INLINE void *
uint16set(void *dst, uint16 val, size_t count)
{
#ifdef VM_ARM_32
   void *tmpDst = dst;

   __asm__ __volatile__ (
      "cmp     %1, #0\n\t"
      "beq     2f\n"
      "1:\n\t"
      "strh    %2, [%0], #2\n\t"
      "subs    %1, %1, #1\n\t"
      "bne     1b\n"
      "2:"
      : "+r" (tmpDst), "+r" (count)
      : "r" (val)
      : "cc", "memory");
#elif defined(VM_ARM_64)
   void   *tmpDst = dst;
   uint64  tmpVal = 0;

   if (count == 0) {
      return dst;
   }

   __asm__ __volatile__ (
      "cbz     %3, 1f\n\t"

      // Copy 16 bits twice...
      "bfm     %2, %3, #0, #15\n\t"
      "lsl     %2, %2, #16\n\t"
      "bfm     %2, %3, #0, #15\n\t"

      // Copy 32 bits from the bottom of the reg. to the top...
      "lsl     %2, %2, #32\n\t"
      "bfm     %2, %2, #32, #63\n"

      // Copy into dst 8 bytes (4 uint16s) at a time
      "1:\t"
      "cmp     %1, #4\n\t"
      "b.lo    2f\n\t"
      "str     %2, [%0], #8\n\t"
      "sub     %1, %1, #4\n\t"
      "b       1b\n"

      // Copy into dst 4 bytes at a time
      "2:\t"
      "cmp     %1, #2\n\t"
      "b.lo    3f\n\t"
      "str     %w2, [%0], #4\n\t"
      "sub     %1, %1, #2\n\t"
      "b       2b\n"

      // We have 1 or zero items left...
      "3:\t"
      "cbz     %1, 4f\n\t"
      "strh    %w2, [%0]\n"
      "4:"
      : "+r" (tmpDst), "+r" (count), "+r" (tmpVal)
      : "r" (val)
      : "cc", "memory");
#else
   size_t dummy0;
   void *dummy1;

   __asm__ __volatile__("\t"
                        "cld"            "\n\t"
                        "rep ; stosw"    "\n"
                        : "=c" (dummy0), "=D" (dummy1)
                        : "0" (count), "1" (dst), "a" (val)
                        : "memory", "cc"
      );
#endif
   return dst;
}


/*
 *----------------------------------------------------------------------
 *
 * uint32set --
 *
 *      memset a given address with an uint32 value, count times.
 *
 * Results:
 *      Pointer to filled memory range.
 *
 * Side effects:
 *      As with memset.
 *
 *----------------------------------------------------------------------
 */

static INLINE void *
uint32set(void *dst, uint32 val, size_t count)
{
#ifdef VM_ARM_32
   void *tmpDst = dst;

   __asm__ __volatile__ (
      "cmp     %1, #0\n\t"
      "beq     2f\n"
      "1:\n\t"
      "str     %2, [%0], #4\n\t"
      "subs    %1, %1, #1\n\t"
      "bne     1b\n"
      "2:"
      : "+r" (tmpDst), "+r" (count)
      : "r" (val)
      : "cc", "memory");
#elif defined(VM_ARM_64)
   void   *tmpDst = dst;

   if (count == 0) {
      return dst;
   }

   __asm__ __volatile__ (
      "cbz     %2, 1f\n\t"

      // Drop our value in the top 32 bits, then copy from there to the bottom
      "lsl     %2, %2, #32\n\t"
      "bfm     %2, %2, #32, #63\n"

      // Copy four at a time
      "1:\t"
      "cmp     %1, #16\n\t"
      "b.lo    2f\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "sub     %1, %1, #16\n\t"
      "b       1b\n"

      // Copy remaining pairs of data
      "2:\t"
      "cmp     %1, #2\n\t"
      "b.lo    3f\n\t"
      "str     %2, [%0], #8\n\t"
      "sub     %1, %1, #2\n\t"
      "b       2b\n"

      // One or zero values left to copy
      "3:\t"
      "cbz     %1, 4f\n\t"
      "str     %w2, [%0]\n\t" // No incr
      "4:"
      : "+r" (tmpDst), "+r" (count), "+r" (val)
      :
      : "cc", "memory");
#else
   size_t dummy0;
   void *dummy1;

   __asm__ __volatile__("\t"
                        "cld"            "\n\t"
                        "rep ; stosl"    "\n"
                        : "=c" (dummy0), "=D" (dummy1)
                        : "0" (count), "1" (dst), "a" (val)
                        : "memory", "cc"
      );
#endif
   return dst;
}

#else /* unknown system: rely on C to write */
static INLINE void *
uint16set(void *dst, uint16 val, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++) {
     ((uint16 *) dst)[i] = val;
   }
   return dst;
}

static INLINE void *
uint32set(void *dst, uint32 val, size_t count)
{
   size_t i;
   for (i = 0; i < count; i++) {
     ((uint32 *) dst)[i] = val;
   }
   return dst;
}
#endif // defined(VM_X86_ANY) || defined(VM_ARM_ANY)
#elif defined(_MSC_VER)

static INLINE void *
uint16set(void *dst, uint16 val, size_t count)
{
#ifdef VM_X86_64
   __stosw((uint16*)dst, val, count);
#elif defined(VM_ARM_32)
   size_t i;
   for (i = 0; i < count; i++) {
      ((uint16 *)dst)[i] = val;
   }
#else
   __asm { pushf;
           mov ax, val;
           mov ecx, count;
           mov edi, dst;
           cld;
           rep stosw;
           popf;
   }
#endif
   return dst;
}

static INLINE void *
uint32set(void *dst, uint32 val, size_t count)
{
#ifdef VM_X86_64
   __stosd((unsigned long*)dst, (unsigned long)val, count);
#elif defined(VM_ARM_32)
   size_t i;
   for (i = 0; i < count; i++) {
      ((uint32 *)dst)[i] = val;
   }
#else
   __asm { pushf;
           mov eax, val;
           mov ecx, count;
           mov edi, dst;
           cld;
           rep stosd;
           popf;
   }
#endif
   return dst;
}

#else
#error "No compiler defined for uint*set"
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Bswap16 --
 *
 *      Swap the 2 bytes of "v" as follows: 32 -> 23.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Bswap16(uint16 v)
{
#if defined(VM_ARM_64)
   __asm__("rev16 %w0, %w0" : "+r"(v));
   return v;
#else
   return ((v >> 8) & 0x00ff) | ((v << 8) & 0xff00);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Bswap32 --
 *
 *      Swap the 4 bytes of "v" as follows: 3210 -> 0123.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Bswap32(uint32 v) // IN
{
#if defined(__GNUC__) && defined(VM_X86_ANY)
   /* Checked against the Intel manual and GCC. --hpreg */
   __asm__(
      "bswap %0"
      : "=r" (v)
      : "0" (v)
   );
   return v;
#elif defined(VM_ARM_32) && !defined(__ANDROID__) && !defined(_MSC_VER)
    __asm__("rev %0, %0" : "+r"(v));
    return v;
#elif defined(VM_ARM_64)
   __asm__("rev32 %x0, %x0" : "+r"(v));
   return v;
#else
   return    (v >> 24)
          | ((v >>  8) & 0xFF00)
          | ((v & 0xFF00) <<  8)
          |  (v << 24)          ;
#endif
}
#define Bswap Bswap32


/*
 *-----------------------------------------------------------------------------
 *
 * Bswap64 --
 *
 *      Swap the 8 bytes of "v" as follows: 76543210 -> 01234567.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Bswap64(uint64 v) // IN
{
#if defined(VM_ARM_64)
   __asm__("rev %0, %0" : "+r"(v));
   return v;
#else
   return ((uint64)Bswap((uint32)v) << 32) | Bswap((uint32)(v >> 32));
#endif
}


/*
 * PAUSE is a P4 instruction that improves spinlock power+performance;
 * on non-P4 IA32 systems, the encoding is interpreted as a REPZ-NOP.
 * Use volatile to avoid NOP removal.
 */
static INLINE void
PAUSE(void)
#if defined(__GNUC__) || defined(VM_ARM_32)
{
#ifdef VM_ARM_64
   __asm__ __volatile__("yield");
#elif defined VM_ARM_32
   /*
    * YIELD is available in ARMv6K and above, so we could probably refine this
    * instead of leaving it empty.
    */
#else // x86
   __asm__ __volatile__( "pause" :);
#endif
}
#elif defined(_MSC_VER)
#ifdef VM_X86_64
{
   _mm_pause();
}
#else /* VM_X86_64 */
#pragma warning( disable : 4035)
{
   __asm _emit 0xf3 __asm _emit 0x90
}
#pragma warning (default: 4035)
#endif /* VM_X86_64 */
#else  /* __GNUC__  */
#error No compiler defined for PAUSE
#endif


/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * volatile because the tsc always changes without the compiler knowing it.
 */
static INLINE uint64
RDTSC(void)
#ifdef __GNUC__
{
#ifdef VM_X86_64
   uint64 tscLow;
   uint64 tscHigh;

   __asm__ __volatile__(
      "rdtsc"
      : "=a" (tscLow), "=d" (tscHigh)
   );

   return tscHigh << 32 | tscLow;
#elif defined(VM_X86_32)
   uint64 tim;

   __asm__ __volatile__(
      "rdtsc"
      : "=A" (tim)
   );

   return tim;
#elif defined(VM_ARM_64)
#if (defined(VMKERNEL) || defined(VMM)) && !defined(VMK_ARM_EL1)
   return MRS(CNTPCT_EL0);
#else
   return MRS(CNTVCT_EL0);
#endif
#else
   /*
    * For platform without cheap timer, just return 0.
    */
   return 0;
#endif
}
#elif defined(_MSC_VER)
#ifdef VM_X86_64
{
   return __rdtsc();
}
#elif defined(VM_ARM_32)
{
   /*
    * We need to do more inverstagetion here to find
    * a microsoft equivalent of that code
    */
   NOT_IMPLEMENTED();
   return 0;
}
#else
#pragma warning( disable : 4035)
{
   __asm _emit 0x0f __asm _emit 0x31
}
#pragma warning (default: 4035)
#endif /* VM_X86_64 */
#else  /* __GNUC__  */
#error No compiler defined for RDTSC
#endif /* __GNUC__  */


/*
 *-----------------------------------------------------------------------------
 *
 * DEBUGBREAK --
 *
 *    Does an int3 for MSVC / GCC, bkpt/brk for ARM. This is a macro to make
 *    sure int3 is always inlined.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VM_ARM_32
#define DEBUGBREAK() __asm__("bkpt")
#elif defined(VM_ARM_64)
#define DEBUGBREAK() __asm__("brk #0")
#elif defined(_MSC_VER)
#define DEBUGBREAK() __debugbreak()
#else
#define DEBUGBREAK() __asm__("int $3")
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * {Clear,Set,Test}Bit{32,64} --
 *
 *    Sets tests or clears a specified single bit in the provided variable.
 *
 *    The index input value specifies which bit to modify and is 0-based.
 *    Index is truncated by hardware to a 5-bit or 6-bit offset for the
 *    32 and 64-bit flavors, respectively, but input values are not validated
 *    with asserts to avoid include dependencies.
 *
 *    64-bit flavors are not handcrafted for 32-bit builds because they may
 *    defeat compiler optimizations.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
SetBit32(uint32 *var, unsigned index)
{
   *var |= 1 << index;
}

static INLINE void
ClearBit32(uint32 *var, unsigned index)
{
   *var &= ~(1 << index);
}

static INLINE void
SetBit64(uint64 *var, unsigned index)
{
   *var |= CONST64U(1) << index;
}

static INLINE void
ClearBit64(uint64 *var, unsigned index)
{
   *var &= ~(CONST64U(1) << index);
}

static INLINE Bool
TestBit32(const uint32 *var, unsigned index)
{
   return (*var & (1 << index)) != 0;
}

static INLINE Bool
TestBit64(const uint64 *var, unsigned index)
{
   return (*var & (CONST64U(1) << index)) != 0;
}

/*
 *-----------------------------------------------------------------------------
 *
 * {Clear,Set,Complement,Test}BitVector --
 *
 *    Sets, clears, complements, or tests a specified single bit in the
 *    provided array.  The index input value specifies which bit to modify
 *    and is 0-based.  Bit number can be +-2Gb (+-128MB) relative from 'var'
 *    variable.
 *
 *    All functions return value of the bit before modification was performed.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
SetBitVector(void *var, int32 index)
{
#if defined(__GNUC__) && defined(VM_X86_ANY)
   Bool bit;
   __asm__ (
      "bts %2, %1;"
      "setc %0"
      : "=qQm" (bit), "+m" (*(uint32 *)var)
      : "rI" (index)
      : "memory", "cc"
   );
   return bit;
#elif defined(_MSC_VER)
   return _bittestandset((long *)var, index) != 0;
#else
   Bool retVal = (((uint8 *)var)[index / 8] & (1 << (index % 8))) != 0;
   ((uint8 *)var)[index / 8] |= 1 << (index % 8);
   return retVal;
#endif
}

static INLINE Bool
ClearBitVector(void *var, int32 index)
{
#if defined(__GNUC__) && defined(VM_X86_ANY)
   Bool bit;
   __asm__ (
      "btr %2, %1;"
      "setc %0"
      : "=qQm" (bit), "+m" (*(uint32 *)var)
      : "rI" (index)
      : "memory", "cc"
   );
   return bit;
#elif defined(_MSC_VER)
   return _bittestandreset((long *)var, index) != 0;
#else
   Bool retVal = (((uint8 *)var)[index / 8] & (1 << (index % 8))) != 0;
   ((uint8 *)var)[index / 8] &= ~(1 << (index % 8));
   return retVal;
#endif
}

static INLINE Bool
ComplementBitVector(void *var, int32 index)
{
#if defined(__GNUC__) && defined(VM_X86_ANY)
   Bool bit;
   __asm__ (
      "btc %2, %1;"
      "setc %0"
      : "=qQm" (bit), "+m" (*(uint32 *)var)
      : "rI" (index)
      : "memory", "cc"
   );
   return bit;
#elif defined(_MSC_VER)
   return _bittestandcomplement((long *)var, index) != 0;
#else
   Bool retVal = (((uint8 *)var)[index / 8] & (1 << (index % 8))) != 0;
   ((uint8 *)var)[index / 8] ^= ~(1 << (index % 8));
   return retVal;
#endif
}

static INLINE Bool
TestBitVector(const void *var, int32 index)
{
#if defined(__GNUC__) && defined(VM_X86_ANY)
   Bool bit;
   __asm__ (
      "bt %2, %1;"
      "setc %0"
      : "=qQm" (bit)
      : "m" (*(const uint32 *)var), "rI" (index)
      : "cc"
   );
   return bit;
#elif defined _MSC_VER
   return _bittest((long *)var, index) != 0;
#else
   return (((const uint8 *)var)[index / 8] & (1 << (index % 8))) != 0;
#endif
}

/*
 *-----------------------------------------------------------------------------
 * RoundUpPow2_{64,32} --
 *
 *   Rounds a value up to the next higher power of 2.  Returns the original
 *   value if it is a power of 2.  The next power of 2 for inputs {0, 1} is 1.
 *   The result is undefined for inputs above {2^63, 2^31} (but equal to 1
 *   in this implementation).
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
RoundUpPow2C64(uint64 value)
{
   if (value <= 1 || value > (CONST64U(1) << 63)) {
      return 1; // Match the assembly's undefined value for large inputs.
   } else {
      return (CONST64U(2) << mssb64_0(value - 1));
   }
}

#if defined(__GNUC__) && defined(VM_X86_64)
static INLINE uint64
RoundUpPow2Asm64(uint64 value)
{
   uint64 out = 2;
   __asm__("lea -1(%[in]), %%rcx;"      // rcx = value - 1.  Preserve original.
           "bsr %%rcx, %%rcx;"          // rcx = log2(value - 1) if value != 1
                                        // if value == 0, then rcx = 63
                                        // if value == 1 then zf = 1, else zf = 0.
           "rol %%cl, %[out];"          // out = 2 << rcx (if rcx != -1)
                                        //     = 2^(log2(value - 1) + 1)
                                        // if rcx == -1 (value == 0), out = 1
                                        // zf is always unmodified.
           "cmovz %[in], %[out]"        // if value == 1 (zf == 1), write 1 to out.
       : [out]"+r"(out) : [in]"r"(value) : "%rcx", "cc");
   return out;
}
#endif

static INLINE uint64
RoundUpPow2_64(uint64 value)
{
#if defined(__GNUC__) && defined(VM_X86_64)
   if (__builtin_constant_p(value)) {
      return RoundUpPow2C64(value);
   } else {
      return RoundUpPow2Asm64(value);
   }
#else
   return RoundUpPow2C64(value);
#endif
}

static INLINE uint32
RoundUpPow2C32(uint32 value)
{
   if (value <= 1 || value > (1U << 31)) {
      return 1; // Match the assembly's undefined value for large inputs.
   } else {
      int mssb32 = mssb32_0(value - 1);
      /* invariant: mssb32 >= 0 */
      return (2U << (uint32)mssb32);
   }
}

#ifdef __GNUC__
static INLINE uint32
RoundUpPow2Asm32(uint32 value)
{
#ifdef VM_ARM_32
   uint32 out = 1;
   // Note: None Thumb only!
   //       The value of the argument "value"
   //       will be affected!
   __asm__("sub %[in], %[in], #1;"         // r1 = value - 1 . if value == 0 then r1 = 0xFFFFFFFF
           "clz %[in], %[in];"             // r1 = log2(value - 1) if value != 1
                                           // if value == 0 then r1 = 0
                                           // if value == 1 then r1 = 32
           "mov %[out], %[out], ror %[in]" // out = 2^(32 - r1)
                                           // if out == 2^32 then out = 1 as it is right rotate
       : [in]"+r"(value),[out]"+r"(out));
   return out;
#elif defined(VM_ARM_64)
   return RoundUpPow2C32(value);
#else
   uint32 out = 2;

   __asm__("lea -1(%[in]), %%ecx;"      // ecx = value - 1.  Preserve original.
           "bsr %%ecx, %%ecx;"          // ecx = log2(value - 1) if value != 1
                                        // if value == 0, then ecx = 31
                                        // if value == 1 then zf = 1, else zf = 0.
           "rol %%cl, %[out];"          // out = 2 << ecx (if ecx != -1)
                                        //     = 2^(log2(value - 1) + 1).
                                        // if ecx == -1 (value == 0), out = 1
                                        // zf is always unmodified
           "cmovz %[in], %[out]"        // if value == 1 (zf == 1), write 1 to out.
       : [out]"+r"(out) : [in]"r"(value) : "%ecx", "cc");
   return out;
#endif
}
#endif // __GNUC__

static INLINE uint32
RoundUpPow2_32(uint32 value)
{
#ifdef __GNUC__
   if (__builtin_constant_p(value)) {
      return RoundUpPow2C32(value);
   } else {
      return RoundUpPow2Asm32(value);
   }
#else
   return RoundUpPow2C32(value);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * PopCount32 --
 *
 *     Counts "1" bits in a uint32.
 *
 * Results:
 *     Returns the number of bits set to 1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE unsigned
PopCount32(uint32 value)
{
   /*
    * Attribution:
    *     This algorithm was copied from:
    *         http://www.aggregate.org/MAGIC#Population Count (Ones Count)
    *
    *     A virtually identical version (but in assembly) appears in an
    *     AMD reference manual.
    *
    *     No license appears in the original code, but the website
    *     header states:
    *
    *     "None of the following coding tricks came from proprietary
    *     sources; further, we believe that each of the tricks we did
    *     not invent is essentially "standard engineering practice" in
    *     the specialized niche where it applies. Thus, although we
    *     have not conducted patent searches, etc., to confirm it, we
    *     believe that these are tricks that freely can be used for
    *     any purpose. Of course, The Aggregate accepts no
    *     responsibility for your use of these tricks; you must
    *     confirm that the trick does what you want and that you can
    *     use it as you intend. That said, we do intend to maintain
    *     this page by adding new algorithms and/or correcting
    *     existing entries. If you have any comments, please contact
    *     Professor Hank Dietz, http://aggregate.org/hankd/"
    *
    *     "This document should be cited using something like the
    *     following bibtex entry:" (most recent retrieval date added)
    *
    *     @techreport{magicalgorithms,
    *     author={Henry Gordon Dietz},
    *     title={{The Aggregate Magic Algorithms}},
    *     institution={University of Kentucky},
    *     howpublished={Aggregate.Org online technical report},
    *     URL={http://aggregate.org/MAGIC/},
    *     urldate={2016-01-27}
    *     }
    */
   value -= ((value >> 1) & 0x55555555);
   value = (((value >> 2) & 0x33333333) + (value & 0x33333333));
   value = (((value >> 4) + value) & 0x0f0f0f0f);
   value += (value >> 8);
   value += (value >> 16);
   return value & 0x0000003f;
}


/*
 *-----------------------------------------------------------------------------
 *
 * PopCount64 --
 *
 *     Counts "1" bits in a uint64.
 *
 * Results:
 *     Returns the number of bits set to 1.
 *
 * Side effects:
 *     None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE unsigned
PopCount64(uint64 value)
{
   value -= (value >> 1) & 0x5555555555555555ULL;
   value = ((value >> 2) & 0x3333333333333333ULL) +
           (value & 0x3333333333333333ULL);
   value = ((value >> 4) + value) & 0x0f0f0f0f0f0f0f0fULL;
   value += value >> 8;
   value += value >> 16;
   value += value >> 32;
   return (unsigned) (value & 0xff);
}


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VM_BASIC_ASM_H_

