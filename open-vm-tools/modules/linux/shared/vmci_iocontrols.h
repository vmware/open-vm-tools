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
 * vmci_iocontrols.h
 *
 *        The VMCI driver io controls.
 */

#ifndef _VMCI_IOCONTROLS_H_
#define _VMCI_IOCONTROLS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vm_assert.h"


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIVA64ToPtr --
 *
 *      Convert a VA64 to a pointer.
 *
 * Results:
 *      Virtual address.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
VMCIVA64ToPtr(VA64 va64) // IN
{
#ifdef VM_X86_64
   ASSERT_ON_COMPILE(sizeof (void *) == 8);
#else
   ASSERT_ON_COMPILE(sizeof (void *) == 4);
   // Check that nothing of value will be lost.
   ASSERT(!(va64 >> 32));
#endif
   return (void *)(uintptr_t)va64;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIPtrToVA64 --
 *
 *      Convert a pointer to a VA64.
 *
 * Results:
 *      Virtual address.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE VA64
VMCIPtrToVA64(void const *ptr) // IN
{
   ASSERT_ON_COMPILE(sizeof ptr <= sizeof (VA64));
   return (VA64)(uintptr_t)ptr;
}


/*
 * Driver version.
 *
 * Increment major version when you make an incompatible change.
 * Compatibility goes both ways (old driver with new executable
 * as well as new driver with old executable).
 */

#define VMCI_VERSION_SHIFT_WIDTH   16 /* Never change this. */
#define VMCI_MAKE_VERSION(_major, _minor)    ((_major) <<                \
                                              VMCI_VERSION_SHIFT_WIDTH | \
                                              (uint16) (_minor))
#define VMCI_VERSION_MAJOR(v)  ((uint32) (v) >> VMCI_VERSION_SHIFT_WIDTH)
#define VMCI_VERSION_MINOR(v)  ((uint16) (v))

/*
 * VMCI_VERSION is always the current version.  Subsequently listed
 * versions are ways of detecting previous versions of the connecting
 * application (i.e., VMX).
 *
 * VMCI_VERSION_HOSTQP: This version introduced host end point support
 * for hosted products.
 *
 * VMCI_VERSION_PREHOSTQP: This is the version prior to the adoption of
 * support for host end-points.
 *
 * VMCI_VERSION_PREVERS2: This fictional version number is intended to
 * represent the version of a VMX which doesn't call into the driver
 * with ioctl VERSION2 and thus doesn't establish its version with the
 * driver.
 */

#define VMCI_VERSION                VMCI_VERSION_HOSTQP
#define VMCI_VERSION_HOSTQP         VMCI_MAKE_VERSION(9, 0)
#define VMCI_VERSION_PREHOSTQP      VMCI_MAKE_VERSION(8, 0)
#define VMCI_VERSION_PREVERS2       VMCI_MAKE_VERSION(1, 0)

#if defined(__linux__) || defined(__APPLE__) || defined(SOLARIS) || defined(VMKERNEL)
/*
 * Linux defines _IO* macros, but the core kernel code ignore the encoded
 * ioctl value. It is up to individual drivers to decode the value (for
 * example to look at the size of a structure to determine which version
 * of a specific command should be used) or not (which is what we
 * currently do, so right now the ioctl value for a given command is the
 * command itself).
 *
 * Hence, we just define the IOCTL_VMCI_foo values directly, with no
 * intermediate IOCTLCMD_ representation.
 */
#  define IOCTLCMD(_cmd) IOCTL_VMCI_ ## _cmd
#else // if defined(__linux__)
/*
 * On platforms other than Linux, IOCTLCMD_foo values are just numbers, and
 * we build the IOCTL_VMCI_foo values around these using platform-specific
 * format for encoding arguments and sizes.
 */
#  define IOCTLCMD(_cmd) IOCTLCMD_VMCI_ ## _cmd
#endif


enum IOCTLCmd_VMCI {
   /*
    * We need to bracket the range of values used for ioctls, because x86_64
    * Linux forces us to explicitly register ioctl handlers by value for
    * handling 32 bit ioctl syscalls.  Hence FIRST and LAST.  Pick something
    * for FIRST that doesn't collide with vmmon (2001+).
    */
#if defined(__linux__)
   IOCTLCMD(FIRST) = 1951,
#else
   /* Start at 0. */
   IOCTLCMD(FIRST),
#endif
   IOCTLCMD(VERSION) = IOCTLCMD(FIRST),

