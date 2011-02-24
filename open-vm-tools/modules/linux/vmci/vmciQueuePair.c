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
 * vmciQueuePair.c --
 *
 *     Implements the VMCI QueuePair API.
 */

#ifdef __linux__
#  include "driver-config.h"
#  include <asm/page.h>
#  include <linux/module.h>
#elif defined(_WIN32)
#  include <wdm.h>
#elif defined(__APPLE__)
#  include <IOKit/IOLib.h>
#endif /* __linux__ */

#include "vm_assert.h"
#include "vm_atomic.h"
#include "vmci_handle_array.h"
#include "vmci_kernel_if.h"
#include "vmciEvent.h"
#include "vmciInt.h"
#include "vmciKernelAPI.h"
#include "vmciQueuePairInt.h"
#include "vmciUtil.h"

#define LGPFX "VMCIQueuePair: "

typedef struct QueuePairEntry {
   VMCIListItem  listItem;
   VMCIHandle    handle;
   VMCIId        peer;
   uint32        flags;
   uint64        produceSize;
   uint64        consumeSize;
   uint32        refCount;
} QueuePairEntry;

typedef struct QPGuestEndpoint {
   QueuePairEntry qp;
   uint64         numPPNs;
   void          *produceQ;
   void          *consumeQ;
   Bool           hibernateFailure;
   PPNSet         ppnSet;
} QPGuestEndpoint;

typedef struct QueuePairList {
   VMCIList       head;
   Atomic_uint32  hibernate;
   VMCIMutex      lock;
} QueuePairList;

static QueuePairList qpGuestEndpoints;
static VMCIHandleArray *hibernateFailedList;
static VMCILock hibernateFailedListLock;

static QueuePairEntry *QueuePairList_FindEntry(QueuePairList *qpList,
                                               VMCIHandle handle);
static void QueuePairList_AddEntry(QueuePairList *qpList,
                                   QueuePairEntry *entry);
static void QueuePairList_RemoveEntry(QueuePairList *qpList,
                                      QueuePairEntry *entry);
static QueuePairEntry *QueuePairList_GetHead(QueuePairList *qpList);
static QPGuestEndpoint *QPGuestEndpointCreate(VMCIHandle handle,
                                            VMCIId peer, uint32 flags,
                                            uint64 produceSize,
                                            uint64 consumeSize,
                                            void *produceQ, void *consumeQ);
static void QPGuestEndpointDestroy(QPGuestEndpoint *entry);
static int VMCIQueuePairAlloc_HyperCall(const QPGuestEndpoint *entry);
static int VMCIQueuePairAllocHelper(VMCIHandle *handle, VMCIQueue **produceQ,
                                    uint64 produceSize, VMCIQueue **consumeQ,
                                    uint64 consumeSize,
                                    VMCIId peer, uint32 flags);
