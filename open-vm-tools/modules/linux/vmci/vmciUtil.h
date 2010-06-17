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
 * vmciUtil.h --
 *
 *      Helper functions.
 */

#ifndef __VMCI_UTIL_H__
#define __VMCI_UTIL_H__

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vmciGuestKernelIf.h"
#include "vmci_infrastructure.h"

#define VMCI_MAJOR_VERSION_NUMBER   1
#define VMCI_MINOR_VERSION_NUMBER   0
#define VMCI_VERSION_NUMBER         \
         ((VMCI_MAJOR_VERSION_NUMBER << 16) | (VMCI_MINOR_VERSION_NUMBER))

typedef struct VMCIGuestDeviceHandle {
   void *obj;
   VMCIObjType objType;
} VMCIGuestDeviceHandle;

void VMCIUtil_Init(void);
void VMCIUtil_Exit(void);
Bool VMCIUtil_CheckHostCapabilities(void);
Bool VMCI_CheckHostCapabilities(void);
Bool VMCI_InInterrupt(void);
void VMCI_ReadDatagramsFromPort(VMCIIoHandle ioHandle, VMCIIoPort dgInPort,
				uint8 *dgInBuffer, size_t dgInBufferSize);

#endif //__VMCI_UTIL_H__
