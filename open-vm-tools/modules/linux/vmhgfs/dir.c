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

#include "cpName.h"
#include "hgfsProto.h"
#include "hgfsUtil.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"
#include "vm_basic_types.h"

/* Private functions. */
static int HgfsUnpackSearchReadReply(HgfsReq *req,
                                     HgfsAttrInfo *attr);
static int HgfsGetNextDirEntry(HgfsSuperInfo *si,
                               HgfsHandle searchHandle,
                               uint32 offset,
                               HgfsAttrInfo *attr,
                               Bool *done);
static int HgfsPackDirOpenRequest(struct inode *inode,
                                  struct file *file,
				  HgfsOp opUsed,
                                  HgfsReq *req);

/* HGFS file operations for directories. */
static int HgfsDirOpen(struct inode *inode,
                       struct file *file);
static int HgfsReaddir(struct file *file,
                       void *dirent,
                       filldir_t filldir);
static int HgfsDirRelease(struct inode *inode,
                          struct file *file);

/* HGFS file operations structure for directories. */
struct file_operations HgfsDirFileOperations = {
   .owner       = THIS_MODULE,
   .open        = HgfsDirOpen,
   .read        = generic_read_dir,
   .readdir     = HgfsReaddir,
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
                          HgfsAttrInfo *attr)  // IN/OUT: Attributes
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
       fileNameLength > HGFS_PACKET_MAX - replySize) {
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
      attr->fileName = kmalloc(fileNameLength + 1, GFP_KERNEL);
      if (attr->fileName == NULL) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackSearchReadReply: out of "
                 "memory allocating filename, ignoring\n"));
         return -ENOMEM;
      }
      memcpy(attr->fileName, fileName, fileNameLength + 1);
   } else {
      attr->fileName = NULL;
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
 *    attr->fileName gets allocated and must be freed by the caller.
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
   opUsed = atomic_read(&hgfsVersionSearchRead);
   if (atomic_read(&hgfsProtocolVersion) == HGFS_VERSION_3) {
      opUsed = HGFS_OP_SEARCH_READ_V3;
   }

   if (opUsed == HGFS_OP_SEARCH_READ_V3) {
      HgfsRequest *header;
      HgfsRequestSearchReadV3 *request;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->op = attr->requestType = opUsed;
      header->id = req->id;

      request = (HgfsRequestSearchReadV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->search = searchHandle;
      request->offset = offset;
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
         result = HgfsUnpackSearchReadReply(req, attr);
         if (result == 0 && attr->fileName == NULL) {
            /* We're at the end of the directory. */
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: end of "
                    "dir\n"));
            *done = TRUE;
         }
         break;

      case -EPROTO:
         /* Retry with Version 1 of SearchRead. Set globally. */
         if (attr->requestType == HGFS_OP_SEARCH_READ_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: Version 3 "
                    "not supported. Falling back to version 2.\n"));
            atomic_set(&hgfsVersionSearchRead, HGFS_OP_SEARCH_READ_V2);
            atomic_set(&hgfsProtocolVersion, HGFS_VERSION_OLD);
            goto retry;
         } else if (attr->requestType == HGFS_OP_SEARCH_READ_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetNextDirEntry: Version 2 "
                    "not supported. Falling back to version 1.\n"));
            atomic_set(&hgfsVersionSearchRead, HGFS_OP_SEARCH_READ);
            atomic_set(&hgfsProtocolVersion, HGFS_VERSION_OLD);
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
HgfsPackDirOpenRequest(struct inode *inode, // IN: Inode of the file to open
                       struct file *file,   // IN: File pointer for this open
                       HgfsOp opUsed,       // IN: Op to be used
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
   if (HgfsBuildPath(name, HGFS_PACKET_MAX - (requestSize - 1),
                     file->f_dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: build path failed\n"));
      return -EINVAL;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: opening \"%s\"\n",
           name));

   /* Convert to CP name. */
   result = CPName_ConvertTo(name,
                             HGFS_PACKET_MAX - (requestSize - 1),
                             name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackDirOpenRequest: CP conversion failed\n"));
      return -EINVAL;
   }

   /* Unescape the CP name. */
   result = HgfsUnescapeBuffer(name, result);
   *nameLength = (uint32) result;
   req->payloadSize = requestSize + result;

   return 0;
}


