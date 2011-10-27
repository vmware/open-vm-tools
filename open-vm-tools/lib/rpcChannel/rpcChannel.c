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
 * @file rpcChannel.c
 *
 *    Common functions to all RPC channel implementations.
 */

#include <string.h>
#include "vm_assert.h"
#include "dynxdr.h"
#include "rpcChannelInt.h"
#include "str.h"
#include "strutil.h"
#include "vmxrpc.h"
#include "xdrutil.h"

/** Internal state of a channel. */
typedef struct RpcChannelInt {
   RpcChannel              impl;
   gchar                  *appName;
   GHashTable             *rpcs;
   GMainContext           *mainCtx;
   GSource                *resetCheck;
   gpointer                appCtx;
   RpcChannelCallback      resetReg;
   RpcChannelResetCb       resetCb;
   gpointer                resetData;
   gboolean                rpcError;
   guint                   rpcErrorCount;
} RpcChannelInt;


/** Max number of times to attempt a channel restart. */
#define RPCIN_MAX_RESTARTS 60

static gboolean
RpcChannelPing(RpcInData *data);

static RpcChannelCallback gRpcHandlers[] =  {
   { "ping", RpcChannelPing, NULL, NULL, NULL, 0 }
};


/**
 * Handler for a "ping" message. Does nothing.
 *
 * @param[in]  data     The RPC data.
 *
 * @return TRUE.
 */

