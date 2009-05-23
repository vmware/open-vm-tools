/*********************************************************
 * Copyright (C) 2005-2008 VMware, Inc. All rights reserved.
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

#ifndef _VMCI_DEF_H_
#define _VMCI_DEF_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_types.h"

/* Register offsets. */
#define VMCI_STATUS_ADDR      0x00
#define VMCI_CONTROL_ADDR     0x04
#define VMCI_ICR_ADDR	      0x08
#define VMCI_IMR_ADDR         0x0c
#define VMCI_DATA_OUT_ADDR    0x10
#define VMCI_DATA_IN_ADDR     0x14
#define VMCI_CAPS_ADDR        0x18
#define VMCI_RESULT_LOW_ADDR  0x1c
#define VMCI_RESULT_HIGH_ADDR 0x20

/* Max number of devices. */
#define VMCI_MAX_DEVICES 1

/* Status register bits. */
#define VMCI_STATUS_INT_ON     0x1

/* Control register bits. */
#define VMCI_CONTROL_RESET        0x1
#define VMCI_CONTROL_INT_ENABLE   0x2
#define VMCI_CONTROL_INT_DISABLE  0x4

/* Capabilities register bits. */
#define VMCI_CAPS_HYPERCALL    0x1 
#define VMCI_CAPS_GUESTCALL    0x2
#define VMCI_CAPS_DATAGRAM     0x4

/* Interrupt Cause register bits. */
#define VMCI_ICR_DATAGRAM     0x1

/* Interrupt Mask register bits. */
#define VMCI_IMR_DATAGRAM     0x1

/* 
 * We have a fixed set of resource IDs available in the VMX. 
 * This allows us to have a very simple implementation since we statically 
 * know how many will create datagram handles. If a new caller arrives and
 * we have run out of slots we can manually increment the maximum size of
 * available resource IDs.
 */

typedef uint32 VMCI_Resource;

/* VMCI reserved hypervisor datagram resource IDs. */
#define VMCI_RESOURCES_QUERY      0
#define VMCI_GET_CONTEXT_ID       1
#define VMCI_SHAREDMEM_CREATE     2
#define VMCI_SHAREDMEM_ATTACH     3
#define VMCI_SHAREDMEM_DETACH     4
#define VMCI_SHAREDMEM_QUERY      5
#define VMCI_DATAGRAM_REQUEST_MAP 6
#define VMCI_DATAGRAM_REMOVE_MAP  7
#define VMCI_EVENT_SUBSCRIBE      8
#define VMCI_EVENT_UNSUBSCRIBE    9
#define VMCI_QUEUEPAIR_ALLOC      10
#define VMCI_QUEUEPAIR_DETACH     11
#define VMCI_VSOCK_VMX_LOOKUP     12
#define VMCI_RESOURCE_MAX         13

/* VMCI Ids. */
typedef uint32 VMCIId;

typedef struct VMCIHandle {
   VMCIId context;
   VMCIId resource;
} VMCIHandle;

static INLINE
VMCIHandle VMCI_MAKE_HANDLE(VMCIId cid, 
			    VMCIId rid)
{
   VMCIHandle h = {cid, rid};
   return h;
}

#define VMCI_HANDLE_TO_CONTEXT_ID(_handle) ((_handle).context)
#define VMCI_HANDLE_TO_RESOURCE_ID(_handle) ((_handle).resource)
#define VMCI_HANDLE_EQUAL(_h1, _h2) ((_h1).context == (_h2).context && \
				     (_h1).resource == (_h2).resource)

#define VMCI_INVALID_ID 0xFFFFFFFF
static const VMCIHandle VMCI_INVALID_HANDLE = {VMCI_INVALID_ID, 
					       VMCI_INVALID_ID};

#define VMCI_HANDLE_INVALID(_handle)   \
   VMCI_HANDLE_EQUAL((_handle), VMCI_INVALID_HANDLE)

/*
 * The below defines can be used to send anonymous requests.
 * This also indicates that no response is expected.
 */
#define VMCI_ANON_SRC_CONTEXT_ID   VMCI_INVALID_ID
#define VMCI_ANON_SRC_RESOURCE_ID  VMCI_INVALID_ID
#define VMCI_ANON_SRC_HANDLE       VMCI_MAKE_HANDLE(VMCI_ANON_SRC_CONTEXT_ID, \
						    VMCI_ANON_SRC_RESOURCE_ID)

