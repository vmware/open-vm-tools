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

#if defined(_WIN32)
#include <windows.h>
#endif

#include "vmware.h"
#include "vm_basic_asm.h"
#include "str.h"
#include "util.h"
#include "hashTable.h"
#include "userlock.h"
#include "hostinfo.h"
#include "ulInt.h"

/*
 * Environment specific implementations of portable read-write locks.
 *
 * There are 5 environment specific primitives:
 *
 * MXUserNativeRWSupported   Are native read-write locks supported?
 * MXUserNativeRWInit        Allocate and initialize a native read-write lock
 * MXUserNativeRWDestroy     Destroy a native read-write lock
 * MXUserNativeRWAcquire     Acquire a native read-write lock
 * MXUserNativeRWRelease     Release a native read-write lock
 */

#if defined(_WIN32)
typedef SRWLOCK NativeRWLock;

typedef VOID (WINAPI *InitializeSRWLockFn)(PSRWLOCK lock);
typedef VOID (WINAPI *AcquireSRWLockSharedFn)(PSRWLOCK lock);
typedef VOID (WINAPI *ReleaseSRWLockSharedFn)(PSRWLOCK lock);
typedef VOID (WINAPI *AcquireSRWLockExclusiveFn)(PSRWLOCK lock);
typedef VOID (WINAPI *ReleaseSRWLockExclusiveFn)(PSRWLOCK lock);

static InitializeSRWLockFn        pInitializeSRWLock;
static AcquireSRWLockSharedFn     pAcquireSRWLockShared;
static ReleaseSRWLockSharedFn     pReleaseSRWLockShared;
static AcquireSRWLockExclusiveFn  pAcquireSRWLockExclusive;
static ReleaseSRWLockExclusiveFn  pReleaseSRWLockExclusive;

static Bool
MXUserNativeRWSupported(void)
{
   static Bool result;
   static Bool done = FALSE;

   if (!done) {
      HMODULE kernel32 = GetModuleHandleW(L"kernel32.dll");

      if (kernel32) {
         pInitializeSRWLock = (InitializeSRWLockFn)
                              GetProcAddress(kernel32,
                                             "InitializeSRWLock");

         pAcquireSRWLockShared = (AcquireSRWLockSharedFn)
                                 GetProcAddress(kernel32,
                                                "AcquireSRWLockShared");

         pReleaseSRWLockShared = (ReleaseSRWLockSharedFn)
                                 GetProcAddress(kernel32,
                                                "ReleaseSRWLockShared");

         pAcquireSRWLockExclusive = (AcquireSRWLockExclusiveFn)
                                    GetProcAddress(kernel32,
                                                   "AcquireSRWLockExclusive");

         pReleaseSRWLockExclusive = (ReleaseSRWLockExclusiveFn)
                                    GetProcAddress(kernel32,
                                                   "ReleaseSRWLockExclusive");

         COMPILER_MEM_BARRIER();

         result = ((pInitializeSRWLock != NULL) &&
                   (pAcquireSRWLockShared != NULL) &&
                   (pReleaseSRWLockShared != NULL) &&
                   (pAcquireSRWLockExclusive != NULL) &&
                   (pReleaseSRWLockExclusive != NULL));

         COMPILER_MEM_BARRIER();
      } else {
         result = FALSE;
      }

      done = TRUE;
   }

   return result;
}

static Bool
MXUserNativeRWInit(NativeRWLock *lock)  // IN:
{
   ASSERT(pInitializeSRWLock);
   (*pInitializeSRWLock)(lock);

   return TRUE;
}

static int
MXUserNativeRWDestroy(NativeRWLock *lock)  // IN:
{
   return 0; // nothing to do
}


static INLINE Bool
MXUserNativeRWAcquire(NativeRWLock *lock,  // IN:
                      Bool forRead,        // IN:
                      int *err)            // OUT:
{
   if (forRead) {
      ASSERT(pAcquireSRWLockShared);
      (*pAcquireSRWLockShared)(lock);
   } else {
      ASSERT(pAcquireSRWLockExclusive);
      (*pAcquireSRWLockExclusive)(lock);
   }

   *err = 0;

   return FALSE;
}

