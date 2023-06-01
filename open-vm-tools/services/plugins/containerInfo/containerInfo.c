/*********************************************************
 * Copyright (c) 2021-2022 VMware, Inc. All rights reserved.
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
 * containerInfo.c --
 *
 *      Captures the information about running containers inside the guest
 *      and publishes it to a guest variable.
 */

#ifndef __linux__
#   error This file should not be compiled.
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "containerInfoInt.h"
#include "codeset.h"
#include "procMgr.h"
#include "str.h"
#include "strutil.h"
#include "conf.h"
#include "util.h"
#include "vm_atomic.h"
#include "vmware/guestrpc/containerInfo.h"
#include "vmware/guestrpc/tclodefs.h"
#include "vmware/tools/log.h"
#include "vmware/tools/threadPool.h"

#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);

/**
 * Default poll interval for containerInfo is 6 hours
 */
#define CONTAINERINFO_DEFAULT_POLL_INTERVAL (6 * 60 * 60)

/**
 * Name of the containerd process. This is used to figure
 * out if the containerd process is running in the list
 * of processes.
 */
#define CONTAINERD_PROCESS_NAME "containerd"

/**
 * Default value for containerinfo query-limit conf key.
 */
#define CONTAINERINFO_DEFAULT_CONTAINER_MAX 256

/**
 * Default value for CONFNAME_CONTAINERINFO_REMOVE_DUPLICATES setting in
 * tools configuration file.
 *
 * TRUE will remove duplicate containers.
 */
#define CONTAINERINFO_DEFAULT_REMOVE_DUPLICATES TRUE

/**
 * Default value for containerd-unix-socket conf key.
 */
#define CONTAINERINFO_DEFAULT_CONTAINERDSOCKET "/run/containerd/containerd.sock"

/**
 * Default value for docker-unix-socket conf key.
 */
#define CONTAINERINFO_DEFAULT_DOCKER_SOCKET "/var/run/docker.sock"

/**
 * Default value for allowed-namespaces conf key.
 */
#define CONTAINERINFO_DEFAULT_ALLOWED_NAMESPACES "moby,k8s.io,default"

/**
 * Name of the 'moby' namespace used by docker.
 */
#define CONTAINERINFO_DOCKER_NAMESPACE_NAME "moby"

/**
 * Maximum size of the guestinfo packet that holds the containerinfo
 * information.
 */
#define CONTAINERINFO_MAX_GUESTINFO_PACKET_SIZE (63 * 1024)

/**
 * Defines current containerinfo poll interval (in seconds).
 *
 * Controlled by containerinfo.poll-interval config file option.
 */
static Atomic_uint32 gContainerInfoPollInterval = { 0 };

/**
 * ContainerInfo gather loop timeout source.
 */
static GSource *gContainerInfoTimeoutSource = NULL;

/**
 * ContainerInfo and AppInfo share the same host side switch so this
 * defines the state of the AppInfo at the host side.
 */
static gboolean gAppInfoEnabledInHost = TRUE;

/**
 * Defines whether task is currently in progress.  libcurl initialization
 * is not thread safe so this atomic bool will ensure only one task is
 * running at a time.
 */
static Atomic_Bool gTaskSubmitted = { FALSE }; // Task has not been submitted.

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
 *****************************************************************************
 */

static void
SetGuestInfo(ToolsAppCtx *ctx,                // IN
             const char *guestVariableName,   // IN
             const char *value)               // IN
{
   char *reply = NULL;
   gchar *msg;
   size_t replyLen;

   ASSERT(guestVariableName != NULL);
   ASSERT(value != NULL);

   msg = g_strdup_printf("info-set guestinfo.%s %s",
                         guestVariableName,
                         value);

   if (!RpcChannel_Send(ctx->rpc,
                        msg,
                        strlen(msg) + 1,
                        &reply,
                        &replyLen)) {
      g_warning("%s: Error sending RPC message: %s\n", __FUNCTION__,
                VM_SAFE_STR(reply));
   } else {
      g_info("%s: Successfully published the container information.\n",
             __FUNCTION__);
   }

   g_free(msg);
   vm_free(reply);
   return;
}


