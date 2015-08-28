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
 * vmciQueuePair.c --
 *
 *    VMCI QueuePair API implementation in the host driver.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_handle_array.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDatagram.h"
#include "vmciDriver.h"
#include "vmciEvent.h"
#include "vmciHashtable.h"
#include "vmciKernelAPI.h"
#include "vmciQueuePair.h"
#include "vmciResource.h"
#include "vmciRoute.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#endif

#define LGPFX "VMCIQueuePair: "


/*
 * In the following, we will distinguish between two kinds of VMX processes -
 * the ones with versions lower than VMCI_VERSION_NOVMVM that use specialized
 * VMCI page files in the VMX and supporting VM to VM communication) and the
 * newer ones that use the guest memory directly. We will in the following refer
 * to the older VMX versions as old-style VMX'en, and the newer ones as new-style
 * VMX'en.
 *
 * The state transition datagram is as follows (the VMCIQPB_ prefix has been
 * removed for readability) - see below for more details on the transtions:
 *
 *            --------------  NEW  -------------
 *            |                                |
 *           \_/                              \_/
 *     CREATED_NO_MEM <-----------------> CREATED_MEM
 *            |    |                           |
 *            |    o-----------------------o   |
 *            |                            |   |
 *           \_/                          \_/ \_/
 *     ATTACHED_NO_MEM <----------------> ATTACHED_MEM
 *            |                            |   |
 *            |     o----------------------o   |
 *            |     |                          |
 *           \_/   \_/                        \_/
 *     SHUTDOWN_NO_MEM <----------------> SHUTDOWN_MEM
 *            |                                |
 *            |                                |
 *            -------------> gone <-------------
 *
 * In more detail. When a VMCI queue pair is first created, it will be in the
 * VMCIQPB_NEW state. It will then move into one of the following states:
 * - VMCIQPB_CREATED_NO_MEM: this state indicates that either:
 *     - the created was performed by a host endpoint, in which case there is no
 *       backing memory yet.
 *     - the create was initiated by an old-style VMX, that uses
 *       VMCIQPBroker_SetPageStore to specify the UVAs of the queue pair at a
 *       later point in time. This state can be distinguished from the one above
 *       by the context ID of the creator. A host side is not allowed to attach
 *       until the page store has been set.
 * - VMCIQPB_CREATED_MEM: this state is the result when the queue pair is created
 *     by a VMX using the queue pair device backend that sets the UVAs of the
 *     queue pair immediately and stores the information for later attachers. At
 *     this point, it is ready for the host side to attach to it.
 * Once the queue pair is in one of the created states (with the exception of the
 * case mentioned for older VMX'en above), it is possible to attach to the queue
 * pair. Again we have two new states possible:
 * - VMCIQPB_ATTACHED_MEM: this state can be reached through the following paths:
 *     - from VMCIQPB_CREATED_NO_MEM when a new-style VMX allocates a queue pair,
 *       and attaches to a queue pair previously created by the host side.
 *     - from VMCIQPB_CREATED_MEM when the host side attaches to a queue pair
 *       already created by a guest.
 *     - from VMCIQPB_ATTACHED_NO_MEM, when an old-style VMX calls
 *       VMCIQPBroker_SetPageStore (see below).
 * - VMCIQPB_ATTACHED_NO_MEM: If the queue pair already was in the
 *     VMCIQPB_CREATED_NO_MEM due to a host side create, an old-style VMX will
 *     bring the queue pair into this state. Once VMCIQPBroker_SetPageStore is
 *     called to register the user memory, the VMCIQPB_ATTACH_MEM state will be
 *     entered.
 * From the attached queue pair, the queue pair can enter the shutdown states
 * when either side of the queue pair detaches. If the guest side detaches first,
 * the queue pair will enter the VMCIQPB_SHUTDOWN_NO_MEM state, where the content
 * of the queue pair will no longer be available. If the host side detaches first,
 * the queue pair will either enter the VMCIQPB_SHUTDOWN_MEM, if the guest memory
 * is currently mapped, or VMCIQPB_SHUTDOWN_NO_MEM, if the guest memory is not
 * mapped (e.g., the host detaches while a guest is stunned).
 *
 * New-style VMX'en will also unmap guest memory, if the guest is quiesced, e.g.,
 * during a snapshot operation. In that case, the guest memory will no longer be
 * available, and the queue pair will transition from *_MEM state to a *_NO_MEM
 * state. The VMX may later map the memory once more, in which case the queue
 * pair will transition from the *_NO_MEM state at that point back to the *_MEM
 * state. Note that the *_NO_MEM state may have changed, since the peer may have
 * either attached or detached in the meantime. The values are laid out such that
 * ++ on a state will move from a *_NO_MEM to a *_MEM state, and vice versa.
 */

typedef enum {
   VMCIQPB_NEW,
   VMCIQPB_CREATED_NO_MEM,
   VMCIQPB_CREATED_MEM,
   VMCIQPB_ATTACHED_NO_MEM,
   VMCIQPB_ATTACHED_MEM,
   VMCIQPB_SHUTDOWN_NO_MEM,
   VMCIQPB_SHUTDOWN_MEM,
   VMCIQPB_GONE
} QPBrokerState;

#define QPBROKERSTATE_HAS_MEM(_qpb) (_qpb->state == VMCIQPB_CREATED_MEM || \
                                     _qpb->state == VMCIQPB_ATTACHED_MEM || \
                                     _qpb->state == VMCIQPB_SHUTDOWN_MEM)

/*
 * In the queue pair broker, we always use the guest point of view for
 * the produce and consume queue values and references, e.g., the
 * produce queue size stored is the guests produce queue size. The
 * host endpoint will need to swap these around. The only exception is
 * the local queue pairs on the host, in which case the host endpoint
 * that creates the queue pair will have the right orientation, and
 * the attaching host endpoint will need to swap.
 */

typedef struct QueuePairEntry {
   VMCIListItem       listItem;
   VMCIHandle         handle;
   VMCIId             peer;
   uint32             flags;
   uint64             produceSize;
   uint64             consumeSize;
   uint32             refCount;
} QueuePairEntry;

typedef struct QPBrokerEntry {
   QueuePairEntry       qp;
   VMCIId               createId;
   VMCIId               attachId;
   QPBrokerState        state;
   Bool                 requireTrustedAttach;
   Bool                 createdByTrusted;
   Bool                 vmciPageFiles;  // Created by VMX using VMCI page files
   VMCIQueue           *produceQ;
   VMCIQueue           *consumeQ;
   VMCIQueueHeader      savedProduceQ;
   VMCIQueueHeader      savedConsumeQ;
   VMCIEventReleaseCB   wakeupCB;
   void                *clientData;
   void                *localMem; // Kernel memory for local queue pair
} QPBrokerEntry;

#if !defined(VMKERNEL)
typedef struct QPGuestEndpoint {
   QueuePairEntry qp;
   uint64         numPPNs;
   void          *produceQ;
   void          *consumeQ;
   Bool           hibernateFailure;
   PPNSet         ppnSet;
} QPGuestEndpoint;
#endif

typedef struct QueuePairList {
   VMCIList       head;
   Atomic_uint32  hibernate;
   VMCIMutex      mutex;
} QueuePairList;

static QueuePairList qpBrokerList;

#define QPE_NUM_PAGES(_QPE) ((uint32)(CEILING(_QPE.produceSize, PAGE_SIZE) + \
                                      CEILING(_QPE.consumeSize, PAGE_SIZE) + 2))

#if !defined(VMKERNEL)
  static QueuePairList qpGuestEndpoints;
  static VMCIHandleArray *hibernateFailedList;
  static VMCILock hibernateFailedListLock;
#endif

static void VMCIQPBrokerLock(void);
static  void VMCIQPBrokerUnlock(void);

static QueuePairEntry *QueuePairList_FindEntry(QueuePairList *qpList,
                                               VMCIHandle handle);
static void QueuePairList_AddEntry(QueuePairList *qpList,
                                   QueuePairEntry *entry);
static void QueuePairList_RemoveEntry(QueuePairList *qpList,
                                      QueuePairEntry *entry);
static QueuePairEntry *QueuePairList_GetHead(QueuePairList *qpList);

static int QueuePairNotifyPeer(Bool attach, VMCIHandle handle, VMCIId myId,
                               VMCIId peerId);

static int VMCIQPBrokerAllocInt(VMCIHandle handle, VMCIId peer,
                                uint32 flags, VMCIPrivilegeFlags privFlags,
                                uint64 produceSize,
                                uint64 consumeSize,
                                QueuePairPageStore *pageStore,
                                VMCIContext *context,
                                VMCIEventReleaseCB wakeupCB,
                                void *clientData,
                                QPBrokerEntry **ent,
                                Bool *swap);
static int VMCIQPBrokerAttach(QPBrokerEntry *entry,
                              VMCIId peer,
                              uint32 flags,
                              VMCIPrivilegeFlags privFlags,
                              uint64 produceSize,
                              uint64 consumeSize,
                              QueuePairPageStore *pageStore,
                              VMCIContext *context,
                              VMCIEventReleaseCB wakeupCB,
                              void *clientData,
                              QPBrokerEntry **ent);
static int VMCIQPBrokerCreate(VMCIHandle handle,
                              VMCIId peer,
                              uint32 flags,
                              VMCIPrivilegeFlags privFlags,
                              uint64 produceSize,
                              uint64 consumeSize,
                              QueuePairPageStore *pageStore,
                              VMCIContext *context,
                              VMCIEventReleaseCB wakeupCB,
                              void *clientData,
                              QPBrokerEntry **ent);
static int VMCIQueuePairAllocHostWork(VMCIHandle *handle, VMCIQueue **produceQ,
                                      uint64 produceSize, VMCIQueue **consumeQ,
                                      uint64 consumeSize,
                                      VMCIId peer, uint32 flags,
                                      VMCIPrivilegeFlags privFlags,
                                      VMCIEventReleaseCB wakeupCB,
                                      void *clientData);
static int VMCIQueuePairDetachHostWork(VMCIHandle handle);

static int QueuePairSaveHeaders(QPBrokerEntry *entry);
static void QueuePairResetSavedHeaders(QPBrokerEntry *entry);

#if !defined(VMKERNEL)

static int QueuePairNotifyPeerLocal(Bool attach, VMCIHandle handle);

static QPGuestEndpoint *QPGuestEndpointCreate(VMCIHandle handle,
                                            VMCIId peer, uint32 flags,
                                            uint64 produceSize,
                                            uint64 consumeSize,
                                            void *produceQ, void *consumeQ);
static void QPGuestEndpointDestroy(QPGuestEndpoint *entry);
static int VMCIQueuePairAllocHypercall(const QPGuestEndpoint *entry);
static int VMCIQueuePairAllocGuestWork(VMCIHandle *handle, VMCIQueue **produceQ,
                                       uint64 produceSize, VMCIQueue **consumeQ,
                                       uint64 consumeSize,
                                       VMCIId peer, uint32 flags,
                                       VMCIPrivilegeFlags privFlags);
