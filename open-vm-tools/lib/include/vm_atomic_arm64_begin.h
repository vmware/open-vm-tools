/*********************************************************
 * Copyright (c) 2017-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * vm_atomic_arm64_begin.h --
 *
 *      Define private macros used to implement ARM64 atomic operations.
 */

#if !(   defined _ATOMIC_H_                                                   \
      || defined _VM_UNINTERRUPTIBLE_H_                                       \
      || defined _WAIT_UNTIL_LIKELY_H_)
#   error "Only files which implement an atomic API can include this file!"
#endif

#include "vm_basic_asm_arm64.h"

/*
 * Atomic_LsePresent should be set to 1 for CPUs that have the LSE extenstion
 * and where the atomic instructions are known to have a performance benefit.
 * Seemingly, on some low-end chips (CA55) there may not be a benefit.
 */

/*
 * The silliness with _VMATOM_HAVE_LSE_DEFINED is necessary because this
 * could be included multiple times (via vm_atomic and vm_atomic_relaxed).
 */
#ifndef _VMATOM_HAVE_LSE_DEFINED
typedef struct  {
   Bool LsePresent;
#ifndef VMKERNEL
   Bool ProbedForLse;
#endif
} Atomic_ConfigParams;

#if defined(VMX86_SERVER) || defined(VMKBOOT)
/*
 * When building UW code for ESXi, Atomic_Config a weak symbol.
 * When building for kernel mode, Atomic_Config is exported by
 * bora/vmkernel/lib/arm64/atomic.c
 */
#ifndef VMKERNEL
#pragma weak Atomic_Config
Atomic_ConfigParams Atomic_Config;
#else
extern Atomic_ConfigParams Atomic_Config;
#endif

static INLINE Bool
Atomic_HaveLse(void)
{
#ifndef VMKERNEL
   /*
    * Can't just include sys/auxv.h, unfortunately.
    */
   extern uint64 getauxval(uint64 type);
#define _VMATOM_AT_HWCAP                16
#define _VMATOM_AT_HWCAP_ATOMICS        (1 << 8)

   if (!Atomic_Config.ProbedForLse) {
      uint64 cap = getauxval(_VMATOM_AT_HWCAP);
      Atomic_Config.LsePresent = (cap & _VMATOM_AT_HWCAP_ATOMICS) != 0;
      SMP_W_BARRIER_W();
      Atomic_Config.ProbedForLse = TRUE;
   }
#undef _VMATOM_AT_HWCAP
#undef _VMATOM_AT_HWCAP_ATOMICS
#endif

   return Atomic_Config.LsePresent;
}
#else /* !VMX86_SERVER && !VMKBOOT */
/*
 * Not building for ESXi? Assume no LSE.
 */
#define Atomic_HaveLse()         FALSE
#endif
#define _VMATOM_HAVE_LSE_DEFINED
#endif /* _VMATOM_HAVE_LSE_DEFINED */

#define _VMATOM_LSE_OPNAME(x)   _VMATOM_LSE_OPNAME_##x
#define _VMATOM_LSE_OPNAME_add  "add"
#define _VMATOM_LSE_OPNAME_sub  "add"
#define _VMATOM_LSE_OPNAME_eor  "eor"
#define _VMATOM_LSE_OPNAME_orr  "set"
#define _VMATOM_LSE_OPNAME_and  "clr"

#define _VMATOM_LSE_OPMOD(x)    _VMATOM_LSE_OPMOD_##x
#define _VMATOM_LSE_OPMOD_add
#define _VMATOM_LSE_OPMOD_sub   -
#define _VMATOM_LSE_OPMOD_eor
#define _VMATOM_LSE_OPMOD_orr
#define _VMATOM_LSE_OPMOD_and   ~

/*                      bit size, instruction suffix, register prefix, extend suffix */
#define _VMATOM_SIZE_8         8,                  b,               w,             b
#define _VMATOM_SIZE_16       16,                  h,               w,             h
#define _VMATOM_SIZE_32       32,                   ,               w,             w
#define _VMATOM_SIZE_64       64,                   ,               x,             x
/*
 * Expand 'size' (a _VMATOM_SIZE_*) into its 4 components, then expand
 * 'snippet' (a _VMATOM_SNIPPET_*)
 */
#define _VMATOM_X2(snippet, size, ...) snippet(size, __VA_ARGS__)
/* Prepend a prefix to 'shortSnippet' and 'shortSize'. */
#define _VMATOM_X(shortSnippet, shortSize, ...)                               \
   _VMATOM_X2(_VMATOM_SNIPPET_##shortSnippet, _VMATOM_SIZE_##shortSize,       \
              __VA_ARGS__)

/* Read relaxed (returned). */
#define _VMATOM_SNIPPET_R_NF(bs, is, rp, es, atm) ({                          \
   uint##bs _val;                                                             \
                                                                              \
   /* ldr is atomic if and only if 'atm' is aligned on #bs bits. */           \
   __asm__ __volatile__(                                                      \
      "ldr"#is" %"#rp"0, %1                                              \n\t"\
      : "=r" (_val)                                                           \
      : "m" (*atm)                                                            \
   );                                                                         \
                                                                              \
   _val;                                                                      \
})

