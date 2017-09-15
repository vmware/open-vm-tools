/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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

#ifndef _VMCI_CALL_DEFS_H_
#define _VMCI_CALL_DEFS_H_

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vmci_defs.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * All structs here are an integral size of their largest member, ie. a struct 
 * with at least one 8-byte member will have a size that is an integral of 8.
 * A struct which has a largest member of size 4 will have a size that is an
 * integral of 4. This is because Windows CL enforces this rule. 32 bit gcc 
 * doesn't e.g. 32 bit gcc can misalign an 8 byte member if it is preceeded by
 * a 4 byte member. 
 */

/*
 * Base struct for vmci datagrams.
 */

typedef struct VMCIDatagram {
   VMCIHandle dst;
   VMCIHandle src;
   uint64     payloadSize;
} VMCIDatagram;


/*
 * Second flag is for creating a well-known handle instead of a per context
 * handle.  Next flag is for deferring datagram delivery, so that the
 * datagram callback is invoked in a delayed context (not interrupt context).
 */
#define VMCI_FLAG_DG_NONE          0
#define VMCI_FLAG_WELLKNOWN_DG_HND 0x1
#define VMCI_FLAG_ANYCID_DG_HND    0x2
#define VMCI_FLAG_DG_DELAYED_CB    0x4

/* Event callback should fire in a delayed context (not interrupt context.) */
#define VMCI_FLAG_EVENT_NONE       0
#define VMCI_FLAG_EVENT_DELAYED_CB 0x1

/* 
 * Maximum supported size of a VMCI datagram for routable datagrams.
 * Datagrams going to the hypervisor are allowed to be larger.
 */
#define VMCI_MAX_DG_SIZE (17 * 4096)
#define VMCI_MAX_DG_PAYLOAD_SIZE (VMCI_MAX_DG_SIZE - sizeof(VMCIDatagram))
#define VMCI_DG_PAYLOAD(_dg) (void *)((char *)(_dg) + sizeof(VMCIDatagram))
#define VMCI_DG_HEADERSIZE sizeof(VMCIDatagram)
#define VMCI_DG_SIZE(_dg) (VMCI_DG_HEADERSIZE + (size_t)(_dg)->payloadSize)
#define VMCI_DG_SIZE_ALIGNED(_dg) ((VMCI_DG_SIZE(_dg) + 7) & (size_t)~7)
#define VMCI_MAX_DATAGRAM_QUEUE_SIZE (VMCI_MAX_DG_SIZE * 2)

/*
 * We allow at least 1024 more event datagrams from the hypervisor past the
 * normally allowed datagrams pending for a given context.  We define this
 * limit on event datagrams from the hypervisor to guard against DoS attack
 * from a malicious VM which could repeatedly attach to and detach from a queue
 * pair, causing events to be queued at the destination VM.  However, the rate
 * at which such events can be generated is small since it requires a VM exit
 * and handling of queue pair attach/detach call at the hypervisor.  Event
 * datagrams may be queued up at the destination VM if it has interrupts
 * disabled or if it is not draining events for some other reason.  1024
 * datagrams is a grossly conservative estimate of the time for which
 * interrupts may be disabled in the destination VM, but at the same time does
 * not exacerbate the memory pressure problem on the host by much (size of each
 * event datagram is small).
 */
#define VMCI_MAX_DATAGRAM_AND_EVENT_QUEUE_SIZE \
   (VMCI_MAX_DATAGRAM_QUEUE_SIZE + \
    1024 * (sizeof(VMCIDatagram) + sizeof(VMCIEventData_Max)))

/*
 * Struct for sending VMCI_DATAGRAM_REQUEST_MAP and
 * VMCI_DATAGRAM_REMOVE_MAP datagrams. Struct size is 32 bytes. All
 * fields in struct are aligned to their natural alignment. These
 * datagrams are obsoleted by the removal of VM to VM communication.
 */
typedef struct VMCIDatagramWellKnownMapMsg {
   VMCIDatagram hdr;
   VMCIId       wellKnownID;
   uint32       _pad;
} VMCIDatagramWellKnownMapMsg;


/*
 * Struct used for querying, via VMCI_RESOURCES_QUERY, the availability of 
 * hypervisor resources. 
 * Struct size is 16 bytes. All fields in struct are aligned to their natural
 * alignment.
 */
typedef struct VMCIResourcesQueryHdr {
   VMCIDatagram hdr;
   uint32       numResources;
   uint32       _padding;
} VMCIResourcesQueryHdr;


/*
 * Convenience struct for negotiating vectors. Must match layout of
 * VMCIResourceQueryHdr minus the VMCIDatagram header.
 */
typedef struct VMCIResourcesQueryMsg {
   uint32        numResources;
   uint32        _padding;
   VMCI_Resource resources[1];
} VMCIResourcesQueryMsg;


/* 
 * The maximum number of resources that can be queried using
 * VMCI_RESOURCE_QUERY is 31, as the result is encoded in the lower 31
 * bits of a positive return value. Negative values are reserved for
 * errors.
 */
#define VMCI_RESOURCE_QUERY_MAX_NUM 31

/* Maximum size for the VMCI_RESOURCE_QUERY request. */
#define VMCI_RESOURCE_QUERY_MAX_SIZE sizeof(VMCIResourcesQueryHdr) \
      + VMCI_RESOURCE_QUERY_MAX_NUM * sizeof(VMCI_Resource)

/* 
 * Struct used for setting the notification bitmap.  All fields in
 * struct are aligned to their natural alignment.
 */
