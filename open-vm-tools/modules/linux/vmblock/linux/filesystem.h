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
 * filesystem.h --
 *
 *  Definitions and prototypes for file system portion of vmblock driver.
 *
 *  There are currently two classes of files in the blocking file system: the
 *  root directory and symlinks to actual files on the file system.  The root
 *  directory provides a way to lookup directory entries in the directory we
 *  are redirecting to; each of these directory entries is presented as
 *  a symlink.  These symlinks within the root directory contain the path of
 *  the actual file and will block any time the inode is accessed or dentry is
 *  revalidated (if there is a pending block).  This blocking ensures that any
 *  access to the file through the symlink will not proceed until the block is
 *  lifted.
 *
 *  Operation tables for the root directory and symlinks are are named Root*Ops
 *  and Link*Ops respectively.  All operations are preceded by their operation
 *  type (e.g., the file_operation table's open is named FileOpOpen and the
 *  inode_operation table's lookup is named InodeOpLookup).
 *
 *  The use of symlinks greatly simplifies the driver's implementation but also
 *  limits blocking to a depth of one level within the redirected directory
 *  (since after the symlink is followed all operations are passed on to the
 *  actual file system and are out of our control).  This limitation is fine
 *  under the current use of this driver.
 */

#ifndef __FILESYSTEM_H__
#define __FILESYSTEM_H__

#include "compat_slab.h"
#include <linux/fs.h>

#include "vm_basic_types.h"

#define INODE_TO_IINFO(_inode)          container_of(_inode, VMBlockInodeInfo, inode)
#define INODE_TO_ACTUALDENTRY(inode)    INODE_TO_IINFO(inode)->actualDentry
#define INODE_TO_ACTUALINODE(inode)     INODE_TO_IINFO(inode)->actualDentry->d_inode

#define VMBLOCK_SUPER_MAGIC 0xabababab

typedef struct VMBlockInodeInfo {
   char name[PATH_MAX];
   size_t nameLen;
   struct dentry *actualDentry;
   /* Embedded inode */
   struct inode inode;
} VMBlockInodeInfo;


ino_t GetNextIno(void);
struct inode *Iget(struct super_block *sb, struct inode *dir,
                   struct dentry *dentry, ino_t ino);
int MakeFullName(struct inode *dir, struct dentry *dentry,
                  char *bufOut, size_t bufOutSize);
void VMBlockReadInode(struct inode *inode);

/* Variables */
extern compat_kmem_cache *VMBlockInodeCache;
/* File system wide superblock operations */
extern struct super_operations VMBlockSuperOps;
/* File operations on fs's root inode to read directory entries. */
extern struct file_operations RootFileOps;
/* Inode operations to lookup inodes of directory entries in fs's root inode. */
extern struct inode_operations RootInodeOps;
/* Dentry operations for our symlinks to actual files (to enable blocking). */
extern struct dentry_operations LinkDentryOps;

#endif /* __FILESYSTEM_H__ */
