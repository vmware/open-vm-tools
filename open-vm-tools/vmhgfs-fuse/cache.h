/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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
 * cache.h --
 *
 * Declarations of cache management functions
 */

#ifndef _HGFS_DRIVER_CACHE_H_
#define _HGFS_DRIVER_CACHE_H_

int HgfsGetAttrCache(const char* path, HgfsAttrInfo *attr);
int HgfsSetAttrCache(const char* path, HgfsAttrInfo *attr);
void HgfsInitCache();
void* HgfsPurgeCache(void*);
void HgfsInvalidateAttrCache(const char* path);

#endif
