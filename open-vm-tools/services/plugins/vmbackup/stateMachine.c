/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * @file stateMachine.c
 *
 * Implements a generic state machine for executing backup operations
 * asynchronously. Since VSS is based on an asynchronous polling model,
 * we're basing all backup operations on a similar model controlled by this
 * state machine, even if it would be more eficient to use an event-driven
 * approach in some cases.
 *
 * For a description of the state machine, check the README file.
 *
 * The sync provider state machine depends on the particular implementation.
 * For the sync driver, it enables the driver and waits for a "snapshot done"
 * message before finishing. For the VSS subsystem, the sync provider just
 * implements a VSS backup cycle.
 */

#include "vmBackupInt.h"

#include <glib-object.h>
#include <gmodule.h>
#include "guestApp.h"
#include "str.h"
#include "strutil.h"
#include "util.h"
#include "vmBackupSignals.h"
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"

#if !defined(__APPLE__)
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif


#define VMBACKUP_ENQUEUE_EVENT() do {                                         \
   gBackupState->timerEvent = g_timeout_source_new(gBackupState->pollPeriod); \
   VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,                                \
                            gBackupState->timerEvent,                         \
                            VmBackupAsyncCallback,                            \
                            NULL,                                             \
                            NULL);                                            \
} while (0)

static VmBackupState *gBackupState = NULL;

static Bool
VmBackupEnableSync(void);


/**
 * Returns a string representation of the given state machine state.
 *
 * @param[in]  state    State of interest.
 *
 * @return A string representation of the state.
 */

static const char *
VmBackupGetStateName(VmBackupMState state)
{
   switch (state) {
   case VMBACKUP_MSTATE_IDLE:
      return "IDLE";

   case VMBACKUP_MSTATE_SCRIPT_FREEZE:
      return "SCRIPT_FREEZE";

   case VMBACKUP_MSTATE_SYNC_FREEZE:
      return "SYNC_FREEZE";

   case VMBACKUP_MSTATE_SYNC_THAW:
      return "SYNC_THAW";

   case VMBACKUP_MSTATE_SCRIPT_THAW:
      return "SCRIPT_THAW";

   case VMBACKUP_MSTATE_SCRIPT_ERROR:
      return "SCRIPT_ERROR";

   case VMBACKUP_MSTATE_SYNC_ERROR:
      return "SYNC_ERROR";

   default:
      NOT_IMPLEMENTED();
   }
}


/**
 * Sends a keep alive backup event to the VMX.
 *
 * @param[in]  clientData     Unused.
 *
 * @return FALSE
 */

static gboolean
VmBackupKeepAliveCallback(void *clientData)
{
   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);
   g_source_unref(gBackupState->keepAlive);
   gBackupState->keepAlive = NULL;
   VmBackup_SendEvent(VMBACKUP_EVENT_KEEP_ALIVE, 0, "");
   return FALSE;
}


/**
 * Sends a command to the VMX asking it to update VMDB about a new backup event.
 * This will restart the keep-alive timer.
 *
 * @param[in]  event    The event to set.
 * @param[in]  code     Error code.
 * @param[in]  dest     Error description.
 *
 * @return TRUE on success.
 */

Bool
VmBackup_SendEvent(const char *event,
                   const uint32 code,
                   const char *desc)
{
   Bool success;
   char *result;
   size_t resultLen;
   gchar *msg;

   ASSERT(gBackupState != NULL);

   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState->keepAlive != NULL) {
      g_source_destroy(gBackupState->keepAlive);
      g_source_unref(gBackupState->keepAlive);
   }

   msg = g_strdup_printf(VMBACKUP_PROTOCOL_EVENT_SET" %s %u %s", event, code, desc);
   success = RpcChannel_Send(gBackupState->ctx->rpc, msg, strlen(msg) + 1,
                             &result, &resultLen);

   if (!success) {
      g_warning("Failed to send event to the VMX: %s.\n", result);
   }

   gBackupState->keepAlive = g_timeout_source_new(VMBACKUP_KEEP_ALIVE_PERIOD / 2);
   VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,
                            gBackupState->keepAlive,
                            VmBackupKeepAliveCallback,
                            NULL,
                            NULL);
   return success;
}


