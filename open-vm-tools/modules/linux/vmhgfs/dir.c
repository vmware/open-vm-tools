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
 * dir.c --
 *
 * Directory operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/module.h>
#include "compat_fs.h"
#include "compat_kernel.h"
#include "compat_slab.h"
#include "compat_mutex.h"

#include "cpName.h"
#include "hgfsEscape.h"
#include "hgfsProto.h"
#include "hgfsUtil.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"
#include "vm_basic_types.h"

/* Private functions. */
static int HgfsPrivateDirReOpen(struct file *file);
static int HgfsPrivateDirOpen(struct file *file,
                              HgfsHandle *handle);
static int HgfsPrivateDirRelease(struct file *file,
                                 HgfsHandle handle);
static int HgfsUnpackSearchReadReply(HgfsReq *req,
                                     HgfsAttrInfo *attr,
                                     char **entryName);
static int HgfsGetNextDirEntry(HgfsSuperInfo *si,
                               HgfsHandle searchHandle,
                               uint32 offset,
                               HgfsAttrInfo *attr,
                               char **entryName,
                               Bool *done);
static int HgfsPackDirOpenRequest(struct file *file,
                                  HgfsOp opUsed,
                                  HgfsReq *req);
static Bool
HgfsReaddirFillEntry(filldir_t filldirCb,
                     void *context,
                     char *entryName,
                     uint32 entryNameLength,
                     loff_t entryPos,
                     ino_t entryIno,
                     uint32 entryType);

/* HGFS file operations for directories. */
static int HgfsDirOpen(struct inode *inode,
                       struct file *file);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
static int HgfsReaddir(struct file *file,
                       struct dir_context *ctx);
#else
static int HgfsReaddir(struct file *file,
                       void *dirent,
                       filldir_t filldir);
#endif
static int HgfsDirRelease(struct inode *inode,
                          struct file *file);
static loff_t HgfsDirLlseek(struct file *file,
                            loff_t offset,
                            int origin);

/* HGFS file operations structure for directories. */
struct file_operations HgfsDirFileOperations = {
   .llseek      = HgfsDirLlseek,
   .owner       = THIS_MODULE,
   .open        = HgfsDirOpen,
   .read        = generic_read_dir,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
   .iterate     = HgfsReaddir,
#else
   .readdir     = HgfsReaddir,
#endif
   .release     = HgfsDirRelease,
};

