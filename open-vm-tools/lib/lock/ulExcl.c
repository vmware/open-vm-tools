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
#include "hostinfo.h"
#include "ulInt.h"
#include "vm_atomic.h"

typedef struct
{
   MXUserAcquisitionStats  acquisitionStats;
   Atomic_Ptr              acquisitionHisto;

   void                   *holder;
   VmTimeType              holdStart;
   MXUserBasicStats        heldStats;
   Atomic_Ptr              heldHisto;
} MXUserStats;

struct MXUserExclLock
{
   MXUserHeader  header;
   MXRecLock     recursiveLock;
   Atomic_Ptr    statsMem;
};


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserStatsActionExcl --
 *
 *      Perform the statistics action for the specified lock.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserStatsActionExcl(MXUserHeader *header)  // IN:
{
   MXUserExclLock *lock = (MXUserExclLock *) header;
   MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

   if (stats) {
      Bool isHot;
      Bool doLog;
      double contentionRatio;

      /*
       * Dump the statistics for the specified lock.
       */

      MXUserDumpAcquisitionStats(&stats->acquisitionStats, header);

      if (Atomic_ReadPtr(&stats->acquisitionHisto) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&stats->acquisitionHisto), header);
      }

      MXUserDumpBasicStats(&stats->heldStats, header);

      if (Atomic_ReadPtr(&stats->heldHisto) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&stats->heldHisto), header);
      }

      /*
       * Has the lock gone "hot"? If so, implement the hot actions.
       */

      MXUserKitchen(&stats->acquisitionStats, &contentionRatio, &isHot,
                    &doLog);

      if (isHot) {
         MXUserForceHisto(&stats->acquisitionHisto,
                          MXUSER_STAT_CLASS_ACQUISITION,
                          MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                          MXUSER_DEFAULT_HISTO_DECADES);
         MXUserForceHisto(&stats->heldHisto,
                          MXUSER_STAT_CLASS_HELD,
                          MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                          MXUSER_DEFAULT_HISTO_DECADES);

         if (doLog) {
            Log("HOT LOCK (%s); contention ratio %f\n",
                lock->header.name, contentionRatio);
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpExclLock --
 *
 *      Dump an exclusive lock.
 *
 * Results:
 *      A dump.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserDumpExclLock(MXUserHeader *header)  // IN:
{
   MXUserExclLock *lock = (MXUserExclLock *) header;
   MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

   Warning("%s: Exclusive lock @ 0x%p\n", __FUNCTION__, lock);

   Warning("\tsignature 0x%X\n", lock->header.signature);
   Warning("\tname %s\n", lock->header.name);
   Warning("\trank 0x%X\n", lock->header.rank);
   Warning("\tserial number %u\n", lock->header.serialNumber);

   Warning("\tcount %u\n", lock->recursiveLock.referenceCount);

   Warning("\taddress of owner data 0x%p\n",
           &lock->recursiveLock.nativeThreadID);

   if (stats && (stats->holder != NULL)) {
      Warning("\tholder 0x%p\n", stats->holder);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ControlExclLock --
 *
 *      Perform the specified command on the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      Depends on the command, no?
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_ControlExclLock(MXUserExclLock *lock,  // IN/OUT:
                       uint32 command,        // IN:
                       ...)                   // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   switch (command) {
   case MXUSER_CONTROL_ACQUISITION_HISTO: {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats) {
         va_list a;
         uint64 minValue;
         uint32 decades;

         va_start(a, command);
         minValue = va_arg(a, uint64);
         decades = va_arg(a, uint32);
         va_end(a);

         MXUserForceHisto(&stats->acquisitionHisto,
                          MXUSER_STAT_CLASS_ACQUISITION, minValue, decades);

         result = TRUE;
      } else {
         result = FALSE;
      }

      break;
   }

   case MXUSER_CONTROL_HELD_HISTO: {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats) {
         va_list a;
         uint32 minValue;
         uint32 decades;

         va_start(a, command);
         minValue = va_arg(a, uint64);
         decades = va_arg(a, uint32);
         va_end(a);

         MXUserForceHisto(&stats->heldHisto, MXUSER_STAT_CLASS_HELD,
                          minValue, decades);

         result = TRUE;
      } else {
         result = FALSE;
      }

      break;
   }

   case MXUSER_CONTROL_ENABLE_STATS: {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (LIKELY(stats == NULL)) {
         MXUserStats *before;

         stats = Util_SafeCalloc(1, sizeof(*stats));

         MXUserAcquisitionStatsSetUp(&stats->acquisitionStats);
         MXUserBasicStatsSetUp(&stats->heldStats, MXUSER_STAT_CLASS_HELD);

         before = (MXUserStats *) Atomic_ReadIfEqualWritePtr(&lock->statsMem,
                                                             NULL,
                                                             (void *) stats);

         if (before) {
            free(stats);
         }

         lock->header.statsFunc = MXUserStatsActionExcl;
      }

      result = TRUE;
      break;
   }

   default:
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateExclLock --
 *
 *      Create an exclusive lock.
 *
 * Results:
 *      NULL  Creation failed
 *      !NULL Creation succeeded
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserExclLock *
MXUser_CreateExclLock(const char *userName,  // IN:
                      MX_Rank rank)          // IN:
{
   char *properName;
   MXUserExclLock *lock;

   lock = Util_SafeCalloc(1, sizeof(*lock));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "X-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   if (!MXRecLockInit(&lock->recursiveLock)) {
      free(properName);
      free(lock);

      return NULL;
   }

   lock->header.signature = MXUserGetSignature(MXUSER_TYPE_EXCL);
   lock->header.name = properName;
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = MXUserDumpExclLock;

   if (MXUserStatsEnabled()) {
      MXUser_ControlExclLock(lock, MXUSER_CONTROL_ENABLE_STATS);
   } else {
      lock->header.statsFunc = NULL;
      Atomic_WritePtr(&lock->statsMem, NULL);
   }

   MXUserAddToList(&lock->header);

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyExclLock --
 *
 *      Destroy an exclusive lock.
 *
 * Results:
 *      Lock is destroyed. Don't use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyExclLock(MXUserExclLock *lock)  // IN:
{
   if (lock != NULL) {
      MXUserStats *stats;

      MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

      if (MXRecLockCount(&lock->recursiveLock) > 0) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Destroy of an acquired exclusive lock\n",
                            __FUNCTION__);
      }

      lock->header.signature = 0;  // just in case...

      MXRecLockDestroy(&lock->recursiveLock);

      MXUserRemoveFromList(&lock->header);

      stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats) {
         MXUserAcquisitionStatsTearDown(&stats->acquisitionStats);
         MXUserBasicStatsTearDown(&stats->heldStats);
         MXUserHistoTearDown(Atomic_ReadPtr(&stats->acquisitionHisto));
         MXUserHistoTearDown(Atomic_ReadPtr(&stats->heldHisto));

         free(stats);
      }

      free(lock->header.name);
      lock->header.name = NULL;
      free(lock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireExclLock --
 *
 *      An acquisition is made (lock is taken) on the specified exclusive lock.
 *
 * Results:
 *      The lock is acquired (locked).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_AcquireExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   MXUserStats *stats;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

   MXUserAcquisitionTracking(&lock->header, TRUE);

   if (stats) {
      VmTimeType begin = Hostinfo_SystemTimerNS();
      VmTimeType value;
      Bool contended;
      MXUserHisto *histo;

      contended = MXRecLockAcquire(&lock->recursiveLock);

      value = Hostinfo_SystemTimerNS() - begin;

      MXUserAcquisitionSample(&stats->acquisitionStats, TRUE, contended,
                              value);

      stats->holder = GetReturnAddress();

      histo = Atomic_ReadPtr(&stats->acquisitionHisto);

      if (UNLIKELY(histo != NULL)) {
         MXUserHistoSample(histo, value, stats->holder);
      }
   } else {
      MXRecLockAcquire(&lock->recursiveLock);
   }

   if (MXRecLockCount(&lock->recursiveLock) > 1) {
      MXUserDumpAndPanic(&lock->header,
                         "%s: Acquire on an acquired exclusive lock\n",
                         __FUNCTION__);
   }

   if (stats) {
      stats->holdStart = Hostinfo_SystemTimerNS();
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ReleaseExclLock --
 *
 *      Release (unlock) an exclusive lock.
 *
 * Results:
 *      The lock is released.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_ReleaseExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   MXUserStats *stats;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

   if (stats) {
      VmTimeType value = Hostinfo_SystemTimerNS() - stats->holdStart;
      MXUserHisto *histo;

      MXUserBasicStatsSample(&stats->heldStats, value);

      histo = Atomic_ReadPtr(&stats->heldHisto);

      if (UNLIKELY(histo != NULL)) {
         MXUserHistoSample(histo, value, stats->holder);
         stats->holder = NULL;
      }
   }

   if (!MXRecLockIsOwner(&lock->recursiveLock)) {
      uint32 lockCount = MXRecLockCount(&lock->recursiveLock);

      MXUserDumpAndPanic(&lock->header,
                         "%s: Non-owner release of an %s exclusive lock\n",
                         __FUNCTION__,
                         lockCount == 0 ? "unacquired" : "acquired");
   }

   MXUserReleaseTracking(&lock->header);

   MXRecLockRelease(&lock->recursiveLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TryAcquireExclLock --
 *
 *      An attempt is made to conditionally acquire (lock) an exclusive lock.
 *
 * Results:
 *      TRUE    Acquired (locked)
 *      FALSE   Not acquired
 *
 * Side effects:
 *      None
 *
 * NOTE:
 *      A "TryAcquire" does not rank check should the acquisition succeed.
 *      This duplicates the behavor of MX locks.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_TryAcquireExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   Bool success;
   MXUserStats *stats;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (MXUserTryAcquireFail(lock->header.name)) {
      return FALSE;
   }

   success = MXRecLockTryAcquire(&lock->recursiveLock);

   if (success) {
      MXUserAcquisitionTracking(&lock->header, FALSE);

      if (MXRecLockCount(&lock->recursiveLock) > 1) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Acquire on an acquired exclusive lock\n",
                            __FUNCTION__);
      }
   }

   stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

   if (stats) {
      MXUserAcquisitionSample(&stats->acquisitionStats, success, !success,
                              0ULL);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IsCurThreadHoldingExclLock --
 *
 *      Is an exclusive lock held by the calling thread?
 *
 * Results:
 *      TRUE    Yes
 *      FALSE   No
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_IsCurThreadHoldingExclLock(MXUserExclLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   return MXRecLockIsOwner(&lock->recursiveLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonExclLock --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      exclusive lock. This is useful for modules that need to protect
 *      something with a lock but don't have an existing Init() entry point
 *      where a lock can be created.
 *
 * Results:
 *      A pointer to the requested lock.
 *
 * Side effects:
 *      Generally the lock's resources are intentionally leaked (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserExclLock *
MXUser_CreateSingletonExclLock(Atomic_Ptr *lockStorage,  // IN/OUT:
                               const char *name,         // IN:
                               MX_Rank rank)             // IN:
{
   MXUserExclLock *lock;

   ASSERT(lockStorage);

   lock = (MXUserExclLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserExclLock *newLock = MXUser_CreateExclLock(name, rank);

      lock = (MXUserExclLock *) Atomic_ReadIfEqualWritePtr(lockStorage, NULL,
                                                           (void *) newLock);

      if (lock) {
         MXUser_DestroyExclLock(newLock);
      } else {
         lock = (MXUserExclLock *) Atomic_ReadPtr(lockStorage);
      }
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateCondVarExclLock --
 *
 *      Create a condition variable for use with the specified exclusive lock.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      The created condition variable will cause a run-time error if it is
 *      used with a lock other than the one it was created for.
 *
 *-----------------------------------------------------------------------------
 */

MXUserCondVar *
MXUser_CreateCondVarExclLock(MXUserExclLock *lock)
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   return MXUserCreateCondVar(&lock->header, &lock->recursiveLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_WaitCondVarExclLock --
 *
 *      Block (sleep) on the specified condition variable. The specified lock
 *      is released upon blocking and is reacquired before returning from this
 *      function.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      It is possible to return from this routine without the condtion
 *      variable having been signalled (spurious wake up); code accordingly!
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_WaitCondVarExclLock(MXUserExclLock *lock,    // IN:
                           MXUserCondVar *condVar)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar,
                     MXUSER_WAIT_INFINITE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TimedWaitCondVarExclLock --
 *
 *      Block (sleep) on the specified condition variable for no longer than
 *      the specified amount of time. The specified lock is released upon
 *      blocking and is reacquired before returning from this function.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      It is possible to return from this routine without the condtion
 *      variable having been signalled (spurious wake up); code accordingly!
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_TimedWaitCondVarExclLock(MXUserExclLock *lock,    // IN:
                                MXUserCondVar *condVar,  // IN:
                                uint32 msecWait)         // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar, msecWait);
}
