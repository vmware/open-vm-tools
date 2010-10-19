/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * @file unityTclo.h
 *
 *    Functions for handling/dispatching Unity TCLO RPCs.
 */

#include "vmware/tools/plugin.h"
#include "unityPlugin.h"

extern "C" {
   #include "rpcin.h"
};

namespace vmware { namespace tools {

void UnityTcloInit();
void UnityTcloCleanup();

UnityUpdateChannel *UnityUpdateChannelInit(void);
void UnityUpdateChannelCleanup(UnityUpdateChannel *updateChannel);
Bool UnitySendUpdates(void *param);
void UnityUpdateCallbackFn(void *param, UnityUpdate *update);
Bool UnityBuildUpdates(void *param, int flags);

Bool UnitySendWindowContents(UnityWindowId windowID,
                             uint32 imageWidth,
                             uint32 imageHeight,
                             const char *imageData,
                             uint32 imageLength);
Bool UnitySendRequestMinimizeOperation(UnityWindowId windowId,
                                       uint32 sequence);
Bool UnityShouldShowTaskbar();

gboolean UnityTcloEnter(RpcInData *data);
gboolean UnityTcloGetUpdate(RpcInData *data);
gboolean UnityTcloExit(RpcInData *data);
gboolean UnityTcloGetWindowPath(RpcInData *data);
gboolean UnityTcloWindowCommand(RpcInData *data);
gboolean UnityTcloGetWindowContents(RpcInData *data);
gboolean UnityTcloGetIconData(RpcInData *data);
gboolean UnityTcloSetDesktopWorkArea(RpcInData *data);
gboolean UnityTcloSetTopWindowGroup(RpcInData *data);
gboolean UnityTcloShowTaskbar(RpcInData *data);
gboolean UnityTcloMoveResizeWindow(RpcInData *data);
gboolean UnityTcloSetDesktopConfig(RpcInData *data);
gboolean UnityTcloSetDesktopActive(RpcInData *data);
gboolean UnityTcloSetWindowDesktop(RpcInData *data);
gboolean UnityTcloConfirmOperation(RpcInData *data);
gboolean UnityTcloSetUnityOptions(RpcInData *data);
gboolean UnityTcloRequestWindowContents(RpcInData *data);
gboolean UnityTcloSendMouseWheel(RpcInData *data);

} /* namespace tools */ } /* namespace vmware */
