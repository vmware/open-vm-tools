/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * stubs.c
 *
 * Stubs for host functions still missing from the guest driver.
 *
 */

#ifdef __linux__
#  include "driver-config.h"
#endif

#include "vmci_kernel_if.h"
#include "vmci_defs.h"
#include "vmciContext.h"
#include "vmciDoorbell.h"


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIQPBroker_Lock --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */
 
void
VMCIQPBroker_Lock(void)
{
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIQPBroker_Unlock --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIQPBroker_Unlock(void)
{
}


/*
 *------------------------------------------------------------------------------
 *
 *  QueuePair_Detach --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIQPBroker_Detach(VMCIHandle handle,
                    VMCIContext *context,
                    Bool detach)
{
   return VMCI_ERROR_GENERIC;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDoorbellHostContextNotify --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDoorbellHostContextNotify(VMCIId srcCID,
                              VMCIHandle handle)
{
   return VMCI_ERROR_GENERIC;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIDoorbellGetPrivFlags --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDoorbellGetPrivFlags(VMCIHandle handle,
                         VMCIPrivilegeFlags *privFlags)
{
   return VMCI_ERROR_GENERIC;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIUnsetNotify --
 *
 *     Stub.  Not called in the guest driver (yet).
 *
 *  Result:
 *     Always VMCI_ERROR_GENERIC.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIUnsetNotify(VMCIContext *context)
{
}
