/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
#else
#if defined(__APPLE__)
#include <dispatch/dispatch.h>
#else
#if (_XOPEN_SOURCE < 600) && !defined(__FreeBSD__) && !defined(sun)
#undef _XOPEN_SOURCE
#define _XOPEN_SOURCE 600
#endif
#include <semaphore.h>
#include <sys/time.h>
#endif
#endif

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"
#include "hostinfo.h"
#if defined(_WIN32)
#include "windowsu.h"
#endif

#define MXUSER_A_BILLION (1000 * 1000 * 1000)

#if defined(_WIN32)
typedef HANDLE NativeSemaphore;
#else
#if defined(__APPLE__)
/*
 * The Mac OS implementation uses dispatch_semaphore_t instead of
 * semaphore_t due to better performance in the uncontended case, by
 * avoiding a system call.  It, also, avoids weird error cases that
 * resulted in bug 916600.
 */
typedef dispatch_semaphore_t NativeSemaphore;
#else
typedef sem_t NativeSemaphore;
#endif
#endif

struct MXUserSemaphore
{
   MXUserHeader     header;
   Atomic_uint32    activeUserCount;
   NativeSemaphore  nativeSemaphore;
   Atomic_Ptr       acquireStatsMem;
};


/*
 *-----------------------------------------------------------------------------
 *
 * Environment specific implementations of portable semaphores.
 *
 * All of these functions return zero (0) for success and non-zero upon
 * failure. The non-zero value is a host specified error code.
 *
 * All down operations return a boolean which indicates if the down
 * operation actually occurred (the counting variable was decremented;
 * a sleep may have occurred). This boolean is valid regardless of the
 * value returned from the down functions.
 *
 * Timed operations always wait for the length of time specified. Should
 * the native system allow interruptions/signals, retries will be performed
 * until the specified amount of time has elapsed.
 *
 * There are 6 environment specific primitives:
 *
 * MXUserInit       Initialize a native semaphore
 * MXUserDestroy    Destroy a native semaphore
 * MXUserDown       Perform a down (P) operation
 * MXUserTimedDown  Perform a down (P) operation with a timeout
 * MXUserTryDown    Perform a try down (P) operation
 * MXUserUp         Perform an up (V) operation
 *
 *-----------------------------------------------------------------------------
 */

#if defined(_WIN32)
static int
MXUserInit(NativeSemaphore *sema)  // OUT:
{
   *sema = Win32U_CreateSemaphore(NULL, 0, INT_MAX, NULL);

   return (*sema == NULL) ? GetLastError() : 0;
}

static int
MXUserDestroy(NativeSemaphore *sema)  // IN:
{
   return CloseHandle(*sema) ? 0 : GetLastError();
}

static int
MXUserTimedDown(NativeSemaphore *sema,  // IN:
                uint32 waitTimeMsec,    // IN:
                Bool *downOccurred)     // OUT:
{
    int err;
    DWORD status;

    status = WaitForSingleObject(*sema, waitTimeMsec);

    switch (status) {
       case WAIT_OBJECT_0:  // The down (decrement) occurred
          *downOccurred = TRUE;
          err = 0;
          break;

       case WAIT_TIMEOUT:  // Timed out; the down (decrement) did not occur
          *downOccurred = FALSE;
          err = 0;
          break;

       default:  // Something really terrible has happened...
          Panic("%s: WaitForSingleObject return value %x\n",
                __FUNCTION__, status);
    }

    return err;
}

static int
MXUserDown(NativeSemaphore *sema)  // IN:
{
   int err;
   Bool downOccurred;

   /*
    * Use an infinite timed wait to implement down. If the timed down
    * succeeds, assert that the down actually occurred.
    */

   err = MXUserTimedDown(sema, INFINITE, &downOccurred);

   if (err == 0) {
      ASSERT(downOccurred);
   }

   return err;
}

static int
MXUserTryDown(NativeSemaphore *sema,  // IN:
              Bool *downOccurred)     // OUT:
{
   /*
    * Use a wait for zero time to implement the try operation. This timed
    * down will either succeed immediately (down occurred), fail (something
    * terrible happened) or time out immediately (the down could not be
    * performed and that is OK).
    */

   return MXUserTimedDown(sema, 0, downOccurred);
}

