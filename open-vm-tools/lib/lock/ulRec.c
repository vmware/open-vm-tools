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

   uint64                  holdStart;
   MXUserBasicStats        heldStats;
   Atomic_Ptr              heldHisto;
} MXUserStats;

struct MXUserRecLock
{
   MXUserHeader         header;
   MXRecLock            recursiveLock;
   Atomic_Ptr           statsMem;
   Atomic_uint32        refCount;

   /*
    * This is the MX recursive lock override pointer. It is used within the
    * VMX only.
    *
    *  NULL   Use the MXRecLock within this structure
    *         MXUser_CreateRecLock was used to create the lock
    *
    * !NULL   Use the MX_MutexRec pointed to by vmmLock
    *         MXUser_BindMXMutexRec was used to create the lock
    */

   struct MX_MutexRec  *vmmLock;
};


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserStatsActionRec --
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
MXUserStatsActionRec(MXUserHeader *header)  // IN:
{
   MXUserRecLock *lock = (MXUserRecLock *) header;
   MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

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
 * MXUser_ControlRecLock --
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
MXUser_ControlRecLock(MXUserRecLock *lock,  // IN/OUT:
                      uint32 command,       // IN:
                      ...)                  // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   switch (command) {
   case MXUSER_CONTROL_ACQUISITION_HISTO: {
      if (vmx86_stats) {
         MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

         if ((stats != NULL) && (lock->vmmLock == NULL)) {
            va_list a;
            uint32 decades;
            uint64 minValue;

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
      } else {
         result = FALSE;
      }

      break;
   }

   case MXUSER_CONTROL_HELD_HISTO: {
      if (vmx86_stats) {
         MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

         if ((stats != NULL) && (lock->vmmLock == NULL)) {
            va_list a;
            uint32 decades;
            uint32 minValue;

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
      } else {
         result = FALSE;
      }

      break; 
   }

   case MXUSER_CONTROL_ENABLE_STATS: {
      if (vmx86_stats) {
         MXUserStats *stats;
         MXUserStats *before;

         stats = Util_SafeCalloc(1, sizeof(*stats));

         MXUserAcquisitionStatsSetUp(&stats->acquisitionStats);
         MXUserBasicStatsSetUp(&stats->heldStats, MXUSER_STAT_CLASS_HELD);

         before = Atomic_ReadIfEqualWritePtr(&lock->statsMem, NULL,
                                             (void *) stats);

         if (before) {
            free(stats);
         }

         lock->header.statsFunc = MXUserStatsActionRec;

         result = TRUE;
      } else {
         result = FALSE;
      }

      break;
   }

   default:
      result = FALSE;
   }

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpRecLock --
 *
 *      Dump a recursive lock.
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
MXUserDumpRecLock(MXUserHeader *header)  // IN:
{
   MXUserRecLock *lock = (MXUserRecLock *) header;

   Warning("%s: Recursive lock @ %p\n", __FUNCTION__, lock);

   Warning("\tsignature 0x%X\n", lock->header.signature);
   Warning("\tname %s\n", lock->header.name);
   Warning("\trank 0x%X\n", lock->header.rank);
   Warning("\tserial number %u\n", lock->header.serialNumber);
   Warning("\treference count %u\n", Atomic_Read(&lock->refCount));

   if (lock->vmmLock == NULL) {
      Warning("\tcount %d\n", MXRecLockCount(&lock->recursiveLock));

      Warning("\taddress of owner data %p\n",
              &lock->recursiveLock.nativeThreadID);
   } else {
      Warning("\tvmmLock %p\n", lock->vmmLock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCreateRecLock --
 *
 *      Create a recursive lock specifying if the lock must always be
 *      silent.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
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

static MXUserRecLock *
MXUserCreateRecLock(const char *userName,  // IN:
                    MX_Rank rank,          // IN:
                    Bool beSilent)         // IN:
{
   Bool doStats;
   char *properName;
   MXUserRecLock *lock;

   lock = Util_SafeCalloc(1, sizeof(*lock));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "R-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   if (!MXRecLockInit(&lock->recursiveLock)) {
      free(properName);
      free(lock);

      return NULL;
   }

   lock->vmmLock = NULL;
   Atomic_Write(&lock->refCount, 1);

   lock->header.signature = MXUserGetSignature(MXUSER_TYPE_REC);
   lock->header.name = properName;
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = MXUserDumpRecLock;

   if (beSilent) {
      doStats = FALSE;
   } else {
      doStats = vmx86_stats && MXUserStatsEnabled();
   }

   if (doStats) {
      MXUser_ControlRecLock(lock, MXUSER_CONTROL_ENABLE_STATS);
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
 * MXUser_CreateRecLockSilent --
 *
 *      Create a recursive lock specifying if the lock must always be
 *      silent - never logging any messages. Silent locks will never
 *      produce any statistics, amongst the aspects of "silent".
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
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

MXUserRecLock *
MXUser_CreateRecLockSilent(const char *userName,  // IN:
                           MX_Rank rank)          // IN:
{
   return MXUserCreateRecLock(userName, rank, TRUE);
}

/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateRecLock --
 *
 *      Create a recursive lock.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
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

MXUserRecLock *
MXUser_CreateRecLock(const char *userName,  // IN:
                     MX_Rank rank)          // IN:
{
   return MXUserCreateRecLock(userName, rank, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserCondDestroyRecLock --
 *
 *      Destroy a recursive lock -- but only if its reference count is zero.
 *
 *      When the lock is bound to a MX lock, only the MXUser "wrapper" is
 *      freed. The caller is responsible for calling MX_DestroyLockRec() on
 *      the MX lock before calling this routine.
 *
 * Results:
 *      Lock is destroyed upon correct reference count. Don't use the
 *      pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserCondDestroyRecLock(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      if (lock->vmmLock == NULL) {
         if (MXRecLockCount(&lock->recursiveLock) > 0) {
            MXUserDumpAndPanic(&lock->header,
                               "%s: Destroy of an acquired recursive lock\n",
                               __FUNCTION__);
         }

         MXRecLockDestroy(&lock->recursiveLock);

         MXUserRemoveFromList(&lock->header);

         if (vmx86_stats) {
            MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

            if (LIKELY(stats != NULL)) {
               MXUserAcquisitionStatsTearDown(&stats->acquisitionStats);
               MXUserHistoTearDown(Atomic_ReadPtr(&stats->acquisitionHisto));

               MXUserBasicStatsTearDown(&stats->heldStats);
               MXUserHistoTearDown(Atomic_ReadPtr(&stats->heldHisto));

               free(stats);
            }
         }
      }

      lock->header.signature = 0;  // just in case...
      free(lock->header.name);
      lock->header.name = NULL;
      free(lock);
   }
}

void
MXUser_DestroyRecLock(MXUserRecLock *lock)  // IN:
{
   if (lock != NULL) {
      MXUserCondDestroyRecLock(lock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserAcquireRecLock --
 *
 *      An acquisition is made (lock is taken) on the specified recursive lock.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
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
MXUser_AcquireRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   if (lock->vmmLock) {
      ASSERT(MXUserMX_LockRec);
      (*MXUserMX_LockRec)(lock->vmmLock);
   } else {
      /* Rank checking is only done on the first acquisition */
      MXUserAcquisitionTracking(&lock->header, TRUE);

      if (vmx86_stats) {
         VmTimeType value = 0;
         MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

         MXRecLockAcquire(&lock->recursiveLock,
                          (stats == NULL) ? NULL : &value);

         if (LIKELY(stats != NULL)) {
            if (MXRecLockCount(&lock->recursiveLock) == 1) {
               MXUserHisto *histo;

               MXUserAcquisitionSample(&stats->acquisitionStats, TRUE,
                                       value != 0, value);

               histo = Atomic_ReadPtr(&stats->acquisitionHisto);

               if (UNLIKELY(histo != NULL)) {
                  MXUserHistoSample(histo, value, GetReturnAddress());
               }

               stats->holdStart = Hostinfo_SystemTimerNS();
            }
         }
      } else {
         MXRecLockAcquire(&lock->recursiveLock,
                          NULL);  // non-stats
      }
   }

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_ReleaseRecLock --
 *
 *      Release (unlock) a recursive lock.
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
MXUser_ReleaseRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   if (lock->vmmLock) {
      ASSERT(MXUserMX_UnlockRec);
      (*MXUserMX_UnlockRec)(lock->vmmLock);
   } else {
      if (vmx86_stats) {
         MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

         if (LIKELY(stats != NULL)) {
            if (MXRecLockCount(&lock->recursiveLock) == 1) {
               VmTimeType value = Hostinfo_SystemTimerNS() - stats->holdStart;
               MXUserHisto *histo = Atomic_ReadPtr(&stats->heldHisto);

               MXUserBasicStatsSample(&stats->heldStats, value);

               if (UNLIKELY(histo != NULL)) {
                  MXUserHistoSample(histo, value, GetReturnAddress());
               }
            }
         }
      }

      if (vmx86_debug && !MXRecLockIsOwner(&lock->recursiveLock)) {
         int lockCount = MXRecLockCount(&lock->recursiveLock);

         MXUserDumpAndPanic(&lock->header,
                            "%s: Non-owner release of an %s recursive lock\n",
                            __FUNCTION__,
                            lockCount == 0 ? "unacquired" : "acquired");
      }

      MXUserReleaseTracking(&lock->header);

      MXRecLockRelease(&lock->recursiveLock);
   }

   /*
    * Don't screw up the reference count! When this is the last reference
    * the lock will self destruct on a release if it is the last "hold"
    * of the lock.
    */

   MXUserCondDestroyRecLock(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TryAcquireRecLock --
 *
 *      An attempt is made to conditionally acquire (lock) a recursive lock.
 *
 *      Only the owner (thread) of a recursive lock may recurse on it.
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
MXUser_TryAcquireRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   Bool success;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   if (lock->vmmLock) {
      ASSERT(MXUserMX_TryLockRec);
      success = (*MXUserMX_TryLockRec)(lock->vmmLock);
   } else {
      if (MXUserTryAcquireFail(lock->header.name)) {
         success = FALSE;
         goto bail;
      }

      success = MXRecLockTryAcquire(&lock->recursiveLock);

      if (success) {
         MXUserAcquisitionTracking(&lock->header, FALSE);
      }

      if (vmx86_stats) {
         MXUserStats *stats = Atomic_ReadPtr(&lock->statsMem);

         if (LIKELY(stats != NULL)) {
            MXUserAcquisitionSample(&stats->acquisitionStats, success,
                                    !success, 0ULL);
         }
      }
   }

bail:

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IsCurThreadHoldingRecLock --
 *
 *      Is a recursive lock held by the calling thread?
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
MXUser_IsCurThreadHoldingRecLock(MXUserRecLock *lock)  // IN:
{
   Bool result;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   if (lock->vmmLock) {
      ASSERT(MXUserMX_IsLockedByCurThreadRec);
      result = (*MXUserMX_IsLockedByCurThreadRec)(lock->vmmLock);
   } else {
      result = MXRecLockIsOwner(&lock->recursiveLock);
   }

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonRecLock --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      recursive lock. This is useful for modules that need to protect
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

MXUserRecLock *
MXUser_CreateSingletonRecLock(Atomic_Ptr *lockStorage,  // IN/OUT:
                              const char *name,         // IN:
                              MX_Rank rank)             // IN:
{
   MXUserRecLock *lock;

   ASSERT(lockStorage);

   lock = Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserRecLock *newLock = MXUser_CreateRecLock(name, rank);

      lock = Atomic_ReadIfEqualWritePtr(lockStorage, NULL, (void *) newLock);

      if (lock) {
         MXUser_DestroyRecLock(newLock);
      } else {
         lock = Atomic_ReadPtr(lockStorage);
      }
   }

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateCondVarRecLock --
 *
 *      Create a condition variable for use with the specified recurisve lock.
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
MXUser_CreateCondVarRecLock(MXUserRecLock *lock)
{
   MXUserCondVar *condVar;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   Atomic_Inc(&lock->refCount);

   condVar =  MXUserCreateCondVar(&lock->header, &lock->recursiveLock);

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }

   return condVar;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_WaitCondVarRecLock --
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
MXUser_WaitCondVarRecLock(MXUserRecLock *lock,     // IN:
                          MXUserCondVar *condVar)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   Atomic_Inc(&lock->refCount);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar,
                     MXUSER_WAIT_INFINITE);

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TimedWaitCondVarRecLock --
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
MXUser_TimedWaitCondVarRecLock(MXUserRecLock *lock,     // IN:
                               MXUserCondVar *condVar,  // IN:
                               uint32 msecWait)         // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   Atomic_Inc(&lock->refCount);

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar, msecWait);

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DumpRecLock --
 *
 *      Dump a recursive lock.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DumpRecLock(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);

   MXUserDumpRecLock(&lock->header);

   if (Atomic_FetchAndDec(&lock->refCount) == 1) {
      Panic("%s: Zero reference count upon exit\n", __FUNCTION__);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_GetRecLockVmm --
 *
 *      Return lock->vmmLock. Perhaps this lock is bound to an MX lock.
 *
 * Results:
 *      lock->vmmLock is returned.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

struct MX_MutexRec *
MXUser_GetRecLockVmm(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   return lock->vmmLock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_GetRecLockRank --
 *
 *      Return the rank of the specified recursive lock.
 *
 * Results:
 *      The rank of the specified recursive lock is returned.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MX_Rank
MXUser_GetRecLockRank(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   return lock->header.rank;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_BindMXMutexRec --
 *
 *      Create an MXUserRecLock that is bound to an (already) initialized
 *      MX_MutexRec.
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

MXUserRecLock *
MXUser_BindMXMutexRec(struct MX_MutexRec *mutex,  // IN:
                      MX_Rank rank)               // IN:
{
   MXUserRecLock *lock;

   ASSERT(mutex);

   /*
    * Cannot perform a binding unless MX_Init has been called. As a side
    * effect it registers these hook functions.
    */

   if ((MXUserMX_LockRec == NULL) ||
       (MXUserMX_UnlockRec == NULL) ||
       (MXUserMX_TryLockRec == NULL) ||
       (MXUserMX_IsLockedByCurThreadRec == NULL)) {
       return NULL;
    }

   /*
    * Initialize the header (so it looks correct in memory) but don't connect
    * this lock to the MXUser statistics or debugging tracking - the MX lock
    * system will take care of this.
    */

   lock = Util_SafeCalloc(1, sizeof(*lock));

   lock->header.signature = MXUserGetSignature(MXUSER_TYPE_REC);
   lock->header.name = Str_SafeAsprintf(NULL, "MX_%p", mutex);
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = NULL;
   lock->header.statsFunc = NULL;

   Atomic_WritePtr(&lock->statsMem, NULL);
   Atomic_Write(&lock->refCount, 1);

   lock->vmmLock = mutex;

   return lock;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IncRefRecLock --
 *
 *      Add a reference to the lock to prevent an immediate destory from
 *      succeeding.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_IncRefRecLock(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   Atomic_Inc(&lock->refCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DecRefRecLock --
 *
 *      Remove a reference to the lock. If the reference count is zero,
 *      the lock is destroyed.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DecRefRecLock(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_REC);

   MXUserCondDestroyRecLock(lock);
}


#if defined(VMX86_VMX)
#include "mutex.h"
#include "mutexRankVMX.h"

/*
 *----------------------------------------------------------------------------
 *
 * MXUser_InitFromMXRec --
 *
 *      Initialize a MX_MutexRec lock and create a MXUserRecLock that binds
 *      to it.
 *
 * Results:
 *      Pointer to the MXUserRecLock.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

MXUserRecLock *
MXUser_InitFromMXRec(const char *name,    // IN:
                     MX_MutexRec *mutex,  // IN:
                     MX_Rank rank,        // IN:
                     Bool isBelowBull)    // IN:
{
   MXUserRecLock *userLock;

   ASSERT(isBelowBull == (rank < RANK_userlevelLock));

   MX_InitLockRec(name, rank, mutex);
   userLock = MXUser_BindMXMutexRec(mutex, rank);
   ASSERT(userLock);

   return userLock;
}
#endif
