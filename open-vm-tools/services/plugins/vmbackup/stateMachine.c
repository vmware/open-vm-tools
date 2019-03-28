/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
#include "guestQuiesce.h"
#if !defined(_WIN32)
#include "vmware/tools/threadPool.h"
#endif
#include "vmware/tools/utils.h"
#include "vmware/tools/vmbackup.h"
#include "xdrutil.h"

#if !defined(__APPLE__)
#include "vm_version.h"
#include "embed_version.h"
#include "vmtoolsd_version.h"
VM_EMBED_VERSION(VMTOOLSD_VERSION_STRING);
#endif

#if defined(__linux__)
#include <sys/io.h>
#include <errno.h>
#include <string.h>
#include "ioplGet.h"
#endif

#define VMBACKUP_ENQUEUE_EVENT() do {                                         \
   gBackupState->timerEvent = g_timeout_source_new(gBackupState->pollPeriod); \
   VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,                                \
                            gBackupState->timerEvent,                         \
                            VmBackupAsyncCallback,                            \
                            NULL,                                             \
                            NULL);                                            \
} while (0)

/*
 * Macros to read values from config file.
 */
#define VMBACKUP_CONFIG_GET_BOOL(config, key, defVal)       \
   VMTools_ConfigGetBoolean(config, "vmbackup", key, defVal)

#define VMBACKUP_CONFIG_GET_STR(config, key, defVal)        \
   VMTools_ConfigGetString(config, "vmbackup", key, defVal)

#define VMBACKUP_CONFIG_GET_INT(config, key, defVal)        \
   VMTools_ConfigGetInteger(config, "vmbackup", key, defVal)

#define VMBACKUP_CFG_ENABLEVSS      "enableVSS"

static VmBackupState *gBackupState = NULL;

static Bool
VmBackupEnableSync(void);

static Bool
VmBackupEnableSyncWait(void);

static Bool
VmBackupEnableCompleteWait(void);


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

   case VMBACKUP_MSTATE_SYNC_FREEZE_WAIT:
      return "SYNC_FREEZE_WAIT";

   case VMBACKUP_MSTATE_SYNC_FREEZE:
      return "SYNC_FREEZE";

   case VMBACKUP_MSTATE_SYNC_THAW:
      return "SYNC_THAW";

   case VMBACKUP_MSTATE_SCRIPT_THAW:
      return "SCRIPT_THAW";

   case VMBACKUP_MSTATE_COMPLETE_WAIT:
      return "COMPLETE_WAIT";

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


#if defined(__linux__)
static Bool
VmBackupPrivSendMsg(gchar *msg,
                    char **result,
                    size_t *resultLen)
{
   Bool success;
   unsigned int oldLevel;

   ASSERT(gBackupState != NULL);

   g_debug("*** %s\n", __FUNCTION__);

   oldLevel = Iopl_Get();

   g_debug("Raising the IOPL, oldLevel=%u\n", oldLevel);
   if (iopl(3) < 0) {
      g_warning("Error raising the IOPL, %s\n", strerror(errno));
   }

   success = RpcChannel_Send(gBackupState->ctx->rpc, msg,
                             strlen(msg) + 1,
                             result, resultLen);

   if (iopl(oldLevel) < 0) {
      g_warning("Error restoring the IOPL, %s\n", strerror(errno));
   }

   return success;
}
#endif


/**
 * Sends a command to the VMX asking it to update VMDB about a new backup event.
 * This will restart the keep-alive timer.
 *
 * As the name implies, does not abort the quiesce operation on failure.
 *
 * @param[in]  event    The event to set.
 * @param[in]  code     Error code.
 * @param[in]  desc     Error description.
 *
 * @return TRUE on success.
 */

Bool
VmBackup_SendEventNoAbort(const char *event,
                          const uint32 code,
                          const char *desc)
{
   Bool success;
   char *result = NULL;
   size_t resultLen;
   gchar *msg;

   ASSERT(gBackupState != NULL);

   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState->keepAlive != NULL) {
      g_source_destroy(gBackupState->keepAlive);
      g_source_unref(gBackupState->keepAlive);
      gBackupState->keepAlive = NULL;
   }

   msg = g_strdup_printf(VMBACKUP_PROTOCOL_EVENT_SET" %s %u %s",
                         event, code, desc);
   g_debug("Sending vmbackup event: %s\n", msg);

