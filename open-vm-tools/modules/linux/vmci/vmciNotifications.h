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
 * vmciNotifications.h --
 *
 *     VMCI notifications API for OS device drivers.
 */

#ifndef _VMCI_NOTIFICATIONS_H_
#define _VMCI_NOTIFICATIONS_H_

#include "vm_basic_types.h"
#include "vmci_defs.h"
#include "vmciKernelAPI.h"

void VMCINotifications_Init(void);
void VMCINotifications_Exit(void);
void VMCINotifications_Sync(void);

Bool VMCI_RegisterNotificationBitmap(PPN bitmapPPN);
void VMCI_ScanNotificationBitmap(uint8 *bitmap);

int VMCINotificationRegister(VMCIHandle *handle, Bool doorbell, uint32 flags,
                             VMCICallback notifyCB, void *callbackData);
int VMCINotificationUnregister(VMCIHandle handle, Bool doorbell);

void VMCINotifications_Hibernate(Bool enterHibernation);

#endif /* !_VMCI_NOTIFICATIONS_H_ */

