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
 * hashTable.h --
 *
 *      Hash table.
 */

#ifndef _HASH_TABLE_H_
#define _HASH_TABLE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"


typedef struct HashTable HashTable;
typedef void (*HashTableFreeEntryFn)(void *clientData);
typedef int (*HashTableForEachCallback)(const char *key, void *value, 
                                        void *clientData);

#define HASH_STRING_KEY		0	// case-sensitive string key
#define HASH_ISTRING_KEY	1	// case-insensitive string key
#define HASH_INT_KEY		2	// uintptr_t or pointer key

HashTable *
HashTable_Alloc(uint32 numEntries, int keyType, HashTableFreeEntryFn fn);

void
HashTable_Free(HashTable *hashTable);

Bool
HashTable_Insert(HashTable  *hashTable,
                 const char *keyStr,
                 void       *clientData);

Bool
HashTable_Lookup(HashTable  *hashTable,
                 const char *keyStr,
                 void **clientData);

Bool
HashTable_Delete(HashTable  *hashTable,
                 const char *keyStr);

void
HashTable_Clear(HashTable *ht);

void
HashTable_ToArray(const HashTable *ht,
                  void ***clientDatas,
                  size_t *size);

size_t
HashTable_GetNumElements(const HashTable *ht);

int
HashTable_ForEach(const HashTable *ht,
                  HashTableForEachCallback cb,
                  void *clientData);

#endif
