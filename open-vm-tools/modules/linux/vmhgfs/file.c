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
 * file.c --
 *
 * File operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/signal.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 15, 0)
#include <linux/uio.h> /* iov_iter_count */
#endif
#include "compat_cred.h"
#include "compat_fs.h"
#include "compat_kernel.h"
#include "compat_slab.h"

/* Must be after compat_fs.h */
#if defined VMW_USE_AIO
#include <linux/aio.h>
#endif

#include "cpName.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "hgfsUtil.h"
#include "fsutil.h"
#include "vm_assert.h"
#include "vm_basic_types.h"

/*
 * Before Linux 2.6.33 only O_DSYNC semantics were implemented, but using
 * the O_SYNC flag.  We continue to use the existing numerical value
 * for O_DSYNC semantics now, but using the correct symbolic name for it.
 * This new value is used to request true Posix O_SYNC semantics.  It is
 * defined in this strange way to make sure applications compiled against
 * new headers get at least O_DSYNC semantics on older kernels.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 33)
#define HGFS_FILECTL_SYNC(flags)            ((flags) & O_DSYNC)
#else
#define HGFS_FILECTL_SYNC(flags)            ((flags) & O_SYNC)
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
typedef struct iov_iter *hgfs_iov;
#define HGFS_IOV_TO_COUNT(iov, nr_segs)                   (iov_iter_count(iov))
#define HGFS_IOV_TO_SEGS(iov, nr_segs)                    (0)
#define HGFS_IOCB_TO_POS(iocb, pos)                       (iocb->ki_pos)
#else
typedef const struct iovec *hgfs_iov;
#define HGFS_IOV_TO_COUNT(iov, nr_segs)                   (iov_length(iov, nr_segs))
#define HGFS_IOV_TO_SEGS(iov, nr_segs)                    (nr_segs)
#define HGFS_IOCB_TO_POS(iocb, pos)                       (pos)
#endif

/* Private functions. */
static int HgfsPackOpenRequest(struct inode *inode,
                               struct file *file,
                               HgfsOp opUsed,
                               HgfsReq *req);
static int HgfsUnpackOpenReply(HgfsReq *req,
                               HgfsOp opUsed,
                               HgfsHandle *file,
                               HgfsLockType *lock);

/* HGFS file operations for files. */
static int HgfsOpen(struct inode *inode,
                    struct file *file);
#if defined VMW_USE_AIO
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t HgfsFileRead(struct kiocb *iocb,
                            struct iov_iter *to);
static ssize_t HgfsFileWrite(struct kiocb *iocb,
                            struct iov_iter *from);
#else // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t HgfsFileRead(struct kiocb *iocb,
                            const struct iovec *iov,
                            unsigned long numSegs,
                            loff_t offset);
static ssize_t HgfsFileWrite(struct kiocb *iocb,
                             const struct iovec *iov,
                             unsigned long numSegs,
                             loff_t offset);
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
#else
static ssize_t HgfsRead(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *offset);
static ssize_t HgfsWrite(struct file *file,
                         const char __user *buf,
                         size_t count,
                         loff_t *offset);
#endif

static loff_t HgfsSeek(struct file *file,
                       loff_t  offset,
                       int origin);
static int HgfsFlush(struct file *file
#if !defined VMW_FLUSH_HAS_1_ARG
                     ,fl_owner_t id
#endif
                     );

#if !defined VMW_FSYNC_31
static int HgfsDoFsync(struct inode *inode);
#endif

static int HgfsFsync(struct file *file,
#if defined VMW_FSYNC_OLD
                     struct dentry *dentry,
#elif defined VMW_FSYNC_31
                     loff_t start,
                     loff_t end,
#endif
                     int datasync);
static int HgfsMmap(struct file *file,
                    struct vm_area_struct *vma);
static int HgfsRelease(struct inode *inode,
                       struct file *file);

#ifndef VMW_SENDFILE_NONE
#if defined VMW_SENDFILE_OLD
static ssize_t HgfsSendfile(struct file *file,
                            loff_t *offset,
                            size_t count,
                            read_actor_t actor,
                            void __user *target);
#else /* defined VMW_SENDFILE_NEW */
static ssize_t HgfsSendfile(struct file *file,
                            loff_t *offset,
                            size_t count,
                            read_actor_t actor,
                            void *target);
#endif
#endif
#ifdef VMW_SPLICE_READ
static ssize_t HgfsSpliceRead(struct file *file,
                              loff_t *offset,
                              struct pipe_inode_info *pipe,
                              size_t len,
                              unsigned int flags);
#endif

