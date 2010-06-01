/*********************************************************
 * Copyright (C) 1998-2004 VMware, Inc. All rights reserved.
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
 * vm_assert.h --
 *
 *	The basic assertion facility for all VMware code.
 *
 *	For proper use, see
 *	http://vmweb.vmware.com/~mts/WebSite/guide/programming/asserts.html
 */

#ifndef _VM_ASSERT_H_
#define _VM_ASSERT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMIROM
#include "includeCheck.h"

// XXX not necessary except some places include vm_assert.h improperly
#include "vm_basic_types.h"
#include "vm_basic_defs.h"


/*
 * XXX old file code
 */

#ifdef FILECODEINT
#error "Don't define FILECODEINT.  It is obsolete."
#endif
#ifdef FILECODE
#error "Don't define FILECODE.  It is obsolete."
#endif


/*
 * Panic and log functions
 */

EXTERN void Log(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN void Warning(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN NORETURN void Panic(const char *fmt, ...) PRINTF_DECL(1, 2);

EXTERN void LogThrottled(uint32 *count, const char *fmt, ...)
            PRINTF_DECL(2, 3);
EXTERN void WarningThrottled(uint32 *count, const char *fmt, ...)
            PRINTF_DECL(2, 3);

/* DB family:  messages which are parsed by logfile database system */
#define WarningDB Warning
#define LogDB Log
#define WarningThrottledDB WarningThrottled
#define LogThrottledDB LogThrottled


/*
 * Stress testing: redefine ASSERT_IFNOT() to taste
 */

#ifndef ASSERT_IFNOT
   /*
    * PR 271512: When compiling with gcc, catch assignments inside an ASSERT.
    *
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
    */

   #ifdef __GNUC__
      #define ASSERT_IFNOT(cond, panic)                                              \
                 ({if (UNLIKELY(!(cond))) { panic; if (0) { if (cond) { ; } } } (void)0;})
   #else
      #define ASSERT_IFNOT(cond, panic)                               \
                 (UNLIKELY(!(cond)) ? (panic) : (void)0)
   #endif
#endif


/*
 * Assert, panic, and log macros
 *
 * Some of these are redefined below undef !VMX86_DEBUG.
 * ASSERT() is special cased because of interaction with Windows DDK.
 */

#if defined VMX86_DEBUG || defined ASSERT_ALWAYS_AVAILABLE
#undef ASSERT
#define ASSERT(cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC(AssertAssert))
#endif
#define ASSERT_BUG(bug, cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC_BUG(bug, AssertAssert))
#define ASSERT_BUG_DEBUGONLY(bug, cond) ASSERT_BUG(bug, cond)

#define PANIC()        _ASSERT_PANIC(AssertPanic)
#define PANIC_BUG(bug) _ASSERT_PANIC_BUG(bug, AssertPanic)

#define ASSERT_NOT_IMPLEMENTED(cond) \
           ASSERT_IFNOT(cond, NOT_IMPLEMENTED())
#define ASSERT_NOT_IMPLEMENTED_BUG(bug, cond) \
           ASSERT_IFNOT(cond, NOT_IMPLEMENTED_BUG(bug))

#define NOT_IMPLEMENTED()        _ASSERT_PANIC(AssertNotImplemented)
#define NOT_IMPLEMENTED_BUG(bug) _ASSERT_PANIC_BUG(bug, AssertNotImplemented)

#define NOT_REACHED()            _ASSERT_PANIC(AssertNotReached)
#define NOT_REACHED_BUG(bug)     _ASSERT_PANIC_BUG(bug, AssertNotReached)

#define ASSERT_MEM_ALLOC(cond) \
           ASSERT_IFNOT(cond, _ASSERT_PANIC(AssertMemAlloc))

#ifdef VMX86_DEVEL
   #define ASSERT_LENGTH(real, expected) \
              ASSERT_IFNOT((real) == (expected), \
                 Panic(AssertLengthFmt, __FILE__, __LINE__, real, expected))
#else
   #define ASSERT_LENGTH(real, expected) ASSERT((real) == (expected))
#endif

#ifdef VMX86_DEVEL
   #define ASSERT_DEVEL(cond) ASSERT(cond)
#else
   #define ASSERT_DEVEL(cond) ((void) 0)
#endif

#define ASSERT_NO_INTERRUPTS()  ASSERT(!INTERRUPTS_ENABLED())
#define ASSERT_HAS_INTERRUPTS() ASSERT(INTERRUPTS_ENABLED())

#define ASSERT_LOG_UNEXPECTED(bug, cond) \
           (UNLIKELY(!(cond)) ? LOG_UNEXPECTED(bug) : 0)
#ifdef VMX86_DEVEL
   #define LOG_UNEXPECTED(bug) \
              Warning(AssertUnexpectedFmt, __FILE__, __LINE__, bug)
#else
   #define LOG_UNEXPECTED(bug) \
              Log(AssertUnexpectedFmt, __FILE__, __LINE__, bug)
#endif

#define ASSERT_NOT_TESTED(cond) (UNLIKELY(!(cond)) ? NOT_TESTED() : 0)
#ifdef VMX86_DEVEL
   #define NOT_TESTED() Warning(AssertNotTestedFmt, __FILE__, __LINE__)
#else
   #define NOT_TESTED() Log(AssertNotTestedFmt, __FILE__, __LINE__)
#endif

#define NOT_TESTED_ONCE()                                               \
   do {                                                                 \
      static Bool alreadyPrinted = FALSE;                               \
      if (UNLIKELY(!alreadyPrinted)) {                                  \
	 alreadyPrinted = TRUE;                                         \
	 NOT_TESTED();                                                  \
      }                                                                 \
   } while (0)

#define NOT_TESTED_1024()                                               \
   do {                                                                 \
      static uint16 count = 0;                                          \
      if (UNLIKELY(count == 0)) { NOT_TESTED(); }                       \
      count = (count + 1) & 1023;                                       \
   } while (0)

#define LOG_ONCE(_s)                                                    \
   do {                                                                 \
      static Bool logged = FALSE;                                       \
      if (!logged) {                                                    \
	 Log _s;                                                        \
         logged = TRUE;                                                 \
      }                                                                 \
   } while (0)


/*
 * Redefine macros that are only in debug versions
 */

#if !defined VMX86_DEBUG && !defined ASSERT_ALWAYS_AVAILABLE // {

#undef  ASSERT
#define ASSERT(cond) ((void) 0)

#undef  ASSERT_BUG_DEBUGONLY
#define ASSERT_BUG_DEBUGONLY(bug, cond) ((void) 0)

#undef  ASSERT_LENGTH
#define ASSERT_LENGTH(real, expected) ((void) 0)

/*
 * Expand NOT_REACHED() as appropriate for each situation.
 *
 * Mainly, we want the compiler to infer the same control-flow
 * information as it would from Panic().  Otherwise, different
 * compilation options will lead to different control-flow-derived
 * errors, causing some make targets to fail while others succeed.
 *
 * VC++ has the __assume() built-in function which we don't trust
 * (see bug 43485); gcc has no such construct; we just panic in
 * userlevel code.  The monitor doesn't want to pay the size penalty
 * (measured at 212 bytes for the release vmm for a minimal infinite
 * loop; panic would cost even more) so it does without and lives
 * with the inconsistency.
 */

#ifdef VMM
#undef  NOT_REACHED
#define NOT_REACHED() ((void) 0)
#else
// keep debug definition
#endif

#undef  ASSERT_LOG_UNEXPECTED
#define ASSERT_LOG_UNEXPECTED(bug, cond) ((void) 0)

#undef LOG_UNEXPECTED
#define LOG_UNEXPECTED(bug) ((void) 0)

#undef  ASSERT_NOT_TESTED
#define ASSERT_NOT_TESTED(cond) ((void) 0)
#undef  NOT_TESTED
#define NOT_TESTED() ((void) 0)
#undef  NOT_TESTED_ONCE
#define NOT_TESTED_ONCE() ((void) 0)
#undef  NOT_TESTED_1024
#define NOT_TESTED_1024() ((void) 0)

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
 */

#define ASSERT_ON_COMPILE(e) \
   do { \
      enum { AssertOnCompileMisused = ((e) ? 1 : -1) }; \
      typedef char AssertOnCompileFailed[AssertOnCompileMisused]; \
   } while (0)


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
   static INLINE void name(void) { \
      assertions \
   }


/*
 * Internal macros, functions, and strings
 *
 * The monitor wants to save space at call sites, so it has specialized
 * functions for each situation.  User level wants to save on implementation
 * so it uses generic functions.
 */

#if !defined VMM || defined MONITOR_APP // {

#define _ASSERT_PANIC(name) \
           Panic(_##name##Fmt "\n", __FILE__, __LINE__)
#define _ASSERT_PANIC_BUG(bug, name) \
           Panic(_##name##Fmt " bugNr=%d\n", __FILE__, __LINE__, bug)

#define AssertLengthFmt     _AssertLengthFmt
#define AssertUnexpectedFmt _AssertUnexpectedFmt
#define AssertNotTestedFmt  _AssertNotTestedFmt

#endif // }

// these don't have newline so a bug can be tacked on
#define _AssertPanicFmt            "PANIC %s:%d"
#define _AssertAssertFmt           "ASSERT %s:%d"
#define _AssertNotImplementedFmt   "NOT_IMPLEMENTED %s:%d"
#define _AssertNotReachedFmt       "NOT_REACHED %s:%d"
#define _AssertMemAllocFmt         "MEM_ALLOC %s:%d"

// these are complete formats with newline
#define _AssertLengthFmt           "LENGTH %s:%d r=%#x e=%#x\n"
#define _AssertUnexpectedFmt       "UNEXPECTED %s:%d bugNr=%d\n"
#define _AssertNotTestedFmt        "NOT_TESTED %s:%d\n"

#endif /* ifndef _VM_ASSERT_H_ */