   /* BEGIN VMCI */
   IOCTLCMD(INIT_CONTEXT),
   IOCTLCMD(CREATE_PROCESS),
   IOCTLCMD(CREATE_DATAGRAM_PROCESS),
   IOCTLCMD(SHAREDMEM_CREATE),
   IOCTLCMD(SHAREDMEM_ATTACH),
   IOCTLCMD(SHAREDMEM_QUERY),
   IOCTLCMD(SHAREDMEM_DETACH),
   IOCTLCMD(VERSION2),
   IOCTLCMD(QUEUEPAIR_ALLOC),
   IOCTLCMD(QUEUEPAIR_SETPAGEFILE),
   IOCTLCMD(QUEUEPAIR_DETACH),
   IOCTLCMD(DATAGRAM_SEND),
   IOCTLCMD(DATAGRAM_RECEIVE),
   IOCTLCMD(DATAGRAM_REQUEST_MAP),
   IOCTLCMD(DATAGRAM_REMOVE_MAP),
   IOCTLCMD(CTX_ADD_NOTIFICATION),
   IOCTLCMD(CTX_REMOVE_NOTIFICATION),
   IOCTLCMD(CTX_GET_CPT_STATE),
   IOCTLCMD(CTX_SET_CPT_STATE),
   IOCTLCMD(GET_CONTEXT_ID),
   /* END VMCI */

   /*
    * BEGIN VMCI SOCKETS
    *
    * We mark the end of the vmci commands and the start of the vmci sockets
    * commands since they are used in separate modules on Linux.
    * */
   IOCTLCMD(LAST),
   IOCTLCMD(SOCKETS_FIRST) = IOCTLCMD(LAST),
   IOCTLCMD(SOCKETS_ACCEPT) = IOCTLCMD(SOCKETS_FIRST),
   IOCTLCMD(SOCKETS_BIND),
   IOCTLCMD(SOCKETS_CLOSE),
   IOCTLCMD(SOCKETS_CONNECT),
   /*
    * The next two values are public (vmci_sockets.h) and cannot be changed.
    * That means the number of values above these cannot be changed either
    * unless the base index (specified below) is updated accordingly.
    */
   IOCTLCMD(SOCKETS_GET_AF_VALUE),
   IOCTLCMD(SOCKETS_GET_LOCAL_CID),
   IOCTLCMD(SOCKETS_GET_SOCK_NAME),
   IOCTLCMD(SOCKETS_GET_SOCK_OPT),
   IOCTLCMD(SOCKETS_GET_VM_BY_NAME),
   IOCTLCMD(SOCKETS_IOCTL),
   IOCTLCMD(SOCKETS_LISTEN),
   IOCTLCMD(SOCKETS_RECV),
   IOCTLCMD(SOCKETS_RECV_FROM),
   IOCTLCMD(SOCKETS_SELECT),
   IOCTLCMD(SOCKETS_SEND),
   IOCTLCMD(SOCKETS_SEND_TO),
   IOCTLCMD(SOCKETS_SET_SOCK_OPT),
   IOCTLCMD(SOCKETS_SHUTDOWN),
   IOCTLCMD(SOCKETS_SOCKET), /* 1990 on Linux. */
   /* END VMCI SOCKETS */

   /*
    * We reserve a range of 4 ioctls for VMCI Sockets to grow.  We cannot
    * reserve many ioctls here since we are close to overlapping with vmmon
    * ioctls.  Define a meta-ioctl if running out of this binary space.
    */
   // Must be last.
   IOCTLCMD(SOCKETS_LAST) = IOCTLCMD(SOCKETS_SOCKET) + 4, /* 1994 on Linux. */

   /*
    * The VSockets ioctls occupy the block above.  We define a new range of
    * VMCI ioctls to maintain binary compatibility between the user land and
    * the kernel driver.  Careful, vmmon ioctls start from 2001, so this means
    * we can add only 4 new VMCI ioctls.  Define a meta-ioctl if running out of
    * this binary space.
    */

   IOCTLCMD(FIRST2),
   IOCTLCMD(SET_NOTIFY) = IOCTLCMD(FIRST2), /* 1995 on Linux. */
   IOCTLCMD(LAST2),
};


