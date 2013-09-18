/*********************************************************
 * Copyright (C) 2010-2013 VMware, Inc. All rights reserved.
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
 * vmciDoorbell.h --
 *
 *    Internal functions in the VMCI Doorbell API.
 */

#ifndef VMCI_DOORBELL_H
#define VMCI_DOORBELL_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_kernel_if.h"
#include "vmci_defs.h"

int VMCIDoorbell_Init(void);
void VMCIDoorbell_Exit(void);
void VMCIDoorbell_Hibernate(Bool enterHibernation);
void VMCIDoorbell_Sync(void);

int VMCIDoorbellHostContextNotify(VMCIId srcCID, VMCIHandle handle);
int VMCIDoorbellGetPrivFlags(VMCIHandle handle, VMCIPrivilegeFlags *privFlags);

Bool VMCI_RegisterNotificationBitmap(PPN bitmapPPN);
void VMCI_ScanNotificationBitmap(uint8 *bitmap);

#endif // VMCI_DOORBELL_H