/*
 * Private function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsUnpackSearchReadReply --
 *
 *    This function abstracts the differences between a SearchReadV1 and
 *    a SearchReadV2. The caller provides the packet containing the reply
 *    and we populate the AttrInfo with version-independent information.
 *
 *    Note that attr->requestType has already been populated so that we
 *    know whether to expect a V1 or V2 reply.
 *
 * Results:
 *    0 on success, anything else on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
static int
HgfsUnpackSearchReadReply(HgfsReq *req,        // IN: Reply packet
                          HgfsAttrInfo *attr,  // IN/OUT: Attributes
                          char **entryName)    // OUT: file name
{
   char *fileName;
   uint32 fileNameLength;
   uint32 replySize;
   int result;

   ASSERT(req);
   ASSERT(attr);

   result = HgfsUnpackCommonAttr(req, attr);
   if (result != 0) {
      return result;
   }

   switch(attr->requestType) {
   case HGFS_OP_SEARCH_READ_V3: {
      HgfsReplySearchReadV3 *replyV3;
      HgfsDirEntry *dirent;

      /* Currently V3 returns only 1 entry. */
      replyV3 = (HgfsReplySearchReadV3 *)(HGFS_REP_PAYLOAD_V3(req));
      replyV3->count = 1;
      replySize = HGFS_REP_PAYLOAD_SIZE_V3(replyV3) + sizeof *dirent;
      dirent = (HgfsDirEntry *)replyV3->payload;
      fileName = dirent->fileName.name;
      fileNameLength = dirent->fileName.length;
      break;
   }
   case HGFS_OP_SEARCH_READ_V2: {
      HgfsReplySearchReadV2 *replyV2;

      replyV2 = (HgfsReplySearchReadV2 *)(HGFS_REQ_PAYLOAD(req));
      replySize = sizeof *replyV2;
      fileName = replyV2->fileName.name;
      fileNameLength = replyV2->fileName.length;
      break;
   }
   case HGFS_OP_SEARCH_READ: {
      HgfsReplySearchRead *replyV1;

      replyV1 = (HgfsReplySearchRead *)(HGFS_REQ_PAYLOAD(req));
      replySize = sizeof *replyV1;
      fileName = replyV1->fileName.name;
      fileNameLength = replyV1->fileName.length;
      break;
   }
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackSearchReadReply: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /*
    * Make sure name length is legal.
    */
   if (fileNameLength > NAME_MAX ||
       fileNameLength > req->bufferSize - replySize) {
      return -ENAMETOOLONG;
   }

   /*
    * If the size of the name is valid (meaning the end of the directory has
    * not yet been reached), copy the name to the AttrInfo struct.
    *
    * XXX: This operation happens often and the length of the filename is
    * bounded by NAME_MAX. Perhaps I should just put a statically-sized
    * array in HgfsAttrInfo and use a slab allocator to allocate the struct.
    */
   if (fileNameLength > 0) {
      /* Sanity check on name length. */
      if (fileNameLength != strlen(fileName)) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackSearchReadReply: name "
                 "length mismatch %u/%Zu, name \"%s\"\n",
                 fileNameLength, strlen(fileName), fileName));
         return -EPROTO;
      }
      *entryName = kmalloc(fileNameLength + 1, GFP_KERNEL);
      if (*entryName == NULL) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackSearchReadReply: out of "
                 "memory allocating filename, ignoring\n"));
         return -ENOMEM;
      }
      memcpy(*entryName, fileName, fileNameLength + 1);
   } else {
      *entryName = NULL;
   }
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetNextDirEntry --
 *
 *    Get the directory entry with the given offset from the server.
 *
 *    fileName gets allocated and must be freed by the caller.
 *
 * Results:
 *    Returns zero on success, negative error on failure. If the
 *    dentry's name is too long, -ENAMETOOLONG is returned.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsGetNextDirEntry(HgfsSuperInfo *si,       // IN: Superinfo for this SB
                    HgfsHandle searchHandle, // IN: Handle of dir
                    uint32 offset,           // IN: Offset of next dentry to get
                    HgfsAttrInfo *attr,      // OUT: File attributes of dentry
                    char **entryName,        // OUT: File name
                    Bool *done)              // OUT: Set true when there are
                                             // no more dentries
{
   HgfsReq *req;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   int result = 0;

   ASSERT(si);
   ASSERT(attr);
   ASSERT(done);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: out of memory "
              "while getting new request\n"));
      return -ENOMEM;
   }

  retry:
   opUsed = hgfsVersionSearchRead;
   if (opUsed == HGFS_OP_SEARCH_READ_V3) {
      HgfsRequest *header;
      HgfsRequestSearchReadV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->op = attr->requestType = opUsed;
      header->id = req->id;

      request = (HgfsRequestSearchReadV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->search = searchHandle;
      request->offset = offset;
      request->flags = 0;
      request->reserved = 0;
      req->payloadSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestSearchRead *request;

      request = (HgfsRequestSearchRead *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = attr->requestType = opUsed;
      request->header.id = req->id;
      request->search = searchHandle;
      request->offset = offset;
      req->payloadSize = sizeof *request;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch(result) {
      case 0:
         result = HgfsUnpackSearchReadReply(req, attr, entryName);
         if (result == 0 && *entryName == NULL) {
            /* We're at the end of the directory. */
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: end of "
                    "dir\n"));
            *done = TRUE;
         }
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (attr->requestType == HGFS_OP_SEARCH_READ_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: Version 3 "
                    "not supported. Falling back to version 2.\n"));
            hgfsVersionSearchRead = HGFS_OP_SEARCH_READ_V2;
            goto retry;
         } else if (attr->requestType == HGFS_OP_SEARCH_READ_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: Version 2 "
                    "not supported. Falling back to version 1.\n"));
            hgfsVersionSearchRead = HGFS_OP_SEARCH_READ;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: unknown error: "
              "%d\n", result));
   }

   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackDirOpenRequest --
 *
 *    Setup the directory open request, depending on the op version.
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
HgfsPackDirOpenRequest(struct file *file,   // IN: File pointer for this open
                       HgfsOp opUsed,       // IN: Op to be used
                       HgfsReq *req)        // IN/OUT: Packet to write into
{
   char *name;
   uint32 *nameLength;
   size_t requestSize;
   int result;

   ASSERT(file);
   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_SEARCH_OPEN_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestSearchOpenV3 *requestV3;

      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestSearchOpenV3 *)HGFS_REQ_PAYLOAD_V3(req);

      /* We'll use these later. */
      name = requestV3->dirName.name;
      nameLength = &requestV3->dirName.length;
      requestV3->dirName.flags = 0;
      requestV3->dirName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->dirName.fid = HGFS_INVALID_HANDLE;
      requestV3->reserved = 0;
      requestSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);
      break;
   }

   case HGFS_OP_SEARCH_OPEN: {
      HgfsRequestSearchOpen *request;

      request = (HgfsRequestSearchOpen *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = opUsed;
      request->header.id = req->id;

      /* We'll use these later. */
      name = request->dirName.name;
      nameLength = &request->dirName.length;
      requestSize = sizeof *request;
      break;
   }

   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(name, req->bufferSize - (requestSize - 1),
                     file->f_dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: build path failed\n"));
      return -EINVAL;
   }
   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: opening \"%s\"\n",
           name));

   /* Convert to CP name. */
   result = CPName_ConvertTo(name,
                             req->bufferSize - (requestSize - 1),
                             name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: CP conversion failed\n"));
      return -EINVAL;
   }

   *nameLength = (uint32) result;
   req->payloadSize = requestSize + result;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateDirOpen --
 *
 *    Called by HgfsDirOpen() and HgfsReaddir() routines.
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
HgfsPrivateDirOpen(struct file *file,    // IN: File pointer for this open
                   HgfsHandle *handle)   // IN: Hgfs handle
{
   HgfsReq *req;
   int result;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   HgfsHandle *replySearch;

   ASSERT(file);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = hgfsVersionSearchOpen;
   if (opUsed == HGFS_OP_SEARCH_OPEN_V3) {
      replySearch = &((HgfsReplySearchOpenV3 *)HGFS_REP_PAYLOAD_V3(req))->search;
   } else {
      replySearch = &((HgfsReplySearchOpen *)HGFS_REQ_PAYLOAD(req))->search;
   }

   result = HgfsPackDirOpenRequest(file, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen error packing request\n"));
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
         /* Save the handle value */
         *handle = *replySearch;
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: Handle returned = %u\n",
                    *replySearch));
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_SEARCH_OPEN_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionSearchOpen = HGFS_OP_SEARCH_OPEN;
            goto retry;
         }
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: server "
                 "returned error: %d\n", result));
         break;

      default:
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: server "
                  "returned error: %d\n", result));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirOpen: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateDirRelease --
 *
 *    Called by HgfsDirRelease() and HgfsReaddir() routines.
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
HgfsPrivateDirRelease(struct file *file,   // IN: File for the dir getting released
                      HgfsHandle handle)   // IN: Hgfs handle
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   HgfsOp opUsed;
   int result = 0;

   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_sb);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: close fh %u\n", handle));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   opUsed = hgfsVersionSearchClose;
   if (opUsed == HGFS_OP_SEARCH_CLOSE_V3) {
      HgfsRequestSearchCloseV3 *request;
      HgfsRequest *header;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestSearchCloseV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->search = handle;
      request->reserved = 0;
      req->payloadSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestSearchClose *request;

      request = (HgfsRequestSearchClose *)(HGFS_REQ_PAYLOAD(req));
      request->header.id = req->id;
      request->header.op = opUsed;
      request->search = handle;
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
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: release handle %u\n",
                 handle));
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_SEARCH_CLOSE_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionSearchClose = HGFS_OP_SEARCH_CLOSE;
            goto retry;
         }
         break;
      default:
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: failed handle %u\n",
                 handle));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPrivateDirRelease: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateDirReOpen --
 *
 *    Reopens the file. Called by HgfsReaddir() routine.
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
HgfsPrivateDirReOpen(struct file *file)   // IN: File pointer for this open
{
   int result = 0;
   HgfsHandle *handle = &FILE_GET_FI_P(file)->handle;
   LOG(4, (KERN_DEBUG "HgfsPrivateDirReOpen: Directory handle in invalid;"
           "Reopening ...\n"));

   result = HgfsPrivateDirRelease(file, *handle);
   if (result) {
      return result;
   }

   result = HgfsPrivateDirOpen(file, handle);
   if (result) {
      return result;
   }

   FILE_GET_FI_P(file)->isStale = FALSE;

   return result;
}


