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
 * inode.c --
 *
 * Inode operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/pagemap.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
#include <linux/namei.h>
#endif
#include <linux/highmem.h>
#include <linux/time.h> // for current_fs_time

#include "compat_cred.h"
#include "compat_dcache.h"
#include "compat_fs.h"
#include "compat_kernel.h"
#include "compat_mm.h"
#include "compat_page-flags.h"
#include "compat_spinlock.h"
#include "compat_version.h"

#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsProto.h"
#include "hgfsUtil.h"
#include "inode.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"


#if defined VMW_DCOUNT_311 || LINUX_VERSION_CODE >= KERNEL_VERSION(3, 11, 0)
/*
 * Linux Kernel versions that are version 3.11 version and newer or are compatible
 * by having the d_count function replacement backported.
 */
#define hgfs_d_count(dentry) d_count(dentry)
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
/*
 * Kernel versions that are not 3.11 version compatible or are just older will
 * use the d_count field.
 */
#define hgfs_d_count(dentry) dentry->d_count
#else
#define hgfs_d_count(dentry) atomic_read(&dentry->d_count)
#endif

#if defined VMW_DALIAS_319 || LINUX_VERSION_CODE >= KERNEL_VERSION(3, 19, 0)
/*
 * Linux Kernel versions that are version 3.19 and newer or are compatible
 * by having the d_alias field moved into a union backported.
 */
#define hgfs_d_alias() d_u.d_alias
#else
/*
 * Kernel versions that are not 3.19 version compatible or are just older will
 * use the d_alias field directly.
 */
#define hgfs_d_alias() d_alias
#endif

/* Private functions. */
static int HgfsDelete(struct inode *dir,
                      struct dentry *dentry,
                      HgfsOp op);
static int HgfsPackSetattrRequest(struct iattr *iattr,
                                  struct dentry *dentry,
                                  Bool allowHandleReuse,
                                  HgfsOp opUsed,
                                  HgfsReq *req,
                                  Bool *changed);
static int HgfsPackCreateDirRequest(struct dentry *dentry,
                                    compat_umode_t mode,
				    HgfsOp opUsed,
                                    HgfsReq *req);
static int HgfsTruncatePages(struct inode *inode,
                             loff_t newSize);
static int HgfsPackSymlinkCreateRequest(struct dentry *dentry,
                                        const char *symname,
                                        HgfsOp opUsed,
                                        HgfsReq *req);

/* HGFS inode operations. */
static int HgfsCreate(struct inode *dir,
                      struct dentry *dentry,
                      compat_umode_t mode,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                      bool excl
#else
                      struct nameidata *nd
#endif
);
static struct dentry *HgfsLookup(struct inode *dir,
                                 struct dentry *dentry,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
                                 unsigned int flags
#else
                                 struct nameidata *nd
#endif
);
static int HgfsMkdir(struct inode *dir,
                     struct dentry *dentry,
                     compat_umode_t mode);
static int HgfsRmdir(struct inode *dir,
                     struct dentry *dentry);
static int HgfsUnlink(struct inode *dir,
                      struct dentry *dentry);
static int HgfsRename(struct inode *oldDir,
                      struct dentry *oldDentry,
                      struct inode *newDir,
                      struct dentry *newDentry);
static int HgfsSymlink(struct inode *dir,
                       struct dentry *dentry,
                       const char *symname);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
static int HgfsPermission(struct inode *inode,
                          int mask,
                          struct nameidata *nameidata);
#elif defined(IPERM_FLAG_RCU)
static int HgfsPermission(struct inode *inode,
                          int mask,
                          unsigned int flags);
#else
static int HgfsPermission(struct inode *inode,
                          int mask);
#endif
static int HgfsGetattr(struct vfsmount *mnt,
                       struct dentry *dentry,
                       struct kstat *stat);

#define HGFS_CREATE_DIR_MASK (HGFS_CREATE_DIR_VALID_FILE_NAME | \
                              HGFS_CREATE_DIR_VALID_SPECIAL_PERMS | \
                              HGFS_CREATE_DIR_VALID_OWNER_PERMS | \
                              HGFS_CREATE_DIR_VALID_GROUP_PERMS | \
                              HGFS_CREATE_DIR_VALID_OTHER_PERMS)

/* HGFS inode operations structure for directories. */
struct inode_operations HgfsDirInodeOperations = {
   /* Optional */
   .create      = HgfsCreate,

   /* Optional */
   .mkdir       = HgfsMkdir,

   .lookup      = HgfsLookup,
   .rmdir       = HgfsRmdir,
   .unlink      = HgfsUnlink,
   .rename      = HgfsRename,
   .symlink     = HgfsSymlink,
   .permission  = HgfsPermission,
   .setattr     = HgfsSetattr,

   /* Optional */
   .getattr     = HgfsGetattr,
};

/* HGFS inode operations structure for files. */
struct inode_operations HgfsFileInodeOperations = {
   .permission  = HgfsPermission,
   .setattr     = HgfsSetattr,

   /* Optional */
   .getattr     = HgfsGetattr,
};

/*
 * Private functions implementations.
 */


