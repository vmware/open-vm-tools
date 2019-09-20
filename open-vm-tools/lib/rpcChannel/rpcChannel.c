/*********************************************************
 * Copyright (C) 2008-2016,2018-2019 VMware, Inc. All rights reserved.
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

#include <stdlib.h>
#include <string.h>
#include "debug.h"
#include "rpcChannelInt.h"

#if defined(NEED_RPCIN)
#include "dynxdr.h"
#include "vmxrpc.h"
#include "xdrutil.h"
#include "rpcin.h"
#endif

#include "str.h"
#include "strutil.h"
#include "util.h"
#include "vm_assert.h"

/** Internal state of a channel. */
typedef struct RpcChannelInt {
   RpcChannel              impl;
#if defined(NEED_RPCIN)
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
   guint                   rpcResetErrorCount;  /* channel reset failures */
   /*
    * The rpcFailureCount is a cumulative count of calls made to
    * RpcChannelError().  When getting repeated channel failures,
    * the channel is constantly stopped and restarted.
    */
   guint                   rpcFailureCount;  /* cumulative channel failures */
   RpcChannelFailureCb     rpcFailureCb;
   guint                   rpcMaxFailures;
   gboolean                rpcInInitialized;
   GSource                *restartTimer; /* Channel restart timer */
#endif
} RpcChannelInt;

#define LGPFX "RpcChannel: "

static gboolean gUseBackdoorOnly = FALSE;

/*
 * Track the vSocket connection failure, so that we can
 * avoid using vSockets until a channel reset/restart or
 * the service itself gets restarted.
 */
static gboolean gVSocketFailed = FALSE;

static void RpcChannelStopNoLock(RpcChannel *chan);


