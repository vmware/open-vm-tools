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
#  define EXPORT_SYMTAB
#  include <asm/page.h>
#  include <linux/module.h>
#elif defined(_WIN32)
#  include <wdm.h>
#elif defined(__APPLE__)
#  include <IOKit/IOLib.h>
#endif /* __linux__ */

#include "vm_assert.h"
#include "vmci_kernel_if.h"
#include "vmciQueuePairInt.h"
#include "vmciUtil.h"
#include "vmciInt.h"
#include "vmciEvent.h"
#include "circList.h"

#define LGPFX "VMCIQueuePair: "

typedef struct QueuePairEntry {
   VMCIHandle handle;
   VMCIId     peer;
   uint32     flags;
   uint64     produceSize;
   uint64     consumeSize;
   uint64     numPPNs;
   PPNSet     ppnSet;
   void      *produceQ;
   void      *consumeQ;
   uint32     refCount;
   ListItem   listItem;
} QueuePairEntry;

typedef struct QueuePairList {
   ListItem  *head;
   VMCIMutex mutex;
} QueuePairList;

static QueuePairList queuePairList;

static QueuePairEntry *QueuePairList_FindEntry(VMCIHandle handle);
static void QueuePairList_AddEntry(QueuePairEntry *entry);
static void QueuePairList_RemoveEntry(QueuePairEntry *entry);
static QueuePairEntry *QueuePairList_GetHead(void);
static QueuePairEntry *QueuePairEntryCreate(VMCIHandle handle,
                                            VMCIId peer, uint32 flags,
                                            uint64 produceSize,
                                            uint64 consumeSize,
                                            void *produceQ, void *consumeQ);
static void QueuePairEntryDestroy(QueuePairEntry *entry);
static int VMCIQueuePairAlloc_HyperCall(const QueuePairEntry *entry);
static int VMCIQueuePairAllocHelper(VMCIHandle *handle, VMCIQueue **produceQ,
                                    uint64 produceSize, VMCIQueue **consumeQ,
                                    uint64 consumeSize,
                                    VMCIId peer, uint32 flags);
