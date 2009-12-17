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

#ifndef _MUTEX_H_
#define _MUTEX_H_

#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifdef _WIN32
#include <WTypes.h> // for HANDLE
#endif

#include "vm_atomic.h"
#include "vthread.h"
#if defined VMM && defined VMCORE
#include "mon_types.h"
#include "x86regname.h"
#endif

#if defined VMM || defined VMX86_VMX
#include "vprobe_static.h"
#define VMM_VMX_ONLY(x) x
#else
#define VMM_VMX_ONLY(x)
#endif

/*
 * mutex.h --
 *
 * This is the header file for mutual exclusion locks ("mutexes"). 
 * These locks can be used by both monitor and user level code. 
 *
 * We provide two variants: regular locks and recursive locks. The
 * latter can be reacquired by the owning thread, in a counting manner.
 *
 * In optimized builds, locks are minimal and fast. In debug builds,
 * we maintain additional information to catch improper use of locks.
 *
 */

/* If adjusting MX_MAX_LOCKS, also adjust VMK's RPC_MAX_WORLD_CONNECTIONS. */
#define MX_MAX_LOCKS             160 /* Max # of locks supported */ 
#define MX_MAX_NAME_LEN          16  /* Max lock/condvar name len incl. zero. */

/* 
 * On linux, semaphore use a pipe for waiting/waking up. If the pipe fills 
 * up, a thread doing a V() can block, which is not very nice. Which is
 * why we cap the number of signals that a semaphore can accumulate.
 */ 

#define MX_MAX_SEMASIGNALS       64  /* Max #of accumulated signals in a counting sema. */

typedef uint32 MX_LockID;
typedef uint8  MXCVLink;

typedef struct MXCVQueue {
   MXCVLink head;
   MXCVLink tail;
   uint8    nsigs;
   uint8    nwaits;   
} MXCVQueue;

typedef union MXCVWord {             /* Private to this module. */
   uint32        all;
   Atomic_uint32 atomic;
   MXCVQueue     s;
} MXCVWord;

/*
 * The detail-level of stats collection is compile-time selectable. 
 *   0: no stats collected
 *   1: samples lock usage at each timer tick
 *   2: count blocking/nonblocking lock operations
 *   3: like level 2, but additionally collect call chains.
 * The defaults for the various builds are: obj = 2, opt = 1, others = 0
 * Lock stats are currently disabled for opt builds. Will reenable soon.
 */

#if defined VMX86_VMX || defined VMM
#define MX_STATS_LEVEL \
    (vmx86_stats ? (vmx86_debug ? 2 : 1) : 0)
#else
#define MX_STATS_LEVEL 0
#endif

#if MX_STATS_LEVEL > 0
#define MX_STATS_ONLY(x) x
#else
#define MX_STATS_ONLY(x)
#endif

/* When we compile with debugging or lock stats, we include extra fields. */
#define MX_FAT_LOCKS (vmx86_debug || MX_STATS_LEVEL > 1)

#if MX_FAT_LOCKS != 0
#define MX_FAT_LOCKS_ONLY(x) x
#else
#define MX_FAT_LOCKS_ONLY(x)
#endif

/*
 * For 64-bit Windows, the single 64-bit semaphore handle is spread
 * across waitHandle (lower half) and signalHandle (upper half). For
 * 32-bit Windows, the single 32-bit semaphore handle is stored in
 * waitHandle. POSIX platforms use two 32-bit fds, waitHandle and
 * signalHandle.
 *
 * #ifdefs and unions are avoided here because of offsetChecker
 * limitations and padding issues. Wrapper functions for Windows
 * provide get/set access to the handle.
 */

typedef struct MXSemaphore {
   int           waitHandle;
   int           signalHandle;
   Atomic_uint32 signalled;
   uint32        pad;
#if MX_FAT_LOCKS != 0
   uint64        blockTime;
#endif
} MXSemaphore;