/*
 *****************************************************************************
 * CheckContainerdRunning --
 *
 * When containers are run, the containerd-shim process gets called.
 * This function checks if containerd process exists in the list of processes,
 * which will signal that the containerinfo loop should be started.
 *
 * @retval TRUE        found containerd process.
 * @retval FALSE       not found.
 *
 *****************************************************************************
 */

static gboolean
CheckContainerdRunning(void)
{
   ProcMgrProcInfoArray *procList;
   size_t procCount;
   int i;
   gboolean result = FALSE;

   procList = ProcMgr_ListProcesses();
   if (procList == NULL) {
      g_warning("%s: Failed to get the list of processes.\n",
                __FUNCTION__);
      return result;
   }

   procCount = ProcMgrProcInfoArray_Count(procList);
   for (i = 0; i < procCount; i++) {
      ProcMgrProcInfo *procInfo = ProcMgrProcInfoArray_AddressOf(procList, i);
      if (procInfo->procCmdName != NULL &&
          strstr(procInfo->procCmdName, CONTAINERD_PROCESS_NAME)) {
         result = TRUE;
         break;
      }
   }

   ProcMgr_FreeProcList(procList);
   return result;
}


/*
 ******************************************************************************
 * ContainerInfo_DestroyContainerData --
 *
 * Free function for container data. This function is called by the glib
 * for each element in the container list while freeing.
 *
 * @param[in] data      Pointer to the container data.
 *
 * @retval NONE
 *
 ******************************************************************************
 */

void
ContainerInfo_DestroyContainerData(void *pointer)
{
   ContainerInfo *info = pointer;

   if (info == NULL) {
      return;
   }

   g_free(info->id);
   g_free(info->image);
   g_free(info);
}


/*
 *****************************************************************************
 * ContainerInfo_DestroyContainerList --
 *
 * Frees the entire memory allocated for the container list.
 *
 * @param[in]  containerList     Pointer to the container list.
 *
 * @retval NONE
 *
 *****************************************************************************
 */

void
ContainerInfo_DestroyContainerList(GSList *containerList)
{
   if (containerList == NULL) {
      return;
   }

   g_slist_free_full(containerList, ContainerInfo_DestroyContainerData);
}


/*
 *****************************************************************************
 * ContainerInfoGetNsJson --
 *
 * Iterates through the list of containers and prepares the JSON string for
 * a specified namespace. The caller must free the resulting JSON string.
 *
 * @param[in]  ns                The name of the namespace
 * @param[in]  containerList     The list of the running containers
 * @param[in]  dockerSocketPath  The path to the unix socket used by docker.
 * @param[in]  removeDuplicates  Remove duplicate containers from the output.
 * @param[in]  maxSize           Maximum size of the JSON output
 * @param[out] resultJson        JSON string that is prepared.
 *
 * @retVal The size of the JSON string returned.
 *
 *****************************************************************************
 */