/* HGFS file operations structure for files. */
struct file_operations HgfsFileFileOperations = {
   .owner      = THIS_MODULE,
   .open       = HgfsOpen,
   .llseek     = HgfsSeek,
   .flush      = HgfsFlush,
#if defined VMW_USE_AIO
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
   /* Fallback to async counterpart, check kernel source read_write.c */
   .read       = NULL,
   .write      = NULL,
   .read_iter  = HgfsFileRead,
   .write_iter = HgfsFileWrite,
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
   .read       = new_sync_read,
   .write      = new_sync_write,
   .read_iter  = HgfsFileRead,
   .write_iter = HgfsFileWrite,
#else // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
   .read       = do_sync_read,
   .write      = do_sync_write,
   .aio_read   = HgfsFileRead,
   .aio_write  = HgfsFileWrite,
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
#else
   .read       = HgfsRead,
   .write      = HgfsWrite,
#endif
   .fsync      = HgfsFsync,
   .mmap       = HgfsMmap,
   .release    = HgfsRelease,
#ifndef VMW_SENDFILE_NONE
   .sendfile   = HgfsSendfile,
#endif
#ifdef VMW_SPLICE_READ
   .splice_read = HgfsSpliceRead,
#endif
};

/* File open mask. */
#define HGFS_FILE_OPEN_MASK (HGFS_OPEN_VALID_MODE | \
                             HGFS_OPEN_VALID_FLAGS | \
                             HGFS_OPEN_VALID_SPECIAL_PERMS | \
			     HGFS_OPEN_VALID_OWNER_PERMS | \
			     HGFS_OPEN_VALID_GROUP_PERMS | \
			     HGFS_OPEN_VALID_OTHER_PERMS | \
			     HGFS_OPEN_VALID_FILE_NAME | \
			     HGFS_OPEN_VALID_SERVER_LOCK)