/*
 *----------------------------------------------------------------------
 *
 * HgfsClearReadOnly --
 *
 *    Try to remove the file/dir read only attribute.
 *
 *    Note when running on Windows servers the entry may have the read-only
 *    flag set and prevent a rename or delete operation from occuring.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsClearReadOnly(struct dentry *dentry)  // IN: file/dir to remove read only
{
   struct iattr enableWrite;

   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsClearReadOnly: removing read-only\n"));
   enableWrite.ia_mode = (dentry->d_inode->i_mode | S_IWUSR);
   enableWrite.ia_valid = ATTR_MODE;
   return HgfsSetattr(dentry, &enableWrite);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsDelete --
 *
 *    Handle both unlink and rmdir requests.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsDelete(struct inode *dir,      // IN: Parent dir of file/dir to delete
           struct dentry *dentry,  // IN: Dentry of file/dir to delete
           HgfsOp op)              // IN: Opcode for file type (file or dir)
{
   HgfsReq *req = NULL;
   int result = 0;
   Bool secondAttempt = FALSE;
   HgfsStatus replyStatus;
   char *fileName = NULL;
   uint32 *fileNameLength;
   uint32 reqSize;
   HgfsOp opUsed;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);
   ASSERT(dentry->d_inode);

   if (!dir || !dentry) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: NULL input\n"));
      result = -EFAULT;
      goto out;
   }

   if ((op != HGFS_OP_DELETE_FILE) &&
       (op != HGFS_OP_DELETE_DIR)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: Invalid opcode\n"));
      result = -EINVAL;
      goto out;
   }

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   if (op == HGFS_OP_DELETE_FILE) {
      opUsed = hgfsVersionDeleteFile;
   } else {
      opUsed = hgfsVersionDeleteDir;
   }

   if (opUsed == HGFS_OP_DELETE_FILE_V3 ||
       opUsed == HGFS_OP_DELETE_DIR_V3) {
      HgfsRequestDeleteV3 *request;
      HgfsRequest *header;

      header = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      header->id = req->id;
      header->op = opUsed;

      request = (HgfsRequestDeleteV3 *)(HGFS_REQ_PAYLOAD_V3(req));
      request->hints = 0;
      fileName = request->fileName.name;
      fileNameLength = &request->fileName.length;
      request->fileName.fid = HGFS_INVALID_HANDLE;
      request->fileName.flags = 0;
      request->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
      request->reserved = 0;
      reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestDelete *request;

      request = (HgfsRequestDelete *)(HGFS_REQ_PAYLOAD(req));
      /* Fill out the request packet. */
      request->header.id = req->id;
      request->header.op = opUsed;
      fileName = request->fileName.name;
      fileNameLength = &request->fileName.length;
      reqSize = sizeof *request;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(fileName, HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: build path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: deleting \"%s\", opUsed %u\n",
           fileName, opUsed));

   /* Convert to CP name. */
   result = CPName_ConvertTo(fileName,
                             HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize),
                             fileName);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: CP conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   *fileNameLength = result;
   req->payloadSize = reqSize + result;

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDelete: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         /*
          * Since we deleted the file, decrement its hard link count. As
          * we don't support hard links, this has the effect of making the
          * link count 0, which means that when the last reference to the
          * inode is dropped, the inode will be freed instead of moved to
          * the unused list.
          *
          * Also update the mtime/ctime of the parent directory, and the
          * ctime of the deleted file.
          */
         compat_drop_nlink(dentry->d_inode);
         dentry->d_inode->i_ctime = dir->i_ctime = dir->i_mtime =
            CURRENT_TIME;
         break;

      case -EACCES:
      case -EPERM:
         /*
          * It's possible that we're talking to a Windows server with
          * a file marked read-only. Let's try again, after removing
          * the read-only bit from the file.
          *
          * XXX: I think old servers will send -EPERM here. Is this entirely
          * safe?
          */
         if (!secondAttempt) {
            secondAttempt = TRUE;
            result = HgfsClearReadOnly(dentry);
            if (result == 0) {
               LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: file is no "
                       "longer read-only, retrying delete\n"));
               goto retry;
            }
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: failed to remove "
                    "read-only property\n"));
         } else {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: second attempt at "
                    "delete failed\n"));
         }
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_DELETE_DIR_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionDeleteDir = HGFS_OP_DELETE_DIR;
            goto retry;
         } else if (opUsed == HGFS_OP_DELETE_FILE_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: Version 3 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionDeleteFile = HGFS_OP_DELETE_FILE;
            goto retry;
         }

         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: server "
                 "returned error: %d\n", result));
         break;
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackSetattrRequest --
 *
 *    Setup the Setattr request, depending on the op version. When possible,
 *    we will issue the setattr request using an existing open HGFS handle.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 *    On success, the changed argument is set indicating whether the
 *    attributes have actually changed.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsPackSetattrRequest(struct iattr *iattr,   // IN: Inode attrs to update from
                       struct dentry *dentry, // IN: File to set attributes of
                       Bool allowHandleReuse, // IN: Can we use a handle?
                       HgfsOp opUsed,         // IN: Op to be used
		       HgfsReq *req,          // IN/OUT: Packet to write into
                       Bool *changed)         // OUT: Have the attrs changed?
{
   HgfsAttrV2 *attrV2;
   HgfsAttr *attr;
   HgfsAttrHint *hints;
   HgfsAttrChanges *update;
   HgfsHandle handle;
   char *fileName = NULL;
   uint32 *fileNameLength = NULL;
   unsigned int valid;
   size_t reqBufferSize;
   size_t reqSize;
   int result = 0;
   uid_t attrUid = -1;
   gid_t attrGid = -1;

   ASSERT(iattr);
   ASSERT(dentry);
   ASSERT(req);
   ASSERT(changed);

   valid = iattr->ia_valid;

   if (valid & ATTR_UID) {
      attrUid = from_kuid(&init_user_ns, iattr->ia_uid);
   }

   if (valid & ATTR_GID) {
      attrGid = from_kgid(&init_user_ns, iattr->ia_gid);
   }

   switch (opUsed) {
   case HGFS_OP_SETATTR_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestSetattrV3 *requestV3;

      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestSetattrV3 *)HGFS_REQ_PAYLOAD_V3(req);
      attrV2 = &requestV3->attr;
      hints = &requestV3->hints;

      /*
       * Clear attributes, mask, and hints before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attrV2, 0, sizeof *attrV2);
      memset(hints, 0, sizeof *hints);

      /*
       * When possible, issue a setattr using an existing handle. This will
       * give us slightly better performance on a Windows server, and is more
       * correct regardless. If we don't find a handle, fall back on setattr
       * by name.
       *
       * Changing the size (via truncate) requires write permissions. Changing
       * the times also requires write permissions on Windows, so we require it
       * here too. Otherwise, any handle will do.
       */
      if (allowHandleReuse && HgfsGetHandle(dentry->d_inode,
                                            (valid & ATTR_SIZE) ||
                                            (valid & ATTR_ATIME) ||
                                            (valid & ATTR_MTIME) ?
                                            HGFS_OPEN_MODE_WRITE_ONLY + 1 : 0,
                                            &handle) == 0) {
         requestV3->fileName.fid = handle;
         requestV3->fileName.flags = HGFS_FILE_NAME_USE_FILE_DESC;
         requestV3->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
         requestV3->fileName.length = 0;
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: setting "
                 "attributes of handle %u\n", handle));
      } else {
         fileName = requestV3->fileName.name;
         fileNameLength = &requestV3->fileName.length;
         requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
         requestV3->fileName.fid = HGFS_INVALID_HANDLE;
         requestV3->fileName.flags = 0;
      }
      requestV3->reserved = 0;
      reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);
      reqBufferSize = HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize);

      /*
       * We only support changing these attributes:
       * - all mode bits (i.e. all permissions)
       * - uid/gid
       * - size
       * - access/write times
       */

      if (valid & ATTR_MODE) {
         attrV2->mask |= HGFS_ATTR_VALID_SPECIAL_PERMS |
            HGFS_ATTR_VALID_OWNER_PERMS | HGFS_ATTR_VALID_GROUP_PERMS |
            HGFS_ATTR_VALID_OTHER_PERMS;
         attrV2->specialPerms = ((iattr->ia_mode &
                                  (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
         attrV2->ownerPerms = ((iattr->ia_mode & S_IRWXU) >> 6);
         attrV2->groupPerms = ((iattr->ia_mode & S_IRWXG) >> 3);
         attrV2->otherPerms = (iattr->ia_mode & S_IRWXO);
         *changed = TRUE;
      }

      if (valid & ATTR_UID) {
         attrV2->mask |= HGFS_ATTR_VALID_USERID;
         attrV2->userId = attrUid;
         *changed = TRUE;
      }

      if (valid & ATTR_GID) {
         attrV2->mask |= HGFS_ATTR_VALID_GROUPID;
         attrV2->groupId = attrGid;
         *changed = TRUE;
      }

      if (valid & ATTR_SIZE) {
         attrV2->mask |= HGFS_ATTR_VALID_SIZE;
         attrV2->size = iattr->ia_size;
         *changed = TRUE;
      }

      if (valid & ATTR_ATIME) {
         attrV2->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
         attrV2->accessTime = HGFS_GET_TIME(iattr->ia_atime);
         if (valid & ATTR_ATIME_SET) {
            *hints |= HGFS_ATTR_HINT_SET_ACCESS_TIME;
         }
         *changed = TRUE;
      }

      if (valid & ATTR_MTIME) {
         attrV2->mask |= HGFS_ATTR_VALID_WRITE_TIME;
         attrV2->writeTime = HGFS_GET_TIME(iattr->ia_mtime);
         if (valid & ATTR_MTIME_SET) {
            *hints |= HGFS_ATTR_HINT_SET_WRITE_TIME;
         }
         *changed = TRUE;
      }
      break;
   }

   case HGFS_OP_SETATTR_V2: {
      HgfsRequestSetattrV2 *requestV2;

      requestV2 = (HgfsRequestSetattrV2 *)(HGFS_REQ_PAYLOAD(req));
      requestV2->header.op = opUsed;
      requestV2->header.id = req->id;

      attrV2 = &requestV2->attr;
      hints = &requestV2->hints;

      /*
       * Clear attributes, mask, and hints before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attrV2, 0, sizeof *attrV2);
      memset(hints, 0, sizeof *hints);

      /*
       * When possible, issue a setattr using an existing handle. This will
       * give us slightly better performance on a Windows server, and is more
       * correct regardless. If we don't find a handle, fall back on setattr
       * by name.
       *
       * Changing the size (via truncate) requires write permissions. Changing
       * the times also requires write permissions on Windows, so we require it
       * here too. Otherwise, any handle will do.
       */
      if (allowHandleReuse && HgfsGetHandle(dentry->d_inode,
                                            (valid & ATTR_SIZE) ||
                                            (valid & ATTR_ATIME) ||
                                            (valid & ATTR_MTIME) ?
                                            HGFS_OPEN_MODE_WRITE_ONLY + 1 : 0,
                                            &handle) == 0) {
         *hints = HGFS_ATTR_HINT_USE_FILE_DESC;
         requestV2->file = handle;
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: setting "
                 "attributes of handle %u\n", handle));
      } else {
         fileName = requestV2->fileName.name;
	 fileNameLength = &requestV2->fileName.length;
      }
      reqSize = sizeof *requestV2;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(req->bufferSize, requestV2);

      /*
       * We only support changing these attributes:
       * - all mode bits (i.e. all permissions)
       * - uid/gid
       * - size
       * - access/write times
       */

      if (valid & ATTR_MODE) {
         attrV2->mask |= HGFS_ATTR_VALID_SPECIAL_PERMS |
            HGFS_ATTR_VALID_OWNER_PERMS | HGFS_ATTR_VALID_GROUP_PERMS |
            HGFS_ATTR_VALID_OTHER_PERMS;
         attrV2->specialPerms = ((iattr->ia_mode &
                                  (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
         attrV2->ownerPerms = ((iattr->ia_mode & S_IRWXU) >> 6);
         attrV2->groupPerms = ((iattr->ia_mode & S_IRWXG) >> 3);
         attrV2->otherPerms = (iattr->ia_mode & S_IRWXO);
         *changed = TRUE;
      }

      if (valid & ATTR_UID) {
         attrV2->mask |= HGFS_ATTR_VALID_USERID;
         attrV2->userId = attrUid;
         *changed = TRUE;
      }

      if (valid & ATTR_GID) {
         attrV2->mask |= HGFS_ATTR_VALID_GROUPID;
         attrV2->groupId = attrGid;
         *changed = TRUE;
      }

      if (valid & ATTR_SIZE) {
         attrV2->mask |= HGFS_ATTR_VALID_SIZE;
         attrV2->size = iattr->ia_size;
         *changed = TRUE;
      }

      if (valid & ATTR_ATIME) {
         attrV2->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
         attrV2->accessTime = HGFS_GET_TIME(iattr->ia_atime);
         if (valid & ATTR_ATIME_SET) {
            *hints |= HGFS_ATTR_HINT_SET_ACCESS_TIME;
         }
         *changed = TRUE;
      }

      if (valid & ATTR_MTIME) {
         attrV2->mask |= HGFS_ATTR_VALID_WRITE_TIME;
         attrV2->writeTime = HGFS_GET_TIME(iattr->ia_mtime);
         if (valid & ATTR_MTIME_SET) {
            *hints |= HGFS_ATTR_HINT_SET_WRITE_TIME;
         }
         *changed = TRUE;
      }
      break;
   }

   case HGFS_OP_SETATTR: {
      HgfsRequestSetattr *request;

      request = (HgfsRequestSetattr *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = opUsed;
      request->header.id = req->id;

      attr = &request->attr;
      update = &request->update;

      /* We'll use these later. */
      fileName = request->fileName.name;
      fileNameLength = &request->fileName.length;
      reqSize = sizeof *request;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(req->bufferSize, request);


      /*
       * Clear attributes before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attr, 0, sizeof *attr);
      memset(update, 0, sizeof *update);

      /*
       * We only support changing these attributes:
       * - owner mode bits (i.e. owner permissions)
       * - size
       * - access/write times
       */

      if (valid & ATTR_MODE) {
         *update |= HGFS_ATTR_PERMISSIONS;
         attr->permissions = ((iattr->ia_mode & S_IRWXU) >> 6);
         *changed = TRUE;
      }

      if (valid & ATTR_SIZE) {
         *update |= HGFS_ATTR_SIZE;
         attr->size = iattr->ia_size;
         *changed = TRUE;
      }

      if (valid & ATTR_ATIME) {
         *update |= HGFS_ATTR_ACCESS_TIME |
            ((valid & ATTR_ATIME_SET) ? HGFS_ATTR_ACCESS_TIME_SET : 0);
         attr->accessTime = HGFS_GET_TIME(iattr->ia_atime);
         *changed = TRUE;
      }

      if (valid & ATTR_MTIME) {
         *update |= HGFS_ATTR_WRITE_TIME |
            ((valid & ATTR_MTIME_SET) ? HGFS_ATTR_WRITE_TIME_SET : 0);
         attr->writeTime = HGFS_GET_TIME(iattr->ia_mtime);
         *changed = TRUE;
      }
      break;
   }

   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Avoid all this extra work when we're doing a setattr by handle. */
   if (fileName != NULL) {

      /* Build full name to send to server. */
      if (HgfsBuildPath(fileName, reqBufferSize, dentry) < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: build path "
                 "failed\n"));
         return -EINVAL;
      }
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: setting "
              "attributes of \"%s\"\n", fileName));

      /* Convert to CP name. */
      result = CPName_ConvertTo(fileName,
                                reqBufferSize,
                                fileName);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: CP "
                 "conversion failed\n"));
         return -EINVAL;
      }

      *fileNameLength = result;
   }
   req->payloadSize = reqSize + result;
   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackCreateDirRequest --
 *
 *    Setup the CreateDir request, depending on the op version.
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
HgfsPackCreateDirRequest(struct dentry *dentry, // IN: Directory to create
                         compat_umode_t mode,   // IN: Mode to assign dir
                         HgfsOp opUsed,         // IN: Op to be used.
                         HgfsReq *req)          // IN/OUT: Packet to write into
{
   char *fileName = NULL;
   uint32 *fileNameLength;
   size_t requestSize;
   int result;

   ASSERT(dentry);
   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_CREATE_DIR_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestCreateDirV3 *requestV3;

      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestCreateDirV3 *)(HGFS_REQ_PAYLOAD_V3(req));

      /* We'll use these later. */
      fileName = requestV3->fileName.name;
      fileNameLength = &requestV3->fileName.length;
      requestV3->fileName.flags = 0;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

      requestSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);

      requestV3->mask = HGFS_CREATE_DIR_MASK;

      /* Set permissions. */
      requestV3->specialPerms = (mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
      requestV3->ownerPerms = (mode & S_IRWXU) >> 6;
      requestV3->groupPerms = (mode & S_IRWXG) >> 3;
      requestV3->otherPerms = (mode & S_IRWXO);
      requestV3->fileAttr = 0;
      break;
   }
   case HGFS_OP_CREATE_DIR_V2: {
      HgfsRequestCreateDirV2 *requestV2;

      requestV2 = (HgfsRequestCreateDirV2 *)(HGFS_REQ_PAYLOAD(req));
      requestV2->header.op = opUsed;
      requestV2->header.id = req->id;

      /* We'll use these later. */
      fileName = requestV2->fileName.name;
      fileNameLength = &requestV2->fileName.length;
      requestSize = sizeof *requestV2;

      requestV2->mask = HGFS_CREATE_DIR_MASK;

      /* Set permissions. */
      requestV2->specialPerms = (mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
      requestV2->ownerPerms = (mode & S_IRWXU) >> 6;
      requestV2->groupPerms = (mode & S_IRWXG) >> 3;
      requestV2->otherPerms = (mode & S_IRWXO);
      break;
   }
   case HGFS_OP_CREATE_DIR: {
      HgfsRequestCreateDir *request;

      request = (HgfsRequestCreateDir *)(HGFS_REQ_PAYLOAD(req));

      /* We'll use these later. */
      fileName = request->fileName.name;
      fileNameLength = &request->fileName.length;
      requestSize = sizeof *request;
      requestSize = sizeof *request;

      /* Set permissions. */
      request->permissions = (mode & S_IRWXU) >> 6;
      break;
   }
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(fileName,
                     req->bufferSize - (requestSize - 1),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: build path "
              "failed\n"));
      return -EINVAL;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: create dir "
           "\"%s\", perms %o\n", fileName, mode));

   /* Convert to CP name. */
   result = CPName_ConvertTo(fileName,
                             req->bufferSize - (requestSize - 1),
                             fileName);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: CP "
              "conversion failed\n"));
      return -EINVAL;
   }

   *fileNameLength = result;
   req->payloadSize = requestSize + result;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsTruncatePages --
 *
 *    Following a truncate operation on the server, we must update the
 *    page cache's view of the file by truncating some pages. This is a
 *    two step procedure. First we call vmtruncate() to truncate all
 *    whole pages. Then we get the boundary page from the page cache
 *    ourselves, compute where the truncation began, and memset() the
 *    rest of the page to zero.
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
HgfsTruncatePages(struct inode *inode, // IN: Inode whose page to truncate
                  loff_t newSize)      // IN: New size of the file
{
   int result;
   pgoff_t pageIndex = newSize >> PAGE_CACHE_SHIFT;
   unsigned pageOffset = newSize & (PAGE_CACHE_SIZE - 1);
   struct page *page;
   char *buffer;

   ASSERT(inode);

   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsTruncatePages: entered\n"));

   /*
    * In 3.8.0, vmtruncate was removed and replaced by calling the check
    * size and set directly.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 8, 0)
   result = vmtruncate(inode, newSize);
#else
   result = inode_newsize_ok(inode, newSize);
   if (0 == result) {
      truncate_setsize(inode, newSize);
   }
#endif
   if (result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsTruncatePages: vmtruncate failed "
              "with error code %d\n", result));
      return result;
   }

   /*
    * This is a bit complicated, so it merits an explanation. grab_cache_page()
    * will give us back the page with the specified index, after having locked
    * and incremented its reference count. We must first map it into memory so
    * we can modify it. After we're done modifying the page, we flush its data
    * from the data cache, unmap it, release our reference, and unlock it.
    */
   page = grab_cache_page(inode->i_mapping, pageIndex);
   if (page == NULL) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsTruncatePages: could not get page "
             "with index %lu from page cache\n", pageIndex));
      return -ENOMEM;
   }
   buffer = kmap(page);
   memset(buffer + pageOffset, 0, PAGE_CACHE_SIZE - pageOffset);
   flush_dcache_page(page);
   kunmap(page);
   page_cache_release(page);
   compat_unlock_page(page);
   return 0;
}