static int VMCIQueuePairDetachGuestWork(VMCIHandle handle);
static int VMCIQueuePairDetachHypercall(VMCIHandle handle);
static void VMCIQPMarkHibernateFailed(QPGuestEndpoint *entry);
static void VMCIQPUnmarkHibernateFailed(QPGuestEndpoint *entry);

extern int VMCI_SendDatagram(VMCIDatagram *);

#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Alloc --
 *
 *      Allocates a VMCI QueuePair. Only checks validity of input
 *      arguments. The real work is done in the host or guest
 *      specific function.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQueuePair_Alloc(VMCIHandle *handle,           // IN/OUT
                    VMCIQueue  **produceQ,        // OUT
                    uint64     produceSize,       // IN
                    VMCIQueue  **consumeQ,        // OUT
                    uint64     consumeSize,       // IN
                    VMCIId     peer,              // IN
                    uint32     flags,             // IN
                    VMCIPrivilegeFlags privFlags, // IN
                    Bool       guestEndpoint,     // IN
                    VMCIEventReleaseCB wakeupCB,  // IN
                    void *clientData)             // IN
{
   if (!handle || !produceQ || !consumeQ || (!produceSize && !consumeSize) ||
       (flags & ~VMCI_QP_ALL_FLAGS)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (guestEndpoint) {
#if !defined(VMKERNEL)
      return VMCIQueuePairAllocGuestWork(handle, produceQ, produceSize, consumeQ,
                                         consumeSize, peer, flags, privFlags);
#else
      return VMCI_ERROR_INVALID_ARGS;
#endif
   } else {
      return VMCIQueuePairAllocHostWork(handle, produceQ, produceSize, consumeQ,
                                        consumeSize, peer, flags, privFlags,
                                        wakeupCB, clientData);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Detach --
 *
 *      Detaches from a VMCI QueuePair. Only checks validity of input argument.
 *      Real work is done in the host or guest specific function.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQueuePair_Detach(VMCIHandle handle,   // IN
                     Bool guestEndpoint)  // IN
{
   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (guestEndpoint) {
#if !defined(VMKERNEL)
      return VMCIQueuePairDetachGuestWork(handle);
#else
      return VMCI_ERROR_INVALID_ARGS;
#endif
   } else {
      return VMCIQueuePairDetachHostWork(handle);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Init --
 *
 *      Initializes the list of QueuePairs.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
QueuePairList_Init(QueuePairList *qpList)  // IN
{
   int ret;

   VMCIList_Init(&qpList->head);
   Atomic_Write(&qpList->hibernate, 0);
   ret = VMCIMutex_Init(&qpList->mutex, "VMCIQPListLock",
                        VMCI_SEMA_RANK_QUEUEPAIRLIST);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Destroy --
 *
 *      Destroy the list's mutex.
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
QueuePairList_Destroy(QueuePairList *qpList)
{
   VMCIMutex_Destroy(&qpList->mutex);
   VMCIList_Init(&qpList->head);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBrokerLock --
 *
 *      Acquires the mutex protecting a VMCI queue pair broker transaction.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIQPBrokerLock(void)
{
   VMCIMutex_Acquire(&qpBrokerList.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBrokerUnlock --
 *
 *      Releases the mutex protecting a VMCI queue pair broker transaction.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIQPBrokerUnlock(void)
{
   VMCIMutex_Release(&qpBrokerList.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_FindEntry --
 *
 *      Finds the entry in the list corresponding to a given handle. Assumes
 *      that the list is locked.
 *
 * Results:
 *      Pointer to entry.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static QueuePairEntry *
QueuePairList_FindEntry(QueuePairList *qpList,  // IN
                        VMCIHandle handle)      // IN
{
   VMCIListItem *next;

   if (VMCI_HANDLE_INVALID(handle)) {
      return NULL;
   }

   VMCIList_Scan(next, &qpList->head) {
      QueuePairEntry *entry = VMCIList_Entry(next, QueuePairEntry, listItem);

      if (VMCI_HANDLE_EQUAL(entry->handle, handle)) {
         return entry;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_AddEntry --
 *
 *      Adds the given entry to the list. Assumes that the list is locked.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
QueuePairList_AddEntry(QueuePairList *qpList,  // IN
                       QueuePairEntry *entry)  // IN
{
   if (entry) {
      VMCIList_Insert(&entry->listItem, &qpList->head);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_RemoveEntry --
 *
 *      Removes the given entry from the list. Assumes that the list is locked.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
QueuePairList_RemoveEntry(QueuePairList *qpList,  // IN
                          QueuePairEntry *entry)  // IN
{
   if (entry) {
      VMCIList_Remove(&entry->listItem);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_GetHead --
 *
 *      Returns the entry from the head of the list. Assumes that the list is
 *      locked.
 *
 * Results:
 *      Pointer to entry.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static QueuePairEntry *
QueuePairList_GetHead(QueuePairList *qpList)
{
   VMCIListItem *first = VMCIList_First(&qpList->head);

   if (first) {
      QueuePairEntry *entry = VMCIList_Entry(first, QueuePairEntry, listItem);
      return entry;
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Init --
 *
 *      Initalizes queue pair broker state.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_Init(void)
{
   return QueuePairList_Init(&qpBrokerList);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Exit --
 *
 *      Destroys the queue pair broker state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIQPBroker_Exit(void)
{
   QPBrokerEntry *entry;

   VMCIQPBrokerLock();

   while ((entry = (QPBrokerEntry *)QueuePairList_GetHead(&qpBrokerList))) {
      QueuePairList_RemoveEntry(&qpBrokerList, &entry->qp);
      VMCI_FreeKernelMem(entry, sizeof *entry);
   }

   VMCIQPBrokerUnlock();
   QueuePairList_Destroy(&qpBrokerList);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Alloc --
 *
 *      Requests that a queue pair be allocated with the VMCI queue
 *      pair broker. Allocates a queue pair entry if one does not
 *      exist. Attaches to one if it exists, and retrieves the page
 *      files backing that QueuePair.  Assumes that the queue pair
 *      broker lock is held.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_Alloc(VMCIHandle handle,             // IN
                   VMCIId peer,                   // IN
                   uint32 flags,                  // IN
                   VMCIPrivilegeFlags privFlags,  // IN
                   uint64 produceSize,            // IN
                   uint64 consumeSize,            // IN
                   QueuePairPageStore *pageStore, // IN/OUT
                   VMCIContext *context)          // IN: Caller
{
   return VMCIQPBrokerAllocInt(handle, peer, flags, privFlags,
                               produceSize, consumeSize,
                               pageStore, context, NULL, NULL,
                               NULL, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairNotifyPeer --
 *
 *      Enqueues an event datagram to notify the peer VM attached to
 *      the given queue pair handle about attach/detach event by the
 *      given VM.
 *
 * Results:
 *      Payload size of datagram enqueued on success, error code otherwise.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

int
QueuePairNotifyPeer(Bool attach,       // IN: attach or detach?
                    VMCIHandle handle, // IN
                    VMCIId myId,       // IN
                    VMCIId peerId)     // IN: CID of VM to notify
{
   int rv;
   VMCIEventMsg *eMsg;
   VMCIEventPayload_QP *evPayload;
   char buf[sizeof *eMsg + sizeof *evPayload];

   if (VMCI_HANDLE_INVALID(handle) || myId == VMCI_INVALID_ID ||
       peerId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   /*
    * Notification message contains: queue pair handle and
    * attaching/detaching VM's context id.
    */

   eMsg = (VMCIEventMsg *)buf;

   /*
    * In VMCIContext_EnqueueDatagram() we enforce the upper limit on number of
    * pending events from the hypervisor to a given VM otherwise a rogue VM
    * could do an arbitrary number of attach and detach operations causing memory
    * pressure in the host kernel.
   */

   /* Clear out any garbage. */
   memset(eMsg, 0, sizeof buf);

   eMsg->hdr.dst = VMCI_MAKE_HANDLE(peerId, VMCI_EVENT_HANDLER);
   eMsg->hdr.src = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                    VMCI_CONTEXT_RESOURCE_ID);
   eMsg->hdr.payloadSize = sizeof *eMsg + sizeof *evPayload - sizeof eMsg->hdr;
   eMsg->eventData.event = attach ? VMCI_EVENT_QP_PEER_ATTACH :
                                    VMCI_EVENT_QP_PEER_DETACH;
   evPayload = VMCIEventMsgPayload(eMsg);
   evPayload->handle = handle;
   evPayload->peerId = myId;

   rv = VMCIDatagram_Dispatch(VMCI_HYPERVISOR_CONTEXT_ID, (VMCIDatagram *)eMsg,
                              FALSE);
   if (rv < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to enqueue QueuePair %s event datagram for "
                    "context (ID=0x%x).\n", attach ? "ATTACH" : "DETACH",
                    peerId));
   }

   return rv;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIQueuePairAllocHostWork --
 *
 *    This function implements the kernel API for allocating a queue
 *    pair.
 *
 * Results:
 *     VMCI_SUCCESS on succes and appropriate failure code otherwise.
 *
 * Side effects:
 *     May allocate memory.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIQueuePairAllocHostWork(VMCIHandle *handle,           // IN/OUT
                           VMCIQueue **produceQ,         // OUT
                           uint64 produceSize,           // IN
                           VMCIQueue **consumeQ,         // OUT
                           uint64 consumeSize,           // IN
                           VMCIId peer,                  // IN
                           uint32 flags,                 // IN
                           VMCIPrivilegeFlags privFlags, // IN
                           VMCIEventReleaseCB wakeupCB,  // IN
                           void *clientData)             // IN
{
   VMCIContext *context;
   QPBrokerEntry *entry;
   int result;
   Bool swap;

   if (VMCI_HANDLE_INVALID(*handle)) {
      VMCIId resourceID = VMCIResource_GetID(VMCI_HOST_CONTEXT_ID);
      if (resourceID == VMCI_INVALID_ID) {
         return VMCI_ERROR_NO_HANDLE;
      }
      *handle = VMCI_MAKE_HANDLE(VMCI_HOST_CONTEXT_ID, resourceID);
   }

   context = VMCIContext_Get(VMCI_HOST_CONTEXT_ID);
   ASSERT(context);

   entry = NULL;
   result = VMCIQPBrokerAllocInt(*handle, peer, flags, privFlags, produceSize,
                                 consumeSize, NULL, context, wakeupCB, clientData,
                                 &entry, &swap);
   if (result == VMCI_SUCCESS) {
      if (swap) {
         /*
          * If this is a local queue pair, the attacher will swap around produce
          * and consume queues.
          */

         *produceQ = entry->consumeQ;
         *consumeQ = entry->produceQ;
      } else {
         *produceQ = entry->produceQ;
         *consumeQ = entry->consumeQ;
      }
   } else {
      *handle = VMCI_INVALID_HANDLE;
      VMCI_DEBUG_LOG(4, (LGPFX"queue pair broker failed to alloc (result=%d).\n",
                         result));
   }
   VMCIContext_Release(context);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIQueuePairDetachHostWork --
 *
 *    This function implements the host kernel API for detaching from
 *    a queue pair.
 *
 * Results:
 *     VMCI_SUCCESS on success and appropriate failure code otherwise.
 *
 * Side effects:
 *     May deallocate memory.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIQueuePairDetachHostWork(VMCIHandle handle) // IN
{
   int result;
   VMCIContext *context;

   context = VMCIContext_Get(VMCI_HOST_CONTEXT_ID);

   result = VMCIQPBroker_Detach(handle, context);

   VMCIContext_Release(context);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBrokerAllocInt --
 *
 *      QueuePair_Alloc for use when setting up queue pair endpoints
 *      on the host. Like QueuePair_Alloc, but returns a pointer to
 *      the QPBrokerEntry on success.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory may be allocated.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPBrokerAllocInt(VMCIHandle handle,             // IN
                     VMCIId peer,                   // IN
                     uint32 flags,                  // IN
                     VMCIPrivilegeFlags privFlags,  // IN
                     uint64 produceSize,            // IN
                     uint64 consumeSize,            // IN
                     QueuePairPageStore *pageStore, // IN/OUT
                     VMCIContext *context,          // IN: Caller
                     VMCIEventReleaseCB wakeupCB,   // IN
                     void *clientData,              // IN
                     QPBrokerEntry **ent,           // OUT
                     Bool *swap)                    // OUT: swap queues?
{
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool create;
   QPBrokerEntry *entry;
   Bool isLocal = flags & VMCI_QPFLAG_LOCAL;
   int result;

   if (VMCI_HANDLE_INVALID(handle) ||
       (flags & ~VMCI_QP_ALL_FLAGS) ||
       (isLocal && (!vmkernel || contextId != VMCI_HOST_CONTEXT_ID ||
                     handle.context != contextId)) ||
       !(produceSize || consumeSize) ||
       !context || contextId == VMCI_INVALID_ID ||
       handle.context == VMCI_INVALID_ID) {
      VMCI_DEBUG_LOG(5, ("Invalid argument - handle, flags, whatever\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (pageStore && !VMCI_QP_PAGESTORE_IS_WELLFORMED(pageStore)) {
      VMCI_DEBUG_LOG(5, ("Invalid argument - page store\n"));
      return VMCI_ERROR_INVALID_ARGS;
   }

   /*
    * In the initial argument check, we ensure that non-vmkernel hosts
    * are not allowed to create local queue pairs.
    */

   ASSERT(vmkernel || !isLocal);

   VMCIQPBrokerLock();

   if (!isLocal && VMCIContext_QueuePairExists(context, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) already attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      VMCIQPBrokerUnlock();
      return VMCI_ERROR_ALREADY_EXISTS;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      create = TRUE;
      result = VMCIQPBrokerCreate(handle, peer, flags, privFlags, produceSize,
                                  consumeSize, pageStore, context, wakeupCB,
                                  clientData, ent);
   } else {
      create = FALSE;
      result = VMCIQPBrokerAttach(entry, peer, flags, privFlags, produceSize,
                                  consumeSize, pageStore, context, wakeupCB,
                                  clientData, ent);
   }

   VMCIQPBrokerUnlock();

   if (swap) {
      *swap = (contextId == VMCI_HOST_CONTEXT_ID) && !(create && isLocal);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBrokerCreate --
 *
 *      The first endpoint issuing a queue pair allocation will create the state
 *      of the queue pair in the queue pair broker.
 *
 *      If the creator is a guest, it will associate a VMX virtual address range
 *      with the queue pair as specified by the pageStore. For compatibility with
 *      older VMX'en, that would use a separate step to set the VMX virtual
 *      address range, the virtual address range can be registered later using
 *      VMCIQPBroker_SetPageStore. In that case, a pageStore of NULL should be
 *      used.
 *
 *      If the creator is the host, a pageStore of NULL should be used as well,
 *      since the host is not able to supply a page store for the queue pair.
 *
 *      For older VMX and host callers, the queue pair will be created in the
 *      VMCIQPB_CREATED_NO_MEM state, and for current VMX callers, it will be
 *      created in VMCOQPB_CREATED_MEM state.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Memory will be allocated, and pages may be pinned.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPBrokerCreate(VMCIHandle handle,             // IN
                   VMCIId peer,                   // IN
                   uint32 flags,                  // IN
                   VMCIPrivilegeFlags privFlags,  // IN
                   uint64 produceSize,            // IN
                   uint64 consumeSize,            // IN
                   QueuePairPageStore *pageStore, // IN
                   VMCIContext *context,          // IN: Caller
                   VMCIEventReleaseCB wakeupCB,   // IN
                   void *clientData,              // IN
                   QPBrokerEntry **ent)           // OUT
{
   QPBrokerEntry *entry = NULL;
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool isLocal = flags & VMCI_QPFLAG_LOCAL;
   int result;
   uint64 guestProduceSize;
   uint64 guestConsumeSize;

   /*
    * Do not create if the caller asked not to.
    */

   if (flags & VMCI_QPFLAG_ATTACH_ONLY) {
      VMCI_DEBUG_LOG(5, ("QP Create - attach only\n"));
      return VMCI_ERROR_NOT_FOUND;
   }

   /*
    * Creator's context ID should match handle's context ID or the creator
    * must allow the context in handle's context ID as the "peer".
    */

   if (handle.context != contextId && handle.context != peer) {
      VMCI_DEBUG_LOG(5, ("QP Create - contextId fail, %x != %x, %x\n",
                         handle.context, contextId, peer));
      return VMCI_ERROR_NO_ACCESS;
   }

   if (VMCI_CONTEXT_IS_VM(contextId) && VMCI_CONTEXT_IS_VM(peer)) {
      VMCI_DEBUG_LOG(5, ("QP Create - VM2VM\n"));
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   /*
    * Creator's context ID for local queue pairs should match the
    * peer, if a peer is specified.
    */

   if (isLocal && peer != VMCI_INVALID_ID && contextId != peer) {
      VMCI_DEBUG_LOG(5, ("QP Create - peer %x, context %x\n",
                         peer, contextId));
      return VMCI_ERROR_NO_ACCESS;
   }

   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_ATOMIC);
   if (!entry) {
      VMCI_DEBUG_LOG(5, ("QP Create - no memory\n"));
      return VMCI_ERROR_NO_MEM;
   }

   if (VMCIContext_GetId(context) == VMCI_HOST_CONTEXT_ID && !isLocal) {
      /*
       * The queue pair broker entry stores values from the guest
       * point of view, so a creating host side endpoint should swap
       * produce and consume values -- unless it is a local queue
       * pair, in which case no swapping is necessary, since the local
       * attacher will swap queues.
       */

      guestProduceSize = consumeSize;
      guestConsumeSize = produceSize;
   } else {
      guestProduceSize = produceSize;
      guestConsumeSize = consumeSize;
   }

   memset(entry, 0, sizeof *entry);
   entry->qp.handle = handle;
   entry->qp.peer = peer;
   entry->qp.flags = flags;
   entry->qp.produceSize = guestProduceSize;
   entry->qp.consumeSize = guestConsumeSize;
   entry->qp.refCount = 1;
   entry->createId = contextId;
   entry->attachId = VMCI_INVALID_ID;
   entry->state = VMCIQPB_NEW;
   entry->requireTrustedAttach =
      (context->privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED) ? TRUE : FALSE;
   entry->createdByTrusted =
      (privFlags & VMCI_PRIVILEGE_FLAG_TRUSTED) ? TRUE : FALSE;
   entry->vmciPageFiles = FALSE;
   entry->wakeupCB = wakeupCB;
   entry->clientData = clientData;
   entry->produceQ = VMCIHost_AllocQueue(guestProduceSize);
   if (entry->produceQ == NULL) {
      result = VMCI_ERROR_NO_MEM;
      VMCI_DEBUG_LOG(5, ("QP Create - no memory PQ\n"));
      goto error;
   }
   entry->consumeQ = VMCIHost_AllocQueue(guestConsumeSize);
   if (entry->consumeQ == NULL) {
      result = VMCI_ERROR_NO_MEM;
      VMCI_DEBUG_LOG(5, ("QP Create - no memory CQ\n"));
      goto error;
   }

   VMCI_InitQueueMutex(entry->produceQ, entry->consumeQ);

   VMCIList_InitEntry(&entry->qp.listItem);

   if (isLocal) {
      ASSERT(pageStore == NULL);

      entry->localMem = VMCI_AllocKernelMem(QPE_NUM_PAGES(entry->qp) * PAGE_SIZE,
                                            VMCI_MEMORY_NONPAGED);
      if (entry->localMem == NULL) {
         result = VMCI_ERROR_NO_MEM;
         VMCI_DEBUG_LOG(5, ("QP Create - no memory LM\n"));
         goto error;
      }
      entry->state = VMCIQPB_CREATED_MEM;
      entry->produceQ->qHeader = entry->localMem;
      entry->consumeQ->qHeader =
         (VMCIQueueHeader *)((uint8 *)entry->localMem +
             (CEILING(entry->qp.produceSize, PAGE_SIZE) + 1) * PAGE_SIZE);
      VMCIQueueHeader_Init(entry->produceQ->qHeader, handle);
      VMCIQueueHeader_Init(entry->consumeQ->qHeader, handle);
   } else if (pageStore) {
      ASSERT(entry->createId != VMCI_HOST_CONTEXT_ID || isLocal);

      /*
       * The VMX already initialized the queue pair headers, so no
       * need for the kernel side to do that.
       */

      result = VMCIHost_RegisterUserMemory(pageStore,
                                           entry->produceQ,
                                           entry->consumeQ);
      if (result < VMCI_SUCCESS) {
         VMCI_DEBUG_LOG(5, ("QP Create - cannot register user memory\n"));
         goto error;
      }
      VMCIHost_MarkQueuesAvailable(entry->produceQ, entry->consumeQ);
      entry->state = VMCIQPB_CREATED_MEM;
   } else {
      /*
       * A create without a pageStore may be either a host side create (in which
       * case we are waiting for the guest side to supply the memory) or an old
       * style queue pair create (in which case we will expect a set page store
       * call as the next step).
       */

      entry->state = VMCIQPB_CREATED_NO_MEM;
   }

   QueuePairList_AddEntry(&qpBrokerList, &entry->qp);
   if (ent != NULL) {
      *ent = entry;
   }

   VMCIContext_QueuePairCreate(context, handle);

   return VMCI_SUCCESS;

error:
   if (entry != NULL) {
      if (entry->produceQ != NULL) {
         VMCIHost_FreeQueue(entry->produceQ, guestProduceSize);
      }
      if (entry->consumeQ != NULL) {
         VMCIHost_FreeQueue(entry->consumeQ, guestConsumeSize);
      }
      VMCI_FreeKernelMem(entry, sizeof *entry);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBrokerAttach --
 *
 *      The second endpoint issuing a queue pair allocation will attach to the
 *      queue pair registered with the queue pair broker.
 *
 *      If the attacher is a guest, it will associate a VMX virtual address range
 *      with the queue pair as specified by the pageStore. At this point, the
 *      already attach host endpoint may start using the queue pair, and an
 *      attach event is sent to it. For compatibility with older VMX'en, that
 *      used a separate step to set the VMX virtual address range, the virtual
 *      address range can be registered later using VMCIQPBroker_SetPageStore. In
 *      that case, a pageStore of NULL should be used, and the attach event will
 *      be generated once the actual page store has been set.
 *
 *      If the attacher is the host, a pageStore of NULL should be used as well,
 *      since the page store information is already set by the guest.
 *
 *      For new VMX and host callers, the queue pair will be moved to the
 *      VMCIQPB_ATTACHED_MEM state, and for older VMX callers, it will be
 *      moved to the VMCOQPB_ATTACHED_NO_MEM state.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Memory will be allocated, and pages may be pinned.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPBrokerAttach(QPBrokerEntry *entry,          // IN
                   VMCIId peer,                   // IN
                   uint32 flags,                  // IN
                   VMCIPrivilegeFlags privFlags,  // IN
                   uint64 produceSize,            // IN
                   uint64 consumeSize,            // IN
                   QueuePairPageStore *pageStore, // IN/OUT
                   VMCIContext *context,          // IN: Caller
                   VMCIEventReleaseCB wakeupCB,   // IN
                   void *clientData,              // IN
                   QPBrokerEntry **ent)           // OUT
{
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool isLocal = flags & VMCI_QPFLAG_LOCAL;
   int result;

   if (entry->state != VMCIQPB_CREATED_NO_MEM &&
       entry->state != VMCIQPB_CREATED_MEM) {
      VMCI_DEBUG_LOG(5, ("QP Attach - state is %x\n", entry->state));
      return VMCI_ERROR_UNAVAILABLE;
   }

   if (isLocal) {
      if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL) ||
          contextId != entry->createId) {
         VMCI_DEBUG_LOG(5, ("QP Attach - invalid args, ctx=%x, createId=%x\n",
                            contextId, entry->createId));
         return VMCI_ERROR_INVALID_ARGS;
      }
   } else if (contextId == entry->createId || contextId == entry->attachId) {
      VMCI_DEBUG_LOG(5, ("QP Attach - already, ctx=%x, create=%x, attach=%x\n",
                         contextId, entry->createId, entry->attachId));
      return VMCI_ERROR_ALREADY_EXISTS;
   }

   ASSERT(entry->qp.refCount < 2);
   ASSERT(entry->attachId == VMCI_INVALID_ID);

   if (VMCI_CONTEXT_IS_VM(contextId) && VMCI_CONTEXT_IS_VM(entry->createId)) {
      VMCI_DEBUG_LOG(5, ("QP Attach - VM2VM\n"));
      return VMCI_ERROR_DST_UNREACHABLE;
   }

   /*
    * If we are attaching from a restricted context then the queuepair
    * must have been created by a trusted endpoint.
    */

   if (context->privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED) {
      if (!entry->createdByTrusted) {
         VMCI_DEBUG_LOG(5, ("QP Attach - restricted vs trusted\n"));
         return VMCI_ERROR_NO_ACCESS;
      }
   }

   /*
    * If we are attaching to a queuepair that was created by a restricted
    * context then we must be trusted.
    */

   if (entry->requireTrustedAttach) {
      if (!(privFlags & VMCI_PRIVILEGE_FLAG_TRUSTED)) {
         VMCI_DEBUG_LOG(5, ("QP Attach - trusted attach required\n"));
         return VMCI_ERROR_NO_ACCESS;
      }
   }

   /*
    * If the creator specifies VMCI_INVALID_ID in "peer" field, access
    * control check is not performed.
    */

   if (entry->qp.peer != VMCI_INVALID_ID && entry->qp.peer != contextId) {
      VMCI_DEBUG_LOG(5, ("QP Attach - bad peer id %x != %x\n",
                         contextId, entry->qp.peer));
      return VMCI_ERROR_NO_ACCESS;
   }

   if (entry->createId == VMCI_HOST_CONTEXT_ID) {
      /*
       * Do not attach if the caller doesn't support Host Queue Pairs
       * and a host created this queue pair.
       */

      if (!VMCIContext_SupportsHostQP(context)) {
         VMCI_DEBUG_LOG(5, ("QP Attach - no attach to host qp\n"));
         return VMCI_ERROR_INVALID_RESOURCE;
      }
   } else if (contextId == VMCI_HOST_CONTEXT_ID) {
      VMCIContext *createContext;
      Bool supportsHostQP;

      /*
       * Do not attach a host to a user created queue pair if that
       * user doesn't support host queue pair end points.
       */

      createContext = VMCIContext_Get(entry->createId);
      supportsHostQP = VMCIContext_SupportsHostQP(createContext);
      VMCIContext_Release(createContext);

      if (!supportsHostQP) {
         VMCI_DEBUG_LOG(5, ("QP Attach - no host attach to qp\n"));
         return VMCI_ERROR_INVALID_RESOURCE;
      }
   }

   if ((entry->qp.flags & ~VMCI_QP_ASYMM) != (flags & ~VMCI_QP_ASYMM_PEER)) {
      VMCI_DEBUG_LOG(5, ("QP Attach - flags mismatch\n"));
      return VMCI_ERROR_QUEUEPAIR_MISMATCH;
   }

   if (contextId != VMCI_HOST_CONTEXT_ID) {
      /*
       * The queue pair broker entry stores values from the guest
       * point of view, so an attaching guest should match the values
       * stored in the entry.
       */

      if (entry->qp.produceSize != produceSize ||
          entry->qp.consumeSize != consumeSize) {
         VMCI_DEBUG_LOG(5, ("QP Attach - queue size mismatch\n"));
         return VMCI_ERROR_QUEUEPAIR_MISMATCH;
      }
   } else if (entry->qp.produceSize != consumeSize ||
              entry->qp.consumeSize != produceSize) {
      VMCI_DEBUG_LOG(5, ("QP Attach - host queue size mismatch\n"));
      return VMCI_ERROR_QUEUEPAIR_MISMATCH;
   }

   if (contextId != VMCI_HOST_CONTEXT_ID) {
      /*
       * If a guest attached to a queue pair, it will supply the backing memory.
       * If this is a pre NOVMVM vmx, the backing memory will be supplied by
       * calling VMCIQPBroker_SetPageStore() following the return of the
       * VMCIQPBroker_Alloc() call. If it is a vmx of version NOVMVM or later,
       * the page store must be supplied as part of the VMCIQPBroker_Alloc call.
       * Under all circumstances must the initially created queue pair not have
       * any memory associated with it already.
       */

      if (entry->state != VMCIQPB_CREATED_NO_MEM) {
         VMCI_DEBUG_LOG(5, ("QP Attach - bad QP state for VM2VM, %x, %p\n",
                            entry->state, pageStore));
         return VMCI_ERROR_INVALID_ARGS;
      }

      if (pageStore != NULL) {
         /*
          * Patch up host state to point to guest supplied memory. The VMX
          * already initialized the queue pair headers, so no need for the
          * kernel side to do that.
          */

         result = VMCIHost_RegisterUserMemory(pageStore,
                                              entry->produceQ,
                                              entry->consumeQ);
         if (result < VMCI_SUCCESS) {
            VMCI_DEBUG_LOG(5, ("QP Attach - cannot register memory\n"));
            return result;
         }
         VMCIHost_MarkQueuesAvailable(entry->produceQ, entry->consumeQ);
         if (entry->qp.flags & VMCI_QPFLAG_NONBLOCK) {
            result = VMCIHost_MapQueues(entry->produceQ, entry->consumeQ,
                                        entry->qp.flags);
            if (result < VMCI_SUCCESS) {
               VMCIHost_ReleaseUserMemory(entry->produceQ, entry->consumeQ);
               VMCI_DEBUG_LOG(5, ("QP Attach - cannot map queues\n"));
               return result;
            }
         }
         entry->state = VMCIQPB_ATTACHED_MEM;
      } else {
         entry->state = VMCIQPB_ATTACHED_NO_MEM;
      }
   } else if (entry->state == VMCIQPB_CREATED_NO_MEM) {
      /*
       * The host side is attempting to attach to a queue pair that doesn't have
       * any memory associated with it. This must be a pre NOVMVM vmx that hasn't
       * set the page store information yet, or a quiesced VM.
       */

      VMCI_DEBUG_LOG(5, ("QP Attach - QP without memory\n"));
      return VMCI_ERROR_UNAVAILABLE;
   } else {
      /*
       * For non-blocking queue pairs, we cannot rely on enqueue/dequeue to map
       * in the pages on the host-side, since it may block, so we make an attempt
       * here.
       */

      if (flags & VMCI_QPFLAG_NONBLOCK) {
         result = VMCIHost_MapQueues(entry->produceQ, entry->consumeQ, flags);
         if (result < VMCI_SUCCESS) {
            VMCI_DEBUG_LOG(5, ("QP Attach - cannot map queues for host\n"));
            return result;
         }
         entry->qp.flags |= flags & (VMCI_QPFLAG_NONBLOCK | VMCI_QPFLAG_PINNED);
      }

      /*
       * The host side has successfully attached to a queue pair.
       */

      entry->state = VMCIQPB_ATTACHED_MEM;
   }

   if (entry->state == VMCIQPB_ATTACHED_MEM) {
      result = QueuePairNotifyPeer(TRUE, entry->qp.handle, contextId,
                                   entry->createId);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to notify peer (ID=0x%x) of attach to queue "
                       "pair (handle=0x%x:0x%x).\n", entry->createId,
                       entry->qp.handle.context, entry->qp.handle.resource));
      }
   }

   entry->attachId = contextId;
   entry->qp.refCount++;
   if (wakeupCB) {
      ASSERT(!entry->wakeupCB);
      entry->wakeupCB = wakeupCB;
      entry->clientData = clientData;
   }

   /*
    * When attaching to local queue pairs, the context already has
    * an entry tracking the queue pair, so don't add another one.
    */

   if (!isLocal) {
      VMCIContext_QueuePairCreate(context, entry->qp.handle);
   } else {
      ASSERT(VMCIContext_QueuePairExists(context, entry->qp.handle));
   }
   if (ent != NULL) {
      *ent = entry;
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_SetPageStore --
 *
 *      VMX'en with versions lower than VMCI_VERSION_NOVMVM use a separate
 *      step to add the UVAs of the VMX mapping of the queue pair. This function
 *      provides backwards compatibility with such VMX'en, and takes care of
 *      registering the page store for a queue pair previously allocated by the
 *      VMX during create or attach. This function will move the queue pair state
 *      to either from VMCIQBP_CREATED_NO_MEM to VMCIQBP_CREATED_MEM or
 *      VMCIQBP_ATTACHED_NO_MEM to VMCIQBP_ATTACHED_MEM. If moving to the
 *      attached state with memory, the queue pair is ready to be used by the
 *      host peer, and an attached event will be generated.
 *
 *      Assumes that the queue pair broker lock is held.
 *
 *      This function is only used by the hosted platform, since there is no
 *      issue with backwards compatibility for vmkernel.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Pages may get pinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_SetPageStore(VMCIHandle handle,      // IN
                          VA64 produceUVA,        // IN
                          VA64 consumeUVA,        // IN
                          VMCIContext *context)   // IN: Caller
{
   QPBrokerEntry *entry;
   int result;
   const VMCIId contextId = VMCIContext_GetId(context);

   if (VMCI_HANDLE_INVALID(handle) || !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   /*
    * We only support guest to host queue pairs, so the VMX must
    * supply UVAs for the mapped page files.
    */

   if (produceUVA == 0 || consumeUVA == 0) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPBrokerLock();

   if (!VMCIContext_QueuePairExists(context, handle)) {
      VMCI_WARNING((LGPFX"Context (ID=0x%x) not attached to queue pair "
                    "(handle=0x%x:0x%x).\n", contextId, handle.context,
                    handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   /*
    * If I'm the owner then I can set the page store.
    *
    * Or, if a host created the QueuePair and I'm the attached peer
    * then I can set the page store.
    */

   if (entry->createId != contextId &&
       (entry->createId != VMCI_HOST_CONTEXT_ID ||
        entry->attachId != contextId)) {
      result = VMCI_ERROR_QUEUEPAIR_NOTOWNER;
      goto out;
   }

   if (entry->state != VMCIQPB_CREATED_NO_MEM &&
       entry->state != VMCIQPB_ATTACHED_NO_MEM) {
      result = VMCI_ERROR_UNAVAILABLE;
      goto out;
   }

   result = VMCIHost_GetUserMemory(produceUVA, consumeUVA,
                                   entry->produceQ, entry->consumeQ);
   if (result < VMCI_SUCCESS) {
      goto out;
   }
   VMCIHost_MarkQueuesAvailable(entry->produceQ, entry->consumeQ);
   result = VMCIHost_MapQueues(entry->produceQ, entry->consumeQ, FALSE);
   if (result < VMCI_SUCCESS) {
     VMCIHost_ReleaseUserMemory(entry->produceQ, entry->consumeQ);
     goto out;
   }

   if (entry->state == VMCIQPB_CREATED_NO_MEM) {
      entry->state = VMCIQPB_CREATED_MEM;
   } else {
      ASSERT(entry->state == VMCIQPB_ATTACHED_NO_MEM);
      entry->state = VMCIQPB_ATTACHED_MEM;
   }
   entry->vmciPageFiles = TRUE;

   if (entry->state == VMCIQPB_ATTACHED_MEM) {
      result = QueuePairNotifyPeer(TRUE, handle, contextId, entry->createId);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to notify peer (ID=0x%x) of attach to queue "
                       "pair (handle=0x%x:0x%x).\n", entry->createId,
                       entry->qp.handle.context, entry->qp.handle.resource));
      }
   }

   result = VMCI_SUCCESS;
out:
   VMCIQPBrokerUnlock();
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Detach --
 *
 *      The main entry point for detaching from a queue pair registered with the
 *      queue pair broker. If more than one endpoint is attached to the queue
 *      pair, the first endpoint will mainly decrement a reference count and
 *      generate a notification to its peer. The last endpoint will clean up
 *      the queue pair state registered with the broker.
 *
 *      When a guest endpoint detaches, it will unmap and unregister the guest
 *      memory backing the queue pair. If the host is still attached, it will
 *      no longer be able to access the queue pair content.
 *
 *      If the queue pair is already in a state where there is no memory
 *      registered for the queue pair (any *_NO_MEM state), it will transition to
 *      the VMCIQPB_SHUTDOWN_NO_MEM state. This will also happen, if a guest 
 *      endpoint is the first of two endpoints to detach. If the host endpoint is
 *      the first out of two to detach, the queue pair will move to the
 *      VMCIQPB_SHUTDOWN_MEM state.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Memory may be freed, and pages may be unpinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_Detach(VMCIHandle  handle,   // IN
                    VMCIContext *context) // IN
{
   QPBrokerEntry *entry;
   const VMCIId contextId = VMCIContext_GetId(context);
   VMCIId peerId;
   Bool isLocal = FALSE;
   int result;

   if (VMCI_HANDLE_INVALID(handle) || !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPBrokerLock();

   if (!VMCIContext_QueuePairExists(context, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) not attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) reports being attached to queue pair "
                         "(handle=0x%x:0x%x) that isn't present in broker.\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   if (contextId != entry->createId && contextId != entry->attachId) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
      goto out;
   }

   if (contextId == entry->createId) {
      peerId = entry->attachId;
      entry->createId = VMCI_INVALID_ID;
   } else {
      peerId = entry->createId;
      entry->attachId = VMCI_INVALID_ID;
   }
   entry->qp.refCount--;

   isLocal = entry->qp.flags & VMCI_QPFLAG_LOCAL;

   if (contextId != VMCI_HOST_CONTEXT_ID) {
      int result;
      Bool headersMapped;

      ASSERT(!isLocal);

      /*
       * Pre NOVMVM vmx'en may detach from a queue pair before setting the page
       * store, and in that case there is no user memory to detach from. Also,
       * more recent VMX'en may detach from a queue pair in the quiesced state.
       */

      VMCI_AcquireQueueMutex(entry->produceQ, TRUE);
      headersMapped = entry->produceQ->qHeader || entry->consumeQ->qHeader;
      if (QPBROKERSTATE_HAS_MEM(entry)) {
         result = VMCIHost_UnmapQueues(INVALID_VMCI_GUEST_MEM_ID,
                                       entry->produceQ,
                                       entry->consumeQ);
         if (result < VMCI_SUCCESS) {
            VMCI_WARNING((LGPFX"Failed to unmap queue headers for queue pair "
                          "(handle=0x%x:0x%x,result=%d).\n", handle.context,
                          handle.resource, result));
         }
         VMCIHost_MarkQueuesUnavailable(entry->produceQ, entry->consumeQ);
         if (entry->vmciPageFiles) {
            VMCIHost_ReleaseUserMemory(entry->produceQ, entry->consumeQ);
         } else {
            VMCIHost_UnregisterUserMemory(entry->produceQ, entry->consumeQ);
         }
      }
      if (!headersMapped) {
         QueuePairResetSavedHeaders(entry);
      }
      VMCI_ReleaseQueueMutex(entry->produceQ);
      if (!headersMapped && entry->wakeupCB) {
         entry->wakeupCB(entry->clientData);
      }
   } else {
      if (entry->wakeupCB) {
         entry->wakeupCB = NULL;
         entry->clientData = NULL;
      }
   }

   if (entry->qp.refCount == 0) {
      QueuePairList_RemoveEntry(&qpBrokerList, &entry->qp);

      if (isLocal) {
         VMCI_FreeKernelMem(entry->localMem, QPE_NUM_PAGES(entry->qp) * PAGE_SIZE);
      }
      VMCI_CleanupQueueMutex(entry->produceQ, entry->consumeQ);
      VMCIHost_FreeQueue(entry->produceQ, entry->qp.produceSize);
      VMCIHost_FreeQueue(entry->consumeQ, entry->qp.consumeSize);
      VMCI_FreeKernelMem(entry, sizeof *entry);

      VMCIContext_QueuePairDestroy(context, handle);
   } else {
      ASSERT(peerId != VMCI_INVALID_ID);
      QueuePairNotifyPeer(FALSE, handle, contextId, peerId);
      if (contextId == VMCI_HOST_CONTEXT_ID && QPBROKERSTATE_HAS_MEM(entry)) {
         entry->state = VMCIQPB_SHUTDOWN_MEM;
      } else {
         entry->state = VMCIQPB_SHUTDOWN_NO_MEM;
      }
      if (!isLocal) {
         VMCIContext_QueuePairDestroy(context, handle);
      }
   }
   result = VMCI_SUCCESS;
out:
   VMCIQPBrokerUnlock();
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Map --
 *
 *      Establishes the necessary mappings for a queue pair given a
 *      reference to the queue pair guest memory. This is usually
 *      called when a guest is unquiesced and the VMX is allowed to
 *      map guest memory once again.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Memory may be allocated, and pages may be pinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_Map(VMCIHandle  handle,      // IN
                 VMCIContext *context,    // IN
                 VMCIQPGuestMem guestMem) // IN
{
   QPBrokerEntry *entry;
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool isLocal = FALSE;
   int result;

   if (VMCI_HANDLE_INVALID(handle) || !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPBrokerLock();

   if (!VMCIContext_QueuePairExists(context, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) not attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) reports being attached to queue pair "
                         "(handle=0x%x:0x%x) that isn't present in broker.\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   if (contextId != entry->createId && contextId != entry->attachId) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
      goto out;
   }

   isLocal = entry->qp.flags & VMCI_QPFLAG_LOCAL;

   if (vmkernel) {
      /*
       * On vmkernel, the readiness of the queue pair can be signalled
       * immediately since the guest memory is already registered.
       */

      VMCI_AcquireQueueMutex(entry->produceQ, TRUE);
      VMCIHost_MarkQueuesAvailable(entry->produceQ, entry->consumeQ);
      if (entry->qp.flags & VMCI_QPFLAG_NONBLOCK) {
         result = VMCIHost_MapQueues(entry->produceQ, entry->consumeQ,
                                     entry->qp.flags);
      } else {
         result = VMCI_SUCCESS;
      }
      if (result == VMCI_SUCCESS) {
         QueuePairResetSavedHeaders(entry);
      }
      VMCI_ReleaseQueueMutex(entry->produceQ);
      if (result == VMCI_SUCCESS) {
         if (entry->wakeupCB) {
            entry->wakeupCB(entry->clientData);
         }
      }
   } else  if (contextId != VMCI_HOST_CONTEXT_ID) {
      QueuePairPageStore pageStore;

      ASSERT(entry->state == VMCIQPB_CREATED_NO_MEM ||
             entry->state == VMCIQPB_SHUTDOWN_NO_MEM ||
             entry->state == VMCIQPB_ATTACHED_NO_MEM);
      ASSERT(!isLocal);

      pageStore.pages = guestMem;
      pageStore.len = QPE_NUM_PAGES(entry->qp);

      VMCI_AcquireQueueMutex(entry->produceQ, TRUE);
      QueuePairResetSavedHeaders(entry);
      result = VMCIHost_RegisterUserMemory(&pageStore, entry->produceQ, entry->consumeQ);
      VMCIHost_MarkQueuesAvailable(entry->produceQ, entry->consumeQ);
      VMCI_ReleaseQueueMutex(entry->produceQ);
      if (result == VMCI_SUCCESS) {
         /*
          * Move state from *_NO_MEM to *_MEM.
          */

         entry->state++;

         ASSERT(entry->state == VMCIQPB_CREATED_MEM ||
                entry->state == VMCIQPB_SHUTDOWN_MEM ||
                entry->state == VMCIQPB_ATTACHED_MEM);

         if (entry->wakeupCB) {
            entry->wakeupCB(entry->clientData);
         }
      }
   } else {
      result = VMCI_SUCCESS;
   }

out:
   VMCIQPBrokerUnlock();
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairSaveHeaders --
 *
 *      Saves a snapshot of the queue headers for the given QP broker
 *      entry. Should be used when guest memory is unmapped.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code if guest memory
 *      can't be accessed..
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
QueuePairSaveHeaders(QPBrokerEntry *entry) // IN
{
   int result;

   if (entry->produceQ->savedHeader != NULL &&
       entry->consumeQ->savedHeader != NULL) {
      /*
       *  If the headers have already been saved, we don't need to do
       *  it again, and we don't want to map in the headers
       *  unnecessarily.
       */
      return VMCI_SUCCESS;
   }
   if (NULL == entry->produceQ->qHeader || NULL == entry->consumeQ->qHeader) {
      result = VMCIHost_MapQueues(entry->produceQ, entry->consumeQ, FALSE);
      if (result < VMCI_SUCCESS) {
         return result;
      }
   }
   memcpy(&entry->savedProduceQ, entry->produceQ->qHeader,
          sizeof entry->savedProduceQ);
   entry->produceQ->savedHeader = &entry->savedProduceQ;
   memcpy(&entry->savedConsumeQ, entry->consumeQ->qHeader,
          sizeof entry->savedConsumeQ);
   entry->consumeQ->savedHeader = &entry->savedConsumeQ;

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairResetSavedHeaders --
 *
 *      Resets saved queue headers for the given QP broker
 *      entry. Should be used when guest memory becomes available
 *      again, or the guest detaches.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
QueuePairResetSavedHeaders(QPBrokerEntry *entry) // IN
{
   if (vmkernel) {
      VMCI_LockQueueHeader(entry->produceQ);
   }
   entry->produceQ->savedHeader = NULL;
   entry->consumeQ->savedHeader = NULL;
   if (vmkernel) {
      VMCI_UnlockQueueHeader(entry->produceQ);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Unmap --
 *
 *      Removes all references to the guest memory of a given queue pair, and
 *      will move the queue pair from state *_MEM to *_NO_MEM. It is usually
 *      called when a VM is being quiesced where access to guest memory should
 *      avoided.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Memory may be freed, and pages may be unpinned.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPBroker_Unmap(VMCIHandle  handle,   // IN
                   VMCIContext *context, // IN
                   VMCIGuestMemID gid)   // IN
{
   QPBrokerEntry *entry;
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool isLocal = FALSE;
   int result;

   if (VMCI_HANDLE_INVALID(handle) || !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPBrokerLock();
   if (!VMCIContext_QueuePairExists(context, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) not attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) reports being attached to queue pair "
                         "(handle=0x%x:0x%x) that isn't present in broker.\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   if (contextId != entry->createId && contextId != entry->attachId) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
      goto out;
   }

   isLocal = entry->qp.flags & VMCI_QPFLAG_LOCAL;

   if (contextId != VMCI_HOST_CONTEXT_ID) {
      ASSERT(entry->state != VMCIQPB_CREATED_NO_MEM &&
             entry->state != VMCIQPB_SHUTDOWN_NO_MEM &&
             entry->state != VMCIQPB_ATTACHED_NO_MEM);
      ASSERT(!isLocal);

      VMCI_AcquireQueueMutex(entry->produceQ, TRUE);
      result = QueuePairSaveHeaders(entry);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to save queue headers for queue pair "
                       "(handle=0x%x:0x%x,result=%d).\n", handle.context,
                       handle.resource, result));
      }
      VMCIHost_UnmapQueues(gid, entry->produceQ, entry->consumeQ);
      VMCIHost_MarkQueuesUnavailable(entry->produceQ, entry->consumeQ);
      if (!vmkernel) {
         /*
          * On hosted, when we unmap queue pairs, the VMX will also
          * unmap the guest memory, so we invalidate the previously
          * registered memory. If the queue pair is mapped again at a
          * later point in time, we will need to reregister the user
          * memory with a possibly new user VA.
          */

         VMCIHost_UnregisterUserMemory(entry->produceQ, entry->consumeQ);

         /*
          * Move state from *_MEM to *_NO_MEM.
          */

         entry->state--;
      }

      VMCI_ReleaseQueueMutex(entry->produceQ);
   }

   result = VMCI_SUCCESS;
out:
   VMCIQPBrokerUnlock();
   return result;
}

#if !defined(VMKERNEL)

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPGuestEndpoints_Init --
 *
 *      Initalizes data structure state keeping track of queue pair
 *      guest endpoints.
 *
 * Results:
 *      VMCI_SUCCESS on success and appropriate failure code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQPGuestEndpoints_Init(void)
{
   int err;

   err = QueuePairList_Init(&qpGuestEndpoints);
   if (err < VMCI_SUCCESS) {
      return err;
   }

   hibernateFailedList = VMCIHandleArray_Create(0);
   if (NULL == hibernateFailedList) {
      QueuePairList_Destroy(&qpGuestEndpoints);
      return VMCI_ERROR_NO_MEM;
   }

   /*
    * The lock rank must be lower than subscriberLock in vmciEvent,
    * since we hold the hibernateFailedListLock while generating
    * detach events.
    */

   err = VMCI_InitLock(&hibernateFailedListLock, "VMCIQPHibernateFailed",
                       VMCI_LOCK_RANK_QPHIBERNATE);
   if (err < VMCI_SUCCESS) {
      VMCIHandleArray_Destroy(hibernateFailedList);
      hibernateFailedList = NULL;
      QueuePairList_Destroy(&qpGuestEndpoints);
   }

   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPGuestEndpoints_Exit --
 *
 *      Destroys all guest queue pair endpoints. If active guest queue
 *      pairs still exist, hypercalls to attempt detach from these
 *      queue pairs will be made. Any failure to detach is silently
 *      ignored.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIQPGuestEndpoints_Exit(void)
{
   QPGuestEndpoint *entry;

   VMCIMutex_Acquire(&qpGuestEndpoints.mutex);

   while ((entry = (QPGuestEndpoint *)QueuePairList_GetHead(&qpGuestEndpoints))) {
      /*
       * Don't make a hypercall for local QueuePairs.
       */
      if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL)) {
         VMCIQueuePairDetachHypercall(entry->qp.handle);
      }
      /*
       * We cannot fail the exit, so let's reset refCount.
       */
      entry->qp.refCount = 0;
      QueuePairList_RemoveEntry(&qpGuestEndpoints, &entry->qp);
      QPGuestEndpointDestroy(entry);
   }

   Atomic_Write(&qpGuestEndpoints.hibernate, 0);
   VMCIMutex_Release(&qpGuestEndpoints.mutex);
   QueuePairList_Destroy(&qpGuestEndpoints);
   VMCI_CleanupLock(&hibernateFailedListLock);
   VMCIHandleArray_Destroy(hibernateFailedList);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPGuestEndpoints_Sync --
 *
 *      Use this as a synchronization point when setting globals, for example,
 *      during device shutdown.
 *
 * Results:
 *      TRUE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCIQPGuestEndpoints_Sync(void)
{
   VMCIMutex_Acquire(&qpGuestEndpoints.mutex);
   VMCIMutex_Release(&qpGuestEndpoints.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QPGuestEndpointCreate --
 *
 *      Allocates and initializes a QPGuestEndpoint structure.
 *      Allocates a QueuePair rid (and handle) iff the given entry has
 *      an invalid handle.  0 through VMCI_RESERVED_RESOURCE_ID_MAX
 *      are reserved handles.  Assumes that the QP list mutex is held
 *      by the caller.
 *
 * Results:
 *      Pointer to structure intialized.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

QPGuestEndpoint *
QPGuestEndpointCreate(VMCIHandle handle,  // IN
                      VMCIId peer,        // IN
                      uint32 flags,       // IN
                      uint64 produceSize, // IN
                      uint64 consumeSize, // IN
                      void *produceQ,     // IN
                      void *consumeQ)     // IN
{
   static VMCIId queuePairRID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
   QPGuestEndpoint *entry;
   const uint64 numPPNs = CEILING(produceSize, PAGE_SIZE) +
                          CEILING(consumeSize, PAGE_SIZE) +
                          2; /* One page each for the queue headers. */

   ASSERT((produceSize || consumeSize) && produceQ && consumeQ);

   if (VMCI_HANDLE_INVALID(handle)) {
      VMCIId contextID = vmci_get_context_id();
      VMCIId oldRID = queuePairRID;

      /*
       * Generate a unique QueuePair rid.  Keep on trying until we wrap around
       * in the RID space.
       */
      ASSERT(oldRID > VMCI_RESERVED_RESOURCE_ID_MAX);
      do {
         handle = VMCI_MAKE_HANDLE(contextID, queuePairRID);
         entry = (QPGuestEndpoint *)QueuePairList_FindEntry(&qpGuestEndpoints,
                                                            handle);
         queuePairRID++;
         if (UNLIKELY(!queuePairRID)) {
            /*
             * Skip the reserved rids.
             */
            queuePairRID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
         }
      } while (entry && queuePairRID != oldRID);

      if (UNLIKELY(entry != NULL)) {
         ASSERT(queuePairRID == oldRID);
         /*
          * We wrapped around --- no rids were free.
          */
         return NULL;
      }
   }

   ASSERT(!VMCI_HANDLE_INVALID(handle) &&
          QueuePairList_FindEntry(&qpGuestEndpoints, handle) == NULL);
   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NORMAL);
   if (entry) {
      entry->qp.handle = handle;
      entry->qp.peer = peer;
      entry->qp.flags = flags;
      entry->qp.produceSize = produceSize;
      entry->qp.consumeSize = consumeSize;
      entry->qp.refCount = 0;
      entry->numPPNs = numPPNs;
      memset(&entry->ppnSet, 0, sizeof entry->ppnSet);
      entry->produceQ = produceQ;
      entry->consumeQ = consumeQ;
      VMCIList_InitEntry(&entry->qp.listItem);
   }
   return entry;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QPGuestEndpointDestroy --
 *
 *      Frees a QPGuestEndpoint structure.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
QPGuestEndpointDestroy(QPGuestEndpoint *entry) // IN
{
   ASSERT(entry);
   ASSERT(entry->qp.refCount == 0);

   VMCI_FreePPNSet(&entry->ppnSet);
   VMCI_CleanupQueueMutex(entry->produceQ, entry->consumeQ);
   VMCI_FreeQueue(entry->produceQ, entry->qp.produceSize);
   VMCI_FreeQueue(entry->consumeQ, entry->qp.consumeSize);
   VMCI_FreeKernelMem(entry, sizeof *entry);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairAllocHypercall --
 *
 *      Helper to make a QueuePairAlloc hypercall when the driver is
 *      supporting a guest device.
 *
 * Results:
 *      Result of the hypercall.
 *
 * Side effects:
 *      Memory is allocated & freed.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQueuePairAllocHypercall(const QPGuestEndpoint *entry) // IN
{
   VMCIQueuePairAllocMsg *allocMsg;
   size_t msgSize;
   int result;

   if (!entry || entry->numPPNs <= 2) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   ASSERT(!(entry->qp.flags & VMCI_QPFLAG_LOCAL));

   msgSize = sizeof *allocMsg + (size_t)entry->numPPNs * sizeof(PPN);
   allocMsg = VMCI_AllocKernelMem(msgSize, VMCI_MEMORY_NONPAGED);
   if (!allocMsg) {
      return VMCI_ERROR_NO_MEM;
   }

   allocMsg->hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                        VMCI_QUEUEPAIR_ALLOC);
   allocMsg->hdr.src = VMCI_ANON_SRC_HANDLE;
   allocMsg->hdr.payloadSize = msgSize - VMCI_DG_HEADERSIZE;
   allocMsg->handle = entry->qp.handle;
   allocMsg->peer = entry->qp.peer;
   allocMsg->flags = entry->qp.flags;
   allocMsg->produceSize = entry->qp.produceSize;
   allocMsg->consumeSize = entry->qp.consumeSize;
   allocMsg->numPPNs = entry->numPPNs;
   result = VMCI_PopulatePPNList((uint8 *)allocMsg + sizeof *allocMsg, &entry->ppnSet);
   if (result == VMCI_SUCCESS) {
      result = VMCI_SendDatagram((VMCIDatagram *)allocMsg);
   }
   VMCI_FreeKernelMem(allocMsg, msgSize);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairAllocGuestWork --
 *
 *      This functions handles the actual allocation of a VMCI queue
 *      pair guest endpoint. Allocates physical pages for the queue
 *      pair. It makes OS dependent calls through generic wrappers.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQueuePairAllocGuestWork(VMCIHandle *handle,           // IN/OUT
                            VMCIQueue **produceQ,         // OUT
                            uint64 produceSize,           // IN
                            VMCIQueue **consumeQ,         // OUT
                            uint64 consumeSize,           // IN
                            VMCIId peer,                  // IN
                            uint32 flags,                 // IN
                            VMCIPrivilegeFlags privFlags) // IN

{
   const uint64 numProducePages = CEILING(produceSize, PAGE_SIZE) + 1;
   const uint64 numConsumePages = CEILING(consumeSize, PAGE_SIZE) + 1;
   void *myProduceQ = NULL;
   void *myConsumeQ = NULL;
   int result;
   QPGuestEndpoint *queuePairEntry = NULL;

   /*
    * XXX Check for possible overflow of 'size' arguments when passed to
    * compat_get_order (after some arithmetic ops).
    */

   ASSERT(handle && produceQ && consumeQ && (produceSize || consumeSize));

   if (privFlags != VMCI_NO_PRIVILEGE_FLAGS) {
      return VMCI_ERROR_NO_ACCESS;
   }

   VMCIMutex_Acquire(&qpGuestEndpoints.mutex);

   /* Check if creation/attachment of a queuepair is allowed. */
   if (!VMCI_CanCreate()) {
      result = VMCI_ERROR_UNAVAILABLE;
      goto error;
   }

   if ((Atomic_Read(&qpGuestEndpoints.hibernate) == 1) &&
       !(flags & VMCI_QPFLAG_LOCAL)) {
      /*
       * While guest OS is in hibernate state, creating non-local
       * queue pairs is not allowed after the point where the VMCI
       * guest driver converted the existing queue pairs to local
       * ones.
       */

      result = VMCI_ERROR_UNAVAILABLE;
      goto error;
   }

   if ((queuePairEntry = (QPGuestEndpoint *)
        QueuePairList_FindEntry(&qpGuestEndpoints, *handle))) {
      if (queuePairEntry->qp.flags & VMCI_QPFLAG_LOCAL) {
         /* Local attach case. */
         if (queuePairEntry->qp.refCount > 1) {
            VMCI_DEBUG_LOG(4, (LGPFX"Error attempting to attach more than "
                               "once.\n"));
            result = VMCI_ERROR_UNAVAILABLE;
            goto errorKeepEntry;
         }

         if (queuePairEntry->qp.produceSize != consumeSize ||
             queuePairEntry->qp.consumeSize != produceSize ||
             queuePairEntry->qp.flags != (flags & ~VMCI_QPFLAG_ATTACH_ONLY)) {
            VMCI_DEBUG_LOG(4, (LGPFX"Error mismatched queue pair in local "
                               "attach.\n"));
            result = VMCI_ERROR_QUEUEPAIR_MISMATCH;
            goto errorKeepEntry;
         }

         /*
          * Do a local attach.  We swap the consume and produce queues for the
          * attacher and deliver an attach event.
          */
         result = QueuePairNotifyPeerLocal(TRUE, *handle);
         if (result < VMCI_SUCCESS) {
            goto errorKeepEntry;
         }
         myProduceQ = queuePairEntry->consumeQ;
         myConsumeQ = queuePairEntry->produceQ;
         goto out;
      }
      result = VMCI_ERROR_ALREADY_EXISTS;
      goto errorKeepEntry;
   }

   myProduceQ = VMCI_AllocQueue(produceSize, flags);
   if (!myProduceQ) {
      VMCI_WARNING((LGPFX"Error allocating pages for produce queue.\n"));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   myConsumeQ = VMCI_AllocQueue(consumeSize, flags);
   if (!myConsumeQ) {
      VMCI_WARNING((LGPFX"Error allocating pages for consume queue.\n"));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   queuePairEntry = QPGuestEndpointCreate(*handle, peer, flags,
                                          produceSize, consumeSize,
                                          myProduceQ, myConsumeQ);
   if (!queuePairEntry) {
      VMCI_WARNING((LGPFX"Error allocating memory in %s.\n", __FUNCTION__));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   result = VMCI_AllocPPNSet(myProduceQ, numProducePages, myConsumeQ,
                             numConsumePages, &queuePairEntry->ppnSet);
   if (result < VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"VMCI_AllocPPNSet failed.\n"));
      goto error;
   }

   /*
    * It's only necessary to notify the host if this queue pair will be
    * attached to from another context.
    */
   if (queuePairEntry->qp.flags & VMCI_QPFLAG_LOCAL) {
      /* Local create case. */
      VMCIId contextId = vmci_get_context_id();

      /*
       * Enforce similar checks on local queue pairs as we do for regular ones.
       * The handle's context must match the creator or attacher context id
       * (here they are both the current context id) and the attach-only flag
       * cannot exist during create.  We also ensure specified peer is this
       * context or an invalid one.
       */
      if (queuePairEntry->qp.handle.context != contextId ||
          (queuePairEntry->qp.peer != VMCI_INVALID_ID &&
           queuePairEntry->qp.peer != contextId)) {
         result = VMCI_ERROR_NO_ACCESS;
         goto error;
      }

      if (queuePairEntry->qp.flags & VMCI_QPFLAG_ATTACH_ONLY) {
         result = VMCI_ERROR_NOT_FOUND;
         goto error;
      }
   } else {
      result = VMCIQueuePairAllocHypercall(queuePairEntry);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"VMCIQueuePairAllocHypercall result = %d.\n",
                       result));
         goto error;
      }
   }

   VMCI_InitQueueMutex((VMCIQueue *)myProduceQ, (VMCIQueue *)myConsumeQ);

   QueuePairList_AddEntry(&qpGuestEndpoints, &queuePairEntry->qp);

out:
   queuePairEntry->qp.refCount++;
   *handle = queuePairEntry->qp.handle;
   *produceQ = (VMCIQueue *)myProduceQ;
   *consumeQ = (VMCIQueue *)myConsumeQ;

   /*
    * We should initialize the queue pair header pages on a local queue pair
    * create.  For non-local queue pairs, the hypervisor initializes the header
    * pages in the create step.
    */
   if ((queuePairEntry->qp.flags & VMCI_QPFLAG_LOCAL) &&
       queuePairEntry->qp.refCount == 1) {
      VMCIQueueHeader_Init((*produceQ)->qHeader, *handle);
      VMCIQueueHeader_Init((*consumeQ)->qHeader, *handle);
   }

   VMCIMutex_Release(&qpGuestEndpoints.mutex);

   return VMCI_SUCCESS;

error:
   VMCIMutex_Release(&qpGuestEndpoints.mutex);
   if (queuePairEntry) {
      /* The queues will be freed inside the destroy routine. */
      QPGuestEndpointDestroy(queuePairEntry);
   } else {
      if (myProduceQ) {
         VMCI_FreeQueue(myProduceQ, produceSize);
      }
      if (myConsumeQ) {
         VMCI_FreeQueue(myConsumeQ, consumeSize);
      }
   }
   return result;

errorKeepEntry:
   /* This path should only be used when an existing entry was found. */
   ASSERT(queuePairEntry->qp.refCount > 0);
   VMCIMutex_Release(&qpGuestEndpoints.mutex);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairDetachHypercall --
 *
 *      Helper to make a QueuePairDetach hypercall when the driver is
 *      supporting a guest device.
 *
 * Results:
 *      Result of the hypercall.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQueuePairDetachHypercall(VMCIHandle handle) // IN
{
   VMCIQueuePairDetachMsg detachMsg;

   detachMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                        VMCI_QUEUEPAIR_DETACH);
   detachMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   detachMsg.hdr.payloadSize = sizeof handle;
   detachMsg.handle = handle;

   return VMCI_SendDatagram((VMCIDatagram *)&detachMsg);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairDetachGuestWork --
 *
 *      Helper for VMCI QueuePair detach interface. Frees the physical
 *      pages for the queue pair.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory may be freed.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQueuePairDetachGuestWork(VMCIHandle handle)   // IN
{
   int result;
   QPGuestEndpoint *entry;
   uint32 refCount;

   ASSERT(!VMCI_HANDLE_INVALID(handle));

   VMCIMutex_Acquire(&qpGuestEndpoints.mutex);

   entry = (QPGuestEndpoint *)QueuePairList_FindEntry(&qpGuestEndpoints, handle);
   if (!entry) {
      VMCIMutex_Release(&qpGuestEndpoints.mutex);
      return VMCI_ERROR_NOT_FOUND;
   }

   ASSERT(entry->qp.refCount >= 1);

   if (entry->qp.flags & VMCI_QPFLAG_LOCAL) {
      result = VMCI_SUCCESS;

      if (entry->qp.refCount > 1) {
         result = QueuePairNotifyPeerLocal(FALSE, handle);
         /*
          * We can fail to notify a local queuepair because we can't allocate.
          * We still want to release the entry if that happens, so don't bail
          * out yet.
          */
      }
   } else {
      result = VMCIQueuePairDetachHypercall(handle);
      if (entry->hibernateFailure) {
         if (result == VMCI_ERROR_NOT_FOUND) {
            /*
             * If a queue pair detach failed when entering
             * hibernation, the guest driver and the device may
             * disagree on its existence when coming out of
             * hibernation. The guest driver will regard it as a
             * non-local queue pair, but the device state is gone,
             * since the device has been powered off. In this case, we
             * treat the queue pair as a local queue pair with no
             * peer.
             */

            ASSERT(entry->qp.refCount == 1);
            result = VMCI_SUCCESS;
         }
         if (result == VMCI_SUCCESS) {
            VMCIQPUnmarkHibernateFailed(entry);
         }
      }
      if (result < VMCI_SUCCESS) {
         /*
          * We failed to notify a non-local queuepair.  That other queuepair
          * might still be accessing the shared memory, so don't release the
          * entry yet.  It will get cleaned up by VMCIQueuePair_Exit()
          * if necessary (assuming we are going away, otherwise why did this
          * fail?).
          */

         VMCIMutex_Release(&qpGuestEndpoints.mutex);
         return result;
      }
   }

   /*
    * If we get here then we either failed to notify a local queuepair, or
    * we succeeded in all cases.  Release the entry if required.
    */

   entry->qp.refCount--;
   if (entry->qp.refCount == 0) {
      QueuePairList_RemoveEntry(&qpGuestEndpoints, &entry->qp);
   }

   /* If we didn't remove the entry, this could change once we unlock. */
   refCount = entry ? entry->qp.refCount :
                      0xffffffff; /*
                                   * Value does not matter, silence the
                                   * compiler.
                                   */

   VMCIMutex_Release(&qpGuestEndpoints.mutex);

   if (refCount == 0) {
      QPGuestEndpointDestroy(entry);
   }
   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * QueuePairNotifyPeerLocal --
 *
 *      Dispatches a queue pair event message directly into the local event
 *      queue.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
QueuePairNotifyPeerLocal(Bool attach,           // IN: attach or detach?
                         VMCIHandle handle)     // IN: queue pair handle
{
   VMCIEventMsg *eMsg;
   VMCIEventPayload_QP *ePayload;
   /* buf is only 48 bytes. */
   char buf[sizeof *eMsg + sizeof *ePayload];
   VMCIId contextId = vmci_get_context_id();

   eMsg = (VMCIEventMsg *)buf;
   ePayload = VMCIEventMsgPayload(eMsg);

   eMsg->hdr.dst = VMCI_MAKE_HANDLE(contextId, VMCI_EVENT_HANDLER);
   eMsg->hdr.src = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                    VMCI_CONTEXT_RESOURCE_ID);
   eMsg->hdr.payloadSize = sizeof *eMsg + sizeof *ePayload - sizeof eMsg->hdr;
   eMsg->eventData.event = attach ? VMCI_EVENT_QP_PEER_ATTACH :
                                    VMCI_EVENT_QP_PEER_DETACH;
   ePayload->peerId = contextId;
   ePayload->handle = handle;

   return VMCIEvent_Dispatch((VMCIDatagram *)eMsg);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPMarkHibernateFailed --
 *
 *      Helper function that marks a queue pair entry as not being
 *      converted to a local version during hibernation. Must be
 *      called with the queue pair list mutex held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIQPMarkHibernateFailed(QPGuestEndpoint *entry) // IN
{
   VMCILockFlags flags;
   VMCIHandle handle;

   /*
    * entry->handle is located in paged memory, so it can't be
    * accessed while holding a spinlock.
    */

   handle = entry->qp.handle;
   entry->hibernateFailure = TRUE;
   VMCI_GrabLock_BH(&hibernateFailedListLock, &flags);
   VMCIHandleArray_AppendEntry(&hibernateFailedList, handle);
   VMCI_ReleaseLock_BH(&hibernateFailedListLock, flags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPUnmarkHibernateFailed --
 *
 *      Helper function that removes a queue pair entry from the group
 *      of handles marked as having failed hibernation. Must be called
 *      with the queue pair list lock held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIQPUnmarkHibernateFailed(QPGuestEndpoint *entry) // IN
{
   VMCILockFlags flags;
   VMCIHandle handle;

   /*
    * entry->handle is located in paged memory, so it can't be
    * accessed while holding a spinlock.
    */

   handle = entry->qp.handle;
   entry->hibernateFailure = FALSE;
   VMCI_GrabLock_BH(&hibernateFailedListLock, &flags);
   VMCIHandleArray_RemoveEntry(hibernateFailedList, handle);
   VMCI_ReleaseLock_BH(&hibernateFailedListLock, flags);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMCIQPGuestEndpoints_Convert --
 *
 *      Guest queue pair endpoints may be converted to local ones in
 *      two cases: when entering hibernation or when the device is
 *      powered off before entering a sleep mode. Below we first
 *      discuss the case of hibernation and then the case of entering
 *      sleep state.
 *
 *      When the guest enters hibernation, any non-local queue pairs
 *      will disconnect no later than at the time the VMCI device
 *      powers off. To preserve the content of the non-local queue
 *      pairs for this guest, we make a local copy of the content and
 *      disconnect from the queue pairs. This will ensure that the
 *      peer doesn't continue to update the queue pair state while the
 *      guest OS is checkpointing the memory (otherwise we might end
 *      up with a inconsistent snapshot where the pointers of the
 *      consume queue are checkpointed later than the data pages they
 *      point to, possibly indicating that non-valid data is
 *      valid). While we are in hibernation mode, we block the
 *      allocation of new non-local queue pairs. Note that while we
 *      are doing the conversion to local queue pairs, we are holding
 *      the queue pair list lock, which will prevent concurrent
 *      creation of additional non-local queue pairs.
 *
 *      The hibernation cannot fail, so if we are unable to either
 *      save the queue pair state or detach from a queue pair, we deal
 *      with it by keeping the queue pair around, and converting it to
 *      a local queue pair when going out of hibernation. Since
 *      failing a detach is highly unlikely (it would require a queue
 *      pair being actively used as part of a DMA operation), this is
 *      an acceptable fall back. Once we come back from hibernation,
 *      these queue pairs will no longer be external, so we simply
 *      mark them as local at that point.
 *
 *      For the sleep state, the VMCI device will also be put into the
 *      D3 power state, which may make the device inaccessible to the
 *      guest driver (Windows unmaps the I/O space). When entering
 *      sleep state, the hypervisor is likely to suspend the guest as
 *      well, which will again convert all queue pairs to local ones.
 *      However, VMCI device clients, e.g., VMCI Sockets, may attempt
 *      to use queue pairs after the device has been put into the D3
 *      power state, so we convert the queue pairs to local ones in
 *      that case as well. When exiting the sleep states, the device
 *      has not been reset, so all device state is still in sync with
 *      the device driver, so no further processing is necessary at
 *      that point.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Queue pairs are detached.
 *
 *----------------------------------------------------------------------------
 */

void
VMCIQPGuestEndpoints_Convert(Bool toLocal,     // IN
                             Bool deviceReset) // IN
{
   if (toLocal) {
      VMCIListItem *next;

      VMCIMutex_Acquire(&qpGuestEndpoints.mutex);

      VMCIList_Scan(next, &qpGuestEndpoints.head) {
         QPGuestEndpoint *entry = (QPGuestEndpoint *)VMCIList_Entry(
                                                        next,
                                                        QueuePairEntry,
                                                        listItem);

         if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL)) {
            UNUSED_PARAM(VMCIQueue *prodQ); // Only used on Win32
            UNUSED_PARAM(VMCIQueue *consQ); // Only used on Win32
            void *oldProdQ;
            UNUSED_PARAM(void *oldConsQ); // Only used on Win32
            int result;

            prodQ = (VMCIQueue *)entry->produceQ;
            consQ = (VMCIQueue *)entry->consumeQ;
            oldConsQ = oldProdQ = NULL;

            VMCI_AcquireQueueMutex(prodQ, TRUE);

            result = VMCI_ConvertToLocalQueue(consQ, prodQ,
                                              entry->qp.consumeSize,
                                              TRUE, &oldConsQ);
            if (result != VMCI_SUCCESS) {
               VMCI_WARNING((LGPFX"Hibernate failed to create local consume "
                             "queue from handle %x:%x (error: %d)\n",
                             entry->qp.handle.context, entry->qp.handle.resource,
                             result));
               VMCI_ReleaseQueueMutex(prodQ);
               VMCIQPMarkHibernateFailed(entry);
               continue;
            }
            result = VMCI_ConvertToLocalQueue(prodQ, consQ,
                                              entry->qp.produceSize,
                                              FALSE, &oldProdQ);
            if (result != VMCI_SUCCESS) {
               VMCI_WARNING((LGPFX"Hibernate failed to create local produce "
                             "queue from handle %x:%x (error: %d)\n",
                             entry->qp.handle.context, entry->qp.handle.resource,
                             result));
               VMCI_RevertToNonLocalQueue(consQ, oldConsQ,
                                          entry->qp.consumeSize);
               VMCI_ReleaseQueueMutex(prodQ);
               VMCIQPMarkHibernateFailed(entry);
               continue;
            }

            /*
             * Now that the contents of the queue pair has been saved,
             * we can detach from the non-local queue pair. This will
             * discard the content of the non-local queues.
             */

            result = VMCIQueuePairDetachHypercall(entry->qp.handle);
            if (result < VMCI_SUCCESS) {
               VMCI_WARNING((LGPFX"Hibernate failed to detach from handle "
                             "%x:%x\n",
                             entry->qp.handle.context,
                             entry->qp.handle.resource));
               VMCI_RevertToNonLocalQueue(consQ, oldConsQ,
                                          entry->qp.consumeSize);
               VMCI_RevertToNonLocalQueue(prodQ, oldProdQ,
                                          entry->qp.produceSize);
               VMCI_ReleaseQueueMutex(prodQ);
               VMCIQPMarkHibernateFailed(entry);
               continue;
            }

            entry->qp.flags |= VMCI_QPFLAG_LOCAL;

            VMCI_ReleaseQueueMutex(prodQ);

            VMCI_FreeQueueBuffer(oldProdQ, entry->qp.produceSize);
            VMCI_FreeQueueBuffer(oldConsQ, entry->qp.consumeSize);

            QueuePairNotifyPeerLocal(FALSE, entry->qp.handle);
         }
      }
      Atomic_Write(&qpGuestEndpoints.hibernate, 1);

      VMCIMutex_Release(&qpGuestEndpoints.mutex);
   } else {
      VMCILockFlags flags;
      VMCIHandle handle;

      /*
       * When a guest enters hibernation, there may be queue pairs
       * around, that couldn't be converted to local queue
       * pairs. When coming out of hibernation, these queue pairs
       * will be restored as part of the guest main mem by the OS
       * hibernation code and they can now be regarded as local
       * versions. Since they are no longer connected, detach
       * notifications are sent to the local endpoint.
       */

      VMCI_GrabLock_BH(&hibernateFailedListLock, &flags);
      while (VMCIHandleArray_GetSize(hibernateFailedList) > 0) {
         handle = VMCIHandleArray_RemoveTail(hibernateFailedList);
         if (deviceReset) {
            QueuePairNotifyPeerLocal(FALSE, handle);
         }
      }
      VMCI_ReleaseLock_BH(&hibernateFailedListLock, flags);

      Atomic_Write(&qpGuestEndpoints.hibernate, 0);
   }
}

#endif  /* !VMKERNEL */
