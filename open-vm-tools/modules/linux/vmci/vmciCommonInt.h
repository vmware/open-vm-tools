/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
#define INCLUDE_ALLOW_VMMON
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
   VMCILock           lock;             /* Locks callQueue and handleArrays. */
   VMCIHandleArray    *wellKnownArray;  /* WellKnown mappings owned by context. */
   VMCIHandleArray    *queuePairArray;  /*
                                         * QueuePairs attached to.  The array of
                                         * handles for queue pairs is accessed
                                         * from the code for QP API, and there
                                         * it is protected by the QP lock.  It
                                         * is also accessed from the context
                                         * clean up path, which does not
                                         * require a lock.  VMCILock is not
                                         * used to protect the QP array field.
                                         */
   VMCIHandleArray    *doorbellArray;   /* Doorbells created by context. */
   VMCIHandleArray    *pendingDoorbellArray; /* Doorbells pending for context. */
   VMCIHandleArray    *notifierArray;   /* Contexts current context is subscribing to. */
   VMCIHost           hostContext;
   VMCIPrivilegeFlags privFlags;
   VMCIHostUser       user;
   Bool               validUser;
#ifdef VMKERNEL
   char               domainName[VMCI_DOMAIN_NAME_MAXLEN];
   Bool               isQuiesced;       /* Whether current VM is quiesced */
   VMCIId             migrateCid;       /* The migrate cid if it is migrating */
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
 *     be trusted. On ESX, the vmci domain must match for unrestricted
 *     domains.
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
                    VMCIPrivilegeFlags partTwo,  // IN
                    const char *srcDomain,       // IN:  Unused on hosted
                    const char *dstDomain)       // IN:  Unused on hosted
{
#ifndef VMKERNEL
   return (((partOne & VMCI_PRIVILEGE_FLAG_RESTRICTED) &&
            !(partTwo & VMCI_PRIVILEGE_FLAG_TRUSTED)) ||
           ((partTwo & VMCI_PRIVILEGE_FLAG_RESTRICTED) &&
            !(partOne & VMCI_PRIVILEGE_FLAG_TRUSTED)));
#else
   /*
    * If source or destination is trusted (hypervisor), we allow the
    * communication.
    */
   if ((partOne & VMCI_PRIVILEGE_FLAG_TRUSTED) ||
       (partTwo & VMCI_PRIVILEGE_FLAG_TRUSTED)) {
      return FALSE;
   }
   /*
    * If source or destination is restricted, we deny the communication.
    */
   if ((partOne & VMCI_PRIVILEGE_FLAG_RESTRICTED) ||
       (partTwo & VMCI_PRIVILEGE_FLAG_RESTRICTED)) {
      return TRUE;
   }
   /*
    * We are here, means that neither of source or destination are trusted, and
    * both are unrestricted.
    */
   ASSERT(!(partOne & VMCI_PRIVILEGE_FLAG_TRUSTED) &&
          !(partTwo & VMCI_PRIVILEGE_FLAG_TRUSTED));
   ASSERT(!(partOne & VMCI_PRIVILEGE_FLAG_RESTRICTED) &&
          !(partTwo & VMCI_PRIVILEGE_FLAG_RESTRICTED));
   /*
    * We now compare the source and destination domain names, and allow
    * communication iff they match.
    */
   return strcmp(srcDomain, dstDomain) ? TRUE : /* Deny. */
                                         FALSE; /* Allow. */
#endif
}

#endif /* _VMCI_COMMONINT_H_ */
