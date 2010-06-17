/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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

#ifndef _ULINT_H_
#define _ULINT_H_

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#if defined(_WIN32)
typedef DWORD MXThreadID;
#define MXUSER_INVALID_OWNER 0xFFFFFFFF
#else
#include <pthread.h>
typedef pthread_t MXThreadID;
#endif

#include "vm_basic_types.h"
#include "vthreadBase.h"

#if defined(MXUSER_STATS)
#include "circList.h"

#define MXUSER_STAT_CLASS_ACQUISITION "a"
#define MXUSER_STAT_CLASS_HELD        "h"
#endif

/*
 * A portable recursive lock.
 */

#define MXUSER_MAX_REC_DEPTH 16

typedef struct {
#if defined(_WIN32)
   CRITICAL_SECTION nativeLock;       // Native lock object
#else
   pthread_mutex_t  nativeLock;       // Native lock object
#endif

   int              referenceCount;   // Acquisition count
   MXThreadID       nativeThreadID;   // Native thread ID

#if defined(MXUSER_DEBUG)
   VThreadID        portableThreadID;  // VThreadID, when available
   const void      *ownerRetAddr;      // return address of acquisition routine
#endif
} MXRecLock;


/*
 * Environment specific implementations of portable recursive locks.
 *
 * A recursive lock is used throughput the MXUser locks because:
 *     - it can be used recursively for recursive locks
 *     - exclusive (non-recursive) locks catch the recursion and panic
 *       rather than deadlock.
 *
 * There are 8 environment specific primitives:
 *
 * MXRecLockCreateInternal      Create lock before use
 * MXRecLockDestroyInternal     Destroy lock after use
 * MXRecLockIsOwner             Is lock owned by the caller?
 * MXRecLockSetNoOwner          Set lock as owner by "nobody"
 * MXRecLockSetOwner            Set lock owner
 * MXRecLockAcquireInternal     Lock the lock
 * MXRecLockTryAcquireInternal  conditionally acquire the lock
 * MXRecLockReleaseInternal     Unlock the lock
 *
 * Windows has a native recursive lock, the CRITICAL_SECTION. POSIXen,
 * unfortunately, do not ensure access to such a facility. The recursive
 * attribute of pthread_mutex_t is not implemented in all environments so
 * we create a recursive implementation using an exclusive pthread_mutex_t
 * and a few lines of code (most of which we need to do anyway).
 */

#if defined(_WIN32)
static INLINE int
MXRecLockCreateInternal(MXRecLock *lock)  // IN/OUT:
{
   Bool success;

   /* http://msdn.microsoft.com/en-us/library/ms682530(VS.85).aspx */
   /* magic number - allocate resources immediately; spin 0x400 times */
   success = InitializeCriticalSectionAndSpinCount(&lock->nativeLock,
                                                   0x80000400);

   return success ? 0 : GetLastError();
}


static INLINE int
MXRecLockDestroyInternal(MXRecLock *lock)  // IN/OUT:
{
   DeleteCriticalSection(&lock->nativeLock);

   return 0;
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return lock->nativeThreadID == GetCurrentThreadId();
}


static INLINE void
MXRecLockSetNoOwner(MXRecLock *lock)  // IN:
{
   lock->nativeThreadID = MXUSER_INVALID_OWNER;
}


static INLINE void
MXRecLockSetOwner(MXRecLock *lock)  // IN/OUT:
{
   lock->nativeThreadID = GetCurrentThreadId();
}


static INLINE int
MXRecLockAcquireInternal(MXRecLock *lock)  // IN:
{
   EnterCriticalSection(&lock->nativeLock);

   return 0;
}


static INLINE int
MXRecLockTryAcquireInternal(MXRecLock *lock)  // IN:
{
   return TryEnterCriticalSection(&lock->nativeLock) ? 0 : EBUSY;
}


static INLINE int
MXRecLockReleaseInternal(MXRecLock *lock)  // IN:
{
   LeaveCriticalSection(&lock->nativeLock);

   return 0;
}
#else
static INLINE int
MXRecLockCreateInternal(MXRecLock *lock)  // IN/OUT:
{
   return pthread_mutex_init(&lock->nativeLock, NULL);
}


static INLINE int
MXRecLockDestroyInternal(MXRecLock *lock)  // IN:
{
   return pthread_mutex_destroy(&lock->nativeLock);
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return pthread_equal(lock->nativeThreadID, pthread_self());
}


static INLINE void
MXRecLockSetNoOwner(MXRecLock *lock)  // IN/OUT:
{
   /* a hack but it works portably */
   memset((void *) &lock->nativeThreadID, 0xFF, sizeof(lock->nativeThreadID));
}


static INLINE void
MXRecLockSetOwner(MXRecLock *lock)  // IN:
{
   lock->nativeThreadID = pthread_self();
}


static INLINE int
MXRecLockAcquireInternal(MXRecLock *lock)  // IN:
{
   return pthread_mutex_lock(&lock->nativeLock);
}