size_t
ContainerInfoGetNsJson(const char *ns,                 // IN
                       GSList *containerList,          // IN
                       const char *dockerSocketPath,   // IN
                       gboolean removeDuplicates,      // IN
                       unsigned int maxSize,           // IN
                       char **resultJson)              // OUT
{
   static const char headerFmt[] = "\"%s\": [";
   static const char footer[] = "]";

   GSList *info;
   gboolean nodeAdded;
   DynBuf dynBuffer;
   size_t resultSize = 0;
   GHashTable *dockerContainerTable = NULL;
   GHashTable *imagesAdded = NULL;
   gchar *escapedImageName = NULL;

   ASSERT(resultJson != NULL);

   DynBuf_Init(&dynBuffer);
   StrUtil_SafeDynBufPrintf(&dynBuffer, headerFmt, ns);

   nodeAdded = FALSE;

   /*
    * The image name may not be set for containers managed by docker.
    * To handle such cases, get the list of containers using Docker APIs.
    */
   if (strcmp(ns, CONTAINERINFO_DOCKER_NAMESPACE_NAME) == 0) {
      dockerContainerTable =
         ContainerInfo_GetDockerContainers(dockerSocketPath);
   }

   if (removeDuplicates) {
      imagesAdded = g_hash_table_new_full(g_str_hash, g_str_equal,
                                          g_free, NULL);
   }

   for (info = containerList; info != NULL; info = info->next) {
      static const char *nodeFmt = "%s{\""
                                   CONTAINERINFO_KEY_IMAGE
                                   "\":\"%s\"}";
      size_t currentBufferSize = DynBuf_GetSize(&dynBuffer);
      gchar *tmpNode;
      size_t len;
      ContainerInfo *node = (ContainerInfo *) info->data;

      g_free(escapedImageName);
      escapedImageName = NULL;

      if (node->image == NULL || node->image[0] == '\0') {
         const char *newImage = NULL;

         if (dockerContainerTable != NULL) {
            newImage = g_hash_table_lookup(dockerContainerTable, node->id);
         }

         if (newImage != NULL) {
            escapedImageName = g_strdup(newImage);
         } else {
            g_warning("%s: Skipping '%s' since image name couldn't "
                      "be retrieved.\n", __FUNCTION__, node->id);
            continue;
         }
      } else {
         escapedImageName = CodeSet_JsonEscape(node->image);
         if (NULL == escapedImageName) {
            g_warning("%s: Failed to escape the image. Skipping '%s'\n",
                      __FUNCTION__, node->id);
            continue;
         }
      }

      if (removeDuplicates) {
         /*
          * Check if the container was already added. If already added, just
          * skip to the next container.
          */
         if (g_hash_table_contains(imagesAdded, escapedImageName)) {
            continue;
         }
      }

      tmpNode = Str_Asprintf(&len, nodeFmt,
                             nodeAdded ? "," : "", escapedImageName);
      if (tmpNode == NULL) {
         g_warning("%s: Out of memory. Skipping '%s'\n",
                   __FUNCTION__, node->id);
         break;
      }

      if (currentBufferSize + len + sizeof footer > maxSize) {
         g_warning("%s: Skipping '%s' due to insufficient size.\n",
                   __FUNCTION__, node->id);
      } else {
         if (removeDuplicates) {
            g_hash_table_add(imagesAdded, escapedImageName);
            escapedImageName = NULL;
         }
         DynBuf_Append(&dynBuffer, tmpNode, len);
         nodeAdded = TRUE;
      }
      g_free(tmpNode);
   }

   if (nodeAdded) {
      DynBuf_Append(&dynBuffer, footer, strlen(footer));
      resultSize = DynBuf_GetSize(&dynBuffer);
      *resultJson = DynBuf_DetachString(&dynBuffer);
   } else {
      resultSize = 0;
      *resultJson = NULL;
   }

   g_free(escapedImageName);

   if (imagesAdded != NULL) {
      g_hash_table_destroy(imagesAdded);
   }

   if (dockerContainerTable != NULL) {
      g_hash_table_destroy(dockerContainerTable);
   }

   DynBuf_Destroy(&dynBuffer);
   return resultSize;
}


/*
 *****************************************************************************
 * ContainerInfoGatherTask --
 *
 * Collects all the desired container related information.
 *
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused
 *
 *****************************************************************************
 */