static INLINE int
MXUserNativeRWRelease(NativeRWLock *lock,  // IN:
                      Bool forRead)        // IN:
{
   if (forRead) {
      ASSERT(pReleaseSRWLockShared);
      (*pReleaseSRWLockShared)(lock);
   } else {
      ASSERT(pReleaseSRWLockExclusive);
      (*pReleaseSRWLockExclusive)(lock);
   }

   return 0;
}
#else  // _WIN32
#if defined(PTHREAD_RWLOCK_INITIALIZER)
typedef pthread_rwlock_t NativeRWLock;
#else
typedef int NativeRWLock;
#endif

static Bool
MXUserNativeRWSupported(void)
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   return TRUE;
#else
   return FALSE;
#endif
}

static Bool
MXUserNativeRWInit(NativeRWLock *lock)  // IN:
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   return (pthread_rwlock_init(lock, NULL) == 0);
#else
   return FALSE;
#endif
}

static int
MXUserNativeRWDestroy(NativeRWLock *lock)  // IN:
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   return pthread_rwlock_destroy(lock);
#else
   return ENOSYS;
#endif
}

static INLINE Bool
MXUserNativeRWAcquire(NativeRWLock *lock,  // IN:
                      Bool forRead,        // IN:
                      int *err)            // OUT:
{
   Bool contended;

#if defined(PTHREAD_RWLOCK_INITIALIZER)
   *err = (forRead) ? pthread_rwlock_tryrdlock(lock) :
                      pthread_rwlock_trywrlock(lock);

   contended = (*err != 0);

   if (*err == EBUSY) {
      *err = (forRead) ? pthread_rwlock_rdlock(lock) :
                         pthread_rwlock_wrlock(lock);
   }
#else
   *err = ENOSYS;
   contended = FALSE;
#endif

   return contended;
}

static INLINE int
MXUserNativeRWRelease(NativeRWLock *lock,  // IN:
                      Bool forRead)        // IN:
{
#if defined(PTHREAD_RWLOCK_INITIALIZER)
   return pthread_rwlock_unlock(lock);
#else
   return ENOSYS;
#endif
}
#endif  // _WIN32

typedef enum {
   RW_UNLOCKED,
   RW_LOCKED_FOR_READ,
   RW_LOCKED_FOR_WRITE
} HolderState;

typedef struct {
   HolderState   state;
   VmTimeType    holdStart;
} HolderContext;

struct MXUserRWLock
{
   MXUserHeader    header;

   Bool            useNative;
   NativeRWLock    nativeLock;
   MXRecLock       recursiveLock;

   Atomic_uint32   holderCount;
   HashTable      *holderTable;

