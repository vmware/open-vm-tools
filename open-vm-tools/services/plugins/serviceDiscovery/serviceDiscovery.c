/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * serviceDiscovery.c --
 *
 * Captures the information about services inside the guest
 * and writes it to Namespace DB.
 */

#include <string.h>

#include "serviceDiscovery.h"
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

#include "vmware/tools/guestrpc.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

#define NSDB_PRIV_GET_VALUES_CMD "namespace-priv-get-values"
#define NSDB_PRIV_SET_KEYS_CMD "namespace-priv-set-keys"

/*
 * Namespace DB used for service discovery
 */
#define SERVICE_DISCOVERY_NAMESPACE_DB_NAME "com.vmware.vrops.sdmp"

/*
 * ready - used to identify if data is succesfully written to Namespace DB
 * signal - signal send by sdmp client for plugin to start data collection
 */
#define SERVICE_DISCOVERY_KEY_NAME_READY "ready"
#define SERVICE_DISCOVERY_KEY_NAME_SIGNAL "signal"

#if defined (_WIN32)

/*
 * keys for types of service data collected by plugin from Windows guest
 */
#define WIN_KEY_NAME_PROCESSES "listening-process-info"
#define WIN_KEY_NAME_CONNECTIONS "connection-info"
#define WIN_KEY_NAME_PERFORMANCE_METRICS "listening-process-perf-metrics"
#define WIN_KEY_NAME_VERSIONS "versions"
#define WIN_KEY_NAME_RELATIONSHIP "pid-to-ppid"
#define WIN_KEY_NAME_NET "net"
#define WIN_KEY_NAME_IIS_PORTS "iis-ports-info"

/*
 * scripts used by plugin to collect from Windows guest
 */
#define WIN_SCRIPT_NAME_PROCESSES "get-listening-process-info.bat"
#define WIN_SCRIPT_NAME_CONNECTIONS "get-connection-info.bat"
#define WIN_SCRIPT_NAME_PERFORMANCE_METRICS "get-performance-metrics.bat"
#define WIN_SCRIPT_NAME_VERSIONS "get-versions.bat"
#define WIN_SCRIPT_NAME_RELATIONSHIP "get-parent-child-rels.bat"
#define WIN_SCRIPT_NAME_NET "net-share.bat"
#define WIN_SCRIPT_NAME_IIS_PORTS "get-iis-ports-info.bat"

#else

/*
 * keys for types of service data collected by plugin from Linux guest
 */
#define LIN_KEY_NAME_PROCESSES "listening-process-info"
#define LIN_KEY_NAME_CONNECTIONS "connection-info"
#define LIN_KEY_NAME_PERFORMANCE_METRICS "listening-process-perf-metrics"
#define LIN_KEY_NAME_VERSIONS "versions"

/*
 * scripts used by plugin to collect from Linux guest
 */
#define LIN_SCRIPT_NAME_PROCESSES "get-listening-process-info.sh"
#define LIN_SCRIPT_NAME_CONNECTIONS "get-connection-info.sh"
#define LIN_SCRIPT_NAME_PERFORMANCE_METRICS "get-listening-process-perf-metrics.sh"
#define LIN_SCRIPT_NAME_VERSIONS "get-versions.sh"

#endif

/*
 * Default value for CONFNAME_SERVICE_DISCOVERY_DISABLED setting in
 * tools configuration file.
 */
#define SERVICE_DISCOVERY_CONF_DEFAULT_DISABLED_VALUE FALSE

/*
 * Polling interval of service discovery plugin in milliseconds
 */
#define SERVICE_DISCOVERY_POLL_INTERVAL 300000

/*
 * Time shift for comparision of time read from the signal and
 * current system time in milliseconds.
 */
#define SERVICE_DISCOVERY_WRITE_DELTA 60000

/*
 * Time to wait in milliseconds before RPC operation
 */
#define SERVICE_DISCOVERY_RPC_WAIT_TIME 100


typedef struct {
   gchar *keyName;
   gchar *val;
} KeyNameValue;

