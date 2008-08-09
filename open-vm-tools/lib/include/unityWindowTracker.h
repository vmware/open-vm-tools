/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * unityWindowTracker.h --
 *
 *    Used to buffer state about a window manager.
 *
 *    In general, clients will notify the window tracker of changes to the window
 *    manager state via:
 *
 *       UnityWindowTracker_AddWindow
 *       UnityWindowTracker_RemoveWindow
 *       UnityWindowTracker_MoveWindow
 *       UnityWindowTracker_ChangeWindowRegion
 *       etc. etc.
 *
 *    And then call UnityWindowTracker_RequestUpdates to pull a summary of the updates
 *    out. The user may call the AddWindow, RemoveWindow, etc functions as often as they
 *    like. The window tracker's job is to compress a series of notifications into the
 *    smallest delta between updates.
 *
 *    Typical use works something like:
 *
 *         UnityWindowTracker_Init(...)
 *         many times do {
 *              many times to {
 *                 UnityWindowTracker_AddWindow, UnityWindowTracker_MoveWindow, etc.
 *              }
 *              UnityWindowTracker_RequestUpdates(...)
 *         }
 *         UnityWindowTracker_Cleanup(...)
 *
 */

#ifndef _UNITY_WINDOW_TRACKER_H_
#define _UNITY_WINDOW_TRACKER_H_

#include "hashTable.h"
#include "region.h"
#include "dynbuf.h"
#include "unityCommon.h"

/* The maximum number of windows that this tracker can track */
#define UNITY_MAX_WINDOWS           1024

/*
 * UNITY_CHANGE_* track changes to windows as we're accumulating state for
 * an update.
 */
#define UNITY_CHANGED_POSITION            (1 << 0)
#define UNITY_CHANGED_REGION              (1 << 1)
#define UNITY_CHANGED_ADDED               (1 << 2)
#define UNITY_CHANGED_REMOVED             (1 << 3)
#define UNITY_CHANGED_TITLE               (1 << 4)
#define UNITY_CHANGED_ZORDER              (1 << 5)
#define UNITY_CHANGED_WINDOW_STATE        (1 << 6)
#define UNITY_CHANGED_WINDOW_ATTRIBUTES   (1 << 7)
#define UNITY_CHANGED_WINDOW_TYPE         (1 << 8)
#define UNITY_CHANGED_WINDOW_ICONS        (1 << 9)
#define UNITY_CHANGED_WINDOW_DESKTOP      (1 << 10)
#define UNITY_CHANGED_ACTIVE_DESKTOP      (1 << 11)

/*
 * UNITY_UPDATE_* flags are passed to UnityWindowTracker_RequestUpdates
 */
#define UNITY_UPDATE_INCREMENTAL       (1 << 0)
#define UNITY_UPDATE_REMOVE_UNTOUCHED  (1 << 1)

typedef enum {
   UNITY_WINDOW_ORDER_TOP = 0,
   UNITY_WINDOW_ORDER_BOTTOM = UNITY_MAX_WINDOWS
} UnityZOrder;

/*
 * Unity callbacks passed to UnityWindowTracker_Init used to notify the
 * caller of changes to the window system.
 */
typedef enum {
   UNITY_UPDATE_ADD_WINDOW,
   UNITY_UPDATE_MOVE_WINDOW,
   UNITY_UPDATE_REMOVE_WINDOW,
   UNITY_UPDATE_CHANGE_WINDOW_REGION,
   UNITY_UPDATE_CHANGE_WINDOW_TITLE,
   UNITY_UPDATE_CHANGE_ZORDER,
   UNITY_UPDATE_CHANGE_WINDOW_STATE,
   UNITY_UPDATE_CHANGE_WINDOW_ATTRIBUTE,
   UNITY_UPDATE_CHANGE_WINDOW_TYPE,
   UNITY_UPDATE_CHANGE_WINDOW_ICON,
   UNITY_UPDATE_CHANGE_WINDOW_DESKTOP,
   UNITY_UPDATE_CHANGE_ACTIVE_DESKTOP
} UnityUpdateType;

