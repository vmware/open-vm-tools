/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * fsutil.h --
 *
 * Functions used in more than one type of filesystem operation will be
 * exported from this file.
 */

#ifndef _HGFS_DRIVER_FSUTIL_H_
#define _HGFS_DRIVER_FSUTIL_H_

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/signal.h>
#include "compat_fs.h"

#include "module.h"                /* For kuid_t kgid_t types. */
#include "inode.h"
#include "request.h"
#include "vm_basic_types.h"
#include "hgfsProto.h"

/*
 * Struct used to pass around attributes that Linux cares about.
 * These aren't just the attributes seen in HgfsAttr[V2]; we add a filename
 * pointer for convenience (used by SearchRead and Getattr).
 */
typedef struct HgfsAttrInfo {
   HgfsOp requestType;
   HgfsAttrValid mask;
   HgfsFileType type;              /* File type */
   uint64 allocSize;               /* Disk allocation size (in bytes) */
   uint64 size;                    /* File size (in bytes) */
   uint64 accessTime;              /* Time of last access */
   uint64 writeTime;               /* Time of last write */
   uint64 attrChangeTime;          /* Time file attributes were last changed */
   HgfsPermissions specialPerms;   /* Special permissions bits */
   HgfsPermissions ownerPerms;     /* Owner permissions bits */
   HgfsPermissions groupPerms;     /* Group permissions bits */
   HgfsPermissions otherPerms;     /* Other permissions bits */
   HgfsPermissions effectivePerms; /* Permissions in effect for the user on the
                                      host. */
   uint32 userId;                  /* UID */
   uint32 groupId;                 /* GID */
   uint64 hostFileId;              /* Inode number */
} HgfsAttrInfo;


/*
 * ino_t is 32-bits on 32-bit arch. We have to squash the 64-bit value down
 * so that it will fit.
 * Note, this is taken from CIFS so we apply the same algorithm.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
/*
 * We use hash_64 to convert the value to 31 bits, and
 * then add 1, to ensure that we don't end up with a 0 as the value.
 */
#if BITS_PER_LONG == 64
static inline ino_t
HgfsUniqueidToIno(uint64 fileid)
{
   return (ino_t)fileid;
}
#else
#include <linux/hash.h>

static inline ino_t
HgfsUniqueidToIno(uint64 fileid)
{
   return (ino_t)hash_64(fileid, (sizeof(ino_t) * 8) - 1) + 1;
}
#endif

#else
static inline ino_t
HgfsUniqueidToIno(uint64 fileid)
{
   ino_t ino = (ino_t) fileid;
   if (sizeof(ino_t) < sizeof(uint64)) {
      ino ^= fileid >> (sizeof(uint64)-sizeof(ino_t)) * 8;
   }
   return ino;
}
#endif

/* Public functions (with respect to the entire module). */
int HgfsUnpackCommonAttr(HgfsReq *req,
                         HgfsAttrInfo *attr);
void HgfsChangeFileAttributes(struct inode *inode,
                              HgfsAttrInfo const *attr);
int HgfsPrivateGetattr(struct dentry *dentry,
                       HgfsAttrInfo *attr,
                       char **fileName);
struct inode *HgfsIget(struct super_block *sb,
                       ino_t ino,
                       HgfsAttrInfo const *attr);
int HgfsInstantiateRoot(struct super_block *sb,
                        struct dentry **rootDentry);
int HgfsInstantiate(struct dentry *dentry,
                    ino_t ino,
                    HgfsAttrInfo const *attr);
int HgfsBuildPath(char *buffer,
                  size_t bufferLen,
                  struct dentry *dentry);
void HgfsDentryAgeReset(struct dentry *dentry);
void HgfsDentryAgeForce(struct dentry *dentry);
int HgfsGetOpenMode(uint32 flags);
int HgfsGetOpenFlags(uint32 flags);
int HgfsCreateFileInfo(struct file *file,
                       HgfsHandle handle);
void HgfsReleaseFileInfo(struct file *file);
int HgfsGetHandle(struct inode *inode,
                  HgfsOpenMode mode,
                  HgfsHandle *handle);
int HgfsStatusConvertToLinux(HgfsStatus hgfsStatus);
void HgfsSetUidGid(struct inode *parent,
                   struct dentry *dentry,
                   kuid_t uid,
                   kgid_t gid);


#endif // _HGFS_DRIVER_FSUTIL_H_
