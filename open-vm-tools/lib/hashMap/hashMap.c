/*********************************************************
 * Copyright (C) 2009-2018,2020 VMware, Inc. All rights reserved.
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

/*********************************************************
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

#include "vm_basic_types.h"
#include "vm_assert.h"

#include <stdlib.h>
#include <string.h>

#include "hashMap.h"
#include "clamped.h"
#include "util.h"
#ifdef VMX86_SERVER
#include "aioMgr.h"
#include "iovector.h"
#endif

/*
 * hashMap.c --
 *
 *    This is a map data structure that can store a sparsely indexed data set.
 *    It is not intended to be thread safe nor should it be used in a way that
 *    this may cause problems.
 *
 *    Entries are stored based on a simple hash of their key.  Memory
 *    allocations are kept to a minimum to ensure that this is appropriate for
 *    use in a kernel mode driver.  Linear probing is used to resolve hash
 *    collisions.
 *
 *    This implementation only supports static length keys.  It might be
 *    possible to store the keys outside the table and thus support keys of
 *    variable length but this isn't currently planned.  This would allow for
 *    string keys for example.
 *
 *    Callers should not store pointers to objects stored in the map as they may
 *    become invalid as a result of a resize.  If you need to share objects
 *    stored as values in the map, then store pointers to the objects instead.
 *
 *    All objects are copied into the map and must be freed as appropriate by
 *    the caller.
 *
 *    This particular HashMap has some important differences from the hash table
 *    implementation in bora/lib/misc
 *    - HashMap supports key removal
 *    - HashMap only supports fixed size keys and values while hashTable
 *      supports string and insensitive string keys and only supports pointer
 *      data.  This means its possible to store entire data structures in
 *      HashMap.
 *    - HashMap uses linear probing to resolve collisions while hashTable uses
 *      chaining.  HashMap will dynamically resize itself as necessary.
 *    - Pointers to HashMap values will be invalidated if the internal structure
 *      is resized.  If this is a problem, you should store the pointer in the
 *      HashMap rather than the object itself.
 *    - HashMap will ensure that a maximum load factor is not exceeded.
 *
 *    hashMap is now restrictd to userworld applications on ESX builds ONLY.
 *    See PR 817760 which has an attached patchfile to remove this limitation
 *    if in future that is felt to be desireable.
 */

#define HASHMAP_DEFAULT_ALPHA 2

struct HashMap {
   uint8 *entries;
   uint32 numEntries;
   uint32 count;
   uint32 alpha;

   size_t keySize;
   size_t dataSize;
   size_t entrySize;

   size_t keyOffset;
   size_t dataOffset;
};

#ifdef VMX86_SERVER
#pragma pack(push, 1)
typedef struct HashMapOnDisk {
   uint32 numEntries;
   uint32 count;
   uint32 alpha;

   uint64 keySize;
   uint64 dataSize;
   uint64 entrySize;

   uint64 keyOffset;
   uint64 dataOffset;
} HashMapOnDisk;
#pragma pack(pop)
#endif

typedef enum {
   HashMapState_EMPTY  = 0,
   HashMapState_FILLED,
   HashMapState_DELETED,
} HashMapEntryState;

typedef struct {
   uint32 state;
   uint32 hash;
} HashMapEntryHeader;

#define NO_FREE_INDEX ((uint32) -1)

static Bool InitMap(struct HashMap *map, uint32 numEntries, uint32 alpha,
                    size_t keySize, size_t dataSize);
static void CalculateEntrySize(struct HashMap *map);
static void GetEntry(struct HashMap *map, uint32 index, HashMapEntryHeader **header, void **key, void **data);
static uint32 ComputeHash(struct HashMap *map, const void *key);
static Bool LookupKey(struct HashMap* map, const void *key, Bool constTimeLookup, HashMapEntryHeader **header, void **data, uint32 *freeIndex);
static Bool CompareKeys(struct HashMap *map, const void *key, const void *compare);
static Bool ConstTimeCompareKeys(struct HashMap *map, const void *key, const void *compare);
static Bool NeedsResize(struct HashMap *map);
static void Resize(struct HashMap *map);
INLINE void EnsureSanity(HashMap *map);

