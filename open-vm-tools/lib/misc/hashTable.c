/*********************************************************
 * Copyright (C) 2004-2017 VMware, Inc. All rights reserved.
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

/*
 * hashTable.c --
 *
 *      An implementation of hashtable with no removals.
 *      For string keys.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef _WIN32
#include <windows.h>
#endif

#include "vmware.h"
#include "hashTable.h"
#include "vm_basic_asm.h"
#include "util.h"
#include "str.h"
#include "vm_atomic.h"


#define HASH_ROTATE     5


/*
 * Pointer to hash table entry
 *
 * The assumption is that Atomic_ReadPtr() and Atomic_WritePtr()
 * are cheap, so can be used with nonatomic hash tables.
 * SETENTRYATOMIC() sets a link to point to a new entry
 * if the current value is the same as the old entry.
 * It returns TRUE on success.
 */

typedef Atomic_Ptr HashTableLink;

#define ENTRY(l) ((HashTableEntry *) Atomic_ReadPtr(&(l)))
#define SETENTRY(l, e) Atomic_WritePtr(&(l), e)
#ifdef NO_ATOMIC_HASHTABLE
#define SETENTRYATOMIC(l, old, new) (Atomic_WritePtr(&(l), new), TRUE)
#else
#define SETENTRYATOMIC(l, old, new) \
   (Atomic_ReadIfEqualWritePtr(&(l), old, new) == (old))
#endif

/*
 * An entry in the hashtable.
 */

typedef struct HashTableEntry {
   HashTableLink     next;
   const void       *keyStr;
   Atomic_Ptr        clientData;
} HashTableEntry;

/*
 * The hashtable structure.
 */

struct HashTable {
   uint32                 numEntries;
   uint32                 numBits;
   int                    keyType;
   Bool                   atomic;
   Bool                   copyKey;
   HashTableFreeEntryFn   freeEntryFn;
   HashTableLink         *buckets;

   size_t                 numElements;
};


/*
 * Local functions
 */

HashTableEntry *HashTableLookupOrInsert(HashTable *ht,
                                        const void *keyStr,
                                        void *clientData);


