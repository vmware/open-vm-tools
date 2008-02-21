/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * vmGuestLib.c --
 *
 *      DLL wrapper for vmGuestLib, compiles the bits of libs in
 *      bora-vmsoft needed for GuestLib.
 */


#ifdef _WIN32
#include <windows.h>
#endif

#include <stdarg.h>
#include <stdio.h>

#include "vmware.h"
#include "str.h"

#include "vmguestlib_version.h"
#include "embed_version.h"
VM_EMBED_VERSION(VMGUESTLIB_VERSION_STRING);

#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * DllMain --
 *
 *    Entry point for the DLL.
 *
 * Results:
 *    TRUE
 *
 * Side effects:
 *    DLL init.
 *
 *-----------------------------------------------------------------------------
 */

BOOL WINAPI
DllMain(HINSTANCE hInstance,
        DWORD dwReason,
        LPVOID lpReserved)
{
   if (dwReason == DLL_PROCESS_ATTACH) {
      /* Any one time initialization should go here. */
   } else if (dwReason == DLL_PROCESS_DETACH) {
   }

   return TRUE;
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * Panic --
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
Panic(const char *fmt, ...) // IN
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
