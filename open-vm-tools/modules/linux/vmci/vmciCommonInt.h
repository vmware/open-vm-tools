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
#include "includeCheck.h"

#include "vm_atomic.h"
#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmci_infrastructure.h"
#include "vmci_handle_array.h"
#include "vmci_kernel_if.h"
#include "circList.h"

struct DatagramQueueEntry {
   ListItem listItem; /* For queuing. */
   VMCIDatagram *dg;  /* Pending datagram. */
};

struct VMCIProcess {
   ListItem         listItem;           /* For global process list. */
   VMCIId           pid;                /* Process id. */
};

struct VMCIDatagramProcess {
   VMCILock   datagramQueueLock;
   VMCIHandle handle;
   VMCIHost   host;
   uint32     pendingDatagrams;
   size_t     datagramQueueSize;
   ListItem   *datagramQueue;
};

#endif /* _VMCI_COMMONINT_H_ */
