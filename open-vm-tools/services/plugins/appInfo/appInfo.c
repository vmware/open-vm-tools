/*********************************************************
 * Copyright (C) 2019 VMware, Inc. All rights reserved.
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
 * appInfo.c --
 *
 *      Captures the information about running applications inside the guest
 *      and publishes it to a guest variable.
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "appInfo.h"
#include "appInfoInt.h"
#include "vmware.h"
#include "conf.h"
#include "dynbuf.h"
#include "escape.h"
#include "str.h"
#include "util.h"
#include "vm_atomic.h"
#include "vmcheck.h"
#include "vmware/tools/log.h"
#include "vmware/tools/threadPool.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

#if defined(_WIN32)
#include "codeset.h"
#endif

/**
 * Maximum size of the packet size that appInfo plugin should send
 * to the VMX. Currently, this is set to 62 KB.
 */
#define MAX_APP_INFO_SIZE (62 * 1024)

/**
 * Default poll interval for appInfo is 30m
 */
#define APP_INFO_POLL_INTERVAL (30 * 60)

/**
 * Default value for CONFNAME_APPINFO_DISABLED setting in
 * tools configuration file.
 *
 * FALSE will enable the plugin. TRUE will disable the plugin.
 */
#define APP_INFO_CONF_DEFAULT_DISABLED_VALUE FALSE

/**
 * Defines the current poll interval (in seconds).
 *
 * This value is controlled by the appinfo.poll-interval config file option.
 */
int appInfoPollInterval = 0;

/**
 * AppInfo gather loop timeout source.
 */
static GSource *appInfoTimeoutSource = NULL;


/*
 *****************************************************************************
 * EscapeJSONString --
 *
 * Escapes a string to be included in JSON content.
 *
 * @param[in] str The string to be escaped.
 *
 * @retval Pointer to a heap-allocated memory. This holds the escaped content
 *         of the string passed by the caller.
 *
 *****************************************************************************
 */

static char *
EscapeJSONString(const char *str)    // IN
{
   /*
    * Escape '"' and '\' characters in the JSON string.
    */

   static const int bytesToEscape[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   // "
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0,   // '\'
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   };

   return Escape_DoString("\\u00", bytesToEscape, str, strlen(str),
                          NULL);
}


/*
 *****************************************************************************
 * SetGuestInfo --
 *
 * Sends a simple key-value update request to the VMX.
 *
 * @param[in] ctx       Application context.
 * @param[in] key       Key sent to the VMX
 * @param[in] value     GuestInfo data sent to the VMX
 *
 * @retval TRUE  RPCI succeeded.
 * @retval FALSE RPCI failed.
 *
 *****************************************************************************
 */

static Bool
SetGuestInfo(ToolsAppCtx *ctx,              // IN:
             const char *guestVariableName, // IN:
             const char *value)             // IN:
{
   Bool status;
   char *reply = NULL;
   gchar *msg;
   size_t replyLen;

   ASSERT(guestVariableName);
   ASSERT(value);

   msg = g_strdup_printf("info-set guestinfo.%s %s",
                         guestVariableName,
                         value);

   status = RpcChannel_Send(ctx->rpc,
                            msg,
                            strlen(msg) + 1,
                            &reply,
                            &replyLen);
   g_free(msg);

   if (!status) {
      g_warning("Error sending RPC message: %s\n", reply ? reply : "NULL");
      vm_free(reply);
      return FALSE;
   } else {
      g_info("Successfully sent the app information.\n");
   }

   status = (*reply == '\0');
   vm_free(reply);
   return status;
}


/*
 *****************************************************************************
 * AppInfoGatherTask --
 *
 * Collects all the desired application related information and updates VMX.
 *
 * @param[in]  data     The application context.
 *
 *****************************************************************************
 */