/* 
 * Although we define these types in a public header file, code 
 * in other modules should never access the lock fields directly. 
 */

typedef struct MX_Mutex {
   Atomic_uint32   nthreads; /* #threads interested in lock; incl. owner.    */
   MX_LockID       lid;      /* Unique id of lock; set at lock init time.    */
   MX_Rank         rank;     /* Rank of this lock; immutable.                */
   uint32          _pad;     /* pad to 8-byte boundary.                      */
#if MX_FAT_LOCKS != 0
   Bool            tracing;  /* True iff tracing is enabled for this lock.   */
   VThreadID       owner;    /* Thread that currently holds lock (if any).   
                              * Only used for assertion purposes. */
   uint64          ip;       /* R/EIP where lock was acquired.               */
#endif
} MX_Mutex;

typedef struct MX_MutexRec {
   MX_Mutex        lck;      /* The core lock in this recursive mutex.       */
   VThreadID       owner;    /* Owner thr. if lock held; else invalid id.    */
   unsigned        count;    /* Number of times currently locked.            */
} MX_MutexRec;

typedef struct MX_Condvar {
   MXCVWord      cvword;     /* Semantics private to this module. */
#if MX_FAT_LOCKS != 0
   char          name[MX_MAX_NAME_LEN]; /* Zero-terminated name for condvar. */
#endif
} MX_Condvar;

typedef struct MX_Barrier {
   MX_Mutex   lck;           /* Lock that protects barrier state.            */
   MX_Condvar cv;            /* Threads wait here for barrier to saturate.   */
   unsigned   threshold;     /* Barrier threshold; set at init time only.    */
   unsigned   nEntered;      /* Number of threads that have reached barrier. */
   MX_FAT_LOCKS_ONLY(uint32 _pad;)
} MX_Barrier;

typedef struct MX_BinSemaphore {
   MXSemaphore   sema;
   MX_Rank       rank;
   Atomic_uint32 signalled;
#if MX_FAT_LOCKS != 0
   char          name[MX_MAX_NAME_LEN]; /* Name of binary semaphore. */
#endif
} MX_BinSemaphore;

typedef struct MX_CountingSemaphore {
   MXSemaphore   sema;
   MX_Rank       rank;
   uint32        _pad;
#if MX_FAT_LOCKS != 0
   char          name[MX_MAX_NAME_LEN]; /* Name of counting semaphore. */
#endif
} MX_CountingSemaphore;

#if MX_STATS_LEVEL > 0
EXTERN MX_Mutex *curLock;
EXTERN void MXInitLockStats(const char *name, MX_Mutex *lck);
#else
#define MXInitLockStats(name, lck)
#endif

#if MX_STATS_LEVEL > 1
EXTERN void MX_LogStats(unsigned epoch);
#else
#define MX_LogStats(epoch)
#endif

#if MX_FAT_LOCKS != 0
EXTERN void MXInitLockFat(const char *name, MX_Rank rank, MX_Mutex *lck);
EXTERN void MXAcquiredLock(MX_Mutex *lck, Bool blocking);
EXTERN void MX_AssertNoLocksHeld(Bool checkPendingLocks);
EXTERN void MXInitCondvarFat(const char *name, MX_Condvar *cv);
EXTERN void MX_CheckRank(MX_Rank rank, const char *name);
EXTERN void MX_CheckRankWithBULL(MX_MutexRec *lock, Bool belowUser);
EXTERN Bool MX_IsLockedByThread(const MX_Mutex *lck, VThreadID tid);
EXTERN Bool MX_IsLockedByCurThread(const MX_Mutex *lck);
EXTERN MX_Rank MX_CurrentRank(void);
#else
/* Use macros to keep string arguments out of release builds. */
#define MXInitLockFat(name, rank, lck)
#define MXAcquiredLock(lck, blocking)
#define MX_AssertNoLocksHeld(checkPendingLocks)
#define MXInitCondvarFat(name, cv)
#define MX_CheckRank(rank, name)
#define MX_CheckRankWithBULL(lock, belowUser)
#endif

