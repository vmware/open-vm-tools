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
   #include "vmware/tools/desktopevents.h"

   TOOLS_MODULE_EXPORT ToolsPluginData *ToolsOnLoad(ToolsAppCtx *ctx);
};

using namespace vmware::tools;

/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginCapabilities --
 *
 *      Called by the service core when the host requests the capabilities
 *      supported by the guest tools.
 *
 * Results:
 *      A list of capabilities to be sent to the host.
 *
 * Side effects:
 *
 *
 *-----------------------------------------------------------------------------
 */

static GArray *
UnityPluginCapabilities(gpointer src,            // IGNORED
                        ToolsAppCtx *ctx,        // IN: The app context.
                        gboolean set,            // IN: true if setting, else unsetting
                        ToolsPluginData *plugin) // IN: Plugin registration data.
{
   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   std::vector<ToolsAppCapability> capabilities = pluginInstance->GetCapabilities(set);

   g_debug("%s: got capability signal, setting = %d.\n", __FUNCTION__, set);
   return VMTools_WrapArray(&capabilities[0], sizeof capabilities[0],
                            capabilities.size());
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginReset --
 *
 *      Handles a reset signal.  Just logs debug information.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
UnityPluginReset(gpointer src,            // IN: Event source.
                 ToolsAppCtx *ctx,        // IN: The app context.
                 ToolsPluginData *plugin) // IN: Plugin registration data.
{
   ASSERT(ctx != NULL);
   g_debug("%s: reset signal for app %s\n", __FUNCTION__, ctx->name);

   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   return pluginInstance->Reset(src);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginShutdown --
 *
 *      Handles a shutdown callback; just logs debug information.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

static void
UnityPluginShutdown(gpointer src,            // IN: The source object.
                    ToolsAppCtx *ctx,        // IN: The app context.
                    ToolsPluginData *plugin) // Plugin registration data.
{
   g_debug("%s: shutdown signal.\n", __FUNCTION__);

   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   pluginInstance->Shutdown(src);

   delete pluginInstance;
   plugin->_private = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginSetOption --
 *
 *      Handles a "Set_Option" callback. Just logs debug information.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      XXX.
 *
 *-----------------------------------------------------------------------------
 */

static gboolean
UnityPluginSetOption(gpointer src,            // IN: Event source.
                     ToolsAppCtx *ctx,        // IN: The app context.
                     const gchar *option,     // IN: Option to set.
                     const gchar *value,      // IN: Option value.
                     ToolsPluginData *plugin) // IN: Plugin registration data.
{
   g_debug("%s: set '%s' to '%s'\n", __FUNCTION__, option, value);
   ToolsPlugin *pluginInstance = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(pluginInstance);

   return pluginInstance->SetOption(src, std::string(option), std::string(value));
}

#ifdef WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * UnityPluginServiceControl --
 *
 *      C thunk for TOOLS_CORE_SIG_SERVICE_CONTROL messages.
 *
 * Results:
 *      Returns the result of calling OnServiceControl on the plugin object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

DWORD
UnityPluginServiceControl(gpointer src,                 // IN
                          ToolsAppCtx *ctx,             // IN
                          SERVICE_STATUS_HANDLE handle, // IN
                          guint control,                // IN
                          guint evtType,                // IN
                          gpointer evtData,             // IN
                          ToolsPluginData *data)        // IN
{
   ToolsPlugin *p = reinterpret_cast<ToolsPlugin*>(data->_private);
   ASSERT(p);
   return p->OnServiceControl(src, ctx, handle, control, evtType, evtData);
}


void
UnityPluginDesktopSwitch(gpointer src,            // IN
                         ToolsAppCtx *ctx,        // IN
                         ToolsPluginData *plugin) // IN
{
   ToolsPlugin *p = reinterpret_cast<ToolsPlugin*>(plugin->_private);
   ASSERT(p);
   p->OnDesktopSwitch();
}


#endif

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
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_RESET, (void *) UnityPluginReset, &regData },
         { TOOLS_CORE_SIG_SHUTDOWN, (void *) UnityPluginShutdown, &regData },
         { TOOLS_CORE_SIG_CAPABILITIES, (void *) UnityPluginCapabilities, &regData },
         { TOOLS_CORE_SIG_SET_OPTION, (void *) UnityPluginSetOption, &regData },
#ifdef WIN32
         { TOOLS_CORE_SIG_SERVICE_CONTROL, (void *) UnityPluginServiceControl, &regData },
         { TOOLS_CORE_SIG_DESKTOP_SWITCH, (void *) UnityPluginDesktopSwitch, &regData },
#endif
      };

      ToolsPlugin *pluginInstance = NULL;

#if WIN32
      pluginInstance = new UnityPluginWin32();
#else // Linux
      pluginInstance = new UnityPlugin();
#endif

      if (!pluginInstance) {
         // There's nothing we can do if we can't construct the plugin instance
         return NULL;
      }

      if (!pluginInstance->Initialize(ctx)) {
         g_warning("%s: Unity Plugin failed to initialize.\n", __FUNCTION__);
         delete pluginInstance;
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

   return NULL;
}
