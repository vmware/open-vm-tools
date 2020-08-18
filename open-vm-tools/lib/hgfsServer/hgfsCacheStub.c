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
 * hgfsCacheStub.c --
 *
 *    This file contains the stub implementation for hgfs cache.
 */

#include "hgfsCache.h"
#include "hgfsServerInt.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCache_Alloc --
 *
 *      Create a cache and the corresponding hash table/doubly linked list/lock.
 *
 * Results:
 *      Always return NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

HgfsCache *
HgfsCache_Alloc(HgfsCacheRemoveLRUCallback callback) // IN
{
   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCache_Destroy --
 *
 *      Destroy a cache and the corresponding hash table/doubly linked list/lock.
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
HgfsCache_Destroy(HgfsCache *cache)                    // IN
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCache_Put --
 *
 *      Put an entry into a cache.
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
HgfsCache_Put(HgfsCache *cache,                    // IN
              const char *key,                     // IN
              void *data)                          // IN
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCache_Get --
 *
 *      Get an entry in a cache.
 *
 * Results:
 *      Always return FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsCache_Get(HgfsCache *cache, // IN
              const char *key,  // IN
              void **data)      // OUT
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCache_Invalidate --
 *
 *      Remove an entry from a cache.
 *
 * Results:
 *      Always return FALSE.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsCache_Invalidate(HgfsCache *cache, // IN
                     const char *key)  // IN
{
   return FALSE;
}
