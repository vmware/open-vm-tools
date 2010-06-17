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

#ifndef _VMCI_QUEUE_H_
#define _VMCI_QUEUE_H_

/*
 *
 * vmciQueue.h --
 *
 *    Defines the queue structure, and helper functions to enqueue and dequeue
 *    items.  XXX needs checksumming?
 */

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#if defined(SOLARIS) || defined(__APPLE__)
#  include <sys/uio.h>
#endif

#if defined VMKERNEL
#  include "vm_atomic.h"
#  include "return_status.h"
#  include "util_copy_dist.h"
#endif


/*
 * VMCIQueue
 *
 * This data type contains the information about a queue.
 *
 * There are two queues (hence, queue pairs) per transaction model between a
 * pair of end points, A & B.  One queue is used by end point A to transmit
 * commands and responses to B.  The other queue is used by B to transmit
 * commands and responses.
 *
 * VMCIQueueKernelIf is a per-OS defined Queue structure.  It contains either a
 * direct pointer to the linear address of the buffer contents or a pointer to
 * structures which help the OS locate those data pages.  See vmciKernelIf.c
 * for each platform for its definition.
 */

typedef struct VMCIQueueKernelIf VMCIQueueKernelIf;

typedef struct VMCIQueue {
   VMCIQueueHeader *qHeader;
   VMCIQueueKernelIf *kernelIf;
} VMCIQueue;


/*
 * ESX uses a buffer type for the memcpy functions.  Currently, none
 * of the hosted products use such a field.  And, to keep the function
 * definitions simple, we use a define to declare the type parameter.
 */

#ifdef VMKERNEL
#define BUF_TYPE	Util_BufferType
#else
#define BUF_TYPE	int
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpy{To,From}QueueFunc() prototypes.  Functions of these
 * types are passed around to enqueue and dequeue routines.  Note that
 * often the functions passed are simply wrappers around memcpy
 * itself.
 *
 * Note: In order for the memcpy typedefs to be compatible with the VMKernel,
 * there's an unused last parameter for the hosted side.  In
 * ESX, that parameter holds a buffer type.
 *
 *-----------------------------------------------------------------------------
 */
typedef int VMCIMemcpyToQueueFunc(VMCIQueue *queue, uint64 queueOffset,
                                  const void *src, size_t srcOffset,
                                  size_t size, BUF_TYPE bufType);
typedef int VMCIMemcpyFromQueueFunc(void *dest, size_t destOffset,
                                    const VMCIQueue *queue, uint64 queueOffset,
                                    size_t size, BUF_TYPE bufType);


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIMemcpy{To,From}Queue[v]() prototypes
 *
 * Note that these routines are NOT SAFE to call on a host end-point
 * until the guest end of the queue pair has attached -AND-
 * SetPageStore().  The VMX crosstalk device will issue the
 * SetPageStore() on behalf of the guest when the guest creates a
 * QueuePair or attaches to one created by the host.  So, if the guest
 * notifies the host that it's attached then the queue is safe to use.
 * Also, if the host registers notification of the connection of the
 * guest, then it will only receive that notification when the guest
 * has issued the SetPageStore() call and not before (when the guest
 * had attached).
 *
 *-----------------------------------------------------------------------------
 */

int VMCIMemcpyToQueue(VMCIQueue *queue, uint64 queueOffset, const void *src,
                      size_t srcOffset, size_t size, BUF_TYPE bufType);
int VMCIMemcpyFromQueue(void *dest, size_t destOffset, const VMCIQueue *queue,
                        uint64 queueOffset, size_t size, BUF_TYPE bufType);

#if defined VMKERNEL || defined (SOLARIS)         || \
   (defined(__APPLE__) && !defined (VMX86_TOOLS)) || \
   (defined(__linux__) && defined(__KERNEL__))

   /*
    * Solaris/Mac/Linux vmciKernelIf.c files provide these functions
    */

int VMCIMemcpyToQueueV(VMCIQueue *queue, uint64 queueOffset, const void *src,
                       size_t srcOffset, size_t size, BUF_TYPE bufType);
int VMCIMemcpyFromQueueV(void *dest, size_t destOffset, const VMCIQueue *queue,
                         uint64 queueOffset, size_t size, BUF_TYPE bufType);
#endif /* Does the O/S support iovec? */


#endif /* !_VMCI_QUEUE_H_ */

