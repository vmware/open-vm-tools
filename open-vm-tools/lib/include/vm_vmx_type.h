/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
#ifndef VM_VMX_TYPE_H
#define VM_VMX_TYPE_H

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * This allows UIs and guest binaries to know what kind of VMX they
 * are dealing with. Don't change those values (only add new ones if
 * needed) because they rely on them --hpreg
 */

typedef enum {
   VMX_TYPE_UNSET,
   VMX_TYPE_EXPRESS, /* This deprecated type was used for VMware Express */
   VMX_TYPE_SCALABLE_SERVER,
   VMX_TYPE_WGS, /* This deprecated type was used for VMware Server */
   VMX_TYPE_WORKSTATION,
   VMX_TYPE_WORKSTATION_ENTERPRISE /* This deprecated type was used for ACE 1.x */
} VMX_Type;


/*
 * This allows UIs and guest binaries to know what platform the VMX is
 * running.
 */

typedef enum {
   VMX_PLATFORM_UNSET,
   VMX_PLATFORM_LINUX,
   VMX_PLATFORM_WIN32,
   VMX_PLATFORM_MACOS,
} VMX_Platform;

#endif
