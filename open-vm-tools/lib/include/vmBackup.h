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
 * vmBackup.h --
 *
 * Declaration of functions shared by the vmBackup implementations (both
 * VSS and non-VSS).
 */

#ifndef _VMBACKUP_H_
#define _VMBACKUP_H_

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "vmbackup_def.h"

#include "dbllnklst.h"
#include "eventManager.h"
#include "rpcin.h"
#include "str.h"

typedef enum {
   VMBACKUP_STATUS_PENDING,
   VMBACKUP_STATUS_FINISHED,
   VMBACKUP_STATUS_CANCELED,
   VMBACKUP_STATUS_ERROR
} VmBackupOpStatus;


/*
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


/*
 * Holds information about the current state of the backup operation.
 * Don't modify the fields directly - rather, use VmBackup_SetCurrentOp,
 * which does most of the handling needed by users of the state machine.
 *
 * The "SendEvent" function is a pointer so that the VSS code, which
 * is linked against a different runtime, can call it and be able to
 * reuse the keep alive functionality without having to link to the
 * vmBackup library.
 */

typedef struct VmBackupState {
   Bool (*SendEvent)(const char *, const uint32, const char *);
   VmBackupOp  *currentOp;
   const char  *currentOpName;
   char        *volumes;
   uint32      pollPeriod;
   Event       *timerEvent;
   Event       *keepAlive;
   Bool (*callback)(struct VmBackupState *);
   Bool        syncProviderRunning;
   Bool        forceRequeue;
   Bool        snapshotDone;
   Bool        syncProviderFailed;
   Bool        generateManifests;
   Bool        clientAborted;
   intptr_t    clientData;
   void        *scripts;
   const char  *configDir;
   ssize_t     currentScript;
} VmBackupState;

typedef Bool (*VmBackupCallback)(VmBackupState *);
typedef Bool (*VmBackupProviderCallback)(VmBackupState *, void *clientData);


/*
 * Defines the interface between the state machine and the implementation
 * of the "sync provider": either the VSS requestor or the sync driver
 * provider, currently.
 */

typedef struct VmBackupSyncProvider {
   VmBackupProviderCallback start;
   VmBackupProviderCallback abort;
   VmBackupProviderCallback snapshotDone;
   void (*release)(struct VmBackupSyncProvider *);
   void *clientData;
} VmBackupSyncProvider;


/* Functions to start / stop the backup state machine. */


Bool VmBackup_Init(RpcIn *rpcin,
                   DblLnkLst_Links *eventQueue,
                   VmBackupSyncProvider *provider);

void VmBackup_Shutdown(RpcIn *rpcin);

VmBackupSyncProvider *VmBackup_NewSyncDriverProvider(void);


/*
 * Macro that checks if the given string matches the given event. Used
 * by the test code to check the events sent to the VMX.
 */
#define VmBackup_IsEvent(str, evt) \
   (Str_Strncmp((str), (evt), (sizeof (evt)) - 1) == 0)


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_SetCurrentOp --
 *
 *    Sets the current asynchronous operation being monitored, and an
 *    optional callback for after it's done executing. If the operation
 *    is NULL, the callback is set to execute later (currently, later = 200ms).
 *
 * Result
 *    FALSE if the given operation is NULL.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

INLINE static Bool
VmBackup_SetCurrentOp(VmBackupState *state,
                      VmBackupOp *op,
                      VmBackupCallback callback,
                      const char *currentOpName)
{
   ASSERT(state != NULL);
   ASSERT(state->currentOp == NULL);
   state->currentOp = op;
   state->callback = callback;
   state->currentOpName = currentOpName;
   state->forceRequeue = (callback != NULL && state->currentOp == NULL);
   return (op != NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_QueryStatus --
 *
 *    Convenience function to call the operation-specific query function.
 *
 * Result
 *    A VmBackupOpStatus value.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

INLINE static VmBackupOpStatus
VmBackup_QueryStatus(VmBackupOp *op)   // IN
{
   ASSERT(op != NULL);
   return op->queryFn(op);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_Cancel --
 *
 *    Convenience function to call the operation-specific cancel function.
 *    Code calling this function should still call VmBackup_QueryStatus()
 *    waiting for it to return a finished status (i.e., something other
 *    than VMBACKUP_STATUS_PENDING).
 *
 * Result
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

INLINE static void
VmBackup_Cancel(VmBackupOp *op)  // IN
{
   ASSERT(op != NULL);
   op->cancelFn(op);
}


/*
 *-----------------------------------------------------------------------------
 *
 *  VmBackup_Release --
 *
 *    Convenience function to call the operation-specific release function.
 *    Releasing a state object that hasn't finished yet (i.e.,
 *    VmBackup_QueryStatus returns VMBACKUP_STATUS_PENDING) can result in
 *    undefined behavior.
 *
 * Result
 *    None.
 *
 * Side effects:
 *    Operation pointer is not valid anymore.
 *
 *-----------------------------------------------------------------------------
 */

INLINE static void
VmBackup_Release(VmBackupOp *op) // IN
{
   ASSERT(op != NULL);
   op->releaseFn(op);
}

#endif