#if defined(_WIN32)
static KeyNameValue gKeyScripts[] = {
   { WIN_KEY_NAME_PROCESSES, WIN_SCRIPT_NAME_PROCESSES },
   { WIN_KEY_NAME_CONNECTIONS, WIN_SCRIPT_NAME_CONNECTIONS },
   { WIN_KEY_NAME_PERFORMANCE_METRICS, WIN_SCRIPT_NAME_PERFORMANCE_METRICS },
   { WIN_KEY_NAME_VERSIONS, WIN_SCRIPT_NAME_VERSIONS },
   { WIN_KEY_NAME_RELATIONSHIP, WIN_SCRIPT_NAME_RELATIONSHIP },
   { WIN_KEY_NAME_IIS_PORTS, WIN_SCRIPT_NAME_IIS_PORTS },
   { WIN_KEY_NAME_NET, WIN_SCRIPT_NAME_NET },
};
#else
static KeyNameValue gKeyScripts[] = {
   { LIN_KEY_NAME_PROCESSES, LIN_SCRIPT_NAME_PROCESSES },
   { LIN_KEY_NAME_CONNECTIONS, LIN_SCRIPT_NAME_CONNECTIONS },
   { LIN_KEY_NAME_PERFORMANCE_METRICS, LIN_SCRIPT_NAME_PERFORMANCE_METRICS },
   { LIN_KEY_NAME_VERSIONS, LIN_SCRIPT_NAME_VERSIONS },
};
#endif

static GSource *gServiceDiscoveryTimeoutSource = NULL;
static gint64 gLastWriteTime = 0;

static GArray *gFullPaths = NULL;
static volatile Bool gTaskSubmitted = FALSE;


/*
 *****************************************************************************
 * GetGuestTimeInMillis --
 *
 * Get system current time in millis.
 *
 * @retval time in millis.
 *
 *****************************************************************************
 */

static gint64
GetGuestTimeInMillis(void)
{
   return g_get_real_time() / 1000;
}


/*
 *****************************************************************************
 * SendRpcMessage --
 *
 * Sends message over RPC channel.
 *
 * @param[in] ctx         Application context.
 * @param[in] msg         Message to send
 * @param[in] msgLen      Length of the message to send
 * @param[out] result     Rpc operation result, freed by callers
 * @param[out] resultLen  Length of Rpc operation result
 *
 * @retval TRUE  RPC message send succeeded.
 * @retval FALSE RPC message send failed.
 *
 *****************************************************************************
 */

static Bool
SendRpcMessage(ToolsAppCtx *ctx,
               char const *msg,
               size_t msgLen,
               char **result,
               size_t *resultLen)
{
   Bool status;
   RpcChannelType rpcChannelType = RpcChannel_GetType(ctx->rpc);

   g_debug("%s: Current RPC channel type: %d\n", __FUNCTION__, rpcChannelType);

   if (rpcChannelType == RPCCHANNEL_TYPE_PRIV_VSOCK) {
      status = RpcChannel_Send(ctx->rpc, msg, msgLen, result, resultLen);
   } else {
      /*
       * After the vmsvc RPC channel falls back to backdoor, it could not
       * send through privileged guest RPC any more.
       */
      g_usleep(SERVICE_DISCOVERY_RPC_WAIT_TIME * 1000);
      status = RpcChannel_SendOneRawPriv(msg, msgLen, result, resultLen);

      /*
       * RpcChannel_SendOneRawPriv returns 'Permission denied' if the
       * privileged vsocket can not be established.
       */
      if (!status && result != NULL &&
          strcmp(*result, "Permission denied") == 0) {
         g_debug("%s: Retrying RPC send", __FUNCTION__);
         free(*result);
         g_usleep(SERVICE_DISCOVERY_RPC_WAIT_TIME * 1000);
         status = RpcChannel_SendOneRawPriv(msg, msgLen, result, resultLen);
      }
   }

   return status;
}


