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

#if defined(_WIN32)
typedef DWORD MXThreadID;
#else
#include <pthread.h>
#include <errno.h>
typedef pthread_t MXThreadID;
#endif

#include "vm_basic_types.h"
#include "vthread.h"

/* XXX hack until thread/nothread IDs are rationalized */
void MXUserIDHack(void);
EXTERN VThreadID (*MXUserThreadCurID)(void);

/*
 * MXUser lock header - all MXUser locks start with this
 */

#define USERLOCK_SIGNATURE 0x75677976 // 'LOCK' in memory

typedef struct {
   const char    *lockName;
   uint32         lockSignature;
   int            lockCount;   // May be Atomic someday (read-write locks?)
   MXThreadID     lockOwner;   // Native thread ID
   MX_Rank        lockRank;
   VThreadID      lockVThreadID;

   /*
    * Statistics data
    *
    * The fancy statistics that we will keep on each lock will live here too,
    * attached via a pointer.
    */

   const void    *lockCaller;  // return address of lock acquisition routine
} MXUserHeader;

/*
 * A portable recursive lock.
 *
 * The MXUser simple and recursive locks are built on top of this.
 */

#define MXUSER_MAX_REC_DEPTH 16

typedef struct {
   MXUserHeader     lockHeader;

#if defined(_WIN32)
   CRITICAL_SECTION lockObject;
#else
   pthread_mutex_t  lockObject;
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
 * There are 7 environment specific primitives:
 *
 * MXGetThreadID       (thread identification)
 * MXRecLockIsOwner    (is lock owned by caller?)
 * MXRecLockObjectInit (initialize)
 * MXRecLockDestroy    (dispose after use)
 * MXRecLockAcquire    (lock)
 * MXRecLockTryAcquire (conditional lock)
 * MXRecLockRelease    (unlock)
 *
 * Windows has a native recursive lock, the CRITICAL_SECTION. POSIXen,
 * unfortunately, do not ensure access to such a facility. The recursive
 * attribute of pthread_mutex_t is not implemented in all environments so
 * we create a recursive implementation using an exclusive pthread_mutex_t
 * and a few lines of code (most of which we need to do anyway).
 */

#if defined(_WIN32)
static INLINE MXThreadID
MXGetThreadID(void)
{
   return GetCurrentThreadId();
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   return lock->lockHeader.lockOwner == MXGetThreadID();
}


static INLINE Bool
MXRecLockObjectInit(CRITICAL_SECTION *lockObject)  // IN/OUT:
{
   /* http://msdn.microsoft.com/en-us/library/ms682530(VS.85).aspx */
   /* magic number - allocate resources immediately; spin 0x400 times */
   return InitializeCriticalSectionAndSpinCount(lockObject, 0x80000400) != 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   DeleteCriticalSection(&lock->lockObject);
   free((void *) lock->lockHeader.lockName);
}


static INLINE void
MXRecLockAcquire(MXRecLock *lock,        // IN/OUT:
                 MXThreadID self,        // IN:
                 const void *location)   // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   EnterCriticalSection(&lock->lockObject);

   ASSERT((lock->lockHeader.lockCount >= 0) &&
          (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

   if (lock->lockHeader.lockCount == 0) {
      ASSERT(lock->lockHeader.lockVThreadID == VTHREAD_INVALID_ID);
      lock->lockHeader.lockOwner = self;
      lock->lockHeader.lockCaller = location;
      lock->lockHeader.lockVThreadID = (*MXUserThreadCurID)();

   }

   lock->lockHeader.lockCount++;
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,        // IN/OUT:
                    MXThreadID self,        // IN:
                    const void *location)   // IN:
{
   Bool success;

   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   success = TryEnterCriticalSection(&lock->lockObject);

   if (success) {
      ASSERT((lock->lockHeader.lockCount >= 0) &&
             (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

      if (lock->lockHeader.lockCount == 0) {
         ASSERT(lock->lockHeader.lockVThreadID == VTHREAD_INVALID_ID);
         lock->lockHeader.lockOwner = self;
         lock->lockHeader.lockCaller = location;
         lock->lockHeader.lockVThreadID = (*MXUserThreadCurID)();
      }

      lock->lockHeader.lockCount++;
   }

   return success;
}


static INLINE void
MXRecLockRelease(MXRecLock *lock)  // IN/OUT:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   ASSERT((lock->lockHeader.lockCount > 0) &&
          (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

   lock->lockHeader.lockCount--;

   if (lock->lockHeader.lockCount == 0) {
      lock->lockHeader.lockCaller = NULL;
      lock->lockHeader.lockVThreadID = VTHREAD_INVALID_ID;
   }

   LeaveCriticalSection(&lock->lockObject);
}
#else
static INLINE MXThreadID
MXGetThreadID(void)
{
   return pthread_self();
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   return pthread_equal(lock->lockHeader.lockOwner, pthread_self());
}


static INLINE Bool
MXRecLockObjectInit(pthread_mutex_t *lockObject)  // IN/OUT:
{
   return pthread_mutex_init(lockObject, NULL) == 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   int err;

   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   err = pthread_mutex_destroy(&lock->lockObject);

   if (vmx86_debug && (err != 0)) {
      Panic("%s: pthread_mutex_destroy returned %d\n", __FUNCTION__, err);
   }

   free((void *) lock->lockHeader.lockName);
}


static INLINE void
MXRecLockAcquire(MXRecLock *lock,  // IN/OUT:
                 MXThreadID self,  // IN:
                 void *location)   // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   if ((lock->lockHeader.lockCount != 0) &&
        pthread_equal(lock->lockHeader.lockOwner, pthread_self())) {
      ASSERT((lock->lockHeader.lockCount > 0) &&
             (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

      lock->lockHeader.lockCount++;
   } else {
      int err;

      err = pthread_mutex_lock(&lock->lockObject);

      if (vmx86_debug && (err != 0)) {
         Panic("%s: pthread_mutex_lock returned %d\n", __FUNCTION__, err);
      }

      ASSERT(lock->lockHeader.lockCount == 0);
      ASSERT(lock->lockHeader.lockVThreadID == VTHREAD_INVALID_ID);

      lock->lockHeader.lockOwner = self;
      lock->lockHeader.lockCaller = location;
      lock->lockHeader.lockCount = 1;
      lock->lockHeader.lockVThreadID = (*MXUserThreadCurID)();
   }
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,  // IN/OUT:
                    MXThreadID self,  // IN:
                    void *location)   // IN:
{
   int err;
   Bool acquired;

   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   err = pthread_mutex_trylock(&lock->lockObject);

   if (err == 0) {
      ASSERT((lock->lockHeader.lockCount >= 0) &&
             (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

      if (lock->lockHeader.lockCount == 0) {
         ASSERT(lock->lockHeader.lockVThreadID == VTHREAD_INVALID_ID);
         lock->lockHeader.lockOwner = self;
         lock->lockHeader.lockCaller = location;
         lock->lockHeader.lockVThreadID = (*MXUserThreadCurID)();
      }

      lock->lockHeader.lockCount++;

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
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   ASSERT((lock->lockHeader.lockCount > 0) &&
          (lock->lockHeader.lockCount < MXUSER_MAX_REC_DEPTH));

   lock->lockHeader.lockCount--;

   if (lock->lockHeader.lockCount == 0) {
      int err;

      lock->lockHeader.lockCaller = NULL;
      lock->lockHeader.lockVThreadID = VTHREAD_INVALID_ID;

      err = pthread_mutex_unlock(&lock->lockObject);

      if (vmx86_debug && (err != 0)) {
         Panic("%s: pthread_mutex_unlock returned %d\n", __FUNCTION__, err);
      }
   }
}
#endif


static INLINE void
MXRecLockInitHeader(MXRecLock *lock,       // IN/OUT:
                    const char *userName,  // IN:
                    MX_Rank rank)          // IN:
{
   lock->lockHeader.lockName = userName;
   lock->lockHeader.lockSignature = USERLOCK_SIGNATURE;
   lock->lockHeader.lockCount = 0;
   lock->lockHeader.lockRank = rank;
   lock->lockHeader.lockVThreadID = VTHREAD_INVALID_ID;
   lock->lockHeader.lockCaller = NULL;
}


/*
 * Initialization of portable recursive lock.
 */

static INLINE Bool
MXRecLockInit(MXRecLock *lock,       // IN/OUT:
              const char *userName,  // IN:
              MX_Rank rank)          // IN:
{
   ASSERT(rank == RANK_UNRANKED);  // NOT FOR LONG
   ASSERT(userName != NULL);

   if (!MXRecLockObjectInit(&lock->lockObject)) {
      return FALSE;
   }

   MXRecLockInitHeader(lock, userName, rank);

   return TRUE;
}


static INLINE uint32
MXRecLockCount(const MXRecLock *lock)  // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   return lock->lockHeader.lockCount;
}


static INLINE const char *
MXRecLockName(const MXRecLock *lock)  // IN:
{
   ASSERT(lock->lockHeader.lockSignature == USERLOCK_SIGNATURE);

   return lock->lockHeader.lockName;
}


static INLINE uint32
MXRecLockSignature(const MXRecLock *lock)  // IN:
{
   return lock->lockHeader.lockSignature;
}

/*
 * The internal lock types
 */

struct MXUserExclLock
{
   MXRecLock basic;
};

struct MXUserRecLock
{
   MXRecLock  basic;

   /*
    * This is the MX recursive lock override pointer. This pointer is NULL
    * for standard MXUser recursive locks. It will be not NULL when a
    * special "bind an MXUser recursive lock to an MX recursive lock"
    * function is called. The binding function will come along in the near
    * future. This will make things like Poll able to user MXUser locks.
    *
    *  NULL   use MXRecLock within this structure
    * !NULL   use pointed to MX recursive lock
    */

   void      *vmmLock;
};

struct MXUserRWLock
{
   MXRecLock         lockRecursive;  // Used as a header or maybe for real

   uint8             lockTaken[VTHREAD_MAX_THREADS];

#if defined(_WIN32)
   SRWLOCK           lockReadWrite;
#else
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   pthread_rwlock_t  lockObject;
#endif
#endif
};

/*
 * Internal functions
 */

#define RW_UNLOCKED          0
#define RW_LOCKED_FOR_READ   1
#define RW_LOCKED_FOR_WRITE  2

Bool MXUserIsAllUnlocked(const MXUserRWLock *lock);

void MXUserDumpAndPanic(MXRecLock *lock,
                        const char *fmt,
                        ...);
#endif
