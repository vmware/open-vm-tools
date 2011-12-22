/*********************************************************
 * Copyright (C) 2003-2009 VMware, Inc. All rights reserved.
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
 * x86cpuid_asm.h
 *
 *	CPUID-related assembly functions.
 */

#ifndef _X86CPUID_ASM_H_
#define _X86CPUID_ASM_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_asm.h"
#include "x86cpuid.h"


#ifdef __GNUC__ // {

/*
 * Checked against the Intel manual and GCC --hpreg
 * 
 * Need __volatile__ and "memory" since CPUID has a synchronizing effect.
 * The CPUID may also change at runtime (APIC flag, etc).
 *
 */

/*
 * %ebx is reserved on i386 PIC.  Apple's gcc-5493 (gcc 4.0) compiling
 * for x86_64 incorrectly errors out saying %ebx is reserved.  This is
 * Apple bug 7304232.
 */
#if vm_x86_64 ? (defined __APPLE_CC__ && __APPLE_CC__ == 5493) : defined __PIC__
#if vm_x86_64
/*
 * Note that this generates movq %rbx,%rbx; cpuid; xchgq %rbx,%rbx ...
 * Unfortunately Apple's assembler does not have .ifnes, and I cannot
 * figure out how to do that with .if.   If we ever enable this code
 * on other 64bit systems, both movq & xchgq should be surrounded by
 * .ifnes \"%%rbx\", \"%q1\" & .endif
 */
#define VM_CPUID_BLOCK  "movq %%rbx, %q1\n\t" \
                        "cpuid\n\t"           \
                        "xchgq %%rbx, %q1\n\t"
#define VM_EBX_OUT(reg) "=&r"(reg)
#else
#define VM_CPUID_BLOCK  "movl %%ebx, %1\n\t" \
                        "cpuid\n\t"          \
                        "xchgl %%ebx, %1\n\t"
#define VM_EBX_OUT(reg) "=&rm"(reg)
#endif
#else
#define VM_CPUID_BLOCK  "cpuid"
#define VM_EBX_OUT(reg) "=b"(reg)
#endif

static INLINE void
__GET_CPUID(int eax,         // IN
            CPUIDRegs *regs) // OUT
{
   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (regs->eax), VM_EBX_OUT(regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
      : "a" (eax)
      : "memory"
   );
}

static INLINE void
__GET_CPUID2(int eax,         // IN
             int ecx,         // IN
             CPUIDRegs *regs) // OUT
{
   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (regs->eax), VM_EBX_OUT(regs->ebx), "=c" (regs->ecx), "=d" (regs->edx)
      : "a" (eax), "c" (ecx)
      : "memory"
   );
}

static INLINE uint32
__GET_EAX_FROM_CPUID(int eax) // IN
{
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx)
      : "a" (eax)
      : "memory", "%ecx", "%edx"
   );

   return eax;
}

static INLINE uint32
__GET_EBX_FROM_CPUID(int eax) // IN
{
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx)
      : "a" (eax)
      : "memory", "%ecx", "%edx"
   );

   return ebx;
}

static INLINE uint32
__GET_ECX_FROM_CPUID(int eax) // IN
{
   uint32 ecx;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=c" (ecx)
      : "a" (eax)
      : "memory", "%edx"
   );

   return ecx;
}

static INLINE uint32
__GET_EDX_FROM_CPUID(int eax) // IN
{
   uint32 edx;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=d" (edx)
      : "a" (eax)
      : "memory", "%ecx"
   );

   return edx;
}


static INLINE uint32
__GET_EAX_FROM_CPUID4(int ecx) // IN
{
   uint32 eax;
   uint32 ebx;

   __asm__ __volatile__(
      VM_CPUID_BLOCK
      : "=a" (eax), VM_EBX_OUT(ebx), "=c" (ecx)
      : "a" (4), "c" (ecx)
      : "memory", "%edx"
   );

   return eax;
}

#undef VM_CPUID_BLOCK
#undef VM_EBX_OUT

#elif defined(_MSC_VER) // } {

static INLINE void
__GET_CPUID(int input, CPUIDRegs *regs)
{
   __cpuid((int *)regs, input);
}

static INLINE void
__GET_CPUID2(int inputEax, int inputEcx, CPUIDRegs *regs)
{
   __cpuidex((int *)regs, inputEax, inputEcx);
}

static INLINE uint32
__GET_EAX_FROM_CPUID(int input)
{
   CPUIDRegs regs;
   __cpuid((int *)&regs, input);
   return regs.eax;
}

static INLINE uint32
__GET_EBX_FROM_CPUID(int input)
{
   CPUIDRegs regs;
   __cpuid((int *)&regs, input);
   return regs.ebx;
}

static INLINE uint32
__GET_ECX_FROM_CPUID(int input)
{
   CPUIDRegs regs;
   __cpuid((int *)&regs, input);
   return regs.ecx;
}

static INLINE uint32
__GET_EDX_FROM_CPUID(int input)
{
   CPUIDRegs regs;
   __cpuid(( int *)&regs, input);
   return regs.edx;
}

static INLINE uint32
__GET_EAX_FROM_CPUID4(int inputEcx)
{
   CPUIDRegs regs;
   __GET_CPUID2(4, inputEcx, &regs);
   return regs.eax;
}

#else // }
#error 
#endif

#define CPUID_FOR_SIDE_EFFECTS() ((void)__GET_EAX_FROM_CPUID(0))

static INLINE void
__GET_CPUID4(int inputEcx, CPUIDRegs *regs)
{
   __GET_CPUID2(4, inputEcx, regs);
}

/* The first parameter is used as an rvalue and then as an lvalue. */
#define GET_CPUID(_ax, _bx, _cx, _dx) { \
   CPUIDRegs regs;                      \
   __GET_CPUID(_ax, &regs);             \
   _ax = regs.eax;                      \
   _bx = regs.ebx;                      \
   _cx = regs.ecx;                      \
   _dx = regs.edx;                      \
}


#endif
