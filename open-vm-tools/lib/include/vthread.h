/*********************************************************
 * Copyright (C) 2002 VMware, Inc. All rights reserved.
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
 * vthread.h --
 *
 *	Thread management
 */

#ifndef _VTHREAD_H_
#define _VTHREAD_H_


#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vthreadBase.h"
#include "vcpuid.h"

#if VTHREAD_USE_PTHREAD
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#endif
#ifdef _WIN32
#include <windows.h>
#endif


#if VTHREAD_MAX_VCPUS < MAX_VCPUS
   #error VTHREAD_MAX_VCPUS < MAX_VCPUS
#endif


/*
 * Scheduler priorities for VThread_SetThreadPriority().
 */

#if VTHREAD_USE_PTHREAD
#ifdef __APPLE__
// These values come from osfmk/kern/sched.h in xnu-792.6.70.
#   define VTHREAD_PRIORITY_IDLE                0
#   define VTHREAD_PRIORITY_LOWEST              11
#   define VTHREAD_PRIORITY_BELOW_NORMAL        21
#   define VTHREAD_PRIORITY_NORMAL              31
#   define VTHREAD_PRIORITY_ABOVE_NORMAL        41
#   define VTHREAD_PRIORITY_HIGHEST             51
// TODO: This is just the highest a user thread can go; not really time critical.
#   define VTHREAD_PRIORITY_TIME_CRITICAL       63
#else
#   define VTHREAD_PRIORITY_IDLE                19
#   define VTHREAD_PRIORITY_LOWEST              15
#   define VTHREAD_PRIORITY_BELOW_NORMAL        10
#   define VTHREAD_PRIORITY_NORMAL              0
#   define VTHREAD_PRIORITY_ABOVE_NORMAL       (-10)
#   define VTHREAD_PRIORITY_HIGHEST            (-15)
#   define VTHREAD_PRIORITY_TIME_CRITICAL      (-20)
#endif // !__APPLE__
#endif
#ifdef _WIN32
#   define VTHREAD_PRIORITY_IDLE                THREAD_PRIORITY_IDLE
#   define VTHREAD_PRIORITY_LOWEST              THREAD_PRIORITY_LOWEST
#   define VTHREAD_PRIORITY_BELOW_NORMAL        THREAD_PRIORITY_BELOW_NORMAL
#   define VTHREAD_PRIORITY_NORMAL              THREAD_PRIORITY_NORMAL
#   define VTHREAD_PRIORITY_ABOVE_NORMAL        THREAD_PRIORITY_ABOVE_NORMAL
#   define VTHREAD_PRIORITY_HIGHEST             THREAD_PRIORITY_HIGHEST
#   define VTHREAD_PRIORITY_TIME_CRITICAL       THREAD_PRIORITY_TIME_CRITICAL
#endif


/*
 * Debuggable builds log per-thread timing info.
 */

#if (defined(VMX86_DEVEL) || defined(VMX86_DEBUG)) && defined(USERLEVEL)
#define VTHREAD_RESOURCE_ACCOUNTING
#endif


/*
 * Private variables
 *
 * Don't use these directly.  Go through the proper interface
 * functions.
 */

#ifdef VMM
extern VThreadID vthreadCurID;
#endif

extern VThreadID vthreadMaxVCPUID;


/*
 * Global functions
 */

