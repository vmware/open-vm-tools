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

#if defined(_WIN32)
typedef DWORD MXThreadID;
#define MXUSER_INVALID_OWNER 0xFFFFFFFF
#else
#include <pthread.h>
#include <errno.h>
typedef pthread_t MXThreadID;
#endif

#include "vm_basic_types.h"
#include "vthreadBase.h"
#include "ulIntShared.h"

#if defined(MXUSER_STATS)
#include "circList.h"

#define MXUSER_HISTOGRAM_NS_PER_BIN 2000
#define MXUSER_HISTOGRAM_MAX_BINS 500

#define MXUSER_STAT_CLASS_ACQUISITION "acquisition"
#define MXUSER_STAT_CLASS_HELD        "held"
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
 * There are 6 environment specific primitives:
 *
 * MXRecLockObjectInit   initialize native lock before use
 * MXRecLockDestroy      destroy lock after use
 * MXRecLockIsOwner      is lock owned by caller?
 * MXRecLockAcquire      lock
 * MXRecLockTryAcquire   conditional lock
 * MXRecLockRelease      unlock
 *
 * Windows has a native recursive lock, the CRITICAL_SECTION. POSIXen,
 * unfortunately, do not ensure access to such a facility. The recursive
 * attribute of pthread_mutex_t is not implemented in all environments so
 * we create a recursive implementation using an exclusive pthread_mutex_t
 * and a few lines of code (most of which we need to do anyway).
 */

#if defined(_WIN32)
static INLINE Bool
MXRecLockObjectInit(CRITICAL_SECTION *nativeLock)  // IN/OUT:
{
   /* http://msdn.microsoft.com/en-us/library/ms682530(VS.85).aspx */
   /* magic number - allocate resources immediately; spin 0x400 times */
   return InitializeCriticalSectionAndSpinCount(nativeLock, 0x80000400) != 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   DeleteCriticalSection(&lock->nativeLock);
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return lock->nativeThreadID == GetCurrentThreadId();
}


