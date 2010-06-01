/*********************************************************
 * Copyright (C) 2006-2007 VMware, Inc. All rights reserved.
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
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vmci_defs.h"

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


/* Flag for creating a wellknown handle instead of a per context handle. */
#define VMCI_FLAG_WELLKNOWN_DG_HND 0x1

/* 
 * Maximum supported size of a VMCI datagram for routable datagrams.
 * Datagrams going to the hypervisor are allowed to be larger.
 */
#define VMCI_MAX_DG_SIZE (17 * 4096)
#define VMCI_MAX_DG_PAYLOAD_SIZE (VMCI_MAX_DG_SIZE - sizeof(VMCIDatagram))
#define VMCI_DG_PAYLOAD(_dg) (void *)((char *)(_dg) + sizeof(VMCIDatagram))
#define VMCI_DG_HEADERSIZE sizeof(VMCIDatagram)
#define VMCI_DG_SIZE(_dg) (VMCI_DG_HEADERSIZE + (size_t)(_dg)->payloadSize)
#define VMCI_DG_SIZE_ALIGNED(_dg) ((VMCI_DG_SIZE(_dg) + 7) & (size_t)CONST64U(0xfffffffffffffff8))
#define VMCI_MAX_DATAGRAM_QUEUE_SIZE  (VMCI_MAX_DG_SIZE * 2)

/* 
 * Struct for sending VMCI_DATAGRAM_REQUEST_MAP and VMCI_DATAGRAM_REMOVE_MAP
 * datagrams. Struct size is 32 bytes. All fields in struct are aligned to
 * their natural alignment.
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
typedef struct VMCIResourcesQueuryHdr {
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
 * Struct used for making VMCI_SHAREDMEM_CREATE message. Struct size is 24 bytes.
 * All fields in struct are aligned to their natural alignment.
 */
typedef struct VMCISharedMemCreateMsg {
   VMCIDatagram hdr;
   VMCIHandle   handle;
   uint32       memSize;
   uint32       _padding;
   /* PPNs placed after struct. */
} VMCISharedMemCreateMsg;


/* 
 * Struct used for sending VMCI_SHAREDMEM_ATTACH messages. Same as struct used 
 * for create messages.
 */
typedef VMCISharedMemCreateMsg VMCISharedMemAttachMsg;


/* 
 * Struct used for sending VMCI_SHAREDMEM_DETACH messsages. Struct size is 16
 * bytes. All fields in struct are aligned to their natural alignment.
 */
typedef struct VMCISharedMemDetachMsg {
   VMCIDatagram hdr;
   VMCIHandle handle;
} VMCISharedMemDetachMsg;


/* 
 * Struct used for sending VMCI_SHAREDMEM_QUERY messages. Same as struct used 
 * for detach messages.
 */
typedef VMCISharedMemDetachMsg VMCISharedMemQueryMsg;


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
/* Update the following (bitwise OR flags) while adding new flags. */
#define VMCI_QP_ALL_FLAGS       (VMCI_QPFLAG_ATTACH_ONLY | VMCI_QPFLAG_LOCAL)

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

#endif