/*
 * ----------------------------------------------------------------------------
 *
 * CheckSanity --
 *
 *   Same code as EnsureSanity except return Bool instead of ASSERTing
 *
 * Results:
 *    TRUE is sane.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

#ifdef VMX86_DEBUG
static INLINE Bool
CheckSanity(HashMap *map)
{
   uint32 i, cnt = 0;

   ASSERT(map);
   for (i = 0; i < map->numEntries; i++) {
      HashMapEntryHeader *header = NULL;
      void *key, *data;

      GetEntry(map, i, &header, &key, &data);
      ASSERT(header);
      ASSERT(header->state == HashMapState_FILLED ||
             header->state == HashMapState_EMPTY ||
             header->state == HashMapState_DELETED);
      if (header->state == HashMapState_FILLED) {
         cnt++;
         if (header->hash != ComputeHash(map, key)) {
            return FALSE;
         }
      }
   }

   if (cnt != map->count) {
      return FALSE;
   }

   if (!map->numEntries) {
      return FALSE;
   }
   return TRUE;
}
#endif

/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_AllocMap --
 *
 *    Allocate a map and the space for the entries.
 *
 * Results:
 *    Returns a pointer to a HashMap or NULL on failure.
 *
 * Side Effects:
 *    Allocates memory.
 *
 * ----------------------------------------------------------------------------
 */