static INLINE Bool
MXRecLockAcquire(MXRecLock *lock,        // IN/OUT:
                 const void *location)   // IN:
{
   Bool acquired;
   Bool contended;

   acquired = TryEnterCriticalSection(&lock->nativeLock);

   if (acquired) {
      contended = FALSE;
   } else {
      EnterCriticalSection(&lock->nativeLock);
      contended = TRUE;
   }

   ASSERT((lock->referenceCount >= 0) &&
           (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

   if (lock->referenceCount == 0) {
      ASSERT(lock->nativeThreadID == MXUSER_INVALID_OWNER);
      lock->nativeThreadID = GetCurrentThreadId();

#if defined(MXUSER_DEBUG)
      lock->ownerRetAddr = location;
      lock->portableThreadID = VThread_CurID();
#endif
   }

   lock->referenceCount++;

   return contended;
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,        // IN/OUT:
                    const void *location)   // IN:
{
   Bool acquired;

   acquired = TryEnterCriticalSection(&lock->nativeLock);

   if (acquired) {
      ASSERT((lock->referenceCount >= 0) &&
             (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

      if (lock->referenceCount == 0) {
         ASSERT(lock->nativeThreadID == MXUSER_INVALID_OWNER);
         lock->nativeThreadID = GetCurrentThreadId();

#if defined(MXUSER_DEBUG)
         lock->ownerRetAddr = location;
         lock->portableThreadID = VThread_CurID();
#endif
      }

      lock->referenceCount++;
   }

   return acquired;
}


static INLINE void
MXRecLockRelease(MXRecLock *lock)  // IN/OUT:
{
   ASSERT((lock->referenceCount > 0) &&
          (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

   lock->referenceCount--;

   if (lock->referenceCount == 0) {
      lock->nativeThreadID = MXUSER_INVALID_OWNER;

#if defined(MXUSER_DEBUG)
      lock->ownerRetAddr = NULL;
      lock->portableThreadID = VTHREAD_INVALID_ID;
#endif
   }

   LeaveCriticalSection(&lock->nativeLock);
}
#else
static INLINE Bool
MXRecLockObjectInit(pthread_mutex_t *nativeLock)  // IN/OUT:
{
   int err;

   err = pthread_mutex_init(nativeLock, NULL);

   if (vmx86_debug && (err != 0)) {
      Panic("%s: pthread_mutex_init returned %d\n", __FUNCTION__, err);
   }

   return err == 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   int err;

   err = pthread_mutex_destroy(&lock->nativeLock);

   if (vmx86_debug && (err != 0)) {
      Panic("%s: pthread_mutex_destroy returned %d\n", __FUNCTION__, err);
   }
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return pthread_equal(lock->nativeThreadID, pthread_self());
}


static INLINE Bool
MXRecLockAcquire(MXRecLock *lock,  // IN/OUT:
                 void *location)   // IN:
{
   Bool contended;

   if ((lock->referenceCount != 0) &&
        pthread_equal(lock->nativeThreadID, pthread_self())) {
      ASSERT((lock->referenceCount > 0) &&
             (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

      lock->referenceCount++;

      contended = FALSE;
   } else {
      int err;

      err = pthread_mutex_trylock(&lock->nativeLock);

      if (err == 0) {
         contended = FALSE;
      } else {
         if (vmx86_debug && (err != EBUSY)) {
            Panic("%s: pthread_mutex_trylock returned %d\n", __FUNCTION__,
                  err);
         }

         err = pthread_mutex_lock(&lock->nativeLock);
         contended = TRUE;
      }

      if (vmx86_debug && (err != 0)) {
         Panic("%s: pthread_mutex_lock returned %d\n", __FUNCTION__, err);
      }

      ASSERT(lock->referenceCount == 0);

#if defined(MXUSER_DEBUG)
      ASSERT(lock->portableThreadID == VTHREAD_INVALID_ID);

      lock->ownerRetAddr = location;
      lock->portableThreadID = VThread_CurID();
#endif

      lock->nativeThreadID = pthread_self();
      lock->referenceCount = 1;
   }

   return contended;
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,  // IN/OUT:
                    void *location)   // IN:
{
   int err;
   Bool acquired;

   err = pthread_mutex_trylock(&lock->nativeLock);

   if (err == 0) {
      ASSERT((lock->referenceCount >= 0) &&
             (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

      if (lock->referenceCount == 0) {
#if defined(MXUSER_DEBUG)
         ASSERT(lock->portableThreadID == VTHREAD_INVALID_ID);

         lock->ownerRetAddr = location;
         lock->portableThreadID = VThread_CurID();
#endif

         lock->nativeThreadID = pthread_self();
      }

      lock->referenceCount++;

      acquired = TRUE;
   } else {
      if (vmx86_debug && (err != EBUSY)) {
         Panic("%s: pthread_mutex_trylock returned %d\n", __FUNCTION__, err);
      }

      acquired = FALSE;
   }

   return acquired;
}


static INLINE void
MXRecLockRelease(MXRecLock *lock)  // IN/OUT:
{
   ASSERT((lock->referenceCount > 0) &&
          (lock->referenceCount < MXUSER_MAX_REC_DEPTH));

   lock->referenceCount--;

   if (lock->referenceCount == 0) {
      int err;

      /* a hack but it works portably */
      memset((void *) &lock->nativeThreadID, 0xFF,
             sizeof(lock->nativeThreadID));

#if defined(MXUSER_DEBUG)
      lock->ownerRetAddr = NULL;
      lock->portableThreadID = VTHREAD_INVALID_ID;
#endif

      err = pthread_mutex_unlock(&lock->nativeLock);

      if (vmx86_debug && (err != 0)) {
         Panic("%s: pthread_mutex_unlock returned %d\n", __FUNCTION__, err);
      }
   }
}
#endif


/*
 * Initialization of portable recursive lock.
 */

static INLINE Bool
MXRecLockInit(MXRecLock *lock)  // IN/OUT:
{
   if (!MXRecLockObjectInit(&lock->nativeLock)) {
      return FALSE;
   }

#if defined(_WIN32)
   lock->nativeThreadID = MXUSER_INVALID_OWNER;
#else
   /* a hack but it works portably */
   memset((void *) &lock->nativeThreadID, 0xFF, sizeof(lock->nativeThreadID));
#endif

   lock->referenceCount = 0;

#if defined(MXUSER_DEBUG)
   lock->portableThreadID = VTHREAD_INVALID_ID;
   lock->ownerRetAddr = NULL;
#endif

   return TRUE;
}


static INLINE uint32
MXRecLockCount(const MXRecLock *lock)  // IN:
{
   return lock->referenceCount;
}

/*
 * MXUser lock header - all MXUser locks start with this
 */

#define USERLOCK_SIGNATURE 0x4B434F4C // 'LOCK' in memory

typedef struct MXUserHeader {
   uint32       lockSignature;
   MX_Rank      lockRank;
   const char  *lockName;
   void       (*lockDumper)(struct MXUserHeader *);

#if defined(MXUSER_STATS)
   void       (*lockStatsAction)(struct MXUserHeader *, unsigned epoch);
   ListItem     lockItem;
   uint32       lockID;
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
MXUserTryAcquireFail(const char *lockName)  // IN:
{
   extern Bool (*MXUserTryAcquireForceFail)(const char *lockName);

   return vmx86_debug && MXUserTryAcquireForceFail &&
          (*MXUserTryAcquireForceFail)(lockName);
}

MXUserCondVar *MXUserCreateCondVar(MXUserHeader *header,
                                   MXRecLock *lock);

void MXUserWaitCondVar(MXUserHeader *header,
                       MXRecLock *lock,
                       MXUserCondVar *condVar);


#if defined(MXUSER_STATS)
typedef struct {
   uint64  numSamples;      // Population sample size
   uint64  minTime;         // Minimum
   uint64  maxTime;         // Maximum
   uint64  timeSum;         // Sum of times (for mean)
   double  timeSquaredSum;  // Sum of times^2 (for S.D.)
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

uint64 MXUserReadTimerNS(void);
uint32 MXUserAllocID(void);
void MXUserAddToList(MXUserHeader *header);
void MXUserRemoveFromList(MXUserHeader *header);

typedef struct MXUserHisto MXUserHisto;

MXUserHisto *MXUserHistoSetUp(uint32 maxBins,
                              uint32 binWidth);

void MXUserHistoTearDown(MXUserHisto *histo);

void MXUserHistoSample(MXUserHisto *histo,
                       uint64 value);

void MXUserHistoDump(unsigned epoch,
                     const char *className,
                     MXUserHeader *header,
                     MXUserHisto *histo);

void MXUserAcquisitionStatsSetUp(MXUserAcquisitionStats *stats);

void MXUserAcquisitionSample(MXUserAcquisitionStats *stats,
                           Bool wasContended,
                           uint64 timeToAcquire);

void MXUserDumpAcquisitionStats(unsigned epoch,
                                const char *className,
                                MXUserHeader *header,
                                MXUserAcquisitionStats *stats);
void
MXUserBasicStatsSetUp(MXUserBasicStats *stats);

void
MXUserBasicStatsSample(MXUserBasicStats *stats,
                       uint64 value);
 
void MXUserDumpBasicStats(unsigned epoch,
                         const char *className,
                         MXUserHeader *header,
                         MXUserBasicStats *stats);

void MXUserKitchen(MXUserAcquisitionStats *stats,
                   double *contentionRatio,
                   Bool *isHot,
                   Bool *doLog);

void MXUserForceHisto(Atomic_Ptr *histoPtr,
                      uint32 maxBins,
                      uint32 binWidth);
#endif

#endif
