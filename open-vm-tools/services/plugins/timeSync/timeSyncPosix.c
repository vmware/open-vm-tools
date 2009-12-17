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

/**
 * @file timeSyncPosix.c
 *
 * Implementation of time sync functions for POSIX systems.
 */

#include "timeSync.h"

#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#if !defined(__APPLE__)
#include <sys/timex.h>
#endif
#include "vm_assert.h"

/*
 * The interval between two ticks (in usecs) can only be altered by 10%,
 * and the default value is 10000. So the values 900000L and 1000000L
 * divided by USER_HZ, which is 100.
 */
#ifdef __linux__
#   define USER_HZ               100			/* from asm/param.h  */
#   define TICK_INCR_NOMINAL    (1000000L / USER_HZ)	/* nominal tick increment */
#   define TICK_INCR_MAX        (1100000L / USER_HZ)	/* maximum tick increment */
#   define TICK_INCR_MIN        (900000L / USER_HZ)	/* minimum tick increment */
#endif


/*
 ******************************************************************************
 * TimeSyncWriteTimeVal --                                              */ /**
 *
 * Convert time represented as microseconds, to a timeval.  This function
 * handles positive and negative values for "time."  For a timeval to be
 * valid tv_usec must be between 0 and 999999.  See
 * http://www.gnu.org/s/libc/manual/html_node/Elapsed-Time.html for more
 * details.
 *
 ******************************************************************************
 */

static INLINE void
TimeSyncWriteTimeVal(int64 time, struct timeval *tv)
{
   int64 sec = time / US_PER_SEC;
   int64 usec = time - sec * US_PER_SEC;
   if (usec < 0) {
      usec += US_PER_SEC;
      sec--;
   }
   ASSERT(0 <= usec && usec < US_PER_SEC &&
          time == sec * US_PER_SEC + usec);
   tv->tv_sec = sec;
   tv->tv_usec = usec;
}


/*
 ******************************************************************************
 * TimeSync_AddToCurrentTime --                                         */ /**
 *
 * Adjust the current system time by adding the given number of
 * microseconds. This function disables any time slewing to correctly set
 * the guest time.
 *
 * @param[in] delta    Microseconds to add.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_AddToCurrentTime(int64 delta)
{
   struct timeval tv;
   int64 newTime;
   int64 now;

   if (!TimeSync_GetCurrentTime(&now)) {
      return FALSE;
   }

   if (TimeSync_IsTimeSlewEnabled()) {
      TimeSync_DisableTimeSlew();
   }

   newTime = now + delta;
   ASSERT(newTime > 0);

   /*
    * timeval.tv_sec is a 32-bit signed integer. So, Linux will treat
    * newTime as a time before the epoch if newTime is a time 68 years
    * after the epoch (beacuse of overflow).
    *
    * If it is a 64-bit linux, everything should be fine.
    */
   if (sizeof tv.tv_sec < 8 && newTime / US_PER_SEC > MAX_INT32) {
      g_debug("overflow: delta=%"FMT64"d, now=%"FMT64"d\n", delta, now);
      return FALSE;
   }

   TimeSyncWriteTimeVal(newTime, &tv);

   if (settimeofday(&tv, NULL) < 0) {
      return FALSE;
   }

   return TRUE;
}


/*
 ******************************************************************************
 * TimeSync_DisableTimeSlew --                                          */ /**
 *
 * Disable time slewing, setting the tick frequency to default. If failed to
 * disable the tick frequency, system time will not reflect the actual time -
 * it will be behind.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_DisableTimeSlew(void)
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval tx = {0};
   int error;

   error = adjtime(&tx, NULL);
   if (error == -1) {
      g_debug("adjtime failed: %s\n", strerror(errno));
      return FALSE;
   }
   return TRUE;

#elif defined(__linux__) /* For Linux. */

   struct timex tx;
   int error;

   tx.modes = ADJ_TICK;
   tx.tick = TICK_INCR_NOMINAL;

   error = adjtimex(&tx);
   if (error == -1) {
      g_debug("adjtimex failed: %s\n", strerror(errno));
      return FALSE;
   }
   g_debug("time slew end\n");
   return TRUE;