EXTERN void MXInitLockWork(MX_Mutex *lck, MX_Rank rank);
EXTERN size_t MX_GetMXStateSize(void);
EXTERN void MX_Init(void *mxStatePtr);
EXTERN void MX_PlantThreadWatchDog(void (*func)(void));
EXTERN void MX_InitPerThread(VThreadID tid);
EXTERN void MX_ExitPerThread(VThreadID tid);
EXTERN void MX_Shutdown(void);
EXTERN void MX_Lock(MX_Mutex *lck);
EXTERN void MX_Unlock(MX_Mutex *lck);
EXTERN void MX_DestroyLock(MX_Mutex *lck);
EXTERN void MX_LockRec(MX_MutexRec *lckr);
EXTERN void MX_UnlockRec(MX_MutexRec *lckr);
EXTERN Bool MX_TryLockRec(MX_MutexRec *lckr);
EXTERN Bool MX_IsLockedByThreadRec(const MX_MutexRec *lckr, VThreadID tid);
EXTERN Bool MX_IsLockedByCurThreadRec(const MX_MutexRec *lckr);
EXTERN void MXInitCondvarWork(MX_Condvar *cv);
EXTERN void MX_Signal(MX_Condvar *cv);
EXTERN void MX_Broadcast(MX_Condvar *cv);
EXTERN void MX_Wait(MX_Condvar *cv, MX_Mutex *lck);
EXTERN void MX_WaitRec(MX_Condvar *cv, MX_MutexRec *lckr);
EXTERN void MX_InitBarrier(MX_Barrier *br, MX_Rank rank, unsigned threshold);
EXTERN void MX_EnterBarrier(MX_Barrier *br);
#if !defined VMM
EXTERN Bool MX_WaitTimeout(MX_Condvar *cv, MX_Mutex *lck, int maxWaitUS);
EXTERN Bool MX_WaitRecTimeout(MX_Condvar *cv, MX_MutexRec *lckr, int maxWaitUS);
#endif

EXTERN void MX_InitBinSemaphore(const char *name, MX_Rank rank, MX_BinSemaphore *binSema);
EXTERN MXSemaHandle MX_BinSemaphoreGetSemaHandle(MX_BinSemaphore *binSema);
#ifdef _WIN32
EXTERN void MX_BinSemaphoreSetSemaHandle(MX_BinSemaphore *binSema, MXSemaHandle h);
#endif
EXTERN void MX_DestroyBinSemaphore(MX_BinSemaphore *binSema);
EXTERN void MX_BinSemaphoreWait(MX_BinSemaphore *binSema);
#if !defined VMM
EXTERN Bool MX_BinSemaphoreTryWaitTimeout(MX_BinSemaphore *binSema, int usTimeout);
#endif
EXTERN void MX_BinSemaphoreSignal(MX_BinSemaphore *binSema);
EXTERN Bool MX_BinSemaphoreTryWait(MX_BinSemaphore *binSema);
EXTERN void MX_InitCountingSemaphore(const char *name, MX_Rank rank, MX_CountingSemaphore *csema);
EXTERN void MX_DestroyCountingSemaphore(MX_CountingSemaphore *csema);
EXTERN void MX_CountingSemaphoreWait(MX_CountingSemaphore *csema);
EXTERN void MX_CountingSemaphoreSignal(MX_CountingSemaphore *csema);
EXTERN Bool MX_CountingSemaphoreTryWait(MX_CountingSemaphore *csema);

