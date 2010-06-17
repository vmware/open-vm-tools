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
 * vmciNotifications.c --
 *
 *      Implementation of the VMCI notifications registration and
 *      delivery, and the related doorbell API for the guest driver.
 */

#ifdef __linux__
#  include "driver-config.h"
#  include "compat_kernel.h"
#  include "compat_module.h"
#endif // linux

#include "vmci_kernel_if.h"
#include "vm_basic_types.h"
#include "vm_assert.h"
#include "vmciKernelAPI.h"
#include "vmci_defs.h"
#include "vmci_call_defs.h"
#include "vmciInt.h"
#include "vmciUtil.h"
#include "circList.h"

#if !defined(SOLARIS) && !defined(__APPLE__)

/*
 * The VMCI Notify hash table provides two mappings:
 * 1) one maps a given notification index in the bitmap to the
 *    entries, giving the set of handlers registered for that
 *    index. This is mainly used for firing handlers for a given
 *    bitmap index.
 * 2) the other maps a handle and a resource (doorbell/queuepair)
 *    to the entry (used to check for duplicates and delete the
 *    entry)
 */

#define HASH_TABLE_SIZE 64
#define VMCI_NOTIF_HASH(val) VMCI_HashId(val, HASH_TABLE_SIZE)

typedef struct VMCINotifyHashEntry {
   uint32         idx; // Bitmap index
   VMCIHandle     handle;
   Bool           doorbell;
   Bool           runDelayed;
   VMCICallback   notifyCB;
   void          *callbackData;
   VMCIEvent      destroyEvent;
   int            refCount;
   ListItem       handleListItem;
   ListItem       idxListItem;
} VMCINotifyHashEntry;

typedef struct VMCINotifyHashTable {
   VMCILock lock;
   ListItem *entriesByIdx[HASH_TABLE_SIZE];
   ListItem *entriesByHandle[HASH_TABLE_SIZE];
} VMCINotifyHashTable;


static int VMCINotifyHashAddEntry(VMCINotifyHashEntry *entry);
static void VMCINotifyHashSetEntryCallback(VMCINotifyHashEntry *entry,
                                           VMCICallback notifyCB);
static VMCINotifyHashEntry *VMCINotifyHashRemoveEntry(VMCIHandle handle,
                                                      Bool doorbell);
static int VMCINotifyReleaseCB(void *clientData);
static void VMCINotifyHashReleaseEntry(VMCINotifyHashEntry *entry);
static VMCINotifyHashEntry *VMCINotifyHashFindByIdx(uint32 idx,
                                                    uint32 *hashBucket);
static VMCINotifyHashEntry *VMCINotifyHashFindByHandle(VMCIHandle handle,
                                                       Bool doorbell,
                                                       uint32 *hashBucket);

static void VMCINotifyDelayedDispatchCB(void *data);
static void VMCINotifyHashFireEntries(uint32 notifyIdx);

static int LinkNotificationHypercall(VMCIHandle handle, Bool doorbell,
                                     uint32 notifyIdx);
static int UnlinkNotificationHypercall(VMCIHandle handle, Bool doorbell);


/*
 * The VMCI notify hash table keeps track of currently registered
 * notifications.
 */

static VMCINotifyHashTable vmciNotifyHT;

/*
 * The maxNotifyIdx is one larger than the currently known bitmap
 * index in use, and is used to determine how much of the bitmap needs
 * to be scanned.
 */

static uint32 maxNotifyIdx;

/*
 * the notifyIdxCount is used for determining whether there are free
 * entries within the bitmap (if notifyIdxCount + 1 < maxNotifyIdx).
 */

static uint32 notifyIdxCount;

/*
 * The lastNotifyIdxReserved is used to track the last index handed
 * out - in the case where multiple handles share a notification
 * index, we hand out indexes round robin based on
 * lastNotifyIdxReserved.
 */

static uint32 lastNotifyIdxReserved;