#else /* Apple */
   return TRUE;

#endif
}


/*
 ******************************************************************************
 * TimeSync_EnableTimeSlew --                                           */ /**
 *
 * Slew the clock so that the time difference is covered within the
 * timeSyncPeriod. timeSyncPeriod is the interval of the time sync loop and we
 * intend to catch up delta us.
 *
 * timeSyncPeriod is ignored on FreeBSD and Solaris.
 *
 * This changes the tick frequency and hence needs to be reset after the time
 * sync is achieved.
 *
 * @param[in] delta              Time difference in us.
 * @param[in] timeSyncPeriod     Time interval in us.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_EnableTimeSlew(int64 delta,
                        int64 timeSyncPeriod)
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval tx;
   struct timeval oldTx;
   int error;

   TimeSyncWriteTimeVal(delta, &tx);

   error = adjtime(&tx, &oldTx);
   if (error == -1) {
      g_debug("adjtime failed: %s\n", strerror(errno));
      return FALSE;
   }
   g_debug("time slew start.\n");
   return TRUE;

#elif defined(__linux__) /* For Linux. */

   struct timex tx;
   int error;
   int64 tick;

   ASSERT(timeSyncPeriod > 0);

   /*
    * Set the tick so that delta time is corrected in timeSyncPeriod period.
    * tick is the number of microseconds added per clock tick. We adjust this
    * so that we get the desired delta + the timeSyncPeriod in timeSyncPeriod
    * interval.
    */
   tx.modes = ADJ_TICK;
   tick = (timeSyncPeriod + delta) /
          ((timeSyncPeriod / US_PER_SEC) * USER_HZ);
   if (tick > TICK_INCR_MAX) {
      tick = TICK_INCR_MAX;
   } else if (tick < TICK_INCR_MIN) {
      tick = TICK_INCR_MIN;
   }
   tx.tick = tick;

   error = adjtimex(&tx);
   if (error == -1) {
      g_debug("adjtimex failed: %s\n", strerror(errno));
      return FALSE;
   }
   g_debug("time slew start: %ld\n", tx.tick);
   return TRUE;

#else /* Apple */

   return FALSE;

#endif
}


/*
 ******************************************************************************
 * TimeSync_GetCurrentTime --                                           */ /**
 *
 * Get the system time in seconds & microseconds.
 *
 * @param[in] secs      Where to store the number of seconds.
 * @param[in] usecs     Where to store the number of microseconds.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_GetCurrentTime(int64 *now)
{
   struct timeval tv;

   ASSERT(now);

   if (gettimeofday(&tv, NULL) < 0) {
      return FALSE;
   }

   *now = (int64)tv.tv_sec * US_PER_SEC + (int64)tv.tv_usec;

   return TRUE;
}


/*
 ******************************************************************************
 * TimeSync_IsTimeSlewEnabled --                                        */ /**
 *
 * Returns TRUE if time slewing has been enabled.
 *
 * @return Whether time slew is enabled.
 *
 ******************************************************************************
 */

Bool
TimeSync_IsTimeSlewEnabled(void)
{
#if defined(__FreeBSD__) || defined(sun)

   struct timeval oldTx;
   int error;

   /*
    * Solaris needs first argument non-NULL and zero
    * to get the old timeval value.
    */
#if defined(sun)
   struct timeval tx = {0};
   error = adjtime(&tx, &oldTx);
#else
   error = adjtime(NULL, &oldTx);
#endif
   if (error == -1) {
      g_debug("adjtime failed: %s.\n", strerror(errno));
      return FALSE;
   }
   return ((oldTx.tv_sec || oldTx.tv_usec) ? TRUE : FALSE);

#elif defined(__linux__) /* For Linux. */

   struct timex tx = {0};
   int error;

   error = adjtimex(&tx);
   if (error == -1) {
      g_debug("adjtimex failed: %s\n", strerror(errno));
      return FALSE;
   }
   return ((tx.tick == TICK_INCR_NOMINAL) ? FALSE : TRUE);

#else /* Apple */

   return FALSE;

#endif
}

