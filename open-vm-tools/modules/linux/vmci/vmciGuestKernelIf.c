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
 * vmciGuestKernelIf.c -- 
 * 
 *      This file implements guest only OS helper functions for VMCI.
 *      This is the linux specific implementation.
 */ 

/* Must come before any kernel header file */
#include "driver-config.h"

#if !defined(linux) || defined(VMKERNEL)
#error "Wrong platform."
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 9)
#include <linux/moduleparam.h>
#endif

#include "compat_version.h"
#include "compat_pci.h"
#include "vm_basic_types.h"
#include "vmciGuestKernelIf.h"


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_ReadPortBytes --
 *
 *      Copy memory from an I/O port to kernel memory.
 *
 * Results:
 *      No results.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMCI_ReadPortBytes(VMCIIoHandle handle,  // IN: Unused
		   VMCIIoPort port,      // IN
		   uint8 *buffer,        // OUT
		   size_t bufferLength)  // IN
{
   insb(port, buffer, bufferLength);
}