static int
MXUserUp(NativeSemaphore *sema)  // IN:
{
   return ReleaseSemaphore(*sema, 1, NULL) ? 0 : GetLastError();
}

#elif defined(__APPLE__)
static int
MXUserInit(NativeSemaphore *sema)  // OUT:
{
   *sema = dispatch_semaphore_create(0);
   return *sema == NULL;
}

static int
MXUserDestroy(NativeSemaphore *sema)  // IN:
{
   dispatch_release(*sema);
   return 0;
}

static int
MXUserTimedDown(NativeSemaphore *sema,  // IN:
                uint32 waitTimeMsec,    // IN:
                Bool *downOccurred)     // OUT:
{
   int64 nsecWait = 1000000LL * (int64)waitTimeMsec;
   dispatch_time_t deadline = dispatch_time(DISPATCH_TIME_NOW, nsecWait);
   *downOccurred = dispatch_semaphore_wait(*sema, deadline) == 0;
   return 0;
}

static int
MXUserDown(NativeSemaphore *sema)  // IN:
{
   dispatch_semaphore_wait(*sema, DISPATCH_TIME_FOREVER);
   return 0;
}

static int
MXUserTryDown(NativeSemaphore *sema,  // IN:
              Bool *downOccurred)     // OUT:
{
   /*
    * Provide 'try' semantics by requesting an immediate timeout.
    */
   *downOccurred = dispatch_semaphore_wait(*sema, DISPATCH_TIME_NOW) == 0;
   return 0;
}

static int
MXUserUp(NativeSemaphore *sema)  // IN:
{
    dispatch_semaphore_signal(*sema);
    return 0;
}

#else
static int
MXUserInit(NativeSemaphore *sema)  // OUT:
{
   return (sem_init(sema, 0, 0) == -1) ? errno : 0;
}

static int
MXUserDestroy(NativeSemaphore *sema)  // IN:
{
   return (sem_destroy(sema) == -1) ? errno : 0;
}

static int
MXUserDown(NativeSemaphore *sema)  // IN:
{
   int err;

   /* Retry any interruptions (EINTR) */
   do {
      err = (sem_wait(sema) == -1) ? errno : 0;
   } while (err == EINTR);

   return err;
}

static int
MXUserTimedDown(NativeSemaphore *sema,  // IN:
                uint32 waitTimeMsec,    // IN:
                Bool *downOccurred)     // OUT:
{
   int err;
   uint64 endNS;
   struct timeval curTime;
   struct timespec endTime;

   /*
    * sem_timedwait takes an absolute time. Yes, this is beyond ridiculous,
    * and the justifications for this vs. relative time makes no sense, but
    * it is what it is...
    */

   gettimeofday(&curTime, NULL);
   endNS = ((uint64) curTime.tv_sec * MXUSER_A_BILLION) +
           ((uint64) curTime.tv_usec * 1000) +
           ((uint64) waitTimeMsec * (1000 * 1000));

   endTime.tv_sec = (time_t) (endNS / MXUSER_A_BILLION);
   endTime.tv_nsec = (long int) (endNS % MXUSER_A_BILLION);

   do {
      err = (sem_timedwait(sema, &endTime) == -1) ? errno : 0;

      if (err == 0) {
         *downOccurred = TRUE;
      } else {
         *downOccurred = FALSE;

         /* Really timed out; no down occurred, no error */
         if (err == ETIMEDOUT) {
            err = 0;
         }
      }
   } while (err == EINTR);

   return err;
}

static int
MXUserTryDown(NativeSemaphore *sema,  // IN:
              Bool *downOccurred)     // OUT:
{
   int err = (sem_trywait(sema) == -1) ? errno : 0;

   if (err == 0) {
      *downOccurred = TRUE;
   } else {
      *downOccurred = FALSE;

      /*
       * If the error that occurred indicates that the try operation cannot
       * succeed (EAGAIN) or was interrupted (EINTR) suppress the error
       * indicator as these are considered "normal", non-error cases.
       *
       * It's OK to not loop on EINTR here since this is a try operation.
       */

      if ((err == EAGAIN) || (err == EINTR)) {
         err = 0;  // no error
      }
   }

   return err;
}

