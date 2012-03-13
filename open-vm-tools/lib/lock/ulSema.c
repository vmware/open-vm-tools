/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
#include <mach/mach_init.h>
#include <mach/task.h>
#include <mach/semaphore.h>
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
#include "win32u.h"
#endif

#define MXUSER_A_BILLION (1000 * 1000 * 1000)

#if defined(_WIN32)
typedef HANDLE NativeSemaphore;
#else
#if defined(__APPLE__)
typedef semaphore_t NativeSemaphore;
#else
typedef sem_t NativeSemaphore;
#endif
#endif

typedef struct
{
   MXUserAcquisitionStats  acquisitionStats;
   Atomic_Ptr              acquisitionHisto;
} MXUserStats;

struct MXUserSemaphore
{
   MXUserHeader     header;
   Atomic_uint32    activeUserCount;
   NativeSemaphore  nativeSemaphore;
   Atomic_Ptr       statsMem;
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
MXUserInit(NativeSemaphore *sema)  // IN:
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
                uint32 msecWait,        // IN:
                Bool *downOccurred)     // OUT:
{
    int err;
    DWORD status;

    status = WaitForSingleObject(*sema, msecWait);

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
MXUserInit(NativeSemaphore *sema)  // IN:
{
   return semaphore_create(mach_task_self(), sema, SYNC_POLICY_FIFO, 0);
}

static int
MXUserDestroy(NativeSemaphore *sema)  // IN:
{
   return semaphore_destroy(mach_task_self(), *sema);
}

static int
MXUserTimedDown(NativeSemaphore *sema,  // IN:
                uint32 msecWait,        // IN:
                Bool *downOccurred)     // OUT:
{
   uint64 nsecWait;
   VmTimeType before;
   kern_return_t err;

   ASSERT_ON_COMPILE(KERN_SUCCESS == 0);

   /*
    * Work in nanoseconds. Time the semaphore_timedwait operation in case
    * it is interrupted (KERN_ABORT). If it is, determine how much time is
    * necessary to fulfill the specified wait time and retry with a new
    * and appropriate timeout.
    */

   nsecWait = 1000000ULL * (uint64) msecWait;
   before = Hostinfo_SystemTimerNS();

   do {
      VmTimeType after;
      mach_timespec_t ts;

      ts.tv_sec = nsecWait / MXUSER_A_BILLION;
      ts.tv_nsec = nsecWait % MXUSER_A_BILLION;

      err = semaphore_timedwait(*sema, ts);
      after = Hostinfo_SystemTimerNS();

      if (err == KERN_SUCCESS) {
         *downOccurred = TRUE;
      } else {
         *downOccurred = FALSE;

         if (err == KERN_OPERATION_TIMED_OUT) {
            /* Really timed out; no down occurred, no error */
            err = KERN_SUCCESS;
         } else {
            if (err == KERN_ABORTED) {
               VmTimeType duration = after - before;

               if (duration < nsecWait) {
                  nsecWait -= duration;

                  before = after;
               } else {
                  err = KERN_SUCCESS;  // "timed out" anyway... no error
               }
            }
         }
      }
   } while (nsecWait && (err == KERN_ABORTED));

   return err;
}

static int
MXUserDown(NativeSemaphore *sema)  // IN:
{
   return semaphore_wait(*sema);
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
    return semaphore_signal(*sema);
}

#else
static int
MXUserInit(NativeSemaphore *sema)  // IN:
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
                uint32 msecWait,        // IN:
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
           ((uint64) msecWait * (1000 * 1000));

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
       * If the error that occured indicates that the try operation cannot
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
   MXUserStats *stats = (MXUserStats *) Atomic_ReadPtr(&sema->statsMem);

   if (stats) {
      Bool isHot;
      Bool doLog;
      double contentionRatio;

      /*
       * Dump the statistics for the specified semaphore.
       */

      MXUserDumpAcquisitionStats(&stats->acquisitionStats, header);

      if (Atomic_ReadPtr(&stats->acquisitionHisto) != NULL) {
         MXUserHistoDump(Atomic_ReadPtr(&stats->acquisitionHisto), header);
      }

      /*
       * Has the semaphore gone "hot"? If so, implement the hot actions.
       */

      MXUserKitchen(&stats->acquisitionStats, &contentionRatio, &isHot,
                    &doLog);

      if (isHot) {
         MXUserForceHisto(&stats->acquisitionHisto,
                          MXUSER_STAT_CLASS_ACQUISITION,
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
   Warning("\tserial number %u\n", sema->header.serialNumber);

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
   MXUserSemaphore *sema;

   sema = Util_SafeCalloc(1, sizeof(*sema));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "Sema-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   if (LIKELY(MXUserInit(&sema->nativeSemaphore) == 0)) {
      Bool doStats;
      MXUserStats *stats;

      sema->header.signature = MXUserGetSignature(MXUSER_TYPE_SEMA);
      sema->header.name = properName;
      sema->header.rank = rank;
      sema->header.serialNumber = MXUserAllocSerialNumber();
      sema->header.dumpFunc = MXUserDumpSemaphore;

      if (vmx86_stats) {
         doStats = MXUserStatsEnabled();
      } else {
         doStats = FALSE;
      }

      if (doStats) {
         sema->header.statsFunc = MXUserStatsActionSema;

         stats = Util_SafeCalloc(1, sizeof(*stats));

         MXUserAcquisitionStatsSetUp(&stats->acquisitionStats);

         Atomic_WritePtr(&sema->statsMem, stats);
      } else {
         sema->header.statsFunc = NULL;
         Atomic_WritePtr(&sema->statsMem, NULL);
      }

      MXUserAddToList(&sema->header);
   } else {
      free(properName);
      free(sema);
      sema = NULL;
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
MXUser_DestroySemaphore(MXUserSemaphore *sema)  // IN:
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
         MXUserStats *stats = Atomic_ReadPtr(&sema->statsMem);

         if (LIKELY(stats != NULL)) {
            MXUserAcquisitionStatsTearDown(&stats->acquisitionStats);
            MXUserHistoTearDown(Atomic_ReadPtr(&stats->acquisitionHisto));

            free(stats);
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
 *      The count will be decremented; a sleep may occur until the decement
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
      MXUserStats *stats = Atomic_ReadPtr(&sema->statsMem);

      if (LIKELY(stats != NULL)) {
         start = Hostinfo_SystemTimerNS();
      }

      err = MXUserTryDown(&sema->nativeSemaphore, &tryDownSuccess);

      if (LIKELY(err == 0)) {
         if (!tryDownSuccess) {
            err = MXUserDown(&sema->nativeSemaphore);
         }
      }

      if (LIKELY((err == 0) && (stats != NULL))) {
         MXUserHisto *histo;
         VmTimeType value = Hostinfo_SystemTimerNS() - start;

         MXUserAcquisitionSample(&stats->acquisitionStats, TRUE,
                                 !tryDownSuccess, value);

         histo = Atomic_ReadPtr(&stats->acquisitionHisto);

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
                          uint32 msecWait)        // IN:
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
      MXUserStats *stats = Atomic_ReadPtr(&sema->statsMem);

      if (LIKELY(stats != NULL)) { 
         start = Hostinfo_SystemTimerNS();
      }

      err = MXUserTryDown(&sema->nativeSemaphore, &tryDownSuccess);

      if (LIKELY(err == 0)) {
         if (tryDownSuccess) {
            downOccurred = TRUE;
         } else {
            err = MXUserTimedDown(&sema->nativeSemaphore, msecWait,
                                  &downOccurred);
         }
      }

      if (LIKELY((err == 0) && (stats != NULL))) {
         VmTimeType value = Hostinfo_SystemTimerNS() - start;

         MXUserAcquisitionSample(&stats->acquisitionStats, downOccurred,
                                 !tryDownSuccess, value);

         if (downOccurred) {
            MXUserHisto *histo = Atomic_ReadPtr(&stats->acquisitionHisto);

            if (UNLIKELY(histo != NULL)) {
               MXUserHistoSample(histo, value, GetReturnAddress());
            }
         }
      }
   } else {
      err = MXUserTimedDown(&sema->nativeSemaphore, msecWait, &downOccurred);
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
 *      This duplicates the behavor of MX semaphores.
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
      MXUserStats *stats = Atomic_ReadPtr(&sema->statsMem);

      if (LIKELY(stats != NULL)) {
         MXUserAcquisitionSample(&stats->acquisitionStats, downOccurred,
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

   Atomic_Inc(&sema->activeUserCount);

   err = MXUserUp(&sema->nativeSemaphore);

   if (UNLIKELY(err != 0)) {
      MXUserDumpAndPanic(&sema->header, "%s: Internal error (%d)\n",
                         __FUNCTION__, err);
   }

   Atomic_Dec(&sema->activeUserCount);
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

