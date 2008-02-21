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
 * Superblock operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/vfs.h>
#include "compat_fs.h"
#include "compat_statfs.h"
#include "compat_kernel.h"
#include "compat_slab.h"
#include "compat_sched.h"
#include "compat_version.h"

#include "hgfsProto.h"
#include "escBitvector.h"
#include "cpName.h"
#include "hgfsUtil.h"
#include "request.h"
#include "fsutil.h"
#include "staticEscape.h"
#include "hgfsDevLinux.h"
#include "module.h"
#include "vm_assert.h"


/* Hgfs filesystem superblock operations */
#ifdef VMW_EMBED_INODE
static struct inode *HgfsAllocInode(struct super_block *sb);
static void HgfsDestroyInode(struct inode *inode);
#endif
static void HgfsReadInode(struct inode *inode);
static void HgfsClearInode(struct inode *inode);
static void HgfsPutSuper(struct super_block *sb);
#if defined(VMW_STATFS_2618)
static int HgfsStatfs(struct dentry *dentry,
                      struct compat_kstatfs *stat);
#else
static int HgfsStatfs(struct super_block *sb,
                      struct compat_kstatfs *stat);
#endif

struct super_operations HgfsSuperOperations = {
#ifdef VMW_EMBED_INODE
   .alloc_inode   = HgfsAllocInode,
   .destroy_inode = HgfsDestroyInode,
#endif
   .read_inode    = HgfsReadInode,
   .clear_inode   = HgfsClearInode,
   .put_super     = HgfsPutSuper,
   .statfs        = HgfsStatfs,
};


#ifdef VMW_EMBED_INODE
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAllocInode --
 *
 *    Hgfs superblock 'alloc_inode' method. Called by the kernel to allocate
 *    a new inode struct. We use this VFS method instead of read_inode because
 *    we want to control both how we allocate and how we fill in the inode.
 *
 * Results:
 *    Non-null: A valid inode.
 *    null: Error in inode allocation.
 *
 * Side effects:
 *    Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

static struct inode *
HgfsAllocInode(struct super_block *sb) // IN: Superblock for the inode
{
   HgfsInodeInfo *iinfo;

   iinfo = kmem_cache_alloc(hgfsInodeCache, GFP_KERNEL);
   if (!iinfo) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsAllocInode: "
              "can't allocate memory\n"));
      return NULL;
   }

   return &iinfo->inode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDestroyInode --
 *
 *    Hgfs superblock 'destroy_inode' method. Called by the kernel when it
 *    deallocates an inode. We use this method instead of clear_inode because
 *    we want to control both how we deallocate and how we clear the inode.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    Frees memory associated with inode.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsDestroyInode(struct inode *inode) // IN: The VFS inode
{
   kmem_cache_free(hgfsInodeCache, INODE_GET_II_P(inode));
}

#endif
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsReadInode --
 *
 *    Hgfs superblock 'read_inode' method. Called by the kernel to fill in a
 *    VFS inode, given its hgfs inode number. Needed by iget().
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsReadInode(struct inode *inode) // IN/OUT: VFS inode to fill in
{
   HgfsInodeInfo *iinfo = INODE_GET_II_P(inode);

   /*
    * If the vfs inode is not embedded within the HgfsInodeInfo, then we
    * haven't yet allocated the HgfsInodeInfo. Do so now.
    * 
    * XXX: We could allocate with GFP_ATOMIC. But instead, we'll do a standard
    * allocation and mark the inode "bad" if the allocation fails. This'll
    * make all subsequent operations on the inode fail, which is what we want.
    */
#ifndef VMW_EMBED_INODE
   iinfo = kmem_cache_alloc(hgfsInodeCache, GFP_KERNEL);
   if (!iinfo) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReadInode: no memory for "
              "iinfo!\n"));
      make_bad_inode(inode);
      return;
   }
