/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
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