#if defined VMM
#ifdef VMX86_DEBUG
EXTERN Bool prohibitBarriers;
#endif
EXTERN void MX_LockMarkEnd(void);
EXTERN void MX_UnlockMarkEnd(void);
#if defined VMCORE
EXTERN void MX_LockCalloutStart(void);
EXTERN void MX_LockCalloutEnd(void);
EXTERN TCA  MX_LockEmit(MX_Mutex *lck, TCA memptr);
EXTERN TCA  MX_UnlockEmit(MX_Mutex *lck, TCA memptr);
EXTERN TCA  MX_LockIndEmit(RegisterName reg, TCA memptr);
EXTERN TCA  MX_UnlockIndEmit(RegisterName reg, TCA memptr);
EXTERN TCA  MX_LockRecEmit(MX_MutexRec *lck, TCA memptr);
EXTERN TCA  MX_UnlockRecEmit(MX_MutexRec *lck, TCA memptr);
EXTERN Bool MX_VCPUNeedsForceWakeup(Vcpuid vcpuid);
EXTERN void MX_SemaphoreForceWakeup(Vcpuid vcpuid);
#endif
#else
EXTERN Bool MX_PowerOn(void);
EXTERN void MX_PowerOff(void);
EXTERN Bool MX_IsInitialized(void);
#endif


/*
 *----------------------------------------------------------------------
 * MX_InitLock --
 *    Initialize a lock. 
 *    Use macros to keep strings out of release builds.
 *    Types: const char *name, MX_Rank rank, MX_Mutex *lck.
 *----------------------------------------------------------------------
 */

#define MX_InitLock(name, rank, lck)                                          \
   do {                                                                       \
      MX_Mutex *_lck = (lck);                                                 \
                                                                              \
      /* Use two fcts to make name dead in release builds. */                 \
      MXInitLockWork(_lck, rank);                                             \
      MXInitLockFat((name), (rank), _lck);                                    \
      MXInitLockStats((name), _lck);                                          \
   } while (0)


/*
 *----------------------------------------------------------------------
 * MX_IsLocked --
 *    Return TRUE iff "lck" is currently locked by some thread.
 *    Since this is an unstable property, except when it is known that
 *    the current thread holds the lock, this operation is mostly useful
 *    to assert that a lock is held (by the current thread).
 *----------------------------------------------------------------------
 */

static INLINE Bool
MX_IsLocked(MX_Mutex *lck)  // IN:
{ 
   return Atomic_Read(&lck->nthreads) != 0;
}


/*
 *----------------------------------------------------------------------
 * MX_TryLock --
 *    Try to get a lock, but don't block if it is unavailable.
 *    Return TRUE if lock was successfully acquired, FALSE otherwise.
 *----------------------------------------------------------------------
 */

static INLINE Bool
MX_TryLock(MX_Mutex *lck)  // IN:
{
   /* This code was written to produce good code for MX_Lock(). 
      If you change it, be sure to inspect the generated code. */
   uint32 old = Atomic_ReadIfEqualWrite(&lck->nthreads, 0, 1);

   if (old == 0) {
#if MX_FAT_LOCKS != 0 
      {
         lck->ip = (uint64)(uintptr_t) GetReturnAddress();
      }
#endif
      MXAcquiredLock(lck, FALSE);
      VMM_VMX_ONLY(VProbe_2Args(VPROBE_MX_LockAcquired, lck->lid, lck->rank));
   }

   return old == 0;
}


/*
 *----------------------------------------------------------------------
 * MX_SetTracing --
 *    Enable or disable tracing of activity on a lock. 
 *    No-op in non-debug builds.
 *----------------------------------------------------------------------
 */

static INLINE void 
MX_SetTracing(MX_Mutex *lck,  // IN:
              Bool t)         // IN:
{
   DEBUG_ONLY(lck->tracing = t);
}


/*
 *----------------------------------------------------------------------
 * MX_InitLockRec --
 *    Initialize a recursive lock.
 *    Use macros to keep strings out of release builds.
 *    Types: const char *name, MX_Rank rank, MX_MutexRec *lckr
 *----------------------------------------------------------------------
 */

