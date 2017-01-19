/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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

/*
 * In the Atomic_* definitions below, memory ordering and atomicity are somewhat
 * conflated in an inconsistent manner. First, we have Atomic_{Read,Write},
 * which only guarantees single copy atomicity, i.e. that the read/write occurs
 * in an atomic fashion, but have no implication on memory ordering. The second
 * class of Atomics are all the non-unfenced operations excluding
 * Atomic_{Read,Write}*, which both imply atomicity and act as a memory barrier,
 * implying sequentially consistent ordering of the atomic operation with all
 * loads/stores prior to and after it.
 *
 * Since on x86, the second class of operations are associated with LOCK
 * semantics, assumptions have been made about the ordering these operations
 * imply on surrounding code (see for example the vmkernel's RefCount
 * implementation). As a result, on arm64 we have to provide these same
 * guarantees. We do this by making use of DMB barriers both before and after
 * the atomic ldrx/strx sequences. A barrier before and after is required to
 * avoid having part of the atomic operation reordered with surrounding code,
 * e.g. a store-load reordering of the strx with a following load outside the
 * Atomic_ op. For the first class of operations, Atomic_{Read,Write}, we do not
 * implement a barrier.
 *
 * This implementation of Atomic operations is suboptimal on arm64, since
 * both atomicity and memory ordering are fused together. Ideally the Atomic
 * operations would only imply atomicity, and an explicit memory barrier in the
 * surrounding code used to enforce ordering where necessary. This would eschew
 * the need for the DMBs. A middle ground can be implemented where we use the
 * arm64 load-acquire/store-release exclusive instructions to implement Atomics.
 * This would imply sequential consistency of the Atomic operations (but not
 * with any of the surrounding non-atomic operations) without the need for a
 * DMB. Using these without a DMB today can still result in problematic
 * reordering by the processor with surrounding non-atomic operations, e.g. a
 * store-load reordering with a stlxr. Future optimization for arm64 should
 * consider the wider change required at the call sites to minimize DMBs.
 *
 * For further details on x86 and ARM memory ordering see
 * https://wiki.eng.vmware.com/ARM/MemoryOrdering.
 */

#ifdef VM_ARM_64
#include "vm_basic_asm_arm64.h"

#define ARM_64_ATOMIC_16_OPV_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint16 _sample;                     \
   __asm__ __volatile__ (              \
      "1: ldxrh   %w0, [%2]      \n\t" \
      #op "       %w0, %w0, %w3  \n\t" \
      "   stxrh   %w1, %w0, [%2] \n\t" \
      "   cbnz    %w1, 1b        \n\t" \
      : "=&r" (_sample),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
})

#define ARM_64_ATOMIC_16_OPO_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint16 _newval;                     \
   uint16 _oldval;                     \
   __asm__ __volatile__ (              \
      "1: ldxrh   %w0, [%3]      \n\t" \
      #op "       %w1, %w0, %w4  \n\t" \
      "   stxrh   %w2, %w1, [%3] \n\t" \
      "   cbnz    %w2, 1b        \n\t" \
      : "=&r" (_oldval),               \
        "=&r" (_newval),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
   _oldval;                            \
})

#define ARM_64_ATOMIC_32_OPV_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint32 _sample;                     \
   __asm__ __volatile__ (              \
      "1: ldxr    %w0, [%2]      \n\t" \
      #op "       %w0, %w0, %w3  \n\t" \
      "   stxr    %w1, %w0, [%2] \n\t" \
      "   cbnz    %w1, 1b        \n\t" \
      : "=&r" (_sample),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
})

#define ARM_64_ATOMIC_32_OPO_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint32 _newval;                     \
   uint32 _oldval;                     \
   __asm__ __volatile__ (              \
      "1: ldxr    %w0, [%3]      \n\t" \
      #op "       %w1, %w0, %w4  \n\t" \
      "   stxr    %w2, %w1, [%3] \n\t" \
      "   cbnz    %w2, 1b        \n\t" \
      : "=&r" (_oldval),               \
        "=&r" (_newval),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
   _oldval;                            \
})

#define ARM_64_ATOMIC_64_OPV_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint64 _sample;                     \
   __asm__ __volatile__ (              \
      "1: ldxr    %x0, [%2]      \n\t" \
      #op "       %x0, %x0, %x3  \n\t" \
      "   stxr    %w1, %x0, [%2] \n\t" \
      "   cbnz    %w1, 1b        \n\t" \
      : "=&r" (_sample),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
})

#define ARM_64_ATOMIC_64_OPO_PRIVATE(atm,modval,op) ({ \
   uint32 _failed;                     \
   uint64 _newval;                     \
   uint64 _oldval;                     \
   __asm__ __volatile__ (              \
      "1: ldxr    %x0, [%3]      \n\t" \
      #op "       %x1, %x0, %x4  \n\t" \
      "   stxr    %w2, %x1, [%3] \n\t" \
      "   cbnz    %w2, 1b        \n\t" \
      : "=&r" (_oldval),               \
        "=&r" (_newval),               \
        "=&r" (_failed)                \
      : "r" (atm),                     \
        "r" (modval)                   \
      : "memory");                     \
   _oldval;                            \
})

#define ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(atm,modval,op) ({ \
   DMB();                                       \
   ARM_64_ATOMIC_16_OPV_PRIVATE(atm,modval,op); \
   DMB();                                       \
})

#define ARM_64_ATOMIC_16_OPO_PRIVATE_FENCED(atm,modval,op) ({ \
   uint16 _oldval_fenced;                                        \
   DMB();                                                        \
   _oldval_fenced = ARM_64_ATOMIC_16_OPO_PRIVATE(atm,modval,op); \
   DMB();                                                        \
   _oldval_fenced;                                               \
})

#define ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(atm,modval,op) ({ \
   DMB();                                       \
   ARM_64_ATOMIC_32_OPV_PRIVATE(atm,modval,op); \
   DMB();                                       \
})

