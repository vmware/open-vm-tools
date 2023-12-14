/*********************************************************
 * Copyright (C) 2019-2022 VMware, Inc. All rights reserved.
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

#include "appInfoInt.h"
#include "vmware.h"
#include "codeset.h"
#include "conf.h"
#include "dynbuf.h"
#include "escape.h"
#include "str.h"
#include "util.h"
#include "vm_atomic.h"
#include "vmcheck.h"
#include "vmware/guestrpc/appInfo.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/threadPool.h"
#include "vmware/tools/utils.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

/**
 * Maximum size of the packet size that appInfo plugin should send
 * to the VMX. Currently, this is set to 62 KB.
 */
#define MAX_APP_INFO_SIZE (62 * 1024)

/**
 * Default poll interval for appInfo is 6 hours
 */
#define APP_INFO_POLL_INTERVAL (360 * 60)

/**
 * Default value for CONFNAME_APPINFO_DISABLED setting in
 * tools configuration file.
 *
 * FALSE will activate the plugin. TRUE will deactivate the plugin.
 */
#define APP_INFO_CONF_DEFAULT_DEACTIVATED_VALUE FALSE

/**
 * Default value for CONFNAME_APPINFO_REMOVE_DUPLICATES setting in
 * tools configuration file.
 *
 * TRUE will remove duplicate applications.
 */
#define APP_INFO_CONF_DEFAULT_REMOVE_DUPLICATES TRUE

/**
 * Default value for CONFNAME_APPINFO_USE_WMI setting in
 * tools configuration file.
 *
 * TRUE will force the plugin to use WMI for getting
 * the application version information.
 */
#define APP_INFO_CONF_USE_WMI_DEFAULT_VALUE    FALSE

/**
 * Defines the current poll interval (in seconds).
 *
 * This value is controlled by the appinfo.poll-interval config file option.
 */
static guint gAppInfoPollInterval = 0;

/**
 * Defines the state of the App Info at the host side.
 */
static gboolean gAppInfoEnabledInHost = TRUE;

/**
 * AppInfo gather loop timeout source.
 */
static GSource *gAppInfoTimeoutSource = NULL;

static void TweakGatherLoop(ToolsAppCtx *ctx, gboolean force);


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
      g_warning("%s: Error sending RPC message: %s\n", __FUNCTION__,
                reply ? reply : "NULL");
      vm_free(reply);
      return FALSE;
   } else {
      g_info("%s: Successfully sent the app information.\n", __FUNCTION__);
   }

   status = (*reply == '\0');
   vm_free(reply);
   return status;
}


/*
 *----------------------------------------------------------------------
 * AppInfo_GetAppList --
 *
 * Generates the application information list.
 *
 * @param[in] config   Tools configuration dictionary.
 *
 * @retval Pointer to the newly allocated application list. The caller must
 *         free the memory using AppInfoDestroyAppList function.
 *         NULL if any error occurs.
 *
 *----------------------------------------------------------------------
 */

GSList *
AppInfo_GetAppList(GKeyFile *config)     // IN
{
   GSList *appList = NULL;
   int i;
   ProcMgrProcInfoArray *procList = NULL;
   size_t procCount;

#ifdef _WIN32
   Bool useWMI;
#endif

   procList = ProcMgr_ListProcesses();

   if (procList == NULL) {
      g_warning("%s: Failed to get the list of processes.\n", __FUNCTION__);
      return appList;
   }

#ifdef _WIN32
   useWMI =  VMTools_ConfigGetBoolean(config,
                                      CONFGROUPNAME_APPINFO,
                                      CONFNAME_APPINFO_USE_WMI,
                                      APP_INFO_CONF_USE_WMI_DEFAULT_VALUE);

   g_debug("%s: useWMI: %d", __FUNCTION__, useWMI);
#endif

   procCount = ProcMgrProcInfoArray_Count(procList);
   for (i = 0; i < procCount; i++) {
      AppInfo *appInfo;
      ProcMgrProcInfo *procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);
#ifdef _WIN32
      appInfo = AppInfo_GetAppInfo(procInfo, useWMI);
#else
      appInfo = AppInfo_GetAppInfo(procInfo);
#endif
      if (NULL != appInfo) {
         appList = g_slist_prepend(appList, appInfo);
      }
   }

   ProcMgr_FreeProcList(procList);

   return appList;
}


/*
 *****************************************************************************
 * AppInfoGatherTask --
 *
 * Collects all the desired application related information and updates VMX.
 *
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused
 *
 *****************************************************************************
 */

