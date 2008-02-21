/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
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

#include "vmware.h"
#include "hashTable.h"
#include "dbllnklst.h"
#include "vm_basic_asm.h"


#define HASH_ROTATE     5


/*
 * An entry in the hashtable. Provides two client datas
 * for ease of use.
 */
typedef struct HashTableEntry {
   DblLnkLst_Links   l;
   const char       *keyStr;
   void             *clientData;
} HashTableEntry;

/*
 * The hashtable structure.
 */
struct HashTable {
   uint32                 numEntries;
   uint32                 numBits;
   int                    keyType;
   HashTableFreeEntryFn   freeEntryFn;
   DblLnkLst_Links       *buckets;

   size_t                 numElements;
};


/*
 * Local functions
 */

static HashTableEntry *HashTableLookup(HashTable *ht, const char *keyStr, 
                                       uint32 hash);


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

static INLINE uint32
HashTableComputeHash(HashTable *ht,            // IN: hash table
                     const char *s)            // IN: string to hash
{
   uint32 h = 0;

   switch (ht->keyType) {
   case HASH_STRING_KEY: {
	 int c;
	 while ((c = (unsigned char) *s++)) {
	    h ^= c;
	    h = h << HASH_ROTATE | h >> (32 - HASH_ROTATE);
	 }
      }
      break;
   case HASH_ISTRING_KEY: {
	 int c;
	 while ((c = tolower((unsigned char) *s++))) {
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

#if defined(N_PLAT_NLM)
static int
strcasecmp(const char *s1,  // IN:
           const char *s2)  // IN:
{
   while (*s1 && tolower(*s1++) == tolower(*s2++));
   
   return tolower(*s1) - tolower(*s2);
}

static int
ffs(uint32 bits)
{
   uint32 i;

   if (bits == 0) {
      i = 0;
   } else {
      i = 1;

      while (bits) {
         i++;
         bits >>= 1;
      }
   }

   return i;
}
#endif

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

static INLINE Bool
HashTableEqualKeys(HashTable *ht,            // IN: hash table
                   const char *key1,         // IN: key
                   const char *key2)         // IN: key
{
   switch (ht->keyType) {
   case HASH_STRING_KEY:
      return strcmp(key1, key2) == 0;

   case HASH_ISTRING_KEY:
      return strcasecmp(key1, key2) == 0;

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
 *	None.
 *
 *----------------------------------------------------------------------
 */

HashTable *
HashTable_Alloc(uint32 numEntries,       // IN: must be a power of 2
                int keyType,             // IN: whether keys are strings
                HashTableFreeEntryFn fn) // IN: free entry function
{
   HashTable *ht;
   uint32 i;

   ASSERT(numEntries > 0);
   if ((numEntries & (numEntries - 1)) != 0) {
      Panic("%s only takes powers of 2 \n", __FUNCTION__);
   }

   ht = (HashTable *) malloc(sizeof(HashTable));
   ASSERT_MEM_ALLOC(ht);

   ht->numBits = ffs(numEntries) - 1;
   ht->numEntries = numEntries;
   ht->keyType = keyType;   
   ht->freeEntryFn = fn;
   ht->buckets = (DblLnkLst_Links *) malloc(ht->numEntries * 
                                            sizeof(DblLnkLst_Links));
   ASSERT_MEM_ALLOC(ht->buckets);

   for (i = 0; i < ht->numEntries; i++) {
      DblLnkLst_Init(&ht->buckets[i]);
   }

   ht->numElements = 0;

   return ht;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_Free --
 *
 *      Free the hash table skeleton.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
HashTable_Free(HashTable *ht) // IN/OUT
{
   ASSERT(ht);

   HashTable_Clear(ht);

   free(ht->buckets);
   free(ht);
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
HashTable_Clear(HashTable *ht) // IN/OUT
{
   int i;

   ASSERT(ht);

   ht->numElements = 0;

   for (i = 0; i < ht->numEntries; i++) {
      DblLnkLst_Links *head;
      DblLnkLst_Links *cur;
      DblLnkLst_Links *next = NULL;

      head = &ht->buckets[i];
      for (cur = head->next; cur != head; cur = next) {
         HashTableEntry *entry = DblLnkLst_Container(cur, HashTableEntry, l);

         ASSERT(entry);
         if (ht->freeEntryFn) {
            ht->freeEntryFn(entry->clientData);
         }
	 next = cur->next;
	 DblLnkLst_Unlink1(cur);
	 free(entry);
      }
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

static HashTableEntry *
HashTableLookup(HashTable *ht,      // IN
                const char *keyStr, // IN
                uint32 hash)        // IN
{
   DblLnkLst_Links *cur, *head;

   head = &ht->buckets[hash];
   for (cur = head->next; cur != head; cur = cur->next) {
      HashTableEntry *entry;

      entry = DblLnkLst_Container(cur, HashTableEntry, l);
      ASSERT(entry);

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
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Lookup(HashTable  *ht,      // IN
                 const char *keyStr,  // IN
                 void **clientData)   // OUT
{
   HashTableEntry *entry;
   uint32 hash = HashTableComputeHash(ht, keyStr);

   entry = HashTableLookup(ht, keyStr, hash);

   if (entry == NULL) {
      return FALSE;
   } else {
      if (clientData) {
         *clientData = entry->clientData;
      }

      return TRUE;
   }
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
 *	See above
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Delete(HashTable  *ht,        // IN/OUT: the hash table
                 const char *keyStr)    // IN: key for the element to remove
{
   DblLnkLst_Links *head, *cur, *next = NULL;
   uint32 hash = HashTableComputeHash(ht, keyStr);

   head = &ht->buckets[hash];
   for (cur = head->next; cur != head; cur = next) {
      HashTableEntry *entry = DblLnkLst_Container(cur, HashTableEntry, l);
      ASSERT(entry);

      next = cur->next;
      if (HashTableEqualKeys(ht, entry->keyStr, keyStr)) {
         if (ht->freeEntryFn) {
            ht->freeEntryFn(entry->clientData);
         }
         DblLnkLst_Unlink1(cur);
         free(DblLnkLst_Container(cur, HashTableEntry, l));
         ht->numElements--;
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
 *      Insert an element into the hashtable. The string key is
 *      not duplicated & thus cannot be free until there is no
 *      more need for the hashtable.
 *
 * Results:
 *      FALSE if item already exists.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

Bool
HashTable_Insert(HashTable  *ht,          // IN/OUT
                 const char *keyStr,      // IN
                 void       *clientData)  // IN
{
   HashTableEntry *entry;
   uint32 hash = HashTableComputeHash(ht, keyStr);

   if (HashTableLookup(ht, keyStr, hash) != NULL) {
      return FALSE;
   }

   entry = (HashTableEntry *) malloc(sizeof(HashTableEntry));
   ASSERT_MEM_ALLOC(entry);
   entry->keyStr = keyStr;
   entry->clientData = clientData;
   DblLnkLst_Init(&entry->l);

   DblLnkLst_LinkLast(&ht->buckets[hash], &entry->l);
   ht->numElements++;
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * HashTable_GetNumElements --
 *
 *      Get the number of elements in the hash table.
 *
 * Results:
 *      The number of elements.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

size_t
HashTable_GetNumElements(const HashTable *ht) // IN:
{
   return ht->numElements;
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
HashTable_ToArray(const HashTable *ht,  // IN
                  void ***clientDatas,  // OUT
                  size_t *size)         // OUT
{
   uint32 i;
   size_t j;

   ASSERT(ht);
   ASSERT(clientDatas);
   ASSERT(size);

   *clientDatas = NULL;
   *size = HashTable_GetNumElements(ht);

   if (0 == *size) {
      return;
   }

   /* alloc array */
   *clientDatas = malloc(*size * sizeof(void *));
   ASSERT_MEM_ALLOC(*clientDatas);

   /* fill array */
   for (i = 0, j = 0; i < ht->numEntries; i++) {
      DblLnkLst_Links *head;
      DblLnkLst_Links *cur;
      DblLnkLst_Links *next = NULL;

      head = &ht->buckets[i];
      for (cur = head->next; cur != head; cur = next) {
         HashTableEntry *entry = DblLnkLst_Container(cur, HashTableEntry, l);
         ASSERT(entry);

         (*clientDatas)[j++] = entry->clientData;
	 next = cur->next;
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

int
HashTable_ForEach(const HashTable *ht,           // IN
                  HashTableForEachCallback cb,   // IN
                  void *clientData)              // IN
{
   int i;

   ASSERT(ht);
   ASSERT(cb);

   for (i = 0; i < ht->numEntries; i++) {
      DblLnkLst_Links *head, *cur;

      head = &ht->buckets[i];
      for (cur = head->next; cur != head; cur = cur->next) {
         int result;
         HashTableEntry *entry = DblLnkLst_Container(cur, HashTableEntry, l);

         ASSERT(entry);
         result = (*cb)(entry->keyStr, entry->clientData, clientData);
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
 *	None.
 *
 *----------------------------------------------------------------------
 */

void
HashPrint(HashTable *ht) // IN
{
   int i;

   for (i = 0; i < ht->numEntries; i++) {
      DblLnkLst_Links *head, *cur;

      head = &ht->buckets[i];
      if (head->next == head) {
         continue;
      }

      printf("%4d: \n", i);

      for (cur = head->next; cur != head; cur = cur->next) {
         HashTableEntry *entry;

         entry = DblLnkLst_Container(cur, HashTableEntry, l);
         ASSERT(entry);

         if (ht->keyType == HASH_INT_KEY) {
            printf("\t%p\n", entry->keyStr);
         } else {
            printf("\t%s\n", entry->keyStr);
         }
      }
   }
}
#endif