/*
 *-----------------------------------------------------------------------------
 *
 * HashTableComputeHash --
 *
 *      Compute hash value based on key type and hash size.
 *
 * Results:
 *      The hash value.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static uint32
HashTableComputeHash(const HashTable *ht,  // IN: hash table
                     const void *s)        // IN: string to hash
{
   uint32 h = 0;

   switch (ht->keyType) {
   case HASH_STRING_KEY: {
         int c;
         unsigned char *keyPtr = (unsigned char *) s;

         while ((c = *keyPtr++)) {
            h ^= c;
            h = h << HASH_ROTATE | h >> (32 - HASH_ROTATE);
         }
      }
      break;
   case HASH_ISTRING_KEY: {
         int c;
         unsigned char *keyPtr = (unsigned char *) s;

         while ((c = tolower(*keyPtr++))) {
            h ^= c;
            h = h << HASH_ROTATE | h >> (32 - HASH_ROTATE);
         }
      }
      break;
   case HASH_INT_KEY:
      ASSERT_ON_COMPILE(sizeof s == 4 || sizeof s == 8);

      if (sizeof s == 4) {
         h = (uint32) (uintptr_t) s;
      } else {
         h = (uint32) (uintptr_t) s ^ (uint32) ((uint64) (uintptr_t) s >> 32);
      }
      h *= 48271;  // http://www.google.com/search?q=48271+pseudorandom
      break;
   default:
      NOT_REACHED();
   }

   {
      int numBits = ht->numBits;
      uint32 mask = MASK(numBits);

      for (; h > mask; h = (h & mask) ^ (h >> numBits)) {
      }
   }

   ASSERT(h < ht->numEntries);

   return h;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HashTableEqualKeys --
 *
 *      Compare two keys based on key type
 *
 * Results:
 *      TRUE if keys are equal.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HashTableEqualKeys(const HashTable *ht,  // IN: hash table
                   const void *key1,     // IN: key
                   const void *key2)     // IN: key
{
   switch (ht->keyType) {
   case HASH_STRING_KEY:
      return Str_Strcmp((const char *) key1, (const char *) key2) == 0;

   case HASH_ISTRING_KEY:
      return Str_Strcasecmp((const char *) key1, (const char *) key2) == 0;

   default:
      return key1 == key2;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Alloc --
 *
 *      Create a hash table.
 *
 * Results:
 *      The new hashtable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HashTable *
HashTable_Alloc(uint32 numEntries,        // IN: must be a power of 2
                int keyType,              // IN: whether keys are strings
                HashTableFreeEntryFn fn)  // IN: free entry function
{
   HashTable *ht;

   ASSERT(numEntries > 0);
   if ((numEntries & (numEntries - 1)) != 0) {
      Panic("%s only takes powers of 2 \n", __FUNCTION__);
   }
#ifdef NO_ATOMIC_HASHTABLE
   VERIFY((keyType & HASH_FLAG_ATOMIC) == 0);
#endif
   ASSERT((keyType & HASH_FLAG_COPYKEY) == 0 ||
          ((keyType & HASH_TYPE_MASK) == HASH_STRING_KEY ||
           (keyType & HASH_TYPE_MASK) == HASH_ISTRING_KEY));

   ht = Util_SafeMalloc(sizeof *ht);

   ht->numBits = lssb32_0(numEntries);
   ht->numEntries = numEntries;
   ht->keyType = keyType & HASH_TYPE_MASK;
   ht->atomic = (keyType & HASH_FLAG_ATOMIC) != 0;
   ht->copyKey = (keyType & HASH_FLAG_COPYKEY) != 0;
   ht->freeEntryFn = fn;
   ht->buckets = Util_SafeCalloc(ht->numEntries, sizeof *ht->buckets);
   ht->numElements = 0;

   return ht;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_AllocOnce --
 *
 *      Create a hash table and store it in the supplied Atomic_Ptr,
 *      unless it's already been created.
 *
 * Results:
 *      The new (or existing) hashtable.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HashTable *
HashTable_AllocOnce(Atomic_Ptr *var,          // IN/OUT: the atomic var
                    uint32 numEntries,        // IN: must be a power of 2
                    int keyType,              // IN: whether keys are strings
                    HashTableFreeEntryFn fn)  // IN: free entry function
{
   HashTable *ht;

   if ((ht = Atomic_ReadPtr(var)) == NULL) {
      HashTable *new = HashTable_Alloc(numEntries, keyType, fn);
#ifdef NO_ATOMIC_HASHTABLE
      Atomic_WritePtr(var, new);
#else
      ht = Atomic_ReadIfEqualWritePtr(var, NULL, new);
#endif
      if (ht == NULL) {
         ht = new;
      } else {
         HashTable_FreeUnsafe(new);
      }
   }
   ASSERT(ht == Atomic_ReadPtr(var));

   return ht;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Clear --
 *
 *      Clear all entries a hashtable by freeing them.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
HashTableClearInternal(HashTable *ht)  // IN/OUT:
{
   int i;

   ht->numElements = 0;

   for (i = 0; i < ht->numEntries; i++) {
      HashTableEntry *entry;

      while ((entry = ENTRY(ht->buckets[i])) != NULL) {
         SETENTRY(ht->buckets[i], ENTRY(entry->next));
         if (ht->copyKey) {
            free((void *) entry->keyStr);
         }
         if (ht->freeEntryFn) {
            ht->freeEntryFn(Atomic_ReadPtr(&entry->clientData));
         }
         free(entry);
      }
   }
}


void
HashTable_Clear(HashTable *ht)  // IN/OUT:
{
   ASSERT(ht);
   ASSERT(!ht->atomic);

   HashTableClearInternal(ht);
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Free --
 *
 *      Free the hash table skeleton. Do not call this if the hashTable is
 *      atomic; use HashTable_FreeUnsafe and HEED THE RESTRICTIONS!
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
HashTable_Free(HashTable *ht)  // IN/OUT:
{
   if (ht != NULL) {
      ASSERT(!ht->atomic);

      HashTableClearInternal(ht);

      free(ht->buckets);
      free(ht);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_FreeUnsafe --
 *
 *      HashTable_Free for atomic hash tables.  Non-atomic hash tables
 *      should always use HashTable_Free.  The caller should make sure
 *      no other thread may access the hash table when this is called.
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
HashTable_FreeUnsafe(HashTable *ht)  // IN/OUT:
{
   if (ht != NULL) {
      HashTableClearInternal(ht);

      free(ht->buckets);
      free(ht);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HashTableLookup --
 *
 *      Core of the lookup function.
 *
 * Results:
 *      A pointer to the found HashTableEntry or NULL if not found
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static HashTableEntry *
HashTableLookup(const HashTable *ht,  // IN:
                const void *keyStr,   // IN:
                uint32 hash)          // IN:
{
   HashTableEntry *entry;

   for (entry = ENTRY(ht->buckets[hash]);
        entry != NULL;
        entry = ENTRY(entry->next)) {
      if (HashTableEqualKeys(ht, entry->keyStr, keyStr)) {
         return entry;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Lookup --
 *
 *      Lookup an element in a hashtable.
 *
 * Results:
 *      TRUE if the entry was found. (the clientData is set)
 *      FALSE otherwise
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Lookup(const HashTable  *ht, // IN:
                 const void *keyStr,   // IN:
                 void **clientData)    // OUT/OPT:
{
   uint32 hash = HashTableComputeHash(ht, keyStr);
   HashTableEntry *entry = HashTableLookup(ht, keyStr, hash);

   if (entry == NULL) {
      return FALSE;
   }

   if (clientData) {
      *clientData = Atomic_ReadPtr(&entry->clientData);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Delete --
 *
 *      Delete an element from the hashtable.
 *
 * Results:
 *      TRUE if the entry was found and subsequently removed
 *      FALSE otherwise
 *
 * Side effects:
 *      See above
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Delete(HashTable  *ht,      // IN/OUT: the hash table
                 const void *keyStr)  // IN: key for the element to remove
{
   return HashTable_LookupAndDelete(ht, keyStr, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_LookupAndDelete --
 *
 *      Look up an element in the hash table, then delete it.
 *
 *      If clientData is specified, then the entry data is returned,
 *      and the free function is not called.
 *
 * Results:
 *      TRUE if the entry was found and subsequently removed
 *      FALSE otherwise
 *      Entry value is returned in *clientData.
 *
 * Side effects:
 *      See above
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_LookupAndDelete(HashTable *ht,       // IN/OUT: the hash table
                          const void *keyStr,  // IN: key for the element
                          void **clientData)   // OUT: return data
{
   uint32 hash = HashTableComputeHash(ht, keyStr);
   HashTableLink *linkp;
   HashTableEntry *entry;

   ASSERT(!ht->atomic);

   for (linkp = &ht->buckets[hash];
        (entry = ENTRY(*linkp)) != NULL;
        linkp = &entry->next) {
      if (HashTableEqualKeys(ht, entry->keyStr, keyStr)) {
         SETENTRY(*linkp, ENTRY(entry->next));
         ht->numElements--;
         if (ht->copyKey) {
            free((void *) entry->keyStr);
         }
         if (clientData != NULL) {
            *clientData = Atomic_ReadPtr(&entry->clientData);
         } else if (ht->freeEntryFn) {
            ht->freeEntryFn(Atomic_ReadPtr(&entry->clientData));
         }
         free(entry);

         return TRUE;
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Insert --
 *
 *      Insert an element into the hashtable.
 *
 *      Unless the hash table was created with HASH_FLAG_COPYKEY,
 *      the string key is not duplicated and thus cannot be freed until
 *      the entry is deleted or the hash table is cleared or freed.
 *
 * Results:
 *      FALSE if item already exists.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Insert(HashTable  *ht,          // IN/OUT:
                 const void *keyStr,      // IN:
                 void       *clientData)  // IN/OPT:
{
   return HashTableLookupOrInsert(ht, keyStr, clientData) == NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_LookupOrInsert --
 *
 *      Look up an a key, return the element if found.
 *      Otherwise, insert it into the hashtable and return it.
 *
 *      Unless the hash table was created with HASH_FLAG_COPYKEY,
 *      the string key is not duplicated and thus cannot be freed until
 *      the entry is deleted or the hash table is cleared or freed.
 *
 * Results:
 *      Old element or new one.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void *
HashTable_LookupOrInsert(HashTable  *ht,          // IN/OUT:
                         const void *keyStr,      // IN:
                         void       *clientData)  // IN:
{
   HashTableEntry *entry = HashTableLookupOrInsert(ht, keyStr, clientData);

   return entry == NULL ? clientData : Atomic_ReadPtr(&entry->clientData);
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_ReplaceOrInsert --
 *
 *      Look up an a key.  If found, replace the existing clientData
 *      and return TRUE.
 *      Otherwise, insert new entry and return FALSE.
 *
 *      Unless the hash table was created with HASH_FLAG_COPYKEY,
 *      the string key is not duplicated and thus cannot be freed until
 *      the entry is deleted or the hash table is cleared or freed.
 *
 * Results:
 *      TRUE if replaced, FALSE if inserted.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_ReplaceOrInsert(HashTable  *ht,          // IN/OUT:
                          const void *keyStr,      // IN:
                          void       *clientData)  // IN:
{
   HashTableEntry *entry = HashTableLookupOrInsert(ht, keyStr, clientData);

   if (entry == NULL) {
      return FALSE;
   }

#ifndef NO_ATOMIC_HASHTABLE
   if (ht->atomic && ht->freeEntryFn) {
      void *old = Atomic_ReadWritePtr(&entry->clientData, clientData);

      ht->freeEntryFn(old);
   } else
#endif
   {
      if (ht->freeEntryFn) {
         ht->freeEntryFn(Atomic_ReadPtr(&entry->clientData));
      }
      Atomic_WritePtr(&entry->clientData, clientData);
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_ReplaceIfEqual --
 *
 *      Look up an a key.  If found, replace the existing clientData
 *      if it is the same as oldClientData and return TRUE.
 *      Return FALSE otherwise.
 *
 * Results:
 *      TRUE if replaced, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_ReplaceIfEqual(HashTable *ht,        // IN/OUT:
                         const void *keyStr,   // IN:
                         void *oldClientData,  // IN/OPT:
                         void *newClientData)  // IN/OPT:
{
   uint32 hash = HashTableComputeHash(ht, keyStr);
   HashTableEntry *entry = HashTableLookup(ht, keyStr, hash);
   Bool retval = FALSE;

   if (entry == NULL) {
      return FALSE;
   }

#ifndef NO_ATOMIC_HASHTABLE
   if (ht->atomic) {
      void *data = Atomic_ReadIfEqualWritePtr(&entry->clientData,
                                              oldClientData, newClientData);
      if (data == oldClientData) {
         retval = TRUE;
         if (ht->freeEntryFn != NULL) {
            ht->freeEntryFn(data);
         }
      }
   } else
#endif
   if (Atomic_ReadPtr(&entry->clientData) == oldClientData) {
      retval = TRUE;
      if (ht->freeEntryFn) {
         ht->freeEntryFn(Atomic_ReadPtr(&entry->clientData));
      }
      Atomic_WritePtr(&entry->clientData, newClientData);
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTableLookupOrInsert --
 *
 *      Look up an a key, return the entry if found.
 *      Otherwise, insert it into the hashtable and return NULL.
 *
 * Results:
 *      Old HashTableEntry or NULL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

HashTableEntry *
HashTableLookupOrInsert(HashTable *ht,       // IN/OUT:
                        const void *keyStr,  // IN:
                        void *clientData)    // IN/OPT:
{
   uint32 hash = HashTableComputeHash(ht, keyStr);
   HashTableEntry *entry = NULL;
   HashTableEntry *oldEntry = NULL;
   HashTableEntry *head;

again:
   head = ENTRY(ht->buckets[hash]);

   oldEntry = HashTableLookup(ht, keyStr, hash);
   if (oldEntry != NULL) {
      if (entry != NULL) {
         if (ht->copyKey) {
            free((void *) entry->keyStr);
         }
         free(entry);
      }

      return oldEntry;
   }

   if (entry == NULL) {
      entry = Util_SafeMalloc(sizeof *entry);
      if (ht->copyKey) {
         entry->keyStr = Util_SafeStrdup(keyStr);
      } else {
         entry->keyStr = keyStr;
      }
      Atomic_WritePtr(&entry->clientData, clientData);
   }
   SETENTRY(entry->next, head);
   if (ht->atomic) {
      if (!SETENTRYATOMIC(ht->buckets[hash], head, entry)) {
         goto again;
      }
   } else {
      SETENTRY(ht->buckets[hash], entry);
   }

   ht->numElements++;

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_GetNumElements --
 *
 *      Get the number of elements in the hash table.
 *
 *      Atomic hash tables do not support this function because
 *      the numElements field is not modified atomically so may be
 *      incorrect.
 *
 * Results:
 *      The number of elements.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

size_t
HashTable_GetNumElements(const HashTable *ht)  // IN:
{
   ASSERT(ht);
   ASSERT(!ht->atomic);

   return ht->numElements;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_KeyArray --
 *
 *      Returns an array of pointers to each key in the hash table.
 *
 * Results:
 *      The key array plus its size. If the hash table is empty
 *      'size' is 0 and 'keys' is NULL. Free 'keys' array with free(). Do *not*
 *      free the individual elements of the array.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
HashTable_KeyArray(const HashTable *ht,  // IN:
                   const void ***keys,   // OUT:
                   size_t *size)         // OUT:
{
   uint32 i;
   size_t j;

   ASSERT(ht);
   ASSERT(keys);
   ASSERT(size);

   ASSERT(!ht->atomic);

   *keys = NULL;
   *size = HashTable_GetNumElements(ht);

   if (*size == 0) {
      return;
   }

   /* alloc array */
   *keys = Util_SafeMalloc(*size * sizeof **keys);

   /* fill array */
   for (i = 0, j = 0; i < ht->numEntries; i++) {
      HashTableEntry *entry;

      for (entry = ENTRY(ht->buckets[i]);
           entry != NULL;
           entry = ENTRY(entry->next)) {
         (*keys)[j++] = entry->keyStr;
      }
   }
}



