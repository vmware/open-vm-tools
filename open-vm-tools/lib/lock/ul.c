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

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"
#include "ulIntShared.h"
#include "hashTable.h"


static Bool mxInPanic = FALSE;  // track when involved in a panic

Bool (*MXUserTryAcquireForceFail)() = NULL;

static MX_Rank (*MXUserMxCheckRank)(void) = NULL;
static void (*MXUserMxLockLister)(void) = NULL;
void (*MXUserMX_LockRec)(struct MX_MutexRec *lock) = NULL;
void (*MXUserMX_UnlockRec)(struct MX_MutexRec *lock) = NULL;
Bool (*MXUserMX_TryLockRec)(struct MX_MutexRec *lock) = NULL;
Bool (*MXUserMX_IsLockedByCurThreadRec)(const struct MX_MutexRec *lock) = NULL;


#if defined(MXUSER_DEBUG)
#define MXUSER_MAX_LOCKS_PER_THREAD (2 * MXUSER_MAX_REC_DEPTH)

typedef struct {
   uint32         locksHeld;
   MXUserHeader  *lockArray[MXUSER_MAX_LOCKS_PER_THREAD];
} MXUserPerThread;

static Atomic_Ptr hashTableMem;


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserGetPerThread --
 *
 *      Return a pointer to the per thread data for the specified thread.
 *
 *      Memory is allocated for the specified thread as necessary. This memory
 *      is never released since it it is highly likely a thread will use a
 *      lock and need to record data in the perThread.
 *
 * Results:
 *      NULL   mayAlloc was FALSE and the thread doesn't have a perThread (yet)
 *     !NULL   the perThread of the specified thread
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

