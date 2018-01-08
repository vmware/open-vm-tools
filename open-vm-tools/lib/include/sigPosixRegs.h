/*********************************************************
 * Copyright (C) 2009-2017 VMware, Inc. All rights reserved.
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
 * sigPosixRegs.h --
 *
 *      Platform-specific definitions for saved CPU registers inside
 *      ucontext_t. These aren't part of sigPosix.h since few source
 *      files need them, and this header is a bit invasive, and it must
 *      be defined before system headers.
 */

#ifndef _SIGPOSIXREGS_H_
#define _SIGPOSIXREGS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if __linux__         // We need the REG_foo offsets in the gregset_t;
#  define _GNU_SOURCE // _GNU_SOURCE maps to __USE_GNU

/* And, the REG_foo definitions conflict with our own in x86.h */
#  if defined(__x86_64__)
#    define REG_RAX GNU_REG_RAX
#    define REG_RBX GNU_REG_RBX
#    define REG_RCX GNU_REG_RCX
#    define REG_RDX GNU_REG_RDX
#    define REG_RSI GNU_REG_RSI
#    define REG_RDI GNU_REG_RDI
#    define REG_RSP GNU_REG_RSP
#    define REG_RBP GNU_REG_RBP
#    define REG_RIP GNU_REG_RIP
#    define REG_R8  GNU_REG_R8
#    define REG_R9  GNU_REG_R9
#    define REG_R10 GNU_REG_R10
#    define REG_R11 GNU_REG_R11
#    define REG_R12 GNU_REG_R12
#    define REG_R13 GNU_REG_R13
#    define REG_R14 GNU_REG_R14
#    define REG_R15 GNU_REG_R15
#  elif defined(__i386__)
#    define REG_EAX GNU_REG_EAX
#    define REG_EBX GNU_REG_EBX
#    define REG_ECX GNU_REG_ECX
#    define REG_EDX GNU_REG_EDX
#    define REG_ESI GNU_REG_ESI
#    define REG_EDI GNU_REG_EDI
#    define REG_ESP GNU_REG_ESP
#    define REG_EBP GNU_REG_EBP
#    define REG_EIP GNU_REG_EIP
#  endif
#endif

#include <signal.h>
#include <sys/ucontext.h>

#if __linux__ && !defined __ANDROID__
#  if defined(__x86_64__)
#    undef REG_RAX
#    undef REG_RBX
#    undef REG_RCX
#    undef REG_RDX
#    undef REG_RSI
#    undef REG_RDI
#    undef REG_RSP
#    undef REG_RBP
#    undef REG_RIP
#    undef REG_R8
#    undef REG_R9
#    undef REG_R10
#    undef REG_R11
#    undef REG_R12
#    undef REG_R13
#    undef REG_R14
#    undef REG_R15
#  elif defined(__i386__)
#    undef REG_EAX
#    undef REG_EBX
#    undef REG_ECX
#    undef REG_EDX
#    undef REG_ESI
#    undef REG_EDI
#    undef REG_ESP
#    undef REG_EBP
#    undef REG_EIP
#  endif
#endif