static void
AppInfoGatherTask(ToolsAppCtx *ctx,    // IN
                  gpointer data)       // IN
{
   DynBuf dynBuffer;
   char tmpBuf[1024];
   int len;
   gchar *tstamp = NULL;
   char *escapedCmd = NULL;
   char *escapedVersion = NULL;
   GSList *appList = NULL;
   GSList *appNode;
   static Atomic_uint64 updateCounter = {0};
   uint64 counter = (uint64) Atomic_ReadInc64(&updateCounter) + 1;
   GHashTable *appsAdded = NULL;
   gchar *key = NULL;

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
   gboolean removeDup =
      VMTools_ConfigGetBoolean(ctx->config,
                               CONFGROUPNAME_APPINFO,
                               CONFNAME_APPINFO_REMOVE_DUPLICATES,
                               APP_INFO_CONF_DEFAULT_REMOVE_DUPLICATES);

   DynBuf_Init(&dynBuffer);

   tstamp = VMTools_GetTimeAsString();

   len = Str_Snprintf(tmpBuf, sizeof tmpBuf, headerFmt,
                      APP_INFO_VERSION_1,
                      counter,
                      tstamp != NULL ? tstamp : "");

   if (len < 0) {
      g_warning("%s: Insufficient space for the header.\n", __FUNCTION__);
      goto quit;
   }

   DynBuf_Append(&dynBuffer, tmpBuf, len);

   appList = AppInfo_SortAppList(AppInfo_GetAppList(ctx->config));
   if (removeDup) {
      appsAdded = g_hash_table_new_full(g_str_hash, g_str_equal,
                                        g_free, NULL);
   }

   for (appNode = appList; appNode != NULL; appNode = appNode->next) {
      size_t currentBufferSize = DynBuf_GetSize(&dynBuffer);
      AppInfo *appInfo = (AppInfo *) appNode->data;

      if (appInfo->appName == NULL ||
          appInfo->version == NULL) {
         goto next_entry;
      }

      if (removeDup) {
         key = g_strdup_printf("%s|%s", appInfo->appName, appInfo->version);
         /*
          * If the key already exists, then this app is a duplicate. Free
          * the key and move to the next application.
          */
         if (g_hash_table_contains(appsAdded, key)) {
            goto next_entry;
         }
      }
      escapedCmd = CodeSet_JsonEscape(appInfo->appName);

      if (NULL == escapedCmd) {
         g_warning("%s: Failed to escape the content of cmdName.\n",
                   __FUNCTION__);
         goto quit;
      }

      escapedVersion = CodeSet_JsonEscape(appInfo->version);
      if (NULL == escapedVersion) {
         g_warning("%s: Failed to escape the content of version information.\n",
                   __FUNCTION__);
         goto quit;
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
         g_warning("%s: Insufficient space for the application information.\n",
                   __FUNCTION__);
         goto next_entry;
      }

      if (currentBufferSize + len + sizeof jsonSuffix > MAX_APP_INFO_SIZE) {
         g_warning("%s: Exceeded the max info packet size."
                   " Truncating the rest of the applications.\n",
                   __FUNCTION__);
         break;
      }

      DynBuf_Append(&dynBuffer, tmpBuf, len);
      if (removeDup) {
         g_hash_table_add(appsAdded, key);
         key = NULL;
      }

next_entry:
      g_free(key);
      key = NULL;
      free(escapedCmd);
      escapedCmd = NULL;
      free(escapedVersion);
      escapedVersion = NULL;
   }

   DynBuf_Append(&dynBuffer, jsonSuffix, sizeof jsonSuffix - 1);
   SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, DynBuf_GetString(&dynBuffer));

quit:
   free(escapedCmd);
   free(escapedVersion);
   AppInfo_DestroyAppList(appList);
   if (appsAdded != NULL) {
      g_hash_table_destroy(appsAdded);
   }
   g_free(key);
   g_free(tstamp);
   DynBuf_Destroy(&dynBuffer);
}


/*
 *****************************************************************************
 * AppInfoGather --
 *
 * Creates a new thread that collects all the desired application related
 * information and updates the VMX. Tweaks the poll gather loop as per the
 * tools configuration after creating the thread.
 *
 * @param[in]  data     The application context.
 *
 * @return G_SOURCE_REMOVE to indicate that the timer should be removed.
 *
 *****************************************************************************
 */

static gboolean
AppInfoGather(gpointer data)      // IN
{
   ToolsAppCtx *ctx = data;

   g_debug("%s: Submitting a task to capture application information.\n",
           __FUNCTION__);

   if (!ToolsCorePool_SubmitTask(ctx, AppInfoGatherTask, NULL, NULL)) {
      g_warning("%s: Failed to submit the task for capturing application "
                "information\n", __FUNCTION__);
   }

   TweakGatherLoop(ctx, TRUE);

   return G_SOURCE_REMOVE;
}


/*
 *****************************************************************************
 * TweakGatherLoopEx --
 *
 * Start, stop, reconfigure a AppInfo Gather poll loop.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * AppInfo Gather loop timeout source. The poll loop will be deactivated if
 * the poll interval is 0.
 *
 * @param[in]     ctx           The application context.
 * @param[in]     pollInterval  Poll interval in seconds. A value of 0 will
 *                              deactivate the loop.
 *
 *****************************************************************************
 */