#if defined(__linux__)
   if (gBackupState->needsPriv) {
      success = VmBackupPrivSendMsg(msg, &result, &resultLen);
   } else {
      success = RpcChannel_Send(gBackupState->ctx->rpc,
                                msg, strlen(msg) + 1,
                                &result, &resultLen);
      if (!success) {
         const char *privErr = "Guest is not privileged";
         if (resultLen > strlen(privErr) &&
             strncmp(result, privErr, strlen(privErr)) == 0) {
            g_debug("Failed to send event: %s\n", result);
            vm_free(result);

            /*
             * PR1444259:
             * Some hosts enforce privilege elevation for sending this
             * event, especially 5.5. This is Linux specific because
             * version 9.4.x on Linux only triggers host side check for
             * privilege elevation by sending iopl_elevation capability
             * to the host.
             */
            gBackupState->needsPriv = TRUE;

            g_debug("Sending event with priv: %s\n", msg);
            success = VmBackupPrivSendMsg(msg, &result, &resultLen);
         } else {
            gBackupState->needsPriv = FALSE;
         }
      }
   }
#else
   success = RpcChannel_Send(gBackupState->ctx->rpc,
                             msg, strlen(msg) + 1,
                             &result, &resultLen);
#endif

   if (success) {
      ASSERT(gBackupState->keepAlive == NULL);
      gBackupState->keepAlive =
         g_timeout_source_new(VMBACKUP_KEEP_ALIVE_PERIOD / 2);
      VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,
                               gBackupState->keepAlive,
                               VmBackupKeepAliveCallback,
                               NULL,
                               NULL);
   } else {
      g_warning("Failed to send vmbackup event: %s, result: %s.\n",
                msg, result);
   }
   vm_free(result);
   g_free(msg);

   return success;
}


/**
 * Sends a command to the VMX asking it to update VMDB about a new backup event.
 * This will restart the keep-alive timer.
 *
 * Aborts the quiesce operation on RPC failure.
 *
 * @param[in]  event    The event to set.
 * @param[in]  code     Error code.
 * @param[in]  desc     Error description.
 *
 * @return TRUE on success.
 */

Bool
VmBackup_SendEvent(const char *event,
                   const uint32 code,
                   const char *desc)
{
   Bool success = VmBackup_SendEventNoAbort(event, code, desc);

   if (!success  && gBackupState->rpcState != VMBACKUP_RPC_STATE_IGNORE) {
      g_debug("Changing rpcState from %d to %d\n",
              gBackupState->rpcState, VMBACKUP_RPC_STATE_ERROR);
      gBackupState->rpcState = VMBACKUP_RPC_STATE_ERROR;
   }

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

   g_mutex_lock(&gBackupState->opLock);
   if (gBackupState->currentOp != NULL) {
      VmBackup_Cancel(gBackupState->currentOp);
      VmBackup_Release(gBackupState->currentOp);
   }
   g_mutex_unlock(&gBackupState->opLock);

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
   if (gBackupState->completer != NULL) {
      gBackupState->completer->release(gBackupState->completer);
   }
   g_mutex_clear(&gBackupState->opLock);
   vm_free(gBackupState->configDir);
   g_free(gBackupState->scriptArg);
   g_free(gBackupState->volumes);
   g_free(gBackupState->snapshots);
   g_free(gBackupState->excludedFileSystems);
   g_free(gBackupState->errorMsg);
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

   if (gBackupState->execScripts &&
       !VmBackup_SetCurrentOp(gBackupState,
                              VmBackup_NewScriptOp(type, gBackupState),
                              NULL,
                              opName)) {
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SCRIPT_ERROR,
                         "Error when starting custom quiesce scripts.");
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

   case VMBACKUP_MSTATE_SYNC_FREEZE_WAIT:
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
   case VMBACKUP_MSTATE_COMPLETE_WAIT:
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

   /*
    * Once we abort the operation, we don't care about RPC state.
    */
   gBackupState->rpcState = VMBACKUP_RPC_STATE_IGNORE;

   if (gBackupState->machineState != VMBACKUP_MSTATE_SCRIPT_ERROR &&
       gBackupState->machineState != VMBACKUP_MSTATE_SYNC_ERROR) {
      const char *eventMsg = "Quiesce aborted.";
      /* Mark the current operation as cancelled. */
      g_mutex_lock(&gBackupState->opLock);
      if (gBackupState->currentOp != NULL) {
         VmBackup_Cancel(gBackupState->currentOp);
         VmBackup_Release(gBackupState->currentOp);
         gBackupState->currentOp = NULL;
      }
      g_mutex_unlock(&gBackupState->opLock);

#ifdef __linux__
      /* If quiescing has been completed, then undo it.  */
      if (gBackupState->machineState == VMBACKUP_MSTATE_SYNC_FREEZE) {
         g_debug("Aborting with file system already quiesced, undo quiescing "
                 "operation.\n");
         if (!gBackupState->provider->undo(gBackupState,
                                      gBackupState->provider->clientData)) {
            g_debug("Quiescing undo failed.\n");
            eventMsg = "Quiesce could not be aborted.";
         }
      }
#endif

      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ABORT,
                         VMBACKUP_REMOTE_ABORT,
                         eventMsg);

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
   g_source_unref(gBackupState->abortTimer);
   gBackupState->abortTimer = NULL;
   VmBackupDoAbort();
   return FALSE;
}