#if defined(__APPLE__)
#if __DARWIN_UNIX03
#ifdef __x86_64__
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rax)
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rbx)
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rcx)
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rdx)
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rdi)
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rsi)
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rbp)
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rsp)
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__rip)
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r8)
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r9)
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r10)
#define SC_R11(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r11)
#define SC_R12(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r12)
#define SC_R13(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r13)
#define SC_R14(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r14)
#define SC_R15(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r15)
#elif defined(__arm__)
#define SC_R0(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[0])
#define SC_R1(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[1])
#define SC_R2(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[2])
#define SC_R3(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[3])
#define SC_R4(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[4])
#define SC_R5(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[5])
#define SC_R6(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[6])
#define SC_R7(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[7])
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[8])
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[9])
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__r[10])
#define SC_FP(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[11])
#define SC_IP(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__r[12])
#define SC_SP(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__sp)
#define SC_LR(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__lr)
#define SC_PC(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__pc)
#elif defined(__aarch64__)
#define SC_X(uc,n) ((unsigned long) (uc)->uc_mcontext->__ss.__x[n])
#define SC_SP(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__sp)
#define SC_PC(uc)  ((unsigned long) (uc)->uc_mcontext->__ss.__pc)
#define SC_PSR(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__cpsr)
#else
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__eax)
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__ebx)
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__ecx)
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__edx)
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__edi)
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__esi)
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__ebp)
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__esp)
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext->__ss.__eip)
#endif
#else
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext->ss.eax)
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext->ss.ebx)
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext->ss.ecx)
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext->ss.edx)
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext->ss.edi)
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext->ss.esi)
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext->ss.ebp)
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext->ss.esp)
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext->ss.eip)
#endif
#elif defined(__FreeBSD__)
#ifdef __x86_64__
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.mc_rax)
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.mc_rbx)
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.mc_rcx)
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.mc_rdx)
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.mc_rdi)
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.mc_rsi)
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.mc_rbp)
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.mc_rsp)
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.mc_rip)
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext.mc_r8)
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext.mc_r9)
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext.mc_r10)
#define SC_R11(uc) ((unsigned long) (uc)->uc_mcontext.mc_r11)
#define SC_R12(uc) ((unsigned long) (uc)->uc_mcontext.mc_r12)
#define SC_R13(uc) ((unsigned long) (uc)->uc_mcontext.mc_r13)
#define SC_R14(uc) ((unsigned long) (uc)->uc_mcontext.mc_r14)
#define SC_R15(uc) ((unsigned long) (uc)->uc_mcontext.mc_r15)
#else
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.mc_eax)
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.mc_ebx)
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.mc_ecx)
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.mc_edx)
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.mc_edi)
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.mc_esi)
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.mc_ebp)
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.mc_esp)
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.mc_eip)
#endif
#elif defined (sun)
#ifdef __x86_64__
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RAX])
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RBX])
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RCX])
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RDX])
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RDI])
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RSI])
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RBP])
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RSP])
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_RIP])
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext.gregs[REG_R8])
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext.gregs[REG_R9])
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R10])
#define SC_R11(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R11])
#define SC_R12(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R12])
#define SC_R13(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R13])
#define SC_R14(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R14])
#define SC_R15(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_R15])
#else
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EAX])
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EBX])
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[ECX])
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EDX])
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EDI])
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[ESI])
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EBP])
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[ESP])
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[EIP])
#endif
#elif defined(ANDROID_X86)
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EAX])
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EBX])
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_ECX])
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EDX])
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EDI])
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_ESI])
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EBP])
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_ESP])
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[REG_EIP])
#else
#ifdef __x86_64__
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RAX])
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RBX])
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RCX])
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RDX])
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RDI])
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RSI])
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RBP])
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RSP])
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_RIP])
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R8])
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R9])
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R10])
#define SC_R11(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R11])
#define SC_R12(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R12])
#define SC_R13(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R13])
#define SC_R14(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R14])
#define SC_R15(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_R15])
#elif defined(__arm__)
#define SC_R0(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r0)
#define SC_R1(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r1)
#define SC_R2(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r2)
#define SC_R3(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r3)
#define SC_R4(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r4)
#define SC_R5(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r5)
#define SC_R6(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r6)
#define SC_R7(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r7)
#define SC_R8(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r8)
#define SC_R9(uc)  ((unsigned long) (uc)->uc_mcontext.arm_r9)
#define SC_R10(uc) ((unsigned long) (uc)->uc_mcontext.arm_r10)
#define SC_FP(uc)  ((unsigned long) (uc)->uc_mcontext.arm_fp)
#define SC_IP(uc)  ((unsigned long) (uc)->uc_mcontext.arm_ip)
#define SC_SP(uc)  ((unsigned long) (uc)->uc_mcontext.arm_sp)
#define SC_LR(uc)  ((unsigned long) (uc)->uc_mcontext.arm_lr)
#define SC_PC(uc)  ((unsigned long) (uc)->uc_mcontext.arm_pc)
#elif defined(__aarch64__)
#define SC_X(uc,n) ((unsigned long) (uc)->uc_mcontext.regs[n])
#define SC_SP(uc)  ((unsigned long) (uc)->uc_mcontext.sp)
#define SC_PC(uc)  ((unsigned long) (uc)->uc_mcontext.pc)
#define SC_PSR(uc) ((unsigned long) (uc)->uc_mcontext.pstate)
#else
#define SC_EAX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EAX])
#define SC_EBX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EBX])
#define SC_ECX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_ECX])
#define SC_EDX(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EDX])
#define SC_EDI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EDI])
#define SC_ESI(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_ESI])
#define SC_EBP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EBP])
#define SC_ESP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_ESP])
#define SC_EIP(uc) ((unsigned long) (uc)->uc_mcontext.gregs[GNU_REG_EIP])
#endif
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _SIGPOSIXREGS_H_
