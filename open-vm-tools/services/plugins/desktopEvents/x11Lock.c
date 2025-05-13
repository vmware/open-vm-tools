/*********************************************************
 * Copyright (c) 2010-2018,2022 VMware, Inc. All rights reserved.
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
 * @file x11lock.c
 *
 * Sets up an X11 lock atom and check it to avoid multiple running instances
 * of vmusr.
 */

#include "desktopEventsInt.h"
#include "vmware.h"

#include <stdlib.h>
#include <string.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>

#define LOCK_ATOM_NAME  "vmware-user-lock"


/*
 ******************************************************************************
 * InitGroupLeader --                                                   */ /**
 *
 * This routine sets a few properties related to our main window created
 * by {gdk,gtk}_init.  Specifically this routine sets the window title,
 * sets the override_redirect X11 property, and reparents it to the root
 * window,
 *
 * In addition, this routine will return Xlib handles for the following
 * objects:
 *   - Main or group leader window
 *   - Display's root window
 *
 * As a result of this function:
 *   - groupLeader will have a title of VMUSER_TITLE.
 *   - groupLeader, if not already directly parented by the root, will be.
 *
 *   - dpy will point to our default display (ex: $DISPLAY).
 *   - groupLeader will point to the window created by gtk_init().
 *   - rootWindow will point to the root window on $DISPLAY.
 *
 * @param[out] groupLeader    Group leader window.
 * @param[out] rootWindow     Root window.
 *
 * @return TRUE on success, FALSE on failure.
 *
 ******************************************************************************
 */

static gboolean
InitGroupLeader(Window *groupLeader,
                Window *rootWindow)
{
   Window myGroupLeader;
   Window myRootWindow;
   XSetWindowAttributes attr;
   GdkDisplay *gdkDisplay;
   GdkWindow *gdkLeader;

   attr.override_redirect = True;

   ASSERT(groupLeader);
   ASSERT(rootWindow);

   gdkDisplay = gdk_display_get_default();
   gdkLeader = gdk_display_get_default_group(gdkDisplay);
   myGroupLeader = GDK_WINDOW_XID(gdkLeader);
   myRootWindow = GDK_ROOT_WINDOW();

   ASSERT(myGroupLeader);
   ASSERT(myRootWindow);

   /* XXX: With g_set_prgname() being called, this can probably go away. */
   XStoreName(gdk_x11_get_default_xdisplay(), myGroupLeader, VMUSER_TITLE);

   /*
    * Confidence check:  Set the override redirect property on our group leader
    * window (not default), then re-parent it to the root window (default).
    * This makes sure that (a) a window manager can't re-parent our window,
    * and (b) that we remain a top-level window.
    */
   XChangeWindowAttributes(gdk_x11_get_default_xdisplay(), myGroupLeader, CWOverrideRedirect,
                           &attr);
   XReparentWindow(gdk_x11_get_default_xdisplay(), myGroupLeader, myRootWindow, 10, 10);
   XSync(gdk_x11_get_default_xdisplay(), FALSE);

   *groupLeader = myGroupLeader;
   *rootWindow = myRootWindow;

   return TRUE;
}


/*
 ******************************************************************************
 * QueryX11Lock --                                                      */ /**
 *
 * This is just a wrapper around XGetWindowProperty which queries the
 * window described by <dpy,w> for the property described by lockAtom.
 *
 * @param[in] dpy          X11 display to query
 * @param[in] w            Window to query
 * @param[in] lockAtom     Atom used for locking
 *
 * @return TRUE if property defined by parameters exists; FALSE otherwise.
 *
 ******************************************************************************
 */