struct HashMap*
HashMap_AllocMap(uint32 numEntries,  // IN
                 size_t keySize,     // IN
                 size_t dataSize)    // IN
{
   return HashMap_AllocMapAlpha(numEntries, HASHMAP_DEFAULT_ALPHA, keySize, dataSize);
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_AllocMapAlpha --
 *
 *    Allocate a map and the space for the entries.  The value of alpha is
 *    treated as a denominator for the maximum allowable load factor.  I.e. an
 *    alpha value of 2 would correspond to a maximum load factor of 0.5.  The
 *    map will be enlarged when elements are added in order to maintain this
 *    load factor.
 *
 * Results:
 *    Returns a pointer to a HashMap or NULL on failure.
 *
 * Side Effects:
 *    Allocates memory.
 *
 * ----------------------------------------------------------------------------
 */

struct HashMap*
HashMap_AllocMapAlpha(uint32 numEntries,  // IN
                      uint32 alpha,       // IN
                      size_t keySize,     // IN
                      size_t dataSize)    // IN
{
   struct HashMap *map;
   map = calloc(1, sizeof *map);

   ASSERT(alpha);

   if (map) {
      if (!InitMap(map, numEntries, alpha, keySize, dataSize)) {
         HashMap_DestroyMap(map);
         return NULL;
      }
   }

   return map;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_DestroyMap --
 *
 *    Destroy a HashMap, clear out all the entries and free the memory.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    Frees memory.
 *
 * ----------------------------------------------------------------------------
 */

void
HashMap_DestroyMap(struct HashMap *map)  // IN
{
   if (map) {
      free(map->entries);
   }
   free(map);
}


/*
 * ----------------------------------------------------------------------------
 *
 * InitMap --
 *
 *    Initializes a map's internal structure.  The value of alpha is treated
 *    as a denominator for the maximum allowable load factor.  I.e. an alpha
 *    value of 2 would correspond to a maximum load factor of 0.5.  The map
 *    will be enlarged when elements are added in order to maintain this load
 *    factor.
 *
 * Results:
 *    Returns TRUE on success or FALSE if the memory allocation failed.
 *
 * Side Effects:
 *    Allocates memory.
 *
 * ----------------------------------------------------------------------------
 */

static Bool
InitMap(struct HashMap *map,     // IN
        uint32 numEntries,       // IN
        uint32 alpha,            // IN
        size_t keySize,          // IN
        size_t dataSize)         // IN
{
   ASSERT(map);
   ASSERT(alpha);
   ASSERT(numEntries);

   /*
    * Ensure that the entries map is at least large enough to hold all of the
    * entries that were requested taking into account the alpha factor.
    */
   numEntries *= alpha;

   map->numEntries = numEntries;
   map->alpha = alpha;
   map->keySize = keySize;
   map->dataSize = dataSize;

   CalculateEntrySize(map);
   map->entries = calloc(numEntries, map->entrySize);

   if (map->entries) {
      EnsureSanity(map);
   }

   return map->entries != NULL;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Put --
 *
 *    Put the value at data against the key.  This will replace any existing
 *    data that is in the table without warning.
 *
 * Results:
 *    TRUE if the put operation is successful
 *    FALSE otherwise
 *
 * Side Effects:
 *    The value in data is copied to the table and can be referenced in the
 *    future by the value in key.
 *
 * ----------------------------------------------------------------------------
 */

Bool
HashMap_Put(struct HashMap *map,    // IN
            const void *key,        // IN
            const void *data)       // IN
{
   uint32 freeIndex;
   HashMapEntryHeader *header;
   void *tableData;

   if (!LookupKey(map, key, FALSE, &header, &tableData, &freeIndex)) {
      uint32 hash = ComputeHash(map, key);
      void *tableKey;

      if (NeedsResize(map)) {
         Resize(map);
         if (LookupKey(map, key, FALSE, &header, &tableData, &freeIndex)) {
            /*
             * Somehow our key appeared after resizing the table.
             */
            ASSERT(FALSE);
         }
         if (freeIndex == NO_FREE_INDEX) {
            /*
             * The resize must have failed.
             */
            return FALSE;
         }
      }

      map->count++;
      GetEntry(map, freeIndex, &header, &tableKey, &tableData);
      ASSERT(header);

      header->state = HashMapState_FILLED;
      header->hash = hash;
      memcpy(tableKey, key, map->keySize);
   }

   ASSERT(data || map->dataSize == 0);
   memcpy(tableData, data, map->dataSize);

   EnsureSanity(map);
   return TRUE;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Get --
 *
 *    Get the value corresponding to the given key.
 *
 * Results:
 *    Returns a pointer to the data that was previously stored by HashMap_Put or
 *    NULL if the key wasn't found.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

void *
HashMap_Get(struct HashMap *map,    // IN
            const void *key)        // IN
{
   void *data;
   uint32 freeIndex;
   HashMapEntryHeader *header;

   if (LookupKey(map, key, FALSE, &header, &data, &freeIndex)) {
      return data;
   }

   return NULL;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_ConstTimeGet --
 *
 *    Timing attack safe version of HashMap_Get. This will call LookupKey with
 *    the constTime flag set to 1 which will do the memory comparison with
 *    Util_ConstTimeMemDiff instead of memcmp. Note that there is a bit of a
 *    time penalty associated with this so only use this if you are looking up
 *    sensitive information.
 *
 * Results:
 *    Returns a pointer to the data that was previously stored by HashMap_Put or
 *    NULL if the key wasn't found.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

void *
HashMap_ConstTimeGet(struct HashMap *map,    // IN
                     const void *key)        // IN
{
   void *data;
   uint32 freeIndex;
   HashMapEntryHeader *header;

   if (LookupKey(map, key, TRUE, &header, &data, &freeIndex)) {
      return data;
   }

   return NULL;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Clear --
 *
 *    Remove all entries from the HashMap.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    All entries in the map are removed.
 *
 * ----------------------------------------------------------------------------
 */

void
HashMap_Clear(struct HashMap *map) // IN
{
   int i = 0;
   HashMapEntryHeader *header;
   void *key, *data;

   ASSERT(map);
   for (i = 0; i < map->numEntries; i++) {
      GetEntry(map, i, &header, &key, &data);
      ASSERT(header);
      header->state = HashMapState_EMPTY;
   }
   map->count = 0;
   EnsureSanity(map);
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Remove --
 *
 *    Remove an entry from the map.
 *
 * Results:
 *    Returns TRUE if the entry was in the map, FALSE if the entry was not in
 *    the map.
 *
 * Side Effects:
 *    The entry is removed from the map.
 *
 * ----------------------------------------------------------------------------
 */

Bool
HashMap_Remove(struct HashMap *map,   // IN
               const void *key)       // IN
{
   uint32 freeIndex;
   HashMapEntryHeader *header;
   void *tableData;

   if (!LookupKey(map, key, FALSE, &header, &tableData, &freeIndex)) {
      return FALSE;
   }

   /*
    * XXX: This could be made slightly smarter.  We could check the next entry
    * to see if it's EMPTY and then mark this one as empty as well.
    */
   map->count--;
   header->state = HashMapState_DELETED;

   EnsureSanity(map);

   return TRUE;
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Count --
 *
 *    Returns the current count of entries in the map.
 *
 * Results:
 *    The current count of entries in the map.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

uint32
HashMap_Count(struct HashMap *map) // IN
{
   return map->count;
}


/*
 * ----------------------------------------------------------------------------
 *
 * CalculateEntrySize --
 *
 *    Calculate the size of the entry and the offsets to the key and data.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    The map structure is adjusted to contain the correct sizes.
 *
 * ----------------------------------------------------------------------------
 */

void
CalculateEntrySize(struct HashMap *map) // IN
{
   size_t alignKeySize, alignDataSize;
   size_t alignKeyOffset, alignDataOffset;

   ASSERT(map);

   alignKeySize = ROUNDUP(map->keySize, 4);
   alignDataSize = ROUNDUP(map->dataSize, 4);

   alignKeyOffset = sizeof (HashMapEntryHeader);
   alignDataOffset = ROUNDUP(alignKeyOffset + alignKeySize, 4);

   map->entrySize = sizeof (HashMapEntryHeader) + alignKeySize + alignDataSize;
   map->keyOffset = alignKeyOffset;
   map->dataOffset = alignDataOffset;
}


/*
 * ----------------------------------------------------------------------------
 *
 * LookupKey --
 *
 *    Use linear probing to find a free space in the table or the data that
 *    we're interested in.
 *
 *    Call this function with constTimeLookup = TRUE to use a timing attack
 *    safe version of memcmp while comparing keys.
 *
 * Returns:
 *    - TRUE if the key was found in the table, FALSE otherwise.
 *    - Returns the entry header on header, data pointer on data and the first
 *    non-filled index that was encountered on freeIndex
 *
 * Side Effects:
 *    - Header and Data are changed.  They should only be considered valid if
 *    the key was found.
 *    - FreeIndex will be updated to point to the first non-filled index.
 *
 * ----------------------------------------------------------------------------
 */

Bool
LookupKey(struct HashMap* map,          // IN
          const void *key,              // IN
          Bool constTimeLookup,         // IN
          HashMapEntryHeader **header,  // OUT
          void **data,                  // OUT
          uint32 *freeIndex)            // OUT
{
   uint32 hash = ComputeHash(map, key);
   uint32 index = hash % map->numEntries;
   uint32 probe = 0;

   Bool done = FALSE, found = FALSE;
   void *tableKey;

   ASSERT(map);
   ASSERT(key);
   ASSERT(data);
   ASSERT(freeIndex);

   *freeIndex = NO_FREE_INDEX;

   while (!done && probe < map->numEntries + 1) {
      uint32 currentIndex = (index + probe) % map->numEntries;

      GetEntry(map, currentIndex, header, &tableKey, data);
      ASSERT(header);

      switch ((*header)->state) {
      case HashMapState_EMPTY:
         done = TRUE;
         /* FALL THROUGH */
      case HashMapState_DELETED:
         /*
          * We're not done if we've found a deleted space.  This may just mean
          * that the entry was deleted but that the target entry may appear
          * later.
          */
         if (*freeIndex == NO_FREE_INDEX) {
            *freeIndex = currentIndex;
         }
         break;
      case HashMapState_FILLED:
         if ((*header)->hash == hash) {
            /*
             * There is some performance penalty to doing a constant time
             * comparison, so only use that version if it's been explicitly
             * asked for.
             */
            if (constTimeLookup) {
               found = ConstTimeCompareKeys(map, key, tableKey);
            } else {
               found = CompareKeys(map, key, tableKey);
            }
            if (found) {
               done = TRUE;
            }
         }
         break;
      default:
         NOT_REACHED();
      }
      probe++;
   }

   ASSERT(found || *freeIndex != NO_FREE_INDEX || map->count == map->numEntries);

   return found;
}


/*
 * ----------------------------------------------------------------------------
 *
 * GetEntry --
 *
 *    Get a specific entry from the entries list.  This does not perform any
 *    hash compare, it's a simple index based lookup.
 *
 * Results:
 *    The header is stored in header, key in key and the data in data.  These
 *    are all direct pointers into the table.
 *
 * Side Effects:
 *    The header, key and data pointers are modified.
 *
 * ----------------------------------------------------------------------------
 */

void
GetEntry(struct HashMap *map,          // IN
         uint32 index,                 // IN
         HashMapEntryHeader **header,  // OUT
         void **key,                   // OUT
         void **data)                  // OUT
{
   uint8 *entry;

   ASSERT(map);
   ASSERT(header);
   ASSERT(key);
   ASSERT(data);
   ASSERT(index < map->numEntries);

   entry = ((uint8 *)map->entries) + (map->entrySize * index);
   ASSERT(entry);
   *header = (HashMapEntryHeader *) (entry);
   *key = (void*) (entry + map->keyOffset);
   *data = (void*) (entry + map->dataOffset);
}


/*
 * ----------------------------------------------------------------------------
 *
 * ComputeHash --
 *
 *    Compute the hash of the given key.
 *
 * Results:
 *    An opaque hash value based on the given key.  Given the same key in the
 *    same map, this function will always return the same value.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

uint32
ComputeHash(struct HashMap *map,  // IN
            const void *key)      // IN
{
   /*
    * djb2, with n == 33. See http://www.cse.yorku.ca/~oz/hash.html.
    *
    * This hash function is largely borrowed from the emmet library in bora/lib
    * This hash table implementation does a hash compare before comparing the
    * keys so it's inappropriate for the hash function to take the modulo before
    * returning.
    */
   uint32 h = 5381;
   const uint8 *keyByte;
   size_t i = 0;

   for (keyByte = key, i = 0; i < map->keySize; keyByte++, i++) {
      h *= 33;
      h += *keyByte;
   }
   return h;
}


/*
 * ----------------------------------------------------------------------------
 *
 * CompareKeys --
 *
 *    Compare two keys to one another.
 *
 * Results:
 *    Returns TRUE if the two keys are binary equal over the length specified
 *    in the map.  FALSE if the two keys are different.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

Bool
CompareKeys(struct HashMap *map,    // IN
            const void *key,        // IN
            const void *compare)    // IN
{
   return memcmp(key, compare, map->keySize) == 0;
}


/*
 * ----------------------------------------------------------------------------
 *
 * ConstTimeCompareKeys --
 *
 *    Timing attack safe version of CompareKeys. Instead of calling memcmp,
 *    which will return after the first character that doesn't match, this
 *    calls a constant time memory comparison function.
 *
 * Results:
 *    Returns TRUE if the two keys are binary equal over the length specified
 *    in the map.  FALSE if the two keys are different.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

Bool
ConstTimeCompareKeys(struct HashMap *map,    // IN
                     const void *key,        // IN
                     const void *compare)    // IN
{
   return Util_ConstTimeMemDiff(key, compare, map->keySize) == 0;
}


/*
 * ----------------------------------------------------------------------------
 *
 * NeedsResize --
 *
 *    Determine if adding another element to the map will require that the map
 *    be resized.  This takes into account the maximum load factor that is
 *    allowed for this map.
 *
 * Results:
 *    Returns TRUE if the map should be resized.
 *
 * Side Effects:
 *    None.
 *
 * ----------------------------------------------------------------------------
 */

Bool
NeedsResize(struct HashMap *map)
{
   uint32 required;

   Clamped_UMul32(&required, map->count, map->alpha);

   return required >= map->numEntries;
}

/*
 * ----------------------------------------------------------------------------
 *
 * Resize --
 *
 *    Doubles the size of the entries array until it is at least large enough
 *    to ensure the maximum load factor is not exceeded.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    The entries list is doubled in size and the entries are copied into the
 *    appropriate location.  Callers should not assume that the locations that
 *    were valid before this was called are still valid as all entries may
 *    appear at different locations after this function completes.
 *
 * ----------------------------------------------------------------------------
 */

void
Resize(struct HashMap *map)   // IN
{
   struct HashMap oldHashMap = *map;
   int i;

   if (map->numEntries == MAX_UINT32) {
      if (map->count < MAX_UINT32) {
         /*
          * We're already at the maximum size of the array and there's still
          * room for the new entry, don't bother resizing.
          */
         return;
      } else {
         /*
          * This situation is fatal, though we're unlikely to ever hit this with
          * realistic usage.
          */
         Panic("Ran out of room in the hashtable\n");
      }
   }

   /*
    * We might, at some point, want to look at making this grow geometrically
    * until we hit some threshold and then grow arithmetically after that.  To
    * keep it simple for now, however, we'll just grow geometrically all the
    * time.
    */
   map->entries = calloc(oldHashMap.numEntries * 2, oldHashMap.entrySize);
   if (!map->entries) {
      map->entries = oldHashMap.entries;
      return;
   }

   do {
      if (!Clamped_UMul32(&map->numEntries, map->numEntries, 2)) {
         /* Prevent overflow and */
         break;
      }
   } while (NeedsResize(map));

   map->count = 0;

   for (i = 0; i < oldHashMap.numEntries; i++) {
      HashMapEntryHeader *oldHeader;
      HashMapEntryHeader *newHeader;
      void *oldKey;
      void *oldData;
      void *newKey;
      void *newData;
      uint32 freeIndex;

      GetEntry(&oldHashMap, i, &oldHeader, &oldKey, &oldData);
      switch (oldHeader->state) {
      case HashMapState_EMPTY:
      case HashMapState_DELETED:
         continue;
      case HashMapState_FILLED:
         if (!LookupKey(map, oldKey, FALSE, &newHeader, &newData, &freeIndex)) {
            GetEntry(map, freeIndex, &newHeader, &newKey, &newData);

            newHeader->hash = oldHeader->hash;
            newHeader->state = HashMapState_FILLED;
            memcpy(newKey, oldKey, map->keySize);
            memcpy(newData, oldData, map->dataSize);

            map->count++;
         } else {
            /*
             * There is only one way that this could happen; the key is in the
             * old map twice.  That's clearly an error.
             */
            ASSERT(FALSE);
            continue;
         }
      }
   }

   ASSERT(oldHashMap.count == map->count);
   free(oldHashMap.entries);
   EnsureSanity(map);
}


/*
 * ----------------------------------------------------------------------------
 *
 * HashMap_Iterate --
 *
 *    Iterate over the contents of the map, optionally clearing each entry as
 *    it's passed.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    itFn is called for each entry in the hashMap until.  If clear is TRUE,
 *    each entry will be removed from the map after it has been seen by the
 *    iterator function.
 *
 * ----------------------------------------------------------------------------
 */

void
HashMap_Iterate(HashMap *map,             // IN
                HashMapIteratorFn itFn,   // IN
                Bool clear,               // IN
                void *userData)           // IN/OUT
{
   int i = 0;
   HashMapEntryHeader *header;
   void *key, *data;

   ASSERT(map);
   ASSERT(itFn);

   for (i = 0; i < map->numEntries; i++) {
      GetEntry(map, i, &header, &key, &data);
      if (header->state == HashMapState_FILLED) {
         itFn(key, data, userData);
         if (clear) {
            map->count--;
         }
      }
      if (clear) {
         header->state = HashMapState_EMPTY;
      }
   }

   ASSERT(map->count == 0 || !clear);
}


/*
 * ----------------------------------------------------------------------------
 *
 * EnsureSanity --
 *
 *    Ensure that the HashMap contents are still sane.  That is, each entry has
 *    a correctly computed hash, the count is correct and the header states are
 *    valid for all entries.  This should be called at the end of every
 *    function which modifies the map.
 *
 * Results:
 *    None.
 *
 * Side Effects:
 *    Fails an assert if the contents are incorrect.
 *
 * ----------------------------------------------------------------------------
 */

void
EnsureSanity(HashMap *map)
{
   ASSERT(CheckSanity(map) == TRUE);
}



