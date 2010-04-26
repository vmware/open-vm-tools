/*********************************************************
 * Copyright (C) 2007-2008 VMware, Inc. All rights reserved.
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
 * @file unityPlatformX11.c
 *
 * Implementation of Unity for guest operating systems that use the X11 windowing
 * system. This file holds the basic things such as initialization/destruction of the
 * UnityPlatform object, overall event handling, and handling of some Unity
 * RPCs that are not window-centric.
 */

#include "unityX11.h"
#include "appUtil.h"
#include "region.h"
#include <sys/time.h>

#include <X11/extensions/Xinerama.h>
#include <X11/extensions/XTest.h>

typedef struct {
   Window realWindowID;
   XEvent xevent;
} UnityTemporaryEvent;

static UnitySpecialWindow *USWindowCreate(UnityPlatform *up,
                                           UnitySpecialEventHandler evHandler,
                                           Window *windows,
                                           int windowCount);
static void USWindowUpdate(UnityPlatform *up,
                           UnitySpecialWindow *usw,
                           Window *windows,
                           int windowCount);
static UnitySpecialWindow *USWindowLookup(UnityPlatform *up, Window window);
static void USWindowDestroy(UnityPlatform *up, UnitySpecialWindow *usw);

static void UnityPlatformProcessXEvent(UnityPlatform *up,
                                       const XEvent *xevent,
                                       Window realEventWindow);
static Window UnityPlatformGetRealEventWindow(UnityPlatform *up, const XEvent *xevent);
static void USRootWindowsProcessEvent(UnityPlatform *up,
                                       UnitySpecialWindow *usw,
                                       const XEvent *xevent,
                                       Window window);
static int UnityPlatformXErrorHandler(Display *dpy, XErrorEvent *xev);
static UnitySpecialWindow *UnityPlatformMakeRootWindowsObject(UnityPlatform *up);

static void UnityPlatformSendClientMessageFull(Display *d,
                                               Window destWindow,
                                               Window w,
                                               Atom messageType,
                                               int format,
                                               int numItems,
                                               const void *data);
static void UnityPlatformStackDnDDetWnd(UnityPlatform *up);
static void UnityPlatformDnDSendClientMessage(UnityPlatform *up,
                                              Window destWindow,
                                              Window w,
                                              Atom messageType,
                                              int format,
                                              int numItems,
                                              const void *data);

static Bool GetRelevantWMWindow(UnityPlatform *up,
                                UnityWindowId windowId,
                                Window *wmWindow);
static Bool SetWindowStickiness(UnityPlatform *up,
                                UnityWindowId windowId,
                                Bool wantSticky);

static const GuestCapabilities platformUnityCaps[] = {
   UNITY_CAP_WORK_AREA,
   UNITY_CAP_START_MENU,
   UNITY_CAP_MULTI_MON,
   UNITY_CAP_VIRTUAL_DESK,
   UNITY_CAP_STICKY_WINDOWS,
};

/*
 * Has to be global for UnityPlatformXErrorHandler
 */
static int unityX11ErrorCount = 0;

/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformIsSupported --
 *
 *      Determine whether this guest supports unity.
 *
 * Results:
 *      TRUE if the guest supports Unity
 *      FALSE otherwise
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityPlatformIsSupported(void)
{
   Display *dpy;
   int major;
   int event_base;
   int error_base;

   dpy = GDK_DISPLAY();
   /*
    * Unity/X11 doesn't yet work with the new vmwgfx driver.  Until that is
    * fixed, we have to disable the feature.
    *
    * As for detecting which driver is in use, the legacy driver provides the
    * VMWARE_CTRL extension for resolution and topology operations, while the
    * new driver is instead controlled via XRandR.  If we don't find said
    * extension, then we'll assume the new driver is in use and disable Unity.
    */
   if (XQueryExtension(dpy, "VMWARE_CTRL", &major, &event_base, &error_base) ==
       False) {
      Debug("Unity is not yet supported under the vmwgfx driver.\n");
      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformInit --
 *
 *      Initialize the UnityPlatform object that represents the platform-specific state.
 *
 * Results:
 *      Pointer to newly allocated UnityPlatform data.
 *
 * Side effects:
 *      No.
 *
 *----------------------------------------------------------------------------
 */

UnityPlatform *
UnityPlatformInit(UnityWindowTracker *tracker,                            // IN
                  UnityUpdateChannel *updateChannel,                      // IN
                  int *blockedWnd,                                        // IN, not used
                  DesktopSwitchCallbackManager *desktopSwitchCallbackMgr) // IN, not used
{
   UnityPlatform *up;
   char *displayName;

   ASSERT(tracker);
   ASSERT(updateChannel);

   Debug("UnityPlatformInit: Running\n");

   up = Util_SafeCalloc(1, sizeof *up);
   up->tracker = tracker;
   up->updateChannel = updateChannel;

   up->savedScreenSaverTimeout = -1;

   /*
    * Because GDK filters events heavily, and we need to do a lot of low-level X work, we
    * just open another connection to the same display.
    */
   displayName = gdk_get_display();
   up->display = XOpenDisplay(displayName);
   if (!up->display) {
      free(up);
      return NULL; // We couldn't connect to the display for some strange reason
   }
   XSetErrorHandler(UnityPlatformXErrorHandler);
   XSynchronize(up->display, TRUE); // So error counting works properly...

   /*
    * Certain applications, like gnome-session during logout, may grab the X
    * server before displaying a modal window.  With the server grabbed, we're
    * unable to correctly track and display windows.
    *
    * The following snippet attempts to work around this by using the XTest
    * extension's ability to make ourselves impervious to X server grabs.
    */
   {
      int dummy1;
      int dummy2;
      int major;
      int minor;

      if ((XTestQueryExtension(up->display, &dummy1, &dummy2,
                               &major, &minor) == True) &&
          ((major > 2) || (major == 2 && minor >= 2))) {
         if (XTestGrabControl(up->display, True) != 1) {
            Debug("XTestGrabControl failed.\n");
         }
      } else {
         Debug("XTest extension not available.\n");
      }
   }

   up->allWindows = HashTable_Alloc(128, HASH_INT_KEY, NULL);
   up->specialWindows = HashTable_Alloc(32, HASH_INT_KEY, NULL);
   up->desktopWindow = NULL;
   up->desktopInfo.initialDesktop = UNITY_X11_INITIALDESKTOP_UNSET;

   /*
    * Find the values of all the atoms
    */
#  define INIT_ATOM(x) up->atoms.x = XInternAtom(up->display, #x, False)

   INIT_ATOM(_NET_WM_WINDOW_TYPE);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_DESKTOP);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_DOCK);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_TOOLBAR);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_TOOLTIP);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_DROPDOWN_MENU);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_POPUP_MENU);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_MENU);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_UTILITY);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_SPLASH);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_DIALOG);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_NORMAL);
   INIT_ATOM(_NET_WM_WINDOW_TYPE_DND);
   INIT_ATOM(_NET_WM_STATE);
   INIT_ATOM(_NET_WM_STATE_HIDDEN);
   INIT_ATOM(_NET_WM_STATE_MODAL);
   INIT_ATOM(_NET_WM_STATE_STICKY);
   INIT_ATOM(_NET_WM_STATE_MAXIMIZED_HORZ);
   INIT_ATOM(_NET_WM_STATE_MAXIMIZED_VERT);
   INIT_ATOM(_NET_WM_STATE_MINIMIZED);
   INIT_ATOM(_NET_WM_STATE_SHADED);
   INIT_ATOM(_NET_WM_STATE_SKIP_TASKBAR);
   INIT_ATOM(_NET_WM_STATE_SKIP_PAGER);
   INIT_ATOM(_NET_WM_STATE_FULLSCREEN);
   INIT_ATOM(_NET_WM_STATE_ABOVE);
   INIT_ATOM(_NET_WM_STATE_BELOW);
   INIT_ATOM(_NET_WM_STATE_DEMANDS_ATTENTION);
   INIT_ATOM(_NET_WM_USER_TIME);
   INIT_ATOM(_NET_WM_USER_TIME_WINDOW);
   INIT_ATOM(_NET_ACTIVE_WINDOW);
   INIT_ATOM(_NET_RESTACK_WINDOW);
   INIT_ATOM(_NET_WM_ICON);
   INIT_ATOM(_NET_WM_PID);
   INIT_ATOM(_NET_WM_STRUT);
   INIT_ATOM(_NET_WM_STRUT_PARTIAL);
   INIT_ATOM(_NET_MOVERESIZE_WINDOW);
   INIT_ATOM(_NET_CLOSE_WINDOW);
   INIT_ATOM(_NET_WM_ALLOWED_ACTIONS);
   INIT_ATOM(_NET_WM_ACTION_MOVE);
   INIT_ATOM(_NET_WM_ACTION_RESIZE);
   INIT_ATOM(_NET_WM_ACTION_MINIMIZE);
   INIT_ATOM(_NET_WM_ACTION_SHADE);
   INIT_ATOM(_NET_WM_ACTION_STICK);
   INIT_ATOM(_NET_WM_ACTION_MAXIMIZE_HORZ);
   INIT_ATOM(_NET_WM_ACTION_MAXIMIZE_VERT);
   INIT_ATOM(_NET_WM_ACTION_FULLSCREEN);
   INIT_ATOM(_NET_WM_ACTION_CHANGE_DESKTOP);
   INIT_ATOM(_NET_WM_ACTION_CLOSE);
   INIT_ATOM(_NET_NUMBER_OF_DESKTOPS);
   INIT_ATOM(_NET_WM_DESKTOP);
   INIT_ATOM(_NET_CURRENT_DESKTOP);
   INIT_ATOM(_NET_DESKTOP_LAYOUT);
   INIT_ATOM(_NET_SUPPORTED);
   INIT_ATOM(_NET_FRAME_EXTENTS);
   INIT_ATOM(WM_CLASS);
   INIT_ATOM(WM_CLIENT_LEADER);
   INIT_ATOM(WM_DELETE_WINDOW);
   INIT_ATOM(WM_ICON);
   INIT_ATOM(WM_NAME);
   INIT_ATOM(WM_PROTOCOLS);
   INIT_ATOM(WM_STATE);
   INIT_ATOM(WM_TRANSIENT_FOR);
   INIT_ATOM(WM_WINDOW_ROLE);

#  undef INIT_ATOM

#if defined(VM_HAVE_X11_SHAPE_EXT)
   if (!XShapeQueryExtension(up->display, &up->shapeEventBase, &up->shapeErrorBase)) {
      up->shapeEventBase = 0;
   }
