/*********************************************************
 * Copyright (C) 2006-2013 VMware, Inc. All rights reserved.
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
 * vmciHashtable.h --
 *
 *    Hash table for use in the APIs.
 */

#ifndef _VMCI_HASHTABLE_H_
#define _VMCI_HASHTABLE_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include "vmci_kernel_if.h"
#include "vmci_defs.h"

typedef struct VMCIHashEntry {
   VMCIHandle            handle;
   int                   refCount;
   struct VMCIHashEntry *next;
} VMCIHashEntry;

typedef struct VMCIHashTable {
   VMCIHashEntry **entries;
   int             size; // Number of buckets in above array.
   VMCILock        lock;
} VMCIHashTable;

VMCIHashTable *VMCIHashTable_Create(int size);
void VMCIHashTable_Destroy(VMCIHashTable *table);
void VMCIHashTable_InitEntry(VMCIHashEntry *entry, VMCIHandle handle);
int VMCIHashTable_AddEntry(VMCIHashTable *table, VMCIHashEntry *entry);
int VMCIHashTable_RemoveEntry(VMCIHashTable *table, VMCIHashEntry *entry);
VMCIHashEntry *VMCIHashTable_GetEntry(VMCIHashTable *table, VMCIHandle handle);
void VMCIHashTable_HoldEntry(VMCIHashTable *table, VMCIHashEntry *entry);
int VMCIHashTable_ReleaseEntry(VMCIHashTable *table, VMCIHashEntry *entry);
Bool VMCIHashTable_EntryExists(VMCIHashTable *table, VMCIHandle handle);
void VMCIHashTable_Sync(VMCIHashTable *table);

#endif // _VMCI_HASHTABLE_H_