/*
 * Private functions.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsPackOpenRequest --
 *
 *    Setup the Open request, depending on the op version.
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
HgfsPackOpenRequest(struct inode *inode, // IN: Inode of the file to open
                    struct file *file,   // IN: File pointer for this open
                    HgfsOp opUsed,       // IN: Op to use
                    HgfsReq *req)        // IN/OUT: Packet to write into
{
   char *name;
   uint32 *nameLength;
   size_t requestSize;
   int result;

   ASSERT(inode);
   ASSERT(file);
   ASSERT(req);

   switch (opUsed) {
    case HGFS_OP_OPEN_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestOpenV3 *requestV3;

      requestHeader = (HgfsRequest *)HGFS_REQ_PAYLOAD(req);
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestOpenV3 *)HGFS_REQ_PAYLOAD_V3(req);
      requestSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);

      /* We'll use these later. */
      name = requestV3->fileName.name;
      nameLength = &requestV3->fileName.length;

      requestV3->mask = HGFS_FILE_OPEN_MASK;

      /* Linux clients need case-sensitive lookups. */
      requestV3->fileName.flags = 0;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;

      /* Set mode. */
      result = HgfsGetOpenMode(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open mode\n"));
         return -EINVAL;
      }
      requestV3->mode = result;

      /* Set flags. */
      result = HgfsGetOpenFlags(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open flags\n"));
         return -EINVAL;
      }
      requestV3->flags = result;

      LOG(4, (KERN_DEBUG "VMware hgfs: %s: mode file %o inode %o -> user %o\n",
              __func__, file->f_mode, inode->i_mode, (inode->i_mode & S_IRWXU) >> 6));
      /* Set permissions. */
      requestV3->specialPerms = (inode->i_mode & (S_ISUID | S_ISGID | S_ISVTX))
                                >> 9;
      requestV3->ownerPerms = (inode->i_mode & S_IRWXU) >> 6;
      requestV3->groupPerms = (inode->i_mode & S_IRWXG) >> 3;
      requestV3->otherPerms = (inode->i_mode & S_IRWXO);

      /* XXX: Request no lock for now. */
      requestV3->desiredLock = HGFS_LOCK_NONE;

      requestV3->reserved1 = 0;
      requestV3->reserved2 = 0;
      break;
   }

   case HGFS_OP_OPEN_V2: {
      HgfsRequestOpenV2 *requestV2;

      requestV2 = (HgfsRequestOpenV2 *)(HGFS_REQ_PAYLOAD(req));
      requestV2->header.op = opUsed;
      requestV2->header.id = req->id;

      /* We'll use these later. */
      name = requestV2->fileName.name;
      nameLength = &requestV2->fileName.length;
      requestSize = sizeof *requestV2;

      requestV2->mask = HGFS_FILE_OPEN_MASK;

      /* Set mode. */
      result = HgfsGetOpenMode(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open mode\n"));
         return -EINVAL;
      }
      requestV2->mode = result;

      /* Set flags. */
      result = HgfsGetOpenFlags(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open flags\n"));
         return -EINVAL;
      }
      requestV2->flags = result;

      /* Set permissions. */
      requestV2->specialPerms = (inode->i_mode & (S_ISUID | S_ISGID | S_ISVTX))
                                >> 9;
      requestV2->ownerPerms = (inode->i_mode & S_IRWXU) >> 6;
      requestV2->groupPerms = (inode->i_mode & S_IRWXG) >> 3;
      requestV2->otherPerms = (inode->i_mode & S_IRWXO);

      /* XXX: Request no lock for now. */
      requestV2->desiredLock = HGFS_LOCK_NONE;
      break;
   }
   case HGFS_OP_OPEN: {
      HgfsRequestOpen *request;

      request = (HgfsRequestOpen *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = opUsed;
      request->header.id = req->id;

      /* We'll use these later. */
      name = request->fileName.name;
      nameLength = &request->fileName.length;
      requestSize = sizeof *request;

      /* Set mode. */
      result = HgfsGetOpenMode(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open mode\n"));
         return -EINVAL;
      }
      request->mode = result;

      /* Set flags. */
      result = HgfsGetOpenFlags(file->f_flags);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: failed to get "
                 "open flags\n"));
         return -EINVAL;
      }
      request->flags = result;

      /* Set permissions. */
      request->permissions = (inode->i_mode & S_IRWXU) >> 6;
      break;
   }
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(name,
                     req->bufferSize - (requestSize - 1),
                     file->f_dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: build path "
              "failed\n"));
      return -EINVAL;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: opening \"%s\", "
           "flags %o, create perms %o\n", name,
           file->f_flags, file->f_mode));

   /* Convert to CP name. */
   result = CPName_ConvertTo(name,
                             req->bufferSize - (requestSize - 1),
                             name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackOpenRequest: CP conversion "
              "failed\n"));
      return -EINVAL;
   }

   *nameLength = (uint32) result;
   req->payloadSize = requestSize + result;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsUnpackOpenReply --
 *
 *    Get interesting fields out of the Open reply, depending on the op
 *    version.
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
HgfsUnpackOpenReply(HgfsReq *req,          // IN: Packet with reply inside
                    HgfsOp opUsed,         // IN: What request op did we send
                    HgfsHandle *file,      // OUT: Handle in reply packet
                    HgfsLockType *lock)    // OUT: The server lock we got
{
   HgfsReplyOpenV3 *replyV3;
   HgfsReplyOpenV2 *replyV2;
   HgfsReplyOpen *replyV1;
   size_t replySize;

   ASSERT(req);
   ASSERT(file);
   ASSERT(lock);

   switch (opUsed) {
   case HGFS_OP_OPEN_V3:
      replyV3 = (HgfsReplyOpenV3 *)HGFS_REP_PAYLOAD_V3(req);
      replySize = HGFS_REP_PAYLOAD_SIZE_V3(replyV3);
      *file = replyV3->file;
      *lock = replyV3->acquiredLock;
      break;
   case HGFS_OP_OPEN_V2:
      replyV2 = (HgfsReplyOpenV2 *)(HGFS_REQ_PAYLOAD(req));
      replySize = sizeof *replyV2;
      *file = replyV2->file;
      *lock = replyV2->acquiredLock;
      break;
   case HGFS_OP_OPEN:
      replyV1 = (HgfsReplyOpen *)(HGFS_REQ_PAYLOAD(req));
      replySize = sizeof *replyV1;
      *file = replyV1->file;
      *lock = HGFS_LOCK_NONE;
      break;
   default:

      /* This really shouldn't happen since we set opUsed ourselves. */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackOpenReply: unexpected "
              "OP type encountered\n"));
      ASSERT(FALSE);
      return -EPROTO;
   }

   if (req->payloadSize != replySize) {
      /*
       * The reply to Open is a fixed size. So the size of the payload
       * really ought to match the expected size of an HgfsReplyOpen[V2].
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackOpenReply: wrong packet "
              "size\n"));
      return -EPROTO;
   }
   return 0;
}


/*
 * HGFS file operations for files.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsOpen --
 *
 *    Called whenever a process opens a file in our filesystem.
 *
 *    We send an "Open" request to the server with the name stored in
 *    this file's inode. If the Open succeeds, we store the filehandle
 *    sent by the server in the file struct so it can be accessed by
 *    read/write/close.
 *
 * Results:
 *    Returns zero if on success, error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsOpen(struct inode *inode,  // IN: Inode of the file to open
         struct file *file)    // IN: File pointer for this open
{
   HgfsReq *req;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   HgfsHandle replyFile;
   HgfsLockType replyLock;
   HgfsInodeInfo *iinfo;
   int result = 0;

   ASSERT(inode);
   ASSERT(inode->i_sb);
   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_inode);

   iinfo = INODE_GET_II_P(inode);

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   /*
    * Set up pointers using the proper struct This lets us check the
    * version exactly once and use the pointers later.
    */

   opUsed = hgfsVersionOpen;
   result = HgfsPackOpenRequest(inode, file, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: error packing request\n"));
      goto out;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply and check return status. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         iinfo->createdAndUnopened = FALSE;
         LOG(10, (KERN_DEBUG "VMware hgfs: HgfsOpen: old hostFileId = "
                  "%"FMT64"u\n", iinfo->hostFileId));
         /*
          * Invalidate the hostFileId as we need to retrieve it from
          * the server.
          */
         iinfo->hostFileId = 0;
         result = HgfsUnpackOpenReply(req, opUsed, &replyFile, &replyLock);
         if (result != 0) {
            break;
         }
         result = HgfsCreateFileInfo(file, replyFile);
         if (result != 0) {
            break;
         }
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsOpen: set handle to %u\n",
                 replyFile));

         /*
          * HgfsCreate faked all of the inode's attributes, so by the time
          * we're done in HgfsOpen, we need to make sure that the attributes
          * in the inode are real. The following is only necessary when
          * O_CREAT is set, otherwise we got here after HgfsLookup (which sent
          * a getattr to the server and got the real attributes).
          *
          * In particular, we'd like to at least try and set the inode's
          * uid/gid to match the caller's. We don't expect this to work,
          * because Windows servers will ignore it, and Linux servers running
          * as non-root won't be able to change it, but we're forward thinking
          * people.
          *
          * Either way, we force a revalidate following the setattr so that
          * we'll get the actual uid/gid from the server.
          */
         if (file->f_flags & O_CREAT) {
            struct dentry *dparent;
            struct inode *iparent;

            /*
             * This is not the root of our file system so there should always
             * be a parent.
             */
            ASSERT(file->f_dentry->d_parent);

            /*
             * Here we obtain a reference on the parent to make sure it doesn't
             * go away.  This might not be necessary, since the existence of
             * a child (which we hold a reference to in this call) should
             * account for a reference in the parent, but it's safe to do so.
             * Overly cautious and safe is better than risky and broken.
             *
             * XXX Note that this and a handful of other hacks wouldn't be
             * necessary if we actually created the file in our create
             * implementation (where references and locks are properly held).
             * We could do this if we were willing to give up support for
             * O_EXCL on 2.4 kernels.
             */
            dparent = dget(file->f_dentry->d_parent);
            iparent = dparent->d_inode;

            HgfsSetUidGid(iparent, file->f_dentry,
                          current_fsuid(), current_fsgid());

            dput(dparent);
         }
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_OPEN_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: Version 3 not "
                    "supported. Falling back to version 2.\n"));
            hgfsVersionOpen = HGFS_OP_OPEN_V2;
            goto retry;
         }

         /* Retry with Version 1 of Open. Set globally. */
         if (opUsed == HGFS_OP_OPEN_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: Version 2 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionOpen = HGFS_OP_OPEN;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: unknown error: "
              "%d\n", result));
   }
