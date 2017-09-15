/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * vmciQPair.c --
 *
 *      This file implements Queue accessor methods.
 *
 *      VMCIQPair is a new interface that hides the queue pair internals.
 *      Rather than access each queue in a pair directly, operations are now
 *      performed on the queue as a whole.  This is simpler and less
 *      error-prone, and allows for future queue pair features to be added
 *      under the hood with no change to the client code.
 *
 *      This also helps in a particular case on Windows hosts, where the memory
 *      allocated by the client (e.g., VMX) will disappear when the client does
 *      (e.g., abnormal termination).  The kernel can't lock user memory into
 *      its address space indefinitely.  By guarding access to the queue
 *      contents we can correctly handle the case where the client disappears.
 *
 *      On code style:
 *
 *      + This entire file started its life as a cut-and-paste of the
 *        static INLINE functions in bora/public/vmci_queue_pair.h.
 *        From there, new copies of the routines were made named
 *        without the prefix VMCI, without the underscore (the one
 *        that followed VMCIQueue_).  The no-underscore versions of
 *        the routines require that the mutexes are held.
 *
 *      + The code -always- uses the xyzLocked() version of any given
 *        routine even when the wrapped function is a one-liner.  The
 *        reason for this decision was to ensure that we didn't have
 *        copies of logic lying around that needed to be maintained.
 *
 *      + Note that we still pass around 'const VMCIQueue *'s.
 *
 *      + Note that mutex is a field within VMCIQueue.  We skirt the
 *        issue of passing around a const VMCIQueue, even though the
 *        mutex field (__mutex, specifically) will get modified by not
 *        ever referring to the mutex -itself- except during
 *        initialization.  Beyond that, the code only passes the
 *        pointer to the mutex, which is also a member of VMCIQueue,
 *        obviously, and which doesn't change after initialization.
 *        This eliminates having to redefine all the functions that
 *        are currently taking const VMCIQueue's so that these
 *        functions are compatible with those definitions.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_handle_array.h"
#include "vmci_defs.h"
#include "vmciKernelAPI.h"
#include "vmciQueue.h"
#include "vmciQueuePair.h"
#include "vmciRoute.h"


/*
 * VMCIQPair
 *
 *      This structure is opaque to the clients.
 */

struct VMCIQPair {
   VMCIHandle handle;
   VMCIQueue *produceQ;
   VMCIQueue *consumeQ;
   uint64 produceQSize;
   uint64 consumeQSize;
   VMCIId peer;
   uint32 flags;
   VMCIPrivilegeFlags privFlags;
   Bool guestEndpoint;
   uint32 blocked;
   VMCIEvent event;
};

static int VMCIQPairMapQueueHeaders(VMCIQueue *produceQ, VMCIQueue *consumeQ,
                                    Bool canBlock);
static int VMCIQPairGetQueueHeaders(const VMCIQPair *qpair,
                                    VMCIQueueHeader **produceQHeader,
                                    VMCIQueueHeader **consumeQHeader);