/*
 * lastNotifyIdxReleased is a one entry cache used to by the index
 * allocation.
 */

static uint32 lastNotifyIdxReleased = PAGE_SIZE;


/*
 *----------------------------------------------------------------------
 *
 * VMCINotifications_Init --
 *
 *      General init code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
VMCINotifications_Init(void)
{
   memset(vmciNotifyHT.entriesByIdx, 0, ARRAY_SIZE(vmciNotifyHT.entriesByIdx));
   memset(vmciNotifyHT.entriesByHandle, 0, ARRAY_SIZE(vmciNotifyHT.entriesByHandle));
   VMCI_InitLock(&vmciNotifyHT.lock, "VMCINotifyHashLock",
                 VMCI_LOCK_RANK_HIGHER_BH);
   return;
}


/*
 *----------------------------------------------------------------------
 *
 * VMCINotifications_Exit --
 *
 *    General exit code.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

void
VMCINotifications_Exit(void)
{
   uint32 bucket;
   ListItem *iter, *iter2;

   for (bucket = 0; bucket < HASH_TABLE_SIZE; bucket++) {
      LIST_SCAN_SAFE(iter, iter2, vmciNotifyHT.entriesByIdx[bucket]) {
         VMCINotifyHashEntry *cur;

         /*
          * We should never get here because all notifications should have been
          * unregistered before we try to unload the driver module.
          * Also, delayed callbacks could still be firing so this cleanup
          * would not be safe.
          * Still it is better to free the memory than not ... so we
          * leave this code in just in case....
          *
          */
         ASSERT(FALSE);

         cur = LIST_CONTAINER(iter, VMCINotifyHashEntry, idxListItem);
         VMCI_FreeKernelMem(cur, sizeof *cur);
      }
   }
   VMCI_CleanupLock(&vmciNotifyHT.lock);

}


/*
 *-------------------------------------------------------------------------
 *
 * VMCINotifyHashAddEntry --
 *
 *     Given a notification entry, adds it to the hashtable of
 *     notifications.
 *
 * Result:
 *     VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *     None.
 *
 *-------------------------------------------------------------------------
 */