/**
 * Post-process the current operation. Calls the queued
 * operations as needed.
 *
 * @param[out]    pending     Tells if currentOp is pending.
 *
 * @return TRUE if currentOp finished or pending, FALSE on error.
 */

static gboolean
VmBackupPostProcessCurrentOp(gboolean *pending)
{
   gboolean retVal = TRUE;
   VmBackupOpStatus status = VMBACKUP_STATUS_FINISHED;

   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);

   *pending = FALSE;

   g_mutex_lock(&gBackupState->opLock);

   if (gBackupState->currentOp != NULL) {
      g_debug("%s: checking %s\n", __FUNCTION__, gBackupState->currentOpName);
      status = VmBackup_QueryStatus(gBackupState->currentOp);
   }

   switch (status) {
   case VMBACKUP_STATUS_PENDING:
      *pending = TRUE;
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
         gchar *msg;
         if (gBackupState->errorMsg != NULL) {
            msg = g_strdup_printf("'%s' operation failed: %s",
                                  gBackupState->currentOpName,
                                  gBackupState->errorMsg);
         } else {
            msg = g_strdup_printf("'%s' operation failed.",
                                  gBackupState->currentOpName);
         }
         VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                            VMBACKUP_UNEXPECTED_ERROR,
                            msg);
         g_free(msg);

         VmBackup_Release(gBackupState->currentOp);
         gBackupState->currentOp = NULL;
         retVal = FALSE;
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

      /*
       * Callback should not acquire opLock, but instead assume that
       * it holds the lock already.
       */
      if (cb(gBackupState)) {
         if (gBackupState->currentOp != NULL || gBackupState->forceRequeue) {
            goto exit;
         }
      } else {
         retVal = FALSE;
         goto exit;
      }
   }

exit:
   g_mutex_unlock(&gBackupState->opLock);
   return retVal;
}


/**
 * Callback to advance the state machine to next state once
 * current operation finishes.
 *
 * @param[in]  clientData     Unused.
 *
 * @return FALSE
 */

