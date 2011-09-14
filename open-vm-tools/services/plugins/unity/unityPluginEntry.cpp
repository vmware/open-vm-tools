/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * unityPluginEntry.cpp --
 *
 *    Implements the unity plugin for the tools services. Registers for the Unity
 *    RPC's and sets the Unity capabilities.
 *
 *    XXX Rewrite all of this to match CUI models and be, like, readable.
 */

#define G_LOG_DOMAIN "unity"

#include "unityPlugin.h"

#ifndef WIN32
#   include "unityPluginPosix.h"
#endif

extern "C" {
   #include "util.h"
   // guestrpc.h defines the RPCIn_Callback which is needed in rpcin.h which is in turn
   // included by unity.h - so guestrpc.h must precede unity.h
   #include "vmware/tools/guestrpc.h"
   #include "unity.h"
   #include "vmware/tools/plugin.h"
   #include "vmware/tools/utils.h"

   TOOLS_MODULE_EXPORT ToolsPluginData *ToolsOnLoad(ToolsAppCtx *ctx);
};

using namespace vmware::tools;


/*
 *-----------------------------------------------------------------------------
 *
 * ToolsOnLoad --
 *
 *      Plugin entry point.  Returns the registration data.
 *
 * Results:
 *      Registration data.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)           // IN: The app context.
{
   static ToolsPluginData regData = {
      "unity",
      NULL,
      NULL
   };

   if (ctx->rpc != NULL) {
      ToolsPlugin *pluginInstance = NULL;

#if WIN32
      pluginInstance = new UnityPluginWin32(ctx);
#else // Linux
      pluginInstance = new UnityPluginPosix(ctx);
#endif

      if (!pluginInstance) {
         // There's nothing we can do if we can't construct the plugin instance
         return NULL;
      }

      if (!pluginInstance->Initialize()) {
         g_warning("%s: Unity Plugin failed to initialize.\n", __FUNCTION__);
         delete pluginInstance;
         return NULL;
      }
      regData._private = pluginInstance;

      std::vector<RpcChannelCallback> rpcs = pluginInstance->GetRpcCallbackList();
      std::vector<ToolsPluginSignalCb> sigs =
         pluginInstance->GetSignalRegistrations(&regData);

      ToolsAppReg regs[] = {
         { TOOLS_APP_GUESTRPC, VMTools_WrapArray(&rpcs[0], sizeof rpcs[0], rpcs.size()) },
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(&sigs[0], sizeof sigs[0], sigs.size()) },
      };

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

      return &regData;
   }

   return NULL;
}
