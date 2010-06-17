/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * vmciEvent.h --
 *
 *      Event code for the vmci guest driver
 */

#ifndef __VMCI_EVENT_H__
#define __VMCI_EVENT_H__

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmciKernelAPI.h"

void VMCIEvent_Init(void);
void VMCIEvent_Exit(void);
int  VMCIEvent_Dispatch(VMCIDatagram *msg);
#ifdef VMX86_TOOLS
Bool VMCIEvent_CheckHostCapabilities(void);
#endif

/*
 * Non-public VMCI Event API for the kernel.
 */

int VMCIEventSubscribe(VMCI_Event event, uint32 flags, VMCI_EventCB callback,
                        void *callbackData, VMCIId *subID);
int VMCIEventUnsubscribe(VMCIId subID);

#endif //__VMCI_EVENT_H__