/* The lowest 16 context ids are reserved for internal use. */
#define VMCI_RESERVED_CID_LIMIT 16

/*
 * Hypervisor context id, used for calling into hypervisor
 * supplied services from the VM. 
 */
#define VMCI_HYPERVISOR_CONTEXT_ID 0

/* 
 * Well-known context id, a logical context that contains 
 * a set of well-known services.
 */
#define VMCI_WELL_KNOWN_CONTEXT_ID 1

/* Todo: Change host context id to dynamic/random id. */
#define VMCI_HOST_CONTEXT_ID  2

/* 
 * The VMCI_CONTEXT_RESOURCE_ID is used together with VMCI_MAKE_HANDLE to make 
 * handles that refer to a specific context.
 */
#define VMCI_CONTEXT_RESOURCE_ID 0


/* VMCI error codes. */
#define VMCI_SUCCESS_QUEUEPAIR_ATTACH     5
#define VMCI_SUCCESS_QUEUEPAIR_CREATE     4
#define VMCI_SUCCESS_LAST_DETACH          3
#define VMCI_SUCCESS_ACCESS_GRANTED       2
#define VMCI_SUCCESS_ENTRY_DEAD           1
#define VMCI_SUCCESS                      0
#define VMCI_ERROR_INVALID_RESOURCE      (-1)
#define VMCI_ERROR_INVALID_ARGS          (-2)
#define VMCI_ERROR_NO_MEM                (-3)
#define VMCI_ERROR_DATAGRAM_FAILED       (-4)
#define VMCI_ERROR_MORE_DATA             (-5)
#define VMCI_ERROR_NO_MORE_DATAGRAMS     (-6)
#define VMCI_ERROR_NO_ACCESS             (-7)
#define VMCI_ERROR_NO_HANDLE             (-8)
#define VMCI_ERROR_DUPLICATE_ENTRY       (-9)
#define VMCI_ERROR_DST_UNREACHABLE       (-10)
#define VMCI_ERROR_PAYLOAD_TOO_LARGE     (-11)
#define VMCI_ERROR_INVALID_PRIV          (-12)
#define VMCI_ERROR_GENERIC               (-13)
#define VMCI_ERROR_PAGE_ALREADY_SHARED   (-14)
#define VMCI_ERROR_CANNOT_SHARE_PAGE     (-15)
#define VMCI_ERROR_CANNOT_UNSHARE_PAGE   (-16)
#define VMCI_ERROR_NO_PROCESS            (-17)
#define VMCI_ERROR_NO_DATAGRAM           (-18)
#define VMCI_ERROR_NO_RESOURCES          (-19)
#define VMCI_ERROR_UNAVAILABLE           (-20)
#define VMCI_ERROR_NOT_FOUND             (-21)
#define VMCI_ERROR_ALREADY_EXISTS        (-22)
#define VMCI_ERROR_NOT_PAGE_ALIGNED      (-23)
#define VMCI_ERROR_INVALID_SIZE          (-24)
#define VMCI_ERROR_REGION_ALREADY_SHARED (-25)
#define VMCI_ERROR_TIMEOUT               (-26)
#define VMCI_ERROR_DATAGRAM_INCOMPLETE   (-27)
#define VMCI_ERROR_INCORRECT_IRQL        (-28)
#define VMCI_ERROR_EVENT_UNKNOWN         (-29)
#define VMCI_ERROR_OBSOLETE              (-30)
#define VMCI_ERROR_QUEUEPAIR_MISMATCH    (-31)
#define VMCI_ERROR_QUEUEPAIR_NOTSET      (-32)
#define VMCI_ERROR_QUEUEPAIR_NOTOWNER    (-33)
#define VMCI_ERROR_QUEUEPAIR_NOTATTACHED (-34)
#define VMCI_ERROR_QUEUEPAIR_NOSPACE     (-35)
#define VMCI_ERROR_QUEUEPAIR_NODATA      (-36)
#define VMCI_ERROR_BUSMEM_INVALIDATION   (-37)
#define VMCI_ERROR_MODULE_NOT_LOADED     (-38)
 