#define ARM_64_ATOMIC_32_OPO_PRIVATE_FENCED(atm,modval,op) ({ \
   uint32 _oldval_fenced;                                        \
   DMB();                                                        \
   _oldval_fenced = ARM_64_ATOMIC_32_OPO_PRIVATE(atm,modval,op); \
   DMB();                                                        \
   _oldval_fenced;                                               \
})

#define ARM_64_ATOMIC_64_OPV_PRIVATE_FENCED(atm,modval,op) ({ \
   DMB();                                       \
   ARM_64_ATOMIC_64_OPV_PRIVATE(atm,modval,op); \
   DMB();                                       \
})

#define ARM_64_ATOMIC_64_OPO_PRIVATE_FENCED(atm,modval,op) ({ \
   uint64 _oldval_fenced;                                        \
   DMB();                                                        \
   _oldval_fenced = ARM_64_ATOMIC_64_OPO_PRIVATE(atm,modval,op); \
   DMB();                                                        \
   _oldval_fenced;                                               \
})

#endif /* VM_ARM_64 */


/* Basic atomic types: 16, 32 and 64 bits */
typedef struct Atomic_uint16 {
   volatile uint16 value;
} Atomic_uint16 ALIGNED(2);

typedef struct Atomic_uint32 {
   volatile uint32 value;
} Atomic_uint32 ALIGNED(4);

typedef struct  Atomic_uint64 {
   volatile uint64 value;
} Atomic_uint64 ALIGNED(8);

/*
 * Prototypes for msft atomics.  These are defined & inlined by the
 * compiler so no function definition is needed.  The prototypes are
 * needed for C++.  Since amd64 compiler doesn't support inline asm we
 * have to use these.  Unfortunately, we still have to use some inline asm
 * for the 32 bit code since the and/or/xor implementations didn't show up
 * until XP or 2k3.
 *
 * The declarations for the intrinsic functions were taken from ntddk.h
 * in the DDK. The declarations must match otherwise the 64-bit C++
 * compiler will complain about second linkage of the intrinsic functions.
 * We define the intrinsic using the basic types corresponding to the
 * Windows typedefs. This avoids having to include windows header files
 * to get to the windows types.
 */
#if defined(_MSC_VER) && _MSC_VER >= 1310 && !defined(BORA_NO_WIN32_INTRINS)
#ifdef __cplusplus
extern "C" {
#endif
long  _InterlockedExchange(long volatile*, long);
long  _InterlockedCompareExchange(long volatile*, long, long);
long  _InterlockedExchangeAdd(long volatile*, long);
long  _InterlockedDecrement(long volatile*);
long  _InterlockedIncrement(long volatile*);
void  _ReadWriteBarrier(void);
#pragma intrinsic(_InterlockedExchange, _InterlockedCompareExchange)
#pragma intrinsic(_InterlockedExchangeAdd, _InterlockedDecrement)
#pragma intrinsic(_InterlockedIncrement, _ReadWriteBarrier)

# if _MSC_VER >= 1600
char     _InterlockedExchange8(char volatile *, char);
char     _InterlockedCompareExchange8(char volatile *, char, char);
#pragma intrinsic(_InterlockedCompareExchange8, _InterlockedCompareExchange8)
#endif

#if defined(VM_X86_64)
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
__int64  _InterlockedCompareExchange64(__int64 volatile*, __int64, __int64);
#if !defined(_WIN64)
#pragma intrinsic(_InterlockedAnd, _InterlockedAnd64)
#pragma intrinsic(_InterlockedOr, _InterlockedOr64)
#pragma intrinsic(_InterlockedXor, _InterlockedXor64)
#pragma intrinsic(_InterlockedExchangeAdd64, _InterlockedIncrement64)
#pragma intrinsic(_InterlockedDecrement64, _InterlockedExchange64)
#pragma intrinsic(_InterlockedCompareExchange64)
#endif /* !_WIN64 */
#endif /* __x86_64__ */

#ifdef __cplusplus
}
#endif
#endif /* _MSC_VER */

#if defined(__arm__)
/*
 * LDREX without STREX or CLREX may cause problems in environments where the
 * context switch may not clear the reference monitor - according ARM manual
 * the reference monitor should be cleared after a context switch, but some
 * may not like Linux kernel's non-preemptive context switch path. So use of
 * ARM routines in kernel code may not be safe.
 */
#   if defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) ||  \
       defined(__ARM_ARCH_7R__)|| defined(__ARM_ARCH_7M__)
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
Atomic_VolatileToAtomic(volatile uint32 *var)  // IN:
{
   return (Atomic_uint32 *)var;
}

/* Convert a volatile uint64 to Atomic_uint64. */
static INLINE Atomic_uint64 *
Atomic_VolatileToAtomic64(volatile uint64 *var)  // IN:
{
   return (Atomic_uint64 *)var;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Init, Atomic_SetFence, AtomicUseFence --
 *
 *      Determine whether an lfence intruction is executed after
 *	every locked instruction.
 *
 *	Certain AMD processors have a bug (see bug 107024) that
 *	requires an lfence after every locked instruction.
 *
 *	The global variable AtomicUseFence controls whether lfence
 *	is used (see AtomicEpilogue).
 *
 *	Atomic_SetFence sets AtomicUseFence to the given value.
 *
 *	Atomic_Init computes and sets AtomicUseFence for x86.
 *	It does not take into account the number of processors.
 *
 *	The rationale for all this complexity is that Atomic_Init
 *	is the easy-to-use interface.  It can be called a number
 *	of times cheaply, and does not depend on other libraries.
 *	However, because the number of CPUs is difficult to compute,
 *	it does without it and always assumes there are more than one.
 *
 *	For programs that care or have special requirements,
 *	Atomic_SetFence can be called directly, in addition to Atomic_Init.
 *	It overrides the effect of Atomic_Init, and can be called
 *	before, after, or between calls to Atomic_Init.
 *
 *-----------------------------------------------------------------------------
 */

// The freebsd assembler doesn't know the lfence instruction
#if defined(__GNUC__) &&                                                \
     __GNUC__ >= 3 &&                                                   \
    (defined(__VMKERNEL__) || !defined(__FreeBSD__)) &&                 \
    (!defined(MODULE) || defined(__VMKERNEL_MODULE__)) &&               \
    !defined(__APPLE__) &&                                              \
    (defined(__i386__) || defined(__x86_64__)) /* PR136775 */
#define ATOMIC_USE_FENCE
#endif

/* 
 * Starting with vSphere 2014, we no longer support ESX on AMD Rev F. 
 * Thus, we can eliminate all dynamic checks for whether to enable 
 * the Errata 147 work-around when compiling many of our binaries. 
 * However, we use an opt-in approach here rather than assuming all 
 * parts of our builds are safe. For example, the "fdm" binary from 
 * a new build may time travel back to hosts running older versions 
 * of ESX on Rev F, so "fdm" continues to require the ability to 
 * dynamically enable the errata work-around. With vSphere 2017,
 * this will no longer be required as the oldest version of ESX that 
 * VC 2017 will support is ESX 2014 (which won't run on Rev F).
 *
 * Modules may explicitly define MAY_NEED_AMD_REVF_WORKAROUND as 0 prior to
 * inclusion of vm_atomic.h when they are safe on AMD Rev F with the elided
 * lfence.
 */
#if !defined(MAY_NEED_AMD_REVF_WORKAROUND)
#if (!defined(VMX86_SERVER) ||                                          \
      (!defined(VMX86_VMX) && !defined(VMKERNEL) &&                     \
       !defined(VMM)       && !defined(VMCORE)))