/*
 *----------------------------------------------------------------------
 *
 * HashTable_ToArray --
 *
 *      Returns an array of pointers to each clientData structure in the
 *      hash table.
 *
 * Results:
 *      The clientData array plus its size. If the hash table is empty
 *      'size' is 0 and 'clientDatas' is NULL. Free with free().
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
HashTable_ToArray(const HashTable *ht,  // IN:
                  void ***clientDatas,  // OUT:
                  size_t *size)         // OUT:
{
   uint32 i;
   size_t j;

   ASSERT(ht);
   ASSERT(clientDatas);
   ASSERT(size);

   ASSERT(!ht->atomic);

   *clientDatas = NULL;
   *size = HashTable_GetNumElements(ht);

   if (*size == 0) {
      return;
   }

   /* alloc array */
   *clientDatas = Util_SafeMalloc(*size * sizeof **clientDatas);

   /* fill array */
   for (i = 0, j = 0; i < ht->numEntries; i++) {
      HashTableEntry *entry;

      for (entry = ENTRY(ht->buckets[i]);
           entry != NULL;
           entry = ENTRY(entry->next)) {
         (*clientDatas)[j++] = Atomic_ReadPtr(&entry->clientData);
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_ForEach --
 *
 *      Walks the hashtable in an undetermined order, calling the
 *      callback function for each value until either the callback
 *      returns a non-zero value or all values have been walked
 *
 * Results:
 *      0 if all callback functions returned 0, otherwise the return
 *      value of the first non-zero callback.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
HashTable_ForEach(const HashTable *ht,          // IN:
                  HashTableForEachCallback cb,  // IN:
                  void *clientData)             // IN:
{
   int i;

   ASSERT(ht);
   ASSERT(cb);

   for (i = 0; i < ht->numEntries; i++) {
      HashTableEntry *entry;

      for (entry = ENTRY(ht->buckets[i]);
           entry != NULL;
           entry = ENTRY(entry->next)) {
         int result = (*cb)(entry->keyStr, Atomic_ReadPtr(&entry->clientData),
                            clientData);

         if (result) {
            return result;
         }
      }
   }

   return 0;
}

#if 0
/*
 *----------------------------------------------------------------------
 *
 * HashPrint --
 *
 *      Print out the contents of a hashtable. Useful for
 *      debugging the data structure & the hashing algorithm.
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
HashPrint(HashTable *ht) // IN
{
   int i;

   for (i = 0; i < ht->numEntries; i++) {
      HashTableEntry *entry;

      if (ht->buckets[i] == NULL) {
         continue;
      }

      printf("%4d: \n", i);

      for (entry = ENTRY(ht->buckets[i]);
           entry != NULL;
           entry = ENTRY(entry->next)) {
         if (ht->keyType == HASH_INT_KEY) {
            printf("\t%p\n", entry->keyStr);
         } else {
            printf("\t%s\n", entry->keyStr);
         }
      }
   }
}
#endif
