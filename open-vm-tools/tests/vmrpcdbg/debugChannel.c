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
 * @file debugChannel.c
 *
 * Implements an RPC Channel that is backed by a "debug plugin". The plugin
 * provides information about what data should be "read" by the RPC Channel,
 * and sinks for the data the application writes to the channel, so that the
 * plugin can perform validation.
 */

#define G_LOG_DOMAIN "rpcdbg"

#include <gmodule.h>

#include "strutil.h"
#include "util.h"
#include "vmrpcdbgInt.h"
#include "rpcChannelInt.h"
#include "vmxrpc.h"
#include "xdrutil.h"
#include "vmware/tools/utils.h"

typedef struct DbgChannelData {
   ToolsAppCtx      *ctx;
   gboolean          hasLibRef;
   RpcDebugPlugin   *plugin;
   GSource          *msgTimer;
} DbgChannelData;

/**
 * Reads one RPC from the plugin and dispatches it to the application.
 * This function will ask the service process to stop running if a
 * failure occurs, where failure is defined either by the validation
 * function returning FALSE, or, if no validation function is provided,
 * the application's callback returning FALSE.
 *
 * @param[in]  _chan    The RPC channel instance.
 *
 * @return TRUE if the callback should continue to be scheduled.
 */

static gboolean
RpcDebugDispatch(gpointer _chan)
{
   gboolean ret;
   RpcChannel *chan = _chan;
   DbgChannelData *cdata = chan->_private;
   RpcDebugPlugin *plugin = cdata->plugin;
   RpcInData data;
   RpcDebugMsgMapping rpcdata;

   memset(&data, 0, sizeof data);
   memset(&rpcdata, 0, sizeof rpcdata);

   if (plugin->sendFn == NULL || !plugin->sendFn(&rpcdata)) {
      RpcDebug_DecRef(cdata->ctx);
      cdata->hasLibRef = FALSE;
      return FALSE;
   } else if (rpcdata.message == NULL) {
      /*
       * Nothing to send. Maybe the debug plugin is waiting for something to
       * happen before sending another message.
       */
      return TRUE;
   }

   data.clientData = chan;
   data.appCtx = cdata->ctx;
   data.args = rpcdata.message;
   data.argsSize = rpcdata.messageLen;

   ret = RpcChannel_Dispatch(&data);
   if (rpcdata.validateFn != NULL) {
      ret = rpcdata.validateFn(&data, ret);
   } else if (!ret) {
      g_debug("RpcChannel_Dispatch returned error for RPC.\n");
   }

   if (data.freeResult) {
      vm_free(data.result);
   }

   if (rpcdata.freeMsg) {
      vm_free(rpcdata.message);
   }

   if (!ret) {
      VMTOOLSAPP_ERROR(cdata->ctx, 1);
      RpcDebug_DecRef(cdata->ctx);
      cdata->hasLibRef = FALSE;
      return FALSE;
   }
   return TRUE;
}


/**
 * Starts sending data to the service. The function will send one RPC
 * approximately every 100ms, to somewhat mimic the behavior of the
 * backdoor-based channel.
 *
 * @param[in]  chan     The RPC channel instance.
 *
 * @return TRUE.
 */

static gboolean
RpcDebugStart(RpcChannel *chan)
{
   DbgChannelData *data = chan->_private;

   ASSERT(data->ctx != NULL);
   ASSERT(data->msgTimer == NULL);

   data->msgTimer = g_timeout_source_new(100);
   VMTOOLSAPP_ATTACH_SOURCE(data->ctx,
                            data->msgTimer,
                            RpcDebugDispatch,
                            chan,
                            NULL);
   return TRUE;
}


/**
 * Stops the channel; cancel the timer that sends polls the debug plugin for
 * messages to send.
 *
 * @param[in]  chan  Unused.
 */

static void
RpcDebugStop(RpcChannel *chan)
{
   DbgChannelData *data = chan->_private;
   if (data->msgTimer != NULL) {
      g_source_unref(data->msgTimer);
      data->msgTimer = NULL;
   }
}


/**
 * Sends the given data to the plugin. This function tries to parse the
 * incoming data based on the "cmd args" convention, and call the "receive"
 * function provided by the debug plugin.
 *
 * @param[in]  chan        The RPC channel instance.
 * @param[in]  data        Data to send.
 * @param[in]  dataLen     Number of bytes to send.
 * @param[out] rpcStatus   Status of RPC command from other side.
 * @param[out] result      Response from other side.
 * @param[out] resultLen   Number of bytes in response.
 *
 * @return The result from the plugin's validation function, or TRUE if
 *         a validation function was not provided.
 */

