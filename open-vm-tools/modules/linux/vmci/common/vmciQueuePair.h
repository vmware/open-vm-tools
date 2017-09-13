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
#include "vmci_iocontrols.h"
#include "vmci_kernel_if.h"
#include "vmciContext.h"
#include "vmciQueue.h"

/*
 * QueuePairPageStore describes how the memory of a given queue pair
 * is backed. When the queue pair is between the host and a guest, the
 * page store consists of references to the guest pages. On vmkernel,
 * this is a list of PPNs, and on hosted, it is a user VA where the
 * queue pair is mapped into the VMX address space.
 */

typedef struct QueuePairPageStore {
   VMCIQPGuestMem pages;  // Reference to pages backing the queue pair.
   uint32 len;            // Length of pageList/virtual addres range (in pages).
} QueuePairPageStore;


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
  return pageStore->len >= 2;
}

int VMCIQPBroker_Init(void);
void VMCIQPBroker_Exit(void);
int VMCIQPBroker_Alloc(VMCIHandle handle, VMCIId peer, uint32 flags,
                       VMCIPrivilegeFlags privFlags,
                       uint64 produceSize, uint64 consumeSize,
                       QueuePairPageStore *pageStore,
                       VMCIContext *context);
int VMCIQPBroker_SetPageStore(VMCIHandle handle, VA64 produceUVA, VA64 consumeUVA,
                              VMCIContext *context);
int VMCIQPBroker_Detach(VMCIHandle handle, VMCIContext *context);

int VMCIQPGuestEndpoints_Init(void);
void VMCIQPGuestEndpoints_Exit(void);
void VMCIQPGuestEndpoints_Sync(void);
void VMCIQPGuestEndpoints_Convert(Bool toLocal, Bool deviceReset);

int VMCIQueuePair_Alloc(VMCIHandle *handle, VMCIQueue **produceQ,
                        uint64 produceSize, VMCIQueue **consumeQ,
                        uint64 consumeSize, VMCIId peer, uint32 flags,
                        VMCIPrivilegeFlags privFlags, Bool guestEndpoint,
                        VMCIEventReleaseCB wakeupCB, void *clientData);
int VMCIQueuePair_Detach(VMCIHandle handle, Bool guestEndpoint);
int VMCIQPBroker_Map(VMCIHandle  handle, VMCIContext *context, VMCIQPGuestMem guestMem);
int VMCIQPBroker_Unmap(VMCIHandle  handle, VMCIContext *context, VMCIGuestMemID gid);
#ifdef VMKERNEL
int VMCIQPBroker_Revalidate(VMCIHandle  handle, VMCIContext *context);
#endif

#endif /* !_VMCI_QUEUE_PAIR_H_ */