static INLINE int
MXRecLockTryAcquireInternal(MXRecLock *lock)  // IN:
{
   return pthread_mutex_trylock(&lock->nativeLock);
}


static INLINE int
MXRecLockReleaseInternal(MXRecLock *lock)  // IN:
{
   return pthread_mutex_unlock(&lock->nativeLock);
}
#endif


static INLINE Bool
MXRecLockInit(MXRecLock *lock)  // IN/OUT:
{
   Bool success = (MXRecLockCreateInternal(lock) == 0);

   if (success) {
      MXRecLockSetNoOwner(lock);

      lock->referenceCount = 0;

#if defined(MXUSER_DEBUG)
      lock->portableThreadID = VTHREAD_INVALID_ID;
      lock->ownerRetAddr = NULL;
#endif
   }

   return success;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   int err = MXRecLockDestroyInternal(lock);

   if (vmx86_debug && (err != 0)) {
      Panic("%s: MXRecLockDestroyInternal returned %d\n", __FUNCTION__, err);
   }
} 


static INLINE uint32
MXRecLockCount(const MXRecLock *lock)  // IN:
{
   return lock->referenceCount;
}


static INLINE void
MXRecLockIncCount(MXRecLock *lock,  // IN/OUT:
                  void *location,   // IN:
                  uint32 count)     // IN:
{
   if (MXRecLockCount(lock) == 0) {
#if defined(MXUSER_DEBUG)
      ASSERT(lock->portableThreadID == VTHREAD_INVALID_ID);

      lock->ownerRetAddr = location;
      lock->portableThreadID = VThread_CurID();
#endif

      MXRecLockSetOwner(lock);
   }

   lock->referenceCount += count;
}


static INLINE Bool
MXRecLockAcquire(MXRecLock *lock,  // IN/OUT:
                 void *location)   // IN:
{
   Bool contended;

   if ((MXRecLockCount(lock) != 0) && MXRecLockIsOwner(lock)) {
      ASSERT((MXRecLockCount(lock) > 0) &&
             (MXRecLockCount(lock) < MXUSER_MAX_REC_DEPTH));

      contended = FALSE;
   } else {
      int err = MXRecLockTryAcquireInternal(lock);

      if (err == 0) {
         contended = FALSE;
      } else {
         if (vmx86_debug && (err != EBUSY)) {
            Panic("%s: MXRecLockTryAcquireInternal returned %d\n",
                  __FUNCTION__, err);
         }

         err = MXRecLockAcquireInternal(lock);
         contended = TRUE;
      }

      if (vmx86_debug && (err != 0)) {
         Panic("%s: MXRecLockAcquireInternal returned %d\n", __FUNCTION__,
               err);
      }

      ASSERT(lock->referenceCount == 0);
   }

   MXRecLockIncCount(lock, location, 1);

   return contended;
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,  // IN/OUT:
                    void *location)   // IN:
{
   int err;
   Bool acquired;

   err = MXRecLockTryAcquireInternal(lock);

   if (err == 0) {
      MXRecLockIncCount(lock, location, 1);

      ASSERT((MXRecLockCount(lock) > 0) &&
             (MXRecLockCount(lock) < MXUSER_MAX_REC_DEPTH));

      acquired = TRUE;
   } else {
      if (vmx86_debug && (err != EBUSY)) {
         Panic("%s: MXRecLockTryAcquireInternal returned %d\n", __FUNCTION__,
               err);
      }

      acquired = FALSE;
   }

   return acquired;
}

static INLINE void
MXRecLockDecCount(MXRecLock *lock,  // IN/OUT:
                  uint32 count)     // IN:
{
   ASSERT(count <= lock->referenceCount);
   lock->referenceCount -= count;

   if (MXRecLockCount(lock) == 0) {
      MXRecLockSetNoOwner(lock);

#if defined(MXUSER_DEBUG)
      lock->ownerRetAddr = NULL;
      lock->portableThreadID = VTHREAD_INVALID_ID;
#endif
   }
}


static INLINE void
MXRecLockRelease(MXRecLock *lock)  // IN/OUT:
{
   ASSERT((MXRecLockCount(lock) > 0) &&
          (MXRecLockCount(lock) < MXUSER_MAX_REC_DEPTH));

   MXRecLockDecCount(lock, 1);

   if (MXRecLockCount(lock) == 0) {
      int err = MXRecLockReleaseInternal(lock);

      if (vmx86_debug && (err != 0)) {
         Panic("%s: MXRecLockReleaseInternal returned %d\n", __FUNCTION__,
               err);
      }
   }
}


/*
 * MXUser header - all MXUser objects start with this
 */

typedef struct MXUserHeader {
   uint32       signature;
   MX_Rank      rank;
   const char  *name;
   void       (*dumpFunc)(struct MXUserHeader *);

#if defined(MXUSER_STATS)
   void       (*statsFunc)(struct MXUserHeader *);
   ListItem     item;
   uint32       identifier;
#endif
} MXUserHeader;


