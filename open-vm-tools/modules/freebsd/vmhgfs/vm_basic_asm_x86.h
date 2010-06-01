/*********************************************************
 * Copyright (C) 1998-2003 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

#ifdef VM_X86_64
/*
 * The gcc inline asm uses the "A" constraint which differs in 32 & 64
 * bit mode.  32 bit means eax and edx, 64 means rax or rdx.
 */
#error "x86-64 not supported"
#endif




/*
 * from linux: usr/include/asm/io.h
 */
#ifdef __GNUC__
#ifndef __SLOW_DOWN_IO
#ifdef SLOW_IO_BY_JUMPING
#define __SLOW_DOWN_IO __asm__ __volatile__("jmp 1f\n1:\tjmp 1f\n1:")
#else
#define __SLOW_DOWN_IO __asm__ __volatile__("outb %al,$0x80")
#endif
#endif
#elif defined _MSC_VER
#ifdef SLOW_IO_BY_JUMPING
#define __SLOW_DOWN_IO __asm jmp SHORT $+2 __asm  jmp SHORT $+2
#else
#define __SLOW_DOWN_IO __asm out 80h,al
#endif
#else
#error
#endif

#ifdef REALLY_SLOW_IO
#define SLOW_DOWN_IO { __SLOW_DOWN_IO; __SLOW_DOWN_IO; __SLOW_DOWN_IO; __SLOW_DOWN_IO; }
#else
#define SLOW_DOWN_IO __SLOW_DOWN_IO
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
FXSAVE_ES1(uint8 *save)
{
   __asm__ __volatile__ ("fxsave %0\n" : "=m" (*save) : : "memory");
}

static INLINE void 
FXRSTOR_ES1(const uint8 *load)
{
   __asm__ __volatile__ ("fxrstor %0\n" : : "m" (*load) : "memory");
}

static INLINE void 
FXRSTOR_AMD_ES0(const uint8 *load)
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
        : "m" (dummy), "m" (*load)
        : "ax", "memory");
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
 *    If that is not the case, then use Div643264(). --hpreg
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
   /* Checked against the Intel manual and GCC --hpreg */
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
   /* Written and tested by mann, checked by dbudko and hpreg */
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
 *       The quotient is 64-bit wide --hpreg
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

   /* Checked against the Intel manual and GCC --hpreg */
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
 *    Unsigned integer by fixed point multiplication:
 *       Unsigned 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Unsigned 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x32 bits to yield a full 96-bit product.
 *    Shift right by shift.
 *    Return the low-order 64 bits of the result.
 *
 * Result:
 *    Product
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__)

static INLINE uint64
Mul64x3264(uint64 multiplicand, uint32 multiplier, uint32 shift)
{
   uint64 result;
   uint32 tmp1, tmp2;
   // ASSERT(shift >= 0 && shift < 64);
  
   /*
    * Written and tested by mann, improved with suggestions by hpreg.
    *
    * The main improvement over the previous version is that the test
    * of shift against 32 is moved out of the asm and into C code.
    * This lets the compiler delete the test and one of the
    * alternative code sequences in the case where shift is a
    * constant.  It also lets us use the best code sequence in each
    * alternative, rather than a compromise.  The downside is that in
    * the non-constant case, this version takes slightly more code
    * space.
    *
    * Note on the constraints: We don't really want multiplicand to
    * start in %edx:%eax as the =A constraint dictates; in fact, we'd
    * prefer any *other* two registers.  But gcc doesn't have
    * constraint syntax for any other register pair, and trying to
    * constrain ((uint32) multiplicand) to one place and (multiplicand
    * >> 32) to another generates *really* bad code -- gcc is just not
    * smart enough, at least in the version we are currently using.
    */
   if (shift < 32) {
      __asm__("mov   %%eax, %2     \n\t" // Save lo(multiplicand) in tmp2
              "mov   %%edx, %%eax  \n\t" // Get hi(multiplicand)
              "mull  %4            \n\t" // p2 = hi(multiplicand) * multiplier
              "xchg  %%eax, %2     \n\t" // Save lo(p2) in tmp2, get lo(multiplicand)
              "mov   %%edx, %1     \n\t" // Save hi(p2) in tmp1
              "mull  %4            \n\t" // p1 = lo(multiplicand) * multiplier
              "addl  %2, %%edx     \n\t" // hi(p1) += lo(p2)
              "adcl  $0, %1        \n\t" // hi(p2) += carry from previous step
              "shrdl %%edx, %%eax  \n\t" // result = hi(p2):hi(p1):lo(p1) >> shift
              "shrdl %1, %%edx"
              : "=A"  (result),
                "=&r" (tmp1),            // use in shrdl requires it to be a register
                "=&r" (tmp2)             // could be "=&rm" but "m" is slower
              : "0"   (multiplicand),
                "rm"  (multiplier),
                "c"   (shift)
              : "cc"
         );
   } else {
      __asm__("mov   %%edx, %2     \n\t" // Save hi(multiplicand) in tmp2
              "mull  %4            \n\t" // p1 = lo(multiplicand) * multiplier
              "mov   %%edx, %1     \n\t" // Save hi(p1) in tmp1
              "mov   %2, %%eax     \n\t" // Discard lo(p1), get hi(multiplicand)
              "mull  %4            \n\t" // p2 = hi(multiplicand) * multiplier
              "addl  %1, %%eax     \n\t" // lo(p2) += hi(p1)
              "adcl  $0, %%edx     \n\t" // hi(p2) += carry from previous step
              "shrdl %%edx, %%eax  \n\t" // result = p2 >> (shift & 31)
              "shrl  %%cl, %%edx"
              : "=A"  (result),
                "=&r" (tmp1),            // could be "=&rm" but "m" is slower
                "=&r" (tmp2)             // could be "=&rm" but "m" is slower
              : "0"   (multiplicand),
                "rm"  (multiplier),
                "c"   (shift)
              : "cc"
         );
   }
   return result;
}