/* Internal error codes. */
#define VMCI_SHAREDMEM_ERROR_BAD_CONTEXT (-1000)

#define VMCI_PATH_MAX 256

/* VMCI reserved events. */
typedef uint32 VMCI_Event;

#define VMCI_EVENT_CTX_ID_UPDATE  0
#define VMCI_EVENT_CTX_REMOVED    1
#define VMCI_EVENT_QP_RESUMED     2
#define VMCI_EVENT_QP_PEER_ATTACH 3
#define VMCI_EVENT_QP_PEER_DETACH 4
#define VMCI_EVENT_MAX            5

/* Reserved guest datagram resource ids. */
#define VMCI_EVENT_HANDLER 0

/* VMCI privileges. */
typedef enum VMCIResourcePrivilegeType {
   VMCI_PRIV_CH_PRIV,
   VMCI_PRIV_DESTROY_RESOURCE,
   VMCI_PRIV_ASSIGN_CLIENT,
   VMCI_PRIV_DG_CREATE,
   VMCI_PRIV_DG_SEND,
   VMCI_PRIV_SM_CREATE,
   VMCI_PRIV_SM_ATTACH,
   VMCI_NUM_PRIVILEGES,
} VMCIResourcePrivilegeType;

/* 
 * VMCI coarse-grained privileges (per context or host
 * process/endpoint. An entity with the restricted flag is only
 * allowed to interact with the hypervisor and trusted entities.
 */
typedef uint32 VMCIPrivilegeFlags;

#define VMCI_PRIVILEGE_FLAG_RESTRICTED     0x01
#define VMCI_PRIVILEGE_FLAG_TRUSTED        0x02
#define VMCI_PRIVILEGE_ALL_FLAGS           (VMCI_PRIVILEGE_FLAG_RESTRICTED | \
				            VMCI_PRIVILEGE_FLAG_TRUSTED)
#define VMCI_NO_PRIVILEGE_FLAGS            0x00
#define VMCI_DEFAULT_PROC_PRIVILEGE_FLAGS  VMCI_NO_PRIVILEGE_FLAGS
#define VMCI_LEAST_PRIVILEGE_FLAGS         VMCI_PRIVILEGE_FLAG_RESTRICTED
#define VMCI_MAX_PRIVILEGE_FLAGS           VMCI_PRIVILEGE_FLAG_TRUSTED

/* VMCI Discovery Service. */

/* Well-known handle to the discovery service. */
#define VMCI_DS_RESOURCE_ID 1 /* Reserved resource ID for discovery service. */
#define VMCI_DS_HANDLE VMCI_MAKE_HANDLE(VMCI_WELL_KNOWN_CONTEXT_ID, \
					VMCI_DS_RESOURCE_ID)
#define VMCI_DS_CONTEXT VMCI_MAKE_HANDLE(VMCI_WELL_KNOWN_CONTEXT_ID, \
					 VMCI_CONTEXT_RESOURCE_ID)

/* Maximum length of a DS message. */
#define VMCI_DS_MAX_MSG_SIZE        300

/* Command actions. */
#define VMCI_DS_ACTION_LOOKUP         0
#define VMCI_DS_ACTION_REGISTER       1
#define VMCI_DS_ACTION_UNREGISTER     2

/* Defines wire-protocol format for a request send to the DS from a context. */
typedef struct VMCIDsRequestHeader {
   int32       action;
   int32       msgid;
   VMCIHandle  handle;
   int32       nameLen;
   int8        name[1];   
} VMCIDsRequestHeader;


/* Defines the wire-protocol format for a request send from the DS to a context. */
typedef struct VMCIDsReplyHeader {
   int32       msgid;
   int32       code;
   VMCIHandle  handle;
   int32       msgLen;
   int8        msg[1];
} VMCIDsReplyHeader;

#define VMCI_PUBLIC_GROUP_NAME "vmci public group"
/* 0 through VMCI_RESERVED_RESOURCE_ID_MAX are reserved. */
#define VMCI_RESERVED_RESOURCE_ID_MAX 1023

#define VMCI_DOMAIN_NAME_MAXLEN  32

#define VMCI_LGPFX "VMCI: "

#endif

