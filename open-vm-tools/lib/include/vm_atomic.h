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

/*
 * vm_atomic.h --
 *
 *       Atomic power
 *
 * Note: Only partially tested on ARM processors: Works for View Open
 *       Client, which shouldn't have threads, and ARMv8 processors.
 *
 *       In ARM, GCC intrinsics (__sync*) compile but might not
 *       work, while MS intrinsics (_Interlocked*) do not compile.
 */

#ifndef _ATOMIC_H_
#define _ATOMIC_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_assert.h"

#if defined __cplusplus
extern "C" {
#endif

/*
 * There are two concepts involved when dealing with atomic accesses:
 * 1. Atomicity of the access itself
 * 2. Ordering of the access with respect to other reads&writes (from the view
 *    of other processors/devices).
 *
 * Two examples help to clarify #2:
 * a. Inc: A caller implementing a simple independent global event counter
 *         might not care if the compiler or processor visibly reorders the
 *         increment around other memory accesses.
 * b. Dec: A caller implementing a reference count absolutely *doesn't* want
 *         the compiler or processor to visibly reordering writes after that
 *         decrement: if that happened, the program could then end up writing
 *         to memory that was freed by another processor.
 *
 * C11 has standardized a good model for expressing these orderings when doing
 * atomics. It defines three *tiers* of ordering:
 * 1. Sequential Consistency (every processor sees the same total order of
 *    events)
 *
 * 2. Acquire/Release ordering (roughly, everybody can agree previous events
 *    have completed, but they might disagree on the ordering of previous
 *    independent events).
 *
 *    The relative ordering provided by this tier is sufficient for common
 *    locking and initialization activities, but is insufficient for unusual
 *    synchronization schemes (e.g. IRIW aka Independent Read Independent
 *    Write designs such Dekker's algorithm, Peterson's algorithm, etc.)
 *
 *    In other words, this tier is close in behavior to Sequential Consistency
 *    in much the same way a General-Relativity universe is close to a
 *    Newtonian universe.
 * 3. Relaxed (i.e unordered/unfenced)
 *
 * In C11 standard's terminology for atomic memory ordering,
 * - in case (a) we want "relaxed" ordering for perf and,
 * - in case (b) we want "sequentially consistent" ordering (or perhaps the
 *   only slightly weaker "release" ordering) for correctness.
 *
 * There are standardized mappings of operations to orderings for every
 * processor architecture. See
 * - https://www.cl.cam.ac.uk/~pes20/cpp/cpp0xmappings.html
 * - http://preshing.com/20120913/acquire-and-release-semantics/
 *
 * In this file:
 * 1. all RMW (Read/Modify/Write) operations are sequentially consistent.
 *    This includes operations like Atomic_IncN, Atomic_ReadIfEqualWriteN,
 *    Atomic_ReadWriteN, etc.
 * 2. all R and W operations are relaxed. This includes operations like
 *    Atomic_WriteN, Atomic_ReadN, Atomic_TestBitN, etc.
 *
 * The below routines of course ensure both the CPU and compiler honor the
 * ordering constraint.
 *
 * Notes:
 * 1. Since R-only and W-only operations do not provide ordering, callers
 *    using them for synchronizing operations like double-checked
 *    initialization or releasing spinlocks must provide extra barriers.
 * 2. This implementation of Atomic operations is suboptimal. On x86,simple
 *    reads and writes have acquire/release semantics at the hardware level.
 *    On arm64, we have separate instructions for sequentially consistent
 *    reads and writes (the same instructions are used for acquire/release).
 *    Neither of these are exposed for R-only or W-only callers.
 *
 * For further details on x86 and ARM memory ordering see
 * https://wiki.eng.vmware.com/ARM/MemoryOrdering.
 */

#ifdef VM_ARM_64
#   include "vm_atomic_arm64_begin.h"
#endif


/* Basic atomic types: 8, 16, 32, 64 and 128 bits */
typedef struct Atomic_uint8 {
   volatile uint8 value;
} Atomic_uint8 ALIGNED(1);

typedef struct Atomic_uint16 {
   volatile uint16 value;
} Atomic_uint16 ALIGNED(2);

typedef struct Atomic_uint32 {
   volatile uint32 value;
} Atomic_uint32 ALIGNED(4);

typedef struct Atomic_uint64 {
   volatile uint64 value;
} Atomic_uint64 ALIGNED(8);

#if defined __GNUC__ && defined VM_64BIT && \
     (defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 || defined VM_ARM_64)
typedef struct Atomic_uint128 {
   volatile uint128 value;
} Atomic_uint128 ALIGNED(16);
#endif

/*
 * Prototypes for msft atomics.  These are defined & inlined by the
 * compiler so no function definition is needed.  The prototypes are
 * needed for C++.
 *
 * The declarations for the intrinsic functions were taken from ntddk.h
 * in the DDK. The declarations must match otherwise the 64-bit C++
 * compiler will complain about second linkage of the intrinsic functions.
 * We define the intrinsic using the basic types corresponding to the
 * Windows typedefs. This avoids having to include windows header files
 * to get to the windows types.
 */
#if defined _MSC_VER && !defined BORA_NO_WIN32_INTRINS
#ifdef __cplusplus
extern "C" {
#endif
long  _InterlockedExchange(long volatile*, long);
long  _InterlockedCompareExchange(long volatile*, long, long);
long  _InterlockedExchangeAdd(long volatile*, long);
long  _InterlockedDecrement(long volatile*);
long  _InterlockedIncrement(long volatile*);
__int64  _InterlockedCompareExchange64(__int64 volatile*, __int64, __int64);
#pragma intrinsic(_InterlockedExchange, _InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchangeAdd, _InterlockedDecrement)
#pragma intrinsic(_InterlockedIncrement)
#pragma intrinsic(_InterlockedCompareExchange64)

# if _MSC_VER >= 1600
char     _InterlockedExchange8(char volatile *, char);
char     _InterlockedCompareExchange8(char volatile *, char, char);
#pragma intrinsic(_InterlockedCompareExchange8, _InterlockedCompareExchange8)
#endif

#if defined VM_X86_64
long     _InterlockedAnd(long volatile*, long);
__int64  _InterlockedAnd64(__int64 volatile*, __int64);
long     _InterlockedOr(long volatile*, long);
__int64  _InterlockedOr64(__int64 volatile*, __int64);
long     _InterlockedXor(long volatile*, long);
__int64  _InterlockedXor64(__int64 volatile*, __int64);
__int64  _InterlockedExchangeAdd64(__int64 volatile*, __int64);
__int64  _InterlockedIncrement64(__int64 volatile*);
__int64  _InterlockedDecrement64(__int64 volatile*);
__int64  _InterlockedExchange64(__int64 volatile*, __int64);
#if !defined _WIN64
#pragma intrinsic(_InterlockedAnd, _InterlockedAnd64)
#pragma intrinsic(_InterlockedOr, _InterlockedOr64)
#pragma intrinsic(_InterlockedXor, _InterlockedXor64)
#pragma intrinsic(_InterlockedExchangeAdd64, _InterlockedIncrement64)
#pragma intrinsic(_InterlockedDecrement64, _InterlockedExchange64)
#endif /* !_WIN64 */
#endif /* __x86_64__ */

#ifdef __cplusplus
}
#endif
#endif /* _MSC_VER */

#if defined __arm__
/*
 * LDREX without STREX or CLREX may cause problems in environments where the
 * context switch may not clear the reference monitor - according ARM manual
 * the reference monitor should be cleared after a context switch, but some
 * may not like Linux kernel's non-preemptive context switch path. So use of
 * ARM routines in kernel code may not be safe.
 */
#   if defined __ARM_ARCH_7__ || defined __ARM_ARCH_7A__ ||  \
       defined __ARM_ARCH_7R__|| defined __ARM_ARCH_7M__
#      define VM_ARM_V7
#      ifdef __KERNEL__
#         warning LDREX/STREX may not be safe in linux kernel, since it      \
          does not issue CLREX on context switch (as of 2011-09-29).
#      endif
#   else
#     error Only ARMv7 extends the synchronization primitives ldrex/strex.   \
            For the lower ARM version, please implement the atomic functions \
            by kernel APIs.
#   endif
#endif

/* Data Memory Barrier */
#ifdef VM_ARM_V7
#define dmb() __asm__ __volatile__("dmb" : : : "memory")
#endif


/* Convert a volatile uint32 to Atomic_uint32. */
static INLINE Atomic_uint32 *
Atomic_VolatileToAtomic32(volatile uint32 *var)  // IN:
{
   return (Atomic_uint32 *)var;
}
#define Atomic_VolatileToAtomic Atomic_VolatileToAtomic32


/* Convert a volatile uint64 to Atomic_uint64. */
static INLINE Atomic_uint64 *
Atomic_VolatileToAtomic64(volatile uint64 *var)  // IN:
{
   return (Atomic_uint64 *)var;
}


/*
 * The Read/Modify/Write operations on x86/x64 are all written using the
 * "memory" constraint. This is to ensure the compiler treats the operation as
 * a full barrier, flushing any pending/cached state currently residing in
 * registers.
 */

#if defined _MSC_VER && _MSC_VER < 1600 && defined __x86_64__
uint8 VMWInterlockedExchange8(uint8 volatile *ptr,
                              uint8 val);

uint8 VMWInterlockedCompareExchange8(uint8 volatile *ptr,
                                     uint8 newVal,
                                     uint8 oldVal);
#endif

#if defined __GNUC__ && defined VM_ARM_32
/* Force the link step to fail for unimplemented functions. */
extern int AtomicUndefined(void const *);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite128 --
 *
 *      Compare and exchange a 16 byte tuple.
 *
 * Results:
 *      old value
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
#if defined __GNUC__ && defined VM_64BIT && \
     (defined __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16 || defined VM_ARM_64)
static INLINE uint128
Atomic_ReadIfEqualWrite128(Atomic_uint128 *ptr,   // IN/OUT
                           uint128        oldVal, // IN
                           uint128        newVal) // IN
{
#ifdef __GCC_HAVE_SYNC_COMPARE_AND_SWAP_16
   return __sync_val_compare_and_swap(&ptr->value, oldVal, newVal);
#elif defined VM_ARM_64
   union {
      uint128 raw;
      struct {
         uint64 lo;
         uint64 hi;
      };
   } res, _old = { oldVal }, _new = { newVal };
   uint32 failed;

   SMP_RW_BARRIER_RW();
   __asm__ __volatile__(
      "1: ldxp    %x0, %x1, %3        \n\t"
      "   cmp     %x0, %x4            \n\t"
      "   ccmp    %x1, %x5, #0, eq    \n\t"
      "   b.ne    2f                  \n\t"
      "   stxp    %w2, %x6, %x7, %3   \n\t"
      "   cbnz    %w2, 1b             \n\t"
      "2:                             \n\t"
      : "=&r" (res.lo),
        "=&r" (res.hi),
        "=&r" (failed),
        "+Q" (ptr->value)
      : "r" (_old.lo),
        "r" (_old.hi),
        "r" (_new.lo),
        "r" (_new.hi)
      : "cc"
   );
   SMP_RW_BARRIER_RW();

   return res.raw;
#endif
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read8 --
 *
 *      Read the value of the specified object atomically.
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_Read8(Atomic_uint8 const *var)  // IN:
{
   uint8 val;

#if defined __GNUC__ && defined VM_ARM_32
   val = AtomicUndefined(var);
#elif defined VM_ARM_64
   val = _VMATOM_X(R, 8, &var->value);
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
   __asm__ __volatile__(
      "movb %1, %0"
      : "=q" (val)
      : "m" (var->value)
   );
#elif defined _MSC_VER
   val = var->value;
#else
#error No compiler defined for Atomic_Read8
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWrite8 --
 *
 *      Read followed by write.
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadWrite8(Atomic_uint8 *var,  // IN/OUT:
                  uint8 val)          // IN:
{
#if defined __GNUC__ && defined VM_ARM_32
   return AtomicUndefined(var + val);
#elif defined VM_ARM_64
   return _VMATOM_X(RW, 8, TRUE, &var->value, val);
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
   __asm__ __volatile__(
      "xchgb %0, %1"
      : "=q" (val),
        "+m" (var->value)
      : "0" (val)
      : "memory"
   );
   return val;
#elif defined _MSC_VER && _MSC_VER >= 1600
   return _InterlockedExchange8((volatile char *)&var->value, val);
#elif defined _MSC_VER && defined __i386__
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm movzx eax, val
      __asm mov ebx, var
      __asm xchg [ebx]Atomic_uint8.value, al
   }
#pragma warning(pop)
#elif defined _MSC_VER && defined __x86_64__
   return VMWInterlockedExchange8(&var->value, val);
#else
#error No compiler defined for Atomic_ReadWrite8
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write8 --
 *
 *      Write the specified value to the specified object atomically.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write8(Atomic_uint8 *var,  // IN/OUT:
              uint8 val)          // IN:
{
#if defined __GNUC__ && defined VM_ARM_32
   AtomicUndefined(var + val);
#elif defined VM_ARM_64
   _VMATOM_X(W, 8, &var->value, val);
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
   __asm__ __volatile__(
      "movb %1, %0"
      : "=m" (var->value)
      : "qn" (val)
   );
#elif defined _MSC_VER
   var->value = val;
#else
#error No compiler defined for Atomic_Write8
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite8 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal.
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      The variable may be modified.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadIfEqualWrite8(Atomic_uint8 *var,  // IN/OUT:
                         uint8 oldVal,       // IN:
                         uint8 newVal)       // IN:
{
#if defined __GNUC__ && defined VM_ARM_32
   return AtomicUndefined(var + oldVal + newVal);
#elif defined VM_ARM_64
   return _VMATOM_X(RIFEQW, 8, TRUE, &var->value, oldVal, newVal);
#elif defined __GNUC__ && (defined __i386__ || defined __x86_64__)
   uint8 val;

   __asm__ __volatile__(
      "lock; cmpxchgb %2, %1"
      : "=a" (val),
        "+m" (var->value)
      : "q" (newVal),
        "0" (oldVal)
      : "cc", "memory"
   );

   return val;
#elif defined _MSC_VER && _MSC_VER >= 1600
   return _InterlockedCompareExchange8((volatile char *)&var->value,
                                       newVal, oldVal);
#elif defined _MSC_VER && defined __i386__
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm mov al, oldVal
      __asm mov ebx, var
      __asm mov cl, newVal
      __asm lock cmpxchg [ebx]Atomic_uint8.value, cl
      __asm movzx eax, al
      // eax is the return value, this is documented to work - edward
   }
#pragma warning(pop)
#elif defined _MSC_VER && defined __x86_64__
   return VMWInterlockedCompareExchange8(&var->value, newVal, oldVal);
#else
#error No compiler defined for Atomic_ReadIfEqualWrite8
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadAnd8 --
 *
 *      Atomic read (returned), bitwise AND with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadAnd8(Atomic_uint8 *var, // IN/OUT
                uint8 val)         // IN
{
   uint8 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 8, TRUE, &var->value, and, val);
#else
   do {
      res = Atomic_Read8(var);
   } while (res != Atomic_ReadIfEqualWrite8(var, res, res & val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_And8 --
 *
 *      Atomic read, bitwise AND with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_And8(Atomic_uint8 *var, // IN/OUT
            uint8 val)         // IN
{
#if defined VM_ARM_64
   _VMATOM_X(OP, 8, TRUE, &var->value, and, val);
#else
   (void)Atomic_ReadAnd8(var, val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadOr8 --
 *
 *      Atomic read (returned), bitwise OR with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadOr8(Atomic_uint8 *var, // IN/OUT
               uint8 val)         // IN
{
   uint8 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 8, TRUE, &var->value, orr, val);
#else
   do {
      res = Atomic_Read8(var);
   } while (res != Atomic_ReadIfEqualWrite8(var, res, res | val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Or8 --
 *
 *      Atomic read, bitwise OR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Or8(Atomic_uint8 *var, // IN/OUT
           uint8 val)         // IN
{
#if defined VM_ARM_64
   _VMATOM_X(OP, 8, TRUE, &var->value, orr, val);
#else
   (void)Atomic_ReadOr8(var, val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadXor8 --
 *
 *      Atomic read (returned), bitwise XOR with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadXor8(Atomic_uint8 *var, // IN/OUT
                uint8 val)         // IN
{
   uint8 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 8, TRUE, &var->value, eor, val);
#else
   do {
      res = Atomic_Read8(var);
   } while (res != Atomic_ReadIfEqualWrite8(var, res, res ^ val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Xor8 --
 *
 *      Atomic read, bitwise XOR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Xor8(Atomic_uint8 *var, // IN/OUT
            uint8 val)         // IN
{
#if defined VM_ARM_64
   _VMATOM_X(OP, 8, TRUE, &var->value, eor, val);
#else
   (void)Atomic_ReadXor8(var, val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadAdd8 --
 *
 *      Atomic read (returned), add a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadAdd8(Atomic_uint8 *var, // IN/OUT
                uint8 val)         // IN
{
   uint8 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 8, TRUE, &var->value, add, val);
#else
   do {
      res = Atomic_Read8(var);
   } while (res != Atomic_ReadIfEqualWrite8(var, res, res + val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Add8 --
 *
 *      Atomic read, add a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Add8(Atomic_uint8 *var, // IN/OUT
            uint8 val)         // IN
{
#if defined VM_ARM_64
   _VMATOM_X(OP, 8, TRUE, &var->value, add, val);
#else
   (void)Atomic_ReadAdd8(var, val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Sub8 --
 *
 *      Atomic read, subtract a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Sub8(Atomic_uint8 *var, // IN/OUT
            uint8 val)         // IN
{
#if defined VM_ARM_64
   _VMATOM_X(OP, 8, TRUE, &var->value, sub, val);
#else
   Atomic_Add8(var, -val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Inc8 --
 *
 *      Atomic read, increment, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Inc8(Atomic_uint8 *var) // IN/OUT
{
   Atomic_Add8(var, 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Dec8 --
 *
 *      Atomic read, decrement, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Dec8(Atomic_uint8 *var) // IN/OUT
{
   Atomic_Sub8(var, 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadInc8 --
 *
 *      Atomic read (returned), increment, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadInc8(Atomic_uint8 *var) // IN/OUT
{
   return Atomic_ReadAdd8(var, 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadDec8 --
 *
 *      Atomic read (returned), decrement, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint8
Atomic_ReadDec8(Atomic_uint8 *var) // IN/OUT
{
   return Atomic_ReadAdd8(var, (uint8)-1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read32 --
 *
 *      Read
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_Read32(Atomic_uint32 const *var) // IN
{
   uint32 value;

#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT(((uintptr_t)var % 4) == 0);
#endif

#if defined __GNUC__
   /*
    * Use inline assembler to force using a single load instruction to
    * ensure that the compiler doesn't split a transfer operation into multiple
    * instructions.
    */

#if defined VM_ARM_32
   __asm__ __volatile__(
      "ldr %0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );
#elif defined VM_ARM_64
   value = _VMATOM_X(R, 32, &var->value);
#else
   __asm__ __volatile__(
      "mov %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
#endif
#elif defined _MSC_VER
   /*
    * Microsoft docs guarantee simple reads and writes to properly
    * aligned 32-bit variables use only a single instruction.
    * http://msdn.microsoft.com/en-us/library/ms684122%28VS.85%29.aspx
    */

   value = var->value;
#else
#error No compiler defined for Atomic_Read
#endif

   return value;
}
#define Atomic_Read Atomic_Read32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWrite32 --
 *
 *      Read followed by write
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadWrite32(Atomic_uint32 *var, // IN/OUT
                   uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 retVal;
   register volatile uint32 res;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[retVal], [%[var]] \n\t"
      "strex %[res], %[val], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [retVal] "=&r" (retVal), [res] "=&r" (res)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(RW, 32, TRUE, &var->value, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "xchgl %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "memory"
   );
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   return _InterlockedExchange((long *)&var->value, (long)val);
#else
#error No compiler defined for Atomic_ReadWrite
#endif // __GNUC__
}
#define Atomic_ReadWrite Atomic_ReadWrite32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write32 --
 *
 *      Write
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write32(Atomic_uint32 *var, // OUT
               uint32 val)         // IN
{
#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT(((uintptr_t)var % 4) == 0);
#endif

#if defined __GNUC__
#if defined VM_ARM_64
   _VMATOM_X(W, 32, &var->value, val);
#elif defined VM_ARM_32
   /*
    * Best left this way due to the intricacies of exclusive load/store
    * operations on legacy (32-bit) ARM.
    *
    * A3.4.1 ARM DDI 0406C:
    *
    * When a processor writes using any instruction other than a
    * Store-Exclusive:
    *
    * - if the write is to a physical address that is not covered by its local
    *   monitor the write does not affect the state of the local monitor
    * - if the write is to a physical address that is covered by its local
    *   monitor it is IMPLEMENTATION DEFINED whether the write affects the
    *   state of the local monitor.
    *
    * A3.4.5 ARM DDI 0406C:
    *
    * If two STREX instructions are executed without an intervening LDREX the
    * second STREX returns a status value of 1. This means that:
    *
    * - ARM recommends that, in a given thread of execution, every STREX has a
    *   preceding LDREX associated with it
    * - it is not necessary for every LDREX to have a subsequent STREX.
    */

   Atomic_ReadWrite32(var, val);
#else
   /*
    * Use inline assembler to force using a single store instruction to
    * ensure that the compiler doesn't split a transfer operation into multiple
    * instructions.
    */

   __asm__ __volatile__(
      "mov %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#endif
#elif defined _MSC_VER
   /*
    * Microsoft docs guarantee simple reads and writes to properly
    * aligned 32-bit variables use only a single instruction.
    * http://msdn.microsoft.com/en-us/library/ms684122%28VS.85%29.aspx
    */

   var->value = val;
#else
#error No compiler defined for Atomic_Write
#endif
}
#define Atomic_Write Atomic_Write32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite32 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      The variable may be modified.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadIfEqualWrite32(Atomic_uint32 *var, // IN/OUT
                          uint32 oldVal,      // IN
                          uint32 newVal)      // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register uint32 retVal;
   register uint32 res;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[retVal], [%[var]] \n\t"
      "mov %[res], #0 \n\t"
      "teq %[retVal], %[oldVal] \n\t"
      "strexeq %[res], %[newVal], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [retVal] "=&r" (retVal), [res] "=&r" (res)
      : [var] "r" (&var->value), [oldVal] "r" (oldVal), [newVal] "r" (newVal)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(RIFEQW, 32, TRUE, &var->value, oldVal, newVal);
#else /* VM_X86_ANY */
   uint32 val;

   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; cmpxchgl %2, %1"
      : "=a" (val),
        "+m" (var->value)
      : "r" (newVal),
        "0" (oldVal)
      : "cc", "memory"
   );
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   return _InterlockedCompareExchange((long *)&var->value,
                                      (long)newVal,
                                      (long)oldVal);
#else
#error No compiler defined for Atomic_ReadIfEqualWrite
#endif
}
#define Atomic_ReadIfEqualWrite Atomic_ReadIfEqualWrite32


#if defined VM_64BIT || defined VM_ARM_V7
/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite64 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      The variable may be modified.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadIfEqualWrite64(Atomic_uint64 *var, // IN/OUT
                          uint64 oldVal,      // IN
                          uint64 newVal)      // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register uint64 retVal;
   register uint32 res;

   dmb();

   /*
    * Under Apple LLVM version 5.0 (clang-500.2.76) (based on LLVM 3.3svn)
    * There will be a warning:
    * "value size does not match register size specified by the constraint
    * and modifier [-Wasm-operand-widths]"
    * on the lines:
    * : [var] "r" (&var->value), [oldVal] "r" (oldVal), [newVal] "r" (newVal)
    *                                          ^
    * : [var] "r" (&var->value), [oldVal] "r" (oldVal), [newVal] "r" (newVal)
    *                                                                 ^
    *
    * Furthermore, using a 32-bits register to store a
    * 64-bits value of an variable looks risky.
    */
#if defined __APPLE__ && __clang__ == 1 && __clang_major__ >= 5
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wasm-operand-widths"
#endif
   __asm__ __volatile__(
   "1: ldrexd %[retVal], %H[retVal], [%[var]] \n\t"
      "mov %[res], #0 \n\t"
      "teq %[retVal], %[oldVal] \n\t"
      "teqeq %H[retVal], %H[oldVal] \n\t"
      "strexdeq %[res], %[newVal], %H[newVal], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [retVal] "=&r" (retVal), [res] "=&r" (res)
      : [var] "r" (&var->value), [oldVal] "r" (oldVal), [newVal] "r" (newVal)
      : "cc"
   );
#if defined __APPLE__ && __clang__ == 1 && __clang_major__ >= 5
#pragma clang diagnostic pop
#endif // defined __APPLE__ && __clang__ == 1 && __clang_major__ >= 5
   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(RIFEQW, 64, TRUE, &var->value, oldVal, newVal);
#else /* VM_X86_64 */
   uint64 val;

   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; cmpxchgq %2, %1"
      : "=a" (val),
        "+m" (var->value)
      : "r" (newVal),
        "0" (oldVal)
      : "cc", "memory"
   );
   return val;
#endif //VM_ARM_V7
#elif defined _MSC_VER
   return _InterlockedCompareExchange64((__int64 *)&var->value,
                                        (__int64)newVal,
                                        (__int64)oldVal);
#else
#error No compiler defined for Atomic_ReadIfEqualWrite64
#endif
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_And32 --
 *
 *      Atomic read, bitwise AND with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_And32(Atomic_uint32 *var, // IN/OUT
             uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[tmp], [%[var]] \n\t"
      "and %[tmp], %[tmp], %[val] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 32, TRUE, &var->value, and, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; andl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined __x86_64__ || defined VM_ARM_32
   _InterlockedAnd((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock And [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_And
#endif
}
#define Atomic_And Atomic_And32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Or32 --
 *
 *      Atomic read, bitwise OR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Or32(Atomic_uint32 *var, // IN/OUT
            uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[tmp], [%[var]] \n\t"
      "orr %[tmp], %[tmp], %[val] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 32, TRUE, &var->value, orr, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; orl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined __x86_64__ || defined VM_ARM_32
   _InterlockedOr((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock Or [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Or
#endif
}
#define Atomic_Or Atomic_Or32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Xor32 --
 *
 *      Atomic read, bitwise XOR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Xor32(Atomic_uint32 *var, // IN/OUT
             uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[tmp], [%[var]] \n\t"
      "eor %[tmp], %[tmp], %[val] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 32, TRUE, &var->value, eor, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; xorl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined __x86_64__ || defined VM_ARM_32
   _InterlockedXor((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock Xor [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Xor
#endif
}
#define Atomic_Xor Atomic_Xor32


#if defined VM_64BIT
/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Xor64 --
 *
 *      Atomic read, bitwise XOR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Xor64(Atomic_uint64 *var, // IN/OUT
             uint64 val)         // IN
{
#if defined __GNUC__
#if defined VM_ARM_64
   _VMATOM_X(OP, 64, TRUE, &var->value, eor, val);
#else /* VM_X86_64 */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; xorq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#endif
#elif defined _MSC_VER
   _InterlockedXor64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_Xor64
#endif
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Add32 --
 *
 *      Atomic read, add a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Add32(Atomic_uint32 *var, // IN/OUT
             uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
   "1: ldrex %[tmp], [%[var]] \n\t"
      "add %[tmp], %[tmp], %[val] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 32, TRUE, &var->value, add, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; addl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   _InterlockedExchangeAdd((long *)&var->value, (long)val);
#else
#error No compiler defined for Atomic_Add
#endif
}
#define Atomic_Add Atomic_Add32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Sub32 --
 *
 *      Atomic read, subtract a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Sub32(Atomic_uint32 *var, // IN/OUT
             uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
      "1: ldrex %[tmp], [%[var]] \n\t"
      "sub %[tmp], %[tmp], %[val] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 32, TRUE, &var->value, sub, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; subl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   _InterlockedExchangeAdd((long *)&var->value, -(long)val);
#else
#error No compiler defined for Atomic_Sub
#endif
}
#define Atomic_Sub Atomic_Sub32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Inc32 --
 *
 *      Atomic read, increment, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Inc32(Atomic_uint32 *var) // IN/OUT
{
#ifdef __GNUC__
#if defined VM_ARM_ANY
   Atomic_Add32(var, 1);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; incl %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   _InterlockedIncrement((long *)&var->value);
#else
#error No compiler defined for Atomic_Inc
#endif
}
#define Atomic_Inc Atomic_Inc32


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Dec32 --
 *
 *      Atomic read, decrement, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Dec32(Atomic_uint32 *var) // IN/OUT
{
#ifdef __GNUC__
#if defined VM_ARM_ANY
   Atomic_Sub32(var, 1);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; decl %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   _InterlockedDecrement((long *)&var->value);
#else
#error No compiler defined for Atomic_Dec
#endif
}
#define Atomic_Dec Atomic_Dec32


/*
 * Note that the technique below can be used to implement ReadX(), where X is
 * an arbitrary mathematical function.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadOr32 --
 *
 *      Atomic read (returned), bitwise OR with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadOr32(Atomic_uint32 *var, // IN/OUT
                uint32 val)         // IN
{
   uint32 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 32, TRUE, &var->value, orr, val);
#else
   do {
      res = Atomic_Read32(var);
   } while (res != Atomic_ReadIfEqualWrite32(var, res, res | val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadAnd32 --
 *
 *      Atomic read (returned), bitwise And with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadAnd32(Atomic_uint32 *var, // IN/OUT
                 uint32 val)         // IN
{
   uint32 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 32, TRUE, &var->value, and, val);
#else
   do {
      res = Atomic_Read32(var);
   } while (res != Atomic_ReadIfEqualWrite32(var, res, res & val));
#endif

   return res;
}


#if defined VM_64BIT
/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadOr64 --
 *
 *      Atomic read (returned), bitwise OR with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadOr64(Atomic_uint64 *var, // IN/OUT
                uint64 val)         // IN
{
   uint64 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 64, TRUE, &var->value, orr, val);
#else
   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite64(var, res, res | val));
#endif

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadAnd64 --
 *
 *      Atomic read (returned), bitwise AND with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadAnd64(Atomic_uint64 *var, // IN/OUT
                 uint64 val)         // IN
{
   uint64 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 64, TRUE, &var->value, and, val);
#else
   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite64(var, res, res & val));
#endif

   return res;
}
#endif /* defined VM_64BIT */


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadAdd32 --
 *
 *      Atomic read (returned), add a value, write.
 *
 *      If you have to implement ReadAdd32() on an architecture other than
 *      x86 or x86-64, you might want to consider doing something similar to
 *      Atomic_ReadOr32().
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadAdd32(Atomic_uint32 *var, // IN/OUT
                 uint32 val)         // IN
{
#if defined __GNUC__
#ifdef VM_ARM_V7
   register volatile uint32 res;
   register volatile uint32 retVal;
   register volatile uint32 tmp;

   dmb();

   __asm__ __volatile__(
      "1: ldrex %[retVal], [%[var]] \n\t"
      "add %[tmp], %[val], %[retVal] \n\t"
      "strex %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [tmp] "=&r" (tmp), [res] "=&r" (res), [retVal] "=&r" (retVal)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(ROP, 32, TRUE, &var->value, add, val);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; xaddl %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "cc", "memory"
   );
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
   return _InterlockedExchangeAdd((long *)&var->value, (long)val);
#else
#error No compiler defined for Atomic_ReadAdd32
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadInc32 --
 *
 *      Atomic read (returned), increment, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadInc32(Atomic_uint32 *var) // IN/OUT
{
   return Atomic_ReadAdd32(var, 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadDec32 --
 *
 *      Atomic read (returned), decrement, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint32
Atomic_ReadDec32(Atomic_uint32 *var) // IN/OUT
{
   return Atomic_ReadAdd32(var, (uint32)-1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_CMPXCHG64 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 * Results:
 *      TRUE if equal, FALSE if not equal
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_CMPXCHG64(Atomic_uint64 *var,   // IN/OUT
                 uint64 oldVal,        // IN
                 uint64 newVal)        // IN
{
#if defined __GNUC__
#if defined VM_ARM_ANY
   return Atomic_ReadIfEqualWrite64(var, oldVal, newVal) == oldVal;
#else /* VM_X86_ANY */

   Bool equal;
   /* Checked against the Intel manual and GCC --walken */
#if defined __x86_64__
   uint64 dummy;
   __asm__ __volatile__(
      "lock; cmpxchgq %3, %0" "\n\t"
      "sete %1"
      : "+m" (*var),
        "=qm" (equal),
        "=a" (dummy)
      : "r" (newVal),
        "2" (oldVal)
      : "cc", "memory"
   );
#else /* 32-bit version for non-ARM */
   typedef struct {
      uint32 lowValue;
      uint32 highValue;
   } S_uint64;

   int dummy1, dummy2;
#   if defined __PIC__
   /*
    * Rules for __asm__ statements in __PIC__ code
    * --------------------------------------------
    *
    * The compiler uses %ebx for __PIC__ code, so an __asm__ statement cannot
    * clobber %ebx. The __asm__ statement can temporarily modify %ebx, but _for
    * each parameter that is used while %ebx is temporarily modified_:
    *
    * 1) The constraint cannot be "m", because the memory location the compiler
    *    chooses could then be relative to %ebx.
    *
    * 2) The constraint cannot be a register class which contains %ebx (such as
    *    "r" or "q"), because the register the compiler chooses could then be
    *    %ebx. (This happens when compiling the Fusion UI with gcc 4.2.1, Apple
    *    build 5577.)
    *
    * 3) Using register classes even for other values is problematic, as gcc
    *    can decide e.g. %ecx == %edi == 0 (as compile-time constants) and
    *    ends up using one register for two things. Which breaks xchg's ability
    *    to temporarily put the PIC pointer somewhere else. PR772455
    *
    * For that reason alone, the __asm__ statement should keep the regions
    * where it temporarily modifies %ebx as small as possible, and should
    * prefer specific register assignments.
    */
   __asm__ __volatile__(
      "xchgl %%ebx, %6"      "\n\t"
      "lock; cmpxchg8b (%3)" "\n\t"
      "xchgl %%ebx, %6"      "\n\t"
      "sete %0"
      : "=qm" (equal),
        "=a" (dummy1),
        "=d" (dummy2)
      : /*
         * See the "Rules for __asm__ statements in __PIC__ code" above: %3
         * must use a register class which does not contain %ebx.
         * "a"/"c"/"d" are already used, so we are left with either "S" or "D".
         *
         * Note that this assembly uses ALL GP registers (with %esp reserved for
         * stack, %ebp reserved for frame, %ebx reserved for PIC).
         */
        "S" (var),
        "1" (((S_uint64 *)&oldVal)->lowValue),
        "2" (((S_uint64 *)&oldVal)->highValue),
        "D" (((S_uint64 *)&newVal)->lowValue),
        "c" (((S_uint64 *)&newVal)->highValue)
      : "cc", "memory"
   );
#   else
   __asm__ __volatile__(
      "lock; cmpxchg8b %0" "\n\t"
      "sete %1"
      : "+m" (*var),
        "=qm" (equal),
        "=a" (dummy1),
        "=d" (dummy2)
      : "2" (((S_uint64 *)&oldVal)->lowValue),
        "3" (((S_uint64 *)&oldVal)->highValue),
        "b" (((S_uint64 *)&newVal)->lowValue),
        "c" (((S_uint64 *)&newVal)->highValue)
      : "cc", "memory"
   );
#   endif
#endif
   return equal;
#endif //VM_ARM_V7
#elif defined _MSC_VER
   return (__int64)oldVal == _InterlockedCompareExchange64((__int64 *)&var->value,
                                                           (__int64)newVal,
                                                           (__int64)oldVal);
#else
#error No compiler defined for Atomic_CMPXCHG64
#endif // !GNUC
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_CMPXCHG32 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 * Results:
 *      TRUE if equal, FALSE if not equal
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_CMPXCHG32(Atomic_uint32 *var,   // IN/OUT
                 uint32 oldVal,        // IN
                 uint32 newVal)        // IN
{
#if defined __GNUC__
#if defined VM_ARM_ANY
   return Atomic_ReadIfEqualWrite32(var, oldVal, newVal) == oldVal;
#else /* VM_X86_ANY */
   Bool equal;
   uint32 dummy;

   __asm__ __volatile__(
      "lock; cmpxchgl %3, %0" "\n\t"
      "sete %1"
      : "+m" (*var),
        "=qm" (equal),
        "=a" (dummy)
      : "r" (newVal),
        "2" (oldVal)
      : "cc", "memory"
   );
   return equal;
#endif /* VM_X86_ANY */
#else // defined __GNUC__
   return Atomic_ReadIfEqualWrite32(var, oldVal, newVal) == oldVal;
#endif // !defined __GNUC__
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read64 --
 *
 *      Read and return.
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_Read64(Atomic_uint64 const *var) // IN
{
#if defined __GNUC__
   uint64 value;
#endif

#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT((uintptr_t)var % 8 == 0);
#endif

#if defined __GNUC__ && defined __x86_64__
   /*
    * Use asm to ensure we emit a single load.
    */
   __asm__ __volatile__(
      "movq %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
#elif defined __GNUC__ && defined __i386__
   /*
    * Since cmpxchg8b will replace the contents of EDX:EAX with the
    * value in memory if there is no match, we need only execute the
    * instruction once in order to atomically read 64 bits from
    * memory.  The only constraint is that ECX:EBX must have the same
    * value as EDX:EAX so that if the comparison succeeds.  We
    * intentionally don't tell gcc that we are using ebx and ecx as we
    * don't modify them and do not care what value they store.
    */
   __asm__ __volatile__(
      "mov %%ebx, %%eax"   "\n\t"
      "mov %%ecx, %%edx"   "\n\t"
      "lock; cmpxchg8b %1"
      : "=&A" (value)
      : "m" (*var)
      : "cc"
   );
#elif defined _MSC_VER && defined __x86_64__
   /*
    * Microsoft docs guarantee "Simple reads and writes to properly
    * aligned 64-bit variables are atomic on 64-bit Windows."
    * http://msdn.microsoft.com/en-us/library/ms684122%28VS.85%29.aspx
    *
    * XXX Verify that value is properly aligned. Bug 61315.
    */
   return var->value;
#elif defined _MSC_VER && defined VM_ARM_32
   return _InterlockedAdd64((__int64 *)&var->value, 0);
#elif defined _MSC_VER && defined __i386__
#   pragma warning(push)
#   pragma warning(disable : 4035)      // disable no-return warning
   {
      __asm mov ecx, var
      __asm mov edx, ecx
      __asm mov eax, ebx
      __asm lock cmpxchg8b [ecx]
      // edx:eax is the return value; this is documented to work. --mann
   }
#   pragma warning(pop)
#elif defined __GNUC__ && defined VM_ARM_V7
   __asm__ __volatile__(
      "ldrexd %[value], %H[value], [%[var]] \n\t"
      : [value] "=&r" (value)
      : [var] "r" (&var->value)
   );
#elif defined VM_ARM_64
   value = _VMATOM_X(R, 64, &var->value);
#endif

#if defined __GNUC__
   return value;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadUnaligned64 --
 *
 *      Atomically read a 64 bit integer, possibly misaligned.
 *      This function can be *very* expensive, costing over 50 kcycles
 *      on Nehalem.
 *
 *      Note that "var" needs to be writable, even though it will not
 *      be modified.
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

#if defined VM_64BIT
static INLINE uint64
Atomic_ReadUnaligned64(Atomic_uint64 const *var)  // IN:
{
   return Atomic_ReadIfEqualWrite64((Atomic_uint64*)var, 0, 0);
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadAdd64 --
 *
 *      Atomically adds a 64-bit integer to another
 *
 * Results:
 *      Returns the old value just prior to the addition
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadAdd64(Atomic_uint64 *var, // IN/OUT
                 uint64 val)         // IN
{
#if defined VM_ARM_64
   return _VMATOM_X(ROP, 64, TRUE, &var->value, add, val);
#elif defined __x86_64__

#if defined __GNUC__
   __asm__ __volatile__(
      "lock; xaddq %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "cc", "memory"
   );
   return val;
#elif defined _MSC_VER
   return _InterlockedExchangeAdd64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_ReadAdd64
#endif

#else
   uint64 oldVal;
   uint64 newVal;

   do {
      oldVal = var->value;
      newVal = oldVal + val;
   } while (!Atomic_CMPXCHG64(var, oldVal, newVal));

   return oldVal;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadSub64 --
 *
 *      Atomically subtracts a 64-bit integer from another.
 *
 *      Note: It is expected that val <= var.  If untrue, the result
 *            cannot be represented in an unsigned type.
 *
 * Results:
 *      Returns the old value just prior to the subtraction
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadSub64(Atomic_uint64 *var, // IN/OUT
                 uint64 val)         // IN
{
#if defined VM_ARM_64
   return _VMATOM_X(ROP, 64, TRUE, &var->value, sub, val);
#else
   return Atomic_ReadAdd64(var, (uint64)-(int64)val);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadInc64 --
 *
 *      Atomically increments a 64-bit integer
 *
 * Results:
 *      Returns the old value just prior to incrementing
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadInc64(Atomic_uint64 *var) // IN/OUT
{
   return Atomic_ReadAdd64(var, 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadDec64 --
 *
 *      Atomically decrements a 64-bit integer
 *
 * Results:
 *      Returns the old value just prior to decrementing
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadDec64(Atomic_uint64 *var) // IN/OUT
{
   return Atomic_ReadAdd64(var, (uint64)CONST64(-1));
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Add64 --
 *
 *      Atomic read, add a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Add64(Atomic_uint64 *var, // IN/OUT
             uint64 val)         // IN
{
#if !defined VM_64BIT
   Atomic_ReadAdd64(var, val); /* Return value is unused. */
#elif defined __GNUC__
#if defined VM_ARM_64
   _VMATOM_X(OP, 64, TRUE, &var->value, add, val);
#else /* defined VM_X86_64 */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; addq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#endif
#elif defined _MSC_VER
   _InterlockedExchangeAdd64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_Add64
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Sub64 --
 *
 *      Atomic read, subtract a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Sub64(Atomic_uint64 *var, // IN/OUT
             uint64 val)         // IN
{
#if !defined VM_64BIT
   Atomic_ReadSub64(var, val); /* Return value is unused. */
#elif defined __GNUC__
#if defined VM_ARM_64
   _VMATOM_X(OP, 64, TRUE, &var->value, sub, val);
#else /* VM_X86_64 */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; subq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#endif
#elif defined _MSC_VER
   _InterlockedExchangeAdd64((__int64 *)&var->value, (__int64)-val);
#else
#error No compiler defined for Atomic_Sub64
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Inc64 --
 *
 *      Atomic read, increment, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Inc64(Atomic_uint64 *var) // IN/OUT
{
#if defined VM_ARM_64
   Atomic_Add64(var, 1);
#elif !defined __x86_64__
   Atomic_ReadInc64(var);  /* Return value is unused. */
#elif defined __GNUC__
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; incq %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#elif defined _MSC_VER
   _InterlockedIncrement64((__int64 *)&var->value);
#else
#error No compiler defined for Atomic_Inc64
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Dec64 --
 *
 *      Atomic read, decrement, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Dec64(Atomic_uint64 *var) // IN/OUT
{
#if defined VM_ARM_64
   Atomic_Sub64(var, 1);
#elif !defined __x86_64__
   Atomic_ReadDec64(var);  /* Return value is unused. */
#elif defined __GNUC__
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; decq %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#elif defined _MSC_VER
   _InterlockedDecrement64((__int64 *)&var->value);
#else
#error No compiler defined for Atomic_Dec64
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWrite64 --
 *
 *      Read followed by write
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
Atomic_ReadWrite64(Atomic_uint64 *var, // IN/OUT
                   uint64 val)         // IN
{
#if defined __x86_64__
#if defined __GNUC__
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "xchgq %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "memory"
   );
   return val;
#elif defined _MSC_VER
   return _InterlockedExchange64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_ReadWrite64
#endif
#elif defined VM_ARM_64
   return _VMATOM_X(RW, 64, TRUE, &var->value, val);
#else
   uint64 oldVal;

   do {
      oldVal = var->value;
   } while (!Atomic_CMPXCHG64(var, oldVal, val));

   return oldVal;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write64 --
 *
 *      Write
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write64(Atomic_uint64 *var, // OUT
               uint64 val)         // IN
{
#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT((uintptr_t)var % 8 == 0);
#endif

#if defined __x86_64__
#if defined __GNUC__
   /*
    * There is no move instruction for 64-bit immediate to memory, so unless
    * the immediate value fits in 32-bit (i.e. can be sign-extended), GCC
    * breaks the assignment into two movl instructions.  The code below forces
    * GCC to load the immediate value into a register first.
    */

   __asm__ __volatile__(
      "movq %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#elif defined _MSC_VER
   /*
    * Microsoft docs guarantee "Simple reads and writes to properly aligned
    * 64-bit variables are atomic on 64-bit Windows."
    * http://msdn.microsoft.com/en-us/library/ms684122%28VS.85%29.aspx
    *
    * XXX Verify that value is properly aligned. Bug 61315.
    */

   var->value = val;
#else
#error No compiler defined for Atomic_Write64
#endif
#elif defined VM_ARM_64
   _VMATOM_X(W, 64, &var->value, val);
#else
   (void)Atomic_ReadWrite64(var, val);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Or64 --
 *
 *      Atomic read, bitwise OR with a 64-bit value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Or64(Atomic_uint64 *var, // IN/OUT
            uint64 val)         // IN
{
#if defined __x86_64__
#if defined __GNUC__
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; orq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined _MSC_VER
   _InterlockedOr64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_Or64
#endif
#elif defined VM_ARM_64
   _VMATOM_X(OP, 64, TRUE, &var->value, orr, val);
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   do {
      oldVal = var->value;
      newVal = oldVal | val;
   } while (!Atomic_CMPXCHG64(var, oldVal, newVal));
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_And64 --
 *
 *      Atomic read, bitwise AND with a 64-bit value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_And64(Atomic_uint64 *var, // IN/OUT
             uint64 val)         // IN
{
#if defined __x86_64__
#if defined __GNUC__
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; andq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined _MSC_VER
   _InterlockedAnd64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_And64
#endif
#elif defined VM_ARM_64
   _VMATOM_X(OP, 64, TRUE, &var->value, and, val);
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   do {
      oldVal = var->value;
      newVal = oldVal & val;
   } while (!Atomic_CMPXCHG64(var, oldVal, newVal));
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_SetBit64 --
 *
 *      Atomically set the bit 'bit' in var.  Bit must be between 0 and 63.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_SetBit64(Atomic_uint64 *var, // IN/OUT
                unsigned bit)       // IN
{
#if defined __x86_64__ && defined __GNUC__
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "lock; btsq %1, %0"
      : "+m" (var->value)
      : "ri" ((uint64)bit)
      : "cc", "memory"
   );
#else
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal | (CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, oldVal, newVal));
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ClearBit64 --
 *
 *      Atomically clear the bit 'bit' in var.  Bit must be between 0 and 63.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_ClearBit64(Atomic_uint64 *var, // IN/OUT
                  unsigned bit)       // IN
{
#if defined __x86_64__ && defined __GNUC__
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "lock; btrq %1, %0"
      : "+m" (var->value)
      : "ri" ((uint64)bit)
      : "cc", "memory"
   );
#else
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal & ~(CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, oldVal, newVal));
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_TestBit64 --
 *
 *      Read the bit 'bit' in var.  Bit must be between 0 and 63.
 *
 * Results:
 *      TRUE if the tested bit was set; else FALSE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_TestBit64(Atomic_uint64 *var, // IN
                 unsigned bit)       // IN
{
   Bool out;
   ASSERT(bit <= 63);
#if defined __x86_64__ && defined __GNUC__
   __asm__ __volatile__(
      "btq %2, %1; setc %0"
      : "=rm"(out)
      : "m" (var->value),
        "rJ" ((uint64)bit)
      : "cc"
   );
#else
   out = (var->value & (CONST64U(1) << bit)) != 0;
#endif
   return out;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_TestSetBit64 --
 *
 *      Atomically test and set the bit 'bit' in var.
 *      Bit must be between 0 and 63.
 *
 * Results:
 *      TRUE if the tested bit was set; else FALSE.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_TestSetBit64(Atomic_uint64 *var, // IN/OUT
                    unsigned bit)       // IN
{
#if defined __x86_64__ && defined __GNUC__
   Bool out;
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "lock; btsq %2, %1; setc %0"
      : "=rm" (out), "+m" (var->value)
      : "rJ" ((uint64)bit)
      : "cc", "memory"
   );
   return out;
#else
   uint64 oldVal;
   uint64 mask;
   ASSERT(bit <= 63);
   mask = CONST64U(1) << bit;
   do {
      oldVal = var->value;
   } while (!Atomic_CMPXCHG64(var, oldVal, oldVal | mask));
   return (oldVal & mask) != 0;
#endif
}


#if defined __GNUC__
/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read16 --
 *
 *      Read and return.
 *
 * Results:
 *      The value of the atomic variable.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Atomic_Read16(Atomic_uint16 const *var) // IN
{
   uint16 value;

#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT((uintptr_t)var % 2 == 0);
#endif

#if defined __GNUC__
#if defined __x86_64__ || defined __i386__

   __asm__ __volatile__(
      "movw %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
#elif defined VM_ARM_V7
   NOT_TESTED();

   __asm__ __volatile__(
      "ldrh %0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );
#elif defined VM_ARM_64
   value = _VMATOM_X(R, 16, &var->value);
#else
#error No 16-bits atomics.
#endif
#endif

   return value;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWrite16 --
 *
 *      Read followed by write
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadWrite16(Atomic_uint16 *var,  // IN/OUT:
                   uint16 val)          // IN:
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "xchgw %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "memory"
   );
   return val;
#elif defined VM_ARM_V7
   register volatile uint16 retVal;
   register volatile uint16 res;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[retVal], [%[var]] \n\t"
      "strexh %[res], %[val], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [retVal] "=&r" (retVal), [res] "=&r" (res)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(RW, 16, TRUE, &var->value, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write16 --
 *
 *      Write
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Write16(Atomic_uint16 *var,  // OUT:
               uint16 val)          // IN:
{
#if defined VMM || defined VM_ARM_64 || defined VMKERNEL || defined VMKERNEL_MODULE
   ASSERT((uintptr_t)var % 2 == 0);
#endif

#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "movw %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#elif defined VM_ARM_64
   _VMATOM_X(W, 16, &var->value, val);
#elif defined VM_ARM_32
   /*
    * Best left this way due to the intricacies of exclusive load/store
    * operations on legacy (32-bit) ARM.
    */
   Atomic_ReadWrite16(var, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite16 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 * Results:
 *      The value of the atomic variable before the write.
 *
 * Side effects:
 *      The variable may be modified.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadIfEqualWrite16(Atomic_uint16 *var,   // IN/OUT
                          uint16 oldVal,        // IN
                          uint16 newVal)        // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   uint16 val;

   __asm__ __volatile__(
      "lock; cmpxchgw %2, %1"
      : "=a" (val),
        "+m" (var->value)
      : "r" (newVal),
        "0" (oldVal)
      : "cc", "memory"
   );
   return val;
#elif defined VM_ARM_V7
   register uint16 retVal;
   register uint16 res;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[retVal], [%[var]] \n\t"
      "mov %[res], #0 \n\t"
      "teq %[retVal], %[oldVal] \n\t"
      "strexheq %[res], %[newVal], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [retVal] "=&r" (retVal), [res] "=&r" (res)
      : [var] "r" (&var->value), [oldVal] "r" (oldVal), [newVal] "r" (newVal)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(RIFEQW, 16, TRUE, &var->value, oldVal, newVal);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_And16 --
 *
 *      Atomic read, bitwise AND with a 16-bit value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_And16(Atomic_uint16 *var, // IN/OUT
             uint16 val)         // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; andw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[tmp], [%[var]] \n\t"
      "and %[tmp], %[tmp], %[val] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 16, TRUE, &var->value, and, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Or16 --
 *
 *      Atomic read, bitwise OR with a 16-bit value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Or16(Atomic_uint16 *var, // IN/OUT
            uint16 val)         // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; orw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[tmp], [%[var]] \n\t"
      "orr %[tmp], %[tmp], %[val] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 16, TRUE, &var->value, orr, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Xor16 --
 *
 *      Atomic read, bitwise XOR with a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Xor16(Atomic_uint16 *var, // IN/OUT
             uint16 val)         // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; xorw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[tmp], [%[var]] \n\t"
      "eor %[tmp], %[tmp], %[val] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 16, TRUE, &var->value, eor, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Add16 --
 *
 *      Atomic read, add a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Add16(Atomic_uint16 *var, // IN/OUT
             uint16 val)         // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; addw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
   "1: ldrexh %[tmp], [%[var]] \n\t"
      "add %[tmp], %[tmp], %[val] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 16, TRUE, &var->value, add, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Sub16 --
 *
 *      Atomic read, subtract a value, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Sub16(Atomic_uint16 *var, // IN/OUT
             uint16 val)         // IN
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; subw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc", "memory"
   );
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
      "1: ldrexh %[tmp], [%[var]] \n\t"
      "sub %[tmp], %[tmp], %[val] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [res] "=&r" (res), [tmp] "=&r" (tmp)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();
#elif defined VM_ARM_64
   _VMATOM_X(OP, 16, TRUE, &var->value, sub, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Inc16 --
 *
 *      Atomic read, increment, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Inc16(Atomic_uint16 *var) // IN/OUT
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; incw %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#elif defined VM_ARM_ANY
   Atomic_Add16(var, 1);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Dec16 --
 *
 *      Atomic read, decrement, write.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_Dec16(Atomic_uint16 *var) // IN/OUT
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; decw %0"
      : "+m" (var->value)
      :
      : "cc", "memory"
   );
#elif defined VM_ARM_ANY
   Atomic_Sub16(var, 1);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadOr16 --
 *
 *      Atomic read (returned), bitwise OR with a value, write.
 *
 * Results:
 *      The value of the variable before the operation.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadOr16(Atomic_uint16 *var, // IN/OUT
                uint16 val)         // IN
{
   uint16 res;

#if defined VM_ARM_64
   res = _VMATOM_X(ROP, 16, TRUE, &var->value, orr, val);
#else
   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite16(var, res, res | val));
#endif

   return res;
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadAdd16 --
 *
 *      Atomically adds a 16-bit integer to another
 *
 * Results:
 *      Returns the old value just prior to the addition
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadAdd16(Atomic_uint16 *var,  // IN/OUT
                 uint16 val)          // IN:
{
#if defined __GNUC__
#if defined __x86_64__ || defined __i386__
   __asm__ __volatile__(
      "lock; xaddw %0, %1"
      : "=r" (val),
        "+m" (var->value)
      : "0" (val)
      : "cc", "memory"
   );
   return val;
#elif defined VM_ARM_V7
   register volatile uint16 res;
   register volatile uint16 retVal;
   register volatile uint16 tmp;

   NOT_TESTED();

   dmb();

   __asm__ __volatile__(
      "1: ldrexh %[retVal], [%[var]] \n\t"
      "add %[tmp], %[val], %[retVal] \n\t"
      "strexh %[res], %[tmp], [%[var]] \n\t"
      "teq %[res], #0 \n\t"
      "bne 1b"
      : [tmp] "=&r" (tmp), [res] "=&r" (res), [retVal] "=&r" (retVal)
      : [var] "r" (&var->value), [val] "r" (val)
      : "cc"
   );

   dmb();

   return retVal;
#elif defined VM_ARM_64
   return _VMATOM_X(ROP, 16, TRUE, &var->value, add, val);
#else
#error No 16-bits atomics.
#endif
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadInc16 --
 *
 *      Atomically increments a 64-bit integer
 *
 * Results:
 *      Returns the old value just prior to incrementing
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadInc16(Atomic_uint16 *var) // IN/OUT
{
   return Atomic_ReadAdd16(var, 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadDec16 --
 *
 *      Atomically decrements a 64-bit integer
 *
 * Results:
 *      Returns the old value just prior to decrementing
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static INLINE uint16
Atomic_ReadDec16(Atomic_uint16 *var) // IN/OUT
{
   return Atomic_ReadAdd16(var, (uint16)-1);
}
#endif

/*
 * Template code for the Atomic_<name> type and its operators.
 *
 * The cast argument is an intermediate type cast to make some
 * compilers stop complaining about casting uint32 <-> void *,
 * even though we only do it in the 32-bit case so they are always
 * the same size.  So for val of type uint32, instead of
 * (void *)val, we have (void *)(uintptr_t)val.
 * The specific problem case is the Windows ddk compiler
 * (as used by the SVGA driver).  -- edward
 *
 * NOTE: See the comment in vm_assert.h for why we need UNUSED_TYPE in
 * AtomicAssertOnCompile(), and why we need to be very careful doing so.
 */

#define MAKE_ATOMIC_TYPE(name, size, in, out, cast)                           \
   typedef Atomic_uint ## size Atomic_ ## name;                               \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   AtomicAssertOnCompile ## name(void)                                        \
   {                                                                          \
      enum { AssertOnCompileMisused =    8 * sizeof (in) == size              \
                                      && 8 * sizeof (out) == size             \
                                      && 8 * sizeof (cast) == size            \
                                         ? 1 : -1 };                          \
      UNUSED_TYPE(typedef char AssertOnCompileFailed[AssertOnCompileMisused]);\
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_Read ## name(Atomic_ ## name const *var)                            \
   {                                                                          \
      return (out)(cast)Atomic_Read ## size(var);                             \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Write ## name(Atomic_ ## name *var,                                 \
                        in val)                                               \
   {                                                                          \
      Atomic_Write ## size(var, (uint ## size)(cast)val);                     \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadWrite ## name(Atomic_ ## name *var,                             \
                            in val)                                           \
   {                                                                          \
      return (out)(cast)Atomic_ReadWrite ## size(var,                         \
                (uint ## size)(cast)val);                                     \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadIfEqualWrite ## name(Atomic_ ## name *var,                      \
                                   in oldVal,                                 \
                                   in newVal)                                 \
   {                                                                          \
      return (out)(cast)Atomic_ReadIfEqualWrite ## size(var,                  \
                (uint ## size)(cast)oldVal, (uint ## size)(cast)newVal);      \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_And ## name(Atomic_ ## name *var,                                   \
                      in val)                                                 \
   {                                                                          \
      Atomic_And ## size(var, (uint ## size)(cast)val);                       \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Or ## name(Atomic_ ## name *var,                                    \
                     in val)                                                  \
   {                                                                          \
      Atomic_Or ## size(var, (uint ## size)(cast)val);                        \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Xor ## name(Atomic_ ## name *var,                                   \
                      in val)                                                 \
   {                                                                          \
      Atomic_Xor ## size(var, (uint ## size)(cast)val);                       \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Add ## name(Atomic_ ## name *var,                                   \
                      in val)                                                 \
   {                                                                          \
      Atomic_Add ## size(var, (uint ## size)(cast)val);                       \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Sub ## name(Atomic_ ## name *var,                                   \
                      in val)                                                 \
   {                                                                          \
      Atomic_Sub ## size(var, (uint ## size)(cast)val);                       \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Inc ## name(Atomic_ ## name *var)                                   \
   {                                                                          \
      Atomic_Inc ## size(var);                                                \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE void                                                         \
   Atomic_Dec ## name(Atomic_ ## name *var)                                   \
   {                                                                          \
      Atomic_Dec ## size(var);                                                \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadOr ## name(Atomic_ ## name *var,                                \
                         in val)                                              \
   {                                                                          \
      return (out)(cast)Atomic_ReadOr ## size(var, (uint ## size)(cast)val);  \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadAdd ## name(Atomic_ ## name *var,                               \
                          in val)                                             \
   {                                                                          \
      return (out)(cast)Atomic_ReadAdd ## size(var, (uint ## size)(cast)val); \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadInc ## name(Atomic_ ## name *var)                               \
   {                                                                          \
      return (out)(cast)Atomic_ReadInc ## size(var);                          \
   }                                                                          \
                                                                              \
                                                                              \
   static INLINE out                                                          \
   Atomic_ReadDec ## name(Atomic_ ## name *var)                               \
   {                                                                          \
      return (out)(cast)Atomic_ReadDec ## size(var);                          \
   }


/*
 * Since we use a macro to generate these definitions, it is hard to look for
 * them. So DO NOT REMOVE THIS COMMENT and keep it up-to-date. --hpreg
 *
 * Atomic_Ptr
 * Atomic_ReadPtr --
 * Atomic_WritePtr --
 * Atomic_ReadWritePtr --
 * Atomic_ReadIfEqualWritePtr --
 * Atomic_AndPtr --
 * Atomic_OrPtr --
 * Atomic_XorPtr --
 * Atomic_AddPtr --
 * Atomic_SubPtr --
 * Atomic_IncPtr --
 * Atomic_DecPtr --
 * Atomic_ReadOrPtr --
 * Atomic_ReadAddPtr --
 * Atomic_ReadIncPtr --
 * Atomic_ReadDecPtr --
 *
 * Atomic_Int
 * Atomic_ReadInt --
 * Atomic_WriteInt --
 * Atomic_ReadWriteInt --
 * Atomic_ReadIfEqualWriteInt --
 * Atomic_AndInt --
 * Atomic_OrInt --
 * Atomic_XorInt --
 * Atomic_AddInt --
 * Atomic_SubInt --
 * Atomic_IncInt --
 * Atomic_DecInt --
 * Atomic_ReadOrInt --
 * Atomic_ReadAddInt --
 * Atomic_ReadIncInt --
 * Atomic_ReadDecInt --
 *
 * Atomic_Bool
 * Atomic_ReadBool --
 * Atomic_WriteBool --
 * Atomic_ReadWriteBool --
 * Atomic_ReadIfEqualWriteBool --
 * Atomic_AndBool --
 * Atomic_OrBool --
 * Atomic_XorBool --
 * Atomic_AddBool --
 * Atomic_SubBool --
 * Atomic_IncBool --
 * Atomic_DecBool --
 * Atomic_ReadOrBool --
 * Atomic_ReadAddBool --
 * Atomic_ReadIncBool --
 * Atomic_ReadDecBool --
 */
#if defined VM_64BIT
MAKE_ATOMIC_TYPE(Ptr, 64, void const *, void *, uintptr_t)
#else
MAKE_ATOMIC_TYPE(Ptr, 32, void const *, void *, uintptr_t)
#endif
MAKE_ATOMIC_TYPE(Int, 32, int, int, int)
MAKE_ATOMIC_TYPE(Bool, 8, Bool, Bool, Bool)

/*
 * Define arbitrary sized bit vector to be used by
 * Atomic_TestSetBitVector and Atomic_TestClearBitVector.
 */
#define ATOMIC_BITVECTOR(varName, capacity) \
      Atomic_uint8 varName[CEILING(capacity, 8)]

/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_TestSetBitVector --
 *
 *      Atomically test and set the bit 'index' in bit vector var.
 *
 *      The index input value specifies which bit to modify and is 0-based.
 *
 * Results:
 *      Returns the value of the bit before modification.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_TestSetBitVector(Atomic_uint8 *var, // IN/OUT
                        unsigned index)    // IN
{
#if defined __x86_64__ && defined __GNUC__
   Bool bit;
   __asm__ __volatile__(
      "lock; bts %2, %1;"
      "setc %0"
      : "=qQm" (bit), "+m" (var->value)
      : "rI" (index)
      : "cc", "memory"
   );
   return bit;
#else
   uint8 bit = 1 << index % 8;
   return (Atomic_ReadOr8(var + index / 8, bit) & bit) != 0;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_TestClearBitVector --
 *
 *      Atomically test and clear the bit 'index' in bit vector var.
 *
 *      The index input value specifies which bit to modify and is 0-based.
 *
 * Results:
 *      Returns the value of the bit before modification.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_TestClearBitVector(Atomic_uint8 *var, // IN/OUT
                          unsigned index)    // IN
{
#if defined __x86_64__ && defined __GNUC__
   Bool bit;
   __asm__ __volatile__(
      "lock; btr %2, %1;"
      "setc %0"
      : "=qQm" (bit), "+m" (var->value)
      : "rI" (index)
      : "cc", "memory"
   );
   return bit;
#else
   uint8 bit = 1 << index % 8;
   return (Atomic_ReadAnd8(var + index / 8, ~bit) & bit) != 0;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_TestBitVector --
 *
 *      Test the bit 'index' (zero-based) in bit vector var.
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Atomic_TestBitVector(const Atomic_uint8 *var, // IN
                     unsigned index)          // IN
{
   uint8 bit = 1 << index % 8;
   return (Atomic_Read8(var + index / 8) & bit) != 0;
}


#ifdef VM_ARM_64
#   include "vm_atomic_arm64_end.h"
#endif

#if defined __cplusplus
}  // extern "C"
#endif

#endif // ifndef _ATOMIC_H_
