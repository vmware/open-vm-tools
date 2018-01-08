/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * hostType.h --
 *
 *      Interface to host-specific information functions
 *   
 */

#ifndef _HOSTTYPE_H_
#define _HOSTTYPE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

Bool HostType_OSIsVMK(void);
Bool HostType_OSIsSimulator(void);

/* Old name. TODO: remove */
static INLINE Bool
HostType_OSIsPureVMK(void)
{
   return HostType_OSIsVMK();
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* ifndef _HOSTTYPE_H_ */