#if defined _WIN32
/*
 * Windows VMCI ioctl definitions.
 */

/*
 * The first is the device name in user-mode.  The next two are for registering
 * or opening the device in kernel-mode, and are always in UNICODE.
 */
#define VMCI_DEVICE_NAME         TEXT("\\\\.\\VMCI")
#define VMCI_DEVICE_NAME_PATH    L"\\Device\\vmci"
#define VMCI_DEVICE_LINK_PATH    L"\\DosDevices\\vmci"

/* These values cannot be changed since some of the ioctl values are public. */
#define FILE_DEVICE_VMCI         0x8103
#define VMCI_IOCTL_BASE_INDEX    0x801
#define VMCIIOCTL_BUFFERED(name) \
      CTL_CODE(FILE_DEVICE_VMCI, \
	       VMCI_IOCTL_BASE_INDEX + IOCTLCMD_VMCI_ ## name, \
	       METHOD_BUFFERED, \
	       FILE_ANY_ACCESS)
#define VMCIIOCTL_NEITHER(name) \
      CTL_CODE(FILE_DEVICE_VMCI, \
	       VMCI_IOCTL_BASE_INDEX + IOCTLCMD_VMCI_ ## name, \
	       METHOD_NEITHER, \
	       FILE_ANY_ACCESS)

#define IOCTL_VMCI_VERSION		VMCIIOCTL_BUFFERED(VERSION)

/* BEGIN VMCI */
#define IOCTL_VMCI_INIT_CONTEXT         VMCIIOCTL_BUFFERED(INIT_CONTEXT)
#define IOCTL_VMCI_CREATE_PROCESS       VMCIIOCTL_BUFFERED(CREATE_PROCESS)
#define IOCTL_VMCI_CREATE_DATAGRAM_PROCESS \
               VMCIIOCTL_BUFFERED(CREATE_DATAGRAM_PROCESS)
#define IOCTL_VMCI_HYPERCALL            VMCIIOCTL_BUFFERED(HYPERCALL)
#define IOCTL_VMCI_SHAREDMEM_CREATE  \
               VMCIIOCTL_BUFFERED(SHAREDMEM_CREATE)
#define IOCTL_VMCI_SHAREDMEM_ATTACH  \
               VMCIIOCTL_BUFFERED(SHAREDMEM_ATTACH)
#define IOCTL_VMCI_SHAREDMEM_QUERY   \
               VMCIIOCTL_BUFFERED(SHAREDMEM_QUERY)
#define IOCTL_VMCI_SHAREDMEM_DETACH  \
               VMCIIOCTL_BUFFERED(SHAREDMEM_DETACH)
#define IOCTL_VMCI_VERSION2		VMCIIOCTL_BUFFERED(VERSION2)
#define IOCTL_VMCI_QUEUEPAIR_ALLOC  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_ALLOC)
#define IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_SETPAGEFILE)
#define IOCTL_VMCI_QUEUEPAIR_DETACH  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_DETACH)
#define IOCTL_VMCI_DATAGRAM_SEND	VMCIIOCTL_BUFFERED(DATAGRAM_SEND)
#define IOCTL_VMCI_DATAGRAM_RECEIVE	VMCIIOCTL_NEITHER(DATAGRAM_RECEIVE)
#define IOCTL_VMCI_DATAGRAM_REQUEST_MAP	VMCIIOCTL_BUFFERED(DATAGRAM_REQUEST_MAP)
#define IOCTL_VMCI_DATAGRAM_REMOVE_MAP	VMCIIOCTL_BUFFERED(DATAGRAM_REMOVE_MAP)
#define IOCTL_VMCI_CTX_ADD_NOTIFICATION	VMCIIOCTL_BUFFERED(CTX_ADD_NOTIFICATION)
#define IOCTL_VMCI_CTX_REMOVE_NOTIFICATION \
               VMCIIOCTL_BUFFERED(CTX_REMOVE_NOTIFICATION)
#define IOCTL_VMCI_CTX_GET_CPT_STATE \
               VMCIIOCTL_BUFFERED(CTX_GET_CPT_STATE)
#define IOCTL_VMCI_CTX_SET_CPT_STATE \
               VMCIIOCTL_BUFFERED(CTX_SET_CPT_STATE)