/**
 * Cleans up the backup state object and sends a "done" event to the VMX.
 */

static void
VmBackupFinalize(void)
{
   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);

   if (gBackupState->abortTimer != NULL) {
      g_source_destroy(gBackupState->abortTimer);
      g_source_unref(gBackupState->abortTimer);
   }
   if (gBackupState->currentOp != NULL) {
      VmBackup_Cancel(gBackupState->currentOp);
      VmBackup_Release(gBackupState->currentOp);
   }

   VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_DONE, VMBACKUP_SUCCESS, "");

   if (gBackupState->timerEvent != NULL) {
      g_source_destroy(gBackupState->timerEvent);
      g_source_unref(gBackupState->timerEvent);
   }

   if (gBackupState->keepAlive != NULL) {
      g_source_destroy(gBackupState->keepAlive);
      g_source_unref(gBackupState->keepAlive);
   }

   gBackupState->provider->release(gBackupState->provider);
   g_free(gBackupState->volumes);
   g_free(gBackupState->snapshots);
   g_free(gBackupState);
   gBackupState = NULL;
}


/**
 * Starts the execution of the scripts for the given action type.
 *
 * @param[in]  type        Type of scripts being started.
 *
 * @return TRUE, unless starting the scripts fails for some reason.
 */

static Bool
VmBackupStartScripts(VmBackupScriptType type)
{
   const char *opName;
   VmBackupMState nextState;
   g_debug("*** %s\n", __FUNCTION__);

   switch (type) {
      case VMBACKUP_SCRIPT_FREEZE:
         opName = "VmBackupOnFreeze";
         nextState = VMBACKUP_MSTATE_SCRIPT_FREEZE;
         break;

      case VMBACKUP_SCRIPT_FREEZE_FAIL:
         opName = "VmBackupOnFreezeFail";
         nextState = VMBACKUP_MSTATE_SCRIPT_ERROR;
         break;

      case VMBACKUP_SCRIPT_THAW:
         opName = "VmBackupOnThaw";
         nextState = VMBACKUP_MSTATE_SCRIPT_THAW;
         break;

      default:
         NOT_REACHED();
   }

   if (!VmBackup_SetCurrentOp(gBackupState,
                              VmBackup_NewScriptOp(type, gBackupState),
                              NULL,
                              opName)) {
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SCRIPT_ERROR,
                         "Error when starting backup scripts.");
      return FALSE;
   }

   gBackupState->machineState = nextState;
   return TRUE;
}


/**
 * Puts the state machine in the right state when an error occurs. The caller
 * should check for the state of the backup upon this function returning: if
 * it's IDLE, it means the backup state should be cleaned up.
 *
 * @return Whether the backup operation should be finalized.
 */

static gboolean
VmBackupOnError(void)
{
   switch (gBackupState->machineState) {
   case VMBACKUP_MSTATE_SCRIPT_FREEZE:
   case VMBACKUP_MSTATE_SYNC_ERROR:
      /* Next state is "script error". */
      if (!VmBackupStartScripts(VMBACKUP_SCRIPT_FREEZE_FAIL)) {
         gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
      }
      break;

   case VMBACKUP_MSTATE_SYNC_FREEZE:
   case VMBACKUP_MSTATE_SYNC_THAW:
      /* Next state is "sync error". */
      gBackupState->pollPeriod = 1000;
      gBackupState->machineState = VMBACKUP_MSTATE_SYNC_ERROR;
      g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                            TOOLS_CORE_SIG_IO_FREEZE,
                            gBackupState->ctx,
                            FALSE);
      break;

   case VMBACKUP_MSTATE_SCRIPT_THAW:
      /* Next state is "idle". */
      gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
      break;

   default:
      g_error("Unexpected machine state on error: %s\n",
              VmBackupGetStateName(gBackupState->machineState));
   }

   return (gBackupState->machineState == VMBACKUP_MSTATE_IDLE);
}