typedef struct VMCINotifyBitmapSetMsg {
   VMCIDatagram hdr;
   PPN          bitmapPPN;
   uint32       _pad;
} VMCINotifyBitmapSetMsg;


/* 
 * Struct used for linking a doorbell handle with an index in the
 * notify bitmap. All fields in struct are aligned to their natural
 * alignment.
 */
typedef struct VMCIDoorbellLinkMsg {
   VMCIDatagram hdr;
   VMCIHandle   handle;
   uint64       notifyIdx;
} VMCIDoorbellLinkMsg;


/* 
 * Struct used for unlinking a doorbell handle from an index in the
 * notify bitmap. All fields in struct are aligned to their natural
 * alignment.
 */
typedef struct VMCIDoorbellUnlinkMsg {
   VMCIDatagram hdr;
   VMCIHandle   handle;
} VMCIDoorbellUnlinkMsg;


/* 
 * Struct used for generating a notification on a doorbell handle. All
 * fields in struct are aligned to their natural alignment.
 */
typedef struct VMCIDoorbellNotifyMsg {
   VMCIDatagram hdr;
   VMCIHandle   handle;
} VMCIDoorbellNotifyMsg;


/* 
 * This struct is used to contain data for events.  Size of this struct is a
 * multiple of 8 bytes, and all fields are aligned to their natural alignment.
 */
typedef struct VMCI_EventData {
   VMCI_Event event; /* 4 bytes. */
   uint32     _pad;
   /*
    * Event payload is put here.
    */
} VMCI_EventData;


/* Callback needed for correctly waiting on events. */

typedef int
(*VMCIDatagramRecvCB)(void *clientData,   // IN: client data for handler
                      VMCIDatagram *msg); // IN: 


/*
 * We use the following inline function to access the payload data associated
 * with an event data.
 */

static INLINE void *
VMCIEventDataPayload(VMCI_EventData *evData) // IN:
{
   return (void *)((char *)evData + sizeof *evData);
}

/*
 * Define the different VMCI_EVENT payload data types here.  All structs must
 * be a multiple of 8 bytes, and fields must be aligned to their natural
 * alignment.
 */
typedef struct VMCIEventPayload_Context {
   VMCIId contextID; /* 4 bytes. */
   uint32 _pad;
} VMCIEventPayload_Context;

typedef struct VMCIEventPayload_QP {
   VMCIHandle handle; /* QueuePair handle. */
   VMCIId     peerId; /* Context id of attaching/detaching VM. */
   uint32     _pad;
} VMCIEventPayload_QP;

/*
 * We define the following struct to get the size of the maximum event data
 * the hypervisor may send to the guest.  If adding a new event payload type
 * above, add it to the following struct too (inside the union).
 */
typedef struct VMCIEventData_Max {
   VMCI_EventData eventData;
   union {
      VMCIEventPayload_Context contextPayload;
      VMCIEventPayload_QP      qpPayload;
   } evDataPayload;
} VMCIEventData_Max;


/* 
 * Struct used for VMCI_EVENT_SUBSCRIBE/UNSUBSCRIBE and VMCI_EVENT_HANDLER 
 * messages.  Struct size is 32 bytes.  All fields in struct are aligned to
 * their natural alignment.
 */
typedef struct VMCIEventMsg {
   VMCIDatagram   hdr;
   VMCI_EventData eventData; /* Has event type and payload. */
   /*
    * Payload gets put here.
    */
} VMCIEventMsg;


/*
 * We use the following inline function to access the payload data associated
 * with an event message.
 */

static INLINE void *
VMCIEventMsgPayload(VMCIEventMsg *eMsg) // IN:
{
   return VMCIEventDataPayload(&eMsg->eventData);
}


/* Flags for VMCI QueuePair API. */
#define VMCI_QPFLAG_ATTACH_ONLY 0x1 /* Fail alloc if QP not created by peer. */
#define VMCI_QPFLAG_LOCAL       0x2 /* Only allow attaches from local context. */
#define VMCI_QPFLAG_NONBLOCK    0x4 /* Host won't block when guest is quiesced. */
/* For asymmetric queuepairs, update as new flags are added. */
#define VMCI_QP_ASYMM           VMCI_QPFLAG_NONBLOCK
#define VMCI_QP_ASYMM_PEER      (VMCI_QPFLAG_ATTACH_ONLY | VMCI_QP_ASYMM)
/* Update the following (bitwise OR flags) while adding new flags. */
#define VMCI_QP_ALL_FLAGS       (VMCI_QPFLAG_ATTACH_ONLY | VMCI_QPFLAG_LOCAL | \
                                 VMCI_QPFLAG_NONBLOCK)

/*
 * Structs used for QueuePair alloc and detach messages.  We align fields of
 * these structs to 64bit boundaries.
 */

typedef struct VMCIQueuePairAllocMsg {
   VMCIDatagram   hdr;
   VMCIHandle     handle;
   VMCIId         peer; /* 32bit field. */
   uint32         flags;
   uint64         produceSize;
   uint64         consumeSize;
   uint64         numPPNs;
   /* List of PPNs placed here. */
} VMCIQueuePairAllocMsg;


typedef struct VMCIQueuePairDetachMsg {
   VMCIDatagram  hdr;
   VMCIHandle    handle;
} VMCIQueuePairDetachMsg;


#if defined __cplusplus
} // extern "C"
#endif

#endif // _VMCI_CALL_DEFS_H_