static MXUserPerThread *
MXUserGetPerThread(void *tid,      // IN: native thread ID
                   Bool mayAlloc)  // IN: alloc perThread if not present?
{
   HashTable *hash;
   MXUserPerThread *perThread;

   hash = HashTable_AllocOnce(&hashTableMem, 1024,
                              HASH_INT_KEY | HASH_FLAG_ATOMIC, NULL);

   perThread = NULL;

   if (!HashTable_Lookup(hash, tid, (void **) &perThread)) {
      /* No entry for this tid was found, allocate one? */

      if (mayAlloc) {
         MXUserPerThread *newEntry = Util_SafeCalloc(1,
                                                     sizeof(MXUserPerThread));

         /*
          * Attempt to (racey) insert a perThread on behalf of the specified
          * thread. If yet another thread takes care of this first, clean up
          * the mess.
          */

         perThread = HashTable_LookupOrInsert(hash, tid, newEntry);
         ASSERT(perThread);

         if (perThread != newEntry) {
            free(newEntry);
         }
      } else {
         perThread = NULL;
      }
   }

   return perThread;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserListLocks
 *
 *      Allow a caller to list, via warnings, the list of locks the caller
 *      has acquired. Ensure that no memory for lock tracking is allocated
 *      if no locks have been taken.
 *
 * Results:
 *      The list is printed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserListLocks(void)
{
   MXUserPerThread *perThread = MXUserGetPerThread(MXUserGetThreadID(),
                                                   FALSE);

   if (perThread != NULL) {
      uint32 i;

      for (i = 0; i < perThread->locksHeld; i++) {
         MXUserHeader *hdr = perThread->lockArray[i];

         Warning("\tMXUser lock %s (@%p) rank 0x%x\n", hdr->name, hdr,
                 hdr->rank);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IsCurThreadHoldingLocks --
 *
 *      Are any MXUser locks held by the calling thread?
 *
 * Results:
 *      TRUE   Yes
 *      FALSE  No
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_IsCurThreadHoldingLocks(void)
{
   MXUserPerThread *perThread = MXUserGetPerThread(MXUserGetThreadID(),
                                                   FALSE);

   return (perThread == NULL) ? FALSE : (perThread->locksHeld != 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserThreadRank --
 *
 *      Return the highest rank held by the specified thread via MXUser locks.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      Can optionally determine if a lock has been locked before.
 *
 *-----------------------------------------------------------------------------
 */

MX_Rank
MXUserThreadRank(MXUserPerThread *perThread,  // IN:
                 MXUserHeader *header,        // IN:
                 Bool *firstUse)              // OUT:
{
   uint32 i;
   Bool foundOnce = TRUE;
   MX_Rank maxRank = RANK_UNRANKED;

   ASSERT(perThread);

   /*
    * Determine the maximum rank held. Note if the lock being acquired
    * was previously entered into the tracking system.
    */

   for (i = 0; i < perThread->locksHeld; i++) {
      MXUserHeader *chkHdr = perThread->lockArray[i];

      maxRank = MAX(chkHdr->rank, maxRank);

      if (chkHdr == header) {
         foundOnce = FALSE;
      }
   }

   if (firstUse) {
      *firstUse = foundOnce;
   }

   return maxRank;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCurrentRank --
 *
 *      Return the highest rank held by the current thread via MXUser locks.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MX_Rank
MXUserCurrentRank(void)
{
   MX_Rank maxRank;
   MXUserPerThread *perThread; 

   perThread = MXUserGetPerThread(MXUserGetThreadID(), FALSE);

   if (perThread == NULL) {
      maxRank = RANK_UNRANKED;
   } else {
      maxRank = MXUserThreadRank(perThread, NULL, NULL);
   }

   return maxRank;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserAcquisitionTracking --
 *
 *      Perform the appropriate tracking for lock acquisition.
 *
 * Results:
 *      Panic when a rank violation is detected (checkRank is TRUE).
 *      Add a lock instance to perThread lock list.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserAcquisitionTracking(MXUserHeader *header,  // IN:
                          Bool checkRank)        // IN:
{
   MXUserPerThread *perThread = MXUserGetPerThread(MXUserGetThreadID(), TRUE);

   ASSERT_NOT_IMPLEMENTED(perThread->locksHeld < MXUSER_MAX_LOCKS_PER_THREAD);

   /*
    * Rank checking anyone?
    *
    * Rank checking is abandoned once we're in a panic situation. This will
    * improve the chances of obtaining a good log and/or coredump.
    */

   if (checkRank && (header->rank != RANK_UNRANKED) && !MXUser_InPanic()) {
      MX_Rank maxRank;
      Bool firstInstance = TRUE;

      /*
       * Determine the highest rank held by the calling thread. Check for
       * MX locks if they are present.
       */

      maxRank = MXUserThreadRank(perThread, header, &firstInstance);

      if (MXUserMxCheckRank) {
         maxRank = MAX(maxRank, (*MXUserMxCheckRank)());
      }

      /*
       * Perform rank checking when a lock is entered into the tracking
       * system for the first time. This works out well because:
       *
       * Recursive locks are rank checked only upon their first acquisition...
       * just like MX locks.
       * 
       * Exclusive locks will have a second entry added into the tracking
       * system but will immediately panic due to the run time checking - no
       * (real) harm done.
       */

      if (firstInstance && (header->rank <= maxRank)) {
         Warning("%s: lock rank violation by thread %s\n", __FUNCTION__,
                 VThread_CurName());
         Warning("%s: locks held:\n", __FUNCTION__);

         if (MXUserMxLockLister) {
            (*MXUserMxLockLister)();
         }

         MXUserListLocks();

         MXUserDumpAndPanic(header, "%s: rank violation\n", __FUNCTION__);
      }
   }

   /* Add lock instance to the calling threads perThread information */
   perThread->lockArray[perThread->locksHeld++] = header;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserReleaseTracking --
 *
 *      Perform the appropriate tracking for lock release.
 *
 * Results:
 *      A panic.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserReleaseTracking(MXUserHeader *header)  // IN: lock, via its header
{
   uint32 i;
   uint32 lastEntry;
   void *tid = MXUserGetThreadID();
   MXUserPerThread *perThread = MXUserGetPerThread(tid, FALSE);

   /* MXUserAcquisitionTracking should have already created a perThread */
   if (UNLIKELY(perThread == NULL)) {
      MXUserDumpAndPanic(header, "%s: perThread not found! (thread %p)\n",
                         __FUNCTION__, tid);
   }

   /* Search the perThread for the argument lock */
   for (i = 0; i < perThread->locksHeld; i++) {
      if (perThread->lockArray[i] == header) {
         break;
      }
   }

   /* The argument lock had better be in the perThread */
   if (UNLIKELY(i >= perThread->locksHeld)) {
      MXUserDumpAndPanic(header, "%s: lock not found! (thread %p; count %u)\n",
                         __FUNCTION__, tid, perThread->locksHeld);
   }

   /* Remove the argument lock from the perThread */
   lastEntry = perThread->locksHeld - 1;

   if (i < lastEntry) {
      perThread->lockArray[i] = perThread->lockArray[lastEntry];
   }

   perThread->lockArray[lastEntry] = NULL;  // tidy up memory
   perThread->locksHeld--;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TryAcquireFailureControl --
 *
 *      Should a TryAcquire operation fail, no matter "what", sometimes?
 *
 *      Failures occur statistically in debug builds to force our code down
 *      all of its paths.
 *
 * Results:
 *      Unknown
 *
 * Side effects:
 *      Always entertaining...
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_TryAcquireFailureControl(Bool (*func)(const char *name))  // IN:
{
   MXUserTryAcquireForceFail = func;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserInternalSingleton --
 *
 *      A "singleton" function for the MXUser internal recursive lock.
 *
 *      Internal MXUser recursive locks have no statistics gathering or
 *      tracking abilities. They need to used with care and rarely.
 *
 * Results:
 *      NULL    Failure
 *      !NULL   A pointer to an initialized MXRecLock
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

MXRecLock *
MXUserInternalSingleton(Atomic_Ptr *storage)  // IN:
{
   MXRecLock *lock = (MXRecLock *) Atomic_ReadPtr(storage);

   if (UNLIKELY(lock == NULL)) {
      MXRecLock *newLock = Util_SafeMalloc(sizeof(MXRecLock));

      if (MXRecLockInit(newLock)) {
         lock = (MXRecLock *) Atomic_ReadIfEqualWritePtr(storage, NULL,
                                                         (void *) newLock);

         if (lock) {
            MXRecLockDestroy(newLock);
            free(newLock);
         } else {
            lock = Atomic_ReadPtr(storage);
         }
      } else {
         free(newLock);
         lock = Atomic_ReadPtr(storage);  // maybe another thread succeeded
      }
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpAndPanic --
 *
 *      Dump a lock, print a message and die
 *
 * Results:
 *      A panic.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserDumpAndPanic(MXUserHeader *header,  // IN:
                   const char *fmt,       // IN:
                   ...)                   // IN:
{
   char *msg;
   va_list ap;

   ASSERT((header != NULL) && (header->dumpFunc != NULL));

   (*header->dumpFunc)(header);

   va_start(ap, fmt);
   msg = Str_SafeVasprintf(NULL, fmt, ap);
   va_end(ap);

   Panic("%s", msg);
}


/*
 *---------------------------------------------------------------------
 * 
 *  MXUser_SetInPanic --
 *	Notify the locking system that a panic is occurring.
 *
 *      This is the "out of the monitor" - userland - implementation. The "in
 *      the monitor" implementation lives in mutex.c.
 *
 *  Results:
 *     Set the internal "in a panic" global variable.
 *
 *  Side effects:
 *     None
 *
 *---------------------------------------------------------------------
 */

void
MXUser_SetInPanic(void)
{
   mxInPanic = TRUE;
}


/*
 *---------------------------------------------------------------------
 * 
 *  MXUser_InPanic --
 *	Is the caller in the midst of a panic?
 *
 *      This is the "out of the monitor" - userland - implementation. The "in
 *      the monitor" implementation lives in mutex.c.
 *
 *  Results:
 *     TRUE   Yes
 *     FALSE  No
 *
 *  Side effects:
 *     None
 *
 *---------------------------------------------------------------------
 */

Bool
MXUser_InPanic(void)
{
   return mxInPanic;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserInstallMxHooks --
 *
 *      The MX facility may notify the MXUser facility that it is place and
 *      that MXUser should check with it. This function should be called from
 *      MX_Init.
 *
 * Results:
 *      As Above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserInstallMxHooks(void (*theLockListFunc)(void),
                     MX_Rank (*theRankFunc)(void),
                     void (*theLockFunc)(struct MX_MutexRec *lock),
                     void (*theUnlockFunc)(struct MX_MutexRec *lock),
                     Bool (*theTryLockFunc)(struct MX_MutexRec *lock),
                     Bool (*theIsLockedFunc)(const struct MX_MutexRec *lock))
{
   /*
    * This function can be called more than once but the second and later
    * invocations must be attempting to install the same hook functions as
    * the first invocation.
    */

   if ((MXUserMxLockLister == NULL) &&
       (MXUserMxCheckRank == NULL) &&
       (MXUserMX_LockRec == NULL) &&
       (MXUserMX_UnlockRec == NULL) &&
       (MXUserMX_TryLockRec == NULL) &&
       (MXUserMX_IsLockedByCurThreadRec == NULL)) {
      MXUserMxLockLister = theLockListFunc;
      MXUserMxCheckRank = theRankFunc;
      MXUserMX_LockRec = theLockFunc;
      MXUserMX_UnlockRec = theUnlockFunc;
      MXUserMX_TryLockRec = theTryLockFunc;
      MXUserMX_IsLockedByCurThreadRec = theIsLockedFunc;
   } else {
      ASSERT((MXUserMxLockLister == theLockListFunc) &&
             (MXUserMxCheckRank == theRankFunc) &&
             (MXUserMX_LockRec == theLockFunc) &&
             (MXUserMX_UnlockRec == theUnlockFunc) &&
             (MXUserMX_TryLockRec == theTryLockFunc) &&
             (MXUserMX_IsLockedByCurThreadRec == theIsLockedFunc)
            );
   }
}