/**
 * Aborts the current operation, unless we're already in an error state.
 */

static void
VmBackupDoAbort(void)
{
   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);
   if (gBackupState->machineState != VMBACKUP_MSTATE_SCRIPT_ERROR &&
       gBackupState->machineState != VMBACKUP_MSTATE_SYNC_ERROR) {
      /* Mark the current operation as cancelled. */
      if (gBackupState->currentOp != NULL) {
         VmBackup_Cancel(gBackupState->currentOp);
         VmBackup_Release(gBackupState->currentOp);
         gBackupState->currentOp = NULL;
      }

      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ABORT,
                         VMBACKUP_REMOTE_ABORT,
                         "Backup aborted.");

      /* Transition to the error state. */
      if (VmBackupOnError()) {
         VmBackupFinalize();
      }
   }
}


/**
 * Timer callback to abort the current operation.
 *
 * @param[in] data    Unused.
 *
 * @return FALSE
 */

static gboolean
VmBackupAbortTimer(gpointer data)
{
   ASSERT(gBackupState != NULL);
   g_warning("Aborting backup operation due to timeout.");
   VmBackupDoAbort();
   g_source_unref(gBackupState->abortTimer);
   gBackupState->abortTimer = NULL;
   return FALSE;
}


/**
 * Callback that checks for the status of the current operation. Calls the
 * queued operations as needed.
 *
 * @param[in]  clientData     Unused.
 *
 * @return FALSE
 */

static gboolean
VmBackupAsyncCallback(void *clientData)
{
   VmBackupOpStatus status = VMBACKUP_STATUS_FINISHED;

   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);

   g_source_unref(gBackupState->timerEvent);
   gBackupState->timerEvent = NULL;

   if (gBackupState->currentOp != NULL) {
      g_debug("VmBackupAsyncCallback: checking %s\n", gBackupState->currentOpName);
      status = VmBackup_QueryStatus(gBackupState->currentOp);
   }

   switch (status) {
   case VMBACKUP_STATUS_PENDING:
      goto exit;

   case VMBACKUP_STATUS_FINISHED:
      if (gBackupState->currentOpName != NULL) {
         g_debug("Async request '%s' completed\n", gBackupState->currentOpName);
         VmBackup_Release(gBackupState->currentOp);
         gBackupState->currentOpName = NULL;
      }
      gBackupState->currentOp = NULL;
      break;

   default:
      {
         Bool freeMsg = TRUE;
         char *errMsg = Str_Asprintf(NULL,
                                     "Asynchronous operation failed: %s",
                                     gBackupState->currentOpName);
         if (errMsg == NULL) {
            freeMsg = FALSE;
            errMsg = "Asynchronous operation failed.";
         }
         VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                            VMBACKUP_UNEXPECTED_ERROR,
                            errMsg);
         if (freeMsg) {
            vm_free(errMsg);
         }

         VmBackup_Release(gBackupState->currentOp);
         gBackupState->currentOp = NULL;
         VmBackupOnError();
         goto exit;
      }
   }

   /*
    * Keep calling the registered callback until it's either NULL, or
    * an asynchronous operation is scheduled.
    */
   while (gBackupState->callback != NULL) {
      VmBackupCallback cb = gBackupState->callback;
      gBackupState->callback = NULL;

      if (cb(gBackupState)) {
         if (gBackupState->currentOp != NULL || gBackupState->forceRequeue) {
            goto exit;
         }
      } else {
         VmBackupOnError();
         goto exit;
      }
   }

   /*
    * At this point, the current operation can be declared finished, and the
    * state machine can move to the next state.
    */
   switch (gBackupState->machineState) {
   case VMBACKUP_MSTATE_SCRIPT_FREEZE:
      /* Next state is "sync freeze". */
      if (!VmBackupEnableSync()) {
         VmBackupOnError();
      }
      break;

   case VMBACKUP_MSTATE_SYNC_FREEZE:
      /*
       * The SYNC_FREEZE -> SYNC_THAW transition is handled by the RPC callback,
       * so this case is a no-op.
       */
      break;

   case VMBACKUP_MSTATE_SYNC_THAW:
      /* Next state is "script thaw". */
      g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                            TOOLS_CORE_SIG_IO_FREEZE,
                            gBackupState->ctx,
                            FALSE);
      if (!VmBackupStartScripts(VMBACKUP_SCRIPT_THAW)) {
         VmBackupOnError();
      }
      break;

   case VMBACKUP_MSTATE_SCRIPT_ERROR:
   case VMBACKUP_MSTATE_SCRIPT_THAW:
      /* Next state is "idle". */
      gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
      break;

   case VMBACKUP_MSTATE_SYNC_ERROR:
      /* Next state is "script error". */
      if (!VmBackupStartScripts(VMBACKUP_SCRIPT_FREEZE_FAIL)) {
         VmBackupOnError();
      }
      break;

   default:
      g_error("Unexpected machine state: %s\n",
              VmBackupGetStateName(gBackupState->machineState));
   }

