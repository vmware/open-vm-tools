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
 * @file vixPlugin.c
 *
 * Tools Service entry point for the VIX plugin.
 */

#define G_LOG_DOMAIN "vix"

#include <string.h>

#include "vmware.h"
#include "foundryToolsDaemon.h"
#include "syncDriver.h"
#include "vixCommands.h"
#include "vixPluginInt.h"
#include "vmtools.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


/**
 * Returns the registration data for either the guestd or userd process.
 *
 * @param[in]  ctx   The application context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "vix",
      NULL,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { VIX_BACKDOORCOMMAND_RUN_PROGRAM,
         FoundryToolsDaemonRunProgram, NULL, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_GET_PROPERTIES,
         FoundryToolsDaemonGetToolsProperties, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_CHECK_USER_ACCOUNT,
         ToolsDaemonTcloCheckUserAccount, NULL, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_SEND_HGFS_PACKET,
         ToolsDaemonHgfsImpersonated, NULL, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_COMMAND,
         ToolsDaemonTcloReceiveVixCommand, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_MOUNT_VOLUME_LIST,
         ToolsDaemonTcloMountHGFS, NULL, NULL, NULL, 0 },
#if defined(_WIN32) || defined(linux)
      { VIX_BACKDOORCOMMAND_SYNCDRIVER_FREEZE,
      ToolsDaemonTcloSyncDriverFreeze, NULL, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_SYNCDRIVER_THAW,
         ToolsDaemonTcloSyncDriverThaw, NULL, NULL, NULL, 0 }
#endif
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) }
   };

   FoundryToolsDaemon_Initialize(ctx);
   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   /*
    * If running the user daemon or if the sync driver is not active, remove
    * the last two elements of the RPC registration array, so that the sync
    * driver RPC commands are ignored.
    */
#if defined(_WIN32) || defined(linux)
   if (strcmp(ctx->name, VMTOOLS_GUEST_SERVICE) != 0 || !SyncDriver_Init()) {
      g_array_remove_range(regs[0].data, regs[0].data->len - 2, 2);
   }
#endif

   return &regData;
}

