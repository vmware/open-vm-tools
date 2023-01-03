/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * @file slewLinux.c
 *
 * Implementation of slewing using Linux's adjtimex system call to alter
 * the tick length.
 */

#include "timeSync.h"
#include "timeSyncPosix.h"

#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/timex.h>
#include "vm_assert.h"

/*
 * The interval between two ticks (in usecs) can only be altered by 10%,
 * and the default value is 10000. So the values 900000L and 1000000L
 * divided by USER_HZ, which is 100.
 */
#ifdef __linux__
#   define USER_HZ             100                  /* from asm/param.h  */
#   define TICK_INCR_NOMINAL  (1000000L / USER_HZ)  /* nominal tick increment */
#   define TICK_INCR_MAX      (1100000L / USER_HZ)  /* maximum tick increment */
#   define TICK_INCR_MIN      (900000L / USER_HZ)   /* minimum tick increment */
#endif


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
}


/*
 ******************************************************************************
 * TimeSync_Slew --                                                     */ /**
 *
 * Slew the clock so that the time difference is covered within the
 * timeSyncPeriod. timeSyncPeriod is the interval of the time sync loop
 * and we intend to catch up delta us.  Report the amount of the previous
 * correction that has not been applied (this may be negative if more than
 * timeSyncPeriod elapsed since the last call).
 *
 * This changes the tick frequency and hence needs to be reset after the time
 * sync is achieved.
 *
 * All times are in microseconds.
 *
 * @param[in]  delta              Correction to apply.
 * @param[in]  timeSyncPeriod     Time interval.
 * @param[out] remaining          Amount of previous correction not applied.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_Slew(int64 delta,
              int64 timeSyncPeriod,
              int64 *remaining)
{
   static int64 startTime = 0;
   static int64 tickLength;
   static int64 deltaRequested;

   struct timex tx;
   int error;
   int64 now;

   ASSERT(timeSyncPeriod > 0);

   if (!TimeSync_GetCurrentTime(&now)) {
      return FALSE;
   }

   if (startTime != 0) {
      int64 ticksElapsed = (now - startTime) / tickLength;
      int64 deltaApplied = ticksElapsed * (tickLength - TICK_INCR_NOMINAL);
      *remaining = deltaRequested - deltaApplied;
   }

   /*
    * Set the tick length so that delta time is corrected in
    * timeSyncPeriod period.  tick is the number of microseconds added per
    * clock tick. We adjust this so that we get the desired delta + the
    * timeSyncPeriod in timeSyncPeriod interval.
    */
   tickLength = 
      (timeSyncPeriod + delta) / ((timeSyncPeriod / US_PER_SEC) * USER_HZ);
   if (tickLength > TICK_INCR_MAX) {
      tickLength = TICK_INCR_MAX;
   } else if (tickLength < TICK_INCR_MIN) {
      tickLength = TICK_INCR_MIN;
   }
   tx.tick = tickLength;
   tx.modes = ADJ_TICK;

   ASSERT(delta != 0 || tickLength == TICK_INCR_NOMINAL);
   
   startTime = now;
   deltaRequested = delta;

   error = adjtimex(&tx);
   if (error == -1) {
      startTime = 0;
      g_debug("adjtimex failed: %s\n", strerror(errno));
      return FALSE;
   }
   g_debug("time slew start: %jd\n", (intmax_t)tx.tick);
   return TRUE;
}