out:
   HgfsFreeRequest(req);

   /*
    * If the open failed (for any reason) and we tried to open a newly created
    * file, we must ensure that the next operation on this inode triggers a
    * revalidate to the server. This is because the file wasn't created on the
    * server, yet we currently believe that it was, because we created a fake
    * inode with a hashed dentry for it in HgfsCreate. We will continue to
    * believe this until the dentry's ttl expires, which will cause a
    * revalidate to the server that will reveal the truth. So in order to find
    * the truth as soon as possible, we'll reset the dentry's last revalidate
    * time now to force a revalidate the next time someone uses the dentry.
    *
    * We're using our own flag to track this case because using O_CREAT isn't
    * good enough: HgfsOpen will be called with O_CREAT even if the file exists
    * on the server, and if that's the case, there's no need to revalidate.
    *
    * XXX: Note that this will need to be reworked if/when we support hard
    * links, because multiple dentries will point to the same inode, and
    * forcing a revalidate on one will not force it on any others.
    */
   if (result != 0 && iinfo->createdAndUnopened == TRUE) {
      HgfsDentryAgeForce(file->f_dentry);
   }
   return result;
}


#if defined VMW_USE_AIO
/*
 *----------------------------------------------------------------------
 *
 * HgfsGenericFileRead --
 *
 *    Called when the kernel initiates an asynchronous read from a file in
 *    our filesystem. Our function is just a thin wrapper around
 *    system generic read function.
 *
 *
 * Results:
 *    Returns the number of bytes read on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ssize_t
HgfsGenericFileRead(struct kiocb *iocb,          // IN: I/O control block
                    hgfs_iov iov,                // IN: Array of I/O vectors
                    unsigned long iovSegs,       // IN: Count of I/O vectors
                    loff_t pos)                  // IN: Position at which to read
{
   ssize_t result;

   LOG(8, (KERN_DEBUG "VMware hgfs: %s(%lu@%Ld)\n",
           __func__, (unsigned long)HGFS_IOV_TO_COUNT(iov, iovSegs),
           (long long) pos));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
   result = generic_file_read_iter(iocb, iov);
#else
   result = generic_file_aio_read(iocb, iov, iovSegs, pos);
#endif

   LOG(8, (KERN_DEBUG "VMware hgfs: %s return %"FMTSZ"d\n",
           __func__, result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFileRead --
 *
 *    Called when the kernel initiates an asynchronous read to a file in
 *    our filesystem. Our function is just a thin wrapper around
 *    generic_file_aio_read() that tries to validate the dentry first.
 *
 * Results:
 *    Returns the number of bytes read on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t
HgfsFileRead(struct kiocb *iocb,      // IN:  I/O control block
             struct iov_iter *iov)    // OUT: Array of I/O buffers
#else // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t
HgfsFileRead(struct kiocb *iocb,      // IN:  I/O control block
             const struct iovec *iov, // OUT: Array of I/O buffers
             unsigned long numSegs,   // IN:  Number of buffers
             loff_t offset)           // IN:  Offset at which to read
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
{
   ssize_t result;
   struct dentry *readDentry;
   loff_t pos;
   unsigned long iovSegs;

   ASSERT(iocb);
   ASSERT(iocb->ki_filp);
   ASSERT(iocb->ki_filp->f_dentry);
   ASSERT(iov);

   pos = HGFS_IOCB_TO_POS(iocb, offset);
   iovSegs = HGFS_IOV_TO_SEGS(iov, numSegs);

   readDentry = iocb->ki_filp->f_dentry;

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s)\n",
           __func__, readDentry->d_parent->d_name.name,
           readDentry->d_name.name));

   result = HgfsRevalidate(readDentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: invalid dentry\n", __func__));
      goto out;
   }

   result = HgfsGenericFileRead(iocb, iov, iovSegs, pos);

out:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGenericFileWrite --
 *
 *    Called when the kernel initiates an asynchronous write to a file in
 *    our filesystem. Our function is just a thin wrapper around
 *    system generic write function.
 *
 *
 * Results:
 *    Returns the number of bytes written on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ssize_t
HgfsGenericFileWrite(struct kiocb *iocb,          // IN: I/O control block
                     hgfs_iov iov,                // IN: Array of I/O vectors
                     unsigned long iovSegs,       // IN: Count of I/O vectors
                     loff_t pos)                  // IN: Position at which to write
{
   ssize_t result;

   LOG(8, (KERN_DEBUG "VMware hgfs: %s(%lu@%Ld)\n",
           __func__, (unsigned long)HGFS_IOV_TO_COUNT(iov, iovSegs),
           (long long) pos));

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
   result = generic_file_write_iter(iocb, iov);
#else
   result = generic_file_aio_write(iocb, iov, iovSegs, pos);
#endif

   LOG(8, (KERN_DEBUG "VMware hgfs: %s return %"FMTSZ"d\n",
           __func__, result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFileWrite --
 *
 *    Called when the kernel initiates an asynchronous write to a file in
 *    our filesystem. Our function is just a thin wrapper around
 *    generic_file_aio_write() that tries to validate the dentry first.
 *
 *    Note that files opened with O_SYNC (or superblocks mounted with
 *    "sync") are synchronously written to by the VFS.
 *
 * Results:
 *    Returns the number of bytes written on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
ssize_t
HgfsFileWrite(struct kiocb *iocb,     // IN:  I/O control block
              struct iov_iter *iov)   // IN:  Array of I/O buffers
#else // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
static ssize_t
HgfsFileWrite(struct kiocb *iocb,      // IN:  I/O control block
              const struct iovec *iov, // IN:  Array of I/O buffers
              unsigned long numSegs,   // IN:  Number of buffers
              loff_t offset)           // IN:  Offset at which to write
#endif // LINUX_VERSION_CODE >= KERNEL_VERSION(3, 16, 0)
{
   ssize_t result;
   struct dentry *writeDentry;
   HgfsInodeInfo *iinfo;
   loff_t pos;
   unsigned long iovSegs;

   ASSERT(iocb);
   ASSERT(iocb->ki_filp);
   ASSERT(iocb->ki_filp->f_dentry);
   ASSERT(iov);

   pos = HGFS_IOCB_TO_POS(iocb, offset);
   iovSegs = HGFS_IOV_TO_SEGS(iov, numSegs);

   writeDentry = iocb->ki_filp->f_dentry;
   iinfo = INODE_GET_II_P(writeDentry->d_inode);

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s)\n",
          __func__, writeDentry->d_parent->d_name.name,
          writeDentry->d_name.name));


   spin_lock(&writeDentry->d_inode->i_lock);
   /*
    * Guard against dentry revalidation invalidating the inode underneath us.
    *
    * Data is being written and may have valid data in a page in the cache.
    * This action prevents any invalidating of the inode when a flushing of
    * cache data occurs prior to syncing the file with the server's attributes.
    * The flushing of cache data would empty our in memory write pages list and
    * would cause the inode modified write time to be updated and so the inode
    * would also be invalidated.
    */
   iinfo->numWbPages++;
   spin_unlock(&writeDentry->d_inode->i_lock);

   result = HgfsRevalidate(writeDentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: invalid dentry\n", __func__));
      goto out;
   }

   result = HgfsGenericFileWrite(iocb, iov, iovSegs, pos);

   if (result >= 0) {
      if (IS_SYNC(writeDentry->d_inode) ||
          HGFS_FILECTL_SYNC(iocb->ki_filp->f_flags)) {
         int error;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
         error = vfs_fsync(iocb->ki_filp, 0);
#else
         error = HgfsDoFsync(writeDentry->d_inode);
#endif
         if (error < 0) {
            result = error;
         }
      }
   }

