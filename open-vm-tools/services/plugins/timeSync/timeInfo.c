/*********************************************************
 * Copyright (C) 2022 VMware, Inc. All rights reserved.
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
 * @file timeInfo.c
 *
 * The feature allows tools to subscribe and receive updates from VMX when
 * time related properties of the host change.
 */

#include "conf.h"
#include "timeSync.h"
#include "system.h"
#include "strutil.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"

typedef struct TimeInfoVmxRpcCtx {
   char *request;
   struct {
      char *reply;
      size_t replyLen;
   } response;
} TimeInfoVmxRpcCtx;

static const char *TIMEINFO_VMXRPC_CLOCKID         = "precisionclock0";
static const char *TIMEINFO_VMXRPC_CMD_SUBSCRIBE   = "subscribe";
static const char *TIMEINFO_VMXRPC_CMD_UNSUBSCRIBE = "unsubscribe";
static const char *TIMEINFO_VMXRPC_STATUS_OK       = "OK";
static ToolsAppCtx *gToolsAppCtx;


/**
 * Cleanup routine after performing GuestRPC.
 *
 * @param[in] rpc TimeInfo RPC context.
 */

static void
TimeInfoVmxRpcDone(TimeInfoVmxRpcCtx *rpc)
{
   free(rpc->request);
   RpcChannel_Free(rpc->response.reply);
   memset(rpc, 0, sizeof *rpc);
}


/**
 * Perform given GuestRPC.
 *
 * @param[in] rpc    TimeInfo RPC context
 * @param[in] method GuestRPC method to invoke.
 * @param[in] argv   List of arguments to GuestRPC
 * @param[in] argc   Number of arguments to GuestRPC
 *
 * @return TRUE on successful invocation of GuestRPC, FALSE otherwise.
 */

static gboolean
TimeInfoVmxRpcDo(TimeInfoVmxRpcCtx *rpc,
                 const char *method,
                 const char *argv[],
                 size_t argc)
{
   Bool ok;
   char *next;
   int i;
   char *status;

   memset(rpc, 0, sizeof *rpc);
   StrUtil_SafeStrcatF(&rpc->request, "timeInfo.%s", method);
   for (i = 0; i < argc; ++i) {
      StrUtil_SafeStrcatF(&rpc->request, " %s", argv[i]);
   }

   g_debug("%s: Sending RPC: '%s'", __FUNCTION__, rpc->request);
   ok = RpcChannel_Send(gToolsAppCtx->rpc, rpc->request,
                        strlen(rpc->request), &rpc->response.reply,
                        &rpc->response.replyLen);
   if (!ok) {
      g_warning("%s: RpcChannel_Send failed.", __FUNCTION__);
      return FALSE;
   }

   if (rpc->response.reply == NULL || rpc->response.replyLen == 0) {
      g_warning("%s: Empty response received from VMX.", __FUNCTION__);
      return FALSE;
   }
   g_debug("%s: RPC response: %s\n", __FUNCTION__, rpc->response.reply);

   next = rpc->response.reply;
   status = StrUtil_GetNextItem(&next, '\n');

   /* On success, extract payload. */
   if (status == NULL ||
       strcmp(status, TIMEINFO_VMXRPC_STATUS_OK) != 0 || next != NULL) {
      g_warning("%s: Invalid result payload.", __FUNCTION__);
      return FALSE;
   }
   return TRUE;
}


/**
 * Subscribe to TimeInfo updates. If successful, VMX will send UPDATE
 * GuestRPCs to tools when host's time related properties change.
 */

static void
TimeInfoVmxSubscribe(void)
{
   TimeInfoVmxRpcCtx vmxRpc;
   const char *argv[1] = { TIMEINFO_VMXRPC_CLOCKID };

   g_debug("%s: Subscribing for notifications from VMX.", __FUNCTION__);
   if (!TimeInfoVmxRpcDo(&vmxRpc, TIMEINFO_VMXRPC_CMD_SUBSCRIBE,
                         argv, ARRAYSIZE(argv))) {
      g_warning("%s: Failed to subscribe with VMX for notifications.",
                __FUNCTION__);
   }
   TimeInfoVmxRpcDone(&vmxRpc);
}


/**
 * Unsubscribe from TimeInfo updates. If successful, VMX will no longer
 * send UPDATE GuestRPC to the tools.
 */

static void
TimeInfoVmxUnsubscribe(void)
{
   TimeInfoVmxRpcCtx vmxRpc;
   const char *argv[1] = { TIMEINFO_VMXRPC_CLOCKID };

   g_debug("%s: Unsubscribing from notifications from VMX.", __FUNCTION__);
   if (!TimeInfoVmxRpcDo(&vmxRpc, TIMEINFO_VMXRPC_CMD_UNSUBSCRIBE,
                         argv, ARRAYSIZE(argv))) {
      g_warning("%s: Failed to unsubscribe from VMX notifications.",
                __FUNCTION__);
   }
   TimeInfoVmxRpcDone(&vmxRpc);
}


/**
 * Initialize TimeInfo in TimeSync.
 *
 * @param[in] ctx The application context.
 */

void
TimeInfo_Init(ToolsAppCtx *ctx)
{
   gboolean timeInfoEnabled =
      g_key_file_get_boolean(ctx->config,
                             CONFGROUPNAME_TIMESYNC,
                             CONFNAME_TIMESYNC_TIMEINFO_ENABLED,
                             NULL);
   ASSERT(vmx86_linux);
   g_debug("%s: TimeInfo support is %senabled.\n",
           __FUNCTION__, !timeInfoEnabled ? "not " : "");
   if (timeInfoEnabled) {
      gToolsAppCtx = ctx;
      TimeInfoVmxSubscribe();
   }
}


/**
 * Cleans up internal TimeInfo state.
 */

void
TimeInfo_Shutdown(void)
{
   if (gToolsAppCtx != NULL) {
      TimeInfoVmxUnsubscribe();
      gToolsAppCtx = NULL;
   }
}
