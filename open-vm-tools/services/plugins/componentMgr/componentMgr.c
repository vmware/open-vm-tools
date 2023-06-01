/*********************************************************
 * Copyright (c) 2021 VMware, Inc. All rights reserved.
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

/*
 * componentMgr.c --
 *
 *      Adds/removes components in the guest OS. Periodically polls
 *      for all the components managed by the plugin and fetches the guestVar
 *      guestinfo./vmware.components.<comp_name>.desiredstate for present or
 *      absent action and accordingly adds/removes the component from the
 *      system.
 *      All actions on a component runs as an async process.
 */

#include "componentMgrPlugin.h"
#include "str.h"
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);


/*
 * componentMgr plugin poll interval timeout source.
 */
static GSource *gComponentMgrTimeoutSource = NULL;

/*
 * Tools application context.
 */
static ToolsAppCtx *gCtx;

/*
 * componentMgr plugin poll interval.
 */
static guint gComponentMgrPollInterval = 0;

static gboolean ComponentMgrCb(gpointer data);


/*
 *****************************************************************************
 * ComponentMgr_GetToolsAppCtx --
 *
 * A getter function to fetch the tools application context in all points of
 * the plugin.
 *
 * @return
 *      Main ToolsAppCtx of the plugin.
 *
 *****************************************************************************
 */

ToolsAppCtx*
ComponentMgr_GetToolsAppCtx()
{
   return gCtx;
}


/*
 *****************************************************************************
 * ReconfigureComponentMgrPollLoopEx --
 *
 * Start, stop and reconfigure componentMgr plugin poll loop.
 * This function is responsible for creating, handling, and resetting the
 * componentMgr loop timeout source.
 *
 * @param[in] ctx Tools application context.
 * @param[in] pollInterval Poll interval in seconds.
 *
 * @return
 *      None
 *
 * Side effects:
 *      Deletes the existing timeout source and recreates a new one.
 *
 *****************************************************************************
 */

static void
ReconfigureComponentMgrPollLoopEx(ToolsAppCtx *ctx,  // IN
                                  gint pollInterval) // IN
{
   if (gComponentMgrPollInterval == pollInterval) {
      g_debug("%s: ComponentMgr poll interval has not been changed since"
              " last time.\n", __FUNCTION__);
      return;
   }

   if (gComponentMgrTimeoutSource != NULL) {
      /*
       * Destroy the existing timeout source.
       */
      g_source_destroy(gComponentMgrTimeoutSource);
      gComponentMgrTimeoutSource = NULL;
   }

   if (pollInterval != 0) {
      if (pollInterval < COMPONENTMGR_MIN_POLL_INTERVAL ||
         pollInterval > (G_MAXINT / 1000)) {
         g_warning("%s: Invalid poll interval. Using default %us.\n",
                   __FUNCTION__, COMPONENTMGR_DEFAULT_POLL_INTERVAL);
         pollInterval = COMPONENTMGR_DEFAULT_POLL_INTERVAL;
      }

      g_info("%s: New value for %s is %us.\n", __FUNCTION__,
             COMPONENTMGR_CONF_POLLINTERVAL,
             pollInterval);

      gComponentMgrTimeoutSource = g_timeout_source_new(pollInterval * 1000);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, gComponentMgrTimeoutSource,
                               ComponentMgrCb, ctx, NULL);
      g_source_unref(gComponentMgrTimeoutSource);
   } else {
      /*
       * Plugin will be disabled since poll interval configured is 0.
       * No components will be managed. Publish guestVar available.
       */
      g_info("%s: ComponentMgr plugin disabled.\n", __FUNCTION__);
      ComponentMgr_PublishAvailableComponents(ctx,
                                              COMPONENTMGR_NONECOMPONENTS);
   }

   gComponentMgrPollInterval = pollInterval;
}


/*
 *****************************************************************************
 * ComponentMgrCb --
 *
 * This function updates the component status managed by the plugin.
 * This function internally calls present/absent actions on the respective
 * components.
 *
 * @param[in] data Tools application context.
 *
 * @return
 *      G_SOURCE_CONTINUE To continue polling.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

static gboolean
ComponentMgrCb(gpointer data) // IN
{
   ToolsAppCtx *ctx = data;

   if (ComponentMgr_CheckAnyAsyncProcessRunning()) {
      g_debug("%s: A component has an async process running. Skipping "
              "component status update.\n", __FUNCTION__);
      return G_SOURCE_CONTINUE;
   }

   /*
    * Update the enabled components managed by the plugin and publish
    * guestVar for all the available components.
    */
   ComponentMgr_UpdateComponentEnableStatus(ctx);

   /*
    * Main function which handles the core logic of taking actions present or
    * absent on the components by reading action from the component guestVars.
    */
   ComponentMgr_UpdateComponentStatus(ctx);

   return G_SOURCE_CONTINUE;
}


