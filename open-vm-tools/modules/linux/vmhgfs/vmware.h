/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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
 * vmware.h --
 *
 *	Standard include file for VMware source code.
 */

#ifndef _VMWARE_H_
#define _VMWARE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMNIXMOD
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"

/*
 * Global error codes. Currently used internally, but may be exported
 * to customers one day, like VM_E_XXX in vmcontrol_constants.h
 */

typedef enum VMwareStatus {
   VMWARE_STATUS_SUCCESS,  /* success */
   VMWARE_STATUS_ERROR,    /* generic error */
   VMWARE_STATUS_NOMEM,    /* generic memory allocation error */
   VMWARE_STATUS_INSUFFICIENT_RESOURCES, /* internal or system resource limit exceeded */
   VMWARE_STATUS_INVALID_ARGS  /* invalid arguments */
} VMwareStatus;

#define VMWARE_SUCCESS(s) ((s) == VMWARE_STATUS_SUCCESS)


#endif // ifndef _VMWARE_H_
