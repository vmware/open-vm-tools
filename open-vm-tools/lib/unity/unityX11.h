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

/*
 * unityX11.h --
 *
 *    Internal state shared between the various modules that implement Unity for X11.
 */

#ifndef _UNITY_X11_H_
#define _UNITY_X11_H_

/*
 * It's necessary to include glib, gtk+, and Xlib before the rest of the header files,
 * and then #undef bool. This is because Xlib has '#define Bool int', while
 * vm_basic_types.h has 'typedef char Bool;'.
 */
#include <glib.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/scrnsaver.h>
#if defined(ScreenSaverMajorVersion) && defined(ScreenSaverMinorVersion)
#define VM_HAVE_X11_SS_EXT 1
#endif
#include <X11/extensions/shape.h>
#if defined(ShapeNumberEvents)
#define VM_HAVE_X11_SHAPE_EXT 1
#endif
#undef Bool

#include "vmware.h"
#include "rpcin.h"
#include "rpcout.h"
#include "unity.h"
#include "unityCommon.h"
#include "unityPlatform.h"
#include "debug.h"
#include "str.h"
#include "strutil.h"
#include "util.h"
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

/*
 * These defines are listed in the EWMH spec, but not available in any header file that I
 * know of.
 */
#ifndef _NET_WM_ORIENTATION_HORZ
#define _NET_WM_ORIENTATION_HORZ 0
#define _NET_WM_ORIENTATION_VERT 1

#define _NET_WM_TOPLEFT     0
#define _NET_WM_TOPRIGHT    1
#define _NET_WM_BOTTOMRIGHT 2
#define _NET_WM_BOTTOMLEFT  3

#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#endif

/*
 * These describe system settings saved when entering Unity mode, and restored upon
 * exiting.
 */
typedef enum {
   UNITY_UI_SCREENSAVER,
   UNITY_UI_TASKBAR_VISIBLE,
#ifdef VM_UNIMPLEMENTED_UNITY_SETTINGS
   UNITY_UI_DROP_SHADOW,
   UNITY_UI_MENU_ANIMATION,
   UNITY_UI_TOOLTIP_ANIMATION,
   UNITY_UI_WINDOW_ANIMATION,
   UNITY_UI_FULL_WINDOW_DRAG,
#endif

   UNITY_UI_MAX_SETTINGS
} UnityUISetting;

typedef enum {
   UNITY_X11_WM__NET_MOVERESIZE_WINDOW,
   UNITY_X11_WM__NET_CLOSE_WINDOW,
   UNITY_X11_WM__NET_RESTACK_WINDOW,
   UNITY_X11_WM__NET_ACTIVE_WINDOW,
   UNITY_X11_WM__NET_WM_ACTION_MINIMIZE,
   UNITY_X11_WM__NET_WM_ACTION_CLOSE,
   UNITY_X11_WM__NET_WM_ACTION_SHADE,
   UNITY_X11_WM__NET_WM_ACTION_STICK,
   UNITY_X11_WM__NET_WM_ACTION_FULLSCREEN,
   UNITY_X11_WM__NET_WM_ACTION_MAXIMIZE_HORZ,
   UNITY_X11_WM__NET_WM_ACTION_MAXIMIZE_VERT,
   UNITY_X11_WM__NET_FRAME_EXTENTS,
   UNITY_X11_WM__NET_WM_STRUT_PARTIAL,
   UNITY_X11_WM__NET_WM_STATE_HIDDEN,
   UNITY_X11_WM__NET_WM_STATE_MINIMIZED,

   UNITY_X11_MAX_WM_PROTOCOLS
} UnityX11WMProtocol;

typedef enum {
   UNITY_X11_WIN_WM_DELETE_WINDOW,
   UNITY_X11_MAX_WIN_PROTOCOLS
} UnityX11WinProtocol;


/*
 * UnitySpecialWindow objects track windows that need special treatment, including root
 * windows and windows that we create.
 */
typedef struct UnitySpecialWindow UnitySpecialWindow;
typedef void (*UnitySpecialEventHandler)(UnityPlatform *up,
                                         UnitySpecialWindow *usw,
                                         const XEvent *xevent,
                                         Window realEventWindow);

struct UnitySpecialWindow {
   UnitySpecialEventHandler evHandler;
   Window *windows;
   size_t numWindows;
   Bool windowsAreOwned;
};

typedef struct UnityPlatformWindow UnityPlatformWindow;

/*
 * Holds platform-specific data.
 */
struct _UnityPlatform {
   Display *display; // X11 display object
   long eventTimeDiff; // Diff between X server time and our local time
   guint unityDisplayWatchID; // Manages listening to the display from the glib main loop