static int VMCIQueuePairDetachHelper(VMCIHandle handle);
static int VMCIQueuePairDetachHyperCall(VMCIHandle handle);
static int QueuePairNotifyPeerLocal(Bool attach, VMCIHandle handle);
static void VMCIQPMarkHibernateFailed(QPGuestEndpoint *entry);
static void VMCIQPUnmarkHibernateFailed(QPGuestEndpoint *entry);


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Init --
 *
 *      Initializes a queue pair list.
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
QueuePairList_Init(QueuePairList *qpList)
{
   int ret;

   VMCIList_Init(&qpList->head);
   Atomic_Write(&qpList->hibernate, 0);
   ret = VMCIMutex_Init(&qpList->lock);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Destroy --
 *
 *      Cleans up state queue pair list state created by
 *      QueuePairList_Init. It destroys the lock protecting the
 *      QueuePair list.
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
   VMCIList_Init(&qpList->head);
   VMCIMutex_Destroy(&qpList->lock); /* No-op on Linux and Windows. */
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPGuestEndpoints_Init --
 *
 *      Initalizes data structure state keeping track of queue pair
 *      guest endpoints.
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
VMCIQPGuestEndpoints_Init(void)
{
   QueuePairList_Init(&qpGuestEndpoints);
   hibernateFailedList = VMCIHandleArray_Create(0);

   /*
    * The lock rank must be lower than subscriberLock in vmciEvent,
    * since we hold the hibernateFailedListLock while generating
    * detach events.
    */

   VMCI_InitLock(&hibernateFailedListLock,
                 "VMCIQPHibernateFailed",
                 VMCI_LOCK_RANK_MIDDLE_BH);
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

   VMCIMutex_Acquire(&qpGuestEndpoints.lock);

   while ((entry = (QPGuestEndpoint *)QueuePairList_GetHead(&qpGuestEndpoints))) {
      /*
       * Don't make a hypercall for local QueuePairs.
       */
      if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL)) {
         VMCIQueuePairDetachMsg detachMsg;

         detachMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                              VMCI_QUEUEPAIR_DETACH);
         detachMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
         detachMsg.hdr.payloadSize = sizeof entry->qp.handle;
         detachMsg.handle = entry->qp.handle;

         (void)VMCI_SendDatagram((VMCIDatagram *)&detachMsg);
      }
      /*
       * We cannot fail the exit, so let's reset refCount.
       */
      entry->qp.refCount = 0;
      QueuePairList_RemoveEntry(&qpGuestEndpoints, &entry->qp);
      QPGuestEndpointDestroy(entry);
   }

   Atomic_Write(&qpGuestEndpoints.hibernate, 0);
   VMCIMutex_Release(&qpGuestEndpoints.lock);
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
   VMCIMutex_Acquire(&qpGuestEndpoints.lock);
   VMCIMutex_Release(&qpGuestEndpoints.lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_FindEntry --
 *
 *    Searches the list of QueuePairs to find if an entry already exists.
 *    Assumes that the lock on the list is held.
 *
 * Results:
 *    Pointer to the entry if it exists, NULL otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static QueuePairEntry *
QueuePairList_FindEntry(QueuePairList *qpList, // IN
                        VMCIHandle handle)     // IN
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
 *    Appends a QueuePair entry to the list. Assumes that the lock on the
 *    list is held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
QueuePairList_AddEntry(QueuePairList *qpList, // IN
                       QueuePairEntry *entry) // IN
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
 *    Removes a QueuePair entry from the list. Assumes that the lock on the
 *    list is held.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
QueuePairList_RemoveEntry(QueuePairList *qpList, // IN
                          QueuePairEntry *entry) // IN
{
   if (entry) {
      VMCIList_Remove(&entry->listItem, &qpList->head);
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
QueuePairList_GetHead(QueuePairList *qpList) // IN
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
 * VMCIQueuePair_Alloc --
 *
 *      Allocates a VMCI QueuePair. Only checks validity of input
 *      arguments.  Real work is done in the OS-specific helper
 *      routine. The privilege flags argument is present to provide
 *      compatibility with the host API; anything other than
 *      VMCI_NO_PRIVILEGE_FLAGS will result in the error
 *      VMCI_ERROR_NO_ACCESS, since requesting privileges from the
 *      guest is not allowed.
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
                    VMCIPrivilegeFlags privFlags) // IN
{
   if (privFlags != VMCI_NO_PRIVILEGE_FLAGS) {
      return VMCI_ERROR_NO_ACCESS;
   }

   if (!handle || !produceQ || !consumeQ || (!produceSize && !consumeSize) ||
       (flags & ~VMCI_QP_ALL_FLAGS)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   return VMCIQueuePairAllocHelper(handle, produceQ, produceSize, consumeQ,
                                   consumeSize, peer, flags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Detach --
 *
 *      Detaches from a VMCI QueuePair. Only checks validity of input argument.
 *      Real work is done in the OS-specific helper routine.
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
VMCIQueuePair_Detach(VMCIHandle handle) // IN
{
   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
   return VMCIQueuePairDetachHelper(handle);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QPGuestEndpointCreate --
 *
 *      Allocates and initializes a QPGuestEndpoint structure.
 *      Allocates a QueuePair rid (and handle) iff the given entry has
 *      an invalid handle.  0 through VMCI_RESERVED_RESOURCE_ID_MAX
 *      are reserved handles.  Assumes that the QP list lock is held
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
      VMCIId contextID = VMCI_GetContextID();
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
   VMCI_FreeQueue(entry->produceQ, entry->qp.produceSize);
   VMCI_FreeQueue(entry->consumeQ, entry->qp.consumeSize);
   VMCI_FreeKernelMem(entry, sizeof *entry);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairAlloc_HyperCall --
 *
 *      Helper to make a QueuePairAlloc hypercall.
 *
 * Results:
 *      Result of the hypercall.
 *
 * Side effects:
 *      Memory is allocated & freed.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIQueuePairAlloc_HyperCall(const QPGuestEndpoint *entry) // IN
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
 * VMCIQueuePairAllocHelper --
 *
 *      Helper for VMCI QueuePairAlloc. Allocates physical pages for the
 *      QueuePair. Makes OS dependent calls through generic wrappers.
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
VMCIQueuePairAllocHelper(VMCIHandle *handle,   // IN/OUT
                         VMCIQueue **produceQ, // OUT
                         uint64 produceSize,   // IN
                         VMCIQueue **consumeQ, // OUT
                         uint64 consumeSize,   // IN
                         VMCIId peer,          // IN
                         uint32 flags)         // IN
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

   VMCIMutex_Acquire(&qpGuestEndpoints.lock);

   /* Do not allow alloc/attach if the device is being shutdown. */
   if (VMCI_DeviceShutdown()) {
      result = VMCI_ERROR_DEVICE_NOT_FOUND;
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

   if ((queuePairEntry = (QPGuestEndpoint *)QueuePairList_FindEntry(
                                               &qpGuestEndpoints, *handle))) {
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

   myProduceQ = VMCI_AllocQueue(produceSize);
   if (!myProduceQ) {
      VMCI_WARNING((LGPFX"Error allocating pages for produce queue.\n"));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   myConsumeQ = VMCI_AllocQueue(consumeSize);
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
      VMCIId contextId = VMCI_GetContextID();

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
      result = VMCIQueuePairAlloc_HyperCall(queuePairEntry);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"VMCIQueuePairAlloc_HyperCall result = %d.\n",
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

   VMCIMutex_Release(&qpGuestEndpoints.lock);

   return VMCI_SUCCESS;

error:
   VMCIMutex_Release(&qpGuestEndpoints.lock);
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
   VMCIMutex_Release(&qpGuestEndpoints.lock);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePairDetachHyperCall --
 *
 *      Helper to make a QueuePairDetach hypercall.
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
VMCIQueuePairDetachHyperCall(VMCIHandle handle) // IN
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
 * VMCIQueuePairDetachHelper --
 *
 *      Helper for VMCI QueuePair detach interface on Linux. Frees the physical
 *      pages for the QueuePair.
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
VMCIQueuePairDetachHelper(VMCIHandle handle)   // IN
{
   int result;
   QPGuestEndpoint *entry;
   uint32 refCount;

   ASSERT(!VMCI_HANDLE_INVALID(handle));

   VMCIMutex_Acquire(&qpGuestEndpoints.lock);

   entry = (QPGuestEndpoint *)QueuePairList_FindEntry(&qpGuestEndpoints, handle);
   if (!entry) {
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   ASSERT(entry->qp.refCount >= 1);

   if (entry->qp.flags & VMCI_QPFLAG_LOCAL) {
      result = VMCI_SUCCESS;

      if (entry->qp.refCount > 1) {
         result = QueuePairNotifyPeerLocal(FALSE, handle);
         if (result < VMCI_SUCCESS) {
            goto out;
         }
      }
   } else {
      result = VMCIQueuePairDetachHyperCall(handle);
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
   }

out:
   if (result >= VMCI_SUCCESS) {
      entry->qp.refCount--;

      if (entry->qp.refCount == 0) {
         QueuePairList_RemoveEntry(&qpGuestEndpoints, &entry->qp);
      }
   }

   /* If we didn't remove the entry, this could change once we unlock. */
   refCount = entry ? entry->qp.refCount :
                      0xffffffff; /*
                                   * Value does not matter, silence the
                                   * compiler.
                                   */

   VMCIMutex_Release(&qpGuestEndpoints.lock);

   if (result >= VMCI_SUCCESS && refCount == 0) {
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
   VMCIId contextId;

   contextId = VMCI_GetContextID();

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
 *      called with the queue pair list lock held.
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

      VMCIMutex_Acquire(&qpGuestEndpoints.lock);

      VMCIList_Scan(next, &qpGuestEndpoints.head) {
         QPGuestEndpoint *entry = (QPGuestEndpoint *)VMCIList_Entry(
                                                        next,
                                                        QueuePairEntry,
                                                        listItem);

         if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL)) {
            VMCIQueue *prodQ;
            VMCIQueue *consQ;
            void *oldProdQ;
            void *oldConsQ;
            int result;

            prodQ = (VMCIQueue *)entry->produceQ;
            consQ = (VMCIQueue *)entry->consumeQ;
            oldConsQ = oldProdQ = NULL;

            VMCI_AcquireQueueMutex(prodQ);

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

            result = VMCIQueuePairDetachHyperCall(entry->qp.handle);
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

      VMCIMutex_Release(&qpGuestEndpoints.lock);
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
