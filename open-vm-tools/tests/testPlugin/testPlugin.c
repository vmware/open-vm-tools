/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file testPlugin.c
 *
 *    Implements a test plugin for the tools services. The plugin register for
 *    a few RPCs that are never sent by the VMX, so to "use" it you have to use
 *    a debug plugin that sends those RPCs. The test debug plugin does that.
 */

#define G_LOG_DOMAIN "test"

#include <glib-object.h>
#include <gmodule.h>
#include "testData.h"
#include "util.h"
#include "vmtoolsApp.h"
#include "vmtools.h"
#include "guestrpc/ghiGetBinaryHandlers.h"


/**
 * Handles a "test.rpcin.msg1" RPC message. The incoming data should be an
 * XDR-encoded GHIBinaryHandlersIconDetails struct; the struct is written back
 * to the RPC channel using RpcChannel_Send (command "test.rpcout.msg1").
 *
 * Also emits a "test-signal", to test custom signal registration.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static Bool
TestPluginRpc1(RpcInData *data)
{
   ToolsAppCtx *ctx = data->appCtx;
   GHIBinaryHandlersIconDetails *details = (GHIBinaryHandlersIconDetails *) data->args;
   char *cmd;
   size_t cmdLen;

   g_assert(details->width == 100);
   g_assert(details->height == 200);
   g_assert(strcmp(details->identifier, "rpc1test") == 0);

   g_signal_emit_by_name(ctx->serviceObj, "test-signal");

   if (!RpcChannel_BuildXdrCommand("test.rpcout.msg1",
                                   xdr_GHIBinaryHandlersIconDetails,
                                   details,
                                   &cmd,
                                   &cmdLen)) {
      g_error("Failed to create test.rpcout.msg1 command.\n");
   }

   if (!RpcChannel_Send(ctx->rpc, cmd, cmdLen, NULL, NULL)) {
      g_error("Failed to send 'test.rpcout.msg1' message.\n");
   }

   vm_free(cmd);

   g_debug("Successfully handled rpc %s\n", data->name);
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Handles a "test.rpcin.msg2" RPC message.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static Bool
TestPluginRpc2(RpcInData *data)
{
   g_debug("%s: %s\n", __FUNCTION__, data->name);
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Handles a "test.rpcin.msg3" RPC message. Logs information to the
 * output and returns data to be serialized using XDR.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static Bool
TestPluginRpc3(RpcInData *data)
{
   TestPluginData *ret;
   g_debug("%s: %s\n", __FUNCTION__, data->name);

   ret = g_malloc(sizeof *ret);
   ret->data = Util_SafeStrdup("Hello World!");

   data->result = (char *) ret;
   return TRUE;
}


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
TestPluginCapabilities(gpointer src,
                       ToolsAppCtx *ctx,
                       gboolean set,
                       ToolsPluginData *plugin)
{
   ToolsAppCapability caps[] = {
      { TOOLS_CAP_OLD, "resolution_set", 0, 1 },
      { TOOLS_CAP_OLD, "display_topology_set", 0, 2 },
      { TOOLS_CAP_NEW, NULL, UNITY_CAP_START_MENU, 1 },
      { TOOLS_CAP_NEW, NULL, GHI_CAP_SHELL_ACTION_BROWSE, 1 }
   };

   g_debug("%s: got capability signal, setting = %d.\n", __FUNCTION__, set);
   return VMTools_WrapArray(caps, sizeof *caps, ARRAYSIZE(caps));
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
TestPluginReset(gpointer src,
                ToolsAppCtx *ctx,
                ToolsPluginData *plugin)
{
   g_assert(ctx != NULL);
   g_debug("%s: reset signal for app %s\n", __FUNCTION__, ctx->name);
   return TRUE;
}


#if defined(G_PLATFORM_WIN32)
/**
 * Handles a session state change callback; this is only called on Windows,
 * from both the "vmsvc" instance (handled by SCM notifications) and from
 * "vmusr" with the "fast user switch" plugin.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  code     Session state change code.
 * @param[in]  id       Session ID.
 * @param[in]  data     Client data.
 */

static void
TestPluginSessionChange(gpointer src,
                        ToolsAppCtx *ctx,
                        DWORD code,
                        DWORD sessionId,
                        ToolsPluginData *plugin)
{
   g_debug("Got session state change signal, code = %u, id = %u\n", code, sessionId);
}
#endif


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
TestPluginShutdown(gpointer src,
                   ToolsAppCtx *ctx,
                   ToolsPluginData *plugin)
{
   g_debug("%s: shutdown signal.\n", __FUNCTION__);
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
TestPluginSetOption(gpointer src,
                    ToolsAppCtx *ctx,
                    const gchar *option,
                    const gchar *value,
                    ToolsPluginData *plugin)
{
   g_debug("%s: set '%s' to '%s'\n", __FUNCTION__, option, value);
   return TRUE;
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
      "testPlugin",
      NULL,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { "test.rpcin.msg1",
         TestPluginRpc1, NULL, xdr_GHIBinaryHandlersIconDetails, NULL,
         sizeof (GHIBinaryHandlersIconDetails) },
      { "test.rpcin.msg2",
         TestPluginRpc2, NULL, NULL, NULL, 0 },
      { "test.rpcin.msg3",
            TestPluginRpc3, NULL, NULL, xdr_TestPluginData, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_RESET, TestPluginReset, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, TestPluginShutdown, &regData },
      { TOOLS_CORE_SIG_CAPABILITIES, TestPluginCapabilities, &regData },
      { TOOLS_CORE_SIG_SET_OPTION, TestPluginSetOption, &regData },
#if defined(G_PLATFORM_WIN32)
      { TOOLS_CORE_SIG_SESSION_CHANGE, TestPluginSessionChange, &regData },
#endif
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

   g_signal_new("test-signal",
                G_OBJECT_TYPE(ctx->serviceObj),
                0,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   return &regData;
}

