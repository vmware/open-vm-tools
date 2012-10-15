/*********************************************************
 * Copyright (C) 2006-2012 VMware, Inc. All rights reserved.
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
 * vmciHashtable.c --
 *
 *     Implementation of the VMCI Hashtable.
 *     TODO: Look into what is takes to use lib/misc/hashTable.c instead of
 *     our own implementation.
 */

#include "vmci_kernel_if.h"
#include "vm_assert.h"
#include "vmci_defs.h"
#include "vmci_infrastructure.h"
#include "vmciCommonInt.h"
#include "vmciDriver.h"
#include "vmciHashtable.h"
#if defined(VMKERNEL)
#  include "vmciVmkInt.h"
#  include "vm_libc.h"
#  include "helper_ext.h"
#endif

#define LGPFX "VMCIHashTable: "

#define VMCI_HASHTABLE_HASH(_h, _sz) \
   VMCI_HashId(VMCI_HANDLE_TO_RESOURCE_ID(_h), (_sz))

static int HashTableUnlinkEntry(VMCIHashTable *table, VMCIHashEntry *entry);
static Bool VMCIHashTableEntryExistsLocked(VMCIHashTable *table,
                                           VMCIHandle handle);


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_Create --
 *     XXX Factor out the hashtable code to be shared amongst host and guest.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

