/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * guestAppPosixX11.c --
 *
 *    X11-support functions for guestAppPosix.c.  These sources maintained
 *    separately only to avoid forcing X11 library dependencies where they're
 *    not needed.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/wait.h>

#include <stdlib.h>     // for free, system
#include <string.h>
#include <strings.h>

#include <X11/Xlib.h>
#undef Bool

#include "guestApp.h"
#include "str.h"
#include "escape.h"
#include "vm_assert.h"

static char *gBrowserEscaped = NULL; // a shell escaped browser path
static Bool gBrowserIsNewNetscape = FALSE;
static XErrorHandler gDefaultXErrorHandler;

static void GuestAppDetectBrowser(void);
static Bool GuestAppFindX11Client(const char *clientName);
static int  GuestAppXErrorHandler(Display *display, XErrorEvent *error_event);


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppX11OpenUrl --
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
GuestAppX11OpenUrl(const char *url, // IN
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
 *      This routine is not thread-safe. 
 *
 *----------------------------------------------------------------------
 */

void
GuestAppDetectBrowser(void)
{
   char *buf = NULL;

   if (gBrowserEscaped) {
      free(gBrowserEscaped);
      gBrowserEscaped = NULL;
      gBrowserIsNewNetscape = FALSE;
   }

   /*
    * XXX Since splitting guestd and vmware-user, vmware-user may be launched
    * by a -display- manager rather than a session manager, rendering tests for
    * "GNOME_DESKTOP_SESSION_ID" or "KDE_FULL_SESSION" environment variables
    * faulty.
    *
    * The workaround (*cough*hack*cough*) for the GNOME case is to instead query
    * the root X11 window, and testing for the existence of a "gnome-session"
    * window.  (The assumption is that if gnome-session is attached to our X11
    * display, the user really is running a GNOME session.)  For KDE, we look
    * for "ksmserver".
    *
    * XXX Pull this out s.t. we need only traverse the list of clients once.
    * XXX Added gnome-panel, startkde as they were previously in xautostart.conf.
    *     On my Ubuntu VM, gnome-session is really started via a symlink of
    *     /usr/bin/x-session-manager -> /etc/alternatives/x-session-manager ->
    *     /usr/bin/gnome-session.  Gnome-session never sets its window title
    *     string, which I assumed it did, and as a result shows up as a client
    *     named "x-session-manager".  In this case, I'm falling back and
    *     using existence of "gnome-panel" as a safe bet that the user is
    *     in a GNOME session.
    * XXX This code should be destroyed.
    */
   if ((GuestAppFindX11Client("gnome-session") ||
        GuestAppFindX11Client("gnome-panel")) &&
       GuestApp_FindProgram("gnome-open")) {
      buf = "gnome-open";
   } else if ((GuestAppFindX11Client("ksmserver") ||
               GuestAppFindX11Client("startkde")) &&
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
 * GuestAppFindX11Client --
 *
 *      Searches for a top-level window names by clientName.
 *
 * Results:
 *      TRUE if such a window was found, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
GuestAppFindX11Client(const char *clientName)   // IN: window title to search for
{
   Display *display;
   Window rootWindow;
   Window temp1;        // throwaway
   Window temp2;        // throwaway
   Window *children = NULL;
   unsigned int nchildren;
   unsigned int i;
   Bool found = FALSE;

   if ((display = XOpenDisplay(NULL)) == NULL) {
      return FALSE;
   }

   rootWindow = DefaultRootWindow(display);
   /*
    * I want to fall back to the original error handler for all but the BadWindow
    * case.  Unfortunately I can't just pass that along, so I need to record the
    * original handler via a global variable.
    */
   gDefaultXErrorHandler = XSetErrorHandler(GuestAppXErrorHandler);

   if (XQueryTree(display, rootWindow, &temp1, &temp2, &children, &nchildren) == 0) {
      goto out;
   }

   for (i = 0; (i < nchildren) && !found; i++) {
      char *name = NULL;
      if ((XFetchName(display, children[i], &name) == 0) ||
          name == NULL) {
         continue;
      }
      if (strcmp(name, clientName) == 0) {
         found = TRUE;
      }
      XFree(name);
   }

out:
   XFree(children);
   XSetErrorHandler(gDefaultXErrorHandler);
   XCloseDisplay(display);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GuestAppXErrorHandler --
 *
 *      Silently ignores BadWindow errors, and passes all others back to the
 *      default error handler for further processing.
 *
 * Results:
 *      Always zero.  (Per Xlib, the return value is always ignored.)
 *
 * Side effects:
 *      None, except those caused by the default error handler (e.g., killing
 *      the client).
 *
 *-----------------------------------------------------------------------------
 */

static int
GuestAppXErrorHandler(Display *display,
                                // IN: X11 display on which error occurred
                      XErrorEvent *error_event)
                                // IN: error received from X11 server
{
   /* Handoff to the default handler for everything but BadWindow. */
   if ((error_event->error_code != BadWindow) &&
       gDefaultXErrorHandler) {
      gDefaultXErrorHandler(display, error_event);
   }

   return 0;
}

#ifdef __cplusplus
}
#endif
