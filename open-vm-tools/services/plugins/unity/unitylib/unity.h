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
 * unity.h --
 *
 *    Commands for unity window manager intergration.
 */

#ifndef _UNITY_H_
#define _UNITY_H_

#include <glib.h>
#include "dndGuest.h"
#include "dynbuf.h"
#ifdef _WIN32
   #include "libExport.hh"
#endif
#include "unityWindowTracker.h"

/*
 * In Unity mode, all our DnD detection windows will be ignored and not displayed
 * on host desktop. Right now we have 4 DnD detection window. 2 for DnD version 2
 * or older, 2 for DnD version 3 or newer.
 */
enum {
   UNITY_BLOCKED_WND_DND_FULL_DET_V2  = 0,
   UNITY_BLOCKED_WND_DND_DET_V2       = 1,
   UNITY_BLOCKED_WND_DND_FULL_DET_V3  = 2,
   UNITY_BLOCKED_WND_DND_DET_V3       = 3,
   UNITY_BLOCKED_WND_MAX              = 4,
};

/*
 * Maximum number of virtual desktops supported.
 */
#define MAX_VIRT_DESK 64

/*
 * Represents a virtual desktop coordinates in the virtual desktop grid.
 * The grid might look like {1,1} {1,2} {2,1} {2,2} or {1,1} {1,2} {1,2} etc.
 */
typedef struct UnityVirtualDesktop {
   int32 x;
   int32 y;
} UnityVirtualDesktop;

typedef struct UnityPoint {
   int32 x;
   int32 y;
} UnityPoint;

/*
 * Rectangle on the Unity desktop (typically relative to the Unity Desktop origin.
  * Width & Height must be positive.
  */
typedef struct {
   int x;
   int y;
   int width;
   int height;
} UnityRect;

/*
 * Represents a virtual desktop configuration.
 */
typedef struct UnityVirtualDesktopArray {
   size_t desktopCount;                              // number of desktops in the grid
   UnityVirtualDesktop desktops[MAX_VIRT_DESK];      // array of desktops
} UnityVirtualDesktopArray;

/* Forward reference. */
typedef struct DesktopSwitchCallbackManager DesktopSwitchCallbackManager;

/*
 * Callback functions for outbound updates from the guest to the host.
 * The Unity library requires these functions to be provided so that the host
 * is correctly updated as to changes of window state (essentially relaying the
 * Unity protocol to the host).
 *
 * Prepares, builds and sends a sequence of Unity Window Tracker updates back
 * to the host. flags is passed back to the UnityWindowTracker_RequestUpdates()
 * function to set what type of updates are
 * required - see bora/lib/public/unityWindowTracker.h
 */
typedef Bool (*UnityHostChannelBuildUpdateCallback)(void *param, int flags);

/*
 * Sends the provided window contents (a PNG Image) for the specified WindowID
 * to the host. A FALSE return indicates the contents were not sent correctly.
 */
typedef Bool (*UnitySendWindowContentsFn)(UnityWindowId windowID,
                                          uint32 imageWidth,
                                          uint32 imageHeight,
                                          const char *imageData,
                                          uint32 imageLength);

/*
 * Notifies the host that the specified window would like to be minimized, the
 * sequence number is returned in a subsequent confirmation.  A FALSE return indicates
 * the contents were not sent correctly.
 */
typedef Bool (*UnitySendRequestMinimizeOperationFn)(UnityWindowId windowId,
                                                    uint32 sequence);
/*
 * Sends a (synchronous) inquiry to the host as to whether the guest taskbar
 * should be visible. A FALSE return indicates that the bar should not be shown,
 * no errors are returned from this function - the default behaviour is to not show
 * the task bar in the guest.
 */
typedef Bool (*UnityShouldShowTaskbarFn)(void);