#endif
   INODE_SET_II_P(inode, iinfo);
   INIT_LIST_HEAD(&iinfo->files);
   iinfo->isReferencedInode = FALSE;
   iinfo->isFakeInodeNumber = FALSE;
   iinfo->createdAndUnopened = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsClearInode --
 *
 *    Hgfs superblock 'clear_inode' method. Called by the kernel when it is
 *    about to destroy a VFS inode.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsClearInode(struct inode *inode) // IN: The VFS inode
{
#ifdef VMW_EMBED_INODE
   /* Do nothing. HgfsDestroyInode will do the dirty work. */
#else
   HgfsInodeInfo *iinfo;

   ASSERT(inode);

   /* The HGFS inode information may be partially constructed --hpreg */
   iinfo = INODE_GET_II_P(inode);
   if (iinfo) {
      kmem_cache_free(hgfsInodeCache, iinfo);
   }
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPutSuper --
 *
 *    Hgfs superblock 'put_super' method. Called after a umount(2) of the
 *    filesystem succeeds.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPutSuper(struct super_block *sb) // IN: The superblock
{
   HgfsSuperInfo *si;

   ASSERT(sb);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPutSuper: was called\n"));

   si = HGFS_SB_TO_COMMON(sb);

   kfree(si->shareName);
   kfree(si);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatfs --
 *
 *    Hgfs superblock 'statfs' method. Called when statfs(2) is invoked on the
 *    filesystem.
 *
 * Results:
 *    0 on success
 *    error < 0 on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

#if defined(VMW_STATFS_2618)
static int
HgfsStatfs(struct dentry *dentry,	// IN : The directory entry
           struct compat_kstatfs *stat) // OUT: Stat to fill in
#else
static int
HgfsStatfs(struct super_block *sb,	// IN : The superblock
           struct compat_kstatfs *stat) // OUT: Stat to fill in
#endif
{
   HgfsReq *req;
   HgfsRequestQueryVolume *request;
   HgfsReplyQueryVolume *reply;
   int result = 0;
   struct dentry *dentryToUse;
   struct super_block *sbToUse;

   ASSERT(stat);
#if defined(VMW_STATFS_2618)
   ASSERT(dentry);
   ASSERT(dentry->d_sb);
   dentryToUse = dentry;
   sbToUse = dentry->d_sb;
#else
   ASSERT(sb);
   ASSERT(sb->s_root);
   dentryToUse = sb->s_root;
   sbToUse = sb;
#endif
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsStatfs: was called\n"));
   memset(stat, 0, sizeof *stat);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   request = (HgfsRequestQueryVolume *)(HGFS_REQ_PAYLOAD(req));

   /* Fill out the request packet. */
   request->header.op = HGFS_OP_QUERY_VOLUME_INFO;
   request->header.id = req->id;

   /* Build full name to send to server. */
   if (HgfsBuildPath(request->fileName.name, HGFS_NAME_BUFFER_SIZE(request), 
                     dentryToUse) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: build path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsStatfs: getting fs stats on \"%s\"\n", 
           request->fileName.name));

   /* Convert to CP name. */
   result = CPName_ConvertTo(request->fileName.name, 
                             HGFS_NAME_BUFFER_SIZE(request),
                             request->fileName.name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: CP conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   /* Unescape the CP name. */
   result = HgfsUnescapeBuffer(request->fileName.name, result);
   request->fileName.length = result;
   req->payloadSize = sizeof *request + result;

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsStatfs: got reply\n"));
      reply = (HgfsReplyQueryVolume *)(HGFS_REQ_PAYLOAD(req));      
      result = HgfsStatusConvertToLinux(reply->header.status);

      /*
       * If the statfs succeeded on the server, copy the stats
       * into the kstatfs struct, otherwise return an error.
       */
      switch (result) {
      case 0:
         stat->f_type = HGFS_SUPER_MAGIC;
         stat->f_bsize = sbToUse->s_blocksize;
         stat->f_namelen = PATH_MAX;
         stat->f_blocks = reply->totalBytes >> sbToUse->s_blocksize_bits;
         stat->f_bfree = reply->freeBytes >> sbToUse->s_blocksize_bits;
         stat->f_bavail = stat->f_bfree;
         break;

      case -EPERM:
         /* 
          * We're cheating! This will cause statfs will return success. 
          * We're doing this because an old server will complain when it gets
          * a statfs on a per-share mount. Rather than have 'df' spit an
          * error, let's just return all zeroes.
          */
         result = 0;
         break;
      
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: server returned error: "
              "%d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: unknown error: %d\n", 
              result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}