   struct { // Atoms that we'll find useful
      Atom _NET_WM_WINDOW_TYPE,
      _NET_WM_WINDOW_TYPE_DESKTOP,
      _NET_WM_WINDOW_TYPE_DOCK,
      _NET_WM_WINDOW_TYPE_TOOLBAR,
      _NET_WM_WINDOW_TYPE_TOOLTIP,
      _NET_WM_WINDOW_TYPE_DROPDOWN_MENU,
      _NET_WM_WINDOW_TYPE_POPUP_MENU,
      _NET_WM_WINDOW_TYPE_MENU,
      _NET_WM_WINDOW_TYPE_UTILITY,
      _NET_WM_WINDOW_TYPE_SPLASH,
      _NET_WM_WINDOW_TYPE_DIALOG,
      _NET_WM_WINDOW_TYPE_NORMAL,
      _NET_WM_WINDOW_TYPE_DND,
      _NET_WM_ALLOWED_ACTIONS,
      _NET_WM_ACTION_MOVE,
      _NET_WM_ACTION_RESIZE,
      _NET_WM_ACTION_MINIMIZE,
      _NET_WM_ACTION_SHADE,
      _NET_WM_ACTION_STICK,
      _NET_WM_ACTION_MAXIMIZE_HORZ,
      _NET_WM_ACTION_MAXIMIZE_VERT,
      _NET_WM_ACTION_FULLSCREEN,
      _NET_WM_ACTION_CHANGE_DESKTOP,
      _NET_WM_ACTION_CLOSE,
      _NET_WM_STATE,
      _NET_WM_STATE_HIDDEN,
      _NET_WM_STATE_MODAL,
      _NET_WM_STATE_STICKY,
      _NET_WM_STATE_MINIMIZED,
      _NET_WM_STATE_MAXIMIZED_HORZ,
      _NET_WM_STATE_MAXIMIZED_VERT,
      _NET_WM_STATE_SHADED,
      _NET_WM_STATE_SKIP_TASKBAR,
      _NET_WM_STATE_SKIP_PAGER,
      _NET_WM_STATE_FULLSCREEN,
      _NET_WM_STATE_ABOVE,
      _NET_WM_STATE_BELOW,
      _NET_WM_STATE_DEMANDS_ATTENTION,
      _NET_WM_USER_TIME,
      _NET_WM_USER_TIME_WINDOW,
      _NET_ACTIVE_WINDOW,
      _NET_RESTACK_WINDOW,
      _NET_WM_PID,
      _NET_WM_ICON,
      _NET_MOVERESIZE_WINDOW,
      _NET_CLOSE_WINDOW,
      _NET_WM_STRUT,
      _NET_WM_STRUT_PARTIAL,
      _NET_NUMBER_OF_DESKTOPS,
      _NET_WM_DESKTOP,
      _NET_CURRENT_DESKTOP,
      _NET_DESKTOP_LAYOUT,
      _NET_SUPPORTED,
      _NET_FRAME_EXTENTS,
      WM_CLASS,
      WM_DELETE_WINDOW,
      WM_ICON,
      WM_NAME,
      WM_PROTOCOLS,
      WM_STATE,
      WM_TRANSIENT_FOR;
   } atoms;

   UnityWindowTracker *tracker;
   UnityUpdateThreadData updateData;

   /*
    * This tracks all toplevel windows, whether or not they are showing through to the
    * window tracker. It also has entries for client windows (which point to the same
    * UnityPlatformWindow objects).
    */
   HashTable *allWindows;
   UnityPlatformWindow *topWindow; // For tracking Z-ordering

   /*
    * This tracks "special" windows, including root windows, the DnD detection window,
    * and the work area-faking window(s). It's mainly used to make sure these windows
    * get proper event handling.
    */
   HashTable *specialWindows;
   UnitySpecialWindow *rootWindows;

   UnityPlatformWindow *desktopWindow;
   UnityDnD dnd;

   UnitySpecialWindow *workAreas;

   UnityRect *needWorkAreas;
   int needNumWorkAreas;

   struct {
      /*
       * Hopefully, most of the time the desktopIDs on host and guest
       * will be the same, but we can't count on it, so these two arrays
       * translate back and forth between guest and unity desktop IDs.
       */
      UnityDesktopId *guestDesktopToUnity;
      uint32 *unityDesktopToGuest;
      size_t numDesktops;

      Atom layoutData[4];

      Atom savedLayoutData[4];
      size_t savedNumDesktops;
      uint32 savedCurrentDesktop;
      uint32 currentDesktop;
   } desktopInfo;

   Bool isRunning;
   Bool stackingChanged;

   Bool haveOriginalSettings;
   Bool currentSettings[UNITY_UI_MAX_SETTINGS];
   Bool originalSettings[UNITY_UI_MAX_SETTINGS];
   Bool needTaskbarSetting;