/*
 * HGFS inode operations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsCreate --
 *
 *    Create inode for a new file. Called directly by vfs_create,
 *    which is called by open_namei (both in fs/namei.c), as a result
 *    of someone doing a creat(2) or an open(2) with O_CREAT.
 *
 *    This gets called BEFORE f_op->open is called, so the file on the
 *    remote end has not been created yet when we get here. So, we
 *    just cheat and create a reasonable looking inode and instantiate
 *    it. When this returns, our open routine will get called, which
 *    will create the actual file on the server. If that fails for
 *    some reason, dentry_open (which calls f_op->open) will cleanup
 *    things and fput the dentry.
 *
 *    XXX: Now that we do care about having valid inode numbers, it is
 *    unfortunate but necessary that we "cheat" here. The problem is that
 *    without the "intent" field from the nameidata struct (which we don't
 *    get prior to 2.5.75), we have no way of knowing whether the file was
 *    opened with O_EXCL or O_TRUNC. Knowing about O_TRUNC isn't crucial
 *    because we can always create the file now and truncate it later, in
 *    HgfsOpen. But without knowing about O_EXCL, we can't "fail if the file
 *    exists on the server", which is the desired behavior for O_EXCL. The
 *    source code for NFSv3 in 2.4.2 describes this shortcoming. The only
 *    solution, barring massive architectural differences between the 2.4 and
 *    2.6 HGFS drivers, is to ignore O_EXCL, but we've supported it up until
 *    now...
 *
 * Results:
 *    Returns zero on success, negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsCreate(struct inode *dir,     // IN: Parent dir to create in
           struct dentry *dentry, // IN: Dentry containing name to create
           compat_umode_t mode,   // IN: Mode of file to be created
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
           bool excl              // IN: O_EXCL
#else
           struct nameidata *nd   // IN: Intent, vfsmount, ...
#endif
           )
{
   HgfsAttrInfo attr;
   int result;

   ASSERT(dir);
   ASSERT(dentry);

   /*
    * We can call HgfsBuildPath and make the full path to this new entry,
    * but why bother if it's only for logging.
    */
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsCreate: new entry \"%s\"\n",
           dentry->d_name.name));

   /* Create appropriate attrs for this file. */
   attr.type = HGFS_FILE_TYPE_REGULAR;
   attr.size = 0; /* just to be explicit */
   attr.specialPerms = ((mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9);
   attr.ownerPerms = (mode & S_IRWXU) >> 6;
   attr.groupPerms = (mode & S_IRWXG) >> 3;
   attr.otherPerms = mode & S_IRWXO;
   attr.mask = HGFS_ATTR_VALID_TYPE | HGFS_ATTR_VALID_SIZE |
      HGFS_ATTR_VALID_SPECIAL_PERMS | HGFS_ATTR_VALID_OWNER_PERMS |
      HGFS_ATTR_VALID_GROUP_PERMS | HGFS_ATTR_VALID_OTHER_PERMS;

   result = HgfsInstantiate(dentry, 0, &attr);

   /*
    * Mark the inode as recently created but not yet opened so that if we do
    * fail to create the actual file in HgfsOpen, we know to force a
    * revalidate so that the next operation on this inode will fail.
    */
   if (result == 0) {
      HgfsInodeInfo *iinfo = INODE_GET_II_P(dentry->d_inode);
      iinfo->createdAndUnopened = TRUE;
   }
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsLookup --
 *
 *    Lookup a file in a directory.
 *
 *    We do a getattr to see if the file exists on the server, and if
 *    so we create a new inode and fill in the fields appropriately by
 *    calling HgfsIget with the results of the getattr, and then
 *    call d_add with the new dentry.
 *
 *    For the curious, the way lookup in linux works (see fs/namei.c)
 *    is roughly as follows: first a d_lookup is done to see if there
 *    is an appropriate entry in the dcache already. If there is, it
 *    is revalidated by calling d_op->d_revalidate, which calls our
 *    HgfsDentryRevalidate (see above). If there is no dentry in the
 *    cache or if the dentry is no longer valid, then namei calls
 *    i_op->lookup, which calls HgfsLookup.
 *
 * Results:
 *    Returns NULL on success, negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static struct dentry *
HgfsLookup(struct inode *dir,      // IN: Inode of parent directory
           struct dentry *dentry,  // IN: Dentry containing name to look up
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
           unsigned int flags
#else
           struct nameidata *nd    // IN: Intent, vfsmount, ...
#endif
           )
{
   HgfsAttrInfo attr;
   struct inode *inode;
   int error = 0;

   ASSERT(dir);
   ASSERT(dentry);

   if (!dir || !dentry) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsLookup: NULL input\n"));
      error = -EFAULT;
      goto error;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsLookup: dir ino %lu, i_dev %u\n",
          dir->i_ino, dir->i_sb->s_dev));
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsLookup: entry name is \"%s\"\n",
           dentry->d_name.name));

   /* Do a getattr on the file to see if it exists on the server. */
   inode = NULL;
   error = HgfsPrivateGetattr(dentry, &attr, NULL);
   if (!error) {
      /* File exists on the server. */

      /*
       * Get the inode with this inode number and the attrs we got from
       * the server.
       */
      inode = HgfsIget(dir->i_sb, 0, &attr);
      if (!inode) {
         error = -ENOMEM;
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsLookup: out of memory getting "
                 "inode\n"));
         goto error;
      }
   } else if (error != -ENOENT) {
      /*
       * Either the file doesn't exist or there was a more serious
       * error; if it's the former, it's okay, we just do nothing.
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsLookup: error other "
              "than ENOENT: %d\n", error));
      goto error;
   }

   /*
    * Set the dentry's time to NOW, set its operations pointer, add it
    * and the new (possibly NULL) inode to the dcache.
    */
   HgfsDentryAgeReset(dentry);
   dentry->d_op = &HgfsDentryOperations;
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsLookup: adding new entry\n"));
   d_add(dentry, inode);

   return NULL;

error:
   return ERR_PTR(error);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsMkdir --
 *
 *    Handle a mkdir request
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsMkdir(struct inode *dir,     // IN: Inode of parent directory
          struct dentry *dentry, // IN: Dentry with name to be created
          compat_umode_t mode)   // IN: Mode of dir to be created
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   HgfsOp opUsed;
   int result = 0;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = hgfsVersionCreateDir;
   result = HgfsPackCreateDirRequest(dentry, mode, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: error packing request\n"));
      goto out;
   }

   /*
    * Send the request and process the reply. Since HgfsReplyCreateDirV2 and
    * HgfsReplyCreateDir are identical, we need no special logic here.
    */
   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsMkdir: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsMkdir: directory created "
                 "successfully, instantiating dentry\n"));
         result = HgfsInstantiate(dentry, 0, NULL);
         if (result == 0) {
            /*
             * Attempt to set host directory's uid/gid to that of the
             * current user.  As with the open(.., O_CREAT) case, this is
             * only expected to work when the hgfs server is running on
             * a Linux machine and as root, but we might as well give it
             * a go.
             */
            HgfsSetUidGid(dir, dentry, current_fsuid(), current_fsgid());
         }

         /*
          * XXX: When we support hard links, this is a good place to
          * increment link count of parent dir.
          */
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CREATE_DIR_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: Version 3 not "
                    "supported. Falling back to version 2.\n"));
            hgfsVersionCreateDir = HGFS_OP_CREATE_DIR_V2;
            goto retry;
         } else if (opUsed == HGFS_OP_CREATE_DIR_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: Version 2 not "
                    "supported. Falling back to version 1.\n"));
            hgfsVersionCreateDir = HGFS_OP_CREATE_DIR;
            goto retry;
         }

         /* Fallthrough. */
         default:
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsMkdir: directory was not "
                    "created, error %d\n", result));
            break;
         }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRmdir --
 *
 *    Handle an rmdir request. Just calls HgfsDelete with the
 *    correct opcode.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsRmdir(struct inode *dir,      // IN: Parent dir of dir to remove
          struct dentry *dentry)  // IN: Dentry of dir to remove
{
   int result;

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsRmdir: was called\n"));