static void
TweakGatherLoopEx(ToolsAppCtx *ctx,       // IN
                  guint pollInterval)     // IN
{
   if (gAppInfoTimeoutSource != NULL) {
      /*
       * Destroy the existing timeout source.
       */
      g_source_destroy(gAppInfoTimeoutSource);
      gAppInfoTimeoutSource = NULL;
   }

   if (pollInterval > 0) {
      if (gAppInfoPollInterval != pollInterval) {
         g_info("%s: New value for %s is %us.\n",
                __FUNCTION__,
                CONFNAME_APPINFO_POLLINTERVAL,
                pollInterval);
      }

      gAppInfoTimeoutSource = g_timeout_source_new(pollInterval * 1000);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, gAppInfoTimeoutSource,
                               AppInfoGather, ctx, NULL);
      g_source_unref(gAppInfoTimeoutSource);
   } else if (gAppInfoPollInterval > 0) {
      g_info("%s: Poll loop for %s deactivated.\n",
             __FUNCTION__, CONFNAME_APPINFO_POLLINTERVAL);
      SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, "");
   }

   gAppInfoPollInterval = pollInterval;
}


/*
 *****************************************************************************
 * TweakGatherLoop --
 *
 * Configues the AppInfo Gather poll loop based on the settings in the
 * tools configuration.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * AppInfo Gather loop timeout source.
 *
 * @param[in]     ctx           The application context.
 * @param[in]     force         If set to TRUE, the poll loop will be
 *                              tweaked even if the poll interval hasn't
 *                              changed from the previous value.
 *
 *****************************************************************************
 */

static void
TweakGatherLoop(ToolsAppCtx *ctx,  // IN
                gboolean force)    // IN
{
   gboolean deactivated =
      VMTools_ConfigGetBoolean(ctx->config,
                               CONFGROUPNAME_APPINFO,
                               CONFNAME_APPINFO_DISABLED,
                               APP_INFO_CONF_DEFAULT_DEACTIVATED_VALUE);

   gint pollInterval;

   if (gAppInfoEnabledInHost && !deactivated) {
      pollInterval = VMTools_ConfigGetInteger(ctx->config,
                                              CONFGROUPNAME_APPINFO,
                                              CONFNAME_APPINFO_POLLINTERVAL,
                                              APP_INFO_POLL_INTERVAL);

      if (pollInterval < 0 || pollInterval > (G_MAXINT / 1000)) {
         g_warning("%s: Invalid poll interval %d. Using default %us.\n",
                   __FUNCTION__, pollInterval, APP_INFO_POLL_INTERVAL);
         pollInterval = APP_INFO_POLL_INTERVAL;
      }
   } else {
      pollInterval = 0;
   }

   if (force || (gAppInfoPollInterval != pollInterval)) {
      /*
       * pollInterval can never be a negative value. Typecasting into
       * guint should not be a problem.
       */
      TweakGatherLoopEx(ctx, (guint) pollInterval);
   }
}


/*
 *****************************************************************************
 * AppInfoServerConfReload --
 *
 * Reconfigures the poll loop interval upon config file reload.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused.
 *
 *****************************************************************************
 */

static void
AppInfoServerConfReload(gpointer src,       // IN
                        ToolsAppCtx *ctx,   // IN
                        gpointer data)      // IN
{
   g_info("%s: Reloading the tools configuration.\n", __FUNCTION__);

   TweakGatherLoop(ctx, FALSE);
}


/*
 *****************************************************************************
 * AppInfoServerShutdown --
 *
 * Cleanup internal data on shutdown.
 *
 * @param[in]  src     The source object.
 * @param[in]  ctx     Application context.
 * @param[in]  data    Unused.
 *
 *****************************************************************************
 */

static void
AppInfoServerShutdown(gpointer src,          // IN
                      ToolsAppCtx *ctx,      // IN
                      gpointer data)         // IN
{
   if (gAppInfoTimeoutSource != NULL) {
      g_source_destroy(gAppInfoTimeoutSource);
      gAppInfoTimeoutSource = NULL;
   }

   SetGuestInfo(ctx, APP_INFO_GUESTVAR_KEY, "");
}


/*
 *----------------------------------------------------------------------------
 *
 * AppInfoServerSetOption --
 *
 * Handle TOOLSOPTION_ENABLE_APPINFO Set_Option callback.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  option   Option being set.
 * @param[in]  value    Option value.
 * @param[in]  plugin   Plugin registration data.
 *
 * @return  TRUE  if the specified option is TOOLSOPTION_ENABLE_APPINFO and
 *                the AppInfo Gather poll loop is reconfigured.
 *          FALSE if the specified option is not TOOLSOPTION_ENABLE_APPINFO
 *                or AppInfo Gather poll loop is not reconfigured.
 *----------------------------------------------------------------------------
 */

