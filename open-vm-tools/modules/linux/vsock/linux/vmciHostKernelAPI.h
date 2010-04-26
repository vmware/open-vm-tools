/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vmciHostKernelAPI.h --
 *
 *    Kernel API exported from the VMCI host driver.
 */

#ifndef __VMCI_HOSTKERNELAPI_H__
#define __VMCI_HOSTKERNELAPI_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"


/* VMCI host kernel API version number. */
#define VMCI_HOST_KERNEL_API_VERSION  1

/* Macros to operate on the driver version number. */
#define VMCI_MAJOR_VERSION(v)       (((v) >> 16) & 0xffff)
#define VMCI_MINOR_VERSION(v)       ((v) & 0xffff)

#include "vmci_defs.h"
#include "vmci_call_defs.h"

#include "vmciQueue.h"


/* PUBLIC: VMCI Device Usage API. */

#if defined(_WIN32)
Bool VMCI_DeviceGet(void);
void VMCI_DeviceRelease(void);
#else // _WIN32
#  define VMCI_DeviceGet() TRUE
#  define VMCI_DeviceRelease()
#endif // _WIN32

/* PUBLIC: VMCI Datagram API. */

int VMCIHost_DatagramCreateHnd(VMCIId resourceID, uint32 flags,
                               VMCIDatagramRecvCB recvCB, void *clientData,
                               VMCIHandle *outHandle);
int VMCIDatagram_CreateHnd(VMCIId resourceID, uint32 flags,
                           VMCIDatagramRecvCB recvCB, void *clientData,
                           VMCIHandle *outHandle);
int VMCIDatagram_CreateHndPriv(VMCIId resourceID, uint32 flags,
                               VMCIPrivilegeFlags privFlags,
                               VMCIDatagramRecvCB recvCB, void *clientData,
                               VMCIHandle *outHandle);
int VMCIDatagram_DestroyHnd(VMCIHandle handle);
int VMCIDatagram_Send(VMCIDatagram *msg);

/* VMCI Utility API. */

#if defined(VMKERNEL)
int VMCI_ContextID2HostVmID(VMCIId contextID, void *hostVmID,
                            size_t hostVmIDLen);
#endif

/* VMCI Event API  */

typedef void (*VMCI_EventCB)(VMCIId subID, VMCI_EventData *ed,
			     void *clientData);

int VMCIEvent_Subscribe(VMCI_Event event, uint32 flags, VMCI_EventCB callback,
                        void *callbackData, VMCIId *subID);
int VMCIEvent_Unsubscribe(VMCIId subID);

/* VMCI Context API */

VMCIPrivilegeFlags VMCIContext_GetPrivFlags(VMCIId contextID);

/* VMCI Doorbell API. */

#define VMCI_FLAG_DELAYED_CB    0x01

typedef void (*VMCICallback)(void *clientData);

int VMCIDoorbell_Create(VMCIHandle *handle, uint32 flags,
                        VMCIPrivilegeFlags privFlags,
                        VMCICallback notifyCB, void *clientData);
int VMCIDoorbell_Destroy(VMCIHandle handle);
int VMCIDoorbell_Notify(VMCIHandle handle,
                        VMCIPrivilegeFlags privFlags);

#endif /* !__VMCI_HOSTKERNELAPI_H__ */

