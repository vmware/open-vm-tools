/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * toolsPlugin.cpp --
 *
 *    C++ wrapper bits around Tools Core C API.
 */

#include "unityPlugin.h"

extern "C" {
#include <glib.h>

#include "vmware.h"
}


namespace vmware {
namespace tools {


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::ToolsPlugin::GetSignalRegistrations --
 *
 *      Returns a vector containing signal registration info (signal name,
 *      callback, callback context).  Signals will be connected by container
 *      after all plugins have successfully registered.
 *
 * Results:
 *      Vector of signal registration info.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

std::vector<ToolsPluginSignalCb>
ToolsPlugin::GetSignalRegistrations(ToolsPluginData* pdata) // IN: plugin private data
   const
{
   std::vector<ToolsPluginSignalCb> signals;
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_RESET, (void*)OnCReset, pdata));
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_SHUTDOWN, (void*)OnCShutdown, pdata));
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_CAPABILITIES, (void*)OnCCapabilities,
                             pdata));
   signals.push_back(
      ToolsPlugin::SignalCtx(TOOLS_CORE_SIG_SET_OPTION, (void*)OnCSetOption, pdata));
   return signals;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmware::tools::ToolsPlugin::OnCReset --
 *
 *      Handles Tools RPC reset signal.  Acts as thunk between C callback and
 *      Reset member function.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Varies.  See child classes' Reset implementations.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
ToolsPlugin::OnCReset(gpointer src,            // IN: Event source.
                      ToolsAppCtx* ctx,        // IN: The app context.
                      ToolsPluginData* plugin) // IN: Plugin registration data.
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
 * vmware::tools::ToolsPlugin::OnCShutdown --
 *
 *      Handler for plugin shutdown event.  Thunks to C++ Shutdown member
 *      function and deletes the ToolsPlugin instance.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Nukes ToolsPlugin.
 *
 *-----------------------------------------------------------------------------
 */

void
ToolsPlugin::OnCShutdown(gpointer src,            // IN: The source object.
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
 * vmware::tools::ToolsPlugin::OnCCapabilities --
 *
 *      Handler for plugin capability (un)registration event.
 *
 * Results:
 *      A list of capabilities to be sent to the host.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

GArray*
ToolsPlugin::OnCCapabilities(
   gpointer src,            // IGNORED
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
 * vmware::tools::ToolsPlugin::OnCSetOption --
 *
 *      Handler for host->guest "set option" event.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Varies.  See child classes' SetOption implementations.
 *
 *-----------------------------------------------------------------------------
 */

gboolean
ToolsPlugin::OnCSetOption(gpointer src,            // IN: Event source.
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


} // namespace tools
} // namespace vmware