   /*
    * XXX: CIFS also sets the size of the deleted directory to 0. Why? I don't
    * know...why not?
    *
    * XXX: When we support hardlinks, we should decrement the link count of
    * the parent directory.
    */
   result = HgfsDelete(dir, dentry, HGFS_OP_DELETE_DIR);
   if (!result) {
      compat_i_size_write(dentry->d_inode, 0);
   }
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsUnlink --
 *
 *    Handle an unlink request. Just calls HgfsDelete with the
 *    correct opcode.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsUnlink(struct inode *dir,      // IN: Parent dir of file to unlink
           struct dentry *dentry)  // IN: Dentry of file to unlink
{
   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsUnlink: was called\n"));

   return HgfsDelete(dir, dentry, HGFS_OP_DELETE_FILE);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRename --
 *
 *    Handle rename requests.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsRename(struct inode *oldDir,      // IN: Inode of original directory
           struct dentry *oldDentry,  // IN: Dentry of file to rename
           struct inode *newDir,      // IN: Inode of new directory
           struct dentry *newDentry)  // IN: Dentry containing new name
{
   HgfsReq *req = NULL;
   char *oldName;
   char *newName;
   Bool secondAttempt=FALSE;
   uint32 *oldNameLength;
   uint32 *newNameLength;
   int result = 0;
   uint32 reqSize;
   HgfsOp opUsed;
   HgfsStatus replyStatus;