exit:
   /* If the state machine is back in IDLE, it means the backup operation finished. */
   if (gBackupState->machineState == VMBACKUP_MSTATE_IDLE) {
      VmBackupFinalize();
   } else {
      gBackupState->forceRequeue = FALSE;
      VMBACKUP_ENQUEUE_EVENT();
   }
   return FALSE;
}


/**
 * Calls the sync provider's start function.
 *
 * @param[in]  state    The backup state.
 *
 * @return Whether the provider's start callback was successful.
 */

static Bool
VmBackupEnableSync(void)
{
   g_debug("*** %s\n", __FUNCTION__);
   g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                         TOOLS_CORE_SIG_IO_FREEZE,
                         gBackupState->ctx,
                         TRUE);
   if (!gBackupState->provider->start(gBackupState,
                                      gBackupState->provider->clientData)) {
      g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                            TOOLS_CORE_SIG_IO_FREEZE,
                            gBackupState->ctx,
                            FALSE);
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SYNC_ERROR,
                         "Error when enabling the sync provider.");
      return FALSE;
   }

   gBackupState->machineState = VMBACKUP_MSTATE_SYNC_FREEZE;
   return TRUE;
}


/* RpcIn callbacks. */


/**
 * Handler for the "vmbackup.start" message. Starts the "freeze" scripts
 * unless there's another backup operation going on or some other
 * unexpected error occurs.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static gboolean
VmBackupStart(RpcInData *data)
{
   GError *err = NULL;
   guint timeout;
   ToolsAppCtx *ctx = data->appCtx;
   VmBackupSyncProvider *provider = NULL;

   size_t i;
   
   /* List of available providers, in order of preference for loading. */
   struct SyncProvider {
      VmBackupSyncProvider *(*ctor)(void);
      const gchar *cfgEntry;
   } providers[] = {
#if defined(_WIN32)
      { VmBackup_NewVssProvider, "enableVSS" },
#endif
      { VmBackup_NewSyncDriverProvider, "enableSyncDriver" },
      { VmBackup_NewNullProvider, NULL },
   };

   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState != NULL) {
      return RPCIN_SETRETVALS(data, "Backup operation already in progress.", FALSE);
   }

   /* Instantiate the sync provider. */
   for (i = 0; i < ARRAYSIZE(providers); i++) {
      gboolean enabled = TRUE;
      struct SyncProvider *sp = &providers[i];

      if (sp->cfgEntry != NULL) {
         enabled = g_key_file_get_boolean(ctx->config,
                                          "vmbackup",
                                          sp->cfgEntry,
                                          &err);
         if (err != NULL) {
            g_clear_error(&err);
            enabled = TRUE;
         }
      }

      if (enabled) {
         provider = sp->ctor();
         if (provider != NULL) {
            break;
         }
      }
   }

   ASSERT(provider != NULL);

   /* Instantiate the backup state and start the operation. */
   gBackupState = g_new0(VmBackupState, 1);
   gBackupState->ctx = data->appCtx;
   gBackupState->pollPeriod = 1000;
   gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
   gBackupState->provider = provider;

   if (data->argsSize > 0) {
      int generateManifests = 0;
      int index = 0;

      if (StrUtil_GetNextIntToken(&generateManifests, &index, data->args, " ")) {
         gBackupState->generateManifests = generateManifests;
      }

      if (data->args[index] != '\0') {
         gBackupState->volumes = g_strndup(data->args + index, data->argsSize - index);
      }
   }

   gBackupState->configDir = GuestApp_GetConfPath();
   if (gBackupState->configDir == NULL) {
      g_warning("Error getting configuration directory.");
      goto error;
   }

   VmBackup_SendEvent(VMBACKUP_EVENT_RESET, VMBACKUP_SUCCESS, "");

   if (!VmBackupStartScripts(VMBACKUP_SCRIPT_FREEZE)) {
      goto error;
   }

   /*
    * VC has a 15 minute timeout for quiesced snapshots. After that timeout,
    * it just discards the operation and sends an error to the caller. But
    * Tools can still keep running, blocking any new quiesced snapshot
    * requests. So we set up our own timer (which is configurable, in case
    * anyone wants to play with it), so that we abort any ongoing operation
    * if we also hit that timeout.
    *
    * See bug 506106.
    */
   timeout = (guint) g_key_file_get_integer(gBackupState->ctx->config,
                                            "vmbackup",
                                            "timeout",
                                            &err);
   if (err != NULL) {
      g_clear_error(&err);
      timeout = 15 * 60;
   }

   /* Treat "0" as no timeout. */
   if (timeout != 0) {
      gBackupState->abortTimer = g_timeout_source_new_seconds(timeout);
      VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,
                               gBackupState->abortTimer,
                               VmBackupAbortTimer,
                               NULL,
                               NULL);
   }

   VMBACKUP_ENQUEUE_EVENT();
   return RPCIN_SETRETVALS(data, "", TRUE);