/*
 * HGFS file operations for directories.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsDirLlseek --
 *
 *    Called whenever a process does rewinddir() or telldir()/seekdir().
 *
 * Results:
 *    Returns zero if on success, error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static loff_t
HgfsDirLlseek(struct file *file,
              loff_t offset,
              int origin)
{
   struct dentry *dentry = file->f_dentry;
   struct inode *inode = dentry->d_inode;
   compat_mutex_t *mtx;

   LOG(4, (KERN_DEBUG "Got llseek call with origin = %d, offset = %u,"
           "pos = %u\n", origin, (uint32)offset, (uint32)file->f_pos));

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
   mtx = &inode->i_sem;
#else
   mtx = &inode->i_mutex;
#endif

   compat_mutex_lock(mtx);

   switch(origin) {

   /* SEEK_CUR */
   case 1: offset += file->f_pos;
           break;
   /* SEEK_SET */
   case 0: break;

   /* SEEK_END */
   case 2:
   default: offset = -EINVAL;
            break;
   }

   if (offset < 0) {
      offset = -EINVAL;
      goto out;
   }

   if (offset != file->f_pos) {
      file->f_pos = offset;
   }

   /*
    * rewinddir() semantics says that It causes the directory stream
    * to refer to the current state of the corresponding directory,
    * as a call to opendir would have done. So when rewinddir() happens,
    * we mark current directory as stale, so that subsequent readdir()
    * call will reopen() the directory.
    *
    * XXX telldir()/seekdir() semantics does not say that we need to refer
    * to the current state of a directory. However, an application that does
    * following: telldir() -> rmdir(current_entry) -> seekdir() and checking
    * whether entry was deleted or not, will break. I have no evidence of an
    * application relying on above behavior, so let's not incur extra cost
    * by reopening directory on telldir()/seekdir() combination. Note: A special
    * case of telldir()/seekdir() to offset 0 will behave same as rewinddir().
    */
   if (!file->f_pos) {
      FILE_GET_FI_P(file)->isStale = TRUE;
   }