/* Read total store order (returned). */
#define _VMATOM_SNIPPET_R(bs, is, rp, es, atm) ({                             \
   uint##bs _val;                                                             \
                                                                              \
   /* ldr is atomic if and only if 'atm' is aligned on #bs bits. */           \
   __asm__ __volatile__(                                                      \
      "dmb ishld                                                         \n\t"\
      "dmb ishst                                                         \n\t"\
      "ldr"#is" %"#rp"0, %1                                              \n\t"\
      "dmb ishld                                                         \n\t"\
      : "=r" (_val)                                                           \
      : "m" (*atm)                                                            \
      : "memory"                                                              \
   );                                                                         \
                                                                              \
   _val;                                                                      \
})

/* Read acquire/seq_cst (returned). */
#define _VMATOM_SNIPPET_R_SC(bs, is, rp, es, atm) ({                          \
   uint##bs _val;                                                             \
                                                                              \
   /* ldar is atomic if and only if 'atm' is aligned on #bs bits. */          \
   __asm__ __volatile__(                                                      \
      "ldar"#is" %"#rp"0, %1                                             \n\t"\
      : "=r" (_val)                                                           \
      : "Q" (*atm)                                                            \
      : "memory"                                                              \
   );                                                                         \
                                                                              \
   _val;                                                                      \
})

/* Write relaxed. */
#define _VMATOM_SNIPPET_W_NF(bs, is, rp, es, atm, val) ({                     \
   /*                                                                         \
    * str is atomic if and only if 'atm' is aligned on #bs bits.              \
    *                                                                         \
    * Clearing the exclusive monitor is not required. The local monitor is    \
    * cleared on any exception return, and the global monitor (as per B2.10.2,\
    * ARM DDI 0487A.k) is cleared by a successful write.                      \
    */                                                                        \
   __asm__ __volatile__(                                                      \
      "str"#is" %"#rp"1, %0                                              \n\t"\
      : "=m" (*atm)                                                           \
      : "r" (val)                                                             \
   );                                                                         \
})

/* Write total store order. */
#define _VMATOM_SNIPPET_W(bs, is, rp, es, atm, val) ({                        \
   __asm__ __volatile__(                                                      \
      "dmb ishld                                                         \n\t"\
      "dmb ishst                                                         \n\t"\
      "str"#is" %"#rp"1, %0                                              \n\t"\
      "dmb ishst                                                         \n\t"\
      : "=m" (*atm)                                                           \
      : "r" (val)                                                             \
      : "memory"                                                              \
   );                                                                         \
})

/* Write release/seq_cst. */
#define _VMATOM_SNIPPET_W_SC(bs, is, rp, es, atm, val) ({                     \
   /*                                                                         \
    * stlr is atomic if and only if 'atm' is aligned on #bs bits.             \
    *                                                                         \
    * Clearing the exclusive monitor is not required. The local monitor is    \
    * cleared on any exception return, and the global monitor (as per B2.10.2,\
    * ARM DDI 0487A.k) is cleared by a successful write.                      \
    */                                                                        \
   __asm__ __volatile__(                                                      \
      "stlr"#is" %"#rp"1, %0                                             \n\t"\
      : "=Q" (*atm)                                                           \
      : "r" (val)                                                             \
      : "memory"                                                              \
   );                                                                         \
})

/*
 * Since on x86, some atomic operations are using LOCK semantics, assumptions
 * have been made about the ordering these operations imply on surrounding
 * code. As a result, on arm64 we have to provide these same guarantees.
 * We do this by making use of DMB barriers both before and after the atomic
 * ldrx/strx sequences.
 */
#define _VMATOM_FENCE(fenced)                                                 \
   if (fenced) {                                                              \
      SMP_RW_BARRIER_RW();                                                    \
   }

/* Read (not returned), op with modval, write. */
#define _VMATOM_SNIPPET_OP(bs, is, rp, es, fenced, atm, op, modval) ({        \
   uint##bs _newval;                                                          \
                                                                              \
   _VMATOM_FENCE(fenced);                                                     \
   if (Atomic_HaveLse()) {                                                    \
      __asm__ __volatile__(                                                   \
         ".arch armv8.2-a                                                \n\t"\
         "st" _VMATOM_LSE_OPNAME(op) #is" %"#rp"1, %0                    \n\t"\
         : "+Q" (*atm)                                                        \
         : "r" (_VMATOM_LSE_OPMOD(op) modval)                                 \
      );                                                                      \
   } else {                                                                   \
      uint32 _failed;                                                         \
      __asm__ __volatile__(                                                   \
         "1: ldxr"#is" %"#rp"0, %2                                       \n\t"\
         "  "#op"      %"#rp"0, %"#rp"0, %"#rp"3                         \n\t"\
         "   stxr"#is" %w1    , %"#rp"0, %2                              \n\t"\
         "   cbnz      %w1    , 1b                                       \n\t"\
         : "=&r" (_newval),                                                   \
           "=&r" (_failed),                                                   \
           "+Q" (*atm)                                                        \
         : "r" (modval)                                                       \
      );                                                                      \
   }                                                                          \
   _VMATOM_FENCE(fenced);                                                     \
})

