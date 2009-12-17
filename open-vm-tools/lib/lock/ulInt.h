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
#include "vthread.h"

/* XXX hack until thread/nothread IDs are rationalized */
void MXUserIDHack(void);
EXTERN VThreadID (*MXUserThreadCurID)(void);

/*
 * A portable recursive lock.
 */

#define MXUSER_MAX_REC_DEPTH 16

typedef struct {
#if defined(_WIN32)
   CRITICAL_SECTION lockObject;    // Native lock object
#else
   pthread_mutex_t  lockObject;    // Native lock object
#endif

   int              lockCount;     // Acquisition count
   MXThreadID       lockOwner;     // Native thread ID

#if defined(VMX86_DEBUG)
   VThreadID        lockVThreadID; // VThreadID, when available
   const void      *lockCaller;    // return address of acquisition routine
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
MXRecLockObjectInit(CRITICAL_SECTION *lockObject)  // IN/OUT:
{
   /* http://msdn.microsoft.com/en-us/library/ms682530(VS.85).aspx */
   /* magic number - allocate resources immediately; spin 0x400 times */
   return InitializeCriticalSectionAndSpinCount(lockObject, 0x80000400) != 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   DeleteCriticalSection(&lock->lockObject);
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return lock->lockOwner == GetCurrentThreadId();
}


static INLINE void
MXRecLockAcquire(MXRecLock *lock,        // IN/OUT:
                 const void *location)   // IN:
{
   EnterCriticalSection(&lock->lockObject);

   ASSERT((lock->lockCount >= 0) && (lock->lockCount < MXUSER_MAX_REC_DEPTH));

   if (lock->lockCount == 0) {
      ASSERT(lock->lockOwner == MXUSER_INVALID_OWNER);
      lock->lockOwner = GetCurrentThreadId();

#if defined(VMX86_DEBUG)
      lock->lockCaller = location;
      lock->lockVThreadID = (*MXUserThreadCurID)();
#endif
   }

   lock->lockCount++;
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,        // IN/OUT:
                    const void *location)   // IN:
{
   Bool success;

   success = TryEnterCriticalSection(&lock->lockObject);

   if (success) {
      ASSERT((lock->lockCount >= 0) &&
             (lock->lockCount < MXUSER_MAX_REC_DEPTH));

      if (lock->lockCount == 0) {
         ASSERT(lock->lockOwner == MXUSER_INVALID_OWNER);
         lock->lockOwner = GetCurrentThreadId();

#if defined(VMX86_DEBUG)
         lock->lockCaller = location;
         lock->lockVThreadID = (*MXUserThreadCurID)();
#endif
      }

      lock->lockCount++;
   }

   return success;
}


static INLINE void
MXRecLockRelease(MXRecLock *lock)  // IN/OUT:
{
   ASSERT((lock->lockCount > 0) && (lock->lockCount < MXUSER_MAX_REC_DEPTH));

   lock->lockCount--;

   if (lock->lockCount == 0) {
      lock->lockOwner = MXUSER_INVALID_OWNER;

#if defined(VMX86_DEBUG)
      lock->lockCaller = NULL;
      lock->lockVThreadID = VTHREAD_INVALID_ID;
#endif
   }

   LeaveCriticalSection(&lock->lockObject);
}
#else
static INLINE Bool
MXRecLockObjectInit(pthread_mutex_t *lockObject)  // IN/OUT:
{
   return pthread_mutex_init(lockObject, NULL) == 0;
}


static INLINE void
MXRecLockDestroy(MXRecLock *lock)  // IN/OUT:
{
   int err;

   err = pthread_mutex_destroy(&lock->lockObject);

   if (vmx86_debug && (err != 0)) {
      Panic("%s: pthread_mutex_destroy returned %d\n", __FUNCTION__, err);
   }
}


static INLINE Bool
MXRecLockIsOwner(const MXRecLock *lock)  // IN:
{
   return pthread_equal(lock->lockOwner, pthread_self());
}


static INLINE void
MXRecLockAcquire(MXRecLock *lock,  // IN/OUT:
                 void *location)   // IN:
{
   if ((lock->lockCount != 0) &&
        pthread_equal(lock->lockOwner, pthread_self())) {
      ASSERT((lock->lockCount > 0) &&
             (lock->lockCount < MXUSER_MAX_REC_DEPTH));

      lock->lockCount++;
   } else {
      int err;

      err = pthread_mutex_lock(&lock->lockObject);

      if (vmx86_debug && (err != 0)) {
         Panic("%s: pthread_mutex_lock returned %d\n", __FUNCTION__, err);
      }

      ASSERT(lock->lockCount == 0);
      ASSERT(lock->lockVThreadID == VTHREAD_INVALID_ID);

      lock->lockOwner = pthread_self();
      lock->lockCount = 1;

#if defined(VMX86_DEBUG)
      lock->lockCaller = location;
      lock->lockVThreadID = (*MXUserThreadCurID)();
#endif
   }
}


static INLINE Bool
MXRecLockTryAcquire(MXRecLock *lock,  // IN/OUT:
                    void *location)   // IN:
{
   int err;
   Bool acquired;

   err = pthread_mutex_trylock(&lock->lockObject);

   if (err == 0) {
      ASSERT((lock->lockCount >= 0) &&
             (lock->lockCount < MXUSER_MAX_REC_DEPTH));

      if (lock->lockCount == 0) {
         ASSERT(lock->lockVThreadID == VTHREAD_INVALID_ID);
         lock->lockOwner = pthread_self();

#if defined(VMX86_DEBUG)
         lock->lockCaller = location;
         lock->lockVThreadID = (*MXUserThreadCurID)();
#endif
      }

      lock->lockCount++;

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
   ASSERT((lock->lockCount > 0) && (lock->lockCount < MXUSER_MAX_REC_DEPTH));

   lock->lockCount--;

   if (lock->lockCount == 0) {
      int err;

      /* a hack but it works portably */
      memset((void *) &lock->lockOwner, 0xFF, sizeof(lock->lockOwner));

#if defined(VMX86_DEBUG)
      lock->lockCaller = NULL;
      lock->lockVThreadID = VTHREAD_INVALID_ID;
#endif

      err = pthread_mutex_unlock(&lock->lockObject);

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
   if (!MXRecLockObjectInit(&lock->lockObject)) {
      return FALSE;
   }

#if defined(_WIN32)
   lock->lockOwner = MXUSER_INVALID_OWNER;
#else
   /* a hack but it works portably */
   memset((void *) &lock->lockOwner, 0xFF, sizeof(lock->lockOwner));
#endif

   lock->lockCount = 0;

#if defined(VMX86_DEBUG)
   lock->lockVThreadID = VTHREAD_INVALID_ID;
   lock->lockCaller = NULL;
#endif

   return TRUE;
}


static INLINE uint32
MXRecLockCount(const MXRecLock *lock)  // IN:
{
   return lock->lockCount;
}


/*
 * MXUser lock header - all MXUser locks start with this
 */

#define USERLOCK_SIGNATURE 0x75677976 // 'LOCK' in memory

typedef struct MXUserHeader {
   uint32         lockSignature;
   MX_Rank        lockRank;
   const char    *lockName;
   void         (*lockDumper)(struct MXUserHeader *);
   /* THIS SPACE FOR RENT (STATISTICS POINTER) */
} MXUserHeader;

/*
 * The internal lock types
 */

struct MXUserExclLock
{
   MXUserHeader  lockHeader;

   MXRecLock     lockRecursive;
};

struct MXUserRecLock
{
   MXUserHeader  lockHeader;

   MXRecLock     lockRecursive;

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

   void        *lockVmm;
};

struct MXUserRWLock
{
   MXUserHeader      lockHeader;

#if defined(_WIN32)
   MXRecLock         lockRecursive;
   SRWLOCK           lockReadWrite;
#else
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   pthread_rwlock_t  lockReadWrite;
#else
   MXRecLock         lockRecursive;
#endif
#endif

   uint8             lockTaken[VTHREAD_MAX_THREADS];
};

/*
 * Internal functions
 */

#define RW_UNLOCKED          0
#define RW_LOCKED_FOR_READ   1
#define RW_LOCKED_FOR_WRITE  2

Bool MXUserIsAllUnlocked(const MXUserRWLock *lock);

void MXUserDumpAndPanic(MXUserHeader *header,
                        const char *fmt,
                        ...);
#endif