   ASSERT(oldDir);
   ASSERT(oldDir->i_sb);
   ASSERT(oldDentry);
   ASSERT(newDir);
   ASSERT(newDentry);

   if (!oldDir || !oldDentry || !newDir || !newDentry) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: NULL input\n"));
      result = -EFAULT;
      goto out;
   }

   if (oldDentry->d_inode && newDentry->d_inode) {
      HgfsInodeInfo *oldIinfo;
      HgfsInodeInfo *newIinfo;
      /*
       * Don't do rename if the source and target are identical (from the
       * viewpoint of the host). It is possible that multiple guest inodes
       * point to the same host inode under the case that both one folder
       * and its subfolder are mapped as hgfs sharese. Please also see the
       * comments at fsutil.c/HgfsIget.
       */
      oldIinfo = INODE_GET_II_P(oldDentry->d_inode);
      newIinfo = INODE_GET_II_P(newDentry->d_inode);
      if (oldIinfo->hostFileId !=0 && newIinfo->hostFileId != 0 &&
          oldIinfo->hostFileId == newIinfo->hostFileId) {
         LOG(4, ("VMware hgfs: %s: source and target are the same file.\n",
                 __func__));
         result = -EEXIST;
         goto out;
      }
   }

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionRename;
   if (opUsed == HGFS_OP_RENAME_V3) {
      HgfsRequestRenameV3 *request = (HgfsRequestRenameV3 *)HGFS_REQ_PAYLOAD_V3(req);
      HgfsRequest *header = (HgfsRequest *)HGFS_REQ_PAYLOAD(req);

      header->op = opUsed;
      header->id = req->id;

      oldName = request->oldName.name;
      oldNameLength = &request->oldName.length;
      request->hints = 0;
      request->oldName.flags = 0;
      request->oldName.fid = HGFS_INVALID_HANDLE;
      request->oldName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      request->reserved = 0;
      reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   } else {
      HgfsRequestRename *request = (HgfsRequestRename *)HGFS_REQ_PAYLOAD(req);

      request->header.op = opUsed;
      oldName = request->oldName.name;
      oldNameLength = &request->oldName.length;
      reqSize = sizeof *request;
   }