/* Read (returned), op with modval, write. */
#define _VMATOM_SNIPPET_ROP(bs, is, rp, es, fenced, atm, op, modval) ({       \
   uint##bs _newval;                                                          \
   uint##bs _oldval;                                                          \
                                                                              \
   _VMATOM_FENCE(fenced);                                                     \
   if (Atomic_HaveLse()) {                                                    \
      __asm__ __volatile__(                                                   \
         ".arch armv8.2-a                                                \n\t"\
         "ld" _VMATOM_LSE_OPNAME(op) #is" %"#rp"2, %"#rp"0, %1           \n\t"\
         : "=r" (_oldval),                                                    \
           "+Q" (*atm)                                                        \
         : "r" (_VMATOM_LSE_OPMOD(op) modval)                                 \
      );                                                                      \
   } else {                                                                   \
      uint32 _failed;                                                         \
      __asm__ __volatile__(                                                   \
         "1: ldxr"#is" %"#rp"0, %3                                       \n\t"\
         "  "#op"      %"#rp"1, %"#rp"0, %"#rp"4                         \n\t"\
         "   stxr"#is" %w2    , %"#rp"1, %3                              \n\t"\
         "   cbnz      %w2    , 1b                                       \n\t"\
         : "=&r" (_oldval),                                                   \
           "=&r" (_newval),                                                   \
           "=&r" (_failed),                                                   \
           "+Q" (*atm)                                                        \
         : "r" (modval)                                                       \
      );                                                                      \
   }                                                                          \
   _VMATOM_FENCE(fenced);                                                     \
                                                                              \
   _oldval;                                                                   \
})

/* Read (returned), write. */
#define _VMATOM_SNIPPET_RW(bs, is, rp, es, fenced, atm, val) ({               \
   uint##bs _oldval;                                                          \
                                                                              \
   _VMATOM_FENCE(fenced);                                                     \
   if (Atomic_HaveLse()) {                                                    \
      __asm__ __volatile__(                                                   \
         ".arch armv8.2-a                                                \n\t"\
         "swp"#is" %"#rp"2, %"#rp"0, %1                                  \n\t"\
         : "=r" (_oldval),                                                    \
           "+Q" (*atm)                                                        \
         : "r" (val)                                                          \
      );                                                                      \
   } else {                                                                   \
      uint32 _failed;                                                         \
      __asm__ __volatile__(                                                   \
         "1: ldxr"#is" %"#rp"0, %2                                       \n\t"\
         "   stxr"#is" %w1    , %"#rp"3, %2                              \n\t"\
         "   cbnz      %w1    , 1b                                       \n\t"\
         : "=&r" (_oldval),                                                   \
           "=&r" (_failed),                                                   \
           "+Q" (*atm)                                                        \
         : "r" (val)                                                          \
      );                                                                      \
   }                                                                          \
   _VMATOM_FENCE(fenced);                                                     \
                                                                              \
   _oldval;                                                                   \
})

/* Read (returned), if equal to old then write new. */
#define _VMATOM_SNIPPET_RIFEQW(bs, is, rp, es, fenced, atm, old, new) ({      \
   uint##bs _oldval;                                                          \
                                                                              \
   _VMATOM_FENCE(fenced);                                                     \
   if (Atomic_HaveLse()) {                                                    \
      __asm__ __volatile__(                                                   \
         ".arch armv8.2-a                                                \n\t"\
         "cas"#is" %"#rp"0, %"#rp"2, %1                                  \n\t"\
         : "=r" (_oldval),                                                    \
           "+Q" (*atm)                                                        \
         : "r" (new), "0" (old)                                               \
      );                                                                      \
   } else {                                                                   \
      uint32 _failed;                                                         \
      __asm__ __volatile__(                                                   \
         "1: ldxr"#is" %"#rp"0, %2                                       \n\t"\
         "   cmp       %"#rp"0, %"#rp"3, uxt"#es"                        \n\t"\
         "   b.ne      2f                                                \n\t"\
         "   stxr"#is" %w1    , %"#rp"4, %2                              \n\t"\
         "   cbnz      %w1    , 1b                                       \n\t"\
         "2:                                                             \n\t"\
         : "=&r" (_oldval),                                                   \
           "=&r" (_failed),                                                   \
           "+Q" (*atm)                                                        \
         : "r" (old),                                                         \
           "r" (new)                                                          \
         : "cc"                                                               \
      );                                                                      \
   }                                                                          \
   _VMATOM_FENCE(fenced);                                                     \
                                                                              \
   _oldval;                                                                   \
})
