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

/**
 * @file unityPlugin.c
 *
 *    Implements the unity plugin for the tools services. Registers for the Unity
 *    RPC's and sets the Unity capabilities.
 */

#define G_LOG_DOMAIN "unity"

#include "unityPlugin.h"

extern "C" {
   #include "util.h"
   // guestrpc.h defines the RPCIn_Callback which is needed in rpcin.h which is in turn
   // included by unity.h - so guestrpc.h must precede unity.h
   #include "vmware/tools/guestrpc.h"
   #include "unity.h"
   #include "vmware/tools/plugin.h"
   #include "vmware/tools/utils.h"
};

extern "C" {
   TOOLS_MODULE_EXPORT ToolsPluginData *ToolsOnLoad(ToolsAppCtx *ctx);
};

using namespace vmware::tools;

/**
 * Called by the service core when the host requests the capabilities supported
 * by the guest tools.
 *
 * @param[in]  src      Unused.
 * @param[in]  ctx      The app context.
 * @param[in]  set      Whether capabilities are being set or unset (unused).
 * @param[in]  plugin   Plugin registration data.
 *
 * @return A list of capabilities to be sent to the host.
 */

static GArray *
UnityPluginCapabilities(gpointer src,
                        ToolsAppCtx *ctx,
                        gboolean set,
                        ToolsPluginData *plugin)
{
   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   std::vector<ToolsAppCapability> capabilities = pluginInstance->GetCapabilities(set);

   g_debug("%s: got capability signal, setting = %d.\n", __FUNCTION__, set);
   return VMTools_WrapArray(&capabilities[0], sizeof capabilities[0], capabilities.size());
}


/**
 * Handles a reset signal; just logs debug information. This callback is
 * called when the service receives a "reset" message from the VMX, meaning
 * the VMX may be restarting the RPC channel (due, for example, to restoring
 * a snapshot, resuming a VM or a VMotion), and should be used to reset any
 * application state that depends on the VMX.
 *
 * @param[in]  src      Event source.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 *
 * @return TRUE on success.
 */

static gboolean
UnityPluginReset(gpointer src,
                 ToolsAppCtx *ctx,
                 ToolsPluginData *plugin)
{
   ASSERT(ctx != NULL);
   g_debug("%s: reset signal for app %s\n", __FUNCTION__, ctx->name);

   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   return pluginInstance->Reset(src);
}



/**
 * Handles a shutdown callback; just logs debug information. This is called
 * before the service is shut down, and should be used to clean up any resources
 * that were initialized by the application.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 */

static void
UnityPluginShutdown(gpointer src,
                    ToolsAppCtx *ctx,
                    ToolsPluginData *plugin)
{
   g_debug("%s: shutdown signal.\n", __FUNCTION__);

   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   pluginInstance->Shutdown(src);

   delete pluginInstance;
   plugin->_private = NULL;
}


/**
 * Handles a "Set_Option" callback. Just logs debug information. This callback
 * is called when the VMX sends a "Set_Option" command to tools, to configure
 * different options whose values are kept outside of the virtual machine.
 *
 * @param[in]  src      Event source.
 * @param[in]  ctx      The app context.
 * @param[in]  plugin   Plugin registration data.
 * @param[in]  option   Option being set.
 * @param[in]  value    Option value.
 *
 * @return TRUE on success.
 */

static gboolean
UnityPluginSetOption(gpointer src,
                     ToolsAppCtx *ctx,
                     const gchar *option,
                     const gchar *value,
                     ToolsPluginData *plugin)
{
   g_debug("%s: set '%s' to '%s'\n", __FUNCTION__, option, value);
   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   return pluginInstance->SetOption(src, std::string(option), std::string(value));
}


/**
 * Plugin entry point. Returns the registration data. This is called once when
 * the plugin is loaded into the service process.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "unity",
      NULL,
      NULL
   };

   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_RESET, (void *) UnityPluginReset, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, (void *) UnityPluginShutdown, &regData },
      { TOOLS_CORE_SIG_CAPABILITIES, (void *) UnityPluginCapabilities, &regData },
      { TOOLS_CORE_SIG_SET_OPTION, (void *) UnityPluginSetOption, &regData },
   };

   ToolsPlugin *pluginInstance = NULL;

#if WIN32
   pluginInstance = new UnityPluginWin32(ctx);
#else // Linux
   pluginInstance = new UnityPlugin(ctx);
#endif

   if (!pluginInstance) {
      // There's nothing we can do if we can't construct the plugin instance
      return NULL;
   }
   regData._private = pluginInstance;

   std::vector<RpcChannelCallback> rpcs = pluginInstance->GetRpcCallbackList();

   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(&rpcs[0], sizeof rpcs[0], rpcs.size()) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   return &regData;
}