static int VMCIQueuePairDetachHelper(VMCIHandle handle);
static int QueuePairNotifyPeerLocal(Bool attach, VMCIHandle handle);


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairLock_Init --
 *
 *      Creates the lock protecting the QueuePair list.
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
QueuePairLock_Init(void)
{
   VMCIMutex_Init(&queuePairList.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairLock_Destroy --
 *
 *      Destroys the lock protecting the QueuePair list.
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
QueuePairLock_Destroy(void)
{
   VMCIMutex_Destroy(&queuePairList.mutex); /* No-op on Linux and Windows. */
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Lock --
 *
 *      Acquires the lock protecting the QueuePair list.
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
QueuePairList_Lock(void)
{
   VMCIMutex_Acquire(&queuePairList.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Unlock --
 *
 *      Releases the lock protecting the QueuePair list.
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
QueuePairList_Unlock(void)
{
   VMCIMutex_Release(&queuePairList.mutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Init --
 *
 *      Initalizes QueuePair data structure state.
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
VMCIQueuePair_Init(void)
{
   queuePairList.head = NULL;
   QueuePairLock_Init();
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Exit --
 *
 *      Destroys all QueuePairs. Makes hypercalls to detach from QueuePairs.
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
VMCIQueuePair_Exit(void)
{
   QueuePairEntry *entry;

   QueuePairList_Lock();

   while ((entry = QueuePairList_GetHead())) {
      /*
       * Don't make a hypercall for local QueuePairs.
       */
      if (!(entry->flags & VMCI_QPFLAG_LOCAL)) {
         VMCIQueuePairDetachMsg detachMsg;

         detachMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                              VMCI_QUEUEPAIR_DETACH);
         detachMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
         detachMsg.hdr.payloadSize = sizeof entry->handle;
         detachMsg.handle = entry->handle;
         
         (void)VMCI_SendDatagram((VMCIDatagram *)&detachMsg);
      }
      /*
       * We cannot fail the exit, so let's reset refCount.
       */
      entry->refCount = 0;
      QueuePairList_RemoveEntry(entry);
      QueuePairEntryDestroy(entry);
   }

   QueuePairList_Unlock();
   QueuePairLock_Destroy();
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
QueuePairList_FindEntry(VMCIHandle handle) // IN:
{
   ListItem *next;

   if (VMCI_HANDLE_INVALID(handle)) {
      return NULL;
   }

   LIST_SCAN(next, queuePairList.head) {
      QueuePairEntry *entry = LIST_CONTAINER(next, QueuePairEntry, listItem);

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
QueuePairList_AddEntry(QueuePairEntry *entry) // IN:
{
   if (entry) {
      LIST_QUEUE(&entry->listItem, &queuePairList.head);
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
QueuePairList_RemoveEntry(QueuePairEntry *entry) // IN:
{
   if (entry) {
      LIST_DEL(&entry->listItem, &queuePairList.head);
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
QueuePairList_GetHead(void)
{
   ListItem *first = LIST_FIRST(queuePairList.head);

   if (first) {
      QueuePairEntry *entry = LIST_CONTAINER(first, QueuePairEntry, listItem);
      return entry;
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_Alloc --
 *
 *      Allocates a VMCI QueuePair. Only checks validity of input arguments.
 *      Real work is done in the OS-specific helper routine.
 *
 * Results:
 *      Success or failure.
 *
 * Side effects:
 *      Memory is allocated.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIQueuePair_Alloc);
#endif

int
VMCIQueuePair_Alloc(VMCIHandle *handle,     // IN/OUT:
                    VMCIQueue  **produceQ,  // OUT:
                    uint64     produceSize, // IN:
                    VMCIQueue  **consumeQ,  // OUT:
                    uint64     consumeSize, // IN:
                    VMCIId     peer,        // IN:
                    uint32     flags)       // IN:
{
   ASSERT_ON_COMPILE(sizeof(VMCIQueueHeader) <= PAGE_SIZE);

   return VMCIQueuePair_AllocPriv(handle, produceQ, produceSize, consumeQ, consumeSize, peer, flags, VMCI_NO_PRIVILEGE_FLAGS);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueuePair_AllocPriv --
 *
 *      Provided for compatibility with the host API. Always returns an error
 *      since requesting privileges from the guest is not allowed. Use
 *      VMCIQueuePair_Alloc instead.
 *
 * Results:
 *      An error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef __linux__
EXPORT_SYMBOL(VMCIQueuePair_AllocPriv);
#endif

int
VMCIQueuePair_AllocPriv(VMCIHandle *handle,           // IN/OUT:
                        VMCIQueue  **produceQ,        // OUT:
                        uint64     produceSize,       // IN:
                        VMCIQueue  **consumeQ,        // OUT:
                        uint64     consumeSize,       // IN:
                        VMCIId     peer,              // IN:
                        uint32     flags,             // IN:
                        VMCIPrivilegeFlags privFlags) // IN:
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

#ifdef __linux__
EXPORT_SYMBOL(VMCIQueuePair_Detach);
#endif

int
VMCIQueuePair_Detach(VMCIHandle handle) // IN:
{
   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
   return VMCIQueuePairDetachHelper(handle);
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairEntryCreate --
 *
 *      Allocates and initializes a QueuePairEntry structure.  Allocates a
 *      QueuePair rid (and handle) iff the given entry has an invalid handle.
 *      0 through VMCI_RESERVED_RESOURCE_ID_MAX are reserved handles.  Assumes
 *      that the QP list lock is held by the caller.
 *
 * Results:
 *      Pointer to structure intialized.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

QueuePairEntry *
QueuePairEntryCreate(VMCIHandle handle,  // IN:
                     VMCIId peer,        // IN:
                     uint32 flags,       // IN:
                     uint64 produceSize, // IN:
                     uint64 consumeSize, // IN:
                     void *produceQ,     // IN:
                     void *consumeQ)     // IN:
{
   static VMCIId queuePairRID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
   QueuePairEntry *entry;
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
         entry = QueuePairList_FindEntry(handle);
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
          QueuePairList_FindEntry(handle) == NULL);
   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NORMAL);
   if (entry) {
      entry->handle = handle;
      entry->peer = peer;
      entry->flags = flags;
      entry->produceSize = produceSize;
      entry->consumeSize = consumeSize;
      entry->numPPNs = numPPNs;
      memset(&entry->ppnSet, 0, sizeof entry->ppnSet);
      entry->produceQ = produceQ;
      entry->consumeQ = consumeQ;
      entry->refCount = 0;
      INIT_LIST_ITEM(&entry->listItem);
   }
   return entry;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairEntryDestroy --
 *
 *      Frees a QueuePairEntry structure.
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
QueuePairEntryDestroy(QueuePairEntry *entry) // IN:
{
   ASSERT(entry);
   ASSERT(entry->refCount == 0);

   VMCI_FreePPNSet(&entry->ppnSet);
   VMCI_FreeQueue(entry->produceQ, entry->produceSize);
   VMCI_FreeQueue(entry->consumeQ, entry->consumeSize);
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
VMCIQueuePairAlloc_HyperCall(const QueuePairEntry *entry) // IN:
{
   VMCIQueuePairAllocMsg *allocMsg;
   size_t msgSize;
   int result;

   if (!entry || entry->numPPNs <= 2) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   ASSERT(!(entry->flags & VMCI_QPFLAG_LOCAL));

   msgSize = sizeof *allocMsg + (size_t)entry->numPPNs * sizeof(PPN);
   allocMsg = VMCI_AllocKernelMem(msgSize, VMCI_MEMORY_NONPAGED);
   if (!allocMsg) {
      return VMCI_ERROR_NO_MEM;
   }

   allocMsg->hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
					VMCI_QUEUEPAIR_ALLOC);
   allocMsg->hdr.src = VMCI_ANON_SRC_HANDLE;
   allocMsg->hdr.payloadSize = msgSize - VMCI_DG_HEADERSIZE;
   allocMsg->handle = entry->handle;
   allocMsg->peer = entry->peer;
   allocMsg->flags = entry->flags;
   allocMsg->produceSize = entry->produceSize;
   allocMsg->consumeSize = entry->consumeSize;
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
VMCIQueuePairAllocHelper(VMCIHandle *handle,   // IN/OUT:
                         VMCIQueue **produceQ, // OUT:
                         uint64 produceSize,   // IN:
                         VMCIQueue **consumeQ, // OUT:
                         uint64 consumeSize,   // IN:
                         VMCIId peer,          // IN:
                         uint32 flags)         // IN:
{
   const uint64 numProducePages = CEILING(produceSize, PAGE_SIZE) + 1;
   const uint64 numConsumePages = CEILING(consumeSize, PAGE_SIZE) + 1;
   void *myProduceQ = NULL;
   void *myConsumeQ = NULL;
   int result;
   QueuePairEntry *queuePairEntry = NULL;

   /*
    * XXX Check for possible overflow of 'size' arguments when passed to
    * compat_get_order (after some arithmetic ops).
    */

   ASSERT(handle && produceQ && consumeQ && (produceSize || consumeSize));

   QueuePairList_Lock();

   if ((queuePairEntry = QueuePairList_FindEntry(*handle))) {
      if (queuePairEntry->flags & VMCI_QPFLAG_LOCAL) {
         /* Local attach case. */
         if (queuePairEntry->refCount > 1) {
            VMCI_LOG((LGPFX "Error attempting to attach more than once.\n"));
            result = VMCI_ERROR_UNAVAILABLE;
            goto errorKeepEntry;
         }

         if (queuePairEntry->produceSize != consumeSize ||
             queuePairEntry->consumeSize != produceSize ||
             queuePairEntry->flags != (flags & ~VMCI_QPFLAG_ATTACH_ONLY)) {
            VMCI_LOG((LGPFX "Error mismatched queue pair in local attach.\n"));
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
      VMCI_LOG((LGPFX "Error allocating pages for produce queue.\n"));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   myConsumeQ = VMCI_AllocQueue(consumeSize);
   if (!myConsumeQ) {
      VMCI_LOG((LGPFX "Error allocating pages for consume queue.\n"));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   queuePairEntry = QueuePairEntryCreate(*handle, peer, flags,
                                         produceSize, consumeSize,
                                         myProduceQ, myConsumeQ);
   if (!queuePairEntry) {
      VMCI_LOG((LGPFX "Error allocating memory in %s.\n", __FUNCTION__));
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   result = VMCI_AllocPPNSet(myProduceQ, numProducePages, myConsumeQ,
                             numConsumePages, &queuePairEntry->ppnSet);
   if (result < VMCI_SUCCESS) {
      VMCI_LOG((LGPFX "VMCI_AllocPPNSet failed.\n"));
      goto error;
   }

   /*
    * It's only necessary to notify the host if this queue pair will be
    * attached to from another context.
    */
   if (queuePairEntry->flags & VMCI_QPFLAG_LOCAL) {
      /* Local create case. */
      VMCIId contextId = VMCI_GetContextID();

      /*
       * Enforce similar checks on local queue pairs as we do for regular ones.
       * The handle's context must match the creator or attacher context id
       * (here they are both the current context id) and the attach-only flag
       * cannot exist during create.  We also ensure specified peer is this
       * context or an invalid one.
       */
      if (queuePairEntry->handle.context != contextId ||
          (queuePairEntry->peer != VMCI_INVALID_ID &&
           queuePairEntry->peer != contextId)) {
         result = VMCI_ERROR_NO_ACCESS;
         goto error;
      }

      if (queuePairEntry->flags & VMCI_QPFLAG_ATTACH_ONLY) {
         result = VMCI_ERROR_NOT_FOUND;
         goto error;
      }
   } else {
      result = VMCIQueuePairAlloc_HyperCall(queuePairEntry);
      if (result < VMCI_SUCCESS) {
         VMCI_LOG((LGPFX "VMCIQueuePairAlloc_HyperCall result = %d.\n",
                   result));
         goto error;
      }
   }

   QueuePairList_AddEntry(queuePairEntry);

out:
   queuePairEntry->refCount++;
   *handle = queuePairEntry->handle;
   *produceQ = (VMCIQueue *)myProduceQ;
   *consumeQ = (VMCIQueue *)myConsumeQ;

   /*
    * We should initialize the queue pair header pages on a local queue pair
    * create.  For non-local queue pairs, the hypervisor initializes the header
    * pages in the create step.
    */
   if ((queuePairEntry->flags & VMCI_QPFLAG_LOCAL) &&
       queuePairEntry->refCount == 1) {
      VMCIQueueHeader_Init((*produceQ)->qHeader, *handle);
      VMCIQueueHeader_Init((*consumeQ)->qHeader, *handle);
   }

   QueuePairList_Unlock();

   return VMCI_SUCCESS;

error:
   QueuePairList_Unlock();
   if (queuePairEntry) {
      /* The queues will be freed inside the destroy routine. */
      QueuePairEntryDestroy(queuePairEntry);
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
   ASSERT(queuePairEntry->refCount > 0);
   QueuePairList_Unlock();
   return result;
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
VMCIQueuePairDetachHelper(VMCIHandle handle)   // IN:
{
   int result;
   QueuePairEntry *entry;
   uint32 refCount;

   ASSERT(!VMCI_HANDLE_INVALID(handle));

   QueuePairList_Lock();

   entry = QueuePairList_FindEntry(handle);
   if (!entry) {
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   ASSERT(entry->refCount >= 1);

   if (entry->flags & VMCI_QPFLAG_LOCAL) {
      result = VMCI_SUCCESS;

      if (entry->refCount > 1) {
         result = QueuePairNotifyPeerLocal(FALSE, handle);
         if (result < VMCI_SUCCESS) {
            goto out;
         }
      }
   } else {
      VMCIQueuePairDetachMsg detachMsg;

      detachMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                           VMCI_QUEUEPAIR_DETACH);
      detachMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
      detachMsg.hdr.payloadSize = sizeof handle;
      detachMsg.handle = handle;

      result = VMCI_SendDatagram((VMCIDatagram *)&detachMsg);
   }

out:
   if (result >= VMCI_SUCCESS) {
      entry->refCount--;

      if (entry->refCount == 0) {
         QueuePairList_RemoveEntry(entry);
      }
   }

   /* If we didn't remove the entry, this could change once we unlock. */
   refCount = entry ? entry->refCount :
                      0xffffffff; /* 
                                   * Value does not matter, silence the
                                   * compiler.
                                   */
                                       

   QueuePairList_Unlock();

   if (result >= VMCI_SUCCESS && refCount == 0) {
      QueuePairEntryDestroy(entry);
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
