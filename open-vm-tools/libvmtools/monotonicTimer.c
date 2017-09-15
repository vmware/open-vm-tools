/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * @file monotonicTimer.c
 *
 * A GSource that implements a timer backed by a monotonic time source.
 */

#include <limits.h>
#include "vmware.h"
#include "system.h"
#include "vmware/tools/utils.h"

typedef struct MTimerSource {
   GSource     src;
   gint        timeout;
   uint64      last;
} MTimerSource;


/*
 *******************************************************************************
 * MTimerSourcePrepare --                                                 */ /**
 *
 * Callback for the "prepare()" event source function. Sets the timeout to
 * the number of milliseconds this timer expects to sleep for. If the timeout
 * has already expired, update the internal state tracking the last time the
 * timer was fired.
 *
 * @param[in]  src         The source.
 * @param[out] timeout     Where to store the timeout.
 *
 * @return TRUE if timeout has already expired.
 *
 *******************************************************************************
 */

static gboolean
MTimerSourcePrepare(GSource *src,
                    gint *timeout)
{
   MTimerSource *timer = (MTimerSource *) src;

   if (timer->timeout == 0) {
      *timeout = 0;
      return TRUE;
   } else {
         uint64 now = System_GetTimeMonotonic() * 10;
         uint64 diff;

         ASSERT(now >= timer->last);

         diff = now - timer->last;
         if (diff >= timer->timeout) {
            timer->last = now;
            *timeout = 0;
            return TRUE;
         }

      *timeout = MIN(INT_MAX, timer->timeout - diff);
      return FALSE;
   }
}


/*
 *******************************************************************************
 * MTimerSourceCheck --                                                   */ /**
 *
 * Checks whether the timeout has expired.
 *
 * @param[in]  src     The source.
 *
 * @return Whether the timeout has expired.
 *
 *******************************************************************************
 */

static gboolean
MTimerSourceCheck(GSource *src)
{
   gint unused;
   return MTimerSourcePrepare(src, &unused);
}


/*
 *******************************************************************************
 * MTimerSourceDispatch --                                                */ /**
 *
 * Calls the callback associated with the timer, if any.
 *
 * @param[in]  src         Unused.
 * @param[in]  callback    The callback to be called.
 * @param[in]  data        User-supplied data.
 *
 * @return The return value of the callback, or FALSE if the callback is NULL.
 *
 *******************************************************************************
 */

static gboolean
MTimerSourceDispatch(GSource *src,
                     GSourceFunc callback,
                     gpointer data)
{
   return (callback != NULL) ? callback(data) : FALSE;
}


/*
 *******************************************************************************
 * MTimerSourceFinalize --                                                */ /**
 *
 * Does nothing. The main glib code already does all the cleanup needed.
 *
 * @param[in]  src     The source.
 *
 *******************************************************************************
 */

static void
MTimerSourceFinalize(GSource *src)
{
}


/**
 *
 * @addtogroup vmtools_utils
 * @{
 */

/*
 *******************************************************************************
 * VMTools_CreateTimer --                                                 */ /**
 *
 * @brief Create a timer based on a monotonic clock source.
 *
 * This timer differs from the glib timeout source, which uses the system time.
 * It is recommended for code that needs more reliable time tracking, using a
 * clock that is not affected by changes in the system time (which can happen
 * when using NTP or the Tools time synchronization feature).
 *
 * @param[in] timeout   The timeout for the timer, must be >= 0.
 *
 * @return The new source.
 *
 *******************************************************************************
 */

GSource *
VMTools_CreateTimer(gint timeout)
{
   static GSourceFuncs srcFuncs = {
      MTimerSourcePrepare,
      MTimerSourceCheck,
      MTimerSourceDispatch,
      MTimerSourceFinalize,
      NULL,
      NULL
   };
   MTimerSource *ret;

   ASSERT(timeout >= 0);

   ret = (MTimerSource *) g_source_new(&srcFuncs, sizeof *ret);
   ret->last = System_GetTimeMonotonic() * 10;
   ret->timeout = timeout;

   return &ret->src;
}

/** @}  */

