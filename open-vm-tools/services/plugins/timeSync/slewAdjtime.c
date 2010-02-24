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

/**
 * @file slewAdjtime.c
 *
 * Implementation of slewing using Posix adjtime system call.
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
 ******************************************************************************
 * TimeSync_DisableTimeSlew --                                          */ /**
 *
 * Disable time slewing, canceling any pending slew.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_DisableTimeSlew(void)
{
   struct timeval tx = {0};
   int error;

   error = adjtime(&tx, NULL);
   if (error == -1) {
      g_debug("adjtime failed: %s\n", strerror(errno));
      return FALSE;
   }
   return TRUE;
}


/*
 ******************************************************************************
 * TimeSync_EnableTimeSlew --                                           */ /**
 *
 * Slew the clock, correcting 'delta' microseconds.  timeSyncPeriod is
 * ignored by this implementation.
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
}
