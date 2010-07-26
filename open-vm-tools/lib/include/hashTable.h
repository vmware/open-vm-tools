/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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

#include "vm_atomic.h"


typedef struct HashTable HashTable;
typedef void (*HashTableFreeEntryFn)(void *clientData);
typedef int (*HashTableForEachCallback)(const char *key, void *value, 
                                        void *clientData);

#define HASH_STRING_KEY		0	// case-sensitive string key
#define HASH_ISTRING_KEY	1	// case-insensitive string key
#define HASH_INT_KEY		2	// uintptr_t or pointer key

/*
 * The flag bits are ored into the type field.
 * Atomic hash tables only support insert, lookup, and replace.
 */

#define HASH_TYPE_MASK		7
#define HASH_FLAG_MASK		(~HASH_TYPE_MASK)
#define HASH_FLAG_ATOMIC	0x08	// thread-safe hash table
#define HASH_FLAG_COPYKEY	0x10	// copy string key

HashTable *
HashTable_Alloc(uint32               numEntries,
                int                  keyType,
                HashTableFreeEntryFn fn);

HashTable *
HashTable_AllocOnce(Atomic_Ptr          *var,
                    uint32               numEntries,
                    int                  keyType,
                    HashTableFreeEntryFn fn);

void
HashTable_Free(HashTable *hashTable);

void
HashTable_FreeUnsafe(HashTable *hashTable);

Bool
HashTable_Insert(HashTable  *hashTable,
                 const void *keyStr,
                 void       *clientData);

Bool
HashTable_Lookup(HashTable  *hashTable,
                 const void *keyStr,
                 void      **clientData);

void *
HashTable_LookupOrInsert(HashTable  *hashTable,
                         const void *keyStr,
                         void       *clientData);

Bool
HashTable_ReplaceOrInsert(HashTable  *hashTable,
                          const void *keyStr,
                          void       *clientData);

Bool
HashTable_ReplaceIfEqual(HashTable  *hashTable,
                         const void *keyStr,
                         void       *oldClientData,
                         void       *newClientData);

Bool
HashTable_Delete(HashTable  *hashTable,
                 const void *keyStr);

Bool
HashTable_LookupAndDelete(HashTable  *hashTable,
                          const void *keyStr,
			  void      **clientData);

void
HashTable_Clear(HashTable *ht);

void
HashTable_ToArray(const HashTable   *ht,
                  void            ***clientDatas,
                  size_t            *size);

void
HashTable_KeyArray(const HashTable   *ht,
                   const void      ***keys,
                   size_t            *size);

size_t
HashTable_GetNumElements(const HashTable *ht);

int
HashTable_ForEach(const HashTable          *ht,
                  HashTableForEachCallback  cb,
                  void                     *clientData);

#endif
