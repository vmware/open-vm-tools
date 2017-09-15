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

#if defined __cplusplus
extern "C" {
#endif


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

#if defined(__linux__) && !defined(VMKERNEL)
#define vmci_device_get(_a, _b, _c, _d) 1
#define vmci_device_release(_x)
#else // !linux
typedef void (VMCI_DeviceShutdownFn)(void *deviceRegistration,
                                     void *userData);
Bool vmci_device_get(uint32 *apiVersion,
                     VMCI_DeviceShutdownFn *deviceShutdownCB,
                     void *userData, void **deviceRegistration);
void vmci_device_release(void *deviceRegistration);
#endif // !linux

#if defined(_WIN32)
/* Called when the client is unloading, for Windows only. */
void vmci_exit(void);
#endif // _WIN32

/* VMCI Datagram API. */

int vmci_datagram_create_handle(uint32 resourceId, uint32 flags,
                                VMCIDatagramRecvCB recvCB, void *clientData,
                                VMCIHandle *outHandle);
int vmci_datagram_create_handle_priv(uint32 resourceID, uint32 flags,
                                     VMCIPrivilegeFlags privFlags,
                                     VMCIDatagramRecvCB recvCB,
                                     void *clientData, VMCIHandle *outHandle);
int vmci_datagram_destroy_handle(VMCIHandle handle);
int vmci_datagram_send(VMCIDatagram *msg);

/* VMCI Utility API. */

VMCIId vmci_get_context_id(void);

#if defined(__linux__) && !defined(VMKERNEL)
/* Returned value is a bool, 0 for false, 1 for true. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
int vmci_is_context_owner(VMCIId contextID, kuid_t uid);
#else
int vmci_is_context_owner(VMCIId contextID, uid_t uid);
#endif
#else // !linux || VMKERNEL
/* Returned value is a VMCI error code. */
int vmci_is_context_owner(VMCIId contextID, void *hostUser);
#endif // !linux || VMKERNEL

uint32 vmci_version(void);
int vmci_cid_2_host_vm_id(VMCIId contextID, void *hostVmID,
                          size_t hostVmIDLen);

/* VMCI Event API. */

typedef void (*VMCI_EventCB)(VMCIId subID, VMCI_EventData *ed,
                             void *clientData);

int vmci_event_subscribe(VMCI_Event event,
#if !defined(__linux__) || defined(VMKERNEL)
                         uint32 flags,
#endif // !linux || VMKERNEL
                         VMCI_EventCB callback,
                         void *callbackData, VMCIId *subID);
int vmci_event_unsubscribe(VMCIId subID);

/* VMCI Context API */

VMCIPrivilegeFlags vmci_context_get_priv_flags(VMCIId contextID);

/* VMCI Queue Pair API. */

typedef struct VMCIQPair VMCIQPair;

int vmci_qpair_alloc(VMCIQPair **qpair, VMCIHandle *handle,
                     uint64 produceQSize, uint64 consumeQSize, VMCIId peer,
                     uint32 flags, VMCIPrivilegeFlags privFlags);
int vmci_qpair_detach(VMCIQPair **qpair);
int vmci_qpair_get_produce_indexes(const VMCIQPair *qpair,
                                   uint64 *producerTail, uint64 *consumerHead);
int vmci_qpair_get_consume_indexes(const VMCIQPair *qpair,
                                   uint64 *consumerTail, uint64 *producerHead);
int64 vmci_qpair_produce_free_space(const VMCIQPair *qpair);
int64 vmci_qpair_produce_buf_ready(const VMCIQPair *qpair);
int64 vmci_qpair_consume_free_space(const VMCIQPair *qpair);
int64 vmci_qpair_consume_buf_ready(const VMCIQPair *qpair);
ssize_t vmci_qpair_enqueue(VMCIQPair *qpair, const void *buf, size_t bufSize,
                           int mode);
ssize_t vmci_qpair_dequeue(VMCIQPair *qpair, void *buf, size_t bufSize,
                           int mode);
ssize_t vmci_qpair_peek(VMCIQPair *qpair, void *buf, size_t bufSize, int mode);

#if (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__))    || \
    (defined(_WIN32)    && defined(WINNT_DDK))
/*
 * Environments that support struct iovec
 */

ssize_t vmci_qpair_enquev(VMCIQPair *qpair, void *iov, size_t iovSize,
                          int mode);
ssize_t vmci_qpair_dequev(VMCIQPair *qpair, void *iov, size_t iovSize,
                          int mode);
ssize_t vmci_qpair_peekv(VMCIQPair *qpair, void *iov, size_t iovSize,
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


#if defined __cplusplus
} // extern "C"
#endif

#endif /* !__VMCI_KERNELAPI_1_H__ */

