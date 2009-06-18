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

#include <stdio.h>
#include <stdlib.h>     // for free, system
#include <string.h>
#include <strings.h>

#include <glib.h>

#include <X11/Xlib.h>
#undef Bool

#include "guestApp.h"
#include "guestAppPosixInt.h"
#include "str.h"
#include "escape.h"
#include "util.h"
#include "vm_assert.h"
#include "system.h"
#include "debug.h"

static const char *gBrowser = NULL; // browser path
static Bool gBrowserIsMalloced = FALSE;
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
   gboolean spawnSuccess;
   GError *gerror = NULL;

   char **argv = NULL;
   char *newNetscapeBuf = NULL;
   Bool success = FALSE;

   ASSERT(url);

   if (!gBrowser) {
      GuestAppDetectBrowser();
   }

   if (!gBrowser) {
      goto abort;
   }

   if (gBrowserIsNewNetscape) {
      /*
       * Per RFC 2616 §3.2.1, HTTP places no bound on URIs.  (Besides, this url
       * could really be -any- URI.)  I.e., that's why I'm eating the cost of
       * allocating memory instead of playing with a static buffer.
       */
      newNetscapeBuf = Str_Asprintf(NULL, "openURL('%s', new-window)", url);
      if (!newNetscapeBuf) {
         goto abort;
      }
      argv = g_malloc(4 * sizeof *argv);
      argv[0] = (char *)gBrowser;
      argv[1] = "-remote";
      argv[2] = newNetscapeBuf;
      argv[3] = NULL;
   } else {
      gint argc;
      /*
       * See GuestAppDetectBrowser(): our browser command could come from gconf,
       * in which case it's a command line that we have to parse, so that we
       * can put the URL in the right place before executing the child process.
       */
      if (!g_shell_parse_argv(gBrowser, &argc, &argv, &gerror)) {
         Debug("Error parsing browser command line: %s\n", gerror->message);
         g_clear_error(&gerror);
         goto abort;
      }
      if (argc > 1) {
         /*
          * If the browser is a command line, we expect "%s" to be the URL
          * placeholder. If not found, bail out.
          */
         gint i;
         for (i = 0; i < argc; i++) {
            if (strcmp(argv[i], "%s") == 0) {
               argv[i] = (char *) url;
               break;
            }
         }
         if (i == argc) {
            Debug("Browser command (%s) doesn't have an URL placeholder.\n", gBrowser);
            goto abort;
         }
      } else {
         g_free(argv);
         argv = g_malloc(3 * sizeof *argv);
         argv[0] = (char *)gBrowser;
         argv[1] = (char *)url;
         argv[2] = NULL;
      }
   }

   /*
    * Spawn the child and hope for the best. Do not block our UI while the
    * help is showing.
    */
   spawnSuccess = g_spawn_async(NULL,     // inherit working directory
                                argv,
                                /*
                                * XXX  Please don't hate me for casting off the
                                * qualifier here.  Glib does -not- modify the
                                * environment, at least not in the parent process,
                                * but their prototype does not specify this argument
                                * as being const.
                                */
                                (char **)guestAppSpawnEnviron,
                                G_SPAWN_SEARCH_PATH
                                 | G_SPAWN_STDOUT_TO_DEV_NULL
                                 | G_SPAWN_STDERR_TO_DEV_NULL,
                                NULL,     // no child setup routine
                                NULL,     // param for child setup routine
                                NULL,     // no child pid
                                &gerror); // GSpawnError

   if (!spawnSuccess) {
      Debug("%s: Unable to launch browser '%s': %d: %s\n", __func__, gBrowser,
            gerror->code, gerror->message);
      g_clear_error(&gerror);
      goto abort;
   }

   success = TRUE;

abort:
   g_free(argv);
   free(newNetscapeBuf);
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
   const char *buf = NULL;

   if (gBrowser) {
      if (gBrowserIsMalloced) {
         g_free((void *)gBrowser);
      }
      gBrowser = NULL;
      gBrowserIsMalloced = FALSE;
      gBrowserIsNewNetscape = FALSE;
   }

   /*
    * XXX Since splitting guestd and vmware-user, vmware-user may be launched
    * by a -display- manager rather than a session manager, rendering exclusive
    * tests for "GNOME_DESKTOP_SESSION_ID" or "KDE_FULL_SESSION" environment
    * variables insufficient.
    *
    * The workaround (*cough*hack*cough*) for the GNOME case is to additionally
    * query the root X11 window, and testing for the existence of a "gnome-session"
    * window.  (The assumption is that if gnome-session is attached to our X11
    * display, the user really is running a GNOME session.)  For KDE, we look for
    * "ksmserver".
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
   if ((getenv("GNOME_DESKTOP_SESSION_ID") != NULL ||
        GuestAppFindX11Client("gnome-session") ||
        GuestAppFindX11Client("gnome-panel")) &&
       GuestApp_FindProgram("gconftool-2")) {
      /*
       * XXX: gnome-open is stupid and doesn't work if it receives a "file:" URL
       * with a query string, since it thinks the query string is part of
       * the path and refuses to open the URL. You can get the current browser
       * by using gconftool-2, in which case the string we get will contain
       * a "%s" where the URL should be, which is handled later.
       */
      char *argv[] = {
         "gconftool-2",
         "--get",
         "/desktop/gnome/url-handlers/http/command"
      };
      gboolean success;
      gchar *out = NULL;
      gint status;
      GError *err = NULL;

      success =  g_spawn_sync(NULL,
                              argv,
                               (char **)guestAppSpawnEnviron,
                               G_SPAWN_SEARCH_PATH | G_SPAWN_STDERR_TO_DEV_NULL,
                               NULL,
                               NULL,
                               &out,
                               NULL,
                               &status,
                               &err);
      if (!success || !WIFEXITED(status) || WEXITSTATUS(status) != 0 ||
          out == NULL || strlen(out) == 0) {
          Warning("Failed to invoke gconftool-2: exit code %d (%s)\n",
                  WEXITSTATUS(status),
                  (err != NULL) ? err->message : "");
          g_clear_error(&err);
      } else {
         buf = g_strchomp(out);
         gBrowserIsMalloced = TRUE;
         goto exit;
      }
   }

   if (((getenv("KDE_FULL_SESSION") != NULL &&
                !strcmp(getenv("KDE_FULL_SESSION"), "true")) ||
               GuestAppFindX11Client("ksmserver") ||
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

exit:
   /*
    * netscape >= 6.2 has a bug, in that if we try to reuse an existing
    * window, and fail, it will return a success code.  We have to test for this
    * eventuality, so we can handle it better.
    */
   if (!strcmp(buf, "netscape")) {
      gBrowserIsNewNetscape =
        (system("netscape -remote 'openURL(file:/some/bad/path.htm, new-window'") == 0);
   }
   gBrowser = buf;
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
