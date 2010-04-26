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

/*
 * syncDriverOps.c --
 *
 * Implements the sync driver provider for the backup state machine.
 */

#include "vmBackupInt.h"

#include "file.h"
#include "procMgr.h"
#include "syncDriver.h"
#include "util.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

typedef struct VmBackupDriverOp {
   VmBackupOp callbacks;
   const char *volumes;
   Bool freeze;
   Bool canceled;
   SyncDriverHandle *syncHandle;
} VmBackupDriverOp;


/*
 *-----------------------------------------------------------------------------
 *
 * VmBackupDriverThaw --
 *
 *    Thaws the frozen filesystems, and cleans up internal state kept by the
 *    code.
 *
 * Results:
 *    Whether thawing was successful.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VmBackupDriverThaw(VmBackupDriverOp *op)
{
   Bool success = SyncDriver_Thaw(*op->syncHandle);
   SyncDriver_CloseHandle(op->syncHandle);
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupDriverOpQuery --
 *
 *    Checks the status of the operation that is enabling or disabling the
 *    sync driver.
 *
 * Result
 *    VMBACKUP_STATUS_PENDING:   still working.
 *    VMBACKUP_STATUS_FINISHED:  done.
 *    VMBACKUP_STATUS_CANCELED:  cancel request fulfilled.
 *    VMBACKUP_STATUS_ERROR:     oops.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VmBackupOpStatus
VmBackupDriverOpQuery(VmBackupOp *_op) // IN
{
   VmBackupDriverOp *op = (VmBackupDriverOp *) _op;
   VmBackupOpStatus ret;

   if (op->freeze) {
      SyncDriverStatus st = SyncDriver_QueryStatus(*op->syncHandle, 0);

      g_debug("SyncDriver status: %d\n", st);
      switch(st) {
      case SYNCDRIVER_BUSY:
         ret = VMBACKUP_STATUS_PENDING;
         break;

      case SYNCDRIVER_IDLE:
         if (op->canceled) {
            VmBackupDriverThaw(op);
         }
         /*
          * This prevents the release callback from freeing the handle, which
          * will be used when thawing in the POSIX case.
          */
         op->syncHandle = NULL;
         ret = (op->canceled) ? VMBACKUP_STATUS_CANCELED : VMBACKUP_STATUS_FINISHED;
         break;

      default:
         VmBackupDriverThaw(op);
         ret = VMBACKUP_STATUS_ERROR;
         break;
      }
   } else {
      ret = VMBACKUP_STATUS_FINISHED;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupDriverOpRelease --
 *
 *    Cleans up data held by the state object.
 *
 * Result
 *    The status of the app.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmBackupDriverOpRelease(VmBackupOp *_op)  // IN
{
   VmBackupDriverOp *op = (VmBackupDriverOp *) _op;
   free(op->syncHandle);
   free(op);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupDriverOpCancel --
 *
 *    Cancel an ongoing sync driver operation. This doesn't actually
 *    cancel the operation, but rather waits for it to finish, since
 *    just killing the worker thread might have undesired side effects.
 *    This will not cancel thaw operations.
 *
 * Result
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmBackupDriverOpCancel(VmBackupOp *_op)   // IN
{
   VmBackupDriverOp *op = (VmBackupDriverOp *) _op;
   op->canceled = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupNewDriverOp --
 *
 *    Enables or disables the sync driver. Note: "volumes" is not copied,
 *    to avoid unnecessary waste of memory, since it's kept in the global
 *    backup state structure.
 *
 * Result
 *    A state object, unless some error occurs.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VmBackupDriverOp *
VmBackupNewDriverOp(VmBackupState *state,       // IN
                    Bool freeze,                // IN
                    SyncDriverHandle *handle,   // IN
                    const char *volumes)        // IN
{
   Bool success;
   VmBackupDriverOp *op = NULL;

   g_return_val_if_fail((handle == NULL || *handle == SYNCDRIVER_INVALID_HANDLE) ||
                        !freeze,
                        NULL);

   op = Util_SafeMalloc(sizeof *op);
   memset(op, 0, sizeof *op);

   op->callbacks.queryFn = VmBackupDriverOpQuery;
   op->callbacks.cancelFn = VmBackupDriverOpCancel;
   op->callbacks.releaseFn = VmBackupDriverOpRelease;
   op->freeze = freeze;
   op->volumes = volumes;

   op->syncHandle = g_new0(SyncDriverHandle, 1);
   *op->syncHandle = (handle != NULL) ? *handle : SYNCDRIVER_INVALID_HANDLE;

   if (freeze) {
      success = SyncDriver_Freeze(op->volumes, op->syncHandle);
   } else {
      success = VmBackupDriverThaw(op);
   }
   if (!success) {
      g_warning("Error %s filesystems.", freeze ? "freezing" : "thawing");
      free(op);
      op = NULL;
   }
   return op;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverReadyForSnapshot --
 *
 *    Sends an event to the VMX indicating that the guest is ready for a
 *    snapshot to be taken (i.e., scripts have run and sync driver is
 *    enabled).
 *
 * Result
 *    TRUE, unless sending the message fails.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VmBackupSyncDriverReadyForSnapshot(VmBackupState *state)
{
   SyncDriverHandle *handle = state->clientData;

   g_debug("*** %s\n", __FUNCTION__);
   if (handle != NULL && *handle != SYNCDRIVER_INVALID_HANDLE) {
      return VmBackup_SendEvent(VMBACKUP_EVENT_SNAPSHOT_COMMIT, 0, "");
   }
   return TRUE;
}


/* Sync provider implementation. */


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverStart --
 *
 *    Starts an asynchronous operation to enable the sync driver.
 *
 * Result
 *    TRUE, unless an error occurs.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VmBackupSyncDriverStart(VmBackupState *state,
                        void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);
   op = VmBackupNewDriverOp(state, TRUE, NULL, state->volumes);

   if (op != NULL) {
      state->clientData = op->syncHandle;
   }

   return VmBackup_SetCurrentOp(state,
                                (VmBackupOp *) op,
                                VmBackupSyncDriverReadyForSnapshot,
                                __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverSnapshotDone --
 *
 *    Starts an asynchronous operation to disable the sync driver.
 *
 * Result
 *    TRUE, unless an error occurs.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VmBackupSyncDriverSnapshotDone(VmBackupState *state,
                               void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);

   op = VmBackupNewDriverOp(state, FALSE, state->clientData, NULL);
   g_free(state->clientData);
   state->clientData = NULL;

   return VmBackup_SetCurrentOp(state, (VmBackupOp *) op, NULL, __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverRelease --
 *
 *    Frees the given pointer.
 *
 * Result
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VmBackupSyncDriverRelease(struct VmBackupSyncProvider *provider)
{
   free(provider);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_NewSyncDriverProvider --
 *
 *    Returns a new VmBackupSyncProvider that will enable the sync driver
 *    as part of the "sync" operation of a backup.
 *
 * Result
 *    NULL on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

VmBackupSyncProvider *
VmBackup_NewSyncDriverProvider(void)
{
   VmBackupSyncProvider *provider;

   if (!SyncDriver_Init()) {
      g_debug("Error initializing the sync driver.\n");
      return NULL;
   }

   provider = Util_SafeMalloc(sizeof *provider);
   provider->start = VmBackupSyncDriverStart;
   provider->snapshotDone = VmBackupSyncDriverSnapshotDone;
   provider->release = VmBackupSyncDriverRelease;
   provider->clientData = NULL;

   return provider;
}