out:
   spin_lock(&writeDentry->d_inode->i_lock);
   iinfo->numWbPages--;
   spin_unlock(&writeDentry->d_inode->i_lock);
   return result;
}


#else
/*
 *----------------------------------------------------------------------
 *
 * HgfsRead --
 *
 *    Called whenever a process reads from a file in our filesystem. Our
 *    function is just a thin wrapper around generic_read_file() that
 *    tries to validate the dentry first.
 *
 * Results:
 *    Returns the number of bytes read on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ssize_t
HgfsRead(struct file *file,  // IN:  File to read from
         char __user *buf,   // OUT: User buffer to copy data into
         size_t count,       // IN:  Number of bytes to read
         loff_t *offset)     // IN:  Offset at which to read
{
   int result;

   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(buf);
   ASSERT(offset);

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s,%Zu@%lld)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name, count, (long long) *offset));

   result = HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRead: invalid dentry\n"));
      goto out;
   }

   result = generic_file_read(file, buf, count, offset);
  out:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsWrite --
 *
 *    Called whenever a process writes to a file in our filesystem. Our
 *    function is just a thin wrapper around generic_write_file() that
 *    tries to validate the dentry first.
 *
 *    Note that files opened with O_SYNC (or superblocks mounted with
 *    "sync") are synchronously written to by the VFS.
 *
 * Results:
 *    Returns the number of bytes written on success, or an error on
 *    failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ssize_t
HgfsWrite(struct file *file,      // IN: File to write to
          const char __user *buf, // IN: User buffer where the data is
          size_t count,           // IN: Number of bytes to write
          loff_t *offset)         // IN: Offset to begin writing at
{
   int result;

   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_inode);
   ASSERT(buf);
   ASSERT(offset);

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s,%Zu@%lld)\n",
           __func__, file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name, count, (long long) *offset));

   result = HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsWrite: invalid dentry\n"));
      goto out;
   }

   result = generic_file_write(file, buf, count, offset);
  out:
   return result;
}
#endif

/*
 *----------------------------------------------------------------------
 *
 * HgfsSeek --
 *
 *    Called whenever a process moves the file pointer for a file in our
 *    filesystem. Our function is just a thin wrapper around
 *    generic_file_llseek() that tries to validate the dentry first.
 *
 * Results:
 *    Returns the new position of the file pointer on success,
 *    or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static loff_t
HgfsSeek(struct file *file,  // IN:  File to seek
         loff_t offset,      // IN:  Number of bytes to seek
         int origin)         // IN:  Position to seek from

{
   loff_t result = -1;

   ASSERT(file);
   ASSERT(file->f_dentry);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s(%s/%s, %u, %lld, %d)\n",
           __func__,
            file->f_dentry->d_parent->d_name.name,
            file->f_dentry->d_name.name,
            FILE_GET_FI_P(file)->handle, offset, origin));

   result = (loff_t) HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: invalid dentry\n", __func__));
      goto out;
   }

   result = generic_file_llseek(file, offset, origin);

  out:
   return result;
}


#if !defined VMW_FSYNC_31
/*
 *----------------------------------------------------------------------
 *
 * HgfsDoFsync --
 *
 *    Helper for HgfsFlush() and HgfsFsync().
 *
 *    The hgfs protocol doesn't support fsync explicityly yet.
 *    So for now, we flush all the pages to presumably honor the
 *    intent of an app calling fsync() which is to get the
 *    data onto persistent storage. As things stand now we're at
 *    the whim of the hgfs server code running on the host to fsync or
 *    not if and when it pleases.
 *
 *
 * Results:
 *    Returns zero on success. Otherwise an error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsDoFsync(struct inode *inode)            // IN: File we operate on
{
   int ret;

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%"FMT64"u)\n",
            __func__,  INODE_GET_II_P(inode)->hostFileId));

   ret = compat_filemap_write_and_wait(inode->i_mapping);

   LOG(4, (KERN_DEBUG "VMware hgfs: %s: returns %d\n",
           __func__, ret));

   return ret;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HgfsFlush --
 *
 *    Called when user process calls fflush() on an hgfs file.
 *    Flush all dirty pages and check for write errors.
 *
 *
 * Results:
 *    Returns zero on success. (Currently always succeeds).
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsFlush(struct file *file                        // IN: file to flush
#if !defined VMW_FLUSH_HAS_1_ARG
          ,fl_owner_t id                           // IN: id not used
#endif
         )
{
   int ret = 0;

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s)\n",
            __func__, file->f_dentry->d_parent->d_name.name,
            file->f_dentry->d_name.name));

   if ((file->f_mode & FMODE_WRITE) == 0) {
      goto exit;
   }


   /* Flush writes to the server and return any errors */
   LOG(6, (KERN_DEBUG "VMware hgfs: %s: calling vfs_sync ... \n",
           __func__));
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 36)
   ret = vfs_fsync(file, 0);