static gboolean
VmBackupAsyncCallback(void *clientData)
{
   gboolean opPending;
   g_debug("*** %s\n", __FUNCTION__);
   ASSERT(gBackupState != NULL);

   g_source_unref(gBackupState->timerEvent);
   gBackupState->timerEvent = NULL;

   /*
    * Move the state machine to the next state, if the
    * currentOp has finished.
    */
   if (!VmBackupPostProcessCurrentOp(&opPending)) {
      VmBackupOnError();
      goto exit;
   } else {
      /*
       * State transition can't be done if currentOp
       * has not finished yet.
       */
      if (opPending) {
         goto exit;
      }

      /*
       * VMX state might have changed when we were processing
       * currentOp. This is usually detected by failures in
       * sending backup event to the host.
       */
      if (gBackupState->rpcState == VMBACKUP_RPC_STATE_ERROR) {
         g_warning("Aborting backup operation due to RPC errors.");
         VmBackupDoAbort();
         goto exit;
      }
   }

   switch (gBackupState->machineState) {
   case VMBACKUP_MSTATE_SCRIPT_FREEZE:
      /* Next state is "sync freeze wait". */
      if (!VmBackupEnableSyncWait()) {
         VmBackupOnError();
      }
      break;

   case VMBACKUP_MSTATE_SYNC_FREEZE_WAIT:
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

   case VMBACKUP_MSTATE_SCRIPT_THAW:
      /* Next state is "complete wait" or "idle". */
      if (!VmBackupEnableCompleteWait()) {
         VmBackupOnError();
      }
      break;

   case VMBACKUP_MSTATE_SCRIPT_ERROR:
   case VMBACKUP_MSTATE_COMPLETE_WAIT:
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
 * Calls the sync provider's start function and moves the state
 * machine to next state.
 *
 * For Windows next state is VMBACKUP_MSTATE_SYNC_FREEZE, whereas
 * next state is VMBACKUP_MSTATE_SYNC_FREEZE_WAIT for Linux. Details
 * below.
 *
 * @return Whether the provider's start callback was successful.
 */

static Bool
VmBackupEnableSyncWait(void)
{
   g_debug("*** %s\n", __FUNCTION__);
   g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                         TOOLS_CORE_SIG_IO_FREEZE,
                         gBackupState->ctx,
                         TRUE);
#if defined(_WIN32)
   gBackupState->freezeStatus = VMBACKUP_FREEZE_FINISHED;
   if (!gBackupState->provider->start(gBackupState,
                                      gBackupState->provider->clientData)) {
#else
   /*
    * PR 1020224:
    * For Linux, FIFREEZE could take really long at times,
    * especially when guest is running IO load. We have also
    * seen slowness in performing open() on NFS mount points.
    * So, we need to run freeze operation in a separate thread
    * and track it with an extra state in the state machine.
    */
   gBackupState->freezeStatus = VMBACKUP_FREEZE_PENDING;
   if (!ToolsCorePool_SubmitTask(gBackupState->ctx,
                                 gBackupState->provider->start,
                                 gBackupState,
                                 NULL)) {
      g_warning("Failed to submit backup start task.");
#endif
      g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                            TOOLS_CORE_SIG_IO_FREEZE,
                            gBackupState->ctx,
                            FALSE);
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SYNC_ERROR,
                         "Error when enabling the sync provider.");
      return FALSE;
   }

#if defined(_WIN32)
   /* Move to next state */
   gBackupState->machineState = VMBACKUP_MSTATE_SYNC_FREEZE;
#else
   g_debug("Submitted backup start task.");
   /* Move to next state */
   gBackupState->machineState = VMBACKUP_MSTATE_SYNC_FREEZE_WAIT;
#endif

   return TRUE;
}


/**
 * Calls the completer's start function and moves the state
 * machine to next state.
 *
 * @return Whether the completer's start callback was successful.
 */

static Bool
VmBackupEnableCompleteWait(void)
{
   Bool ret = TRUE;

   g_debug("*** %s\n", __FUNCTION__);

   if (gBackupState->completer == NULL) {
      gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
      goto exit;
   }

   if (gBackupState->abortTimer != NULL) {
      g_source_destroy(gBackupState->abortTimer);
      g_source_unref(gBackupState->abortTimer);

      /* Host make timeout maximum as 15 minutes for complete process. */
      if (gBackupState->timeout > GUEST_QUIESCE_DEFAULT_TIMEOUT_IN_SEC) {
         gBackupState->timeout = GUEST_QUIESCE_DEFAULT_TIMEOUT_IN_SEC;
      }

      if (gBackupState->timeout != 0) {
         g_debug("Using completer timeout: %u\n", gBackupState->timeout);
         gBackupState->abortTimer =
            g_timeout_source_new_seconds(gBackupState->timeout);
         VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,
            gBackupState->abortTimer,
            VmBackupAbortTimer,
            NULL,
            NULL);
      }
   }

   if (gBackupState->completer->start(gBackupState,
                                      gBackupState->completer->clientData)) {
      /* Move to next state */
      gBackupState->machineState = VMBACKUP_MSTATE_COMPLETE_WAIT;
   } else {
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SYNC_ERROR,
                         "Error when enabling the sync provider.");
      ret = FALSE;
   }

