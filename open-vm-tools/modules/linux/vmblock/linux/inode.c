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
 * inode.c --
 *
 *   Inode operations for the file system of the vmblock driver.
 *
 */

#include "driver-config.h"
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/namei.h>

#include "vmblockInt.h"
#include "filesystem.h"
#include "block.h"


/* Inode operations */
static struct dentry *InodeOpLookup(struct inode *dir,
                                    struct dentry *dentry, struct nameidata *nd);
static int InodeOpReadlink(struct dentry *dentry, char __user *buffer, int buflen);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static void *InodeOpFollowlink(struct dentry *dentry, struct nameidata *nd);
#else
static int InodeOpFollowlink(struct dentry *dentry, struct nameidata *nd);
#endif


struct inode_operations RootInodeOps = {
   .lookup = InodeOpLookup,
};

static struct inode_operations LinkInodeOps = {
   .readlink    = InodeOpReadlink,
   .follow_link = InodeOpFollowlink,
};


/*
 *----------------------------------------------------------------------------
 *
 * InodeOpLookup --
 *
 *    Looks up a name (dentry) in provided directory.  Invoked every time
 *    a directory entry is traversed in path lookups.
 *
 * Results:
 *    NULL on success, negative error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static struct dentry *
InodeOpLookup(struct inode *dir,      // IN: parent directory's inode
              struct dentry *dentry,  // IN: dentry to lookup
              struct nameidata *nd)   // IN: lookup intent and information
{
   char *filename;
   struct inode *inode;
   int ret;

   if (!dir || !dentry) {
      Warning("InodeOpLookup: invalid args from kernel\n");
      return ERR_PTR(-EINVAL);
   }

   /* The kernel should only pass us our own inodes, but check just to be safe. */
   if (!INODE_TO_IINFO(dir)) {
      Warning("InodeOpLookup: invalid inode provided\n");
      return ERR_PTR(-EINVAL);
   }

   /* Get a slab from the kernel's names_cache of PATH_MAX-sized buffers. */
   filename = __getname();
   if (!filename) {
      Warning("InodeOpLookup: unable to obtain memory for filename.\n");
      return ERR_PTR(-ENOMEM);
   }

   ret = MakeFullName(dir, dentry, filename, PATH_MAX);
   if (ret < 0) {
      Warning("InodeOpLookup: could not construct full name\n");
      __putname(filename);
      return ERR_PTR(ret);
   }

   /* Block if there is a pending block on this file */
   BlockWaitOnFile(filename, NULL);
   __putname(filename);

   inode = Iget(dir->i_sb, dir, dentry, GetNextIno());
   if (!inode) {
      Warning("InodeOpLookup: failed to get inode\n");
      return ERR_PTR(-ENOMEM);
   }

   dentry->d_op = &LinkDentryOps;
   dentry->d_time = jiffies;

   /*
    * If the actual file's dentry doesn't have an inode, it means the file we
    * are redirecting to doesn't exist.  Give back the inode that was created
    * for this and add a NULL dentry->inode entry in the dcache.  (The NULL
    * entry is added so ops to create files/directories are invoked by VFS.)
    */
   if (!INODE_TO_ACTUALDENTRY(inode) || !INODE_TO_ACTUALINODE(inode)) {
      iput(inode);
      d_add(dentry, NULL);
      return NULL;
   }

   inode->i_mode = S_IFLNK | S_IRWXUGO;
   inode->i_size = INODE_TO_IINFO(inode)->nameLen;
   inode->i_version = 1;
   inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
   inode->i_uid = inode->i_gid = 0;
   inode->i_op = &LinkInodeOps;

   d_add(dentry, inode);
   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * InodeOpReadlink --
 *
 *    Provides the symbolic link's contents to the user.  Invoked when
 *    readlink(2) is invoked on our symlinks.
 *
 * Results:
 *    0 on success, negative error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
InodeOpReadlink(struct dentry *dentry,  // IN : dentry of symlink
                char __user *buffer,    // OUT: output buffer (user space)
                int buflen)             // IN : length of output buffer
{
   VMBlockInodeInfo *iinfo;

   if (!dentry || !buffer) {
      Warning("InodeOpReadlink: invalid args from kernel\n");
      return -EINVAL;
   }

   iinfo = INODE_TO_IINFO(dentry->d_inode);
   if (!iinfo) {
      return -EINVAL;
   }

   return vfs_readlink(dentry, buffer, buflen, iinfo->name);
}


/*
 *----------------------------------------------------------------------------
 *
 * InodeOpFollowlink --
 *
 *    Provides the inode corresponding to this symlink through the nameidata
 *    structure.
 *
 * Results:
 *    0 on success, negative error on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
static void *
#else
static int
#endif
InodeOpFollowlink(struct dentry *dentry,  // IN : dentry of symlink
                  struct nameidata *nd)   // OUT: stores result
{
   int ret;
   VMBlockInodeInfo *iinfo;

   if (!dentry) {
      Warning("InodeOpReadlink: invalid args from kernel\n");
      ret = -EINVAL;
      goto out;
   }

   iinfo = INODE_TO_IINFO(dentry->d_inode);
   if (!iinfo) {
      ret = -EINVAL;
      goto out;
   }

   ret = vfs_follow_link(nd, iinfo->name);

out:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 13)
   return ERR_PTR(ret);
#else
   return ret;
#endif
}
