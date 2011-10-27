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
   uint64                  holdStart;
   MXUserBasicStats        heldStats;
   Atomic_Ptr              heldHisto;
} MXUserStats;

#define MXUSER_REC_SIGNATURE 0x43524B4C // 'LKRC' in memory

struct MXUserRecLock
{
   MXUserHeader         header;
   MXRecLock            recursiveLock;
   Atomic_Ptr           statsMem;
   Atomic_uint32        destroyRefCount;
   Atomic_uint32        destroyWasCalled;

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

   Warning("%s: Recursive lock @ 0x%p\n", __FUNCTION__, lock);

   Warning("\tsignature 0x%X\n", lock->header.signature);
   Warning("\tname %s\n", lock->header.name);
   Warning("\trank 0x%X\n", lock->header.rank);
   Warning("\tserial number %u\n", lock->header.serialNumber);

   if (lock->vmmLock == NULL) {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      Warning("\tcount %u\n", lock->recursiveLock.referenceCount);

#if defined(_WIN32)
      Warning("\towner %u\n", lock->recursiveLock.nativeThreadID);
#else
      Warning("\towner 0x%p\n",
              (void *)(uintptr_t)lock->recursiveLock.nativeThreadID);
#endif

      if (stats && (stats->holder != NULL)) {
         Warning("\tholder %p\n", stats->holder);
      }
   } else {
      Warning("\tvmmLock 0x%p\n", lock->vmmLock);
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

   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   switch (command) {
   case MXUSER_CONTROL_ACQUISITION_HISTO: {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats && (lock->vmmLock == NULL)) {
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

      if (stats && (lock->vmmLock == NULL)) {
         va_list a;
         uint64 minValue;
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

         lock->header.statsFunc = MXUserStatsActionRec;
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
   Atomic_Write(&lock->destroyRefCount, 1);
   Atomic_Write(&lock->destroyWasCalled, 0);

   lock->header.signature = MXUSER_REC_SIGNATURE;
   lock->header.name = properName;
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = MXUserDumpRecLock;

   if (beSilent || !MXUserStatsEnabled()) {
      lock->header.statsFunc = NULL;
      Atomic_WritePtr(&lock->statsMem, NULL);
   } else {
      MXUser_ControlRecLock(lock, MXUSER_CONTROL_ENABLE_STATS);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (Atomic_FetchAndDec(&lock->destroyRefCount) == 1) {
      if (lock->vmmLock == NULL) {
         MXUserStats *stats;

         if (MXRecLockCount(&lock->recursiveLock) > 0) {
            MXUserDumpAndPanic(&lock->header,
                               "%s: Destroy of an acquired recursive lock\n",
                               __FUNCTION__);
         }

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
      ASSERT(lock->header.signature == MXUSER_REC_SIGNATURE);

      /*
       * May not call destroy on a lock more than once
       *
       * That the code can get here may only occur if the reference count
       * mechanism is used.
       */

      if (Atomic_FetchAndInc(&lock->destroyWasCalled) != 0) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Destroy of a destroyed recursive lock\n",
                            __FUNCTION__);
      }

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

static void
MXUserAcquireRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (lock->vmmLock) {
      ASSERT(MXUserMX_LockRec);
      (*MXUserMX_LockRec)(lock->vmmLock);
   } else {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      /* Rank checking is only done on the first acquisition */
      MXUserAcquisitionTracking(&lock->header, TRUE);

      if (stats) {
         Bool contended;
         VmTimeType begin = Hostinfo_SystemTimerNS();

         contended = MXRecLockAcquire(&lock->recursiveLock);

         if (MXRecLockCount(&lock->recursiveLock) == 1) {
            MXUserHisto *histo;
            VmTimeType value = Hostinfo_SystemTimerNS() - begin;

            MXUserAcquisitionSample(&stats->acquisitionStats, TRUE, contended,
                                    value);

            stats->holder = GetReturnAddress();

            histo = Atomic_ReadPtr(&stats->acquisitionHisto);

            if (UNLIKELY(histo != NULL)) {
               MXUserHistoSample(histo, value, stats->holder);
            }

            stats->holdStart = Hostinfo_SystemTimerNS();
         }
      } else {
         MXRecLockAcquire(&lock->recursiveLock);
      }
   }
}


void
MXUser_AcquireRecLock(MXUserRecLock *lock)  // IN/OUT:
{
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   ASSERT(Atomic_Read(&lock->destroyWasCalled) == 0);

   MXUserAcquireRecLock(lock);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (lock->vmmLock) {
      ASSERT(MXUserMX_UnlockRec);
      (*MXUserMX_UnlockRec)(lock->vmmLock);
   } else {
      MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats) {
         if (MXRecLockCount(&lock->recursiveLock) == 1) {
            VmTimeType value = Hostinfo_SystemTimerNS() - stats->holdStart;
            MXUserHisto *histo = Atomic_ReadPtr(&stats->heldHisto);

            MXUserBasicStatsSample(&stats->heldStats, value);

            if (UNLIKELY(histo != NULL)) {
               MXUserHistoSample(histo, value, stats->holder);
               stats->holder = NULL;
            }
         }
      }

      if (!MXRecLockIsOwner(&lock->recursiveLock)) {
         uint32 lockCount = MXRecLockCount(&lock->recursiveLock);

         MXUserDumpAndPanic(&lock->header,
                            "%s: Non-owner release of an %s recursive lock\n",
                            __FUNCTION__,
                            lockCount == 0 ? "unacquired" : "acquired");
      }

      MXUserReleaseTracking(&lock->header);

      MXRecLockRelease(&lock->recursiveLock);
   }
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

   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (UNLIKELY(Atomic_Read(&lock->destroyWasCalled) != 0)) {
      return FALSE;
   }

   if (lock->vmmLock) {
      ASSERT(MXUserMX_TryLockRec);
      success = (*MXUserMX_TryLockRec)(lock->vmmLock);
   } else {
      MXUserStats *stats;

      if (MXUserTryAcquireFail(lock->header.name)) {
         return FALSE;
      }

      success = MXRecLockTryAcquire(&lock->recursiveLock);

      if (success) {
         MXUserAcquisitionTracking(&lock->header, FALSE);
      }

      stats = (MXUserStats *) Atomic_ReadPtr(&lock->statsMem);

      if (stats) {
         MXUserAcquisitionSample(&stats->acquisitionStats, success,
                                 !success, 0ULL);
      }
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
MXUser_IsCurThreadHoldingRecLock(const MXUserRecLock *lock)  // IN:
{
   Bool result;
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (lock->vmmLock) {
      ASSERT(MXUserMX_IsLockedByCurThreadRec);
      result = (*MXUserMX_IsLockedByCurThreadRec)(lock->vmmLock);
   } else {
      result = MXRecLockIsOwner(&lock->recursiveLock);
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

   lock = (MXUserRecLock *) Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserRecLock *newLock = MXUser_CreateRecLock(name, rank);

      lock = (MXUserRecLock *) Atomic_ReadIfEqualWritePtr(lockStorage, NULL,
                                                          (void *) newLock);

      if (lock) {
         MXUser_DestroyRecLock(newLock);
      } else {
         lock = (MXUserRecLock *) Atomic_ReadPtr(lockStorage);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   return MXUserCreateCondVar(&lock->header, &lock->recursiveLock);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar,
                     MXUSER_WAIT_INFINITE);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));
   ASSERT(lock->vmmLock == NULL);  // only unbound locks

   MXUserWaitCondVar(&lock->header, &lock->recursiveLock, condVar, msecWait);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   MXUserDumpRecLock(&lock->header);
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
MXUser_GetRecLockVmm(const MXUserRecLock *lock)  // IN:
{
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

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
MXUser_GetRecLockRank(const MXUserRecLock *lock)  // IN:
{
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

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

   lock->header.signature = MXUSER_REC_SIGNATURE;
   lock->header.name = Str_SafeAsprintf(NULL, "MX_%p", mutex);
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = NULL;
   lock->header.statsFunc = NULL;

   Atomic_WritePtr(&lock->statsMem, NULL);
   Atomic_Write(&lock->destroyRefCount, 1);
   Atomic_Write(&lock->destroyWasCalled, 0);

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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   Atomic_Inc(&lock->destroyRefCount);
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
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   MXUserCondDestroyRecLock(lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireWeakRefRecLock --
 *
 *      Acquire a lock only if destroy has not be called on it. This is 
 *      special implementation that will have limited lifetime. Once Poll
 *      is upgraded to use trylock this implementation will go away.
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_AcquireWeakRefRecLock(MXUserRecLock *lock)  // IN:
{
   ASSERT(lock && (lock->header.signature == MXUSER_REC_SIGNATURE));

   if (Atomic_Read(&lock->destroyWasCalled) != 0) {
      return FALSE;
   } else {
      MXUserAcquireRecLock(lock);

      return TRUE;
   }
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
