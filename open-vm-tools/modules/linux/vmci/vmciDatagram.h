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
#include "vmci_call_defs.h"
#include "vmci_kernel_if.h"
#include "vmci_infrastructure.h"
#include "vmci_iocontrols.h"

void VMCIDatagram_Init(void);
void VMCIDatagram_Sync(void);
Bool VMCIDatagram_CheckHostCapabilities(void);
int VMCIDatagram_Dispatch(VMCIId contextID, VMCIDatagram *msg);

#endif //__VMCI_DATAGRAM_H__
