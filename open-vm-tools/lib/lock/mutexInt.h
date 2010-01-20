/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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
 * mutexInt.h --
 *
 *	Internal interface for the Mutex module
 */

#ifndef _MUTEXINT_H_
#define _MUTEXINT_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "mutex.h"
#if defined VMX86_VMX || defined VMM || defined MONITOR_APP
#include "usercall.h"
#endif

/*
 * NOTE to Windows users on semaphores:
 *
 * The semaphores that are implemented here are for the implementation
 * of the MX_* primitives.  They are used in a specialized way such that 
 * they serve the purpose of the MX_* implementation. The Windows 
 * implementation uses Windows Event objects and not Semaphore objects 
 * for implementation reasons. 
 * The Windows driver needs to access the Windows Event object during a 
 * module call. The object lookup function in the driver can lookup up 
 * Windows Event objects and not Windows Semaphore objects.  Since Windows
 * Event objects have the proper semantics, they are used to implement
 * our semaphores.
 *
 * Since the monitor is always compiled on Linux, SEMA_INVALID_HANDLE
 * really has to be the same value on all hosts.  This nicely matches
 * Windows' INVALID_HANDLE_VALUE, which is asserted in MXSemaphoreInit().
 */ 

#if defined VMX86_VMX || defined VMM || defined MONITOR_APP
typedef struct MXSemaphoreRPC {
   RPCBlock     rpc;
   uint32       _pad0;
   MXSemaphore  sema;
} MXSemaphoreRPC;
#endif

void MXSemaphoreInit(MXSemaphore *sema);
void MXSemaphoreInvalidate(MXSemaphore *sema);
void MXSemaphoreDestroy(MXSemaphore *sema);
void MXSemaphoreWait(MXSemaphore *sema, MX_Rank rank);
Bool MXSemaphoreWaitTimeout(MXSemaphore *sema, MX_Rank rank, int maxWaitUS);
Bool MXSemaphoreTryWait(MXSemaphore *sema);
void MXSemaphoreSignal(MXSemaphore *sema);


/*
 * Locks
 */

#define MX_LOCK_ID_NULL  MX_MAX_LOCKS

/*
 * Each lock has an affiliate (per-lock) data structure. This data structure 
 * is kept separately from the lock itself to make it visible across the 
 * vmx/vmm divide, regardless of the visibility of the lock.
 */

typedef struct MXPerLock {
   MXSemaphore   sema;                /* Semaphore for blocking.             */
   Atomic_uint32 isActive;            /* Whether this "perLock" is active.   */
#if MX_FAT_LOCKS != 0
   uint32      pad;                   /* Pad for windows (align next field). */
   uint64      lockCount;             /* #times this lock was acquired.      */
   uint64      lockCountBlocking;     /* #times acquisition was blocking.    */
   MX_Rank     rank;                  /* Rank of this lock; immutable.       */
   MX_LockID   next;                  /* Next held lock on per-thread list.  */
   char        name[MX_MAX_NAME_LEN]; /* Zero-terminated name for lock.      */
#endif
} MXPerLock;


/*
 * Condition variables. We use only CAS and the host binary semaphore
 * primitive.
 *
 * We keep a list of waiters, who are awakened in FIFO order. Because it
 * is impossible to append to the end of a singly-linked list using only
 * CAS, we use an unusual representation of the queue waiters.
 *
 * The queue is represented as two lists, "head" and "tail." The first item in
 * the queue appears in "head". Subsequent items in the queue may follow in
 * "head". If "tail" is non-empty, it contains a list of queue items logically
 * behind all those in "head", and _in reversed order_. I.e., if "tail" isn't
 * empty, its _first_ item is the _last_ item in the queue.
 *
 * This way, entering an item into the queue always boils down to the simple,
 * CAS-implementable operation of prepending an item to the head of a linked
 * list: the "head" list if the queue is empty, and the "tail" list otherwise.
 */

#define MX_LIST_INVAL ((MXCVLink)-1U)

/*
 * Support for locks and condition variables
 */