static void
ContainerInfoGatherTask(ToolsAppCtx *ctx,   // IN
                        gpointer data)      // IN
{
   gchar *timeStampString = NULL;
   int limit;
   gint64 startInfoGatherTime;
   gint64 endInfoGatherTime;
   gchar *containerdSocketPath = NULL;
   gchar *nsConfValue = NULL;
   gchar **nsList;
   static Atomic_uint64 updateCounter = {1};
   uint64 counter;
   int i;
   DynBuf dynBuffer;
   gchar tmpBuf[256];
   size_t len;
   gboolean nsAdded;
   char *dockerSocketPath = NULL;
   GHashTable *nsParsed;
   gboolean removeDuplicates;

   static char headerFmt[] = "{"
                     "\"" CONTAINERINFO_KEY_VERSION  "\":\"%d\","
                     "\"" CONTAINERINFO_KEY_UPDATE_COUNTER "\":%"FMT64"u,"
                     "\"" CONTAINERINFO_KEY_PUBLISHTIME    "\":\"%s\","
                     "\"" CONTAINERINFO_KEY "\":{";
   static char footer[] = "}}";

   if (Atomic_ReadIfEqualWriteBool(&gTaskSubmitted, FALSE, TRUE)) {
      g_info("%s: Previously submitted task is not completed\n", __FUNCTION__);
      return;
   }

   timeStampString = VMTools_GetTimeAsString();
   counter = (uint64) Atomic_ReadInc64(&updateCounter);

   DynBuf_Init(&dynBuffer);
   len = Str_Snprintf(tmpBuf, sizeof tmpBuf,
                      headerFmt,
                      CONTAINERINFO_VERSION_1,
                      counter,
                      (timeStampString != NULL) ? timeStampString : "");
   ASSERT(len > 0);

   DynBuf_Append(&dynBuffer, tmpBuf, len);

   if (!CheckContainerdRunning()) {
      g_info("%s: Could not find running containerd process on the system.\n",
             __FUNCTION__);
      goto exit;
   }

   limit =
      VMTools_ConfigGetInteger(ctx->config,
                               CONFGROUPNAME_CONTAINERINFO,
                               CONFNAME_CONTAINERINFO_LIMIT,
                               CONTAINERINFO_DEFAULT_CONTAINER_MAX);

   if (limit < 1) {
      g_warning("%s: invalid max-containers %d. Using default %d.\n",
                __FUNCTION__,
                limit,
                CONTAINERINFO_DEFAULT_CONTAINER_MAX);
      limit = CONTAINERINFO_DEFAULT_CONTAINER_MAX;
   }

   nsConfValue =
      VMTools_ConfigGetString(ctx->config,
                              CONFGROUPNAME_CONTAINERINFO,
                              CONFNAME_CONTAINERINFO_ALLOWED_NAMESPACES,
                              CONTAINERINFO_DEFAULT_ALLOWED_NAMESPACES);
   g_strstrip(nsConfValue);

   if (nsConfValue[0] == '\0') {
      g_warning("%s: Empty value found for %s.%s key. Ignoring.",
                __FUNCTION__, CONFGROUPNAME_CONTAINERINFO,
                CONFNAME_CONTAINERINFO_ALLOWED_NAMESPACES);
      goto exit;
   }

   containerdSocketPath =
      VMTools_ConfigGetString(ctx->config,
                              CONFGROUPNAME_CONTAINERINFO,
                              CONFNAME_CONTAINERINFO_CONTAINERDSOCKET,
                              CONTAINERINFO_DEFAULT_CONTAINERDSOCKET);
   g_strstrip(containerdSocketPath);

   dockerSocketPath =
      VMTools_ConfigGetString(ctx->config,
                              CONFGROUPNAME_CONTAINERINFO,
                              CONFNAME_CONTAINERINFO_DOCKERSOCKET,
                              CONTAINERINFO_DEFAULT_DOCKER_SOCKET);
   g_strstrip(dockerSocketPath);

   if (dockerSocketPath[0] == '\0') {
      g_warning("%s: Empty value found for %s.%s key. Using default %s.",
                __FUNCTION__, CONFGROUPNAME_CONTAINERINFO,
                CONFNAME_CONTAINERINFO_DOCKERSOCKET,
                CONTAINERINFO_DEFAULT_DOCKER_SOCKET);
      g_free(dockerSocketPath);
      dockerSocketPath = g_strdup(CONTAINERINFO_DEFAULT_DOCKER_SOCKET);
   }

   removeDuplicates =
      VMTools_ConfigGetBoolean(ctx->config,
                               CONFGROUPNAME_CONTAINERINFO,
                               CONFNAME_CONTAINERINFO_REMOVE_DUPLICATES,
                               CONTAINERINFO_DEFAULT_REMOVE_DUPLICATES);

   startInfoGatherTime = g_get_monotonic_time();

   nsList = g_strsplit(nsConfValue, ",", 0);
   nsAdded = FALSE;
   nsParsed = g_hash_table_new(g_str_hash, g_str_equal);

   for (i = 0; nsList[i] != NULL; i++) {
      size_t currentBufferSize = DynBuf_GetSize(&dynBuffer);
      size_t maxSizeRemaining = CONTAINERINFO_MAX_GUESTINFO_PACKET_SIZE -
                                currentBufferSize - sizeof(footer);

      gchar *nsJsonString;
      size_t nsJsonSize;
      GSList *containerList;

      g_strstrip(nsList[i]);
      if (nsList[i][0] == '\0') {
         g_warning("%s: Empty value found for the namespace. Skipping.",
                   __FUNCTION__);
         continue;
      }

      if (g_hash_table_contains(nsParsed, nsList[i])) {
         g_debug("%s: Skipping the duplicate namespace: %s",
                 __FUNCTION__, nsList[i]);
         continue;
      }

      if (nsAdded) {
         maxSizeRemaining--; // Minus size of ','
      }

      if (maxSizeRemaining == 0 ||
          maxSizeRemaining > CONTAINERINFO_MAX_GUESTINFO_PACKET_SIZE) {
         break;
      }

      containerList =
         ContainerInfo_GetContainerList(nsList[i], containerdSocketPath,
                                        (unsigned int) limit);
      g_hash_table_add(nsParsed, nsList[i]);
      if (containerList == NULL) {
         continue;
      }

      nsJsonSize = ContainerInfoGetNsJson(nsList[i], containerList,
                   dockerSocketPath, removeDuplicates, maxSizeRemaining,
                   &nsJsonString);
      if (nsJsonSize > 0 && nsJsonSize <= maxSizeRemaining) {
         if (nsAdded) {
            DynBuf_Append(&dynBuffer, ",", 1);
         }
         DynBuf_Append(&dynBuffer, nsJsonString, nsJsonSize);
         nsAdded = TRUE;
      }
      g_free(nsJsonString);
      ContainerInfo_DestroyContainerList(containerList);
   }

   g_hash_table_destroy(nsParsed);
   g_strfreev(nsList);

   endInfoGatherTime = g_get_monotonic_time();

   g_info("%s: time to complete containerInfo gather = %" G_GINT64_FORMAT " us\n",
          __FUNCTION__, endInfoGatherTime - startInfoGatherTime);

exit:
   if (Atomic_Read32(&gContainerInfoPollInterval) == 0) {
      /*
       * If gatherLoop is disabled then make sure this thread
       * did not overwrite the guestVar. The guestVar should be
       * cleared out in this case.
       */
      SetGuestInfo(ctx, CONTAINERINFO_GUESTVAR_KEY, "");
   } else {
      DynBuf_Append(&dynBuffer, footer, sizeof(footer));
      SetGuestInfo(ctx,
                   CONTAINERINFO_GUESTVAR_KEY,
                   DynBuf_GetString(&dynBuffer));
   }

   DynBuf_Destroy(&dynBuffer);
   g_free(dockerSocketPath);
   g_free(containerdSocketPath);
   g_free(nsConfValue);
   g_free(timeStampString);
   Atomic_WriteBool(&gTaskSubmitted, FALSE);
}


