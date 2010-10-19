/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * @file vmBackupInt.h
 *
 * Internal definitions used by the vmbackup code.
 */

#ifndef _VMBACKUPINT_H_
#define _VMBACKUPINT_H_

#define G_LOG_DOMAIN "vmbackup"

#include <glib.h>
#include "vmware.h"
#include "vmware/guestrpc/vmbackup.h"
#include "vmware/tools/plugin.h"

typedef enum {
   VMBACKUP_STATUS_PENDING,
   VMBACKUP_STATUS_FINISHED,
   VMBACKUP_STATUS_CANCELED,
   VMBACKUP_STATUS_ERROR
} VmBackupOpStatus;

typedef enum {
   VMBACKUP_SCRIPT_FREEZE,
   VMBACKUP_SCRIPT_FREEZE_FAIL,
   VMBACKUP_SCRIPT_THAW
} VmBackupScriptType;

typedef enum {
   VMBACKUP_MSTATE_IDLE,
   VMBACKUP_MSTATE_SCRIPT_FREEZE,
   VMBACKUP_MSTATE_SYNC_FREEZE,
   VMBACKUP_MSTATE_SYNC_THAW,
   VMBACKUP_MSTATE_SCRIPT_THAW,
   VMBACKUP_MSTATE_SCRIPT_ERROR,
   VMBACKUP_MSTATE_SYNC_ERROR
} VmBackupMState;

/**
 * This is a "base struct" for asynchronous operations monitored by the
 * state machine. Each implementation should provide these three functions
 * at the start of the struct so that the state machine can properly
 * interact with it.
 */

typedef struct VmBackupOp {
   VmBackupOpStatus (*queryFn)(struct VmBackupOp *);
   void (*releaseFn)(struct VmBackupOp *);
   void (*cancelFn)(struct VmBackupOp *);
} VmBackupOp;


struct VmBackupSyncProvider;

/**
 * Holds information about the current state of the backup operation.
 * Don't modify the fields directly - rather, use VmBackup_SetCurrentOp,
 * which does most of the handling needed by users of the state machine.
 */

typedef struct VmBackupState {
   ToolsAppCtx   *ctx;
   VmBackupOp    *currentOp;
   const char    *currentOpName;
   char          *volumes;
   char          *snapshots;
   guint          pollPeriod;
   GSource       *abortTimer;
   GSource       *timerEvent;
   GSource       *keepAlive;
   Bool (*callback)(struct VmBackupState *);
   Bool           forceRequeue;
   Bool           generateManifests;
   Bool           quiesceApps;
   Bool           quiesceFS;
   Bool           allowHWProvider;
   Bool           execScripts;
   char          *scriptArg;
   guint          timeout;
   gpointer       clientData;
   void          *scripts;
   const char    *configDir;
   ssize_t        currentScript;
   gchar         *errorMsg;
   VmBackupMState machineState;
   struct VmBackupSyncProvider *provider;
} VmBackupState;

typedef Bool (*VmBackupCallback)(VmBackupState *);
typedef Bool (*VmBackupProviderCallback)(VmBackupState *, void *clientData);


/**
 * Defines the interface between the state machine and the implementation
 * of the "sync provider": either the VSS requestor or the sync driver
 * provider, currently.
 */

typedef struct VmBackupSyncProvider {
   VmBackupProviderCallback start;
   VmBackupProviderCallback snapshotDone;
   void (*release)(struct VmBackupSyncProvider *);
   void *clientData;
} VmBackupSyncProvider;


/**
 * Sets the current asynchronous operation being monitored, and an
 * optional callback for after it's done executing. If the operation
 * is NULL, the callback is set to execute later (currently, later = 200ms).
 *
 * @param[in]  state          The backup state.
 * @param[in]  op             The current op to set.
 * @param[in]  callback       Function to call after the operation is finished.
 * @param[in]  currentOpName  Operation name, for debugging.
 *
 * @return TRUE if @a op is not NULL.
 */

static INLINE Bool
VmBackup_SetCurrentOp(VmBackupState *state,
                      VmBackupOp *op,
                      VmBackupCallback callback,
                      const char *currentOpName)
{
   ASSERT(state != NULL);
   ASSERT(state->currentOp == NULL);
   ASSERT(currentOpName != NULL);
   state->currentOp = op;
   state->callback = callback;
   state->currentOpName = currentOpName;
   state->forceRequeue = (callback != NULL && state->currentOp == NULL);
   return (op != NULL);
}


/**
 * Convenience function to call the operation-specific query function.
 *
 * @param[in]  op    The backup op.
 *
 * @return The operation's status.
 */

static INLINE VmBackupOpStatus
VmBackup_QueryStatus(VmBackupOp *op)
{
   ASSERT(op != NULL);
   return op->queryFn(op);
}


/**
 * Convenience function to call the operation-specific cancel function.
 * Code calling this function should still call VmBackup_QueryStatus()
 * waiting for it to return a finished status (i.e., something other
 * than VMBACKUP_STATUS_PENDING).
 *
 * @param[in]  op    The backup op.
 */

static INLINE void
VmBackup_Cancel(VmBackupOp *op)
{
   ASSERT(op != NULL);
   op->cancelFn(op);
}


/**
 * Convenience function to call the operation-specific release function.
 * Releasing a state object that hasn't finished yet (i.e.,
 * VmBackup_QueryStatus returns VMBACKUP_STATUS_PENDING) can result in
 * undefined behavior.
 *
 * @param[in]  op    The backup op.
 */

static INLINE void
VmBackup_Release(VmBackupOp *op)
{
   if (op != NULL) {
      ASSERT(op->releaseFn != NULL);
      op->releaseFn(op);
   }
}


VmBackupSyncProvider *
VmBackup_NewNullProvider(void);

VmBackupSyncProvider *
VmBackup_NewSyncDriverProvider(void);

#if defined(G_PLATFORM_WIN32)
VmBackupSyncProvider *
VmBackup_NewVssProvider(void);

void
VmBackup_UnregisterSnapshotProvider(void);
#endif

VmBackupOp *
VmBackup_NewScriptOp(VmBackupScriptType freeze,
                     VmBackupState *state);

Bool
VmBackup_SendEvent(const char *event,
                   const uint32 code,
                   const char *desc);

#endif /* _VMBACKUPINT_H_*/

