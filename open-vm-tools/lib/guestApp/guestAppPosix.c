/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * guestAppPosix.c --
 *
 *    Utility functions for guest applications, Posix specific implementations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/wait.h>

#include "guestApp.h"
#include "str.h"
#include "vm_assert.h"
#include "guestAppPosixInt.h"


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_OpenUrl --
 *
 *      Open a web browser on the URL. Copied from apps/vmuiLinux/app.cc
 *
 * Results:
 *      TRUE on success, FALSE on otherwise
 *
 * Side effects:
 *      Spawns off another process which runs a web browser.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_OpenUrl(const char *url, // IN
                 Bool maximize)   // IN: open the browser maximized? Ignored for now.
{
#ifdef GUESTAPP_HAS_X11
   return GuestAppX11OpenUrl(url, maximize);
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestApp_FindProgram --
 *
 *      Find a program using the system path. Copy from apps/vmuiLinux/app.cc
 *
 * Results:
 *      TRUE if found, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
GuestApp_FindProgram(const char *program)
{
   const char *path = getenv("PATH");
   char *p;
   char fullpath[1000];

   for (; path != NULL; path = p == NULL ? p : p + 1) {
      int n;
      p = index(path, ':');

      if (p == NULL) { // last component
         n = strlen(path);
      } else {
         n = p - path;
      }

      Str_Snprintf(fullpath, sizeof fullpath, "%.*s/%s", n, path, program);
      if (strlen(fullpath) == sizeof fullpath - 1) {    // overflow
         continue;
      } else if (access(fullpath, X_OK) != 0) {  // no such file or not executable
         continue;
      }
      return TRUE;
   }
   return FALSE;
}


#ifdef __cplusplus
}
#endif
