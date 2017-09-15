/*********************************************************
 * Copyright (C) 2006-2012,2014-2016 VMware, Inc. All rights reserved.
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
 * vmciContext.c --
 *
 *     Platform independent routines for VMCI calls.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciContext.h"
#include "vmciDatagram.h"
#include "vmciDoorbell.h"
#include "vmciDriver.h"
#include "vmciEvent.h"
#include "vmciKernelAPI.h"
#include "vmciQueuePair.h"
#if defined(_WIN32)
#  include "kernelStubsSal.h"
#elif defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIContext: "

static void VMCIContextFreeContext(VMCIContext *context);
static Bool VMCIContextExists(VMCIId cid);
static int VMCIContextFireNotification(VMCIId contextID,
                                       VMCIPrivilegeFlags privFlags);
#if defined(VMKERNEL)
static void VMCIContextReleaseGuestMemLocked(VMCIContext *context,
                                             VMCIGuestMemID gid,
                                             Bool powerOff);
static void VMCIContextInFilterCleanup(VMCIContext *context);
#endif

/*
 * List of current VMCI contexts.
 */

static struct {
   VMCIList head;
   VMCILock lock;
   VMCILock firingLock;
} contextList;


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextSignalNotify --
 *
 *      Sets the notify flag to TRUE.  Assumes that the context lock is
 *      held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VMCIContextSignalNotify(VMCIContext *context) // IN:
{
#ifndef VMX86_SERVER
   if (context->notify) {
      *context->notify = TRUE;
   }
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextClearNotify --
 *
 *      Sets the notify flag to FALSE.  Assumes that the context lock is
 *      held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VMCIContextClearNotify(VMCIContext *context) // IN:
{
#ifndef VMX86_SERVER
   if (context->notify) {
      *context->notify = FALSE;
   }
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextClearNotifyAndCall --
 *
 *      If nothing requires the attention of the guest, clears both
 *      notify flag and call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static INLINE void
VMCIContextClearNotifyAndCall(VMCIContext *context) // IN:
{
   if (context->pendingDatagrams == 0 &&
       VMCIHandleArray_GetSize(context->pendingDoorbellArray) == 0) {
      VMCIHost_ClearCall(&context->hostContext);
      VMCIContextClearNotify(context);
   }
}


#ifndef VMX86_SERVER
/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_CheckAndSignalNotify --
 *
 *      Sets the context's notify flag iff datagrams are pending for this
 *      context.  Called from VMCISetupNotify().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_CheckAndSignalNotify(VMCIContext *context) // IN:
{
   VMCILockFlags flags;

   ASSERT(context);
   VMCI_GrabLock(&context->lock, &flags);
   if (context->pendingDatagrams) {
      VMCIContextSignalNotify(context);
   }
   VMCI_ReleaseLock(&context->lock, flags);
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_Init --
 *
 *      Initializes the VMCI context module.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_Init(void)
{
   int err;

   VMCIList_Init(&contextList.head);

   err = VMCI_InitLock(&contextList.lock, "VMCIContextListLock",
                       VMCI_LOCK_RANK_CONTEXTLIST);
   if (err < VMCI_SUCCESS) {
      return err;
   }

   err = VMCI_InitLock(&contextList.firingLock, "VMCIContextFiringLock",
                       VMCI_LOCK_RANK_CONTEXTFIRE);
   if (err < VMCI_SUCCESS) {
      VMCI_CleanupLock(&contextList.lock);
   }

   return err;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_Exit --
 *
 *      Cleans up the contexts module.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_Exit(void)
{
   VMCI_CleanupLock(&contextList.firingLock);
   VMCI_CleanupLock(&contextList.lock);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_InitContext --
 *
 *      Allocates and initializes a VMCI context.
 *
 * Results:
 *      Returns 0 on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_InitContext(VMCIId cid,                   // IN
                        VMCIPrivilegeFlags privFlags, // IN
                        uintptr_t eventHnd,           // IN
                        int userVersion,              // IN: User's vers no.
                        VMCIHostUser *user,           // IN
                        VMCIContext **outContext)     // OUT
{
   VMCILockFlags flags;
   VMCIContext *context;
   int result;

   if (privFlags & ~VMCI_PRIVILEGE_ALL_FLAGS) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid flag (flags=0x%x) for VMCI context.\n",
                         privFlags));
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (userVersion == 0) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   context = VMCI_AllocKernelMem(sizeof *context, VMCI_MEMORY_NONPAGED);
   if (context == NULL) {
      VMCI_WARNING((LGPFX"Failed to allocate memory for VMCI context.\n"));
      return VMCI_ERROR_NO_MEM;
   }
   memset(context, 0, sizeof *context);

   VMCIList_InitEntry(&context->listItem);
   VMCIList_Init(&context->datagramQueue);

   context->userVersion = userVersion;

   context->queuePairArray = VMCIHandleArray_Create(0);
   if (!context->queuePairArray) {
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   context->doorbellArray = VMCIHandleArray_Create(0);
   if (!context->doorbellArray) {
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   context->pendingDoorbellArray = VMCIHandleArray_Create(0);
   if (!context->pendingDoorbellArray) {
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   context->notifierArray = VMCIHandleArray_Create(0);
   if (context->notifierArray == NULL) {
      result = VMCI_ERROR_NO_MEM;
      goto error;
   }

   result = VMCI_InitLock(&context->lock, "VMCIContextLock",
                          VMCI_LOCK_RANK_CONTEXT);
   if (result < VMCI_SUCCESS) {
      goto error;
   }
   Atomic_Write(&context->refCount, 1);

#if defined(VMKERNEL)
   result = VMCIMutex_Init(&context->guestMemMutex, "VMCIGuestMem",
                           VMCI_SEMA_RANK_GUESTMEM);
   if (result < VMCI_SUCCESS) {
      VMCI_CleanupLock(&context->lock);
      goto error;
   }
   context->curGuestMemID = INVALID_VMCI_GUEST_MEM_ID;

   context->inFilters = NULL;
#endif

   /* Inititialize host-specific VMCI context. */
   VMCIHost_InitContext(&context->hostContext, eventHnd);

   context->privFlags = privFlags;

   /*
    * If we collide with an existing context we generate a new and use it
    * instead. The VMX will determine if regeneration is okay. Since there
    * isn't 4B - 16 VMs running on a given host, the below loop will terminate.
    */
   VMCI_GrabLock(&contextList.lock, &flags);
   ASSERT(cid != VMCI_INVALID_ID);
   while (VMCIContextExists(cid)) {

      /*
       * If the cid is below our limit and we collide we are creating duplicate
       * contexts internally so we want to assert fail in that case.
       */
      ASSERT(cid >= VMCI_RESERVED_CID_LIMIT);

      /* We reserve the lowest 16 ids for fixed contexts. */
      cid = MAX(cid, VMCI_RESERVED_CID_LIMIT-1) + 1;
      if (cid == VMCI_INVALID_ID) {
         cid = VMCI_RESERVED_CID_LIMIT;
      }
   }
   ASSERT(!VMCIContextExists(cid));
   context->cid = cid;
   context->validUser = user != NULL;
   if (context->validUser) {
      context->user = *user;
   }
   VMCIList_Insert(&context->listItem, &contextList.head);
   VMCI_ReleaseLock(&contextList.lock, flags);

#ifdef VMKERNEL
   VMCIContext_SetFSRState(context, FALSE, VMCI_INVALID_ID, eventHnd, FALSE);
#endif

#ifndef VMX86_SERVER
   context->notify = NULL;
#  ifdef __linux__
   context->notifyPage = NULL;
#  endif
#endif

   *outContext = context;
   return VMCI_SUCCESS;

error:
   if (context->notifierArray) {
      VMCIHandleArray_Destroy(context->notifierArray);
   }
   if (context->queuePairArray) {
      VMCIHandleArray_Destroy(context->queuePairArray);
   }
   if (context->doorbellArray) {
      VMCIHandleArray_Destroy(context->doorbellArray);
   }
   if (context->pendingDoorbellArray) {
      VMCIHandleArray_Destroy(context->pendingDoorbellArray);
   }
   VMCI_FreeKernelMem(context, sizeof *context);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_ReleaseContext --
 *
 *      Cleans up a VMCI context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_ReleaseContext(VMCIContext *context)   // IN
{
   VMCILockFlags flags;

   /* Dequeue VMCI context. */

   VMCI_GrabLock(&contextList.lock, &flags);
   VMCIList_Remove(&context->listItem);
   VMCI_ReleaseLock(&contextList.lock, flags);

   VMCIContext_Release(context);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIContextFreeContext --
 *
 *      Deallocates all parts of a context datastructure. This
 *      functions doesn't lock the context, because it assumes that
 *      the caller is holding the last reference to context. As paged
 *      memory may be freed as part of the call, the function must be
 *      called without holding any spinlocks as this is not allowed on
 *      Windows.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Paged memory is freed.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCIContextFreeContext(VMCIContext *context)  // IN
{
   VMCIListItem *curr;
   VMCIListItem *next;
   DatagramQueueEntry *dqEntry;
   VMCIHandle tempHandle;

   /* Fire event to all contexts interested in knowing this context is dying. */
   VMCIContextFireNotification(context->cid, context->privFlags);

   /*
    * Cleanup all queue pair resources attached to context.  If the VM dies
    * without cleaning up, this code will make sure that no resources are
    * leaked.
    */

   tempHandle = VMCIHandleArray_GetEntry(context->queuePairArray, 0);
   while (!VMCI_HANDLE_EQUAL(tempHandle, VMCI_INVALID_HANDLE)) {
      if (VMCIQPBroker_Detach(tempHandle, context) < VMCI_SUCCESS) {
         /*
          * When VMCIQPBroker_Detach() succeeds it removes the handle from the
          * array.  If detach fails, we must remove the handle ourselves.
          */
         VMCIHandleArray_RemoveEntry(context->queuePairArray, tempHandle);
      }
      tempHandle = VMCIHandleArray_GetEntry(context->queuePairArray, 0);
   }

   /*
    * It is fine to destroy this without locking the datagramQueue, as
    * this is the only thread having a reference to the context.
    */

   VMCIList_ScanSafe(curr, next, &context->datagramQueue) {
      dqEntry = VMCIList_Entry(curr, DatagramQueueEntry, listItem);
      VMCIList_Remove(curr);
      ASSERT(dqEntry && dqEntry->dg);
      ASSERT(dqEntry->dgSize == VMCI_DG_SIZE(dqEntry->dg));
      VMCI_FreeKernelMem(dqEntry->dg, dqEntry->dgSize);
      VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);
   }

   VMCIHandleArray_Destroy(context->notifierArray);
   VMCIHandleArray_Destroy(context->queuePairArray);
   VMCIHandleArray_Destroy(context->doorbellArray);
   VMCIHandleArray_Destroy(context->pendingDoorbellArray);
   VMCI_CleanupLock(&context->lock);
#if defined(VMKERNEL)
   VMCIContextInFilterCleanup(context);
   VMCIMutex_Destroy(&context->guestMemMutex);
#endif
   VMCIHost_ReleaseContext(&context->hostContext);
#ifndef VMX86_SERVER
#  ifdef __linux__
   /* TODO Windows and Mac OS. */
   VMCIUnsetNotify(context);
#  endif
#endif
   VMCI_FreeKernelMem(context, sizeof *context);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_PendingDatagrams --
 *
 *      Returns the current number of pending datagrams. The call may
 *      also serve as a synchronization point for the datagram queue,
 *      as no enqueue operations can occur concurrently.
 *
 * Results:
 *      Length of datagram queue for the given context.
 *
 * Side effects:
 *      Locks datagram queue.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_PendingDatagrams(VMCIId cid,      // IN
                             uint32 *pending) // OUT
{
   VMCIContext *context;
   VMCILockFlags flags;

   context = VMCIContext_Get(cid);
   if (context == NULL) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCI_GrabLock(&context->lock, &flags);
   if (pending) {
      *pending = context->pendingDatagrams;
   }
   VMCI_ReleaseLock(&context->lock, flags);
   VMCIContext_Release(context);

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_EnqueueDatagram --
 *
 *      Queues a VMCI datagram for the appropriate target VM
 *      context.
 *
 * Results:
 *      Size of enqueued data on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_EnqueueDatagram(VMCIId cid,         // IN: Target VM
                            VMCIDatagram *dg,   // IN:
                            Bool notify)        // IN:
{
   DatagramQueueEntry *dqEntry;
   VMCIContext *context;
   VMCILockFlags flags;
   VMCIHandle dgSrc;
   size_t vmciDgSize;

   ASSERT(dg);
   vmciDgSize = VMCI_DG_SIZE(dg);
   ASSERT(vmciDgSize <= VMCI_MAX_DG_SIZE);

   /* Get the target VM's VMCI context. */
   context = VMCIContext_Get(cid);
   if (context == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid context (ID=0x%x).\n", cid));
      return VMCI_ERROR_INVALID_ARGS;
   }

   /* Allocate guest call entry and add it to the target VM's queue. */
   dqEntry = VMCI_AllocKernelMem(sizeof *dqEntry, VMCI_MEMORY_NONPAGED);
   if (dqEntry == NULL) {
      VMCI_WARNING((LGPFX"Failed to allocate memory for datagram.\n"));
      VMCIContext_Release(context);
      return VMCI_ERROR_NO_MEM;
   }
   dqEntry->dg = dg;
   dqEntry->dgSize = vmciDgSize;
   dgSrc = dg->src;
   VMCIList_InitEntry(&dqEntry->listItem);

   VMCI_GrabLock(&context->lock, &flags);

#if defined(VMKERNEL)
   if (context->inFilters != NULL) {
      if (VMCIFilterDenyDgIn(context->inFilters->filters, dg)) {
         VMCI_ReleaseLock(&context->lock, flags);
         VMCIContext_Release(context);
         VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);
         return VMCI_ERROR_NO_ACCESS;
      }
   }
#endif

   /*
    * We put a higher limit on datagrams from the hypervisor.  If the pending
    * datagram is not from hypervisor, then we check if enqueueing it would
    * exceed the VMCI_MAX_DATAGRAM_QUEUE_SIZE limit on the destination.  If the
    * pending datagram is from hypervisor, we allow it to be queued at the
    * destination side provided we don't reach the
    * VMCI_MAX_DATAGRAM_AND_EVENT_QUEUE_SIZE limit.
    */
   if (context->datagramQueueSize + vmciDgSize >=
         VMCI_MAX_DATAGRAM_QUEUE_SIZE &&
       (!VMCI_HANDLE_EQUAL(dgSrc,
                           VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                            VMCI_CONTEXT_RESOURCE_ID)) ||
        context->datagramQueueSize + vmciDgSize >=
         VMCI_MAX_DATAGRAM_AND_EVENT_QUEUE_SIZE)) {
      VMCI_ReleaseLock(&context->lock, flags);
      VMCIContext_Release(context);
      VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);
      VMCI_DEBUG_LOG(10, (LGPFX"Context (ID=0x%x) receive queue is full.\n",
                          cid));
      return VMCI_ERROR_NO_RESOURCES;
   }

   VMCIList_Insert(&dqEntry->listItem, &context->datagramQueue);
   context->pendingDatagrams++;
   context->datagramQueueSize += vmciDgSize;

   if (notify) {
      VMCIContextSignalNotify(context);
      VMCIHost_SignalCall(&context->hostContext);
   }

   VMCI_ReleaseLock(&context->lock, flags);
   VMCIContext_Release(context);

   return vmciDgSize;
}

#undef VMCI_MAX_DATAGRAM_AND_EVENT_QUEUE_SIZE


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextExists --
 *
 *      Internal helper to check if a context with the specified context
 *      ID exists. Assumes the contextList.lock is held.
 *
 * Results:
 *      TRUE if a context exists with the given cid.
 *      FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
VMCIContextExists(VMCIId cid)    // IN
{
   VMCIContext *context;
   VMCIListItem *next;
   Bool rv = FALSE;

   VMCIList_Scan(next, &contextList.head) {
      context = VMCIList_Entry(next, VMCIContext, listItem);
      if (context->cid == cid) {
         rv = TRUE;
         break;
      }
   }
   return rv;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_Exists --
 *
 *      Verifies whether a context with the specified context ID exists.
 *
 * Results:
 *      TRUE if a context exists with the given cid.
 *      FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_Exists(VMCIId cid)    // IN
{
   VMCILockFlags flags;
   Bool rv;

   VMCI_GrabLock(&contextList.lock, &flags);
   rv = VMCIContextExists(cid);
   VMCI_ReleaseLock(&contextList.lock, flags);
   return rv;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_Get --
 *
 *      Retrieves VMCI context corresponding to the given cid.
 *
 * Results:
 *      VMCI context on success, NULL otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCIContext *
VMCIContext_Get(VMCIId cid)  // IN
{
   VMCIContext *context = NULL;
   VMCIListItem *next;
   VMCILockFlags flags;

   if (cid == VMCI_INVALID_ID) {
      return NULL;
   }

   VMCI_GrabLock(&contextList.lock, &flags);
   if (VMCIList_Empty(&contextList.head)) {
      goto out;
   }

   VMCIList_Scan(next, &contextList.head) {
      context = VMCIList_Entry(next, VMCIContext, listItem);
      if (context->cid == cid) {
         /*
          * At this point, we are sure that the reference count is
          * larger already than zero. When starting the destruction of
          * a context, we always remove it from the context list
          * before decreasing the reference count. As we found the
          * context here, it hasn't been destroyed yet. This means
          * that we are not about to increase the reference count of
          * something that is in the process of being destroyed.
          */

         Atomic_Inc(&context->refCount);
         break;
      }
   }

out:
   VMCI_ReleaseLock(&contextList.lock, flags);
   return (context && context->cid == cid) ? context : NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_Release --
 *
 *      Releases the VMCI context. If this is the last reference to
 *      the context it will be deallocated. A context is created with
 *      a reference count of one, and on destroy, it is removed from
 *      the context list before its reference count is
 *      decremented. Thus, if we reach zero, we are sure that nobody
 *      else are about to increment it (they need the entry in the
 *      context list for that). This function musn't be called with a
 *      lock held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Paged memory may be deallocated.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_Release(VMCIContext *context)  // IN
{
   uint32 refCount;
   ASSERT(context);
   refCount = Atomic_ReadDec32(&context->refCount);
   if (refCount == 1) {
      VMCIContextFreeContext(context);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_DequeueDatagram --
 *
 *      Dequeues the next datagram and returns it to caller.
 *      The caller passes in a pointer to the max size datagram
 *      it can handle and the datagram is only unqueued if the
 *      size is less than maxSize. If larger maxSize is set to
 *      the size of the datagram to give the caller a chance to
 *      set up a larger buffer for the guestcall.
 *
 * Results:
 *      On success:  0 if no more pending datagrams, otherwise the size of
 *                   the next pending datagram.
 *      On failure:  appropriate error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_DequeueDatagram(VMCIContext *context, // IN
                            size_t *maxSize,      // IN/OUT: max size of datagram caller can handle.
                            VMCIDatagram **dg)    // OUT:
{
   DatagramQueueEntry *dqEntry;
   VMCIListItem *listItem;
   VMCILockFlags flags;
   int rv;

   ASSERT(context && dg);

   /* Dequeue the next datagram entry. */
   VMCI_GrabLock(&context->lock, &flags);
   if (context->pendingDatagrams == 0) {
      VMCIContextClearNotifyAndCall(context);
      VMCI_ReleaseLock(&context->lock, flags);
      VMCI_DEBUG_LOG(4, (LGPFX"No datagrams pending.\n"));
      return VMCI_ERROR_NO_MORE_DATAGRAMS;
   }

   listItem = VMCIList_First(&context->datagramQueue);
   ASSERT (listItem != NULL);

   dqEntry = VMCIList_Entry(listItem, DatagramQueueEntry, listItem);
#if defined(_WIN32)
   _Analysis_assume_(dqEntry != NULL);
#endif
   ASSERT(dqEntry->dg);

   /* Check size of caller's buffer. */
   if (*maxSize < dqEntry->dgSize) {
      *maxSize = dqEntry->dgSize;
      VMCI_ReleaseLock(&context->lock, flags);
      VMCI_DEBUG_LOG(4, (LGPFX"Caller's buffer should be at least "
                         "(size=%u bytes).\n", (uint32)*maxSize));
      return VMCI_ERROR_NO_MEM;
   }

   VMCIList_Remove(listItem);
   context->pendingDatagrams--;
   context->datagramQueueSize -= dqEntry->dgSize;
   if (context->pendingDatagrams == 0) {
      VMCIContextClearNotifyAndCall(context);
      rv = VMCI_SUCCESS;
   } else {
      /*
       * Return the size of the next datagram.
       */
      DatagramQueueEntry *nextEntry;

      listItem = VMCIList_First(&context->datagramQueue);
      ASSERT(listItem);
      nextEntry = VMCIList_Entry(listItem, DatagramQueueEntry, listItem);
#if defined(_WIN32)
      _Analysis_assume_(nextEntry != NULL);
#endif
      ASSERT(nextEntry && nextEntry->dg);
      /*
       * The following size_t -> int truncation is fine as the maximum size of
       * a (routable) datagram is 68KB.
       */
      rv = (int)nextEntry->dgSize;
   }
   VMCI_ReleaseLock(&context->lock, flags);

   /* Caller must free datagram. */
   ASSERT(dqEntry->dgSize == VMCI_DG_SIZE(dqEntry->dg));
   *dg = dqEntry->dg;
   dqEntry->dg = NULL;
   VMCI_FreeKernelMem(dqEntry, sizeof *dqEntry);

   return rv;
}


#ifdef VMKERNEL
/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SetFSRState --
 *
 *      Set the states related to FSR (quiesced state, migrateCid,
 *      active event handle).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_SetFSRState(VMCIContext *context, // IN
                        Bool isQuiesced,      // IN
                        VMCIId migrateCid,    // IN
                        uintptr_t eventHnd,   // IN
                        Bool isLocked)        // IN
{
   VMCILockFlags flags;
   if (!context) {
      return;
   }
   if (!isLocked) {
      VMCI_GrabLock(&context->lock, &flags);
   }
   context->isQuiesced = isQuiesced;
   context->migrateCid = migrateCid;
   VMCIHost_SetActiveHnd(&context->hostContext, eventHnd);
   if (!isLocked) {
      VMCI_ReleaseLock(&context->lock, flags);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_FindAndUpdateSrcFSR --
 *
 *      Find the source context for fast-suspend-resume. If found, the
 *      source context's FSR state is changed to reflect the new active
 *      event handle.
 *
 * Results:
 *      If found, source context for fast-suspend-resume, NULL otherwise.
 *
 * Side effects:
 *      The source context reference count increased and the caller is
 *      supposed to release the context once it is done using it.
 *
 *----------------------------------------------------------------------
 */

VMCIContext *
VMCIContext_FindAndUpdateSrcFSR(VMCIId migrateCid,      // IN
                                uintptr_t eventHnd,     // IN
                                uintptr_t *srcEventHnd) // IN/OUT
{
   VMCIContext *contextSrc = VMCIContext_Get(migrateCid);

   if (contextSrc) {
      VMCILockFlags flags;

      VMCI_GrabLock(&contextSrc->lock, &flags);
      if (contextSrc->isQuiesced && contextSrc->migrateCid == migrateCid) {
         if (srcEventHnd) {
            *srcEventHnd = VMCIHost_GetActiveHnd(&contextSrc->hostContext);
            ASSERT(*srcEventHnd != VMCI_INVALID_ID);
         }
         VMCIContext_SetFSRState(contextSrc, FALSE, VMCI_INVALID_ID,
                                 eventHnd, TRUE);
         VMCI_ReleaseLock(&contextSrc->lock, flags);
         return contextSrc;
      }
      VMCI_ReleaseLock(&contextSrc->lock, flags);
      VMCIContext_Release(contextSrc);
   }
   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_IsActiveHnd --
 *
 *      Whether the curent event handle is the active handle.
 *
 * Results:
 *      TRUE if the event handle is active, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_IsActiveHnd(VMCIContext *context, // IN
                        uintptr_t eventHnd)   // IN
{
   VMCILockFlags flags;
   Bool isActive;

   ASSERT(context);
   VMCI_GrabLock(&context->lock, &flags);
   isActive = VMCIHost_IsActiveHnd(&context->hostContext, eventHnd);
   VMCI_ReleaseLock(&context->lock, flags);
   return isActive;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_GetActiveHnd --
 *
 *      Returns the curent event handle.
 *
 * Results:
 *      The current active handle.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

uintptr_t
VMCIContext_GetActiveHnd(VMCIContext *context) // IN
{
   VMCILockFlags flags;
   uintptr_t activeHnd;

   ASSERT(context);
   VMCI_GrabLock(&context->lock, &flags);
   activeHnd = VMCIHost_GetActiveHnd(&context->hostContext);
   VMCI_ReleaseLock(&context->lock, flags);
   return activeHnd;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SetInactiveHnd --
 *
 *      Set the handle to be the inactive one.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_SetInactiveHnd(VMCIContext *context, // IN
                           uintptr_t eventHnd)   // IN
{
   VMCILockFlags flags;

   ASSERT(context);
   VMCI_GrabLock(&context->lock, &flags);
   VMCIHost_SetInactiveHnd(&context->hostContext, eventHnd);
   VMCI_ReleaseLock(&context->lock, flags);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_RemoveHnd --
 *
 *      Remove the event handle from host context.
 *
 * Results:
 *      Whether the handle exists and removed, also number of handles
 *      before removal and number of handles after removal.
 *
 * Side effects:
 *      If this is active handle, the inactive handle becomes active.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_RemoveHnd(VMCIContext *context, // IN
                      uintptr_t eventHnd,   // IN
                      uint32 *numOld,       // OUT
                      uint32 *numNew)       // OUT
{
   VMCILockFlags flags;
   uint32 numHandleOld, numHandleNew;
   Bool ret;

   ASSERT(context);
   VMCI_GrabLock(&context->lock, &flags);
   numHandleOld = VMCIHost_NumHnds(&context->hostContext);
   ret = VMCIHost_RemoveHnd(&context->hostContext, eventHnd);
   numHandleNew = VMCIHost_NumHnds(&context->hostContext);
   /*
    * This is needed to prevent FSR to share this
    * context while this context is being destroyed.
    */
   if (ret && numHandleOld == 1 && numHandleNew == 1) {
      context->migrateCid = VMCI_INVALID_ID;
   }
   VMCI_ReleaseLock(&context->lock, flags);

   if (numOld) {
      *numOld = numHandleOld;
   }
   if (numNew) {
      *numNew = numHandleNew;
   }
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIContext_ClearDatagrams --
 *
 *      Clear pending datagrams.
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
VMCIContext_ClearDatagrams(VMCIContext *context) // IN
{
   int retval;
   VMCIDatagram *dg = NULL;
   size_t size = VMCI_MAX_DG_SIZE;
   uint32 pending;

   /* Drop all datagrams that are currently pending for given context. */
   if (context == NULL) {
      return;
   }
   retval = VMCIContext_PendingDatagrams(context->cid, &pending);
   if (retval != VMCI_SUCCESS) {
      /*
       * This shouldn't happen as we already verified that the context
       * exists.
       */

      ASSERT(FALSE);
      return;
   }

   /*
    * We drain the queue for any datagrams pending at the beginning of
    * the loop. As datagrams may arrive at any point in time, we
    * cannot guarantee that the queue is empty after this point. Only
    * removing a fixed number of pending datagrams prevents us from
    * looping forever.
    */

   while (pending > 0 &&
          VMCIContext_DequeueDatagram(context, &size, &dg) >= 0) {
      ASSERT(dg);
      VMCI_FreeKernelMem(dg, VMCI_DG_SIZE(dg));
      --pending;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SetId --
 *
 *      Set the cid of given VMCI context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_SetId(VMCIContext *context,         // IN
                  VMCIId cid)                   // IN:
{
   VMCILockFlags flags;

   if (!context) {
      return;
   }
   VMCI_GrabLock(&context->lock, &flags);
   context->cid = cid;
   VMCI_ReleaseLock(&context->lock, flags);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextGenerateEvent --
 *
 *      Generates a VMCI event that only takes context ID as event data.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
VMCIContextGenerateEvent(VMCIId cid,       // IN
                         VMCI_Event event) // IN
{
   VMCIEventMsg *eMsg;
   VMCIEventPayload_Context *ePayload;
   /* buf is only 48 bytes. */
   char buf[sizeof *eMsg + sizeof *ePayload];

   eMsg = (VMCIEventMsg *)buf;
   ePayload = VMCIEventMsgPayload(eMsg);

   eMsg->hdr.dst = VMCI_MAKE_HANDLE(VMCI_HOST_CONTEXT_ID, VMCI_EVENT_HANDLER);
   eMsg->hdr.src = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                    VMCI_CONTEXT_RESOURCE_ID);
   eMsg->hdr.payloadSize = sizeof *eMsg + sizeof *ePayload - sizeof eMsg->hdr;
   eMsg->eventData.event = event;
   ePayload->contextID = cid;

   (void)VMCIEvent_Dispatch((VMCIDatagram *)eMsg);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_NotifyGuestPaused --
 *
 *      Notify subscribers of a execution state change of the VM
 *      with the given context ID. This will happen when a VM is
 *      quiesced/unquiesced.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_NotifyGuestPaused(VMCIId cid,  // IN
                              Bool paused) // IN
{
   VMCIContextGenerateEvent(cid, paused ? VMCI_EVENT_GUEST_PAUSED :
                                          VMCI_EVENT_GUEST_UNPAUSED);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_NotifyMemoryAccess --
 *
 *      Notify subscribers of a memory access change to the device.
 *      This can occur when the device is enabled/disabled/reset.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_NotifyMemoryAccess(VMCIId cid, // IN
                               Bool on)    // IN
{
   VMCIContextGenerateEvent(cid, on ? VMCI_EVENT_MEM_ACCESS_ON :
                                      VMCI_EVENT_MEM_ACCESS_OFF);
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_GetId --
 *
 *      Retrieves cid of given VMCI context.
 *
 * Results:
 *      VMCIId of context on success, VMCI_INVALID_ID otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCIId
VMCIContext_GetId(VMCIContext *context) // IN:
{
   if (!context) {
      return VMCI_INVALID_ID;
   }
   ASSERT(context->cid != VMCI_INVALID_ID);
   return context->cid;
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_context_get_priv_flags --
 *
 *      Retrieves the privilege flags of the given VMCI context ID.
 *
 * Results:
 *     Context's privilege flags.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_context_get_priv_flags)
VMCIPrivilegeFlags
vmci_context_get_priv_flags(VMCIId contextID)  // IN
{
   if (VMCI_HostPersonalityActive()) {
      VMCIPrivilegeFlags flags;
      VMCIContext *context;

      context = VMCIContext_Get(contextID);
      if (!context) {
         return VMCI_LEAST_PRIVILEGE_FLAGS;
      }
      flags = context->privFlags;
      VMCIContext_Release(context);
      return flags;
   }
   return VMCI_NO_PRIVILEGE_FLAGS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_AddNotification --
 *
 *      Add remoteCID to list of contexts current contexts wants
 *      notifications from/about.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      As in VMCIHandleArray_AppendEntry().
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_AddNotification(VMCIId contextID,  // IN:
                            VMCIId remoteCID)  // IN:
{
   int result = VMCI_ERROR_ALREADY_EXISTS;
   VMCILockFlags flags;
   VMCILockFlags firingFlags;
   VMCIHandle notifierHandle;
   VMCIContext *context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }

   if (VMCI_CONTEXT_IS_VM(contextID) && VMCI_CONTEXT_IS_VM(remoteCID)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Context removed notifications for other VMs not "
                         "supported (src=0x%x, remote=0x%x).\n",
                         contextID, remoteCID));
      result = VMCI_ERROR_DST_UNREACHABLE;
      goto out;
   }

   if (context->privFlags & VMCI_PRIVILEGE_FLAG_RESTRICTED) {
      result = VMCI_ERROR_NO_ACCESS;
      goto out;
   }

   notifierHandle = VMCI_MAKE_HANDLE(remoteCID, VMCI_EVENT_HANDLER);
   VMCI_GrabLock(&contextList.firingLock, &firingFlags);
   VMCI_GrabLock(&context->lock, &flags);
   if (!VMCIHandleArray_HasEntry(context->notifierArray, notifierHandle)) {
      VMCIHandleArray_AppendEntry(&context->notifierArray, notifierHandle);
      result = VMCI_SUCCESS;
   }
   VMCI_ReleaseLock(&context->lock, flags);
   VMCI_ReleaseLock(&contextList.firingLock, firingFlags);
out:
   VMCIContext_Release(context);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_RemoveNotification --
 *
 *      Remove remoteCID from current context's list of contexts it is
 *      interested in getting notifications from/about.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_RemoveNotification(VMCIId contextID,  // IN:
                               VMCIId remoteCID)  // IN:
{
   VMCILockFlags flags;
   VMCILockFlags firingFlags;
   VMCIContext *context = VMCIContext_Get(contextID);
   VMCIHandle tmpHandle;
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }
   VMCI_GrabLock(&contextList.firingLock, &firingFlags);
   VMCI_GrabLock(&context->lock, &flags);
   tmpHandle =
      VMCIHandleArray_RemoveEntry(context->notifierArray,
                                  VMCI_MAKE_HANDLE(remoteCID,
                                                   VMCI_EVENT_HANDLER));
   VMCI_ReleaseLock(&context->lock, flags);
   VMCI_ReleaseLock(&contextList.firingLock, firingFlags);
   VMCIContext_Release(context);

   if (VMCI_HANDLE_EQUAL(tmpHandle, VMCI_INVALID_HANDLE)) {
      return VMCI_ERROR_NOT_FOUND;
   }
   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextFireNotification --
 *
 *      Fire notification for all contexts interested in given cid.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIContextFireNotification(VMCIId contextID,             // IN
                            VMCIPrivilegeFlags privFlags) // IN
{
   uint32 i, arraySize;
   VMCIListItem *next;
   VMCILockFlags flags;
   VMCILockFlags firingFlags;
   VMCIHandleArray *subscriberArray;
   VMCIHandle contextHandle = VMCI_MAKE_HANDLE(contextID, VMCI_EVENT_HANDLER);

   /*
    * We create an array to hold the subscribers we find when scanning through
    * all contexts.
    */
   subscriberArray = VMCIHandleArray_Create(0);
   if (subscriberArray == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   /*
    * Scan all contexts to find who is interested in being notified about
    * given contextID. We have a special firingLock that we use to synchronize
    * across all notification operations. This avoids us having to take the
    * context lock for each HasEntry call and it solves a lock ranking issue.
    */
   VMCI_GrabLock(&contextList.firingLock, &firingFlags);
   VMCI_GrabLock(&contextList.lock, &flags);
   VMCIList_Scan(next, &contextList.head) {
      VMCIContext *subCtx = VMCIList_Entry(next, VMCIContext, listItem);

      /*
       * We only deliver notifications of the removal of contexts, if
       * the two contexts are allowed to interact.
       */

      if (VMCIHandleArray_HasEntry(subCtx->notifierArray, contextHandle) &&
          !VMCIDenyInteraction(privFlags, subCtx->privFlags)) {
         VMCIHandleArray_AppendEntry(&subscriberArray,
                                     VMCI_MAKE_HANDLE(subCtx->cid,
                                                      VMCI_EVENT_HANDLER));
      }
   }
   VMCI_ReleaseLock(&contextList.lock, flags);
   VMCI_ReleaseLock(&contextList.firingLock, firingFlags);

   /* Fire event to all subscribers. */
   arraySize = VMCIHandleArray_GetSize(subscriberArray);
   for (i = 0; i < arraySize; i++) {
      int result;
      VMCIEventMsg *eMsg;
      VMCIEventPayload_Context *evPayload;
      char buf[sizeof *eMsg + sizeof *evPayload];

      eMsg = (VMCIEventMsg *)buf;

      /* Clear out any garbage. */
      memset(eMsg, 0, sizeof *eMsg + sizeof *evPayload);
      eMsg->hdr.dst = VMCIHandleArray_GetEntry(subscriberArray, i);
      eMsg->hdr.src = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                       VMCI_CONTEXT_RESOURCE_ID);
      eMsg->hdr.payloadSize = sizeof *eMsg + sizeof *evPayload -
                              sizeof eMsg->hdr;
      eMsg->eventData.event = VMCI_EVENT_CTX_REMOVED;
      evPayload = VMCIEventMsgPayload(eMsg);
      evPayload->contextID = contextID;

      result = VMCIDatagram_Dispatch(VMCI_HYPERVISOR_CONTEXT_ID,
                                     (VMCIDatagram *)eMsg, FALSE);
      if (result < VMCI_SUCCESS) {
         VMCI_DEBUG_LOG(4, (LGPFX"Failed to enqueue event datagram "
                            "(type=%d) for context (ID=0x%x).\n",
                            eMsg->eventData.event, eMsg->hdr.dst.context));
         /* We continue to enqueue on next subscriber. */
      }
   }
   VMCIHandleArray_Destroy(subscriberArray);

   return VMCI_SUCCESS;
}

/*
 *----------------------------------------------------------------------
 *
 * VMCIContextDgHypervisorSaveStateSize --
 *
 *      Calculate the size for the hypervisor datagram checkpoint
 *      save data.
 *
 *      The format is as follows:
 *
 *      uint32 count - number of entries
 *      uint32 size  - size of first entry
 *      char bytes[] - contents of first entry
 *      uint32 size  - size of second entry
 *      char bytes[] - contents of second entry
 *      ...
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIContextDgHypervisorSaveStateSize(VMCIContext *context,  // IN
                                     uint32 *bufSize,       // IN/OUT
                                     char **cptBufPtr)      // UNUSED
{
   uint32 total;
   VMCIListItem *iter;

   UNREFERENCED_PARAMETER(cptBufPtr);

   *bufSize = total = 0;

   VMCIList_Scan(iter, &context->datagramQueue) {
      DatagramQueueEntry *dqEntry =
         VMCIList_Entry(iter, DatagramQueueEntry, listItem);

      if (dqEntry->dg->src.context == VMCI_HYPERVISOR_CONTEXT_ID) {
         /* Size of the datagram followed by the contents of the datagram. */
         total += sizeof(uint32) + dqEntry->dgSize;
      }
   }

   if (total > 0) {
      /* Don't forget the datagram count. */
      *bufSize = total + sizeof(uint32);
   }

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextDgHypervisorSaveState --
 *
 *      Get the hypervisor datagram checkpoint save data.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
VMCIContextDgHypervisorSaveState(VMCIContext *context,   // IN
                                 uint32 *bufSize,        // IN/OUT
                                 char **cptBufPtr)       // OUT
{
   uint8 *p;
   uint32 total;
   uint32 count;
   VMCIListItem *iter;

   /* Need at least the count field and the size of one entry. */
   if (*bufSize < sizeof(uint32) * 2) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   p = VMCI_AllocKernelMem(*bufSize, VMCI_MEMORY_NONPAGED);
   if (p == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   *cptBufPtr = (char *)p;

   /* Leave space for the datagram count at the start. */
   total  = sizeof(uint32);
   p     += sizeof(uint32);

   count = 0;
   VMCIList_Scan(iter, &context->datagramQueue) {
      DatagramQueueEntry *dqEntry =
         VMCIList_Entry(iter, DatagramQueueEntry, listItem);

      if (dqEntry->dg->src.context == VMCI_HYPERVISOR_CONTEXT_ID) {

         /*
          * VMX might have capped the amount of space we can use to checkpoint
          * hypervisor datagrams. Respect that here. Those that are not written
          * to the buffer will get dropped.
          */

         if (total + sizeof(uint32) + dqEntry->dgSize > *bufSize) {
            break;
         }

         total += sizeof(uint32) + dqEntry->dgSize;

         /*
          * Write in the size of the datagram followed by the contents of the
          * datagram itself.
          */

         *(uint32 *)p = dqEntry->dgSize;
         p += sizeof(uint32);

         memcpy(p, dqEntry->dg, dqEntry->dgSize);
         p += dqEntry->dgSize;

         count++;
      }
   }

   /* Now rollback and write the count at the start of the block. */
   *(uint32 *)(*cptBufPtr) = count;

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_GetCheckpointState --
 *
 *      Get current context's checkpoint state of given type.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_GetCheckpointState(VMCIId contextID,    // IN:
                               uint32 cptType,      // IN:
                               uint32 *bufSize,     // IN/OUT:
                               char   **cptBufPtr)  // OUT:
{
   int i, result;
   VMCILockFlags flags;
   uint32 arraySize, cptDataSize;
   VMCIHandleArray *array;
   VMCIContext *context;
   char *cptBuf;
   Bool getContextID;

   ASSERT(bufSize && cptBufPtr);

   context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_GrabLock(&context->lock, &flags);
   if (cptType == VMCI_NOTIFICATION_CPT_STATE) {
      ASSERT(context->notifierArray);
      array = context->notifierArray;
      getContextID = TRUE;
   } else if (cptType == VMCI_WELLKNOWN_CPT_STATE) {
      /*
       * For compatibility with VMX'en with VM to VM communication, we
       * always return zero wellknown handles.
       */

      *bufSize = 0;
      *cptBufPtr = NULL;
      result = VMCI_SUCCESS;
      goto release;
   } else if (cptType == VMCI_DOORBELL_CPT_STATE) {
      ASSERT(context->doorbellArray);
      array = context->doorbellArray;
      getContextID = FALSE;
   } else if (cptType == VMCI_DG_HYPERVISOR_SAVE_STATE_SIZE) {
      result = VMCIContextDgHypervisorSaveStateSize(context, bufSize, cptBufPtr);
      goto release;
   } else if (cptType == VMCI_DG_HYPERVISOR_SAVE_STATE) {
      result = VMCIContextDgHypervisorSaveState(context, bufSize, cptBufPtr);
      goto release;
   } else {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid cpt state (type=%d).\n", cptType));
      result = VMCI_ERROR_INVALID_ARGS;
      goto release;
   }

   arraySize = VMCIHandleArray_GetSize(array);
   if (arraySize > 0) {
      if (cptType == VMCI_DOORBELL_CPT_STATE) {
         cptDataSize = arraySize * sizeof(VMCIDoorbellCptState);
      } else {
         cptDataSize = arraySize * sizeof(VMCIId);
      }
      if (*bufSize < cptDataSize) {
         *bufSize = cptDataSize;
         result = VMCI_ERROR_MORE_DATA;
         goto release;
      }

      cptBuf = VMCI_AllocKernelMem(cptDataSize,
                                   VMCI_MEMORY_NONPAGED | VMCI_MEMORY_ATOMIC);
      if (cptBuf == NULL) {
         result = VMCI_ERROR_NO_MEM;
         goto release;
      }

      for (i = 0; i < arraySize; i++) {
         VMCIHandle tmpHandle = VMCIHandleArray_GetEntry(array, i);
         if (cptType == VMCI_DOORBELL_CPT_STATE) {
/*
 * PreFAST thinks this might overflow on arraySize>=2. However, we've
 * looked *very* carefully at this, tested PreFAST's assumptions, and
 * concluded PreFAST is getting confused about the relationships between
 * cptDataSize, arraySize, and i.
 */
#if defined(_WIN32)
#pragma warning(suppress: 6386)
#endif
            ((VMCIDoorbellCptState *)cptBuf)[i].handle = tmpHandle;
         } else {
            ((VMCIId *)cptBuf)[i] =
               getContextID ? tmpHandle.context : tmpHandle.resource;
         }
      }
      *bufSize = cptDataSize;
      *cptBufPtr = cptBuf;
   } else {
      *bufSize = 0;
      *cptBufPtr = NULL;
   }
   result = VMCI_SUCCESS;

release:
   VMCI_ReleaseLock(&context->lock, flags);
   VMCIContext_Release(context);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SetCheckpointState --
 *
 *      Set current context's checkpoint state of given type.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_SetCheckpointState(VMCIId contextID, // IN:
                               uint32 cptType,   // IN:
                               uint32 bufSize,   // IN:
                               char   *cptBuf)   // IN:
{
   uint32 i;
   VMCIId currentID;
   int result = VMCI_SUCCESS;
   uint32 numIDs = bufSize / sizeof(VMCIId);
   ASSERT(cptBuf);

   if (cptType == VMCI_WELLKNOWN_CPT_STATE && numIDs > 0) {
      /*
       * We would end up here if VMX with VM to VM communication
       * attempts to restore a checkpoint with wellknown handles.
       */

      VMCI_WARNING((LGPFX"Attempt to restore checkpoint with obsolete "
                    "wellknown handles.\n"));
      return VMCI_ERROR_OBSOLETE;
   }

   if (cptType != VMCI_NOTIFICATION_CPT_STATE) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid cpt state (type=%d).\n", cptType));
      return VMCI_ERROR_INVALID_ARGS;
   }

   for (i = 0; i < numIDs && result == VMCI_SUCCESS; i++) {
      currentID = ((VMCIId *)cptBuf)[i];
      result = VMCIContext_AddNotification(contextID, currentID);
      if (result != VMCI_SUCCESS) {
         break;
      }
   }
   if (result != VMCI_SUCCESS) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to set cpt state (type=%d) (error=%d).\n",
                         cptType, result));
   }
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_ReceiveNotificationsGet --
 *
 *      Retrieves the specified context's pending notifications in the
 *      form of a handle array. The handle arrays returned are the
 *      actual data - not a copy and should not be modified by the
 *      caller. They must be released using
 *      VMCIContext_ReceiveNotificationsRelease.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_ReceiveNotificationsGet(VMCIId contextID,                // IN
                                    VMCIHandleArray **dbHandleArray, // OUT
                                    VMCIHandleArray **qpHandleArray) // OUT
{
   VMCIContext *context;
   VMCILockFlags flags;
   int result = VMCI_SUCCESS;

   ASSERT(dbHandleArray && qpHandleArray);

   context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }
   VMCI_GrabLock(&context->lock, &flags);

   *dbHandleArray = context->pendingDoorbellArray;
   context->pendingDoorbellArray =  VMCIHandleArray_Create(0);
   if (!context->pendingDoorbellArray) {
      context->pendingDoorbellArray = *dbHandleArray;
      *dbHandleArray = NULL;
      result = VMCI_ERROR_NO_MEM;
   }
   *qpHandleArray = NULL;

   VMCI_ReleaseLock(&context->lock, flags);
   VMCIContext_Release(context);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_ReceiveNotificationsRelease --
 *
 *      Releases handle arrays with pending notifications previously
 *      retrieved using VMCIContext_ReceiveNotificationsGet. If the
 *      notifications were not successfully handed over to the guest,
 *      success must be false.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_ReceiveNotificationsRelease(VMCIId contextID,               // IN
                                        VMCIHandleArray *dbHandleArray, // IN
                                        VMCIHandleArray *qpHandleArray, // IN
                                        Bool success)                   // IN
{
   VMCIContext *context = VMCIContext_Get(contextID);

   if (context) {
      VMCILockFlags flags;

      VMCI_GrabLock(&context->lock, &flags);
      if (!success) {
         VMCIHandle handle;

         /*
          * New notifications may have been added while we were not
          * holding the context lock, so we transfer any new pending
          * doorbell notifications to the old array, and reinstate the
          * old array.
          */

         handle = VMCIHandleArray_RemoveTail(context->pendingDoorbellArray);
         while (!VMCI_HANDLE_INVALID(handle)) {
            ASSERT(VMCIHandleArray_HasEntry(context->doorbellArray, handle));
            if (!VMCIHandleArray_HasEntry(dbHandleArray, handle)) {
               VMCIHandleArray_AppendEntry(&dbHandleArray, handle);
            }
            handle = VMCIHandleArray_RemoveTail(context->pendingDoorbellArray);
         }
         VMCIHandleArray_Destroy(context->pendingDoorbellArray);
         context->pendingDoorbellArray = dbHandleArray;
         dbHandleArray = NULL;
      } else {
         VMCIContextClearNotifyAndCall(context);
      }
      VMCI_ReleaseLock(&context->lock, flags);
      VMCIContext_Release(context);
   } else {
         /*
          * The OS driver part is holding on to the context for the
          * duration of the receive notification ioctl, so it should
          * still be here.
          */

         ASSERT(FALSE);
   }

   if (dbHandleArray) {
      VMCIHandleArray_Destroy(dbHandleArray);
   }
   if (qpHandleArray) {
      VMCIHandleArray_Destroy(qpHandleArray);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_DoorbellCreate --
 *
 *      Registers that a new doorbell handle has been allocated by the
 *      context. Only doorbell handles registered can be notified.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_DoorbellCreate(VMCIId contextID,   // IN
                           VMCIHandle handle)  // IN
{
   VMCIContext *context;
   VMCILockFlags flags;
   int result;

   if (contextID == VMCI_INVALID_ID || VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_GrabLock(&context->lock, &flags);
   if (!VMCIHandleArray_HasEntry(context->doorbellArray, handle)) {
      VMCIHandleArray_AppendEntry(&context->doorbellArray, handle);
      result = VMCI_SUCCESS;
   } else {
      result = VMCI_ERROR_DUPLICATE_ENTRY;
   }
   VMCI_ReleaseLock(&context->lock, flags);

   VMCIContext_Release(context);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_DoorbellDestroy --
 *
 *      Unregisters a doorbell handle that was previously registered
 *      with VMCIContext_DoorbellCreate.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_DoorbellDestroy(VMCIId contextID,   // IN
                            VMCIHandle handle)  // IN
{
   VMCIContext *context;
   VMCILockFlags flags;
   VMCIHandle removedHandle;

   if (contextID == VMCI_INVALID_ID || VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_GrabLock(&context->lock, &flags);
   removedHandle = VMCIHandleArray_RemoveEntry(context->doorbellArray, handle);
   VMCIHandleArray_RemoveEntry(context->pendingDoorbellArray, handle);
   VMCI_ReleaseLock(&context->lock, flags);

   VMCIContext_Release(context);

   if (VMCI_HANDLE_INVALID(removedHandle)) {
      return VMCI_ERROR_NOT_FOUND;
   } else {
      return VMCI_SUCCESS;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_DoorbellDestroyAll --
 *
 *      Unregisters all doorbell handles that were previously
 *      registered with VMCIContext_DoorbellCreate.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_DoorbellDestroyAll(VMCIId contextID) // IN
{
   VMCIContext *context;
   VMCILockFlags flags;
   VMCIHandle removedHandle;

   if (contextID == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   context = VMCIContext_Get(contextID);
   if (context == NULL) {
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_GrabLock(&context->lock, &flags);
   do {
      removedHandle = VMCIHandleArray_RemoveTail(context->doorbellArray);
   } while(!VMCI_HANDLE_INVALID(removedHandle));
   do {
      removedHandle = VMCIHandleArray_RemoveTail(context->pendingDoorbellArray);
   } while(!VMCI_HANDLE_INVALID(removedHandle));
   VMCI_ReleaseLock(&context->lock, flags);

   VMCIContext_Release(context);

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_NotifyDoorbell --
 *
 *      Registers a notification of a doorbell handle initiated by the
 *      specified source context. The notification of doorbells are
 *      subject to the same isolation rules as datagram delivery. To
 *      allow host side senders of notifications a finer granularity
 *      of sender rights than those assigned to the sending context
 *      itself, the host context is required to specify a different
 *      set of privilege flags that will override the privileges of
 *      the source context.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_NotifyDoorbell(VMCIId srcCID,                   // IN
                           VMCIHandle handle,               // IN
                           VMCIPrivilegeFlags srcPrivFlags) // IN
{
   VMCIContext *dstContext;
   VMCILockFlags flags;
   int result;

   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   /* Get the target VM's VMCI context. */
   dstContext = VMCIContext_Get(handle.context);
   if (dstContext == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid context (ID=0x%x).\n", handle.context));
      return VMCI_ERROR_NOT_FOUND;
   }

   if (srcCID != handle.context) {
      VMCIPrivilegeFlags dstPrivFlags;

      if (VMCI_CONTEXT_IS_VM(srcCID) && VMCI_CONTEXT_IS_VM(handle.context)) {
         VMCI_DEBUG_LOG(4, (LGPFX"Doorbell notification from VM to VM not "
                            "supported (src=0x%x, dst=0x%x).\n",
                            srcCID, handle.context));
         result = VMCI_ERROR_DST_UNREACHABLE;
         goto out;
      }

      result = VMCIDoorbellGetPrivFlags(handle, &dstPrivFlags);
      if (result < VMCI_SUCCESS) {
         VMCI_WARNING((LGPFX"Failed to get privilege flags for destination "
                       "(handle=0x%x:0x%x).\n", handle.context,
                       handle.resource));
         goto out;
      }

      if (srcCID != VMCI_HOST_CONTEXT_ID ||
          srcPrivFlags == VMCI_NO_PRIVILEGE_FLAGS) {
         srcPrivFlags = vmci_context_get_priv_flags(srcCID);
      }

      if (VMCIDenyInteraction(srcPrivFlags, dstPrivFlags)) {
         result = VMCI_ERROR_NO_ACCESS;
         goto out;
      }
   }

   if (handle.context == VMCI_HOST_CONTEXT_ID) {
      result = VMCIDoorbellHostContextNotify(srcCID, handle);
   } else {
      VMCI_GrabLock(&dstContext->lock, &flags);

#if defined(VMKERNEL)
      if (dstContext->inFilters != NULL &&
          VMCIFilterProtoDeny(dstContext->inFilters->filters, handle.resource,
                              VMCI_FP_DOORBELL)) {
         result = VMCI_ERROR_NO_ACCESS;
      } else
#endif // VMKERNEL
      if (!VMCIHandleArray_HasEntry(dstContext->doorbellArray, handle)) {
         result = VMCI_ERROR_NOT_FOUND;
      } else {
         if (!VMCIHandleArray_HasEntry(dstContext->pendingDoorbellArray, handle)) {
            VMCIHandleArray_AppendEntry(&dstContext->pendingDoorbellArray, handle);

            VMCIContextSignalNotify(dstContext);
#if defined(VMKERNEL)
            VMCIHost_SignalBitmap(&dstContext->hostContext);
#else
            VMCIHost_SignalCall(&dstContext->hostContext);
#endif
         }
         result = VMCI_SUCCESS;
      }
      VMCI_ReleaseLock(&dstContext->lock, flags);
   }

out:
   VMCIContext_Release(dstContext);

   return result;
}


#ifdef VMKERNEL

/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SignalPendingDoorbells --
 *
 *      Signals the guest if any doorbell notifications are
 *      pending. This is used after the VMCI device is unquiesced to
 *      ensure that no pending notifications go unnoticed, since
 *      signals may not be fully processed while the device is
 *      quiesced.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_SignalPendingDoorbells(VMCIId contextID)
{
   VMCIContext *context;
   VMCILockFlags flags;
   Bool pending;

   context = VMCIContext_Get(contextID);
   if (!context) {
      ASSERT(FALSE);
      return;
   }

   VMCI_GrabLock(&context->lock, &flags);
   pending = VMCIHandleArray_GetSize(context->pendingDoorbellArray) > 0;
   VMCI_ReleaseLock(&context->lock, flags);

   if (pending) {
      VMCIHost_SignalBitmapAlways(&context->hostContext);
   }

   VMCIContext_Release(context);
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SignalPendingDatagrams --
 *
 *      Signals the guest if any datagrams are pending. This is used
 *      after the VMCI device is unquiesced to ensure that no pending
 *      datagrams go unnoticed, since signals may not be fully
 *      processed while the device is quiesced.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_SignalPendingDatagrams(VMCIId contextID)
{
   Bool pending;
   VMCIContext *context;
   VMCILockFlags flags;

   context = VMCIContext_Get(contextID);
   if (!context) {
      ASSERT(FALSE);
      return;
   }

   VMCI_GrabLock(&context->lock, &flags);
   pending = context->pendingDatagrams;
   VMCI_ReleaseLock(&context->lock, flags);

   if (pending) {
      VMCIHost_SignalCallAlways(&context->hostContext);
   }

   VMCIContext_Release(context);
}

#endif // defined(VMKERNEL)


/*
 *----------------------------------------------------------------------
 *
 * vmci_cid_2_host_vm_id --
 *
 *      Maps a context ID to the host specific (process/world) ID
 *      of the VM/VMX.
 *
 * Results:
 *      VMCI_SUCCESS on success, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_cid_2_host_vm_id)
int
vmci_cid_2_host_vm_id(VMCIId contextID,    // IN
                      void *hostVmID,      // OUT
                      size_t hostVmIDLen)  // IN
{
#if defined(VMKERNEL)
   VMCIContext *context;
   VMCIHostVmID vmID;
   int result;

   context = VMCIContext_Get(contextID);
   if (!context) {
      return VMCI_ERROR_NOT_FOUND;
   }

   result = VMCIHost_ContextToHostVmID(&context->hostContext, &vmID);
   if (result == VMCI_SUCCESS) {
      if (sizeof vmID == hostVmIDLen) {
         memcpy(hostVmID, &vmID, hostVmIDLen);
      } else {
         result = VMCI_ERROR_INVALID_ARGS;
      }
   }

   VMCIContext_Release(context);

   return result;
#else // !defined(VMKERNEL)
   UNREFERENCED_PARAMETER(contextID);
   UNREFERENCED_PARAMETER(hostVmID);
   UNREFERENCED_PARAMETER(hostVmIDLen);
   return VMCI_ERROR_UNAVAILABLE;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * vmci_is_context_owner --
 *
 *      Determines whether a given host OS specific representation of
 *      user is the owner of the VM/VMX.
 *
 * Results:
 *      Linux: 1 (true) if hostUser is owner, 0 (false) otherwise.
 *      Other: VMCI_SUCCESS if the hostUser is owner, error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_is_context_owner)
#if defined(__linux__) && !defined(VMKERNEL)
int
vmci_is_context_owner(VMCIId contextID,   // IN
                      VMCIHostUser uid)          // IN
{
   int isOwner = 0;

   if (VMCI_HostPersonalityActive()) {
      VMCIContext *context = VMCIContext_Get(contextID);
      if (context) {
         if (context->validUser) {
            if (VMCIHost_CompareUser(&uid,
                                     &context->user) == VMCI_SUCCESS) {
               isOwner = 1;
            }
         }
         VMCIContext_Release(context);
      }
   }

   return isOwner;
}
#else // !linux || VMKERNEL
int
vmci_is_context_owner(VMCIId contextID,   // IN
                      void *hostUser)     // IN
{
   if (VMCI_HostPersonalityActive()) {
      VMCIContext *context;
      VMCIHostUser *user = (VMCIHostUser *)hostUser;
      int retval;

      if (vmkernel) {
         return VMCI_ERROR_UNAVAILABLE;
      }

      if (!hostUser) {
         return VMCI_ERROR_INVALID_ARGS;
      }

      context = VMCIContext_Get(contextID);
      if (!context) {
         return VMCI_ERROR_NOT_FOUND;
      }

      if (context->validUser) {
         retval = VMCIHost_CompareUser(user, &context->user);
      } else {
         retval = VMCI_ERROR_UNAVAILABLE;
      }
      VMCIContext_Release(context);

      return retval;
   }
   return VMCI_ERROR_UNAVAILABLE;
}
#endif // !linux || VMKERNEL


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_SupportsHostQP --
 *
 *      Can host QPs be connected to this user process.  The answer is
 *      FALSE unless a sufficient version number has previously been set
 *      by this caller.
 *
 * Results:
 *      TRUE if context supports host queue pairs, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_SupportsHostQP(VMCIContext *context)    // IN: Context structure
{
#ifdef VMKERNEL
   return TRUE;
#else
   if (!context || context->userVersion < VMCI_VERSION_HOSTQP) {
      return FALSE;
   }
   return TRUE;
#endif
}




/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_QueuePairCreate --
 *
 *      Registers that a new queue pair handle has been allocated by
 *      the context.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_QueuePairCreate(VMCIContext *context, // IN: Context structure
                            VMCIHandle handle)    // IN
{
   int result;

   if (context == NULL || VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (!VMCIHandleArray_HasEntry(context->queuePairArray, handle)) {
      VMCIHandleArray_AppendEntry(&context->queuePairArray, handle);
      result = VMCI_SUCCESS;
   } else {
      result = VMCI_ERROR_DUPLICATE_ENTRY;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_QueuePairDestroy --
 *
 *      Unregisters a queue pair handle that was previously registered
 *      with VMCIContext_QueuePairCreate.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherewise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_QueuePairDestroy(VMCIContext *context, // IN: Context structure
                             VMCIHandle handle)    // IN
{
   VMCIHandle removedHandle;

   if (context == NULL || VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   removedHandle = VMCIHandleArray_RemoveEntry(context->queuePairArray, handle);

   if (VMCI_HANDLE_INVALID(removedHandle)) {
      return VMCI_ERROR_NOT_FOUND;
   } else {
      return VMCI_SUCCESS;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_QueuePairExists --
 *
 *      Determines whether a given queue pair handle is registered
 *      with the given context.
 *
 * Results:
 *      TRUE, if queue pair is registered with context. FALSE, otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_QueuePairExists(VMCIContext *context, // IN: Context structure
                            VMCIHandle handle)    // IN
{
   Bool result;

   if (context == NULL || VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIHandleArray_HasEntry(context->queuePairArray, handle);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_RegisterGuestMem --
 *
 *      Tells the context that guest memory is available for
 *      access. This should only be used when unquiescing the VMCI
 *      device of a guest.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Notifies host side endpoints of queue pairs that the queue pairs
 *      can be accessed.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_RegisterGuestMem(VMCIContext *context, // IN: Context structure
                             VMCIGuestMemID gid)   // IN: Reference to guest
{
#ifdef VMKERNEL
   uint32 numQueuePairs;
   uint32 cur;

   VMCIMutex_Acquire(&context->guestMemMutex);

   if (context->curGuestMemID != INVALID_VMCI_GUEST_MEM_ID) {
      if (context->curGuestMemID != gid) {
         /*
          * The guest memory has been registered with a different guest
          * memory ID. This is possible if we attempt to continue the
          * execution of the source VMX following a failed FSR.
          */

         VMCIContextReleaseGuestMemLocked(context, context->curGuestMemID,
                                          FALSE);
      } else {
         /*
          * When unquiescing the device during a restore sync not part
          * of an FSR, we will already have registered the guest
          * memory when creating the device, so we don't need to do it
          * again. Also, there are no active queue pairs at this
          * point, so nothing to do.
          */

         ASSERT(VMCIHandleArray_GetSize(context->queuePairArray) == 0);
         goto out;
      }
   }
   context->curGuestMemID = gid;

   /*
    * It is safe to access the queue pair array here, since no changes
    * to the queuePairArray can take place until after the unquiescing
    * is complete.
    */

   numQueuePairs = VMCIHandleArray_GetSize(context->queuePairArray);
   for (cur = 0; cur < numQueuePairs; cur++) {
      VMCIHandle handle;
      handle = VMCIHandleArray_GetEntry(context->queuePairArray, cur);
      if (!VMCI_HANDLE_EQUAL(handle, VMCI_INVALID_HANDLE)) {
         int res;

         res = VMCIQPBroker_Map(handle, context, NULL);
         if (res < VMCI_SUCCESS) {
            VMCI_WARNING(("Failed to map guest memory for queue pair "
                          "(handle=0x%x:0x%x, res=%d).\n",
                          handle.context, handle.resource, res));
         }
      }
   }

out:
   VMCIMutex_Release(&context->guestMemMutex);
#else
   UNREFERENCED_PARAMETER(context);
   UNREFERENCED_PARAMETER(gid);
#endif
}


#ifdef VMKERNEL
/*
 *----------------------------------------------------------------------
 *
 * VMCIContextReleaseGuestMemLocked --
 *
 *      A version of VMCIContext_ReleaseGuestMem that assumes that the
 *      guest mem lock is already held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
VMCIContextReleaseGuestMemLocked(VMCIContext *context, // IN: Context structure
                                 VMCIGuestMemID gid,   // IN: Reference to guest
                                 Bool powerOff)        // IN: Device going away
{
   uint32 numQueuePairs;
   uint32 cur;

   if (powerOff) {
      VMCIContext_NotifyMemoryAccess(context->cid, FALSE);
   }

   /*
    * It is safe to access the queue pair array here, since no changes
    * to the queuePairArray can take place when the the quiescing
    * has been initiated, or when the device is being cleaned up.
    */

   numQueuePairs = VMCIHandleArray_GetSize(context->queuePairArray);
   for (cur = 0; cur < numQueuePairs; cur++) {
      VMCIHandle handle;
      handle = VMCIHandleArray_GetEntry(context->queuePairArray, cur);
      if (!VMCI_HANDLE_EQUAL(handle, VMCI_INVALID_HANDLE)) {
         int res;

         res = VMCIQPBroker_Unmap(handle, context, gid);
         if (res < VMCI_SUCCESS) {
            VMCI_WARNING(("Failed to unmap guest memory for queue pair "
                          "(handle=0x%x:0x%x, res=%d).\n",
                          handle.context, handle.resource, res));
         }
      }
   }
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_ReleaseGuestMem --
 *
 *      Releases all the contexts references to guest memory, if the
 *      caller identified by the gid was the last one to register the
 *      guest memory. This should only be used when quiescing or
 *      cleaning up the VMCI device of a guest.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCIContext_ReleaseGuestMem(VMCIContext *context, // IN: Context structure
                            VMCIGuestMemID gid,   // IN: Reference to guest
                            Bool powerOff)        // IN: Device is going away
{
#ifdef VMKERNEL
   VMCIMutex_Acquire(&context->guestMemMutex);

   if (context->curGuestMemID == gid) {
      /*
       * In the case of an FSR, we may have multiple VMX'en
       * registering and releasing guest memory concurrently. The
       * common case is that the source will clean up its device state
       * after a successful FSR, where the destination may already
       * have registered guest memory. So we only release guest
       * memory, if this is the same gid, that registered the memory.
       */

      VMCIContextReleaseGuestMemLocked(context, gid, powerOff);
      context->curGuestMemID = INVALID_VMCI_GUEST_MEM_ID;
   }

   VMCIMutex_Release(&context->guestMemMutex);
#else
   UNREFERENCED_PARAMETER(context);
   UNREFERENCED_PARAMETER(gid);
   UNREFERENCED_PARAMETER(powerOff);
#endif
}

#if defined(VMKERNEL)
/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_RevalidateMappings --
 *
 *      Updates the mappings for all QPs.  Should only be called with the VMCI
 *      device lock held.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
VMCIContext_RevalidateMappings(VMCIContext *context) // IN: Context structure
{
   uint32 numQueuePairs;
   uint32 cur;

   numQueuePairs = VMCIHandleArray_GetSize(context->queuePairArray);
   for (cur = 0; cur < numQueuePairs; cur++) {
      VMCIHandle handle;

      handle = VMCIHandleArray_GetEntry(context->queuePairArray, cur);
      if (!VMCI_HANDLE_EQUAL(handle, VMCI_INVALID_HANDLE)) {
         int res = VMCIQPBroker_Revalidate(handle, context);

         if (res < VMCI_SUCCESS) {
            VMCI_WARNING(("Failed to revalidate guest mappings for queue "
                          " pair (handle=0x%x:0x%x, res=%d).\n",
                          handle.context, handle.resource, res));
            /*
             * I have not seen these errors but I do not think they should be
             * considered fatal.
             */
            if (res != VMCI_ERROR_NOT_FOUND &&
                res != VMCI_ERROR_QUEUEPAIR_NOTATTACHED) {
               return FALSE;
            }
         }
      }
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContext_FilterSet --
 *
 *      Sets an ingoing (host to guest) filter for the VMCI firewall of the
 *      given context. If a filter list already exists for the given filter
 *      entry, the old entry will be deleted. It is assumed that the list
 *      can be used as is, and that the memory backing it will be freed by the
 *      VMCI Context module once the filter is deleted.
 *
 * Results:
 *      VMCI_SUCCESS on success,
 *      VMCI_ERROR_NOT_FOUND if there is no active context linked to the cid,
 *      VMCI_ERROR_INVALID_ARGS if a non-VM cid is specified.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCIContext_FilterSet(VMCIId cid,                // IN
                      VMCIFilterState *filters)  // IN
{
   VMCIContext *context;
   VMCILockFlags flags;
   VMCIFilterState *oldState;

   if (!VMCI_CONTEXT_IS_VM(cid)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   context = VMCIContext_Get(cid);
   if (!context) {
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_GrabLock(&context->lock, &flags);

   oldState = context->inFilters;
   context->inFilters = filters;

   VMCI_ReleaseLock(&context->lock, flags);
   if (oldState) {
      VMCIVMKDevFreeFilterState(oldState);
   }
   VMCIContext_Release(context);

   return VMCI_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCIContextInFilterCleanup --
 *
 *      When a context is destroyed, all filters will be deleted.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
VMCIContextInFilterCleanup(VMCIContext *context)
{
   if (context->inFilters != NULL) {
      VMCIVMKDevFreeFilterState(context->inFilters);
      context->inFilters = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * VMCI_Uuid2ContextId --
 *
 *      Given a running VM's UUID, retrieve the VM's VMCI context ID.
 *      The given UUID is local to the host; it is _not_ the UUID
 *      handed out by VC.  It comes from the "bios.uuid" field in the
 *      VMX file.  We walk the context list and try to match the given
 *      UUID against each context.  If we get a match, we return the
 *      contexts's VMCI ID.
 *
 * Results:
 *      VMCI_SUCCESS if found and *contextID contains the CID.
 *      VMCI_ERROR_INVALID_ARGS for bad parameters.
 *      VMCI_ERROR_NOT_FOUND if no match.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
VMCI_Uuid2ContextId(const char *uuidString, // IN
                    VMCIId *contextID)      // OUT
{
   int err;
   VMCIListItem *next;
   VMCILockFlags flags;

   if (!uuidString || *uuidString == '\0' || !contextID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   err = VMCI_ERROR_NOT_FOUND;

   VMCI_GrabLock(&contextList.lock, &flags);
   VMCIList_Scan(next, &contextList.head) {
      VMCIContext *context = VMCIList_Entry(next, VMCIContext, listItem);
      if (VMCIHost_ContextHasUuid(&context->hostContext, uuidString) ==
          VMCI_SUCCESS) {
         *contextID = context->cid;
         err = VMCI_SUCCESS;
         break;
      }
   }
   VMCI_ReleaseLock(&contextList.lock, flags);

   return err;
}
#endif // VMKERNEL
