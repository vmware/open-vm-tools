/*********************************************************
 * Copyright (C) 1998-2004 VMware, Inc. All rights reserved.
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

#ifdef _MSC_VER

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

/*
 * GET_CURRENT_RIP
 *
 * Return an approximation of the current instruction pointer. For example for a
 * function call
 * foo.c
 * L123: Foo(GET_CURRENT_RIP())
 *
 * The return value from GET_CURRENT_RIP will point a debugger to L123.
 */
#if defined(__GNUC__)
#define GET_CURRENT_RIP() ({                                                   \
      void *__rip;                                                             \
      asm("lea 0(%%rip), %0;\n\t"                                              \
         : "=r" (__rip));                                                      \
      __rip;                                                                   \
})
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
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
   __asm__ __volatile__ (
        ".byte 0x48, 0x0f, 0xae, 0x21 \n"
        :
        : "c" ((uint8 *)save), "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#else
   __asm__ __volatile__ (
        "xsaveq %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#endif
}

static INLINE void 
XSAVE_COMPAT_ES1(void *save, uint64 mask)
{
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
   __asm__ __volatile__ (
        ".byte 0x0f, 0xae, 0x21 \n"
        :
        : "c" ((uint8 *)save), "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#else
   __asm__ __volatile__ (
        "xsave %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#endif
}

static INLINE void 
XSAVEOPT_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        ".byte 0x48, 0x0f, 0xae, 0x31 \n"
        :
        : "c" ((uint8 *)save), "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void 
XRSTOR_ES1(const void *load, uint64 mask)
{
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
   __asm__ __volatile__ (
        ".byte 0x48, 0x0f, 0xae, 0x29 \n"
        :
        : "c" ((const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#else
   __asm__ __volatile__ (
        "xrstorq %0 \n"
        :
        : "m" (*(const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#endif
}

static INLINE void 
XRSTOR_COMPAT_ES1(const void *load, uint64 mask)
{
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
   __asm__ __volatile__ (
        ".byte 0x0f, 0xae, 0x29 \n"
        :
        : "c" ((const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#else
   __asm__ __volatile__ (
        "xrstor %0 \n"
        :
        : "m" (*(const uint8 *)load),
          "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
#endif
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
#if __GNUC__ < 4 || __GNUC__ == 4 && __GNUC_MINOR__ == 1
        ".byte 0x48, 0x0f, 0xae, 0x29 \n"
        :
        : "m" (dummy), "c" ((const uint8 *)load),
          "b" ((uint32)mask), "d" ((uint32)(mask >> 32))
#else
        "xrstorq %1 \n"
        :
        : "m" (dummy), "m" (*(const uint8 *)load),
          "b" ((uint32)mask), "d" ((uint32)(mask >> 32))
#endif
        : "eax", "memory");
}

#endif /* __GNUC__ */


/*
 *-----------------------------------------------------------------------------
 *
 * Mul64x6464 --
 *
 *    Unsigned integer by fixed point multiplication:
 *       result = multiplicand * multiplier >> shift
 * 
 *       Unsigned 64-bit integer multiplicand.
 *       Unsigned 64-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Unsigned 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x64 bits to yield a full 128-bit product.
 *    Shift result in RDX:RAX right by "shift".
 *    Return the low-order 64 bits of the above.
 *
 * Result:
 *    Product
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__)

static INLINE uint64
Mul64x6464(uint64 multiplicand,
           uint64 multiplier,
           uint32 shift)
{
   uint64 result, dummy;

   __asm__("mulq    %3      \n\t"
           "shrdq   %1, %0  \n\t"
           : "=a" (result),
             "=d" (dummy)
           : "0"  (multiplier),
             "rm" (multiplicand),
         "c"  (shift)
           : "cc");
   return result;
}

#elif defined(_MSC_VER)

static INLINE uint64
Mul64x6464(uint64 multiplicand, uint64 multiplier, uint32 shift)
{
   uint64 tmplo, tmphi;
   tmplo = _umul128(multiplicand, multiplier, &tmphi);
   return __shiftright128(tmplo, tmphi, (uint8) shift);
}

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Muls64x64s64 --
 *
 *    Signed integer by fixed point multiplication:
 *       result = multiplicand * multiplier >> shift
 * 
 *       Signed 64-bit integer multiplicand.
 *       Unsigned 64-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Signed 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x64 bits to yield a full 128-bit product.
 *    Shift result in RDX:RAX right by "shift".
 *    Return the low-order 64 bits of the above.
 *
 *    Note: using an unsigned shift instruction is correct because
 *    shift < 64 and we return only the low 64 bits of the shifted
 *    result.
 *
 * Result:
 *    Product
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__)

static inline int64
Muls64x64s64(int64 multiplicand, int64 multiplier, uint32 shift)
{
   int64 result, dummy;

   __asm__("imulq   %3      \n\t"
       "shrdq   %1, %0  \n\t"
       : "=a" (result),
         "=d" (dummy)
       : "0"  (multiplier),
         "rm" (multiplicand),
         "c"  (shift)
       : "cc");
   return result;
}

#elif defined(_MSC_VER)

static INLINE int64
Muls64x64s64(int64 multiplicand, int64 multiplier, uint32 shift)
{
   int64 tmplo, tmphi;

   tmplo = _mul128(multiplicand, multiplier, &tmphi);
   return __shiftright128(tmplo, tmphi, (uint8) shift);
}

#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Mul64x3264 --
 *
 *    Unsigned integer by fixed point multiplication:
 *       result = multiplicand * multiplier >> shift
 * 
 *       Unsigned 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Unsigned 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x64 bits to yield a full 128-bit product.
 *    Shift result in RDX:RAX right by "shift".
 *    Return the low-order 64 bits of the above.
 *
 * Result:
 *    Return the low-order 64 bits of ((multiplicand * multiplier) >> shift)
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
 *    Signed integer by fixed point multiplication:
 *       result = (multiplicand * multiplier) >> shift
 * 
 *       Signed 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Signed 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x64 bits to yield a full 128-bit product.
 *    Shift result in RDX:RAX right by "shift".
 *    Return the low-order 64 bits of the above.
 *
 * Result:
 *    Return the low-order 64 bits of ((multiplicand * multiplier) >> shift)
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   return Muls64x64s64(multiplicand, multiplier, shift);
}


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
