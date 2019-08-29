/*********************************************************
 * Copyright (C) 2013,2019 VMware, Inc. All rights reserved.
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
 * filesystem.h --
 *
 * High-level filesystem operations for the filesystem portion of
 * the vmhgfs driver.
 */

#ifndef _HGFS_DRIVER_FILESYSTEM_H_
#define _HGFS_DRIVER_FILESYSTEM_H_

#define G_LOG_DOMAIN "vmhgfs-fuse"

#include "vm_basic_types.h"
#include <sys/statvfs.h>
#include "vmware/tools/utils.h"
#include "vmware/tools/log.h"

typedef struct HgfsFuseState {
   Bool sessionEnabled;
   uint64 sessionId;
   uint8 headerVersion;
   uint32 maxPacketSize;
   /*
    * When mount a subdirectory of hgfs shared directory, basePath holds
    * the prefix to the root. e.g. 'mount.vmhgfs .host:/shared/sub /hgfs',
    * base path would be '/shared/sub' (trailing '/' will be removed).
    */
   char *basePath;
   size_t basePathLen;

   GKeyFile *conf;

} HgfsFuseState;

/* Public functions (with respect to the entire module). */
int HgfsStatfs(const char *path, struct statvfs *stat);

#endif // _HGFS_DRIVER_FILESYSTEM_H_