#else
   ret = HgfsDoFsync(file->f_dentry->d_inode);
#endif

exit:
   LOG(4, (KERN_DEBUG "VMware hgfs: %s: returns %d\n",
           __func__, ret));
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFsync --
 *
 *    Called when user process calls fsync() on hgfs file.
 *
 *    The hgfs protocol doesn't support fsync explicitly yet,
 *    so for now, we flush all the pages to presumably honor the
 *    intent of an app calling fsync() which is to get the
 *    data onto persistent storage, and as things stand now we're at
 *    the whim of the hgfs server code running on the host to fsync or
 *    not if and when it pleases.
 *
 * Results:
 *    Returns zero on success. (Currently always succeeds).
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsFsync(struct file *file,            // IN: File we operate on
#if defined VMW_FSYNC_OLD
          struct dentry *dentry,        // IN: Dentry for this file
#elif defined VMW_FSYNC_31
          loff_t start,                 // IN: start of range to sync
          loff_t end,                   // IN: end of range to sync
#endif
          int datasync)                 // IN: fdatasync or fsync
{
   int ret = 0;
   loff_t startRange;
   loff_t endRange;
   struct inode *inode;

#if defined VMW_FSYNC_31
   startRange = start;
   endRange = end;
#else
   startRange = 0;
   endRange = MAX_INT64;
#endif

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s/%s, %lld, %lld, %d)\n",
           __func__,
           file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           startRange, endRange,
           datasync));

   /* Flush writes to the server and return any errors */
   inode = file->f_dentry->d_inode;
