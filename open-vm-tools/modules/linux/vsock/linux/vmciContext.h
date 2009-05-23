/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 *	VMCI state to enable sending calls between VMs.
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
#include "vmci_infrastructure.h"

#define MAX_QUEUED_GUESTCALLS_PER_VM  100

typedef struct VMCIContext VMCIContext;

int VMCIContext_Init(void);
void VMCIContext_Exit(void);
int VMCIContext_InitContext(VMCIId cid, VMCIPrivilegeFlags flags,
                            uintptr_t eventHnd, int version,
                            VMCIContext **context);
#ifdef VMKERNEL
int VMCIContext_SetDomainName(VMCIContext *context, const char *domainName);
int VMCIContext_GetDomainName(VMCIId contextID, char *domainName,
                              size_t domainNameBufSize);
#endif
Bool VMCIContext_SupportsHostQP(VMCIContext *context);
void VMCIContext_ReleaseContext(VMCIContext *context);
int VMCIContext_EnqueueDatagram(VMCIId cid, VMCIDatagram *dg);
int VMCIContext_DequeueDatagram(VMCIContext *context, size_t *maxSize, 
				VMCIDatagram **dg);
int VMCIContext_PendingDatagrams(VMCIId cid, uint32 *pending);
VMCIContext *VMCIContext_Get(VMCIId cid);
void VMCIContext_Release(VMCIContext *context);
Bool VMCIContext_Exists(VMCIId cid);

VMCIPrivilegeFlags VMCIContext_GetPrivFlagsInt(VMCIId contextID);
VMCIId VMCIContext_GetId(VMCIContext *context);
int VMCIContext_AddGroupEntry(VMCIContext *context,
                              VMCIHandle entryHandle);
VMCIHandle VMCIContext_RemoveGroupEntry(VMCIContext *context,
                                        VMCIHandle entryHandle);
int VMCIContext_AddWellKnown(VMCIId contextID, VMCIId wellKnownID); 
int VMCIContext_RemoveWellKnown(VMCIId contextID, VMCIId wellKnownID);
int VMCIContext_AddNotification(VMCIId contextID, VMCIId remoteCID);
int VMCIContext_RemoveNotification(VMCIId contextID, VMCIId remoteCID);
int VMCIContext_GetCheckpointState(VMCIId contextID, uint32 cptType, 
                                   uint32 *numCIDs, char **cptBufPtr);
int VMCIContext_SetCheckpointState(VMCIId contextID, uint32 cptType, 
                                   uint32 numCIDs, char *cptBuf);
#ifndef VMX86_SERVER
void VMCIContext_CheckAndSignalNotify(VMCIContext *context);
#  ifdef __linux__
/* TODO Windows and Mac OS. */
void VMCIUnsetNotify(VMCIContext *context);
#  endif
#endif

#ifdef VMKERNEL
int VMCIContextID2HostVmID(VMCIId contextID, void *hostVmID, size_t hostVmIDLen);
#endif

#endif // _VMCI_CONTEXT_H_
