/*********************************************************
 * Copyright (C) 2013-2019 VMware, Inc. All rights reserved.
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

long _InterlockedXor(long volatile *, long);
#pragma intrinsic(_InterlockedXor)

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
   X86MSR_SetMSR(MSR_BIOS_SIGN_ID, 0),          \
   __GET_EAX_FROM_CPUID(1),                     \
   X86MSR_GetMSR(MSR_BIOS_SIGN_ID))


/*
 *-----------------------------------------------------------------------------
 *
 * MFENCE --
 *
 *      Wrapper around the MFENCE instruction.
 *
 *      Caveat Emptor! This function is _NOT_ _PORTABLE_ and most certainly
 *      not something you should use. Take a look at the SMP_*_BARRIER_*,
 *      DMA_*_BARRIER_* and MMIO_*_BARRIER_* interfaces instead, when writing
 *      general OS/VMM code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See MFENCE instruction in Intel SDM or AMD Programmer's Manual.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MFENCE(void)
{
#ifdef __GNUC__
   __asm__ __volatile__(
      "mfence"
      ::: "memory"
   );
#elif defined _MSC_VER
   _ReadWriteBarrier();
   _mm_mfence();
   _ReadWriteBarrier();
#else
#error No compiler defined for MFENCE
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * LFENCE --
 *
 *      Wrapper around the LFENCE instruction.
 *
 *      Caveat Emptor! This function is _NOT_ _PORTABLE_ and most certainly
 *      not something you should use. Take a look at the SMP_*_BARRIER_*,
 *      DMA_*_BARRIER_* and MMIO_*_BARRIER_* interfaces instead, when writing
 *      general OS/VMM code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See LFENCE instruction in Intel SDM or AMD Programmer's Manual.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
LFENCE(void)
{
#ifdef __GNUC__
   __asm__ __volatile__(
      "lfence"
      : : : "memory"
   );
#elif defined _MSC_VER
   _ReadWriteBarrier();
   _mm_lfence();
   _ReadWriteBarrier();
#else
#error No compiler defined for LFENCE
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * SFENCE --
 *
 *      Wrapper around the SFENCE instruction.
 *
 *      Caveat Emptor! This function is _NOT_ _PORTABLE_ and most certainly
 *      not something you should use. Take a look at the SMP_*_BARRIER_*,
 *      DMA_*_BARRIER_* and MMIO_*_BARRIER_* interfaces instead, when writing
 *      general OS/VMM code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See SFENCE instruction in Intel SDM or AMD Programmer's Manual.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
SFENCE(void)
{
#ifdef __GNUC__
   __asm__ __volatile__(
      "sfence"
      : : : "memory"
   );
#elif defined _MSC_VER
   _ReadWriteBarrier();
#if defined VM_X86_32
   __asm sfence;
#else
   _mm_sfence();
#endif
   _ReadWriteBarrier();
#else
#error No compiler defined for SFENCE
#endif
}


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
   MFENCE();
   LFENCE();
}


/*
 *-----------------------------------------------------------------------------
 *
 * LOCKED_INSN_BARRIER --
 *
 *      Implements a full WB load/store barrier using a locked instruction.
 *
 *      See PR 1674199 for details. You may choose to use this for
 *      performance reasons over MFENCE iff you are only dealing with
 *      WB memory accesses.
 *
 *      DANGER! Do not use this barrier instead of MFENCE when dealing
 *      with non-temporal instructions or UC/WC memory accesses.
 *
 *      Caveat Emptor! This function is _NOT_ _PORTABLE_ and most certainly
 *      not something you should use. Take a look at the SMP_*_BARRIER_*,
 *      DMA_*_BARRIER_* and MMIO_*_BARRIER_* interfaces instead, when writing
 *      general OS/VMM code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Cause WB loads and stores before the call to be globally visible
 *      before WB loads and stores after this call.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
LOCKED_INSN_BARRIER(void)
{
   volatile long temp = 0;

#if defined __GNUC__
   __asm__ __volatile__ (
      "lock xorl $1, %0"
      : "+m" (temp)
      : /* no additional inputs */
      : "cc", "memory");
