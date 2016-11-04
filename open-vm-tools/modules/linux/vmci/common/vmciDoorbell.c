/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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
 * vmciDoorbell.c --
 *
 *    This file implements the VMCI doorbell API on the host.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciDatagram.h"
#include "vmciDoorbell.h"
#include "vmciDriver.h"
#include "vmciKernelAPI.h"
#include "vmciResource.h"
#include "vmciRoute.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIDoorbell: "

#if !defined(__APPLE__)

#define VMCI_DOORBELL_INDEX_TABLE_SIZE 64
#define VMCI_DOORBELL_HASH(_idx) \
   VMCI_HashId((_idx), VMCI_DOORBELL_INDEX_TABLE_SIZE)


/*
 * DoorbellEntry describes the a doorbell notification handle allocated by the
 * host.
 */

typedef struct VMCIDoorbellEntry {
   VMCIResource        resource;
   uint32              idx;
   VMCIListItem        idxListItem;
   VMCIPrivilegeFlags  privFlags;
   Bool                isDoorbell;
   Bool                runDelayed;
   VMCICallback        notifyCB;
   void                *clientData;
   VMCIEvent           destroyEvent;
   Atomic_uint32       active;       // Only used by guest personality
} VMCIDoorbellEntry;

typedef struct VMCIDoorbellIndexTable {
   VMCILock lock;
   VMCIList entries[VMCI_DOORBELL_INDEX_TABLE_SIZE];
} VMCIDoorbellIndexTable;


/* The VMCI index table keeps track of currently registered doorbells. */
static VMCIDoorbellIndexTable vmciDoorbellIT;


/*
 * The maxNotifyIdx is one larger than the currently known bitmap index in
 * use, and is used to determine how much of the bitmap needs to be scanned.
 */

static uint32 maxNotifyIdx;

/*
 * The notifyIdxCount is used for determining whether there are free entries
 * within the bitmap (if notifyIdxCount + 1 < maxNotifyIdx).
 */

static uint32 notifyIdxCount;

/*
 * The lastNotifyIdxReserved is used to track the last index handed out - in
 * the case where multiple handles share a notification index, we hand out
 * indexes round robin based on lastNotifyIdxReserved.
 */

static uint32 lastNotifyIdxReserved;

/* This is a one entry cache used to by the index allocation. */
static uint32 lastNotifyIdxReleased = PAGE_SIZE;