/*
 *****************************************************************************
 * WriteData --
 *
 * Sends key-value update request to the Namespace DB.
 *
 * @param[in] ctx       Application context.
 * @param[in] key       Key sent to the Namespace DB
 * @param[in] value     Service data sent to the Namespace DB
 * @param[in] len       Service data len
 *
 * @retval TRUE  Namespace DB write over RPC succeeded.
 * @retval FALSE Namespace DB write over RPC failed.
 *
 *****************************************************************************
 */

Bool
WriteData(ToolsAppCtx *ctx,
          const char *key,
          const char *data,
          const size_t len)
{
   Bool status = FALSE;
   DynBuf buf;
   gchar *timeStamp = NULL;

   if (data != NULL) {
      timeStamp = g_strdup_printf("%" G_GINT64_FORMAT, gLastWriteTime);
   }

   DynBuf_Init(&buf);

   /*
    * Format is:
    *
    * namespace-set-keys <namespace>\0<numOps>\0<op>\0<key>\0<value>\0<oldVal>
    *
    * We have just a single op, and want to always set the value, clobbering
    * anything already there.
    */
   if (!DynBuf_Append(&buf, NSDB_PRIV_SET_KEYS_CMD,
                      strlen(NSDB_PRIV_SET_KEYS_CMD)) ||
       !DynBuf_Append(&buf, " ", 1) ||
       !DynBuf_AppendString(&buf, SERVICE_DISCOVERY_NAMESPACE_DB_NAME) ||
       !DynBuf_AppendString(&buf, "1") || // numOps
       !DynBuf_AppendString(&buf, "0") || // op 0 == setAlways
       !DynBuf_AppendString(&buf, key)) {
      g_warning("%s: Could not construct buffer header\n", __FUNCTION__);
      goto out;
   }

   if (data != NULL) {
      if (!DynBuf_Append(&buf, timeStamp, strlen(timeStamp)) ||
          !DynBuf_Append(&buf, ",", 1) ||
          !DynBuf_Append(&buf, data, len) ||
          !DynBuf_Append(&buf, "", 1)) {
         g_warning("%s: Could not construct write buffer\n", __FUNCTION__);
         goto out;
      }
   } else {
      if (!DynBuf_Append(&buf, "", 1)) {
         g_warning("%s: Could not construct delete buffer\n", __FUNCTION__);
         goto out;
      }
   }

   if (!DynBuf_Append(&buf, "", 1)) {
      g_warning("%s: Could not construct buffer footer\n", __FUNCTION__);
      goto out;
   } else {
      char *result = NULL;
      size_t resultLen;

      status = SendRpcMessage(ctx, DynBuf_Get(&buf), DynBuf_GetSize(&buf),
                              &result, &resultLen);
      if (!status) {
         g_warning("%s: Failed to update %s, result: %s resultLen: %" FMTSZ
                   "u\n", __FUNCTION__, key, (result != NULL) ?
                   result : "(null)", resultLen);
      }

      if (result != NULL) {
         free(result);
      }
   }

out:
   DynBuf_Destroy(&buf);
   g_free(timeStamp);

   return status;
}


/*
 *****************************************************************************
 * ReadData --
 *
 * Reads value from Namespace DB by given key.
 *
 * @param[in] ctx             Application context.
 * @param[in] key             Key sent to the Namespace DB
 * @param[out] resultData     Data fetched from Namespace DB, freed by callers
 * @param[out] resultDataLen  Length of data fetched from Namespace DB
 *
 * @retval TRUE  Namespace DB read over RPC succeeded.
 * @retval FALSE Namespace DB read over RPC failed.
 *
 *****************************************************************************
 */