error:
   gBackupState->provider->release(gBackupState->provider);
   g_free(gBackupState);
   return RPCIN_SETRETVALS(data, "Error initializing backup.", FALSE);
}


/**
 * Aborts the current operation if one is active, and stops the backup
 * process. If the sync provider has been activated, tell it to abort
 * the ongoing operation.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE
 */

static gboolean
VmBackupAbort(RpcInData *data)
{
   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState == NULL) {
      return RPCIN_SETRETVALS(data, "Error: no backup in progress", FALSE);
   }

   VmBackupDoAbort();
   return RPCIN_SETRETVALS(data, "", TRUE);
}


/**
 * Notifies the sync provider to thaw the file systems and puts the state
 * machine in the SYNC_THAW state.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE
 */

static gboolean
VmBackupSnapshotDone(RpcInData *data)
{
   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState == NULL) {
      return RPCIN_SETRETVALS(data, "Error: no backup in progress", FALSE);
   } else if (gBackupState->machineState != VMBACKUP_MSTATE_SYNC_FREEZE) {
      g_warning("Error: unexpected state for snapshot done message: %s",
                VmBackupGetStateName(gBackupState->machineState));
      return RPCIN_SETRETVALS(data,
                              "Error: unexpected state for snapshot done message.",
                              FALSE);
   } else {
      if (data->argsSize > 1) {
         gBackupState->snapshots = g_strndup(data->args + 1, data->argsSize - 1);
      }
      if (!gBackupState->provider->snapshotDone(gBackupState,
                                                gBackupState->provider->clientData)) {
         VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                            VMBACKUP_SYNC_ERROR,
                            "Error when notifying the sync provider.");
         if (VmBackupOnError()) {
            VmBackupFinalize();
         }
      } else {
         gBackupState->machineState = VMBACKUP_MSTATE_SYNC_THAW;
      }
      return RPCIN_SETRETVALS(data, "", TRUE);
   }
}


