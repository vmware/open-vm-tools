/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * vm_basic_asm_x86_64.h
 *
 *	Basic x86_64 asm macros.
 */

#ifndef _VM_BASIC_ASM_X86_64_H_
#define _VM_BASIC_ASM_X86_64_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifndef VM_X86_64
#error "This file is x86-64 only!"
#endif

#if defined(_MSC_VER) && !defined(BORA_NO_WIN32_INTRINS)

#ifdef __cplusplus
extern "C" {
#endif
uint64 _umul128(uint64 multiplier, uint64 multiplicand,
                uint64 *highProduct);
int64 _mul128(int64 multiplier, int64 multiplicand,
              int64 *highProduct);
uint64 __shiftright128(uint64 lowPart, uint64 highPart, uint8 shift);
#ifdef __cplusplus
}
#endif

#pragma intrinsic(_umul128, _mul128, __shiftright128)

#endif // _MSC_VER

#if defined(__GNUC__)
/*
 * GET_CURRENT_PC
 *
 * Returns the current program counter (i.e. instruction pointer i.e. rip
 * register on x86_64). In the example below:
 *
 *   foo.c
 *   L123: Foo(GET_CURRENT_PC())
 *
 * the return value from GET_CURRENT_PC will point a debugger to L123.
 */
#define GET_CURRENT_PC() ({                                           \
      void *__rip;                                                    \
      asm("lea 0(%%rip), %0;\n\t"                                     \
         : "=r" (__rip));                                             \
      __rip;                                                          \
})

/*
 * GET_CURRENT_LOCATION
 *
 * Updates the arguments with the values of the %rip, %rbp, and %rsp
 * registers at the current code location where the macro is invoked,
 * and the return address.
 */
#define GET_CURRENT_LOCATION(rip, rbp, rsp, retAddr)  do {         \
      asm("lea 0(%%rip), %0\n"                                     \
          "mov %%rbp, %1\n"                                        \
          "mov %%rsp, %2\n"                                        \
          : "=r" (rip), "=r" (rbp), "=r" (rsp));                   \
      retAddr = (uint64) GetReturnAddress();                       \
   } while (0)
#endif

/*
 * FXSAVE/FXRSTOR
 *     save/restore SIMD/MMX fpu state
 *
 * The pointer passed in must be 16-byte aligned.
 *
 * Intel and AMD processors behave differently w.r.t. fxsave/fxrstor. Intel
 * processors unconditionally save the exception pointer state (instruction
 * ptr., data ptr., and error instruction opcode). FXSAVE_ES1 and FXRSTOR_ES1
 * work correctly for Intel processors.
 *
 * AMD processors only save the exception pointer state if ES=1. This leads to a
 * security hole whereby one process/VM can inspect the state of another process
 * VM. The AMD recommended workaround involves clobbering the exception pointer
 * state unconditionally, and this is implemented in FXRSTOR_AMD_ES0. Note that
 * FXSAVE_ES1 will only save the exception pointer state for AMD processors if
 * ES=1.
 *
 * The workaround (FXRSTOR_AMD_ES0) only costs 1 cycle more than just doing an
 * fxrstor, on both AMD Opteron and Intel Core CPUs.
 */
#if defined(__GNUC__)

static INLINE void
FXSAVE_ES1(void *save)
{
   __asm__ __volatile__ ("fxsaveq %0  \n" : "=m" (*(uint8 *)save) : : "memory");
}

static INLINE void
FXSAVE_COMPAT_ES1(void *save)
{
   __asm__ __volatile__ ("fxsave %0  \n" : "=m" (*(uint8 *)save) : : "memory");
}

static INLINE void
FXRSTOR_ES1(const void *load)
{
   __asm__ __volatile__ ("fxrstorq %0 \n"
                         : : "m" (*(const uint8 *)load) : "memory");
}

static INLINE void
FXRSTOR_COMPAT_ES1(const void *load)
{
   __asm__ __volatile__ ("fxrstor %0 \n"
                         : : "m" (*(const uint8 *)load) : "memory");
}

static INLINE void
FXRSTOR_AMD_ES0(const void *load)
{
   uint64 dummy = 0;

   __asm__ __volatile__
       ("fnstsw  %%ax    \n"     // Grab x87 ES bit
        "bt      $7,%%ax \n"     // Test ES bit
        "jnc     1f      \n"     // Jump if ES=0
        "fnclex          \n"     // ES=1. Clear it so fild doesn't trap
        "1:              \n"
        "ffree   %%st(7) \n"     // Clear tag bit - avoid poss. stack overflow
        "fildl   %0      \n"     // Dummy Load from "safe address" changes all
                                 // x87 exception pointers.
        "fxrstorq %1 \n"
        :
        : "m" (dummy), "m" (*(const uint8 *)load)
        : "ax", "memory");
}

