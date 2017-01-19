/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * vmsignal.c --
 *
 *    Posix signal handling utility functions
 *
 */

#ifndef VMX86_DEVEL

#endif


#include <stdio.h>
#include <errno.h>
#include <string.h>


#include "vmsignal.h"


/*
 * Signal_SetGroupHandler --
 *
 *    Set a signal handler for a group of signals.
 *    We carefully ensure that if the handler is only used to handle the
 *    signals of the group, the handling of all the signals of the group is
 *    serialized, which means that the handler is not re-entrant.
 *
 * Return value:
 *    1 on success
 *    0 on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 */

int
Signal_SetGroupHandler(int const *signals,          // IN
                       struct sigaction *olds,      // OUT
                       unsigned int nr,             // IN
                       void (*handler)(int signal)) // IN
{
   unsigned int i;
   struct sigaction new;

   new.sa_handler = handler;
   if (sigemptyset(&new.sa_mask)) {
      fprintf(stderr, "Unable to empty a signal set: %s.\n\n", strerror(errno));

      return 0;
   }
   for (i = 0; i < nr; i++) {
      if (sigaddset(&new.sa_mask, signals[i])) {
         fprintf(stderr, "Unable to add a signal to a signal set: %s.\n\n", strerror(errno));

         return 0;
      }
   }
   new.sa_flags = 0;

   for (i = 0; i < nr; i++) {
      if (sigaction(signals[i], &new, &olds[i])) {
         fprintf(stderr, "Unable to modify the handler of the signal %d: %s.\n\n", signals[i], strerror(errno));

         return 0;
      }
   }

   return 1;
}


/*
 * Signal_ResetGroupHandler --
 *
 *    Reset the handler of each signal of a group of signals
 *
 * Return value:
 *    1 on success
 *    0 on failure (detail is displayed)
 *
 * Side effects:
 *    None
 *
 */

int
Signal_ResetGroupHandler(int const *signals,           // IN
                         struct sigaction const *olds, // IN
                         unsigned int nr)              // IN
{
   unsigned int i;

   for (i = 0; i < nr; i++) {
      if (sigaction(signals[i], &olds[i], NULL)) {
         fprintf(stderr, "Unable to reset the handler of the signal %d: %s.\n\n", signals[i], strerror(errno));

         return 0;
      }
   }

   return 1;
}