static int
MXUserUp(NativeSemaphore *sema)  // IN:
{
   return (sem_post(sema) == -1) ? errno : 0;
}
#endif  // _WIN32


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserStatsActionSema --
 *
 *      Perform the statistics action for the specified semaphore.
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
MXUserStatsActionSema(MXUserHeader *header)  // IN:
{
   MXUserSemaphore *sema = (MXUserSemaphore *) header;
   MXUserAcquireStats *acquireStats = Atomic_ReadPtr(&sema->acquireStatsMem);

   if (LIKELY(acquireStats != NULL)) {
      Bool isHot;
      Bool doLog;
      double contentionRatio;

      /*
       * Dump the statistics for the specified semaphore.
       */

      MXUserDumpAcquisitionStats(&acquireStats->data, header);

      if (Atomic_ReadPtr(&acquireStats->histo) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&acquireStats->histo), header);
      }

      /*
       * Has the semaphore gone "hot"? If so, implement the hot actions.
       */

      MXUserKitchen(&acquireStats->data, &contentionRatio, &isHot, &doLog);

      if (isHot) {
         MXUserForceAcquisitionHisto(&sema->acquireStatsMem,
                                     MXUSER_DEFAULT_HISTO_MIN_VALUE_NS,
                                     MXUSER_DEFAULT_HISTO_DECADES);

         if (doLog) {
            Log("HOT SEMAPHORE (%s); contention ratio %f\n",
                sema->header.name, contentionRatio);
         }
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpSemaphore --
 *
 *      Dump a semaphore.
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
MXUserDumpSemaphore(MXUserHeader *header)  // IN:
{
   MXUserSemaphore *sema = (MXUserSemaphore *) header;

   Warning("%s: semaphore @ %p\n", __FUNCTION__, sema);

   Warning("\tsignature 0x%X\n", sema->header.signature);
   Warning("\tname %s\n", sema->header.name);
   Warning("\trank 0x%X\n", sema->header.rank);
   Warning("\tserial number %"FMT64"u\n", sema->header.serialNumber);

   Warning("\treference count %u\n", Atomic_Read(&sema->activeUserCount));
   Warning("\taddress of native semaphore %p\n", &sema->nativeSemaphore);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSemaphore --
 *
 *      Create a (counting) semaphore.
 *
 *      The initial count of the semaphore is zero (0). The maximum count that
 *      a semaphore handles is unknown but may be assumed to be large; the
 *      largest signed 32-bit number is an acceptable choice.
 *
 * Results:
 *      A pointer to a semaphore.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserSemaphore *
MXUser_CreateSemaphore(const char *userName,  // IN:
                       MX_Rank rank)          // IN:
{
   char *properName;
   MXUserSemaphore *sema = Util_SafeCalloc(1, sizeof *sema);

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "Sema-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   if (LIKELY(MXUserInit(&sema->nativeSemaphore) == 0)) {
      uint32 statsMode;

      sema->header.signature = MXUserGetSignature(MXUSER_TYPE_SEMA);
      sema->header.name = properName;
      sema->header.rank = rank;
      sema->header.serialNumber = MXUserAllocSerialNumber();
      sema->header.dumpFunc = MXUserDumpSemaphore;

      statsMode = MXUserStatsMode();

      switch (MXUserStatsMode()) {
      case 0:
         MXUserDisableStats(&sema->acquireStatsMem, NULL);
         sema->header.statsFunc = NULL;
         break;

      case 1:
      case 2:
         MXUserEnableStats(&sema->acquireStatsMem, NULL);
         sema->header.statsFunc = MXUserStatsActionSema;
         break;

      default:
         Panic("%s: unknown stats mode: %d!\n", __FUNCTION__, statsMode);
      }

      MXUserAddToList(&sema->header);
   } else {
      Panic("%s: native lock initialization routine failed\n", __FUNCTION__);
   }

   return sema;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroySemaphore --
 *
 *      Destroy a semaphore
 *
 * Results:
 *      The semaphore is destroyed. Don't try to use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroySemaphore(MXUserSemaphore *sema)  // IN/OUT:
{
   if (LIKELY(sema != NULL)) {
      int err;

      MXUserValidateHeader(&sema->header, MXUSER_TYPE_SEMA);

      if (Atomic_Read(&sema->activeUserCount) != 0) {
         MXUserDumpAndPanic(&sema->header,
                            "%s: Attempted destroy on semaphore while in use\n",
                            __FUNCTION__);
      }

      sema->header.signature = 0;  // just in case...

      err = MXUserDestroy(&sema->nativeSemaphore);

      if (UNLIKELY(err != 0)) {
         MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                            __FUNCTION__, err);
      }

      MXUserRemoveFromList(&sema->header);

      if (vmx86_stats) {
         MXUserAcquireStats *acquireStats;

         acquireStats = Atomic_ReadPtr(&sema->acquireStatsMem);

         if (LIKELY(acquireStats != NULL)) {
            MXUserAcquisitionStatsTearDown(&acquireStats->data);
            MXUserHistoTearDown(Atomic_ReadPtr(&acquireStats->histo));

            free(acquireStats);
         }
      }

      free(sema->header.name);
      sema->header.name = NULL;
      free(sema);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DownSemaphore --
 *
 *      Perform a down (P; probeer te verlagen; "try to reduce") operation
 *      on a semaphore.
 *
 * Results:
 *      The count will be decremented; a sleep may occur until the decrement
 *      is possible.
 *
 * Side effects:
 *      The caller may sleep.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DownSemaphore(MXUserSemaphore *sema)  // IN/OUT:
{
   int err;

   ASSERT(sema);
   MXUserValidateHeader(&sema->header, MXUSER_TYPE_SEMA);

   Atomic_Inc(&sema->activeUserCount);

   MXUserAcquisitionTracking(&sema->header, TRUE);  // rank checking

   if (vmx86_stats) {
      VmTimeType start = 0;
      Bool tryDownSuccess = FALSE;
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&sema->acquireStatsMem);

      if (LIKELY(acquireStats != NULL)) {
         start = Hostinfo_SystemTimerNS();
      }

      err = MXUserTryDown(&sema->nativeSemaphore, &tryDownSuccess);

      if (LIKELY(err == 0)) {
         if (!tryDownSuccess) {
            err = MXUserDown(&sema->nativeSemaphore);
         }
      }

      if (LIKELY((err == 0) && (acquireStats != NULL))) {
         MXUserHisto *histo;
         VmTimeType value = Hostinfo_SystemTimerNS() - start;

         MXUserAcquisitionSample(&acquireStats->data, TRUE,
                                 !tryDownSuccess, value);

         histo = Atomic_ReadPtr(&acquireStats->histo);

         if (UNLIKELY(histo != NULL)) {
            MXUserHistoSample(histo, value, GetReturnAddress());
         }
      }
   } else {
      err = MXUserDown(&sema->nativeSemaphore);
   }

   if (UNLIKELY(err != 0)) {
      MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                         __FUNCTION__, err);
   }

   MXUserReleaseTracking(&sema->header);

   Atomic_Dec(&sema->activeUserCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TimedDownSemaphore --
 *
 *      Perform a down (P; probeer te verlagen; "try to reduce") operation
 *      on a semaphore with a timeout. The wait time will always have elapsed
 *      before the routine returns.
 *
 * Results:
 *      TRUE   Down operation occurred (count has been decremented)
 *      FALSE  Down operation did not occur (time out occurred)
 *
 * Side effects:
 *      The caller may sleep.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_TimedDownSemaphore(MXUserSemaphore *sema,  // IN/OUT:
                          uint32 waitTimeMsec)    // IN:
{
   int err;
   Bool downOccurred = FALSE;

   ASSERT(sema);
   MXUserValidateHeader(&sema->header, MXUSER_TYPE_SEMA);

   Atomic_Inc(&sema->activeUserCount);

   MXUserAcquisitionTracking(&sema->header, TRUE);  // rank checking

   if (vmx86_stats) {
      VmTimeType start = 0;
      Bool tryDownSuccess = FALSE;
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&sema->acquireStatsMem);

      if (LIKELY(acquireStats != NULL)) {
         start = Hostinfo_SystemTimerNS();
      }

      err = MXUserTryDown(&sema->nativeSemaphore, &tryDownSuccess);

      if (LIKELY(err == 0)) {
         if (tryDownSuccess) {
            downOccurred = TRUE;
         } else {
            err = MXUserTimedDown(&sema->nativeSemaphore, waitTimeMsec,
                                  &downOccurred);
         }
      }

      if (LIKELY((err == 0) && (acquireStats != NULL))) {
         VmTimeType value = Hostinfo_SystemTimerNS() - start;

         MXUserAcquisitionSample(&acquireStats->data, downOccurred,
                                 !tryDownSuccess, value);

         if (downOccurred) {
            MXUserHisto *histo = Atomic_ReadPtr(&acquireStats->histo);

            if (UNLIKELY(histo != NULL)) {
               MXUserHistoSample(histo, value, GetReturnAddress());
            }
         }
      }
   } else {
      err = MXUserTimedDown(&sema->nativeSemaphore, waitTimeMsec,
                            &downOccurred);
   }

   if (UNLIKELY(err != 0)) {
      MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                         __FUNCTION__, err);
   }

   MXUserReleaseTracking(&sema->header);

   Atomic_Dec(&sema->activeUserCount);

   return downOccurred;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_TryDownSemaphore --
 *
 *      Perform a try down (P; probeer te verlagen; "try to reduce") operation
 *      on a semaphore.
 *
 * Results:
 *      TRUE   Down operation occurred (count has been decremented)
 *      FALSE  Down operation did not occur
 *
 * Side effects:
 *      None
 *
 * NOTE:
 *      A "TryAcquire" does not rank check should the down operation succeed.
 *      This duplicates the behavior of MX semaphores.
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUser_TryDownSemaphore(MXUserSemaphore *sema)  // IN/OUT:
{
   int err;
   Bool downOccurred = FALSE;

   ASSERT(sema);
   MXUserValidateHeader(&sema->header, MXUSER_TYPE_SEMA);

   Atomic_Inc(&sema->activeUserCount);

   err = MXUserTryDown(&sema->nativeSemaphore, &downOccurred);

   if (UNLIKELY(err != 0)) {
      MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                         __FUNCTION__, err);
   }

   if (vmx86_stats) {
      MXUserAcquireStats *acquireStats;

      acquireStats = Atomic_ReadPtr(&sema->acquireStatsMem);

      if (LIKELY(acquireStats != NULL)) {
         MXUserAcquisitionSample(&acquireStats->data, downOccurred,
                                 !downOccurred, 0ULL);
      }
   }

   Atomic_Dec(&sema->activeUserCount);

   return downOccurred;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_UpSemaphore --
 *
 *      Perform an up (V; verhogen; "increase") operation on a semaphore.
 *
 * Results:
 *      The semaphore count is incremented. Any thread waiting on the
 *      semaphore is awoken.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_UpSemaphore(MXUserSemaphore *sema)  // IN/OUT:
{
   int err;

   ASSERT(sema);
   MXUserValidateHeader(&sema->header, MXUSER_TYPE_SEMA);

   /*
    * The purpose of the activeUserCount tracking is to help catch potentially
    * fatal cases of destroying an active semaphore; it is not expected to be
    * (nor it can be) perfect (with low overhead). In this case the time spent
    * in up is tiny and a decrement at the bottom might not be reached before
    * another thread comes out of down and does a destroy - so no
    * activeUserCount tracking here.
    */

   err = MXUserUp(&sema->nativeSemaphore);

   if (UNLIKELY(err != 0)) {
      MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                         __FUNCTION__, err);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonSemaphore --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      semaphore. This is useful for modules that need to protect something
 *      with a semaphore but don't have an existing Init() entry point where a
 *      semaphore can be created.
 *
 * Results:
 *      A pointer to the requested semaphore.
 *
 * Side effects:
 *      Generally the semaphore's resources are intentionally leaked
 *      (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserSemaphore *
MXUser_CreateSingletonSemaphore(Atomic_Ptr *semaStorage,  // IN/OUT:
                                const char *name,         // IN:
                                MX_Rank rank)             // IN:
{
   MXUserSemaphore *sema;

   ASSERT(semaStorage);

   sema = Atomic_ReadPtr(semaStorage);

   if (UNLIKELY(sema == NULL)) {
      MXUserSemaphore *newSema = MXUser_CreateSemaphore(name, rank);

      sema = Atomic_ReadIfEqualWritePtr(semaStorage, NULL, (void *) newSema);

      if (sema) {
         MXUser_DestroySemaphore(newSema);
      } else {
         sema = Atomic_ReadPtr(semaStorage);
      }
   }

   return sema;
}