#define MAY_NEED_AMD_REVF_WORKAROUND 1
#else
#define MAY_NEED_AMD_REVF_WORKAROUND 0
#endif
#endif

#if MAY_NEED_AMD_REVF_WORKAROUND
#if defined(VMATOMIC_IMPORT_DLLDATA)
VMX86_EXTERN_DATA Bool AtomicUseFence;
#else
EXTERN Bool AtomicUseFence;
#endif
EXTERN Bool atomicFenceInitialized;
#else   /* MAY_NEED_AMD_REVF_WORKAROUND */
#define AtomicUseFence         FALSE
#define atomicFenceInitialized TRUE
#endif  /* MAY_NEED_AMD_REVF_WORKAROUND */


void AtomicInitFence(void);

static INLINE void
Atomic_Init(void)
{
#ifdef ATOMIC_USE_FENCE
   if (MAY_NEED_AMD_REVF_WORKAROUND && !atomicFenceInitialized) {
      AtomicInitFence();
   }
#endif
}

static INLINE void
Atomic_SetFence(Bool fenceAfterLock)  // IN:
{
   (void)fenceAfterLock;     /* Work around unused parameter. */
#if MAY_NEED_AMD_REVF_WORKAROUND
   AtomicUseFence = fenceAfterLock;
   atomicFenceInitialized = TRUE;
#endif
}


/* Conditionally execute fence after interlocked instruction. */
static INLINE void
AtomicEpilogue(void)
{
#if MAY_NEED_AMD_REVF_WORKAROUND && defined(ATOMIC_USE_FENCE)
#ifdef VMM
      /* The monitor conditionally patches out the lfence when not needed.*/
      /* Construct a MonitorPatchTextEntry in the .patchtext section. */
      asm volatile ("1:\n\t"
                    "lfence\n\t"
                    "2:\n\t"
                    ".pushsection .patchtext\n\t"
                    ".quad 1b\n\t"
                    ".quad 2b\n\t"
                    ".quad 0\n\t"
                    ".popsection\n\t" ::: "memory");
#else
      if (UNLIKELY(AtomicUseFence)) {
         asm volatile ("lfence" ::: "memory");
      }
#endif
#endif
}


/*
 * All the assembly code is tricky and written conservatively.
 * For example, to make sure gcc won't introduce copies,
 * we force the addressing mode like this:
 *
 *    "xchgl %0, (%1)"
 *    : "=r" (val)
 *    : "r" (&var->value),
 *      "0" (val)
 *    : "memory"
 *
 * - edward
 *
 * Actually - turns out that gcc never generates memory aliases (it
 * still does generate register aliases though), so we can be a bit
 * more agressive with the memory constraints. The code above can be
 * modified like this:
 *
 *    "xchgl %0, %1"
 *    : "=r" (val),
 *      "=m" (var->value),
 *    : "0" (val),
 *      "1" (var->value)
 *
 * The advantages are that gcc can use whatever addressing mode it
 * likes to access the memory value, and that we dont have to use a
 * way-too-generic "memory" clobber as there is now an explicit
 * declaration that var->value is modified.
 *
 * see also /usr/include/asm/atomic.h to convince yourself this is a
 * valid optimization.
 *
 * - walken
 */

#if defined(_MSC_VER) && _MSC_VER < 1600 && defined(__x86_64__)
Bool VMWInterlockedExchangeBool(Bool volatile *ptr,
                                Bool val);

Bool VMWInterlockedCompareExchangeBool(Bool volatile *ptr,
                                       Bool newVal,
                                       Bool oldVal);
#endif

typedef struct Atomic_Bool {
   volatile Bool value;
} Atomic_Bool;

/* The ARM team can come along and add the code real soon now */
#if defined(VM_ARM_32) || defined(VM_ARM_64)
extern Bool AtomicUndefinedOnARM(void);
MY_ASSERTS(ARM64_BOOL_SIZE,
           ASSERT_ON_COMPILE(sizeof(Atomic_Bool) == sizeof(uint8));
   )
#endif

#if defined VMKERNEL || defined VMM || defined VM_ARM_64
/*
 *-----------------------------------------------------------------------------
 *
 * CMPXCHG1B --
 *
 *      Compare and exchange a single byte.
 *
 * Results:
 *      The value read from ptr.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint8
CMPXCHG1B(volatile uint8 *ptr, // IN/OUT
          uint8 oldVal,        // IN
          uint8 newVal)        // IN
{
   uint8 val;

#if defined(VM_ARM_64)
     register uint32 failed;

   __asm__ __volatile__ (
      "   dmb     sy             \n\t"
      "1: ldxrb   %w0, [%2]      \n\t"
      "   cmp     %w0, %w3, UXTB \n\t"
      "   b.ne    2f             \n\t"
      "   stxrb   %w1, %w4, [%2] \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "2: clrex                  \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (val), "=&r" (failed)
      : "r" (ptr), "r" (oldVal), "r" (newVal)
      : "cc", "memory");
#else
   __asm__ __volatile__("lock; cmpxchgb %b2, %1"
                        : "=a" (val),
                          "+m" (*ptr)
                        : "r" (newVal),
                          "0" (oldVal)
                        : "cc");

#endif /* defined(VM_ARM_64) */
   return val;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadBool --
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

