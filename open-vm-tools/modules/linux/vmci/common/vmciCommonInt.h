/*********************************************************
 * Copyright (C) 2006-2012 VMware, Inc. All rights reserved.
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
 * vmciCommonInt.h --
 *
 * Struct definitions for VMCI internal common code.
 */

#ifndef _VMCI_COMMONINT_H_
#define _VMCI_COMMONINT_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_atomic.h"
#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmci_infrastructure.h"
#include "vmci_handle_array.h"
#include "vmci_kernel_if.h"

/*
 *  The DatagramQueueEntry is a queue header for the in-kernel VMCI
 *  datagram queues. It is allocated in non-paged memory, as the
 *  content is accessed while holding a spinlock. The pending datagram
 *  itself may be allocated from paged memory. We shadow the size of
 *  the datagram in the non-paged queue entry as this size is used
 *  while holding the same spinlock as above.
 */

typedef struct DatagramQueueEntry {
   VMCIListItem   listItem;  /* For queuing. */
   size_t         dgSize;    /* Size of datagram. */
   VMCIDatagram   *dg;       /* Pending datagram. */
} DatagramQueueEntry;


/*
 * The VMCIFilterState captures the state of all VMCI filters in one
 * direction. The ranges array contains all filter list in a single
 * memory chunk, and the filter list pointers in the VMCIProtoFilters
 * point into the ranges array.
 */

typedef struct VMCIFilterState {
   VMCIProtoFilters filters;
   VMCIIdRange *ranges;
   size_t rangesSize;
} VMCIFilterState;


struct VMCIContext {
   VMCIListItem       listItem;         /* For global VMCI list. */
   VMCIId             cid;
   Atomic_uint32      refCount;
   VMCIList           datagramQueue;    /* Head of per VM queue. */
   uint32             pendingDatagrams;
   size_t             datagramQueueSize;/* Size of datagram queue in bytes. */
   int                userVersion;      /*
                                         * Version of the code that created
                                         * this context; e.g., VMX.
                                         */
   VMCILock           lock;             /*
                                         * Locks datagramQueue, inFilters,
                                         * doorbellArray, pendingDoorbellArray
                                         * and notifierArray.
                                         */
   VMCIHandleArray    *queuePairArray;  /*
                                         * QueuePairs attached to.  The array of
                                         * handles for queue pairs is accessed
                                         * from the code for QP API, and there
                                         * it is protected by the QP lock.  It
                                         * is also accessed from the context
                                         * clean up path, which does not
                                         * require a lock.  VMCILock is not
                                         * used to protect the QP array.
                                         */
   VMCIHandleArray    *doorbellArray;   /* Doorbells created by context. */
   VMCIHandleArray    *pendingDoorbellArray; /* Doorbells pending for context. */
   VMCIHandleArray    *notifierArray;   /* Contexts current context is subscribing to. */
   VMCIHost           hostContext;
   VMCIPrivilegeFlags privFlags;
   VMCIHostUser       user;
   Bool               validUser;
#ifdef VMKERNEL
   Bool               isQuiesced;       /* Whether current VM is quiesced */
   VMCIId             migrateCid;       /* The migrate cid if it is migrating */
   VMCIMutex          guestMemMutex;    /*
                                         * Coordinates guest memory
                                         * registration/release during FSR.
                                         */
   VMCIGuestMemID     curGuestMemID;    /* ID of current registered guest mem */
   VMCIFilterState    *inFilters;       /* Ingoing filters for VMCI traffic. */
#endif
#ifndef VMX86_SERVER
   Bool               *notify;          /* Notify flag pointer - hosted only. */
#  ifdef __linux__
   struct page        *notifyPage;      /* Page backing the notify UVA. */
#  endif
#endif
};


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDenyInteraction --
 *
 *     Utilility function that checks whether two entities are allowed
 *     to interact. If one of them is restricted, the other one must
 *     be trusted.
 *
 *  Result:
 *     TRUE if the two entities are not allowed to interact. FALSE otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static INLINE Bool
VMCIDenyInteraction(VMCIPrivilegeFlags partOne,  // IN
                    VMCIPrivilegeFlags partTwo)  // IN
{
   return (((partOne & VMCI_PRIVILEGE_FLAG_RESTRICTED) &&
            !(partTwo & VMCI_PRIVILEGE_FLAG_TRUSTED)) ||
           ((partTwo & VMCI_PRIVILEGE_FLAG_RESTRICTED) &&
            !(partOne & VMCI_PRIVILEGE_FLAG_TRUSTED)));
}

#endif /* _VMCI_COMMONINT_H_ */
