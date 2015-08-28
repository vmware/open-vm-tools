/*********************************************************
 * Copyright (C) 2006-2013,2015 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmciContext.h --
 *
 *    VMCI state to enable sending calls between VMs.
 */

#ifndef _VMCI_CONTEXT_H_
#define _VMCI_CONTEXT_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmci_handle_array.h"
#include "vmci_infrastructure.h"
#include "vmci_kernel_if.h"
#include "vmciCommonInt.h"

#define MAX_QUEUED_GUESTCALLS_PER_VM  100

typedef struct VMCIContext VMCIContext;

int VMCIContext_Init(void);
void VMCIContext_Exit(void);
int VMCIContext_InitContext(VMCIId cid, VMCIPrivilegeFlags flags,
                            uintptr_t eventHnd, int version,
                            VMCIHostUser *user, VMCIContext **context);
#ifdef VMKERNEL
void VMCIContext_SetFSRState(VMCIContext *context,
                             Bool isQuiesced,
                             VMCIId migrateCid,
                             uintptr_t eventHnd,
                             Bool isLocked);
VMCIContext *VMCIContext_FindAndUpdateSrcFSR(VMCIId migrateCid,
                                             uintptr_t eventHnd,
                                             uintptr_t *srcEventHnd);
Bool VMCIContext_IsActiveHnd(VMCIContext *context, uintptr_t eventHnd);
uintptr_t VMCIContext_GetActiveHnd(VMCIContext *context);
void VMCIContext_SetInactiveHnd(VMCIContext *context, uintptr_t eventHnd);
Bool VMCIContext_RemoveHnd(VMCIContext *context,
                           uintptr_t eventHnd,
                           uint32 *numOld,
                           uint32 *numNew);
void VMCIContext_ClearDatagrams(VMCIContext *context);
void VMCIContext_SetId(VMCIContext *context, VMCIId cid);
void VMCIContext_NotifyGuestPaused(VMCIId cid, Bool paused);
void VMCIContext_NotifyMemoryAccess(VMCIId cid, Bool on);
#endif
Bool VMCIContext_SupportsHostQP(VMCIContext *context);
void VMCIContext_ReleaseContext(VMCIContext *context);
int VMCIContext_EnqueueDatagram(VMCIId cid, VMCIDatagram *dg, Bool notify);
int VMCIContext_DequeueDatagram(VMCIContext *context, size_t *maxSize,
                                VMCIDatagram **dg);
int VMCIContext_PendingDatagrams(VMCIId cid, uint32 *pending);
VMCIContext *VMCIContext_Get(VMCIId cid);
void VMCIContext_Release(VMCIContext *context);
Bool VMCIContext_Exists(VMCIId cid);

VMCIId VMCIContext_GetId(VMCIContext *context);
int VMCIContext_AddNotification(VMCIId contextID, VMCIId remoteCID);
int VMCIContext_RemoveNotification(VMCIId contextID, VMCIId remoteCID);
int VMCIContext_GetCheckpointState(VMCIId contextID, uint32 cptType,
                                   uint32 *numCIDs, char **cptBufPtr);
int VMCIContext_SetCheckpointState(VMCIId contextID, uint32 cptType,
                                   uint32 numCIDs, char *cptBuf);
void VMCIContext_RegisterGuestMem(VMCIContext *context, VMCIGuestMemID gid);
void VMCIContext_ReleaseGuestMem(VMCIContext *context, VMCIGuestMemID gid,
                                 Bool powerOff);

int VMCIContext_QueuePairCreate(VMCIContext *context, VMCIHandle handle);
int VMCIContext_QueuePairDestroy(VMCIContext *context, VMCIHandle handle);
Bool VMCIContext_QueuePairExists(VMCIContext *context, VMCIHandle handle);

#ifndef VMX86_SERVER
void VMCIContext_CheckAndSignalNotify(VMCIContext *context);
#  ifdef __linux__
/* TODO Windows and Mac OS. */
void VMCIUnsetNotify(VMCIContext *context);
#  endif
#endif

int VMCIContext_DoorbellCreate(VMCIId contextID, VMCIHandle handle);
int VMCIContext_DoorbellDestroy(VMCIId contextID, VMCIHandle handle);
int VMCIContext_DoorbellDestroyAll(VMCIId contextID);
int VMCIContext_NotifyDoorbell(VMCIId cid, VMCIHandle handle,
                               VMCIPrivilegeFlags srcPrivFlags);

int VMCIContext_ReceiveNotificationsGet(VMCIId contextID,
                                        VMCIHandleArray **dbHandleArray,
                                        VMCIHandleArray **qpHandleArray);
void VMCIContext_ReceiveNotificationsRelease(VMCIId contextID,
                                             VMCIHandleArray *dbHandleArray,
                                             VMCIHandleArray *qpHandleArray,
                                             Bool success);
#if defined(VMKERNEL)
void VMCIContext_SignalPendingDoorbells(VMCIId contextID);
void VMCIContext_SignalPendingDatagrams(VMCIId contextID);
int VMCIContext_FilterSet(VMCIId cid, VMCIFilterState *filterState);
int VMCI_Uuid2ContextId(const char *uuidString, VMCIId *contextID);
#endif // VMKERNEL
#endif // _VMCI_CONTEXT_H_
