/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#if !defined VMM && !defined WIN32
#define VTHREAD_USE_PTHREAD 1
#include <signal.h>
#endif


/*
 * Types
 */

typedef unsigned VThreadID;

#define VTHREAD_INVALID_ID	(VThreadID)(~0u)

/* XXX Vestigial need as an MXState array size */
#define VTHREAD_MAX_THREADS	96

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

#define VTHREAD_VMX_ID		0
#define VTHREAD_MKS_ID		1
#define VTHREAD_OTHER_ID	2
#define VTHREAD_ALLOCSTART_ID	3

#define VTHREADBASE_MAX_NAME    32  /* Arbitrary */


typedef struct {
   VThreadID  id;
   char       name[VTHREADBASE_MAX_NAME];
#if !defined _WIN32
   Atomic_Int signalNestCount;
#endif
} VThreadBaseData;

/* Common VThreadBase functions */
const char *VThreadBase_CurName(void);
VThreadID VThreadBase_CurID(void);
void VThreadBase_SetName(const char *name);

/* For implementing a thread library */
Bool VThreadBase_InitWithTLS(VThreadBaseData *tls);
void VThreadBase_ForgetSelf(void);
void VThreadBase_SetNoIDFunc(void (*func)(void),
                             void (*destr)(void *));

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
void VThreadBase_SetIsInSignal(VThreadID tid, Bool isInSignal);
int VThreadBase_SigMask(int how, const sigset_t *newmask, sigset_t *oldmask);
#endif

#endif /* VMM */


#endif // VTHREAD_BASE_H
