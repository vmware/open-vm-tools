/*********************************************************
 * Copyright (C) 2006-2013 VMware, Inc. All rights reserved.
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
 * vmciResource.h --
 *
 *    VMCI Resource Access Control API.
 */

#ifndef _VMCI_RESOURCE_H_
#define _VMCI_RESOURCE_H_

#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_defs.h"
#include "vmci_kernel_if.h"
#include "vmciHashtable.h"
#include "vmciContext.h"

#define RESOURCE_CONTAINER(ptr, type, member) \
   ((type *)((char *)(ptr) - offsetof(type, member)))

typedef void(*VMCIResourceFreeCB)(void *resource);

typedef enum {
   VMCI_RESOURCE_TYPE_ANY,
   VMCI_RESOURCE_TYPE_API,
   VMCI_RESOURCE_TYPE_GROUP,
   VMCI_RESOURCE_TYPE_DATAGRAM,
   VMCI_RESOURCE_TYPE_DOORBELL,
} VMCIResourceType;

typedef struct VMCIResource {
   VMCIHashEntry         hashEntry;
   VMCIResourceType      type;
   VMCIResourceFreeCB    containerFreeCB;    // Callback to free container
                                             // object when refCount is 0.
   void                  *containerObject;   // Container object reference.
} VMCIResource;


int VMCIResource_Init(void);
void VMCIResource_Exit(void);
void VMCIResource_Sync(void);

VMCIId VMCIResource_GetID(VMCIId contextID);

int VMCIResource_Add(VMCIResource *resource, VMCIResourceType resourceType,
                     VMCIHandle resourceHandle,
                     VMCIResourceFreeCB containerFreeCB, void *containerObject);
void VMCIResource_Remove(VMCIHandle resourceHandle,
                         VMCIResourceType resourceType);
VMCIResource *VMCIResource_Get(VMCIHandle resourceHandle,
                               VMCIResourceType resourceType);
void VMCIResource_Hold(VMCIResource *resource);
int VMCIResource_Release(VMCIResource *resource);
VMCIHandle VMCIResource_Handle(VMCIResource *resource);


#endif // _VMCI_RESOURCE_H_