static gboolean
QueryX11Lock(Display *dpy,
             Window w,
             Atom lockAtom)
{
   Atom ptype;                  // returned property type
   int pfmt;                    // returned property format
   unsigned long np;            // returned # of properties
   unsigned long remaining;     // amount of data remaining in property
   unsigned char *data = NULL;

   if (XGetWindowProperty(dpy, w, lockAtom, 0, 1, False, lockAtom,
                          &ptype, &pfmt, &np, &remaining, &data) != Success) {
      g_warning("%s: Unable to query window %lx for property %s\n", __func__, w,
                LOCK_ATOM_NAME);
      return FALSE;
   }

   /*
    * Xlib is wacky.  If the following test is true, then our property
    * didn't exist for the window in question.  As a result, `data' is
    * unset, so don't worry about the lack of XFree(data) here.
    */
   if (ptype == None) {
      return FALSE;
   }

   /*
    * We care only about the existence of the property, not its value.
    */
   XFree(data);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FetchNameErrorHandler --
 *
 *      According to XFetchName document, XFetchName may generate a BadWindow
 *      error. In this case, the pointer we pass to XFetchName doesn't name a
 *      defined window. X is asynchronous, there isn't a gurantee that the
 *      window is still alive between the time the window is obtained and
 *      the time a event is sent to the window. So, for this scenario, since
 *      the window doesn't exist, we can ignore checking its name. This can
 *      avoid this plugin from crashing. Refer to PR 1871141 for details.
 *
 * Results:
 *      Logs error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
FetchNameErrorHandler(Display *display,
                      XErrorEvent *error)
{
   /* 256 is enough for the error description. */
   char msg[256];
   XGetErrorText(display, error->error_code, msg, sizeof(msg));

   g_warning("X Error %d (%s): request %d.%d\n",
             error->error_code, msg, error->request_code, error->minor_code);

   return 0;
}


/*
 ******************************************************************************
 * AcquireDisplayLock --                                                */ /**
 *
 * This function "locks" the display against being "claimed" by another
 * instance of vmware-user.  It will succeed if we're the first/only
 * instance of vmware-user, and fail otherwise.
 *
 * NB: This routine must be called -after- gtk_init().
 *
 * Vmware-user enjoys per-display exclusivity using the following algorithm:
 *
 *   1.  Grab X server.  (I.e., get exclusive access.)
 *   2.  Search for top-level X windows meeting the following criteria:
 *       a.  named "vmware-user"
 *       b.  has the property "vmware-user-lock" set.
 *   3a. If any such windows described above found, then another vmware-user
 *       process is attached to this display, so we consider the display
 *       locked.
 *   3b. Else we're the only one.  Set the "vmware-user-lock" property on
 *       our top-level window.
 *   4.  Ungrab the X server.
 *
 * The first time this routine is ever called during the lifetime of an X
 * session, a new X11 Atom, "vmware-user-lock" is created for the lifetime
 * of the X server.
 *
 * The "vmware-user-lock" property may be set on this process's group leader
 * window.
 *
 * @return TRUE if "lock" acquired (i.e., we're the first/only vmware-user
 *         process); otherwise FALSE.
 *
 ******************************************************************************
 */

static gboolean
AcquireDisplayLock(void)
{
   Display *defaultDisplay;     // Current default X11 display.
   Window rootWindow;           // Root window of defaultDisplay; used as root node
                                // passed to XQueryTree().
   Window groupLeader;          // Our instance's window group leader.  This is
                                // implicitly created by gtk_init().

   Window *children = NULL;     // Array of windows returned by XQueryTree().
   unsigned int nchildren;      // Length of children.

   Window dummy1, dummy2;       // Throwaway window IDs for XQueryTree().
   Atom lockAtom;               // Refers to the "vmware-user-lock" X11 Atom.

   unsigned int index;
   Bool alreadyLocked = FALSE;  // Set to TRUE if we discover lock is held.
   Bool retval = FALSE;

   defaultDisplay = gdk_x11_get_default_xdisplay();

   /*
    * Reset some of our main window's settings & fetch Xlib handles for
    * the GDK group leader and root windows.
    */
   if (InitGroupLeader(&groupLeader, &rootWindow) == FALSE) {
      g_warning("%s: unable to initialize main window.\n", __func__);
      return FALSE;
   }

   /*
    * Look up the lock atom, creating it if it doesn't already exist.
    */
   lockAtom = XInternAtom(defaultDisplay, LOCK_ATOM_NAME, False);
   if (lockAtom == None) {
      g_warning("%s: unable to create X11 atom: " LOCK_ATOM_NAME "\n", __func__);
      return FALSE;
   }

   /*
    * Okay, so at this point the following is done:
    *
    *   1.  Our top-level / group leader window is a child of the display's
    *       root window.
    *   2.  The window manager can't get its hands on said window.
    *   3.  We have a handle on the X11 atom which will be used to identify
    *       the X11 property used as our lock.
    */

   g_debug("%s: Grabbing X server.\n", __func__);

   /*
    * Neither of these can fail, or at least not in the sense that they'd
    * return an error.  Instead we'd likely see an X11 I/O error, tearing
    * the connection down.
    *
    * XSync simply blocks until the XGrabServer request is acknowledged
    * by the server.  It makes sure that we don't continue issuing requests,
    * such as XQueryTree, until the server grants our "grab".
    */
   XGrabServer(defaultDisplay);
   XSync(defaultDisplay, False);

   /*
    * WARNING:  At this point, we have grabbed the X server.  Consider the
    * UI to be completely frozen.  Under -no- circumstances should we return
    * without ungrabbing the server first.
    */

   if (XQueryTree(defaultDisplay, rootWindow, &dummy1, &dummy2, &children,
                  &nchildren) == 0) {
      g_warning("%s: XQueryTree failed\n", __func__);
      goto out;
   }

   /*
    * Iterate over array of top-level windows.  Search for those named
    * vmware-user and with the property "vmware-user-lock" set.
    *
    * If any such windows are found, then another process has already
    * claimed this X session.
    */
   for (index = 0; (index < nchildren) && !alreadyLocked; index++) {
      char *name = NULL;
      /* Use customized error handler to suppress BadWindow error. */
      int (*oldXErrorHandler)(Display*, XErrorEvent*) =
         XSetErrorHandler(FetchNameErrorHandler);

      /* Skip unless window is named vmware-user. */
      if ((XFetchName(defaultDisplay, children[index], &name) == 0) ||
          (name == NULL) ||
          strcmp(name, VMUSER_TITLE)) {
         XFree(name);

         /* Restore default error handler. */
         XSetErrorHandler(oldXErrorHandler);
         continue;
      }

      /* Restore default error handler. */
      XSetErrorHandler(oldXErrorHandler);

      /*
       * Query the window for the "vmware-user-lock" property.
       */
      alreadyLocked = QueryX11Lock(defaultDisplay, children[index], lockAtom);
      XFree(name);
   }

   /*
    * Yay.  Lock isn't held, so go ahead and acquire it.
    */
   if (!alreadyLocked) {
      unsigned char dummy[] = "1";
      g_debug("%s: Setting property " LOCK_ATOM_NAME "\n", __func__);
      /*
       * NB: Current Xlib always returns one.  This may generate a -fatal- IO
       * error, though.
       */
      XChangeProperty(defaultDisplay, groupLeader, lockAtom, lockAtom, 8,
                      PropModeReplace, dummy, sizeof dummy);
      retval = TRUE;
   }

out:
   XUngrabServer(defaultDisplay);
   XSync(defaultDisplay, False);
   XFree(children);

   return retval;
}


/*
 ******************************************************************************
 * X11Lock_Init --                                                      */ /**
 *
 * Initializes GTK, and sets up a lock atom to make sure only one vmusr
 * instance is running.
 *
 * On error, this function will request that the application's main loop stop
 * running.
 *
 * @param[in] ctx       Application context.
 * @param[in] pdata     Registration data.
 *
 * @return TRUE on success, FALSE if another vmusr instance owns the display or
 *         not running in the right container (vmusr).
 *
 ******************************************************************************
 */

gboolean
X11Lock_Init(ToolsAppCtx *ctx,
             ToolsPluginData *pdata)
{
   int argc = 0;
   char *argv[] = { NULL, NULL };

   if (!TOOLS_IS_USER_SERVICE(ctx)) {
      VMTOOLSAPP_ERROR(ctx, EXIT_FAILURE);
      return FALSE;
   }

   /*
    * We depend on the window title when performing (primitive) vmware-user
    * session detection, and unfortunately for us, GTK has a nasty habit of
    * retitling toplevel windows.  That said, we can control GTK's default
    * title by setting Glib's application or program name.
    *
    * XXX Consider using g_set_application_name("VMware User Agent") or
    * similar.
    */
   g_set_prgname(VMUSER_TITLE);
   argv[0] = VMUSER_TITLE;

#if GTK_MAJOR_VERSION > 3 || (GTK_MAJOR_VERSION == 3 && GTK_MINOR_VERSION >= 10)
   /*
    * On recent distros, Wayland is the default display server. If the obtained
    * display or window is a wayland one, applying X11 specific functions on them
    * will result in crashes. Before migrating the X11 specific code to Wayland,
    * force using X11 as the backend of Gtk+3. gdk_set_allowed_backends() is
    * introduced since Gtk+3.10 and Wayland is supported from Gtk+3.10.
    */
   gdk_set_allowed_backends("x11");
#endif
   /* XXX: is calling gtk_init() multiple times safe? */
   /* gtk_disable_setlocale() must be called before gtk_init() */
   gtk_disable_setlocale();
   gtk_init(&argc, (char ***) &argv);

   if (!AcquireDisplayLock()) {
      g_warning("Another instance of vmware-user already running. Exiting.\n");
      VMTOOLSAPP_ERROR(ctx, EXIT_FAILURE);
      return FALSE;
   }

   return TRUE;
}

