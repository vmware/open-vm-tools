/*********************************************************
 * Copyright (C) 2007-2019, 2021 VMware, Inc. All rights reserved.
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
#include "syncManifest.h"

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#endif

/*
 * Define an enumeration type VmBackupOpType and a corresponding array
 * VmBackupOpName whose entries provide the printable names of the
 * enumeration ids in VmBackupOpType.
 *
 * VmBackupOpType and VmBackupOpName are each defined as an invocation
 * of a macro VMBACKUP_OPLIST.  VMBACKUP_OPLIST specifies a list of
 * enumeration ids using a macro VMBACKUP_OP that must be defined before
 * invoking VMBACKUP_OPLIST.  VMBACKUP_OP takes a single argument, which
 * should be an enumeration id, and is defined to generate from the id
 * either the id itself or a string to be used as its printable name.  The
 * result is that an invocation of VMBACKUP_OPLIST generates either the
 * list of enumeration ids or the list of their printable names.
 */
#define VMBACKUP_OPLIST         \
    VMBACKUP_OP(OP_FREEZE),     \
    VMBACKUP_OP(OP_THAW),       \
    VMBACKUP_OP(OP_UNDO),

#define VMBACKUP_OPID(id)       id
#define VMBACKUP_OPNAME(id)     #id

#undef VMBACKUP_OP
#define VMBACKUP_OP(id)       VMBACKUP_OPID(id)

typedef enum {
   VMBACKUP_OPLIST
} VmBackupOpType;

#undef VMBACKUP_OP
#define VMBACKUP_OP(id)       VMBACKUP_OPNAME(id)

static const char *VmBackupOpName[] = {
   VMBACKUP_OPLIST
};

#undef VMBACKUP_OP