/*
 * HGFS file operations for directories.
 */

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
   HgfsReq *req;
   int result;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   HgfsHandle *replySearch;

   ASSERT(inode);
   ASSERT(inode->i_sb);
   ASSERT(file);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = atomic_read(&hgfsVersionSearchOpen);
   replySearch = &((HgfsReplySearchOpen *)HGFS_REQ_PAYLOAD(req))->search;
   if (atomic_read(&hgfsProtocolVersion) == HGFS_VERSION_3) {
      opUsed = HGFS_OP_SEARCH_OPEN_V3;
      replySearch = &((HgfsReplySearchOpenV3 *)HGFS_REP_PAYLOAD_V3(req))->search;
   }

   result = HgfsPackDirOpenRequest(inode, file, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: error packing request\n"));
      goto out;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply and check return status. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      if (result == 0) {
         result = HgfsCreateFileInfo(file, *replySearch);
         switch (result) {
	 case 0:
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: set handle to %u\n",
                    *replySearch));
            break;
         case -EPROTO:
            /* Retry with Version 1 of SearchOpen. Set globally. */
            if (opUsed == HGFS_OP_SEARCH_OPEN_V3) {
               LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: Version 3 not "
                       "supported. Falling back to version 1.\n"));
               atomic_set(&hgfsVersionSearchOpen, HGFS_OP_SEARCH_OPEN);
               atomic_set(&hgfsProtocolVersion, HGFS_VERSION_OLD);
               goto retry;
            }
            break;

         default:
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: server "
                    "returned error: %d\n", result));
            break;
         }
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirOpen: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReaddir --
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
 *     - Passing the correct d_type to filldir seems to be non-critical;
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
HgfsReaddir(struct file *file, // IN:  Directory to read from
            void *dirent,      // OUT: Buffer to copy dentries into
            filldir_t filldir) // IN:  Filler function
{
   HgfsSuperInfo *si;
   HgfsAttrInfo attr;
   uint32 d_type;    // type of dirent
   char *escName = NULL; // buf for escaped version of name
   size_t escNameLength = NAME_MAX + 1;
   int nameLength = 0;
   int result = 0;
   Bool done = FALSE;
   ino_t ino;

   ASSERT(file);
   ASSERT(dirent);

   if (!file ||
      !(file->f_dentry) ||
      !(file->f_dentry->d_inode)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReaddir: null input\n"));
      return -EFAULT;
   }

   ASSERT(file->f_dentry->d_inode->i_sb);

   si = HGFS_SB_TO_COMMON(file->f_dentry->d_inode->i_sb);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReaddir: dir with name %s, "
           "inum %lu, f_pos %Lu\n",
          file->f_dentry->d_name.name,
          file->f_dentry->d_inode->i_ino,
          file->f_pos));

   /*
    * Some day when we're out of things to do we can move this to a slab
    * allocator.
    */
   escName = kmalloc(escNameLength, GFP_KERNEL);
   if (!escName) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReaddir: out of memory allocating "
              "escaped name buffer\n"));
      return  -ENOMEM;
   }

   while (1) {
      /*
       * Nonzero result = we failed to get valid reply from server.
       * Zero result:
       *     - done == TRUE means we hit the end of the directory
       *     - Otherwise, attr.fileName has the name of the next dirent
       *
       */
      result = HgfsGetNextDirEntry(si,
                                   FILE_GET_FI_P(file)->handle,
                                   (uint32)file->f_pos,
                                   &attr,
                                   &done);
      if (result == -ENAMETOOLONG) {
         /*
          * Skip dentry if its name is too long (see below).
          *
          * XXX: If a bad server sends us bad packets, we can loop here
          * forever, as I did while testing *grumble*. Maybe we should error
          * in that case.
          */
         file->f_pos++;
         continue;
      } else if (result) {
         /* Error  */
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsReaddir: error "
                 "getting dentry\n"));
         kfree(escName);
         return result;
      }
      if (done == TRUE) {
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReaddir: end of dir reached\n"));
         break;
      }

      /*
       * Escape all non-printable characters (which for linux is just
       * "/").
       *
       * Note that normally we would first need to convert from the
       * CP name format, but that is done implicitely here since we
       * are guaranteed to have just one path component per dentry.
       */
      result = HgfsEscapeBuffer(attr.fileName,
                                strlen(attr.fileName),
                                escNameLength,
                                escName);
      kfree(attr.fileName);

      /*
       * Check the filename length.
       *
       * If the name is too long to be represented in linux, we simply
       * skip it (i.e., that file is not visible to our filesystem) by
       * incrementing file->f_pos and repeating the loop to get the
       * next dentry.
       *
       * HgfsEscapeBuffer returns a negative value if the escaped
       * output didn't fit in the specified output size, so we can
       * just check its return value.
       */
      if (result < 0) {
         /*
          * XXX: Another area where a bad server could cause us to loop
          * forever.
          */
         file->f_pos++;
         continue;
      }

      nameLength = result;

      /* Assign the correct dentry type. */
      switch (attr.type) {

      case HGFS_FILE_TYPE_SYMLINK:
         d_type = DT_LNK;
         break;

      case HGFS_FILE_TYPE_REGULAR:
         d_type = DT_REG;
         break;

      case HGFS_FILE_TYPE_DIRECTORY:
         d_type = DT_DIR;
         break;

      default:
         /*
          * XXX Should never happen. I'd put NOT_IMPLEMENTED() here
          * but if the driver ever goes in the host it's probably not
          * a good idea for an attacker to be able to hang the host
          * simply by using a bogus file type in a reply. [bac]
          */
         d_type = DT_UNKNOWN;
         break;
      }

      /*
       * It is unfortunate, but the HGFS server sends back '.' and ".."
       * when we do a SearchRead. In an ideal world, these would be faked
       * on the client, but it would be a real backwards-compatibility
       * hassle to change the behavior at this point.
       *
       * So instead, we'll take the '.' and ".." and modify their inode
       * numbers so they match what the client expects.
       */
      if (!strncmp(escName, ".", sizeof ".")) {
         ino = file->f_dentry->d_inode->i_ino;
      } else if (!strncmp(escName, "..", sizeof "..")) {
         ino = compat_parent_ino(file->f_dentry);
      } else {
         if (attr.mask & HGFS_ATTR_VALID_FILEID) {
            ino = attr.hostFileId;
         } else {
            ino = iunique(file->f_dentry->d_inode->i_sb,
                          HGFS_RESERVED_INO);
         }
      }

      /*
       * Call filldir for this dentry.
       */
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReaddir: calling filldir "
              "with \"%s\", %u, %Lu\n", escName, nameLength, file->f_pos));
      result = filldir(dirent,         /* filldir callback struct */
                       escName,        /* name of dirent */
                       nameLength,     /* length of name */
                       file->f_pos,    /* offset of dirent */
                       ino,            /* inode number (0 makes it not show) */
                       d_type);        /* type of dirent */
      if (result) {
         /*
          * This means that filldir ran out of room in the user buffer
          * it was copying into; we just break out and return, but
          * don't increment f_pos. So the next time the user calls
          * getdents, this dentry will be requested again, will get
          * retrieved again, and get copied properly to the user.
          */
         break;
      }
      file->f_pos++;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsReaddir: finished\n"));
   kfree(escName);
   return 0;
}


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
   HgfsReq *req;
   HgfsStatus replyStatus;
   HgfsHandle handle;
   HgfsOp opUsed;
   int result = 0;

   ASSERT(inode);
   ASSERT(file);
   ASSERT(file->f_dentry);
   ASSERT(file->f_dentry->d_sb);

   handle = FILE_GET_FI_P(file)->handle;
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: close fh %u\n", handle));

   HgfsReleaseFileInfo(file);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   if (atomic_read(&hgfsProtocolVersion) == HGFS_VERSION_3) {
      HgfsRequestSearchCloseV3 *request;
      HgfsRequest *header;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed = HGFS_OP_SEARCH_CLOSE_V3;

      request = (HgfsRequestSearchCloseV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->search = handle;
      req->payloadSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestSearchClose *request;

      request = (HgfsRequestSearchClose *)(HGFS_REQ_PAYLOAD(req));
      request->header.id = req->id;
      request->header.op = opUsed = HGFS_OP_SEARCH_CLOSE;
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
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: release handle %u\n",
                 handle));
         break;
      case -EPROTO:
         /* Retry with Version 2 of Open. Set globally. */
         if (opUsed == HGFS_OP_SEARCH_CLOSE_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            atomic_set(&hgfsProtocolVersion, HGFS_VERSION_OLD);
            goto retry;
         }
         break;
      default:
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: failed handle %u\n",
                 handle));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDirRelease: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsOpen: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}