/*
 * The per thread information.
 */

#if defined(MXUSER_DEBUG) || defined(MXUSER_STATS)
#define MXUSER_MAX_LOCKS_PER_THREAD (2 * MXUSER_MAX_REC_DEPTH)

typedef struct {
#if defined(MXUSER_DEBUG)
   uint32         locksHeld;
   MXUserHeader  *lockArray[MXUSER_MAX_LOCKS_PER_THREAD];
#endif

#if defined(MXUSER_STATS)
   uint64         totalAcquisitions;      // total thread lock acquisitions
   uint64         contendedAcquisitions;  // contended subset of above
#endif
} MXUserPerThread;

MXUserPerThread *MXUserGetPerThread(VThreadID tid,
                                    Bool mayAlloc);
#endif

/*
 * Internal functions
 */

void MXUserDumpAndPanic(MXUserHeader *header,
                        const char *fmt,
                        ...);

MXRecLock *MXUserInternalSingleton(Atomic_Ptr *storage);

#if defined(MXUSER_DEBUG)
void MXUserAcquisitionTracking(MXUserHeader *header,
                           Bool checkRank);
void MXUserReleaseTracking(MXUserHeader *header);
#else
static INLINE void
MXUserAcquisitionTracking(MXUserHeader *header,  // IN:
                          Bool checkRank)        // IN:
{
   return;
}

static INLINE void
MXUserReleaseTracking(MXUserHeader *header)  // IN:
{
   return;
}
#endif

static INLINE Bool
MXUserTryAcquireFail(const char *name)  // IN:
{
   extern Bool (*MXUserTryAcquireForceFail)(const char *name);

   return vmx86_debug && MXUserTryAcquireForceFail &&
          (*MXUserTryAcquireForceFail)(name);
}

MXUserCondVar *MXUserCreateCondVar(MXUserHeader *header,
                                   MXRecLock *lock);

Bool MXUserWaitCondVar(MXUserHeader *header,
                       MXRecLock *lock,
                       MXUserCondVar *condVar,
                       uint32 msecWait);


#if defined(MXUSER_STATS)
typedef struct {
   char    *typeName;        // Name
   uint64   numSamples;      // Population sample size
   uint64   minTime;         // Minimum
   uint64   maxTime;         // Maximum
   uint64   timeSum;         // Sum of times (for mean)
   double   timeSquaredSum;  // Sum of times^2 (for S.D.)
} MXUserBasicStats;

typedef struct {
   uint64            numContended;     // Number of contended acquires
   uint64            timeContended;    // Time spent contended on acquires
   uint64            numUncontended;   // Number of uncontended acquires
   uint64            timeUncontended;  // Time spent uncontended on acquires

   MXUserBasicStats  basicStats;
} MXUserAcquisitionStats;

typedef struct {
   MXUserBasicStats  basicStats;       // total held statistics
} MXUserReleaseStats;

uint32 MXUserAllocID(void);
void MXUserAddToList(MXUserHeader *header);
void MXUserRemoveFromList(MXUserHeader *header);

typedef struct MXUserHisto MXUserHisto;

MXUserHisto *MXUserHistoSetUp(char *typeName,
                              uint64 minValue,
                              uint32 decades);

void MXUserHistoTearDown(MXUserHisto *histo);

void MXUserHistoSample(MXUserHisto *histo,
                       uint64 value);

void MXUserHistoDump(MXUserHisto *histo,
                     MXUserHeader *header);

void MXUserAcquisitionStatsSetUp(MXUserAcquisitionStats *stats);

void MXUserAcquisitionSample(MXUserAcquisitionStats *stats,
                             Bool wasContended,
                             uint64 timeToAcquire);

void MXUserDumpAcquisitionStats(MXUserAcquisitionStats *stats,
                                MXUserHeader *header);

void MXUserAcquisitionStatsTearDown(MXUserAcquisitionStats *stats);

void
MXUserBasicStatsSetUp(MXUserBasicStats *stats,
                      char *typeName);

void
MXUserBasicStatsSample(MXUserBasicStats *stats,
                       uint64 value);
 
void MXUserDumpBasicStats(MXUserBasicStats *stats,
                          MXUserHeader *header);

void MXUserBasicStatsTearDown(MXUserBasicStats *stats);

void MXUserKitchen(MXUserAcquisitionStats *stats,
                   double *contentionRatio,
                   Bool *isHot,
                   Bool *doLog);

void MXUserForceHisto(Atomic_Ptr *histoPtr,
                      char *typeName,
                      uint64 minValue,
                      uint32 decades);
#endif

extern void (*MXUserMX_LockRec)(struct MX_MutexRec *lock);
extern void (*MXUserMX_UnlockRec)(struct MX_MutexRec *lock);
extern Bool (*MXUserMX_TryLockRec)(struct MX_MutexRec *lock);
extern Bool (*MXUserMX_IsLockedByCurThreadRec)(const struct MX_MutexRec *lock);

#endif