   Atomic_Ptr      heldStatsMem;
   Atomic_Ptr      acquireStatsMem;
};


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserStatsActionRW --
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
MXUserStatsActionRW(MXUserHeader *header)  // IN:
{
   MXUserRWLock *lock = (MXUserRWLock *) header;
   MXUserHeldStats *heldStats = Atomic_ReadPtr(&lock->heldStatsMem);
   MXUserAcquireStats *acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

   if (heldStats) {
      MXUserDumpBasicStats(&heldStats->data, header);

      if (Atomic_ReadPtr(&heldStats->histo) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&heldStats->histo), header);
      }
   }

   if (acquireStats) {
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

      if (isHot) {
         MXUserForceAcquisitionHisto(&lock->acquireStatsMem,
                                     MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                                     MXUSER_DEFAULT_HISTO_DECADES);

         if (heldStats) {
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
 * MXUserDumpRWLock --
 *
 *      Dump an read-write lock.
 *
 * Results:
 *      A dump.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserDumpRWLock(MXUserHeader *header)  // IN:
{
   MXUserRWLock *lock = (MXUserRWLock *) header;

   Warning("%s: Read-write lock @ 0x%p\n", __FUNCTION__, lock);

   Warning("\tsignature 0x%X\n", lock->header.signature);
   Warning("\tname %s\n", lock->header.name);
   Warning("\trank 0x%X\n", lock->header.rank);
   Warning("\tserial number %"FMT64"u\n", lock->header.serialNumber);

   if (LIKELY(lock->useNative)) {
      Warning("\taddress of native lock 0x%p\n", &lock->nativeLock);
   } else {
      Warning("\tcount %d\n", MXRecLockCount(&lock->recursiveLock));
   }

   Warning("\tholderCount %d\n", Atomic_Read(&lock->holderCount));
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateRWLock --
 *
 *      Create a read/write lock.
 *
 *      If native read-write locks are not available, a recursive lock will
 *      be used to provide one reader or one writer access... which is
 *      better than nothing.
 *
 * Results:
 *      A pointer to a read/write lock.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserFreeHashEntry(void *data)  // IN:
{
   free(data);
}

MXUserRWLock *
MXUser_CreateRWLock(const char *userName,  // IN:
                    MX_Rank rank)          // IN:
{
   Bool lockInited;
   char *properName;
   Bool useNative = MXUserNativeRWSupported();
   MXUserRWLock *lock = Util_SafeCalloc(1, sizeof *lock);

   if (userName == NULL) {
      if (LIKELY(useNative)) {
         properName = Str_SafeAsprintf(NULL, "RW-%p", GetReturnAddress());
      } else {
         /* emulated */
         properName = Str_SafeAsprintf(NULL, "RWemul-%p", GetReturnAddress());
      }
   } else {
      properName = Util_SafeStrdup(userName);
   }

   lock->header.signature = MXUserGetSignature(MXUSER_TYPE_RW);
   lock->header.name = properName;
   lock->header.rank = rank;
   lock->header.serialNumber = MXUserAllocSerialNumber();
   lock->header.dumpFunc = MXUserDumpRWLock;

   /*
    * Always attempt to use native locks when they are available. If, for some
    * reason, a native lock should be available but isn't, fall back to using
    * an internal recursive lock - something is better than nothing.
    */

   lock->useNative = useNative && MXUserNativeRWInit(&lock->nativeLock);

   lockInited = MXRecLockInit(&lock->recursiveLock);

   if (LIKELY(lockInited)) {
      uint32 statsMode;

      lock->holderTable = HashTable_Alloc(256,
                                          HASH_INT_KEY | HASH_FLAG_ATOMIC,
                                          MXUserFreeHashEntry);

      statsMode = MXUserStatsMode();

      switch (MXUserStatsMode()) {
      case 0:
         MXUserDisableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
         lock->header.statsFunc = NULL;
         break;

      case 1:
         MXUserEnableStats(&lock->acquireStatsMem, NULL);
         lock->header.statsFunc = MXUserStatsActionRW;
         break;

      case 2:
         MXUserEnableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
         lock->header.statsFunc = MXUserStatsActionRW;
         break;

      default:
         Panic("%s: unknown stats mode: %d!\n", __FUNCTION__, statsMode);
      }

      MXUserAddToList(&lock->header);
   } else {
      Panic("%s: native lock initialization routine failed\n", __FUNCTION__);
   }

   return lock;
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
   if (LIKELY(lock != NULL)) {
      MXUserValidateHeader(&lock->header, MXUSER_TYPE_RW);

      if (Atomic_Read(&lock->holderCount) != 0) {
         MXUserDumpAndPanic(&lock->header,
                            "%s: Destroy on an acquired read-write lock\n",
                            __FUNCTION__);
      }

      if (LIKELY(lock->useNative)) {
         int err = MXUserNativeRWDestroy(&lock->nativeLock);

         if (UNLIKELY(err != 0)) {
            MXUserDumpAndPanic(&lock->header, "%s: Internal error (%d)\n",
                               __FUNCTION__, err);
         }
      }

      MXRecLockDestroy(&lock->recursiveLock);

      MXUserRemoveFromList(&lock->header);

      if (vmx86_stats) {
         MXUserDisableStats(&lock->acquireStatsMem, &lock->heldStatsMem);
      }

      HashTable_FreeUnsafe(lock->holderTable);

      lock->header.signature = 0;  // just in case...
      free(lock->header.name);
      lock->header.name = NULL;
      free(lock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserGetHolderContext --
 *
 *      Look up the context of the calling thread with respect to the
 *      specified lock and return it.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static HolderContext *
MXUserGetHolderContext(MXUserRWLock *lock)  // IN:
{
   HolderContext *result;
   void *threadID = MXUserCastedThreadID();

   ASSERT(lock->holderTable);

   if (!HashTable_Lookup(lock->holderTable, threadID, (void **) &result)) {
      HolderContext *newContext = Util_SafeMalloc(sizeof *newContext);

      newContext->holdStart = 0;
      newContext->state = RW_UNLOCKED;

      result = HashTable_LookupOrInsert(lock->holderTable, threadID,
                                        (void *) newContext);

      if (result != newContext) {
         free(newContext);
      }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserAcquisition --
 *
 *      Acquire a read-write lock in the specified mode.
 *
 * Results:
 *      As above.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
MXUserAcquisition(MXUserRWLock *lock,  // IN/OUT:
                  Bool forRead)        // IN:
{
   HolderContext *myContext;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_RW);

   MXUserAcquisitionTracking(&lock->header, TRUE);

   myContext = MXUserGetHolderContext(lock);

   if (UNLIKELY(myContext->state != RW_UNLOCKED)) {
      MXUserDumpAndPanic(&lock->header,
                         "%s: AcquireFor%s after AcquireFor%s\n",
                         __FUNCTION__,
                        forRead ? "Read" : "Write",
                        (myContext->state == RW_LOCKED_FOR_READ) ? "Read" :
                                                                   "Write");
   }

   if (vmx86_stats) {
      VmTimeType value;
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&lock->acquireStatsMem);

      if (lock->useNative) {
         int err = 0;
         Bool contended;
         VmTimeType begin = Hostinfo_SystemTimerNS();

         contended = MXUserNativeRWAcquire(&lock->nativeLock, forRead, &err);

         value = contended ? Hostinfo_SystemTimerNS() - begin : 0;

         if (UNLIKELY(err != 0)) {
            MXUserDumpAndPanic(&lock->header, "%s: Error %d: contended %d\n",
                               __FUNCTION__, err, contended);
         }
      } else {
         value = 0;

         MXRecLockAcquire(&lock->recursiveLock,
                          (acquireStats == NULL) ? NULL : &value);
      }

      if (LIKELY(acquireStats != NULL)) {
         MXUserHisto *histo;
         MXUserHeldStats *heldStats;

         /*
          * The statistics are not atomically safe so protect them when
          * necessary.
          */

         if (forRead && lock->useNative) {
            MXRecLockAcquire(&lock->recursiveLock,
                             NULL);  // non-stats
         }

         MXUserAcquisitionSample(&acquireStats->data, TRUE,
                            value > acquireStats->data.contentionDurationFloor,
                                 value);

         histo = Atomic_ReadPtr(&acquireStats->histo);

         if (UNLIKELY(histo != NULL)) {
            MXUserHistoSample(histo, value, GetReturnAddress());
         }

         if (forRead && lock->useNative) {
            MXRecLockRelease(&lock->recursiveLock);
         }

         heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

         if (UNLIKELY(heldStats != NULL)) {
            myContext->holdStart = Hostinfo_SystemTimerNS();
         }
      }
   } else {
      if (LIKELY(lock->useNative)) {
         int err = 0;

         MXUserNativeRWAcquire(&lock->nativeLock, forRead, &err);

         if (UNLIKELY(err != 0)) {
            MXUserDumpAndPanic(&lock->header, "%s: Error %d\n",
                               __FUNCTION__, err);
         }
      } else {
         MXRecLockAcquire(&lock->recursiveLock,
                          NULL);  // non-stats
      }
   }

   if (!forRead || !lock->useNative) {
      ASSERT(Atomic_Read(&lock->holderCount) == 0);
   }

   Atomic_Inc(&lock->holderCount);
   myContext->state = forRead ? RW_LOCKED_FOR_READ : RW_LOCKED_FOR_WRITE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireForRead --
 *
 *      Acquire a read-write lock for read-shared access. A thread may have
 *      only one read lock outstanding on a read-write lock - no recursive
 *      access.
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
   MXUserAcquisition(lock, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_AcquireForWrite --
 *
 *      Acquire a read-write lock for write-exclusive access. A thread may
 *      have only a single write lock outstanding on a read-write lock.
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
   MXUserAcquisition(lock, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_IsCurThreadHolding --
 *
 *      Is the read-write lock held in the mode queried?
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
MXUser_IsCurThreadHoldingRWLock(MXUserRWLock *lock,  // IN:
                                uint32 queryType)    // IN:
{
   HolderContext *myContext;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_RW);

   myContext = MXUserGetHolderContext(lock);

   switch (queryType) {
   case MXUSER_RW_FOR_READ:
      return myContext->state == RW_LOCKED_FOR_READ;

   case MXUSER_RW_FOR_WRITE:
      return myContext->state == RW_LOCKED_FOR_WRITE;

   case MXUSER_RW_LOCKED:
      return myContext->state != RW_UNLOCKED;

   default:
      Panic("%s: unknown query type %d\n", __FUNCTION__, queryType);
   }
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
   HolderContext *myContext;

   ASSERT(lock);
   MXUserValidateHeader(&lock->header, MXUSER_TYPE_RW);

   myContext = MXUserGetHolderContext(lock);

   if (vmx86_stats) {
      MXUserHeldStats *heldStats = Atomic_ReadPtr(&lock->heldStatsMem);

      if (UNLIKELY(heldStats != NULL)) {
         MXUserHisto *histo;
         VmTimeType duration = Hostinfo_SystemTimerNS() - myContext->holdStart;

         /*
          * The statistics are not always atomically safe so protect them
          * when necessary
          */

         if ((myContext->state == RW_LOCKED_FOR_READ) && lock->useNative) {
            MXRecLockAcquire(&lock->recursiveLock,
                             NULL);  // non-stats
         }

         MXUserBasicStatsSample(&heldStats->data, duration);

         histo = Atomic_ReadPtr(&heldStats->histo);

         if (UNLIKELY(histo != NULL)) {
            MXUserHistoSample(histo, duration, GetReturnAddress());
         }

         if ((myContext->state == RW_LOCKED_FOR_READ) && lock->useNative) {
            MXRecLockRelease(&lock->recursiveLock);
         }
      }
   }

   if (UNLIKELY(myContext->state == RW_UNLOCKED)) {
      MXUserDumpAndPanic(&lock->header,
                         "%s: Release of an unacquired read-write lock\n",
                         __FUNCTION__);
   }

   MXUserReleaseTracking(&lock->header);

   Atomic_Dec(&lock->holderCount);

   if (LIKELY(lock->useNative)) {
      int err = MXUserNativeRWRelease(&lock->nativeLock,
                                      myContext->state == RW_LOCKED_FOR_READ);

      if (UNLIKELY(err != 0)) {
         MXUserDumpAndPanic(&lock->header, "%s: Internal error (%d)\n",
                            __FUNCTION__, err);
      }
   } else {
      ASSERT(Atomic_Read(&lock->holderCount) == 0);
      MXRecLockRelease(&lock->recursiveLock);
   }

   myContext->state = RW_UNLOCKED;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonRWLockInt --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      RW lock. This is useful for modules that need to protect something
 *      with a lock but don't have an existing Init() entry point where a
 *      lock can be created.
 *
 * Results:
 *      A pointer to the requested lock.
 *
 * Side effects:
 *      Generally the lock's resources are intentionally leaked (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserRWLock *
MXUser_CreateSingletonRWLockInt(Atomic_Ptr *lockStorage,  // IN/OUT:
                                const char *name,         // IN:
                                MX_Rank rank)             // IN:
{
   MXUserRWLock *lock;

   ASSERT(lockStorage);

   lock = Atomic_ReadPtr(lockStorage);

   if (UNLIKELY(lock == NULL)) {
      MXUserRWLock *newLock = MXUser_CreateRWLock(name, rank);

      lock = Atomic_ReadIfEqualWritePtr(lockStorage, NULL, (void *) newLock);

      if (lock) {
         MXUser_DestroyRWLock(newLock);
      } else {
         lock = Atomic_ReadPtr(lockStorage);
      }
   }

   return lock;
}