static int VMCIQPairWakeupCB(void *clientData);
static int VMCIQPairReleaseMutexCB(void *clientData);
static Bool VMCIQPairWaitForReadyQueue(VMCIQPair *qpair);


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairLock --
 *
 *      Helper routine that will lock the QPair before subsequent operations.
 *
 * Results:
 *      VMCI_SUCCESS if lock acquired. VMCI_ERROR_WOULD_BLOCK if queue mutex
 *      couldn't be acquired and qpair isn't allowed to block.
 *
 * Side effects:
 *      May block.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIQPairLock(const VMCIQPair *qpair) // IN
{
#if !defined VMX86_VMX
   return VMCI_AcquireQueueMutex(qpair->produceQ,
                                 !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
#else
   return VMCI_SUCCESS
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairUnlock --
 *
 *      Helper routine that will unlock the QPair after various operations.
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
VMCIQPairUnlock(const VMCIQPair *qpair) // IN
{
#if !defined VMX86_VMX
   VMCI_ReleaseQueueMutex(qpair->produceQ);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairHeaderLock --
 *
 *      Helper routine that will lock the queue pair header before subsequent
 *      operations. If the queue pair is non blocking, a spinlock will be used.
 *      Otherwise, a regular mutex locking the complete queue pair will be used.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May block.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQPairLockHeader(const VMCIQPair *qpair) // IN
{
#if !defined VMX86_VMX
   if (qpair->flags & VMCI_QPFLAG_NONBLOCK) {
      VMCI_LockQueueHeader(qpair->produceQ);
   } else {
      (void)VMCI_AcquireQueueMutex(qpair->produceQ,
                                   !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairUnlockHeader --
 *
 *      Helper routine that unlocks the queue pair header after calling
 *      VMCIQPairHeaderLock.
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
VMCIQPairUnlockHeader(const VMCIQPair *qpair) // IN
{
#if !defined VMX86_VMX
   if (qpair->flags & VMCI_QPFLAG_NONBLOCK) {
      VMCI_UnlockQueueHeader(qpair->produceQ);
   } else {
      VMCI_ReleaseQueueMutex(qpair->produceQ);
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueAddProducerTail() --
 *
 *      Helper routine to increment the Producer Tail.
 *
 * Results:
 *      VMCI_ERROR_NOT_FOUND if the vmmWorld registered with the queue
 *      cannot be found. Otherwise VMCI_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIQueueAddProducerTail(VMCIQueue *queue, // IN/OUT
                         size_t add,       // IN
                         uint64 queueSize) // IN
{
   VMCIQueueHeader_AddProducerTail(queue->qHeader, add, queueSize);
   return VMCI_QueueHeaderUpdated(queue);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueAddConsumerHead() --
 *
 *      Helper routine to increment the Consumer Head.
 *
 * Results:
 *      VMCI_ERROR_NOT_FOUND if the vmmWorld registered with the queue
 *      cannot be found. Otherwise VMCI_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIQueueAddConsumerHead(VMCIQueue *queue, // IN/OUT
                         size_t add,       // IN
                         uint64 queueSize) // IN
{
   VMCIQueueHeader_AddConsumerHead(queue->qHeader, add, queueSize);
   return VMCI_QueueHeaderUpdated(queue);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairGetQueueHeaders --
 *
 *      Helper routine that will retrieve the produce and consume
 *      headers of a given queue pair. If the guest memory of the
 *      queue pair is currently not available, the saved queue headers
 *      will be returned, if these are available.
 *
 * Results:
 *      VMCI_SUCCESS if either current or saved queue headers are found.
 *      Appropriate error code otherwise.
 *
 * Side effects:
 *      May block.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPairGetQueueHeaders(const VMCIQPair *qpair,            // IN
                         VMCIQueueHeader **produceQHeader,  // OUT
                         VMCIQueueHeader **consumeQHeader)  // OUT
{
   int result;

   result = VMCIQPairMapQueueHeaders(qpair->produceQ, qpair->consumeQ,
                                     !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
   if (result == VMCI_SUCCESS) {
      *produceQHeader = qpair->produceQ->qHeader;
      *consumeQHeader = qpair->consumeQ->qHeader;
   } else if (qpair->produceQ->savedHeader && qpair->consumeQ->savedHeader) {
      ASSERT(!qpair->guestEndpoint);
      *produceQHeader = qpair->produceQ->savedHeader;
      *consumeQHeader = qpair->consumeQ->savedHeader;
      result = VMCI_SUCCESS;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairMapQueueHeaders --
 *
 *      The queue headers may not be mapped at all times. If a queue is
 *      currently not mapped, it will be attempted to do so.
 *
 * Results:
 *      VMCI_SUCCESS if queues were validated, appropriate error code otherwise.
 *
 * Side effects:
 *      May attempt to map in guest memory.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPairMapQueueHeaders(VMCIQueue *produceQ, // IN
                         VMCIQueue *consumeQ, // IN
                         Bool canBlock)       // IN
{
   int result;

   if (NULL == produceQ->qHeader || NULL == consumeQ->qHeader) {
      if (canBlock) {
         result = VMCIHost_MapQueues(produceQ, consumeQ, 0);
      } else {
         result = VMCI_ERROR_QUEUEPAIR_NOT_READY;
      }
      if (result < VMCI_SUCCESS) {
         if (produceQ->savedHeader && consumeQ->savedHeader) {
            return VMCI_ERROR_QUEUEPAIR_NOT_READY;
         } else {
            return VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
         }
      }
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairWakeupCB --
 *
 *      Callback from VMCI queue pair broker indicating that a queue
 *      pair that was previously not ready, now either is ready or
 *      gone forever.
 *
 * Results:
 *      VMCI_SUCCESS always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPairWakeupCB(void *clientData)
{
   VMCIQPair *qpair = (VMCIQPair *)clientData;
   ASSERT(qpair);

   VMCIQPairLock(qpair);
   while (qpair->blocked > 0) {
      qpair->blocked--;
      VMCI_SignalEvent(&qpair->event);
   }
   VMCIQPairUnlock(qpair);

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairReleaseMutexCB --
 *
 *      Callback from VMCI_WaitOnEvent releasing the queue pair mutex
 *      protecting the queue pair header state.
 *
 * Results:
 *      0 always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMCIQPairReleaseMutexCB(void *clientData)
{
   VMCIQPair *qpair = (VMCIQPair *)clientData;
   ASSERT(qpair);
   VMCIQPairUnlock(qpair);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPairWaitForReadyQueue --
 *
 *      Makes the calling thread wait for the queue pair to become
 *      ready for host side access.
 *
 * Results:
 *     TRUE when thread is woken up after queue pair state change.
 *     FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
VMCIQPairWaitForReadyQueue(VMCIQPair *qpair)
{
   if (UNLIKELY(qpair->guestEndpoint)) {
      ASSERT(FALSE);
      return FALSE;
   }
   if (qpair->flags & VMCI_QPFLAG_NONBLOCK) {
      return FALSE;
   }
   qpair->blocked++;
   VMCI_WaitOnEvent(&qpair->event, VMCIQPairReleaseMutexCB, qpair);
   VMCIQPairLock(qpair);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_alloc --
 *
 *      This is the client interface for allocating the memory for a
 *      VMCIQPair structure and then attaching to the underlying
 *      queue.  If an error occurs allocating the memory for the
 *      VMCIQPair structure, no attempt is made to attach.  If an
 *      error occurs attaching, then there's the VMCIQPair structure
 *      is freed.
 *
 * Results:
 *      An err, if < 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_alloc)
int
vmci_qpair_alloc(VMCIQPair **qpair,            // OUT
                 VMCIHandle *handle,           // OUT
                 uint64 produceQSize,          // IN
                 uint64 consumeQSize,          // IN
                 VMCIId peer,                  // IN
                 uint32 flags,                 // IN
                 VMCIPrivilegeFlags privFlags) // IN
{
   VMCIQPair *myQPair;
   int retval;
   VMCIHandle src = VMCI_INVALID_HANDLE;
   VMCIHandle dst = VMCI_MAKE_HANDLE(peer, VMCI_INVALID_ID);
   VMCIRoute route;
   VMCIEventReleaseCB wakeupCB;
   void *clientData;

   /*
    * Restrict the size of a queuepair.  The device already enforces a limit
    * on the total amount of memory that can be allocated to queuepairs for a
    * guest.  However, we try to allocate this memory before we make the
    * queuepair allocation hypercall.  On Windows and Mac OS, we request a
    * single, continguous block, and it will fail if the OS cannot satisfy the
    * request. On Linux, we allocate each page separately, which means rather
    * than fail, the guest will thrash while it tries to allocate, and will
    * become increasingly unresponsive to the point where it appears to be hung.
    * So we place a limit on the size of an individual queuepair here, and
    * leave the device to enforce the restriction on total queuepair memory.
    * (Note that this doesn't prevent all cases; a user with only this much
    * physical memory could still get into trouble.)  The error used by the
    * device is NO_RESOURCES, so use that here too.
    */

   if (produceQSize + consumeQSize < MAX(produceQSize, consumeQSize) ||
       produceQSize + consumeQSize > VMCI_MAX_GUEST_QP_MEMORY) {
      return VMCI_ERROR_NO_RESOURCES;
   }

   retval = VMCI_Route(&src, &dst, FALSE, &route);
   if (retval < VMCI_SUCCESS) {
      if (VMCI_GuestPersonalityActive()) {
         route = VMCI_ROUTE_AS_GUEST;
      } else {
         route = VMCI_ROUTE_AS_HOST;
      }
   }

   if (flags & VMCI_QPFLAG_NONBLOCK && !vmkernel) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   myQPair = VMCI_AllocKernelMem(sizeof *myQPair, VMCI_MEMORY_NONPAGED);
   if (!myQPair) {
      return VMCI_ERROR_NO_MEM;
   }

   myQPair->produceQSize = produceQSize;
   myQPair->consumeQSize = consumeQSize;
   myQPair->peer = peer;
   myQPair->flags = flags;
   myQPair->privFlags = privFlags;

   clientData = NULL;
   wakeupCB = NULL;
   if (VMCI_ROUTE_AS_HOST == route) {
      myQPair->guestEndpoint = FALSE;
      if (!(flags & VMCI_QPFLAG_LOCAL)) {
         myQPair->blocked = 0;
         VMCI_CreateEvent(&myQPair->event);
         wakeupCB = VMCIQPairWakeupCB;
         clientData = (void *)myQPair;
      }
   } else {
      myQPair->guestEndpoint = TRUE;
   }

   retval = VMCIQueuePair_Alloc(handle,
                                &myQPair->produceQ,
                                myQPair->produceQSize,
                                &myQPair->consumeQ,
                                myQPair->consumeQSize,
                                myQPair->peer,
                                myQPair->flags,
                                myQPair->privFlags,
                                myQPair->guestEndpoint,
                                wakeupCB,
                                clientData);

   if (retval < VMCI_SUCCESS) {
      if (VMCI_ROUTE_AS_HOST == route && !(flags & VMCI_QPFLAG_LOCAL)) {
         VMCI_DestroyEvent(&myQPair->event);
      }
      VMCI_FreeKernelMem(myQPair, sizeof *myQPair);
      return retval;
   }

   *qpair = myQPair;
   myQPair->handle = *handle;

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_detach --
 *
 *      This is the client interface for detaching from a VMCIQPair.
 *      Note that this routine will free the memory allocated for the
 *      VMCIQPair structure, too.
 *
 * Results:
 *      An error, if < 0.
 *
 * Side effects:
 *      Will clear the caller's pointer to the VMCIQPair structure.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_detach)
int
vmci_qpair_detach(VMCIQPair **qpair) // IN/OUT
{
   int result;
   VMCIQPair *oldQPair;

   if (!qpair || !(*qpair)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   oldQPair = *qpair;
   result = VMCIQueuePair_Detach(oldQPair->handle, oldQPair->guestEndpoint);

   /*
    * The guest can fail to detach for a number of reasons, and if it does so,
    * it will cleanup the entry (if there is one).  The host can fail too, but
    * it won't cleanup the entry immediately, it will do that later when the
    * context is freed.  Either way, we need to release the qpair struct here;
    * there isn't much the caller can do, and we don't want to leak.
    */

   if (!(oldQPair->guestEndpoint || (oldQPair->flags & VMCI_QPFLAG_LOCAL))) {
      VMCI_DestroyEvent(&oldQPair->event);
   }
   VMCI_FreeKernelMem(oldQPair, sizeof *oldQPair);
   *qpair = NULL;

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_get_produce_indexes --
 *
 *      This is the client interface for getting the current indexes of the
 *      QPair from the point of the view of the caller as the producer.
 *
 * Results:
 *      err, if < 0
 *      Success otherwise.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_get_produce_indexes)
int
vmci_qpair_get_produce_indexes(const VMCIQPair *qpair, // IN
                               uint64 *producerTail,   // OUT
                               uint64 *consumerHead)   // OUT
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      VMCIQueueHeader_GetPointers(produceQHeader, consumeQHeader,
                                  producerTail, consumerHead);
   }
   VMCIQPairUnlockHeader(qpair);

   if (result == VMCI_SUCCESS &&
       ((producerTail && *producerTail >= qpair->produceQSize) ||
        (consumerHead && *consumerHead >= qpair->produceQSize))) {
      return VMCI_ERROR_INVALID_SIZE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_get_consume_indexes --
 *
 *      This is the client interface for getting the current indexes of the
 *      QPair from the point of the view of the caller as the consumer.
 *
 * Results:
 *      err, if < 0
 *      Success otherwise.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_get_consume_indexes)
int
vmci_qpair_get_consume_indexes(const VMCIQPair *qpair, // IN
                               uint64 *consumerTail,   // OUT
                               uint64 *producerHead)   // OUT
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      VMCIQueueHeader_GetPointers(consumeQHeader, produceQHeader,
                                  consumerTail, producerHead);
   }
   VMCIQPairUnlockHeader(qpair);

   if (result == VMCI_SUCCESS &&
       ((consumerTail && *consumerTail >= qpair->consumeQSize) ||
        (producerHead && *producerHead >= qpair->consumeQSize))) {
      return VMCI_ERROR_INVALID_SIZE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_produce_free_space --
 *
 *      This is the client interface for getting the amount of free
 *      space in the QPair from the point of the view of the caller as
 *      the producer which is the common case.
 *
 * Results:
 *      Err, if < 0.
 *      Full queue if = 0.
 *      Number of available bytes into which data can be enqueued if > 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_produce_free_space)
int64
vmci_qpair_produce_free_space(const VMCIQPair *qpair) // IN
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      result = VMCIQueueHeader_FreeSpace(produceQHeader, consumeQHeader,
                                         qpair->produceQSize);
   } else {
      result = 0;
   }
   VMCIQPairUnlockHeader(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_consume_free_space --
 *
 *      This is the client interface for getting the amount of free
 *      space in the QPair from the point of the view of the caller as
 *      the consumer which is not the common case (see
 *      VMCIQPair_ProduceFreeSpace(), above).
 *
 * Results:
 *      Err, if < 0.
 *      Full queue if = 0.
 *      Number of available bytes into which data can be enqueued if > 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_consume_free_space)
int64
vmci_qpair_consume_free_space(const VMCIQPair *qpair) // IN
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      result = VMCIQueueHeader_FreeSpace(consumeQHeader, produceQHeader,
                                         qpair->consumeQSize);
   } else {
      result = 0;
   }
   VMCIQPairUnlockHeader(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_produce_buf_ready --
 *
 *      This is the client interface for getting the amount of
 *      enqueued data in the QPair from the point of the view of the
 *      caller as the producer which is not the common case (see
 *      VMCIQPair_ConsumeBufReady(), above).
 *
 * Results:
 *      Err, if < 0.
 *      Empty queue if = 0.
 *      Number of bytes ready to be dequeued if > 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_produce_buf_ready)
int64
vmci_qpair_produce_buf_ready(const VMCIQPair *qpair) // IN
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      result = VMCIQueueHeader_BufReady(produceQHeader, consumeQHeader,
                                        qpair->produceQSize);
   } else {
      result = 0;
   }
   VMCIQPairUnlockHeader(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_consume_buf_ready --
 *
 *      This is the client interface for getting the amount of
 *      enqueued data in the QPair from the point of the view of the
 *      caller as the consumer which is the normal case.
 *
 * Results:
 *      Err, if < 0.
 *      Empty queue if = 0.
 *      Number of bytes ready to be dequeued if > 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_consume_buf_ready)
int64
vmci_qpair_consume_buf_ready(const VMCIQPair *qpair) // IN
{
   VMCIQueueHeader *produceQHeader;
   VMCIQueueHeader *consumeQHeader;
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLockHeader(qpair);
   result = VMCIQPairGetQueueHeaders(qpair, &produceQHeader, &consumeQHeader);
   if (result == VMCI_SUCCESS) {
      result = VMCIQueueHeader_BufReady(consumeQHeader, produceQHeader,
                                        qpair->consumeQSize);
   } else {
      result = 0;
   }
   VMCIQPairUnlockHeader(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * EnqueueLocked --
 *
 *      Enqueues a given buffer to the produce queue using the provided
 *      function. As many bytes as possible (space available in the queue)
 *      are enqueued.
 *
 *      Assumes the queue->mutex has been acquired.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NOSPACE if no space was available to enqueue data.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue
 *      (as defined by the queue size).
 *      VMCI_ERROR_INVALID_ARGS, if an error occured when accessing the buffer.
 *      VMCI_ERROR_QUEUEPAIR_NOTATTACHED, if the queue pair pages aren't
 *      available.
 *      VMCI_ERROR_NOT_FOUND, if the vmmWorld registered with the queue pair
 *      cannot be found.
 *      Otherwise, the number of bytes written to the queue is returned.
 *
 * Side effects:
 *      Updates the tail pointer of the produce queue.
 *
 *-----------------------------------------------------------------------------
 */

static ssize_t
EnqueueLocked(VMCIQueue *produceQ,                   // IN
              VMCIQueue *consumeQ,                   // IN
              const uint64 produceQSize,             // IN
              const void *buf,                       // IN
              size_t bufSize,                        // IN
              int bufType,                           // IN
              VMCIMemcpyToQueueFunc memcpyToQueue,   // IN
              Bool canBlock)                         // IN
{
   int64 freeSpace;
   uint64 tail;
   size_t written;
   ssize_t result;

#if !defined VMX86_VMX
   if (UNLIKELY(VMCI_EnqueueToDevNull(produceQ))) {
      return (ssize_t) bufSize;
   }

   result = VMCIQPairMapQueueHeaders(produceQ, consumeQ, canBlock);
   if (UNLIKELY(result != VMCI_SUCCESS)) {
      return result;
   }
#endif

   freeSpace = VMCIQueueHeader_FreeSpace(produceQ->qHeader,
                                         consumeQ->qHeader,
                                         produceQSize);
   if (freeSpace == 0) {
      return VMCI_ERROR_QUEUEPAIR_NOSPACE;
   }

   if (freeSpace < VMCI_SUCCESS) {
      return (ssize_t)freeSpace;
   }

   written = (size_t)(freeSpace > bufSize ? bufSize : freeSpace);
   tail = VMCIQueueHeader_ProducerTail(produceQ->qHeader);
   if (LIKELY(tail + written < produceQSize)) {
      result = memcpyToQueue(produceQ, tail, buf, 0, written, bufType,
                             canBlock);
   } else {
      /* Tail pointer wraps around. */

      const size_t tmp = (size_t)(produceQSize - tail);

      result = memcpyToQueue(produceQ, tail, buf, 0, tmp, bufType, canBlock);
      if (result >= VMCI_SUCCESS) {
         result = memcpyToQueue(produceQ, 0, buf, tmp, written - tmp, bufType,
                                canBlock);
      }
   }

   if (result < VMCI_SUCCESS) {
      return result;
   }

   result = VMCIQueueAddProducerTail(produceQ, written, produceQSize);
   if (result < VMCI_SUCCESS) {
      return result;
   }
   return written;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DequeueLocked --
 *
 *      Dequeues data (if available) from the given consume queue. Writes data
 *      to the user provided buffer using the provided function.
 *
 *      Assumes the queue->mutex has been acquired.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NODATA if no data was available to dequeue.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue
 *      (as defined by the queue size).
 *      VMCI_ERROR_INVALID_ARGS, if an error occured when accessing the buffer.
 *      VMCI_ERROR_NOT_FOUND, if the vmmWorld registered with the queue pair
 *      cannot be found.
 *      Otherwise the number of bytes dequeued is returned.
 *
 * Side effects:
 *      Updates the head pointer of the consume queue.
 *
 *-----------------------------------------------------------------------------
 */

static ssize_t
DequeueLocked(VMCIQueue *produceQ,                        // IN
              VMCIQueue *consumeQ,                        // IN
              const uint64 consumeQSize,                  // IN
              void *buf,                                  // IN
              size_t bufSize,                             // IN
              int bufType,                                // IN
              VMCIMemcpyFromQueueFunc memcpyFromQueue,    // IN
              Bool updateConsumer,                        // IN
              Bool canBlock)                              // IN
{
   int64 bufReady;
   uint64 head;
   size_t read;
   ssize_t result;

#if !defined VMX86_VMX
   result = VMCIQPairMapQueueHeaders(produceQ, consumeQ, canBlock);
   if (UNLIKELY(result != VMCI_SUCCESS)) {
      return result;
   }
#endif

   bufReady = VMCIQueueHeader_BufReady(consumeQ->qHeader,
                                       produceQ->qHeader,
                                       consumeQSize);
   if (bufReady == 0) {
      return VMCI_ERROR_QUEUEPAIR_NODATA;
   }
   if (bufReady < VMCI_SUCCESS) {
      return (ssize_t)bufReady;
   }

   read = (size_t)(bufReady > bufSize ? bufSize : bufReady);
   head = VMCIQueueHeader_ConsumerHead(produceQ->qHeader);
   if (LIKELY(head + read < consumeQSize)) {
      result = memcpyFromQueue(buf, 0, consumeQ, head, read, bufType, canBlock);
   } else {
      /* Head pointer wraps around. */

      const size_t tmp = (size_t)(consumeQSize - head);

      result = memcpyFromQueue(buf, 0, consumeQ, head, tmp, bufType, canBlock);
      if (result >= VMCI_SUCCESS) {
         result = memcpyFromQueue(buf, tmp, consumeQ, 0, read - tmp, bufType,
                                  canBlock);
      }
   }

   if (result < VMCI_SUCCESS) {
      return result;
   }

   if (updateConsumer) {
      result = VMCIQueueAddConsumerHead(produceQ, read, consumeQSize);
      if (result < VMCI_SUCCESS) {
         return result;
      }
   }

   return read;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_enqueue --
 *
 *      This is the client interface for enqueueing data into the queue.
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes enqueued if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_enqueue)
ssize_t
vmci_qpair_enqueue(VMCIQPair *qpair,        // IN
                   const void *buf,         // IN
                   size_t bufSize,          // IN
                   int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = EnqueueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->produceQSize,
                             buf, bufSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyToQueueLocal:
                             VMCIMemcpyToQueue,
                             !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_dequeue --
 *
 *      This is the client interface for dequeueing data from the queue.
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes dequeued if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_dequeue)
ssize_t
vmci_qpair_dequeue(VMCIQPair *qpair,        // IN
                   void *buf,               // IN
                   size_t bufSize,          // IN
                   int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = DequeueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->consumeQSize,
                             buf, bufSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyFromQueueLocal:
                             VMCIMemcpyFromQueue,
                             TRUE, !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_peek --
 *
 *      This is the client interface for peeking into a queue.  (I.e.,
 *      copy data from the queue without updating the head pointer.)
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes peeked, if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_peek)
ssize_t
vmci_qpair_peek(VMCIQPair *qpair,    // IN
                void *buf,           // IN
                size_t bufSize,      // IN
                int bufType)         // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = DequeueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->consumeQSize,
                             buf, bufSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyFromQueueLocal:
                             VMCIMemcpyFromQueue,
                             FALSE, !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}


#if (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__))    || \
    (defined(_WIN32)    && defined(WINNT_DDK))

/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_enquev --
 *
 *      This is the client interface for enqueueing data into the queue.
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes enqueued if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_enquev)
ssize_t
vmci_qpair_enquev(VMCIQPair *qpair,        // IN
                  void *iov,               // IN
                  size_t iovSize,          // IN
                  int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = EnqueueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->produceQSize,
                             iov, iovSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyToQueueVLocal:
                             VMCIMemcpyToQueueV,
                             !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_dequev --
 *
 *      This is the client interface for dequeueing data from the queue.
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes dequeued if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_dequev)
ssize_t
vmci_qpair_dequev(VMCIQPair *qpair,         // IN
                  void *iov,                // IN
                  size_t iovSize,           // IN
                  int bufType)              // IN
{
   ssize_t result;

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = DequeueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->consumeQSize,
                             iov, iovSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyFromQueueVLocal:
                             VMCIMemcpyFromQueueV,
                             TRUE, !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmci_qpair_peekv --
 *
 *      This is the client interface for peeking into a queue.  (I.e.,
 *      copy data from the queue without updating the head pointer.)
 *
 * Results:
 *      Err, if < 0.
 *      Number of bytes peeked, if >= 0.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_qpair_peekv)
ssize_t
vmci_qpair_peekv(VMCIQPair *qpair,           // IN
                 void *iov,                  // IN
                 size_t iovSize,             // IN
                 int bufType)                // IN
{
   ssize_t result;

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = VMCIQPairLock(qpair);
   if (result != VMCI_SUCCESS) {
      return result;
   }

   do {
      result = DequeueLocked(qpair->produceQ,
                             qpair->consumeQ,
                             qpair->consumeQSize,
                             iov, iovSize, bufType,
                             qpair->flags & VMCI_QPFLAG_LOCAL?
                             VMCIMemcpyFromQueueVLocal:
                             VMCIMemcpyFromQueueV,
                             FALSE, !(qpair->flags & VMCI_QPFLAG_NONBLOCK));
      if (result == VMCI_ERROR_QUEUEPAIR_NOT_READY) {
         if (!VMCIQPairWaitForReadyQueue(qpair)) {
            result = VMCI_ERROR_WOULD_BLOCK;
         }
      }
   } while (result == VMCI_ERROR_QUEUEPAIR_NOT_READY);

   VMCIQPairUnlock(qpair);

   return result;
}

#endif /* Systems that support struct iovec */