#if defined VMW_FSYNC_31
   ret = filemap_write_and_wait_range(inode->i_mapping, startRange, endRange);
#else
   ret = HgfsDoFsync(inode);
#endif

   LOG(4, (KERN_DEBUG "VMware hgfs: %s: written pages  %lld, %lld returns %d)\n",
           __func__, startRange, endRange, ret));
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsMmap --
 *
 *    Called when user process calls mmap() on hgfs file. This is a very
 *    thin wrapper function- we simply attempt to revalidate the
 *    dentry prior to calling generic_file_mmap().
 *
 * Results:
 *    Returns zero on success.
 *    Returns negative error value on failure
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

static int
HgfsMmap(struct file *file,            // IN: File we operate on
         struct vm_area_struct *vma)   // IN/OUT: VM area information
{
   int result;

   ASSERT(file);
   ASSERT(vma);
   ASSERT(file->f_dentry);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s(%s/%s)\n",
           __func__,
           file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name));

   result = HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: invalid dentry\n", __func__));
      goto out;
   }

   result = generic_file_mmap(file, vma);
  out:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRelease --
 *
 *    Called when the last user of a file closes it, i.e. when the
 *    file's f_count becomes zero.
 *
 * Results:
 *    Returns zero on success, or an error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsRelease(struct inode *inode,  // IN: Inode that this file points to
            struct file *file)    // IN: File that is getting released
{
   HgfsReq *req;
   HgfsHandle handle;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   int result = 0;

