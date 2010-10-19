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
 * @file unityPlugin.cpp
 *
 *    Implements an object that provides the entry points for tools unity plugin.
 */

#include "vmware/tools/plugin.h"

extern "C" {
   #include "vmware.h"
   #include "conf.h"
   #include "debug.h"
   #include "dynxdr.h"
   #include "guestrpc/unity.h"
   #include "guestrpc/unityActive.h"
   #include "rpcin.h"
   #include "strutil.h"
#if defined(OPEN_VM_TOOLS)
   #include "unitylib/unity.h"
#else
   #include "unity.h"
#endif // OPEN_VM_TOOLS
   #include "xdrutil.h"
};

#include "ghIntegration.h"
#include "ghiTclo.h"
#include "unityPlugin.h"
#include "unityTclo.h"
#include "ghIntegrationInt.h"
#if defined(G_PLATFORM_WIN32)
#include "NotifyIconRpcCallback.h"
#endif // G_PLATFORM_WIN32

#define UNITY_CAP_NAME "unity"

namespace vmware { namespace tools {

/*
 * The static instance of the notification icon RPC callback object. This object
 * sends an RPC to the VMX in response to any change to the set of notification
 * icons.
 */
#if defined(G_PLATFORM_WIN32)
static NotifyIconRpcCallback gNotifyIconCallback;
#endif // G_PLATFORM_WIN32

/**
 * Constructor for the Unity plugin, initialized Unity, and common options values
 *
 * @param[in]  ctx      Host application context.
 *
 */

UnityPlugin::UnityPlugin(ToolsAppCtx *ctx) : mUnityUpdateChannel(NULL)
{
   ASSERT(ctx);

   UnityHostCallbacks unityHostCallbacks;
   memset(&unityHostCallbacks, 0, sizeof unityHostCallbacks);
   unityHostCallbacks.buildUpdateCB = &UnityBuildUpdates;
   unityHostCallbacks.updateCB = &UnityUpdateCallbackFn;
   unityHostCallbacks.sendWindowContents = &UnitySendWindowContents;
   unityHostCallbacks.sendRequestMinimizeOperation = &UnitySendRequestMinimizeOperation;
   unityHostCallbacks.shouldShowTaskbar = &UnityShouldShowTaskbar;
   UnityTcloInit();

   /*
    * Initialize the update channel.
    */
   mUnityUpdateChannel = UnityUpdateChannelInit();
   if (NULL == mUnityUpdateChannel) {
      Warning("%s: Unable to initialize Unity update channel.\n", __FUNCTION__);
      return;
   }
   unityHostCallbacks.updateCbCtx = mUnityUpdateChannel;

   Unity_Init(NULL, unityHostCallbacks, ctx->serviceObj);

   GHITcloInit();
   GHIHostCallbacks ghiHostCallbacks;
   memset(&ghiHostCallbacks, 0, sizeof ghiHostCallbacks);
   ghiHostCallbacks.launchMenuChange = &GHILaunchMenuChangeRPC;
   ghiHostCallbacks.sendTrashFolderState = &GHISendTrashFolderStateRPC;
#if defined(G_PLATFORM_WIN32)
   GHI_Init(ctx->mainLoop, NULL, ghiHostCallbacks);
   GHI_RegisterNotifyIconCallback(&gNotifyIconCallback);
#else
   GHI_Init(ctx->mainLoop, ctx->envp, ghiHostCallbacks);
#endif // G_PLATFORM_WIN32

   if (g_key_file_get_boolean(ctx->config, CONFGROUPNAME_UNITY,
                              CONFNAME_UNITY_ENABLEDEBUG, NULL)) {
      Unity_InitializeDebugger();
   }
   Unity_SetForceEnable(g_key_file_get_boolean(ctx->config, CONFGROUPNAME_UNITY,
                                               CONFNAME_UNITY_FORCEENABLE, NULL));

   /*
    * If no preferred color is in the config file then use a light gray tone,
    * the value is stored as xBGR.
    */
   int desktopColor = 0;
   GError *e = NULL;
   desktopColor = g_key_file_get_integer(ctx->config, CONFGROUPNAME_UNITY,
                                         CONFNAME_UNITY_BACKGROUNDCOLOR, &e);
   if (e != NULL) {
      desktopColor = /* red */ 0xdc |
                     /* green */ 0xdc << 8 |
                     /* blue */ 0xdc << 16;
   }
   Unity_SetConfigDesktopColor(desktopColor);
}


/**
 * Destructor for the Unity plugin, cleanup Unity
 */

UnityPlugin::~UnityPlugin()
{
   Unity_Cleanup();
   UnityUpdateChannelCleanup(mUnityUpdateChannel);
   UnityTcloCleanup();
#if defined(G_PLATFORM_WIN32)
   GHI_UnregisterNotifyIconCallback(&gNotifyIconCallback);
#endif // G_PLATFORM_WIN32
   GHI_Cleanup();
   GHITcloCleanup();
}

/**
 * Called by the service core when the host requests the capabilities supported
 * by the guest tools.
 *
 * @param[in]  set      Whether capabilities are being set or unset.
 *
 * @return A list of capabilities to be sent to the host.
 */

std::vector<ToolsAppCapability>
UnityPlugin::GetCapabilities(gboolean set)
{
   std::vector<ToolsAppCapability> capsVector;

   /* We can't use UNITY_RPC_UNITY_CAP here because that define includes the
    * tools.capability prefix which CoreServices will automatically prepend to the
    * supplied name.
    */
   capsVector.push_back(ToolsAppCapabilityOldEntry(UNITY_CAP_NAME, Unity_IsSupported() ? 1 : 0));

   capsVector.push_back(ToolsAppCapabilityNewEntry(UNITY_CAP_STATUS_UNITY_ACTIVE, TRUE));

   return capsVector;
}


/**
 * Called by the service core when the host requests the RPCs supported
 * by the guest tools.
 *
 * @return A list of RPC Callbacks to be sent to the host.
 */

std::vector<RpcChannelCallback>
UnityPlugin::GetRpcCallbackList()
{
   std::vector<RpcChannelCallback> rpcList;

   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_ENTER, UnityTcloEnter));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_UPDATE_FULL, UnityTcloGetUpdate));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_UPDATE_INCREMENTAL, UnityTcloGetUpdate));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_WINDOW_PATH, UnityTcloGetWindowPath));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_WINDOW_SETTOP, UnityTcloSetTopWindowGroup));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_WINDOW_CONTENTS, UnityTcloGetWindowContents));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_ICON_DATA, UnityTcloGetIconData));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_DESKTOP_WORK_AREA_SET, UnityTcloSetDesktopWorkArea));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_SHOW_TASKBAR, UnityTcloShowTaskbar));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_EXIT, UnityTcloExit));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_WINDOW_MOVE_RESIZE, UnityTcloMoveResizeWindow));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_DESKTOP_CONFIG_SET, UnityTcloSetDesktopConfig));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_DESKTOP_ACTIVE_SET, UnityTcloSetDesktopActive));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_WINDOW_DESKTOP_SET, UnityTcloSetWindowDesktop));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_CONFIRM_OPERATION, UnityTcloConfirmOperation));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_SET_OPTIONS, UnityTcloSetUnityOptions));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_WINDOW_CONTENTS_REQUEST, UnityTcloRequestWindowContents));
   rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_SEND_MOUSE_WHEEL, UnityTcloSendMouseWheel));

   char **commandList = NULL;
   Unity_GetWindowCommandList(&commandList);
   for (int i = 0; commandList && (NULL != commandList[i]); i++) {
      rpcList.push_back(RpcChannelCallbackEntry(commandList[i], UnityTcloWindowCommand));
   }

   if (GHI_IsSupported()) {
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_BINARY_INFO, GHITcloGetBinaryInfo));
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_OPEN_LAUNCHMENU, GHITcloOpenStartMenu));
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_LAUNCHMENU_ITEM, GHITcloGetStartMenuItem));
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_CLOSE_LAUNCHMENU, GHITcloCloseStartMenu));
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_SHELL_OPEN, GHITcloShellOpen));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_GUEST_SHELL_ACTION, GHITcloShellAction));
      rpcList.push_back(RpcChannelCallbackEntry(UNITY_RPC_GET_BINARY_HANDLERS, GHITcloGetBinaryHandlers));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_SET_GUEST_HANDLER, GHITcloSetGuestHandler));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_RESTORE_DEFAULT_GUEST_HANDLER, GHITcloRestoreDefaultGuestHandler));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_OUTLOOK_SET_TEMP_FOLDER, GHITcloSetOutlookTempFolder));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_OUTLOOK_RESTORE_TEMP_FOLDER, GHITcloRestoreOutlookTempFolder));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_TRASH_FOLDER_ACTION, GHITcloTrashFolderAction));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_TRASH_FOLDER_GET_ICON, GHITcloTrashFolderGetIcon));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_TRAY_ICON_SEND_EVENT, GHITcloTrayIconSendEvent));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_TRAY_ICON_START_UPDATES, GHITcloTrayIconStartUpdates));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_TRAY_ICON_STOP_UPDATES, GHITcloTrayIconStopUpdates));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_SET_FOCUSED_WINDOW, GHITcloSetFocusedWindow));
      rpcList.push_back(RpcChannelCallbackEntry(GHI_RPC_GET_EXEC_INFO_HASH, GHITcloGetExecInfoHash));
   }

   return rpcList;
}

} /* namespace tools */ } /* namespace vmware */