static gboolean
RpcDebugSend(RpcChannel *chan,
             char const *data,
             size_t dataLen,
             Bool *rpcStatus,
             char **result,
             size_t *resultLen)
{
   char *copy;
   gpointer xdrdata = NULL;
   DbgChannelData *cdata = chan->_private;
   RpcDebugPlugin *plugin = cdata->plugin;
   RpcDebugRecvMapping *mapping = NULL;
   RpcDebugRecvFn recvFn = NULL;
   gboolean ret = TRUE;

   ASSERT(cdata->ctx != NULL);

   /* Be paranoid. Like the VMX, NULL-terminate the incoming data. */
   copy = g_malloc(dataLen + 1);
   memcpy(copy, data, dataLen);
   copy[dataLen] = '\0';

   /* Try to find a mapping the the command in the RPC. */
   if (plugin->recvFns) {
      char *cmd;
      unsigned int idx = 0;

      cmd = StrUtil_GetNextToken(&idx, copy, " ");
      if (cmd != NULL) {
         RpcDebugRecvMapping *test;
         for (test = plugin->recvFns; test->name != NULL; test++) {
            if (strcmp(test->name, cmd) == 0) {
               mapping = test;
               break;
            }
         }
      }
      vm_free(cmd);
   }

   if (mapping != NULL) {
      recvFn = mapping->recvFn;
      if (mapping->xdrProc != NULL) {
         char *start;

         ASSERT(mapping->xdrSize > 0);

         /* Find out where the XDR data starts. */
         start = strchr(copy, ' ');
         if (start == NULL) {
            RpcDebug_SetResult("Can't find command delimiter.", result, resultLen);
            ret = FALSE;
            goto exit;
         }
         start++;

         xdrdata = g_malloc0(mapping->xdrSize);
         if (!XdrUtil_Deserialize(start,
                                  dataLen - (start - copy),
                                  mapping->xdrProc,
                                  xdrdata)) {
            RpcDebug_SetResult("XDR deserialization failed.", result, resultLen);
            ret = FALSE;
            goto exit;
         }
      }
   } else {
      recvFn = plugin->dfltRecvFn;
   }

   if (recvFn != NULL) {
      ret = recvFn((xdrdata != NULL) ? xdrdata : copy, dataLen, result, resultLen);
   } else {
      RpcDebug_SetResult("", result, resultLen);
   }

exit:
   if (xdrdata != NULL) {
      VMX_XDR_FREE(mapping->xdrProc, xdrdata);
      g_free(xdrdata);
   }
   g_free(copy);
   /* For now, just make rpcStatus same as ret */
   *rpcStatus = ret;
   return ret;
}


/**
 * Intiializes internal state for the inbound channel.
 *
 * @param[in]  chan     The RPC channel instance.
 * @param[in]  ctx      Unused.
 * @param[in]  appName  Unused.
 * @param[in]  appCtx   A ToolsAppCtx instance.
 */

static void
RpcDebugSetup(RpcChannel *chan,
              GMainContext *ctx,
              const char *appName,
              gpointer appCtx)
{
   DbgChannelData *cdata = chan->_private;
   cdata->ctx = appCtx;
}


/**
 * Cleans up the internal channel state.
 *
 * @param[in]  chan     The RPC channel instance.
 */

static void
RpcDebugShutdown(RpcChannel *chan)
{
   DbgChannelData *cdata = chan->_private;
   ASSERT(cdata->ctx != NULL);
   if (cdata->hasLibRef) {
      RpcDebug_DecRef(cdata->ctx);
   }
   g_free(chan->_private);
}


/**
 * Instantiates a new RPC Debug Channel. This function will load and initialize
 * the given debug plugin.
 *
 * This function will panic is something wrong happens while loading the plugin.
 *
 * @param[in]  ctx         The application context.
 * @param[in]  data        Debug library data.
 *
 * @return A new channel.
 */

RpcChannel *
RpcDebug_NewDebugChannel(ToolsAppCtx *ctx,
                         RpcDebugLibData *data)
{
   DbgChannelData *cdata;
   RpcChannel *ret;
   static RpcChannelFuncs funcs = {
      RpcDebugStart,
      RpcDebugStop,
      RpcDebugSend,
      RpcDebugSetup,
      RpcDebugShutdown,
      NULL,
      NULL,
      NULL
   };

   ASSERT(data != NULL);
   ret = RpcChannel_Create();
   ret->funcs = &funcs;

   cdata = g_malloc0(sizeof *cdata);
   cdata->plugin = data->debugPlugin;
   ret->_private = cdata;

   RpcDebug_IncRef();
   return ret;
}

