/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
};

#define VMCI_QPAIR_NO_QUEUE(_qp) (!(_qp)->produceQ->qHeader || \
                                  !(_qp)->consumeQ->qHeader)


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Alloc --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_Alloc)
int
VMCIQPair_Alloc(VMCIQPair **qpair,            // OUT
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

   myQPair = VMCI_AllocKernelMem(sizeof *myQPair, VMCI_MEMORY_NONPAGED);
   if (!myQPair) {
      return VMCI_ERROR_NO_MEM;
   }
   memset(myQPair, 0, sizeof *myQPair);

   myQPair->produceQSize = produceQSize;
   myQPair->consumeQSize = consumeQSize;
   myQPair->peer = peer;
   myQPair->flags = flags;
   myQPair->privFlags = privFlags;

   retval = VMCI_Route(&src, &dst, FALSE, &route);
   if (retval < VMCI_SUCCESS) {
      if (VMCI_GuestPersonalityActive()) {
         route = VMCI_ROUTE_AS_GUEST;
      } else {
         route = VMCI_ROUTE_AS_HOST;
      }
   }

   if (VMCI_ROUTE_AS_HOST == route) {
      myQPair->guestEndpoint = FALSE;
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
                                myQPair->guestEndpoint);

   if (retval < VMCI_SUCCESS) {
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
 * VMCIQPair_Detach --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_Detach)
int
VMCIQPair_Detach(VMCIQPair **qpair) // IN/OUT
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

   memset(oldQPair, 0, sizeof *oldQPair);
   oldQPair->handle = VMCI_INVALID_HANDLE;
   oldQPair->peer = VMCI_INVALID_ID;
   VMCI_FreeKernelMem(oldQPair, sizeof *oldQPair);
   *qpair = NULL;

   return result;
}


/*
 * "Windows blocking call."
 *
 *      Note that on the Windows platform, kernel module clients may
 *      block when calling into any these rouintes.  The reason is
 *      that a mutex has to be acquired in order to view/modify the
 *      VMCIQueue structure fields: pointers, handle, and buffer data.
 *      However, other platforms don't require the acquisition of a
 *      mutex and thus don't block.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Lock --
 *
 *      Helper routine that will lock the QPair before subsequent operations.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Windows blocking call.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
VMCIQPairLock(const VMCIQPair *qpair) // IN
{
#if !defined VMX86_VMX
   VMCI_AcquireQueueMutex(qpair->produceQ);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Unlock --
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
 * VMCIQPair_GetProduceIndexes --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_GetProduceIndexes)
int
VMCIQPair_GetProduceIndexes(const VMCIQPair *qpair, // IN
                            uint64 *producerTail,   // OUT
                            uint64 *consumerHead)   // OUT
{
   int result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
   } else {
      VMCIQueueHeader_GetPointers(qpair->produceQ->qHeader,
                                  qpair->consumeQ->qHeader,
                                  producerTail,
                                  consumerHead);
      result = VMCI_SUCCESS;
   }

   VMCIQPairUnlock(qpair);

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
 * VMCIQPair_GetConsumeIndexes --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_GetConsumeIndexes)
int
VMCIQPair_GetConsumeIndexes(const VMCIQPair *qpair, // IN
                            uint64 *consumerTail,   // OUT
                            uint64 *producerHead)   // OUT
{
   int result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
   } else {
      VMCIQueueHeader_GetPointers(qpair->consumeQ->qHeader,
                                  qpair->produceQ->qHeader,
                                  consumerTail,
                                  producerHead);
      result = VMCI_SUCCESS;
   }

   VMCIQPairUnlock(qpair);

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
 * VMCIQPair_ProduceFreeSpace --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_ProduceFreeSpace)
int64
VMCIQPair_ProduceFreeSpace(const VMCIQPair *qpair) // IN
{
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = 0;
   } else {
      result = VMCIQueueHeader_FreeSpace(qpair->produceQ->qHeader,
                                         qpair->consumeQ->qHeader,
                                         qpair->produceQSize);
   }

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_ConsumeFreeSpace --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_ConsumeFreeSpace)
int64
VMCIQPair_ConsumeFreeSpace(const VMCIQPair *qpair) // IN
{
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = 0;
   } else {
      result = VMCIQueueHeader_FreeSpace(qpair->consumeQ->qHeader,
                                         qpair->produceQ->qHeader,
                                         qpair->consumeQSize);
   }

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_ProduceBufReady --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_ProduceBufReady)
int64
VMCIQPair_ProduceBufReady(const VMCIQPair *qpair) // IN
{
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = 0;
   } else {
      result = VMCIQueueHeader_BufReady(qpair->produceQ->qHeader,
                                        qpair->consumeQ->qHeader,
                                        qpair->produceQSize);
   }

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_ConsumeBufReady --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_ConsumeBufReady)
int64
VMCIQPair_ConsumeBufReady(const VMCIQPair *qpair) // IN
{
   int64 result;

   if (!qpair) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   if (UNLIKELY(VMCI_QPAIR_NO_QUEUE(qpair))) {
      result = 0;
   } else {
      result = VMCIQueueHeader_BufReady(qpair->consumeQ->qHeader,
                                        qpair->produceQ->qHeader,
                                        qpair->consumeQSize);
   }

   VMCIQPairUnlock(qpair);

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
 *      Otherwise, the number of bytes written to the queue is returned.
 *
 * Side effects:
 *      Updates the tail pointer of the produce queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
EnqueueLocked(VMCIQueue *produceQ,                   // IN
              const VMCIQueue *consumeQ,             // IN
              const uint64 produceQSize,             // IN
              const void *buf,                       // IN
              size_t bufSize,                        // IN
              int bufType,                           // IN
              VMCIMemcpyToQueueFunc memcpyToQueue)   // IN
{
   int64 freeSpace;
   uint64 tail;
   size_t written;
   ssize_t result;

#if !defined VMX86_VMX
   if (UNLIKELY(VMCI_EnqueueToDevNull(produceQ))) {
      return (ssize_t) bufSize;
   }

   if (UNLIKELY(!produceQ->qHeader || !consumeQ->qHeader)) {
      return VMCI_ERROR_QUEUEPAIR_NOTATTACHED;
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
      result = memcpyToQueue(produceQ, tail, buf, 0, written, bufType);
   } else {
      /* Tail pointer wraps around. */

      const size_t tmp = (size_t)(produceQSize - tail);

      result = memcpyToQueue(produceQ, tail, buf, 0, tmp, bufType);
      if (result >= VMCI_SUCCESS) {
         result = memcpyToQueue(produceQ, 0, buf, tmp, written - tmp, bufType);
      }
   }

   if (result < VMCI_SUCCESS) {
      return result;
   }

   VMCIQueueHeader_AddProducerTail(produceQ->qHeader, written, produceQSize);
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
 *      Otherwise the number of bytes dequeued is returned.
 *
 * Side effects:
 *      Updates the head pointer of the consume queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