/*
 *****************************************************************************
 * ComponentMgrPollLoop --
 *
 * This function reads the poll interval and included configurations from the
 * config file and reconfigures the poll loop of the plugin.
 *
 * @param[in] data Tools application context.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

static void
ComponentMgrPollLoop(ToolsAppCtx *ctx) // IN
{
   gint pollInterval;
   gchar *listString;

   pollInterval = VMTools_ConfigGetInteger(ctx->config,
                                           COMPONENTMGR_CONF_GROUPNAME,
                                           COMPONENTMGR_CONF_POLLINTERVAL,
                                           COMPONENTMGR_DEFAULT_POLL_INTERVAL);

   listString = VMTools_ConfigGetString(ctx->config,
                                        COMPONENTMGR_CONF_GROUPNAME,
                                        COMPONENTMGR_CONF_INCLUDEDCOMPONENTS,
                                        COMPONENTMGR_ALLCOMPONENTS);

   /*
    * If the included conf value is not present or is empty, no components will
    * be enabled and plugin will be disabled until further change.
    */
   if (listString == NULL || *listString == '\0' ||
       Str_Strcmp(listString, COMPONENTMGR_NONECOMPONENTS) == 0) {
      g_info("%s: No components managed by the plugin. Plugin disabled."
              " Set value included in configuration.\n",  __FUNCTION__);
      pollInterval = 0;
   }

   g_free(listString);
   ReconfigureComponentMgrPollLoopEx(ctx, pollInterval);
}


/*
 ******************************************************************************
 * ComponentMgrServerShutdown --
 *
 * This function cleans up plugin internal data on shutdown.
 *
 * @param[in] src The source object.
 * @param[in] ctx Tools application context.
 * @param[in] data Unused.
 *
 * @return
 *      None
 *
 * Side effects:
 *      Destroys all timeout sources for all the components.
 *      Destroys all running async process for all the components.
 *
 ******************************************************************************
 */

static void
ComponentMgrServerShutdown(gpointer src,     // IN
                           ToolsAppCtx *ctx, // IN
                           gpointer data)    // IN
{
   if (gComponentMgrTimeoutSource != NULL) {
      /*
       * Destroy the existing timeout source.
       */
      g_source_destroy(gComponentMgrTimeoutSource);
      gComponentMgrTimeoutSource = NULL;
   }

   // Destroy all GSource timers for all managed components.
   ComponentMgr_Destroytimers();

   // Destroy and free all running async process for all components.
   ComponentMgr_DestroyAsyncProcess();
}


/*
 ******************************************************************************
 * ComponentMgrServerConfReload --
 *
 * This function reconfigures the poll loop interval upon config file reload.
 *
 * @param[in] src The source object.
 * @param[in] ctx Tools application context.
 * @param[in] data Unused.
 *
 * @return
 *      None.
 *
 * Side effects:
 *      None.
 *
 ******************************************************************************
 */

static void
ComponentMgrServerConfReload(gpointer src,     // IN
                             ToolsAppCtx *ctx, // IN
                             gpointer data)    // IN
{
   ComponentMgrPollLoop(ctx);
}


/*
 ******************************************************************************
 * ComponentMgrServerReset --
 *
 * Callback function that gets called whenever the RPC channel gets reset.
 *
 * @param[in] src The source object.
 * @param[in] ctx Tools application context.
 * @param[in] data Unused.
 *
 * @return
 *     None.
 *
 * Side effects:
 *     Reinitializes the plugin timeout source.
 *
 ******************************************************************************
 */

static void
ComponentMgrServerReset(gpointer src,     // IN
                        ToolsAppCtx *ctx, // IN
                        gpointer data)    // IN
{
   /*
    * Handle reset for componentMgr loop.
    */
   if (gComponentMgrTimeoutSource != NULL) {
      ASSERT(gComponentMgrPollInterval != 0);

      ReconfigureComponentMgrPollLoopEx(ctx, gComponentMgrPollInterval);
   } else {
      ComponentMgrPollLoop(ctx);
   }
}

/*
 *****************************************************************************
 * ToolsOnLoad --
 *
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in] ctx Tools application context.
 *
 * @return
 *      Plugin registration data.
 *
 * Side effects:
 *      None.
 *
 *****************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx) // IN
{
   static ToolsPluginData regData = {
      "componentMgr",
      NULL,
      NULL
   };

   /*
    * Return NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("%s: Not running in a VMware VM.\n", __FUNCTION__);
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("%s: Not running in vmsvc daemon: container name='%s'.\n",
             __FUNCTION__, ctx->name);
      return NULL;
   }

   gCtx = ctx;

   /*
    * This plugin is useless without an RpcChannel.  If we don't have one,
    * just bail.
    */
   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CONF_RELOAD, ComponentMgrServerConfReload, NULL },
         { TOOLS_CORE_SIG_RESET, ComponentMgrServerReset, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, ComponentMgrServerShutdown, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs))
         }
      };

      regData.regs = VMTools_WrapArray(regs,
                                       sizeof *regs,
                                       ARRAYSIZE(regs));

      return &regData;
   }

   return NULL;
}
