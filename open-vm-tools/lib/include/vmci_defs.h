/*********************************************************
 * Copyright (C) 2005-2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "vm_basic_defs.h"
#include "vm_atomic.h"
#include "vm_assert.h"

#if defined __cplusplus
extern "C" {
#endif

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
#define VMCI_CAPS_HYPERCALL     0x1
#define VMCI_CAPS_GUESTCALL     0x2
#define VMCI_CAPS_DATAGRAM      0x4
#define VMCI_CAPS_NOTIFICATIONS 0x8
#define VMCI_CAPS_PPN64         0x10
#define VMCI_CAPS_CLEAR_TO_ACK  (0x1 << 31)

#define VMCI_CAPS_NOT_ACKED (VMCI_CAPS_HYPERCALL | VMCI_CAPS_GUESTCALL | \
                             VMCI_CAPS_DATAGRAM | VMCI_CAPS_NOTIFICATIONS)

/* Interrupt Cause register bits. */
#define VMCI_ICR_DATAGRAM      0x1
#define VMCI_ICR_NOTIFICATION  0x2

/* Interrupt Mask register bits. */
#define VMCI_IMR_DATAGRAM      0x1
#define VMCI_IMR_NOTIFICATION  0x2

/* Interrupt type. */
typedef enum VMCIIntrType {
   VMCI_INTR_TYPE_INTX = 0,
   VMCI_INTR_TYPE_MSI =  1,
   VMCI_INTR_TYPE_MSIX = 2
} VMCIIntrType;

/*
 * Maximum MSI/MSI-X interrupt vectors in the device.
 */
#define VMCI_MAX_INTRS 2

/*
 * Supported interrupt vectors.  There is one for each ICR value above,
 * but here they indicate the position in the vector array/message ID.
 */
#define VMCI_INTR_DATAGRAM     0
#define VMCI_INTR_NOTIFICATION 1


/*
 * A single VMCI device has an upper limit of 128 MiB on the amount of
 * memory that can be used for queue pairs. Since each queue pair
 * consists of at least two pages, the memory limit also dictates the
 * number of queue pairs a guest can create.
 */
#define VMCI_MAX_GUEST_QP_MEMORY (128 * 1024 * 1024)
#define VMCI_MAX_GUEST_QP_COUNT  (VMCI_MAX_GUEST_QP_MEMORY / PAGE_SIZE / 2)

/*
 * There can be at most PAGE_SIZE doorbells since there is one doorbell
 * per byte in the doorbell bitmap page.
 */
#define VMCI_MAX_GUEST_DOORBELL_COUNT PAGE_SIZE

/*
 * We have a fixed set of resource IDs available in the VMX.
 * This allows us to have a very simple implementation since we statically
 * know how many will create datagram handles. If a new caller arrives and
 * we have run out of slots we can manually increment the maximum size of
 * available resource IDs.
 */

typedef uint32 VMCI_Resource;

/* VMCI reserved hypervisor datagram resource IDs. */
#define VMCI_RESOURCES_QUERY        0
#define VMCI_GET_CONTEXT_ID         1
#define VMCI_SET_NOTIFY_BITMAP      2
#define VMCI_DOORBELL_LINK          3
#define VMCI_DOORBELL_UNLINK        4
#define VMCI_DOORBELL_NOTIFY        5
/*
 * VMCI_DATAGRAM_REQUEST_MAP and VMCI_DATAGRAM_REMOVE_MAP are
 * obsoleted by the removal of VM to VM communication.
 */
#define VMCI_DATAGRAM_REQUEST_MAP   6
#define VMCI_DATAGRAM_REMOVE_MAP    7
#define VMCI_EVENT_SUBSCRIBE        8
#define VMCI_EVENT_UNSUBSCRIBE      9
#define VMCI_QUEUEPAIR_ALLOC        10
#define VMCI_QUEUEPAIR_DETACH       11
/*
 * VMCI_VSOCK_VMX_LOOKUP was assigned to 12 for Fusion 3.0/3.1,
 * WS 7.0/7.1 and ESX 4.1
 */
