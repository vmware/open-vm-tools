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
 * vmciProcess.h --
 *
 *      Process code for the Linux guest driver
 */

#ifndef __VMCI_PROCESS_H__
#define __VMCI_PROCESS_H__

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"

#include "vmci_defs.h"
#include "vmci_handle_array.h"

typedef struct VMCIProcess VMCIProcess;

void VMCIProcess_Init(void);
void VMCIProcess_Exit(void);
Bool VMCIProcess_CheckHostCapabilities(void);
int VMCIProcess_Create(VMCIProcess **outProcess);
void VMCIProcess_Destroy(VMCIProcess *process);
VMCIProcess *VMCIProcess_Get(VMCIId processID);
		
#endif //__VMCI_PROCESS_H__
