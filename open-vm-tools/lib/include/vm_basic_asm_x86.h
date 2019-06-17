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
 * vm_basic_asm_x86.h
 *
 *	Basic IA32 asm macros
 */

#ifndef _VM_BASIC_ASM_X86_H_
#define _VM_BASIC_ASM_X86_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined __cplusplus
extern "C" {
#endif


#ifdef VM_X86_64
/*
 * The gcc inline asm uses the "A" constraint which differs in 32 & 64
 * bit mode.  32 bit means eax and edx, 64 means rax or rdx.
 */
#error "x86-64 not supported"
#endif

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
   __asm__ __volatile__ ("fxsave %0\n" : "=m" (*(uint8 *)save) : : "memory");
}

static INLINE void 
FXRSTOR_ES1(const void *load)
{
   __asm__ __volatile__ ("fxrstor %0\n"
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
        "fxrstor %1      \n"
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
        "xsave %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void 
XSAVEOPT_ES1(void *save, uint64 mask)
{
   __asm__ __volatile__ (
        "xsaveopt %0 \n"
        : "=m" (*(uint8 *)save)
        : "a" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "memory");
}

static INLINE void 
XRSTOR_ES1(const void *load, uint64 mask)
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
        "xrstor %1 \n"
        :
        : "m" (dummy), "m" (*(const uint8 *)load),
          "b" ((uint32)mask), "d" ((uint32)(mask >> 32))
        : "eax", "memory");
}
#endif /* __GNUC__ */

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
 *    Use this function if you are certain that:
 *    o Either the quotient will fit in 32 bits,
 *    o Or your code is ready to handle a #DE exception indicating overflow.
 *    If that is not the case, then use Div643264().
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
Div643232(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint32 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   __asm__(
      "divl %4"
      : "=a" (*quotient),
        "=d" (*remainder)
      : "0" ((uint32)dividend),
        "1" ((uint32)(dividend >> 32)),
        "rm" (divisor)
      : "cc"
   );
}

#elif defined _MSC_VER

static INLINE void
Div643232(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint32 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   __asm {
      mov  eax, DWORD PTR [dividend]
      mov  edx, DWORD PTR [dividend+4]
      div  DWORD PTR [divisor]
      mov  edi, DWORD PTR [quotient]
      mov  [edi], eax
      mov  edi, DWORD PTR [remainder]
      mov  [edi], edx
   }
}

#else
#error No compiler defined for Div643232
#endif


#if defined(__GNUC__)
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

static INLINE void
Div643264(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint64 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   uint32 hQuotient;
   uint32 lQuotient;

   __asm__(
      "divl %5"        "\n\t"
      "movl %%eax, %0" "\n\t"
      "movl %4, %%eax" "\n\t"
      "divl %5"
      : "=&rm" (hQuotient),
        "=a" (lQuotient),
        "=d" (*remainder)
      : "1" ((uint32)(dividend >> 32)),
        "g" ((uint32)dividend),
        "rm" (divisor),
        "2" (0)
      : "cc"
   );
   *quotient = (uint64)hQuotient << 32 | lQuotient;
}
#endif


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

#if defined(__GNUC__) && \
   (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)) && \
   !defined(MUL64_NO_ASM)