#elif defined _MSC_VER
   /*
    * Ignore warning about _InterlockedXor operation on a local variable; we are
    * using the operation for its side-effects only.
    */
   #pragma warning(suppress:28113)
   _InterlockedXor(&temp, 1);
#else
#error LOCKED_INSN_BARRIER not defined for this compiler
#endif
}

/*
 * Memory Barriers
 * ===============
 *
 *    Terminology
 *    -----------
 *
 * A compiler memory barrier prevents the compiler from re-ordering memory
 * accesses accross the barrier. It is not a CPU instruction, it is a compiler
 * directive (i.e. it does not emit any code).
 *
 * A CPU memory barrier prevents the CPU from re-ordering memory accesses
 * accross the barrier. It is a CPU instruction.
 *
 * A memory barrier is the union of a compiler memory barrier and a CPU memory
 * barrier. A compiler memory barrier is a useless construct by itself. It is
 * only useful when combined with a CPU memory barrier, to implement a memory
 * barrier.
 *
 *    Semantics
 *    ---------
 *
 * At the time COMPILER_*_BARRIER were created (and references to them were
 * added to the code), the code was only targetting x86. The intent of the code
 * was really to use a memory barrier, but because x86 uses a strongly ordered
 * memory model, the CPU would not re-order memory accesses, and the code could
 * get away with using just a compiler memory barrier. So COMPILER_*_BARRIER
 * were born and were implemented as compiler memory barriers _on x86_. But
 * make no mistake, _the semantics that the code expects from
 * COMPILER_*_BARRIER are that of a memory barrier_!
 *
 *    DO NOT USE!
 *    -----------
 *
 * On at least one non-x86 architecture, COMPILER_*_BARRIER are
 * 1) Misnomers
 * 2) Not fine-grained enough to provide the best performance.
 * For the above two reasons, usage of COMPILER_*_BARRIER is now deprecated.
 * _Do not add new references to COMPILER_*_BARRIER._ Instead, precisely
 * document the intent of your code by using
 * <mem_type/purpose>_<before_access_type>_BARRIER_<after_access_type>.
 * Existing references to COMPILER_*_BARRIER are being slowly but surely
 * converted, and when no references are left, COMPILER_*_BARRIER will be
 * retired.
 *
 * Thanks for pasting this whole comment into every architecture header.
 */

#if defined __GNUC__
#   define COMPILER_READ_BARRIER()  COMPILER_MEM_BARRIER()
#   define COMPILER_WRITE_BARRIER() COMPILER_MEM_BARRIER()
#   define COMPILER_MEM_BARRIER()   __asm__ __volatile__("" ::: "memory")
#elif defined _MSC_VER
#   define COMPILER_READ_BARRIER()  _ReadBarrier()
#   define COMPILER_WRITE_BARRIER() _WriteBarrier()
#   define COMPILER_MEM_BARRIER()   _ReadWriteBarrier()
#endif

