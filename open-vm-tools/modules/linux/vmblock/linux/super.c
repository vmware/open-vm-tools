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
 * super.c --
 *
 *   Super operations for the file system portion of the vmblock driver.
 *
 */

#include "driver-config.h"
#include "compat_fs.h"
#include "compat_statfs.h"

#include "vmblockInt.h"
#include "filesystem.h"

/* Super block operations */
#ifdef VMW_EMBED_INODE
static struct inode *SuperOpAllocInode(struct super_block *sb);
static void SuperOpDestroyInode(struct inode *inode);
#else
static void SuperOpClearInode(struct inode *inode);
#endif
static void SuperOpReadInode(struct inode *inode);
#ifdef VMW_STATFS_2618
static int SuperOpStatfs(struct dentry *dentry, struct compat_kstatfs *stat);
#else
static int SuperOpStatfs(struct super_block *sb, struct compat_kstatfs *stat);
#endif


struct super_operations VMBlockSuperOps = {
#ifdef VMW_EMBED_INODE
   .alloc_inode   = SuperOpAllocInode,
   .destroy_inode = SuperOpDestroyInode,
#else
   .clear_inode   = SuperOpClearInode,
#endif
   .read_inode    = SuperOpReadInode,
   .statfs        = SuperOpStatfs,
};


#ifdef VMW_EMBED_INODE
/*
 *----------------------------------------------------------------------------
 *
 *  SuperOpAllocInode --
 *
 *    Allocates an inode info from the cache.  See function comment for Iget()
 *    for a complete explanation of how inode allocation works.
 *
 * Results:
 *    A pointer to the embedded inode on success, NULL on failure.
 *
 * Side effects:
 *    iinfo is initialized by InodeCacheCtor().
 *
 *----------------------------------------------------------------------------
 */

static struct inode *
SuperOpAllocInode(struct super_block *sb) // IN: superblock of file system
{
   VMBlockInodeInfo *iinfo;

   iinfo = kmem_cache_alloc(VMBlockInodeCache, GFP_KERNEL);
   if (!iinfo) {
      Warning("SuperOpAllocInode: could not allocate iinfo\n");
      return NULL;
   }

   /* The inode we give back to VFS is embedded within our inode info struct. */
   return &iinfo->inode;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * SuperOpDestroyInode --
 * SuperOpClearInode --
 *
 *    Destroys the provided inode by freeing the inode info.  In the embedded
 *    inode case, this includes the actual inode itself; in the non-embedded
 *    inode case, the inode is freed by the kernel.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
#ifdef VMW_EMBED_INODE
SuperOpDestroyInode(struct inode *inode)  // IN: Inode to free
#else
SuperOpClearInode(struct inode *inode)    // IN: Inode to free
#endif
{
   kmem_cache_free(VMBlockInodeCache, INODE_TO_IINFO(inode));
}


/*
 *----------------------------------------------------------------------------
 *
 * SuperOpReadInode --
 *
 *    Performs any filesystem wide inode initialization.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
SuperOpReadInode(struct inode *inode)  // IN: Inode to initialize
{
   VMBlockInodeInfo *iinfo = INODE_TO_IINFO(inode);

   iinfo->name[0] = '\0';
   iinfo->nameLen = 0;
   iinfo->actualDentry = NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * SuperOpStatfs --
 *
 *    Implements a null statfs.
 *
 * Results:
 *    Zero on success, negative error on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef VMW_STATFS_2618
static int
SuperOpStatfs(struct dentry *dentry,
              struct compat_kstatfs *stat)
#else
static int
SuperOpStatfs(struct super_block *sb,
              struct compat_kstatfs *stat)
#endif
{
   if (!stat) {
      return -EINVAL;
   }

   stat->f_type = VMBLOCK_SUPER_MAGIC;
   stat->f_bsize = 0;
   stat->f_namelen = NAME_MAX;
   stat->f_blocks = 0;
   stat->f_bfree = 0;
   stat->f_bavail = 0;

   return 0;
}
