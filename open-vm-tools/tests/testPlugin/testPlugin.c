/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
#include <CUnit/CUnit.h>

#include "testData.h"
#include "util.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/rpcdebug.h"
#include "vmware/tools/utils.h"

#define TEST_APP_PROVIDER        "TestProvider"
#define TEST_APP_NAME            "TestProviderApp1"
#define TEST_APP_ERROR           "TestProviderError"
#define TEST_APP_DONT_REGISTER   "TestProviderDontRegister"

#define TEST_SIG_INVALID   "TestInvalidSignal"

typedef struct TestApp {
   const char *name;
} TestApp;


static gboolean gInvalidAppError = FALSE;
static gboolean gInvalidAppProvider = FALSE;
static gboolean gInvalidSigError = FALSE;
static gboolean gValidAppRegistration = FALSE;


/**
 * Handles a "test.rpcin.msg1" RPC message. The incoming data should be an
 * XDR-encoded TestPluginData struct; the struct is written back
 * to the RPC channel using RpcChannel_Send (command "test.rpcout.msg1").
 *
 * Also emits a "test-signal", to test custom signal registration.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static gboolean
TestPluginRpc1(RpcInData *data)
{
   ToolsAppCtx *ctx = data->appCtx;
   TestPluginData *testdata = (TestPluginData *) data->args;
   char *cmd;
   size_t cmdLen;

   CU_ASSERT_STRING_EQUAL(testdata->data, "rpc1test");
   CU_ASSERT_EQUAL(testdata->f_int, 1357);
   CU_ASSERT(testdata->f_bool);

   g_signal_emit_by_name(ctx->serviceObj, "test-signal");

   if (!RpcChannel_BuildXdrCommand("test.rpcout.msg1",
                                   xdr_TestPluginData,
                                   testdata,
                                   &cmd,
                                   &cmdLen)) {
      vm_error("Failed to create test.rpcout.msg1 command.");
   }

   if (!RpcChannel_Send(ctx->rpc, cmd, cmdLen, NULL, NULL)) {
      vm_error("Failed to send 'test.rpcout.msg1' message.");
   }

   vm_free(cmd);

   vm_debug("Successfully handled rpc %s", data->name);
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Handles a "test.rpcin.msg2" RPC message.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static gboolean
TestPluginRpc2(RpcInData *data)
{
   vm_debug("%s", data->name);
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

static gboolean
TestPluginRpc3(RpcInData *data)
{
   TestPluginData *ret;
   vm_debug("%s", data->name);

   ret = g_malloc(sizeof *ret);
   ret->data = Util_SafeStrdup("Hello World!");
   ret->f_int = 8642;
   ret->f_bool = TRUE;

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

   vm_debug("got capability signal, setting = %d.", set);
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
   RPCDEBUG_ASSERT(ctx != NULL, FALSE);
   vm_debug("reset signal for app %s", ctx->name);
   return TRUE;
}


#if defined(G_PLATFORM_WIN32)
/**
 * Handles a service control signal; this is only called on Windows.
 *
 * @param[in] src                    The source object.
 * @param[in] ctx                    The application context.
 * @param[in] serviceStatusHandle    Handle of type SERVICE_STATUS_HANDLE.
 * @param[in] controlCode            Control code.
 * @param[in] eventType              Unused.
 * @param[in] eventData              Unused.
 * @param[in] data                   Unused.
 *
 * @retval ERROR_CALL_NOT_IMPLEMENTED
 */

static DWORD
TestPluginServiceControl(gpointer src,
                         ToolsAppCtx *ctx,
                         gpointer serviceStatusHandle,
                         guint controlCode,
                         guint eventType,
                         gpointer eventData,
                         gpointer data)
{
   vm_debug("Got service control signal, code = %u, event = %u",
            controlCode, eventType);
   return ERROR_CALL_NOT_IMPLEMENTED;
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
   vm_debug("shutdown signal.");
   CU_ASSERT(gInvalidSigError);
   CU_ASSERT(gInvalidAppError);
   CU_ASSERT(gInvalidAppProvider);
   CU_ASSERT(gValidAppRegistration);
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
   vm_debug("set '%s' to '%s'", option, value);
   return TRUE;
}