static INLINE Bool
VThread_IsValidID(VThreadID tid) // IN: The thread id.
{
   return tid < VTHREAD_MAX_THREADS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_CurID --
 *
 *      Get the current thread ID. This is only inline for the monitor.
 *
 * Results:
 *      Thread ID.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMM
static INLINE VThreadID
VThread_CurID(void)
{
   ASSERT(VThread_IsValidID(vthreadCurID));

   return vthreadCurID;
}
#endif


static INLINE Bool
VThread_IsVMXID(VThreadID tid) // IN: The thread id.
{
   return tid == VTHREAD_VMX_ID;
}


static INLINE Bool
VThread_IsMKSID(VThreadID tid) // IN: The thread id.
{
   return tid == VTHREAD_MKS_ID;
}


static INLINE Bool
VThread_IsVCPUID(VThreadID tid) // IN: The thread id.
{
   ASSERT(VThread_IsValidID(vthreadMaxVCPUID));

   return tid >= VTHREAD_VCPU0_ID && tid < vthreadMaxVCPUID;
}


static INLINE Bool
VThread_IsVMX(void)
{
   return VThread_IsVMXID(VThread_CurID());
}


static INLINE Bool
VThread_IsMKS(void)
{
   return VThread_IsMKSID(VThread_CurID());
}


static INLINE Bool
VThread_IsVCPU(void)
{
   return VThread_IsVCPUID(VThread_CurID());
}


static INLINE Bool
VThread_IsVCPU0(void)
{
   return VThread_CurID() == VTHREAD_VCPU0_ID;
}


static INLINE Vcpuid
VThread_ThreadIDToVCPUID(VThreadID tid) // IN: The thread id.
{
   ASSERT(VThread_IsVCPUID(tid));

   return tid - VTHREAD_VCPU0_ID;
}


static INLINE VThreadID
VThread_VCPUIDToThreadID(Vcpuid vcpuID)
{
   VThreadID threadID = VTHREAD_VCPU0_ID + vcpuID;

   ASSERT(VThread_IsVCPUID(threadID));

   return threadID;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VThread_BestVCPUID --
 *
 *      Return the "best" VCPU to use (for actions), which is
 *	either the current VCPU (if any) or some default.
 *
 *	The monitor version needs to be a macro because of an include
 *	problem: CurVcpuid() is defined in monitor.h, which includes
 *	sharedAreaVCPU.h, which includes misc_shared.h,
 *	which includes mutex.h, which includes us.
 *
 * Results:
 *      The best VCPU ID.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMM
#define VThread_BestVCPUID() CurVcpuid()

#else
#ifdef VCPUID_INVALID
static INLINE Vcpuid
VThread_BestVCPUID(void)
{
   VThreadID threadID = VThread_CurID();

   return VThread_IsVCPUID(threadID) ? VThread_ThreadIDToVCPUID(threadID) :
                                       BOOT_VCPU_ID;
}
#endif
#endif


#ifdef VMM // {

/*
 * Save a few bytes and inline VThread_MonitorInit().
 *
 * The VCPU ID and number of VCPU's are passed in because
 * (it's the right thing to do, and) they require monitor.h,
 * which includes mutex.h, which includes us.
 * -- edward
 */

static INLINE void
VThread_MonitorInit(Vcpuid vcpuID, int numVCPUs)
{
   /*
    * Initialize vthreadMaxVCPUID first because
    * VThread_VCPUIDToThreadID() depends on it.
    */

   vthreadMaxVCPUID = VTHREAD_VCPU0_ID + numVCPUs;
   vthreadCurID = VThread_VCPUIDToThreadID(vcpuID);
}

#else // } {

void VThread_SetNumVCPUs(int numVCPUs);
VThreadID VThread_AllocID(void);
Bool VThread_IsAllocatedID(VThreadID tid);
void VThread_ReserveID(VThreadID tid);
void VThread_FreeID(VThreadID tid);
VThreadID VThread_CreateThread(void (*fn)(void *), void *data,
			       VThreadID tid, const char *name);
void VThread_DestroyThread(VThreadID tid);
Bool VThread_IsCurrentVThreadValid(void);
void VThread_WaitThread(VThreadID tid);
void VThread_SetPriorityLimits(int min, int max);
void VThread_AdjustThreadPriority(VThreadID tid, int inc, int incHighest,
                                  int incTimeCritical);
Bool VThread_SetThreadPriority(VThreadID tid, int newPrio);
uintptr_t VThread_GetApproxStackTop(VThreadID tid);

void VThread_WatchThread(VThreadID tid, Bool watched);
void VThread_WatchDog(void);	// who let the ...
void VThread_WatchDogPoll(void *clientData);

void VThread_SetExitHook(void (*hook)(int loopCount));

#ifdef _WIN32
HANDLE VThread_GetHostThreadHandle(VThreadID tid);
DWORD VThread_GetHostThreadID(VThreadID tid);
#else
int VThread_GetHostThreadPID(VThreadID tid);
void VThread_SetIsInSignal(VThreadID tid, Bool isInSignal);
#endif

#endif // }

#ifdef VTHREAD_RESOURCE_ACCOUNTING
void VThread_DumpThreadRUsage(void);
#else
#define VThread_DumpThreadRUsage()
#endif


#endif // ifndef _VTHREAD_H_
