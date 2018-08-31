/*********************************************************
 * Copyright (C) 2009-2018 VMware, Inc. All rights reserved.
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

#ifndef _HASHMAP_H_
#define _HASHMAP_H_

#include <vmware.h>
#ifdef VMX86_SERVER
#include "aioMgr.h"
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct HashMap HashMap;

/*
 * ----------------------------------------------------------------------------
 *
 * HashMapIteratorFn --
 *
 *    A function pointer definition that should be passed to HashMap_Iterate,
 *    it will be called for all entries in the hashmap.  The key and data will
 *    be set appropriately and the user data pointer will be passed untouched
 *    from the HashMap_Iterate call.
 *
 * Results:
 *    void
 *
 * Side Effects:
 *    Implementation dependent.
 *
 * ----------------------------------------------------------------------------
 */

typedef void (* HashMapIteratorFn)(void *key, void *data, void *userData);

HashMap *HashMap_AllocMap(uint32 numEntries, size_t keySize, size_t dataSize);
HashMap *HashMap_AllocMapAlpha(uint32 numEntries, uint32 alpha, size_t keySize,
                               size_t dataSize);
void HashMap_DestroyMap(HashMap *map);
Bool HashMap_Put(HashMap *map, const void *key, const void *data);
void *HashMap_Get(HashMap *map, const void *key);
void *HashMap_ConstTimeGet(struct HashMap *map, const void *key);
void HashMap_Clear(HashMap *map);
Bool HashMap_Remove(HashMap *map, const void *key);
uint32 HashMap_Count(HashMap *map);
void HashMap_Iterate(HashMap* map, HashMapIteratorFn mapFn, Bool clear,
      void *userData);
Bool HashMap_DoTests(void);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _HASHMAP_H_ */
