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

#ifndef __VMCI_INT_H__
#define __VMCI_INT_H__

#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vmci_call_defs.h"
#include "vmciProcess.h"

#define DOLOG(...) printk(KERN_INFO __VA_ARGS__)
#define VMCI_LOG(_args) DOLOG _args
/* XXX We need to make this consistant between the guest and the host. */
#define VMCILOG(_args) DOLOG _args

/* 
 * Called by common code, hence the different naming convention. 
 * XXX Should be in vmci.h.
 */
int VMCI_SendDatagram(VMCIDatagram *dg);
Bool VMCI_DeviceEnabled(void);

#endif /* __VMCIINT_H__ */