static gboolean
AppInfoServerSetOption(gpointer src,         // IN
                       ToolsAppCtx *ctx,     // IN
                       const gchar *option,  // IN
                       const gchar *value,   // IN
                       gpointer data)        // IN
{
   gboolean retVal = FALSE;

   if (strcmp(option, TOOLSOPTION_ENABLE_APPINFO) == 0) {
      g_debug("%s: Tools set option %s=%s.\n",
              __FUNCTION__, TOOLSOPTION_ENABLE_APPINFO, value);

      if (strcmp(value, "1") == 0 && !gAppInfoEnabledInHost) {
         gAppInfoEnabledInHost = TRUE;
         retVal = TRUE;
      } else if (strcmp(value, "0") == 0 && gAppInfoEnabledInHost) {
         gAppInfoEnabledInHost = FALSE;
         retVal = TRUE;
      }

      if (retVal) {
         g_info("%s: State of AppInfo is changed to '%s' at host side.\n",
                __FUNCTION__, gAppInfoEnabledInHost ? "enabled" : "deactivated" );

         TweakGatherLoop(ctx, TRUE);
      }
   }

   return retVal;
}


/*
 ******************************************************************************
 * AppInfoServerReset --
 *
 * Callback function that gets called whenever the RPC channel gets reset.
 * Disables the poll loop and sets a one time poll.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Application context.
 * @param[in]  data     Unused.
 *
 ******************************************************************************
 */

static void
AppInfoServerReset(gpointer src,
                   ToolsAppCtx *ctx,
                   gpointer data)
{
   /*
    * gAppInfoTimeoutSource is used to figure out if the poll loop is
    * enabled or not. If the poll loop is deactivated, then
    * gAppInfoTimeoutSource will be set to NULL.
    */
   if (gAppInfoTimeoutSource != NULL) {
      guint interval;

      ASSERT(gAppInfoPollInterval != 0);

#define MIN_APPINFO_INTERVAL 30

      if (gAppInfoPollInterval > MIN_APPINFO_INTERVAL) {
         GRand *gRand = g_rand_new();

         /*
          * The RPC channel may get reset due to various conditions like
          * snapshotting the VM, vmotion the VM, instant cloning of the VM.
          * In order to avoid potential load spikes in case of instant clones,
          * randomize the poll interval after a channel reset.
          */

         interval = g_rand_int_range(gRand,
                                     MIN_APPINFO_INTERVAL,
                                     gAppInfoPollInterval);
         g_rand_free(gRand);
      } else {
         interval = gAppInfoPollInterval;
      }

#undef MIN_APPINFO_INTERVAL

      g_info("%s: Using poll interval: %u.\n", __FUNCTION__, interval);

      TweakGatherLoopEx(ctx, interval);
   } else {
      /*
       * Channel got reset. VM might have vMotioned to an older host
       * that doesn't send the 'Set_Option enableAppInfo'.
       * Set gAppInfoEnabledInHost to TRUE and tweak the gather loop.
       * Else, the application information may never be captured.
       */
      if (!gAppInfoEnabledInHost) {
         gAppInfoEnabledInHost = TRUE;
         TweakGatherLoop(ctx, TRUE);
      } else {
         g_debug("%s: Poll loop deactivated. Ignoring.\n", __FUNCTION__);
      }
   }
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
ToolsOnLoad(ToolsAppCtx *ctx)    // IN
{
   static ToolsPluginData regData = {
      "appInfo",
      NULL,
      NULL
   };

   /*
    * Return NULL to deactivate the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("%s: Not running in a VMware VM.\n", __FUNCTION__);
      return NULL;
   }

   /*
    * Return NULL to deactivate the plugin if not running in vmsvc daemon.
    */
   if (!TOOLS_IS_MAIN_SERVICE(ctx)) {
      g_info("%s: Not running in vmsvc daemon: container name='%s'.\n",
             __FUNCTION__, ctx->name);
      return NULL;
   }

   /*
    * This plugin is useless without an RpcChannel.  If we don't have one,
    * just bail.
    */
   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CONF_RELOAD, AppInfoServerConfReload, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, AppInfoServerShutdown, NULL },
         { TOOLS_CORE_SIG_RESET, AppInfoServerReset, NULL },
         { TOOLS_CORE_SIG_SET_OPTION, AppInfoServerSetOption, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs))
         }
      };

      regData.regs = VMTools_WrapArray(regs,
                                       sizeof *regs,
                                       ARRAYSIZE(regs));

      /*
       * Set up the AppInfo gather loop.
       */
      TweakGatherLoop(ctx, TRUE);

      return &regData;
   }

   return NULL;
}