#elif defined _MSC_VER
#pragma warning(disable: 4035)

static INLINE uint64
Mul64x3264(uint64 multiplicand, uint32 multiplier, uint32 shift)
{
   // ASSERT(shift >= 0 && shift < 64);

   /* Written and tested by mann, checked by dbudko and hpreg */
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
      mov  eax, edx                         // result = hi(p2):hi(p1) >> (shift & 31)
      mov  edx, ebx
      shrd eax, edx, cl
      shr  edx, cl
      jmp  SHORT l3
   l2:
      shrd eax, edx, cl                     // result = hi(p2):hi(p1):lo(p1) >> shift
      shrd edx, ebx, cl
   l3:
   }
   // return with result in edx:eax
}

#pragma warning(default: 4035)
#else
#error No compiler defined for Mul64x3264
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Muls64x32s64 --
 *
 *    Signed integer by fixed point multiplication:
 *       Signed 64-bit integer multiplicand.
 *       Unsigned 32-bit fixed point multiplier, represented as
 *         multiplier >> shift, where shift < 64.
 *       Signed 64-bit integer product.
 *
 * Implementation:
 *    Multiply 64x32 bits to yield a full 96-bit product.
 *    Shift right by the location of the binary point.
 *    Return the low-order 64 bits of the result.
 *
 * Result:
 *    Product
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__)

static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   int64 result;
   uint32 tmp1, tmp2;
   // ASSERT(shift >= 0 && shift < 64);

   /* Written and tested by mann, checked by dbudko and hpreg */
   /* XXX hpreg suggested some improvements that we haven't converged on yet */
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
           "mov   %%edx, %%eax\n\t"   // result = hi(p2):hi(p1) >> (shift & 31)
           "mov   %1, %%edx\n\t"
           "shrdl %%edx, %%eax\n\t"
           "sarl  %%cl, %%edx\n\t"
           "jmp   3f\n"
        "2:\n\t"
           "shrdl %%edx, %%eax\n\t"   // result = hi(p2):hi(p1):lo(p1) >> shift
           "shrdl %1, %%edx\n"
        "3:\n\t"
           : "=A" (result), "=&r" (tmp1), "=&r" (tmp2)
           : "0" (multiplicand), "rm" (multiplier), "c" (shift)
           : "cc");
   return result;
}

#elif defined _MSC_VER
#pragma warning(disable: 4035)

static INLINE int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   //ASSERT(shift >= 0 && shift < 64);
  
   /* Written and tested by mann, checked by dbudko and hpreg */
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
      mov  eax, edx                         // result = hi(p2):hi(p1) >> (shift & 31)
      mov  edx, ebx
      shrd eax, edx, cl
      sar  edx, cl
      jmp  SHORT l3
   l2:
      shrd eax, edx, cl                     // result = hi(p2):hi(p1):lo(p1) << shift
      shrd edx, ebx, cl
   l3:
   }
   // return with result in edx:eax
}

#pragma warning(default: 4035)
#else
#error No compiler defined for Muls64x32s64
#endif


#endif
