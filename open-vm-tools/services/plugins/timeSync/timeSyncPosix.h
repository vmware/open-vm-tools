/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
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

#ifndef _TIMESYNC_POSIX_H_
#define _TIMESYNC_POSIX_H_

/**
 * @file timeSyncPosix.h
 *
 * Posix specific functions and definitions related to syncing time.
 */

#include "timeSync.h"
#include "vm_assert.h"
#include <sys/time.h>

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

#endif /* _TIMESYNC_INT_H_ */