exit:
   return ret;
}


/**
 * After sync provider's start function returns and moves the state
 * machine to VMBACKUP_MSTATE_SYNC_FREEZE state.
 *
 * @return Whether the provider's start callback was successful.
 */

static Bool
VmBackupEnableSync(void)
{
   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState->freezeStatus == VMBACKUP_FREEZE_ERROR) {
      g_signal_emit_by_name(gBackupState->ctx->serviceObj,
                            TOOLS_CORE_SIG_IO_FREEZE,
                            gBackupState->ctx,
                            FALSE);
      VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                         VMBACKUP_SYNC_ERROR,
                         "Error when enabling the sync provider.");
      return FALSE;

   } else if (gBackupState->freezeStatus == VMBACKUP_FREEZE_CANCELED ||
              gBackupState->freezeStatus == VMBACKUP_FREEZE_FINISHED) {
      /* Move to next state */
      gBackupState->machineState = VMBACKUP_MSTATE_SYNC_FREEZE;
   } else {
      ASSERT(gBackupState->freezeStatus == VMBACKUP_FREEZE_PENDING);
   }

   return TRUE;
}


/**
 * Starts the quiesce operation according to the supplied specification unless
 * some unexpected error occurs.
 *
 * @param[in]  data          RPC data.
 * @param[in]  forceQuiesce  Only allow Vss quiescing on Windows platform or
 *                           SyncDriver quiescing on Linux platform ( only file
 *                           system quiescing )
 *
 * @return TRUE on success.
 */

static gboolean
VmBackupStartCommon(RpcInData *data,
                    gboolean forceQuiesce)
{
   ToolsAppCtx *ctx = data->appCtx;
   VmBackupSyncProvider *provider = NULL;
   VmBackupSyncCompleter *completer = NULL;

   size_t i;

   /* List of available providers, in order of preference for loading. */
   struct SyncProvider {
      VmBackupSyncProvider *(*ctor)(void);
      const gchar *cfgEntry;
   } providers[] = {
#if defined(_WIN32)
      { VmBackup_NewVssProvider, VMBACKUP_CFG_ENABLEVSS},
#endif
      { VmBackup_NewSyncDriverProvider, "enableSyncDriver" },
      { VmBackup_NewNullProvider, NULL },
   };

   if (forceQuiesce) {
      if (gBackupState->quiesceApps || gBackupState->quiesceFS) {
         /*
          * If quiescing is requested on windows platform,
          * only allow VSS provider
          */
#if defined(_WIN32)
         if (VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                      VMBACKUP_CFG_ENABLEVSS, TRUE)) {
            provider = VmBackup_NewVssProvider();
            if (provider != NULL) {
               completer = VmBackup_NewVssCompleter(provider);
               if (completer == NULL) {
                  g_warning("VSS completion helper cannot be initialized.");
                  provider->release(provider);
                  provider = NULL;
               }
            }
         }
#elif defined(__linux__)
         /*
          * If quiescing is requested on linux platform,
          * only allow SyncDriver provider
          */
         if (gBackupState->quiesceFS &&
             VMBACKUP_CONFIG_GET_BOOL(ctx->config, "enableSyncDriver", TRUE)) {
            provider = VmBackup_NewSyncDriverOnlyProvider();
         }
#endif
      } else {
         /* If no quiescing is requested only allow null provider */
         provider = VmBackup_NewNullProvider();
      }
   } else {
      /* Instantiate the sync provider. */
      for (i = 0; i < ARRAYSIZE(providers); i++) {
         struct SyncProvider *sp = &providers[i];

         if (VMBACKUP_CONFIG_GET_BOOL(ctx->config, sp->cfgEntry, TRUE)) {
            provider = sp->ctor();
            if (provider != NULL) {
#if defined(_WIN32)
               if (sp->cfgEntry != NULL &&
                   Str_Strcmp(sp->cfgEntry, VMBACKUP_CFG_ENABLEVSS) == 0) {
                  completer = VmBackup_NewVssCompleter(provider);
                  if (completer == NULL) {
                     g_warning("VSS completion helper cannot be initialized.");
                     provider->release(provider);
                     provider = NULL;
                     continue;
                  }
                  break;
               }
#else
               break;
#endif
            }
         }
      }
   }

   if (provider == NULL) {
      g_warning("Requested quiescing cannot be initialized.");
      goto error;
   }

   /* Instantiate the backup state and start the operation. */
   gBackupState->ctx = data->appCtx;
   gBackupState->pollPeriod = 1000;
   gBackupState->machineState = VMBACKUP_MSTATE_IDLE;
   gBackupState->freezeStatus = VMBACKUP_FREEZE_FINISHED;
   gBackupState->provider = provider;
   gBackupState->completer = completer;
   gBackupState->needsPriv = FALSE;
   g_mutex_init(&gBackupState->opLock);
   gBackupState->enableNullDriver = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "enableNullDriver",
                                                             TRUE);
   gBackupState->rpcState = VMBACKUP_RPC_STATE_NORMAL;

   g_debug("Using quiesceApps = %d, quiesceFS = %d, allowHWProvider = %d,"
           " execScripts = %d, scriptArg = %s, timeout = %u,"
           " enableNullDriver = %d, forceQuiesce = %d\n",
           gBackupState->quiesceApps, gBackupState->quiesceFS,
           gBackupState->allowHWProvider, gBackupState->execScripts,
           (gBackupState->scriptArg != NULL) ? gBackupState->scriptArg : "",
           gBackupState->timeout, gBackupState->enableNullDriver, forceQuiesce);
