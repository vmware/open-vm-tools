/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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
 * vm_basic_asm_x86_common.h --
 *
 *	Basic assembler macros common to 32-bit and 64-bit x86 ISA.
 */

#ifndef _VM_BASIC_ASM_X86_COMMON_H_
#define _VM_BASIC_ASM_X86_COMMON_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifndef VM_X86_ANY
#error "Should be included only in x86 builds"
#endif

/*
 * x86-64 windows doesn't support inline asm so we have to use these
 * intrinsic functions defined in the compiler.  Not all of these are well
 * documented.  There is an array in the compiler dll (c1.dll) which has
 * an array of the names of all the intrinsics minus the leading
 * underscore.  Searching around in the ntddk.h file can also be helpful.
 *
 * The declarations for the intrinsic functions were taken from the DDK.
 * Our declarations must match the ddk's otherwise the 64-bit c++ compiler
 * will complain about second linkage of the intrinsic functions.
 * We define the intrinsic using the basic types corresponding to the
 * Windows typedefs. This avoids having to include windows header files
 * to get to the windows types.
 */
#if defined(_MSC_VER) && !defined(BORA_NO_WIN32_INTRINS)
#ifdef __cplusplus
extern "C" {
#endif
/*
 * It seems x86 & x86-64 windows still implements these intrinsic
 * functions.  The documentation for the x86-64 suggest the
 * __inbyte/__outbyte intrinsics even though the _in/_out work fine and
 * __inbyte/__outbyte aren't supported on x86.
 */
int            _inp(unsigned short);
unsigned short _inpw(unsigned short);
unsigned long  _inpd(unsigned short);

int            _outp(unsigned short, int);
unsigned short _outpw(unsigned short, unsigned short);
unsigned long  _outpd(uint16, unsigned long);
#pragma intrinsic(_inp, _inpw, _inpd, _outp, _outpw, _outpw, _outpd)

/*
 * Prevents compiler from re-ordering reads, writes and reads&writes.
 * These functions do not add any instructions thus only affect
 * the compiler ordering.
 *
 * See:
 * `Lockless Programming Considerations for Xbox 360 and Microsoft Windows'
 * http://msdn.microsoft.com/en-us/library/bb310595(VS.85).aspx
 */
void _ReadBarrier(void);
void _WriteBarrier(void);
void _ReadWriteBarrier(void);
#pragma intrinsic(_ReadBarrier, _WriteBarrier, _ReadWriteBarrier)

void _mm_mfence(void);
void _mm_lfence(void);
#pragma intrinsic(_mm_mfence, _mm_lfence)

unsigned int __getcallerseflags(void);
#pragma intrinsic(__getcallerseflags)

#ifdef VM_X86_64
/*
 * intrinsic functions only supported by x86-64 windows as of 2k3sp1
 */
unsigned __int64 __rdtsc(void);
void             __stosw(unsigned short *, unsigned short, size_t);
void             __stosd(unsigned long *, unsigned long, size_t);
void             _mm_pause(void);
#pragma intrinsic(__rdtsc, __stosw, __stosd, _mm_pause)

unsigned char  _BitScanForward64(unsigned long *, unsigned __int64);
unsigned char  _BitScanReverse64(unsigned long *, unsigned __int64);
#pragma intrinsic(_BitScanForward64, _BitScanReverse64)
#endif /* VM_X86_64 */

unsigned char  _BitScanForward(unsigned long *, unsigned long);
unsigned char  _BitScanReverse(unsigned long *, unsigned long);
#pragma intrinsic(_BitScanForward, _BitScanReverse)

unsigned char  _bittest(const long *, long);
unsigned char  _bittestandset(long *, long);
unsigned char  _bittestandreset(long *, long);
unsigned char  _bittestandcomplement(long *, long);
#pragma intrinsic(_bittest, _bittestandset, _bittestandreset, _bittestandcomplement)
#ifdef VM_X86_64
unsigned char  _bittestandset64(__int64 *, __int64);
unsigned char  _bittestandreset64(__int64 *, __int64);
#pragma intrinsic(_bittestandset64, _bittestandreset64)
#endif // VM_X86_64
#ifdef __cplusplus
}
#endif
#endif // _MSC_VER

#ifdef __GNUC__
/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * volatile because reading from port can modify the state of the underlying
 * hardware.
 *
 * Note: The undocumented %z construct doesn't work (internal compiler error)
 *       with gcc-2.95.1
 */

#define __GCC_IN(s, type, name) \
static INLINE type              \
name(uint16 port)               \
{                               \
   type val;                    \
                                \
   __asm__ __volatile__(        \
      "in" #s " %w1, %0"        \
      : "=a" (val)              \
      : "Nd" (port)             \
   );                           \
                                \
   return val;                  \
}