static int
VMCINotifyHashAddEntry(VMCINotifyHashEntry *entry) // IN
{
   VMCILockFlags flags;
   uint32 bucket;
   uint32 newNotifyIdx;
   int result;
   static VMCIId notifyRID = 0;

   ASSERT(entry);

   VMCI_GrabLock_BH(&vmciNotifyHT.lock, &flags);

   if (VMCI_HANDLE_INVALID(entry->handle)) {
      VMCIHandle newHandle;
      VMCIId oldRID = notifyRID;
      Bool found;

      do {
         newHandle = VMCI_MAKE_HANDLE(VMCI_GetContextID(), notifyRID);
         notifyRID++;
         found = VMCINotifyHashFindByHandle(newHandle, entry->doorbell, NULL) != NULL;
      } while(found && oldRID != notifyRID);
      if (UNLIKELY(found)) {
         /*
          * We went full circle and still didn't find a free handle.
          */

         result = VMCI_ERROR_NO_HANDLE;
         goto out;
      }
      entry->handle = newHandle;
   }

   if (VMCINotifyHashFindByHandle(entry->handle, entry->doorbell, &bucket)) {
      result = VMCI_ERROR_ALREADY_EXISTS;
      goto out;
   }
   LIST_QUEUE(&entry->handleListItem, &vmciNotifyHT.entriesByHandle[bucket]);

   /*
    * Below we try to allocate an index in the notification bitmap
    * with "not too much" sharing between resources. If we use less
    * that the full bitmap, we either add to the end if there are no
    * unused flags withing the currently used area, or we search for
    * unused ones. If we use the full bitmap, we allocate the index
    * round robin.
    */

   if (maxNotifyIdx < PAGE_SIZE || notifyIdxCount < PAGE_SIZE) {
      if (lastNotifyIdxReleased < maxNotifyIdx &&
          !VMCINotifyHashFindByIdx(lastNotifyIdxReleased, NULL)) {
         newNotifyIdx = lastNotifyIdxReleased;
         lastNotifyIdxReleased = PAGE_SIZE;
      } else {
         Bool reused = FALSE;
         newNotifyIdx = lastNotifyIdxReserved;
         if (notifyIdxCount + 1 < maxNotifyIdx) {
            do {
               if (!VMCINotifyHashFindByIdx(newNotifyIdx, NULL)) {
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

   bucket = VMCI_NOTIF_HASH(newNotifyIdx);
   entry->refCount++;
   entry->idx = newNotifyIdx;
   LIST_QUEUE(&entry->idxListItem, &vmciNotifyHT.entriesByIdx[bucket]);
   result = VMCI_SUCCESS;
out:
   VMCI_ReleaseLock_BH(&vmciNotifyHT.lock, flags);
   return result;
}


/*
 *-------------------------------------------------------------------------
 *
 * VMCINotifyHashSetEntryCallback --
 *
 *     Sets the notify callback of the given entry. Once the callback
 *     has been set, it may start firing.
 *
 * Result:
 *     None.
 *
 * Side effects:
 *     None.
 *
 *-------------------------------------------------------------------------
 */

static void
VMCINotifyHashSetEntryCallback(VMCINotifyHashEntry *entry, // IN
                               VMCICallback notifyCB)      // IN
{
   VMCILockFlags flags;

   ASSERT(entry && notifyCB);

   VMCI_GrabLock_BH(&vmciNotifyHT.lock, &flags);
   entry->notifyCB = notifyCB;
   VMCI_ReleaseLock_BH(&vmciNotifyHT.lock, flags);
}


/*
 *-------------------------------------------------------------------------
 *
 * VMCINotifyHashRemoveEntry --
 *
 *     Removes the entry identified by the handle of the given
 *     resource type from the hash table.
 *
 * Result:
 *     Pointer to entry if removed, NULL if not found.
 *
 * Side effects:
 *     None.
 *
 *-------------------------------------------------------------------------
 */

static VMCINotifyHashEntry *
VMCINotifyHashRemoveEntry(VMCIHandle handle, // IN
                          Bool doorbell)     // IN
{
   VMCILockFlags flags;
   VMCINotifyHashEntry *entry;
   uint32 bucket;

   VMCI_GrabLock_BH(&vmciNotifyHT.lock, &flags);
   entry = VMCINotifyHashFindByHandle(handle, doorbell, &bucket);
   if (entry) {
      ASSERT(entry->refCount > 0);
      LIST_DEL(&entry->handleListItem, &vmciNotifyHT.entriesByHandle[bucket]);
      bucket = VMCI_NOTIF_HASH(entry->idx);
      LIST_DEL(&entry->idxListItem, &vmciNotifyHT.entriesByIdx[bucket]);
      notifyIdxCount--;
      if (entry->idx == maxNotifyIdx - 1) {
         /*
          * If we delete an entry with the maximum known notification
          * index, we take the opportunity to prune the current
          * max. As there might be other unused indices immediately
          * below, we lower the maximum until we hit an index in use.
          */

         while (maxNotifyIdx > 0 &&
                !VMCINotifyHashFindByIdx(maxNotifyIdx - 1, NULL)) {
            maxNotifyIdx--;
         }
      }
      lastNotifyIdxReleased = entry->idx;
   }
   VMCI_ReleaseLock_BH(&vmciNotifyHT.lock, flags);

   return entry;
}


/*
 *------------------------------------------------------------------------------
 *
 * VMCINotifyReleaseCB --
 *
 *     Callback to release the notification entry reference. It is
 *     called by the VMCI_WaitOnEvent function before it blocks.
 *
 * Result:
 *     0.
 *
 * Side effects:
 *     Releases hash entry (see below).
 *
 *------------------------------------------------------------------------------
 */

static int
VMCINotifyReleaseCB(void *clientData) // IN
{
   VMCINotifyHashEntry *entry = (VMCINotifyHashEntry *)clientData;
   ASSERT(entry);
   VMCINotifyHashReleaseEntry(entry);

   return 0;
}


/*
 *-------------------------------------------------------------------------
 *
 * VMCINotifyHashReleaseEntry --
 *
 *     Drops a reference to the current hash entry. If this is the last
 *     reference then the entry is freed.
 *
 * Result:
 *     None.
 *
 * Side effects:
 *     May signal event.
 *
 *-------------------------------------------------------------------------
 */

static void
VMCINotifyHashReleaseEntry(VMCINotifyHashEntry *entry) // IN
{
   VMCILockFlags flags;

   VMCI_GrabLock_BH(&vmciNotifyHT.lock, &flags);
   entry->refCount--;

   /*
    * Check if this is last reference and signal the destroy event if
    * so.
    */

   if (entry->refCount == 0) {
      VMCI_SignalEvent(&entry->destroyEvent);
   }
   VMCI_ReleaseLock_BH(&vmciNotifyHT.lock, flags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCINotifyHashFindByIdx --
 *
 *    Find hash entry by bitmap index. Assumes lock is
 *    held. Regardless of whether an entry was found, the bucket that
 *    the entry would have been in is returned in hashBucket (if
 *    valid).
 *
 * Results:
 *    Entry if found, NULL if not.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VMCINotifyHashEntry *
VMCINotifyHashFindByIdx(uint32 idx,         // IN
                        uint32 *hashBucket) // IN/OUT: hash value for idx
{
   ListItem *iter;
   uint32 bucket = VMCI_NOTIF_HASH(idx);

   if (hashBucket) {
      *hashBucket = bucket;
   }

   LIST_SCAN(iter, vmciNotifyHT.entriesByIdx[bucket]) {
      VMCINotifyHashEntry *cur =
         LIST_CONTAINER(iter, VMCINotifyHashEntry, idxListItem);
      if (cur->idx == idx) {
         return cur;
      }
   }
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCINotifyHashFindByHandle --
 *
 *    Find hash entry by handle and resoruce. Assumes lock is
 *    held. Regardless of whether an entry was found, the bucket that
 *    the entry would have been in is returned in hashBucket (if
 *    valid).
 *
 * Results:
 *    Entry if found, NULL if not.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static VMCINotifyHashEntry *
VMCINotifyHashFindByHandle(VMCIHandle handle,  // IN
                           Bool doorbell,      // IN
                           uint32 *hashBucket) // IN/OUT: hash value for handle
{
   ListItem *iter;
   uint32 bucket = VMCI_NOTIF_HASH(handle.resource);

   if (hashBucket) {
      *hashBucket = bucket;
   }

   LIST_SCAN(iter, vmciNotifyHT.entriesByHandle[bucket]) {
      VMCINotifyHashEntry *cur =
         LIST_CONTAINER(iter, VMCINotifyHashEntry, handleListItem);
      if (VMCI_HANDLE_EQUAL(cur->handle, handle) && cur->doorbell == doorbell) {
         return cur;
      }
   }
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCINotifyDelayedDispatchCB --
 *
 *      Calls the specified callback in a delayed context.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
VMCINotifyDelayedDispatchCB(void *data) // IN
{
   VMCINotifyHashEntry *entry = (VMCINotifyHashEntry *)data;

   ASSERT(data);

   entry->notifyCB(entry->callbackData);

   VMCINotifyHashReleaseEntry(entry);
}


/*
 *-------------------------------------------------------------------------
 *
 * VMCINotifyHashFireEntries --
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
VMCINotifyHashFireEntries(uint32 notifyIdx) // IN
{
   VMCILockFlags flags;
   ListItem *iter;
   int bucket = VMCI_NOTIF_HASH(notifyIdx);

   VMCI_GrabLock_BH(&vmciNotifyHT.lock, &flags);

   LIST_SCAN(iter, vmciNotifyHT.entriesByIdx[bucket]) {
      VMCINotifyHashEntry *cur = LIST_CONTAINER(iter, VMCINotifyHashEntry,
                                                idxListItem);
      if (cur->idx == notifyIdx && cur->notifyCB) {
         if (cur->runDelayed) {
            int err;

            cur->refCount++;
            err = VMCI_ScheduleDelayedWork(VMCINotifyDelayedDispatchCB, cur);
            if (err != VMCI_SUCCESS) {
               cur->refCount--;
               goto out;
            }
         } else {
            cur->notifyCB(cur->callbackData);
         }
      }
   }
out:
   VMCI_ReleaseLock_BH(&vmciNotifyHT.lock, flags);
}


/*
 *----------------------------------------------------------------------
 *
 * LinkNotificationHypercall --
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
 *----------------------------------------------------------------------
 */

static int
LinkNotificationHypercall(VMCIHandle handle,  // IN
                          Bool doorbell,      // IN
                          uint32 notifyIdx)   // IN
{
   VMCIDoorbellLinkMsg linkMsg;
   int result;
   VMCIId resourceID;

   ASSERT(!VMCI_HANDLE_INVALID(handle));

   if (doorbell) {
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

   result = VMCI_SendDatagram((VMCIDatagram *)&linkMsg);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * UnlinkNotificationHypercall --
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
 *----------------------------------------------------------------------
 */

static int
UnlinkNotificationHypercall(VMCIHandle handle, // IN
                            Bool doorbell)     // IN
{
   VMCIDoorbellUnlinkMsg unlinkMsg;
   int result;
   VMCIId resourceID;

   ASSERT(!VMCI_HANDLE_INVALID(handle));

   if (doorbell) {
      resourceID = VMCI_DOORBELL_UNLINK;
   } else {
      ASSERT(FALSE);
      return VMCI_ERROR_UNAVAILABLE;
   }

   unlinkMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, resourceID);
   unlinkMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   unlinkMsg.hdr.payloadSize = sizeof unlinkMsg - VMCI_DG_HEADERSIZE;
   unlinkMsg.handle = handle;

   result = VMCI_SendDatagram((VMCIDatagram *)&unlinkMsg);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCINotificationRegister --
 *
 *      Links a resource with an index in the notification bitmap.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      Notification state is created both in guest and on host.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCINotificationRegister(VMCIHandle *handle,     // IN
                         Bool doorbell,          // IN
                         uint32 flags,           // IN
                         VMCICallback notifyCB,  // IN
                         void *callbackData)     // IN
{
   int result;
   VMCINotifyHashEntry *entry;

   if (!notifyCB || !handle) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   entry = VMCI_AllocKernelMem(sizeof *entry, VMCI_MEMORY_NONPAGED);
   if (entry == NULL) {
      return VMCI_ERROR_NO_MEM;
   }

   entry->runDelayed = (flags & VMCI_FLAG_DELAYED_CB) ? TRUE : FALSE;
   if (entry->runDelayed && !VMCI_CanScheduleDelayedWork()) {
      VMCI_FreeKernelMem(entry, sizeof *entry);
      return VMCI_ERROR_INVALID_ARGS;
   }

   /*
    * Reserve an index in the notification bitmap.
    */

   entry->handle = *handle;
   entry->doorbell = doorbell;
   entry->notifyCB = NULL; // Wait with this until link is established in hypervisor
   entry->callbackData = callbackData;
   entry->refCount = 0;
   result = VMCINotifyHashAddEntry(entry);
   if (result != VMCI_SUCCESS) {
      VMCI_FreeKernelMem(entry, sizeof *entry);
      return result;
   }

   VMCI_CreateEvent(&entry->destroyEvent);

   result = LinkNotificationHypercall(entry->handle, doorbell, entry->idx);
   if (result != VMCI_SUCCESS) {
      VMCI_LOG(("Failed to link handle 0x%x:0x%x of resource %s to index, "
                "err 0x%x.\n", entry->handle.context, entry->handle.resource,
                entry->doorbell ? "doorbell" : "queue pair", result));
      VMCINotifyHashRemoveEntry(entry->handle, entry->doorbell);
      VMCI_DestroyEvent(&entry->destroyEvent);
      VMCI_FreeKernelMem(entry, sizeof *entry);
   } else {
      /*
       * When the handle is set, the notification callback may start
       * to fire. Since flags in the notification bitmap can be
       * shared, a given callback may fire immediately.
       */

      VMCINotifyHashSetEntryCallback(entry, notifyCB);
      *handle = entry->handle;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCINotificationUnregister --
 *
 *      Unregisters a notification previously created through
 *      VMCINotificationRegister. This function may block. The call
 *      always succeeds if the notification exists.
 *
 * Results:
 *      VMCI_SUCCESS if success, VMCI_ERROR_NOT_FOUND otherwise.
 *
 * Side effects:
 *      Notification state is destroyed both in guest and hypervisor.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCINotificationUnregister(VMCIHandle handle, // IN
                           Bool doorbell)     // IN
{
   VMCINotifyHashEntry *entry;
   int result;

   entry = VMCINotifyHashRemoveEntry(handle, doorbell);
   if (!entry) {
      ASSERT(FALSE);
      return VMCI_ERROR_NOT_FOUND;
   }

   VMCI_WaitOnEvent(&entry->destroyEvent, VMCINotifyReleaseCB, entry);
   VMCI_DestroyEvent(&entry->destroyEvent);
   VMCI_FreeKernelMem(entry, sizeof *entry);

   result = UnlinkNotificationHypercall(handle, doorbell);
   if (result != VMCI_SUCCESS) {
      /*
       * The only reason this should fail would be an inconsistency
       * between guest and hypervisor state, where the guest believes
       * it has an active registration whereas the hypervisor
       * doesn't. Since the handle has now been removed in the guest,
       * we just print a warning and return success.
       */

      ASSERT(FALSE);

      VMCI_LOG(("Unlink of %s  handle 0x%x:0x%x unknown by hypervisor.\n",
                doorbell ? "doorbell" : "queuepair",
                handle.context, handle.resource));
   }
   return VMCI_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCI_RegisterNotificationBitmap --
 *
 *      Verify that the host supports the hypercalls we need. If it does not,
 *      try to find fallback hypercalls and use those instead.
 *
 * Results:
 *      TRUE if the bitmap is registered successfully with the device, FALSE
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
VMCI_RegisterNotificationBitmap(PPN bitmapPPN) // IN
{
   VMCINotifyBitmapSetMsg bitmapSetMsg;
   int result;

   bitmapSetMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID,
                                           VMCI_SET_NOTIFY_BITMAP);
   bitmapSetMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   bitmapSetMsg.hdr.payloadSize = sizeof bitmapSetMsg - VMCI_DG_HEADERSIZE;
   bitmapSetMsg.bitmapPPN = bitmapPPN;

   result = VMCI_SendDatagram((VMCIDatagram *)&bitmapSetMsg);
   if (result != VMCI_SUCCESS) {
      VMCI_LOG(("VMCINotifications: Failed to register PPN %u as notification "
                "bitmap (error : %d).\n", bitmapPPN, result));
      return FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
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
 *-----------------------------------------------------------------------------
 */

void
VMCI_ScanNotificationBitmap(uint8 *bitmap) // IN
{
   size_t idx;

   for (idx = 0; idx < maxNotifyIdx; idx++) {
      if (bitmap[idx] & 0x1) {
         bitmap[idx] &= ~1;
         VMCINotifyHashFireEntries(idx);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbell_Create --
 *
 *      Creates a doorbell with the given callback. If the handle is
 *      VMCI_INVALID_HANDLE, a free handle will be assigned, if
 *      possible. The callback can be run in interrupt context (the
 *      default) or delayed (in a kernel thread) by specifying the
 *      flag VMCI_FLAG_DELAYED_CB. If delayed execution is selected, a
 *      given callback may not be run if the kernel is unable to
 *      allocate memory for the delayed execution (highly unlikely).
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIDoorbell_Create(VMCIHandle *handle,            // IN
                    uint32 flags,                  // IN
                    VMCIPrivilegeFlags privFlags,  // IN: Unused in guest
                    VMCICallback notifyCB,         // IN
                    void *clientData)              // IN
{
   if (!handle || !notifyCB || flags & ~VMCI_FLAG_DELAYED_CB) {
      return VMCI_ERROR_INVALID_ARGS;
   }
   if (privFlags & ~VMCI_LEAST_PRIVILEGE_FLAGS) {
      return VMCI_ERROR_NO_ACCESS;
   }

   return VMCINotificationRegister(handle, TRUE, flags,
                                   notifyCB, clientData);
}

#if defined(__linux__)
EXPORT_SYMBOL(VMCIDoorbell_Create);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbell_Destroy --
 *
 *      Destroys a doorbell previously created with
 *      VMCIDoorbell_Create. This operation may block waiting for a
 *      callback to finish.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      May block.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIDoorbell_Destroy(VMCIHandle handle) // IN
{
   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }
   return VMCINotificationUnregister(handle, TRUE);
}

#if defined(__linux__)
EXPORT_SYMBOL(VMCIDoorbell_Destroy);
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbell_Notify --
 *
 *      Generates a notification on the doorbell identified by the
 *      handle.
 *
 * Results:
 *      VMCI_SUCCESS on success, appropriate error code otherwise.
 *
 * Side effects:
 *      May do a hypercall.
 *
 *-----------------------------------------------------------------------------
 */

int
VMCIDoorbell_Notify(VMCIHandle handle,             // IN
                    VMCIPrivilegeFlags privFlags)  // IN: Unused in guest
{
   VMCIDoorbellNotifyMsg notifyMsg;
   int result;

   if (VMCI_HANDLE_INVALID(handle)) {
      return VMCI_ERROR_INVALID_ARGS;
   }

   if (privFlags & ~VMCI_LEAST_PRIVILEGE_FLAGS) {
      return VMCI_ERROR_NO_ACCESS;
   }

   notifyMsg.hdr.dst = VMCI_MAKE_HANDLE(VMCI_HYPERVISOR_CONTEXT_ID, VMCI_DOORBELL_NOTIFY);
   notifyMsg.hdr.src = VMCI_ANON_SRC_HANDLE;
   notifyMsg.hdr.payloadSize = sizeof notifyMsg - VMCI_DG_HEADERSIZE;
   notifyMsg.handle = handle;

   result = VMCI_SendDatagram((VMCIDatagram *)&notifyMsg);

   return result;
}

#if defined(__linux__)
EXPORT_SYMBOL(VMCIDoorbell_Notify);
#endif

#else // defined(SOLARIS) || defined(__APPLE__)

/*
 *-----------------------------------------------------------------------------
 *
 * VMCIDoorbell_Create/VMCIDoorbell_Destroy/VMCIDoorbell_Notify --
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

int
VMCIDoorbell_Create(VMCIHandle *handle,            // IN
                    uint32 flags,                  // IN
                    VMCIPrivilegeFlags privFlags,  // IN
                    VMCICallback notifyCB,         // IN
                    void *clientData)              // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


int
VMCIDoorbell_Destroy(VMCIHandle handle)  // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}


int
VMCIDoorbell_Notify(VMCIHandle handle,             // IN
                    VMCIPrivilegeFlags privFlags)  // IN
{
   return VMCI_ERROR_UNAVAILABLE;
}

#endif
