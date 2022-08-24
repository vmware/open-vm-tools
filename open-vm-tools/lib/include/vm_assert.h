/*********************************************************
 * Copyright (C) 1998-2022 VMware, Inc. All rights reserved.
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
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
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
 * vm_assert.h --
 *
 *	The basic assertion facility for all VMware code.
 *
 *      For proper use, see bora/doc/assert
 */

#ifndef _VM_ASSERT_H_
#define _VM_ASSERT_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

// XXX not necessary except some places include vm_assert.h improperly
#include "vm_basic_types.h"

/* No stdarg.h on Linux kernels 5.15+ */
#ifndef KBUILD_MODNAME
#include <stdarg.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Some bits of vmcore are used in VMKernel code and cannot have
 * the VMKERNEL define due to other header dependencies.
 */
#if defined(VMKERNEL) && !defined(VMKPANIC)
#define VMKPANIC 1
#endif

/*
 * Internal macros, functions, and strings
 *
 * The monitor wants to save space at call sites, so it has specialized
 * functions for each situation.  User level wants to save on implementation
 * so it uses generic functions.
 */

#if !defined VMM ||                                                     \
    defined BINARY_CHECKER || defined COREQUERY || defined DECODER ||   \
    defined DIS16 || defined FROBOS || defined TRAPAPI_APP ||           \
    defined VMM_LINKER || defined VMSS2CORE

