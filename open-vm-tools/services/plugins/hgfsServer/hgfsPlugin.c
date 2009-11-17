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
 * @file hgfsPlugin.c
 *
 * Functionality to utilize the hgfs server in bora/lib as a tools plugin.
 */

#include <string.h>

#define G_LOG_DOMAIN "hgfsd"

#include "hgfs.h"
#include "hgfsServerPolicy.h"
#include "hgfsServer.h"
#include "hgfsChannel.h"
#include "vm_assert.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


/**
 * Sets up the channel for HGFS.
 *
 * NOTE: Initialize the Hgfs server for only for now.
 * This will move into a separate file when full interface implemented.
 *
 * @param[in]  data  Unused RPC request data.
 *
 * @return TRUE on success, FALSE on error.
 */

Bool
HgfsChannel_Init(void *data)     // IN: Unused data
{
   HgfsServerSessionCallbacks *serverCBTable = NULL;
   return HgfsServer_InitState(&serverCBTable, NULL);
}


/**
 * Close up the channel for HGFS.
 *
 * NOTE: Close open sessions in the HGFS server currently.
 * This will move into a separate file when full interface implemented.
 *
 * @param[in]  data  Unused RPC request data.
 *
 * @return None.
 */

void
HgfsChannel_Exit(void *data)
{
   ASSERT(data != NULL);
   HgfsServer_ExitState();
}


/**
 * Handles hgfs requests.
 *
 * @param[in]  data  RPC request data.
 *
 * @return TRUE on success, FALSE on error.
 */

static gboolean
HgfsServerRpcInDispatch(RpcInData *data)
{
   size_t packetSize;
   static char packet[HGFS_LARGE_PACKET_MAX];


   ASSERT(data->clientData == NULL);

   if (data->argsSize == 0) {
      return RPCIN_SETRETVALS(data, "1 argument required", FALSE);
   }

   packetSize = data->argsSize - 1;
   HgfsServer_ProcessPacket(data->args + 1, packet, &packetSize, 0);

   data->result = packet;
   data->resultLen = packetSize;
   return TRUE;
}


/**
 * Sends the HGFS capability to the VMX.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  set      Whether setting or unsetting the capability.
 * @param[in]  data     Unused.
 *
 * @return NULL. The function sends the capability directly.
 */

static GArray *
HgfsServerCapReg(gpointer src,
                 ToolsAppCtx *ctx,
                 gboolean set,
                 gpointer data)
{
   gchar *msg;
   const char *appName = NULL;

   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) == 0) {
      appName = TOOLS_DAEMON_NAME;
   } else if (strcmp(ctx->name, VMTOOLS_USER_SERVICE) == 0) {
      appName = TOOLS_DND_NAME;
   } else {
      g_error("Shouldn't reach this.\n");
   }

   msg = g_strdup_printf("tools.capability.hgfs_server %s %d",
                         appName,
                         set ? 1 : 0);

   /*
    * Prior to WS55, the VMX did not know about the "hgfs_server"
    * capability. This doesn't mean that the HGFS server wasn't needed, it's
    * just that the capability was introduced in CS 225439 so that the VMX
    * could decide which HGFS server to communicate with.
    *
    * Long story short, we shouldn't care if this function fails.
    */
   if (ctx->rpc && !RpcChannel_Send(ctx->rpc, msg, strlen(msg) + 1, NULL, NULL)) {
      g_warning("Setting HGFS server capability failed!\n");
   }

   g_free(msg);
   return NULL;
}


/**
 * Returns the registration data for the HGFS server.
 *
 * @param[in]  ctx   The application context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "hgfsServer",
      NULL,
      NULL
   };

   /*
    * Passing NULL here is safe because the shares maintained by the guest
    * policy server never change, invalidating the need for an invalidate
    * function.
    */
   if (!HgfsServerPolicy_Init(NULL)) {
      g_warning("HgfsServerPolicy_Init() failed, aborting HGFS server init.\n");
      return NULL;
   }

   if (!HgfsChannel_Init(NULL)) {
      g_warning("HgfsServer_InitState() failed, aborting HGFS server init.\n");
      HgfsServerPolicy_Cleanup();
      return NULL;
   }

   {
      RpcChannelCallback rpcs[] = {
         { HGFS_SYNC_REQREP_CMD, HgfsServerRpcInDispatch, NULL, NULL, NULL, 0 }
      };
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CAPABILITIES, HgfsServerCapReg, &regData }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   }

   return &regData;
}
