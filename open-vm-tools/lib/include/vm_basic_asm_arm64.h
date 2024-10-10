/*********************************************************
 * Copyright (C) 2013-2024 VMware, Inc. All rights reserved.
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
 * vm_basic_asm_arm64.h --
 *
 *      Basic assembler macros for the AArch64 architecture.
 */

#ifndef _VM_BASIC_ASM_ARM64_H_
#define _VM_BASIC_ASM_ARM64_H_

#include "vm_basic_defs.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 *----------------------------------------------------------------------
 *
 * _DMB --
 *
 *      Data memory barrier.
 *
 *      Memory barrier governing visibility of explicit load/stores.
 *
 *      The options for shareability domains are:
 *      NSH     - Non-shareable
 *      ISH     - Inner Shareable
 *      OSH     - Outer Shareable
 *      default - Full System
 *
 *      The options for access types are:
 *      LD      - Load         , Barrier, Load _or Store_ (yes, really)
 *      ST      - Store        , Barrier, Store
 *      default - Load or Store, Barrier, Load or Store
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

#if defined __GNUC__
#define _DMB(t) asm volatile ("dmb " #t ::: "memory")
#elif defined _MSC_VER
#define _DMB(t) __dmb(_ARM64_BARRIER_##t)
#else
#error No compiler defined for _DMB
#endif


/*
 *----------------------------------------------------------------------
 *
 * _DSB --
 *
 *      Data synchronization barrier.
 *
 *      Synchronizes the execution stream with memory accesses. Like a DMB but
 *      also forces all cache/TLB maintenance operations to complete.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

#if defined __GNUC__
#define _DSB(t) asm volatile ("dsb " #t ::: "memory")
#elif defined _MSC_VER
#define _DSB(t) __dsb(_ARM64_BARRIER_##t)
#else
#error No compiler defined for _DSB
#endif


/*
 *----------------------------------------------------------------------
 *
 * ISB --
 *
 *      Instruction synchronization barrier.
 *
 *      Pipeline flush - all instructions fetched after ISB have effects of
 *      cache/maintenance and system register updates prior to the ISB visible.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static inline void
ISB(void)
{
#if defined __GNUC__
   asm volatile ("isb" ::: "memory");
#elif defined _MSC_VER
   __isb(_ARM64_BARRIER_SY);
#else
#error No compiler defined for ISB
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * ESB --
 *
 *      Error synchronization barrier.
 *
 *      Error synchronization event as per Arm ARM. NOP if ARMv8.2
 *      RAS extensions are not implemented.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      SError exception or DISR/VDISR getting updated.
 *
 *----------------------------------------------------------------------
 */

#if defined __GNUC__
static inline void
ESB(void)
{
   /*
    * The assembler of gcc 9.3 with -march=armv8-a errors out with
    * "Error: selected processor does not support `esb'"
    * There is no way to cleanly temporarily push/pop -march=armv8.2-a or the
    * ras extension. The error does not occur with gcc versions >= 10.2.
    */
# if __GNUC__ > 10 || __GNUC__ == 10 && __GNUC_MINOR__ >= 2
   asm volatile("esb" ::: "memory");
# else
   asm volatile(
      ".arch armv8.2-a"       "\n\t"
      "esb"                   "\n\t"
      ".arch " XSTR(VMW_ARCH)
      ::: "memory"
   );
# endif
}
#endif

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
 * => A compiler memory barrier on its own is useful for coordinating
 *    with an interrupt handler (or preemption logic in the scheduler)
 *    on the same CPU, so that the order of read and write
 *    instructions in code that might be interrupted is consistent
 *    with the barriers. But when there are other CPUs involved, or
 *    other types of devices like memory-mapped I/O and DMA
 *    controllers, a compiler memory barrier is not enough.
 *
 * A CPU memory barrier prevents the CPU from re-ordering memory accesses
 * accross the barrier. It is a CPU instruction.
 *
 * => On its own the CPU instruction isn't useful because the compiler
 *    may reorder loads and stores around the CPU instruction.  It is
 *    useful only when combined with a compiler memory barrier.
 *
 * A memory barrier is the union of a compiler memory barrier and a CPU memory
 * barrier.
 *
 *    Semantics
 *    ---------
 *
 * At the time COMPILER_MEM_BARRIER was created (and references to it were
 * added to the code), the code was only targetting x86. The intent of the code
 * was really to use a memory barrier, but because x86 uses a strongly ordered
 * memory model, the CPU would not re-order most memory accesses (store-load
 * ordering still requires MFENCE even on x86), and the code could get away
 * with using just a compiler memory barrier. So COMPILER_MEM_BARRIER was born
 * and was implemented as a compiler memory barrier _on x86_. But make no
 * mistake, _the semantics that the code expects from COMPILER_MEM_BARRIER is
 * that of a memory barrier_!
 *
 *    DO NOT USE!
 *    -----------
 *
 * On at least one non-x86 architecture, COMPILER_MEM_BARRIER is
 * 1) A misnomer
 * 2) Not fine-grained enough to provide the best performance.
 * For the above two reasons, usage of COMPILER_MEM_BARRIER is now deprecated.
 * _Do not add new references to COMPILER_MEM_BARRIER._ Instead, precisely
 * document the intent of your code by using
 * <mem_type/purpose>_<before_access_type>_BARRIER_<after_access_type>.
 * Existing references to COMPILER_MEM_BARRIER are being slowly but surely
 * converted, and when no references are left, COMPILER_MEM_BARRIER will be
 * retired.
 *
 * Thanks for pasting this whole comment into every architecture header.
 */