#define IOCTL_VMCI_GET_CONTEXT_ID    \
               VMCIIOCTL_BUFFERED(GET_CONTEXT_ID)
/* END VMCI */

/* BEGIN VMCI SOCKETS */
#define IOCTL_VMCI_SOCKETS_ACCEPT \
               VMCIIOCTL_BUFFERED(SOCKETS_ACCEPT)
#define IOCTL_VMCI_SOCKETS_BIND \
               VMCIIOCTL_BUFFERED(SOCKETS_BIND)
#define IOCTL_VMCI_SOCKETS_CLOSE \
               VMCIIOCTL_BUFFERED(SOCKETS_CLOSE)
#define IOCTL_VMCI_SOCKETS_CONNECT \
               VMCIIOCTL_BUFFERED(SOCKETS_CONNECT)
#define IOCTL_VMCI_SOCKETS_GET_AF_VALUE \
               VMCIIOCTL_BUFFERED(SOCKETS_GET_AF_VALUE)
#define IOCTL_VMCI_SOCKETS_GET_LOCAL_CID \
               VMCIIOCTL_BUFFERED(SOCKETS_GET_LOCAL_CID)
#define IOCTL_VMCI_SOCKETS_GET_SOCK_NAME \
               VMCIIOCTL_BUFFERED(SOCKETS_GET_SOCK_NAME)
#define IOCTL_VMCI_SOCKETS_GET_SOCK_OPT \
               VMCIIOCTL_BUFFERED(SOCKETS_GET_SOCK_OPT)
#define IOCTL_VMCI_SOCKETS_GET_VM_BY_NAME \
               VMCIIOCTL_BUFFERED(SOCKETS_GET_VM_BY_NAME)
#define IOCTL_VMCI_SOCKETS_IOCTL \
               VMCIIOCTL_BUFFERED(SOCKETS_IOCTL)
#define IOCTL_VMCI_SOCKETS_LISTEN \
               VMCIIOCTL_BUFFERED(SOCKETS_LISTEN)
#define IOCTL_VMCI_SOCKETS_RECV \
               VMCIIOCTL_BUFFERED(SOCKETS_RECV)
#define IOCTL_VMCI_SOCKETS_RECV_FROM \
               VMCIIOCTL_BUFFERED(SOCKETS_RECV_FROM)
#define IOCTL_VMCI_SOCKETS_SELECT \
               VMCIIOCTL_BUFFERED(SOCKETS_SELECT)
#define IOCTL_VMCI_SOCKETS_SEND \
               VMCIIOCTL_BUFFERED(SOCKETS_SEND)
#define IOCTL_VMCI_SOCKETS_SEND_TO \
               VMCIIOCTL_BUFFERED(SOCKETS_SEND_TO)
#define IOCTL_VMCI_SOCKETS_SET_SOCK_OPT \
               VMCIIOCTL_BUFFERED(SOCKETS_SET_SOCK_OPT)
#define IOCTL_VMCI_SOCKETS_SHUTDOWN \
               VMCIIOCTL_BUFFERED(SOCKETS_SHUTDOWN)
#define IOCTL_VMCI_SOCKETS_SOCKET \
               VMCIIOCTL_BUFFERED(SOCKETS_SOCKET)
/* END VMCI SOCKETS */

#endif // _WIN32


/*
 * VMCI driver initialization. This block can also be used to
 * pass initial group membership etc.
 */
typedef struct VMCIInitBlock {
   VMCIId             cid;
   VMCIPrivilegeFlags flags;
#ifdef _WIN32
   uint64             event; /* Handle for signalling vmci calls on windows. */
#endif // _WIN32
} VMCIInitBlock;

typedef struct VMCISharedMemInfo {
   VMCIHandle handle;
   uint32     size;
   uint32     result;     
   VA64       va; /* Currently only used in the guest. */ 
   char       pageFileName[VMCI_PATH_MAX];
} VMCISharedMemInfo;

typedef struct VMCIQueuePairAllocInfo {
   VMCIHandle handle;
   VMCIId     peer;
   uint32     flags;
   uint64     produceSize;
   uint64     consumeSize;
   VA64       producePageFile; /* User VA. */
   VA64       consumePageFile; /* User VA. */
   uint64     producePageFileSize; /* Size of the file name array. */
   uint64     consumePageFileSize; /* Size of the file name array. */ 
   int32      result;
   uint32     _pad;
} VMCIQueuePairAllocInfo;

