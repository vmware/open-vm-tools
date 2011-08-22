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

/**
 * @file unityPlatform.h
 *
 * Implementation specific functionality
 */

#ifndef _UNITYPLATFORM_H_
#define _UNITYPLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus
#include "unityWindowTracker.h"
#include "unity.h"
#ifdef __cplusplus
};
#endif // __cplusplus

typedef struct _UnityPlatform UnityPlatform;

/*
 * Implemented by unityPlatform[Win32|X11|Cocoa (ha!)].c
 */

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

Bool UnityPlatformIsSupported(void);
UnityPlatform *UnityPlatformInit(UnityWindowTracker *tracker,
                                 UnityHostCallbacks hostCallbacks);
void UnityPlatformCleanup(UnityPlatform *up);
Bool UnityPlatformEnterUnity(UnityPlatform *up);
void UnityPlatformExitUnity(UnityPlatform *up);
Bool UnityPlatformUpdateWindowState(UnityPlatform *up,
                                    UnityWindowTracker *tracker);
void UnityPlatformSaveSystemSettings(UnityPlatform *up);
void UnityPlatformRestoreSystemSettings(UnityPlatform *up);
Bool UnityPlatformGetWindowPath(UnityPlatform *up,
                                UnityWindowId window,
                                DynBuf *windowPathUtf8,
                                DynBuf *execPathUtf8);
Bool UnityPlatformGetBinaryInfo(UnityPlatform *up,
                                const char *pathUtf8,
                                DynBuf *buf);
Bool UnityPlatformSetTopWindowGroup(UnityPlatform *up,
                                    UnityWindowId *windows,
                                    unsigned int windowCount);
Bool UnityPlatformCloseWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformShowWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformHideWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformMinimizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformUnminimizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformMaximizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformUnmaximizeWindow(UnityPlatform *up, UnityWindowId window);
Bool UnityPlatformGetWindowContents(UnityPlatform *up,
                                    UnityWindowId window,
                                    DynBuf *imageData,
                                    uint32 *width,
                                    uint32 *height);
Bool UnityPlatformMoveResizeWindow(UnityPlatform *up,
                                   UnityWindowId window,
                                   UnityRect *moveResizeRect);
void UnityPlatformShowTaskbar(UnityPlatform *up, Bool showTaskbar);
void UnityPlatformShowDesktop(UnityPlatform *up, Bool showDesktop);
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
Bool UnityPlatformStickWindow(UnityPlatform *up,
                              UnityWindowId windowId);
Bool UnityPlatformUnstickWindow(UnityPlatform *up,
                                UnityWindowId windowId);
void UnityPlatformSetInterlockMinimizeOperation(UnityPlatform *up,Bool enabled);
Bool UnityPlatformConfirmMinimizeOperation(UnityPlatform *up,
                                           UnityWindowId windowId,
                                           uint32 sequence,
                                           Bool allow);
Bool UnityPlatformIsUnityRunning(UnityPlatform *up);
void UnityPlatformUnlock(UnityPlatform *up);
void UnityPlatformLock(UnityPlatform *up);
void UnityPlatformUpdateDnDDetWnd(UnityPlatform *up,
                                  Bool show);
void UnityPlatformSetActiveDnDDetWnd(UnityPlatform *up, UnityDnD *detWnd);

void UnityPlatformDoUpdate(UnityPlatform *up, Bool incremental);

void UnityPlatformSetConfigDesktopColor(UnityPlatform *up, int desktopColor);

Bool UnityPlatformRequestWindowContents(UnityPlatform *up,
                                        UnityWindowId windowIds[],
                                        uint32 numWindowIds);

Bool UnityPlatformSendMouseWheel(UnityPlatform *up,
                                 int32 deltaX,
                                 int32 deltaY,
                                 int32 deltaZ,
                                 uint32 modifierFlags);
void UnityPlatformSetDisableCompositing(UnityPlatform *up, Bool disabled);

#ifdef __cplusplus
};
#endif // __cplusplus

#endif // _UNITYPLATFORM_H_
