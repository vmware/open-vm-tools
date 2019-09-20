/*********************************************************
 * Copyright (C) 2004-2017,2019 VMware, Inc. All rights reserved.
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
 * hashTable.h --
 *
 *      Hash table.
 */

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_defs.h"
#include "vm_atomic.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct HashTable HashTable;
typedef struct PtrHashTable PtrHashTable;

typedef void (*HashTableFreeEntryFn)(void *clientData);

typedef int (*HashTableForEachCallback)(const char *key,
                                        void *value,
                                        void *clientData);

#define HASH_STRING_KEY         0       // case-sensitive string key
#define HASH_ISTRING_KEY        1       // case-insensitive string key
#define HASH_INT_KEY            2       // uintptr_t or pointer key

/*
 * The flag bits are ored into the type field.
 * Atomic hash tables only support insert, lookup, and replace.
 */

#define HASH_TYPE_MASK          7
#define HASH_FLAG_MASK          (~HASH_TYPE_MASK)
#define HASH_FLAG_ATOMIC        0x08    // thread-safe hash table
#define HASH_FLAG_COPYKEY       0x10    // copy string key

HashTable *
HashTable_Alloc(uint32               numEntries,  // IN:
                int                  keyType,     // IN:
                HashTableFreeEntryFn fn);         // IN/OPT:

HashTable *
HashTable_AllocOnce(Atomic_Ptr          *var,         // IN/OUT:
                    uint32               numEntries,  // IN:
                    int                  keyType,     // IN:
                    HashTableFreeEntryFn fn);         // IN/OPT:

void
HashTable_Free(HashTable *hashTable);  // IN/OUT:

void
HashTable_FreeUnsafe(HashTable *hashTable);  // IN/OUT:

Bool
HashTable_Insert(HashTable  *hashTable,    // IN/OUT:
                 const void *keyStr,       // IN:
                 void       *clientData);  // IN/OPT:

Bool
HashTable_Lookup(const HashTable  *hashTable,    // IN:
                 const void       *keyStr,       // IN:
                 void            **clientData);  // OUT/OPT:

void *
HashTable_LookupOrInsert(HashTable  *hashTable,    // IN/OUT:
                         const void *keyStr,       // IN:
                         void       *clientData);  // IN/OPT:

Bool
HashTable_ReplaceOrInsert(HashTable  *hashTable,    // IN/OUT:
                          const void *keyStr,       // IN:
                          void       *clientData);  // IN/OPT

Bool
HashTable_ReplaceIfEqual(HashTable  *hashTable,       // IN/OUT:
                         const void *keyStr,          // IN:
                         void       *oldClientData,   // IN/OPT
                         void       *newClientData);  // IN/OPT

Bool
HashTable_Delete(HashTable  *hashTable,  // IN/OUT:
                 const void *keyStr);    // IN:

Bool
HashTable_LookupAndDelete(HashTable  *hashTable,    // IN/OUT:
                          const void *keyStr,       // IN:
                          void      **clientData);  // OUT:

void
HashTable_Clear(HashTable *ht);  // IN/OUT:

void
HashTable_ToArray(const HashTable   *ht,           // IN:
                  void            ***clientDatas,  // OUT:
                  size_t            *size);        // OUT:

void
HashTable_KeyArray(const HashTable   *ht,     // IN:
                   const void      ***keys,   // OUT:
                   size_t            *size);  // OUT:

size_t
HashTable_GetNumElements(const HashTable *ht);  // IN:

int
HashTable_ForEach(const HashTable          *ht,           // IN:
                  HashTableForEachCallback  cb,           // IN:
                  void                     *clientData);  // IN:

/*
 * Specialize hash table that uses the callers data structure as its
 * hash entry as well, the hash key being an address that must be unique.
 */

typedef struct PtrHashEntry {
   struct PtrHashEntry  *next;
   void                 *ptr;
} PtrHashEntry;

/*
 * PTRHASH_CONTAINER - get the struct for this entry (like PtrHashEntry)
 * @ptr: the &struct PtrHashEntry pointer.
 * @type:   the type of the struct this is embedded in.
 * @member: the name of the list struct within the struct.
 */

#define PTRHASH_CONTAINER(ptr, type, member) \
   ((type *)((char *)(ptr) - offsetof(type, member)))

typedef int (*PtrHashForEachCallback)(PtrHashEntry *entry,
                                      const void *clientData);

PtrHashTable *PtrHash_Alloc(uint32 numBuckets);

void PtrHash_Free(PtrHashTable *hashTable);

size_t PtrHash_AllocSize(PtrHashTable *ht);

size_t PtrHash_GetNumElements(const PtrHashTable *ht);

int PtrHash_ForEach(const PtrHashTable *ht,
                    PtrHashForEachCallback cb,
                    const void *clientData);

PtrHashEntry *PtrHash_Lookup(const PtrHashTable *hashTable,
                             const void *keyPtr);

PtrHashEntry *PtrHash_LookupAndDelete(PtrHashTable *hashTable,
                                      const void *keyPtr);

Bool PtrHash_Insert(PtrHashTable *hashTable,
                    PtrHashEntry *entry);

Bool PtrHash_Delete(PtrHashTable *hashTable,
                    const void *keyPtr);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