#define VMCI_HGFS_TRANSPORT         13
#define VMCI_UNITY_PBRPC_REGISTER   14
/*
 * This resource is used for VMCI socket control packets sent to the
 * hypervisor (CID 0) because RID 1 is already reserved.
 */
#define VSOCK_PACKET_HYPERVISOR_RID 15
#define VMCI_RESOURCE_MAX           16
/*
 * The core VMCI device functionality only requires the resource IDs of
 * VMCI_QUEUEPAIR_DETACH and below.
 */
#define VMCI_CORE_DEVICE_RESOURCE_MAX  VMCI_QUEUEPAIR_DETACH

/*
 * VMCI reserved host datagram resource IDs.
 * vsock control channel has resource id 1.
 */
#define VMCI_DVFILTER_DATA_PATH_DATAGRAM 2

/* VMCI Ids. */
typedef uint32 VMCIId;

typedef struct VMCIIdRange {
   int8 action;   // VMCI_FA_X, for use in filters.
   VMCIId begin;  // Beginning of range
   VMCIId end;    // End of range
} VMCIIdRange;

typedef struct VMCIHandle {
   VMCIId context;
   VMCIId resource;
} VMCIHandle;

static INLINE VMCIHandle
VMCI_MAKE_HANDLE(VMCIId cid,  // IN:
                 VMCIId rid)  // IN:
{
   VMCIHandle h;
   h.context = cid;
   h.resource = rid;
   return h;
}

/*
 *----------------------------------------------------------------------
 *
 * VMCI_HANDLE_TO_UINT64 --
 *
 *     Helper for VMCI handle to uint64 conversion.
 *
 * Results:
 *     The uint64 value.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

static INLINE uint64
VMCI_HANDLE_TO_UINT64(VMCIHandle handle) // IN:
{
   uint64 handle64;

   handle64 = handle.context;
   handle64 <<= 32;
   handle64 |= handle.resource;
   return handle64;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_UINT64_TO_HANDLE --
 *
 *     Helper for uint64 to VMCI handle conversion.
 *
 * Results:
 *     The VMCI handle value.
 *
 * Side effects:
 *     None.
 *
 *----------------------------------------------------------------------
 */

static INLINE VMCIHandle
VMCI_UINT64_TO_HANDLE(uint64 handle64) // IN:
{
   VMCIId context = (VMCIId)(handle64 >> 32);
   VMCIId resource = (VMCIId)handle64;

   return VMCI_MAKE_HANDLE(context, resource);
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
 * Well-known context id, a logical context that contains a set of
 * well-known services. This context ID is now obsolete.
 */
#define VMCI_WELL_KNOWN_CONTEXT_ID 1

/*
 * Context ID used by host endpoints.
 */
#define VMCI_HOST_CONTEXT_ID  2
#define VMCI_HOST_CONTEXT_INVALID_EVENT         ((uintptr_t)~0)

#define VMCI_CONTEXT_IS_VM(_cid) (VMCI_INVALID_ID != _cid && \
                                  _cid > VMCI_HOST_CONTEXT_ID)

/*
 * The VMCI_CONTEXT_RESOURCE_ID is used together with VMCI_MAKE_HANDLE to make
 * handles that refer to a specific context.
 */
#define VMCI_CONTEXT_RESOURCE_ID 0


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI error codes.
 *
 *-----------------------------------------------------------------------------
 */

#define VMCI_SUCCESS_QUEUEPAIR_ATTACH     5
#define VMCI_SUCCESS_QUEUEPAIR_CREATE     4
#define VMCI_SUCCESS_LAST_DETACH          3
#define VMCI_SUCCESS_ACCESS_GRANTED       2
#define VMCI_SUCCESS_ENTRY_DEAD           1
#define VMCI_SUCCESS                      0LL
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
#define VMCI_ERROR_DEVICE_NOT_FOUND      (-39)
#define VMCI_ERROR_QUEUEPAIR_NOT_READY   (-40)
#define VMCI_ERROR_WOULD_BLOCK           (-41)

/* VMCI clients should return error code withing this range */
#define VMCI_ERROR_CLIENT_MIN     (-500)
#define VMCI_ERROR_CLIENT_MAX     (-550)