/*
 * For backwards compatibility, here is a version of the
 * VMCIQueuePairPageFileInfo before host support end-points was added.
 * Note that the current version of that structure requires VMX to
 * pass down the VA of the mapped file.  Before host support was added
 * there was nothing of the sort.  So, when the driver sees the ioctl
 * with a parameter that is the sizeof
 * VMCIQueuePairPageFileInfo_NoHostQP then it can infer that the version
 * of VMX running can't attach to host end points because it doesn't
 * provide the VA of the mapped files.
 *
 * The Linux driver doesn't get an indication of the size of the
 * structure passed down from user space.  So, to fix a long standing
 * but unfiled bug, the _pad field has been renamed to version.
 * Existing versions of VMX always initialize the PageFileInfo
 * structure so that _pad, er, version is set to 0.
 *
 * A version value of 1 indicates that the size of the structure has
 * been increased to include two UVA's: produceUVA and consumeUVA.
 * These UVA's are of the mmap()'d queue contents backing files.
 *
 * In addition, if when VMX is sending down the
 * VMCIQueuePairPageFileInfo structure it gets an error then it will
 * try again with the _NoHostQP version of the file to see if an older
 * VMCI kernel module is running.
 */
typedef struct VMCIQueuePairPageFileInfo_NoHostQP {
   VMCIHandle handle;
   VA64       producePageFile; /* User VA. */
   VA64       consumePageFile; /* User VA. */
   uint64     producePageFileSize; /* Size of the file name array. */
   uint64     consumePageFileSize; /* Size of the file name array. */
   int32      result;
   uint32     version;         /* Was _pad. Must be 0. */
} VMCIQueuePairPageFileInfo_NoHostQP;

typedef struct VMCIQueuePairPageFileInfo {
   VMCIHandle handle;
   VA64       producePageFile; /* User VA. */
   VA64       consumePageFile; /* User VA. */
   uint64     producePageFileSize; /* Size of the file name array. */
   uint64     consumePageFileSize; /* Size of the file name array. */
   int32      result;
   uint32     version;   /* Was _pad. */
   VA64       produceVA; /* User VA of the mapped file. */
   VA64       consumeVA; /* User VA of the mapped file. */
} VMCIQueuePairPageFileInfo;

typedef struct VMCIQueuePairDetachInfo {
   VMCIHandle handle;
   int32      result;
   uint32     _pad;
} VMCIQueuePairDetachInfo;

typedef struct VMCIDatagramSendRecvInfo {
   VA64   addr;
   uint32 len;
   int32  result;
} VMCIDatagramSendRecvInfo;

/* Used to create datagram endpoints in guest or host userlevel. */
typedef struct VMCIDatagramCreateInfo {
   VMCIId      resourceID;
   uint32      flags; 
#ifdef _WIN32
   int         eventHnd;
#else
   int         _unused;
#endif
   int         result;     // result of handle create operation
   VMCIHandle  handle;     // handle if successfull
} VMCIDatagramCreateInfo;

/* Used to add/remove well-known datagram mappings. */
typedef struct VMCIDatagramMapInfo {
   VMCIId      wellKnownID;
   int         result;
} VMCIDatagramMapInfo;


/* Used to add/remove remote context notifications. */
typedef struct VMCINotifyAddRemoveInfo {
   VMCIId      remoteCID;
   int         result;
} VMCINotifyAddRemoveInfo;


/* Used to set/get current context's checkpoint state. */
typedef struct VMCICptBufInfo {
   VA64        cptBuf;
   uint32      cptType;
   uint32      bufSize;
   int32       result;
   uint32      _pad;
} VMCICptBufInfo;

/* Used to pass notify flag's address to the host driver. */
typedef struct VMCISetNotifyInfo {
   VA64        notifyUVA;
   int32       result;
   uint32      _pad;
} VMCISetNotifyInfo;

/* User space daemon command numbers. */
typedef enum VMCIDRequestType {
    VMCID_REQ_NEW_PAGE_STORE,
    VMCID_REQ_FREE_PAGE_STORE,
    VMCID_REQ_ATTACH_PAGE_STORE,
    VMCID_REQ_DETACH_PAGE_STORE,
} VMCIDRequestType;