   /* Build full old name to send to server. */
   if (HgfsBuildPath(oldName, HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize),
                     oldDentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: build old path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: Old name: \"%s\"\n",
           oldName));

   /* Convert old name to CP format. */
   result = CPName_ConvertTo(oldName,
                             HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize),
                             oldName);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: oldName CP "
              "conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   *oldNameLength = result;
   reqSize += result;

   /*
    * Build full new name to send to server.
    * Note the different buffer length. This is because HgfsRequestRename
    * contains two filenames, and once we place the first into the packet we
    * must account for it when determining the amount of buffer available for
    * the second.
    */
   if (opUsed == HGFS_OP_RENAME_V3) {
      HgfsRequestRenameV3 *request = (HgfsRequestRenameV3 *)HGFS_REQ_PAYLOAD_V3(req);
      HgfsFileNameV3 *newNameP;
      newNameP = (HgfsFileNameV3 *)((char *)&request->oldName +
                                    sizeof request->oldName + result);
      newName = newNameP->name;
      newNameLength = &newNameP->length;
      newNameP->flags = 0;
      newNameP->fid = HGFS_INVALID_HANDLE;
      newNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   } else {
      HgfsRequestRename *request = (HgfsRequestRename *)HGFS_REQ_PAYLOAD(req);
      HgfsFileName *newNameP;
      newNameP = (HgfsFileName *)((char *)&request->oldName +
                                  sizeof request->oldName + result);
      newName = newNameP->name;
      newNameLength = &newNameP->length;
   }

   if (HgfsBuildPath(newName, HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize) - result,
                     newDentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: build new path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: New name: \"%s\"\n",
           newName));

   /* Convert new name to CP format. */
   result = CPName_ConvertTo(newName,
                             HGFS_NAME_BUFFER_SIZET(req->bufferSize, reqSize) - result,
                             newName);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: newName CP "
              "conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   *newNameLength = result;
   reqSize += result;
   req->payloadSize = reqSize;

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      if (result == -EPROTO) {
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_RENAME_V3) {
            hgfsVersionRename = HGFS_OP_RENAME;
            goto retry;
         } else {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: server "
                    "returned error: %d\n", result));
            goto out;
         }
      } else if ((-EACCES == result) || (-EPERM == result)) {
         /*
          * It's possible that we're talking to a Windows server with
          * a file marked read-only. Let's try again, after removing
          * the read-only bit from the file.
          *
          * XXX: I think old servers will send -EPERM here. Is this entirely
          * safe?
          * We can receive EACCES or EPERM if we don't have the correct
          * permission on the source file. So lets not assume that we have
          * a target and only clear the target if there is one.
          */
         if (!secondAttempt && newDentry->d_inode != NULL) {
            secondAttempt = TRUE;
            LOG(4, (KERN_DEBUG "VMware hgfs: %s:clear target RO mode %8x\n",
                    __func__, newDentry->d_inode->i_mode));
            result = HgfsClearReadOnly(newDentry);
            if (result == 0) {
               LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: file is no "
                       "longer read-only, retrying rename\n"));
               goto retry;
            }
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: failed to remove "
                    "read-only property\n"));
         } else {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: second attempt or "
                    "no target failed\n"));
         }
      } else if (0 != result) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: failed with result %d\n", result));
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: unknown error: "
              "%d\n", result));
   }

#if LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)
   if (result == 0) {
      /*
       * We force revalidate to go get the file info as soon as needed.
       * We only add this fix, borrowed from CIFS, for newer versions
       * of the kernel which have the current_fs_time function.
       * For details see bug 1613734 but here is a short summary.
       * This addresses issues in editors such as gedit which use
       * rename when saving the updated contents of a file.
       * If we don't force the revalidation here, then the dentry
       * will randomly age over some time which will then pick up the
       * file's new timestamps from the server at that time.
       * This delay will cause the editor to think the file has been modified
       * underneath it and prompt the user if they want to reload the file.
       */
      HgfsDentryAgeForce(oldDentry);
      HgfsDentryAgeForce(newDentry);
      oldDir->i_ctime = oldDir->i_mtime = newDir->i_ctime =
         newDir->i_mtime = current_fs_time(oldDir->i_sb);
   }
