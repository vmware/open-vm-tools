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
 * fsutil.h --
 *
 * Functions used in more than one type of filesystem operation will be
 * exported from this file.
 */

#ifndef _HGFS_DRIVER_FSUTIL_H_
#define _HGFS_DRIVER_FSUTIL_H_


#include "request.h"
#include "vm_basic_types.h"
#include "hgfsProto.h"
#include <fuse.h>

#if defined(__FreeBSD__) || defined(__SOLARIS__) || defined(__APPLE__)
typedef long long loff_t;
#endif

/*
 * Struct used to pass around attributes.
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
   char *fileName;                 /* Either symlink target or filename */
} HgfsAttrInfo;

int
HgfsClearReadOnly(const char* path,
                  HgfsAttrInfo *enableWrite);

int
HgfsRestoreReadOnly(const char* path,
                    HgfsAttrInfo *enableWrite);

ssize_t
HgfsWrite(struct fuse_file_info *fi,
          const char  *buf,
          size_t count,
          loff_t offset);

int
HgfsRename(const char* from, const char* to);

/* HGFS file operations for files. */

int
HgfsOpen(const char *path,
         struct fuse_file_info *fi);

int
HgfsCreate(const char *path,
           mode_t permsMode,
           struct fuse_file_info *fi);

ssize_t
HgfsRead(struct fuse_file_info *fi,
         char  *buf,
         size_t count,
         loff_t offset);

int
HgfsSetattr(const char* path,
            HgfsAttrInfo *attr);


int HgfsUnpackCommonAttr(void *rawAttr,
                         HgfsOp requestType,
                         HgfsAttrInfo *attrInfo);

int
HgfsPrivateGetattr(HgfsHandle handle,
                   const char* path,
                   HgfsAttrInfo *attr);

int
HgfsStatusConvertToLinux(HgfsStatus hgfsStatus);

int
HgfsGetOpenMode(uint32 flags);


int
HgfsDirOpen(const char* path, HgfsHandle* handle);

int
HgfsReaddir(HgfsHandle handle,
            void *dirent,
            fuse_fill_dir_t filldir);

int
HgfsMkdir(const char *path,
          int mode);

int
HgfsDelete(const char* path,
           HgfsOp op);

int
HgfsSymlink(const char* source,
            const char *symname);

void
HgfsResetOps(void);

unsigned long
HgfsCalcBlockSize(uint64 tsize);

#endif // _HGFS_DRIVER_FSUTIL_H_