VMCIHashTable *
VMCIHashTable_Create(int size)
{
   VMCIHashTable *table = VMCI_AllocKernelMem(sizeof *table,
                                              VMCI_MEMORY_NONPAGED);
   if (table == NULL) {
      return NULL;
   }

   table->entries = VMCI_AllocKernelMem(sizeof *table->entries * size,
                                        VMCI_MEMORY_NONPAGED);
   if (table->entries == NULL) {
      VMCI_FreeKernelMem(table, sizeof *table);
      return NULL;
   }
   memset(table->entries, 0, sizeof *table->entries * size);
   table->size = size;
   if (VMCI_InitLock(&table->lock, "VMCIHashTableLock",
                     VMCI_LOCK_RANK_HASHTABLE) < VMCI_SUCCESS) {
      VMCI_FreeKernelMem(table->entries, sizeof *table->entries * size);
      VMCI_FreeKernelMem(table, sizeof *table);
      return NULL;
   }

   return table;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_Destroy --
 *     This function should be called at module exit time.
 *     We rely on the module ref count to insure that no one is accessing any
 *     hash table entries at this point in time. Hence we should be able to just
 *     remove all entries from the hash table.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIHashTable_Destroy(VMCIHashTable *table)
{
   VMCILockFlags flags;
#if 0
   DEBUG_ONLY(int i;)
   DEBUG_ONLY(int leakingEntries = 0;)
#endif

   ASSERT(table);

   VMCI_GrabLock_BH(&table->lock, &flags);
#if 0
#ifdef VMX86_DEBUG
   for (i = 0; i < table->size; i++) {
      VMCIHashEntry *head = table->entries[i];
      while (head) {
         leakingEntries++;
         head = head->next;
      }
   }
   if (leakingEntries) {
      VMCI_WARNING((LGPFX"Leaking entries (%d) for hash table (%p).\n",
                    leakingEntries, table));
   }
#endif // VMX86_DEBUG
#endif
   VMCI_FreeKernelMem(table->entries, sizeof *table->entries * table->size);
   table->entries = NULL;
   VMCI_ReleaseLock_BH(&table->lock, flags);
   VMCI_CleanupLock(&table->lock);
   VMCI_FreeKernelMem(table, sizeof *table);
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_InitEntry --
 *     Initializes a hash entry;
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */
void
VMCIHashTable_InitEntry(VMCIHashEntry *entry,  // IN
                        VMCIHandle handle)     // IN
{
   ASSERT(entry);
   entry->handle = handle;
   entry->refCount = 0;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_AddEntry --
 *     XXX Factor out the hashtable code to be shared amongst host and guest.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIHashTable_AddEntry(VMCIHashTable *table,   // IN
                       VMCIHashEntry *entry)   // IN
{
   int idx;
   VMCILockFlags flags;

   ASSERT(entry);
   ASSERT(table);

   VMCI_GrabLock_BH(&table->lock, &flags);

   /* Check if creation of a new hashtable entry is allowed. */
   if (!VMCI_CanCreate()) {
      VMCI_ReleaseLock_BH(&table->lock, flags);
      return VMCI_ERROR_UNAVAILABLE;
   }

   if (VMCIHashTableEntryExistsLocked(table, entry->handle)) {
      VMCI_DEBUG_LOG(4, (LGPFX"Entry (handle=0x%x:0x%x) already exists.\n",
                         entry->handle.context, entry->handle.resource));
      VMCI_ReleaseLock_BH(&table->lock, flags);
      return VMCI_ERROR_DUPLICATE_ENTRY;
   }

   idx = VMCI_HASHTABLE_HASH(entry->handle, table->size);
   ASSERT(idx < table->size);

   /* New entry is added to top/front of hash bucket. */
   entry->refCount++;
   entry->next = table->entries[idx];
   table->entries[idx] = entry;
   VMCI_ReleaseLock_BH(&table->lock, flags);

   return VMCI_SUCCESS;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_RemoveEntry --
 *     XXX Factor out the hashtable code to shared amongst API and perhaps
 *     host and guest.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIHashTable_RemoveEntry(VMCIHashTable *table, // IN
                          VMCIHashEntry *entry) // IN
{
   int result;
   VMCILockFlags flags;

   ASSERT(table);
   ASSERT(entry);

   VMCI_GrabLock_BH(&table->lock, &flags);

   /* First unlink the entry. */
   result = HashTableUnlinkEntry(table, entry);
   if (result != VMCI_SUCCESS) {
      /* We failed to find the entry. */
      goto done;
   }

   /* Decrement refcount and check if this is last reference. */
   entry->refCount--;
   if (entry->refCount == 0) {
      result = VMCI_SUCCESS_ENTRY_DEAD;
      goto done;
   }

  done:
   VMCI_ReleaseLock_BH(&table->lock, flags);

   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTableGetEntryLocked --
 *
 *       Looks up an entry in the hash table, that is already locked.
 *
 *  Result:
 *       If the element is found, a pointer to the element is returned.
 *       Otherwise NULL is returned.
 *
 *  Side effects:
 *       The reference count of the returned element is increased.
 *
 *------------------------------------------------------------------------------
 */

static VMCIHashEntry *
VMCIHashTableGetEntryLocked(VMCIHashTable *table,  // IN
                            VMCIHandle handle)     // IN
{
   VMCIHashEntry *cur = NULL;
   int idx;

   ASSERT(!VMCI_HANDLE_EQUAL(handle, VMCI_INVALID_HANDLE));
   ASSERT(table);

   idx = VMCI_HASHTABLE_HASH(handle, table->size);

   cur = table->entries[idx];
   while (TRUE) {
      if (cur == NULL) {
         break;
      }

      if (VMCI_HANDLE_TO_RESOURCE_ID(cur->handle) ==
          VMCI_HANDLE_TO_RESOURCE_ID(handle)) {
         if ((VMCI_HANDLE_TO_CONTEXT_ID(cur->handle) ==
              VMCI_HANDLE_TO_CONTEXT_ID(handle)) ||
             (VMCI_INVALID_ID == VMCI_HANDLE_TO_CONTEXT_ID(cur->handle))) {
            cur->refCount++;
            break;
         }
      }
      cur = cur->next;
   }

   return cur;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_GetEntry --
 *     XXX Factor out the hashtable code to shared amongst API and perhaps
 *     host and guest.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

VMCIHashEntry *
VMCIHashTable_GetEntry(VMCIHashTable *table,  // IN
                       VMCIHandle handle)     // IN
{
   VMCIHashEntry *entry;
   VMCILockFlags flags;

   if (VMCI_HANDLE_EQUAL(handle, VMCI_INVALID_HANDLE)) {
     return NULL;
   }

   ASSERT(table);

   VMCI_GrabLock_BH(&table->lock, &flags);
   entry = VMCIHashTableGetEntryLocked(table, handle);
   VMCI_ReleaseLock_BH(&table->lock, flags);

   return entry;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_HoldEntry --
 *
 *     Hold the given entry.  This will increment the entry's reference count.
 *     This is like a GetEntry() but without having to lookup the entry by
 *     handle.
 *
 *  Result:
 *     None.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

void
VMCIHashTable_HoldEntry(VMCIHashTable *table, // IN
                        VMCIHashEntry *entry) // IN/OUT
{
   VMCILockFlags flags;

   ASSERT(table);
   ASSERT(entry);

   VMCI_GrabLock_BH(&table->lock, &flags);
   entry->refCount++;
   VMCI_ReleaseLock_BH(&table->lock, flags);
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTableReleaseEntryLocked --
 *
 *       Releases an element previously obtained with
 *       VMCIHashTableGetEntryLocked.
 *
 *  Result:
 *       If the entry is removed from the hash table, VMCI_SUCCESS_ENTRY_DEAD
 *       is returned. Otherwise, VMCI_SUCCESS is returned.
 *
 *  Side effects:
 *       The reference count of the entry is decreased and the entry is removed
 *       from the hash table on 0.
 *
 *------------------------------------------------------------------------------
 */

static int
VMCIHashTableReleaseEntryLocked(VMCIHashTable *table,  // IN
                                VMCIHashEntry *entry)  // IN
{
   int result = VMCI_SUCCESS;

   ASSERT(table);
   ASSERT(entry);

   entry->refCount--;
   /* Check if this is last reference and report if so. */
   if (entry->refCount == 0) {

      /*
       * Remove entry from hash table if not already removed. This could have
       * happened already because VMCIHashTable_RemoveEntry was called to unlink
       * it. We ignore if it is not found. Datagram handles will often have
       * RemoveEntry called, whereas SharedMemory regions rely on ReleaseEntry
       * to unlink the entry, since the creator does not call RemoveEntry when
       * it detaches.
       */

      HashTableUnlinkEntry(table, entry);
      result = VMCI_SUCCESS_ENTRY_DEAD;
   }

   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_ReleaseEntry --
 *     XXX Factor out the hashtable code to shared amongst API and perhaps
 *     host and guest.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

int
VMCIHashTable_ReleaseEntry(VMCIHashTable *table,  // IN
                           VMCIHashEntry *entry)  // IN
{
   VMCILockFlags flags;
   int result;

   ASSERT(table);
   VMCI_GrabLock_BH(&table->lock, &flags);
   result = VMCIHashTableReleaseEntryLocked(table, entry);
   VMCI_ReleaseLock_BH(&table->lock, flags);

   return result;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTable_EntryExists --
 *     XXX Factor out the hashtable code to shared amongst API and perhaps
 *     host and guest.
 *
 *  Result:
 *     TRUE if handle already in hashtable. FALSE otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

Bool
VMCIHashTable_EntryExists(VMCIHashTable *table,  // IN
                          VMCIHandle handle)     // IN
{
   Bool exists;
   VMCILockFlags flags;

   ASSERT(table);

   VMCI_GrabLock_BH(&table->lock, &flags);
   exists = VMCIHashTableEntryExistsLocked(table, handle);
   VMCI_ReleaseLock_BH(&table->lock, flags);

   return exists;
}


/*
 *------------------------------------------------------------------------------
 *
 *  VMCIHashTableEntryExistsLocked --
 *
 *     Unlocked version of VMCIHashTable_EntryExists.
 *
 *  Result:
 *     TRUE if handle already in hashtable. FALSE otherwise.
 *
 *  Side effects:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static Bool
VMCIHashTableEntryExistsLocked(VMCIHashTable *table,   // IN
                               VMCIHandle handle)      // IN

{
   VMCIHashEntry *entry;
   int idx;

   ASSERT(table);

   idx = VMCI_HASHTABLE_HASH(handle, table->size);

   entry = table->entries[idx];
   while (entry) {
      if (VMCI_HANDLE_TO_RESOURCE_ID(entry->handle) ==
          VMCI_HANDLE_TO_RESOURCE_ID(handle)) {
         if ((VMCI_HANDLE_TO_CONTEXT_ID(entry->handle) ==
              VMCI_HANDLE_TO_CONTEXT_ID(handle)) ||
             (VMCI_INVALID_ID == VMCI_HANDLE_TO_CONTEXT_ID(handle)) ||
             (VMCI_INVALID_ID == VMCI_HANDLE_TO_CONTEXT_ID(entry->handle))) {
            return TRUE;
         }
      }
      entry = entry->next;
   }

   return FALSE;
}


/*
 *------------------------------------------------------------------------------
 *
 *  HashTableUnlinkEntry --
 *     XXX Factor out the hashtable code to shared amongst API and perhaps
 *     host and guest.
 *     Assumes caller holds table lock.
 *
 *  Result:
 *     None.
 *
 *------------------------------------------------------------------------------
 */

static int
HashTableUnlinkEntry(VMCIHashTable *table, // IN
                     VMCIHashEntry *entry) // IN
{
   int result;
   VMCIHashEntry *prev, *cur;
   int idx;

   idx = VMCI_HASHTABLE_HASH(entry->handle, table->size);

   prev = NULL;
   cur = table->entries[idx];
   while (TRUE) {
      if (cur == NULL) {
         result = VMCI_ERROR_NOT_FOUND;
         break;
      }
      if (VMCI_HANDLE_EQUAL(cur->handle, entry->handle)) {
         ASSERT(cur == entry);

         /* Remove entry and break. */
         if (prev) {
            prev->next = cur->next;
         } else {
            table->entries[idx] = cur->next;
         }
         cur->next = NULL;
         result = VMCI_SUCCESS;
         break;
      }
      prev = cur;
      cur = cur->next;
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMCIHashTable_Sync --
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
 *-----------------------------------------------------------------------------
 */

void
VMCIHashTable_Sync(VMCIHashTable *table)
{
   VMCILockFlags flags;
   ASSERT(table);
   VMCI_GrabLock_BH(&table->lock, &flags);
   VMCI_ReleaseLock_BH(&table->lock, flags);
}