typedef struct UnityHostCallbacks {
   UnityHostChannelBuildUpdateCallback buildUpdateCB;
   UnityUpdateCallback updateCB; // From UnityWindowTracker.h
   UnitySendWindowContentsFn sendWindowContents;
   UnitySendRequestMinimizeOperationFn sendRequestMinimizeOperation;
   UnityShouldShowTaskbarFn shouldShowTaskbar;

   // Context/Cookie passed to buildUpdateCB and updateCB
   void *updateCbCtx;
} UnityHostCallbacks;

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

void Unity_Init(UnityHostCallbacks hostCallbacks,
                gpointer serviceObj);
Bool Unity_IsActive(void);
Bool Unity_IsSupported(void);
Bool Unity_Enter(void);
void Unity_Exit(void);
void Unity_Cleanup(void);
void Unity_UnityToLocalPoint(UnityPoint *localPt, UnityPoint *unityPt);
void Unity_LocalToUnityPoint(UnityPoint *unityPt, UnityPoint *localPt);
void Unity_GetWindowCommandList(char ***commandList);
void Unity_SetActiveDnDDetWnd(UnityDnD *state);

#ifdef _WIN32
LIB_EXPORT HWND Unity_GetHwndFromUnityId(UnityWindowId id);
#endif

void Unity_SetUnityOptions(uint32 newFeaturesMask);

/*
 * Retrieve window metadata, contents, icons, path to owning window.
 */
Bool Unity_RequestWindowContents(UnityWindowId windowIds[], uint32 numWindowIds);
Bool Unity_GetWindowContents(UnityWindowId window,
                             DynBuf *imageData,
                             uint32 *width,
                             uint32 *height);
Bool Unity_GetIconData(UnityWindowId window,
                       UnityIconType iconType,
                       UnityIconSize iconSize,
                       uint32 dataOffset,
                       uint32 dataLength,
                       DynBuf *imageData,
                       uint32 *fullLength);
Bool Unity_GetWindowPath(UnityWindowId window,
                         DynBuf *windowPathUtf8,
                         DynBuf *execPathUtf8);

/*
 * Desktop Appearance.
 */
void Unity_ShowTaskbar(Bool showTaskbar);
void Unity_SetConfigDesktopColor(int desktopColor);
void Unity_ShowDesktop(Bool showDesktop);

/*
 * Post a request to asynchronously retrieve Unity updates, or synchronously
 * receive them via the updateChannel.
 */
void Unity_GetUpdate(Bool incremental);
void Unity_GetUpdates(int flags);

/*
 * Virtual Desktop configuration and window location.
 */
Bool Unity_SetDesktopConfig(const UnityVirtualDesktopArray *desktopConfig);
Bool Unity_SetInitialDesktop(UnityDesktopId desktopId);
Bool Unity_SetDesktopActive(UnityDesktopId desktopId);
Bool Unity_SetWindowDesktop(UnityWindowId windowId, UnityDesktopId desktopId);
Bool Unity_SetDesktopWorkAreas(UnityRect workAreas[], uint32 numWorkAreas);
Bool Unity_MoveResizeWindow(UnityWindowId window,
                            UnityRect *moveResizeRect);

/*
 * Window state, grouping and order.
 */
Bool Unity_WindowCommand(UnityWindowId window, const char *command);
Bool Unity_SetTopWindowGroup(UnityWindowId windows[], unsigned int windowCount);

/*
 * Interlocked operations.
 */
Bool Unity_ConfirmOperation(unsigned int operation,
                            UnityWindowId windowId,
                            uint32 sequence,
                            Bool allow);

/*
 * Mouse Wheel event forwarding.
 */
Bool Unity_SendMouseWheel(int32 deltaX, int32 deltaY, int32 deltaZ, uint32 modifierFlags);

/*
 * Debugging aids.
 */
void Unity_SetForceEnable(Bool forceEnable);
void Unity_InitializeDebugger(void);

#ifdef __cplusplus
};
#endif // __cplusplus
#endif
