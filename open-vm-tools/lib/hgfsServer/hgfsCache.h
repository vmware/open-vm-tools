/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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
 * hgfsCache.h --
 *
 *    A customized LRU cache which is built by combining two data structures:
 *    a doubly linked list and a hash table.
 */

#ifndef _HGFS_CACHE_H_
#define _HGFS_CACHE_H_

#include "dbllnklst.h"
#include "userlock.h"

typedef void(*HgfsCacheRemoveLRUCallback)(void *data);

typedef struct HgfsCache {
   void *hashTable;
   DblLnkLst_Links links;
   MXUserExclLock *lock;
   HgfsCacheRemoveLRUCallback callback;
} HgfsCache;

HgfsCache *HgfsCache_Alloc(HgfsCacheRemoveLRUCallback callback);
void HgfsCache_Destroy(HgfsCache *cache);
void HgfsCache_Put(HgfsCache *cache, const char *key, void *data);
Bool HgfsCache_Get(HgfsCache *cache, const char *key, void **data);
Bool HgfsCache_Invalidate(HgfsCache *cache, const char *key);

#endif // ifndef _HGFS_CACHE_H_
