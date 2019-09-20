/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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

struct MXUserExclLock
{
   MXUserHeader  header;
   MXRecLock     recursiveLock;
   Atomic_Ptr    heldStatsMem;
   Atomic_Ptr    acquireStatsMem;
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
   MXUserHeldStats *heldStats = Atomic_ReadPtr(&lock->heldStatsMem);
   MXUserAcquireStats *acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

   if (UNLIKELY(heldStats != NULL)) {
      MXUserDumpBasicStats(&heldStats->data, header);

      if (Atomic_ReadPtr(&heldStats->histo) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&heldStats->histo), header);
      }
   }

   if (LIKELY(acquireStats != NULL)) {
      Bool isHot;
      Bool doLog;
      double contentionRatio;

      /*
       * Dump the statistics for the specified lock.
       */

      MXUserDumpAcquisitionStats(&acquireStats->data, header);

      if (Atomic_ReadPtr(&acquireStats->histo) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&acquireStats->histo), header);
      }

      /*
       * Has the lock gone "hot"? If so, implement the hot actions.
       */

      MXUserKitchen(&acquireStats->data, &contentionRatio, &isHot, &doLog);

      if (UNLIKELY(isHot)) {
         MXUserForceAcquisitionHisto(&lock->acquireStatsMem,
                                     MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                                     MXUSER_DEFAULT_HISTO_DECADES);

         if (UNLIKELY(heldStats != NULL)) {
            MXUserForceHeldHisto(&lock->heldStatsMem,
                                 MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                                 MXUSER_DEFAULT_HISTO_DECADES);
         }

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
 * MXUser_EnableStatsExclLock
 *
 *      Enable basic stats on the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_EnableStatsExclLock(MXUserExclLock *lock,       // IN/OUT:
                           Bool trackAcquisitionTime,  // IN:
                           Bool trackHeldTime)         // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      MXUserEnableStats(trackAcquisitionTime ? &lock->acquireStatsMem : NULL,
                        trackHeldTime        ? &lock->heldStatsMem    : NULL);
   }

   return vmx86_stats;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DisableStatsExclLock --
 *
 *      Disable basic stats on the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_DisableStatsExclLock(MXUserExclLock *lock)  // IN/OUT:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      MXUserDisableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
   }

   return vmx86_stats;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_SetContentionRatioFloorExclLock --
 *
 *      Set the contention ratio floor for the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_SetContentionRatioFloorExclLock(MXUserExclLock *lock,  // IN/OUT:
                                       double ratio)          // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      result = MXUserSetContentionRatioFloor(&lock->acquireStatsMem, ratio);
   } else {
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_SetContentionCountFloorExclLock --
 *
 *      Set the contention count floor for the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_SetContentionCountFloorExclLock(MXUserExclLock *lock,  // IN/OUT:
                                       uint64 count)          // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      result = MXUserSetContentionCountFloor(&lock->acquireStatsMem, count);
   } else {
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_SetContentionDurationFloorExclLock --
 *
 *      Set the contention count floor for the specified lock.
 *
 * Results:
 *      TRUE    succeeded
 *      FALSE   failed
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_SetContentionDurationFloorExclLock(MXUserExclLock *lock,  // IN/OUT:
                                          uint64 duration)       // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      result = MXUserSetContentionDurationFloor(&lock->acquireStatsMem,
                                                duration);
   } else {
      result = FALSE;
   }

   return result;
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

   Warning("%s: Exclusive lock @ %p\n", __FUNCTION__, lock);

   Warning("\tsignature 0x%X\n", lock->header.signature);
   Warning("\tname %s\n", lock->header.name);
   Warning("\trank 0x%X\n", lock->header.rank);
   Warning("\tserial number %"FMT64"u\n", lock->header.serialNumber);

   Warning("\tlock count %d\n", MXRecLockCount(&lock->recursiveLock));

   Warning("\taddress of owner data %p\n",
           &lock->recursiveLock.nativeThreadID);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateExclLock --
 *
 *      Create an exclusive lock.
 *
 * Results:
 *      A pointer to an exclusive lock.
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
   uint32 statsMode;
   char *properName;
   MXUserExclLock *lock = Util_SafeCalloc(1, sizeof *lock);

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "X-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   if (UNLIKELY(!MXRecLockInit(&lock->recursiveLock))) {
      Panic("%s: native lock initialization routine failed\n", __FUNCTION__);
   }

   lock->header.signature = MXUserGetSignature(MXUSER_TYPE_EXCL);
   lock->header.name = properName;
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = MXUserDumpExclLock;

   statsMode = MXUserStatsMode();

   switch (MXUserStatsMode()) {
   case 0:
      MXUserDisableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
      lock->header.statsFunc = NULL;
      break;

   case 1:
      MXUserEnableStats(&lock->acquireStatsMem, NULL);
      lock->header.statsFunc = MXUserStatsActionExcl;
      break;

   case 2:
      MXUserEnableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
      lock->header.statsFunc = MXUserStatsActionExcl;
      break;

   default:
      Panic("%s: unknown stats mode: %d!\n", __FUNCTION__, statsMode);
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
      MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

      if (MXRecLockCount(&lock->recursiveLock) > 0) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Destroy of an acquired exclusive lock\n",
                            __FUNCTION__);
      }

      MXRecLockDestroy(&lock->recursiveLock);

      MXUserRemoveFromList(&lock->header);

      if (vmx86_stats) {
         MXUserDisableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
      }

      lock->header.signature = 0;  // just in case...
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
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   MXUserAcquisitionTracking(&lock->header, TRUE);

   if (vmx86_stats) {
      VmTimeType value = 0;
      MXUserHeldStats *heldStats;
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

      MXRecLockAcquire(&lock->recursiveLock,
                       (acquireStats == NULL) ? NULL : &value);

      if (LIKELY(acquireStats != NULL)) {
         MXUserHisto *histo;

         MXUserAcquisitionSample(&acquireStats->data, TRUE,
                            value > acquireStats->data.contentionDurationFloor,
                                 value);

         histo = Atomic_ReadPtr(&acquireStats->histo);

         if (UNLIKELY(histo != NULL)) {
            MXUserHistoSample(histo, value, GetReturnAddress());
         }

         heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

         if (UNLIKELY(heldStats != NULL)) {
            heldStats->holdStart = Hostinfo_SystemTimerNS();
         }
      }
   } else {
      MXRecLockAcquire(&lock->recursiveLock,
                       NULL);  // non-stats
   }

   if (vmx86_debug && (MXRecLockCount(&lock->recursiveLock) > 1)) {
      MXUserDumpAndPanic(&lock->header,
                         "%s: Acquire on an acquired exclusive lock\n",
                         __FUNCTION__);
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
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (vmx86_stats) {
      MXUserHeldStats *heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

      if (UNLIKELY(heldStats != NULL)) {
         VmTimeType value = Hostinfo_SystemTimerNS() - heldStats->holdStart;
         MXUserHisto *histo = Atomic_ReadPtr(&heldStats->histo);

         MXUserBasicStatsSample(&heldStats->data, value);

         if (UNLIKELY(histo != NULL)) {
            MXUserHistoSample(histo, value, GetReturnAddress());
         }
      }
   }

   if (vmx86_debug) {
      if (MXRecLockCount(&lock->recursiveLock) == 0) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Release of an unacquired exclusive lock\n",
                             __FUNCTION__);
      }

      if (!MXRecLockIsOwner(&lock->recursiveLock)) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Non-owner release of a exclusive lock\n",
                            __FUNCTION__);
      }
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

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   if (MXUserTryAcquireFail(lock->header.name)) {
      return FALSE;
   }

   success = MXRecLockTryAcquire(&lock->recursiveLock);

   if (success) {
      MXUserAcquisitionTracking(&lock->header, FALSE);

      if (vmx86_debug && (MXRecLockCount(&lock->recursiveLock) > 1)) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Acquire on an acquired exclusive lock\n",
                            __FUNCTION__);
      }
   }

   if (vmx86_stats) {
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

      if (LIKELY(acquireStats != NULL)) {
         MXUserAcquisitionSample(&acquireStats->data, success, !success,
                                 0ULL);
      }
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
 * MXUser_CreateSingletonExclLockInt --
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
MXUser_CreateSingletonExclLockInt(Atomic_Ptr *lockStorage,  // IN/OUT:
                                  const char *name,         // IN:
                                  MX_Rank rank)             // IN:
{
   MXUserExclLock *lock;

   ASSERT(lockStorage);

   lock = Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserExclLock *newLock = MXUser_CreateExclLock(name, rank);

      lock = Atomic_ReadIfEqualWritePtr(lockStorage, NULL, (void *) newLock);

      if (lock) {
         MXUser_DestroyExclLock(newLock);
      } else {
         lock = Atomic_ReadPtr(lockStorage);
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
                                uint32 waitTimeMsec)     // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar,
                     waitTimeMsec);
}