#define MX_InitLockRec(name, rank, lckr)                                      \
   do {                                                                       \
      MX_MutexRec *_lckr = (lckr);                                            \
      MX_InitLock((name), (rank), &_lckr->lck);                               \
      _lckr->owner = VTHREAD_INVALID_ID;                                      \
      _lckr->count = 0;                                                       \
   } while (0)


/*
 *----------------------------------------------------------------------
 * MX_IsLockedRec --
 *    Similar to "MX_IsLocked()" but applies to recursive locks.
 *----------------------------------------------------------------------
 */

static INLINE Bool
MX_IsLockedRec(MX_MutexRec *lckr)  // IN:
{
   return MX_IsLocked(&lckr->lck);
}


/*
 *----------------------------------------------------------------------
 * MX_SetTracingRec --
 *    Similar to "MX_SetTracing()" but applies to recursive locks.
 *----------------------------------------------------------------------
 */

static INLINE void 
MX_SetTracingRec(MX_MutexRec *lckr,  // IN:
                 Bool t)             // IN:
{
   MX_SetTracing(&lckr->lck, t);
}


/*
 *----------------------------------------------------------------------
 * MX_DestroyLockRec --
 *    Like "MX_DestroyLock()" but applies to recursive mutex.
 *----------------------------------------------------------------------
 */

static INLINE void 
MX_DestroyLockRec(MX_MutexRec *lckr)  // IN:
{ 
   MX_DestroyLock(&lckr->lck); 
}


/*
 *----------------------------------------------------------------------
 * MX_LockOwnerRec --
 *    Return the lock owner, or VTHREAD_INVALID_ID.
 *----------------------------------------------------------------------
 */

static INLINE VThreadID 
MX_LockOwnerRec(MX_MutexRec *lckr)  // IN:
{ 
   return lckr->owner;
}


/*
 *----------------------------------------------------------------------
 * MX_InitCondvar --
 *    Initialize a condition variable.
 *    Use macro to keep strings out of release builds.
 *    Types: const char *name, MX_Condvar *cv.
 *----------------------------------------------------------------------
 */

#define MX_InitCondvar(name, cv)                                              \
   do {                                                                       \
      MX_Condvar *_cv = (cv);                                                 \
      MXInitCondvarWork(_cv);                                                 \
      MXInitCondvarFat((name), _cv);                                          \
   } while (0)


/*
 *----------------------------------------------------------------------
 * MX_DestroyBarrier --
 *    Release all resources held by a barrier.
 *----------------------------------------------------------------------
 */

static INLINE void
MX_DestroyBarrier(MX_Barrier *br)  // IN:
{
   ASSERT(br->nEntered == 0);
   MX_DestroyLock(&br->lck);
}


/*
 *----------------------------------------------------------------------
 * MX_GetRank --
 *    Returns a lock's rank.
 *----------------------------------------------------------------------
 */

static INLINE MX_Rank
MX_GetRank(MX_Mutex *lock)  // IN:
{
   ASSERT(lock != NULL);

   return lock->rank;
}


/*
 *----------------------------------------------------------------------
 * MX_GetRankRec --
 *    Returns a recursive lock's rank.
 *----------------------------------------------------------------------
 */

static INLINE MX_Rank
MX_GetRankRec(MX_MutexRec *lock)  // IN:
{
   ASSERT(lock != NULL);

   return MX_GetRank(&lock->lck);
}


#if defined VMM && defined VMCORE // {

EXTERN int inSemaWait;

/*
 *-----------------------------------------------------------------------------
 *
 * MX_SemaphoreInWait --
 *    TRUE if this vcpu is waiting on any semaphore. This is currently used by 
 *    the poweroff code to find out whether we are holding any (partial) locks.
 *
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
MX_SemaphoreInWait(void)
{
   ASSERT(inSemaWait >= 0);

   return inSemaWait > 0;
}

#endif // }

#endif  // _MUTEX_H_
