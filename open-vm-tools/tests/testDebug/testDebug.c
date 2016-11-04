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
 * @file testDebug.c
 *
 * A simple debug plugin that validates the messages sent by the service after
 * a "reset" is received, and also interacts with the test plugin to test the
 * functions provided by the service.
 */

#define G_LOG_DOMAIN "testDebug"
#include <glib-object.h>
#include <CUnit/CUnit.h>

#include "util.h"
#include "testData.h"
#include "xdrutil.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/rpcdebug.h"

static gboolean
TestDebugValidateReset(RpcInData *data, gboolean ret);

static gboolean
TestDebugValidateUnknown(RpcInData *data, gboolean ret);

static gboolean
TestDebugValidateRpc3(RpcInData *data, gboolean ret);

#define SET_OPTION_TEST ("Set_Option " TOOLSOPTION_BROADCASTIP " 1")

/** RPC messages injected into the application using RpcDebug_SendNext(). */
static RpcDebugMsgMapping gRpcMessages[] = {
   { "reset", sizeof "reset", TestDebugValidateReset, FALSE },
   { "ping", sizeof "ping", NULL, FALSE },
   { "Capabilities_Register", sizeof "Capabilities_Register", NULL, FALSE },
   { "test.rpcin.unknown", sizeof "test.rpcin.unknown", TestDebugValidateUnknown, FALSE },
   /* This one is initialized manually, since it contains dynamic data. */
   { NULL, 0, NULL, TRUE },
   { "test.rpcin.msg2", sizeof "test.rpcin.msg2", NULL, FALSE },
   { "test.rpcin.msg3", sizeof "test.rpcin.msg3", TestDebugValidateRpc3, FALSE },
   { SET_OPTION_TEST, sizeof SET_OPTION_TEST, NULL, FALSE },
   { "Capabilities_Register", sizeof "Capabilities_Register", NULL, FALSE },
   /* NULL terminator. */
   { NULL, 0, NULL, FALSE }
};

static gboolean gSignalReceived = FALSE;
static ToolsAppCtx *gCtx;


/**
 * Handles a "test-signal" sent by the test plugin. Just sets a static variable
 * that will be checked by TestDebugReceiveRpc1 to make sure custom signals are
 * working.
 *
 * @param[in]  src      Unused.
 * @param[in]  data     Unused.
 */

static void
TestDebugHandleSignal(gpointer src,
                      gpointer data)
{
   g_debug("Received test signal.\n");
   gSignalReceived = TRUE;
}


/**
 * Validates the response from a "reset".
 *
 * @param[in]  data     RPC request data.
 * @param[in]  ret      Return value from RPC handler.
 *
 * @return @a ret.
 */

static gboolean
TestDebugValidateReset(RpcInData *data,
                       gboolean ret)
{
   RPCDEBUG_ASSERT(data->result != NULL, FALSE);
   CU_ASSERT_STRING_EQUAL(data->result, "ATR debug");
   return (gboolean) ret;
}


/**
 * Validates a "test.rpcout.msg1" message sent by the test plugin. This message
 * is sent when the plugin receives a "test.rpcin.msg1" RPC, and contains an
 * XDR-encoded TestPluginData struct.
 *
 * @param[in]  data        Incoming data.
 * @param[in]  dataLen     Size of incoming data.
 * @param[out] result      Result sent back to the application.
 * @param[out] resultLen   Length of result.
 *
 * @return TRUE (asserts on failure).
 */

static gboolean
TestDebugReceiveRpc1(char *data,
                     size_t dataLen,
                     char **result,
                     size_t *resultLen)
{
   TestPluginData *details = (TestPluginData *) data;

   CU_ASSERT(gSignalReceived);
   CU_ASSERT_STRING_EQUAL(details->data, "rpc1test");
   CU_ASSERT_EQUAL(details->f_int, 1357);
   CU_ASSERT(details->f_bool);

   return TRUE;
}


/**
 * Validates the response of the "msg3" RPC.
 *
 * @param[in] data   RPC data.
 * @param[in] ret    RPC result.
 *
 * @return Whether the RPC succeded.
 */

