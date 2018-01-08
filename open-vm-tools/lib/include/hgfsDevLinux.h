/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
#define HGFS_FUSENAME "vmhgfs-fuse"     // Name of FS (e.g. "-o subtype=vmhgfs-fuse")
#define HGFS_FUSETYPE "fuse." HGFS_FUSENAME // Type of FS (e.g. "fuse.vmhgfs-fuse")
#define HGFS_MOUNT_POINT "/mnt/hgfs"    // Type of FS (e.g. vmhgfs-fuse )
#define HGFS_DEVICE_NAME "dev"          // Name of our device under /proc/fs/HGFS_NAME/
#define HGFS_SUPER_MAGIC 0xbacbacbc     // Superblock magic number
#define HGFS_DEFAULT_TTL 1              // Default TTL for dentries

typedef enum {
   HGFS_MOUNTINFO_VERSION_NONE,
   HGFS_MOUNTINFO_VERSION_1,
   HGFS_MOUNTINFO_VERSION_2,
} HgfsMountInfoVersion;

/*
 * The mount info flags.
 * These specify flags from options parsed on the mount command line.
 */
#define HGFS_MNTINFO_SERVER_INO         (1 << 0) /* Use server inode numbers? */

/*
 * Mount information, passed from pserver process to kernel
 * at mount time.
 *
 * XXX: I'm hijacking this struct. In the future, when the Solaris HGFS driver
 * loses its pserver, the struct will be used by /sbin/mount.vmhgfs solely.
 * As is, it is also used by the Solaris pserver.
 */
typedef
struct HgfsMountInfo {
   uint32 magicNumber;        // hgfs magic number
   uint32 infoSize;           // HgfsMountInfo structure size
   HgfsMountInfoVersion version; // HgfsMountInfo structure version
   uint32 fd;                 // file descriptor of client file
   uint32 flags;              // hgfs specific mount flags
#ifndef sun
   uid_t uid;                 // desired owner of files
   Bool uidSet;               // is the owner actually set?
   gid_t gid;                 // desired group of files
   Bool gidSet;               // is the group actually set?
   unsigned short fmask;      // desired file mask
   unsigned short dmask;      // desired directory mask
   uint32 ttl;                // number of seconds before revalidating dentries
#if defined __APPLE__
   char shareNameHost[MAXPATHLEN]; // must be ".host"
   char shareNameDir[MAXPATHLEN];  // desired share name for mounting
#else
   const char *shareNameHost; // must be ".host"
   const char *shareNameDir;  // desired share name for mounting
#endif
#endif
}
#if __GNUC__
__attribute__((__packed__))
#else
#   error Compiler packing...
#endif
HgfsMountInfo;

/*
 * Version 1 of the MountInfo object.
 * This is used so that newer kernel clients can allow mounts using
 * older versions of the mounter application for backwards compatibility.
 */
typedef
struct HgfsMountInfoV1 {
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
#if defined __APPLE__
   char shareNameHost[MAXPATHLEN]; // must be ".host"
   char shareNameDir[MAXPATHLEN];  // desired share name for mounting
#else
   const char *shareNameHost; // must be ".host"
   const char *shareNameDir;  // desired share name for mounting
#endif
#endif
} HgfsMountInfoV1;

#endif //ifndef _HGFS_DEV_H_
