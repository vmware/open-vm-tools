/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * vmciKernelAPI.h --
 *
 *    Kernel API exported from the VMCI host and guest drivers.
 */

#ifndef __VMCI_KERNELAPI_H__
#define __VMCI_KERNELAPI_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"


/* Macros to operate on the driver version number. */
#define VMCI_MAJOR_VERSION(v)       (((v) >> 16) & 0xffff)
#define VMCI_MINOR_VERSION(v)       ((v) & 0xffff)

#include "vmci_defs.h"
#include "vmci_call_defs.h"


/* PUBLIC: VMCI Device Usage API. */

Bool VMCI_DeviceGet(void);
void VMCI_DeviceRelease(void);

/* PUBLIC: VMCI Datagram API. */

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

VMCIId VMCI_GetContextID(void);
uint32 VMCI_Version(void);
int VMCI_ContextID2HostVmID(VMCIId contextID, void *hostVmID,
                            size_t hostVmIDLen);

/* VMCI Event API. */

typedef void (*VMCI_EventCB)(VMCIId subID, VMCI_EventData *ed,
			     void *clientData);

int VMCIEvent_Subscribe(VMCI_Event event, uint32 flags, VMCI_EventCB callback,
                        void *callbackData, VMCIId *subID);
int VMCIEvent_Unsubscribe(VMCIId subID);

/* VMCI Context API */

VMCIPrivilegeFlags VMCIContext_GetPrivFlags(VMCIId contextID);

/* VMCI Discovery Service API. */

int VMCIDs_Lookup(const char *name, VMCIHandle *out);

/* VMCI Doorbell API. */

#define VMCI_FLAG_DELAYED_CB    0x01

typedef void (*VMCICallback)(void *clientData);

int VMCIDoorbell_Create(VMCIHandle *handle, uint32 flags,
                        VMCIPrivilegeFlags privFlags,
                        VMCICallback notifyCB, void *clientData);
int VMCIDoorbell_Destroy(VMCIHandle handle);
int VMCIDoorbell_Notify(VMCIHandle handle,
                        VMCIPrivilegeFlags privFlags);

/* VMCI Queue Pair API. */

typedef struct VMCIQPair VMCIQPair;

int VMCIQPair_Alloc(VMCIQPair **qpair,
                    VMCIHandle *handle,
                    uint64 produceQSize,
                    uint64 consumeQSize,
                    VMCIId peer,
                    uint32 flags,
                    VMCIPrivilegeFlags privFlags);

void VMCIQPair_Detach(VMCIQPair **qpair);

void VMCIQPair_Init(VMCIQPair *qpair);
void VMCIQPair_GetProduceIndexes(const VMCIQPair *qpair,
                                 uint64 *producerTail,
                                 uint64 *consumerHead);
void VMCIQPair_GetConsumeIndexes(const VMCIQPair *qpair,
                                 uint64 *consumerTail,
                                 uint64 *producerHead);
int64 VMCIQPair_ProduceFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ProduceBufReady(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeFreeSpace(const VMCIQPair *qpair);
int64 VMCIQPair_ConsumeBufReady(const VMCIQPair *qpair);
ssize_t VMCIQPair_Enqueue(VMCIQPair *qpair,
                          const void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Dequeue(VMCIQPair *qpair,
                          void *buf,
                          size_t bufSize,
                          int mode);
ssize_t VMCIQPair_Peek(VMCIQPair *qpair,
                       void *buf,
                       size_t bufSize,
                       int mode);

#if defined (SOLARIS) || (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__))
/*
 * Environments that support struct iovec
 */

ssize_t VMCIQPair_EnqueueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_DequeueV(VMCIQPair *qpair,
                           void *iov,
                           size_t iovSize,
                           int mode);
ssize_t VMCIQPair_PeekV(VMCIQPair *qpair,
                        void *iov,
                        size_t iovSize,
                        int mode);
#endif /* Systems that support struct iovec */

#endif /* !__VMCI_KERNELAPI_H__ */