static gboolean
TestDebugValidateRpc3(RpcInData *data,
                      gboolean ret)
{
   TestPluginData pdata = { NULL, };

   CU_ASSERT(XdrUtil_Deserialize(data->result, data->resultLen,
                                 xdr_TestPluginData, &pdata));

   CU_ASSERT_STRING_EQUAL(pdata.data, "Hello World!");
   CU_ASSERT_EQUAL(pdata.f_int, 8642);
   CU_ASSERT(pdata.f_bool);

   VMX_XDR_FREE(xdr_TestPluginData, &pdata);
   return ret;
}


/**
 * Validates a "version" message sent during capability registration.
 * No validation is actually done - the message is just printed.
 *
 * @param[in]  data        Incoming data.
 * @param[in]  dataLen     Size of incoming data.
 * @param[out] result      Result sent back to the application.
 * @param[out] resultLen   Length of result.
 *
 * @return TRUE.
 */

static gboolean
TestDebugReceiveVersion(char *data,
                        size_t dataLen,
                        char **result,
                        size_t *resultLen)
{
   g_debug("Received tools version message: %s\n", data);
   RpcDebug_SetResult("", result, resultLen);
   return TRUE;
}


/**
 * Validates the results for an unknown RPC sent to the guest.
 *
 * @param[in]  data     RPC request data.
 * @param[in]  ret      Return value from RPC handler.
 *
 * @return The opposite of @a ret.
 */

static gboolean
TestDebugValidateUnknown(RpcInData *data,
                         gboolean ret)
{
   CU_ASSERT_STRING_EQUAL(data->result, "Unknown Command");
   return !ret;
}


/**
 * Populates the RPC request data with the next message in the queue.
 *
 * @param[in]  rpcdata     Data for the injected RPC request data.
 *
 * @return TRUE if sending messages, FALSE if no more messages to be sent.
 */

static gboolean
TestDebugSendNext(RpcDebugMsgMapping *rpcdata)
{
   static RpcDebugMsgList msgList = { gRpcMessages, 0 };
   if (!RpcDebug_SendNext(rpcdata, &msgList)) {
      /*
       * Test for bug 391553: send two resets in sequence without
       * pumping the main loop. The channel should handle the second
       * reset successfully instead of asserting.
       */
      int i;
      for (i = 0; i < 2; i++) {
         RpcInData data;
         memset(&data, 0, sizeof data);
         data.clientData = gCtx->rpc;
         data.appCtx = gCtx;
         data.args = "reset";
         data.argsSize = sizeof "reset";

         g_debug("reset test %d\n", i + 1);
         RpcChannel_Dispatch(&data);
      }
      return FALSE;
   }
   return TRUE;
}


/**
 * Returns the debug plugin's registration data.
 *
 * @param[in]  ctx      The application context.
 *
 * @return The application data.
 */

TOOLS_MODULE_EXPORT RpcDebugPlugin *
RpcDebugOnLoad(ToolsAppCtx *ctx)
{
   static RpcDebugRecvMapping recvFns[] = {
      { "tools.set.version", TestDebugReceiveVersion, NULL, 0 },
      { "test.rpcout.msg1", TestDebugReceiveRpc1,
        xdr_TestPluginData, sizeof (TestPluginData) },
      { NULL, NULL }
   };
   static ToolsPluginData pluginData = {
      "testDebug",
      NULL,
      NULL,
      NULL,
   };
   static RpcDebugPlugin regData = {
      recvFns,
      NULL,
      TestDebugSendNext,
      NULL,
      &pluginData,
   };

   /* Standard plugin interface, used to listen for signals. */
   {
      ToolsPluginSignalCb sigs[] = {
         { "test-signal", TestDebugHandleSignal, NULL },
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS, VMTOOLS_WRAP_ARRAY(sigs) },
      };

      pluginData.regs = VMTOOLS_WRAP_ARRAY(regs);
   }

   /* Initialize the paylod of the "test.rpcin.msg1" RPC. */
   {
      TestPluginData testdata;
      testdata.data = "rpc1test";
      testdata.f_int = 1357;
      testdata.f_bool = TRUE;

      /* Build the command for the "test.rpcin.msg1" RPC. */
      if (!RpcChannel_BuildXdrCommand("test.rpcin.msg1",
                                      xdr_TestPluginData,
                                      &testdata,
                                      &gRpcMessages[4].message,
                                      &gRpcMessages[4].messageLen)) {
         g_error("Failed to create test.rpcin.msg1 command.\n");
      }
   }

   gCtx = ctx;
   return &regData;
}