#endif /* __GNUC__ */

/*
 * XSAVE/XRSTOR
 *     save/restore GSSE/SIMD/MMX fpu state
 *
 * The pointer passed in must be 64-byte aligned.
 * See above comment for more information.
 */
#if defined(__GNUC__) && (defined(VMM) || defined(VMKERNEL) || defined(FROBOS))

static INLINE void
XSAVE_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        "xsaveq %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XSAVE_COMPAT_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        "xsave %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XSAVEOPT_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        "xsaveoptq %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XSAVEC_COMPAT_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        "xsavec %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XRSTOR_ES1(const void *load, uint64 mask)
{
   __asm__ __volatile__ (
        "xrstorq %0 \n"
        :
        : "m" (*(const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XRSTOR_COMPAT_ES1(const void *load, uint64 mask)
{
   __asm__ __volatile__ (
        "xrstor %0 \n"
        :
        : "m" (*(const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void
XRSTOR_AMD_ES0(const void *load, uint64 mask)
{
   uint64 dummy = 0;

   __asm__ __volatile__
       ("fnstsw  %%ax    \n"     // Grab x87 ES bit
        "bt      $7,%%ax \n"     // Test ES bit
        "jnc     1f      \n"     // Jump if ES=0
        "fnclex          \n"     // ES=1. Clear it so fild doesn't trap
        "1:              \n"
        "ffree   %%st(7) \n"     // Clear tag bit - avoid poss. stack overflow
        "fildl   %0      \n"     // Dummy Load from "safe address" changes all
                                 // x87 exception pointers.
        "mov %%ebx, %%eax \n"
        "xrstorq %1 \n"
        :
        : "m" (dummy), "m" (*(const uint8 *)load),
          "b" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "eax", "memory");
}

#endif /* __GNUC__ */

/*
 * XTEST
 *     Return TRUE if processor is in transaction region.
 *
 *  Using condition codes as output values (=@ccnz) requires gcc6 or
 *  above.  Clang does not support condition codes as output
 *  constraints.
 *
 */
#if defined(__GNUC__) && (defined(VMM) || defined(VMKERNEL) || defined(FROBOS))
static INLINE Bool
xtest(void)
{
   Bool result;
#if defined(__clang__)
   __asm__ __volatile__("xtest\n"
                        "setnz %%al"
                        : "=a" (result) : : "cc");
#else
   __asm__ __volatile__("xtest"
                        : "=@ccnz" (result) : : "cc");
#endif
   return result;
}

#endif /* __GNUC__ */

/*
 *-----------------------------------------------------------------------------
 *
 * Mul64x6464 --
 *
 *    Unsigned integer by fixed point multiplication, with rounding:
 *       result = floor(multiplicand * multiplier * 2**(-shift) + 0.5)
 *
 *       Unsigned 64-bit integer multiplicand.
 *       Unsigned 64-bit fixed point multiplier, represented as
 *         (multiplier, shift), where shift < 64.
 *
 * Result:
 *       Unsigned 64-bit integer product.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__) && !defined(MUL64_NO_ASM)

static INLINE uint64
Mul64x6464(uint64 multiplicand,
           uint64 multiplier,
           uint32 shift)
{
   /*
    * Implementation:
    *    Multiply 64x64 bits to yield a full 128-bit product.
    *    Clear the carry bit (needed for the shift == 0 case).
    *    Shift result in RDX:RAX right by "shift".
    *    Add the carry bit.  (If shift > 0, this is the highest order bit
    *      that was discarded by the shift; else it is 0.)
    *    Return the low-order 64 bits of the above.
    *
    */
   uint64 result, dummy;

   __asm__("mulq    %3           \n\t"
           "clc                  \n\t"
           "shrdq   %b4, %1, %0  \n\t"
           "adc     $0, %0       \n\t"
           : "=a" (result),
             "=d" (dummy)
           : "0"  (multiplier),
             "rm" (multiplicand),
             "c"  (shift)
           : "cc");
   return result;
}

#elif defined(_MSC_VER) && !defined(MUL64_NO_ASM)

static INLINE uint64
Mul64x6464(uint64 multiplicand,
           uint64 multiplier,
           uint32 shift)
{
   /*
    * Unfortunately, MSVC intrinsics don't give us access to the carry
    * flag after a 128-bit shift, so the implementation is more
    * awkward:
    *    Multiply 64x64 bits to yield a full 128-bit product.
    *    Shift result right by "shift".
    *    If shift != 0, extract and add in highest order bit that was
    *      discarded by the shift.
    *    Return the low-order 64 bits of the above.
    */
   uint64 tmplo, tmphi;
   tmplo = _umul128(multiplicand, multiplier, &tmphi);
   if (shift == 0) {
      return tmplo;
   } else {
      return __shiftright128(tmplo, tmphi, (uint8) shift) +
         ((tmplo >> (shift - 1)) & 1);
   }
}

#else
#define MUL64_NO_ASM 1
#include "mul64.h"
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Muls64x64s64 --
 *
 *    Signed integer by fixed point multiplication, with rounding:
 *       result = floor(multiplicand * multiplier * 2**(-shift) + 0.5)
 *
 *       Signed 64-bit integer multiplicand.
 *       Unsigned 64-bit fixed point multiplier, represented as
 *         (multiplier, shift), where shift < 64.
 *
 * Result:
 *       Signed 64-bit integer product.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__) && !defined(MUL64_NO_ASM)

static inline int64
Muls64x64s64(int64 multiplicand,
             int64 multiplier,
             uint32 shift)
{
   int64 result, dummy;

   /* Implementation:
    *    Multiply 64x64 bits to yield a full 128-bit product.
    *    Clear the carry bit (needed for the shift == 0 case).
    *    Shift result in RDX:RAX right by "shift".
    *    Add the carry bit.  (If shift > 0, this is the highest order bit
    *      that was discarded by the shift; else it is 0.)
    *    Return the low-order 64 bits of the above.
    *
    *    Note: using the unsigned shrd instruction is correct because
    *    shift < 64 and we return only the low 64 bits of the shifted
    *    result.
    */
   __asm__("imulq   %3           \n\t"
           "clc                  \n\t"
           "shrdq   %b4, %1, %0  \n\t"
           "adc     $0, %0       \n\t"
           : "=a" (result),
             "=d" (dummy)
           : "0"  (multiplier),
             "rm" (multiplicand),
             "c"  (shift)
           : "cc");
   return result;
}

#elif defined(_MSC_VER) && !defined(MUL64_NO_ASM)

static INLINE int64
Muls64x64s64(int64 multiplicand,
             int64 multiplier,
             uint32 shift)
{
   /*
    * Unfortunately, MSVC intrinsics don't give us access to the carry
    * flag after a 128-bit shift, so the implementation is more
    * awkward:
    *    Multiply 64x64 bits to yield a full 128-bit product.
    *    Shift result right by "shift".
    *    If shift != 0, extract and add in highest order bit that was
    *      discarded by the shift.
    *    Return the low-order 64 bits of the above.
    *
    * Note: using an unsigned shift is correct because shift < 64 and
    * we return only the low 64 bits of the shifted result.
    */
   int64 tmplo, tmphi;
   tmplo = _mul128(multiplicand, multiplier, &tmphi);
   if (shift == 0) {
      return tmplo;
   } else {
      return __shiftright128(tmplo, tmphi, (uint8) shift) +
         ((tmplo >> (shift - 1)) & 1);
   }
}

#endif

#ifndef MUL64_NO_ASM
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
   return Mul64x6464(multiplicand, multiplier, shift);
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
   return Muls64x64s64(multiplicand, multiplier, shift);
}
#endif

#if defined(__GNUC__)

static INLINE void *
uint64set(void *dst, uint64 val, uint64 count)
{
   int dummy0;
   int dummy1;
   __asm__ __volatile__("\t"
                        "cld"            "\n\t"
                        "rep ; stosq"    "\n"
                        : "=c" (dummy0), "=D" (dummy1)
                        : "0" (count), "1" (dst), "a" (val)
                        : "memory", "cc");
   return dst;
}

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Div643232 --
 *
 *    Unsigned integer division:
 *       The dividend is 64-bit wide
 *       The divisor  is 32-bit wide
 *       The quotient is 32-bit wide
 *
 *    Use this function if you are certain that the quotient will fit in 32 bits,
 *    If that is not the case, a #DE exception was generated in 32-bit version,
 *    but not in this 64-bit version. So please be careful.
 *
 * Results:
 *    Quotient and remainder
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__) || defined(_MSC_VER)

static INLINE void
Div643232(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint32 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   *quotient = (uint32)(dividend / divisor);
   *remainder = (uint32)(dividend % divisor);
}

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Div643264 --
 *
 *    Unsigned integer division:
 *       The dividend is 64-bit wide
 *       The divisor  is 32-bit wide
 *       The quotient is 64-bit wide
 *
 * Results:
 *    Quotient and remainder
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__)

static INLINE void
Div643264(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint64 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   *quotient = dividend / divisor;
   *remainder = dividend % divisor;
}

#endif

#endif // _VM_BASIC_ASM_X86_64_H_