/*
 *****************************************************************************
 * ContainerInfoGather --
 *
 * Creates a new thread that collects all the desired container related
 * information and updates the VMX. Tweaks the poll gather loop as per the
 * tools configuration after creating the thread.
 *
 * @param[in]  data     The application context.
 *
 * @retval  G_SOURCE_REMOVE to indicate that the timer should be removed.
 *
 *****************************************************************************
 */

static gboolean
ContainerInfoGather(gpointer data)   // IN
{
   ToolsAppCtx *ctx = data;

   g_debug("%s: Submitting a task to capture container information.\n",
           __FUNCTION__);

   if (!ToolsCorePool_SubmitTask(ctx, ContainerInfoGatherTask, NULL, NULL)) {
      g_warning("%s: Failed to submit the task for capturing container "
                "information\n", __FUNCTION__);
   }

   TweakGatherLoop(ctx, TRUE);

   return G_SOURCE_REMOVE;
}


/*
 *****************************************************************************
 * TweakGatherLoopEx --
 *
 * Start, stop, reconfigure a ContainerInfo Gather poll loop.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * ContainerInfo Gather loop timeout source. The poll loop will be disabled if
 * the poll interval is 0.
 *
 * @param[in]     ctx           The application context.
 * @param[in]     pollInterval  Poll interval in seconds. A value of 0 will
 *                              disable the loop.
 *
 *****************************************************************************
 */

