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
 *	Internal functions in the VMCI Simple Datagram API.
 */

#ifndef _VMCI_DATAGRAM_H_
#define _VMCI_DATAGRAM_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmciContext.h"
#include "vmci_call_defs.h"
#ifndef VMX86_SERVER
#include "vmci_iocontrols.h"
#endif // !VMX86_SERVER

typedef struct VMCIDatagramProcess VMCIDatagramProcess;

/* Init functions. */
int VMCIDatagram_Init(void);
void VMCIDatagram_Exit(void);


/* Datagram API for non-public use in host context*/
int VMCIDatagramCreateHndInt(VMCIId resourceID, uint32 flags,
			     VMCIDatagramRecvCB recvCB, void *clientData,
   			     VMCIHandle *outHandle);
int VMCIDatagramCreateHndPriv(VMCIId resourceID, uint32 flags,
			      VMCIPrivilegeFlags privFlags,
			      VMCIDatagramRecvCB recvCB, void *clientData,
			      VMCIHandle *outHandle);
int VMCIDatagramDestroyHndInt(VMCIHandle handle);
int VMCIDatagram_Dispatch(VMCIId contextID, VMCIDatagram *dg);
int VMCIDatagramSendInt(VMCIDatagram *msg);
int VMCIDatagram_GetPrivFlags(VMCIHandle handle, VMCIPrivilegeFlags *privFlags);

/* Non public datagram API. */
int VMCIDatagramRequestWellKnownMap(VMCIId wellKnownID, VMCIId contextID,
				    VMCIPrivilegeFlags privFlags);
int VMCIDatagramRemoveWellKnownMap(VMCIId wellKnownID, VMCIId contextID);

#ifndef VMX86_SERVER
/* Userlevel process datagram API for host context. */
int VMCIDatagramProcess_Create(VMCIDatagramProcess **outDgmProc,
                               VMCIDatagramCreateInfo *createInfo,
                               uintptr_t eventHnd);
void VMCIDatagramProcess_Destroy(VMCIDatagramProcess *dgmProc);
int VMCIDatagramProcess_ReadCall(VMCIDatagramProcess *dgmProc,
                                 size_t maxSize, VMCIDatagram **dg);
#endif // !VMX86_SERVER

#endif // _VMCI_DATAGRAM_H_


