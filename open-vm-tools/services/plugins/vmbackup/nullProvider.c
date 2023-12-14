/*********************************************************
 * Copyright (c) 2010-2016, 2022 VMware, Inc. All rights reserved.
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
 * @file nullProvider.c
 *
 * Implements a sync provider that doesn't really do anything, so that we can at
 * least run freeze / thaw scripts if no lower-level freeze functionality is
 * available.
 */

#if !defined(_WIN32)
#  include <unistd.h>
#endif

#include "vmBackupInt.h"


#if defined(_WIN32)

/*
 ******************************************************************************
 * VmBackupNullStart --                                                 */ /**
 *
 * Sends the "commit snapshot" event to the host.
 *
 * @param[in] state         Backup state.
 * @param[in] clientData    Unused.
 *
 * @return Whether successfully sent the signal to the host.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullStart(VmBackupState *state,
                  void *clientData)
{
   VmBackup_SetCurrentOp(state, NULL, NULL, __FUNCTION__);
   return VmBackup_SendEvent(VMBACKUP_EVENT_SNAPSHOT_COMMIT, 0, "");
}


/*
 ******************************************************************************
 * VmBackupNullSnapshotDone --                                          */ /**
 *
 * Does nothing, just keep the backup state machine alive.
 *
 * @param[in] state         Backup state.
 * @param[in] clientData    Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullSnapshotDone(VmBackupState *state,
                         void *clientData)
{
   VmBackup_SetCurrentOp(state, NULL, NULL, __FUNCTION__);
   return TRUE;
}

#else

/*
 ******************************************************************************
 * VmBackupNullReadyForSnapshot --                                      */ /**
 *
 * Sends an event to the VMX indicating that the guest is ready for a
 * snapshot to be taken (i.e., scripts have run and Nulldriver is
 * enabled).
 *
 * @param[in] state         Backup state.
 *
 * @return TRUE, unless sending the message fails.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullReadyForSnapshot(VmBackupState *state)
{
   Bool success;

   g_debug("*** %s\n", __FUNCTION__);
   success = VmBackup_SendEvent(VMBACKUP_EVENT_SNAPSHOT_COMMIT, 0, "");
   if (success) {
      state->freezeStatus = VMBACKUP_FREEZE_FINISHED;
   } else {
      g_warning("Failed to send commit event to host");
      state->freezeStatus = VMBACKUP_FREEZE_ERROR;
   }
   return success;
}


/*
 ******************************************************************************
 * VmBackupNullOpQuery --                                               */ /**
 *
 * Checks the status of the operation that is enabling or disabling the
 * Null driver. Nulldriver is enabled immediately and there is nothing
 * to disable.
 *
 * @param[in] op        VmBackupOp.
 *
 * @return VMBACKUP_STATUS_FINISHED always.
 *
 ******************************************************************************
 */

static VmBackupOpStatus
VmBackupNullOpQuery(VmBackupOp *op) // IN
{
   return VMBACKUP_STATUS_FINISHED;
}


/*
 ******************************************************************************
 * VmBackupNullOpRelease --                                             */ /**
 *
 * Cleans up data held by the op object.
 *
 * @param[in] op        VmBackupOp.
 *
 ******************************************************************************
 */

static void
VmBackupNullOpRelease(VmBackupOp *op)  // IN
{
   g_free(op);
}


/*
 ******************************************************************************
 * VmBackupNullOpCancel --                                              */ /**
 *
 * Cancel an ongoing Nulldriver operation. This doesn't actually
 * do anything because there is no operation to cancel as such.
 *
 * @param[in] op        VmBackupOp.
 *
 ******************************************************************************
 */

static void
VmBackupNullOpCancel(VmBackupOp *op)   // IN
{
   /* Nothing to do */
}


/*
 ******************************************************************************
 * VmBackupNullStart --                                                 */ /**
 *
 * Calls sync(2) on POSIX systems. Sets up an asynchronous operation
 * for tracking.
 *
 * @param[in] ctx           Plugin context.
 * @param[in] state         Backup state.
 *
 ******************************************************************************
 */

static void
VmBackupNullStart(ToolsAppCtx *ctx,
                  void *clientData)
{
   VmBackupOp *op;
   VmBackupState *state = (VmBackupState*) clientData;

   g_debug("*** %s\n", __FUNCTION__);

   op = g_new0(VmBackupOp, 1);
   op->queryFn = VmBackupNullOpQuery;
   op->cancelFn = VmBackupNullOpCancel;
   op->releaseFn = VmBackupNullOpRelease;

   /*
    * This is more of a "let's at least do something" than something that
    * will actually ensure data integrity...
    */
   sync();

   VmBackup_SetCurrentOp(state,
                         op,
                         VmBackupNullReadyForSnapshot,
                         __FUNCTION__);
}


/*
 ******************************************************************************
 * VmBackupNullSnapshotDone --                                          */ /**
 *
 * Does nothing except setting up an asynchronous operation to keep the
 * backup state machine alive.
 *
 * @param[in] state         Backup state.
 * @param[in] clientData    Unused.
 *
 * @return TRUE.
 *
 ******************************************************************************
 */

static Bool
VmBackupNullSnapshotDone(VmBackupState *state,
                         void *clientData)
{
   VmBackupOp *op;

   g_debug("*** %s\n", __FUNCTION__);

   op = g_new0(VmBackupOp, 1);
   op->queryFn = VmBackupNullOpQuery;
   op->cancelFn = VmBackupNullOpCancel;
   op->releaseFn = VmBackupNullOpRelease;

   VmBackup_SetCurrentOp(state, op, NULL, __FUNCTION__);
   return TRUE;
}


/*
 ******************************************************************************
 * VmBackupNullUndo --                                                  */ /**
 *
 * Update the state machine state with the currentOpName.
 *
 * Can be called when snapshot times out.  See PR2993571 and PR3003917.
 *
 * @param[in] state        the backup state
 * @param[in] clientData   client data
 *
 * @return TRUE
 *
 ******************************************************************************
 */

static Bool
VmBackupNullUndo(VmBackupState *state,
                 void *clientData)
{
   g_debug("*** %s\n", __FUNCTION__);
   VmBackup_SetCurrentOp(state, NULL, NULL, __FUNCTION__);
   return TRUE;
}

#endif

/*
 ******************************************************************************
 * VmBackupNullRelease --                                               */ /**
 *
 * Frees memory associated with this sync provider.
 *
 * @param[in] provider     The provider.
 *
 ******************************************************************************
 */

static void
VmBackupNullRelease(VmBackupSyncProvider *provider)
{
   g_free(provider);
}


/*
 ******************************************************************************
 * VmBackup_NewNullProvider --                                          */ /**
 *
 * Returns a new null provider.
 *
 * @return A VmBackupSyncProvider, never NULL.
 *
 ******************************************************************************
 */

VmBackupSyncProvider *
VmBackup_NewNullProvider(void)
{
   VmBackupSyncProvider *provider;

   provider = g_new(VmBackupSyncProvider, 1);
   provider->start = VmBackupNullStart;
#if !defined(_WIN32)
   provider->undo = VmBackupNullUndo;
#endif
   provider->snapshotDone = VmBackupNullSnapshotDone;
   provider->release = VmBackupNullRelease;
   provider->clientData = NULL;

   return provider;
}

