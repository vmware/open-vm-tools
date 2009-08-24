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
 */

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "log.h"
#include "unityWindowTracker.h"

#define LOGLEVEL_MODULE uwt
#include "loglevel_user.h"

/*
 * Helper Functions --
 */

static void FreeWindowInfo(UnityWindowInfo *info);
static int RemoveUntouchedWindow(const char *key, void *value,
                                 void *clientData);
static int GarbageCollectRemovedWindows(const char *key, void *value,
                                        void *clientData);
static int ResetChangedBits(const char *key, void *value, void *clientData);
static int PushUpdates(const char *key, void *value, void *clientData);
static Bool TitlesEqual(DynBuf *first, DynBuf *second);
static int PushZOrder(UnityWindowTracker *tracker);
static int PushActiveDesktop(UnityWindowTracker *tracker);


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_Init --
 *
 *      Create a new unity window tracker.  The client should pass in a
 *      callbacks object, which will be used to notify them of updates
 *      in UnityWindowTracker_RequestUpdates.
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
UnityWindowTracker_Init(UnityWindowTracker *tracker,     // IN
                        UnityUpdateCallback cb)          // IN
{
   memset(tracker, 0, sizeof(UnityWindowTracker));
   tracker->cb = cb;
   tracker->windows = HashTable_Alloc(128, HASH_INT_KEY,
                                      (HashTableFreeEntryFn)FreeWindowInfo);
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_Cleanup --
 *
 *      Destory a unity window tracker.
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
UnityWindowTracker_Cleanup(UnityWindowTracker *tracker)  // IN
{
   HashTable_Free(tracker->windows);
   memset(tracker, 0, sizeof(UnityWindowTracker));
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_SetDataFreeFunc --
 *
 *      Sets the function that will be called to free the app data associated with a
 *      window.
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
UnityWindowTracker_SetDataFreeFunc(UnityWindowTracker *tracker,  // IN
                                   UnityDataFreeFunc freeFn)     // IN
{
   tracker->freeFn = freeFn;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_LookupWindow --
 *
 *      Returns the window with the specified window id, or NULL if no
 *      such window exists.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

UnityWindowInfo *
UnityWindowTracker_LookupWindow(UnityWindowTracker *tracker,      // IN
                                UnityWindowId id)                 // IN
{
   UnityWindowInfo *info = NULL;
   HashTable_Lookup(tracker->windows, (const char *)(long)id, (void **)&info);
   return info;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_AddWindow --
 *
 *      Add a new window to the window tracker
 *
 * Results:
 *      A pointer to the UnityWindowInfo for the added window
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

UnityWindowInfo *
UnityWindowTracker_AddWindow(UnityWindowTracker *tracker,   // IN
                             UnityWindowId id,              // IN
                             DynBuf *windowPathUtf8,        // IN
                             DynBuf *execPathUtf8)          // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (!info) {
      size_t windowPathSize;
      size_t execPathSize;

      info = (UnityWindowInfo *)Util_SafeCalloc(1, sizeof(UnityWindowInfo));
      info->tracker = tracker;
      info->id = id;
      info->type = UNITY_WINDOW_TYPE_NONE;
      info->desktopId = tracker->activeDesktopId;
      DynBuf_Init(&info->titleUtf8);
      DynBuf_Init(&info->windowPathUtf8);
      DynBuf_Init(&info->execPathUtf8);

      /*
       * Ensure that the provided paths only include one NUL terminator
       * at the end of the buffer, or keep the paths empty otherwise.
       */
      windowPathSize = DynBuf_GetSize(windowPathUtf8);
      if (windowPathSize > 0) {
         Bool isNullTerminated = Str_Strlen((char *)DynBuf_Get(windowPathUtf8),
                                            windowPathSize) == windowPathSize - 1;
         ASSERT(isNullTerminated);
         if (isNullTerminated) {
            DynBuf_Copy(windowPathUtf8, &info->windowPathUtf8);
         }
      }
      execPathSize = DynBuf_GetSize(execPathUtf8);
      if (execPathSize > 0) {
         Bool isNullTerminated = Str_Strlen((char *)DynBuf_Get(execPathUtf8),
                                            execPathSize) == execPathSize - 1;
         ASSERT(isNullTerminated);
         if (isNullTerminated) {
            DynBuf_Copy(execPathUtf8, &info->execPathUtf8);
         }
      }

      LOG(2, ("Unity adding new window (id:%d)\n", id));
      HashTable_Insert(tracker->windows, (const char *)(long)id, info);
      info->changed |= UNITY_CHANGED_ADDED;
      info->changed |= UNITY_CHANGED_WINDOW_DESKTOP;
   } else {
      info->changed &= ~UNITY_CHANGED_REMOVED;
      LOG(2, ("Window already exists in UnityAddWindow (id:%d)\n", id));
   }
   info->touched = TRUE;

   return info;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_AddWindowWithData --
 *
 *      Add a new window to the window tracker, and sets its application data to the
 *      specified 'data'. The tracker's DataFreeFunc will be used to free 'data' when it
 *      needs to be destroyed (see UnityWindowTracker_SetDataFreeFunc).  If the window
 *      already exists and has 'data' set on it, that will be destroyed and replaced with
 *      the new 'data' pointer.
 *
 * Results:
 *      A pointer to the UnityWindowInfo for the added window.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

UnityWindowInfo *
UnityWindowTracker_AddWindowWithData(UnityWindowTracker *tracker,    // IN
                                     UnityWindowId id,               // IN
                                     DynBuf *windowPathUtf8,         // IN
                                     DynBuf *execPathUtf8,           // IN
                                     void *data)                     // IN
{
   UnityWindowInfo *info = UnityWindowTracker_AddWindow(tracker,
                                                        id,
                                                        windowPathUtf8,
                                                        execPathUtf8);

   if (info) {
      if (info->data
          && tracker->freeFn
          && (info->data != data)) {
         tracker->freeFn(tracker, info, info->data);
      }

      info->data = data;
   }

   return info;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_MoveWindow --
 *
 *      Notify the window tracker that the window with the specified id
 *      has moved.
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
UnityWindowTracker_MoveWindow(UnityWindowTracker *tracker,  // IN
                              UnityWindowId id,             // IN
                              int x1,                       // IN
                              int y1,                       // IN
                              int x2,                       // IN
                              int y2)                       // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (info->rect.x1 != x1 || info->rect.y1 != y1
          || x2 != info->rect.x2 || y2 != info->rect.y2) {
         LOG(2, ("Unity moving window (id:%d pos:%d,%d, %d,%d)\n", id, x1, y1, x2, y2));
         info->rect.x1 = x1;
         info->rect.y1 = y1;
         info->rect.x2 = x2;
         info->rect.y2 = y2;
         info->changed |= UNITY_CHANGED_POSITION;
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_SetWindowTitle --
 *
 *      Notify the window tracker that the window with the specified id
 *      has changed its title.
 *
 *      This function does not take ownership of the DynBuf; caller
 *      is assumed to free it.
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
UnityWindowTracker_SetWindowTitle(UnityWindowTracker *tracker,  // IN
                                  UnityWindowId id,             // IN
                                  DynBuf *titleUtf8)            // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (!TitlesEqual(&info->titleUtf8, titleUtf8)) {
         LOG(2, ("Unity setting window title (id:%d title:%s)\n", id,
             (const unsigned char*)DynBuf_Get(titleUtf8)));
         info->changed |= UNITY_CHANGED_TITLE;
         DynBuf_Destroy(&info->titleUtf8);
         DynBuf_Copy(titleUtf8, &info->titleUtf8);
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeWindowRegion --
 *
 *      Change the window region of the specified window.  A NULL region
 *      means the window region is simply the bounds of the window.
 *
 *      This function does not take ownership of the RegionPtr; caller is
 *      assumed to free it.
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
UnityWindowTracker_ChangeWindowRegion(UnityWindowTracker *tracker,   // IN
                                      UnityWindowId id,              // IN
                                      RegionPtr region)              // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (region) {
         if (!info->region) {
            LOG(2, ("Unity adding window region (id:%d)\n", id));
            info->changed |= UNITY_CHANGED_REGION;
            info->region = miRegionCreate(&miEmptyBox, 0);
         }
         if (!miRegionsEqual(info->region, region)) {
            LOG(2, ("Unity changing window region (id:%d)\n", id));
            info->changed |= UNITY_CHANGED_REGION;
            miRegionCopy(info->region, region);
         }
      } else {
         if (info->region) {
            LOG(2, ("Unity removing window region (id:%d)\n", id));
            info->changed |= UNITY_CHANGED_REGION;
            miRegionDestroy(info->region);
            info->region = NULL;
         }
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeWindowState --
 *
 *      Change window state (minimized or not) of the specified window.
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
UnityWindowTracker_ChangeWindowState(UnityWindowTracker *tracker,  // IN
                                     UnityWindowId id,             // IN
                                     uint32 state)                 // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (state != info->state) {
         info->changed |= UNITY_CHANGED_WINDOW_STATE;
         info->state = state;
         LOG(2, ("Unity changing window state (id:%d) to %d\n", id, state));
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowState --
 *
 *      Get window state (minimized or not) of the specified window.
 *
 * Results:
 *      TRUE if the window exists in the window tracker.
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
UnityWindowTracker_GetWindowState(UnityWindowTracker *tracker,  // IN
                                  UnityWindowId id,             // IN
                                  uint32 *state)                // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   ASSERT(state);
   if (info) {
      *state = info->state;
      return TRUE;
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeWindowAttribute --
 *
 *      Sets the value of a particular attribute on a particular window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Marks the window tracker as having updates.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityWindowTracker_ChangeWindowAttribute(UnityWindowTracker *tracker, // IN
                                         UnityWindowId id,            // IN
                                         UnityWindowAttribute attr,   // IN
                                         Bool enabled)                // IN
{
   UnityWindowInfo *info;
   ASSERT(tracker);
   ASSERT(attr < UNITY_MAX_ATTRIBUTES);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;

      /*
       * If this is a new attribute (that didn't exist before) or
       * if the attribute value has changed, remember the new value
       * (enabled or disabled) and mark it as existing and
       * changed.
       */

      if (!(info->attributes[attr] & UNITY_INFO_ATTR_EXISTS) ||
          ((info->attributes[attr] & UNITY_INFO_ATTR_ENABLED) !=
           (enabled ? UNITY_INFO_ATTR_ENABLED : 0))) {
         info->changed |= UNITY_CHANGED_WINDOW_ATTRIBUTES;
         info->attributes[attr] = (UNITY_INFO_ATTR_EXISTS |
                                   UNITY_INFO_ATTR_CHANGED |
                                   (enabled ? UNITY_INFO_ATTR_ENABLED : 0));
         LOG(2, ("Unity changing window (id:%d) attribute %d = %s\n",
                 id, attr, enabled ? "TRUE" : "FALSE"));
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowAttribute --
 *
 *      Retrieves the current value of a window attribute.
 *
 * Results:
 *      TRUE if the attribute value was retrieved successfully.
 *      FALSE if the attribute had never been set on the specified window, or if the
 *      window does not exist.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityWindowTracker_GetWindowAttribute(UnityWindowTracker *tracker,  // IN
                                      UnityWindowId id,             // IN
                                      UnityWindowAttribute attr,    // IN
                                      Bool *enabled)                // IN
{
   Bool retval = FALSE;
   UnityWindowInfo *info;
   ASSERT(tracker);
   ASSERT(attr < UNITY_MAX_ATTRIBUTES);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info
       && (info->attributes[attr] & UNITY_INFO_ATTR_EXISTS)) {
      *enabled = (info->attributes[attr] & UNITY_INFO_ATTR_ENABLED) ? TRUE : FALSE;
      retval = TRUE;
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeWindowType --
 *
 *      Sets the window type of the specified window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Marks the window tracker as having updates.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityWindowTracker_ChangeWindowType(UnityWindowTracker *tracker,  // IN
                                    UnityWindowId id,             // IN
                                    UnityWindowType winType)      // IN
{
   UnityWindowInfo *info;
   ASSERT(tracker);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (winType != info->type) {
         info->changed |= UNITY_CHANGED_WINDOW_TYPE;
         info->type = winType;
         LOG(2, ("Unity changing window (id:%d) type to %d\n",
                 id, winType));
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowType --
 *
 *      Retrieves the window type of the specified window.
 *
 * Results:
 *      TRUE if successful, FALSE if failed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityWindowTracker_GetWindowType(UnityWindowTracker *tracker, // IN
                                 UnityWindowId id,            // IN
                                 UnityWindowType *winType)    // IN
{
   Bool retval = FALSE;
   UnityWindowInfo *info;

   ASSERT(tracker);
   ASSERT(winType);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      *winType = info->type;
      retval = TRUE;
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_NotifyIconChanged --
 *
 *      Marks the window tracker as having a changed icon for a window.
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
UnityWindowTracker_NotifyIconChanged(UnityWindowTracker *tracker, // IN
                                     UnityWindowId id,            // IN
                                     UnityIconType iconType)      // IN
{
   UnityWindowInfo *info;

   ASSERT(tracker);
   ASSERT(iconType < UNITY_MAX_ICONS);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      LOG(2, ("Unity icon changed on window (id:%d)\n", id));
      info->touched = TRUE;
      info->changed |= UNITY_CHANGED_WINDOW_ICONS;
      info->icons[iconType] |= UNITY_INFO_ATTR_CHANGED | UNITY_INFO_ATTR_EXISTS;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeWindowDesktop --
 *
 *      Saves the window desktop information if that was changed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Window desktop information might be updated.
 *      Window marked as touched.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityWindowTracker_ChangeWindowDesktop(UnityWindowTracker *tracker,  // IN
                                       UnityWindowId id,             // IN
                                       UnityDesktopId desktopId)     // IN
{
   UnityWindowInfo *info;
   ASSERT(tracker);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      info->touched = TRUE;
      if (desktopId != info->desktopId) {
         info->changed |= UNITY_CHANGED_WINDOW_DESKTOP;
         info->desktopId = desktopId;
         LOG(2, ("Unity changing window (id:%u) desktop to %d\n",
                 id, desktopId));
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetWindowDesktop --
 *
 *      Get the window desktop information.
 *
 * Results:
 *      TRUE if this window exists in the window tracker, desktopId contains
 *      window desktop.
 *      FALSE otherwise, desktopId undefined.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnityWindowTracker_GetWindowDesktop(UnityWindowTracker *tracker,  // IN
                                    UnityWindowId id,             // IN
                                    UnityDesktopId *desktopId)    // OUT
{
   Bool retval = FALSE;
   UnityWindowInfo *info;

   ASSERT(tracker);
   ASSERT(desktopId);

   info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      *desktopId = info->desktopId;
      retval = TRUE;
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_ChangeActiveDesktop --
 *
 *      Saves the active desktop information if that was changed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Active desktop information might be updated.
 *
 *-----------------------------------------------------------------------------
 */

void
UnityWindowTracker_ChangeActiveDesktop(UnityWindowTracker *tracker,  // IN
                                       UnityDesktopId desktopId)     // IN
{
   if (desktopId != tracker->activeDesktopId) {
      tracker->activeDesktopId = desktopId;
      tracker->activeDesktopChanged = TRUE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityWindowTracker_GetActiveDesktop --
 *
 *      Return the active desktop id.
 *
 * Results:
 *      Active desktop id.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

UnityDesktopId
UnityWindowTracker_GetActiveDesktop(UnityWindowTracker *tracker)    // IN
{
   return tracker->activeDesktopId;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_RemoveWindow --
 *
 *      Remove the window with the specified id from the tracker.
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
UnityWindowTracker_RemoveWindow(UnityWindowTracker *tracker,      // IN
                                UnityWindowId id)                 // IN
{
   UnityWindowInfo *info = UnityWindowTracker_LookupWindow(tracker, id);
   if (info) {
      LOG(2, ("Unity removing window (id:%d)\n", id));
      info->changed |= UNITY_CHANGED_REMOVED;
      info->touched = TRUE;
      /*
       * Don't remove it yet.  We can only do so later...
       */
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_SendUpdate --
 *
 *      Update the window tracker via a UnityUpdate structure instead
 *      of a call to AddWindow, MoveWindow, etc.  Useful for forwarding
 *      notifications between unity windows trackers without writing
 *      a ton of boilerplate.
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
UnityWindowTracker_SendUpdate(UnityWindowTracker *tracker,        // IN
                              UnityUpdate *update)                // IN
{
   switch (update->type) {
   case UNITY_UPDATE_ADD_WINDOW:
      UnityWindowTracker_AddWindow(tracker,
                                   update->u.addWindow.id,
                                   &update->u.addWindow.windowPathUtf8,
                                   &update->u.addWindow.execPathUtf8);
      break;

   case UNITY_UPDATE_MOVE_WINDOW:
      UnityWindowTracker_MoveWindow(tracker,
                                    update->u.moveWindow.id,
                                    update->u.moveWindow.rect.x1,
                                    update->u.moveWindow.rect.y1,
                                    update->u.moveWindow.rect.x2,
                                    update->u.moveWindow.rect.y2);
      break;

   case UNITY_UPDATE_REMOVE_WINDOW:
      UnityWindowTracker_RemoveWindow(tracker,
                                      update->u.removeWindow.id);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_REGION:
      UnityWindowTracker_ChangeWindowRegion(tracker,
                                            update->u.changeWindowRegion.id,
                                            update->u.changeWindowRegion.region);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TITLE:
      UnityWindowTracker_SetWindowTitle(tracker,
                                        update->u.changeWindowTitle.id,
                                        &update->u.changeWindowTitle.titleUtf8);
      break;

   case UNITY_UPDATE_CHANGE_ZORDER:
      UnityWindowTracker_SetZOrder(tracker, update->u.zorder.ids,
                                   update->u.zorder.count);
      /*
       * This function is only every called from the host. Thus, if we get
       * a zorder changed event from the guest it's safe to blindly trust it
       * mark the zorder as changed. See bug 409742 for more info.
       */
      tracker->zorderChanged = TRUE;
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_STATE:
      UnityWindowTracker_ChangeWindowState(tracker,
                                           update->u.changeWindowState.id,
                                           update->u.changeWindowState.state);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ATTRIBUTE:
      UnityWindowTracker_ChangeWindowAttribute(tracker,
                                               update->u.changeWindowAttribute.id,
                                               update->u.changeWindowAttribute.attr,
                                               update->u.changeWindowAttribute.value);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_TYPE:
      UnityWindowTracker_ChangeWindowType(tracker,
                                          update->u.changeWindowType.id,
                                          update->u.changeWindowType.winType);
      break;

   case UNITY_UPDATE_CHANGE_WINDOW_ICON:
      UnityWindowTracker_NotifyIconChanged(tracker,
                                           update->u.changeWindowIcon.id,
                                           update->u.changeWindowIcon.iconType);
      break;
   case UNITY_UPDATE_CHANGE_WINDOW_DESKTOP:
      UnityWindowTracker_ChangeWindowDesktop(tracker,
                                             update->u.changeWindowDesktop.id,
                                             update->u.changeWindowDesktop.desktopId);
      break;

   case UNITY_UPDATE_CHANGE_ACTIVE_DESKTOP:
      UnityWindowTracker_ChangeActiveDesktop(tracker,
                                             update->u.changeActiveDesktop.desktopId);
      break;

   default:
      NOT_IMPLEMENTED();
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_SetZOrder --
 *
 *      Notify the window tracker of the Z-order of all windows.  Window ids
 *      at the front of the list are at the top of the z-order.
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
UnityWindowTracker_SetZOrder(UnityWindowTracker *tracker,   // IN
                             UnityWindowId zorder[],        // IN
                             int count)                     // IN
{
   count = MIN(count, ARRAYSIZE(tracker->zorder));

   if ((count != tracker->count) ||
         (memcmp(tracker->zorder, zorder, count * sizeof(tracker->zorder[0])) != 0)) {
      memcpy(tracker->zorder, zorder, count * sizeof(tracker->zorder[0]));
      tracker->count = count;
      tracker->zorderChanged = TRUE;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_SetZPosition --
 *
 *      Notify the window tracker of the Z-order of one window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updates the Z-ordering of the tracker.
 *
 *----------------------------------------------------------------------------
 */

void
UnityWindowTracker_SetZPosition(UnityWindowTracker *tracker, // IN
                                UnityWindowId id,            // IN
                                uint32 zorder)               // IN
{
   int newIndex, oldIndex;

   /* First, figure out where the window will be in the list */
   switch(zorder) {
   case UNITY_WINDOW_ORDER_BOTTOM:
      newIndex = tracker->count - 1;
      break;
   case UNITY_WINDOW_ORDER_TOP:
   default:
      newIndex = zorder;
      break;
   }

   /* Then, find where the window is the list. If it's not there, it's an error. */
   for (oldIndex = 0; oldIndex < tracker->count; oldIndex++) {
      if (id == tracker->zorder[oldIndex]) {
         break;
      }
   }
   ASSERT(oldIndex < tracker->count);

   /* Next, make space for the WindowId at its new spot */
   if (newIndex < oldIndex) {
      memmove(tracker->zorder + newIndex + 1, tracker->zorder + newIndex,
              oldIndex - newIndex);
   } else if (newIndex > oldIndex) {
      memmove(tracker->zorder + oldIndex, tracker->zorder + oldIndex + 1,
              newIndex - oldIndex);
   }

   /* Finally, put it in place */
   tracker->zorder[newIndex] = id;
   tracker->zorderChanged = TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * UnityWindowTracker_RequestUpdates --
 *
 *      Request a summary of all the updates pushed into the window tracker
 *      since the last call to UnityWindowTracker_RequestUpdates.
 *
 *      If UNITY_UPDATE_INCREMENTAL is set in flags, callbacks will only
 *      fire for elements which have changed since the last call to
 *      UnityWindowTracker_RequestUpdates.  Otherwise the entire state of
 *      the window tracker is sent via the callbacks.
 *
 *      If UNITY_UPDATE_REMOVE_UNTOUCHED is set in flags, windows for
 *      which there have been no updates since the last
 *      UnityWindowTracker_RequestUpdates call will be automatically removed.
 *      Useful if the client has no way of getting remove window notifications.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Lots of callbacks are fired.
 *
 *----------------------------------------------------------------------------
 */

void
UnityWindowTracker_RequestUpdates(UnityWindowTracker *tracker, // IN
                                  uint32 flags,                // IN
                                  void *param)                 // IN
{
   tracker->cbparam = param;
   tracker->updateFlags = flags;

   /*
    * If necessary, remove windows which didn't receive updates.
    */
   if (flags & UNITY_UPDATE_REMOVE_UNTOUCHED) {
      HashTable_ForEach(tracker->windows, RemoveUntouchedWindow, tracker);
   }

   /*
    * Push updates for the windows remaining...
    */
   HashTable_ForEach(tracker->windows, PushUpdates, tracker);

   /* Push Z order */
   PushZOrder(tracker);

   /* Push active desktop info */
   PushActiveDesktop(tracker);

   /*
    * ...then really delete things which were removed...
    */
   while (HashTable_ForEach(tracker->windows, GarbageCollectRemovedWindows,
          tracker)) {
      continue;
   }

   /*
    * ...and clear all the changed and touched bits of what's left to get ready
    * for the next iteration.
    */
   HashTable_ForEach(tracker->windows, ResetChangedBits, NULL);
}


/*
 *----------------------------------------------------------------------------
 *
 * FreeWindowInfo --
 *
 *      Destroy a window.
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
FreeWindowInfo(UnityWindowInfo *info)              // IN
{
   if (info) {
      UnityWindowTracker *tracker = info->tracker;

      if (tracker->freeFn && info->data) {
         tracker->freeFn(tracker, info, info->data);
      }

      if (info->region) {
         miRegionDestroy(info->region);
      }
      DynBuf_Destroy(&info->titleUtf8);
      DynBuf_Destroy(&info->windowPathUtf8);
      DynBuf_Destroy(&info->execPathUtf8);
      free(info);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * RemoveUntouchedWindow --
 *
 *      If the window specified hasn't been touched (i.e. an update function
 *      has been called on it), remove it from the window tracker.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
RemoveUntouchedWindow(const char *key,       // IN: window id
                      void *value,           // IN: UnityWindowInfo
                      void *clientData)      // IN: UnityWindowTracker
{
   UnityWindowTracker *tracker = (UnityWindowTracker *)clientData;
   UnityWindowInfo *info = (UnityWindowInfo *)value;
   if (!info->touched) {
      UnityWindowId id = (UnityWindowId)(long)key;
      LOG(2, ("Removing untouched window (id:%d)\n", id));
      UnityWindowTracker_RemoveWindow(tracker, id);
   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * GarbageCollectRemovedWindows --
 *
 *      Delete all window objects for windows which are marked as
 *      removed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
GarbageCollectRemovedWindows(const char *key,       // IN: window id
                             void *value,           // IN: UnityWindowInfo
                             void *clientData)      // IN: UnityWindowTracker
{
   UnityWindowTracker *tracker = (UnityWindowTracker *)clientData;
   UnityWindowInfo *info = (UnityWindowInfo *)value;
   if (info->reap) {
      LOG(2, ("Destroying window (id:%d)\n", (UnityWindowId)(long)key));
      HashTable_Delete(tracker->windows, key);
      return 1;
   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * ResetChangedBits --
 *
 *      Reset the changed and touched bits for this window.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
ResetChangedBits(const char *key,       // IN: window id
                 void *value,           // IN: UnityWindowInfo
                 void *clientData)      // IN: UnityWindowTracker
{
   UnityWindowInfo *info = (UnityWindowInfo *)value;
   int i;

   if (info->changed & UNITY_CHANGED_WINDOW_ATTRIBUTES) {
      for (i = 0; i < UNITY_MAX_ATTRIBUTES; i++) {
         info->attributes[i] &= ~UNITY_INFO_ATTR_CHANGED;
      }
   }

   if (info->changed & UNITY_CHANGED_WINDOW_ICONS) {
      for (i = 0; i < UNITY_MAX_ICONS; i++) {
         info->icons[i] &= ~UNITY_INFO_ATTR_CHANGED;
      }
   }

   info->changed = 0;
   info->touched = FALSE;
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * PushUpdates --
 *
 *      Fire all callback functions relevant for this window (as determined
 *      by the changed bits).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
PushUpdates(const char *key,       // IN: window id
            void *value,           // IN: UnityWindowInfo
            void *clientData)      // IN: UnityWindowTracker
{
   UnityWindowTracker *tracker = (UnityWindowTracker *)clientData;
   UnityWindowInfo *info = (UnityWindowInfo *)value;
   UnityWindowId id = (UnityWindowId)(long)key;
   UnityUpdate update;
   Bool incremental = (tracker->updateFlags & UNITY_UPDATE_INCREMENTAL) != 0;

   if (info->changed & UNITY_CHANGED_REMOVED) {
      /*
       * Now that we've sent the update, mark the window as deleted
       * so it will be reaped.
       */
      info->reap = TRUE;
      update.type = UNITY_UPDATE_REMOVE_WINDOW;
      update.u.removeWindow.id = id;
      (*tracker->cb)(tracker->cbparam, &update);
   } else {
      if (!incremental || (info->changed & UNITY_CHANGED_ADDED)) {
         update.type = UNITY_UPDATE_ADD_WINDOW;
         update.u.addWindow.id = id;
         DynBuf_Init(&update.u.addWindow.windowPathUtf8);
         DynBuf_Init(&update.u.addWindow.execPathUtf8);
         if (DynBuf_GetSize(&info->windowPathUtf8)) {
            DynBuf_Copy(&info->windowPathUtf8, &update.u.addWindow.windowPathUtf8);
         }
         if (DynBuf_GetSize(&info->execPathUtf8)) {
            DynBuf_Copy(&info->execPathUtf8, &update.u.addWindow.execPathUtf8);
         }
         (*tracker->cb)(tracker->cbparam, &update);
         DynBuf_Destroy(&update.u.addWindow.windowPathUtf8);
         DynBuf_Destroy(&update.u.addWindow.execPathUtf8);
      }
      if (!incremental || (info->changed & UNITY_CHANGED_POSITION)) {
         update.type = UNITY_UPDATE_MOVE_WINDOW;
         update.u.moveWindow.id = id;
         update.u.moveWindow.rect = info->rect;
         (*tracker->cb)(tracker->cbparam, &update);
      }
      if (!incremental || (info->changed & UNITY_CHANGED_REGION)) {
         update.type = UNITY_UPDATE_CHANGE_WINDOW_REGION;
         update.u.changeWindowRegion.id = id;
         update.u.changeWindowRegion.region = info->region;
         (*tracker->cb)(tracker->cbparam, &update);
      }
      if (!incremental || (info->changed & UNITY_CHANGED_TITLE)) {
         update.type = UNITY_UPDATE_CHANGE_WINDOW_TITLE;
         update.u.changeWindowTitle.id = id;
         DynBuf_Init(&update.u.changeWindowTitle.titleUtf8);
         DynBuf_Copy(&info->titleUtf8, &update.u.changeWindowTitle.titleUtf8);
         (*tracker->cb)(tracker->cbparam, &update);
         DynBuf_Destroy(&update.u.changeWindowTitle.titleUtf8);
      }
      if (!incremental || (info->changed & UNITY_CHANGED_WINDOW_ICONS)) {
         UnityIconType i;

         update.type = UNITY_UPDATE_CHANGE_WINDOW_ICON;
         update.u.changeWindowIcon.id = id;
         for (i = 0; i < UNITY_MAX_ICONS; i++) {
            if ((info->icons[i] & UNITY_INFO_ATTR_EXISTS)
                && (!incremental
                    || (info->icons[i] & UNITY_INFO_ATTR_CHANGED))) {
               update.u.changeWindowIcon.iconType = i;
               (*tracker->cb)(tracker->cbparam, &update);
            }
         }
      }
      if (!incremental || (info->changed & UNITY_CHANGED_WINDOW_TYPE)) {
         update.type = UNITY_UPDATE_CHANGE_WINDOW_TYPE;
         update.u.changeWindowType.id = id;
         update.u.changeWindowType.winType = info->type;
         (*tracker->cb)(tracker->cbparam, &update);
      }

      /*
       * Please make sure WINDOW_ATTRIBUTES is checked before WINDOW_STATE, to allow
       * vmware-vmx on the host side to only pay attention to WINDOW_ATTRIBUTES if
       * desired.
       */
      if (!incremental || (info->changed & UNITY_CHANGED_WINDOW_ATTRIBUTES)) {
         UnityWindowAttribute i;

         update.type = UNITY_UPDATE_CHANGE_WINDOW_ATTRIBUTE;
         update.u.changeWindowAttribute.id = id;
         for (i = 0; i < UNITY_MAX_ATTRIBUTES; i++) {
            if ((info->attributes[i] & UNITY_INFO_ATTR_EXISTS)
                && (!incremental ||
                    (info->attributes[i] & UNITY_INFO_ATTR_CHANGED))) {
               update.u.changeWindowAttribute.attr = i;
               update.u.changeWindowAttribute.value =
                  (info->attributes[i] & UNITY_INFO_ATTR_ENABLED) ? TRUE : FALSE;
               (*tracker->cb)(tracker->cbparam, &update);
            }
         }
      }

      if (!incremental || (info->changed & UNITY_CHANGED_WINDOW_STATE)) {
         update.type = UNITY_UPDATE_CHANGE_WINDOW_STATE;
         update.u.changeWindowState.id = id;
         update.u.changeWindowState.state = info->state;
         (*tracker->cb)(tracker->cbparam, &update);
      }

      if (!incremental || (info->changed & UNITY_CHANGED_WINDOW_DESKTOP)) {
         update.type = UNITY_UPDATE_CHANGE_WINDOW_DESKTOP;
         update.u.changeWindowDesktop.id = id;
         update.u.changeWindowDesktop.desktopId = info->desktopId;
         (*tracker->cb)(tracker->cbparam, &update);
      }

   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * PushZOrder --
 *
 *      Fire callback function if Z Order was changed
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
PushZOrder(UnityWindowTracker *tracker)     // IN: UnityWindowTracker
{
   UnityUpdate update;
   Bool incremental = (tracker->updateFlags & UNITY_UPDATE_INCREMENTAL) != 0;
   if (!incremental || tracker->zorderChanged) {
      update.type = UNITY_UPDATE_CHANGE_ZORDER;
      update.u.zorder.count = tracker->count;
      memcpy(&update.u.zorder.ids, tracker->zorder,
             tracker->count * sizeof(update.u.zorder.ids[0]));
      (*tracker->cb)(tracker->cbparam, &update);

      tracker->zorderChanged = FALSE;
   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * PushActiveDesktop --
 *
 *      Fire callback function if the active desktop was changed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
PushActiveDesktop(UnityWindowTracker *tracker)     // IN: UnityWindowTracker
{
   UnityUpdate update;
   Bool incremental = (tracker->updateFlags & UNITY_UPDATE_INCREMENTAL) != 0;
   if (!incremental || tracker->activeDesktopChanged) {
      update.type = UNITY_UPDATE_CHANGE_ACTIVE_DESKTOP;
      update.u.changeActiveDesktop.desktopId = tracker->activeDesktopId;
      (*tracker->cb)(tracker->cbparam, &update);

      tracker->activeDesktopChanged = FALSE;
   }
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * TitlesEqual--
 *
 *      Performs string comparison on the titles held in DynBufs
 *
 * Results:
 *       TRUE  if first == second
 *       FALSE if first != second
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
TitlesEqual(DynBuf *first,       // IN: First window title
            DynBuf *second)      // IN: Second window title
{
   if (DynBuf_GetSize(first) != DynBuf_GetSize(second)) {
      return FALSE;
   }
   return (strncmp((const unsigned char*)DynBuf_Get(first),
                   (const unsigned char*)DynBuf_Get(second),
                   DynBuf_GetSize(first)) == 0) ? TRUE : FALSE;
}