static void
AppInfoGatherTask(ToolsAppCtx *ctx,
                  void *data)
{
   DynBuf dynBuffer;
   char tmpBuf[1024];
   int len;
   gchar *tstamp = NULL;
   char *escapedCmd = NULL;
   char *escapedVersion = NULL;
   GSList *appList;
   GSList *appNode;
   static Atomic_uint64 updateCounter = {0};
   uint64 counter = (uint64) Atomic_ReadInc64(&updateCounter) + 1;

   static char headerFmt[] = "{\n"
                     "\"" APP_INFO_KEY_VERSION        "\":\"%d\", \n"
                     "\"" APP_INFO_KEY_UPDATE_COUNTER "\":\"%"FMT64"d\", \n"
                     "\"" APP_INFO_KEY_PUBLISHTIME    "\":\"%s\", \n"
                     "\"" APP_INFO_KEY_APPS           "\":[";
   static char jsonPerAppFmt[] =
                           "%s\n" // , for all the elements after the first one
                           "{"
                           "\"" APP_INFO_KEY_APP_NAME    "\":\"%s\""
                           ","
                           "\"" APP_INFO_KEY_APP_VERSION "\":\"%s\""
                           "}";
   static char jsonSuffix[] = "]}";

   DynBuf_Init(&dynBuffer);

   tstamp = VMTools_GetTimeAsString();

   len = Str_Snprintf(tmpBuf, sizeof tmpBuf, headerFmt,
                      APP_INFO_VERSION_1,
                      counter,
                      tstamp != NULL ? tstamp : "");
   if (len <= 0) {
      g_warning("Insufficient space for the header.\n");
      goto abort;
   }

   DynBuf_Append(&dynBuffer, tmpBuf, len);

   appList = AppInfo_SortAppList(AppInfo_GetAppList());

   for (appNode = appList; appNode != NULL; appNode = appNode->next) {
      size_t currentBufferSize = DynBuf_GetSize(&dynBuffer);
      AppInfo *appInfo = (AppInfo *) appNode->data;

      if (appInfo->appName == NULL ||
          appInfo->version == NULL) {
         goto next_entry;
      }

      escapedCmd = EscapeJSONString(appInfo->appName);

      if (NULL == escapedCmd) {
         g_warning("Failed to escape the content of cmdName\n");
         goto abort;
      }

      escapedVersion = EscapeJSONString(appInfo->version);
      if (NULL == escapedVersion) {
         g_warning("Failed to escape the content of version information.\n");
         goto abort;
      }

      if (appNode == appList) {
         len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerAppFmt,
                            "", escapedCmd, escapedVersion);
      } else {
         /*
          * If this is not the first element, then add ',' at the beginning.
          */
         len = Str_Snprintf(tmpBuf, sizeof tmpBuf, jsonPerAppFmt,
                            ",", escapedCmd, escapedVersion);
      }

      if (len < 0) {
         g_warning("Insufficient space for the application information.\n");
         goto next_entry;
      }

      if (currentBufferSize + len + sizeof jsonSuffix > MAX_APP_INFO_SIZE) {
         g_warning("Exceeded the max info packet size."
                   " Truncating the rest of the applications.\n");
         break;
      }

      DynBuf_Append(&dynBuffer, tmpBuf, len);

next_entry:
      free(escapedCmd);
      escapedCmd = NULL;
      free(escapedVersion);
      escapedVersion = NULL;
   }

   DynBuf_Append(&dynBuffer, jsonSuffix, sizeof jsonSuffix - 1);
   SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, DynBuf_GetString(&dynBuffer));

abort:
   free(escapedCmd);
   free(escapedVersion);
   AppInfo_DestroyAppList(appList);
   g_free(tstamp);
   DynBuf_Destroy(&dynBuffer);
}


/*
 *****************************************************************************
 * AppInfoGather --
 *
 * Creates a new thread that collects all the desired application related
 * information and udates the VMX.
 *
 * @param[in]  data     The application context.
 *
 * @return TRUE to indicate that the timer should be rescheduled.
 *
 *****************************************************************************
 */

static gboolean
AppInfoGather(gpointer data)
{
   ToolsAppCtx *ctx = data;
   if (!ToolsCorePool_SubmitTask(ctx, AppInfoGatherTask, NULL, NULL)) {
      g_warning("%s: failed to start information gather thread\n",
                __FUNCTION__);
   }

   return TRUE;
}


/*
 *****************************************************************************
 * TweakGatherLoop --
 *
 * @brief Start, stop, reconfigure a GuestInfoGather poll loop.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * AppDiscoveryGather loop timeout source.
 *
 * @param[in]     ctx           The app context.
 * @param[in]     enable        Whether to enable the gather loop.
 *
 *****************************************************************************
 */

