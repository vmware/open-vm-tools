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
 * vmci_infrastructure.h --
 *
 *      This file implements the VMCI infrastructure.
 */

#ifndef _VMCI_INFRASTRUCTURE_H_
#define _VMCI_INFRASTRUCTURE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMMEXT
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vmware.h"
#include "vmci_defs.h"

typedef enum {
   VMCIOBJ_VMX_VM = 10,
   VMCIOBJ_CONTEXT,
   VMCIOBJ_PROCESS,
   VMCIOBJ_DATAGRAM_PROCESS,
   VMCIOBJ_NOT_SET,
} VMCIObjType;

/* Guestcalls currently support a maximum of 8 uint64 arguments. */
#define VMCI_GUESTCALL_MAX_ARGS_SIZE 64

/* Used to determine what checkpoint state to get and set. */
#define VMCI_NOTIFICATION_CPT_STATE 0x1
#define VMCI_WELLKNOWN_CPT_STATE 0x2
#define VMCI_QP_CPT_STATE 0x3
#define VMCI_QP_INFO_CPT_STATE 0x4

/* Used to control the VMCI device in the vmkernel */
#define VMCI_DEV_RESET            0x01
#define VMCI_DEV_QP_RESET         0x02
#define VMCI_DEV_QUIESCE          0x03
#define VMCI_DEV_UNQUIESCE        0x04
#define VMCI_DEV_QP_BREAK_SHARING 0x05

/*
 *-------------------------------------------------------------------------
 *
 *  VMCI_Hash --
 *
 *     Hash function used by the Simple Datagram API. Based on the djb2
 *     hash function by Dan Bernstein.
 *
 *  Result:
 *     Returns guest call size.
 *
 *  Side effects:
 *     None.
 *
 *-------------------------------------------------------------------------
 */

static INLINE int
VMCI_Hash(VMCIHandle handle, // IN
          unsigned size)     // IN
{
   unsigned     i;
   int          hash        = 5381;
   const uint64 handleValue = QWORD(handle.resource, handle.context);

   for (i = 0; i < sizeof handle; i++) {
      hash = ((hash << 5) + hash) + (uint8)(handleValue >> (i * 8));
   }
   return hash & (size - 1);
}

#endif // _VMCI_INFRASTRUCTURE_H_
