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
 * @file testDebug.c
 *
 * A simple debug plugin that validates the messages sent by the service after
 * a "reset" is received, and also interacts with the test plugin to test the
 * functions provided by the service.
 */

#define G_LOG_DOMAIN "testDebug"
#include <glib-object.h>
#include "util.h"
#include "vm_app.h"
#include "vmrpcdbg.h"
#include "vmtools.h"
#include "guestrpc/ghiGetBinaryHandlers.h"

static gboolean
TestDebugValidateReset(RpcInData *data, Bool ret);

static gboolean
TestDebugValidateUnknown(RpcInData *data, Bool ret);

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
   { "test.rpcin.msg3", sizeof "test.rpcin.msg3", NULL, FALSE },
   { SET_OPTION_TEST, sizeof SET_OPTION_TEST, NULL, FALSE },
   { "Capabilities_Register", sizeof "Capabilities_Register", NULL, FALSE },
   /* NULL terminator. */
   { NULL, 0, NULL, FALSE }
};

static gboolean gSignalReceived = FALSE;


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
                       Bool ret)
{
   ToolsAppCtx *ctx = data->appCtx;
   g_assert(data->result != NULL);
   if (strcmp(data->result, "ATR debug") != 0) {
      g_error("Unexpected response to reset: %s\n", data->result);
   }

   /*
    * If reset was successful, connect the "test-signal" signal so we
    * test custom registration of signals. The test plugin will emit
    * this signal after it sends an "test.rpcout.msg1" RPC as part of
    * handling a "test.rpcin.msg1" RPC.
    */
   g_signal_connect(ctx->serviceObj,
                    "test-signal",
                    G_CALLBACK(TestDebugHandleSignal),
                    NULL);

   return (gboolean) ret;
}


/**
 * Validates a "test.rpcout.msg1" message sent by the test plugin. This message
 * is sent when the plugin receives a "test.rpcin.msg1" RPC, and contains an
 * XDR-encoded GHIBinaryHandlersIconDetails struct.
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
   GHIBinaryHandlersIconDetails *details = (GHIBinaryHandlersIconDetails *) data;

   g_assert(gSignalReceived);
   g_assert(details->width == 100);
   g_assert(details->height == 200);
   g_assert(strcmp(details->identifier, "rpc1test") == 0);

   g_debug("Successfully validated rpc1!\n");
   return TRUE;
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
   RPCDEBUG_SET_RESULT("", result, resultLen);
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
                         Bool ret)
{
   g_assert(strcmp(data->result, "Unknown Command") == 0);
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
   return RpcDebug_SendNext(rpcdata, &msgList);
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
        xdr_GHIBinaryHandlersIconDetails, sizeof (GHIBinaryHandlersIconDetails) },
      { NULL, NULL }
   };
   static RpcDebugPlugin regData = {
      recvFns,
      NULL,
      TestDebugSendNext,
      NULL
   };

   GHIBinaryHandlersIconDetails details;
   details.width = 100;
   details.height = 200;
   details.identifier = "rpc1test";

   /* Build the command for the "test.rpcin.msg1" RPC. */
   if (!RpcChannel_BuildXdrCommand("test.rpcin.msg1",
                                   xdr_GHIBinaryHandlersIconDetails,
                                   &details,
                                   &gRpcMessages[4].message,
                                   &gRpcMessages[4].messageLen)) {
      g_error("Failed to create test.rpcin.msg1 command.\n");
   }

   return &regData;
}

