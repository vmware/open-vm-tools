/*********************************************************
 * Copyright (C) 2012 VMware, Inc. All rights reserved.
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

/*
 * sleep.c --
 *
 *   Portable, signal-safe implementation of Util_Usleep and Util_Sleep.
 */

#include "vmware.h"
#include "util.h"
#include "hostinfo.h"

#ifndef _WIN32
#include <unistd.h>
#endif

/*
 *----------------------------------------------------------------------
 *
 * Util_Usleep --
 *
 *    Sleeps for at least usec microseconds.  If interrupted by a signal,
 *    goes back to sleep.
 *
 *    This function is a drop-in replacement for usleep(3), so the argument is
 *    long because usleep(3) takes a long.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    A delay.
 *
 *----------------------------------------------------------------------
 */

void
Util_Usleep(long usec) // IN
{
   VmTimeType t_end, t;

   t_end = Hostinfo_SystemTimerUS() + usec;

   do {
      usleep(usec);
      t = Hostinfo_SystemTimerUS();
      usec = t_end - t;
   } while (t < t_end);
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Sleep --
 *
 *    Sleeps for at least sec seconds.  If interrupted by a signal,
 *    goes back to sleep.
 *
 *    This function is a drop-in replacement for sleep(3), so the argument is
 *    unsigned int because sleep(3) takes unsigned int.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    A delay.
 *
 *----------------------------------------------------------------------
 */

void
Util_Sleep(unsigned int sec) // IN
{
   Util_Usleep((long)sec * 1000 * 1000);
}
