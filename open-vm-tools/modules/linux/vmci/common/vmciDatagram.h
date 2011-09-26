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
 *    Internal functions in the VMCI Simple Datagram API.
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

#define VMCI_MAX_DELAYED_DG_HOST_QUEUE_SIZE 256

/* Init functions. */
int VMCIDatagram_Init(void);
void VMCIDatagram_Exit(void);

/* Datagram API for non-public use. */
int VMCIDatagram_Dispatch(VMCIId contextID, VMCIDatagram *dg, Bool fromGuest);
int VMCIDatagram_InvokeGuestHandler(VMCIDatagram *dg);
int VMCIDatagram_GetPrivFlags(VMCIHandle handle, VMCIPrivilegeFlags *privFlags);

/* Misc. */
void VMCIDatagram_Sync(void);
Bool VMCIDatagram_CheckHostCapabilities(void);

#endif // _VMCI_DATAGRAM_H_