   int savedScreenSaverTimeout;

   Bool wmProtocols[UNITY_X11_MAX_WM_PROTOCOLS];

   int shapeEventBase;
   int shapeErrorBase;
};

/*
 * Holds per-window platform-specific data.
 */

struct UnityPlatformWindow {
   int refs;

   /*
    * In X11, we want to watch both the top-level window (normally created by the window
    * manager) and the application's window.  The UnityWindowId will correspond to the
    * former - to save lookups later, we track both of them here, as well as their root
    * window.
    */
   Window toplevelWindow;
   Window clientWindow;
   Window rootWindow;
   int screenNumber;
   int desktopNumber;
   UnityPlatformWindow *higherWindow;
   UnityPlatformWindow *lowerWindow;

   UnityWindowType windowType;

   struct {
      DynBuf data;
      UnityIconSize size;
      UnityIconType type;
   } iconPng;

   XConfigureEvent *lastConfigureEvent; // Used for replying to MoveResizeWindow
   Bool windowProtocols[UNITY_X11_MAX_WIN_PROTOCOLS];

   Bool isRelevant; // Whether the window is relayed through the window tracker
   Bool isOverrideRedirect; // Allow detecting changes in this attr
   Bool isViewable;
   Bool wasViewable;
   Bool wantInputFocus;

   /*
    * Mini state-machine:
    *
    * isMaximized, isMinimized, and isHidden can all be set or unset independently
    * from the host's perspective.
    *
    * isMaximized can pretty much be set independently.
    * Leaving isHidden requires WM_HINTS to be set correctly.
    * Entering isHidden requires XWithdrawWindow.
    * If !isHidden, !isMinimized -> isMinimized requires XIconifyWindow()
    * If !isHidden, isMinimized -> !isMinimized requires just mapping the window.
    */
   Bool isHidden;
   Bool isMinimized;
   Bool isMaximized;
};

/*
 * Implemented by unityPlatformX11Window.c
 */
void UPWindow_ProcessEvent(UnityPlatform *up,
                           UnityPlatformWindow *upw,
                           Window realEventWindow,
                           const XEvent *xevent);
UnityPlatformWindow *UPWindow_Create(UnityPlatform *up, Window window);
void UPWindow_CheckRelevance(UnityPlatform *up,
                             UnityPlatformWindow *upw,
                             const XEvent *motivator);
void UPWindow_Ref(UnityPlatform *up, UnityPlatformWindow *upw);
void UPWindow_Restack(UnityPlatform *up, UnityPlatformWindow *upw, Window above);
void UPWindow_Unref(UnityPlatform *up, UnityPlatformWindow *upw);
Bool UPWindow_ProtocolSupported(const UnityPlatform *up,
                                const UnityPlatformWindow *upw,
                                UnityX11WinProtocol proto);
UnityPlatformWindow *UPWindow_Lookup(UnityPlatform *up, Window window);
void UPWindow_SetUserTime(UnityPlatform *up,
                          UnityPlatformWindow *upw);

/*
 * Implemented by unityPlatformX11.c
 */
Bool UnityPlatformWMProtocolSupported(UnityPlatform *up, UnityX11WMProtocol proto);
Bool UnityPlatformIsRootWindow(UnityPlatform *up, Window window);
void UnityPlatformSendPendingUpdates(UnityPlatform *up, int flags);
uint32 UnityX11GetCurrentDesktop(UnityPlatform *up);
void UnityX11SetCurrentDesktop(UnityPlatform *up, uint32 currentDesktop);
Time UnityPlatformGetServerTime(UnityPlatform *up);

#if GTK_MAJOR_VERSION >= 2
#   define UnityPlatformProcessMainLoop() g_main_context_iteration(NULL, TRUE)
#else
#   define UnityPlatformProcessMainLoop() gtk_main_iteration()
#endif

int UnityPlatformGetErrorCount(UnityPlatform *up);
void UnityPlatformResetErrorCount(UnityPlatform *up);
void UnityPlatformDumpUpdate(UnityPlatform *up);
Bool UnityPlatformSetTaskbarVisible(UnityPlatform *up, Bool currentSetting);
void UnityPlatformSendClientMessage(UnityPlatform *up, Window destWindow,
				    Window w, Atom messageType,
				    int format, int numItems, const void *data);
void UnityPlatformSyncDesktopConfig(UnityPlatform *up);
void UnityX11SaveSystemSettings(UnityPlatform *up);
void UnityX11RestoreSystemSettings(UnityPlatform *up);
size_t UnityPlatformGetNumVirtualDesktops(UnityPlatform *up);
void UnityPlatformGetVirtualDesktopLayout(UnityPlatform *up, Atom *layoutData);
const char *UnityPlatformGetEventString(UnityPlatform *up, int type);

#endif
