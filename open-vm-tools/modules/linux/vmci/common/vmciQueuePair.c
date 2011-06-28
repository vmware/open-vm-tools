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
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIQueuePair: "

/*
 * The context that creates the QueuePair becomes producer of produce queue,
 * and consumer of consume queue. The context on other end for the QueuePair
 * has roles reversed for these two queues.
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
   Bool                 pageStoreSet;
   Bool                 allowAttach;
   Bool                 requireTrustedAttach;
   Bool                 createdByTrusted;
#ifdef VMKERNEL
   QueuePairPageStore   store;
#elif defined(__linux__) || defined(_WIN32) || defined(__APPLE__) || \
      defined(SOLARIS)
   /*
    * Always created but only used if a host endpoint attaches to this
    * queue.
    */

   VMCIQueue           *produceQ;
   VMCIQueue           *consumeQ;
   char                 producePageFile[VMCI_PATH_MAX];
   char                 consumePageFile[VMCI_PATH_MAX];
   PageStoreAttachInfo *attachInfo;
#endif
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

#ifdef VMKERNEL
typedef VMCILock VMCIQPLock;
# define VMCIQPLock_Init(_l, _r) \
   _r = VMCI_InitLock(_l, "VMCIQPLock", VMCI_LOCK_RANK_HIGH)
# define VMCIQPLock_Destroy(_l)  VMCI_CleanupLock(_l)
# define VMCIQPLock_Acquire(_l)  VMCI_GrabLock(_l, NULL)
# define VMCIQPLock_Release(_l)  VMCI_ReleaseLock(_l, 0)
#else
typedef VMCIMutex VMCIQPLock;
# define VMCIQPLock_Init(_l, _r) _r = VMCIMutex_Init(_l)
# define VMCIQPLock_Destroy(_l)  VMCIMutex_Destroy(_l)
# define VMCIQPLock_Acquire(_l)  VMCIMutex_Acquire(_l)
# define VMCIQPLock_Release(_l)  VMCIMutex_Release(_l)
#endif

typedef struct QueuePairList {
   VMCIList       head;
   Atomic_uint32  hibernate;
   VMCIQPLock     lock;
} QueuePairList;

static QueuePairList qpBrokerList;

#if !defined(VMKERNEL)
  static QueuePairList qpGuestEndpoints;
  static VMCIHandleArray *hibernateFailedList;
  static VMCILock hibernateFailedListLock;
#endif

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
                                QPBrokerEntry **ent);

#if !defined(VMKERNEL)
static int VMCIQueuePairAllocHostWork(VMCIHandle *handle, VMCIQueue **produceQ,
                                      uint64 produceSize, VMCIQueue **consumeQ,
                                      uint64 consumeSize,
                                      VMCIId peer, uint32 flags,
                                      VMCIPrivilegeFlags privFlags);
