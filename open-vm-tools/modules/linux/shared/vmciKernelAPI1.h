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
 * vmciKernelAPI1.h --
 *
 *    Kernel API (v1) exported from the VMCI host and guest drivers.
 */

#ifndef __VMCI_KERNELAPI_1_H__
#define __VMCI_KERNELAPI_1_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"


/* VMCI module namespace on vmkernel. */

#define MOD_VMCI_NAMESPACE "com.vmware.vmci"

/* Define version 1. */

#undef  VMCI_KERNEL_API_VERSION
#define VMCI_KERNEL_API_VERSION_1 1
#define VMCI_KERNEL_API_VERSION   VMCI_KERNEL_API_VERSION_1

/* Macros to operate on the driver version number. */

#define VMCI_MAJOR_VERSION(v) (((v) >> 16) & 0xffff)
#define VMCI_MINOR_VERSION(v) ((v) & 0xffff)

#if defined(_WIN32)
/* Path to callback object in object manager, for Windows only. */
#define VMCI_CALLBACK_OBJECT_PATH L"\\Callback\\VMCIDetachCB"
#endif // _WIN32

/* VMCI Device Usage API. */

typedef void (VMCI_DeviceShutdownFn)(void *deviceRegistration,
                                     void *userData);

Bool VMCI_DeviceGet(uint32 *apiVersion, VMCI_DeviceShutdownFn *deviceShutdownCB,
                    void *userData, void **deviceRegistration);
void VMCI_DeviceRelease(void *deviceRegistration);

#if defined(_WIN32)
/* Called when the client is unloading, for Windows only. */
void VMCI_Exit(void);
#endif // _WIN32

/* VMCI Datagram API. */

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
int VMCI_IsContextOwner(VMCIId contextID, void *hostUser);

/* VMCI Event API. */

typedef void (*VMCI_EventCB)(VMCIId subID, VMCI_EventData *ed,
                             void *clientData);

int VMCIEvent_Subscribe(VMCI_Event event, uint32 flags, VMCI_EventCB callback,
                        void *callbackData, VMCIId *subID);
int VMCIEvent_Unsubscribe(VMCIId subID);

/* VMCI Context API */

VMCIPrivilegeFlags VMCIContext_GetPrivFlags(VMCIId contextID);

/* VMCI Queue Pair API. */

typedef struct VMCIQPair VMCIQPair;

int VMCIQPair_Alloc(VMCIQPair **qpair,
                    VMCIHandle *handle,
                    uint64 produceQSize,
                    uint64 consumeQSize,
                    VMCIId peer,
                    uint32 flags,
                    VMCIPrivilegeFlags privFlags);

int VMCIQPair_Detach(VMCIQPair **qpair);

int VMCIQPair_GetProduceIndexes(const VMCIQPair *qpair,
                                uint64 *producerTail,
                                uint64 *consumerHead);
int VMCIQPair_GetConsumeIndexes(const VMCIQPair *qpair,
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
    (defined(__linux__) && defined(__KERNEL__)) || \
    (defined(_WIN32) && defined(WINNT_DDK))
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


/* Typedefs for all of the above, used by the IOCTLs and the kernel library. */

typedef void (VMCI_DeviceReleaseFct)(void *);
typedef int (VMCIDatagram_CreateHndFct)(VMCIId, uint32, VMCIDatagramRecvCB,
                                        void *, VMCIHandle *);
typedef int (VMCIDatagram_CreateHndPrivFct)(VMCIId, uint32, VMCIPrivilegeFlags,
                                            VMCIDatagramRecvCB, void *,
                                            VMCIHandle *);
typedef int (VMCIDatagram_DestroyHndFct)(VMCIHandle);
typedef int (VMCIDatagram_SendFct)(VMCIDatagram *);
typedef VMCIId (VMCI_GetContextIDFct)(void);
typedef uint32 (VMCI_VersionFct)(void);
typedef int (VMCI_ContextID2HostVmIDFct)(VMCIId, void *, size_t);
typedef int (VMCI_IsContextOwnerFct)(VMCIId, void *);
typedef int (VMCIEvent_SubscribeFct)(VMCI_Event, uint32, VMCI_EventCB, void *,
                                     VMCIId *);
typedef int (VMCIEvent_UnsubscribeFct)(VMCIId);
typedef VMCIPrivilegeFlags (VMCIContext_GetPrivFlagsFct)(VMCIId);
typedef int (VMCIQPair_AllocFct)(VMCIQPair **, VMCIHandle *, uint64, uint64,
                                 VMCIId, uint32, VMCIPrivilegeFlags);
typedef int (VMCIQPair_DetachFct)(VMCIQPair **);
typedef int (VMCIQPair_GetProduceIndexesFct)(const VMCIQPair *, uint64 *,
                                             uint64 *);
typedef int (VMCIQPair_GetConsumeIndexesFct)(const VMCIQPair *, uint64 *,
                                             uint64 *);
typedef int64 (VMCIQPair_ProduceFreeSpaceFct)(const VMCIQPair *);
typedef int64 (VMCIQPair_ProduceBufReadyFct)(const VMCIQPair *);
typedef int64 (VMCIQPair_ConsumeFreeSpaceFct)(const VMCIQPair *);
typedef int64 (VMCIQPair_ConsumeBufReadyFct)(const VMCIQPair *);
typedef ssize_t (VMCIQPair_EnqueueFct)(VMCIQPair *, const void *, size_t, int);
typedef ssize_t (VMCIQPair_DequeueFct)(VMCIQPair *, void *, size_t, int);
typedef ssize_t (VMCIQPair_PeekFct)(VMCIQPair *, void *, size_t, int);
typedef ssize_t (VMCIQPair_EnqueueVFct)(VMCIQPair *qpair, void *, size_t, int);
typedef ssize_t (VMCIQPair_DequeueVFct)(VMCIQPair *qpair, void *, size_t, int);
typedef ssize_t (VMCIQPair_PeekVFct)(VMCIQPair *qpair, void *, size_t, int);


#endif /* !__VMCI_KERNELAPI_1_H__ */