static void
TweakGatherLoop(ToolsAppCtx *ctx,
                gboolean enable)
{
   gint pollInterval = 0;

   if (enable) {
      pollInterval = APP_INFO_POLL_INTERVAL * 1000;

      /*
      * Check the config registry for custom poll interval,
      * converting from seconds to milliseconds.
      */
      if (g_key_file_has_key(ctx->config, CONFGROUPNAME_APPINFO,
                             CONFNAME_APPINFO_POLLINTERVAL, NULL)) {
         GError *gError = NULL;

         pollInterval = g_key_file_get_integer(ctx->config,
                                               CONFGROUPNAME_APPINFO,
                                               CONFNAME_APPINFO_POLLINTERVAL,
                                               &gError);
         pollInterval *= 1000;

         if (pollInterval < 0 || gError) {
            g_warning("Invalid %s.%s value. Using default %us.\n",
                      CONFGROUPNAME_APPINFO,
                      CONFNAME_APPINFO_POLLINTERVAL,
                      APP_INFO_POLL_INTERVAL);
            pollInterval = APP_INFO_POLL_INTERVAL * 1000;
         }

         g_clear_error(&gError);
      }
   }

   if (appInfoTimeoutSource != NULL) {
      /*
       * If the interval hasn't changed, let's not interfere with the existing
       * timeout source.
       */
      if (pollInterval == appInfoPollInterval) {
         ASSERT(pollInterval);
         return;
      }

      /*
       * Destroy the existing timeout source since the interval has changed.
       */

      g_source_destroy(appInfoTimeoutSource);
      appInfoTimeoutSource = NULL;
   }

   /*
    * All checks have passed.  Create a new timeout source and attach it.
    */
   appInfoPollInterval = pollInterval;

   if (appInfoPollInterval) {
      g_info("New value for %s is %us.\n",
             CONFNAME_APPINFO_POLLINTERVAL,
             appInfoPollInterval / 1000);

      appInfoTimeoutSource = g_timeout_source_new(appInfoPollInterval);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, appInfoTimeoutSource,
                               AppInfoGather, ctx, NULL);
      g_source_unref(appInfoTimeoutSource);
   } else {
      g_info("Poll loop for %s disabled.\n", CONFNAME_APPINFO_POLLINTERVAL);
      SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, "");
   }
}


/*
 *****************************************************************************
 * AppInfoServerConfReload --
 *
 * @brief Reconfigures the poll loop interval upon config file reload.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused.
 *
 *****************************************************************************
 */

static void
AppInfoServerConfReload(gpointer src,
                        ToolsAppCtx *ctx,
                        gpointer data)
{
   gboolean disabled =
      VMTools_ConfigGetBoolean(ctx->config,
                               CONFGROUPNAME_APPINFO,
                               CONFNAME_APPINFO_DISABLED,
                               APP_INFO_CONF_DEFAULT_DISABLED_VALUE);
   TweakGatherLoop(ctx, !disabled);
}


/*
 *****************************************************************************
 * AppInfoServerShutdown --
 *
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     Unused.
 * @param[in]  data    Unused.
 *
 *****************************************************************************
 */

static void
AppInfoServerShutdown(gpointer src,
                      ToolsAppCtx *ctx,
                      gpointer data)
{
   if (appInfoTimeoutSource != NULL) {
      g_source_destroy(appInfoTimeoutSource);
      appInfoTimeoutSource = NULL;
   }

   SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, "");
}


/*
 *****************************************************************************
 * ToolsOnLoad --
 *
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data.
 *
 *****************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "appInfo",
      NULL,
      NULL
   };

   uint32 vmxVersion = 0;
   uint32 vmxType = VMX_TYPE_UNSET;

   /*
    * Return NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("Not running in a VMware VM.\n");
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if VM is not running on ESX host.
    */
   if (!VmCheck_GetVersion(&vmxVersion, &vmxType) ||
       vmxType != VMX_TYPE_SCALABLE_SERVER) {
      g_info("VM is not running on ESX host.\n");
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("Not running in vmsvc daemon: container name='%s'.\n",
             ctx->name);
      return NULL;
   }

   /*
    * This plugin is useless without an RpcChannel.  If we don't have one,
    * just bail.
    */
   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CONF_RELOAD, AppInfoServerConfReload, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, AppInfoServerShutdown, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs))
         }
      };

      gboolean disabled = FALSE;

      regData.regs = VMTools_WrapArray(regs,
                                       sizeof *regs,
                                       ARRAYSIZE(regs));

      /*
       * Set up the AppInfo gather loop.
       */
      disabled =
         VMTools_ConfigGetBoolean(ctx->config,
                                  CONFGROUPNAME_APPINFO,
                                  CONFNAME_APPINFO_DISABLED,
                                  APP_INFO_CONF_DEFAULT_DISABLED_VALUE);
      TweakGatherLoop(ctx, !disabled);

      return &regData;
   }

   return NULL;
}


/*
 * END Tools Core Services goodies.
 *****************************************************************************
 */