#endif

   return up;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformCleanup --
 *
 *      One-time platform-specific cleanup code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformCleanup(UnityPlatform *up) // IN
{
   if (!up) {
      return;
   }

   /*
    * Caller should've called Unity_Exit first.
    */
   ASSERT(!up->isRunning);
   ASSERT(up->glibSource == NULL);

   if (up->specialWindows) {
      HashTable_Free(up->specialWindows);
      up->specialWindows = NULL;
   }
   if (up->allWindows) {
      HashTable_Free(up->allWindows);
      up->allWindows = NULL;
   }

   if (up->display) {
      XCloseDisplay(up->display);
      up->display = NULL;
   }

   free(up->desktopInfo.guestDesktopToUnity);
   up->desktopInfo.guestDesktopToUnity = NULL;
   free(up->desktopInfo.unityDesktopToGuest);
   up->desktopInfo.unityDesktopToGuest = NULL;
   up->desktopWindow = NULL;

   free(up);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformRegisterCaps --
 *
 *      Register guest platform specific capabilities with the VMX.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformRegisterCaps(UnityPlatform *up) // IN
{
   ASSERT(up);

   if (!RpcOut_sendOne(NULL, NULL, UNITY_RPC_SHOW_TASKBAR_CAP " %d",
                       Unity_IsSupported() ? 1 : 0)) {
      Debug("Could not set unity show taskbar cap\n");
   }

   AppUtil_SendGuestCaps(platformUnityCaps, ARRAYSIZE(platformUnityCaps), TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformUnregisterCaps --
 *
 *      Unregister guest platform specific capabilities with the VMX.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformUnregisterCaps(UnityPlatform *up) // IN
{
   /*
    * This function may potentially be called during UnityPlatform destruction.
    */
   if (!up) {
      return;
   }

   AppUtil_SendGuestCaps(platformUnityCaps, ARRAYSIZE(platformUnityCaps), FALSE);

   if (!RpcOut_sendOne(NULL, NULL, UNITY_RPC_SHOW_TASKBAR_CAP " 0")) {
      Debug("Failed to unregister Unity taskbar capability\n");
   }
}


/*****************************************************************************
 * Unity main loop and event handling                                        *
 *****************************************************************************/


/*
 *-----------------------------------------------------------------------------
 *
 * USWindowCreate --
 *
 *      Creates a new UnitySpecialWindow. Ownership of the 'windows' memory is taken over
 *      by the newly created object, but that memory MUST be malloc'd...
 *
 * Results:
 *      New UnitySpecialWindow object.
 *
 * Side effects:
 *      Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

static UnitySpecialWindow *
USWindowCreate(UnityPlatform *up,                    // IN
               UnitySpecialEventHandler evHandler,   // IN
               Window *windows,                      // IN
               int windowCount)                      // IN
{
   UnitySpecialWindow *usw;

   ASSERT(up);

   usw = Util_SafeCalloc(1, sizeof *usw);
   usw->evHandler = evHandler;
   USWindowUpdate(up, usw, windows, windowCount);

   return usw;
}


/*
 *-----------------------------------------------------------------------------
 *
 * USWindowUpdate --
 *
 *      Updates this USWindow with a new list of X windows. Ownership of the 'windows'
 *      memory is taken over by this USWindow object. That memory MUST be malloc'd...
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Old window list may be destroyed and freed.
 *
 *-----------------------------------------------------------------------------
 */

static void
USWindowUpdate(UnityPlatform *up,       // IN
               UnitySpecialWindow *usw, // IN
               Window *windows,         // IN
               int windowCount)         // IN
{
   int i;

   ASSERT(up);
   ASSERT(usw);

   for (i = 0; i < usw->numWindows; i++) {
      XSelectInput(up->display, usw->windows[i], 0);
      HashTable_Delete(up->specialWindows, GUINT_TO_POINTER(usw->windows[i]));
   }

   free(usw->windows);
   usw->windows = windows;
   usw->numWindows = windowCount;

   for (i = 0; i < windowCount; i++) {
      HashTable_Insert(up->specialWindows, GUINT_TO_POINTER(windows[i]), usw);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * USWindowLookup --
 *
 *      Looks up a special window
 *
 * Results:
 *      UnitySpecialWindow object
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static UnitySpecialWindow *
USWindowLookup(UnityPlatform *up, // IN
               Window window)     // IN
{
   UnitySpecialWindow *retval = NULL;

   HashTable_Lookup(up->specialWindows, GUINT_TO_POINTER(window), (void **)&retval);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * USWindowDestroy --
 *
 *      Destroys a UnitySpecialWindow
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

static void
USWindowDestroy(UnityPlatform *up,       // IN
                UnitySpecialWindow *usw) // IN
{
   int i;

   ASSERT(up);
   ASSERT(usw);

   for (i = 0; i < usw->numWindows; i++) {
      HashTable_Delete(up->specialWindows, GUINT_TO_POINTER(usw->windows[i]));

      if (usw->windowsAreOwned) {
         XDestroyWindow(up->display, usw->windows[i]);
      } else {
         /*
          * For now, assume we don't have any special windows that get extension events
          * and need a call like XScreenSaverSelectInput...
          */
         XSelectInput(up->display, usw->windows[i], 0);
      }
   }
   free(usw->windows);

   free(usw);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformMakeRootWindowsObject --
 *
 *      Creates a UnitySpecialWindow to handle the root windows.
 *
 * Results:
 *      UnitySpecialWindow
 *
 * Side effects:
 *      Selects for events on the root windows.
 *
 *-----------------------------------------------------------------------------
 */

static UnitySpecialWindow *
UnityPlatformMakeRootWindowsObject(UnityPlatform *up) // IN
{
   static const long eventMask =
      StructureNotifyMask
      | PropertyChangeMask
      | SubstructureNotifyMask
      | FocusChangeMask;
   int i;
   int numRootWindows;
   Window *rootWindows;

   ASSERT(up);

   numRootWindows = ScreenCount(up->display);
   ASSERT(numRootWindows > 0);

   rootWindows = Util_SafeCalloc(numRootWindows, sizeof rootWindows[0]);
   for (i = 0; i < numRootWindows; i++) {
      rootWindows[i] = RootWindow(up->display, i);
   }

   for (i = 0; i < numRootWindows; i++) {
      XSelectInput(up->display, rootWindows[i], eventMask);
   }

   return USWindowCreate(up, USRootWindowsProcessEvent, rootWindows, numRootWindows);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetErrorCount --
 *
 *      Retrieves the current count of X11 errors received by Unity.
 *
 * Results:
 *      Current error count.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
UnityPlatformGetErrorCount(UnityPlatform *up) // IN
{
   return unityX11ErrorCount;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformResetErrorCount --
 *
 *      Resets the Unity X11 error count to zero.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Error count reset.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformResetErrorCount(UnityPlatform *up) // IN
{
   unityX11ErrorCount = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformXErrorHandler --
 *
 *      Handler for all X event errors.
 *
 * Results:
 *      1.
 *
 * Side effects:
 *      Updates our X11 error counter.
 *
 *-----------------------------------------------------------------------------
 */

static int
UnityPlatformXErrorHandler(Display *dpy,     // IN
                           XErrorEvent *xev) // IN
{
   char buf[1024];
   XGetErrorText(dpy, xev->error_code, buf, sizeof buf);
   Debug("> VMwareUserXErrorHandler: error %s on resource %#lx for request %d\n",
         buf, xev->resourceid, xev->request_code);

   unityX11ErrorCount++;

   return 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetServerTime --
 *
 *      Returns an educated guess at the X server's current timestamp
 *
 * Results:
 *      X server timestamp
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Time
UnityPlatformGetServerTime(UnityPlatform *up) // IN
{
   Time retval;
   struct timeval tv;

   gettimeofday(&tv, NULL);
   retval = up->eventTimeDiff + (tv.tv_sec * 1000) + (tv.tv_usec / 1000);

   Debug("UserTime is guessed at %lu\n", retval);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * ComparePointers --
 *
 *      Compares two pointers to see whether they're equal.
 *
 * Results:
 *      -1, 0, or 1 (same meaning as strcmp return values)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
ComparePointers(const void *p1, // IN
                const void *p2) // IN
{
   /*
    * Helper function for UnityPlatformKillHelperThreads
    */
   const void * const *ptr1 = p1;
   const void * const *ptr2 = p2;
   ptrdiff_t diff = (*ptr2 - *ptr1);

   if (diff < 0) {
      return -1;
   } else if (diff > 0) {
      return 1;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformKillHelperThreads --
 *
 *      Tears down the Unity "running" state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Restores system settings.
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformKillHelperThreads(UnityPlatform *up) // IN
{
   UnityPlatformWindow **upwList;
   UnitySpecialWindow **uswList;
   size_t i;
   size_t numWindows;

   if (!up || !up->isRunning) {
      ASSERT(up->glibSource == NULL);
      return;
   }

   UnityX11EventTeardownSource(up);

   up->desktopInfo.numDesktops = 0; // Zero means host has not set virtual desktop config
   UnityX11RestoreSystemSettings(up);

   HashTable_ToArray(up->allWindows,
                     (void ***)&upwList,
                     &numWindows);
   qsort(upwList, numWindows, sizeof *upwList, ComparePointers);
   for (i = 0; i < numWindows; i++) {
      if (i < (numWindows - 1) && upwList[i] == upwList[i + 1]) {
         continue;
      }

      UPWindow_Unref(up, upwList[i]);
   }
   free(upwList);

   up->workAreas = NULL;
   up->rootWindows = NULL;
   HashTable_ToArray(up->specialWindows,
                     (void ***)&uswList,
                     &numWindows);
   qsort(uswList, numWindows, sizeof *uswList, ComparePointers);
   for (i = 0; i < numWindows; i++) {
      if (i < (numWindows - 1) && uswList[i] == uswList[i + 1]) {
         continue;
      }

      USWindowDestroy(up, uswList[i]);
   }
   free(uswList);

   XSync(up->display, TRUE);
   up->desktopInfo.initialDesktop = UNITY_X11_INITIALDESKTOP_UNSET;
   up->isRunning = FALSE;

   Debug("Leaving unity mode\n");
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityX11GetWMProtocols --
 *
 *      Updates the list of protocols supported by the window manager.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnityX11GetWMProtocols(UnityPlatform *up) // IN
{
   Atom propertyType;
   int propertyFormat;
   unsigned long itemsReturned;
   unsigned long bytesRemaining;
   Atom *valueReturned = NULL;

   ASSERT(up);

   memset(up->wmProtocols, 0, sizeof up->wmProtocols);
   if (XGetWindowProperty(up->display, up->rootWindows->windows[0],
                          up->atoms._NET_SUPPORTED, 0,
                          1024, False, AnyPropertyType,
                          &propertyType, &propertyFormat, &itemsReturned,
                          &bytesRemaining, (unsigned char **)&valueReturned)
      != Success) {
      return;
   }

   if ((propertyType == XA_ATOM || propertyType == XA_CARDINAL)
       && propertyFormat == 32) {
      int i;

      for (i = 0; i < itemsReturned; i++) {
         if (valueReturned[i] == up->atoms._NET_MOVERESIZE_WINDOW) {
            up->wmProtocols[UNITY_X11_WM__NET_MOVERESIZE_WINDOW] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_CLOSE_WINDOW) {
            up->wmProtocols[UNITY_X11_WM__NET_CLOSE_WINDOW] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_RESTACK_WINDOW) {
            up->wmProtocols[UNITY_X11_WM__NET_RESTACK_WINDOW] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_ACTIVE_WINDOW) {
            up->wmProtocols[UNITY_X11_WM__NET_ACTIVE_WINDOW] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_MINIMIZE) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_MINIMIZE] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_CLOSE) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_CLOSE] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_SHADE) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_SHADE] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_STICK) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_STICK] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_FULLSCREEN) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_FULLSCREEN] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_MAXIMIZE_HORZ) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_MAXIMIZE_HORZ] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_ACTION_MAXIMIZE_VERT) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_ACTION_MAXIMIZE_VERT] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_FRAME_EXTENTS) {
            up->wmProtocols[UNITY_X11_WM__NET_FRAME_EXTENTS] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_STRUT_PARTIAL) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_STRUT_PARTIAL] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_STATE_HIDDEN) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_STATE_HIDDEN] = TRUE;
         } else if (valueReturned[i] == up->atoms._NET_WM_STATE_MINIMIZED) {
            up->wmProtocols[UNITY_X11_WM__NET_WM_STATE_MINIMIZED] = TRUE;
         }
      }
   }

   XFree(valueReturned);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformStartHelperThreads --
 *
 *      Start Unity running.
 *
 * Results:
 *      TRUE if successful
 *      FALSE otherwise
 *
 * Side effects:
 *      Saves and changes system settings.
 *      Starts watching for windowing system events.
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityPlatformStartHelperThreads(UnityPlatform *up) // IN
{
   ASSERT(up);
   ASSERT(up->glibSource == NULL);

   XSync(up->display, TRUE);
   up->rootWindows = UnityPlatformMakeRootWindowsObject(up);
   up->isRunning = TRUE;
   up->eventTimeDiff = 0;

   UnityX11SaveSystemSettings(up);

   UnityX11GetWMProtocols(up);

   if (up->desktopInfo.numDesktops) {
      UnityDesktopId activeDesktop;

      UnityPlatformSyncDesktopConfig(up);

      if (up->desktopInfo.initialDesktop != UNITY_X11_INITIALDESKTOP_UNSET) {
         Debug("%s: Setting activeDesktop to initialDesktop (%u).\n", __func__,
               up->desktopInfo.initialDesktop);
         activeDesktop = up->desktopInfo.initialDesktop;
      } else {
         activeDesktop = UnityWindowTracker_GetActiveDesktop(up->tracker);
      }
      if (UnityPlatformSetDesktopActive(up, activeDesktop)) {;
         /*
          * XXX Rather than setting this directly here, should we instead wait for a
          * PropertyNotify event from the window manager to one of the root windows?
          * Doing so may be safer in that it confirms that our request was honored by
          * the window manager.
          */
         UnityWindowTracker_ChangeActiveDesktop(up->tracker, activeDesktop);
      }
   }

   if (up->needWorkAreas) {
      /*
       * UNEXPECTED: The host called SetDesktopWorkArea before entering Unity mode, so we
       * need to go back and apply the remembered work area info.
       */

      UnityPlatformSetDesktopWorkAreas(up, up->needWorkAreas, up->needNumWorkAreas);
      free(up->needWorkAreas);
      up->needWorkAreas = NULL;
   }

   /*
    * Set up a callback in the glib main loop to listen for incoming X events on the
    * unity display connection.
    */
   UnityX11EventEstablishSource(up);

   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformIsUnityRunning --
 *
 *      Check to see if we are still in the unity mode.
 *
 * Results:
 *      TRUE if we are in Unity mode
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityPlatformIsUnityRunning(UnityPlatform *up) // IN
{
   ASSERT(up);

   return up->isRunning;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformLock --
 *
 *      Does nothing - our implementation is not threaded.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None (really).
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformLock(UnityPlatform *up) // IN
{
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformUnlock --
 *
 *      Does nothing - our implementation is not threaded.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
UnityPlatformUnlock(UnityPlatform *up) // IN
{
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformUpdateZOrder --
 *
 *      Push the Z-Order of all windows into the window tracker.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
UnityPlatformUpdateZOrder(UnityPlatform *up) // IN
{
   UnityWindowId *elements;
   size_t numElements;
   UnityPlatformWindow *curWindow;

   ASSERT(up);
   if (!up->stackingChanged) {
      return;
   }

   elements = alloca(UNITY_MAX_WINDOWS * sizeof elements[0]);
   for (numElements = 0, curWindow = up->topWindow;
        curWindow; curWindow = curWindow->lowerWindow) {
      if (curWindow->isRelevant) {
         elements[numElements++] = curWindow->toplevelWindow;
      }
   }

   UnityWindowTracker_SetZOrder(up->tracker,
                                elements,
                                numElements);
   up->stackingChanged = FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformUpdateWindowState --
 *
 *      Walk through /all/ the windows on the guest, pushing everything we know about
 *      them into the unity window tracker.
 *
 * Results:
 *      TRUE indicating we need help from the common code to generate
 *      remove window events (see unity.c)
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityPlatformUpdateWindowState(UnityPlatform *up,               // IN
                               UnityWindowTracker *tracker)     // IN
{
   int curRoot;
   Window lowerWindow = None;

   if (!up || !up->rootWindows) {
      Debug("BUG: UpdateWindowState was called before we are fully in Unity mode...\n");
      return FALSE;
   }

   for (curRoot = 0; curRoot < up->rootWindows->numWindows; curRoot++) {
      int i;
      Window dummyWin;
      Window *children;
      unsigned int numChildren;

      XQueryTree(up->display, up->rootWindows->windows[curRoot],
                 &dummyWin, &dummyWin, &children, &numChildren);

      for (i = 0; i < numChildren; i++) {
         UnityPlatformWindow *upw;

         if (!HashTable_Lookup(up->allWindows,
                               GUINT_TO_POINTER(children[i]),
                               (void **)&upw)) {
            upw = UPWindow_Create(up, children[i]);
            if (!upw) {
               continue; // Window may have disappeared since the XQueryTree
            }
            UPWindow_CheckRelevance(up, upw, NULL);
            UPWindow_Restack(up, upw, lowerWindow);
         }

	 lowerWindow = upw->toplevelWindow;
      }

      XFree(children);
   }

   UnityPlatformUpdateZOrder(up);
   /*
    * up is not populated with the window layout structure when
    * UnityPlatformUpdateDnDDetWnd is intially called.
    */
   UnityPlatformStackDnDDetWnd(up);

   if (up->needTaskbarSetting) {
      up->needTaskbarSetting = FALSE;
      /*
       * This is called in this seemingly random spot to make sure that the taskbar
       * visibility is properly set once we have a full picture of the windowing system
       * state. Although the routine is called prior to this by SaveSystemSettings(), the
       * up->allWindows hash table is not complete until this point, which occurs at a
       * random time of the host's choosing.
       */
      UnityPlatformSetTaskbarVisible(up, up->currentSettings[UNITY_UI_TASKBAR_VISIBLE]);
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityX11HandleEvents --
 *
 *      Handle incoming events
 *
 * Results:
 *      TRUE if the main loop should continue watching for events from the display.
 *
 * Side effects:
 *      Events read from the X display and processed.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
UnityX11HandleEvents(gpointer data) // IN
{
   UnityPlatform *up = (UnityPlatform *) data;
   GList *incomingEvents = NULL;
   Bool restackDetWnd = FALSE;

   ASSERT(up);
   ASSERT(up->isRunning);

   Debug("Starting unity event handling\n");
   while (XEventsQueued(up->display, QueuedAfterFlush)) {
      /*
       * This outer loop is here to make sure we really process all available events
       * before returning.
       */

      while (XEventsQueued(up->display, QueuedAlready)) {
         UnityTemporaryEvent *ev;

         ev = Util_SafeCalloc(1, sizeof *ev);
         XNextEvent(up->display, &ev->xevent);
         ev->realWindowID = UnityPlatformGetRealEventWindow(up, &ev->xevent);

         /*
          * Restack dnd detection window when either
          *   1.  the desktop window may overlap detection window, or
          *   2.  a window is inserted directly above the desktop (and therefore
          *       below the DND window).
          */
         if (up->desktopWindow &&
             ev->xevent.type == ConfigureNotify &&
             (up->desktopWindow->toplevelWindow == ev->realWindowID ||
              up->desktopWindow->toplevelWindow == ev->xevent.xconfigure.above)) {
            restackDetWnd = TRUE;
         }

         if (ev->xevent.type == DestroyNotify) {
            /*
             * Unfortunately, X's event-driven model has an inherent race condition for
             * parties that are observing events on windows that are controlled by other
             * applications. Basically, if we're processing an event on a window, that
             * window may have already been destroyed, and there doesn't seem to really
             * be a way to detect this. We just have to try to cut down the probability
             * of those as much as possible, by discarding any events on a window if
             * they're immediately followed by a DestroyNotify on the same window...
             */
            GList *curItem;
            GList *nextItem = NULL;

            for (curItem = incomingEvents; curItem; curItem = nextItem) {
               UnityTemporaryEvent *otherEvent = curItem->data;
               nextItem = curItem->next;

               if (otherEvent->realWindowID == ev->realWindowID) {
                  free(curItem->data);
                  incomingEvents = g_list_remove_link(incomingEvents, curItem);
               }
            }
         }

         incomingEvents = g_list_append(incomingEvents, ev);
      }

      while (incomingEvents) {
         GList *nextItem;
         UnityTemporaryEvent *tempEvent = incomingEvents->data;

         UnityPlatformProcessXEvent(up, &tempEvent->xevent, tempEvent->realWindowID);

         nextItem = incomingEvents->next;
         free(incomingEvents->data);
         g_list_free_1(incomingEvents);
         incomingEvents = nextItem;
      }

      if (restackDetWnd) {
         UnityPlatformStackDnDDetWnd(up);
      }
      UnityPlatformUpdateZOrder(up);
      UnityPlatformDoUpdate(up, TRUE);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetEventString --
 *
 *      Allows stringifying events for debugging purposes
 *
 * Results:
 *      A stringified version of the event name. It's a static value - do not free this.
 *
 * Side effects:
 *      None.
 *-----------------------------------------------------------------------------
 */

const char *
UnityPlatformGetEventString(UnityPlatform *up, // IN
                            int type)          // IN
{
#if defined(VM_HAVE_X11_SHAPE_EXT)
   if (up->shapeEventBase
       && type == (up->shapeEventBase + ShapeNotify)) {
      return "ShapeNotify";
   }
#endif

   switch (type) {
   case KeyPress: return "KeyPress";
   case KeyRelease: return "KeyRelease";
   case ButtonPress: return "ButtonPress";
   case ButtonRelease: return "ButtonRelease";
   case MotionNotify: return "MotionNotify";
   case EnterNotify: return "EnterNotify";
   case LeaveNotify: return "LeaveNotify";
   case FocusIn: return "FocusIn";
   case FocusOut: return "FocusOut";
   case KeymapNotify: return "KeymapNotify";
   case Expose: return "Expose";
   case GraphicsExpose: return "GraphicsExpose";
   case NoExpose: return "NoExpose";
   case VisibilityNotify: return "VisibilityNotify";
   case CreateNotify: return "CreateNotify";
   case DestroyNotify: return "DestroyNotify";
   case UnmapNotify: return "UnmapNotify";
   case MapNotify: return "MapNotify";
   case MapRequest: return "MapRequest";
   case ReparentNotify: return "ReparentNotify";
   case ConfigureNotify: return "ConfigureNotify";
   case ConfigureRequest: return "ConfigureRequest";
   case GravityNotify: return "GravityNotify";
   case ResizeRequest: return "ResizeRequest";
   case CirculateNotify: return "CirculateNotify";
   case CirculateRequest: return "CirculateRequest";
   case PropertyNotify: return "PropertyNotify";
   case SelectionClear: return "SelectionClear";
   case SelectionRequest: return "SelectionRequest";
   case SelectionNotify: return "SelectionNotify";
   case ColormapNotify: return "ColormapNotify";
   case ClientMessage: return "ClientMessage";
   case MappingNotify: return "MappingNotify";
   default: return "<Unknown>";
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetRealEventWindow --
 *
 *      For debugging purposes, retrieves the window that the event happened on (as
 *      opposed to the window the event was sent to)
 *
 * Results:
 *      The window that the event actually happened on.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Window
UnityPlatformGetRealEventWindow(UnityPlatform *up,    // IN
                                const XEvent *xevent) // IN
{
#if defined(VM_HAVE_X11_SHAPE_EXT)
   if (up->shapeEventBase
       && xevent->type == (up->shapeEventBase + ShapeNotify)) {
      XShapeEvent *sev = (XShapeEvent *) xevent;

      return sev->window;
   }
#endif

   switch (xevent->type) {
   case CreateNotify:
      return xevent->xcreatewindow.window;

   case DestroyNotify:
      return xevent->xdestroywindow.window;

   case MapNotify:
      return xevent->xmap.window;

   case UnmapNotify:
      return xevent->xunmap.window;

   case ReparentNotify:
      return xevent->xreparent.window;

   case ConfigureNotify:
      return xevent->xconfigure.window;

   case CirculateNotify:
      return xevent->xcirculate.window;

   case PropertyNotify:
      return xevent->xproperty.window;

   case FocusIn:
   case FocusOut:
      return xevent->xfocus.window;

   default:
      return xevent->xany.window;

   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformUpdateEventTimeDiff --
 *
 *      Updates our idea of the difference between X server time and our local time.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updated event time diff.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnityPlatformUpdateEventTimeDiff(UnityPlatform *up,    // IN
                                 const XEvent *xevent) // IN
{
   Time serverTime;
   Time localTime;
   struct timeval localTv;

   switch (xevent->type) {
   case KeyPress:
   case KeyRelease:
      serverTime = xevent->xkey.time;
      break;
   case ButtonPress:
   case ButtonRelease:
      serverTime = xevent->xbutton.time;
      break;
   case MotionNotify:
      serverTime = xevent->xmotion.time;
      break;
   case EnterNotify:
   case LeaveNotify:
      serverTime = xevent->xcrossing.time;
      break;
   case PropertyNotify:
      serverTime = xevent->xproperty.time;
      break;
   case SelectionClear:
      serverTime = xevent->xselectionclear.time;
      break;
   case SelectionRequest:
      serverTime = xevent->xselectionrequest.time;
      break;
   case SelectionNotify:
      serverTime = xevent->xselection.time;
      break;

   default:
      return;
   }

   gettimeofday(&localTv, NULL);
   localTime = (localTv.tv_sec * 1000) + (localTv.tv_usec / 1000); // Convert to ms
   up->eventTimeDiff = serverTime - localTime;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformProcessXEvent --
 *
 *      Processes an incoming X event.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May create or destroy UnityPlatformWindow objects.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnityPlatformProcessXEvent(UnityPlatform *up,      // IN
                           const XEvent *xevent,   // IN
                           Window realEventWindow) // IN
{
   UnityPlatformWindow *upw = NULL;
   const char *eventName;

   ASSERT(up);
   ASSERT(xevent);

   UnityPlatformUpdateEventTimeDiff(up, xevent);

   eventName = UnityPlatformGetEventString(up, xevent->type);
   upw = UPWindow_Lookup(up, realEventWindow);
   if (!upw) {
      UnitySpecialWindow *usw = USWindowLookup(up, realEventWindow);
      if (usw) {
         if (usw->evHandler) {
            usw->evHandler(up, usw, xevent, realEventWindow);
         }

         return;
      } else if (xevent->type == CreateNotify) {
         /* Ignore decoration widgets.  They'll be reparented later. */
         if (UnityX11Util_IsWindowDecorationWidget(up, realEventWindow)) {
            Debug("%s: Window %#lx is a decoration widget.  Ignore it.\n",
                  __func__, realEventWindow);
            return;
         }

         /*
          * It may be a new window that we don't know about yet. Let's create an object
          * to track it.
          */
         upw = UPWindow_Create(up, realEventWindow);
         if (upw) {
            UPWindow_CheckRelevance(up, upw, NULL);
         } else {
            Debug("UnityX11ThreadProcessEvent BOMBED:"
                  " %s on window %#lx (reported to %#lx)\n",
                  eventName, realEventWindow, xevent->xany.window);
         }
      } else {
         /*
          * If we use them on non-CreateNotify events, the above lines of code wind up
          * trying to create objects for crazy windows that don't exist...
          */
         Debug("Ignoring %s event on unknown window %#lx (reported to %#lx)\n",
               eventName, realEventWindow, xevent->xany.window);
      }
   }

   if (upw) {
      UPWindow_ProcessEvent(up, upw, realEventWindow, xevent);
      if (upw->deleteWhenSafe) {
         Debug("%s: refs %u, deleteWhenSafe %u\n", __func__, upw->refs,
               upw->deleteWhenSafe);
         UPWindow_Unref(up, upw);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformIsRootWindow --
 *
 *      Checks whether a given window ID is the root window. Necessary because each
 *      screen has a separate root window, which makes checking a little more complicated
 *      than ==.
 *
 * Results:
 *      TRUE if the given window is a root window, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityPlatformIsRootWindow(UnityPlatform *up, // IN
                          Window window)     // IN
{
   ASSERT(up);

   return (USWindowLookup(up, window) == up->rootWindows);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityX11SetCurrentDesktop --
 *
 *      Sets the active virtual desktop.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Changes the virtual desktop.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityX11SetCurrentDesktop(UnityPlatform *up,     // IN
                          uint32 currentDesktop) // IN: guest-side desktop ID
{
   Atom data[5] = {0,0,0,0,0};

   ASSERT(up);
   ASSERT(up->rootWindows->windows);

   up->desktopInfo.currentDesktop = currentDesktop;
   data[0] = currentDesktop;
   data[1] = UnityPlatformGetServerTime(up);
   UnityPlatformSendClientMessage(up,
				  up->rootWindows->windows[0],
				  up->rootWindows->windows[0],
				  up->atoms._NET_CURRENT_DESKTOP,
				  32, 5, data);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityX11GetCurrentDesktop --
 *
 *      Gets the active virtual desktop.
 *
 * Results:
 *      The active virtual desktop. If it cannot be retrieved for any reason, a
 *      reasonable default of '0' will be returned.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
UnityX11GetCurrentDesktop(UnityPlatform *up) // IN
{
   Atom propertyType;
   int propertyFormat;
   unsigned long itemsReturned;
   unsigned long bytesRemaining;
   Atom *valueReturned;
   uint32 currentDesktop;

   ASSERT(up);
   ASSERT(up->rootWindows);

   if (XGetWindowProperty(up->display, up->rootWindows->windows[0],
                          up->atoms._NET_CURRENT_DESKTOP, 0,
                          1024, False, AnyPropertyType,
                          &propertyType, &propertyFormat, &itemsReturned,
                          &bytesRemaining, (unsigned char **)&valueReturned)
       == Success
       && propertyType == XA_CARDINAL
       && propertyFormat == 32) {
      ASSERT(itemsReturned == 1);

      currentDesktop = valueReturned[0];
   } else {
      currentDesktop = 0;
   }
   XFree(valueReturned);

   return currentDesktop;
}


/*
 *-----------------------------------------------------------------------------
 *
 * USRootWindowsUpdateCurrentDesktop --
 *
 *      Looks at the root window to figure out the current desktop
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updates UnityWindowTracker.
 *
 *-----------------------------------------------------------------------------
 */

static void
USRootWindowsUpdateCurrentDesktop(UnityPlatform *up,       // IN
                                  UnitySpecialWindow *usw, // IN
                                  Window window)           // IN
{
   uint32 currentDesktop;
   UnityDesktopId unityDesktop;

   /*
    * XXX right now this is going to break if there are multiple screens in the guest,
    * since each one can have an independant 'current' desktop...
    */

   ASSERT(up);
   currentDesktop = UnityX11GetCurrentDesktop(up);

   if (currentDesktop >= up->desktopInfo.numDesktops) {
      Warning("Active desktop is out of range for some strange reason\n");
      currentDesktop = 0;
   }

   unityDesktop = up->desktopInfo.guestDesktopToUnity[currentDesktop];
   Debug("%s: currentDesktop %u, unityDesktop %u\n", __func__, currentDesktop, unityDesktop);
   UnityWindowTracker_ChangeActiveDesktop(up->tracker, unityDesktop);
}


/*
 *-----------------------------------------------------------------------------
 *
 * USRootWindowsProcessEvent --
 *
 *      Processes an event that occurred on one of the root windows.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
USRootWindowsProcessEvent(UnityPlatform *up,       // IN
                          UnitySpecialWindow *usw, // IN
                          const XEvent *xevent,    // IN
                          Window window)           // IN
{

   /*
    * XXX Do we need to handle situations where the root window changes size? Any other
    * properties?
    */
   switch (xevent->type) {
   case PropertyNotify:
      if (xevent->xproperty.atom == up->atoms._NET_CURRENT_DESKTOP) {
         USRootWindowsUpdateCurrentDesktop(up, usw, window);
      } else if (xevent->xproperty.atom == up->atoms._NET_NUMBER_OF_DESKTOPS) {
         size_t numDesktops;

         numDesktops = UnityPlatformGetNumVirtualDesktops(up);
         if (numDesktops != up->desktopInfo.numDesktops) {
            UnityPlatformSyncDesktopConfig(up);
         }
      } else if (xevent->xproperty.atom == up->atoms._NET_DESKTOP_LAYOUT) {
         Atom layoutData[4];

         UnityPlatformGetVirtualDesktopLayout(up, layoutData);
         if (memcmp(layoutData, up->desktopInfo.layoutData, sizeof layoutData) != 0) {
            UnityPlatformSyncDesktopConfig(up);
         }
      }
      break;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformWMProtocolSupported --
 *
 *      Returns whether the window manager supports a particular protocol.
 *
 * Results:
 *      TRUE if the protocol is supported, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityPlatformWMProtocolSupported(UnityPlatform *up,         // IN
                                 UnityX11WMProtocol proto)  // IN
{
   ASSERT(up);
   ASSERT(proto < UNITY_X11_MAX_WM_PROTOCOLS);

   return up->wmProtocols[proto];
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformSendClientMessageFull --
 *
 *      Sends an XSendEvent.
 *
 * Results:
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */


static void
UnityPlatformSendClientMessageFull(Display *d,        // IN
                                   Window destWindow, // IN: Window to send msg to
                                   Window w,          // IN: What the msg's "To:"
                                                      // header should be, so to speak.
                                   Atom messageType,  // IN
                                   int format,        // IN
                                   int numItems,      // IN
                                   const void *data)  // IN
{
   XClientMessageEvent ev;
   int i;

   memset(&ev, 0, sizeof ev);
   ev.type = ClientMessage;
   ev.window = w;
   ev.message_type = messageType;
   ev.format = format;
   switch (format) {
   case 8:
      ASSERT(numItems <= ARRAYSIZE(ev.data.b));
      for (i = 0; i < numItems; i++) {
         const char *datab = data;
         ev.data.b[i] = datab[i];
      }
      break;
   case 16:
      ASSERT(numItems <= ARRAYSIZE(ev.data.s));
      for (i = 0; i < numItems; i++) {
         const short *datas = data;
         ev.data.s[i] = datas[i];
      }
      break;
   case 32:
      ASSERT(numItems <= ARRAYSIZE(ev.data.l));
      for (i = 0; i < numItems; i++) {
         const Atom *datal = data;
         ev.data.l[i] = datal[i];
      }
      break;
   }
   if (! XSendEvent(d, destWindow, False,
                    PropertyChangeMask|SubstructureRedirectMask|SubstructureNotifyMask, (XEvent *)&ev)) {
      Debug("XSendEvent failed\n");
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSendClientMessage --
 *
 *      Sends an XClientMessageEvent (such as one of the _NET_WM messages)
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformSendClientMessage(UnityPlatform *up, // IN
                               Window destWindow, // IN: Window to actually send msg to
                               Window w,          // IN: What the msg's "To:" header
                                                  // should be, so to speak.
                               Atom messageType,  // IN
                               int format,        // IN
                               int numItems,      // IN
                               const void *data)  // IN
{
   UnityPlatformSendClientMessageFull(up->display, destWindow, w, messageType,
                                      format, numItems, data);
}


/*****************************************************************************
 * Misc Unity RPCs that need to be handled                                   *
 *****************************************************************************/


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformSetTopWindowGroup --
 *
 *      Set the group of windows on top of all others.
 *
 * Results:
 *      TRUE if success,
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityPlatformSetTopWindowGroup(UnityPlatform *up,        // IN: Platform data
                               UnityWindowId *windows,   // IN: array of window ids
                               unsigned int windowCount) // IN: # of windows in the array
{
   Window sibling = None;
   int i;

   ASSERT(up);
   ASSERT(windows);
   ASSERT(windowCount);

   /*
    * Restack everything bottom to top.
    */
   for (i = 0; i < windowCount; i++) {
      UnityPlatformWindow *upw;
      Window curWindow;
      Atom data[5] = {0,0,0,0,0};

      upw = UPWindow_Lookup(up, windows[i]);
      if (!upw) {
         continue;
      }

      curWindow = upw->clientWindow ? upw->clientWindow : upw->toplevelWindow;
      UPWindow_SetUserTime(up, upw);

      if (UnityPlatformWMProtocolSupported(up, UNITY_X11_WM__NET_RESTACK_WINDOW)) {
         data[0] = 2;           // Magic source indicator to give full control
         data[1] = sibling;
         data[2] = Above;

         UnityPlatformSendClientMessage(up, up->rootWindows->windows[0],
                                        curWindow,
                                        up->atoms._NET_RESTACK_WINDOW,
                                        32, 5, data);
      } else {
         XWindowChanges winch = {
            .stack_mode = Above,
            .sibling = sibling
         };
         unsigned int valueMask = CWStackMode;

         if (sibling != None) {
            valueMask |= CWSibling;
         }

         /*
          * As of writing, Metacity doesn't support _NET_RESTACK_WINDOW and
          * will block our attempt to raise a window unless it's active, so
          * we activate the window first.
          */
         if (UnityPlatformWMProtocolSupported(up, UNITY_X11_WM__NET_ACTIVE_WINDOW)) {
            data[0] = 2;           // Magic source indicator to give full control
            data[1] = UnityPlatformGetServerTime(up);
            data[2] = None;
            UnityPlatformSendClientMessage(up, up->rootWindows->windows[0],
                                           curWindow,
                                           up->atoms._NET_ACTIVE_WINDOW,
                                           32, 5, data);
         }

         XReconfigureWMWindow(up->display, upw->toplevelWindow, 0, valueMask, &winch);
      }

      sibling = upw->toplevelWindow;
   }

   XSync(up->display, False);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformDnDSendClientMessage --
 *
 *      XXX This is a hack because UnityPlatformSendClientMessage doesn't work
 *      for dnd windows.
 *      Sends an XClientMessageEvent (such as one of the _NET_WM messages)
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnityPlatformDnDSendClientMessage(UnityPlatform *up, // IN
                                  Window destWindow, // IN: Window to send msg to
                                  Window w,          // IN: What the msg's "To:"
                                                     // header should be, so to speak.
                                  Atom messageType,  // IN
                                  int format,        // IN
                                  int numItems,      // IN
                                  const void *data)  // IN
{
   UnityPlatformSendClientMessageFull(GDK_DISPLAY(), destWindow, w,
                                      messageType, format, numItems, data);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityPlatformStackDnDDetWnd --
 *
 *      Updates the stacking order of the dnd detection window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
UnityPlatformStackDnDDetWnd(UnityPlatform *up)
{
   static const Atom onDesktop[] = { 0xFFFFFFFF, 0, 0, 0, 0 };

   if (!up->dnd.setMode || !up->dnd.detWnd) {
      Debug("%s: DnD not yet initialized.\n", __func__);
      return;
   }

   if (!up->desktopWindow) {
      Debug("Desktop Window not cached. Tracker isn't populated.\n");
      return;
   }

   /* Show the window on every desktop. */
   UnityPlatformDnDSendClientMessage(up,
                                     up->rootWindows->windows[0],
                                     GDK_WINDOW_XWINDOW(up->dnd.detWnd->window),
                                     up->atoms._NET_WM_DESKTOP,
                                     32, 5, onDesktop);

   if (up->desktopWindow) {
      XWindowChanges ch;
      XSetWindowAttributes sa;
      Window desktop = up->desktopWindow->toplevelWindow;

      /* Prevent the window manager from managing our detection window. */
      sa.override_redirect = True;
      XChangeWindowAttributes(GDK_DISPLAY(),
                              GDK_WINDOW_XWINDOW(up->dnd.detWnd->window),
                              CWOverrideRedirect, &sa);

      /* Resize and restack the detection window. */
      ch.x = 0;
      ch.y = 0;
      ch.width = 65535;
      ch.height = 65535;
      ch.sibling = desktop;
      ch.stack_mode = Above;

      XConfigureWindow(GDK_DISPLAY(), GDK_WINDOW_XWINDOW(up->dnd.detWnd->window),
                       CWX|CWY|CWWidth|CWHeight|CWStackMode | CWSibling, &ch);

      Debug("Restacking dnd detection window.\n");
   } else {
      /*
       * Attempt to rely on window manager if we cannot find a window to stack
       * above.
       */
      Atom position[] = { _NET_WM_STATE_ADD,
      up->atoms._NET_WM_STATE_STICKY,
      up->atoms._NET_WM_STATE_BELOW, 0, 0 };

      Debug("Unable to locate desktop window to restack detection window above.\n");
      UnityPlatformDnDSendClientMessage(up,
                                        up->rootWindows->windows[0],
                                        GDK_WINDOW_XWINDOW(up->dnd.detWnd->window),
                                        up->atoms._NET_WM_STATE,
                                        32, 5, position);
      XMoveResizeWindow(GDK_DISPLAY(),
                        GDK_WINDOW_XWINDOW(up->dnd.detWnd->window),
                        0, 0, 65535, 65535);
   }
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformUpdateDnDDetWnd --
 *
 *     Shows/hides a full-screen drag detection wnd for unity guest->host DnD.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

void
UnityPlatformUpdateDnDDetWnd(UnityPlatform *up, // IN
                             Bool show)         // IN
{

   if (!up || !up->dnd.setMode || !up->dnd.detWnd) {
      /*
       * This function may potentially be called during UnityPlatform destruction.
       */
      return;
   }

   if (show) {
      gtk_widget_show(up->dnd.detWnd);
      UnityPlatformStackDnDDetWnd(up);
      Debug("Showing dnd detection window.\n");
   } else {
      gtk_widget_hide(up->dnd.detWnd);
      Debug("Hiding dnd detection window.\n");
   }

   up->dnd.setMode(show);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformSetActiveDnDDetWnd --
 *
 *     Set current full-screen drag detection wnd. The caller retains ownership
 *     of the data. The caller is responsible for updating the active dnd det
 *     wnd.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

void
UnityPlatformSetActiveDnDDetWnd(UnityPlatform *up, // IN
                                UnityDnD *data)      // IN
{
   up->dnd = *data;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformSetDesktopWorkAreas --
 *
 *     Sets the work areas for all screens.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityPlatformSetDesktopWorkAreas(UnityPlatform *up,     // IN
                                 UnityRect workAreas[], // IN
                                 uint32 numWorkAreas)   // IN
{
   int iScreens;
   int i;
   XineramaScreenInfo *screenInfo = NULL;
   int numScreens;
   RegionPtr strutsRegion;
   RegionPtr screenRegion;
   RegionPtr workAreasRegion;
   XID (*strutInfos)[12];
   int numStrutInfos;

   if (!up->rootWindows) {
      /*
       * We're not in Unity mode yet. Save the info until we are.
       */

      up->needWorkAreas = Util_SafeMalloc(numWorkAreas * sizeof *up->needWorkAreas);
      memcpy(up->needWorkAreas, workAreas, numWorkAreas * sizeof *up->needWorkAreas);
      up->needNumWorkAreas = numWorkAreas;
      return TRUE;
   }

   if (!UnityPlatformWMProtocolSupported(up, UNITY_X11_WM__NET_WM_STRUT_PARTIAL)) {
      Debug("Window manager does not support _NET_WM_STRUT_PARTIAL - not setting desktop work area.\n");
      return FALSE;
   }

   ASSERT(up);
   ASSERT(up->rootWindows);

   /*
    * Get the geometry of all attached screens.  If we're running multi-mon,
    * we'll query the Xinerama extension.  Otherwise we just fall back to
    * examining our root window's geometry.
    */
   if (XineramaQueryExtension(up->display, &i, &i)) {
      screenInfo = XineramaQueryScreens(up->display, &numScreens);
   }

   if (!screenInfo) {
      uint32 rootX;
      uint32 rootY;
      uint32 rootWidth;
      uint32 rootHeight;
      uint32 dummy;
      Window winDummy;

      if (numWorkAreas > 1) {
         Debug("Xinerama extension not present, or XineramaQueryScreens failed,"
               " but multiple work areas were requested.\n");
         return FALSE;
      }

      if (!XGetGeometry(up->display, up->rootWindows->windows[0], &winDummy,
                        &rootX, &rootY, &rootWidth, &rootHeight,
                        &dummy, &dummy)) {
         return FALSE;
      }

      screenInfo = Util_SafeCalloc(1, sizeof *screenInfo);
      numScreens = 1;

      screenInfo->x_org = rootX;
      screenInfo->y_org = rootY;
      screenInfo->width = rootWidth;
      screenInfo->height = rootHeight;
   }

   /*
    * New and improved wild'n'crazy scheme to map the host's work area
    * coordinates to a collection of struts.
    *
    * This implementation depends upon the y-x banded rectangles
    * implementation of lib/region.
    *
    * In short, here's how things go:
    *
    *    1.  For each Xinerama screen (or the root window in case we have no
    *        Xinerama) and host work area, a region is created.  A strut
    *        region is then created by subtracting the work area region from
    *        the screen region.
    *
    *    2.  This remaining region will contain between 0 and 4 rectangles,
    *        each of which will be transformed into a strut window.
    *
    *        For each of these rectangles, we infer based on their dimensions
    *        which screen boundary the resulting strut should be bound to.
    *
    *        a.  Boxes touching both the left and right sides of the screen
    *            are either top or bottom struts, depending on whether they
    *            also touch the top or bottom edge.
    *
    *        b.  Any remaining box will touch either the left OR the right
    *            side, but not both.  (Such an irregular layout cannot be
    *            described by the work areas RPC.)  That box's strut will
    *            then be attached to the same side of the screen.
    *
    * While also not perfect, this algorithm should do a better job of creating
    * struts along their correct sides of a screen than its predecessor.  It
    * will let us assume the common case that what we define as a strut attached
    * to the left or right side really should be attached to the left or right,
    * rather than attached to the top or bottom and spanning the height of the
    * display.
    *
    * Pathological case:
    *    1.  Screen geometry: 1280x960.
    *        Left strut: 100px wide, 600px tall.  Touches top of screen.
    *        Right strut: 1180px wide, 100px tall.  Touches top of screen.
    *
    *    2.  Note that these struts touch each other.  We'd interpret the resulting
    *        work area as follows:
    *
    *        Top strut: 1280px wide, 100px tall.
    *        Left strut: 100px wide, 500px tall, starting from y = 100.
    *
    * I believe this sort of layout to be uncommon enough that we can accept
    * failure here.  If we -really- want to get these things right, then
    * we should send strut information explicitly, rather than having the
    * guest try to deduce it from work area geometry.
    */

   /*
    * One strut per screen edge = at most 4 strutInfos.
    */
   strutInfos = alloca(4 * sizeof *strutInfos * numScreens);
   memset(strutInfos, 0, 4 * sizeof *strutInfos * numScreens);
   numStrutInfos = 0;

   for (iScreens = 0; iScreens < numScreens; iScreens++) {
      int iRects;

      xRectangle screenRect;
      xRectangle workAreaRect;

      /*
       * Steps 1a. Create screen, work area regions.
       */
      screenRect.x = screenInfo[iScreens].x_org;
      screenRect.y = screenInfo[iScreens].y_org;
      screenRect.width = screenInfo[iScreens].width;
      screenRect.height = screenInfo[iScreens].height;
      screenRect.info.type = UpdateRect;

      workAreaRect.x = workAreas[iScreens].x;
      workAreaRect.y = workAreas[iScreens].y;
      workAreaRect.width = workAreas[iScreens].width;
      workAreaRect.height = workAreas[iScreens].height;
      workAreaRect.info.type = UpdateRect;

      screenRegion = miRectsToRegion(1, &screenRect, 0);
      workAreasRegion = miRectsToRegion(1, &workAreaRect, 0);

      /*
       * Step 1b.  Create struts region by subtracting work area from screen.
       */
      strutsRegion = miRegionCreate(NULL, 0);
      miSubtract(strutsRegion, screenRegion, workAreasRegion);
      miRegionDestroy(workAreasRegion);
      miRegionDestroy(screenRegion);

      /*
       * Step 2.  Transform struts region rectangles into individual struts.
       */
      for (iRects = 0; iRects < REGION_NUM_RECTS(strutsRegion);
           iRects++, numStrutInfos++) {
         BoxPtr p = REGION_RECTS(strutsRegion) + iRects;
         int bounds = 0;
#define TOUCHES_LEFT          0x1
#define TOUCHES_RIGHT         0x2
#define TOUCHES_TOP           0x4
#define TOUCHES_BOTTOM        0x8

         if (p->x1 == screenRect.x) { bounds |= TOUCHES_LEFT; }
         if (p->x2 == screenRect.x + screenRect.width) { bounds |= TOUCHES_RIGHT; }
         if (p->y1 == screenRect.y) { bounds |= TOUCHES_TOP; }
         if (p->y2 == screenRect.y + screenRect.height) { bounds |= TOUCHES_BOTTOM; }

         /*
          * strutInfos is passed directly to
          * XSetProperty(..._NET_WM_STRUTS_PARTIAL).  I.e., look up that
          * property's entry in NetWM/wm-spec for more info on the indices.
          *
          * I went the switch/case route only because it does a better job
          * (for me) of organizing & showing -all- possible cases.  YMMV.
          *
          * The region code treats rectanges as ranges from [x1,x2) and
          * [y1,y2).  In other words, x2 and y2 are OUTSIDE the region.  I
          * guess it makes calculating widths/heights easier.  However, the
          * strut width/height dimensions are INCLUSIVE, so we'll subtract 1
          * from the "end" (as opposed to "start") value.
          *
          * (Ex:  A 1600x1200 display with a 25px top strut would be marked
          * as top = 25, top_start_x = 0, top_end_x = 1599.)
          */
         switch (bounds) {
         case TOUCHES_LEFT | TOUCHES_RIGHT | TOUCHES_TOP:
            /* Top strut. */
            strutInfos[numStrutInfos][2] = p->y2 - p->y1;
            strutInfos[numStrutInfos][8] = p->x1;
            strutInfos[numStrutInfos][9] = p->x2 - 1;
            break;
         case TOUCHES_LEFT | TOUCHES_RIGHT | TOUCHES_BOTTOM:
            /* Bottom strut. */
            strutInfos[numStrutInfos][3] = p->y2 - p->y1;
            strutInfos[numStrutInfos][10] = p->x1;
            strutInfos[numStrutInfos][11] = p->x2 - 1;
            break;
         case TOUCHES_LEFT:
         case TOUCHES_LEFT | TOUCHES_TOP:
         case TOUCHES_LEFT | TOUCHES_BOTTOM:
         case TOUCHES_LEFT | TOUCHES_TOP | TOUCHES_BOTTOM:
            /* Left strut. */
            strutInfos[numStrutInfos][0] = p->x2 - p->x1;
            strutInfos[numStrutInfos][4] = p->y1;
            strutInfos[numStrutInfos][5] = p->y2 - 1;
            break;
         case TOUCHES_RIGHT:
         case TOUCHES_RIGHT | TOUCHES_TOP:
         case TOUCHES_RIGHT | TOUCHES_BOTTOM:
         case TOUCHES_RIGHT | TOUCHES_TOP | TOUCHES_BOTTOM:
            /* Right strut. */
            strutInfos[numStrutInfos][1] = p->x2 - p->x1;
            strutInfos[numStrutInfos][6] = p->y1;
            strutInfos[numStrutInfos][7] = p->y2 - 1;
            break;
         case TOUCHES_LEFT | TOUCHES_RIGHT | TOUCHES_TOP | TOUCHES_BOTTOM:
            Warning("%s: Struts occupy entire display.", __func__);
            /* FALLTHROUGH */
         default:
            Warning("%s: Irregular strut configuration: bounds %4x\n", __func__, bounds);
            miRegionDestroy(strutsRegion);
            goto out;
            break;
         }
      }
#undef TOUCHES_LEFT
#undef TOUCHES_RIGHT
#undef TOUCHES_TOP
#undef TOUCHES_BOTTOM

      miRegionDestroy(strutsRegion);
   }

   /*
    * The first step is making sure we have enough windows in existence to list the
    * _NET_WM_STRUT_PARTIAL properties for each screen.
    */
   if (!up->workAreas
       || up->workAreas->numWindows != numStrutInfos) {
      Window *newWinList;

      newWinList = Util_SafeCalloc(numStrutInfos, sizeof *newWinList);
      if (up->workAreas) {
         memcpy(newWinList, up->workAreas->windows,
                MIN(numStrutInfos, up->workAreas->numWindows) * sizeof *newWinList);
      }

      /*
       * Destroy unneeded windows
       */
      for (i = numStrutInfos; i < (up->workAreas ? up->workAreas->numWindows : 0); i++) {
         XDestroyWindow(up->display, up->workAreas->windows[i]);
      }

      /*
       * Create additional windows as needed.
       */
      for (i = up->workAreas ? up->workAreas->numWindows : 0; i < numStrutInfos; i++) {
         static const char strutWindowName[] = "vmware-user workarea struts";
         Atom allDesktops = -1;
         newWinList[i] = XCreateWindow(up->display, up->rootWindows->windows[0],
                                       -50, -50, 1, 1, 0, CopyFromParent, InputOnly,
                                       CopyFromParent, 0, NULL);
         XChangeProperty(up->display, newWinList[i], up->atoms._NET_WM_WINDOW_TYPE,
                         XA_ATOM, 32, PropModeReplace,
                         (unsigned char *)&up->atoms._NET_WM_WINDOW_TYPE_DOCK, 1);
         XChangeProperty(up->display, newWinList[i], up->atoms._NET_WM_DESKTOP,
                         XA_CARDINAL, 32, PropModeReplace,
                         (unsigned char *)&allDesktops, 1);
         XStoreName(up->display, newWinList[i], strutWindowName);
         XMapWindow(up->display, newWinList[i]);
      }

      if (up->workAreas) {
         USWindowUpdate(up, up->workAreas, newWinList, numStrutInfos);
      } else {
         up->workAreas = USWindowCreate(up, NULL, newWinList, numStrutInfos);
         up->workAreas->windowsAreOwned = TRUE;
      }
   }

   /*
    * Now actually set the _NET_WM_STRUT_PARTIAL property on our special 'struts'
    * windows.
    */
   for (i = 0; i < numStrutInfos; i++) {
      Window strutWindow;

      strutWindow = up->workAreas->windows[i];

      XChangeProperty(up->display, strutWindow, up->atoms._NET_WM_STRUT_PARTIAL, XA_CARDINAL,
                      32, PropModeReplace, (unsigned char *)strutInfos[i], ARRAYSIZE(strutInfos[i]));
   }

out:
   free(screenInfo);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetNumVirtualDesktops --
 *
 *      Retrieves the number of virtual desktops currently set in the guest.
 *
 * Results:
 *      Number of desktops.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

size_t
UnityPlatformGetNumVirtualDesktops(UnityPlatform *up) // IN
{
   Atom propertyType;
   int propertyFormat;
   unsigned long itemsReturned;
   unsigned long bytesRemaining;
   Atom *valueReturned;
   size_t retval;

   ASSERT(up);
   if (XGetWindowProperty(up->display, up->rootWindows->windows[0],
                          up->atoms._NET_NUMBER_OF_DESKTOPS, 0,
                          1024, False, AnyPropertyType,
                          &propertyType, &propertyFormat, &itemsReturned,
                          &bytesRemaining, (unsigned char **)&valueReturned)
       == Success
       && propertyType == XA_CARDINAL
       && propertyFormat == 32) {
      ASSERT(itemsReturned == 1);

      retval = valueReturned[0];
   } else {
      retval = 1;
   }
   XFree(valueReturned);

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformGetVirtualDesktopLayout --
 *
 *      Retrieves the guest's current virtual desktop layout info, and stores it in
 *      'layoutData' (an array of 4 Atoms).
 *
 * Results:
 *      Desktop layout stored in 'layoutData'
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformGetVirtualDesktopLayout(UnityPlatform *up, // IN
                                     Atom *layoutData)  // OUT
{
   Atom propertyType;
   int propertyFormat;
   unsigned long itemsReturned;
   unsigned long bytesRemaining;
   Atom *valueReturned;

   ASSERT(up);

   layoutData[3] = _NET_WM_TOPLEFT;
   if (XGetWindowProperty(up->display, up->rootWindows->windows[0],
                          up->atoms._NET_DESKTOP_LAYOUT, 0,
                          1024, False, AnyPropertyType,
                          &propertyType, &propertyFormat, &itemsReturned,
                          &bytesRemaining, (unsigned char **)&valueReturned)
       == Success
       && propertyType == XA_CARDINAL
       && propertyFormat == 32) {
      ASSERT(itemsReturned == 3 || itemsReturned == 4);

      memcpy(layoutData,
             valueReturned,
             itemsReturned * sizeof *valueReturned);
   } else {
      layoutData[0] = _NET_WM_ORIENTATION_HORZ;
      layoutData[1] = 0;
      layoutData[2] = 1;
   }
   XFree(valueReturned);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSyncDesktopConfig --
 *
 *      This routine takes the virtual desktop configuration stored in UnityPlatform and
 *      makes sure that the guest's actual virtual desktop configuration matches. This is
 *      done in three situations:
 *        1. Updating the guest's virtual desktop config to match the host's, right after
 *        the host's virtual desktop config has changed.
 *        2. Forcing the guest's virtual desktop config back to the host's, right after
 *        the user uses the guest's pager to alter the guest virtual desktop config.
 *        3. Restoring the guest's virtual desktop configuration when exiting Unity mode.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Guest windows may jump to different virtual desktops if desktops are removed.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformSyncDesktopConfig(UnityPlatform *up) // IN
{
   Atom data[5] = {0, 0, 0, 0, 0};

   ASSERT(up);

   if (!up->rootWindows || !up->display) {
      return; // This function might be called while not in Unity mode
   }

   data[0] = up->desktopInfo.numDesktops;
   UnityPlatformSendClientMessage(up,
				  up->rootWindows->windows[0],
				  up->rootWindows->windows[0],
				  up->atoms._NET_NUMBER_OF_DESKTOPS,
				  32, 5,
				  data);
   XChangeProperty(up->display, up->rootWindows->windows[0],
                   up->atoms._NET_DESKTOP_LAYOUT, XA_CARDINAL,
                   32, PropModeReplace, (unsigned char *)up->desktopInfo.layoutData, 4);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformSetDesktopConfig --
 *
 *     Set the virtual desktop configuration as specified by the host.
 *
 * Results:
 *     Returns TRUE if successful, and FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityPlatformSetDesktopConfig(UnityPlatform *up,                             // IN
                              const UnityVirtualDesktopArray *desktopConfig) // IN
{
   int i;
   int x;
   int y;
   UnityVirtualDesktop minDesktop;
   UnityVirtualDesktop maxDesktop;
   UnityVirtualDesktop desktopSpread;
   int unityDesktopLayout[MAX_VIRT_DESK][MAX_VIRT_DESK];
   int guestDesktopLayout[MAX_VIRT_DESK][MAX_VIRT_DESK];

   ASSERT(up);
   ASSERT(desktopConfig);
   ASSERT(desktopConfig->desktopCount >= 1);

   /*
    * This long section of code mainly exists to verify that the host's virtual desktop
    * setup can be represented on our end, and to figure out how best to do it. We could
    * do this simply, if we didn't have to deal with the possibility of having 5 virtual
    * desktops in a 3x2 layout, which is a very real possibility on Linux hosts...
    */
   memset(unityDesktopLayout, 0xFF, sizeof unityDesktopLayout); // Set all entries to -1
   minDesktop = desktopConfig->desktops[0];
   maxDesktop = minDesktop;
   for (i = 1; i < desktopConfig->desktopCount; i++) {
      if (desktopConfig->desktops[i].x < minDesktop.x) {
         minDesktop.x = desktopConfig->desktops[i].x;
      }
      if (desktopConfig->desktops[i].y < minDesktop.y) {
         minDesktop.y = desktopConfig->desktops[i].y;
      }
      if (desktopConfig->desktops[i].x > maxDesktop.x) {
         maxDesktop.x = desktopConfig->desktops[i].x;
      }
      if (desktopConfig->desktops[i].y > maxDesktop.y) {
         maxDesktop.y = desktopConfig->desktops[i].y;
      }
   }
   desktopSpread.x = maxDesktop.x - minDesktop.x;
   desktopSpread.y = maxDesktop.y - minDesktop.y;

   for (i = 0; i < desktopConfig->desktopCount; i++) {
      int32 localX = desktopConfig->desktops[i].x - minDesktop.x;
      int32 localY = desktopConfig->desktops[i].y - minDesktop.y;

      if (localY >= MAX_VIRT_DESK || localX >= MAX_VIRT_DESK) {
         Warning("Unity virtual desktop layout has holes that are too big to handle\n");
         return FALSE;
      }

      unityDesktopLayout[localX][localY] = i;
   }

   for (x = 0; x < desktopSpread.x; x++) {
      for (y = 0; y < desktopSpread.y; y++) {
         if (unityDesktopLayout[x][y] < 0) {
            Warning("Unity virtual desktop layout has holes that we can't handle.\n");
            return FALSE;
         }
      }
   }

   /*
    * Check along the left edge to make sure that there aren't any gaps between virtual
    * desktops.
    */
   for (x = desktopSpread.x, y = 0; y <= desktopSpread.y; y++) {
      if (unityDesktopLayout[x][y] < 0) {
         break;
      }
   }
   for (; y <= desktopSpread.y; y++) {
      if (unityDesktopLayout[x][y] >= 0) {
         Warning("Unity virtual desktop layout has holes along the right edge.\n");
         return FALSE;
      }
   }

   /*
    * Check along the bottom edge to make sure that there aren't any gaps between virtual
    * desktops.
    */
   for (y = desktopSpread.y, x = 0; x <= desktopSpread.x; x++) {
      if (unityDesktopLayout[x][y] < 0) {
         break;
      }
   }
   for (; x <= desktopSpread.x; x++) {
      if (unityDesktopLayout[x][y] >= 0) {
         Warning("Unity virtual desktop layout has holes along the bottom edge.\n");
         return FALSE;
      }
   }

   /*
    * Now we know we have a workable virtual desktop layout - let's figure out how to
    * communicate it to the window manager & pager.
    */
   up->desktopInfo.layoutData[0] = _NET_WM_ORIENTATION_HORZ; // Orientation
   up->desktopInfo.layoutData[1] = (desktopSpread.x + 1); // # of columns
   up->desktopInfo.layoutData[2] = (desktopSpread.y + 1); // # of rows
   up->desktopInfo.layoutData[3] = _NET_WM_TOPLEFT; // Starting corner

   if (((desktopSpread.x + 1) * (desktopSpread.y + 1)) >= desktopConfig->desktopCount
       && desktopSpread.x > 0
       && desktopSpread.y > 1
       && unityDesktopLayout[desktopSpread.x][desktopSpread.y - 1] < 0) {
      /*
       * We know there is are least two holes at the end of the layout, /and/ the holes
       * go up the right side, so therefore we need to use vertical orientation for the
       * EWMH layout.
       */
      up->desktopInfo.layoutData[0] = _NET_WM_ORIENTATION_VERT;
   }

   /*
    * Figure out what the guest-side desktop IDs will be, based on our chosen
    * orientation.
    */
   i = 0;
   memset(guestDesktopLayout, 0xFF, sizeof guestDesktopLayout); // Set all entries to -1
   if (up->desktopInfo.layoutData[0] == _NET_WM_ORIENTATION_HORZ) {
      for (y = 0; y <= desktopSpread.y; y++) {
         for (x = 0; x <= desktopSpread.x; x++) {
            if (unityDesktopLayout[x][y] >= 0) {
               guestDesktopLayout[x][y] = i++;
            }
         }
      }
   } else {
      for (x = 0; x <= desktopSpread.x; x++) {
         for (y = 0; y <= desktopSpread.y; y++) {
            if (unityDesktopLayout[x][y] >= 0) {
               guestDesktopLayout[x][y] = i++;
            }
         }
      }
   }

   up->desktopInfo.numDesktops = desktopConfig->desktopCount;

   /*
    * Build tables to translate between guest-side and Unity-side desktop IDs.
    */
   up->desktopInfo.guestDesktopToUnity =
      Util_SafeRealloc(up->desktopInfo.guestDesktopToUnity,
                       up->desktopInfo.numDesktops
                       * sizeof up->desktopInfo.guestDesktopToUnity[0]);
   up->desktopInfo.unityDesktopToGuest =
      Util_SafeRealloc(up->desktopInfo.unityDesktopToGuest,
                       up->desktopInfo.numDesktops
                       * sizeof up->desktopInfo.unityDesktopToGuest[0]);
   for (i = 0; i < up->desktopInfo.numDesktops; i++) {
      int guestNum;
      UnityVirtualDesktop curDesk = desktopConfig->desktops[i];

      guestNum = guestDesktopLayout[curDesk.x - minDesktop.x][curDesk.y - minDesktop.y];
      up->desktopInfo.guestDesktopToUnity[guestNum] = i;
      up->desktopInfo.unityDesktopToGuest[i] = guestNum;
   }

   /*
    * Make the configuration actually take effect.
    */
   UnityPlatformSyncDesktopConfig(up);

   return TRUE;
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformSetInitialDesktop --
 *
 *     Set a desktop specified by the desktop id as the initial state.
 *
 * Results:
 *     Returns TRUE if successful, and FALSE otherwise.
 *
 * Side effects:
 *     Some windows might be hidden and some shown.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityPlatformSetInitialDesktop(UnityPlatform *up,         // IN
                               UnityDesktopId desktopId)  // IN
{
   ASSERT(up);
   up->desktopInfo.initialDesktop = desktopId;
   return UnityPlatformSetDesktopActive(up, desktopId);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformSetDesktopActive --
 *
 *     Switch to the specified virtual desktop. The desktopId is an index
 *     into the desktop configuration array.
 *
 * Results:
 *     Returns TRUE if successful, and FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityPlatformSetDesktopActive(UnityPlatform *up,         // IN
                              UnityDesktopId desktopId)  // IN
{
   ASSERT(up);

   /*
    * Update the uwt with the new active desktop info.
    */

   UnityWindowTracker_ChangeActiveDesktop(up->tracker, desktopId);

   if (desktopId >= up->desktopInfo.numDesktops) {
      return FALSE;
   }

   if (!up->rootWindows) {
      /*
       * We may not be into Unity mode yet, but we pretend it succeeded, and then do the
       * switch later for real.
       */
      return TRUE;
   }

   UnityX11SetCurrentDesktop(up, up->desktopInfo.unityDesktopToGuest[desktopId]);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformDoUpdate --
 *
 *      This function is used to (possibly asynchronously) collect Unity window
 *      updates and send them to the host via the RPCI update channel.
 *
 * Results:
 *      Updates will be collected.  Updates may be sent.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformDoUpdate(UnityPlatform *up,        // IN:
                      Bool incremental)         // IN: Incremental vs. full update
{
   ASSERT(up);
   ASSERT(up->updateChannel);

   if (!incremental) {
      UnityPlatformUpdateWindowState(up, up->tracker);
   }

   UnityWindowTracker_RequestUpdates(up->tracker,
                                     incremental ? UNITY_UPDATE_INCREMENTAL : 0,
                                     &up->updateChannel->updates);

   /*
    * Notify the host iff UnityWindowTracker_RequestUpdates pushed a valid
    * update into the UpdateChannel buffer.
    */
   if (DynBuf_GetSize(&up->updateChannel->updates) > (up->updateChannel->cmdSize + 2)) {
#ifdef VMX86_DEBUG
      const char *dataBuf = DynBuf_Get(&up->updateChannel->updates);
      size_t dataSize = DynBuf_GetSize(&up->updateChannel->updates);
      ASSERT(dataBuf[up->updateChannel->cmdSize] != '\0');
      ASSERT(dataBuf[dataSize - 1] == '\0');
#endif

      /* The update must be double nul terminated. */
      DynBuf_AppendString(&up->updateChannel->updates, "");

      if (!UnitySendUpdates(up->updateChannel)) {
         /* XXX We should probably exit Unity. */
         Debug("UPDATE TRANSMISSION FAILED! --------------------\n");
         /*
          * At this point, the update buffer contains a stream of updates
          * terminated by a double nul. Rather than flush the input stream,
          * let's "unseal" it by removing the second nul, allowing further
          * updates to be appended and sent later.
          */
         DynBuf_SetSize(&up->updateChannel->updates,
                        DynBuf_GetSize(&up->updateChannel->updates) - 1);
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_UnityToLocalPoint --
 *
 *      Initializes localPt structure based on the input unityPt structure.
 *      Translate point from Unity coordinate to local coordinate.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *----------------------------------------------------------------------------
 */

void
Unity_UnityToLocalPoint(UnityPoint *localPt, // IN/OUT
                        UnityPoint *unityPt) // IN
{
   ASSERT(localPt);
   ASSERT(unityPt);
   localPt->x = unityPt->x;
   localPt->y = unityPt->y;
}


/*
 *----------------------------------------------------------------------------
 *
 * Unity_LocalToUnityPoint --
 *
 *      Initializes unityPt structure based on the input localPt structure.
 *      Translate point from local coordinate to Unity coordinate.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *----------------------------------------------------------------------------
 */

void
Unity_LocalToUnityPoint(UnityPoint *unityPt, // IN/OUT
                        UnityPoint *localPt) // IN
{
   ASSERT(unityPt);
   ASSERT(localPt);
   unityPt->x = localPt->x;
   unityPt->y = localPt->y;
}


/*
 ******************************************************************************
 * UnityPlatformStickWindow --                                           */ /**
 *
 * @brief "Stick" a window to the desktop.
 *
 * @param[in] up       Platform context.
 * @param[in] windowId Operand window.
 *
 * @retval TRUE  Success.
 * @retval FALSE Failure.
 *
 ******************************************************************************
 */

Bool
UnityPlatformStickWindow(UnityPlatform *up,      // IN
                         UnityWindowId windowId) // IN
{
   return SetWindowStickiness(up, windowId, TRUE);
}


/*
 ******************************************************************************
 * UnityPlatformUnstickWindow --                                         */ /**
 *
 * @brief "Unstick" a window from the desktop.
 *
 * @param[in] up       Platform context.
 * @param[in] windowId Operand window.
 *
 * @retval TRUE  Success.
 * @retval FALSE Failure.
 *
 ******************************************************************************
 */

Bool
UnityPlatformUnstickWindow(UnityPlatform *up,      // IN
                           UnityWindowId windowId) // IN
{
   return SetWindowStickiness(up, windowId, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSetConfigDesktopColor --
 *
 *      Set the preferred desktop background color for use when in Unity Mode.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityPlatformSetConfigDesktopColor(UnityPlatform *up, int desktopColor)
{
   ASSERT(up);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformRequestWindowContents --
 *
 *     Validate the list of supplied window IDs and once validated add them to a list
 *     of windows whose contents should be sent to the host.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityPlatformRequestWindowContents(UnityPlatform *up,
                                   UnityWindowId windowIds[],
                                   uint32 numWindowIds)
{
   ASSERT(up);

   /* Not implemented */
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformConfirmMinimizeOperation --
 *
 *     Minimize a window (if allowed) by the host.
 *
 * Results:
 *     Returns TRUE if successful, and FALSE otherwise.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
UnityPlatformConfirmMinimizeOperation(UnityPlatform *up,        // IN
                                      UnityWindowId windowId,   // IN
                                      uint32 sequence,          // IN
                                      Bool allow)               // IN
{
   ASSERT(up);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPlatformSetInterlockMinimizeOperation --
 *
 *     Enable (or Disable) the interlocking (relaying) of minimize operations
 *     through the host.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

void UnityPlatformSetInterlockMinimizeOperation(UnityPlatform *up,   // IN
                                                Bool enabled)        // IN
{
   ASSERT(up);
}


/*
 *------------------------------------------------------------------------------
 *
 * UnityPlatformWillRemoveWindow --
 *
 *    Called when a window is removed from the UnityWindowTracker.
 *
 *    NOTE: This function is called with the platform lock held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *------------------------------------------------------------------------------
 */

void
UnityPlatformWillRemoveWindow(UnityPlatform *up,      // IN
                              UnityWindowId windowId) // IN
{
   ASSERT(up);
}


/*
 ******************************************************************************
 * Begin file-scope functions.
 *
 */


/*
 ******************************************************************************
 * GetRelevantWMWindow --                                                */ /**
 *
 * @brief Given a UnityWindowId, return the X11 window relevant to WM operations.
 *
 * Starting with a Unity window, look for and return its associated
 * clientWindow.  If there is no clientWindow, then return the top-level window.
 *
 * @param[in]  up        Unity/X11 context.
 * @param[in]  windowId  Window search for.
 * @param[out] wmWindow  Set to the relevant X11 window.
 *
 * @todo Consider exporting this for use in unityPlatformX11Window.c.
 *
 * @retval TRUE  Relevant window found and recorded in @a wmWindow.
 * @retval FALSE Unable to find @a windowId.
 *
 ******************************************************************************
 */

static Bool
GetRelevantWMWindow(UnityPlatform *up,          // IN
                    UnityWindowId windowId,     // IN
                    Window *wmWindow)           // OUT
{
   UnityPlatformWindow *upw;

   ASSERT(up);

   upw = UPWindow_Lookup(up, windowId);
   if (!upw) {
      return FALSE;
   }

   *wmWindow = upw->clientWindow ? upw->clientWindow : upw->toplevelWindow;
   return TRUE;
}


/*
 ******************************************************************************
 * SetWindowStickiness --                                                */ /**
 *
 * @brief Sets or clears a window's sticky state.
 *
 * @param[in] up         Unity/X11 context.
 * @param[in] windowId   Operand window.
 * @param[in] wantSticky Set to TRUE to stick a window, FALSE to unstick it.
 *
 * @retval TRUE  Request successfully sent to X server.
 * @retval FALSE Request failed.
 *
 ******************************************************************************
 */

static Bool
SetWindowStickiness(UnityPlatform *up,          // IN
                    UnityWindowId windowId,     // IN
                    Bool wantSticky)            // IN
{
   GdkWindow *gdkWindow;
   Window curWindow;

   ASSERT(up);

   if (!GetRelevantWMWindow(up, windowId, &curWindow)) {
      Debug("%s: Lookup against window %#x failed.\n", __func__, windowId);
      return FALSE;
   }

   gdkWindow = gdk_window_foreign_new(curWindow);
   if (gdkWindow == NULL) {
      Debug("%s: Unable to create Gdk window?! (%#x)\n", __func__, windowId);
      return FALSE;
   }

   if (wantSticky) {
      gdk_window_stick(gdkWindow);
   } else {
      gdk_window_unstick(gdkWindow);
   }

   gdk_flush();
   g_object_unref(G_OBJECT(gdkWindow));

   return TRUE;
}


/*
 *
 * End file-scope functions.
 ******************************************************************************
 */