/* Internal error codes. */
#define VMCI_SHAREDMEM_ERROR_BAD_CONTEXT (-1000)

#define VMCI_PATH_MAX 256

/* VMCI reserved events. */
typedef uint32 VMCI_Event;

#define VMCI_EVENT_CTX_ID_UPDATE  0  // Only applicable to guest endpoints
#define VMCI_EVENT_CTX_REMOVED    1  // Applicable to guest and host
#define VMCI_EVENT_QP_RESUMED     2  // Only applicable to guest endpoints
#define VMCI_EVENT_QP_PEER_ATTACH 3  // Applicable to guest, host and VMX
#define VMCI_EVENT_QP_PEER_DETACH 4  // Applicable to guest, host and VMX
#define VMCI_EVENT_MEM_ACCESS_ON  5  // Applicable to VMX and vmk.  On vmk,
                                     // this event has the Context payload type.
#define VMCI_EVENT_MEM_ACCESS_OFF 6  // Applicable to VMX and vmk.  Same as
                                     // above for the payload type.
#define VMCI_EVENT_GUEST_PAUSED   7  // Applicable to vmk. This event has the
                                     // Context payload type.
#define VMCI_EVENT_GUEST_UNPAUSED 8  // Applicable to vmk. Same as above for
                                     // the payload type.
#define VMCI_EVENT_MAX            9

/*
 * Of the above events, a few are reserved for use in the VMX, and
 * other endpoints (guest and host kernel) should not use them. For
 * the rest of the events, we allow both host and guest endpoints to
 * subscribe to them, to maintain the same API for host and guest
 * endpoints.
 */

#define VMCI_EVENT_VALID_VMX(_event) (_event == VMCI_EVENT_QP_PEER_ATTACH || \
                                      _event == VMCI_EVENT_QP_PEER_DETACH || \
                                      _event == VMCI_EVENT_MEM_ACCESS_ON || \
                                      _event == VMCI_EVENT_MEM_ACCESS_OFF)

#if defined(VMX86_SERVER)
#define VMCI_EVENT_VALID(_event) (_event < VMCI_EVENT_MAX)
#else // VMX86_SERVER
#define VMCI_EVENT_VALID(_event) (_event < VMCI_EVENT_MAX && \
                                  _event != VMCI_EVENT_MEM_ACCESS_ON && \
                                  _event != VMCI_EVENT_MEM_ACCESS_OFF && \
                                  _event != VMCI_EVENT_GUEST_PAUSED && \
                                  _event != VMCI_EVENT_GUEST_UNPAUSED)
#endif // VMX86_SERVER

/* Reserved guest datagram resource ids. */
#define VMCI_EVENT_HANDLER 0

