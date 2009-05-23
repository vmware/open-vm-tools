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
 * vmciDatagram.h --
 *
 *      Simple Datagram API for the Linux guest driver.
 */

#ifndef __VMCI_DATAGRAM_H__
#define __VMCI_DATAGRAM_H__

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmci_infrastructure.h"
#include "vmciGuestKernelAPI.h"
#include "vmci_iocontrols.h"

typedef struct DatagramQueueEntry DatagramQueueEntry;
typedef struct VMCIDatagramProcess VMCIDatagramProcess;

void VMCIDatagram_Init(void);
Bool VMCIDatagram_CheckHostCapabilities(void);
int VMCIDatagram_Dispatch(VMCIId contextID, VMCIDatagram *msg);

int VMCIDatagramCreateHndInt(VMCIId resourceID,
                             uint32 flags,
                             VMCIDatagramRecvCB recvCB,
                             void *clientData,
                             VMCIHandle *outHandle);
int VMCIDatagramCreateHndPriv(VMCIId resourceID,
                              uint32 flags,
                              VMCIPrivilegeFlags privFlags,
                              VMCIDatagramRecvCB recvCB,
                              void *clientData,
                              VMCIHandle *outHandle); /* Compat */
int VMCIDatagramDestroyHndInt(VMCIHandle handle);

int VMCIDatagramProcess_Create(VMCIDatagramProcess **outDgmProc,
                               VMCIDatagramCreateInfo *createInfo);
void VMCIDatagramProcess_Destroy(VMCIDatagramProcess *dgmProc);
int VMCIDatagramProcess_ReadCall(VMCIDatagramProcess *dgmProc,
				 size_t maxSize, VMCIDatagram **dg);

#endif //__VMCI_DATAGRAM_H__