typedef struct MXPerThread {
#if MX_FAT_LOCKS != 0
   uint64      lockCount;            /* #times this thread acquired a lock.  */
   uint64      lockCountBlocking;    /* #times acquisition was blocking.     */
   MX_LockID   first;                /* First lock held by this thread.      */
   uint32      partiallyLocked;      /* Locks partially acquired by thread.  */
#endif
   MXSemaphore sema;                 /* sema for CV blocking                 */
   MXCVLink    cvLink;               /* forward link for CV wait queue       */
   Bool        initialized;
   uint8       _pad[6];
} MXPerThread;

typedef struct MXState {
#if defined(__i386__) || defined(__x86_64__)
   uint64        startTSC;                  /* Init-time TSC.                */
#endif
   Atomic_uint32 numLocks;                  /* Number of active locks.       */
   unsigned      spinLimit;                 /* Spin limit before blocking.   */
   Bool          doneInit;                  /* To make init idempotent.      */
   uint32        pad;                       /* Make windows and linux agree. */
   MXPerLock     perLock[MX_MAX_LOCKS];     /* Indexed by lock id.           */
   MXPerThread   perThread[VTHREAD_MAX_THREADS]; /* Indexed by thread id.    */
} MXState;

#ifdef VMM
EXTERN SHARED_PER_VM MXState mxState;
#else
extern MXState *mxState;             /* Module state; in shared area. */
#endif

static INLINE MXState *
GetMXState(void)
{
#ifdef VMM
   return &mxState;
#else
   return mxState;
#endif
}

/*
 * Iterate "lid" over all locks held by "tid", which *must* be the 
 * current thread to avoid races.
 */

#define FOR_ALL_LOCKS_HELD(tid, lid)     \
   for (lid = GetPerThread(tid)->first;  \
        lid != MX_LOCK_ID_NULL;          \
        lid = GetPerLock(lid)->next)

#define FOR_ALL_ACTIVE_LOCKS(lid)            \
   for (lid = 0; lid < MX_MAX_LOCKS; lid++)  \
      if (Atomic_Read(&GetPerLock(lid)->isActive))

static INLINE MXPerLock *
GetPerLock(MX_LockID lid)  // IN:
{
   ASSERT(lid < MX_MAX_LOCKS);

   return GetMXState()->perLock + lid;
}

static INLINE MXPerThread *
GetPerThread(VThreadID tid)  // IN:
{
   return GetMXState()->perThread + tid;
}

static INLINE MXPerThread *
GetMyPerThread(void)
{
   return GetPerThread(VThread_CurID());
}

static INLINE MXSemaHandle
MXSemaphoreGetSemaHandle(MXSemaphore *sema)  // IN:
{
#ifdef _WIN32
   MXSemaHandle *ph = (MXSemaHandle *) &sema->waitHandle;

   return *ph;
#else
   return sema->waitHandle;
#endif
}

#ifdef _WIN32

static INLINE void
MXSemaphoreSetSemaHandle(MXSemaphore *sema,  // IN/OUT:
                         MXSemaHandle h)     // IN:
{
   MXSemaHandle *ph = (MXSemaHandle *) &sema->waitHandle;
   *ph = h;

#ifndef _WIN64
   /*
    * This handle might be seen by a 64-bit vmmon, so make sure the
    * upper 32-bits are set correctly.
    */

   if (h == (MXSemaHandle) VMW_INVALID_HANDLE) {
      sema->signalHandle = (int) VMW_INVALID_HANDLE;
   } else {
      sema->signalHandle = 0;
   }
#endif // _WIN64
}

#endif // _WIN32

#ifdef USERLEVEL
void MXInvalidatePerThread(VThreadID tid);
#endif


/*
 * Lock stats
 */

#if MX_STATS_LEVEL > 2
void MXIncLockCount(void);
#else
#define MXIncLockCount()
#endif

#define MX_WAITTIMEOUT       1000   // milliseconds

#endif // ifndef _MUTEXINT_H_
