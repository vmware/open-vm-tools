/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
int HgfsInstantiate(struct dentry *dentry,
                    ino_t ino,
                    HgfsAttrInfo const *attr);
int HgfsBuildPath(char *buffer,
                  size_t bufferLen,
                  struct dentry *dentry);
void HgfsDentryAgeReset(struct dentry *dentry);
void HgfsDentryAgeForce(struct dentry *dentry);
int HgfsGetOpenMode(uint32 flags);
int HgfsCreateFileInfo(struct file *file,
                       HgfsHandle handle);
void HgfsReleaseFileInfo(struct file *file);
int HgfsGetHandle(struct inode *inode,
                  HgfsOpenMode mode,
                  HgfsHandle *handle);
int HgfsStatusConvertToLinux(HgfsStatus hgfsStatus);
void HgfsSetUidGid(struct inode *parent,
                   struct dentry *dentry,
                   uid_t uid,
                   gid_t gid);
struct inode *HgfsGetInode(struct super_block *sb, ino_t ino);
void HgfsDoReadInode(struct inode *inode);


#endif // _HGFS_DRIVER_FSUTIL_H_