#define VMCI_VMCID_INVALID_REQ  CONST64U(-1)

/* Used to pass requests/responses to the user space daemon. */
typedef struct VMCIDRpc {
    uint64           reqId;
    uint32           reqType;
    uint32           reqResult;
    /* Passing page file names */
    VA64             producePageFile; /* User VA. */
    VA64             consumePageFile; /* User VA. */
    uint64           producePageFileSize; /* Size of the file name array. */
    uint64           consumePageFileSize; /* Size of the file name array. */
    /* Used for attach/detach */
    VA64             produceVA;
    VA64             consumeVA;
    uint64           numProducePages;
    uint64           numConsumePages;
} VMCIDRpc;

#ifdef __APPLE__
/*
 * Mac OS ioctl definitions.
 *
 * Mac OS defines _IO* macros, and the core kernel code uses the size encoded
 * in the ioctl value to copy the memory back and forth (depending on the
 * direction encoded in the ioctl value) between the user and kernel address
 * spaces.
 * See iocontrolsMacOS.h for details on how this is done. We use sockets only
 * for vmci.
 */

#include <sys/ioccom.h>

enum VMCrossTalkSockOpt {
   VMCI_SO_VERSION = 0,
   VMCI_SO_CONTEXT                  = IOCTL_VMCI_INIT_CONTEXT,
   VMCI_SO_PROCESS                  = IOCTL_VMCI_CREATE_PROCESS,
   VMCI_SO_DATAGRAM_PROCESS         = IOCTL_VMCI_CREATE_DATAGRAM_PROCESS,
   VMCI_SO_SHAREDMEM_CREATE         = IOCTL_VMCI_SHAREDMEM_CREATE,
   VMCI_SO_SHAREDMEM_ATTACH         = IOCTL_VMCI_SHAREDMEM_ATTACH,
   VMCI_SO_SHAREDMEM_QUERY          = IOCTL_VMCI_SHAREDMEM_QUERY,
   VMCI_SO_SHAREDMEM_DETACH         = IOCTL_VMCI_SHAREDMEM_DETACH,
   VMCI_SO_VERSION2                 = IOCTL_VMCI_VERSION2,
   VMCI_SO_QUEUEPAIR_ALLOC          = IOCTL_VMCI_QUEUEPAIR_ALLOC,
   VMCI_SO_QUEUEPAIR_SETPAGEFILE    = IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE,
   VMCI_SO_QUEUEPAIR_DETACH         = IOCTL_VMCI_QUEUEPAIR_DETACH,
   VMCI_SO_DATAGRAM_SEND            = IOCTL_VMCI_DATAGRAM_SEND,
   VMCI_SO_DATAGRAM_RECEIVE         = IOCTL_VMCI_DATAGRAM_RECEIVE, 
   VMCI_SO_DATAGRAM_REQUEST_MAP     = IOCTL_VMCI_DATAGRAM_REQUEST_MAP,
   VMCI_SO_DATAGRAM_REMOVE_MAP      = IOCTL_VMCI_DATAGRAM_REMOVE_MAP, 
   VMCI_SO_CTX_ADD_NOTIFICATION     = IOCTL_VMCI_CTX_ADD_NOTIFICATION, 
   VMCI_SO_CTX_REMOVE_NOTIFICATION  = IOCTL_VMCI_CTX_REMOVE_NOTIFICATION, 
   VMCI_SO_CTX_GET_CPT_STATE        = IOCTL_VMCI_CTX_GET_CPT_STATE, 
   VMCI_SO_CTX_SET_CPT_STATE        = IOCTL_VMCI_CTX_SET_CPT_STATE, 
   VMCI_SO_GET_CONTEXT_ID           = IOCTL_VMCI_GET_CONTEXT_ID,
   VMCI_SO_USERFD,
};

#  define VMCI_MACOS_HOST_DEVICE_BASE    "com.vmware.kext.vmci"
#  ifdef VMX86_DEVEL
#     define VMCI_MACOS_HOST_DEVICE VMCI_MACOS_HOST_DEVICE_BASE ".devel"
#  else
#     define VMCI_MACOS_HOST_DEVICE VMCI_MACOS_HOST_DEVICE_BASE
#  endif

#endif

/* Clean up helper macros */
#undef IOCTLCMD

#endif // ifndef _VMCI_IOCONTROLS_H_