/**
 * Prints out the registration data for the test provider.
 *
 * @param[in] ctx     Unused.
 * @param[in] prov    Unused.
 * @param[in] plugin  Unused.
 * @param[in] reg     Registration data (should be a string).
 *
 * @retval FALSE if registration value is TEST_APP_ERROR.
 * @retval TRUE otherwise.
 */

static gboolean
TestProviderRegisterApp(ToolsAppCtx *ctx,
                        ToolsAppProvider *prov,
                        ToolsPluginData *plugin,
                        gpointer reg)
{
   TestApp *app = reg;
   vm_debug("registration data is '%s'", app->name);
   gValidAppRegistration |= strcmp(app->name, TEST_APP_NAME) == 0;
   CU_ASSERT(strcmp(app->name, TEST_APP_DONT_REGISTER) != 0);
   return (strcmp(app->name, TEST_APP_ERROR) != 0);
}


/**
 * Registration error callback; make sure it's called for the errors we expect.
 *
 * @see plugin.h (for parameter descriptions)
 *
 * @retval FALSE for TEST_APP_ERROR.
 * @retval TRUE otherwise.
 */

static gboolean
TestPluginErrorCb(ToolsAppCtx *ctx,
                  ToolsAppType type,
                  gpointer data,
                  ToolsPluginData *plugin)
{
   /* Make sure the non-existant signal we tried to register fires an error. */
   if (type == TOOLS_APP_SIGNALS) {
      ToolsPluginSignalCb *sig = data;
      CU_ASSERT(strcmp(sig->signame, TEST_SIG_INVALID) == 0);
      gInvalidSigError = TRUE;
   }

   /* Make sure we're notified about the "error" app we tried to register. */
   if (type == 42) {
      TestApp *app = data;
      CU_ASSERT(strcmp(app->name, TEST_APP_ERROR) == 0);
      gInvalidAppError = TRUE;
      return FALSE;
   }

   /* Make sure we're notified about a non-existant app provider. */
   if (type == 43) {
      CU_ASSERT(data == NULL);
      gInvalidAppProvider = TRUE;
   }

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
      TestPluginErrorCb,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { "test.rpcin.msg1",
         TestPluginRpc1, NULL, xdr_TestPluginData, NULL,
         sizeof (TestPluginData) },
      { "test.rpcin.msg2",
         TestPluginRpc2, NULL, NULL, NULL, 0 },
      { "test.rpcin.msg3",
            TestPluginRpc3, NULL, NULL, xdr_TestPluginData, 0 }
   };
   ToolsAppProvider provs[] = {
      { TEST_APP_PROVIDER, 42, sizeof (char *), NULL, TestProviderRegisterApp, NULL, NULL }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_RESET, TestPluginReset, &regData },
      { TOOLS_CORE_SIG_SHUTDOWN, TestPluginShutdown, &regData },
      { TOOLS_CORE_SIG_CAPABILITIES, TestPluginCapabilities, &regData },
      { TOOLS_CORE_SIG_SET_OPTION, TestPluginSetOption, &regData },
#if defined(G_PLATFORM_WIN32)
      { TOOLS_CORE_SIG_SERVICE_CONTROL, TestPluginServiceControl, &regData },
#endif
      { TEST_SIG_INVALID, TestPluginReset, &regData },
   };
   TestApp tapp[] = {
      { TEST_APP_NAME },
      { TEST_APP_ERROR },
      { TEST_APP_DONT_REGISTER }
   };
   TestApp tnoprov[] = {
      { "TestAppNoProvider" }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTOOLS_WRAP_ARRAY(rpcs) },
      { TOOLS_APP_PROVIDER, VMTOOLS_WRAP_ARRAY(provs) },
      { TOOLS_APP_SIGNALS,  VMTOOLS_WRAP_ARRAY(sigs) },
      { 42,                 VMTOOLS_WRAP_ARRAY(tapp) },
      { 43,                 VMTOOLS_WRAP_ARRAY(tnoprov) },
   };

   vm_info("loading test plugin...");

   g_signal_new("test-signal",
                G_OBJECT_TYPE(ctx->serviceObj),
                0,
                0,
                NULL,
                NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);

   regData.regs = VMTOOLS_WRAP_ARRAY(regs);
   return &regData;
}