static gboolean
RpcChannelPing(RpcInData *data)
{
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Callback for restarting the RPC channel.
 *
 * @param[in]  _chan    The RPC channel
 *
 * @return FALSE
 */

static gboolean
RpcChannelRestart(gpointer _chan)
{
   RpcChannelInt *chan = _chan;

   RpcChannel_Stop(&chan->impl);
   if (!RpcChannel_Start(&chan->impl)) {
      g_warning("Channel restart failed [%d]\n", chan->rpcErrorCount);
      if (chan->resetCb != NULL) {
         chan->resetCb(&chan->impl, FALSE, chan->resetData);
      }
   } else {
      chan->rpcError = FALSE;
   }

   return FALSE;
}


/**
 * Checks and potentially resets the RPC channel. This code is based on the
 * toolsDaemon.c function "ToolsDaemon_CheckReset".
 *
 * @param[in]  _chan    The RPC channel.
 *
 * @return FALSE. The reset callback will schedule a new check when it's called.
 */

static gboolean
RpcChannelCheckReset(gpointer _chan)
{
   static int channelTimeoutAttempts = RPCIN_MAX_RESTARTS;
   RpcChannelInt *chan = _chan;

   /* Check the channel state. */
   if (chan->rpcError) {
      GSource *src;

      if (++(chan->rpcErrorCount) > channelTimeoutAttempts) {
         g_warning("Failed to reset channel after %u attempts\n",
                   chan->rpcErrorCount - 1);
         if (chan->resetCb != NULL) {
            chan->resetCb(&chan->impl, FALSE, chan->resetData);
         }
         goto exit;
      }

      /* Schedule the channel restart for 1 sec in the future. */
      g_debug("Resetting channel [%u]\n", chan->rpcErrorCount);
      src = g_timeout_source_new(1000);
      g_source_set_callback(src, RpcChannelRestart, chan, NULL);
      g_source_attach(src, chan->mainCtx);
      g_source_unref(src);
      goto exit;
   }

   /* Reset was successful. */
   g_debug("Channel was reset successfully.\n");
   chan->rpcErrorCount = 0;

   if (chan->resetCb != NULL) {
      chan->resetCb(&chan->impl, TRUE, chan->resetData);
   }

exit:
   g_source_unref(chan->resetCheck);
   chan->resetCheck = NULL;
   return FALSE;
}


/**
 * Handles an RPC reset. Calls the reset callback of all loaded plugins.
 *
 * @param[in]  data     The RPC data.
 *
 * @return TRUE.
 */

static gboolean
RpcChannelReset(RpcInData *data)
{
   gchar *msg;
   RpcChannelInt *chan = data->clientData;

   if (chan->resetCheck == NULL) {
      chan->resetCheck = g_idle_source_new();
      g_source_set_priority(chan->resetCheck, G_PRIORITY_HIGH);
      g_source_set_callback(chan->resetCheck, RpcChannelCheckReset, chan, NULL);
      g_source_attach(chan->resetCheck, chan->mainCtx);
   }

   msg = Str_Asprintf(NULL, "ATR %s", chan->appName);
   ASSERT_MEM_ALLOC(msg);
   return RPCIN_SETRETVALSF(data, msg, TRUE);
}


/**
 * A wrapper for standard RPC callback functions which provides automatic
 * XDR serialization / deserialization if requested by the application.
 *
 * @param[in]  data     RpcIn data.
 * @param[in]  rpc      The RPC registration data.
 *
 * @return Whether the RPC was handled successfully.
 */

static Bool
RpcChannelXdrWrapper(RpcInData *data,
                     RpcChannelCallback *rpc)
{
   Bool ret;
   RpcInData copy;
   void *xdrData = NULL;

   if (rpc->xdrIn != NULL) {
      xdrData = malloc(rpc->xdrInSize);
      if (xdrData == NULL) {
         ret = RPCIN_SETRETVALS(data, "Out of memory.", FALSE);
         goto exit;
      }

      memset(xdrData, 0, rpc->xdrInSize);
      if (!XdrUtil_Deserialize(data->args + 1, data->argsSize - 1,
                               rpc->xdrIn, xdrData)) {
         ret = RPCIN_SETRETVALS(data, "XDR deserialization failed.", FALSE);
         free(xdrData);
         goto exit;
      }

      copy.name = data->name;
      copy.args = xdrData;
      copy.argsSize = rpc->xdrInSize;
      copy.result = data->result;
      copy.resultLen = data->resultLen;
      copy.freeResult = data->freeResult;
      copy.appCtx = data->appCtx;
      copy.clientData = rpc->clientData;
   } else {
      memcpy(&copy, data, sizeof copy);
   }

   ret = rpc->callback(&copy);

   if (rpc->xdrIn != NULL) {
      VMX_XDR_FREE(rpc->xdrIn, xdrData);
      free(xdrData);
      copy.args = NULL;
      data->result = copy.result;
      data->resultLen = copy.resultLen;
      data->freeResult = copy.freeResult;
   }

   if (rpc->xdrOut != NULL && copy.result != NULL) {
      XDR xdrs;
      xdrproc_t xdrProc = rpc->xdrOut;

      if (DynXdr_Create(&xdrs) == NULL) {
         ret = RPCIN_SETRETVALS(data, "Out of memory.", FALSE);
         goto exit;
      }

      if (!xdrProc(&xdrs, copy.result)) {
         ret = RPCIN_SETRETVALS(data, "XDR serialization failed.", FALSE);
         DynXdr_Destroy(&xdrs, TRUE);
         goto exit;
      }

      if (copy.freeResult) {
         VMX_XDR_FREE(rpc->xdrOut, copy.result);
      }
      data->result = DynXdr_Get(&xdrs);
      data->resultLen = XDR_GETPOS(&xdrs);
      data->freeResult = TRUE;
      DynXdr_Destroy(&xdrs, FALSE);
   }

exit:
   if (copy.freeResult && copy.result != NULL) {
      g_free(copy.result);
   }
   return ret;
}


/**
 * Builds an "rpcout" command to send a XDR struct.
 *
 * @param[in]  cmd         The command name.
 * @param[in]  xdrProc     Function to use for serializing the XDR struct.
 * @param[in]  xdrData     The XDR struct to serialize.
 * @param[out] result      Where to store the serialized data.
 * @param[out] resultLen   Where to store the serialized data length.
 *
 * @return Whether successfully built the command.
 */

gboolean
RpcChannel_BuildXdrCommand(const char *cmd,
                           void *xdrProc,
                           void *xdrData,
                           char **result,
                           size_t *resultLen)
{
   Bool ret = FALSE;
   xdrproc_t proc = xdrProc;
   XDR xdrs;

   if (DynXdr_Create(&xdrs) == NULL) {
      return FALSE;
   }

   if (!DynXdr_AppendRaw(&xdrs, cmd, strlen(cmd))) {
      goto exit;
   }

   if (!DynXdr_AppendRaw(&xdrs, " ", 1)) {
      goto exit;
   }

   if (!proc(&xdrs, xdrData)) {
      goto exit;
   }

   *result = DynXdr_Get(&xdrs);
   *resultLen = xdr_getpos(&xdrs);

   ret = TRUE;

exit:
   DynXdr_Destroy(&xdrs, !ret);
   return ret;
}


/**
 * Creates a new RpcChannel without any implementation.
 *
 * This is mainly for use of code that is implementing a custom RpcChannel.
 * Such implementations should provide their own "constructor"-type function
 * which should then call this function to get an RpcChannel instance. They
 * should then fill in the function pointers that provide the implementation
 * for the channel before making the channel available to the callers.
 *
 * @return A new RpcChannel instance.
 */

RpcChannel *
RpcChannel_Create(void)
{
   RpcChannelInt *chan = g_new0(RpcChannelInt, 1);
   return &chan->impl;
}


/**
 * Dispatches the given RPC to the registered handler. This mimics the behavior
 * of the RpcIn library (but is not tied to that particular implementation of
 * an RPC channel).
 *
 * @param[in,out]    data     The RPC data.
 *
 * @return Whether the RPC was handled successfully.
 */

gboolean
RpcChannel_Dispatch(RpcInData *data)
{
   char *name = NULL;
   unsigned int index = 0;
   size_t nameLen;
   Bool status;
   RpcChannelCallback *rpc = NULL;
   RpcChannelInt *chan = data->clientData;

   name = StrUtil_GetNextToken(&index, data->args, " ");
   if (name == NULL) {
      status = RPCIN_SETRETVALS(data, "Bad command", FALSE);
      goto exit;
   }

   if (chan->rpcs != NULL) {
      rpc = g_hash_table_lookup(chan->rpcs, name);
   }

   if (rpc == NULL) {
      status = RPCIN_SETRETVALS(data, "Unknown Command", FALSE);
      goto exit;
   }

   /* Adjust the RPC arguments. */
   nameLen = strlen(name);
   data->name = name;
   data->args = data->args + nameLen;
   data->argsSize -= nameLen;
   data->appCtx = chan->appCtx;
   data->clientData = rpc->clientData;

   if (rpc->xdrIn != NULL || rpc->xdrOut != NULL) {
      status = RpcChannelXdrWrapper(data, rpc);
   } else {
      status = rpc->callback(data);
   }

   ASSERT(data->result != NULL);

exit:
   data->name = NULL;
   free(name);
   return status;
}


/**
 * Shuts down an RPC channel and release any held resources.
 *
 * @param[in]  chan     The RPC channel.
 *
 * @return  Whether the channel was shut down successfully.
 */

gboolean
RpcChannel_Destroy(RpcChannel *chan)
{
   size_t i;
   RpcChannelInt *cdata = (RpcChannelInt *) chan;

   if (cdata->impl.shutdown != NULL) {
      cdata->impl.shutdown(chan);
   }

   RpcChannel_UnregisterCallback(chan, &cdata->resetReg);
   for (i = 0; i < ARRAYSIZE(gRpcHandlers); i++) {
      RpcChannel_UnregisterCallback(chan, &gRpcHandlers[i]);
   }

   if (cdata->rpcs != NULL) {
      g_hash_table_destroy(cdata->rpcs);
      cdata->rpcs = NULL;
   }

   cdata->resetCb = NULL;
   cdata->resetData = NULL;
   cdata->appCtx = NULL;

   g_free(cdata->appName);
   cdata->appName = NULL;

   if (cdata->mainCtx != NULL) {
      g_main_context_unref(cdata->mainCtx);
      cdata->mainCtx = NULL;
   }

   if (cdata->resetCheck != NULL) {
      g_source_destroy(cdata->resetCheck);
      cdata->resetCheck = NULL;
   }

   g_free(cdata);
   return TRUE;
}


/**
 * Error handling function for the RPC channel. Enqueues the "check reset"
 * function for running later, if it's not yet enqueued.
 *
 * @param[in]  _chan       The RPC channel.
 * @param[in]  status      Error description.
 */

void
RpcChannel_Error(void *_chan,
                 char const *status)
{
   RpcChannelInt *chan = _chan;
   chan->rpcError = TRUE;
   g_warning("Error in the RPC receive loop: %s.\n", status);

   if (chan->resetCheck == NULL) {
      chan->resetCheck = g_idle_source_new();
      g_source_set_callback(chan->resetCheck, RpcChannelCheckReset, chan, NULL);
      g_source_attach(chan->resetCheck, chan->mainCtx);
   }
}


/**
 * Initializes the RPC channel for inbound operations.
 *
 * This function must be called before starting the channel if the application
 * wants to receive messages on the channel. Applications don't need to call it
 * if only using the outbound functionality.
 *
 * @param[in]  chan        The RPC channel.
 * @param[in]  appName     TCLO application name.
 * @param[in]  mainCtx     Application event context.
 * @param[in]  appCtx      Application context.
 * @param[in]  resetCb     Callback for when a reset occurs.
 * @param[in]  resetData   Client data for the reset callback.
 */

void
RpcChannel_Setup(RpcChannel *chan,
                 const gchar *appName,
                 GMainContext *mainCtx,
                 gpointer appCtx,
                 RpcChannelResetCb resetCb,
                 gpointer resetData)
{
   size_t i;
   RpcChannelInt *cdata = (RpcChannelInt *) chan;

   cdata->appName = g_strdup(appName);
   cdata->appCtx = appCtx;
   cdata->mainCtx = g_main_context_ref(mainCtx);
   cdata->resetCb = resetCb;
   cdata->resetData = resetData;

   cdata->resetReg.name = "reset";
   cdata->resetReg.callback = RpcChannelReset;
   cdata->resetReg.clientData = chan;

   /* Register the callbacks handled by the rpcChannel library. */
   RpcChannel_RegisterCallback(chan, &cdata->resetReg);

   for (i = 0; i < ARRAYSIZE(gRpcHandlers); i++) {
      RpcChannel_RegisterCallback(chan, &gRpcHandlers[i]);
   }

   if (cdata->impl.setup != NULL) {
      cdata->impl.setup(&cdata->impl, mainCtx, appName, appCtx);
   }
}


/**
 * Sets the non-freeable result of the given RPC context to the given value.
 * The result should be a NULL-terminated string.
 *
 * @param[in] data     RPC context.
 * @param[in] result   Result string.
 * @param[in] retVal   Return value of this function.
 *
 * @return @a retVal
 */

gboolean
RpcChannel_SetRetVals(RpcInData *data,
                      char const *result,
                      gboolean retVal)
{
   ASSERT(data);

   /* This cast is safe: data->result will not be freed. */
   data->result = (char *)result;
   data->resultLen = strlen(data->result);
   data->freeResult = FALSE;

   return retVal;
}


/**
 * Sets the freeable result of the given RPC context to the given value.
 * The result should be a NULL-terminated string.
 *
 * @param[in] data     RPC context.
 * @param[in] result   Result string.
 * @param[in] retVal   Return value of this function.
 *
 * @return @a retVal
 */

gboolean
RpcChannel_SetRetValsF(RpcInData *data,
                       char *result,
                       gboolean retVal)
{
   ASSERT(data);

   data->result = result;
   data->resultLen = strlen(data->result);
   data->freeResult = TRUE;

   return retVal;
}


/**
 * Registers a new RPC handler in the given RPC channel. This function is
 * not thread-safe.
 *
 * @param[in]  chan     The channel instance.
 * @param[in]  rpc      Info about the RPC being registered.
 */

void
RpcChannel_RegisterCallback(RpcChannel *chan,
                            RpcChannelCallback *rpc)
{
   RpcChannelInt *cdata = (RpcChannelInt *) chan;
   ASSERT(rpc->name != NULL && strlen(rpc->name) > 0);
   ASSERT(rpc->callback);
   ASSERT(rpc->xdrIn == NULL || rpc->xdrInSize > 0);
   if (cdata->rpcs == NULL) {
      cdata->rpcs = g_hash_table_new(g_str_hash, g_str_equal);
   }
   if (g_hash_table_lookup(cdata->rpcs, rpc->name) != NULL) {
      g_error("Trying to overwrite existing RPC registration for %s!\n", rpc->name);
   }
   g_hash_table_insert(cdata->rpcs, (gpointer) rpc->name, rpc);
}


/**
 * Unregisters a new RPC handler from the given RPC channel. This function is
 * not thread-safe.
 *
 * @param[in]  chan     The channel instance.
 * @param[in]  rpc      Info about the RPC being unregistered.
 */

void
RpcChannel_UnregisterCallback(RpcChannel *chan,
                              RpcChannelCallback *rpc)
{
   RpcChannelInt *cdata = (RpcChannelInt *) chan;
   if (cdata->rpcs != NULL) {
      g_hash_table_remove(cdata->rpcs, rpc->name);
   }
}

