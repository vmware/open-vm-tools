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
 * vmciResource.c --
 *
 *     Implementation of the VMCI Resource Access Control API.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciHashtable.h"
#include "vmciResource.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#  include "vmciDriver.h"
#else
#  include "vmciDriver.h"
#endif

#define LGPFX "VMCIResource: "

/* 0 through VMCI_RESERVED_RESOURCE_ID_MAX are reserved. */
static uint32 resourceID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
static VMCILock resourceIdLock;

static void VMCIResourceDoRemove(VMCIResource *resource);

static VMCIHashTable *resourceTable = NULL;


/* Public Resource Access Control API. */

/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Init --
 *
 *      Initializes the VMCI Resource Access Control API. Creates a hashtable
 *      to hold all resources, and registers vectors and callbacks for
 *      hypercalls.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIResource_Init(void)
{
   int err = VMCI_InitLock(&resourceIdLock, "VMCIRIDLock",
                           VMCI_LOCK_RANK_RESOURCE);
   if (err < VMCI_SUCCESS) {
      return err;
   }

   resourceTable = VMCIHashTable_Create(128);
   if (resourceTable == NULL) {
      VMCI_WARNING((LGPFX"Failed creating a resource hash table for VMCI.\n"));
      VMCI_CleanupLock(&resourceIdLock);
      return VMCI_ERROR_NO_MEM;
   }

   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Exit --
 *
 *      Cleans up resources.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIResource_Exit(void)
{
   /* Cleanup resources.*/
   VMCI_CleanupLock(&resourceIdLock);

   if (resourceTable) {
      VMCIHashTable_Destroy(resourceTable);
   }
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIResource_GetID --
 *
 *     Return resource ID. The first VMCI_RESERVED_RESOURCE_ID_MAX are
 *     reserved so we start from its value + 1.
 *
 *  Result:
 *     VMCI resource id on success, VMCI_INVALID_ID on failure.
 *
 *  Side effects:
 *     None.
 *
 *
 *------------------------------------------------------------------------------
 */

VMCIId
VMCIResource_GetID(VMCIId contextID)
{
   VMCIId oldRID = resourceID;
   VMCIId currentRID;
   Bool foundRID = FALSE;

   /*
    * Generate a unique resource ID.  Keep on trying until we wrap around
    * in the RID space.
    */
   ASSERT(oldRID > VMCI_RESERVED_RESOURCE_ID_MAX);

   do {
      VMCILockFlags flags;
      VMCIHandle handle;

      VMCI_GrabLock(&resourceIdLock, &flags);
      currentRID = resourceID;
      handle = VMCI_MAKE_HANDLE(contextID, currentRID);
      resourceID++;
      if (UNLIKELY(resourceID == VMCI_INVALID_ID)) {
         /*
          * Skip the reserved rids.
          */

         resourceID = VMCI_RESERVED_RESOURCE_ID_MAX + 1;
      }
      VMCI_ReleaseLock(&resourceIdLock, flags);
      foundRID = !VMCIHashTable_EntryExists(resourceTable, handle);
   } while (!foundRID && resourceID != oldRID);

   if (UNLIKELY(!foundRID)) {
      return VMCI_INVALID_ID;
   } else {
      return currentRID;
   }
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Add --
 *
 * Results:
 *      VMCI_SUCCESS if successful, error code if not.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIResource_Add(VMCIResource *resource,                // IN
                 VMCIResourceType resourceType,         // IN
                 VMCIHandle resourceHandle,             // IN
                 VMCIResourceFreeCB containerFreeCB,    // IN
                 void *containerObject)                 // IN
{
   int result;

   ASSERT(resource);

   if (VMCI_HANDLE_EQUAL(resourceHandle, VMCI_INVALID_HANDLE)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Invalid argument resource (handle=0x%x:0x%x).\n",
                         resourceHandle.context, resourceHandle.resource));
      return VMCI_ERROR_INVALID_ARGS;
   }

   VMCIHashTable_InitEntry(&resource->hashEntry, resourceHandle);
   resource->type = resourceType;
   resource->containerFreeCB = containerFreeCB;
   resource->containerObject = containerObject;

   /* Add resource to hashtable. */
   result = VMCIHashTable_AddEntry(resourceTable, &resource->hashEntry);
   if (result != VMCI_SUCCESS) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to add entry to hash table "
                         "(result=%d).\n", result));
      return result;
   }

   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Remove --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIResource_Remove(VMCIHandle resourceHandle,     // IN:
                    VMCIResourceType resourceType) // IN:
{
   VMCIResource *resource = VMCIResource_Get(resourceHandle, resourceType);
   if (resource == NULL) {
      return;
   }

   /* Remove resource from hashtable. */
   VMCIHashTable_RemoveEntry(resourceTable, &resource->hashEntry);

   VMCIResource_Release(resource);
   /* resource could be freed by now. */
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Get --
 *
 * Results:
 *      Resource is successful. Otherwise NULL.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCIResource *
VMCIResource_Get(VMCIHandle resourceHandle,     // IN
                 VMCIResourceType resourceType) // IN
{
   VMCIResource *resource;
   VMCIHashEntry *entry = VMCIHashTable_GetEntry(resourceTable, resourceHandle);
   if (entry == NULL) {
      return NULL;
   }
   resource = RESOURCE_CONTAINER(entry, VMCIResource, hashEntry);
   if (resourceType == VMCI_RESOURCE_TYPE_ANY ||
       resource->type == resourceType) {
      return resource;
   }
   VMCIHashTable_ReleaseEntry(resourceTable, entry);
   return NULL;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Hold --
 *
 *      Hold the given resource.  This will hold the hashtable entry.  This
 *      is like doing a Get() but without having to lookup the resource by
 *      handle.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIResource_Hold(VMCIResource *resource)
{
   ASSERT(resource);
   VMCIHashTable_HoldEntry(resourceTable, &resource->hashEntry);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResourceDoRemove --
 *
 *      Deallocates data structures associated with the given resource
 *      and invoke any call back registered for the resource.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May deallocate memory and invoke a callback for the removed resource.
 *
 *------------------------------------------------------------------------------
 */

static void INLINE
VMCIResourceDoRemove(VMCIResource *resource)
{
   ASSERT(resource);

   if (resource->containerFreeCB) {
      resource->containerFreeCB(resource->containerObject);
      /* Resource has been freed don't dereference it. */
   }
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Release --
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      resource's containerFreeCB will get called if last reference.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIResource_Release(VMCIResource *resource)
{
   int result;

   ASSERT(resource);

   result = VMCIHashTable_ReleaseEntry(resourceTable, &resource->hashEntry);
   if (result == VMCI_SUCCESS_ENTRY_DEAD) {
      VMCIResourceDoRemove(resource);
   }

   /*
    * We propagate the information back to caller in case it wants to know
    * whether entry was freed.
    */
   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Handle --
 *
 *      Get the handle for the given resource.
 *
 * Results:
 *      The resource's associated handle.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

VMCIHandle
VMCIResource_Handle(VMCIResource *resource)
{
   ASSERT(resource);
   return resource->hashEntry.handle;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIResource_Sync --
 *
 *      Use this as a synchronization point when setting globals, for example,
 *      during device shutdown.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIResource_Sync(void)
{
   VMCIHashTable_Sync(resourceTable);
}