static void VMCIDoorbellFreeCB(void *clientData);
static int VMCIDoorbellReleaseCB(void *clientData);
static void VMCIDoorbellDelayedDispatchCB(void *data);


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbell_Init --
 *
 *    General init code.
 *
 * Result:
 *    VMCI_SUCCESS on success, lock allocation error otherwise.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDoorbell_Init(void)
{
   uint32 bucket;

   for (bucket = 0; bucket < ARRAYSIZE(vmciDoorbellIT.entries); ++bucket) {
      VMCIList_Init(&vmciDoorbellIT.entries[bucket]);
   }

   return VMCI_InitLock(&vmciDoorbellIT.lock, "VMCIDoorbellIndexTableLock",
                        VMCI_LOCK_RANK_DOORBELL);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbell_Exit --
 *
 *    General init code.
 *
 * Result:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIDoorbell_Exit(void)
{
   VMCI_CleanupLock(&vmciDoorbellIT.lock);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellFreeCB --
 *
 *    Callback to free doorbell entry structure when resource is no longer used,
 *    ie. the reference count reached 0.  The entry is freed in
 *    VMCIDoorbell_Destroy(), which is waiting on the signal that gets fired
 *    here.
 *
 * Result:
 *    None.
 *
 * Side effects:
 *    Signals VMCI event.
 *
 *------------------------------------------------------------------------------
 */

static void
VMCIDoorbellFreeCB(void *clientData)  // IN
{
   VMCIDoorbellEntry *entry = (VMCIDoorbellEntry *)clientData;
   ASSERT(entry);
   VMCI_SignalEvent(&entry->destroyEvent);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellReleaseCB --
 *
 *     Callback to release the resource reference. It is called by the
 *     VMCI_WaitOnEvent function before it blocks.
 *
 * Result:
 *     Always 0.
 *
 * Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDoorbellReleaseCB(void *clientData) // IN: doorbell entry
{
   VMCIDoorbellEntry *entry = (VMCIDoorbellEntry *)clientData;
   ASSERT(entry);
   VMCIResource_Release(&entry->resource);
   return 0;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellGetPrivFlags --
 *
 *    Utility function that retrieves the privilege flags associated
 *    with a given doorbell handle. For guest endpoints, the
 *    privileges are determined by the context ID, but for host
 *    endpoints privileges are associated with the complete
 *    handle. Hypervisor endpoints are not yet supported.
 *
 * Result:
 *    VMCI_SUCCESS on success,
 *    VMCI_ERROR_NOT_FOUND if handle isn't found,
 *    VMCI_ERROR_INVALID_ARGS if handle is invalid.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDoorbellGetPrivFlags(VMCIHandle handle,             // IN
                         VMCIPrivilegeFlags *privFlags) // OUT
{
   if (privFlags == NULL || handle.context == VMCI_INVALID_ID) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (handle.context == VMCI_HOST_CONTEXT_ID) {
      VMCIDoorbellEntry *entry;
      VMCIResource *resource;

      resource = VMCIResource_Get(handle, VMCI_RESOURCE_TYPE_DOORBELL);
      if (resource == NULL) {
         return VMCI_ERROR_NOT_FOUND;
      }
      entry = RESOURCE_CONTAINER(resource, VMCIDoorbellEntry, resource);
      *privFlags = entry->privFlags;
      VMCIResource_Release(resource);
   } else if (handle.context == VMCI_HYPERVISOR_CONTEXT_ID) {
       /* Hypervisor endpoints for notifications are not supported (yet). */
      return VMCI_ERROR_INVALID_ARGS;
   } else {
      *privFlags = vmci_context_get_priv_flags(handle.context);
   }

   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbellIndexTableFind --
 *
 *    Find doorbell entry by bitmap index.
 *
 * Results:
 *    Entry if found, NULL if not.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VMCIDoorbellEntry *
VMCIDoorbellIndexTableFind(uint32 idx) // IN
{
   uint32 bucket = VMCI_DOORBELL_HASH(idx);
   VMCIListItem *iter;

   ASSERT(VMCI_GuestPersonalityActive());

   VMCIList_Scan(iter, &vmciDoorbellIT.entries[bucket]) {
      VMCIDoorbellEntry *cur =
         VMCIList_Entry(iter, VMCIDoorbellEntry, idxListItem);

      ASSERT(cur);

      if (idx == cur->idx) {
         return cur;
      }
   }

   return NULL;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellIndexTableAdd --
 *
 *    Add the given entry to the index table.  This will hold() the entry's
 *    resource so that the entry is not deleted before it is removed from the
 *    table.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

static void
VMCIDoorbellIndexTableAdd(VMCIDoorbellEntry *entry) // IN/OUT
{
   uint32 bucket;
   uint32 newNotifyIdx;
   VMCILockFlags flags;

   ASSERT(entry);
   ASSERT(VMCI_GuestPersonalityActive());

   VMCIResource_Hold(&entry->resource);

   VMCI_GrabLock_BH(&vmciDoorbellIT.lock, &flags);

   /*
    * Below we try to allocate an index in the notification bitmap with "not
    * too much" sharing between resources. If we use less that the full bitmap,
    * we either add to the end if there are no unused flags within the
    * currently used area, or we search for unused ones. If we use the full
    * bitmap, we allocate the index round robin.
    */

   if (maxNotifyIdx < PAGE_SIZE || notifyIdxCount < PAGE_SIZE) {
      if (lastNotifyIdxReleased < maxNotifyIdx &&
          !VMCIDoorbellIndexTableFind(lastNotifyIdxReleased)) {
         newNotifyIdx = lastNotifyIdxReleased;
         lastNotifyIdxReleased = PAGE_SIZE;
      } else {
         Bool reused = FALSE;
         newNotifyIdx = lastNotifyIdxReserved;
         if (notifyIdxCount + 1 < maxNotifyIdx) {
            do {
               if (!VMCIDoorbellIndexTableFind(newNotifyIdx)) {
                  reused = TRUE;
                  break;
               }
               newNotifyIdx = (newNotifyIdx + 1) % maxNotifyIdx;
            } while(newNotifyIdx != lastNotifyIdxReleased);
         }
         if (!reused) {
            newNotifyIdx = maxNotifyIdx;
            maxNotifyIdx++;
         }
      }
   } else {
      newNotifyIdx = (lastNotifyIdxReserved + 1) % PAGE_SIZE;
   }
   lastNotifyIdxReserved = newNotifyIdx;
   notifyIdxCount++;

   entry->idx = newNotifyIdx;
   bucket = VMCI_DOORBELL_HASH(entry->idx);
   VMCIList_Insert(&entry->idxListItem, &vmciDoorbellIT.entries[bucket]);

   VMCI_ReleaseLock_BH(&vmciDoorbellIT.lock, flags);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellIndexTableRemove --
 *
 *    Remove the given entry from the index table.  This will release() the
 *    entry's resource.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

static void
VMCIDoorbellIndexTableRemove(VMCIDoorbellEntry *entry) // IN/OUT
{
   VMCILockFlags flags;

   ASSERT(entry);
   ASSERT(VMCI_GuestPersonalityActive());

   VMCI_GrabLock_BH(&vmciDoorbellIT.lock, &flags);

   VMCIList_Remove(&entry->idxListItem);

   notifyIdxCount--;
   if (entry->idx == maxNotifyIdx - 1) {
      /*
       * If we delete an entry with the maximum known notification index, we
       * take the opportunity to prune the current max. As there might be other
       * unused indices immediately below, we lower the maximum until we hit an
       * index in use.
       */

      while (maxNotifyIdx > 0 &&
             !VMCIDoorbellIndexTableFind(maxNotifyIdx - 1)) {
         maxNotifyIdx--;
      }
   }
   lastNotifyIdxReleased = entry->idx;

   VMCI_ReleaseLock_BH(&vmciDoorbellIT.lock, flags);

   VMCIResource_Release(&entry->resource);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellLink --
 *
 *    Creates a link between the given doorbell handle and the given
 *    index in the bitmap in the device backend.
 *
 * Results:
 *    VMCI_SUCCESS if success, appropriate error code otherwise.
 *
 * Side effects:
 *    Notification state is created in hypervisor.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDoorbellLink(VMCIHandle handle, // IN
                 Bool isDoorbell,   // IN
                 uint32 notifyIdx)  // IN
{
#if defined(VMKERNEL)
   VMCI_WARNING((LGPFX"Cannot send down to host from VMKERNEL.\n"));
   return VMCI_ERROR_DST_UNREACHABLE;
#else // VMKERNEL
   VMCIId resourceID;
   VMCIDoorbellLinkMsg linkMsg;

   ASSERT(!VMCI_HANDLE_INVALID(handle));
   ASSERT(VMCI_GuestPersonalityActive());

   if (isDoorbell) {
      resourceID = VMCI_DOORBELL_LINK;
   } else {
      ASSERT(FALSE);
      return VMCI_ERROR_UNAVAILABLE;
   }

   linkMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, resourceID);
   linkMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   linkMsg.hdr.payloadSize = sizeof linkMsg - VMCI_DG_HEADERSIZE;
   linkMsg.handle = handle;
   linkMsg.notifyIdx = notifyIdx;

   return VMCI_SendDatagram((VMCIDatagram *)&linkMsg);
#endif // VMKERNEL
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellUnlink --
 *
 *    Unlinks the given doorbell handle from an index in the bitmap in
 *    the device backend.
 *
 * Results:
 *      VMCI_SUCCESS if success, appropriate error code otherwise.
 *
 * Side effects:
 *      Notification state is destroyed in hypervisor.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDoorbellUnlink(VMCIHandle handle, // IN
                   Bool isDoorbell)   // IN
{
#if defined(VMKERNEL)
   VMCI_WARNING((LGPFX"Cannot send down to host from VMKERNEL.\n"));
   return VMCI_ERROR_DST_UNREACHABLE;
#else // VMKERNEL
   VMCIId resourceID;
   VMCIDoorbellUnlinkMsg unlinkMsg;

   ASSERT(!VMCI_HANDLE_INVALID(handle));
   ASSERT(VMCI_GuestPersonalityActive());

   if (isDoorbell) {
      resourceID = VMCI_DOORBELL_UNLINK;
   } else {
      ASSERT(FALSE);
      return VMCI_ERROR_UNAVAILABLE;
   }

   unlinkMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, resourceID);
   unlinkMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   unlinkMsg.hdr.payloadSize = sizeof unlinkMsg - VMCI_DG_HEADERSIZE;
   unlinkMsg.handle = handle;

   return VMCI_SendDatagram((VMCIDatagram *)&unlinkMsg);
#endif // VMKERNEL
}


/*
 *------------------------------------------------------------------------------
 *
 * vmci_doorbell_create --
 *
 *    Creates a doorbell with the given callback. If the handle is
 *    VMCI_INVALID_HANDLE, a free handle will be assigned, if
 *    possible. The callback can be run immediately (potentially with
 *    locks held - the default) or delayed (in a kernel thread) by
 *    specifying the flag VMCI_FLAG_DELAYED_CB. If delayed execution
 *    is selected, a given callback may not be run if the kernel is
 *    unable to allocate memory for the delayed execution (highly
 *    unlikely).
 *
 * Results:
 *    VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_doorbell_create)
int
vmci_doorbell_create(VMCIHandle *handle,            // IN/OUT
                     uint32 flags,                  // IN
                     VMCIPrivilegeFlags privFlags,  // IN
                     VMCICallback notifyCB,         // IN
                     void *clientData)              // IN
{
   VMCIDoorbellEntry *entry;
   VMCIHandle newHandle;
   int result;

   if (!handle || !notifyCB || flags & ~VMCI_FLAG_DELAYED_CB ||
       privFlags & ~VMCI_PRIVILEGE_ALL_FLAGS) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NONPAGED);
   if (entry == NULL) {
      VMCI_WARNING((LGPFX"Failed allocating memory for datagram entry.\n"));
      return VMCI_ERROR_NO_MEM;
   }

   if (!VMCI_CanScheduleDelayedWork() && (flags & VMCI_FLAG_DELAYED_CB)) {
      result = VMCI_ERROR_INVALID_ARGS;
      goto freeMem;
   }

   if (VMCI_HANDLE_INVALID(*handle)) {
      VMCIId contextID = vmci_get_context_id();
      VMCIId resourceID = VMCIResource_GetID(contextID);
      if (resourceID == VMCI_INVALID_ID) {
         result = VMCI_ERROR_NO_HANDLE;
         goto freeMem;
      }
      newHandle = VMCI_MAKE_HANDLE(contextID, resourceID);
   } else {
      Bool validContext;

      /*
       * Validate the handle.  We must do both of the checks below
       * because we can be acting as both a host and a guest at the
       * same time. We always allow the host context ID, since the
       * host functionality is in practice always there with the
       * unified driver.
       */

      validContext = FALSE;
      if (VMCI_HOST_CONTEXT_ID == handle->context) {
         validContext = TRUE;
      }
      if (VMCI_GuestPersonalityActive() &&
          vmci_get_context_id() == handle->context) {
         validContext = TRUE;
      }

      if (!validContext || VMCI_INVALID_ID == handle->resource) {
         VMCI_DEBUG_LOG(4, (LGPFX"Invalid argument (handle=0x%x:0x%x).\n",
                            handle->context, handle->resource));
         result = VMCI_ERROR_INVALID_ARGS;
         goto freeMem;
      }

      newHandle = *handle;
   }

   entry->idx = 0;
   VMCIList_Init(&entry->idxListItem);
   entry->privFlags = privFlags;
   entry->isDoorbell = TRUE;
   entry->runDelayed = (flags & VMCI_FLAG_DELAYED_CB) ? TRUE : FALSE;
   entry->notifyCB = notifyCB;
   entry->clientData = clientData;
   Atomic_Write(&entry->active, 0);
   VMCI_CreateEvent(&entry->destroyEvent);

   result = VMCIResource_Add(&entry->resource, VMCI_RESOURCE_TYPE_DOORBELL,
                             newHandle, VMCIDoorbellFreeCB, entry);
   if (result != VMCI_SUCCESS) {
      VMCI_WARNING((LGPFX"Failed to add new resource (handle=0x%x:0x%x).\n",
                    newHandle.context, newHandle.resource));
      if (result == VMCI_ERROR_DUPLICATE_ENTRY) {
         result = VMCI_ERROR_ALREADY_EXISTS;
      }
      goto destroy;
   }

   if (VMCI_GuestPersonalityActive()) {
      VMCIDoorbellIndexTableAdd(entry);
      result = VMCIDoorbellLink(newHandle, entry->isDoorbell, entry->idx);
      if (VMCI_SUCCESS != result) {
         goto destroyResource;
     }
      Atomic_Write(&entry->active, 1);
   }

   if (VMCI_HANDLE_INVALID(*handle)) {
      *handle = newHandle;
   }

   return result;

destroyResource:
   VMCIDoorbellIndexTableRemove(entry);
   VMCIResource_Remove(newHandle, VMCI_RESOURCE_TYPE_DOORBELL);
destroy:
   VMCI_DestroyEvent(&entry->destroyEvent);
freeMem:
   VMCI_FreeKernelMem(entry, sizeof *entry);
   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 * vmci_doorbell_destroy --
 *
 *    Destroys a doorbell previously created with
 *    vmci_doorbell_create. This operation may block waiting for a
 *    callback to finish.
 *
 * Results:
 *    VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *    May block.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_doorbell_destroy)
int
vmci_doorbell_destroy(VMCIHandle handle)  // IN
{
   VMCIDoorbellEntry *entry;
   VMCIResource *resource;

   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   resource = VMCIResource_Get(handle, VMCI_RESOURCE_TYPE_DOORBELL);
   if (resource == NULL) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to destroy doorbell (handle=0x%x:0x%x).\n",
                         handle.context, handle.resource));
      return VMCI_ERROR_NOT_FOUND;
   }
   entry = RESOURCE_CONTAINER(resource, VMCIDoorbellEntry, resource);

   if (VMCI_GuestPersonalityActive()) {
      int result;

      VMCIDoorbellIndexTableRemove(entry);

      result = VMCIDoorbellUnlink(handle, entry->isDoorbell);
      if (VMCI_SUCCESS != result) {

         /*
          * The only reason this should fail would be an inconsistency between
          * guest and hypervisor state, where the guest believes it has an
          * active registration whereas the hypervisor doesn't. One case where
          * this may happen is if a doorbell is unregistered following a
          * hibernation at a time where the doorbell state hasn't been restored
          * on the hypervisor side yet. Since the handle has now been removed
          * in the guest, we just print a warning and return success.
          */

         VMCI_DEBUG_LOG(4, (LGPFX"Unlink of %s (handle=0x%x:0x%x) unknown by "
                            "hypervisor (error=%d).\n",
                            entry->isDoorbell ? "doorbell" : "queuepair",
                            handle.context, handle.resource, result));
      }
   }

   /*
    * Now remove the resource from the table.  It might still be in use
    * after this, in a callback or still on the delayed work queue.
    */

   VMCIResource_Remove(handle, VMCI_RESOURCE_TYPE_DOORBELL);

   /*
    * We now wait on the destroyEvent and release the reference we got
    * above.
    */

   VMCI_WaitOnEvent(&entry->destroyEvent, VMCIDoorbellReleaseCB, entry);

   /*
    * We know that we are now the only reference to the above entry so
    * can safely free it.
    */

   VMCI_DestroyEvent(&entry->destroyEvent);
   VMCI_FreeKernelMem(entry, sizeof *entry);

   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellNotifyAsGuest --
 *
 *    Notify another guest or the host.  We send a datagram down to the
 *    host via the hypervisor with the notification info.
 *
 * Results:
 *    VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *    May do a hypercall.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIDoorbellNotifyAsGuest(VMCIHandle handle,            // IN
                          VMCIPrivilegeFlags privFlags) // IN
{
#if defined(VMKERNEL)
   VMCI_WARNING((LGPFX"Cannot send down to host from VMKERNEL.\n"));
   return VMCI_ERROR_DST_UNREACHABLE;
#else // VMKERNEL
   VMCIDoorbellNotifyMsg notifyMsg;

   UNREFERENCED_PARAMETER(privFlags);

   ASSERT(VMCI_GuestPersonalityActive());

   notifyMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                        VMCI_DOORBELL_NOTIFY);
   notifyMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   notifyMsg.hdr.payloadSize = sizeof notifyMsg - VMCI_DG_HEADERSIZE;
   notifyMsg.handle = handle;

   return VMCI_SendDatagram((VMCIDatagram *)&notifyMsg);
#endif // VMKERNEL
}


/*
 *------------------------------------------------------------------------------
 *
 * vmci_doorbell_notify --
 *
 *    Generates a notification on the doorbell identified by the
 *    handle. For host side generation of notifications, the caller
 *    can specify what the privilege of the calling side is.
 *
 * Results:
 *    VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *    May do a hypercall.
 *
 *------------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(vmci_doorbell_notify)
int
vmci_doorbell_notify(VMCIHandle dst,               // IN
                     VMCIPrivilegeFlags privFlags) // IN
{
   int retval;
   VMCIRoute route;
   VMCIHandle src;

   if (VMCI_HANDLE_INVALID(dst) || (privFlags & ~VMCI_PRIVILEGE_ALL_FLAGS)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   src = VMCI_INVALID_HANDLE;
   retval = VMCI_Route(&src, &dst, FALSE, &route);
   if (retval < VMCI_SUCCESS) {
      return retval;
   }

   if (VMCI_ROUTE_AS_HOST == route) {
      return VMCIContext_NotifyDoorbell(VMCI_HOST_CONTEXT_ID, dst, privFlags);
   }

   if (VMCI_ROUTE_AS_GUEST == route) {
      return VMCIDoorbellNotifyAsGuest(dst, privFlags);
   }

   VMCI_WARNING((LGPFX"Unknown route (%d) for doorbell.\n", route));
   return VMCI_ERROR_DST_UNREACHABLE;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellDelayedDispatchCB --
 *
 *    Calls the specified callback in a delayed context.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

static void
VMCIDoorbellDelayedDispatchCB(void *data) // IN
{
   VMCIDoorbellEntry *entry = (VMCIDoorbellEntry *)data;

   ASSERT(data);

   entry->notifyCB(entry->clientData);

   VMCIResource_Release(&entry->resource);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbellHostContextNotify --
 *
 *    Dispatches a doorbell notification to the host context.
 *
 * Results:
 *    VMCI_SUCCESS on success. Appropriate error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIDoorbellHostContextNotify(VMCIId srcCID,     // IN
                              VMCIHandle handle) // IN
{
   VMCIDoorbellEntry *entry;
   VMCIResource *resource;
   int result;

   UNREFERENCED_PARAMETER(srcCID);

   ASSERT(VMCI_HostPersonalityActive());

   if (VMCI_HANDLE_INVALID(handle)) {
      VMCI_DEBUG_LOG(4,
                     (LGPFX"Notifying an invalid doorbell (handle=0x%x:0x%x).\n",
                      handle.context, handle.resource));
      return VMCI_ERROR_INVALID_ARGS;
   }

   resource = VMCIResource_Get(handle, VMCI_RESOURCE_TYPE_DOORBELL);
   if (resource == NULL) {
      VMCI_DEBUG_LOG(4,
                     (LGPFX"Notifying an unknown doorbell (handle=0x%x:0x%x).\n",
                      handle.context, handle.resource));
      return VMCI_ERROR_NOT_FOUND;
   }
   entry = RESOURCE_CONTAINER(resource, VMCIDoorbellEntry, resource);

   if (entry->runDelayed) {
      result = VMCI_ScheduleDelayedWork(VMCIDoorbellDelayedDispatchCB, entry);
      if (result < VMCI_SUCCESS) {
         /*
          * If we failed to schedule the delayed work, we need to
          * release the resource immediately. Otherwise, the resource
          * will be released once the delayed callback has been
          * completed.
          */

         VMCI_DEBUG_LOG(10, (LGPFX"Failed to schedule delayed doorbell "
                             "notification (result=%d).\n", result));
         VMCIResource_Release(resource);
      }
   } else {
      entry->notifyCB(entry->clientData);
      VMCIResource_Release(resource);
      result = VMCI_SUCCESS;
   }
   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbell_Hibernate --
 *
 *      When a guest leaves hibernation, the device driver state is out of sync
 *      with the device state, since the driver state has doorbells registered
 *      that aren't known to the device.  This function takes care of
 *      reregistering any doorbells. In case an error occurs during
 *      reregistration (this is highly unlikely since 1) it succeeded the first
 *      time 2) the device driver is the only source of doorbell registrations),
 *      we simply log the error.  The doorbell can still be destroyed using
 *      VMCIDoorbell_Destroy.
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
VMCIDoorbell_Hibernate(Bool enterHibernate)
{
   uint32 bucket;
   VMCIListItem *iter;
   VMCILockFlags flags;

   if (!VMCI_GuestPersonalityActive() || enterHibernate) {
      return;
   }

   VMCI_GrabLock_BH(&vmciDoorbellIT.lock, &flags);

   for (bucket = 0; bucket < ARRAYSIZE(vmciDoorbellIT.entries); bucket++) {
      VMCIList_Scan(iter, &vmciDoorbellIT.entries[bucket]) {
         int result;
         VMCIHandle h;
         VMCIDoorbellEntry *cur;

         cur = VMCIList_Entry(iter, VMCIDoorbellEntry, idxListItem);
         h = VMCIResource_Handle(&cur->resource);
         result = VMCIDoorbellLink(h, cur->isDoorbell, cur->idx);
         if (result != VMCI_SUCCESS && result != VMCI_ERROR_DUPLICATE_ENTRY) {
            VMCI_WARNING((LGPFX"Failed to reregister doorbell "
                          "(handle=0x%x:0x%x) of resource %s to index "
                          "(error=%d).\n",
                          h.context, h.resource,
                          cur->isDoorbell ? "doorbell" : "queue pair", result));
         }
      }
   }

   VMCI_ReleaseLock_BH(&vmciDoorbellIT.lock, flags);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCIDoorbell_Sync --
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
VMCIDoorbell_Sync(void)
{
   VMCILockFlags flags;
   VMCI_GrabLock_BH(&vmciDoorbellIT.lock, &flags);
   VMCI_ReleaseLock_BH(&vmciDoorbellIT.lock, flags);
   VMCIResource_Sync();
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCI_RegisterNotificationBitmap --
 *
 *      Register the notification bitmap with the host.
 *
 * Results:
 *      TRUE if the bitmap is registered successfully with the device, FALSE
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *------------------------------------------------------------------------------
 */

Bool
VMCI_RegisterNotificationBitmap(PPN bitmapPPN)
{
   int result;
   VMCINotifyBitmapSetMsg bitmapSetMsg;

   /*
    * Do not ASSERT() on the guest device here.  This function can get called
    * during device initialization, so the ASSERT() will fail even though
    * the device is (almost) up.
    */

   bitmapSetMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                           VMCI_SET_NOTIFY_BITMAP);
   bitmapSetMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   bitmapSetMsg.hdr.payloadSize = sizeof bitmapSetMsg - VMCI_DG_HEADERSIZE;
   bitmapSetMsg.bitmapPPN = bitmapPPN;

   result = VMCI_SendDatagram((VMCIDatagram *)&bitmapSetMsg);
   if (result != VMCI_SUCCESS) {
      VMCI_DEBUG_LOG(4, (LGPFX"Failed to register (PPN=%u) as "
                         "notification bitmap (error=%d).\n",
                         bitmapPPN, result));
      return FALSE;
   }
   return TRUE;
}


/*
 *-------------------------------------------------------------------------
 *
 * VMCIDoorbellFireEntries --
 *
 *     Executes or schedules the handlers for a given notify index.
 *
 * Result:
 *     Notification hash entry if found. NULL otherwise.
 *
 * Side effects:
 *     Whatever the side effects of the handlers are.
 *
 *-------------------------------------------------------------------------
 */

static void
VMCIDoorbellFireEntries(uint32 notifyIdx) // IN
{
   uint32 bucket = VMCI_DOORBELL_HASH(notifyIdx);
   VMCIListItem *iter;
   VMCILockFlags flags;

   ASSERT(VMCI_GuestPersonalityActive());

   VMCI_GrabLock_BH(&vmciDoorbellIT.lock, &flags);

   VMCIList_Scan(iter, &vmciDoorbellIT.entries[bucket]) {
      VMCIDoorbellEntry *cur =
         VMCIList_Entry(iter, VMCIDoorbellEntry, idxListItem);

      ASSERT(cur);

      if (cur->idx == notifyIdx && Atomic_Read(&cur->active) == 1) {
         ASSERT(cur->notifyCB);
         if (cur->runDelayed) {
            int err;

            VMCIResource_Hold(&cur->resource);
            err = VMCI_ScheduleDelayedWork(VMCIDoorbellDelayedDispatchCB, cur);
            if (err != VMCI_SUCCESS) {
               VMCIResource_Release(&cur->resource);
               goto out;
            }
         } else {
            cur->notifyCB(cur->clientData);
         }
      }
   }

out:
   VMCI_ReleaseLock_BH(&vmciDoorbellIT.lock, flags);
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCI_ScanNotificationBitmap --
 *
 *      Scans the notification bitmap, collects pending notifications,
 *      resets the bitmap and invokes appropriate callbacks.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      May schedule tasks, allocate memory and run callbacks.
 *
 *------------------------------------------------------------------------------
 */

void
VMCI_ScanNotificationBitmap(uint8 *bitmap)
{
   uint32 idx;

   ASSERT(bitmap);
   ASSERT(VMCI_GuestPersonalityActive());

   for (idx = 0; idx < maxNotifyIdx; idx++) {
      if (bitmap[idx] & 0x1) {
         bitmap[idx] &= ~1;
         VMCIDoorbellFireEntries(idx);
      }
   }
}


#else // __APPLE__

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbell_Create/VMCIDoorbell_Destroy/VMCIDoorbell_Notify/
 * VMCIDoorbellHostContextNotify/VMCIDoorbellGetPrivFlags/
 * VMCIDoorbell_Init/VMCIDoorbell_Exit --
 *
 *      The doorbell functions have yet to be implemented for Solaris
 *      and Mac OS X guest drivers.
 *
 * Results:
 *      VMCI_ERROR_UNAVAILABLE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

VMCI_EXPORT_SYMBOL(VMCIDoorbell_Create)
int
VMCIDoorbell_Create(VMCIHandle *handle,            // IN
                    uint32 flags,                  // IN
                    VMCIPrivilegeFlags privFlags,  // IN
                    VMCICallback notifyCB,         // IN
                    void *clientData)              // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


VMCI_EXPORT_SYMBOL(VMCIDoorbell_Destroy)
int
VMCIDoorbell_Destroy(VMCIHandle handle)  // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


VMCI_EXPORT_SYMBOL(VMCIDoorbell_Notify)
int
VMCIDoorbell_Notify(VMCIHandle handle,             // IN
                    VMCIPrivilegeFlags privFlags)  // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


int
VMCIDoorbellHostContextNotify(VMCIId srcCID,     // IN
                              VMCIHandle handle) // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


int
VMCIDoorbellGetPrivFlags(VMCIHandle handle,             // IN
                         VMCIPrivilegeFlags *privFlags) // OUT
{
   return VMCI_ERROR_UNAVAILABLE;
}


int
VMCIDoorbell_Init(void)
{
   return VMCI_SUCCESS;
}


void
VMCIDoorbell_Exit(void)
{
}

#endif // __APPLE__
