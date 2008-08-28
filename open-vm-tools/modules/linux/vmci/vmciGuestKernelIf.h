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
 * vmciGuestKernelIf.h -- 
 * 
 *      This file defines OS encapsulation helper functions that are
 *      needed only in VMCI guest kernel code. It must work for
 *      windows, solaris and linux kernel, ie. using defines
 *      where necessary.
 */ 
 
#ifndef _VMCI_GUEST_KERNEL_IF_H_
#define _VMCI_GUEST_KERNEL_IF_H_

#if !defined(linux) && !defined(_WIN32) && !defined(SOLARIS)
#error "Platform not supported."
#endif

#if defined(_WIN32)
#include <ntddk.h>
#endif 

#ifdef SOLARIS
#  include <sys/ddi.h>
#  include <sys/sunddi.h> 
#  include <sys/types.h>
#endif

#include "vm_basic_types.h"
#include "vmci_defs.h"

#if defined(linux)
  typedef unsigned short int VMCIIoPort;
  typedef int VMCIIoHandle;
#elif defined(_WIN32)
  typedef PUCHAR VMCIIoPort;
  typedef int VMCIIoHandle;
#elif defined(SOLARIS)
  typedef uint8_t * VMCIIoPort;
  typedef ddi_acc_handle_t VMCIIoHandle;
#endif // VMKERNEL

void VMCI_ReadPortBytes(VMCIIoHandle handle, VMCIIoPort port, uint8 *buffer,
			size_t bufferLength);

#endif // _VMCI_GUEST_KERNEL_IF_H_

