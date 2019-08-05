/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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
 * @file dndcp.cpp --
 *
 * Entry points for DnD and CP plugin.
 *
 * No platform-specific code belongs here. See copyPasteDnDWrapper.[h|cpp]
 * for abstraction API to platform implementations, and copyPasteDnDImpl.h
 * for implementation class inteface. To add a new platform, derive from
 * CopyPasteDnDImpl.
 */

#include "vmware.h"

#define G_LOG_DOMAIN "dndcp"

extern "C" {
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/plugin.h"
#include "vmware/tools/utils.h"
}

#include <string.h>
#include "copyPasteDnDWrapper.h"

extern "C" {

/**
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src      Unused.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
DnDCPShutdown(gpointer src,
              ToolsAppCtx *ctx,
              gpointer data)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();
   if (p) {
      p->UnregisterCP();
      p->UnregisterDnD();
   }
   CopyPasteDnDWrapper::Destroy();
}


/**
 * Handle a reset signal.
 *
 * @param[in]  src      Unused.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
DnDCPReset(gpointer src,
           ToolsAppCtx *ctx,
           gpointer data)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();
   if (p) {
      p->OnReset();
   }
}


/**
 * Handle a no_rpc signal.
 *
 * @param[in]  src      Unused.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
DnDCPNoRpc(gpointer src,
           ToolsAppCtx *ctx,
           gpointer data)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();
   if (p) {
      p->OnNoRpc();
   }
}


/**
 * Returns the list of the plugin's capabilities.
 *
 *
 * @param[in]  src      Unused.
 * @param[in]  ctx      Unused.
 * @param[in]  set      Whether setting or unsetting the capability.
 * @param[in]  data     Unused.
 *
 * @return A list of capabilities.
 */

static GArray *
DnDCPCapabilities(gpointer src,
                  ToolsAppCtx *ctx,
                  gboolean set,
                  gpointer data)
{
   g_debug("%s: enter\n", __FUNCTION__);
   CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();
   if (p) {
      p->OnCapReg(set);
   }
   return NULL;
}


/**
 * Handles SetOption callback.
 *
 *
 * @param[in]  src       Unused.
 * @param[in]  ctx       Unused.
 * @param[in]  data      Unused.
 * @param[in]  option    The option being set.
 * @param[in]  value     The value the option is being set to.
 *
 * @return TRUE on success, FALSE otherwise.
 */

static gboolean
DnDCPSetOption(gpointer src,
               ToolsAppCtx *ctx,
               const gchar *option,
               const gchar *value,
               gpointer data)
{
   gboolean ret = FALSE;

   ASSERT(option);
   ASSERT(value);
   g_debug("%s: enter option %s value %s\n", __FUNCTION__, option, value);
   CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();

   if (option == NULL || (strcmp(option, TOOLSOPTION_ENABLEDND) != 0 &&
                         strcmp(option, TOOLSOPTION_COPYPASTE) != 0)) {
      goto out;
   }

   if (value == NULL || (strcmp(value, "2") != 0 &&
                         strcmp(value, "1") != 0 &&
                         strcmp(value, "0") != 0)) {
      goto out;
   }

   if (p) {
      p->Init(ctx);
      ret = p->OnSetOption(option, value);
   }
out:
   return ret;
}


/**
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "dndCP",
      NULL,
      NULL
   };

   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CAPABILITIES, (void *) DnDCPCapabilities, NULL },
         { TOOLS_CORE_SIG_RESET, (void *) DnDCPReset, NULL },
         { TOOLS_CORE_SIG_NO_RPC, (void *) DnDCPNoRpc, NULL },
         { TOOLS_CORE_SIG_SET_OPTION, (void *) DnDCPSetOption, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, (void *) DnDCPShutdown, NULL }
      };

      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) }
      };

      /*
       * DnD/CP Initialization here.
       */

      CopyPasteDnDWrapper *p = CopyPasteDnDWrapper::GetInstance();
      if (p) {
         p->Init(ctx);
         p->PointerInit();
      }

      regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));
      return &regData;
   }

   return NULL;
}

}
