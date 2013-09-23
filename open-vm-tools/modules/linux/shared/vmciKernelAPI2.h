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
 * vmciKernelAPI2.h --
 *
 *    Kernel API (v2) exported from the VMCI host and guest drivers.
 */

#ifndef __VMCI_KERNELAPI_2_H__
#define __VMCI_KERNELAPI_2_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"


#include "vmciKernelAPI1.h"


/* Define version 2. */

#undef  VMCI_KERNEL_API_VERSION
#define VMCI_KERNEL_API_VERSION_2 2
#define VMCI_KERNEL_API_VERSION   VMCI_KERNEL_API_VERSION_2


/* VMCI Doorbell API. */

#define VMCI_FLAG_DELAYED_CB 0x01

typedef void (*VMCICallback)(void *clientData);

int vmci_doorbell_create(VMCIHandle *handle, uint32 flags,
                         VMCIPrivilegeFlags privFlags, VMCICallback notifyCB,
                         void *clientData);
int vmci_doorbell_destroy(VMCIHandle handle);
int vmci_doorbell_notify(VMCIHandle handle, VMCIPrivilegeFlags privFlags);

/* Typedefs for all of the above, used by the IOCTLs and the kernel library. */

typedef int (VMCIDoorbell_CreateFct)(VMCIHandle *, uint32, VMCIPrivilegeFlags,
                                     VMCICallback, void *);
typedef int (VMCIDoorbell_DestroyFct)(VMCIHandle);
typedef int (VMCIDoorbell_NotifyFct)(VMCIHandle, VMCIPrivilegeFlags);


#endif /* !__VMCI_KERNELAPI_2_H__ */
