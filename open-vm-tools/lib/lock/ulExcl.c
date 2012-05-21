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
   MXUserAcquisitionStats  data;
   Atomic_Ptr              histo;
} MXUserAcquireStats;

typedef struct
{
   VmTimeType        holdStart;
   MXUserBasicStats  data;
   Atomic_Ptr        histo;
} MXUserHeldStats;

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
         MXUserForceHisto(&acquireStats->histo,
                          MXUSER_STAT_CLASS_ACQUISITION,
                          MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                          MXUSER_DEFAULT_HISTO_DECADES);

         if (UNLIKELY(heldStats != NULL)) {
            MXUserForceHisto(&heldStats->histo,
                             MXUSER_STAT_CLASS_HELD,
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
      if (vmx86_stats) {
         MXUserAcquireStats *acquireStats;

         acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

         if (acquireStats == NULL) {
            result = FALSE;
         } else {
            va_list a;
            uint32 decades;
            uint64 minValue;

            va_start(a, command);
            minValue = va_arg(a, uint64);
            decades = va_arg(a, uint32);
            va_end(a);

            MXUserForceHisto(&acquireStats->histo,
                             MXUSER_STAT_CLASS_ACQUISITION, minValue, decades);

            result = TRUE;
         }

      } else {
         result = FALSE;
      }

      break;
   }

   case MXUSER_CONTROL_HELD_HISTO: {
      if (vmx86_stats) {
         MXUserHeldStats *heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

         if (heldStats == NULL) {
            result = FALSE;
         } else {
            va_list a;
            uint32 decades;
            uint32 minValue;

            va_start(a, command);
            minValue = va_arg(a, uint64);
            decades = va_arg(a, uint32);
            va_end(a);

            MXUserForceHisto(&heldStats->histo, MXUSER_STAT_CLASS_HELD,
                             minValue, decades);

            result = TRUE;
         }
      } else {
         result = FALSE;
      }

      break;
   }

   case MXUSER_CONTROL_ENABLE_STATS: {
      if (vmx86_stats) {
         va_list a;
         Bool trackHeldTimes;
         MXUserHeldStats *heldStats;
         MXUserAcquireStats *acquireStats;

         acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

         if (LIKELY(acquireStats == NULL)) {
            MXUserAcquireStats *before;

            acquireStats = Util_SafeCalloc(1, sizeof(*acquireStats));
            MXUserAcquisitionStatsSetUp(&acquireStats->data);

            before = Atomic_ReadIfEqualWritePtr(&lock->acquireStatsMem, NULL,
                                                (void *) acquireStats);

            if (before) {
               free(acquireStats);
            }
         }

         va_start(a, command);
         trackHeldTimes = va_arg(a, int);
         va_end(a);

         heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

         if ((heldStats == NULL) && trackHeldTimes) {
            MXUserHeldStats *before;

            heldStats = Util_SafeCalloc(1, sizeof(*heldStats));
            MXUserBasicStatsSetUp(&heldStats->data, MXUSER_STAT_CLASS_HELD);

            before = Atomic_ReadIfEqualWritePtr(&lock->heldStatsMem, NULL,
                                                (void *) heldStats);

            if (before) {
               free(heldStats);
            }
         }

         lock->header.statsFunc = MXUserStatsActionExcl;

         result = TRUE;
      } else {
         result = FALSE;
      }

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
   Warning("\tserial number %u\n", lock->header.serialNumber);

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
   uint32 statsMode;
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

   statsMode = MXUserStatsMode();

   switch (statsMode) {
   case 0:
      lock->header.statsFunc = NULL;
      Atomic_WritePtr(&lock->acquireStatsMem, NULL);
      Atomic_WritePtr(&lock->heldStatsMem, NULL);
      break;

   case 1:
      MXUser_ControlExclLock(lock, MXUSER_CONTROL_ENABLE_STATS, FALSE);
      break;

   case 2:
      MXUser_ControlExclLock(lock, MXUSER_CONTROL_ENABLE_STATS, TRUE);
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

      lock->header.signature = 0;  // just in case...

      MXRecLockDestroy(&lock->recursiveLock);

      MXUserRemoveFromList(&lock->header);

      if (vmx86_stats) {
         MXUserHeldStats *heldStats;
         MXUserAcquireStats *acquireStats;

         acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

         if (LIKELY(acquireStats != NULL)) {
            MXUserAcquisitionStatsTearDown(&acquireStats->data);
            MXUserHistoTearDown(Atomic_ReadPtr(&acquireStats->histo));

            free(acquireStats);
         }

         heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

         if (UNLIKELY(heldStats != NULL)) {
            MXUserBasicStatsTearDown(&heldStats->data);
            MXUserHistoTearDown(Atomic_ReadPtr(&heldStats->histo));

            free(heldStats);
         }
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

         MXUserAcquisitionSample(&acquireStats->data, TRUE, value != 0,
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

   if (vmx86_debug && !MXRecLockIsOwner(&lock->recursiveLock)) {
      int lockCount = MXRecLockCount(&lock->recursiveLock);

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
                                uint32 msecWait)         // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_EXCL);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar, msecWait);
}