static Bool
ReadData(ToolsAppCtx *ctx,
         const char *key,
         char **resultData,
         size_t *resultDataLen)
{
   DynBuf buf;
   Bool status = FALSE;

   ASSERT(key);

   *resultData = NULL;
   *resultDataLen = 0;

   DynBuf_Init(&buf);

   /*
    * Format is
    *
    * namespace-get-values <namespace>\0<key>\0...
    *
    */
   if (!DynBuf_Append(&buf, NSDB_PRIV_GET_VALUES_CMD,
                      strlen(NSDB_PRIV_GET_VALUES_CMD)) ||
       !DynBuf_Append(&buf, " ", 1) ||
       !DynBuf_AppendString(&buf, SERVICE_DISCOVERY_NAMESPACE_DB_NAME) ||
       !DynBuf_AppendString(&buf, key)) {
      g_warning("%s: Could not construct request buffer\n", __FUNCTION__);
      goto done;
    }

   status = SendRpcMessage(ctx, DynBuf_Get(&buf), DynBuf_GetSize(&buf),
                           resultData, resultDataLen);
   if (!status) {
      g_debug("%s: Read over RPC failed, result: %s, resultDataLen: %" FMTSZ
              "u\n", __FUNCTION__, (*resultData != NULL) ?
              *resultData : "(null)", *resultDataLen);
   }
done:
   DynBuf_Destroy(&buf);
   return status;
}


/*
 *****************************************************************************
 * DeleteData --
 *
 * Deletes key/value from Namespace DB.
 *
 * @param[in] ctx       Application context.
 * @param[in] key       Key of entry to be deleted from the Namespace DB
 *
 * @retval TRUE  Namespace DB delete over RPC succeeded.
 * @retval FALSE Namespace DB delete over RPC failed.
 *
 *****************************************************************************
 */

static Bool
DeleteData(ToolsAppCtx *ctx,
           const char *key)
{
   return WriteData(ctx, key, NULL, 0);
}


/*
 *****************************************************************************
 * CleanupNamespaceDB --
 *
 * Deletes all the chunks written to the Namespace DB in previous cycle.
 *
 * @param[in] ctx       Application context.
 *
 *****************************************************************************
 */

static void
CleanupNamespaceDB(ToolsAppCtx *ctx) {
   int i;

   g_debug("%s: Performing cleanup of previous data\n", __FUNCTION__);

   ASSERT(gFullPaths);

   for (i = 0; i < gFullPaths->len; i++) {
      char *value = NULL;
      size_t len = 0;
      KeyNameValue tmp = g_array_index(gFullPaths, KeyNameValue, i);
      /*
       * Read count of chunks, ignore timestamp, iterate over chunks
       * and remove them.
       */
      if (ReadData(ctx, tmp.keyName, &value, &len) && len > 1) {
         char *token = NULL;
         g_debug("%s: Read %s from Namespace DB\n", __FUNCTION__, value);

         /*
         * Delete this key anyways, afterwards check for chunks
         */
         if (!DeleteData(ctx, tmp.keyName)) {
            g_warning("%s: Not able to delete entry %s", __FUNCTION__,
                      tmp.keyName);
         }

         if (NULL == strtok(value, ",")) {
            g_warning("%s: Malformed data for %s in Namespace DB",
                      __FUNCTION__, tmp.keyName);
            if (value) {
               free(value);
               value = NULL;
            }
            continue;
         }
         token = strtok(NULL, ",");
         if (token != NULL) {
            int count = (int) g_ascii_strtoll(token, NULL, 10);
            int j;
            gchar *msg = NULL;

            for (j = 0; j < count; j++) {
               msg = g_strdup_printf("%s-%d", tmp.keyName, j + 1);
               g_debug("%s: Deleting entry %s",__FUNCTION__, msg);
               if (!DeleteData(ctx, msg)) {
                  g_warning("%s: Not able to delete entry %s",
                            __FUNCTION__, msg);
               }
               g_free(msg);
            }
         } else {
            g_warning("%s: Chunk count has invalid value %s", __FUNCTION__,
                      value);
         }
      } else {
         g_warning("%s: Key %s not found in Namespace DB\n", __FUNCTION__,
                   tmp.keyName);
      }

      if (value) {
         free(value);
         value = NULL;
      }
   }
}


/*
 *****************************************************************************
 * ServiceDiscoveryTask --
 *
 * Task to gather discovered services' data and write to Namespace DB.
 *
 * @param[in] ctx             Application context.
 * @param[in] data            Data pointer, not used.
 *
 *****************************************************************************
 */