out:
   compat_mutex_unlock(mtx);
   return offset;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsDirOpen --
 *
 *    Called whenever a process opens a directory in our filesystem.
 *
 *    We send a "Search Open" request to the server with the name
 *    stored in this file's inode. If the Open succeeds, we store the
 *    search handle sent by the server in the file struct so it can be
 *    accessed by readdir and close.
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
HgfsDirOpen(struct inode *inode,  // IN: Inode of the dir to open
            struct file *file)    // IN: File pointer for this open
{
   int result;

   HgfsHandle handle;

   ASSERT(inode);
   ASSERT(inode->i_sb);
   ASSERT(file);

   result = HgfsPrivateDirOpen(file, &handle);
   if (!result) {
      result = HgfsCreateFileInfo(file, handle);
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddirRefreshEntries --
 *
 *    refresh the file entries if the handle is stale by reopening.
 *
 * Results:
 *    Zero on success, otherwise failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsReaddirRefreshEntries(struct file *file)    // IN: File pointer for this open
{
   int result = 0;

   /*
    * rm -rf 6.10+ breaks because it does following:
    * an 'fd = open()' on a directory, followed by unlinkat()
    * which removes an entry from the directory it and then
    * fdopendir(fd). We get a call on open() but not on fdopendir(),
    * which means that we do not reflect the action of unlinkat(),
    * and thus rm -rf gets confused and marking entry as unremovable.
    * Note that this problem exists because hgfsServer reads all
    * the directory entries at open(). Interested reader may look at
    * coreutils/src/remove.c file.
    *
    * So as a workaround, we ask the server to populate entries on
    * first readdir() call rather than opendir(). This effect is
    * achieved by closing and reopening the directory. Grrr!
    *
    * XXX We should get rid of this code when/if we remove the above
    * behavior from hgfsServer.
    */
   if (FILE_GET_FI_P(file)->isStale) {
      result = HgfsPrivateDirReOpen(file);
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: error: stale handle (%s) return %d)\n",
            __func__, file->f_dentry->d_name.name, result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetFileInode --
 *
 *    Get file inode from the hgfs attributes or generate from the super block.
 *
 * Results:
 *    The inode entry.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static ino_t
HgfsGetFileInode(HgfsAttrInfo const *attr,     // IN: Attrs to use
                 struct super_block *sb)       // IN: Superblock of this fs
{
   ino_t inodeEntry;
   uint64 tempIno;
   HgfsSuperInfo *si;

   ASSERT(attr);
   ASSERT(sb);

   si = HGFS_SB_TO_COMMON(sb);

   if ((si->mntFlags & HGFS_MNT_SERVER_INUM) != 0 &&
       (attr->mask & HGFS_ATTR_VALID_FILEID) != 0) {
      tempIno = attr->hostFileId;
   } else {
      tempIno = iunique(sb, HGFS_RESERVED_INO);
   }

   inodeEntry = HgfsUniqueidToIno(tempIno);
   LOG(4, (KERN_DEBUG "VMware hgfs: %s: return %lu\n", __func__, inodeEntry));
   return inodeEntry;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetFileType --
 *
 *    Get file type according to the hgfs attributes.
 *
 * Results:
 *    The file type.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static uint32
HgfsGetFileType(HgfsAttrInfo const *attr)     // IN: Attrs to use
{
   uint32 type;

   ASSERT(attr);

   switch (attr->type) {
   case HGFS_FILE_TYPE_SYMLINK:
      type = DT_LNK;
      break;

   case HGFS_FILE_TYPE_REGULAR:
      type = DT_REG;
      break;

   case HGFS_FILE_TYPE_DIRECTORY:
      type = DT_DIR;
      break;

   default:
      /*
       * XXX Should never happen. I'd put NOT_IMPLEMENTED() here
       * but if the driver ever goes in the host it's probably not
       * a good idea for an attacker to be able to hang the host
       * simply by using a bogus file type in a reply. [bac]
       *
       * Well it happens! Refer bug 548177 for details. In short,
       * when the user deletes a share, we hit this code path.
       *
       */
      type = DT_UNKNOWN;
      break;
   }

   LOG(4, (KERN_DEBUG "VMware hgfs: %s: return %d\n", __func__, type));
   return type;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddirNextEntry --
 *
 *    Called whenever a process opens a directory in our filesystem.
 *
 *    We send a "Search Open" request to the server with the name
 *    stored in this file's inode. If the Open succeeds, we store the
 *    search handle sent by the server in the file struct so it can be
 *    accessed by readdir and close.
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
HgfsReaddirNextEntry(struct file *file,              // IN: file
                     loff_t entryPos,                // IN: position
                     Bool dotAndDotDotIgnore,        // IN: ignore "." and ".."
                     size_t entryNameBufLen,         // IN: name buffer length
                     char *entryName,                // OUT: entry name
                     uint32 *entryNameLength,        // OUT: max name length
                     ino_t *entryIno,                // OUT: inode entry number
                     uint32 *entryType,              // OUT: entry type
                     Bool *entryIgnore,              // OUT: ignore this entry or not
                     Bool *entryEnd)                 // OUT: no more entries
{
   HgfsSuperInfo *si;
   HgfsAttrInfo entryAttrs;
   char *fileName = NULL;
   int result;

   ASSERT(file->f_dentry->d_inode->i_sb);

   si = HGFS_SB_TO_COMMON(file->f_dentry->d_inode->i_sb);
   *entryIgnore = FALSE;

   /*
    * Nonzero result = we failed to get valid reply from server.
    * Zero result:
    *     - done == TRUE means we hit the end of the directory
    *     - Otherwise, fileName has the name of the next dirent
    *
    */

   result = HgfsGetNextDirEntry(si,
                                FILE_GET_FI_P(file)->handle,
                                (uint32)entryPos,
                                &entryAttrs,
                                &fileName,
                                entryEnd);
   if (result == -ENAMETOOLONG) {
      /*
       * Skip dentry if its name is too long (see below).
       *
       * XXX: If a bad server sends us bad packets, we can loop here
       * forever, as I did while testing *grumble*. Maybe we should error
       * in that case.
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: error getnextdentry name %d\n",
               __func__, result));
      *entryIgnore = TRUE;
      result = 0;
      goto exit;
   } else if (result) {
      /* Error  */
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: error getnextdentry %d\n",
               __func__, result));
      goto exit;
   }

   if (*entryEnd) {
      LOG(10, (KERN_DEBUG "VMware hgfs: %s: end of dir reached\n", __func__));
      goto exit;
   }

   /*
    * Escape all non-printable characters (which for linux is just
    * "/").
    *
    * Note that normally we would first need to convert from the
    * CP name format, but that is done implicitely here since we
    * are guaranteed to have just one path component per dentry.
    */
   result = HgfsEscape_Do(fileName, strlen(fileName),
                          entryNameBufLen, entryName);
   kfree(fileName);
   fileName = NULL;

   /*
    * Check the filename length.
    *
    * If the name is too long to be represented in linux, we simply
    * skip it (i.e., that file is not visible to our filesystem).
    *
    * HgfsEscape_Do returns a negative value if the escaped
    * output didn't fit in the specified output size, so we can
    * just check its return value.
    */
   if (result < 0) {
      /*
       * XXX: Another area where a bad server could cause us to loop
       * forever.
       */
      *entryIgnore = TRUE;
      result = 0;
      goto exit;
   }

   *entryNameLength = result;
   result = 0;

   /*
    * It is unfortunate, but the HGFS server sends back '.' and ".."
    * when we do a SearchRead. In an ideal world, these would be faked
    * on the client, but it would be a real backwards-compatibility
    * hassle to change the behavior at this point.
    *
    * So instead, we'll take the '.' and ".." and modify their inode
    * numbers so they match what the client expects.
    */
   if (!strncmp(entryName, ".", sizeof ".")) {
      if (!dotAndDotDotIgnore) {
         *entryIno = file->f_dentry->d_inode->i_ino;
      } else {
         *entryIgnore = TRUE;
      }
   } else if (!strncmp(entryName, "..", sizeof "..")) {
      if (!dotAndDotDotIgnore) {
         *entryIno = compat_parent_ino(file->f_dentry);
      } else {
         *entryIgnore = TRUE;
      }
   } else {
     *entryIno = HgfsGetFileInode(&entryAttrs, file->f_dentry->d_inode->i_sb);
   }

   if (*entryIgnore) {
      goto exit;
   }

   /* Assign the correct dentry type. */
   *entryType = HgfsGetFileType(&entryAttrs);

exit:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsDoReaddir --
 *
 *    Handle a readdir request. See details below if interested.
 *
 *    Readdir is a bit complicated, and is best understood by reading
 *    the code. For the impatient, here is an overview of the major
 *    moving parts [bac]:
 *
 *     - Getdents syscall calls readdir, which is supposed to call
 *       filldir some number of times.
 *     - Each time it's called, filldir updates a struct with the
 *       number of bytes copied thus far, and sets an error code if
 *       appropriate.
 *     - When readdir returns, getdents checks the struct to see if
 *       any dentries were copied, and if so returns the byte count.
 *       Otherwise, it returns the error from the struct (which should
 *       still be zero if filldir was never called).
 *
 *       A consequence of this last fact is that if there are no more
 *       dentries, then readdir should NOT call filldir, and should
 *       return from readdir with a non-error.
 *
 *    Other notes:
 *
 *     - Passing an inum of zero to filldir doesn't work. At a minimum,
 *       you have to make up a bogus inum for each dentry.
 *     - Passing the correct entryType to filldir seems to be non-critical;
 *       apparently most programs (such as ls) stat each file if they
 *       really want to know what type it is. However, passing the
 *       correct type means that ls doesn't bother calling stat on
 *       directories, and that saves an entire round trip per dirctory
 *       dentry.
 *
 * Results:
 *    Returns zero if on success, negative error on failure.
 *    (According to /fs/readdir.c, any non-negative return value
 *    means it succeeded).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsDoReaddir(struct file *file,         // IN:
              Bool dotAndDotDotIgnore,   // IN: ignore "." and ".."
              filldir_t filldirCb,       // IN: system filler callback
              void *filldirCtx,          // IN/OUT: system filler context
              loff_t *fillPos,           // IN/OUT: fill entry position
              loff_t *currentPos)        // IN/OUT: current position
{
   char *entryName = NULL; // buf for escaped version of name
   size_t entryNameBufLen = NAME_MAX + 1;
   int entryNameLength = 0;
   int result = 0;
   Bool entryEnd = FALSE;

   ASSERT(file);
   ASSERT(filldirCtx);

   if (!file ||
      !(file->f_dentry) ||
      !(file->f_dentry->d_inode)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReaddir: null input\n"));
      return -EFAULT;
   }

   LOG(4, (KERN_DEBUG "VMware hgfs: %s(%s, inum %lu, pos %Lu)\n",
          __func__,
          file->f_dentry->d_name.name,
          file->f_dentry->d_inode->i_ino,
          *currentPos));

   /*
    * Refresh entries if required. See rm -rf 6.10+ breaking issue.
    */
   result = HgfsReaddirRefreshEntries(file);
   if (result != 0) {
      return result;
   }

   /*
    * Some day when we're out of things to do we can move this to a slab
    * allocator.
    */
   entryName = kmalloc(entryNameBufLen, GFP_KERNEL);
   if (entryName == NULL) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReaddir: out of memory allocating "
              "escaped name buffer\n"));
      return -ENOMEM;
   }

   while (!entryEnd) {
      Bool entryIgnore;
      ino_t entryIno = 0;
      uint32 entryType = DT_UNKNOWN;

      result = HgfsReaddirNextEntry(file,
                                    *currentPos,
                                    dotAndDotDotIgnore,
                                    entryNameBufLen,
                                    entryName,
                                    &entryNameLength,
                                    &entryIno,
                                    &entryType,
                                    &entryIgnore,
                                    &entryEnd);

      if (result != 0) {
         /* An error occurred retrieving the entry, so exit. */
         break;
      }

      if (entryEnd) {
         LOG(10, (KERN_DEBUG "VMware hgfs: %s: end of dir reached\n", __func__));
         continue;
      }

      if (entryIgnore) {
         *currentPos += 1;
         continue;
      }

      /*
       * Call the HGFS wrapper to the system fill function to set this dentry.
       */
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: dir_emit(%s, %u, @ (fill %Lu HGFS %Lu)\n",
              __func__, entryName, entryNameLength, *fillPos, *currentPos));
      if (!HgfsReaddirFillEntry(filldirCb,        /* filldir callback function */
                                filldirCtx,       /* filldir callback struct */
                                entryName,        /* name of dirent */
                                entryNameLength,  /* length of name */
                                *fillPos,         /* fill entry position */
                                entryIno,         /* inode number (0 makes it not show) */
                                entryType)) {     /* type of dirent */
         /*
          * This means that dir_emit ran out of room in the user buffer
          * it was copying into; we just break out and return, but
          * don't increment f_pos. So the next time the user calls
          * getdents, this dentry will be requested again, will get
          * retrieved again, and get copied properly to the user.
          */
         result = 0;
         break;
      }
      *currentPos += 1;
      *fillPos += 1;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: return\n",__func__));
   kfree(entryName);
   return 0;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddir --
 *
 *    Handle a readdir request.
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
HgfsReaddir(struct file *file,         // IN:
            struct dir_context *ctx)   // IN:
{
   HgfsFileInfo *fInfo = FILE_GET_FI_P(file);

   if (0 == ctx->pos) {
      fInfo->direntPos = 0;
   }

   /* If either dot and dotdot are filled in for us we can exit. */
   if (!dir_emit_dots(file, ctx)) {
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: dir_emit_dots(%s, @ %Lu)\n",
              __func__, file->f_dentry->d_name.name, ctx->pos));
      return 0;
   }

