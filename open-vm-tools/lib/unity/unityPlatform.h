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
 * unityPlatform.h --
 *
 *    Implementation specific functionality
 */

#ifndef _UNITY_PLATFORM_H_
#define _UNITY_PLATFORM_H_

#include "unityWindowTracker.h"
#include "unity.h"

/*
 * This data structure is used when gathering and sending
 * unity updates.
 */

typedef struct {
   DynBuf updates;
   size_t cmdSize;
   RpcOut *rpcOut;
   uint32 flags;
} UnityUpdateThreadData;

typedef struct {
   int x;
   int y;
   int width;
   int height;
} UnityRect;

typedef struct _UnityPlatform UnityPlatform;

/*
 * Implemented by unityPlatform[Win32|X11|Cocoa (ha!)].c
 */

Bool UnityPlatformIsSupported(void);
UnityPlatform *UnityPlatformInit(UnityWindowTracker *tracker,
                                 int* blockedWnd);
void UnityPlatformCleanup(UnityPlatform *up);
void UnityPlatformRegisterCaps(UnityPlatform *up);
void UnityPlatformUnregisterCaps(UnityPlatform *up);
Bool UnityPlatformUpdateWindowState(UnityPlatform *up,
                                    UnityWindowTracker *tracker);
void UnityPlatformSaveSystemSettings(UnityPlatform *up);
void UnityPlatformRestoreSystemSettings(UnityPlatform *up);
Bool UnityPlatformGetWindowPath(UnityPlatform *up,
                                UnityWindowId window,
                                DynBuf *buf);
Bool UnityPlatformGetNativeWindowPath(UnityPlatform *up,
                                      UnityWindowId window,
                                      DynBuf *buf);
Bool UnityPlatformGetBinaryInfo(UnityPlatform *up,
                                const char *pathUtf8,
                                DynBuf *buf);
Bool UnityPlatformRestoreWindow(UnityPlatform *up,
                                UnityWindowId window);
Bool UnityPlatformSetTopWindowGroup(UnityPlatform *up,
                                    UnityWindowId *windows,
                                    unsigned int windowCount);
Bool UnityPlatformCloseWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformShowWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformHideWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformMinimizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformMaximizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformUnmaximizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformGetWindowContents(UnityPlatform *up,
                                    UnityWindowId window,
                                    DynBuf *imageData);
Bool UnityPlatformMoveResizeWindow(UnityPlatform *up,
                                   UnityWindowId window,
                                   UnityRect *moveResizeRect);
void UnityPlatformShowTaskbar(UnityPlatform *up, Bool showTaskbar);
Bool UnityPlatformGetIconData(UnityPlatform *up,
                              UnityWindowId window,
                              UnityIconType iconType,
                              UnityIconSize iconSize,
                              uint32 dataOffset,
                              uint32 dataLength,
                              DynBuf *imageData,
                              uint32 *fullLength);
Bool UnityPlatformSetDesktopWorkAreas(UnityPlatform *up,
                                      UnityRect workAreas[],
                                      uint32 numWorkAreas);
Bool UnityPlatformSetDesktopConfig(UnityPlatform *up,
                                   const UnityVirtualDesktopArray *desktops);
Bool UnityPlatformSetInitialDesktop(UnityPlatform *up,
                                    UnityDesktopId desktopId);
Bool UnityPlatformSetDesktopActive(UnityPlatform *up,
                                   UnityDesktopId desktopId);
Bool UnityPlatformSetWindowDesktop(UnityPlatform *up,
                                   UnityWindowId windowId,
                                   UnityDesktopId desktopId);
Bool UnityPlatformIsUnityRunning(UnityPlatform *up);
Bool UnityPlatformStartHelperThreads(UnityPlatform *up);
void UnityPlatformKillHelperThreads(UnityPlatform *up);
void UnityPlatformUnlock(UnityPlatform *up);
void UnityPlatformLock(UnityPlatform *up);
void UnityPlatformUpdateDnDDetWnd(UnityPlatform *up,
                                  Bool show);
void UnityPlatformSetActiveDnDDetWnd(UnityPlatform *up, UnityDnD *detWnd);

/* Functions implemented in unity.c for use by the platform-specific code. */
void UnityGetUpdateCommon(int flags, DynBuf *buf);
Bool UnityUpdateThreadInit(UnityUpdateThreadData *updateData);
void UnityUpdateThreadCleanup(UnityUpdateThreadData *updateData);
Bool UnitySendUpdates(UnityUpdateThreadData *updateData);

#endif
