/*********************************************************
 * Copyright (C) 2011-2013 VMware, Inc. All rights reserved.
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
 * vmciRoute.h --
 *
 *    VMCI Routing.
 */

#ifndef _VMCI_ROUTE_H_
#define _VMCI_ROUTE_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"


typedef enum {
   VMCI_ROUTE_NONE,
   VMCI_ROUTE_AS_HOST,
   VMCI_ROUTE_AS_GUEST,
} VMCIRoute;


int VMCI_Route(VMCIHandle *src, const VMCIHandle *dst, Bool fromGuest,
               VMCIRoute *route);
const char *VMCI_RouteString(VMCIRoute route);


#endif // _VMCI_ROUTE_H_
