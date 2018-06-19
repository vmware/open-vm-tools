/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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
#include "timeSyncPosix.h"

#include <errno.h>
#include <glib.h>
#include <string.h>
#include <time.h>
#include <sys/wait.h>
#include <sys/time.h>
#include "vm_assert.h"


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
 * TimeSync_IsGuestSyncServiceRunning --                                */ /**
 *
 * Check if the guest time sync service is running.
 *
 * @return TRUE if running and FALSE if not running or not implemented.
 *
 ******************************************************************************
 */

Bool
TimeSync_IsGuestSyncServiceRunning(void)
{
   // Not Implemented.
   return FALSE;
}


/*
 ******************************************************************************
 * TimeSync_DoGuestResync --                                            */ /**
 *
 * Issue a resync command to the guest time sync service.
 *
 * @return TRUE on success and FALSE on failure or if not implemented.
 *
 ******************************************************************************
 */

Bool
TimeSync_DoGuestResync(void *_ctx)
{
   // Not Implemented.
   return FALSE;
}
