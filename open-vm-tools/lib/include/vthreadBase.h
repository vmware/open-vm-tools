/*********************************************************
 * Copyright (C) 2006-2017 VMware, Inc. All rights reserved.
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
 * vthreadBase.h --
 *
 *	Subset of vthread defines that are used by libs that need to make
 *      vthread calls but don't actually do any vthreading.
 *
 *      May be used without lib/thread or with lib/thread.  (But don't try
 *      to do both simultaneously, since lib/thread needs to do more
 *      bookkeeping.)
 */

#ifndef VTHREAD_BASE_H
#define VTHREAD_BASE_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_atomic.h"

#if !defined VMM

#if !defined WIN32
#include <signal.h>
#endif

/*
 * Most major OSes now support __thread, so begin making TLS access via
 * __thread the common case. If VMW_HAVE_TLS is defined, __thread may
 * be used.
 *
 * Linux: since glibc-2.3
 * Windows: since Vista and vs2005 via __declspec(thread)
 *          (Prior to Vista, __declspec(thread) was ignored when
 *           a library is loaded via LoadLibrary / delay-load)
 * macOS: since 10.7 via clang (xcode-4.6)
 * iOS: 64-bit since 8.0, 32-bit since 9.0 (per llvm commit)
 * watchOS: since 2.0
 * Android: since NDKr12 (June 2016, per NDK wiki)
 * FreeBSD and Solaris: "a long time", gcc-4.1 was needed.
 */
#if defined __ANDROID__
   /* No modern NDK currently in use, no macro known */
#elif defined __APPLE__
   /* macOS >= 10.7 tested. iOS >= {8.0,9.0} NOT tested */
#  if __MAC_OS_X_VERSION_MIN_REQUIRED+0 >= 1070
#     define VMW_HAVE_TLS
#  elif  (defined __LP64__ && __IPHONE_OS_VERSION_MIN_REQUIRED+0 >= 80000) || \
         (!defined __LP64__ && __IPHONE_OS_VERSION_MIN_REQUIRED+0 >= 90000)
#     define VMW_HAVE_TLS
#  endif
#else
   /* All other platforms require new enough version to support TLS */
#  define VMW_HAVE_TLS
#endif

#endif /* VMM */

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Types
 */

typedef uintptr_t VThreadID;

#define VTHREAD_INVALID_ID    (VThreadID)(0)

#ifdef VMM
/*
 *-----------------------------------------------------------------------------
 *
 * VThread_CurID --
 * VThread_CurName --
 *
 *      Get the current thread ID / name. This is only inline for the monitor.
 *
 *      The extern symbols herein are defined in monitor.c and
 *      initialized by SharedArea_PowerOn.
 *
 * Results:
 *      Thread ID / name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE VThreadID
VThread_CurID(void)
{
   extern const VThreadID vthreadCurID;

   return vthreadCurID;
}

static INLINE const char *
VThread_CurName(void)
{
   extern const char vcpuThreadName[];

   return vcpuThreadName;
}

#else

#define VTHREADBASE_MAX_NAME    32  /* Arbitrary */


/* Common VThreadBase functions */
const char *VThreadBase_CurName(void);
VThreadID VThreadBase_CurID(void);
void VThreadBase_SetName(const char *name);
void VThreadBase_SetNamePrefix(const char *prefix);

/* For implementing a thread library */
void VThreadBase_ForgetSelf(void);

/* Match up historical VThread_ names with VThreadBase_ names */
static INLINE const char *
VThread_CurName(void)
{ return VThreadBase_CurName(); }

static INLINE VThreadID
VThread_CurID(void)
{ return VThreadBase_CurID(); }

static INLINE void
VThread_SetName(const char *name)
{ VThreadBase_SetName(name); }


#ifdef _WIN32
static INLINE Bool
VThreadBase_IsInSignal(void)
{
   /* Win32 does not worry about async-signal-safety. */
   return FALSE;
}
#else
Bool VThreadBase_IsInSignal(void);
void VThreadBase_SetIsInSignal(Bool isInSignal);
#endif

uint64 VThreadBase_GetKernelID(void);

#endif /* VMM */

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // VTHREAD_BASE_H
