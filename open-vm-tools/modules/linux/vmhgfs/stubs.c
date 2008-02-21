/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * stubs.c
 *
 * Contains stubs and helper functions.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include "kernelStubs.h"
#include "module.h"
#include "vm_assert.h"

/*
 *----------------------------------------------------------------------
 *
 * Debug --
 *
 *    If debugging is enabled, output debug information.
 *
 * Result
 *    None
 *
 * Side-effects
 *    None
 *
 *----------------------------------------------------------------------
 */

void
Debug(char const *fmt, // IN: Format string
      ...)             // IN: Arguments
{
   va_list args;
   int numBytes;
   static char out[128];

   va_start(args, fmt);
   numBytes = Str_Vsnprintf(out, sizeof out, fmt, args);
   va_end(args);

   if (numBytes > 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s", out));
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Log --
 *
 *    Needs to be defined.
 *
 * Result
 *    None
 *
 * Side-effects
 *    None
 *
 *----------------------------------------------------------------------
 */
void
Log(const char *string, ...)
{
   // do nothing.
}