static int VMCIQueuePairDetachHostWork(VMCIHandle handle);

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
                    Bool       guestEndpoint)     // IN
{
   if (!handle || !produceQ || !consumeQ || (!produceSize && !consumeSize) ||
       (flags & ~VMCI_QP_ALL_FLAGS)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (guestEndpoint) {
      return VMCIQueuePairAllocGuestWork(handle, produceQ, produceSize, consumeQ,
                                         consumeSize, peer, flags, privFlags);
   } else {
      return VMCIQueuePairAllocHostWork(handle, produceQ, produceSize, consumeQ,
                                        consumeSize, peer, flags, privFlags);
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
      return VMCIQueuePairDetachGuestWork(handle);
   } else {
      return VMCIQueuePairDetachHostWork(handle);
   }
}
#endif // !defined(VMKERNEL)


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
   VMCIQPLock_Init(&qpList->lock, ret);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * QueuePairList_Destroy --
 *
 *      Destroy the list's lock.
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
   VMCIQPLock_Destroy(&qpList->lock);
   VMCIList_Init(&qpList->head);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Lock --
 *
 *      Acquires the lock protecting a VMCI queue pair broker transaction.
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
VMCIQPBroker_Lock(void)
{
   VMCIQPLock_Acquire(&qpBrokerList.lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Unlock --
 *
 *      Releases the lock protecting a VMCI queue pair broker transaction.
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
VMCIQPBroker_Unlock(void)
{
   VMCIQPLock_Release(&qpBrokerList.lock);
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
 * QueuePairDenyConnection --
 *
 *      On ESX we check if the domain names of the two contexts match.
 *      Otherwise we deny the connection.  We always allow the connection on
 *      hosted.
 *
 * Results:
 *      Boolean result.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
QueuePairDenyConnection(VMCIId contextId, // IN:  Unused on hosted
                        VMCIId peerId)    // IN:  Unused on hosted
{
#ifndef VMKERNEL
   return FALSE; /* Allow on hosted. */
#else
   char contextDomain[VMCI_DOMAIN_NAME_MAXLEN];
   char peerDomain[VMCI_DOMAIN_NAME_MAXLEN];

   ASSERT(contextId != VMCI_INVALID_ID);
   if (peerId == VMCI_INVALID_ID) {
      return FALSE; /* Allow. */
   }
   if (VMCIContext_GetDomainName(contextId, contextDomain,
                                 sizeof contextDomain) != VMCI_SUCCESS) {
      return TRUE; /* Deny. */
   }
   if (VMCIContext_GetDomainName(peerId, peerDomain, sizeof peerDomain) !=
       VMCI_SUCCESS) {
      return TRUE; /* Deny. */
   }
   return strcmp(contextDomain, peerDomain) ? TRUE : /* Deny. */
                                              FALSE; /* Allow. */
#endif
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

   VMCIQPBroker_Lock();

   while ((entry = (QPBrokerEntry *)QueuePairList_GetHead(&qpBrokerList))) {
      QueuePairList_RemoveEntry(&qpBrokerList, &entry->qp);
      VMCI_FreeKernelMem(entry, sizeof *entry);
   }

   VMCIQPBroker_Unlock();
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
                               pageStore, context, NULL);
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
                     QPBrokerEntry **ent)           // OUT
{
   QPBrokerEntry *entry = NULL;
   int result;
   const VMCIId contextId = VMCIContext_GetId(context);
   Bool isLocal = flags & VMCI_QPFLAG_LOCAL;

   if (VMCI_HANDLE_INVALID(handle) ||
       (flags & ~VMCI_QP_ALL_FLAGS) ||
       (isLocal && (!vmkernel || contextId != VMCI_HOST_CONTEXT_ID ||
                     handle.context != contextId)) ||
       !(produceSize || consumeSize) ||
       !context || contextId == VMCI_INVALID_ID ||
       handle.context == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

#ifdef VMKERNEL
   if (!pageStore || (!pageStore->shared && !isLocal)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
#else
   /*
    * On hosted, pageStore can be NULL if the caller doesn't want the
    * information
    */
   if (pageStore && !VMCI_QP_PAGESTORE_IS_WELLFORMED(pageStore)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
#endif // VMKERNEL


   /*
    * In the initial argument check, we ensure that non-vmkernel hosts
    * are not allowed to create local queue pairs.
    */

   ASSERT(vmkernel || !isLocal);

   if (!isLocal && VMCIHandleArray_HasEntry(context->queuePairArray, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) already attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_ALREADY_EXISTS;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) { /* Create case. */
      /*
       * Do not create if the caller asked not to.
       */

      if (flags & VMCI_QPFLAG_ATTACH_ONLY) {
         result = VMCI_ERROR_NOT_FOUND;
         goto out;
      }

      /*
       * Creator's context ID should match handle's context ID or the creator
       * must allow the context in handle's context ID as the "peer".
       */

      if (handle.context != contextId && handle.context != peer) {
         result = VMCI_ERROR_NO_ACCESS;
         goto out;
      }

      /*
       * Check if we should allow this QueuePair connection.
       */

      if (QueuePairDenyConnection(contextId, peer)) {
         result = VMCI_ERROR_NO_ACCESS;
         goto out;
      }

      entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_ATOMIC);
      if (!entry) {
         result = VMCI_ERROR_NO_MEM;
         goto out;
      }

      memset(entry, 0, sizeof *entry);
      entry->qp.handle = handle;
      entry->qp.peer = peer;
      entry->qp.flags = flags;
      entry->qp.produceSize = produceSize;
      entry->qp.consumeSize = consumeSize;
      entry->qp.refCount = 1;
      entry->createId = contextId;
      entry->attachId = VMCI_INVALID_ID;
      entry->pageStoreSet = FALSE;
      entry->allowAttach = TRUE;
      entry->requireTrustedAttach =
         (context->privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED) ? TRUE : FALSE;
      entry->createdByTrusted =
         (privFlags & VMCI_PRIVILEGE_FLAG_TRUSTED) ? TRUE : FALSE;

#ifndef VMKERNEL
      {
         uint64 numProducePages;
         uint64 numConsumePages;

         entry->produceQ = VMCIHost_AllocQueue(produceSize);
         if (entry->produceQ == NULL) {
            result = VMCI_ERROR_NO_MEM;
            goto errorDealloc;
         }

         entry->consumeQ = VMCIHost_AllocQueue(consumeSize);
         if (entry->consumeQ == NULL) {
            result = VMCI_ERROR_NO_MEM;
            goto errorDealloc;
         }

         entry->attachInfo = VMCI_AllocKernelMem(sizeof *entry->attachInfo,
                                                 VMCI_MEMORY_NORMAL);
         if (entry->attachInfo == NULL) {
            result = VMCI_ERROR_NO_MEM;
            goto errorDealloc;
         }
         memset(entry->attachInfo, 0, sizeof *entry->attachInfo);

         VMCI_InitQueueMutex(entry->produceQ, entry->consumeQ);

         numProducePages = CEILING(produceSize, PAGE_SIZE) + 1;
         numConsumePages = CEILING(consumeSize, PAGE_SIZE) + 1;

         entry->attachInfo->numProducePages = numProducePages;
         entry->attachInfo->numConsumePages = numConsumePages;
      }
#endif /* !VMKERNEL */

      VMCIList_InitEntry(&entry->qp.listItem);

      QueuePairList_AddEntry(&qpBrokerList, &entry->qp);
      result = VMCI_SUCCESS_QUEUEPAIR_CREATE;
   } else { /* Attach case. */

      /*
       * Check for failure conditions.
       */

      if (isLocal) {
         if (!(entry->qp.flags & VMCI_QPFLAG_LOCAL) ||
             contextId != entry->createId) {
            result = VMCI_ERROR_INVALID_ARGS;
            goto out;
         }
      } else if (contextId == entry->createId || contextId == entry->attachId) {
         result = VMCI_ERROR_ALREADY_EXISTS;
         goto out;
      }

      /*
       * QueuePairs are create/destroy entities.  There's no notion of
       * disconnecting/re-attaching, so once a queue pair entry has
       * been attached to, no further attaches are allowed. This
       * guards against both re-attaching and attaching to a queue
       * pair that already has two peers.
       */

      if (!entry->allowAttach) {
         result = VMCI_ERROR_UNAVAILABLE;
         goto out;
      }
      ASSERT(entry->qp.refCount < 2);
      ASSERT(entry->attachId == VMCI_INVALID_ID);

      /*
       * If we are attaching from a restricted context then the queuepair
       * must have been created by a trusted endpoint.
       */

      if (context->privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED) {
         if (!entry->createdByTrusted) {
            result = VMCI_ERROR_NO_ACCESS;
            goto out;
         }
      }

      /*
       * If we are attaching to a queuepair that was created by a restricted
       * context then we must be trusted.
       */

      if (entry->requireTrustedAttach) {
         if (!(privFlags & VMCI_PRIVILEGE_FLAG_TRUSTED)) {
            result = VMCI_ERROR_NO_ACCESS;
            goto out;
         }
      }

      /*
       * If the creator specifies VMCI_INVALID_ID in "peer" field, access
       * control check is not performed.
       */

      if (entry->qp.peer != VMCI_INVALID_ID && entry->qp.peer != contextId) {
         result = VMCI_ERROR_NO_ACCESS;
         goto out;
      }

#ifndef VMKERNEL
      /*
       * VMKernel doesn't need to check the capabilities because the
       * whole system is installed as the kernel and matching VMX.
       */

      if (entry->createId == VMCI_HOST_CONTEXT_ID) {
         /*
          * Do not attach if the caller doesn't support Host Queue Pairs
          * and a host created this queue pair.
          */

         if (!VMCIContext_SupportsHostQP(context)) {
            result = VMCI_ERROR_INVALID_RESOURCE;
            goto out;
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
            result = VMCI_ERROR_INVALID_RESOURCE;
            goto out;
         }
      }
#endif // !VMKERNEL

      if (entry->qp.produceSize != consumeSize ||
          entry->qp.consumeSize != produceSize ||
          entry->qp.flags != (flags & ~VMCI_QPFLAG_ATTACH_ONLY)) {
         result = VMCI_ERROR_QUEUEPAIR_MISMATCH;
         goto out;
      }

      /*
       * On VMKERNEL (e.g., ESX) we don't allow an attach until
       * the page store information has been set.
       *
       * However, on hosted products we support an attach to a
       * QueuePair that hasn't had its page store established yet.  In
       * fact, that's how a VMX guest will approach a host-created
       * QueuePair.  After the VMX guest does the attach, VMX will
       * receive the CREATE status code to indicate that it should
       * create the page files for the QueuePair contents.  It will
       * then issue a separate call down to set the page store.  That
       * will complete the attach case.
       */
      if (vmkernel && !entry->pageStoreSet) {
         result = VMCI_ERROR_QUEUEPAIR_NOTSET;
         goto out;
      }

      /*
       * Check if we should allow this QueuePair connection.
       */

      if (QueuePairDenyConnection(contextId, entry->createId)) {
         result = VMCI_ERROR_NO_ACCESS;
         goto out;
      }

#ifdef VMKERNEL
      pageStore->store = entry->store.store;
#else
      if (pageStore && entry->pageStoreSet) {
         ASSERT(entry->producePageFile[0] && entry->consumePageFile[0]);
         if (pageStore->producePageFileSize < sizeof entry->consumePageFile) {
            result = VMCI_ERROR_NO_MEM;
            goto out;
         }
         if (pageStore->consumePageFileSize < sizeof entry->producePageFile) {
            result = VMCI_ERROR_NO_MEM;
            goto out;
         }

         if (pageStore->user) {
            if (VMCI_CopyToUser(pageStore->producePageFile,
                                entry->consumePageFile,
                                sizeof entry->consumePageFile)) {
               result = VMCI_ERROR_GENERIC;
               goto out;
            }

            if (VMCI_CopyToUser(pageStore->consumePageFile,
                                entry->producePageFile,
                                sizeof entry->producePageFile)) {
               result = VMCI_ERROR_GENERIC;
               goto out;
            }
         } else {
            memcpy(VMCIVA64ToPtr(pageStore->producePageFile),
                   entry->consumePageFile,
                   sizeof entry->consumePageFile);
            memcpy(VMCIVA64ToPtr(pageStore->consumePageFile),
                   entry->producePageFile,
                   sizeof entry->producePageFile);
         }
      }
#endif // VMKERNEL

      /*
       * We only send notification if the other end of the QueuePair
       * is not the host (in hosted products).  In the case that a
       * host created the QueuePair, we'll send notification when the
       * guest issues the SetPageStore() (see next function).  The
       * reason is that the host can't use the QueuePair until the
       * SetPageStore() is complete.
       *
       * Note that in ESX we always send the notification now
       * because the host can begin to enqueue immediately.
       */

      if (vmkernel || entry->createId != VMCI_HOST_CONTEXT_ID) {
         result = QueuePairNotifyPeer(TRUE, handle, contextId, entry->createId);
         if (result < VMCI_SUCCESS) {
            goto out;
         }
      }

      entry->attachId = contextId;
      entry->qp.refCount++;
      entry->allowAttach = FALSE;

      /*
       * Default response to an attach is _ATTACH.  However, if a host
       * created the QueuePair then we're a guest (because
       * host-to-host isn't supported).  And thus, the guest's VMX
       * needs to create the backing for the port.  So, we send up a
       * _CREATE response.
       */

      if (!vmkernel && entry->createId == VMCI_HOST_CONTEXT_ID) {
         result = VMCI_SUCCESS_QUEUEPAIR_CREATE;
      } else {
         result = VMCI_SUCCESS_QUEUEPAIR_ATTACH;
      }
   }

#ifndef VMKERNEL
   goto out;

   /*
    * Cleanup is only necessary on hosted
    */

errorDealloc:
   if (entry->produceQ != NULL) {
      VMCI_FreeKernelMem(entry->produceQ, sizeof *entry->produceQ);
   }
   if (entry->consumeQ != NULL) {
      VMCI_FreeKernelMem(entry->consumeQ, sizeof *entry->consumeQ);
   }
   if (entry->attachInfo != NULL) {
      VMCI_FreeKernelMem(entry->attachInfo, sizeof *entry->attachInfo);
   }
   VMCI_FreeKernelMem(entry, sizeof *entry);
#endif // !VMKERNEL

out:
   if (result >= VMCI_SUCCESS) {
      ASSERT(entry);
      if (ent != NULL) {
         *ent = entry;
      }

      /*
       * When attaching to local queue pairs, the context already has
       * an entry tracking the queue pair, so don't add another one.
       */

      if (!isLocal || result == VMCI_SUCCESS_QUEUEPAIR_CREATE) {
         ASSERT(!VMCIHandleArray_HasEntry(context->queuePairArray, handle));
         VMCIHandleArray_AppendEntry(&context->queuePairArray, handle);
      } else {
         ASSERT(VMCIHandleArray_HasEntry(context->queuePairArray, handle));
      }
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_SetPageStore --
 *
 *      The creator of a queue pair uses this to regsiter the page
 *      store for a given queue pair.  Assumes that the queue pair
 *      broker lock is held.
 *
 *      Note now that sometimes the client that attaches to a queue
 *      pair will set the page store.  This happens on hosted products
 *      because the host doesn't have a mechanism for creating the
 *      backing memory for queue contents.  ESX does and so this is a
 *      moot point there.  For example, note that in
 *      VMCIQPBrokerAllocInt() an attaching guest receives the _CREATE
 *      result code (instead of _ATTACH) on hosted products only, not
 *      on VMKERNEL.
 *
 *      As a result, this routine now always creates the host
 *      information even if the queue pair is only used by guests.  At
 *      the time a guest creates a queue pair it doesn't know if a
 *      host or guest will attach.  So, the host information always
 *      has to be created.
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
VMCIQPBroker_SetPageStore(VMCIHandle handle,             // IN
                          QueuePairPageStore *pageStore, // IN
                          VMCIContext *context)          // IN: Caller
{
   QPBrokerEntry *entry;
   int result;
   const VMCIId contextId = VMCIContext_GetId(context);
#ifndef VMKERNEL
   QueuePairPageStore normalizedPageStore;
#endif

   if (VMCI_HANDLE_INVALID(handle) || !pageStore ||
       !VMCI_QP_PAGESTORE_IS_WELLFORMED(pageStore) ||
       !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (!VMCIHandleArray_HasEntry(context->queuePairArray, handle)) {
      VMCI_WARNING((LGPFX"Context (ID=0x%x) not attached to queue pair "
                    "(handle=0x%x:0x%x).\n", contextId, handle.context,
                    handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

#ifndef VMKERNEL
   /*
    * If the client supports Host QueuePairs then it must provide the
    * UVA's of the mmap()'d files backing the QueuePairs.
    */

   if (VMCIContext_SupportsHostQP(context) &&
       (pageStore->producePageUVA == 0 ||
        pageStore->consumePageUVA == 0)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
#endif // !VMKERNEL

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
   if (entry->pageStoreSet) {
      result = VMCI_ERROR_UNAVAILABLE;
      goto out;
   }
#ifdef VMKERNEL
   entry->store = *pageStore;
#else
   /*
    * Normalize the page store information from the point of view of
    * the VMX process with respect to the QueuePair.  If VMX has
    * attached to a host-created QueuePair and is passing down
    * PageStore information then we must switch the produce/consume
    * queue information before applying it to the QueuePair.
    *
    * In other words, the QueuePair structure (entry->state) is
    * oriented with respect to the host that created it.  However, VMX
    * is sending down information relative to its view of the world
    * which is opposite of the host's.
    */

   if (entry->createId == contextId) {
      normalizedPageStore.producePageFile = pageStore->producePageFile;
      normalizedPageStore.consumePageFile = pageStore->consumePageFile;
      normalizedPageStore.producePageFileSize = pageStore->producePageFileSize;
      normalizedPageStore.consumePageFileSize = pageStore->consumePageFileSize;
      normalizedPageStore.producePageUVA = pageStore->producePageUVA;
      normalizedPageStore.consumePageUVA = pageStore->consumePageUVA;
   } else {
      normalizedPageStore.producePageFile = pageStore->consumePageFile;
      normalizedPageStore.consumePageFile = pageStore->producePageFile;
      normalizedPageStore.producePageFileSize = pageStore->consumePageFileSize;
      normalizedPageStore.consumePageFileSize = pageStore->producePageFileSize;
      normalizedPageStore.producePageUVA = pageStore->consumePageUVA;
      normalizedPageStore.consumePageUVA = pageStore->producePageUVA;
   }

   if (normalizedPageStore.producePageFileSize > sizeof entry->producePageFile) {
      result = VMCI_ERROR_NO_MEM;
       goto out;
   }
   if (normalizedPageStore.consumePageFileSize > sizeof entry->consumePageFile) {
      result = VMCI_ERROR_NO_MEM;
      goto out;
   }
   if (pageStore->user) {
      if (VMCI_CopyFromUser(entry->producePageFile,
                            normalizedPageStore.producePageFile,
                            (size_t)normalizedPageStore.producePageFileSize)) {
         result = VMCI_ERROR_GENERIC;
         goto out;
      }

      if (VMCI_CopyFromUser(entry->consumePageFile,
                            normalizedPageStore.consumePageFile,
                            (size_t)normalizedPageStore.consumePageFileSize)) {
         result = VMCI_ERROR_GENERIC;
         goto out;
      }
   } else {
      memcpy(entry->consumePageFile,
             VMCIVA64ToPtr(normalizedPageStore.consumePageFile),
             (size_t)normalizedPageStore.consumePageFileSize);
      memcpy(entry->producePageFile,
             VMCIVA64ToPtr(normalizedPageStore.producePageFile),
             (size_t)normalizedPageStore.producePageFileSize);
   }

   /*
    * Copy the data into the attachInfo structure
    */

   memcpy(&entry->attachInfo->producePageFile[0],
          &entry->producePageFile[0],
          (size_t)normalizedPageStore.producePageFileSize);
   memcpy(&entry->attachInfo->consumePageFile[0],
          &entry->consumePageFile[0],
          (size_t)normalizedPageStore.consumePageFileSize);

   /*
    * NOTE: The UVAs that follow may be 0.  In this case an older VMX has
    * issued a SetPageFile call without mapping the backing files for the
    * queue contents.  The result of this is that the queue pair cannot
    * be connected by host.
    */

   entry->attachInfo->produceBuffer = normalizedPageStore.producePageUVA;
   entry->attachInfo->consumeBuffer = normalizedPageStore.consumePageUVA;

   if (VMCIContext_SupportsHostQP(context)) {
      result = VMCIHost_GetUserMemory(entry->attachInfo,
                                      entry->produceQ,
                                      entry->consumeQ);

      if (result < VMCI_SUCCESS) {
         goto out;
      }
   }
#endif // VMKERNEL

   /*
    * In the event that the QueuePair was created by a host in a
    * hosted kernel, then we send notification now that the QueuePair
    * contents backing files are attached to the Queues.  Note in
    * VMCIQPBrokerAllocInt(), above, we skipped this step when the
    * creator was a host (on hosted).
    */

   if (!vmkernel && entry->createId == VMCI_HOST_CONTEXT_ID) {
      result = QueuePairNotifyPeer(TRUE, handle, contextId, entry->createId);
      if (result < VMCI_SUCCESS) {
         goto out;
      }
   }

   entry->pageStoreSet = TRUE;
   result = VMCI_SUCCESS;

out:
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPBroker_Detach --
 *
 *      Informs the VMCI queue pair broker that a context has detached
 *      from a given QueuePair handle.  Assumes that the queue pair
 *      broker lock is held.  If the "detach" input parameter is
 *      FALSE, the queue pair entry is not removed from the list of
 *      queue pairs registered with the queue pair broker, and the
 *      context is not detached from the given handle.  If "detach" is
 *      TRUE, the detach operation really happens.  With "detach" set
 *      to FALSE, the caller can query if the "actual" detach
 *      operation would succeed or not.  The return value from this
 *      function remains the same irrespective of the value of the
 *      boolean "detach".
 *
 *      Also note that the result code for a VM detaching from a
 *      VM-host queue pair is always VMCI_SUCCESS_LAST_DETACH.  This
 *      is so that VMX can unlink the backing files.  On the host side
 *      the files are either locked (Mac OS/Linux) or the contents are
 *      saved (Windows).
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
VMCIQPBroker_Detach(VMCIHandle  handle,   // IN
                    VMCIContext *context, // IN
                    Bool detach)          // IN: Really detach?
{
   QPBrokerEntry *entry;
   int result;
   const VMCIId contextId = VMCIContext_GetId(context);
   VMCIId peerId;
   Bool isLocal = FALSE;

   if (VMCI_HANDLE_INVALID(handle) ||
       !context || contextId == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (!VMCIHandleArray_HasEntry(context->queuePairArray, handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context (ID=0x%x) not attached to queue pair "
                         "(handle=0x%x:0x%x).\n",
                         contextId, handle.context, handle.resource));
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   entry = (QPBrokerEntry *)QueuePairList_FindEntry(&qpBrokerList, handle);
   if (!entry) {
      result = VMCI_ERROR_NOT_FOUND;
      goto out;
   }

   isLocal = entry->qp.flags & VMCI_QPFLAG_LOCAL;

   ASSERT(vmkernel || !isLocal);

   if (contextId != entry->createId && contextId != entry->attachId) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
      goto out;
   }

   if (contextId == entry->createId) {
      peerId = entry->attachId;
   } else {
      peerId = entry->createId;
   }

   if (!detach) {
      /* Do not update the queue pair entry. */

      ASSERT(entry->qp.refCount == 1 || entry->qp.refCount == 2);

      if (entry->qp.refCount == 1 ||
          (!vmkernel && peerId == VMCI_HOST_CONTEXT_ID)) {
         result = VMCI_SUCCESS_LAST_DETACH;
      } else {
         result = VMCI_SUCCESS;
      }

      goto out;
   }

   if (contextId == entry->createId) {
      entry->createId = VMCI_INVALID_ID;
   } else {
      entry->attachId = VMCI_INVALID_ID;
   }
   entry->qp.refCount--;

#ifdef _WIN32
   /*
    * If the caller detaching is a usermode process (e.g., VMX), then
    * we must detach the mappings now.  On Windows.
    *
    * VMCIHost_SaveProduceQ() will save the guest's produceQ so that
    * the host can pick up the data after the guest is gone.
    *
    * We save the ProduceQ whenever the guest detaches (even if VMX
    * continues to run).  If we didn't do this, then we'd have the
    * problem of finding and releasing the memory when the client goes
    * away because we won't be able to find the client in the list of
    * QueuePair entries.  The detach code path (has already) set the
    * contextId for detached end-point to VMCI_INVALID_ID.  (See just
    * a few lines above where that happens.)  Sure, we could fix that,
    * and then we could look at all entries finding ones where the
    * contextId of either creator or attach matches the going away
    * context's Id.  But, if we just copy out the guest's produceQ
    * -always- then we reduce the logic changes elsewhere.
    */

   /*
    * Some example paths through this code:
    *
    * Guest-to-guest: the code will call ReleaseUserMemory() once when
    * the first guest detaches.  And then a second time when the
    * second guest detaches.  That's OK.  Nobody is using the user
    * memory (because there is no host attached) and
    * ReleaseUserMemory() tracks its resources.
    *
    * Host detaches first: the code will not call anything because
    * contextId == VMCI_HOST_CONTEXT_ID and because (in the second if
    * () clause below) refCount > 0.
    *
    * Guest detaches second: the first if clause, below, will not be
    * taken because refCount is already 0.  The second if () clause
    * (below) will be taken and it will simply call
    * ReleaseUserMemory().
    *
    * Guest detaches first: the code will call SaveProduceQ().
    *
    * Host detaches second: the code will call ReleaseUserMemory()
    * which will free the kernel allocated Q memory.
    */

   if (entry->pageStoreSet &&
       contextId != VMCI_HOST_CONTEXT_ID &&
       VMCIContext_SupportsHostQP(context) &&
       entry->qp.refCount) {
      /*
       * It's important to pass down produceQ and consumeQ in the
       * correct order because the produceQ that is to be saved is the
       * guest's, so we have to be sure that the routine sees the
       * guest's produceQ as (in this case) the first Q parameter.
       */

      if (entry->attachId == VMCI_HOST_CONTEXT_ID) {
         VMCIHost_SaveProduceQ(entry->attachInfo,
                               entry->produceQ,
                               entry->consumeQ,
                               entry->qp.produceSize);
      } else if (entry->createId == VMCI_HOST_CONTEXT_ID) {
         VMCIHost_SaveProduceQ(entry->attachInfo,
                               entry->consumeQ,
                               entry->produceQ,
                               entry->qp.consumeSize);
      } else {
         VMCIHost_ReleaseUserMemory(entry->attachInfo,
                                    entry->produceQ,
                                    entry->consumeQ);
      }
   }
#endif // _WIN32

   if (!entry->qp.refCount) {
      QueuePairList_RemoveEntry(&qpBrokerList, &entry->qp);

#ifndef VMKERNEL
      if (entry->pageStoreSet &&
          VMCIContext_SupportsHostQP(context)) {
         VMCIHost_ReleaseUserMemory(entry->attachInfo,
                                    entry->produceQ,
                                    entry->consumeQ);
      }
      if (entry->attachInfo) {
         VMCI_FreeKernelMem(entry->attachInfo, sizeof *entry->attachInfo);
      }
      if (entry->produceQ) {
         VMCI_FreeKernelMem(entry->produceQ, sizeof *entry->produceQ);
      }
      if (entry->consumeQ) {
         VMCI_FreeKernelMem(entry->consumeQ, sizeof *entry->consumeQ);
      }
#endif // !VMKERNEL

      VMCI_FreeKernelMem(entry, sizeof *entry);
      result = VMCI_SUCCESS_LAST_DETACH;
   } else {
      /*
       * XXX: If we ever allow the creator to detach and attach again
       * to the same queue pair, we need to handle the mapping of the
       * shared memory region in vmkernel differently. Currently, we
       * assume that an attaching VM always needs to swap the two
       * queues.
       */

      ASSERT(peerId != VMCI_INVALID_ID);
      QueuePairNotifyPeer(FALSE, handle, contextId, peerId);
      if (!vmkernel && peerId == VMCI_HOST_CONTEXT_ID) {
         result = VMCI_SUCCESS_LAST_DETACH;
      } else {
         result = VMCI_SUCCESS;
      }
   }

out:
   if (result >= VMCI_SUCCESS && detach) {
      if (!isLocal || result == VMCI_SUCCESS_LAST_DETACH) {
         VMCIHandleArray_RemoveEntry(context->queuePairArray, handle);
      }
   }
   return result;
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
    * could do arbitrary number of attached and detaches causing memory
    * pressure in the host kernel.
   */

   /* Clear out any garbage. */
   memset(eMsg, 0, sizeof *eMsg + sizeof *evPayload);

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

#if !defined(VMKERNEL)

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
                           VMCIPrivilegeFlags privFlags) // IN
{
   VMCIContext *context;
   int result;
   QPBrokerEntry *entry;

   result = VMCI_SUCCESS;

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
   VMCIQPBroker_Lock();
   result = VMCIQPBrokerAllocInt(*handle, peer, flags, privFlags, produceSize,
                                 consumeSize, NULL, context, &entry);

   if (result >= VMCI_SUCCESS) {
      ASSERT(entry != NULL);

      if (entry->createId == VMCI_HOST_CONTEXT_ID) {
         *produceQ = entry->produceQ;
         *consumeQ = entry->consumeQ;
      } else {
         *produceQ = entry->consumeQ;
         *consumeQ = entry->produceQ;
      }

      result = VMCI_SUCCESS;
   } else {
      *handle = VMCI_INVALID_HANDLE;
      VMCI_DEBUG_LOG(4, (LGPFX"queue pair broker failed to alloc (result=%d).\n",
                         result));
   }

   VMCIQPBroker_Unlock();
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

   VMCIQPBroker_Lock();
   result = VMCIQPBroker_Detach(handle, context, TRUE);
   VMCIQPBroker_Unlock();

   VMCIContext_Release(context);
   return result;
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
                       VMCI_LOCK_RANK_MIDDLE_BH);
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

   VMCIQPLock_Acquire(&qpGuestEndpoints.lock);

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
   VMCIQPLock_Release(&qpGuestEndpoints.lock);
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
   VMCIQPLock_Acquire(&qpGuestEndpoints.lock);
   VMCIQPLock_Release(&qpGuestEndpoints.lock);
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

   VMCIQPLock_Acquire(&qpGuestEndpoints.lock);

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

   VMCIQPLock_Release(&qpGuestEndpoints.lock);

   return VMCI_SUCCESS;

error:
   VMCIQPLock_Release(&qpGuestEndpoints.lock);
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
   VMCIQPLock_Release(&qpGuestEndpoints.lock);
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

   VMCIQPLock_Acquire(&qpGuestEndpoints.lock);

   entry = (QPGuestEndpoint *)QueuePairList_FindEntry(&qpGuestEndpoints, handle);
   if (!entry) {
      VMCIQPLock_Release(&qpGuestEndpoints.lock);
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

         VMCIQPLock_Release(&qpGuestEndpoints.lock);
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

   VMCIQPLock_Release(&qpGuestEndpoints.lock);

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

      VMCIQPLock_Acquire(&qpGuestEndpoints.lock);

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

      VMCIQPLock_Release(&qpGuestEndpoints.lock);
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