static void
ServiceDiscoveryTask(ToolsAppCtx *ctx,
                     void *data)
{
   Bool status = FALSE;
   int i;
   gint64 previousWriteTime = gLastWriteTime;

   gTaskSubmitted = TRUE;

   /*
    * We are going to write to Namespace DB, update glastWriteTime
    */
   gLastWriteTime = GetGuestTimeInMillis();

   /*
    * Reset "ready" flag to stop readers until all data is written
    */
   status = WriteData(ctx, SERVICE_DISCOVERY_KEY_NAME_READY, "FALSE", 5);
   if (!status) {
      gLastWriteTime = previousWriteTime;
      g_warning("%s: Failed to reset %s flag", __FUNCTION__,
                SERVICE_DISCOVERY_KEY_NAME_READY);
      goto out;
   }

   /*
    * Remove chunks written to DB in the previous iteration
    */
   CleanupNamespaceDB(ctx);

   for (i = 0; i < gFullPaths->len; i++) {
      KeyNameValue tmp = g_array_index(gFullPaths, KeyNameValue, i);
      if (!PublishScriptOutputToNamespaceDB(ctx, tmp.keyName, tmp.val)) {
         g_debug("%s: PublishScriptOutputToNamespaceDB failed for script %s\n",
                 __FUNCTION__, tmp.val);
      }
   }

   /*
    * Update ready flag
    */
   status = WriteData(ctx, SERVICE_DISCOVERY_KEY_NAME_READY, "TRUE", 4);
   if (!status) {
      g_warning("%s: Failed to update ready flag", __FUNCTION__);
   }

out:
   gTaskSubmitted = FALSE;
}


/*
 *****************************************************************************
 * checkForWrite --
 *
 * Performs needed checks to decide if Data should be written to Namespace DB
 * or not.
 *
 * First check - checks if interval related information, stored in Namespace DB
 * under key "signal" and in format of "interval,timestamp" is outdated or not.
 *
 * Second check - checks if time greater than interval read from Namespace DB
 * has elapsed since the last write operation.
 *
 * @param[in] ctx     The application context.
 *
 * @retval TRUE  Execute scripts and write service data to Namespace DB.
 * @retval FALSE Omit this cycle wihtout any script running.
 *
 *****************************************************************************
 */

static Bool
checkForWrite(ToolsAppCtx *ctx)
{
   char *signal = NULL;
   size_t signalLen = 0;
   Bool result = FALSE;

   /*
    * Read signal from Namespace DB
    */
   if (!ReadData(ctx, SERVICE_DISCOVERY_KEY_NAME_SIGNAL, &signal, &signalLen)) {
      g_debug("%s: Failed to read necessary information from Namespace DB\n",
              __FUNCTION__);
   } else {
      gint64 clientTimestamp;
      int clientInterval;
      gint64 currentTime;

      if ((signal != NULL) && (strcmp(signal, "")) && signalLen > 0) {
         char *token1;
         char *token2;
         g_debug("%s: signal = %s last write time = %" G_GINT64_FORMAT "\n",
                 __FUNCTION__, signal, gLastWriteTime);
         /*
          * parse signal, it should be in "interval,timestamp" format
          */
         token1 = strtok(signal, ",");
         token2 = strtok(NULL, ",");
         if (token1 != NULL && token2 != NULL) {
            currentTime = GetGuestTimeInMillis();
            clientInterval = (int) g_ascii_strtoll(token1, NULL, 10);
            clientTimestamp = g_ascii_strtoll(token2, NULL, 10);

            if (clientInterval == 0 || clientTimestamp == 0) {
               g_warning("%s: Wrong value of interval and timestamp",
                         __FUNCTION__);
            } else if ((currentTime - clientTimestamp) < (5 * clientInterval)) {
               if ((currentTime - gLastWriteTime +
                   SERVICE_DISCOVERY_WRITE_DELTA) >= clientInterval) {
                  result = TRUE;
               }
            } else {
               /*
               * Signal is outdated, reset the last write time
               */
               gLastWriteTime = 0;
            }

            g_debug("%s: result=%s client interval = %d "
                    "client timestamp =% " G_GINT64_FORMAT
                    " system time = %" G_GINT64_FORMAT
                    " previous write time = %" G_GINT64_FORMAT "\n",
                    __FUNCTION__, result ? "true" : "false", clientInterval,
                    clientTimestamp, currentTime, gLastWriteTime);
         } else {
            g_warning("%s: Wrong value of signal", __FUNCTION__);
         }
      } else {
         g_warning("%s: signal was NULL or empty", __FUNCTION__);
      }
   }

   if (signal) {
      free(signal);
   }

   return result;
}


