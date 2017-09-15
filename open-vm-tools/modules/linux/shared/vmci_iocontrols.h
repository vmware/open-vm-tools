/*********************************************************
 * Copyright (C) 2007-2014 VMware, Inc. All rights reserved.
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
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_assert.h"
#include "vmci_defs.h"

#if defined(_WIN32) && defined(WINNT_DDK)
/* We need to expose the API through an IOCTL on Windows.  Use latest API. */
#include "vmciKernelAPI.h"
#endif // _WIN32 && WINNT_DDK

#if defined __cplusplus
extern "C" {
#endif


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
#ifdef VM_64BIT
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
 * VMCI_VERSION_NOVMVM: This version removed support for VM to VM
 * communication.
 *
 * VMCI_VERSION_NOTIFY: This version introduced doorbell notification
 * support.
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

#define VMCI_VERSION                VMCI_VERSION_NOVMVM
#define VMCI_VERSION_NOVMVM         VMCI_MAKE_VERSION(11, 0)
#define VMCI_VERSION_NOTIFY         VMCI_MAKE_VERSION(10, 0)
#define VMCI_VERSION_HOSTQP         VMCI_MAKE_VERSION(9, 0)
#define VMCI_VERSION_PREHOSTQP      VMCI_MAKE_VERSION(8, 0)
#define VMCI_VERSION_PREVERS2       VMCI_MAKE_VERSION(1, 0)

/*
 * VMCISockets driver version.  The version is platform-dependent and is
 * embedded in vsock_version.h for each platform.  It can be obtained via
 * VMCISock_Version() (which uses IOCTL_VMCI_SOCKETS_VERSION).  The
 * following is simply for constructing an unsigned integer value from the
 * comma-separated version in the header.  This must match the macros defined
 * in vmci_sockets.h.  An example of using this is:
 * uint16 parts[4] = { VSOCK_DRIVER_VERSION_COMMAS };
 * uint32 version = VMCI_SOCKETS_MAKE_VERSION(parts);
 */

#define VMCI_SOCKETS_MAKE_VERSION(_p) \
   ((((_p)[0] & 0xFF) << 24) | (((_p)[1] & 0xFF) << 16) | ((_p)[2]))

#if defined(__linux__) || defined(VMKERNEL)
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
#elif defined (__APPLE__)
#include <sys/ioccom.h>
#define IOCTLCMD(_cmd) IOCTL_VMCI_ ## _cmd
#define IOCTLCMD_I(_cmd, _type) \
   IOCTL_VMCI_MACOS_ ## _cmd = _IOW('V', IOCTL_VMCI_ ## _cmd, _type)
#define IOCTLCMD_O(_cmd, _type) \
   IOCTL_VMCI_MACOS_ ## _cmd = _IOR('V', IOCTL_VMCI_ ## _cmd, _type)
#define IOCTLCMD_IO(_cmd, _type) \
   IOCTL_VMCI_MACOS_ ## _cmd = _IOWR('V', IOCTL_VMCI_ ## _cmd, _type)
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

   /*
    * The following two were used for process and datagram process creation.
    * They are not used anymore and reserved for future use.
    * They will fail if issued.
    */
   IOCTLCMD(RESERVED1),
   IOCTLCMD(RESERVED2),

   /*
    * The following used to be for shared memory. It is now unused and and is
    * reserved for future use. It will fail if issued.
    */
   IOCTLCMD(RESERVED3),

   /*
    * The follwoing three were also used to be for shared memory. An
    * old WS6 user-mode client might try to use them with the new
    * driver, but since we ensure that only contexts created by VMX'en
    * of the appropriate version (VMCI_VERSION_NOTIFY or
    * VMCI_VERSION_NEWQP) or higher use these ioctl, everything is
    * fine.
    */
   IOCTLCMD(QUEUEPAIR_SETVA),
   IOCTLCMD(NOTIFY_RESOURCE),
   IOCTLCMD(NOTIFICATIONS_RECEIVE),
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

   /*
    * This used to be for accept() on Windows and Mac OS, which is now
    * redundant (since we now use real handles).  It is used instead for
    * getting the version.  This value is now public, so it cannot change.
    */
   IOCTLCMD(SOCKETS_VERSION) = IOCTLCMD(SOCKETS_FIRST),
   IOCTLCMD(SOCKETS_BIND),

   /*
    * This used to be for close() on Windows and Mac OS, but is no longer
    * used for the same reason as accept() above.  It is used instead for
    * sending private symbols to the Mac OS driver.
    */
   IOCTLCMD(SOCKETS_SET_SYMBOLS),
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
   IOCTLCMD(SOCKETS_SOCKET),
   IOCTLCMD(SOCKETS_UUID_2_CID), /* 1991 on Linux. */
   /* END VMCI SOCKETS */

   /*
    * We reserve a range of 3 ioctls for VMCI Sockets to grow.  We cannot
    * reserve many ioctls here since we are close to overlapping with vmmon
    * ioctls.  Define a meta-ioctl if running out of this binary space.
    */
   // Must be last.
   IOCTLCMD(SOCKETS_LAST) = IOCTLCMD(SOCKETS_UUID_2_CID) + 3, /* 1994 on Linux. */
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

#if defined (__APPLE__)
/*
 * The size of this must match the size of VSockIoctlPrivSyms in
 * modules/vsock/common/vsockIoctl.h.
 */
#pragma pack(push, 1)
struct IOCTLCmd_VMCIMacOS_PrivSyms {
   char data[344];
};
#pragma pack(pop)

enum IOCTLCmd_VMCIMacOS {
   IOCTLCMD_I(SOCKETS_SET_SYMBOLS, struct IOCTLCmd_VMCIMacOS_PrivSyms),
   IOCTLCMD_O(SOCKETS_VERSION, unsigned int),
   IOCTLCMD_O(SOCKETS_GET_AF_VALUE, int),
   IOCTLCMD_O(SOCKETS_GET_LOCAL_CID, unsigned int),
};
#endif // __APPLE__


#if defined _WIN32
/*
 * Windows VMCI ioctl definitions.
 */

/* PUBLIC: For VMCISockets user-mode clients that use CreateFile(). */
#define VMCI_INTERFACE_VSOCK_PUBLIC_NAME TEXT("\\\\.\\VMCI")

/* PUBLIC: For VMCISockets user-mode clients that use NtCreateFile(). */
#define VMCI_INTERFACE_VSOCK_PUBLIC_NAME_NT L"\\??\\VMCI"

/* PUBLIC: For the VMX, which uses CreateFile(). */
#define VMCI_INTERFACE_VMX_PUBLIC_NAME TEXT("\\\\.\\VMCIDev\\VMX")

/* PRIVATE NAMES */
#define VMCI_DEVICE_VMCI_LINK_PATH  L"\\DosDevices\\VMCIDev"
#define VMCI_DEVICE_VSOCK_LINK_PATH L"\\DosDevices\\vmci"
#define VMCI_DEVICE_HOST_NAME_PATH  L"\\Device\\VMCIHostDev"
#define VMCI_DEVICE_GUEST_NAME_PATH L"\\Device\\VMCIGuestDev"
/* PRIVATE NAMES */

/* These values cannot be changed since some of the ioctl values are public. */
#define FILE_DEVICE_VMCI      0x8103
#define VMCI_IOCTL_BASE_INDEX 0x801
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

enum IOCTLCmd_VMCIWin32 {
   IOCTLCMD(DEVICE_GET) = IOCTLCMD(LAST2) + 1,
   IOCTLCMD(SOCKETS_SERVICE_GET),
   IOCTLCMD(SOCKETS_STOP),
};

#define IOCTL_VMCI_VERSION VMCIIOCTL_BUFFERED(VERSION)

/* BEGIN VMCI */
#define IOCTL_VMCI_INIT_CONTEXT \
               VMCIIOCTL_BUFFERED(INIT_CONTEXT)
#define IOCTL_VMCI_HYPERCALL \
               VMCIIOCTL_BUFFERED(HYPERCALL)
#define IOCTL_VMCI_CREATE_DATAGRAM_HANDLE  \
               VMCIIOCTL_BUFFERED(CREATE_DATAGRAM_HANDLE)
#define IOCTL_VMCI_DESTROY_DATAGRAM_HANDLE  \
               VMCIIOCTL_BUFFERED(DESTROY_DATAGRAM_HANDLE)
#define IOCTL_VMCI_NOTIFY_RESOURCE    \
               VMCIIOCTL_BUFFERED(NOTIFY_RESOURCE)
#define IOCTL_VMCI_NOTIFICATIONS_RECEIVE    \
               VMCIIOCTL_BUFFERED(NOTIFICATIONS_RECEIVE)
#define IOCTL_VMCI_VERSION2 \
               VMCIIOCTL_BUFFERED(VERSION2)
#define IOCTL_VMCI_QUEUEPAIR_ALLOC  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_ALLOC)
#define IOCTL_VMCI_QUEUEPAIR_SETVA  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_SETVA)
#define IOCTL_VMCI_QUEUEPAIR_SETPAGEFILE  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_SETPAGEFILE)
#define IOCTL_VMCI_QUEUEPAIR_DETACH  \
               VMCIIOCTL_BUFFERED(QUEUEPAIR_DETACH)
#define IOCTL_VMCI_DATAGRAM_SEND \
               VMCIIOCTL_BUFFERED(DATAGRAM_SEND)
#define IOCTL_VMCI_DATAGRAM_RECEIVE \
               VMCIIOCTL_NEITHER(DATAGRAM_RECEIVE)
#define IOCTL_VMCI_DATAGRAM_REQUEST_MAP \
               VMCIIOCTL_BUFFERED(DATAGRAM_REQUEST_MAP)
#define IOCTL_VMCI_DATAGRAM_REMOVE_MAP \
               VMCIIOCTL_BUFFERED(DATAGRAM_REMOVE_MAP)
#define IOCTL_VMCI_CTX_ADD_NOTIFICATION \
               VMCIIOCTL_BUFFERED(CTX_ADD_NOTIFICATION)
#define IOCTL_VMCI_CTX_REMOVE_NOTIFICATION \
               VMCIIOCTL_BUFFERED(CTX_REMOVE_NOTIFICATION)
#define IOCTL_VMCI_CTX_GET_CPT_STATE \
               VMCIIOCTL_BUFFERED(CTX_GET_CPT_STATE)
#define IOCTL_VMCI_CTX_SET_CPT_STATE \
               VMCIIOCTL_BUFFERED(CTX_SET_CPT_STATE)
#define IOCTL_VMCI_GET_CONTEXT_ID    \
               VMCIIOCTL_BUFFERED(GET_CONTEXT_ID)
#define IOCTL_VMCI_DEVICE_GET \
               VMCIIOCTL_BUFFERED(DEVICE_GET)
/* END VMCI */

/* BEGIN VMCI SOCKETS */
#define IOCTL_VMCI_SOCKETS_VERSION \
               VMCIIOCTL_BUFFERED(SOCKETS_VERSION)
#define IOCTL_VMCI_SOCKETS_BIND \
               VMCIIOCTL_BUFFERED(SOCKETS_BIND)
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
#define IOCTL_VMCI_SOCKETS_RECV_FROM \
               VMCIIOCTL_BUFFERED(SOCKETS_RECV_FROM)
#define IOCTL_VMCI_SOCKETS_SELECT \
               VMCIIOCTL_BUFFERED(SOCKETS_SELECT)
#define IOCTL_VMCI_SOCKETS_SEND_TO \
               VMCIIOCTL_BUFFERED(SOCKETS_SEND_TO)
#define IOCTL_VMCI_SOCKETS_SET_SOCK_OPT \
               VMCIIOCTL_BUFFERED(SOCKETS_SET_SOCK_OPT)
#define IOCTL_VMCI_SOCKETS_SHUTDOWN \
               VMCIIOCTL_BUFFERED(SOCKETS_SHUTDOWN)
#define IOCTL_VMCI_SOCKETS_SERVICE_GET \
               VMCIIOCTL_BUFFERED(SOCKETS_SERVICE_GET)
#define IOCTL_VMCI_SOCKETS_STOP \
               VMCIIOCTL_NEITHER(SOCKETS_STOP)
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

typedef struct VMCIQueuePairAllocInfo_VMToVM {
   VMCIHandle handle;
   VMCIId     peer;
   uint32     flags;
   uint64     produceSize;
   uint64     consumeSize;
#if !defined(VMX86_SERVER) && !defined(VMKERNEL)
   VA64       producePageFile; /* User VA. */
   VA64       consumePageFile; /* User VA. */
   uint64     producePageFileSize; /* Size of the file name array. */
   uint64     consumePageFileSize; /* Size of the file name array. */
#else
   PPN *      PPNs;
   uint64     numPPNs;
#endif
   int32      result;
   uint32     _pad;
} VMCIQueuePairAllocInfo_VMToVM;

typedef struct VMCIQueuePairAllocInfo {
   VMCIHandle handle;
   VMCIId     peer;
   uint32     flags;
   uint64     produceSize;
   uint64     consumeSize;
#if !defined(VMX86_SERVER) && !defined(VMKERNEL)
   VA64       ppnVA; /* Start VA of queue pair PPNs. */
#else
   PPN *      PPNs;
#endif
   uint64     numPPNs;
   int32      result;
   uint32     version;
} VMCIQueuePairAllocInfo;

typedef struct VMCIQueuePairSetVAInfo {
   VMCIHandle handle;
   VA64       va; /* Start VA of queue pair PPNs. */
   uint64     numPPNs;
   uint32     version;
   int32      result;
} VMCIQueuePairSetVAInfo;

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
#if !defined(VMX86_SERVER) && !defined(VMKERNEL)
   VA64       producePageFile; /* User VA. */
   VA64       consumePageFile; /* User VA. */
   uint64     producePageFileSize; /* Size of the file name array. */
   uint64     consumePageFileSize; /* Size of the file name array. */
#endif
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

#define VMCI_NOTIFY_RESOURCE_QUEUE_PAIR 0
#define VMCI_NOTIFY_RESOURCE_DOOR_BELL  1

#define VMCI_NOTIFY_RESOURCE_ACTION_NOTIFY  0
#define VMCI_NOTIFY_RESOURCE_ACTION_CREATE  1
#define VMCI_NOTIFY_RESOURCE_ACTION_DESTROY 2

/*
 * Used to create and destroy doorbells, and generate a notification
 * for a doorbell or queue pair.
 */

typedef struct VMCINotifyResourceInfo {
   VMCIHandle  handle;
   uint16      resource;
   uint16      action;
   int32       result;
} VMCINotifyResourceInfo;

/*
 * Used to recieve pending notifications for doorbells and queue
 * pairs.
 */

typedef struct VMCINotificationReceiveInfo {
   VA64        dbHandleBufUVA;
   uint64      dbHandleBufSize;
   VA64        qpHandleBufUVA;
   uint64      qpHandleBufSize;
   int32       result;
   uint32      _pad;
} VMCINotificationReceiveInfo;

#if defined(_WIN32) && defined(WINNT_DDK)
/*
 * Used on Windows to expose the API calls that are no longer exported.  This
 * is kernel-mode only, and both sides will have the same bitness, so we can
 * use pointers directly.
 */

/* Version 1. */
typedef struct VMCIDeviceGetInfoVer1 {
   VMCI_DeviceReleaseFct *deviceRelease;
   VMCIDatagram_CreateHndFct *dgramCreateHnd;
   VMCIDatagram_CreateHndPrivFct *dgramCreateHndPriv;
   VMCIDatagram_DestroyHndFct *dgramDestroyHnd;
   VMCIDatagram_SendFct *dgramSend;
   VMCI_GetContextIDFct *getContextId;
   VMCI_VersionFct *version;
   VMCIEvent_SubscribeFct *eventSubscribe;
   VMCIEvent_UnsubscribeFct *eventUnsubscribe;
   VMCIQPair_AllocFct *qpairAlloc;
   VMCIQPair_DetachFct *qpairDetach;
   VMCIQPair_GetProduceIndexesFct *qpairGetProduceIndexes;
   VMCIQPair_GetConsumeIndexesFct *qpairGetConsumeIndexes;
   VMCIQPair_ProduceFreeSpaceFct *qpairProduceFreeSpace;
   VMCIQPair_ProduceBufReadyFct *qpairProduceBufReady;
   VMCIQPair_ConsumeFreeSpaceFct *qpairConsumeFreeSpace;
   VMCIQPair_ConsumeBufReadyFct *qpairConsumeBufReady;
   VMCIQPair_EnqueueFct *qpairEnqueue;
   VMCIQPair_DequeueFct *qpairDequeue;
   VMCIQPair_PeekFct *qpairPeek;
   VMCIQPair_EnqueueVFct *qpairEnqueueV;
   VMCIQPair_DequeueVFct *qpairDequeueV;
   VMCIQPair_PeekVFct *qpairPeekV;
   VMCI_ContextID2HostVmIDFct *contextID2HostVmID;
   VMCI_IsContextOwnerFct *isContextOwner;
   VMCIContext_GetPrivFlagsFct *contextGetPrivFlags;
} VMCIDeviceGetInfoVer1;

/* Version 2. */
typedef struct VMCIDeviceGetInfoVer2 {
   VMCIDoorbell_CreateFct *doorbellCreate;
   VMCIDoorbell_DestroyFct *doorbellDestroy;
   VMCIDoorbell_NotifyFct *doorbellNotify;
} VMCIDeviceGetInfoVer2;

typedef struct VMCIDeviceGetInfoHdr {
   /* Requested API version on input, supported version on output. */
   uint32 apiVersion;
   VMCI_DeviceShutdownFn *deviceShutdownCB;
   void *userData;
   void *deviceRegistration;
} VMCIDeviceGetInfoHdr;

/* Combination of all versions. */
typedef struct VMCIDeviceGetInfo {
   VMCIDeviceGetInfoHdr hdr;
   VMCIDeviceGetInfoVer1 ver1;
   VMCIDeviceGetInfoVer2 ver2;
} VMCIDeviceGetInfo;
#endif // _WIN32 && WINNT_DDK


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
   VMCI_SO_NOTIFY_RESOURCE          = IOCTL_VMCI_NOTIFY_RESOURCE,
   VMCI_SO_NOTIFICATIONS_RECEIVE    = IOCTL_VMCI_NOTIFICATIONS_RECEIVE,
   VMCI_SO_VERSION2                 = IOCTL_VMCI_VERSION2,
   VMCI_SO_QUEUEPAIR_ALLOC          = IOCTL_VMCI_QUEUEPAIR_ALLOC,
   VMCI_SO_QUEUEPAIR_SETVA          = IOCTL_VMCI_QUEUEPAIR_SETVA,
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

#define VMCI_MACOS_HOST_DEVICE "com.vmware.kext.vmci"

#endif

/* Clean up helper macros */
#undef IOCTLCMD


#if defined __cplusplus
} // extern "C"
#endif

#endif // ifndef _VMCI_IOCONTROLS_H_