/*
 * Memory barriers. These take the form of
 *
 * <mem_type/purpose>_<before_access_type>_BARRIER_<after_access_type>
 *
 * where:
 *   <mem_type/purpose> is either SMP, DMA, or MMIO.
 *   <*_access type> is either R(load), W(store) or RW(any).
 *
 * Above every use of these memory barriers in the code, there _must_ be a
 * comment to justify the use, i.e. a comment which:
 *
 * 1) Precisely identifies which memory accesses must not be re-ordered across
 *    the memory barrier.
 * 2) Explains why it is important that the memory accesses not be re-ordered.
 *
 * Thanks for pasting this whole comment into every architecture header.
 *
 * This is a simplified version of Table 7-3 (Memory Access Ordering Rules) from
 * AMD AMD64 Architecture Programmer's Manual Volume 2: System Programming
 * (September 2018, Publication 24593, Revision 3.30).
 *
 * https://www.amd.com/system/files/TechDocs/24593.pdf#page=228
 *
 * This table only includes the memory types we care about in the context of
 * SMP, DMA and MMIO barriers.
 *
 * +-------------+------+------+------+------+------+------+
 * |\ 2nd mem op |      |      |      |      |      |      |
 * | `---------. | R WB | R UC | R WC | W WB | W UC | W WC |
 * | 1st mem op \|      |      |      |      |      |      |
 * +-------------+------+------+------+------+------+------+
 * |    R WB     |      |      | LF1  |      |      |      |
 * +-------------+------+------+------+------+------+------+
 * |    R UC     |      |      | LF1  |      |      |      |
 * +-------------+------+------+------+------+------+------+
 * |    R WC     |      |      | LF1  |      |      |      |
 * +-------------+------+------+------+------+------+------+
 * |    W WB     | MF1  |      | MF1  |      |      | SF2  |
 * +-------------+------+------+------+------+------+------+
 * |    W UC     | MF1  |      | MF1  |      |      | SF2  |
 * +-------------+------+------+------+------+------+------+
 * |    W WC     | MF1  |      | MF1  | SF1  |      | SF2  |
 * +-------------+------+------+------+------+------+------+
 *
 * MF1 - WB or WC load may pass a previous non-conflicting WB, WC or UC store.
 *       Use MFENCE. This is a combination of rules 'e' and 'i' in the AMD
 *       diagram.
 * LF1 - WC load may pass a previous WB, WC or UC load. Use LFENCE. This is
 *       rule 'b' in the AMD diagram.
 * SF1 - WB store may pass a previous WC store. Use SFENCE. This is rule 'j' in
 *       the AMD diagram.
 * SF2 - WC store may pass a previous UC, WB or non-conflicting WC store. Use
 *       SFENCE. This is rule 'h' in the AMD diagram.
 *
 * To figure out the specific barrier required, pick and collapse the relevant
 * rows and columns, choosing the strongest barrier.
 *
 * SMP barriers only concern with access to "normal memory" (write-back cached
 * i.e. WB using above terminology), so we only need to worry about store-load
 * reordering. In other cases a compiler barrier is sufficient. SMP store-load
 * reordering is handled with a locked XOR (instead of a proper MFENCE
 * instructon) for performance reasons. See PR 1674199 for more details.
 *
 * DMA barriers are equivalent to SMP barriers on x86.
 *
 * MMIO barriers are used to mix access to different memory types, so more
 * reordering is possible, and is handled via LFENCE/SFENCE. Also, a proper
 * MFENCE must be used instead of the locked XOR trick, due to the latter
 * not guarding non-temporal/WC accesses.
 */

#define SMP_R_BARRIER_R()     COMPILER_READ_BARRIER()
#define SMP_R_BARRIER_W()     COMPILER_MEM_BARRIER()
#define SMP_R_BARRIER_RW()    COMPILER_MEM_BARRIER()
#define SMP_W_BARRIER_R()     LOCKED_INSN_BARRIER()
#define SMP_W_BARRIER_W()     COMPILER_WRITE_BARRIER()
#define SMP_W_BARRIER_RW()    LOCKED_INSN_BARRIER()
#define SMP_RW_BARRIER_R()    LOCKED_INSN_BARRIER()
#define SMP_RW_BARRIER_W()    COMPILER_MEM_BARRIER()
#define SMP_RW_BARRIER_RW()   LOCKED_INSN_BARRIER()

/*
 * Like the above, only for use with observers other than CPUs,
 * i.e. DMA masters. Same as SMP barriers for x86.
 */

