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
 * @file pllLinux.c
 *
 * Implementation of the NTP PLL using Linux's adjtimex system call.
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


static void
TimeSyncLogPLLState(const char *prefix, struct timex *tx)
{
   g_debug("%s : off %jd freq %jd maxerr %jd esterr %jd status %d "
           "const %jd precision %jd tolerance %jd tick %jd\n",
           prefix, (intmax_t)tx->offset, (intmax_t)tx->freq, (intmax_t)tx->maxerror, (intmax_t)tx->esterror, 
           tx->status, (intmax_t)tx->constant, (intmax_t)tx->precision, (intmax_t)tx->tolerance, (intmax_t)tx->tick);
}

/*
 ******************************************************************************
 * TimeSync_PLLSupported --                                             */ /**
 *
 * Report whether the platform supports an NTP style Type-II Phase Locked
 * Loop for correcting the time.
 *
 * @return TRUE iff NTP Phase Locked Loop is supported.
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLSupported(void)
{
   return TRUE;
}


/*
 ******************************************************************************
 * TimeSync_PLLSetFrequency --                                          */ /**
 *
 * Set the frequency of the PLL.  
 *
 * @param[in] ppmCorrection  The parts per million error that should be 
 *                           corrected.  This value is the ppm shifted 
 *                           left by 16 to match NTP.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLSetFrequency(int64 ppmCorrection)
{
   struct timex tx;
   int error;

   tx.modes = ADJ_FREQUENCY;
   tx.freq = ppmCorrection;

   error = adjtimex(&tx);
   if (error == -1) {
      g_debug("%s: adjtimex failed: %d %s\n", __FUNCTION__,
              error, strerror(errno));
         return FALSE;
   }
   TimeSyncLogPLLState(__FUNCTION__, &tx);

   return TRUE;
}


/*
 ******************************************************************************
 * TimeSync_PLLUpdate --                                                */ /**
 *
 * Updates the PLL with a new offset.
 *
 * @param[in] offset         The offset between the host and the guest.
 *
 * @return TRUE on success.
 *
 ******************************************************************************
 */

Bool
TimeSync_PLLUpdate(int64 offset)
{
   struct timex tx;
   int error;

   if (offset < -500000) {
      offset = -500000;
      g_debug("%s: clamped offset at -500000\n", __FUNCTION__);
   }
   if (offset > 500000) {
      offset = 500000;
      g_debug("%s: clamped offset at 500000\n", __FUNCTION__);
   }


   tx.modes = ADJ_OFFSET | ADJ_MAXERROR | ADJ_ESTERROR;
   tx.offset = offset;
   tx.esterror = 0;
   tx.maxerror = 0;

   error = adjtimex(&tx);
   if (error == -1) {
      g_debug("%s: adjtimex set offset failed: %d %s\n", __FUNCTION__,
              error, strerror(errno));
         return FALSE;
   }
   TimeSyncLogPLLState(__FUNCTION__, &tx);

   /* Ensure that the kernel discipline is in the right mode.  STA_PLLs
    * should be set and STA_UNSYNC should not be set.  
    * 
    * The time constant is a bit trickier.  In "Computer Network Time
    * Synchronization" the terms used are "time constant" and "poll
    * exponent" where time constant = 2 ^ poll exponent.  Valid values for
    * the poll exponent are 4 through 17, corresponding to a range of 16s
    * to 131072s (36 hours).  On linux things are a bit different though:
    * tx.constant appears to be the poll exponent and when trying to set
    * the poll exponent, tx.constant should be set to poll exponent - 4.
    *
    * We want to set the time constant to as low a value as possible.  The
    * core NTP PLL that the kernel discipline implements is built assuming
    * that there is a clock filter with a variable delay of up to 8.
    * Since TimeSyncReadHostAndGuest retries if the error is large, we
    * don't need to implement the clock filter.  Hence we want a time
    * constant of 60/8 = 7, but settle for the lowest available: 16.  This
    * allows us to react to changes relatively fast.
    */
   if (tx.constant != 4) {
      tx.modes = ADJ_TIMECONST;
      tx.constant = 0;
      error = adjtimex(&tx);
      if (error == -1) {
         g_debug("%s: adjtimex set time constant failed: %d %s\n", __FUNCTION__,
                 error, strerror(errno));
         return FALSE;
      }
      g_debug("Set PLL time constant\n");
      TimeSyncLogPLLState(__FUNCTION__, &tx);
   }
   if ((tx.status & STA_PLL) != STA_PLL || (tx.status & STA_UNSYNC) != 0) {
      tx.modes = ADJ_STATUS;
      tx.status = STA_PLL;
      error = adjtimex(&tx);
      if (error == -1) {
         g_debug("%s: adjtimex set status failed: %d %s\n", __FUNCTION__,
                 error, strerror(errno));
         return FALSE;
      }
      g_debug("Set PLL status\n");
      TimeSyncLogPLLState(__FUNCTION__, &tx);
   }
   return TRUE;
}