static void
TweakGatherLoopEx(ToolsAppCtx *ctx,     // IN
                  guint pollInterval)   // IN
{
   if (gContainerInfoTimeoutSource != NULL) {
      /*
       * Destroy the existing timeout source.
       */
      g_source_destroy(gContainerInfoTimeoutSource);
      gContainerInfoTimeoutSource = NULL;
   }

   if (pollInterval > 0) {
      if (Atomic_Read32(&gContainerInfoPollInterval) != pollInterval) {
         g_info("%s: New value for %s is %us.\n",
                __FUNCTION__,
                CONFNAME_CONTAINERINFO_POLLINTERVAL,
                pollInterval);
      }

      gContainerInfoTimeoutSource = g_timeout_source_new(pollInterval * 1000);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, gContainerInfoTimeoutSource,
                               ContainerInfoGather, ctx, NULL);
      g_source_unref(gContainerInfoTimeoutSource);
      Atomic_Write32(&gContainerInfoPollInterval, pollInterval);
   } else if (Atomic_Read32(&gContainerInfoPollInterval) > 0) {
      g_info("%s: Poll loop for %s disabled.\n",
             __FUNCTION__, CONFNAME_CONTAINERINFO_POLLINTERVAL);
      Atomic_Write32(&gContainerInfoPollInterval, 0);
      SetGuestInfo(ctx, CONTAINERINFO_GUESTVAR_KEY, "");
   }
}


/*
 *****************************************************************************
 * TweakGatherLoop --
 *
 * Configures the ContainerInfo Gather poll loop based on the settings in the
 * tools configuration.
 *
 * This function is responsible for creating, manipulating, and resetting a
 * ContainerInfo Gather loop timeout source.
 *
 * @param[in]     ctx           The application context.
 * @param[in]     force         If set to TRUE, the poll loop will be
 *                              tweaked even if the poll interval hasn't
 *                              changed from the previous value.
 *
 *****************************************************************************
 */

static void
TweakGatherLoop(ToolsAppCtx *ctx,   // IN
                gboolean force)     // IN
{
   gint pollInterval;

   if (gAppInfoEnabledInHost) {
      pollInterval =
         VMTools_ConfigGetInteger(ctx->config,
                                  CONFGROUPNAME_CONTAINERINFO,
                                  CONFNAME_CONTAINERINFO_POLLINTERVAL,
                                  CONTAINERINFO_DEFAULT_POLL_INTERVAL);

      if (pollInterval < 0 || pollInterval > (G_MAXINT / 1000)) {
         g_warning("%s: Invalid poll interval %d. Using default %us.\n",
                   __FUNCTION__, pollInterval,
                   CONTAINERINFO_DEFAULT_POLL_INTERVAL);
         pollInterval = CONTAINERINFO_DEFAULT_POLL_INTERVAL;
      }
   } else {
      pollInterval = 0;
   }

   if (force || (Atomic_Read32(&gContainerInfoPollInterval) != pollInterval)) {
      /*
       * pollInterval can never be a negative value. Typecasting into
       * guint should not be a problem.
       */
      TweakGatherLoopEx(ctx, (guint) pollInterval);
   }
}


/*
 *****************************************************************************
 * ContainerInfoServerConfReload --
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
ContainerInfoServerConfReload(gpointer src,       // IN
                              ToolsAppCtx *ctx,   // IN
                              gpointer data)      // IN
{
   g_info("%s: Reloading the tools configuration.\n", __FUNCTION__);

   TweakGatherLoop(ctx, FALSE);
}


/*
 *****************************************************************************
 * ContainerInfoServerShutdown --
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
ContainerInfoServerShutdown(gpointer src,       // IN
                            ToolsAppCtx *ctx,   // IN
                            gpointer data)      // IN
{
   if (gContainerInfoTimeoutSource != NULL) {
      g_source_destroy(gContainerInfoTimeoutSource);
      gContainerInfoTimeoutSource = NULL;
   }

   SetGuestInfo(ctx, CONTAINERINFO_GUESTVAR_KEY, "");
}


/*
 *----------------------------------------------------------------------------
 *
 * ContainerInfoServerSetOption --
 *
 * Handle TOOLSOPTION_ENABLE_APPINFO Set_Option callback. This callback is
 * necessary because containerInfo shares AppInfo's host side switch.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  option   Option being set.
 * @param[in]  value    Option value.
 * @param[in]  plugin   Plugin registration data.
 *
 * @retval  TRUE  if the specified option is TOOLSOPTION_ENABLE_APPINFO and
 *                the containerInfo Gather poll loop is reconfigured.
 * @retval  FALSE if the specified option is not TOOLSOPTION_ENABLE_APPINFO
 *                or containerInfo Gather poll loop is not reconfigured.
 *----------------------------------------------------------------------------
 */