# if defined (VMKPANIC)
#  include "vmk_assert.h"
# else /* !VMKPANIC */
#  define _ASSERT_PANIC(name) \
           Panic(_##name##Fmt "\n", __FILE__, __LINE__)
#  define _ASSERT_PANIC_BUG(bug, name) \
           Panic(_##name##Fmt " bugNr=%d\n", __FILE__, __LINE__, bug)
#  define _ASSERT_PANIC_NORETURN(name) \
           Panic(_##name##Fmt "\n", __FILE__, __LINE__)
#  define _ASSERT_PANIC_BUG_NORETURN(bug, name) \
           Panic(_##name##Fmt " bugNr=%d\n", __FILE__, __LINE__, bug)
# endif /* VMKPANIC */
#endif


// These strings don't have newline so that a bug can be tacked on.
#define _AssertAssertFmt           "ASSERT %s:%d"
#define _AssertVerifyFmt           "VERIFY %s:%d"
#define _AssertNotImplementedFmt   "NOT_IMPLEMENTED %s:%d"
#define _AssertNotReachedFmt       "NOT_REACHED %s:%d"
#define _AssertMemAllocFmt         "MEM_ALLOC %s:%d"
#define _AssertNotTestedFmt        "NOT_TESTED %s:%d"


/*
 * Panic and log functions
 */

void Log(const char *fmt, ...)     PRINTF_DECL(1, 2);
void Warning(const char *fmt, ...) PRINTF_DECL(1, 2);

#if defined VMKPANIC
void Panic_SaveRegs(void);

NORETURN void Panic_NoSave(const char *fmt, ...) PRINTF_DECL(1, 2);

# define Panic(fmt...)        \
   do {                       \
      Panic_SaveRegs();       \
      Panic_NoSave(fmt);      \
   } while(0)

#else /* !VMKPANIC */
NORETURN void Panic(const char *fmt, ...) PRINTF_DECL(1, 2);
#endif

void LogThrottled(uint32 *count, const char *fmt, ...) PRINTF_DECL(2, 3);
void WarningThrottled(uint32 *count, const char *fmt, ...) PRINTF_DECL(2, 3);


#ifndef ASSERT_IFNOT
   /*
    * 'UNLIKELY' is defined with __builtin_expect, which does not warn when
    * passed an assignment (gcc bug 36050). To get around this, we put 'cond'
    * in an 'if' statement and make sure it never gets executed by putting
    * that inside of 'if (0)'. We use gcc's statement expression syntax to
    * make ASSERT an expression because some code uses it that way.
    *
    * Since statement expression syntax is a gcc extension and since it's
    * not clear if this is a problem with other compilers, the ASSERT
    * definition was not changed for them. Using a bare 'cond' with the
    * ternary operator may provide a solution.
    *
    * PR 271512: When compiling with gcc, catch assignments inside an ASSERT.
    */

# ifdef __GNUC__
#  define ASSERT_IFNOT(cond, panic)                                       \
         ({if (UNLIKELY(!(cond))) { panic; if (0) { if (cond) {;}}} (void)0;})
# else
#  define ASSERT_IFNOT(cond, panic)                                       \
         (UNLIKELY(!(cond)) ? (panic) : (void)0)
# endif
#endif


/*
 * Assert, panic, and log macros
 *
 * Some of these are redefined or undef below in !VMX86_DEBUG.
 */

#if defined VMX86_DEBUG
   /*
    * Assert is a debug-only construct.
    *
    * Assert should capture (i.e., document and validate) invariants,
    * including method preconditions, postconditions, loop invariants,
    * class invariants, data structure invariants, etc.
    *
    * ASSERT() is special cased because of interaction with Windows DDK.
    */
# undef  ASSERT
# define ASSERT(cond) ASSERT_IFNOT(cond, _ASSERT_PANIC(AssertAssert))
# define ASSERT_BUG(bug, cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC_BUG(bug, AssertAssert))
#endif

   /*
    * Verify is present on all build types.
    *
    * Verify should protect against missing functionality (e.g., unhandled
    * cases), bugs and other forms of gaps, and also be used as the fail-safe
    * way to plug remaining security risks. Verify is not the correct primitive
    * to use to validate an invariant, as a condition never being true implies
    * that it need not be handled.
    */
#undef  VERIFY
#define VERIFY(cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC_NORETURN(AssertVerify))
#define VERIFY_BUG(bug, cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC_BUG_NORETURN(bug, AssertVerify))

   /*
    * NOT IMPLEMENTED is useful to indicate that a codepath has not yet
    * been implemented, and should cause execution to forcibly quit if it is
    * ever reached. Some instances use NOT_IMPLEMENTED for things that will
    * never be implemented (as implied by ASSERT_NOT_IMPLEMENTED).
    *
    * PR1151214 asks for ASSERT_NOT_IMPLEMENTED to be replaced with VERIFY.
    * ASSERT_NOT_IMPLEMENTED is a conditional NOT_IMPLEMENTED. Despite the
    * name, ASSERT_NOT_IMPLEMENTED is present in release builds.
    *
    * NOT_IMPLEMENTED_BUG is NOT_IMPLEMENTED with the bug number included
    * in the panic string.
    */
#define ASSERT_NOT_IMPLEMENTED(cond) \
           ASSERT_IFNOT(cond, NOT_IMPLEMENTED())

#if defined VMKPANIC || defined VMM
# define NOT_IMPLEMENTED()        _ASSERT_PANIC_NORETURN(AssertNotImplemented)
#else
# define NOT_IMPLEMENTED()        _ASSERT_PANIC(AssertNotImplemented)
#endif

#if defined VMM
# define NOT_IMPLEMENTED_BUG(bug) \
          _ASSERT_PANIC_BUG_NORETURN(bug, AssertNotImplemented)
#else
# define NOT_IMPLEMENTED_BUG(bug) _ASSERT_PANIC_BUG(bug, AssertNotImplemented)
#endif

   /*
    * NOT_REACHED is meant to indicate code paths that we can never
    * execute. This can be very dangerous on release builds due to how
    * some compilers behave when you do potentially reach the point
    * indicated by NOT_REACHED and can lead to very difficult to debug
    * failures. NOT_REACHED should be used sparingly due to this.
    *
    * On debug builds, NOT_REACHED is a Panic with a fixed string.
    */
#if defined VMKPANIC || defined VMM
# define NOT_REACHED()            _ASSERT_PANIC_NORETURN(AssertNotReached)
#else
# define NOT_REACHED()            _ASSERT_PANIC(AssertNotReached)
#endif

#if !defined VMKERNEL && !defined VMKBOOT && !defined VMKERNEL_MODULE
   /*
    * PR 2621164,2624036: ASSERT_MEM_ALLOC is deprecated and should not be
    * used. Please use VERIFY where applicable, since the latter aligns
    * better with the consistency model as defined by bora/doc/assert. You
    * could also consider the Util_Safe*alloc* functions in userland.
    *
    * Despite its name, ASSERT_MEM_ALLOC is present in both debug and release
    * builds.
    */
# define ASSERT_MEM_ALLOC(cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC(AssertMemAlloc))
#endif

   /*
    * ASSERT_NO_INTERRUPTS & ASSERT_HAS_INTERRUPTS are shorthand to
    * assert whether interrupts are disabled or enabled.
    */
#define ASSERT_NO_INTERRUPTS()  ASSERT(!INTERRUPTS_ENABLED())
#define ASSERT_HAS_INTERRUPTS() ASSERT(INTERRUPTS_ENABLED())

   /*
    * NOT_TESTED may be used to indicate that we've reached a code path.
    * It simply puts an entry in the log file.
    *
    * ASSERT_NOT_TESTED does the same, conditionally.
    * NOT_TESTED_ONCE will only log the first time we executed it.
    * NOT_TESTED_1024 will only log every 1024th time we execute it.
    */
#ifdef VMX86_DEVEL
# define NOT_TESTED()      Warning(_AssertNotTestedFmt "\n", __FILE__, __LINE__)
#else
# define NOT_TESTED()      Log(_AssertNotTestedFmt "\n", __FILE__, __LINE__)
#endif

#define ASSERT_NOT_TESTED(cond) (UNLIKELY(!(cond)) ? NOT_TESTED() : (void)0)
#define NOT_TESTED_ONCE()       DO_ONCE(NOT_TESTED())

#define NOT_TESTED_1024()                                               \
   do {                                                                 \
      static MONITOR_ONLY(PERVCPU) uint16 count = 0;                    \
      if (UNLIKELY(count == 0)) { NOT_TESTED(); }                       \
      count = (count + 1) & 1023;                                       \
   } while (0)

#define LOG_ONCE(...) DO_ONCE(Log(__VA_ARGS__))


/*
 * Redefine macros that have a different behaviour on release
 * builds. This includes no behaviour (ie. removed).
 */

#if !defined VMX86_DEBUG // {

# undef  ASSERT
# define ASSERT(cond)          ((void)0)
# define ASSERT_BUG(bug, cond) ((void)0)

/*
 * NOT_REACHED on debug builds is a Panic; but on release
 * builds reaching it is __builtin_unreachable()
 * which is "undefined behaviour" according to
 * gcc. (https://gcc.gnu.org/onlinedocs/gcc/Other-Builtins.html)
 *
 * When used correctly __builtin_unreachable() allows the compiler
 * to generate slightly better code and eliminates some warnings
 * when the compiler can't identify a "fallthrough" path that is
 * never reached.
 *
 * When used incorrectly, __builtin_unreachable is a dangerous
 * construct and we should structure code in such a way that we
 * need fewer instances of NOT_REACHED to silence the compiler,
 * and use the function attribute "noreturn" where appropriate
 * and potentially then using NOT_REACHED as documentation.
 *
 * We should *never* have code after NOT_REACHED in a block as
 * it's unclear to the reader if that path is ever possible, and
 * as mentioned above, gcc will do weird and wonderful things to us.
 *
 * Mainly, we want the compiler to infer the same control-flow
 * information as it would from Panic().  Otherwise, different
 * compilation options will lead to different control-flow-derived
 * errors, causing some make targets to fail while others succeed.
 *
 * VC++ has the __assume() built-in function which we don't trust (see
 * bug 43485).  However, __assume() is used in the Windows ULM
 * implementation, because the newer compiler used for that project
 * generates correct code.
 *
 * With gcc, the __builtin_unreachable() extension is used when the
 * compiler is known to support it.
 */

# if defined VMKPANIC || defined VMM || defined ULM_ESX
#  undef  NOT_REACHED
#  define NOT_REACHED() __builtin_unreachable()
# elif defined ULM_WIN
#  undef  NOT_REACHED
#  define NOT_REACHED() __assume(0)
# else
 // keep debug definition
# endif

# undef LOG_UNEXPECTED
# define LOG_UNEXPECTED(bug)     ((void)0)

# undef  ASSERT_NOT_TESTED
# define ASSERT_NOT_TESTED(cond) ((void)0)
# undef  NOT_TESTED
# define NOT_TESTED()            ((void)0)
# undef  NOT_TESTED_ONCE
# define NOT_TESTED_ONCE()       ((void)0)
# undef  NOT_TESTED_1024
# define NOT_TESTED_1024()       ((void)0)

#endif // !VMX86_DEBUG }


/*
 * Compile-time assertions.
 *
 * ASSERT_ON_COMPILE does not use the common
 * switch (0) { case 0: case (e): ; } trick because some compilers (e.g. MSVC)
 * generate code for it.
 *
 * The implementation uses both enum and typedef because the typedef alone is
 * insufficient; gcc allows arrays to be declared with non-constant expressions
 * (even in typedefs, where it makes no sense).
 *
 * NOTE: if GCC ever changes so that it ignores unused types altogether, this
 * assert might not fire!  We explicitly mark it as unused because GCC 4.8+
 * uses -Wunused-local-typedefs as part of -Wall, which means the typedef will
 * generate a warning.
 */

#if defined(_Static_assert) || defined(__cplusplus) ||                         \
    !defined(__GNUC__) || __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 6)
#define ASSERT_ON_COMPILE(e) \
   do { \
      enum { AssertOnCompileMisused = ((e) ? 1 : -1) }; \
      UNUSED_TYPE(typedef char AssertOnCompileFailed[AssertOnCompileMisused]); \
   } while (0)
#else
#define ASSERT_ON_COMPILE(e) \
   do {                      \
      _Static_assert(e, #e); \
   } while (0)
#endif

/*
 * To put an ASSERT_ON_COMPILE() outside a function, wrap it
 * in MY_ASSERTS().  The first parameter must be unique in
 * each .c file where it appears.  For example,
 *
 * MY_ASSERTS(FS3_INT,
 *    ASSERT_ON_COMPILE(sizeof(FS3_DiskLock) == 128);
 *    ASSERT_ON_COMPILE(sizeof(FS3_DiskLockReserved) == DISK_BLOCK_SIZE);
 *    ASSERT_ON_COMPILE(sizeof(FS3_DiskBlock) == DISK_BLOCK_SIZE);
 *    ASSERT_ON_COMPILE(sizeof(Hardware_DMIUUID) == 16);
 * )
 *
 * Caution: ASSERT() within MY_ASSERTS() is silently ignored.
 * The same goes for anything else not evaluated at compile time.
 */

#define MY_ASSERTS(name, assertions) \
   static INLINE void name(void) {   \
      assertions                     \
   }

/*
 * Avoid generating extra code due to asserts which are required by
 * Clang static analyzer, e.g. right before a statement would fail, using
 * the __clang_analyzer__ macro defined only when clang SA is parsing files.
 */
#ifdef __clang_analyzer__
# define ANALYZER_ASSERT(cond) ASSERT(cond)
#else
# define ANALYZER_ASSERT(cond) ((void)0)
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* ifndef _VM_ASSERT_H_ */
