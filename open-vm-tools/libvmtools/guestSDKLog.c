/*********************************************************
 * Copyright (C) 2013-2016 VMware, Inc. All rights reserved.
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
 * guestSDKLog.c --
 *
 *   guestSDK logging stubs. In guestSDK, we only do logging in OBJ builds.
 *
 */

#include <stdio.h>
#include "str.h"


/*
 *-----------------------------------------------------------------------------
 *
 * GuestSDK_Debug --
 *
 *    Log debug messages.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Death.
 *
 *-----------------------------------------------------------------------------
 */

void
GuestSDK_Debug(const char *fmt, ...)
{
#ifdef VMX86_LOG /* only do logging on OBJ builds */
   va_list args;

   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestSDK_Log --
 *
 *    Log messages.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Death.
 *
 *-----------------------------------------------------------------------------
 */

void
GuestSDK_Log(const char *fmt, ...)
{
#ifdef VMX86_LOG /* only do logging on OBJ builds */
   va_list args;

   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestSDK_Warning --
 *
 *    Log warning messages.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Death.
 *
 *-----------------------------------------------------------------------------
 */

void
GuestSDK_Warning(const char *fmt, ...)
{
#ifdef VMX86_LOG /* only do logging on OBJ builds */
   va_list args;

   va_start(args, fmt);
   vfprintf(stderr, fmt, args);
   va_end(args);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestSDK_Panic --
 *
 *    Panic.  Apps have to implement this for themselves.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Death.
 *
 *-----------------------------------------------------------------------------
 */

void
GuestSDK_Panic(const char *fmt, ...) // IN
{
   va_list args;
   char buffer[1024];
   volatile char *p = NULL;

   va_start(args, fmt);
   Str_Vsnprintf(buffer, sizeof buffer, fmt, args);
   va_end(args);

   printf("PANIC: %s", buffer);

   /* force segfault */
   buffer[0] = *p;
   while (1) ; // avoid compiler warning
}