   /* It is sufficient to pass the context as it contains the filler function. */
   return HgfsDoReaddir(file, TRUE, NULL, ctx, &ctx->pos, &fInfo->direntPos);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddirFillEntry --
 *
 *    Fill a readdir entry.
 *
 *    Failure means that fill ran out of room in the user buffer
 *    it was copying into.
 *
 * Results:
 *    Returns TRUE on success, or FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsReaddirFillEntry(filldir_t filldirCb,            // IN: System filler callback
                     void *filldirCtx,               // IN/OUT: System filler context
                     char *entryName,                // IN: entry name
                     uint32 entryNameLength,         // IN: max name length
                     loff_t entryPos,                // IN: position = (ctx-pos)
                     ino_t entryIno,                 // IN: inode entry number
                     uint32 entryType)               // IN: entry type
{
   struct dir_context *ctx = filldirCtx;
   Bool result;

   ASSERT(filldirCb == NULL);   /* Contained within the context structure. */
   ASSERT(ctx != NULL);
   ASSERT(ctx->pos == entryPos);
   ASSERT(entryName != NULL);
   ASSERT(entryNameLength != 0);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: dir_emit(%s, %u, %Lu)\n",
            __func__, entryName, entryNameLength, ctx->pos));

   result = dir_emit(ctx,              /* filldir callback struct */
                     entryName,        /* name of dirent */
                     entryNameLength,  /* length of name */
                     entryIno,         /* inode number (0 makes it not show) */
                     entryType);       /* type of dirent */
   return result;
}
#else


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddir --
 *
 *    Handle a readdir request.
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
HgfsReaddir(struct file *file, // IN:  Directory to read from
            void *dirent,      // OUT: Buffer to copy dentries into
            filldir_t filldir) // IN:  Filler function
{
   HgfsFileInfo *fInfo = FILE_GET_FI_P(file);

   if (0 == file->f_pos) {
      fInfo->direntPos = 0;
   }

   return HgfsDoReaddir(file, FALSE, filldir, dirent, &file->f_pos, &fInfo->direntPos);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddirFillEntry --
 *
 *    Fill a readdir entry.
 *
 *    Failure means that fill ran out of room in the user buffer
 *    it was copying into.
 *
 * Results:
 *    Returns TRUE on success, or FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsReaddirFillEntry(filldir_t filldirCb,            // IN: System filler callback
                     void *filldirCtx,               // IN/OUT: System filler context
                     char *entryName,                // IN: entry name
                     uint32 entryNameLength,         // IN: max name length
                     loff_t entryPos,                // IN: position
                     ino_t entryIno,                 // IN: inode entry number
                     uint32 entryType)               // IN: entry type
{
   Bool result = TRUE;
   int fillResult;

   ASSERT(filldirCb != NULL);
   ASSERT(filldirCtx != NULL);
   ASSERT(entryName != NULL);
   ASSERT(entryNameLength != 0);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: calling filldir(%s, %u, %Lu\n",
           __func__, entryName, entryNameLength, entryPos));

   fillResult = filldirCb(filldirCtx,       /* filldir callback struct */
                          entryName,        /* name of dirent */
                          entryNameLength,  /* length of name */
                          entryPos,         /* offset of dirent */
                          entryIno,         /* inode number (0 makes it not show) */
                          entryType);       /* type of dirent */

   if (fillResult != 0) {
      result = FALSE;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: %s: return %d\n", __func__, result));
   return result;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HgfsDirRelease --
 *
 *    Called when the last reader of a directory closes it, i.e. when
 *    the directory's file f_count field becomes zero.
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
HgfsDirRelease(struct inode *inode,  // IN: Inode that the file* points to
               struct file *file)    // IN: File for the dir getting released
{
   HgfsHandle handle;

   ASSERT(inode);
   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_sb);

   handle = FILE_GET_FI_P(file)->handle;

   HgfsReleaseFileInfo(file);

   return HgfsPrivateDirRelease(file, handle);
}