static INLINE uint64
Mul64x3264(uint64 multiplicand, uint32 multiplier, uint32 shift)
{
   uint64 result;
   uint32 tmp1, tmp2;
   // ASSERT(shift >= 0 && shift < 64);
  
   __asm__("mov   %%eax, %2\n\t"      // Save lo(multiplicand)
           "mov   %%edx, %%eax\n\t"   // Get hi(multiplicand)
           "mull  %4\n\t"             // p2 = hi(multiplicand) * multiplier
           "xchg  %%eax, %2\n\t"      // Save lo(p2), get lo(multiplicand)
           "mov   %%edx, %1\n\t"      // Save hi(p2)
           "mull  %4\n\t"             // p1 = lo(multiplicand) * multiplier
           "addl  %2, %%edx\n\t"      // hi(p1) += lo(p2)
           "adcl  $0, %1\n\t"         // hi(p2) += carry from previous step
           "cmpl  $32, %%ecx\n\t"     // shift < 32?
           "jl    2f\n\t"             // Go if so
           "shll  $1, %%eax\n\t"      // Save lo(p1) bit 31 in CF in case shift=32
           "mov   %%edx, %%eax\n\t"   // result = hi(p2):hi(p1) >> (shift & 31)
           "mov   %1, %%edx\n\t"
           "shrdl %%edx, %%eax\n\t"
           "mov   $0, %2\n\t"
           "adcl  $0, %2\n\t"         // Get highest order bit shifted out, from CF
           "shrl  %%cl, %%edx\n\t"
           "jmp   3f\n"
        "2:\n\t"
           "xor   %2, %2\n\t"
           "shrdl %%edx, %%eax\n\t"   // result = hi(p2):hi(p1):lo(p1) >> shift
           "adcl  $0, %2\n\t"         // Get highest order bit shifted out, from CF
           "shrdl %1, %%edx\n"
        "3:\n\t"
           "addl  %2, %%eax\n\t"      // result += highest order bit shifted out
           "adcl  $0, %%edx"
           : "=A" (result), "=&r" (tmp1), "=&r" (tmp2)
           : "0" (multiplicand), "rm" (multiplier), "c" (shift)
           : "cc");
   return result;
}

#elif defined _MSC_VER
#pragma warning(disable: 4035)

static INLINE uint64
Mul64x3264(uint64 multiplicand, uint32 multiplier, uint32 shift)
{
   // ASSERT(shift >= 0 && shift < 64);

   __asm {
      mov  eax, DWORD PTR [multiplicand+4]  // Get hi(multiplicand)
      mul  DWORD PTR [multiplier]           // p2 = hi(multiplicand) * multiplier
      mov  ecx, eax                         // Save lo(p2)
      mov  ebx, edx                         // Save hi(p2)
      mov  eax, DWORD PTR [multiplicand]    // Get lo(multiplicand)
      mul  DWORD PTR [multiplier+0]         // p1 = lo(multiplicand) * multiplier
      add  edx, ecx                         // hi(p1) += lo(p2)
      adc  ebx, 0                           // hi(p2) += carry from previous step
      mov  ecx, DWORD PTR [shift]           // Get shift
      cmp  ecx, 32                          // shift < 32?
      jl   SHORT l2                         // Go if so
      shl  eax, 1                           // Save lo(p1) bit 31 in CF in case shift=32
      mov  eax, edx                         // result = hi(p2):hi(p1) >> (shift & 31)
      mov  edx, ebx
      shrd eax, edx, cl
      mov  esi, 0
      adc  esi, 0                           // Get highest order bit shifted out, from CF
      shr  edx, cl
      jmp  SHORT l3
   l2:
      Xor  esi, esi
      shrd eax, edx, cl                     // result = hi(p2):hi(p1):lo(p1) >> shift
      adc  esi, 0                           // Get highest order bit shifted out, from CF
      shrd edx, ebx, cl
   l3:
      add  eax, esi                         // result += highest order bit shifted out
      adc  edx, 0
   }
   // return with result in edx:eax
}

#pragma warning(default: 4035)
#else
#define MUL64_NO_ASM 1
#include "mul64.h"
#endif

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

#if defined(__GNUC__) && \
   (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)) && \
   !defined(MUL64_NO_ASM)

