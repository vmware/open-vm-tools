/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#include "syncDriver.h"
#include "vixCommands.h"
#include "vixPluginInt.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

/**
 * IO freeze signal handler. Restrict VIX commands.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The application context.
 * @param[in]  freeze   Whether I/O is being frozen.
 * @param[in]  data     Unused.
 */

static void
VixIOFreeze(gpointer src,
            ToolsAppCtx *ctx,
            gboolean freeze,
            gpointer data)
{
   FoundryToolsDaemon_RestrictVixCommands(ctx, freeze);
}

/**
 * Clean up internal state on shutdown.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  plugin   Plugin registration data.
 */

static void
VixShutdown(gpointer src,
            ToolsAppCtx *ctx,
            ToolsPluginData *plugin)
{
   FoundryToolsDaemon_Uninitialize(ctx);
}


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
      { VIX_BACKDOORCOMMAND_COMMAND,
         ToolsDaemonTcloReceiveVixCommand, NULL, NULL, 0 },
      { VIX_BACKDOORCOMMAND_MOUNT_VOLUME_LIST,
         ToolsDaemonTcloMountHGFS, NULL, NULL, NULL, 0 },
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_SHUTDOWN, VixShutdown, &regData }
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
   };

#if defined(G_PLATFORM_WIN32)
   ToolsCore_InitializeCOM(ctx);
#endif

   FoundryToolsDaemon_Initialize(ctx);
   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   if (TOOLS_IS_MAIN_SERVICE(ctx) && SyncDriver_Init()) {
      size_t i;
      size_t reg;

      for (reg = 0; reg < ARRAYSIZE(regs); reg++) {
         if (regs[reg].type == TOOLS_APP_SIGNALS) {
            /*
             * If running the system daemon and if the sync driver is active,
             * add signal registrations for IO_FREEZE signal.
             */
            ToolsPluginSignalCb sysSigs[] = {
               { TOOLS_CORE_SIG_IO_FREEZE, VixIOFreeze, NULL }
            };

            for (i = 0; i < ARRAYSIZE(sysSigs); i++) {
               g_array_append_val(regs[reg].data, sysSigs[i]);
            }
         }
#if defined(_WIN32) || defined(__linux__)
         else if (regs[reg].type == TOOLS_APP_GUESTRPC) {
            /*
             * If running the system daemon and if the sync driver is active,
             * add RPC registrations for sync driver RPC commands.
             */
            RpcChannelCallback sysRpcs[] = {
               { VIX_BACKDOORCOMMAND_SYNCDRIVER_FREEZE,
                  ToolsDaemonTcloSyncDriverFreeze, NULL, NULL, NULL, 0 },
               { VIX_BACKDOORCOMMAND_SYNCDRIVER_THAW,
                  ToolsDaemonTcloSyncDriverThaw, NULL, NULL, NULL, 0 }
            };

            for (i = 0; i < ARRAYSIZE(sysRpcs); i++) {
               g_array_append_val(regs[reg].data, sysRpcs[i]);
            }
         }
#endif
      }
   }

   return &regData;
}