#if defined(NEED_RPCIN)
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
   gboolean chanStarted;

   /* Synchronize with any RpcChannel_Send calls by other threads. */
   g_mutex_lock(&chan->impl.outLock);
   g_source_unref(chan->restartTimer);
   chan->restartTimer = NULL;

   RpcChannelStopNoLock(&chan->impl);

   /* Clear vSocket channel failure */
   Log(LGPFX "Clearing backdoor behavior ...\n");
   gVSocketFailed = FALSE;

   chanStarted = RpcChannel_Start(&chan->impl);
   g_mutex_unlock(&chan->impl.outLock);
   if (!chanStarted) {
      Warning("Channel restart failed [%d]\n", chan->rpcResetErrorCount);
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

      if (++(chan->rpcResetErrorCount) > channelTimeoutAttempts) {
         Warning("Failed to reset channel after %u attempts\n",
                 chan->rpcResetErrorCount - 1);
         if (chan->resetCb != NULL) {
            chan->resetCb(&chan->impl, FALSE, chan->resetData);
         }
         goto exit;
      }

      /* Schedule the channel restart for 1 sec in the future. */
      Debug(LGPFX "Resetting channel [%u]\n", chan->rpcResetErrorCount);
      ASSERT(chan->restartTimer == NULL);
      chan->restartTimer = g_timeout_source_new(1000);
      g_source_set_callback(chan->restartTimer, RpcChannelRestart, chan, NULL);
      g_source_attach(chan->restartTimer, chan->mainCtx);
      goto exit;
   }

   /* Reset was successful. */
   Log(LGPFX "Channel was reset successfully.\n");
   chan->rpcResetErrorCount = 0;
   Log(LGPFX "Clearing backdoor behavior ...\n");
   gVSocketFailed = FALSE;

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

   copy.freeResult = FALSE;
   copy.result = NULL;
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

      if (!xdrProc(&xdrs, copy.result, 0)) {
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

   if (!proc(&xdrs, xdrData, 0)) {
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
      Debug(LGPFX "Bad command (null) received.\n");
      status = RPCIN_SETRETVALS(data, "Bad command", FALSE);
      goto exit;
   }

   if (chan->rpcs != NULL) {
      rpc = g_hash_table_lookup(chan->rpcs, name);
   }

   if (rpc == NULL) {
      Debug(LGPFX "Unknown Command '%s': Handler not registered.\n", name);
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
 * @param[in]  failureCB   Callback for when the channel failure limit is hit.
 * @param[in]  maxFailures Maximum channel failures allowed.
 */

void
RpcChannel_Setup(RpcChannel *chan,
                 const gchar *appName,
                 GMainContext *mainCtx,
                 gpointer appCtx,
                 RpcChannelResetCb resetCb,
                 gpointer resetData,
                 RpcChannelFailureCb failureCb,
                 guint maxFailures)
{
   size_t i;
   RpcChannelInt *cdata = (RpcChannelInt *) chan;

   ASSERT(!cdata->rpcInInitialized);

   cdata->appName = g_strdup(appName);
   cdata->appCtx = appCtx;
   cdata->mainCtx = g_main_context_ref(mainCtx);
   cdata->resetCb = resetCb;
   cdata->resetData = resetData;
   cdata->rpcFailureCb = failureCb;
   cdata->rpcMaxFailures = maxFailures;

   cdata->resetReg.name = "reset";
   cdata->resetReg.callback = RpcChannelReset;
   cdata->resetReg.clientData = chan;

   /* Register the callbacks handled by the rpcChannel library. */
   RpcChannel_RegisterCallback(chan, &cdata->resetReg);

   for (i = 0; i < ARRAYSIZE(gRpcHandlers); i++) {
      RpcChannel_RegisterCallback(chan, &gRpcHandlers[i]);
   }

   if (chan->funcs != NULL && chan->funcs->setup != NULL) {
      chan->funcs->setup(chan, mainCtx, appName, appCtx);
   } else {
      chan->mainCtx = g_main_context_ref(mainCtx);
      chan->in = RpcIn_Construct(mainCtx, RpcChannel_Dispatch, chan);
      ASSERT(chan->in != NULL);
   }

   cdata->rpcInInitialized = TRUE;
}


/**
 * Undo the RpcChannel_Setup() function if it was called earlier
 *
 * Note the outLock is initialized in the RpcChannel_New() function,
 * and should only be freed in the corresponding RpcChannel_Destroy() function.
 *
 * @param[in]  chan        The RPC channel instance.
 */

static void
RpcChannelTeardown(RpcChannel *chan)
{
   size_t i;
   RpcChannelInt *cdata = (RpcChannelInt *) chan;

   if (NULL == cdata || !cdata->rpcInInitialized) {
      return;
   }

   /*
    * Stop the restartTimer.
    */
   if (cdata->restartTimer) {
      g_source_destroy(cdata->restartTimer);
      g_source_unref(cdata->restartTimer);
      cdata->restartTimer = NULL;
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
   cdata->rpcFailureCb = NULL;

   g_free(cdata->appName);
   cdata->appName = NULL;

   if (chan->mainCtx != NULL) {
      g_main_context_unref(chan->mainCtx);
      chan->mainCtx = NULL;
   }

   if (cdata->mainCtx != NULL) {
      g_main_context_unref(cdata->mainCtx);
      cdata->mainCtx = NULL;
   }

   if (cdata->resetCheck != NULL) {
      g_source_destroy(cdata->resetCheck);
      cdata->resetCheck = NULL;
   }

   cdata->rpcInInitialized = FALSE;
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
      Panic("Trying to overwrite existing RPC registration for %s!\n", rpc->name);
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


/**
 * Callback function to clear the cumulative channel error count when RpcIn
 * is able to establish a working connection following an error or reset.
 *
 * @param[in]  _chan       The RPC channel.
 */

static void
RpcChannelClearError(void *_chan)
{
   RpcChannelInt *chan = _chan;

   Debug(LGPFX " %s: Clearing cumulative RpcChannel error count; was %d\n",
         __FUNCTION__, chan->rpcFailureCount);
   chan->rpcFailureCount = 0;
}


/**
 * Error handling function for the RPC channel. Enqueues the "check reset"
 * function for running later, if it's not yet enqueued.
 *
 * @param[in]  _chan       The RPC channel.
 * @param[in]  status      Error description.
 */

static void
RpcChannelError(void *_chan,
                char const *status)
{
   RpcChannelInt *chan = _chan;

   chan->rpcError = TRUE;

   /*
    * XXX: Workaround for PR 935520.
    * Revert the log call to Warning() after fixing PR 955746.
    */
   Debug(LGPFX "Error in the RPC receive loop: %s.\n", status);

   /*
    * If an RPC failure callback has been registered and the failure limit
    * check has not been suppressed, check whether the RpcChannel failure
    * limit has been reached.
    */
   if (chan->rpcFailureCb != NULL &&
       chan->rpcMaxFailures > 0 &&
       ++chan->rpcFailureCount >= chan->rpcMaxFailures) {
      /* Maximum number of channel errors has been reached. */
      Warning(LGPFX "RpcChannel failure count %d; calling the failure "
                    "callback function.\n", chan->rpcFailureCount);
      chan->rpcFailureCb(chan->resetData);
   }

   if (chan->resetCheck == NULL) {
      chan->resetCheck = g_idle_source_new();
      g_source_set_callback(chan->resetCheck, RpcChannelCheckReset, chan, NULL);
      g_source_attach(chan->resetCheck, chan->mainCtx);
   }
}
#endif


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
 * Shuts down an RPC channel and release any held resources.
 *
 * We use the outLock to protect the chan->inStarted and chan->in as well,
 * even though a separate lock is more desirable in theory to reduce contention.
 * See RpcChannel_Stop() and RpcChannelStopNoLock() where it is done same.
 *
 * @param[in]  chan     The RPC channel.
 *
 */

void
RpcChannel_Destroy(RpcChannel *chan)
{
   if (NULL == chan) {
      return;
   }

   g_mutex_lock(&chan->outLock);

   RpcChannelStopNoLock(chan);

   if (chan->funcs != NULL && chan->funcs->shutdown != NULL) {
      /* backend shutdown function also frees backend resources */
      chan->funcs->shutdown(chan);
   }

#if defined(NEED_RPCIN)
   RpcChannelTeardown(chan);
#endif

   g_mutex_unlock(&chan->outLock);

   g_mutex_clear(&chan->outLock);

   g_free(chan);
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
 * Force to create backdoor channels only.
 * This provides a kill-switch to disable vsocket channels if needed.
 * This needs to be called before RpcChannel_New to take effect.
 */

void
RpcChannel_SetBackdoorOnly(void)
{
   gUseBackdoorOnly = TRUE;
   Debug(LGPFX "Using vsocket is disabled.\n");
}


/**
 * Create an RpcChannel instance using a prefered channel implementation,
 * currently this is VSockChannel.
 *
 * @return  RpcChannel
 */

RpcChannel *
RpcChannel_New(void)
{
   RpcChannel *chan;
#if (defined(__linux__) && !defined(USERWORLD)) || defined(_WIN32)
   chan = (gUseBackdoorOnly || gVSocketFailed) ?
          BackdoorChannel_New() : VSockChannel_New();
#else
   chan = BackdoorChannel_New();
#endif
   return chan;
}


/**
 * Start an RPC channel. We may fallback to backdoor channel when other type
 * of channel fails to start.
 *
 * @param[in]  chan        The RPC channel instance.
 *
 * @return TRUE on success.
 */

gboolean
RpcChannel_Start(RpcChannel *chan)
{
   gboolean ok;
   const RpcChannelFuncs *funcs;

   if (chan == NULL || chan->funcs == NULL || chan->funcs->start == NULL) {
      return FALSE;
   }

   if (chan->outStarted) {
#if defined(NEED_RPCIN)
      /* Already started. Make sure both channels are in sync and return. */
      ASSERT(chan->in == NULL || chan->inStarted);
#endif
      return TRUE;
   }

#if defined(NEED_RPCIN)
   if (chan->in != NULL && !chan->inStarted) {
      ok = RpcIn_start(chan->in, RPCIN_MAX_DELAY, RpcChannelError,
                       RpcChannelClearError, chan);
      chan->inStarted = ok;
   }
#endif

   funcs = chan->funcs;
   ok = funcs->start(chan);

   if (!ok && funcs->onStartErr != NULL) {
      Log(LGPFX "Fallback to backdoor ...\n");
      funcs->onStartErr(chan);
      ok = BackdoorChannel_Fallback(chan);
      /*
       * As vSocket is not available, we stick the backdoor
       * behavior until the channel is reset/restarted.
       */
      Log(LGPFX "Sticking backdoor behavior ...\n");
      gVSocketFailed = TRUE;
   }

   return ok;
}


/**
 * Stop the RPC channel.
 * The outLock must be acquired by the caller.
 *
 * @param[in]  chan        The RPC channel instance.
 */

static void
RpcChannelStopNoLock(RpcChannel *chan)
{
   if (chan == NULL || chan->funcs == NULL || chan->funcs->stop == NULL) {
      return;
   }

   chan->funcs->stop(chan);

#if defined(NEED_RPCIN)
   if (chan->in != NULL) {
      if (chan->inStarted) {
         RpcIn_stop(chan->in);
         chan->inStarted = FALSE;
      }
   } else {
      ASSERT(!chan->inStarted);
   }
#endif
}


/**
 * Wrapper for the stop function of an RPC channel struct.
 *
 * @param[in]  chan        The RPC channel instance.
 */

void
RpcChannel_Stop(RpcChannel *chan)
{
   g_mutex_lock(&chan->outLock);
   RpcChannelStopNoLock(chan);
   g_mutex_unlock(&chan->outLock);
}


/**
 * Wrapper for get channel type function of an RPC channel struct.
 *
 * @param[in]  chan        The RPC channel instance.
 */

RpcChannelType
RpcChannel_GetType(RpcChannel *chan)
{
   if (chan == NULL || chan->funcs == NULL || chan->funcs->getType == NULL) {
      return RPCCHANNEL_TYPE_INACTIVE;
   }
   return chan->funcs->getType(chan);
}


/**
 * Free the allocated memory for the results from RpcChannel_Send* calls.
 *
 * @param[in] ptr   result from RpcChannel_Send* calls.
 *
 * @return none
 */

void
RpcChannel_Free(void *ptr)
{
   free(ptr);
}


/**
 * Send function of an RPC channel struct. Retry once if it fails for
 * non-backdoor Channels. Backdoor channel already tries inside. A second try
 * may create a different type of channel.
 *
 * @param[in]  chan        The RPC channel instance.
 * @param[in]  data        Data to send.
 * @param[in]  dataLen     Number of bytes to send.
 * @param[out] result      Response from other side (should be freed by
 *                         calling RpcChannel_Free).
 * @param[out] resultLen   Number of bytes in response.
 *
 * @return The status from the remote end (TRUE if call was successful).
 */

gboolean
RpcChannel_Send(RpcChannel *chan,
                char const *data,
                size_t dataLen,
                char **result,
                size_t *resultLen)
{
   gboolean ok;
   Bool rpcStatus;
   char *res = NULL;
   size_t resLen = 0;
   const RpcChannelFuncs *funcs;

   Debug(LGPFX "Sending: %"FMTSZ"u bytes\n", dataLen);

   ASSERT(chan && chan->funcs);

   g_mutex_lock(&chan->outLock);

   funcs = chan->funcs;
   ASSERT(funcs->send);

   if (result != NULL) {
      *result = NULL;
   }
   if (resultLen != NULL) {
      *resultLen = 0;
   }

   ok = funcs->send(chan, data, dataLen, &rpcStatus, &res, &resLen);

   if (!ok && (funcs->getType(chan) != RPCCHANNEL_TYPE_BKDOOR) &&
       (funcs->stopRpcOut != NULL)) {

      free(res);
      res = NULL;
      resLen = 0;

      /* retry once */
      Log(LGPFX "Stop RpcOut channel and try to send again ...\n");
      funcs->stopRpcOut(chan);
      if (RpcChannel_Start(chan)) {
         /* The channel may get switched from vsocket to backdoor */
         funcs = chan->funcs;
         ASSERT(funcs->send);
         ok = funcs->send(chan, data, dataLen, &rpcStatus, &res, &resLen);
         goto done;
      }

      ok = FALSE;
      goto exit;
   }

done:
   if (ok) {
      Debug(LGPFX "Recved %"FMTSZ"u bytes\n", resLen);
   }

   if (result != NULL) {
      *result = res;
   } else {
      free(res);
   }
   if (resultLen != NULL) {
      *resultLen = resLen;
   }

exit:
   g_mutex_unlock(&chan->outLock);
   return ok && rpcStatus;
}


/**
 * Open/close RpcChannel each time for sending a Rpc message, this is a wrapper
 * for RpcChannel APIs.
 *
 * @param[in]  data        request data
 * @param[in]  dataLen     data length
 * @param[in]  result      reply, should be freed by calling RpcChannel_Free.
 * @param[in]  resultLen   reply length
 * @param[in]  priv        TRUE : create VSock channel for privileged guest RPC.
                           FALSE: follow regular RPC channel creation process.

 * @returns    TRUE on success.
 */

static gboolean
RpcChannelSendOneRaw(const char *data,
                     size_t dataLen,
                     char **result,
                     size_t *resultLen,
                     gboolean priv)
{
   RpcChannel *chan;
   gboolean status = FALSE;

#if (defined(__linux__) && !defined(USERWORLD)) || defined(_WIN32)
   chan = priv ? VSockChannel_New() : RpcChannel_New();
#else
   chan = RpcChannel_New();
#endif

   if (chan == NULL) {
      if (result != NULL) {
         *result = Util_SafeStrdup("RpcChannel: Unable to create "
                                   "the RpcChannel object");
         if (resultLen != NULL) {
            *resultLen = strlen(*result);
         }
      }
      goto sent;
   } else if (!RpcChannel_Start(chan)) {
      if (result != NULL) {
         *result = Util_SafeStrdup("RpcChannel: Unable to open the "
                                   "communication channel");
         if (resultLen != NULL) {
            *resultLen = strlen(*result);
         }
      }
      goto sent;
   } else if (priv && RpcChannel_GetType(chan) != RPCCHANNEL_TYPE_PRIV_VSOCK) {
      if (result != NULL) {
         *result = Util_SafeStrdup("Permission denied");
         if (resultLen != NULL) {
            *resultLen = strlen(*result);
         }
      }
      goto sent;
   } else if (!RpcChannel_Send(chan, data, dataLen, result, resultLen)) {
      /* We already have the description of the error */
      goto sent;
   }

   status = TRUE;

sent:
   Debug(LGPFX "Request %s: reqlen=%"FMTSZ"u, replyLen=%"FMTSZ"u\n",
         status ? "OK" : "FAILED", dataLen, resultLen ? *resultLen : 0);
   if (chan) {
      RpcChannel_Stop(chan);
      RpcChannel_Destroy(chan);
   }

   return status;
}


/**
 * Open/close RpcChannel each time for sending a Rpc message, this is a wrapper
 * for RpcChannel APIs.
 *
 * @param[in]  data        request data
 * @param[in]  dataLen     data length
 * @param[in]  result      reply, should be freed by calling RpcChannel_Free.
 * @param[in]  resultLen   reply length

 * @returns    TRUE on success.
 */

gboolean
RpcChannel_SendOneRaw(const char *data,
                      size_t dataLen,
                      char **result,
                      size_t *resultLen)
{
   return RpcChannelSendOneRaw(data, dataLen, result, resultLen, FALSE);
}


#if defined(__linux__) || defined(_WIN32)

/**
 * Open/close VSock RPC Channel each time for sending a privileged Rpc message,
 * this is a wrapper for RpcChannel APIs.
 *
 * @param[in]  data        request data
 * @param[in]  dataLen     data length
 * @param[in]  result      reply, should be freed by calling RpcChannel_Free.
 * @param[in]  resultLen   reply length

 * @returns    TRUE on success.
 */

gboolean
RpcChannel_SendOneRawPriv(const char *data,
                          size_t dataLen,
                          char **result,
                          size_t *resultLen)
{
   return RpcChannelSendOneRaw(data, dataLen, result, resultLen, TRUE);
}

#endif


/**
 * Open/close RpcChannel each time for sending a Rpc message, this is a wrapper
 * for RpcChannel APIs.
 *
 * @param[out] reply       reply, should be freed by calling RpcChannel_Free.
 * @param[out] repLen      reply length
 * @param[in]  reqFmt      request data
 * @param[in]  args        optional arguments depending on reqFmt.
 * @param[in]  priv        TRUE : create VSock channel for privileged guest RPC.
                           FALSE: follow regular RPC channel creation process.

 * @returns    TRUE on success.
 */

static gboolean
RpcChannelSendOne(char **reply,
                  size_t *repLen,
                  char const *reqFmt,
                  va_list args,
                  gboolean priv)
{
   gboolean status;
   char *request;
   size_t reqLen = 0;

   status = FALSE;

   /* Format the request string */
   request = Str_Vasprintf(&reqLen, reqFmt, args);

   /*
    * If Str_Vasprintf failed, write NULL into the reply if the caller wanted
    * a reply back.
    */
   if (request == NULL) {
      goto error;
   }

   status = RpcChannelSendOneRaw(request, reqLen, reply, repLen, priv);

   free(request);

   return status;

error:
   if (reply) {
      *reply = NULL;
   }

   if (repLen) {
      *repLen = 0;
   }
   return FALSE;
}


/**
 * Open/close RpcChannel each time for sending a Rpc message, this is a wrapper
 * for RpcChannel APIs.
 *
 * @param[out] reply       reply, should be freed by calling RpcChannel_Free.
 * @param[out] repLen      reply length
 * @param[in]  reqFmt      request data
 * @param[in]  ...         optional arguments depending on reqFmt.

 * @returns    TRUE on success.
 */

gboolean
RpcChannel_SendOne(char **reply,
                   size_t *repLen,
                   char const *reqFmt,
                   ...)
{
   va_list args;
   gboolean status;

   va_start(args, reqFmt);
   status = RpcChannelSendOne(reply, repLen, reqFmt, args, FALSE);
   va_end(args);

   return status;
}


#if defined(__linux__) || defined(_WIN32)

/**
 * Open/close VSock RPC Channel each time for sending a privileged Rpc message,
 * this is a wrapper for RpcChannel APIs.
 *
 * @param[out] reply       reply, should be freed by calling RpcChannel_Free.
 * @param[out] repLen      reply length
 * @param[in]  reqFmt      request data
 * @param[in]  ...         optional arguments depending on reqFmt.

 * @returns    TRUE on success.
 */

gboolean
RpcChannel_SendOnePriv(char **reply,
                       size_t *repLen,
                       char const *reqFmt,
                       ...)
{
   va_list args;
   gboolean status;

   va_start(args, reqFmt);
   status = RpcChannelSendOne(reply, repLen, reqFmt, args, TRUE);
   va_end(args);

   return status;
}

#endif
