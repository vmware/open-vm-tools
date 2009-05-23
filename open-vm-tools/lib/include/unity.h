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

#include <string.h>
#include "dndGuest.h"
#include "dbllnklst.h"
#include "guestApp.h"
#include "dynbuf.h"
#include "str.h"
#include "rpcin.h"
#ifdef _WIN32
#include "unityCommon.h"
#endif
/*
 * In Unity mode, all our DnD detection windows will be ignored and not displayed
 * on host desktop. Right now we have 4 DnD detection window. 2 for DnD version 2
 * or older, 2 for DnD version 3 or newer.
 */
enum{
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
 * Represents a virtual desktop configuration. 
 */

typedef struct UnityVirtualDesktopArray {
   size_t desktopCount;                              // number of desktops in the grid
   UnityVirtualDesktop desktops[MAX_VIRT_DESK];      // array of desktops
} UnityVirtualDesktopArray;


void Unity_Init(GuestApp_Dict *conf, int* blockedWnd);
void Unity_InitBackdoor(struct RpcIn *rpcIn);
Bool Unity_IsActive(void);
Bool Unity_IsSupported(void);
void Unity_SetActiveDnDDetWnd(UnityDnD *state);
void Unity_Exit(void);
void Unity_Cleanup(void);
void Unity_RegisterCaps(void);
void Unity_UnregisterCaps(void);
void Unity_UnityToLocalPoint(UnityPoint *localPt, UnityPoint *unityPt);
void Unity_LocalToUnityPoint(UnityPoint *unityPt, UnityPoint *localPt);
#ifdef _WIN32
HWND Unity_GetHwndFromUnityId(UnityWindowId id);
#endif

#endif