DequeueLocked(VMCIQueue *produceQ,                        // IN
              const VMCIQueue *consumeQ,                  // IN
              const uint64 consumeQSize,                  // IN
              void *buf,                                  // IN
              size_t bufSize,                             // IN
              int bufType,                                // IN
              VMCIMemcpyFromQueueFunc memcpyFromQueue,    // IN
              Bool updateConsumer)                        // IN
{
   int64 bufReady;
   uint64 head;
   size_t read;
   ssize_t result;

#if !defined VMX86_VMX
   if (UNLIKELY(!produceQ->qHeader ||
                !consumeQ->qHeader)) {
      return VMCI_ERROR_QUEUEPAIR_NODATA;
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
      result = memcpyFromQueue(buf, 0, consumeQ, head, read, bufType);
   } else {
      /* Head pointer wraps around. */

      const size_t tmp = (size_t)(consumeQSize - head);

      result = memcpyFromQueue(buf, 0, consumeQ, head, tmp, bufType);
      if (result >= VMCI_SUCCESS) {
         result = memcpyFromQueue(buf, tmp, consumeQ, 0, read - tmp, bufType);
      }
   }

   if (result < VMCI_SUCCESS) {
      return result;
   }

   if (updateConsumer) {
      VMCIQueueHeader_AddConsumerHead(produceQ->qHeader,
                                      read,
                                      consumeQSize);
   }

   return read;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Enqueue --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_Enqueue)
ssize_t
VMCIQPair_Enqueue(VMCIQPair *qpair,        // IN
                  const void *buf,         // IN
                  size_t bufSize,          // IN
                  int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   result = EnqueueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->produceQSize,
                          buf, bufSize, bufType,
                          VMCIMemcpyToQueue);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Dequeue --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_Dequeue)
ssize_t
VMCIQPair_Dequeue(VMCIQPair *qpair,        // IN
                  void *buf,               // IN
                  size_t bufSize,          // IN
                  int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   result = DequeueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->consumeQSize,
                          buf, bufSize, bufType,
                          VMCIMemcpyFromQueue,
                          TRUE);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_Peek --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_Peek)
ssize_t
VMCIQPair_Peek(VMCIQPair *qpair,    // IN
               void *buf,           // IN
               size_t bufSize,      // IN
               int bufType)         // IN
{
   ssize_t result;

   if (!qpair || !buf) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   result = DequeueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->consumeQSize,
                          buf, bufSize, bufType,
                          VMCIMemcpyFromQueue,
                          FALSE);

   VMCIQPairUnlock(qpair);

   return result;
}


#if defined (SOLARIS) || (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
    (defined(__linux__) && defined(__KERNEL__)) || \
    (defined(_WIN32) && defined(WINNT_DDK))

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_EnqueueV --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_EnqueueV)
ssize_t
VMCIQPair_EnqueueV(VMCIQPair *qpair,        // IN
                   void *iov,               // IN
                   size_t iovSize,          // IN
                   int bufType)             // IN
{
   ssize_t result;

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   result = EnqueueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->produceQSize,
                          iov, iovSize, bufType,
                          VMCIMemcpyToQueueV);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_DequeueV --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_DequeueV)
ssize_t
VMCIQPair_DequeueV(VMCIQPair *qpair,         // IN
                   void *iov,                // IN
                   size_t iovSize,           // IN
                   int bufType)              // IN
{
   ssize_t result;

   VMCIQPairLock(qpair);

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   result = DequeueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->consumeQSize,
                          iov, iovSize, bufType,
                          VMCIMemcpyFromQueueV,
                          TRUE);

   VMCIQPairUnlock(qpair);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQPair_PeekV --
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

VMCI_EXPORT_SYMBOL(VMCIQPair_PeekV)
ssize_t
VMCIQPair_PeekV(VMCIQPair *qpair,           // IN
                void *iov,                  // IN
                size_t iovSize,             // IN
                int bufType)                // IN
{
   ssize_t result;

   if (!qpair || !iov) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIQPairLock(qpair);

   result = DequeueLocked(qpair->produceQ,
                          qpair->consumeQ,
                          qpair->consumeQSize,
                          iov, iovSize, bufType,
                          VMCIMemcpyFromQueueV,
                          FALSE);

   VMCIQPairUnlock(qpair);

   return result;
}

#endif /* Systems that support struct iovec */