/*
 * Single UnityUpdate struct that gets passed into the UnityUpdateCallback
 * function.  Unity updates have a very long way to travel (tools ->
 * vmx -> mks -> vnc -> (wire) -> vnc -> ui) and having a single callback
 * function greatly reduces the amount of plumbing that has to be written
 * for every singe command.
 */

typedef struct UnityUpdate {
   UnityUpdateType type;
   union {
      struct {
         UnityWindowId     id;
      } addWindow;

      struct {
         UnityWindowId     id;
      } removeWindow;

      struct {
         UnityWindowId     id;
         BoxRec            rect;
      } moveWindow;

      struct {
         UnityWindowId     id;
         RegionPtr         region;
      } changeWindowRegion;

      struct {
         UnityWindowId     id;
         DynBuf            titleUtf8;
      } changeWindowTitle;

      struct {
         uint32            count;
         UnityWindowId     ids[UNITY_MAX_WINDOWS];
      } zorder;

      struct {
         UnityWindowId     id;
         uint32            state;
      } changeWindowState;

      struct {
         UnityWindowId     id;
         UnityWindowAttribute attr;
         uint32 value;
      } changeWindowAttribute;

      struct {
         UnityWindowId id;
         UnityWindowType winType;
      } changeWindowType;

      struct {
         UnityWindowId id;
         UnityIconType iconType;
      } changeWindowIcon;

      struct {
         UnityWindowId id;
         UnityDesktopId desktopId;
      } changeWindowDesktop;

      struct {
         UnityDesktopId desktopId;
      } changeActiveDesktop;

   } u;
} UnityUpdate;

typedef void (*UnityUpdateCallback)(void *param, UnityUpdate *update);

/*
 * Internal state --
 * Do not fiddle with these bits!  They are included in this header to aid in debugging.
 * Enjoy looking at them, but consider them READ ONLY (!!)
 *
 */

#define UNITY_INFO_ATTR_EXISTS  (1 << 7)
#define UNITY_INFO_ATTR_CHANGED (1 << 6)
#define UNITY_INFO_ATTR_ENABLED (1 << 0)

typedef struct {
   UnityWindowId     id;
   DynBuf            titleUtf8;
   RegionPtr         region;
   BoxRec            rect;
   uint32            state;
   UnityWindowType   type;
   UnityDesktopId    desktopId;

   /* Each element is an OR of the UNITY_INFO_ATTR_* values */
   unsigned char     attributes[UNITY_MAX_ATTRIBUTES];
   /* Ditto, but only EXISTS and CHANGED apply... */
   unsigned char     icons[UNITY_MAX_ICONS];

   Bool              reap;
   /*
    * Used to track if a window was reported during the last update cycle (whether or not
    * its properties actually changed), so that we can automatically have windows removed
    * in Unity implementations that poll for changes (such as Win32).
    */
   Bool              touched;
   /*
    * Used to track which properties of a window changed during the last update cycle.
    */
   int               changed;

   void             *data;

   /*
    * This points back to the tracker. It's either this, or a bigger hack in
    * lib/misc/hash.c
    */
   void		    *tracker;
} UnityWindowInfo;

typedef struct _UnityWindowTracker UnityWindowTracker;

typedef void (*UnityDataFreeFunc)(UnityWindowTracker *tracker,
                                  UnityWindowInfo *windowInfo,
                                  void *data);

struct _UnityWindowTracker {
   HashTable      *windows;
   UnityWindowId  zorder[UNITY_MAX_WINDOWS];
   uint32         count; // in zorder array
   Bool           zorderChanged;

   UnityDesktopId activeDesktopId;
   Bool           activeDesktopChanged;

   void           *cbparam;
   UnityUpdateCallback cb;
   uint32         updateFlags;

   UnityDataFreeFunc freeFn;
};

/*
 * Public Functions --
 */

void UnityWindowTracker_Init(UnityWindowTracker *tracker,
                             UnityUpdateCallback cb);
void UnityWindowTracker_Cleanup(UnityWindowTracker *tracker);
void UnityWindowTracker_SetDataFreeFunc(UnityWindowTracker *tracker,
                                        UnityDataFreeFunc freeFn);
UnityWindowInfo *UnityWindowTracker_AddWindow(UnityWindowTracker *tracker,
                                              UnityWindowId id);