/*
 * To match x86 TSO semantics, we need to guarantee ordering for
 * everything _except_ store-load:
 *
 * - DMB ISHLD orders load-load and load-store.
 * - DMB ISHST orders store-store.
 *
 * In contrast, SMP_RW_BARRIER_RW, or DMB ISH, orders all four
 * (load-load, load-store, store-load, store-store), so it's stronger
 * than we need -- like x86 MFENCE.
 */
#define COMPILER_MEM_BARRIER() do { _DMB(ISHLD); _DMB(ISHST); } while (0)

/*
 * Memory barriers. These take the form of
 *
 * <mem_type/purpose>_<before_access_type>_BARRIER_<after_access_type>
 *
 * where:
 *   <mem_type/purpose> is either INTR, SMP, DMA, or MMIO.
 *   <*_access type> is either R(load), W(store) or RW(any).
 *
 * Above every use of these memory barriers in the code, there _must_ be a
 * comment to justify the use, i.e. a comment which:
 * 1) Precisely identifies which memory accesses must not be re-ordered across
 *    the memory barrier.
 * 2) Explains why it is important that the memory accesses not be re-ordered.
 *
 * Thanks for pasting this whole comment into every architecture header.
 */

#define SMP_R_BARRIER_R()     SMP_R_BARRIER_RW()
#define SMP_R_BARRIER_W()     SMP_R_BARRIER_RW()
#define SMP_R_BARRIER_RW()    _DMB(ISHLD)
#define SMP_W_BARRIER_R()     SMP_RW_BARRIER_RW()
#define SMP_W_BARRIER_W()     _DMB(ISHST)
#define SMP_W_BARRIER_RW()    SMP_RW_BARRIER_RW()
#define SMP_RW_BARRIER_R()    SMP_RW_BARRIER_RW()
#define SMP_RW_BARRIER_W()    SMP_RW_BARRIER_RW()
#define SMP_RW_BARRIER_RW()   _DMB(ISH)

/*
 * Like the above, only for use with cache coherent observers other than CPUs,
 * i.e. DMA masters.
 * On ARM it means that we extend the `dmb' options to an outer-shareable
 * memory where all our devices are.
 */

#define DMA_R_BARRIER_R()     DMA_R_BARRIER_RW()
#define DMA_R_BARRIER_W()     DMA_R_BARRIER_RW()
#define DMA_R_BARRIER_RW()    _DMB(OSHLD)
#define DMA_W_BARRIER_R()     DMA_RW_BARRIER_RW()
#define DMA_W_BARRIER_W()     _DMB(OSHST)
#define DMA_W_BARRIER_RW()    DMA_RW_BARRIER_RW()
#define DMA_RW_BARRIER_R()    DMA_RW_BARRIER_RW()
#define DMA_RW_BARRIER_W()    DMA_RW_BARRIER_RW()
#define DMA_RW_BARRIER_RW()   _DMB(OSH)

/*
 * And finally a set for use with MMIO accesses.
 * Synchronization of accesses to a non-cache coherent device memory
 * (in general case) requires strongest available barriers on ARM.
 */