typedef struct VmBackupDriverOp {
   VmBackupOp callbacks;
   const char *volumes;
   VmBackupOpType opType;
   Bool canceled;
   SyncDriverHandle *syncHandle;
   SyncManifest *manifest;
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
VmBackupDriverThaw(SyncDriverHandle *handle)
{
   Bool success = SyncDriver_Thaw(*handle);
   SyncDriver_CloseHandle(handle);
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

   if (op->opType == OP_FREEZE) {
      SyncDriverStatus st = SyncDriver_QueryStatus(*op->syncHandle, 0);

      g_debug("SyncDriver status: %d\n", st);
      switch(st) {
      case SYNCDRIVER_BUSY:
         ret = VMBACKUP_STATUS_PENDING;
         break;

      case SYNCDRIVER_IDLE:
         if (op->canceled) {
            VmBackupDriverThaw(op->syncHandle);
         }
         /*
          * This prevents the release callback from freeing the handle, which
          * will be used when thawing in the POSIX case.
          */
         op->syncHandle = NULL;
         ret = (op->canceled) ? VMBACKUP_STATUS_CANCELED : VMBACKUP_STATUS_FINISHED;
         break;

      default:
         VmBackupDriverThaw(op->syncHandle);
         ret = VMBACKUP_STATUS_ERROR;
         break;
      }
   } else {
      if (op->manifest != NULL) {
         SyncManifestSend(op->manifest);
      }
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

   g_free(op->syncHandle);
   SyncManifestRelease(op->manifest);
   free(op);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupDriverOpCancel --
 *
 *    Cancel an ongoing sync driver operation. This doesn't actually
 *    cancel the operation, but rather waits for it to finish, since
 *    just forcing the worker thread to quit might have undesired side
 *    effects.  This will not cancel thaw operations.
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
                    VmBackupOpType opType,      // IN
                    SyncDriverHandle *handle,   // IN
                    const char *volumes,        // IN
                    Bool useNullDriverPrefs)    // IN
{
   Bool success;
   VmBackupDriverOp *op = NULL;

   g_return_val_if_fail((handle == NULL ||
                         *handle == SYNCDRIVER_INVALID_HANDLE) ||
                        opType != OP_FREEZE,
                        NULL);

   op = Util_SafeMalloc(sizeof *op);
   memset(op, 0, sizeof *op);

   op->callbacks.queryFn = VmBackupDriverOpQuery;
   op->callbacks.cancelFn = VmBackupDriverOpCancel;
   op->callbacks.releaseFn = VmBackupDriverOpRelease;
   op->opType = opType;
   op->volumes = volumes;

   op->syncHandle = g_new0(SyncDriverHandle, 1);
   *op->syncHandle = (handle != NULL) ? *handle : SYNCDRIVER_INVALID_HANDLE;

   switch (opType) {
      case OP_FREEZE:
         success = SyncDriver_Freeze(op->volumes,
                                     useNullDriverPrefs ?
                                        state->enableNullDriver : FALSE,
                                     op->syncHandle,
                                     state->excludedFileSystems);
         break;
      case OP_THAW:
         op->manifest = SyncNewManifest(state, *op->syncHandle);
         success = VmBackupDriverThaw(op->syncHandle);
         break;
      default:
         ASSERT(opType == OP_UNDO);
         success = VmBackupDriverThaw(op->syncHandle);
         break;
   }
   if (!success) {
      g_warning("Error trying to perform %s on filesystems.",
                VmBackupOpName[opType]);
      g_free(op->syncHandle);
      SyncManifestRelease(op->manifest);
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
      Bool success;
      success = VmBackup_SendEvent(VMBACKUP_EVENT_SNAPSHOT_COMMIT, 0, "");
      if (success) {
         state->freezeStatus = VMBACKUP_FREEZE_FINISHED;
      } else {
         /*
          * If the vmx does not know this event (e.g. due to an RPC timeout),
          * then filesystems need to be thawed here because snapshotDone
          * will not be sent by the vmx.
          */
         g_debug("VMX state changed; thawing filesystems.\n");
         if (!VmBackupDriverThaw(handle)) {
            g_warning("Error thawing filesystems.\n");
         }
         state->freezeStatus = VMBACKUP_FREEZE_ERROR;
      }
      return success;
   }

   /* op failed */
   state->freezeStatus = VMBACKUP_FREEZE_ERROR;
   return TRUE;
}


/* Sync provider implementation. */

#if defined(_WIN32)
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
   op = VmBackupNewDriverOp(state, OP_FREEZE, NULL, state->volumes, TRUE);

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
 *  VmBackupSyncDriverOnlyStart --
 *
 *    Starts an asynchronous operation to enable the sync driver without using
 *    NullDriver fallback.
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
VmBackupSyncDriverOnlyStart(VmBackupState *state,
                            void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);
   op = VmBackupNewDriverOp(state, OP_FREEZE, NULL, state->volumes, FALSE);

   if (op != NULL) {
      state->clientData = op->syncHandle;
   }

   return VmBackup_SetCurrentOp(state,
                                (VmBackupOp *) op,
                                VmBackupSyncDriverReadyForSnapshot,
                                __FUNCTION__);
}

#else

/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverStart --
 *
 *    Starts an asynchronous operation to enable the sync driver.
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
VmBackupSyncDriverStart(ToolsAppCtx *ctx,
                        void *clientData)
{
   VmBackupDriverOp *op;
   VmBackupState *state = (VmBackupState*) clientData;

   g_debug("*** %s\n", __FUNCTION__);
   op = VmBackupNewDriverOp(state, OP_FREEZE, NULL, state->volumes, TRUE);

   if (op != NULL) {
      state->clientData = op->syncHandle;
   }

   VmBackup_SetCurrentOp(state,
                         (VmBackupOp *) op,
                         VmBackupSyncDriverReadyForSnapshot,
                         __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverOnlyStart --
 *
 *    Starts an asynchronous operation to enable the sync driver without using
 *    NullDriver fallback.
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
VmBackupSyncDriverOnlyStart(ToolsAppCtx *ctx,
                            void *clientData)
{
   VmBackupDriverOp *op;
   VmBackupState *state = (VmBackupState*) clientData;

   g_debug("*** %s\n", __FUNCTION__);
   op = VmBackupNewDriverOp(state, OP_FREEZE, NULL, state->volumes, FALSE);

   if (op != NULL) {
      state->clientData = op->syncHandle;
   }

   VmBackup_SetCurrentOp(state,
                         (VmBackupOp *) op,
                         VmBackupSyncDriverReadyForSnapshot,
                         __FUNCTION__);
}
#endif


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

   op = VmBackupNewDriverOp(state, OP_THAW, state->clientData, NULL, TRUE);
   g_free(state->clientData);
   state->clientData = NULL;

   return VmBackup_SetCurrentOp(state, (VmBackupOp *) op, NULL, __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverOnlySnapshotDone --
 *
 *    Starts an asynchronous operation to disable the sync driver
 *    that does not fallback to NullDriver.
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
VmBackupSyncDriverOnlySnapshotDone(VmBackupState *state,
                                   void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);

   op = VmBackupNewDriverOp(state, OP_THAW, state->clientData, NULL, FALSE);
   g_free(state->clientData);
   state->clientData = NULL;

   return VmBackup_SetCurrentOp(state, (VmBackupOp *) op, NULL, __FUNCTION__);
}


#if defined(__linux__)
/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverUndo --
 *
 *    Undo a completed quiescing operation.
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
VmBackupSyncDriverUndo(VmBackupState *state,
                       void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);

   op = VmBackupNewDriverOp(state, OP_UNDO, state->clientData, NULL, TRUE);
   g_free(state->clientData);
   state->clientData = NULL;

   return VmBackup_SetCurrentOp(state, (VmBackupOp *) op, NULL, __FUNCTION__);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackupSyncDriverOnlyUndo --
 *
 *    Undo a completed quiescing operation.
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
VmBackupSyncDriverOnlyUndo(VmBackupState *state,
                           void *clientData)
{
   VmBackupDriverOp *op;

   g_debug("*** %s\n", __FUNCTION__);

   op = VmBackupNewDriverOp(state, OP_UNDO, state->clientData, NULL, FALSE);
   g_free(state->clientData);
   state->clientData = NULL;

   return VmBackup_SetCurrentOp(state, (VmBackupOp *) op, NULL, __FUNCTION__);
}
#endif


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
 *  VmBackup_NewSyncDriverProviderInternal --
 *
 *    Returns a new VmBackupSyncProvider that will enable the sync driver
 *    as part of the "sync" operation of a backup. If useNullDriverPrefs is
 *    set to TRUE, VmBackupSyncProvider created will fallback (if required)
 *    to NullDriver based on the preferences. If useNullDriverPrefs is set
 *    to FALSE, VmBackupSyncProvider created will ignore the preferences and
 *    have its' fixed behavior, which is to not use NullDriver.
 *
 * Result
 *    NULL on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VmBackupSyncProvider *
VmBackup_NewSyncDriverProviderInternal(Bool useNullDriverPrefs)
{
   VmBackupSyncProvider *provider;

   if (!SyncDriver_Init()) {
      g_debug("Error initializing the sync driver.\n");
      return NULL;
   }

   provider = Util_SafeMalloc(sizeof *provider);
   if (useNullDriverPrefs) {
      provider->start = VmBackupSyncDriverStart;
      provider->snapshotDone = VmBackupSyncDriverSnapshotDone;
#if defined(__linux__)
      provider->undo = VmBackupSyncDriverUndo;
#endif
   } else {
      provider->start = VmBackupSyncDriverOnlyStart;
      provider->snapshotDone = VmBackupSyncDriverOnlySnapshotDone;
#if defined(__linux__)
      provider->undo = VmBackupSyncDriverOnlyUndo;
#endif
   }

   provider->release = VmBackupSyncDriverRelease;
   provider->clientData = NULL;

   return provider;
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_NewSyncDriverProvider --
 *
 *    Returns a new VmBackupSyncProvider that will enable the sync driver
 *    as part of the "sync" operation of a backup. This provider uses
 *    NullDriver fallback based on the preferences set in tools.conf.
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
   return VmBackup_NewSyncDriverProviderInternal(TRUE);
}


#if defined(__linux__)

/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_NewSyncDriverOnlyProvider --
 *
 *    Returns a new VmBackupSyncProvider that will enable the sync driver
 *    as part of the "sync" operation of a backup. This provider does not
 *    use NullDriver fallback (ignores the preferences set in tools.conf).
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
VmBackup_NewSyncDriverOnlyProvider(void)
{
   return VmBackup_NewSyncDriverProviderInternal(FALSE);
}

#endif