/* VMCI privileges. */
typedef enum VMCIResourcePrivilegeType {
   VMCI_PRIV_CH_PRIV,
   VMCI_PRIV_DESTROY_RESOURCE,
   VMCI_PRIV_ASSIGN_CLIENT,
   VMCI_PRIV_DG_CREATE,
   VMCI_PRIV_DG_SEND,
   VMCI_PRIV_NOTIFY,
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

#define VMCI_PUBLIC_GROUP_NAME "vmci public group"
/* 0 through VMCI_RESERVED_RESOURCE_ID_MAX are reserved. */
#define VMCI_RESERVED_RESOURCE_ID_MAX 1023

#define VMCI_DOMAIN_NAME_MAXLEN  32

#define VMCI_LGPFX "VMCI: "
#define VMCI_DRIVER_NAME "vmci"


/*
 * VMCIQueueHeader
 *
 * A Queue cannot stand by itself as designed.  Each Queue's header
 * contains a pointer into itself (the producerTail) and into its peer
 * (consumerHead).  The reason for the separation is one of
 * accessibility: Each end-point can modify two things: where the next
 * location to enqueue is within its produceQ (producerTail); and
 * where the next dequeue location is in its consumeQ (consumerHead).
 *
 * An end-point cannot modify the pointers of its peer (guest to
 * guest; NOTE that in the host both queue headers are mapped r/w).
 * But, each end-point needs read access to both Queue header
 * structures in order to determine how much space is used (or left)
 * in the Queue.  This is because for an end-point to know how full
 * its produceQ is, it needs to use the consumerHead that points into
 * the produceQ but -that- consumerHead is in the Queue header for
 * that end-points consumeQ.
 *
 * Thoroughly confused?  Sorry.
 *
 * producerTail: the point to enqueue new entrants.  When you approach
 * a line in a store, for example, you walk up to the tail.
 *
 * consumerHead: the point in the queue from which the next element is
 * dequeued.  In other words, who is next in line is he who is at the
 * head of the line.
 *
 * Also, producerTail points to an empty byte in the Queue, whereas
 * consumerHead points to a valid byte of data (unless producerTail ==
 * consumerHead in which case consumerHead does not point to a valid
 * byte of data).
 *
 * For a queue of buffer 'size' bytes, the tail and head pointers will be in
 * the range [0, size-1].
 *
 * If produceQHeader->producerTail == consumeQHeader->consumerHead
 * then the produceQ is empty.
 */

typedef struct VMCIQueueHeader {
   /* All fields are 64bit and aligned. */
   VMCIHandle    handle;       /* Identifier. */
   Atomic_uint64 producerTail; /* Offset in this queue. */
   Atomic_uint64 consumerHead; /* Offset in peer queue. */
} VMCIQueueHeader;


/*
 * If one client of a QueuePair is a 32bit entity, we restrict the QueuePair
 * size to be less than 4GB, and use 32bit atomic operations on the head and
 * tail pointers. 64bit atomic read on a 32bit entity involves cmpxchg8b which
 * is an atomic read-modify-write. This will cause traces to fire when a 32bit
 * consumer tries to read the producer's tail pointer, for example, because the
 * consumer has read-only access to the producer's tail pointer.
 *
 * We provide the following macros to invoke 32bit or 64bit atomic operations
 * based on the architecture the code is being compiled on.
 */

/* Architecture independent maximum queue size. */
#define QP_MAX_QUEUE_SIZE_ARCH_ANY   CONST64U(0xffffffff)

#ifdef __x86_64__
#  define QP_MAX_QUEUE_SIZE_ARCH     CONST64U(0xffffffffffffffff)
#  define QPAtomic_ReadOffset(x)     Atomic_Read64(x)
#  define QPAtomic_WriteOffset(x, y) Atomic_Write64(x, y)
#else
   /*
    * Wrappers below are being used to call Atomic_Read32 because of the
    * 'type punned' compilation warning received when Atomic_Read32 is
    * called with a Atomic_uint64 pointer typecasted to Atomic_uint32
    * pointer from QPAtomic_ReadOffset. Ditto with QPAtomic_WriteOffset.
    */

   static INLINE uint32
   TypeSafe_Atomic_Read32(void *var) // IN:
   {
      return Atomic_Read32((Atomic_uint32 *)(var));
   }

   static INLINE void
   TypeSafe_Atomic_Write32(void *var, uint32 val) // IN:
   {
      Atomic_Write32((Atomic_uint32 *)(var), (uint32)(val));
   }

#  define QP_MAX_QUEUE_SIZE_ARCH  CONST64U(0xffffffff)
#  define QPAtomic_ReadOffset(x)  TypeSafe_Atomic_Read32((void *)(x))
#  define QPAtomic_WriteOffset(x, y) \
          TypeSafe_Atomic_Write32((void *)(x), (uint32)(y))
#endif	/* __x86_64__  */


static INLINE PPN32
VMCI_PPN64_TO_PPN32(PPN ppn)
{
   ASSERT(ppn <= MAX_UINT32);
   return (PPN32)ppn;
}

/*
 *-----------------------------------------------------------------------------
 *
 * QPAddPointer --
 *
 *      Helper to add a given offset to a head or tail pointer. Wraps the value
 *      of the pointer around the max size of the queue.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
QPAddPointer(Atomic_uint64 *var, // IN:
             size_t add,         // IN:
             uint64 size)        // IN:
{
   uint64 newVal = QPAtomic_ReadOffset(var);

   if (newVal >= size - add) {
      newVal -= size;
   }
   newVal += add;

   QPAtomic_WriteOffset(var, newVal);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_ProducerTail() --
 *
 *      Helper routine to get the Producer Tail from the supplied queue.
 *
 * Results:
 *      The contents of the queue's producer tail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
VMCIQueueHeader_ProducerTail(const VMCIQueueHeader *qHeader) // IN:
{
   VMCIQueueHeader *qh = (VMCIQueueHeader *)qHeader;
   return QPAtomic_ReadOffset(&qh->producerTail);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_ConsumerHead() --
 *
 *      Helper routine to get the Consumer Head from the supplied queue.
 *
 * Results:
 *      The contents of the queue's consumer tail.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE uint64
VMCIQueueHeader_ConsumerHead(const VMCIQueueHeader *qHeader) // IN:
{
   VMCIQueueHeader *qh = (VMCIQueueHeader *)qHeader;
   return QPAtomic_ReadOffset(&qh->consumerHead);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_AddProducerTail() --
 *
 *      Helper routine to increment the Producer Tail.  Fundamentally,
 *      QPAddPointer() is used to manipulate the tail itself.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQueueHeader_AddProducerTail(VMCIQueueHeader *qHeader, // IN/OUT:
                                size_t add,               // IN:
                                uint64 queueSize)         // IN:
{
   QPAddPointer(&qHeader->producerTail, add, queueSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_AddConsumerHead() --
 *
 *      Helper routine to increment the Consumer Head.  Fundamentally,
 *      QPAddPointer() is used to manipulate the head itself.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQueueHeader_AddConsumerHead(VMCIQueueHeader *qHeader, // IN/OUT:
                                size_t add,               // IN:
                                uint64 queueSize)         // IN:
{
   QPAddPointer(&qHeader->consumerHead, add, queueSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_CheckAlignment --
 *
 *      Checks if the given queue is aligned to page boundary.  Returns TRUE if
 *      the alignment is good.
 *
 * Results:
 *      TRUE or FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
VMCIQueueHeader_CheckAlignment(const VMCIQueueHeader *qHeader) // IN:
{
   uintptr_t hdr, offset;

   hdr = (uintptr_t) qHeader;
   offset = hdr & (PAGE_SIZE -1);

   return offset == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_GetPointers --
 *
 *      Helper routine for getting the head and the tail pointer for a queue.
 *      Both the VMCIQueues are needed to get both the pointers for one queue.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQueueHeader_GetPointers(const VMCIQueueHeader *produceQHeader, // IN:
                            const VMCIQueueHeader *consumeQHeader, // IN:
                            uint64 *producerTail,                  // OUT:
                            uint64 *consumerHead)                  // OUT:
{
   if (producerTail) {
      *producerTail = VMCIQueueHeader_ProducerTail(produceQHeader);
   }

   if (consumerHead) {
      *consumerHead = VMCIQueueHeader_ConsumerHead(consumeQHeader);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_ResetPointers --
 *
 *      Reset the tail pointer (of "this" queue) and the head pointer (of
 *      "peer" queue).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQueueHeader_ResetPointers(VMCIQueueHeader *qHeader) // IN/OUT:
{
   QPAtomic_WriteOffset(&qHeader->producerTail, CONST64U(0));
   QPAtomic_WriteOffset(&qHeader->consumerHead, CONST64U(0));
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_Init --
 *
 *      Initializes a queue's state (head & tail pointers).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQueueHeader_Init(VMCIQueueHeader *qHeader, // IN/OUT:
                     const VMCIHandle handle)  // IN:
{
   qHeader->handle = handle;
   VMCIQueueHeader_ResetPointers(qHeader);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_FreeSpace --
 *
 *      Finds available free space in a produce queue to enqueue more
 *      data or reports an error if queue pair corruption is detected.
 *
 * Results:
 *      Free space size in bytes or an error code.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int64
VMCIQueueHeader_FreeSpace(const VMCIQueueHeader *produceQHeader, // IN:
                          const VMCIQueueHeader *consumeQHeader, // IN:
                          const uint64 produceQSize)             // IN:
{
   uint64 tail;
   uint64 head;
   uint64 freeSpace;

   tail = VMCIQueueHeader_ProducerTail(produceQHeader);
   head = VMCIQueueHeader_ConsumerHead(consumeQHeader);

   if (tail >= produceQSize || head >= produceQSize) {
      return VMCI_ERROR_INVALID_SIZE;
   }

   /*
    * Deduct 1 to avoid tail becoming equal to head which causes ambiguity. If
    * head and tail are equal it means that the queue is empty.
    */

   if (tail >= head) {
      freeSpace = produceQSize - (tail - head) - 1;
   } else {
      freeSpace = head - tail - 1;
   }

   return freeSpace;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueHeader_BufReady --
 *
 *      VMCIQueueHeader_FreeSpace() does all the heavy lifting of
 *      determing the number of free bytes in a Queue.  This routine,
 *      then subtracts that size from the full size of the Queue so
 *      the caller knows how many bytes are ready to be dequeued.
 *
 * Results:
 *      On success, available data size in bytes (up to MAX_INT64).
 *      On failure, appropriate error code.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int64
VMCIQueueHeader_BufReady(const VMCIQueueHeader *consumeQHeader, // IN:
                         const VMCIQueueHeader *produceQHeader, // IN:
                         const uint64 consumeQSize)             // IN:
{
   int64 freeSpace;

   freeSpace = VMCIQueueHeader_FreeSpace(consumeQHeader,
                                         produceQHeader,
                                         consumeQSize);
   if (freeSpace < VMCI_SUCCESS) {
      return freeSpace;
   } else {
      return consumeQSize - freeSpace - 1;
   }
}


/*
 * Defines for the VMCI traffic filter:
 * - VMCI_FA_<name> defines the filter action values
 * - VMCI_FP_<name> defines the filter protocol values
 * - VMCI_FD_<name> defines the direction values (guest or host)
 * - VMCI_FT_<name> are the type values (allow or deny)
 */

#define VMCI_FA_INVALID -1
#define VMCI_FA_ALLOW    0
#define VMCI_FA_DENY     (VMCI_FA_ALLOW + 1)
#define VMCI_FA_MAX      (VMCI_FA_DENY + 1)

#define VMCI_FP_INVALID     -1
#define VMCI_FP_HYPERVISOR   0
#define VMCI_FP_QUEUEPAIR    (VMCI_FP_HYPERVISOR + 1)
#define VMCI_FP_DOORBELL     (VMCI_FP_QUEUEPAIR + 1)
#define VMCI_FP_DATAGRAM     (VMCI_FP_DOORBELL + 1)
#define VMCI_FP_STREAMSOCK   (VMCI_FP_DATAGRAM + 1)
#define VMCI_FP_ANY          (VMCI_FP_STREAMSOCK + 1)
#define VMCI_FP_MAX          (VMCI_FP_ANY + 1)

#define VMCI_FD_INVALID  -1
#define VMCI_FD_GUEST     0
#define VMCI_FD_HOST      (VMCI_FD_GUEST + 1)
#define VMCI_FD_ANY       (VMCI_FD_HOST + 1)
#define VMCI_FD_MAX       (VMCI_FD_ANY + 1)

/*
 * The filter list tracks VMCI Id ranges for a given filter.
 */

typedef struct {
   uint32 len;
   VMCIIdRange *list;
} VMCIFilterList;


/*
 * The filter info is used to communicate the filter configuration
 * from the VMX to the host kernel.
 */

typedef struct {
   VA64   list;   // List of VMCIIdRange
   uint32 len;    // Length of list
   uint8  dir;    // VMCI_FD_X
   uint8  proto;  // VMCI_FP_X
} VMCIFilterInfo;

/*
 * In the host kernel, the ingoing and outgoing filters are
 * separated. The VMCIProtoFilters type captures all filters in one
 * direction. The VMCIFilters type captures all filters.
 */

typedef VMCIFilterList VMCIProtoFilters[VMCI_FP_MAX];
typedef VMCIProtoFilters VMCIFilters[VMCI_FD_MAX];

#if defined __cplusplus
} // extern "C"
#endif

#endif // _VMCI_DEF_H_
