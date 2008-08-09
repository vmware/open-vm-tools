/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * hgfsDev.h --
 * 
 *    Header for code shared between the hgfs linux kernel module driver
 *    and the pserver.
 */

#ifndef _HGFS_DEV_H_
#define _HGFS_DEV_H_

#include "vm_basic_types.h"
#include "hgfs.h"

#define HGFS_NAME "vmhgfs"              // Name of FS (e.g. "mount -t vmhgfs")
#define HGFS_DEVICE_NAME "dev"          // Name of our device under /proc/fs/HGFS_NAME/
#define HGFS_SUPER_MAGIC 0xbacbacbc     // Superblock magic number
#define HGFS_PROTOCOL_VERSION 1         // Incremented when something changes
#define HGFS_DEFAULT_TTL 1              // Default TTL for dentries

/* 
 * Mount information, passed from pserver process to kernel
 * at mount time.
 *
 * XXX: I'm hijacking this struct. In the future, when the Solaris HGFS driver
 * loses its pserver, the struct will be used by /sbin/mount.vmhgfs solely.
 * As is, it is also used by the Solaris pserver.
 */
typedef struct HgfsMountInfo {
   uint32 magicNumber;        // hgfs magic number
   uint32 version;            // protocol version
   uint32 fd;                 // file descriptor of client file
#ifndef sun
   uid_t uid;                 // desired owner of files
   Bool uidSet;               // is the owner actually set?
   gid_t gid;                 // desired group of files
   Bool gidSet;               // is the group actually set?
   unsigned short fmask;      // desired file mask
   unsigned short dmask;      // desired directory mask
   uint32 ttl;                // number of seconds before revalidating dentries
   const char *shareNameHost; // must be ".host"
   const char *shareNameDir;  // desired share name for mounting
#endif
} HgfsMountInfo;

#endif //ifndef _HGFS_DEV_H_
