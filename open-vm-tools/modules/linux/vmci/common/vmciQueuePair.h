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
 * vmciQueuePair.h --
 *
 *    VMCI QueuePair API implementation in the host driver.
 */

#ifndef _VMCI_QUEUE_PAIR_H_
#define _VMCI_QUEUE_PAIR_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmciContext.h"
#include "vmci_iocontrols.h"
#include "vmciQueue.h"
#ifdef VMKERNEL
#include "vm_atomic.h"
#include "util_copy_dist.h"
#include "shm.h"

/*
 * On vmkernel, the queue pairs are either backed by shared memory or
 * kernel memory allocated on the VMCI heap. Shared memory is used for
 * guest to guest and guest to host queue pairs, whereas the heap
 * allocated queue pairs are used for host local queue pairs.
 */

typedef struct QueuePairPageStore {
   Bool shared; // Indicates whether the pages are stored in shared memory
   union {
      Shm_ID   shmID;
      void    *ptr;
   } store;
} QueuePairPageStore;

#else

typedef struct QueuePairPageStore {
   Bool user;                  // Whether the page file strings are userspace pointers
   VA64 producePageFile;       // Name of the file
   VA64 consumePageFile;       // Name of the file
   uint64 producePageFileSize; // Size of the string
   uint64 consumePageFileSize; // Size of the string
   VA64 producePageUVA;        // User space VA of the mapped file in VMX
   VA64 consumePageUVA;        // User space VA of the mapped file in VMX
} QueuePairPageStore;

#endif // !VMKERNEL

#if (defined(__linux__) || defined(_WIN32) || defined(__APPLE__) || \
     defined(SOLARIS)) && !defined(VMKERNEL)
struct VMCIQueue;

typedef struct PageStoreAttachInfo {
   char producePageFile[VMCI_PATH_MAX];
   char consumePageFile[VMCI_PATH_MAX];
   uint64 numProducePages;
   uint64 numConsumePages;

   /* User VAs in the VMX task */
   VA64   produceBuffer;
   VA64   consumeBuffer;

   /*
    * Platform-specific references to the physical pages backing the
    * queue. These include a page for the header.
    *
    * PR361589 tracks this, too.
    */

#if defined(__linux__)
   struct page **producePages;
   struct page **consumePages;
#elif defined(_WIN32)
   void *kmallocPtr;
   size_t kmallocSize;
   PMDL produceMDL;
   PMDL consumeMDL;
#elif defined(__APPLE__)
   /*
    * All the Mac OS X fields are members of the VMCIQueue
    */
#endif
} PageStoreAttachInfo;

#endif // (__linux__ || _WIN32 || __APPLE__) && !VMKERNEL


/*
 *------------------------------------------------------------------------------
 *
 *  VMCI_QP_PAGESTORE_IS_WELLFORMED --
 *
 *     Utility function that checks whether the fields of the page
 *     store contain valid values.
 *
 *  Result:
 *     TRUE if the page store is wellformed. FALSE otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static INLINE Bool
VMCI_QP_PAGESTORE_IS_WELLFORMED(QueuePairPageStore *pageStore) // IN
{
#ifdef VMKERNEL
   return (pageStore->shared && pageStore->store.shmID != SHM_INVALID_ID) ||
          (!pageStore->shared && pageStore->store.ptr != NULL);
#else
   return pageStore->producePageFile && pageStore->consumePageFile &&
          pageStore->producePageFileSize && pageStore->consumePageFileSize;
#endif // !VMKERNEL
}

int VMCIQPBroker_Init(void);
void VMCIQPBroker_Exit(void);
void VMCIQPBroker_Lock(void);
void VMCIQPBroker_Unlock(void);
int VMCIQPBroker_Alloc(VMCIHandle handle, VMCIId peer, uint32 flags,
                       VMCIPrivilegeFlags privFlags,
                       uint64 produceSize, uint64 consumeSize,
                       QueuePairPageStore *pageStore,
                       VMCIContext *context);
int VMCIQPBroker_SetPageStore(VMCIHandle handle,
                              QueuePairPageStore *pageStore,
                              VMCIContext *context);
int VMCIQPBroker_Detach(VMCIHandle handle, VMCIContext *context, Bool detach);

int VMCIQPGuestEndpoints_Init(void);
void VMCIQPGuestEndpoints_Exit(void);
void VMCIQPGuestEndpoints_Sync(void);
void VMCIQPGuestEndpoints_Convert(Bool toLocal, Bool deviceReset);

int VMCIQueuePair_Alloc(VMCIHandle *handle, VMCIQueue **produceQ,
                        uint64 produceSize, VMCIQueue **consumeQ,
                        uint64 consumeSize, VMCIId peer, uint32 flags,
                        VMCIPrivilegeFlags privFlags, Bool guestEndpoint);
int VMCIQueuePair_Detach(VMCIHandle handle, Bool guestEndpoint);


#endif /* !_VMCI_QUEUE_PAIR_H_ */