#if defined(__linux__)
   gBackupState->excludedFileSystems =
         VMBACKUP_CONFIG_GET_STR(ctx->config, "excludedFileSystems", NULL);
   g_debug("Using excludedFileSystems = \"%s\"\n",
           (gBackupState->excludedFileSystems != NULL) ?
            gBackupState->excludedFileSystems : "(null)");
#endif
   g_debug("Quiescing volumes: %s",
           (gBackupState->volumes) ? gBackupState->volumes : "(null)");

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
    * First check if the timeout is specified by the RPC command, if not,
    * check the tools.conf file, otherwise use the default.
    *
    * See bug 506106.
    */
   if (gBackupState->timeout == 0) {
      gBackupState->timeout = VMBACKUP_CONFIG_GET_INT(ctx->config, "timeout",
                                       GUEST_QUIESCE_DEFAULT_TIMEOUT_IN_SEC);
   }

   /* Treat "0" as no timeout. */
   if (gBackupState->timeout != 0) {
      gBackupState->abortTimer =
          g_timeout_source_new_seconds(gBackupState->timeout);
      VMTOOLSAPP_ATTACH_SOURCE(gBackupState->ctx,
                               gBackupState->abortTimer,
                               VmBackupAbortTimer,
                               NULL,
                               NULL);
   }

   VMBACKUP_ENQUEUE_EVENT();
   return RPCIN_SETRETVALS(data, "", TRUE);

error:
   if (gBackupState->keepAlive != NULL) {
      g_source_destroy(gBackupState->keepAlive);
      g_source_unref(gBackupState->keepAlive);
      gBackupState->keepAlive = NULL;
   }
   if (gBackupState->provider) {
      gBackupState->provider->release(gBackupState->provider);
   }
   if (gBackupState->completer) {
      gBackupState->completer->release(gBackupState->completer);
   }
   vm_free(gBackupState->configDir);
   g_free(gBackupState->scriptArg);
   g_free(gBackupState->volumes);
   g_free(gBackupState);
   gBackupState = NULL;
   return RPCIN_SETRETVALS(data, "Error initializing quiesce operation.",
                           FALSE);
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
   ToolsAppCtx *ctx = data->appCtx;

   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState != NULL) {
      return RPCIN_SETRETVALS(data, "Quiesce operation already in progress.",
                              FALSE);
   }
   gBackupState = g_new0(VmBackupState, 1);
   if (data->argsSize > 0) {
      int generateManifests = 0;
      uint32 index = 0;

      if (StrUtil_GetNextIntToken(&generateManifests, &index, data->args, " ")) {
         gBackupState->generateManifests = generateManifests;
      }
      gBackupState->quiesceApps = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                           "quiesceApps",
                                                           TRUE);
      gBackupState->quiesceFS = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                         "quiesceFS",
                                                         TRUE);
      gBackupState->allowHWProvider = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "allowHWProvider",
                                                             TRUE);
      gBackupState->execScripts = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                           "execScripts",
                                                           TRUE);
      gBackupState->scriptArg = VMBACKUP_CONFIG_GET_STR(ctx->config,
                                                        "scriptArg",
                                                        NULL);
      gBackupState->timeout = VMBACKUP_CONFIG_GET_INT(ctx->config,
                                                      "timeout", 0);
      gBackupState->vssUseDefault = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "vssUseDefault",
                                                             TRUE);

      /* get volume uuids if provided */
      if (data->args[index] != '\0') {
         gBackupState->volumes = g_strndup(data->args + index,
                                           data->argsSize - index);
      }
   }
   return VmBackupStartCommon(data, VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "forceQuiesce",
                                                             FALSE));
}