static INLINE Bool
Atomic_ReadBool(Atomic_Bool const *var)  // IN:
{
   Bool val;

#if defined(__GNUC__) && defined(VM_ARM_32)
   val = AtomicUndefinedOnARM();
#elif defined(__GNUC__) && defined(VM_ARM_64)
   __asm__ __volatile__ (
      "ldrb    %w0, [%1]"
      : "=r" (val)
      : "r" (&var->value)
   );

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   __asm__ __volatile__(
      "movb %1, %0"
      : "=q" (val)
      : "m" (var->value)
   );
#elif defined(_MSC_VER)
   val = var->value;
#else
#error No compiler defined for Atomic_ReadBool
#endif

   return val;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWriteBool --
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

static INLINE Bool
Atomic_ReadWriteBool(Atomic_Bool *var,  // IN/OUT:
                     Bool val)          // IN:
{
#if defined(__GNUC__) && defined(VM_ARM_32)
   return AtomicUndefinedOnARM();
#elif defined(__GNUC__) && defined(VM_ARM_64)
   register Bool retVal;
   register uint32 failed;

   __asm__ __volatile__(
      "   dmb     sy             \n\t"
      "1: ldxrb   %w0, [%2]      \n\t"
      "   stxrb   %w1, %w3, [%2] \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (val)
      : "memory"
   );

   return retVal;

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   __asm__ __volatile__(
      "xchgb %0, %1"
      : "=q" (val),
        "+m" (var->value)
      : "0" (val)
   );
   AtomicEpilogue();
   return val;
#elif defined(_MSC_VER) && _MSC_VER >= 1600
   return _InterlockedExchange8(&var->value, val);
#elif defined(_MSC_VER) && defined(__i386__)
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm movzx eax, val
      __asm mov ebx, var
      __asm xchg [ebx]Atomic_Bool.value, al
   }
#pragma warning(pop)
#elif defined(_MSC_VER) && defined(__x86_64__)
   return VMWInterlockedExchangeBool(&var->value, val);
#else
#error No compiler defined for Atomic_ReadBool
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_WriteBool --
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
Atomic_WriteBool(Atomic_Bool *var,  // IN/OUT:
                 Bool val)          // IN:
{
#if defined(__GNUC__) && defined(VM_ARM_32)
   AtomicUndefinedOnARM();
#elif defined(__GNUC__) && defined(VM_ARM_64)
   Atomic_ReadWriteBool(var, val);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   __asm__ __volatile__(
      "movb %1, %0"
      : "=m" (var->value)
      : "qn" (val)
   );
#elif defined(_MSC_VER)
   var->value = val;
#else
#error No compiler defined for Atomic_WriteBool
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWriteBool --
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

static INLINE Bool
Atomic_ReadIfEqualWriteBool(Atomic_Bool *var,  // IN/OUT:
                            Bool oldVal,       // IN:
                            Bool newVal)       // IN:
{
#if defined(__GNUC__) && defined(VM_ARM_32)
   return AtomicUndefinedOnARM();
#elif defined(__GNUC__) && defined(VM_ARM_64)
   return (Bool)CMPXCHG1B((volatile uint8 *)&var->value, oldVal, newVal);
#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))
   Bool val;

   __asm__ __volatile__(
      "lock; cmpxchgb %2, %1"
      : "=a" (val),
        "+m" (var->value)
      : "q" (newVal),
        "0" (oldVal)
      : "cc"
   );
   AtomicEpilogue();
   return val;
#elif defined(_MSC_VER) && _MSC_VER >= 1600
   return _InterlockedCompareExchange8(&var->value, newVal, oldVal);
#elif defined(_MSC_VER) && defined(__i386__)
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm mov al, oldVal
      __asm mov ebx, var
      __asm mov cl, newVal
      __asm lock cmpxchg [ebx]Atomic_Bool.value, cl
      __asm movzx eax, al
      // eax is the return value, this is documented to work - edward
   }
#pragma warning(pop)
#elif defined(_MSC_VER) && defined(__x86_64__)
   return VMWInterlockedCompareExchangeBool(&var->value, newVal, oldVal);
#else
#error No compiler defined for Atomic_ReadIfEqualWriteBool
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Read --
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
Atomic_Read(Atomic_uint32 const *var) // IN
{
   uint32 value;

#if defined(VMM)
   ASSERT(((uintptr_t)var % 4) == 0);
#endif

#if defined(__GNUC__)
   /*
    * Use inline assembler to force using a single load instruction to
    * ensure that the compiler doesn't split a transfer operation into multiple
    * instructions.
    */

#if defined(VM_ARM_32)
   __asm__ __volatile__(
      "ldr %0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );
#elif defined(VM_ARM_64)
   __asm__ __volatile__ (
      "ldr    %w0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );
#else
   __asm__ __volatile__(
      "mov %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
#endif
#elif defined(_MSC_VER)
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
#define Atomic_Read32 Atomic_Read


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadWrite --
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
Atomic_ReadWrite(Atomic_uint32 *var, // IN/OUT
                 uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   register uint32 retVal;
   register uint32 failed;

   __asm__ __volatile__(
      "   dmb     sy             \n\t"
      "1: ldxr   %w0, [%2]       \n\t"
      "   stxr   %w1, %w3, [%2]  \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (val)
      : "memory"
   );

   return retVal;
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "xchgl %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
   );
   AtomicEpilogue();
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   return _InterlockedExchange((long *)&var->value, (long)val);
#else
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm mov eax, val
      __asm mov ebx, var
      __asm xchg [ebx]Atomic_uint32.value, eax
      // eax is the return value, this is documented to work - edward
   }
#pragma warning(pop)
#endif // _MSC_VER >= 1310
#else
#error No compiler defined for Atomic_ReadWrite
#endif // __GNUC__
}
#define Atomic_ReadWrite32 Atomic_ReadWrite


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Write --
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
Atomic_Write(Atomic_uint32 *var, // OUT
             uint32 val)         // IN
{
#if defined(VMM)
   ASSERT(((uintptr_t)var % 4) == 0);
#endif

#if defined(__GNUC__)
#if defined(VM_ARM_ANY)
   /*
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

   Atomic_ReadWrite(var, val);
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
#elif defined(_MSC_VER)
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
#define Atomic_Write32 Atomic_Write


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_ReadIfEqualWrite --
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
Atomic_ReadIfEqualWrite(Atomic_uint32 *var, // IN/OUT
                        uint32 oldVal,      // IN
                        uint32 newVal)      // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   register uint32 retVal;
   register uint32 failed;

   __asm__ __volatile__ (
      "   dmb     sy             \n\t"
      "1: ldxr   %w0, [%2]       \n\t"
      "   cmp     %w0, %w3       \n\t"
      "   b.ne    2f             \n\t"
      "   stxr   %w1, %w4, [%2]  \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "2: clrex                  \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (oldVal), "r" (newVal)
      : "cc", "memory");

   return retVal;
#else /* VM_X86_ANY */
   uint32 val;

   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; cmpxchgl %2, %1"
      : "=a" (val),
	"+m" (var->value)
      : "r" (newVal),
	"0" (oldVal)
      : "cc"
   );
   AtomicEpilogue();
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   return _InterlockedCompareExchange((long *)&var->value,
				      (long)newVal,
				      (long)oldVal);
#else
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm mov eax, oldVal
      __asm mov ebx, var
      __asm mov ecx, newVal
      __asm lock cmpxchg [ebx]Atomic_uint32.value, ecx
      // eax is the return value, this is documented to work - edward
   }
#pragma warning(pop)
#endif
#else
#error No compiler defined for Atomic_ReadIfEqualWrite
#endif
}
#define Atomic_ReadIfEqualWrite32 Atomic_ReadIfEqualWrite


#if defined(VM_64BIT) || defined(VM_ARM_V7)
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
#if defined(__GNUC__)
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
#if defined(__APPLE__) && __clang__ == 1 && __clang_major__ >= 5
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
#if defined(__APPLE__) && __clang__ == 1 && __clang_major__ >= 5
#pragma clang diagnostic pop
#endif // defined(__APPLE__) && __clang__ == 1 && __clang_major__ >= 5
   dmb();

   return retVal;
#elif defined(VM_ARM_64)
   register uint64 retVal;
   register uint32 failed;

   __asm__ __volatile__ (
      "   dmb     sy             \n\t"
      "1: ldxr    %x0, [%2]      \n\t"
      "   cmp     %x0, %x3       \n\t"
      "   b.ne    2f             \n\t"
      "   stxr    %w1, %x4, [%2] \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "2: clrex                  \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (oldVal), "r" (newVal)
      : "cc", "memory");

   return retVal;
#else /* VM_X86_64 */
   uint64 val;

   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; cmpxchgq %2, %1"
      : "=a" (val),
	"+m" (var->value)
      : "r" (newVal),
	"0" (oldVal)
      : "cc"
   );
   AtomicEpilogue();
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
 * Atomic_And --
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
Atomic_And(Atomic_uint32 *var, // IN/OUT
           uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(var, val, and);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; andl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined(__x86_64__)
   _InterlockedAnd((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock and [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_And
#endif
}
#define Atomic_And32 Atomic_And


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Or --
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
Atomic_Or(Atomic_uint32 *var, // IN/OUT
          uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(var, val, orr);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; orl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined(__x86_64__)
   _InterlockedOr((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock or [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Or
#endif
}
#define Atomic_Or32 Atomic_Or


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Xor --
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
Atomic_Xor(Atomic_uint32 *var, // IN/OUT
           uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(var, val, eor);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; xorl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if defined(__x86_64__)
   _InterlockedXor((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock xor [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Xor
#endif
}
#define Atomic_Xor32 Atomic_Xor


#if defined(VM_64BIT)
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
#if defined(__GNUC__)
#if defined(VM_ARM_64)
   ARM_64_ATOMIC_64_OPV_PRIVATE_FENCED(var, val, eor);
#else /* VM_X86_64 */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; xorq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
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
 * Atomic_Add --
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
Atomic_Add(Atomic_uint32 *var, // IN/OUT
           uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(var, val, add);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; addl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   _InterlockedExchangeAdd((long *)&var->value, (long)val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock add [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Add
#endif
}
#define Atomic_Add32 Atomic_Add


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Sub --
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
Atomic_Sub(Atomic_uint32 *var, // IN/OUT
           uint32 val)         // IN
{
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_32_OPV_PRIVATE_FENCED(var, val, sub);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; subl %1, %0"
      : "+m" (var->value)
      : "ri" (val)
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   _InterlockedExchangeAdd((long *)&var->value, (long)-val);
#else
   __asm mov eax, val
   __asm mov ebx, var
   __asm lock sub [ebx]Atomic_uint32.value, eax
#endif
#else
#error No compiler defined for Atomic_Sub
#endif
}
#define Atomic_Sub32 Atomic_Sub


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Inc --
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
Atomic_Inc(Atomic_uint32 *var) // IN/OUT
{
#ifdef __GNUC__
#if defined(VM_ARM_ANY)
   Atomic_Add(var, 1);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; incl %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   _InterlockedIncrement((long *)&var->value);
#else
   __asm mov ebx, var
   __asm lock inc [ebx]Atomic_uint32.value
#endif
#else
#error No compiler defined for Atomic_Inc
#endif
}
#define Atomic_Inc32 Atomic_Inc


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_Dec --
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
Atomic_Dec(Atomic_uint32 *var) // IN/OUT
{
#ifdef __GNUC__
#if defined(VM_ARM_ANY)
   Atomic_Sub(var, 1);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; decl %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   _InterlockedDecrement((long *)&var->value);
#else
   __asm mov ebx, var
   __asm lock dec [ebx]Atomic_uint32.value
#endif
#else
#error No compiler defined for Atomic_Dec
#endif
}
#define Atomic_Dec32 Atomic_Dec


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

   do {
      res = Atomic_Read(var);
   } while (res != Atomic_ReadIfEqualWrite(var, res, res | val));

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

   do {
      res = Atomic_Read(var);
   } while (res != Atomic_ReadIfEqualWrite(var, res, res & val));

   return res;
}


#if defined(VM_64BIT)
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

   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite64(var, res, res | val));

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

   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite64(var, res, res & val));

   return res;
}
#endif /* defined(VM_64BIT) */


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
#if defined(__GNUC__)
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
#elif defined(VM_ARM_64)
   return ARM_64_ATOMIC_32_OPO_PRIVATE_FENCED(var, val, add);
#else /* VM_X86_ANY */
   /* Checked against the Intel manual and GCC --walken */
   __asm__ __volatile__(
      "lock; xaddl %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
      : "cc"
   );
   AtomicEpilogue();
   return val;
#endif /* VM_X86_ANY */
#elif defined _MSC_VER
#if _MSC_VER >= 1310
   return _InterlockedExchangeAdd((long *)&var->value, (long)val);
#else
#pragma warning(push)
#pragma warning(disable : 4035)         // disable no-return warning
   {
      __asm mov eax, val
      __asm mov ebx, var
      __asm lock xadd [ebx]Atomic_uint32.value, eax
   }
#pragma warning(pop)
#endif
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
 * Usage of this helper struct is strictly reserved to the following
 * function. --hpreg
 */
typedef struct {
   uint32 lowValue;
   uint32 highValue;
} S_uint64;


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_CMPXCHG64 --
 *
 *      Compare exchange: Read variable, if equal to oldVal, write newVal
 *
 *      XXX: Ensure that if this function is to be inlined by gcc, it is
 *      compiled with -fno-strict-aliasing. Otherwise it will break.
 *      Unfortunately we know that gcc 2.95.3 (used to build the FreeBSD 3.2
 *      Tools) does not honor -fno-strict-aliasing. As a workaround, we avoid
 *      inlining the function entirely for versions of gcc under 3.0.
 *
 * Results:
 *      TRUE if equal, FALSE if not equal
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(__GNUC__) && __GNUC__ < 3
static Bool
#else
static INLINE Bool
#endif
Atomic_CMPXCHG64(Atomic_uint64 *var,   // IN/OUT
                 uint64 const *oldVal, // IN
                 uint64 const *newVal) // IN
{
#if defined(__GNUC__)
#if defined(VM_ARM_ANY)
   return (Atomic_ReadIfEqualWrite64(var, *oldVal, *newVal) == *oldVal);
#else /* VM_X86_ANY */

   Bool equal;
   /* Checked against the Intel manual and GCC --walken */
#if defined(__x86_64__)
   uint64 dummy;
   __asm__ __volatile__(
      "lock; cmpxchgq %3, %0" "\n\t"
      "sete %1"
      : "+m" (*var),
	"=qm" (equal),
	"=a" (dummy)
      : "r" (*newVal),
        "2" (*oldVal)
      : "cc"
   );
#else /* 32-bit version for non-ARM */
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
#      if __GNUC__ < 3 // Part of #188541 - for RHL 6.2 etc.
   __asm__ __volatile__(
      "xchg %%ebx, %6"       "\n\t"
      "mov 4(%%ebx), %%ecx"  "\n\t"
      "mov (%%ebx), %%ebx"   "\n\t"
      "lock; cmpxchg8b (%3)" "\n\t"
      "xchg %%ebx, %6"       "\n\t"
      "sete %0"
      : "=a" (equal),
        "=d" (dummy2),
        "=D" (dummy1)
      : /*
         * See the "Rules for __asm__ statements in __PIC__ code" above: %3
         * must use a register class which does not contain %ebx.
         */
        "S" (var),
        "0" (((S_uint64 const *)oldVal)->lowValue),
        "1" (((S_uint64 const *)oldVal)->highValue),
        "D" (newVal)
      : "ecx", "cc", "memory"
   );
#      else
   __asm__ __volatile__(
      "xchgl %%ebx, %6"      "\n\t"
      "lock; cmpxchg8b (%3)" "\n\t"
      "xchgl %%ebx, %6"      "\n\t"
      "sete %0"
      :	"=qm" (equal),
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
        "1" (((S_uint64 const *)oldVal)->lowValue),
        "2" (((S_uint64 const *)oldVal)->highValue),
        "D" (((S_uint64 const *)newVal)->lowValue),
        "c" (((S_uint64 const *)newVal)->highValue)
      : "cc", "memory"
   );
#      endif
#   else
   __asm__ __volatile__(
      "lock; cmpxchg8b %0" "\n\t"
      "sete %1"
      : "+m" (*var),
	"=qm" (equal),
	"=a" (dummy1),
	"=d" (dummy2)
      : "2" (((S_uint64 const *)oldVal)->lowValue),
        "3" (((S_uint64 const *)oldVal)->highValue),
        "b" (((S_uint64 const *)newVal)->lowValue),
        "c" (((S_uint64 const *)newVal)->highValue)
      : "cc"
   );
#   endif
#endif
   AtomicEpilogue();
   return equal;
#endif //VM_ARM_V7
#elif defined _MSC_VER
#if defined(__x86_64__)
   return (__int64)*oldVal == _InterlockedCompareExchange64((__int64 *)&var->value,
                                                            (__int64)*newVal,
                                                            (__int64)*oldVal);
#else
#pragma warning(push)
#pragma warning(disable : 4035)		// disable no-return warning
   {
      __asm mov esi, var
      __asm mov edx, oldVal
      __asm mov ecx, newVal
      __asm mov eax, [edx]S_uint64.lowValue
      __asm mov edx, [edx]S_uint64.highValue
      __asm mov ebx, [ecx]S_uint64.lowValue
      __asm mov ecx, [ecx]S_uint64.highValue
      __asm lock cmpxchg8b [esi]
      __asm sete al
      __asm movzx eax, al
      // eax is the return value, this is documented to work - edward
   }
#pragma warning(pop)
#endif
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
#if defined(__GNUC__)
#if defined(VM_ARM_ANY)
   return (Atomic_ReadIfEqualWrite(var, oldVal, newVal) == oldVal);
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
      : "cc"
   );
   AtomicEpilogue();
   return equal;
#endif /* VM_X86_ANY */
#else // defined(__GNUC__)
   return (Atomic_ReadIfEqualWrite(var, oldVal, newVal) == oldVal);
#endif // !defined(__GNUC__)
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
#if defined(__GNUC__) && defined(__x86_64__)
   uint64 value;

#ifdef VMM
   ASSERT((uintptr_t)var % 8 == 0);
#endif
   /*
    * Use asm to ensure we emit a single load.
    */
   __asm__ __volatile__(
      "movq %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
   return value;
#elif defined(__GNUC__) && defined(__i386__)
   uint64 value;
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
   AtomicEpilogue();
   return value;
#elif defined (_MSC_VER) && defined(__x86_64__)
   /*
    * Microsoft docs guarantee "Simple reads and writes to properly
    * aligned 64-bit variables are atomic on 64-bit Windows."
    * http://msdn.microsoft.com/en-us/library/ms684122%28VS.85%29.aspx
    *
    * XXX Verify that value is properly aligned. Bug 61315.
    */
   return var->value;
#elif defined (_MSC_VER) && defined(__i386__)
#   pragma warning(push)
#   pragma warning(disable : 4035)		// disable no-return warning
   {
      __asm mov ecx, var
      __asm mov edx, ecx
      __asm mov eax, ebx
      __asm lock cmpxchg8b [ecx]
      // edx:eax is the return value; this is documented to work. --mann
   }
#   pragma warning(pop)
#elif defined(__GNUC__) && defined (VM_ARM_V7)
   uint64 value;

   __asm__ __volatile__(
      "ldrexd %[value], %H[value], [%[var]] \n\t"
      : [value] "=&r" (value)
      : [var] "r" (&var->value)
   );

   return value;
#elif defined(__GNUC__) && defined(VM_ARM_64)
   uint64 value;

   __asm__ __volatile__(
      "ldr     %0, [%1]"
      : "=r" (value)
      : "r" (&var->value));

   return value;
#endif /* defined(__GNUC__) && defined(VM_ARM_64) */
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

#if defined(VM_64BIT)
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
#if defined(VM_ARM_64)
   return ARM_64_ATOMIC_64_OPO_PRIVATE_FENCED(var, val, add);
#elif defined(__x86_64__)

#if defined(__GNUC__)
   __asm__ __volatile__(
      "lock; xaddq %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
      : "cc"
   );
   AtomicEpilogue();
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
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));

   return oldVal;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Atomic_ReadSub64 --
 *
 *      Atomically subtracts a 64-bit integer to another
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
   return Atomic_ReadAdd64(var, -val);
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
   return Atomic_ReadAdd64(var, CONST64U(-1));
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
#if !defined(VM_64BIT)
   Atomic_ReadAdd64(var, val); /* Return value is unused. */
#elif defined(__GNUC__)
#if defined(VM_ARM_64)
   ARM_64_ATOMIC_64_OPV_PRIVATE_FENCED(var, val, add);
#else /* defined(VM_X86_64) */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; addq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
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
#if !defined(__x86_64__)
   Atomic_ReadSub64(var, val); /* Return value is unused. */
#elif defined(__GNUC__)
#if defined(VM_ARM_64)
   ARM_64_ATOMIC_64_OPV_PRIVATE_FENCED(var, val, sub);
#else /* VM_X86_64 */
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; subq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
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
#if !defined(__x86_64__)
   Atomic_ReadInc64(var);  /* Return value is unused. */
#elif defined(__GNUC__)
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; incq %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
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
#if !defined(__x86_64__)
   Atomic_ReadDec64(var);  /* Return value is unused. */
#elif defined(__GNUC__)
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; decq %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
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
#if defined(__x86_64__)
#if defined(__GNUC__)
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "xchgq %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
   );
   AtomicEpilogue();
   return val;
#elif defined _MSC_VER
   return _InterlockedExchange64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_ReadWrite64
#endif
#else
   uint64 oldVal;

   do {
      oldVal = var->value;
   } while (!Atomic_CMPXCHG64(var, &oldVal, &val));

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
#if defined(__x86_64__)
#if defined(__GNUC__)

#ifdef VMM
   ASSERT((uintptr_t)var % 8 == 0);
#endif
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
#else  /* defined(__x86_64__) */
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
#if defined(__x86_64__)
#if defined(__GNUC__)
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; orq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined _MSC_VER
   _InterlockedOr64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_Or64
#endif
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   do {
      oldVal = var->value;
      newVal = oldVal | val;
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
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
#if defined(__x86_64__)
#if defined(__GNUC__)
   /* Checked against the AMD manual and GCC --hpreg */
   __asm__ __volatile__(
      "lock; andq %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined _MSC_VER
   _InterlockedAnd64((__int64 *)&var->value, (__int64)val);
#else
#error No compiler defined for Atomic_And64
#endif
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   do {
      oldVal = var->value;
      newVal = oldVal & val;
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
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
                uint64 bit)         // IN
{
#if defined(__x86_64__)
#if defined(__GNUC__)
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "lock; bts %1, %0"
      : "+m" (var->value)
      : "ri" (bit)
      : "cc"
   );
   AtomicEpilogue();
#elif defined _MSC_VER
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal | (CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
#else
#error No compiler defined for Atomic_SetBit64
#endif
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal | (CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
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
                  uint64 bit)         // IN
{
#if defined(__x86_64__)
#if defined(__GNUC__)
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "lock; btr %1, %0"
      : "+m" (var->value)
      : "ri" (bit)
      : "cc"
   );
   AtomicEpilogue();
#elif defined _MSC_VER
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal & ~(CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
#else
#error No compiler defined for Atomic_ClearBit64
#endif
#else // __x86_64__
   uint64 oldVal;
   uint64 newVal;
   ASSERT(bit <= 63);
   do {
      oldVal = var->value;
      newVal = oldVal & ~(CONST64U(1) << bit);
   } while (!Atomic_CMPXCHG64(var, &oldVal, &newVal));
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
                 uint64 bit)         // IN
{
#if defined(__x86_64__)
#if defined(__GNUC__)
   Bool out = FALSE;
   ASSERT(bit <= 63);
   __asm__ __volatile__(
      "bt %2, %1; setc %0"
      : "=rm"(out)
      : "m" (var->value),
        "rJ" (bit)
      : "cc"
   );
   return out;
#elif defined _MSC_VER
   ASSERT(bit <= 63);
   return (var->value & (CONST64U(1) << bit)) != 0;
#else
#error No compiler defined for Atomic_TestBit64
#endif
#else // __x86_64__
   ASSERT(bit <= 63);
   return (var->value & (CONST64U(1) << bit)) != 0;
#endif
}


#if defined(__GNUC__)
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

#ifdef VMM
   ASSERT((uintptr_t)var % 2 == 0);
#endif

#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))

   __asm__ __volatile__(
      "movw %1, %0"
      : "=r" (value)
      : "m" (var->value)
   );
#elif defined(VM_ARM_V7)
   NOT_TESTED();

   __asm__ __volatile__(
      "ldrh %0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );
#elif defined(VM_ARM_64)
   __asm__ __volatile__ (
      "ldrh   %w0, [%1]"
      : "=r" (value)
      : "r" (&var->value)
   );

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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "xchgw %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
   );
   AtomicEpilogue();
   return val;
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   register uint16 retVal;
   register uint16 failed;

   __asm__ __volatile__(
      "   dmb     sy             \n\t"
      "1: ldxrh  %w0, [%2]       \n\t"
      "   stxrh  %w1, %w3, [%2]  \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (val)
      : "memory"
   );

   return retVal;
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
#ifdef VMM
   ASSERT((uintptr_t)var % 2 == 0);
#endif

#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))

   __asm__ __volatile__(
      "movw %1, %0"
      : "=m" (var->value)
      : "r" (val)
   );
#elif defined(VM_ARM_ANY)
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   uint16 val;

   __asm__ __volatile__(
      "lock; cmpxchgw %2, %1"
      : "=a" (val),
	"+m" (var->value)
      : "r" (newVal),
	"0" (oldVal)
      : "cc"
   );
   AtomicEpilogue();
   return val;
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   register uint16 retVal;
   register uint16 failed;

   __asm__ __volatile__ (
      "   dmb     sy             \n\t"
      "1: ldxrh  %w0, [%2]       \n\t"
      "   cmp     %w0, %w3       \n\t"
      "   b.ne    2f             \n\t"
      "   stxrh  %w1, %w4, [%2]  \n\t"
      "   cbnz    %w1, 1b        \n\t"
      "2: clrex                  \n\t"
      "   dmb     sy             \n\t"
      : "=&r" (retVal), "=&r" (failed)
      : "r" (&var->value), "r" (oldVal), "r" (newVal)
      : "cc", "memory");

   return retVal;
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; andw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(var, val, and);
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; orw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(var, val, orr);
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; xorw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(var, val, eor);
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; addw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(var, val, add);
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; subw %1, %0"
      : "+m" (var->value)
      : "re" (val)
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   ARM_64_ATOMIC_16_OPV_PRIVATE_FENCED(var, val, sub);
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; incw %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_ANY)
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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; decw %0"
      : "+m" (var->value)
      :
      : "cc"
   );
   AtomicEpilogue();
#elif defined(VM_ARM_ANY)
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

   do {
      res = var->value;
   } while (res != Atomic_ReadIfEqualWrite16(var, res, res | val));

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
#if defined(__GNUC__)
#if (defined(__x86_64__) || defined(__i386__))
   __asm__ __volatile__(
      "lock; xaddw %0, %1"
      : "=r" (val),
	"+m" (var->value)
      : "0" (val)
      : "cc"
   );
   AtomicEpilogue();
   return val;
#elif defined(VM_ARM_V7)
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
#elif defined(VM_ARM_64)
   return ARM_64_ATOMIC_16_OPO_PRIVATE_FENCED(var, val, add);
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
   return Atomic_ReadAdd16(var, -1);
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
 */
#if defined(VM_64BIT)
MAKE_ATOMIC_TYPE(Ptr, 64, void const *, void *, uintptr_t)
#else
MAKE_ATOMIC_TYPE(Ptr, 32, void const *, void *, uintptr_t)
#endif
MAKE_ATOMIC_TYPE(Int, 32, int, int, int)


/* Prevent the compiler from re-ordering memory references. */
#ifdef __GNUC__
#define ATOMIC_COMPILER_BARRIER()   __asm__ __volatile__ ("": : :"memory")
#elif defined(_MSC_VER)
#define ATOMIC_COMPILER_BARRIER()   _ReadWriteBarrier()
#else
#error No compiler defined for ATOMIC_COMPILER_BARRIER
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Atomic_MFence --
 *
 *      Implements mfence in terms of a lock xor. The reason for implementing
 *      our own mfence is that not all of our supported cpus have an assembly
 *      mfence (P3, Athlon). We put it here to avoid duplicating code which is
 *      also why it is prefixed with "Atomic_". Moreover, this implementation
 *      performs slightly better than 'mfence' on modern CPUs (See PR 817059).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Cause loads and stores prior to this to be globally
 *      visible.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Atomic_MFence(void)
{
   Atomic_uint32 fence;
   ATOMIC_COMPILER_BARRIER();
   Atomic_Xor(&fence, 0x1);
   ATOMIC_COMPILER_BARRIER();
}

#ifdef ATOMIC_COMPILER_BARRIER
#undef ATOMIC_COMPILER_BARRIER
#endif /* ATOMIC_COMPILER_BARRIER */

#endif // ifndef _ATOMIC_H_
