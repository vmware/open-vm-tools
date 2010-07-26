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
 * module.h --
 *
 * Global module definitions for the entire vmhgfs driver.
 */

#ifndef _HGFS_DRIVER_MODULE_H_
#define _HGFS_DRIVER_MODULE_H_

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <asm/atomic.h>
#include "compat_fs.h"
#include "compat_semaphore.h"
#include "compat_slab.h"
#include "compat_spinlock.h"
#include "compat_version.h"

#include "rpcout.h"
#include "hgfsProto.h"

#ifndef __user
#define __user
#endif

/* Logging stuff. */
#ifdef VMX86_DEVEL
extern int LOGLEVEL_THRESHOLD;

#define LOG(level, args) ((void) (LOGLEVEL_THRESHOLD >= (level) ? (printk args) : 0))
#else
#define LOG(level, args)
#endif

/* Blocksize to be set in superblock. (XXX how is this used?) */
#define HGFS_BLOCKSIZE 1024

/* The amount of time we'll wait for the backdoor to process our request. */
#define HGFS_REQUEST_TIMEOUT (30 * HZ)

/*
 * Inode number of the root inode. We set this to be non-zero because,
 * according to glibc source, when the returned inode number in a dirent
 * is zero, that entry has been deleted. This is presumably when you've done
 * an opendir, the file is deleted, and then you do a readdir. The point is
 * that if the root inode is zero, aliases to it (such as '.' and "..") won't
 * appear in a directory listing.
 */
#define HGFS_ROOT_INO 1

/* Leave HGFS_ROOT_INO and below out of inode number generation. */
#define HGFS_RESERVED_INO HGFS_ROOT_INO + 1

/*
 * Macros for accessing members that are private to this code in
 * sb/inode/file structs.
 */
#define HGFS_SET_SB_TO_COMMON(sb, common) do { (sb)->s_fs_info = (common); } while (0)
#define HGFS_SB_TO_COMMON(sb)             ((HgfsSuperInfo *)(sb)->s_fs_info)

#define INODE_GET_II_P(_inode) container_of(_inode, HgfsInodeInfo, inode)

#if defined VMW_INODE_2618
#define INODE_SET_II_P(inode, info) do { (inode)->i_private = (info); } while (0)
#else
#define INODE_SET_II_P(inode, info) do { (inode)->u.generic_ip = (info); } while (0)
#endif

#define HGFS_DECLARE_TIME(unixtm) struct timespec unixtm
#define HGFS_EQUAL_TIME(unixtm1, unixtm2) timespec_equal(&unixtm1, &unixtm2)
#define HGFS_SET_TIME(unixtm,nttime) HgfsConvertFromNtTimeNsec(&unixtm, nttime)
#define HGFS_GET_TIME(unixtm) HgfsConvertTimeSpecToNtTime(&unixtm)
#define HGFS_GET_CURRENT_TIME() ({                                     \
                                    struct timespec ct = CURRENT_TIME; \
                                    HGFS_GET_TIME(ct);                 \
                                 })

/*
 * Beware! This macro returns list of two elements. Do not add braces around.
 */
#define HGFS_PRINT_TIME(unixtm) unixtm.tv_sec, unixtm.tv_nsec

/*
 * For files opened in our actual Host/Guest filesystem, the
 * file->private_data field is used for storing the HgfsFileInfo of the
 * opened file. This macro is for accessing the file information from the
 * file *.
 */
#define FILE_SET_FI_P(file, info) do { (file)->private_data = info; } while (0)
#define FILE_GET_FI_P(file)         ((HgfsFileInfo *)(file)->private_data)

/* Data kept in each superblock in sb->u. */
typedef struct HgfsSuperInfo {
   uid_t uid;                       /* UID of user who mounted this fs. */
   Bool uidSet;                     /* Was the UID specified at mount-time? */
   gid_t gid;                       /* GID of user who mounted this fs. */
   Bool gidSet;                     /* Was the GID specified at mount-time? */
   mode_t fmask;                    /* File permission mask. */
   mode_t dmask;                    /* Directory permission mask. */
   uint32 ttl;                      /* Maximum dentry age (in ticks). */
   char *shareName;                 /* Mounted share name. */
   size_t shareNameLen;             /* To avoid repeated strlen() calls. */
} HgfsSuperInfo;

/*
 * HGFS specific per-inode data.
 */
typedef struct HgfsInodeInfo {
   /* Embedded inode. */
   struct inode inode;

   /* Inode number given by the host. */
   uint64 hostFileId;

   /* Was the inode number for this inode generated via iunique()? */
   Bool isFakeInodeNumber;

   /* Is this a fake inode created in HgfsCreate that has yet to be opened? */
   Bool createdAndUnopened;

   /* Is this inode referenced by HGFS? (needed by HgfsInodeLookup()) */
   Bool isReferencedInode;

   /* List of open files for this inode. */
   struct list_head files;
} HgfsInodeInfo;

/*
 * HGFS specific per-file data.
 */
typedef struct HgfsFileInfo {

   /* Links to place this object on the inode's list of open files. */
   struct list_head list;

   /* Handle to be sent to the server. Needed for writepage(). */
   HgfsHandle handle;

   /*
    * Mode with which handle was opened. When we reuse a handle, we need to
    * choose one with appropriate permissions.
    */
   HgfsOpenMode mode;

   /*
    * Do we need to reopen a directory ? Note that this is only used
    * for directories.
    */
   Bool isStale;

} HgfsFileInfo;


/*
 * Global synchronization primitives.
 */

/*
 * We use hgfsBigLock to protect certain global structures that are locked for
 * a very short amount of time.
 */
extern spinlock_t hgfsBigLock;

/* Hgfs filesystem structs. */
extern struct super_operations HgfsSuperOperations;
extern struct dentry_operations HgfsDentryOperations;
extern struct inode_operations HgfsFileInodeOperations;
extern struct inode_operations HgfsDirInodeOperations;
extern struct inode_operations HgfsLinkInodeOperations;
extern struct file_operations HgfsFileFileOperations;
extern struct file_operations HgfsDirFileOperations;
extern struct address_space_operations HgfsAddressSpaceOperations;

/* Other global state. */
extern compat_kmem_cache *hgfsInodeCache;

extern HgfsOp hgfsVersionOpen;
extern HgfsOp hgfsVersionRead;
extern HgfsOp hgfsVersionWrite;
extern HgfsOp hgfsVersionClose;
extern HgfsOp hgfsVersionSearchOpen;
extern HgfsOp hgfsVersionSearchRead;
extern HgfsOp hgfsVersionSearchClose;
extern HgfsOp hgfsVersionGetattr;
extern HgfsOp hgfsVersionSetattr;
extern HgfsOp hgfsVersionCreateDir;
extern HgfsOp hgfsVersionDeleteFile;
extern HgfsOp hgfsVersionDeleteDir;
extern HgfsOp hgfsVersionRename;
extern HgfsOp hgfsVersionQueryVolumeInfo;
extern HgfsOp hgfsVersionCreateSymlink;

#endif // _HGFS_DRIVER_MODULE_H_