/**
 * Handler for the "vmbackup.startWithOpts" message. Starts processing the
 * quiesce operation according to the supplied specification unless there's
 * another backup operation going on or some other unexpected error occurs.
 *
 * . If createManifest is true, the guest generates a manifest about the
 *   application involved during quiescing.
 * . If quiesceApps is true, the guest involves applications during
 *   quiescing. If quiesceFS is true, the guest performs file system
 *   quiescing. If both quiesceApps and quiesceFS are true, the guest
 *   falls back to file system quiescing if application quiescing is not
 *   supported in the guest. If both quiesceApps and quiesceFS are false,
 *   the guest performs no quiescing but will still run the custom scripts
 *   provided execScripts is true.
 * . If writableSnapshot is true, the guest assumes that writable snapshot
 *   based quiescing can be performed.
 * . If execScripts is true, the guest calls pre-freeze and post-thaw
 *   scripts before and after quiescing.
 * . The scriptArg string is passed to the pre-freeze and post-thaw scripts
 *   as an argument so that the scripts can be configured to perform
 *   actions based this argument.
 * . The timeout in seconds overrides the default timeout of 15 minutes
 *   that the guest uses to abort a long quiesce operation. If the timeout
 *   is 0, the default timeout is used.
 * . The volumes argument is a list of diskUuids separated by space.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE on success.
 */

