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
#include "hgfsServerManager.h"
#include "vm_assert.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"


#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


/**
 * Clean up internal state on shutdown.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  plugin   Plugin registration data.
 */

static void
HgfsServerShutdown(gpointer src,
                   ToolsAppCtx *ctx,
                   ToolsPluginData *plugin)
{
   HgfsServerMgrData *mgrData = plugin->_private;
   HgfsServerManager_Unregister(mgrData);
   g_free(mgrData);
}


/**
 * Handles hgfs requests.
 *
 * @param[in]  data  RPC request data.
 *
 * @return TRUE on success, FALSE on error.
 */

static gboolean
HgfsServerRpcDispatch(RpcInData *data)
{
   HgfsServerMgrData *mgrData;
   size_t replySize;
   static char reply[HGFS_LARGE_PACKET_MAX];


   ASSERT(data->clientData != NULL);
   mgrData = data->clientData;

   if (data->argsSize == 0) {
      return RPCIN_SETRETVALS(data, "1 argument required", FALSE);
   }

   replySize = sizeof reply;
   HgfsServerManager_ProcessPacket(mgrData, data->args + 1, data->argsSize - 1, reply, &replySize);

   data->result = reply;
   data->resultLen = replySize;
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
                 ToolsPluginData *plugin)
{
   gchar *msg;
   const char *appName = NULL;

   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) == 0) {
      appName = TOOLS_DAEMON_NAME;
   } else if (strcmp(ctx->name, VMTOOLS_USER_SERVICE) == 0) {
      appName = TOOLS_DND_NAME;
   } else {
      NOT_REACHED();
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
   HgfsServerMgrData *mgrData;

   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) != 0 &&
       strcmp(ctx->name, VMTOOLS_USER_SERVICE) != 0) {
      g_info("Unknown container '%s', not loading HGFS plugin.", ctx->name);
      return NULL;
   }

   mgrData = g_malloc0(sizeof *mgrData);
   HgfsServerManager_DataInit(mgrData,
                              ctx->name,
                              NULL,       // rpc channel unused
                              NULL);      // no rpc callback

   if (!HgfsServerManager_Register(mgrData)) {
      g_warning("HgfsServer_InitState() failed, aborting HGFS server init.\n");
      g_free(mgrData);
      return NULL;
   }

   {
      RpcChannelCallback rpcs[] = {
         { HGFS_SYNC_REQREP_CMD, HgfsServerRpcDispatch, mgrData, NULL, NULL, 0 }
      };
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CAPABILITIES, HgfsServerCapReg, &regData },
         { TOOLS_CORE_SIG_SHUTDOWN, HgfsServerShutdown, &regData }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
   }
   regData._private = mgrData;

   return &regData;
}