#define MMIO_R_BARRIER_R()    MMIO_R_BARRIER_RW()
#define MMIO_R_BARRIER_W()    MMIO_R_BARRIER_RW()
#define MMIO_R_BARRIER_RW()   _DSB(LD)
#define MMIO_W_BARRIER_R()    MMIO_RW_BARRIER_RW()
#define MMIO_W_BARRIER_W()    _DSB(ST)
#define MMIO_W_BARRIER_RW()   MMIO_RW_BARRIER_RW()
#define MMIO_RW_BARRIER_R()   MMIO_RW_BARRIER_RW()
#define MMIO_RW_BARRIER_W()   MMIO_RW_BARRIER_RW()
#define MMIO_RW_BARRIER_RW()  _DSB(SY)

#ifdef __GNUC__

/*
 * _GET_CURRENT_PC --
 * GET_CURRENT_PC --
 *
 * Returns the current program counter. In the example below:
 *
 *   foo.c
 *   L123: Foo(GET_CURRENT_PC())
 *
 * the return value from GET_CURRENT_PC will point a debugger to L123.
 */

#define _GET_CURRENT_PC(pc)                                                   \
   asm volatile ("1: adr %0, 1b" : "=r" (pc))

static INLINE_ALWAYS void *
GET_CURRENT_PC(void)
{
   void *pc;

   _GET_CURRENT_PC(pc);
   return pc;
}

/*
 * GET_CURRENT_LOCATION --
 *
 * Updates the arguments with the values of the pc, fp, sp and the
 * return address at the current code location where the macro is invoked.
 */

#define GET_CURRENT_LOCATION(pc, fp, sp, retAddr) do {                        \
   _GET_CURRENT_PC(pc);                                                       \
   asm volatile ("mov %0, sp" : "=r" (sp));                                   \
   fp = (uint64)GetFrameAddr();                                               \
   retAddr = (uint64)GetReturnAddress();                                      \
} while (0)


/*
 *----------------------------------------------------------------------
 *
 * MRS --
 *
 *      Get the value of system register 'name'.
 *
 * Results:
 *      The value.
 *
 * Side effects:
 *      Depends on 'name'.
 *
 *----------------------------------------------------------------------
 */

#define MRS(name) ({                                                          \
   uint64 val;                                                                \
   asm volatile ("mrs %0, " XSTR(name) : "=r" (val) :: "memory");             \
   val;                                                                       \
})


/*
 *----------------------------------------------------------------------
 *
 * MSR --
 * MSR_IMMED --
 *
 *      Set the value of system register 'name'.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Depends on 'name'.
 *
 *----------------------------------------------------------------------
 */

#define MSR(name, val)                                                        \
   asm volatile ("msr " XSTR(name) ", %0" :: "r" (val) : "memory")

#define MSR_IMMED(name, val)                                                  \
   asm volatile ("msr " XSTR(name) ", %0" :: "i" (val) : "memory")

#endif // ifdef __GNUC__


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