/*
 *****************************************************************************
 * ServiceDiscoveryThread --
 *
 * Creates a new thread that collects all the desired application related
 * information and updates the Namespace DB.
 *
 * @param[in]  data     The application context.
 *
 * @return TRUE to indicate that the timer should be rescheduled.
 *
 *****************************************************************************
 */

static Bool
ServiceDiscoveryThread(gpointer data)
{
   ToolsAppCtx *ctx = data;

   /*
    * First check for taskSubmitted, if it is true automatically omit this
    * cycle even without checking for write to avoid resetting last write
    * time.
    */
   if (gTaskSubmitted || !checkForWrite(ctx)) {
      g_debug("%s: Data should not be written taskSubmitted = %s\n",
              __FUNCTION__, gTaskSubmitted ? "True" : "False");
   } else {
      g_debug("%s: Submitting task to write\n", __FUNCTION__);
      if (!ToolsCorePool_SubmitTask(ctx, ServiceDiscoveryTask, NULL, NULL)) {
         g_warning("%s: failed to start information gather thread\n",
                   __FUNCTION__);
      }
   }

   return TRUE;
}


/*
 *****************************************************************************
 * TweakDiscoveryLoop --
 *
 * @brief Start service discovery poll loop.
 *
 * @param[in] ctx  The app context.
 *
 *****************************************************************************
 */

static void
TweakDiscoveryLoop(ToolsAppCtx *ctx)
{
   if (gServiceDiscoveryTimeoutSource == NULL) {
      gServiceDiscoveryTimeoutSource =
                      g_timeout_source_new(SERVICE_DISCOVERY_POLL_INTERVAL);
      VMTOOLSAPP_ATTACH_SOURCE(ctx, gServiceDiscoveryTimeoutSource,
                               ServiceDiscoveryThread, ctx, NULL);
      g_source_unref(gServiceDiscoveryTimeoutSource);
   }
}


/*
 ******************************************************************************
 * ServiceDiscoveryServerConfReload --
 *
 * @brief Reconfigures the poll loop interval upon config file reload.
 *
 * @param[in]  src     Unused.
 * @param[in]  ctx     The application context.
 * @param[in]  data    Unused.
 *
 ******************************************************************************
 */

static void
ServiceDiscoveryServerConfReload(gpointer src,
                                 ToolsAppCtx *ctx,
                                 gpointer data)
{
   gboolean disabled =
      VMTools_ConfigGetBoolean(ctx->config,
                               CONFGROUPNAME_SERVICEDISCOVERY,
                               CONFNAME_SERVICEDISCOVERY_DISABLED,
                               SERVICE_DISCOVERY_CONF_DEFAULT_DISABLED_VALUE);
   if (!disabled) {
      g_info("%s: Service discovery loop started\n", __FUNCTION__);
      TweakDiscoveryLoop(ctx);
   } else if (gServiceDiscoveryTimeoutSource != NULL) {
      gLastWriteTime = 0;
      g_source_destroy(gServiceDiscoveryTimeoutSource);
      gServiceDiscoveryTimeoutSource = NULL;
      g_info("%s: Service discovery loop disabled\n", __FUNCTION__);
   }
}


