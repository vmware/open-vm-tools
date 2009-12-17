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

#include <errno.h>

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateRWLock --
 *
 *      Create a read-write lock.
 *
 * Results:
 *      A pointer to a read-write lock.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserRWLock *
MXUser_CreateRWLock(const char *userName,  // IN:
                    MX_Rank rank)          // IN:
{
   char *properName;
   MXUserRWLock *lock;

   ASSERT(rank == RANK_UNRANKED);  // NOT FOR LONG

   lock = Util_SafeCalloc(1, sizeof(*lock));

   if (userName == NULL) {
#if defined(PTHREAD_RWLOCK_INITIALIZER)
      properName = Str_SafeAsprintf(NULL, "RW-%p", GetReturnAddress());
#else
      /* emulated */
      properName = Str_SafeAsprintf(NULL, "RWemul-%p", GetReturnAddress());
#endif
   } else {
      properName = Util_SafeStrdup(userName);
   }

#if defined(PTHREAD_RWLOCK_INITIALIZER)
   if (pthread_rwlock_init(&lock->lockObject, NULL) != 0) {
      free((void *) properName);
      free(lock);
      lock = NULL;
   }
#endif

   /* Establish standard header. This may get used for real too. */
   if (!MXRecLockInit(&lock->lockRecursive, properName, rank)) {
      free((void *) properName);
      free(lock);
      lock = NULL;
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireForRead --
 *
 *      Acquire a read-write lock for read-shared access.
 *
 * Results:
 *      The lock is acquired.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_AcquireForRead(MXUserRWLock *lock)  // IN/OUT:
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   int err;
#endif

   VThreadID self = VThread_CurID();

   ASSERT(MXRecLockSignature(&lock->lockRecursive) == USERLOCK_SIGNATURE);

#if defined(PTHREAD_RWLOCK_INITIALIZER)
   err = pthread_rwlock_rdlock(&lock->lockObject);

   if (err == 0) {
      if (lock->lockTaken[self] == RW_LOCKED_FOR_READ) {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: AcquireForRead after AcquireForRead",
                            __FUNCTION__);
      }
   } else {
      MXUserDumpAndPanic(&lock->lockRecursive, "%s: %s",
                         (err == EDEADLK) ? "Deadlock detected (%d)" :
                                            "Internal error (%d)",
                         __FUNCTION__, err);
   }
#else
   MXRecLockAcquire(&lock->lockRecursive, MXGetThreadID(), GetReturnAddress());

   if (lock->lockTaken[self] != RW_UNLOCKED) {
      if (lock->lockTaken[self] == RW_LOCKED_FOR_READ) {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: AcquireForRead after AcquireForRead"
                            __FUNCTION__, self);
      } else {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: AcquireForRead after AcquireForWrite"
                            __FUNCTION__, self);
      }
   }

   ASSERT(MXUserIsAllUnlocked(lock));
#endif

   lock->lockTaken[self] = RW_LOCKED_FOR_READ;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireForWrite --
 *
 *      Acquire a read-write lock for write-exclusive access.
 *
 * Results:
 *      The lock is acquired.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_AcquireForWrite(MXUserRWLock *lock)  // IN/OUT:
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   int err;
#endif

   VThreadID self = VThread_CurID();

   ASSERT(MXRecLockSignature(&lock->lockRecursive) == USERLOCK_SIGNATURE);

#if defined(PTHREAD_RWLOCK_INITIALIZER)
   err = pthread_rwlock_wrlock(&lock->lockObject);

   if (err != 0) {
      MXUserDumpAndPanic(&lock->lockRecursive, "%s: %s",
                         (err == EDEADLK) ? "Deadlock detected (%d)" :
                                            "Internal error (%d)",
                         __FUNCTION__, err);
   }
#else
   MXRecLockAcquire(&lock->lockRecursive, MXGetThreadID(), GetReturnAddress());

   if (lock->lockTaken[self] != RW_UNLOCKED) {
      if (lock->lockTaken[self] == RW_LOCKED_FOR_READ) {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: AcquireForRead after AcquireForWrite"
                            __FUNCTION__);
      } else {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: AcquireForWrite after AcquireForWrite"
                            __FUNCTION__);
      }
   }
#endif

   ASSERT(MXUserIsAllUnlocked(lock));

   lock->lockTaken[self] = RW_LOCKED_FOR_WRITE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ReleaseRWLock --
 *
 *      A read-write lock is released (unlocked).
 *
 * Results:
 *      The lock is released (unlocked).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_ReleaseRWLock(MXUserRWLock *lock)  // IN/OUT:
{
   VThreadID self = VThread_CurID();
   uint8 myState = lock->lockTaken[self];

   ASSERT(MXRecLockSignature(&lock->lockRecursive) == USERLOCK_SIGNATURE);

   if (myState == RW_UNLOCKED) {
      MXUserDumpAndPanic(&lock->lockRecursive,
                         "%s: Release of read-lock not by owner (%d)",
                         __FUNCTION__, self);
   }

   lock->lockTaken[self] = RW_UNLOCKED;

#if defined(PTHREAD_RWLOCK_INITIALIZER)
   if (vmx86_debug && (myState == RW_LOCKED_FOR_WRITE)) {
      ASSERT(MXUserIsAllUnlocked(lock));
   }

   pthread_rwlock_unlock(&lock->lockObject);
#else
   ASSERT(MXUserIsAllUnlocked(lock));
   MXRecLockRelease(&lock->lockRecursive);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyRWLock --
 *
 *      Destroy a read-write lock.
 *
 * Results:
 *      The lock is destroyed. Don't try to use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyRWLock(MXUserRWLock *lock)  // IN:
{
   if (lock != NULL) {
      ASSERT(MXRecLockSignature(&lock->lockRecursive) == USERLOCK_SIGNATURE);

      if (!MXUserIsAllUnlocked(lock)) {
         MXUserDumpAndPanic(&lock->lockRecursive,
                            "%s: Destroy on read-lock while still acquired",
                            __FUNCTION__);
      }

#if defined(PTHREAD_RWLOCK_INITIALIZER)
      pthread_rwlock_destroy(&lock->lockObject);
#endif

      MXRecLockDestroy(&lock->lockRecursive);
      free(lock);
   }
}