static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   int64 result;
   uint32 tmp1, tmp2;
   // ASSERT(shift >= 0 && shift < 64);

   __asm__("mov   %%eax, %2\n\t"      // Save lo(multiplicand)
           "mov   %%edx, %%eax\n\t"   // Get hi(multiplicand)
           "test  %%eax, %%eax\n\t"   // Check sign of multiplicand
           "jl    0f\n\t"             // Go if negative
           "mull  %4\n\t"             // p2 = hi(multiplicand) * multiplier
           "jmp   1f\n"
        "0:\n\t"
           "mull  %4\n\t"             // p2 = hi(multiplicand) * multiplier
           "sub   %4, %%edx\n"        // hi(p2) += -1 * multiplier
        "1:\n\t"
           "xchg  %%eax, %2\n\t"      // Save lo(p2), get lo(multiplicand)
           "mov   %%edx, %1\n\t"      // Save hi(p2)
           "mull  %4\n\t"             // p1 = lo(multiplicand) * multiplier
           "addl  %2, %%edx\n\t"      // hi(p1) += lo(p2)
           "adcl  $0, %1\n\t"         // hi(p2) += carry from previous step
           "cmpl  $32, %%ecx\n\t"     // shift < 32?
           "jl    2f\n\t"             // Go if so
           "shll  $1, %%eax\n\t"      // Save lo(p1) bit 31 in CF in case shift=32
           "mov   %%edx, %%eax\n\t"   // result = hi(p2):hi(p1) >> (shift & 31)
           "mov   %1, %%edx\n\t"
           "shrdl %%edx, %%eax\n\t"
           "mov   $0, %2\n\t"
           "adcl  $0, %2\n\t"         // Get highest order bit shifted out from CF
           "sarl  %%cl, %%edx\n\t"
           "jmp   3f\n"
        "2:\n\t"
           "xor   %2, %2\n\t"
           "shrdl %%edx, %%eax\n\t"   // result = hi(p2):hi(p1):lo(p1) >> shift
           "adcl  $0, %2\n\t"         // Get highest order bit shifted out from CF
           "shrdl %1, %%edx\n"
        "3:\n\t"
           "addl  %2, %%eax\n\t"      // result += highest order bit shifted out
           "adcl  $0, %%edx"
           : "=A" (result), "=&r" (tmp1), "=&rm" (tmp2)
           : "0" (multiplicand), "rm" (multiplier), "c" (shift)
           : "cc");
   return result;
}

#elif defined(_MSC_VER)
#pragma warning(disable: 4035)

static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   //ASSERT(shift >= 0 && shift < 64);
  
   __asm {
      mov  eax, DWORD PTR [multiplicand+4]  // Get hi(multiplicand)
      test eax, eax                         // Check sign of multiplicand
      jl   SHORT l0                         // Go if negative
      mul  DWORD PTR [multiplier]           // p2 = hi(multiplicand) * multiplier
      jmp  SHORT l1
   l0:
      mul  DWORD PTR [multiplier]           // p2 = hi(multiplicand) * multiplier
      sub  edx, DWORD PTR [multiplier]      // hi(p2) += -1 * multiplier
   l1:
      mov  ecx, eax                         // Save lo(p2)
      mov  ebx, edx                         // Save hi(p2)
      mov  eax, DWORD PTR [multiplicand]    // Get lo(multiplicand)
      mul  DWORD PTR [multiplier]           // p1 = lo(multiplicand) * multiplier
      add  edx, ecx                         // hi(p1) += lo(p2)
      adc  ebx, 0                           // hi(p2) += carry from previous step
      mov  ecx, DWORD PTR [shift]           // Get shift
      cmp  ecx, 32                          // shift < 32?
      jl   SHORT l2                         // Go if so
      shl  eax, 1                           // Save lo(p1) bit 31 in CF in case shift=32
      mov  eax, edx                         // result = hi(p2):hi(p1) >> (shift & 31)
      mov  edx, ebx
      shrd eax, edx, cl
      mov  esi, 0
      adc  esi, 0                           // Get highest order bit shifted out, from CF
      sar  edx, cl
      jmp  SHORT l3
   l2:
      Xor  esi, esi
      shrd eax, edx, cl                     // result = hi(p2):hi(p1):lo(p1) << shift
      adc  esi, 0                           // Get highest order bit shifted out, from CF
      shrd edx, ebx, cl
   l3:
      add  eax, esi                         // result += highest order bit shifted out
      adc  edx, 0
   }
   // return with result in edx:eax
}

#pragma warning(default: 4035)
#endif


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VM_BASIC_ASM_X86_H_