static gboolean
VmBackupStartWithOpts(RpcInData *data)
{
   ToolsAppCtx *ctx = data->appCtx;
   GuestQuiesceParams *params;
   GuestQuiesceParamsV1 *paramsV1 = NULL;
   GuestQuiesceParamsV2 *paramsV2;
   gboolean retval;

   g_debug("*** %s\n", __FUNCTION__);
   if (gBackupState != NULL) {
      return RPCIN_SETRETVALS(data, "Quiesce operation already in progress.",
                              FALSE);
   }
   params = (GuestQuiesceParams *)data->args;

#if defined(_WIN32)
   if (params->ver != GUESTQUIESCEPARAMS_V1 &&
       params->ver != GUESTQUIESCEPARAMS_V2) {
      g_warning("%s: Incompatible quiesce parameter version. \n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Incompatible quiesce parameter version",
                              FALSE);
   }
#else
   if (params->ver != GUESTQUIESCEPARAMS_V1) {
      g_warning("%s: Incompatible quiesce parameter version. \n", __FUNCTION__);
      return RPCIN_SETRETVALS(data, "Incompatible quiesce parameter version",
                              FALSE);
   }
#endif

   gBackupState = g_new0(VmBackupState, 1);

   if (params->ver == GUESTQUIESCEPARAMS_V1) {
      paramsV1 = params->GuestQuiesceParams_u.guestQuiesceParamsV1;
      gBackupState->vssUseDefault = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "vssUseDefault",
                                                             TRUE);
   } else if (params->ver == GUESTQUIESCEPARAMS_V2) {
      paramsV2 = params->GuestQuiesceParams_u.guestQuiesceParamsV2;
      paramsV1 = &paramsV2->paramsV1;
      gBackupState->vssBackupContext = paramsV2->vssBackupContext;
      gBackupState->vssBackupType = paramsV2->vssBackupType;
      gBackupState->vssBootableSystemState = paramsV2->vssBootableSystemState;
      gBackupState->vssPartialFileSupport = paramsV2->vssPartialFileSupport;
      gBackupState->vssUseDefault = VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                             "vssUseDefault",
                                                             FALSE);
   }

   if (paramsV1 != NULL) {
      gBackupState->generateManifests = paramsV1->createManifest;
      gBackupState->quiesceApps = paramsV1->quiesceApps;
      gBackupState->quiesceFS = paramsV1->quiesceFS;
      gBackupState->allowHWProvider = paramsV1->writableSnapshot;
      gBackupState->execScripts = paramsV1->execScripts;
      gBackupState->scriptArg = g_strndup(paramsV1->scriptArg,
                                          strlen(paramsV1->scriptArg));
      gBackupState->timeout = paramsV1->timeout;
      gBackupState->volumes = g_strndup(paramsV1->diskUuids,
                                        strlen(paramsV1->diskUuids));
   }

   retval = VmBackupStartCommon(data, VMBACKUP_CONFIG_GET_BOOL(ctx->config,
                                                               "forceQuiesce",
                                                               TRUE));
   return retval;
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
      return RPCIN_SETRETVALS(data, "Error: no quiesce operation in progress",
                              FALSE);
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
      return RPCIN_SETRETVALS(data, "Error: no quiesce operation in progress", FALSE);
   } else if (gBackupState->machineState != VMBACKUP_MSTATE_SYNC_FREEZE) {
      g_warning("Error: unexpected state for snapshot done message: %s",
                VmBackupGetStateName(gBackupState->machineState));
      return RPCIN_SETRETVALS(data,
                              "Error: unexpected state for quiesce done message.",
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
 * Notifies the completer to complete the provider process
 * machine in the COMPLETE_WAIT state.
 *
 * @param[in]  data     RPC data.
 *
 * @return TRUE
 */

static gboolean
VmBackupSnapshotCompleted(RpcInData *data)
{
   g_debug("*** %s\n", __FUNCTION__);

   if (gBackupState == NULL ||
       gBackupState->completer == NULL) {
      return RPCIN_SETRETVALS(data, "Error: no quiesce complete in progress",
                              FALSE);
   } else if (gBackupState->machineState != VMBACKUP_MSTATE_COMPLETE_WAIT) {
      g_warning("Error: unexpected state for snapshot complete message: %s",
                VmBackupGetStateName(gBackupState->machineState));
      return RPCIN_SETRETVALS(data,
                              "Error: unexpected state for complete message.",
                              FALSE);
   } else {
      if (!gBackupState->completer->snapshotCompleted(gBackupState,
             gBackupState->completer->clientData)) {
         VmBackup_SendEvent(VMBACKUP_EVENT_REQUESTOR_ERROR,
                            VMBACKUP_SYNC_ERROR,
                            "Error when notifying the sync completer.");
         if (VmBackupOnError()) {
            VmBackupFinalize();
         }
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
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN, "Backup is idle.\n");
   } else {
      ToolsCore_LogState(TOOLS_STATE_LOG_PLUGIN,
                         "Backup is in state: %s\n",
                         VmBackupGetStateName(gBackupState->machineState));
   }
}


/**
 * Reset callback.  Currently does nothing.
 *
 * @param[in]  src      The source object.  Unused.
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
      /* START_WITH_OPTS command supported only on Windows for now */
      { VMBACKUP_PROTOCOL_START_WITH_OPTS, VmBackupStartWithOpts, NULL,
                    xdr_GuestQuiesceParams, NULL, sizeof (GuestQuiesceParams) },
      { VMBACKUP_PROTOCOL_ABORT, VmBackupAbort, NULL, NULL, NULL, 0 },
      { VMBACKUP_PROTOCOL_SNAPSHOT_COMPLETED, VmBackupSnapshotCompleted, NULL,
                    NULL, NULL, 0 },
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
    * or the null provider, depending on the configuration. On success, send
    * a request to unregister the VMware snapshot provider.
    */
   if (ToolsCore_InitializeCOM(ctx)) {
      VmBackup_UnregisterSnapshotProvider();
   } else {
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

