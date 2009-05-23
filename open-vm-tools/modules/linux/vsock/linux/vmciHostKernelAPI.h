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
#if defined(VMKERNEL)
#include "vm_atomic.h"
#include "return_status.h"
#include "util_copy_dist.h"
#endif
#include "vmci_call_defs.h"

#if !defined(VMKERNEL)
#include "vmci_queue_pair.h"
#endif


/* VMCI Datagram API. */

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

int VMCIEvent_Subscribe(VMCI_Event event, VMCI_EventCB callback,
                        void *callbackData, VMCIId *subID);
int VMCIEvent_Unsubscribe(VMCIId subID);

/* VMCI Context API */

VMCIPrivilegeFlags VMCIContext_GetPrivFlags(VMCIId contextID);

/*
 * Queue pair operations for manipulating the content of a queue
 * pair are defined in vmci_queue_pair.h for non-vmkernel hosts.
 * Here we define the API for allocating and detaching from queue
 * pairs for all hosts, and content manipulation functions for
 * VMKERNEL.
 */

#if defined(VMKERNEL)

/*
 * For guest kernels, the VMCIQueue is directly mapped onto the
 * metadata page of the queue. In vmkernel, we use the VMCIQueue
 * structure to store the information necessary to retrieve the MPNs
 * of the queue. By redefining the structure, queue pair code using
 * the API below will be portable between guests and host kernels.
 */

typedef struct VMCIQueue {
   uint32 offset;                // Start page of queue in backing store
   uint32 len;                   // Number of pages in the queue
   struct QueuePairEntry *entry; // Endpoint state for the queue pair
} VMCIQueue;

/* VMCI Queuepair API  */
void VMCIQueue_Init(const VMCIHandle handle, VMCIQueue *queue);
void VMCIQueue_GetPointers(const VMCIQueue *produceQ, const VMCIQueue *consumeQ,
                           uint64 *producerTail, uint64 *consumerHead);
int64 VMCIQueue_FreeSpace(const VMCIQueue *produceQueue,
                          const VMCIQueue *consumeQueue,
                          const uint64 produceQSize);
int64 VMCIQueue_BufReady(const VMCIQueue *consumeQueue,
                         const VMCIQueue *produceQueue,
                         const uint64 consumeQSize);
ssize_t VMCIQueue_Enqueue(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                          const uint64 produceQSize, const void *buf,
                          size_t bufSize, Util_BufferType bufType);
ssize_t VMCIQueue_Peek(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                       const uint64 consumeQSize, void *buf,
                       size_t bufSize, Util_BufferType bufType);
ssize_t VMCIQueue_PeekV(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                        const uint64 consumeQSize, void *buf,
                        size_t bufSize, Util_BufferType bufType);
ssize_t VMCIQueue_Discard(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                          const uint64 consumeQSize, size_t bufSize);
ssize_t VMCIQueue_Dequeue(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                          const uint64 consumeQSize, void *buf,
                          size_t bufSize, Util_BufferType bufType);
ssize_t VMCIQueue_EnqueueV(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                           const uint64 produceQSize, const void *buf,
                           size_t bufSize, Util_BufferType bufType);
ssize_t VMCIQueue_DequeueV(VMCIQueue *produceQueue, const VMCIQueue *consumeQueue,
                           const uint64 consumeQSize, void *buf,
                           size_t bufSize, Util_BufferType bufType);
#endif	/* VMKERNEL  */

int VMCIQueuePair_Alloc(VMCIHandle *handle, VMCIQueue **produceQ,
                        uint64 produceSize, VMCIQueue **consumeQ,
                        uint64 consumeSize, VMCIId peer, uint32 flags);
int VMCIQueuePair_AllocPriv(VMCIHandle *handle, VMCIQueue **produceQ,
                            uint64 produceSize, VMCIQueue **consumeQ,
                            uint64 consumeSize, VMCIId peer, uint32 flags,
                            VMCIPrivilegeFlags privFlags);
int VMCIQueuePair_Detach(VMCIHandle handle);

#endif /* !__VMCI_HOSTKERNELAPI_H__ */