__GCC_IN(b, uint8, INB)
__GCC_IN(w, uint16, INW)
__GCC_IN(l, uint32, IN32)


/*
 * Checked against the Intel manual and GCC --hpreg
 *
 * Note: The undocumented %z construct doesn't work (internal compiler error)
 *       with gcc-2.95.1
 */

#define __GCC_OUT(s, s2, port, val) do { \
   __asm__(                              \
      "out" #s " %" #s2 "1, %w0"         \
      :                                  \
      : "Nd" (port), "a" (val)           \
   );                                    \
} while (0)

#define OUTB(port, val) __GCC_OUT(b, b, port, val)
#define OUTW(port, val) __GCC_OUT(w, w, port, val)
#define OUT32(port, val) __GCC_OUT(l, , port, val)

#define GET_CURRENT_EIP(_eip) \
      __asm__ __volatile("call 0\n\tpopl %0" : "=r" (_eip): );

static INLINE unsigned int
GetCallerEFlags(void)
{
   unsigned long flags;
   asm volatile("pushf; pop %0" : "=r"(flags));
   return flags;
}

#elif defined(_MSC_VER)
static INLINE  uint8
INB(uint16 port)
{
   return (uint8)_inp(port);
}
static INLINE void
OUTB(uint16 port, uint8 value)
{
   _outp(port, value);
}
static INLINE uint16
INW(uint16 port)
{
   return _inpw(port);
}
static INLINE void
OUTW(uint16 port, uint16 value)
{
   _outpw(port, value);
}
static INLINE  uint32
IN32(uint16 port)
{
   return _inpd(port);
}
static INLINE void
OUT32(uint16 port, uint32 value)
{
   _outpd(port, value);
}

#ifndef VM_X86_64
#ifdef NEAR
#undef NEAR
#endif

#define GET_CURRENT_EIP(_eip) do { \
   __asm call NEAR PTR $+5 \
   __asm pop eax \
   __asm mov _eip, eax \
} while (0)
#endif // VM_X86_64

static INLINE unsigned int
GetCallerEFlags(void)
{
   return __getcallerseflags();
}

#endif // __GNUC__

/* Sequence recommended by Intel for the Pentium 4. */
#define INTEL_MICROCODE_VERSION() (             \
   __SET_MSR(MSR_BIOS_SIGN_ID, 0),              \
   __GET_EAX_FROM_CPUID(1),                     \
   __GET_MSR(MSR_BIOS_SIGN_ID))

/*
 *-----------------------------------------------------------------------------
 *
 * RDTSC_BARRIER --
 *
 *      Implements an RDTSC fence.  Instructions executed prior to the
 *      fence will have completed before the fence and all stores to
 *      memory are flushed from the store buffer.
 *
 *      On AMD, MFENCE is sufficient.  On Intel, only LFENCE is
 *      documented to fence RDTSC, but LFENCE won't drain the store
 *      buffer.  So, use MFENCE;LFENCE, which will work on both AMD and
 *      Intel.
 *
 *      It is the callers' responsibility to check for SSE2 before
 *      calling this function.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Cause loads and stores prior to this to be globally visible, and
 *      RDTSC will not pass.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
RDTSC_BARRIER(void)
{
#ifdef __GNUC__
   __asm__ __volatile__(
      "mfence \n\t"
      "lfence \n\t"
      ::: "memory"
   );
#elif defined _MSC_VER
   /* Prevent compiler from moving code across mfence/lfence. */
   _ReadWriteBarrier();
   _mm_mfence();
   _mm_lfence();
   _ReadWriteBarrier();
#else
#error No compiler defined for RDTSC_BARRIER
#endif
}


/*
 * Compiler/CPU barriers. These take the form of <mem access type>_<mem access
 * type>_MEM_BARRIER, where <mem access type> is either LD (load), ST (store) or
 * LDST (any). On x86 we only need to care specifically about store-load
 * reordering on normal memory types and mfence, otherwise only a compiler
 * barrier is needed.
 */
#define LD_LD_MEM_BARRIER()      COMPILER_MEM_BARRIER()
#define LD_ST_MEM_BARRIER()      COMPILER_MEM_BARRIER()
#define LD_LDST_MEM_BARRIER()    COMPILER_MEM_BARRIER()
#define ST_LD_MEM_BARRIER()      asm volatile ("mfence" ::: "memory")
#define ST_ST_MEM_BARRIER()      COMPILER_MEM_BARRIER()
#define ST_LDST_MEM_BARRIER()    ST_LD_MEM_BARRIER()
#define LDST_LD_MEM_BARRIER()    ST_LD_MEM_BARRIER()
#define LDST_ST_MEM_BARRIER()    COMPILER_MEM_BARRIER()
#define LDST_LDST_MEM_BARRIER()  ST_LD_MEM_BARRIER()

#endif // _VM_BASIC_ASM_X86_COMMON_H_