/**
 * Prints some information about the plugin's state to the log.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
VmBackupDumpState(gpointer src,
                  ToolsAppCtx *ctx,
                  gpointer data)
{
   if (gBackupState == NULL) {
      g_message("Backup is idle.\n");
   } else {
      g_message("Backup is in state: %s\n",
                VmBackupGetStateName(gBackupState->machineState));
   }
}


/**
 * Reset callback. Currently does nothing.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      Unused.
 * @param[in]  data     Unused.
 */

static void
VmBackupReset(gpointer src,
              ToolsAppCtx *ctx,
              gpointer data)
{
}


/**
 * Cleans up the plugin.
 *
 * @param[in]  src      The source object.
 * @param[in]  ctx      The app context.
 * @param[in]  data     Unused.
 */

static void
VmBackupShutdown(gpointer src,
                 ToolsAppCtx *ctx,
                 gpointer data)
{
   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState != NULL) {
      VmBackupFinalize();
   }
}


/**
 * Plugin entry point. Initializes internal plugin state.
 *
 * @param[in]  ctx   The app context.
 *
 * @return The registration data. Returns NULL if there is no sync provider
 *         available to quiesce the filesystems.
 */

TOOLS_MODULE_EXPORT ToolsPluginData *
ToolsOnLoad(ToolsAppCtx *ctx)
{
   static ToolsPluginData regData = {
      "vmbackup",
      NULL,
      NULL
   };

   RpcChannelCallback rpcs[] = {
      { VMBACKUP_PROTOCOL_START, VmBackupStart, NULL, NULL, NULL, 0 },
      { VMBACKUP_PROTOCOL_ABORT, VmBackupAbort, NULL, NULL, NULL, 0 },
      { VMBACKUP_PROTOCOL_SNAPSHOT_DONE, VmBackupSnapshotDone, NULL, NULL, NULL, 0 }
   };
   ToolsPluginSignalCb sigs[] = {
      { TOOLS_CORE_SIG_DUMP_STATE, VmBackupDumpState, NULL },
      { TOOLS_CORE_SIG_RESET, VmBackupReset, NULL },
      { TOOLS_CORE_SIG_SHUTDOWN, VmBackupShutdown, NULL },
   };
   ToolsAppReg regs[] = {
      { TOOLS_APP_GUESTRPC, VMTools_WrapArray(rpcs, sizeof *rpcs, ARRAYSIZE(rpcs)) },
      { TOOLS_APP_SIGNALS, VMTools_WrapArray(sigs, sizeof *sigs, ARRAYSIZE(sigs)) },
   };

#if defined(G_PLATFORM_WIN32)
   /*
    * If initializing COM fails (unlikely), we'll fallback to the sync driver
    * or the null provider, depending on the configuration.
    */
   if (!ToolsCore_InitializeCOM(ctx)) {
      g_warning("Failed to initialize COM, VSS support will be unavailable.");
   }
#endif

   regData.regs = VMTools_WrapArray(regs, sizeof *regs, ARRAYSIZE(regs));

   g_signal_new(TOOLS_CORE_SIG_IO_FREEZE,
                G_OBJECT_TYPE(ctx->serviceObj),
                0,
                0,
                NULL,
                NULL,
                g_cclosure_user_marshal_VOID__POINTER_BOOLEAN,
                G_TYPE_NONE,
                2,
                G_TYPE_POINTER,
                G_TYPE_BOOLEAN);

   return &regData;
}

