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

#ifndef _PUBLIC_VMCI_QUEUE_PAIR_H_
#define _PUBLIC_VMCI_QUEUE_PAIR_H_

/*
 *
 * vmci_queue_pair.h --
 *
 *    Defines queue layout in memory, and helper functions to enqueue and
 *    dequeue items. XXX needs checksumming?
 */

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMX
#include "includeCheck.h"

#if defined(__APPLE__) && defined(KERNEL)
#  include "vmci_kernel_if.h"
#endif // __APPLE__

#include "vm_basic_defs.h"
#include "vm_basic_types.h"
#include "vm_atomic.h"
#include "vmci_defs.h"
#include "vm_assert.h"
#if defined(SOLARIS)
#include <sys/uio.h>
#endif

#if (defined(__linux__) && defined(__KERNEL__)) || defined(SOLARIS)
#include "vmci_kernel_if.h"
#endif


#if defined(__linux__) && defined(__KERNEL__)
struct page;
#endif


/*
 * For a queue of buffer 'size' bytes, the tail and head pointers will be in
 * the range [0, size-1].
 */

typedef struct VMCIQueueHeader {
   /* All fields are 64bit and aligned. */
   VMCIHandle    handle; /* Identifier. */
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
#define QP_MAX_QUEUE_SIZE_ARCH_ANY  CONST64U(0xffffffff)

#ifdef __x86_64__
#  define QP_MAX_QUEUE_SIZE_ARCH  CONST64U(0xffffffffffffffff)
#  define QPAtomic_ReadOffset(x)  Atomic_Read64(x)
#  define QPAtomic_WriteOffset(x, y) Atomic_Write64(x, y)
#else
   /*
    * Wrappers below are being used to call Atomic_Read32 because of the
    * 'type punned' compilation warning received when Atomic_Read32 is
    * called with a Atomic_uint64 pointer typecasted to Atomic_uint32 
    * pointer from QPAtomic_ReadOffset. Ditto with QPAtomic_WriteOffset.
    */

   static INLINE uint32
   TypeSafe_Atomic_Read32(void *var)                // IN:
   {
      return Atomic_Read32((Atomic_uint32 *)(var));
   }

   static INLINE void
   TypeSafe_Atomic_Write32(void *var, uint32 val)   // IN:
   {
      Atomic_Write32((Atomic_uint32 *)(var), (uint32)(val));
   }
   
#  define QP_MAX_QUEUE_SIZE_ARCH  CONST64U(0xffffffff)
#  define QPAtomic_ReadOffset(x)  TypeSafe_Atomic_Read32((void *)(x))
#  define QPAtomic_WriteOffset(x, y) \
          TypeSafe_Atomic_Write32((void *)(x), (uint32)(y))
#endif	/* __x86_64__  */


/*
 *-----------------------------------------------------------------------------
 *
 * AddPointer --
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
AddPointer(Atomic_uint64 *var, // IN:
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
 * VMCIQueue
 *
 * This data type contains the information about a queue.
 *
 * There are two queues (hence, queue pairs) per transaction model
 * between a pair of end points, A & B.  One queue is used by end
 * point A to transmit commands and responses to B.  The other queue
 * is used by B to transmit commands and responses.
 *
 * There are six distinct definitions of the VMCIQueue structure.
 * Five are found below along with descriptions of under what build
 * type the VMCIQueue structure type is compiled.  The sixth is
 * located in a separate file and is used in the VMKernel context.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMX86_TOOLS) || defined(VMX86_VMX)
#  if defined(__linux__) && defined(__KERNEL__)
      /*
       * Linux Kernel Guest.
       *
       * The VMCIQueue is two or more machine pages where the first
       * contains the queue header and the second and subsequent pages
       * contain an array of struct page pointers to the actual pages
       * that contain the queue data.
       */
      typedef struct VMCIQueue {
	 VMCIQueueHeader queueHeader;
	 uint8 _padding[PAGE_SIZE - sizeof(VMCIQueueHeader)];
	 struct page *page[0];
      } VMCIQueue;
#  elif defined(SOLARIS)
      /*
       * Solaris Kernel Guest.
       *
       * kmem_cache_alloc with object size of one page is being used to get
       * page aligned memory for queueHeader. VMCIQueue stores a pointer to 
       * the queueHeader and the array of virtual addresses of actual pages 
       * that contain the queue data.
       */
      typedef struct VMCIQueue {
         VMCIQueueHeader *queueHeaderPtr;
         void *vaddr[0];
      } VMCIQueue;

#  else
      /*
       * VMX App and Windows Kernel Guest
       *
       * The VMCIQueue is two or more machine pages where the first
       * contains the queue header and the second and subsequent pages
       * contain the actual queue buffer data.
       */
      typedef struct VMCIQueue {
	 VMCIQueueHeader queueHeader;
	 uint8 _padding[PAGE_SIZE - sizeof(VMCIQueueHeader)];
	 uint8 buffer[0];  // VMX doesn't use this today
      } VMCIQueue;
#  endif
#else
#  if defined(__linux__) && defined(__KERNEL__)
      /*
       * Linux Kernel Host
       *
       * The VMCIQueueHeader has been created elsewhere and this
       * structure (VMCIQueue) needs a pointer to that
       * VMCIQueueHeader.
       *
       * Also, the queue contents are managed by an array of struct
       * pages which are managed elsewhere.  So, this structure simply
       * contains the address of that array of struct pages.
       */
      typedef struct VMCIQueue {
	 VMCIQueueHeader *queueHeaderPtr;
	 struct page **page;
      } VMCIQueue;
#  else
      /*
       * Windows Host
       *
       * The VMCIQueueHeader has been created elsewhere and this
       * structure (VMCIQueue) needs a pointer to that
       * VMCIQueueHeader.
       *
       * Also, the queue contents are managed by an array of bytes
       * which are managed elsewhere.  So, this structure simply
       * contains the address of that array of bytes.
       */
      typedef struct VMCIQueue {
	 VMCIQueueHeader *queueHeaderPtr;
	 uint8 *buffer;
      } VMCIQueue;
#  endif
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_GetHeader() --
 *
 *      Helper routine to get the address of the VMCIQueueHeader.
 *      Note that this routine is useful because there are two ways
 *      queues are declared (at least as far as the queue header is
 *      concerned, anyway).  In one way, the queue contains the header
 *      in the other the queue contains a pointer to the header.  This
 *      helper hides that ambiguity from users of queues who need
 *      access to the header.
 *
 * Results:
 *      The address of the queue header.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE VMCIQueueHeader *
VMCIQueue_GetHeader(const VMCIQueue *q)
{
#if defined(VMX86_TOOLS) || defined(VMX86_VMX)
   /*
    * All guest variants except Solaris have a complete queue header
    */
#if defined (SOLARIS)
   return (VMCIQueueHeader *)q->queueHeaderPtr;
#else
   return (VMCIQueueHeader *)&q->queueHeader;
#endif	/* SOLARIS  */
#else
   /*
    * All host variants have a pointer to the queue header
    */
   return (VMCIQueueHeader *)q->queueHeaderPtr;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_ProducerTail() --
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
VMCIQueue_ProducerTail(const VMCIQueue *queue)
{
   return QPAtomic_ReadOffset(&VMCIQueue_GetHeader(queue)->producerTail);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_ConsumerHead() --
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
VMCIQueue_ConsumerHead(const VMCIQueue *queue)
{
   return QPAtomic_ReadOffset(&VMCIQueue_GetHeader(queue)->consumerHead);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_AddProducerTail() --
 *
 *      Helper routine to increment the Producer Tail.  Fundamentally,
 *      AddPointer() is used to manipulate the tail itself.
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
VMCIQueue_AddProducerTail(VMCIQueue *queue,
                          size_t add,
                          uint64 queueSize)
{
   AddPointer(&VMCIQueue_GetHeader(queue)->producerTail, add, queueSize);
}

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_AddConsumerHead() --
 *
 *      Helper routine to increment the Consumer Head.  Fundamentally,
 *      AddPointer() is used to manipulate the head itself.
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
VMCIQueue_AddConsumerHead(VMCIQueue *queue,
                          size_t add,
                          uint64 queueSize)
{
   AddPointer(&VMCIQueue_GetHeader(queue)->consumerHead, add, queueSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpy{To,From}QueueFunc() prototypes.  Functions of these
 * types are passed around to enqueue and dequeue routines.  Note that
 * often the functions passed are simply wrappers around memcpy
 * itself.
 *
 *-----------------------------------------------------------------------------
 */
typedef int VMCIMemcpyToQueueFunc(VMCIQueue *queue, uint64 queueOffset,
                                  const void *src, size_t srcOffset,
                                  size_t size);
typedef int VMCIMemcpyFromQueueFunc(void *dest, size_t destOffset,
                                    const VMCIQueue *queue, uint64 queueOffset,
                                    size_t size);

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpy{To,From}Queue[v]() prototypes (and, in some cases, an
 * inline version).
 * 
 *-----------------------------------------------------------------------------
 */

#if defined (SOLARIS) || (defined(__linux__) && defined(__KERNEL__))
int VMCIMemcpyToQueue(VMCIQueue *queue, uint64 queueOffset, const void *src,
                      size_t srcOffset, size_t size);
int VMCIMemcpyFromQueue(void *dest, size_t destOffset, const VMCIQueue *queue,
                        uint64 queueOffset, size_t size);
int VMCIMemcpyToQueueV(VMCIQueue *queue, uint64 queueOffset, const void *src,
                       size_t srcOffset, size_t size);
int VMCIMemcpyFromQueueV(void *dest, size_t destOffset, const VMCIQueue *queue,
                         uint64 queueOffset, size_t size);
#elif defined(_WIN32) && defined(WINNT_DDK)
int VMCIMemcpyToQueue(VMCIQueue *queue, uint64 queueOffset, const void *src,
                      size_t srcOffset, size_t size);
int VMCIMemcpyFromQueue(void *dest, size_t destOffset, const VMCIQueue *queue,
                        uint64 queueOffset, size_t size);
#else


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyToQueue --
 *
 *      Wrapper for memcpy --- copies from a given buffer to a VMCI Queue.
 *      Assumes that offset + size does not wrap around in the queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIMemcpyToQueue(VMCIQueue *queue,   // OUT:
                  uint64 queueOffset, // IN:
                  const void *src,    // IN:
                  size_t srcOffset,   // IN:
                  size_t size)        // IN:
{
   memcpy(queue->buffer + queueOffset, (uint8 *)src + srcOffset, size);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpyFromQueue --
 *
 *      Wrapper for memcpy --- copies to a given buffer from a VMCI Queue.
 *      Assumes that offset + size does not wrap around in the queue.
 *
 * Results:
 *      Zero on success, negative error code on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIMemcpyFromQueue(void *dest,             // OUT:
                    size_t destOffset,      // IN:
                    const VMCIQueue *queue, // IN:
                    uint64 queueOffset,     // IN:
                    size_t size)            // IN:
{
   memcpy((uint8 *)dest + destOffset, queue->buffer + queueOffset, size);
   return 0;
}
#endif /* __linux__ && __KERNEL__ */


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_CheckAlignment --
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
VMCIQueue_CheckAlignment(const VMCIQueue *queue) // IN:
{
   /*
    * Checks if header of the queue is page aligned.
    */
   return ((uintptr_t)VMCIQueue_GetHeader(queue) & (PAGE_SIZE - 1)) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_GetPointers --
 *
 *      Helper routine for getting the head and the tail pointer for a queue.
 *	Both the VMCIQueues are needed to get both the pointers for one queue.
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
VMCIQueue_GetPointers(const VMCIQueue *produceQ,
                      const VMCIQueue *consumeQ,
                      uint64 *producerTail,
                      uint64 *consumerHead)
{
   *producerTail = VMCIQueue_ProducerTail(produceQ);
   *consumerHead = VMCIQueue_ConsumerHead(consumeQ);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_ResetPointers --
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
VMCIQueue_ResetPointers(VMCIQueue *queue) // IN:
{
   QPAtomic_WriteOffset(&VMCIQueue_GetHeader(queue)->producerTail, CONST64U(0));
   QPAtomic_WriteOffset(&VMCIQueue_GetHeader(queue)->consumerHead, CONST64U(0));
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_Init --
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
VMCIQueue_Init(const VMCIHandle handle, // IN:
               VMCIQueue *queue)        // IN:
{
   ASSERT_NOT_IMPLEMENTED(VMCIQueue_CheckAlignment(queue));
   VMCIQueue_GetHeader(queue)->handle = handle;
   VMCIQueue_ResetPointers(queue);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueueFreeSpaceInt --
 *
 *      Finds available free space in a produce queue to enqueue more
 *      data or reports an error if queue pair corruption is detected.
 *
 * Results:
 *      Free space size in bytes.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
VMCIQueueFreeSpaceInt(const VMCIQueue *produceQueue, // IN:
                      const VMCIQueue *consumeQueue, // IN:
                      const uint64 produceQSize,     // IN:
                      uint64 *freeSpace)             // OUT:
{
   const uint64 tail = VMCIQueue_ProducerTail(produceQueue);
   const uint64 head = VMCIQueue_ConsumerHead(consumeQueue);

   ASSERT(freeSpace);

   if (tail >= produceQSize || head >= produceQSize) {
      return VMCI_ERROR_INVALID_SIZE;
   }

   /*
    * Deduct 1 to avoid tail becoming equal to head which causes ambiguity. If
    * head and tail are equal it means that the queue is empty.
    */
   if (tail >= head) {
      *freeSpace = produceQSize - (tail - head) - 1;
   } else {
      *freeSpace = head - tail - 1;
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_FreeSpace --
 *
 *      Finds available free space in a produce queue to enqueue more data.
 *
 * Results:
 *      On success, free space size in bytes (up to MAX_INT64).
 *      On failure, appropriate error code.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int64
VMCIQueue_FreeSpace(const VMCIQueue *produceQueue, // IN:
                    const VMCIQueue *consumeQueue, // IN:
                    const uint64 produceQSize)     // IN:
{
   uint64 freeSpace;
   int retval;

   retval = VMCIQueueFreeSpaceInt(produceQueue, consumeQueue, produceQSize,
                                  &freeSpace);

   if (retval != VMCI_SUCCESS) {
      return retval;
   }

   return MIN(freeSpace, MAX_INT64);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_BufReady --
 *
 *      Finds available data to dequeue from a consume queue.
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
VMCIQueue_BufReady(const VMCIQueue *consumeQueue, // IN:
                   const VMCIQueue *produceQueue, // IN:
                   const uint64 consumeQSize)     // IN:
{
   int retval;
   uint64 freeSpace;

   retval = VMCIQueueFreeSpaceInt(consumeQueue, produceQueue,
                                  consumeQSize, &freeSpace);
   if (retval != VMCI_SUCCESS) {
      return retval;
   } else {
      uint64 available = consumeQSize - freeSpace - 1;
      return MIN(available, MAX_INT64);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * __VMCIQueue_Enqueue --
 *
 *      Enqueues a given buffer to the produce queue using the provided
 *      function. As many bytes as possible (space available in the queue)
 *      are enqueued.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NOSPACE if no space was available to enqueue data.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise, the number of bytes written to the queue is returned.
 *
 * Side effects:
 *      Updates the tail pointer of the produce queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
__VMCIQueue_Enqueue(VMCIQueue *produceQueue,               // IN:
                    const VMCIQueue *consumeQueue,         // IN:
                    const uint64 produceQSize,             // IN:
                    const void *buf,                       // IN:
                    size_t bufSize,                        // IN:
                    VMCIMemcpyToQueueFunc memcpyToQueue)   // IN:
{
   const int64 freeSpace = VMCIQueue_FreeSpace(produceQueue, consumeQueue,
                                               produceQSize);
   const uint64 tail = VMCIQueue_ProducerTail(produceQueue);
   size_t written;

   if (!freeSpace) {
      return VMCI_ERROR_QUEUEPAIR_NOSPACE;
   }
   if (freeSpace < 0) {
      return (ssize_t)freeSpace;
   }

   written = (size_t)(freeSpace > bufSize ? bufSize : freeSpace);
   if (LIKELY(tail + written < produceQSize)) {
      memcpyToQueue(produceQueue, tail, buf, 0, written);
   } else {
      /* Tail pointer wraps around. */
      const size_t tmp = (size_t)(produceQSize - tail);

      memcpyToQueue(produceQueue, tail, buf, 0, tmp);
      memcpyToQueue(produceQueue, 0, buf, tmp, written - tmp);
   }
   AddPointer(&VMCIQueue_GetHeader(produceQueue)->producerTail, written, produceQSize);
   return written;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_Enqueue --
 *
 *      Enqueues a given buffer to the produce queue. As many bytes as possible
 *      (space available in the queue) are enqueued. If bufSize is larger than
 *      the maximum value of ssize_t the result is unspecified.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NOSPACE if no space was available to enqueue data.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise, the number of bytes written to the queue is returned.
 *
 * Side effects:
 *      Updates the tail pointer of the produce queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
VMCIQueue_Enqueue(VMCIQueue *produceQueue,       // IN:
                  const VMCIQueue *consumeQueue, // IN:
                  const uint64 produceQSize,     // IN:
                  const void *buf,               // IN:
                  size_t bufSize)                // IN:
{
   return __VMCIQueue_Enqueue(produceQueue, consumeQueue, produceQSize,
                              buf, bufSize, VMCIMemcpyToQueue);
}


#if defined (SOLARIS) || (defined(__linux__) && defined(__KERNEL__))
/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_EnqueueV --
 *
 *      Enqueues a given iovec to the produce queue. As many bytes as possible
 *      (space available in the queue) are enqueued. If bufSize is larger than
 *      the maximum value of ssize_t the result is unspecified.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NOSPACE if no space was available to enqueue data.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise, the number of bytes written to the queue is returned.
 *
 * Side effects:
 *      Updates the tail pointer of the produce queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
VMCIQueue_EnqueueV(VMCIQueue *produceQueue,       // IN:
                   const VMCIQueue *consumeQueue, // IN:
                   const uint64 produceQSize,     // IN:
                   struct iovec *iov,             // IN:
                   size_t iovSize)                // IN:
{
   return __VMCIQueue_Enqueue(produceQueue, consumeQueue, produceQSize,
                              (void *)iov, iovSize, VMCIMemcpyToQueueV);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * __VMCIQueue_Dequeue --
 *
 *      Dequeues data (if available) from the given consume queue. Writes data
 *      to the user provided buffer using the provided function.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NODATA if no data was available to dequeue.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise the number of bytes dequeued is returned.
 *
 * Side effects:
 *      Updates the head pointer of the consume queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
__VMCIQueue_Dequeue(VMCIQueue *produceQueue,                    // IN:
                    const VMCIQueue *consumeQueue,              // IN:
                    const uint64 consumeQSize,                  // IN:
                    void *buf,                                  // IN:
                    size_t bufSize,                             // IN:
                    VMCIMemcpyFromQueueFunc memcpyFromQueue)    // IN:
{
   const int64 bufReady = VMCIQueue_BufReady(consumeQueue, produceQueue,
                                             consumeQSize);
   const uint64 head = VMCIQueue_ConsumerHead(produceQueue);
   size_t written;

   if (!bufReady) {
      return VMCI_ERROR_QUEUEPAIR_NODATA;
   }
   if (bufReady < 0) {
      return (ssize_t)bufReady;
   }

   written = (size_t)(bufReady > bufSize ? bufSize : bufReady);
   if (LIKELY(head + written < consumeQSize)) {
      memcpyFromQueue(buf, 0, consumeQueue, head, written);
   } else {
      /* Head pointer wraps around. */
      const size_t tmp = (size_t)(consumeQSize - head);

      memcpyFromQueue(buf, 0, consumeQueue, head, tmp);
      memcpyFromQueue(buf, tmp, consumeQueue, 0, written - tmp);
   }
   AddPointer(&VMCIQueue_GetHeader(produceQueue)->consumerHead, written, consumeQSize);
   return written;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_Dequeue --
 *
 *      Dequeues data (if available) from the given consume queue. Writes data
 *      to the user provided buffer. If bufSize is larger than the maximum
 *      value of ssize_t the result is unspecified.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NODATA if no data was available to dequeue.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise the number of bytes dequeued is returned.
 *
 * Side effects:
 *      Updates the head pointer of the consume queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
VMCIQueue_Dequeue(VMCIQueue *produceQueue,       // IN:
                  const VMCIQueue *consumeQueue, // IN:
                  const uint64 consumeQSize,     // IN:
                  void *buf,                     // IN:
                  size_t bufSize)                // IN:
{
   return __VMCIQueue_Dequeue(produceQueue, consumeQueue, consumeQSize,
                              buf, bufSize, VMCIMemcpyFromQueue);
}


#if defined(SOLARIS) || (defined(__linux__) && defined(__KERNEL__))
/*
 *-----------------------------------------------------------------------------
 *
 * VMCIQueue_DequeueV --
 *
 *      Dequeues data (if available) from the given consume queue. Writes data
 *      to the user provided iovec. If bufSize is larger than the maximum
 *      value of ssize_t the result is unspecified.
 *
 * Results:
 *      VMCI_ERROR_QUEUEPAIR_NODATA if no data was available to dequeue.
 *      VMCI_ERROR_INVALID_SIZE, if any queue pointer is outside the queue 
 *      (as defined by the queue size).
 *      Otherwise the number of bytes dequeued is returned.
 *
 * Side effects:
 *      Updates the head pointer of the consume queue.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE ssize_t
VMCIQueue_DequeueV(VMCIQueue *produceQueue,       // IN:
                   const VMCIQueue *consumeQueue, // IN:
                   const uint64 consumeQSize,     // IN:
                   struct iovec *iov,             // IN:
                   size_t iovSize)                // IN:
{
   return __VMCIQueue_Dequeue(produceQueue, consumeQueue, consumeQSize,
                              (void *)iov, iovSize, VMCIMemcpyFromQueueV);
}
#endif

#endif /* !_PUBLIC_VMCI_QUEUE_PAIR_H_ */