#endif // LINUX_VERSION_CODE > KERNEL_VERSION(2, 6, 32)

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackSymlinkCreateRequest --
 *
 *    Setup the create symlink request, depending on the op version.
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
HgfsPackSymlinkCreateRequest(struct dentry *dentry,   // IN: File pointer for this open
                             const char *symname,     // IN: Target name
                             HgfsOp opUsed,           // IN: Op to be used
                             HgfsReq *req)            // IN/OUT: Packet to write into
{
   HgfsRequestSymlinkCreateV3 *requestV3 = NULL;
   HgfsRequestSymlinkCreate *request = NULL;
   char *symlinkName;
   uint32 *symlinkNameLength;
   char *targetName;
   uint32 *targetNameLength;
   size_t targetNameBytes;

   size_t requestSize;
   int result;

   ASSERT(dentry);
   ASSERT(symname);
   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_CREATE_SYMLINK_V3: {
      HgfsRequest *requestHeader;

      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestSymlinkCreateV3 *)HGFS_REQ_PAYLOAD_V3(req);

      /* We'll use these later. */
      symlinkName = requestV3->symlinkName.name;
      symlinkNameLength = &requestV3->symlinkName.length;
      requestV3->symlinkName.flags = 0;
      requestV3->symlinkName.fid = HGFS_INVALID_HANDLE;
      requestV3->symlinkName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->reserved = 0;
      requestSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);
      break;
   }
   case HGFS_OP_CREATE_SYMLINK: {

      request = (HgfsRequestSymlinkCreate *)(HGFS_REQ_PAYLOAD(req));
      request->header.op = opUsed;
      request->header.id = req->id;

      /* We'll use these later. */
      symlinkName = request->symlinkName.name;
      symlinkNameLength = &request->symlinkName.length;
      requestSize = sizeof *request;
      break;
   }
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   if (HgfsBuildPath(symlinkName, req->bufferSize - (requestSize - 1),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: build symlink path "
              "failed\n"));
      return -EINVAL;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: Symlink name: \"%s\"\n",
           symlinkName));

   /* Convert symlink name to CP format. */
   result = CPName_ConvertTo(symlinkName,
                             req->bufferSize - (requestSize - 1),
                             symlinkName);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: symlinkName CP "
              "conversion failed\n"));
      return -EINVAL;
   }

   *symlinkNameLength = result;
   req->payloadSize = requestSize + result;

   /*
    * Note the different buffer length. This is because HgfsRequestSymlink
    * contains two filenames, and once we place the first into the packet we
    * must account for it when determining the amount of buffer available for
    * the second.
    *
    * Also note that targetNameBytes accounts for the NUL character. Once
    * we've converted it to CP name, it won't be NUL-terminated and the length
    * of the string in the packet itself won't account for it.
    */
   if (opUsed == HGFS_OP_CREATE_SYMLINK_V3) {
      HgfsFileNameV3 *fileNameP;
      fileNameP = (HgfsFileNameV3 *)((char *)&requestV3->symlinkName +
                                     sizeof requestV3->symlinkName + result);
      targetName = fileNameP->name;
      targetNameLength = &fileNameP->length;
      fileNameP->flags = 0;
      fileNameP->fid = HGFS_INVALID_HANDLE;
      fileNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   } else {
      HgfsFileName *fileNameP;
      fileNameP = (HgfsFileName *)((char *)&request->symlinkName +
                                   sizeof request->symlinkName + result);
      targetName = fileNameP->name;
      targetNameLength = &fileNameP->length;
   }
   targetNameBytes = strlen(symname) + 1;

   /* Copy target name into request packet. */
   if (targetNameBytes > req->bufferSize - (requestSize - 1)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: target name is too "
              "big\n"));
      return -EINVAL;
   }
   memcpy(targetName, symname, targetNameBytes);
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSymlinkCreateRequest: target name: \"%s\"\n",
           targetName));

   /* Convert target name to CPName-lite format. */
   CPNameLite_ConvertTo(targetName, targetNameBytes - 1, '/');

   *targetNameLength = targetNameBytes - 1;
   req->payloadSize += targetNameBytes - 1;

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSymlink --
 *
 *    Handle a symlink request
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsSymlink(struct inode *dir,     // IN: Inode of parent directory
            struct dentry *dentry, // IN: Dentry of new symlink file
            const char *symname)   // IN: Target name
{
   HgfsReq *req;
   int result = 0;
   HgfsOp opUsed;
   HgfsStatus replyStatus;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);
   ASSERT(symname);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = hgfsVersionCreateSymlink;
   result = HgfsPackSymlinkCreateRequest(dentry, symname, opUsed, req);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: error packing request\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: got reply\n"));
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);
      if (result == 0) {
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: symlink created "
                 "successfully, instantiating dentry\n"));
         result = HgfsInstantiate(dentry, 0, NULL);
      } else if (result == -EPROTO) {
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CREATE_SYMLINK_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: Version 3 "
                    "not supported. Falling back to version 2.\n"));
            hgfsVersionCreateSymlink = HGFS_OP_CREATE_SYMLINK;
            goto retry;
         } else {
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: symlink was not "
                    "created, error %d\n", result));
         }
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAccessInt --
 *
 *      Check to ensure the user has the specified type of access to the file.
 *
 * Results:
 *      Returns 0 if access is allowed and a non-zero error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsAccessInt(struct dentry *dentry, // IN: dentry to check access for
              int mask)              // IN: access mode requested.
{

   HgfsAttrInfo attr;
   int ret;

   if (!dentry) {
      return 0;
   }
   ret = HgfsPrivateGetattr(dentry, &attr, NULL);
   if (ret == 0) {
      uint32 effectivePermissions;

      if (attr.mask & HGFS_ATTR_VALID_EFFECTIVE_PERMS) {
         effectivePermissions = attr.effectivePerms;
      } else {
         /*
          * If the server did not return actual effective permissions then
          * need to calculate ourselves. However we should avoid unnecessary
          * denial of access so perform optimistic permissions calculation.
          * It is safe since host enforces necessary restrictions regardless of
          * the client's decisions.
          */
         effectivePermissions =
            attr.ownerPerms | attr.groupPerms | attr.otherPerms;
      }

      if ((effectivePermissions & mask) != mask) {
         ret = -EACCES;
      }
      LOG(8, ("VMware Hgfs: %s: effectivePermissions: %d, ret: %d\n",
              __func__, effectivePermissions, ret));
   } else {
      LOG(4, ("VMware Hgfs: %s: HgfsPrivateGetattr failed.\n", __func__));
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPermission --
 *
 *    Check for access rights on Hgfs. Called from VFS layer for each
 *    file access.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 6, 0)
static int
HgfsPermission(struct inode *inode,
               int mask)
{
   LOG(8, ("VMware hgfs: %s: inode->mode: %8x mask: %8x\n", __func__,
           inode->i_mode, mask));
   /*
    * For sys_access, we go to the host for permission checking;
    * otherwise return 0.
    */
   if (mask & MAY_ACCESS) { /* For sys_access. */
      struct dentry *dentry;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
      struct hlist_node *p;
#endif

      if (mask & MAY_NOT_BLOCK)
         return -ECHILD;

      /* Find a dentry with valid d_count. Refer bug 587879. */
      hlist_for_each_entry(dentry,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 9, 0)
                           p,
#endif
                           &inode->i_dentry,
                           hgfs_d_alias()) {
         int dcount = hgfs_d_count(dentry);
         if (dcount) {
            LOG(4, ("Found %s %d \n", dentry->d_name.name, dcount));
            return HgfsAccessInt(dentry, mask & (MAY_READ | MAY_WRITE | MAY_EXEC));
         }
      }
      ASSERT(FALSE);
   }
   return 0;
}
#else
static int
HgfsPermission(struct inode *inode,
               int mask
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
               , struct nameidata *nd
#elif defined(IPERM_FLAG_RCU)
               , unsigned int flags
#endif
               )
{
   LOG(8, ("VMware hgfs: %s: inode->mode: %8x mask: %8x\n", __func__,
           inode->i_mode, mask));
   /*
    * For sys_access, we go to the host for permission checking;
    * otherwise return 0.
    */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 27)
   if (nd != NULL && (nd->flags & LOOKUP_ACCESS)) { /* For sys_access. */
#else
   if (mask & MAY_ACCESS) { /* For sys_access. */
#endif
      struct list_head *pos;

      /*
       * In 2.6.38 path walk is done in 2 distinct modes: rcu-walk and
       * ref-walk. Ref-walk is the classic one; rcu is lockless and is
       * not allowed to sleep. We insist on using ref-walk since our
       * transports may sleep. In 3.1 IPERM_FLAG_RCU was replaced with
       * MAY_NOT_BLOCK.
       */
#if defined(MAY_NOT_BLOCK)
      if (mask & MAY_NOT_BLOCK)
         return -ECHILD;
#elif defined(IPERM_FLAG_RCU)
      if (flags & IPERM_FLAG_RCU)
         return -ECHILD;
#endif

      /* Find a dentry with valid d_count. Refer bug 587879. */
      list_for_each(pos, &inode->i_dentry) {
         int dcount;
         struct dentry *dentry = list_entry(pos, struct dentry, hgfs_d_alias());
         dcount = hgfs_d_count(dentry);
         if (dcount) {
            LOG(4, ("Found %s %d \n", (dentry)->d_name.name, dcount));
            return HgfsAccessInt(dentry, mask & (MAY_READ | MAY_WRITE | MAY_EXEC));
         }
      }
      ASSERT(FALSE);
   }
   return 0;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetattr --
 *
 *    Hgfs superblock 'getattr' method.
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

static int
HgfsGetattr(struct vfsmount *mnt,  // Unused
            struct dentry *dentry, // IN
            struct kstat *stat)    // OUT
{
   int err;

   // XXX ASSERT(mnt); ? --hpreg
   ASSERT(dentry);
   ASSERT(stat);

   err = HgfsRevalidate(dentry);
   if (err) {
      return err;
   }

   /* Convert stats from the VFS inode format to the kernel format --hpreg */
   generic_fillattr(dentry->d_inode, stat);
   // XXX Should we set stat->blocks and stat->blksize? --hpreg

   return 0;
}

/*
 * Public function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsSetattr --
 *
 *    Handle a setattr request. Call HgfsSetattrCopy to determine
 *    which fields need updating and convert them to the HgfsAttr
 *    format, then send the request to the server.
 *
 * Results:
 *    Returns zero on success, or a negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSetattr(struct dentry *dentry,  // IN: File to set attributes of
            struct iattr *iattr)    // IN: Attributes to set
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   int result = 0;
   Bool changed = FALSE;
   Bool allowHandleReuse = TRUE;
   HgfsOp opUsed;

   ASSERT(dentry);
   ASSERT(dentry->d_inode);
   ASSERT(dentry->d_sb);
   ASSERT(iattr);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   /* Fill out the request packet. */
   opUsed = hgfsVersionSetattr;
   result = HgfsPackSetattrRequest(iattr, dentry, allowHandleReuse,
                                   opUsed, req, &changed);
   if (result != 0 || !changed) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: no attrs changed\n"));
      goto out;
   }

   /*
    * Flush all dirty pages prior to sending the request if we're going to
    * modify the file size or change the last write time.
    */
   if (iattr->ia_valid & ATTR_SIZE || iattr->ia_valid & ATTR_MTIME) {
      ASSERT(dentry->d_inode->i_mapping);
      compat_filemap_write_and_wait(dentry->d_inode->i_mapping);
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         /*
          * If we modified the file size, we must truncate our pages from the
          * page cache.
          */
         if (iattr->ia_valid & ATTR_SIZE) {
            result = HgfsTruncatePages(dentry->d_inode, iattr->ia_size);
         }

         /* Fallthrough to revalidate. */
      case -EPERM:
         /*
          * Now that the server's attributes are updated, let's update our
          * local view of them. Unfortunately, we can't trust iattr, because
          * the server may have chosen to ignore certain attributes that we
          * asked it to set. For example, a Windows server will have ignored
          * the mode nearly entirely. Therefore, rather than calling
          * inode_setattr() to update the inode with the contents of iattr,
          * just force a revalidate.
          *
          * XXX: Note that EPERM gets similar treatment, as the server may
          * have updated some of the attributes and still sent us an error.
          */
         HgfsDentryAgeForce(dentry);
         HgfsRevalidate(dentry);
         break;

      case -EBADF:
         /*
          * This can happen if we attempted a setattr by handle and the handle
          * was closed. Because we have no control over the backdoor, it's
          * possible that an attacker closed our handle, in which case the
          * driver still thinks the handle is open. So a straight-up
          * "goto retry" would cause an infinite loop. Instead, let's retry
          * with a setattr by name.
          */
         if (allowHandleReuse) {
            allowHandleReuse = FALSE;
            goto retry;
         }

         /*
          * There's no reason why the server should have sent us this error
          * when we haven't used a handle. But to prevent an infinite loop in
          * the driver, let's make sure that we don't retry again.
          */
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_SETATTR_V3) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: Version 3 "
                    "not supported. Falling back to version 2.\n"));
            hgfsVersionSetattr = HGFS_OP_SETATTR_V2;
            goto retry;
         } else if (opUsed == HGFS_OP_SETATTR_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: Version 2 "
                    "not supported. Falling back to version 1.\n"));
            hgfsVersionSetattr = HGFS_OP_SETATTR;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: server "
              "returned error: %d\n", result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: unknown error: "
              "%d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRevalidate --
 *
 *    Called when the kernel wants to check that an inode is still
 *    valid. Called with the dentry that points to the inode we're
 *    interested in.
 *
 *    We call HgfsPrivateGetattr with the inode's remote name, and if
 *    it succeeds we update the inode's attributes and return zero
 *    (success). Otherwise, we return an error.
 *
 * Results:
 *    Returns zero if inode is valid, negative error if not.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsRevalidate(struct dentry *dentry)  // IN: Dentry to revalidate
{
   int error = 0;
   HgfsSuperInfo *si;
   unsigned long age;
   HgfsInodeInfo *iinfo;

   ASSERT(dentry);
   si = HGFS_SB_TO_COMMON(dentry->d_sb);

   if (!dentry->d_inode) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: null input\n"));
      return -EINVAL;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: name %s, "
           "inum %lu\n", dentry->d_name.name, dentry->d_inode->i_ino));

   age = jiffies - dentry->d_time;
   iinfo = INODE_GET_II_P(dentry->d_inode);

   if (age > si->ttl || iinfo->hostFileId == 0) {
      HgfsAttrInfo attr;
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: dentry is too old, "
              "getting new attributes\n"));
      /*
       * Sync unwritten file data so the file size on the server will
       * be current with our view of the file.
       */
      compat_filemap_write_and_wait(dentry->d_inode->i_mapping);
      error = HgfsPrivateGetattr(dentry, &attr, NULL);
      if (!error) {
         /*
          * If server provides file ID, we need to check whether it has changed
          * since last revalidation. There might be a case that at server side
          * the same file name has been used for other file during the period.
          */
         if (attr.mask & HGFS_ATTR_VALID_FILEID) {
            if (iinfo->hostFileId == 0) {
               /* hostFileId was invalidated, so update it here */
               iinfo->hostFileId = attr.hostFileId;
            } else if (iinfo->hostFileId != attr.hostFileId) {
               LOG(4, ("VMware hgfs: %s: host file id mismatch. Expected "
                       "%"FMT64"u, got %"FMT64"u.\n", __func__,
                       iinfo->hostFileId, attr.hostFileId));
               return -EINVAL;
            }
         }
         /* Update inode's attributes and reset the age. */
         HgfsChangeFileAttributes(dentry->d_inode, &attr);
         HgfsDentryAgeReset(dentry);
      }
   } else {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: using cached dentry "
              "attributes\n"));
   }

   return error;
}