   ASSERT(inode);
   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_sb);

   handle = FILE_GET_FI_P(file)->handle;
   LOG(6, (KERN_DEBUG "VMware hgfs: %s(%s/%s, %u)\n",
           __func__,
           file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           handle));

   /*
    * This may be our last open handle to an inode, so we should flush our
    * dirty pages before closing it.
    */
   compat_filemap_write_and_wait(inode->i_mapping);

   HgfsReleaseFileInfo(file);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   opUsed = hgfsVersionClose;
   if (opUsed == HGFS_OP_CLOSE_V3) {
      HgfsRequest *header;
      HgfsRequestCloseV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestCloseV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->file = handle;
      request->reserved = 0;
      req->payloadSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestClose *request;

      request = (HgfsRequestClose *)(HGFS_REQ_PAYLOAD(req));
      request->header.id = req->id;
      request->header.op = opUsed;
      request->file = handle;
      req->payloadSize = sizeof *request;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: released handle %u\n",
                 handle));
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CLOSE_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionClose = HGFS_OP_CLOSE;
            goto retry;
         }
         break;
      default:
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: failed handle %u\n",
                 handle));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRelease: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


#ifndef VMW_SENDFILE_NONE
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSendfile --
 *
 *    sendfile() wrapper for HGFS. Note that this is for sending a file
 *    from HGFS to another filesystem (or socket). To use HGFS as the
 *    destination file in a call to sendfile(), we must implement sendpage()
 *    as well.
 *
 *    Like mmap(), we're just interested in validating the dentry and then
 *    calling into generic_file_sendfile().
 *
 * Results:
 *    Returns number of bytes written on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

#if defined VMW_SENDFILE_OLD
static ssize_t
HgfsSendfile(struct file *file,    // IN: File to read from
             loff_t *offset,       // IN/OUT: Where to start reading
             size_t count,         // IN: How much to read
             read_actor_t actor,   // IN: Routine to send a page of data
             void __user *target)  // IN: Destination file/socket
#elif defined VMW_SENDFILE_NEW
static ssize_t
HgfsSendfile(struct file *file,    // IN: File to read from
             loff_t *offset,       // IN/OUT: Where to start reading
             size_t count,         // IN: How much to read
             read_actor_t actor,   // IN: Routine to send a page of data
             void *target)         // IN: Destination file/socket
#endif
{
   ssize_t result;

   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(target);
   ASSERT(offset);
   ASSERT(actor);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSendfile: was called\n"));

   result = HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSendfile: invalid dentry\n"));
      goto out;
   }

   result = generic_file_sendfile (file, offset, count, actor, target);
  out:
   return result;

}
#endif


#ifdef VMW_SPLICE_READ
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSpliceRead --
 *
 *    splice_read() wrapper for HGFS. Note that this is for sending a file
 *    from HGFS to another filesystem (or socket). To use HGFS as the
 *    destination file in a call to splice, we must implement splice_write()
 *    as well.
 *
 *    Like mmap(), we're just interested in validating the dentry and then
 *    calling into generic_file_splice_read().
 *
 * Results:
 *    Returns number of bytes written on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static ssize_t
HgfsSpliceRead(struct file *file,            // IN: File to read from
               loff_t *offset,               // IN/OUT: Where to start reading
               struct pipe_inode_info *pipe, // IN: Pipe where to write data
               size_t len,                   // IN: How much to read
               unsigned int flags)           // IN: Various flags
{
   ssize_t result;

   ASSERT(file);
   ASSERT(file->f_dentry);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s(%s/%s, %lu@%Lu)\n",
           __func__,
           file->f_dentry->d_parent->d_name.name,
           file->f_dentry->d_name.name,
           (unsigned long) len, (unsigned long long) *offset));

   result = HgfsRevalidate(file->f_dentry);
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: invalid dentry\n", __func__));
      goto out;
   }

   result = generic_file_splice_read(file, offset, pipe, len, flags);
out:
   return result;

}
#endif