static gboolean
ContainerInfoServerSetOption(gpointer src,          // IN
                             ToolsAppCtx *ctx,      // IN
                             const gchar *option,   // IN
                             const gchar *value,    // IN
                             gpointer data)         // IN
{
   gboolean stateChanged = FALSE;

   if (strcmp(option, TOOLSOPTION_ENABLE_APPINFO) == 0) {
      g_debug("%s: Tools set option %s=%s.\n",
              __FUNCTION__, TOOLSOPTION_ENABLE_APPINFO, value);

      if (strcmp(value, "1") == 0 && !gAppInfoEnabledInHost) {
         gAppInfoEnabledInHost = TRUE;
         stateChanged = TRUE;
      } else if (strcmp(value, "0") == 0 && gAppInfoEnabledInHost) {
         gAppInfoEnabledInHost = FALSE;
         stateChanged = TRUE;
      }

      if (stateChanged) {
         g_info("%s: State of AppInfo is changed to '%s' at host side.\n",
                __FUNCTION__, gAppInfoEnabledInHost ? "enabled" : "disabled");
         TweakGatherLoop(ctx, TRUE);
      }
   }

   return stateChanged;
}


/*
 *****************************************************************************
 * ContainerInfoServerReset --
 *
 * Callback function that gets called whenever the RPC channel gets reset.
 * Disable the current timer and set a timer for random interval; after that
 * interval, the timer will be resumed at the standard interval.
 * The one time random interval is intended to avoid the possibility that the
 * containerinfo plugin might run at the same time in a collection of
 * VMs - such as might be created by instant clone - which could in turn cause
 * a load spike on the host.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Application context.
 * @param[in]  data     Unused.
 *
 *****************************************************************************
 */

static void
ContainerInfoServerReset(gpointer src,       // IN
                         ToolsAppCtx *ctx,   // IN
                         gpointer data)      // IN
{
   /*
    * Handle reset for containerinfo loop.
    */
   if (gContainerInfoTimeoutSource != NULL) {
      guint interval;

      ASSERT(Atomic_Read32(&gContainerInfoPollInterval) != 0);

#define MIN_CONTAINERINFO_INTERVAL 30

      if (Atomic_Read32(&gContainerInfoPollInterval) >
          MIN_CONTAINERINFO_INTERVAL) {
         GRand *gRand = g_rand_new();

         interval = g_rand_int_range(gRand,
                           MIN_CONTAINERINFO_INTERVAL,
                           Atomic_Read32(&gContainerInfoPollInterval));
         g_rand_free(gRand);
      } else {
         interval = Atomic_Read32(&gContainerInfoPollInterval);
      }

#undef MIN_CONTAINERINFO_INTERVAL

      g_info("%s: Using poll interval for containerinfo loop: %u.\n",
             __FUNCTION__, interval);

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
         g_debug("%s: Poll loop disabled. Ignoring.\n", __FUNCTION__);
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
 * @retval  The registration data.
 *
 *****************************************************************************
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)   // IN
{
   static ToolsPluginData regData = {
      "containerInfo",
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

   /*
    * This plugin is useless without an RpcChannel.  If we don't have one,
    * just bail.
    */
   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_CONF_RELOAD, ContainerInfoServerConfReload, NULL },
         { TOOLS_CORE_SIG_SHUTDOWN, ContainerInfoServerShutdown, NULL },
         { TOOLS_CORE_SIG_RESET, ContainerInfoServerReset, NULL },
         { TOOLS_CORE_SIG_SET_OPTION, ContainerInfoServerSetOption, NULL }
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
       * Set up the containerInfo gather loop.
       */
      TweakGatherLoop(ctx, TRUE);

      return &regData;
   }

   return NULL;
}
