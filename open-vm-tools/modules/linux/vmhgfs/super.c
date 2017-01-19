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
#include "hgfsDevLinux.h"
#include "module.h"
#include "vm_assert.h"


/* Hgfs filesystem superblock operations */
static struct inode *HgfsAllocInode(struct super_block *sb);
static void HgfsDestroyInode(struct inode *inode);
static void HgfsPutSuper(struct super_block *sb);
#if defined VMW_STATFS_2618
static int HgfsStatfs(struct dentry *dentry,
                      struct compat_kstatfs *stat);
#else
static int HgfsStatfs(struct super_block *sb,
                      struct compat_kstatfs *stat);
#endif

struct super_operations HgfsSuperOperations = {
   .alloc_inode   = HgfsAllocInode,
   .destroy_inode = HgfsDestroyInode,
   .put_super     = HgfsPutSuper,
   .statfs        = HgfsStatfs,
};


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
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 0, 0)
   bdi_destroy(&si->bdi);
#endif
   kfree(si->shareName);
   kfree(si);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackQueryVolumeRequest --
 *
 *    Setup the query volume request, depending on the op version.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsPackQueryVolumeRequest(struct dentry *dentry,   // IN: File pointer for this open
                           HgfsOp opUsed,           // IN: Op to be used.
                           HgfsReq *req)            // IN/OUT: Packet to write into
{
   char *name;
   uint32 *nameLength;
   size_t requestSize;
   int result;

   ASSERT(dentry);
   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_QUERY_VOLUME_INFO_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestQueryVolumeV3 *requestV3;

      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestQueryVolumeV3 *)HGFS_REQ_PAYLOAD_V3(req);

      /* We'll use these later. */
      name = requestV3->fileName.name;
      nameLength = &requestV3->fileName.length;
      requestV3->fileName.flags = 0;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->reserved = 0;
      requestSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);
      break;
   }
   case HGFS_OP_QUERY_VOLUME_INFO: {
      HgfsRequestQueryVolume *request;

      request = (HgfsRequestQueryVolume *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = opUsed;
      request->header.id = req->id;

      /* We'll use these later. */
      name = request->fileName.name;
      nameLength = &request->fileName.length;
      requestSize = sizeof *request;
      break;
   }
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackQueryVolumeRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(name, req->bufferSize - (requestSize - 1),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackQueryVolumeRequest: build path failed\n"));
      return -EINVAL;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackQueryVolumeRequest: opening \"%s\"\n",
           name));

   /* Convert to CP name. */
   result = CPName_ConvertTo(name,
                             req->bufferSize - (requestSize - 1),
                             name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackQueryVolumeRequest: CP conversion failed\n"));
      return -EINVAL;
   }

   *nameLength = (uint32) result;
   req->payloadSize = requestSize + result;

   return 0;
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

#if defined VMW_STATFS_2618
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
   int result = 0;
   struct dentry *dentryToUse;
   struct super_block *sbToUse;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   uint64 freeBytes;
   uint64 totalBytes;

   ASSERT(stat);
#if defined VMW_STATFS_2618
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

  retry:
   opUsed = hgfsVersionQueryVolumeInfo;
   result = HgfsPackQueryVolumeRequest(dentryToUse, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: error packing request\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsStatfs: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      /*
       * If the statfs succeeded on the server, copy the stats
       * into the kstatfs struct, otherwise return an error.
       */
      switch (result) {
      case 0:
         stat->f_type = HGFS_SUPER_MAGIC;
         stat->f_bsize = sbToUse->s_blocksize;
         stat->f_namelen = PATH_MAX;
         if (opUsed == HGFS_OP_QUERY_VOLUME_INFO_V3) {
            totalBytes = ((HgfsReplyQueryVolumeV3 *)HGFS_REP_PAYLOAD_V3(req))->totalBytes;
            freeBytes = ((HgfsReplyQueryVolumeV3 *)HGFS_REP_PAYLOAD_V3(req))->freeBytes;
         } else {
            totalBytes = ((HgfsReplyQueryVolume *)HGFS_REQ_PAYLOAD(req))->totalBytes;
            freeBytes = ((HgfsReplyQueryVolume *)HGFS_REQ_PAYLOAD(req))->freeBytes;
         }
         stat->f_blocks = totalBytes >> sbToUse->s_blocksize_bits;
         stat->f_bfree = freeBytes >> sbToUse->s_blocksize_bits;
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

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_QUERY_VOLUME_INFO_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsStatfs: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionQueryVolumeInfo = HGFS_OP_QUERY_VOLUME_INFO;
            goto retry;
         }
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
