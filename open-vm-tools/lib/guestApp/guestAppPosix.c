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
#include "escape.h"
#include "vm_assert.h"

static char *gBrowserEscaped = NULL; // a shell escaped browser path
static Bool gBrowserIsNewNetscape = FALSE;

void GuestAppDetectBrowser(void);


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
   char *buf = NULL;
   char *urlEscaped = NULL;
   Bool success = FALSE;
   int ret;

   ASSERT(url);

   if (!gBrowserEscaped) {
      GuestAppDetectBrowser();
   }

   if (!gBrowserEscaped) {
      goto abort;
   }
   urlEscaped = (char*)Escape_Sh(url, strlen(url), NULL);
   if (!urlEscaped) {
      goto abort;
   }

   if (gBrowserIsNewNetscape) {
      buf = Str_Asprintf(NULL, 
                          "%s -remote 'openURL('%s', new-window)' >/dev/null 2>&1 &", 
                          gBrowserEscaped, urlEscaped);
   } else {
      buf = Str_Asprintf(NULL, "%s %s >/dev/null 2>&1 &", gBrowserEscaped, urlEscaped);
   }

   if (buf == NULL) {
      goto abort;
   }
     
   ret = system(buf);

   /*
    * If the program terminated other than by exit() or return, i.e., was
    * hit by a signal, or if the exit status indicates something other
    * than success, then the URL wasn't opened and we should indicate
    * failure.
    */
   if (!WIFEXITED(ret) || (WEXITSTATUS(ret) != 0)) {
      goto abort;
   }

   success = TRUE;
 abort:
   free(buf);
   free(urlEscaped);
   
   return success;
}


/*
 *----------------------------------------------------------------------
 *
 * GuestAppDetectBrowser -- 
 *
 *	Figure out what browser to use, and note if it is a new Netscape.
 *      Copied from apps/vmuiLinux/app.cc
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
GuestAppDetectBrowser(void)
{
   char *buf;

   if (gBrowserEscaped) {
      free(gBrowserEscaped);
      gBrowserEscaped = NULL;
      gBrowserIsNewNetscape = FALSE;
   }

   if (getenv("GNOME_DESKTOP_SESSION_ID") != NULL &&
       GuestApp_FindProgram("gnome-open")) {
      buf = "gnome-open";
   } else if (getenv("KDE_FULL_SESSION") != NULL && 
              !strcmp(getenv("KDE_FULL_SESSION"), "true") &&
              GuestApp_FindProgram("konqueror")) {
      buf = "konqueror";
   } else if (GuestApp_FindProgram("mozilla-firefox")) {
      buf = "mozilla-firefox";
   } else if (GuestApp_FindProgram("firefox")) {
      buf = "firefox";
   } else if (GuestApp_FindProgram("mozilla")) {
      buf = "mozilla";
   } else if (GuestApp_FindProgram("netscape")) {
      buf = "netscape";
   } else {
      return;
   }

   /*
    * netscape >= 6.2 has a bug, in that if we try to reuse an existing 
    * window, and fail, it will return a success code.  We have to test for this 
    * eventuality, so we can handle it better.
    */
   if (!strcmp(buf, "netscape")) {
      gBrowserIsNewNetscape =
        (system("netscape -remote 'openURL(file:/some/bad/path.htm, new-window'") == 0);
   }
   gBrowserEscaped = (char *)Escape_Sh(buf, strlen(buf), NULL);
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