UnityWindowInfo *UnityWindowTracker_AddWindowWithData(UnityWindowTracker *tracker,
                                                      UnityWindowId id,
                                                      void *data);
void UnityWindowTracker_MoveWindow(UnityWindowTracker *tracker,
                                   UnityWindowId id, int x1, int y1, int x2,
                                   int y2);
void UnityWindowTracker_ChangeWindowRegion(UnityWindowTracker *tracker,
                                           UnityWindowId id, RegionPtr region);
void UnityWindowTracker_RemoveWindow(UnityWindowTracker *tracker,
                                     UnityWindowId id);
void UnityWindowTracker_SendUpdate(UnityWindowTracker *tracker,
                                   UnityUpdate *update);
void UnityWindowTracker_SetWindowTitle(UnityWindowTracker *tracker,
                                       UnityWindowId id,
                                       DynBuf *titleUtf8);

/*
 * Please note that calling ChangeWindowState directly is deprecated. Use
 * SetWindowAttribute and SetWindowType instead.
 */
void UnityWindowTracker_ChangeWindowState(UnityWindowTracker *tracker,
                                          UnityWindowId id,
                                          uint32 state);
/*
 * Deprecated. Use GetWindowAttribute and GetWindowType instead.
 */
Bool UnityWindowTracker_GetWindowState(UnityWindowTracker *tracker,
                                       UnityWindowId id,
                                       uint32 *state);

void UnityWindowTracker_ChangeWindowAttribute(UnityWindowTracker *tracker,
                                              UnityWindowId id,
                                              UnityWindowAttribute attr,
                                              Bool enabled);
Bool UnityWindowTracker_GetWindowAttribute(UnityWindowTracker *tracker,
                                           UnityWindowId id,
                                           UnityWindowAttribute attr,
                                           Bool *enabled);
void UnityWindowTracker_ChangeWindowType(UnityWindowTracker *tracker,
                                         UnityWindowId id,
                                         UnityWindowType winType);
Bool UnityWindowTracker_GetWindowType(UnityWindowTracker *tracker,
                                      UnityWindowId id,
                                      UnityWindowType *winType);
void UnityWindowTracker_NotifyIconChanged(UnityWindowTracker *tracker,
                                          UnityWindowId id,
                                          UnityIconType iconType);
void UnityWindowTracker_ChangeWindowDesktop(UnityWindowTracker *tracker,
                                            UnityWindowId id,
                                            UnityDesktopId desktopId);
Bool UnityWindowTracker_GetWindowDesktop(UnityWindowTracker *tracker,
                                         UnityWindowId id,
                                         UnityDesktopId *desktopId);
void UnityWindowTracker_ChangeActiveDesktop(UnityWindowTracker *tracker,
                                            UnityDesktopId desktopId);
UnityDesktopId UnityWindowTracker_GetActiveDesktop(UnityWindowTracker *tracker);
void UnityWindowTracker_SetZOrder(UnityWindowTracker *tracker, UnityWindowId zorder[],
                                  int count);
void UnityWindowTracker_SetZPosition(UnityWindowTracker *tracker, UnityWindowId window,
                                  uint32 zorder);
void UnityWindowTracker_RequestUpdates(UnityWindowTracker *tracker,
                                       uint32 flags, void *param);
UnityWindowInfo *UnityWindowTracker_LookupWindow(UnityWindowTracker *tracker,
                                                 UnityWindowId id);


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowData --
 *
 *      Returns the app data pointer associated with a particular Unity window.
 *
 * Results:
 *      Pointer to the app data. May be NULL if no data is set or 'info' is invalid.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
UnityWindowTracker_GetWindowData(UnityWindowTracker *tracker, // IN
                                 UnityWindowInfo *info)       // IN
{
   return info ? info->data : NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowDataById --
 *
 *      Returns the app data pointer associated with a particular Unity window ID.
 *
 * Results:
 *      Pointer to the app data. May be NULL if no data is set or 'info' is invalid.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
UnityWindowTracker_GetWindowDataById(UnityWindowTracker *tracker, // IN
                                     UnityWindowId winId)         // IN
{
   return UnityWindowTracker_GetWindowData(tracker,
                                           UnityWindowTracker_LookupWindow(tracker,
                                                                           winId));
}
#endif