#define DMA_R_BARRIER_R()     SMP_R_BARRIER_R()
#define DMA_R_BARRIER_W()     SMP_R_BARRIER_W()
#define DMA_R_BARRIER_RW()    SMP_R_BARRIER_RW()
#define DMA_W_BARRIER_R()     SMP_W_BARRIER_R()
#define DMA_W_BARRIER_W()     SMP_W_BARRIER_W()
#define DMA_W_BARRIER_RW()    SMP_W_BARRIER_RW()
#define DMA_RW_BARRIER_R()    SMP_RW_BARRIER_R()
#define DMA_RW_BARRIER_W()    SMP_RW_BARRIER_W()
#define DMA_RW_BARRIER_RW()   SMP_RW_BARRIER_RW()

/*
 * And finally a set for use with MMIO accesses. These barriers must be stronger
 * because they are used when mixing accesses to different memory types.
 */

#define MMIO_R_BARRIER_R()    LFENCE()
#define MMIO_R_BARRIER_W()    SMP_R_BARRIER_W()
#define MMIO_R_BARRIER_RW()   LFENCE()
#define MMIO_W_BARRIER_R()    MFENCE()
#define MMIO_W_BARRIER_W()    SFENCE()
#define MMIO_W_BARRIER_RW()   MFENCE()
#define MMIO_RW_BARRIER_R()   MFENCE()
#define MMIO_RW_BARRIER_W()   SFENCE()
#define MMIO_RW_BARRIER_RW()  MFENCE()


/*
 *----------------------------------------------------------------------
 *
 * MMIORead8 --
 *
 *      IO read from address "addr".
 *
 * Results:
 *      8-bit value at given location.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint8
MMIORead8(const volatile void *addr)
{
   volatile uint8 *addr8 = (volatile uint8 *) addr;

   return *addr8;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIOWrite8 --
 *
 *      IO write to address "addr".
 *
 *----------------------------------------------------------------------
 */
static INLINE void
MMIOWrite8(volatile void *addr, // IN
           uint8 val)           // IN
{
   volatile uint8 *addr8 = (volatile uint8 *) addr;

   *addr8 = val;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIORead16 --
 *
 *      IO read from address "addr".
 *
 * Results:
 *      16-bit value at given location.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint16
MMIORead16(const volatile void *addr)
{
   volatile uint16 *addr16 = (volatile uint16 *) addr;

   return *addr16;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIOWrite16 --
 *
 *      IO write to address "addr".
 *
 *----------------------------------------------------------------------
 */
static INLINE void
MMIOWrite16(volatile void *addr,  // IN
            uint16 val)           // IN
{
   volatile uint16 *addr16 = (volatile uint16 *) addr;

   *addr16 = val;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIORead32 --
 *
 *      IO read from address "addr".
 *
 * Results:
 *      32-bit value at given location.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint32
MMIORead32(const volatile void *addr)
{
   volatile uint32 *addr32 = (volatile uint32 *) addr;

   return *addr32;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIOWrite32 --
 *
 *      IO write to address "addr".
 *
 *----------------------------------------------------------------------
 */
static INLINE void
MMIOWrite32(volatile void *addr, // OUT
            uint32 val)
{
   volatile uint32 *addr32 = (volatile uint32 *) addr;

   *addr32 = val;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIORead64 --
 *
 *      IO read from address "addr".
 *
 * Results:
 *      64-bit value at given location.
 *
 *----------------------------------------------------------------------
 */
static INLINE uint64
MMIORead64(const volatile void *addr)
{
   volatile uint64 *addr64 = (volatile uint64 *) addr;

   return *addr64;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIOWrite64 --
 *
 *      IO write to address "addr".
 *
 *----------------------------------------------------------------------
 */
static INLINE void
MMIOWrite64(volatile void *addr, // OUT
            uint64 val)
{
   volatile uint64 *addr64 = (volatile uint64 *) addr;

   *addr64 = val;
}

#endif // _VM_BASIC_ASM_X86_COMMON_H_