/*
 *****************************************************************************
 * ServiceDiscoveryServerShutdown --
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
ServiceDiscoveryServerShutdown(gpointer src,
                               ToolsAppCtx *ctx,
                               gpointer data)
{
   if (gServiceDiscoveryTimeoutSource != NULL) {
      g_source_destroy(gServiceDiscoveryTimeoutSource);
      gServiceDiscoveryTimeoutSource = NULL;
   }

   if (gFullPaths != NULL) {
      int i = 0;
      guint len = gFullPaths->len;
      for (i = 0; i < len; ++i) {
         g_free(g_array_index(gFullPaths, KeyNameValue, i).keyName);
         g_free(g_array_index(gFullPaths, KeyNameValue, i).val);
      }
      g_array_free(gFullPaths, TRUE);
   }
}


/*
 *****************************************************************************
 * ConstructScriptPaths --
 *
 * Construct final paths of the scripts that will be used for execution.
 *
 *****************************************************************************
 */

static void
ConstructScriptPaths(void)
{
   int i;
   gchar *scriptInstallDir;
#if !defined(OPEN_VM_TOOLS)
   gchar *toolsInstallDir;
#endif

   if (gFullPaths != NULL) {
      return;
   }

   gFullPaths = g_array_sized_new(FALSE, TRUE, sizeof(KeyNameValue),
                                  ARRAYSIZE(gKeyScripts));

#if defined(OPEN_VM_TOOLS)
   scriptInstallDir = Util_SafeStrdup(VMTOOLS_SERVICE_DISCOVERY_SCRIPTS);
#else
   toolsInstallDir = GuestApp_GetInstallPath();
   scriptInstallDir = g_strdup_printf("%s%s%s%s%s", toolsInstallDir, DIRSEPS,
                                      "serviceDiscovery", DIRSEPS, "scripts");
   g_free(toolsInstallDir);
#endif

   for (i = 0; i < ARRAYSIZE(gKeyScripts); ++i) {
      KeyNameValue tmp;
      tmp.keyName = g_strdup_printf("%s", gKeyScripts[i].keyName);
#if defined(_WIN32)
      tmp.val = g_strdup_printf("\"%s%s%s\"", scriptInstallDir,
                                DIRSEPS, gKeyScripts[i].val);
#else
      tmp.val = g_strdup_printf("%s%s%s", scriptInstallDir, DIRSEPS,
                                gKeyScripts[i].val);
#endif
      g_array_insert_val(gFullPaths, i, tmp);
   }

   g_free(scriptInstallDir);
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
      "serviceDiscovery",
      NULL,
      NULL
   };

   uint32 vmxVersion = 0;
   uint32 vmxType = VMX_TYPE_UNSET;

   /*
    * Return NULL to disable the plugin if not running in a VMware VM.
    */
   if (!ctx->isVMware) {
      g_info("%s: Not running in a VMware VM.\n", __FUNCTION__);
      return NULL;
   }

   /*
    * Return NULL to disable the plugin if VM is not running on ESX host.
    */
   if (!VmCheck_GetVersion(&vmxVersion, &vmxType) ||
       vmxType != VMX_TYPE_SCALABLE_SERVER) {
      g_info("%s, VM is not running on ESX host.\n", __FUNCTION__);
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
   if (ctx->rpc != NULL) {
      ToolsPluginSignalCb sigs[] = {
         { TOOLS_CORE_SIG_SHUTDOWN, ServiceDiscoveryServerShutdown, NULL },
         { TOOLS_CORE_SIG_CONF_RELOAD, ServiceDiscoveryServerConfReload, NULL }
      };
      ToolsAppReg regs[] = {
         { TOOLS_APP_SIGNALS,
           VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs))
         }
      };
      gboolean disabled;

      regData.regs = VMTools_WrapArray(regs,
                                       sizeof *regs,
                                       ARRAYSIZE(regs));
      /*
       * Append scripts absolute paths based on installation dirs.
       */
      ConstructScriptPaths();

      disabled =
         VMTools_ConfigGetBoolean(ctx->config,
                                  CONFGROUPNAME_SERVICEDISCOVERY,
                                  CONFNAME_SERVICEDISCOVERY_DISABLED,
                                  SERVICE_DISCOVERY_CONF_DEFAULT_DISABLED_VALUE);
      if (!disabled) {
         TweakDiscoveryLoop(ctx);
      }

      return &regData;
   }

   return NULL;
}