static inline uint32
MMIORead32(const volatile void *addr) // IN
{
   uint32 res;

#if defined __GNUC__
   asm volatile ("ldr %w0, %1"
                 : "=r" (res)
                 : "m" (*(const volatile uint32 *)addr));
#elif defined _MSC_VER
   res = __iso_volatile_load32((const volatile __int32 *)addr);
#else
#error No compiler defined for MMIORead32
#endif
   return res;
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

static inline uint64
MMIORead64(const volatile void *addr) // IN
{
   uint64 res;

#if defined __GNUC__
   asm volatile ("ldr %x0, %1"
                 : "=r" (res)
                 : "m" (*(const volatile uint64 *)addr));
#elif defined _MSC_VER
   res = __iso_volatile_load64((const volatile __int64 *)addr);
#else
#error No compiler defined for MMIORead64
#endif
   return res;
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

static inline void
MMIOWrite32(volatile void *addr, // OUT
            uint32 val)          // IN
{
#if defined __GNUC__
   asm volatile ("str %w1, %0"
                 : "=m" (*(volatile uint32 *)addr)
                 : "r" (val));
#elif defined _MSC_VER
   __iso_volatile_store32((volatile __int32 *)addr, val);
#else
#error No compiler defined for MMIOWrite32
#endif
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

static inline void
MMIOWrite64(volatile void *addr, // OUT
            uint64 val)          // IN
{
#if defined __GNUC__
   asm volatile ("str %x1, %0"
                 : "=m" (*(volatile uint64 *)addr)
                 : "r" (val));
#elif defined _MSC_VER
   __iso_volatile_store64((volatile __int64 *)addr, val);
#else
#error No compiler defined for MMIOWrite64
#endif
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

static inline uint16
MMIORead16(const volatile void *addr) // IN
{
   uint16 res;

#if defined __GNUC__
   asm volatile ("ldrh %w0, %1"
                 : "=r" (res)
                 : "m" (*(const volatile uint16 *)addr));
#elif defined _MSC_VER
   res = __iso_volatile_load16((const volatile __int16 *)addr);
#else
#error No compiler defined for MMIORead16
#endif
   return res;
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

static inline void
MMIOWrite16(volatile void *addr,  // OUT
            uint16 val)           // IN
{
#if defined __GNUC__
   asm volatile ("strh %w1, %0"
                 : "=m" (*(volatile uint16 *)addr)
                 : "r" (val));
#elif defined _MSC_VER
   __iso_volatile_store16((volatile __int16 *)addr, val);
#else
#error No compiler defined for MMIOWrite16
#endif
}


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

static inline uint8
MMIORead8(const volatile void *addr) // IN
{
   uint8 res;

#if defined __GNUC__
   asm volatile ("ldrb %w0, %1"
                 : "=r" (res)
                 : "m" (*(const volatile uint8 *)addr));
#elif defined _MSC_VER
   res = __iso_volatile_load8((const volatile __int8 *)addr);
#else
#error No compiler defined for MMIORead8
#endif
   return res;
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

static inline void
MMIOWrite8(volatile void *addr, // OUT
           uint8 val)           // IN
{
#if defined __GNUC__
   asm volatile ("strb %w1, %0"
                 : "=m" (*(volatile uint8 *)addr)
                 : "r" (val));
#elif defined _MSC_VER
   __iso_volatile_store8((volatile __int8 *)addr, val);
#else
#error No compiler defined for MMIOWrite8
#endif
}


#ifdef VM_HAS_INT128
/*
 *----------------------------------------------------------------------
 *
 * MMIORead128 --
 *
 *      IO read from address "addr".
 *
 * Results:
 *      128-bit value at given location.
 *
 *----------------------------------------------------------------------
 */
static inline uint128
MMIORead128(const volatile void *addr) // IN
{
   union {
      uint128 val128;
      struct {
         uint64 val64[2];
      };
   } res;
   asm volatile ("ldp %x0, %x1, %2"
                 : "=r" (res.val64[0]), "=r" (res.val64[1])
                 : "Q" (*(const volatile uint128 *)addr));
   return res.val128;
}


/*
 *----------------------------------------------------------------------
 *
 * MMIOWrite128 --
 *
 *      IO write to address "addr".
 *
 *----------------------------------------------------------------------
 */
static inline void
MMIOWrite128(volatile void *addr, // OUT
             uint128 val)         // IN
{
   union {
      uint128 val128;
      struct {
         uint64 val64[2];
      };
   } res = { .val128 = val };
   asm volatile ("stp %x1, %x2, %0"
                 : "=Q" (*(volatile uint128 *)addr)
                 : "r" (res.val64[0]), "r" (res.val64[1]));
}
#endif // VM_HAS_INT128


#ifdef __GNUC__

/*
 *----------------------------------------------------------------------
 *
 * WFI --
 *
 *      Wait for interrupt.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static inline void
WFI(void)
{
   asm volatile ("wfi" ::: "memory");
}


/*
 *----------------------------------------------------------------------
 *
 * WFE --
 *
 *      Wait for event.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static inline void
WFE(void)
{
   asm volatile ("wfe" ::: "memory");
}


/*
 *----------------------------------------------------------------------
 *
 * SEV --
 *
 *      Generate a global event.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static inline void
SEV(void)
{
   asm volatile ("sev" ::: "memory");
}


/*
 *-----------------------------------------------------------------------------
 *
 * SetSPELx --
 *
 *    Set SP_ELx to the given value when operating with SP_EL0.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static inline void
SetSPELx(VA va)
{
   asm volatile (
      "msr     spsel, #1 \n\t"
      "mov     sp, %0    \n\t"
      "msr     spsel, #0 \n\t"
      :
      : "r" (va)
      : "memory"
   );
}


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
 *    Use this function if you are certain that the quotient will fit in 32 bits.
 *
 * Results:
 *    Quotient and remainder
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static inline void
Div643232(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint32 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   *quotient = (uint32)(dividend / divisor);
   *remainder = (uint32)(dividend % divisor);
}

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

static inline void
Div643264(uint64 dividend,   // IN
          uint32 divisor,    // IN
          uint64 *quotient,  // OUT
          uint32 *remainder) // OUT
{
   *quotient = dividend / divisor;
   *remainder = dividend % divisor;
}


/*
 *----------------------------------------------------------------------
 *
 * uint64set --
 *
 *      memset a given address with an uint64 value, count times.
 *
 * Results:
 *      Pointer to filled memory range.
 *
 * Side effects:
 *      As with memset.
 *
 *----------------------------------------------------------------------
 */

static inline void *
uint64set(void *dst, uint64 val, uint64 count)
{
   void   *tmpDst = dst;

   if (count == 0) {
      return dst;
   }

   __asm__ __volatile__ (
      // Copy two at a time
      "1:\t"
      "cmp     %1, #8\n\t"
      "b.lo    2f\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "stp     %2, %2, [%0], #16\n\t"
      "sub     %1, %1, #8\n\t"
      "b       1b\n"

      // Copy remaining data
      "2:\t"
      "cbz     %1, 3f\n\t"
      "str     %2, [%0], #8\n\t"
      "sub     %1, %1, #1\n\t"
      "b       2b\n"

      // One or zero values left to copy
      "3:\n\t"
      : "+r" (tmpDst), "+r" (count)
      : "r" (val)
      : "cc", "memory");

   return dst;
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
 *      On arm64, we need to do an ISB according to ARM ARM to prevent
 *      instruction reordering, and to ensure no store reordering we do a DMB.
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

static inline void
RDTSC_BARRIER(void)
{
   ISB();
   _DMB(SY);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DCacheCleanInvalidate --
 *
 *      Data Cache Clean and Invalidate to Point of Coherence range.
 *
 *      Use the typical cache line size, for simplicity.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline void
DCacheCleanInvalidate(VA va, uint64 len)
{
   VA dva;

   /* Clean and Invalidate D-cache to PoC. */
   for (dva = ROUNDDOWN(va, CACHELINE_SIZE);
        dva < va + len;
        dva += CACHELINE_SIZE) {
      asm volatile ("dc civac, %0" :: "r" (dva) : "memory");
   }

   /* Ensure completion. */
   _DSB(SY);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DCacheClean --
 *
 *      Data Cache Clean range to Point of Coherence.
 *
 *      Use the typical cache line size, for simplicity.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static inline void
DCacheClean(VA va, uint64 len)
{
   VA dva;

   /* Clean D-cache to PoC. */
   for (dva = ROUNDDOWN(va, CACHELINE_SIZE);
        dva < va + len;
        dva += CACHELINE_SIZE) {
      asm volatile ("dc cvac, %0" :: "r" (dva) : "memory");
   }

   /* Ensure completion of clean. */
   _DSB(SY);
}

#endif // ifdef __GNUC__

#if defined _MSC_VER
/* Until we implement Mul64x6464() with Windows intrinsics... */
#define MUL64_NO_ASM 1
#endif

#ifdef MUL64_NO_ASM
#include "mul64.h"
#else
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

static inline uint64
Mul64x6464(uint64 multiplicand,
           uint64 multiplier,
           uint32 shift)
{
   if (shift == 0) {
      return multiplicand * multiplier;
   } else {
      uint64 lo, hi;

      asm("mul   %0, %2, %3" "\n\t"
          "umulh %1, %2, %3"
          : "=&r" (lo), "=r" (hi)
          : "r" (multiplicand), "r" (multiplier));
      return (hi << (64 - shift) | lo >> shift) + (lo >> (shift - 1) & 1);
   }
}


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

static inline int64
Muls64x64s64(int64 multiplicand,
             int64 multiplier,
             uint32 shift)
{
   if (shift == 0) {
      return multiplicand * multiplier;
   } else {
      uint64 lo, hi;

      asm("mul   %0, %2, %3" "\n\t"
          "smulh %1, %2, %3"
          : "=&r" (lo), "=r" (hi)
          : "r" (multiplicand), "r" (multiplier));
      return (hi << (64 - shift) | lo >> shift) + (lo >> (shift - 1) & 1);
   }
}


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

static inline uint64
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

static inline int64
Muls64x32s64(int64 multiplicand, uint32 multiplier, uint32 shift)
{
   return Muls64x64s64(multiplicand, multiplier, shift);
}
#endif


#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _VM_BASIC_ASM_ARM64_H_
