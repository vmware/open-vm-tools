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
 * Inode operations for the filesystem portion of the vmhgfs driver.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/errno.h>
#include <linux/pagemap.h>

#include "compat_fs.h"
#include "compat_highmem.h"
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

/*
 * The inode_operations structure changed in 2.5.18:
 * before:
 * . 'getattr' was defined but unused
 * . 'revalidate' was defined and used
 * after:
 * 1) 'getattr' changed and became used
 * 2) 'revalidate' was removed
 *
 * Note: Mandrake backported 1) but not 2) starting with 2.4.8-26mdk
 *
 *   --hpreg
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 18)
#   define HGFS_GETATTR_ONLY 1
#else
#   undef HGFS_GETATTR_ONLY
#endif


/* Private functions. */
static int HgfsDelete(struct inode *dir,
                      struct dentry *dentry,
                      HgfsOp op);
static int HgfsPackSetattrRequest(struct iattr *iattr,
                                  struct dentry *dentry,
                                  HgfsReq *req,
                                  Bool *changed,
                                  Bool allowHandleReuse);
static int HgfsPackCreateDirRequest(struct dentry *dentry,
                                    int mode,
                                    HgfsReq *req);
static int HgfsTruncatePages(struct inode *inode,
                             loff_t newSize);

/* HGFS inode operations. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static int HgfsCreate(struct inode *dir,
                      struct dentry *dentry,
                      int mode,
                      struct nameidata *nd);
static struct dentry *HgfsLookup(struct inode *dir,
                                 struct dentry *dentry,
                                 struct nameidata *nd);
#else
static int HgfsCreate(struct inode *dir,
                      struct dentry *dentry,
                      int mode);
static struct dentry *HgfsLookup(struct inode *dir,
                                 struct dentry *dentry);
#endif
static int HgfsMkdir(struct inode *dir,
                     struct dentry *dentry,
                     int mode);
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
#ifdef HGFS_GETATTR_ONLY
static int HgfsGetattr(struct vfsmount *mnt,
                       struct dentry *dentry,
                       struct kstat *stat);
#endif

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
   .setattr     = HgfsSetattr,

#ifdef HGFS_GETATTR_ONLY
   /* Optional */
   .getattr     = HgfsGetattr,
#else
   /* Optional */
   .revalidate  = HgfsRevalidate,
#endif
};

/* HGFS inode operations structure for files. */
struct inode_operations HgfsFileInodeOperations = {
   .setattr     = HgfsSetattr,

#ifdef HGFS_GETATTR_ONLY
   /* Optional */
   .getattr     = HgfsGetattr,
#else
   /* Optional */
   .revalidate  = HgfsRevalidate,
#endif
};

/*
 * Private functions implementations.
 */

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
   struct HgfsSuperInfo *si;
   HgfsRequestDelete *request;
   HgfsReplyDelete *reply;
   HgfsReq *req = NULL;
   int result = 0;
   Bool secondAttempt = FALSE;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);
   ASSERT(dentry->d_inode);

   if (!dir || !dentry) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: NULL input\n"));
      result = -EFAULT;
      goto out;
   }

   /* Check opcode. */
   if ((op != HGFS_OP_DELETE_FILE) &&
      (op != HGFS_OP_DELETE_DIR)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: Invalid opcode\n"));
      result = -EINVAL;
      goto out;
   }


   si = HGFS_SB_TO_COMMON(dir->i_sb);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   request = (HgfsRequestDelete *)(HGFS_REQ_PAYLOAD(req));

  retry:
   /* Fill out the request packet. */
   request->header.id = req->id;
   request->header.op = op;

   /* Build full name to send to server. */
   if (HgfsBuildPath(request->fileName.name, HGFS_NAME_BUFFER_SIZE(request),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: build path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDelete: deleting \"%s\", op %u\n",
           request->fileName.name, op));

   /* Convert to CP name. */
   result = CPName_ConvertTo(request->fileName.name,
                             HGFS_NAME_BUFFER_SIZE(request),
                             request->fileName.name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: CP conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   /* Unescape the CP name. */
   result = HgfsUnescapeBuffer(request->fileName.name, result);
   request->fileName.length = result;
   req->payloadSize = sizeof *request + result;

   result = HgfsSendRequest(req);
   if (result == 0) {
      if (req->payloadSize != sizeof *reply) {
         /* This packet size should never vary. */
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: wrong packet size\n"));
         result = -EPROTO;
      } else {

         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsDelete: got reply\n"));
         reply = (HgfsReplyDelete *)(HGFS_REQ_PAYLOAD(req));
         result = HgfsStatusConvertToLinux(reply->header.status);

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
               struct iattr enableWrite;
               secondAttempt = TRUE;

               LOG(4, (KERN_DEBUG "VMware hgfs: HgfsDelete: access denied, "
                       "attempting to work around read-only bit\n"));
               enableWrite.ia_mode = (dentry->d_inode->i_mode | S_IWUSR);
               enableWrite.ia_valid = ATTR_MODE;
               result = HgfsSetattr(dentry, &enableWrite);
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
         default:
            break;
         }
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
                       HgfsReq *req,          // IN/OUT: Packet to write into
                       Bool *changed,         // OUT: Have the attrs changed?
                       Bool allowHandleReuse) // IN: Can we use a handle?
{
   HgfsRequest *requestHeader;
   HgfsRequestSetattrV2 *requestV2;
   HgfsRequestSetattr *request;
   HgfsAttrV2 *attrV2;
   HgfsAttr *attr;
   HgfsAttrHint *hints;
   HgfsAttrChanges *update;
   HgfsFileName *fileNameP;
   HgfsHandle handle;
   unsigned int valid;
   size_t reqBufferSize;
   size_t reqSize;
   int result = 0;

   ASSERT(iattr);
   ASSERT(dentry);
   ASSERT(req);
   ASSERT(changed);

   valid = iattr->ia_valid;

   requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
   switch (requestHeader->op) {
   case HGFS_OP_SETATTR_V2:
      requestV2 = (HgfsRequestSetattrV2 *)(HGFS_REQ_PAYLOAD(req));
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
         fileNameP = NULL;
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: setting "
                 "attributes of handle %u\n", handle));
      } else {
         fileNameP = &requestV2->fileName;
      }
      reqSize = sizeof *requestV2;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(requestV2);

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
         attrV2->userId = iattr->ia_uid;
         *changed = TRUE;
      }

      if (valid & ATTR_GID) {
         attrV2->mask |= HGFS_ATTR_VALID_GROUPID;
         attrV2->groupId = iattr->ia_gid;
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
   case HGFS_OP_SETATTR:
      request = (HgfsRequestSetattr *)(HGFS_REQ_PAYLOAD(req));
      attr = &request->attr;
      update = &request->update;

      /* We'll use these later. */
      fileNameP = &request->fileName;
      reqSize = sizeof *request;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(request);


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
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Avoid all this extra work when we're doing a setattr by handle. */
   if (fileNameP != NULL) {

      /* Build full name to send to server. */
      if (HgfsBuildPath(fileNameP->name, reqBufferSize, dentry) < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: build path "
                 "failed\n"));
         return -EINVAL;
      }
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: setting "
              "attributes of \"%s\"\n", fileNameP->name));

      /* Convert to CP name. */
      result = CPName_ConvertTo(fileNameP->name,
                                reqBufferSize,
                                fileNameP->name);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackSetattrRequest: CP "
                 "conversion failed\n"));
         return -EINVAL;
      }

      /* Unescape the CP name. */
      result = HgfsUnescapeBuffer(fileNameP->name, result);
      fileNameP->length = result;
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
                         int mode,              // IN: Mode to assign dir
                         HgfsReq *req)          // IN/OUT: Packet to write into
{
   HgfsRequest *requestHeader;
   HgfsRequestCreateDirV2 *requestV2;
   HgfsRequestCreateDir *request;
   HgfsFileName *fileNameP;
   size_t requestSize;
   int result;

   ASSERT(dentry);
   ASSERT(req);

   requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));

   switch (requestHeader->op) {
   case HGFS_OP_CREATE_DIR_V2:
      requestV2 = (HgfsRequestCreateDirV2 *)(HGFS_REQ_PAYLOAD(req));

      /* We'll use these later. */
      fileNameP = &requestV2->fileName;
      requestSize = sizeof *requestV2;

      requestV2->mask =
         HGFS_CREATE_DIR_VALID_FILE_NAME |
         HGFS_CREATE_DIR_VALID_SPECIAL_PERMS |
         HGFS_CREATE_DIR_VALID_OWNER_PERMS |
         HGFS_CREATE_DIR_VALID_GROUP_PERMS |
         HGFS_CREATE_DIR_VALID_OTHER_PERMS;

      /* Set permissions. */
      requestV2->specialPerms = (mode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
      requestV2->ownerPerms = (mode & S_IRWXU) >> 6;
      requestV2->groupPerms = (mode & S_IRWXG) >> 3;
      requestV2->otherPerms = (mode & S_IRWXO);
      break;
   case HGFS_OP_CREATE_DIR:
      request = (HgfsRequestCreateDir *)(HGFS_REQ_PAYLOAD(req));

      /* We'll use these later. */
      fileNameP = &request->fileName;
      requestSize = sizeof *request;

      /* Set permissions. */
      request->permissions = (mode & S_IRWXU) >> 6;
      break;
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: unexpected "
              "OP type encountered\n"));
      return -EPROTO;
   }

   /* Build full name to send to server. */
   if (HgfsBuildPath(fileNameP->name,
                     HGFS_PACKET_MAX - (requestSize - 1),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: build path "
              "failed\n"));
      return -EINVAL;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: create dir "
           "\"%s\", perms %o\n", fileNameP->name, mode));

   /* Convert to CP name. */
   result = CPName_ConvertTo(fileNameP->name,
                             HGFS_PACKET_MAX - (requestSize - 1),
                             fileNameP->name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsPackCreateDirRequest: CP "
              "conversion failed\n"));
      return -EINVAL;
   }

   /* Unescape the CP name. */
   result = HgfsUnescapeBuffer(fileNameP->name, result);
   fileNameP->length = result;
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
   result = compat_vmtruncate(inode, newSize);
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static int
HgfsCreate(struct inode *dir,     // IN: Parent dir to create in
           struct dentry *dentry, // IN: Dentry containing name to create
           int mode,              // IN: Mode of file to be created
	   struct nameidata *nd)  // IN: Intent, vfsmount, ...
#else
static int
HgfsCreate(struct inode *dir,     // IN: Parent dir to create in
           struct dentry *dentry, // IN: Dentry containing name to create
           int mode)              // IN: Mode of file to be created
#endif
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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 75)
static struct dentry *
HgfsLookup(struct inode *dir,      // IN: Inode of parent directory
           struct dentry *dentry,  // IN: Dentry containing name to look up
	   struct nameidata *nd)   // IN: Intent, vfsmount, ...
#else
static struct dentry *
HgfsLookup(struct inode *dir,      // IN: Inode of parent directory
           struct dentry *dentry)  // IN: Dentry containing name to look up
#endif
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
   attr.fileName = NULL;
   error = HgfsPrivateGetattr(dentry, &attr);
   if (!error) {
      /* File exists on the server. */

      /*
       * Get the inode with this inode number and the attrs we got from
       * the server.
       */
      inode = HgfsIget(dir->i_sb, 0, &attr);
      kfree(attr.fileName);
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
          int mode)              // IN: Mode of dir to be created
{
   struct HgfsSuperInfo *si;
   HgfsReq *req;
   HgfsOp opUsed;
   HgfsRequest *requestHeader;
   HgfsReplyCreateDir *reply;
   int result = 0;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);

   si = HGFS_SB_TO_COMMON(dir->i_sb);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
  retry:
   /*
    * Set up pointers using the proper struct This lets us check the
    * version exactly once and use the pointers later.
    */
   requestHeader->op = opUsed = atomic_read(&hgfsVersionCreateDir);
   requestHeader->id = req->id;

   result = HgfsPackCreateDirRequest(dentry, mode, req);
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
      if (req->payloadSize != sizeof *reply) {
         /* This packet size should never vary. */
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: wrong packet size\n"));
         result = -EPROTO;
      } else {

         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsMkdir: got reply\n"));
         reply = (HgfsReplyCreateDir *)(HGFS_REQ_PAYLOAD(req));
         result = HgfsStatusConvertToLinux(reply->header.status);

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
               HgfsSetUidGid(dir, dentry, current->fsuid, current->fsgid);
            }

            /*
             * XXX: When we support hard links, this is a good place to
             * increment link count of parent dir.
             */
            break;
         case -EPROTO:
         /* Retry with Version 1 of CreateDir. Set globally. */
         if (opUsed == HGFS_OP_CREATE_DIR_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsMkdir: Version 2 not "
                    "supported. Falling back to version 1.\n"));
            atomic_set(&hgfsVersionCreateDir, HGFS_OP_CREATE_DIR);
            goto retry;
         }

         /* Fallthrough. */
         default:
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsMkdir: directory was not "
                    "created, error %d\n", result));
            break;
         }
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
   struct HgfsSuperInfo *si;
   HgfsReq *req = NULL;
   HgfsRequestRename *request;
   HgfsReplyRename *reply;
   HgfsFileName *newNameP = NULL;
   int result = 0;

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

   si = HGFS_SB_TO_COMMON(oldDir->i_sb);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   request = (HgfsRequestRename *)(HGFS_REQ_PAYLOAD(req));

   request->header.id = req->id;
   request->header.op = HGFS_OP_RENAME;

   /* Build full old name to send to server. */
   if (HgfsBuildPath(request->oldName.name, HGFS_NAME_BUFFER_SIZE(request),
                     oldDentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: build old path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: Old name: \"%s\"\n",
           request->oldName.name));

   /* Convert old name to CP format. */
   result = CPName_ConvertTo(request->oldName.name,
                             HGFS_NAME_BUFFER_SIZE(request),
                             request->oldName.name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: oldName CP "
              "conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   /* Unescape the old CP name. */
   result = HgfsUnescapeBuffer(request->oldName.name, result);
   request->oldName.length = result;
   req->payloadSize = sizeof *request + result;

   /*
    * Build full new name to send to server.
    * Note the different buffer length. This is because HgfsRequestRename
    * contains two filenames, and once we place the first into the packet we
    * must account for it when determining the amount of buffer available for
    * the second.
    */
   newNameP = (HgfsFileName *)((char *)&request->oldName +
                               sizeof request->oldName + result);
   if (HgfsBuildPath(newNameP->name, HGFS_NAME_BUFFER_SIZE(request) - result,
                     newDentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: build new path failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: New name: \"%s\"\n",
           newNameP->name));

   /* Convert new name to CP format. */
   result = CPName_ConvertTo(newNameP->name,
                             HGFS_NAME_BUFFER_SIZE(request) - result,
                             newNameP->name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: newName CP "
              "conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   /* Unescape the new CP name. */
   result = HgfsUnescapeBuffer(newNameP->name, result);
   newNameP->length = result;
   req->payloadSize += result;

   result = HgfsSendRequest(req);
   if (result == 0) {
      if (req->payloadSize != sizeof *reply) {
         /* This packet size should never vary. */
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRename: wrong packet size\n"));
         result = -EPROTO;
      } else {
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRename: got reply\n"));
         reply = (HgfsReplyRename *)(HGFS_REQ_PAYLOAD(req));
         result = HgfsStatusConvertToLinux(reply->header.status);
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

out:
   HgfsFreeRequest(req);
   return result;
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
   struct HgfsSuperInfo *si;
   HgfsReq *req;
   HgfsRequestSymlinkCreate *request;
   HgfsReplySymlinkCreate *reply;
   HgfsFileName *targetNameP = NULL;
   int result = 0;
   size_t targetNameBytes;

   ASSERT(dir);
   ASSERT(dir->i_sb);
   ASSERT(dentry);
   ASSERT(symname);

   si = HGFS_SB_TO_COMMON(dir->i_sb);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   request = (HgfsRequestSymlinkCreate *)(HGFS_REQ_PAYLOAD(req));

   request->header.id = req->id;
   request->header.op = HGFS_OP_CREATE_SYMLINK;

   /* Build full symlink name to send to server. */
   if (HgfsBuildPath(request->symlinkName.name, HGFS_NAME_BUFFER_SIZE(request),
                     dentry) < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: build symlink path "
              "failed\n"));
      result = -EINVAL;
      goto out;
   }
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: Symlink name: \"%s\"\n",
           request->symlinkName.name));

   /* Convert symlink name to CP format. */
   result = CPName_ConvertTo(request->symlinkName.name,
                             HGFS_NAME_BUFFER_SIZE(request),
                             request->symlinkName.name);
   if (result < 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: symlinkName CP "
              "conversion failed\n"));
      result = -EINVAL;
      goto out;
   }

   /* Unescape the symlink CP name. */
   result = HgfsUnescapeBuffer(request->symlinkName.name, result);
   request->symlinkName.length = result;
   req->payloadSize = sizeof *request + result;

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
   targetNameP = (HgfsFileName *)((char *)&request->symlinkName +
                                  sizeof request->symlinkName + result);
   targetNameBytes = strlen(symname) + 1;

   /* Copy target name into request packet. */
   if (targetNameBytes > HGFS_NAME_BUFFER_SIZE(request) - result) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: target name is too "
              "big\n"));
      result = -EINVAL;
      goto out;
   }
   memcpy(targetNameP->name, symname, targetNameBytes);
   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: target name: \"%s\"\n",
           targetNameP->name));

   /* Convert target name to CPName-lite format. */
   CPNameLite_ConvertTo(targetNameP->name, targetNameBytes - 1, '/');

   /* Unescape the target CP-lite name. */
   result = HgfsUnescapeBuffer(targetNameP->name, targetNameBytes - 1);
   targetNameP->length = result;
   req->payloadSize += result;

   result = HgfsSendRequest(req);
   if (result == 0) {
      if (req->payloadSize != sizeof *reply) {
         /* This packet size should never vary. */
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSymlink: wrong packet size\n"));
         result = -EPROTO;
      } else {
         LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: got reply\n"));
         reply = (HgfsReplySymlinkCreate *)(HGFS_REQ_PAYLOAD(req));
         result = HgfsStatusConvertToLinux(reply->header.status);
         if (result == 0) {
            LOG(6, (KERN_DEBUG "VMware hgfs: HgfsSymlink: symlink created "
                    "successfully, instantiating dentry\n"));
            result = HgfsInstantiate(dentry, 0, NULL);
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


#ifdef HGFS_GETATTR_ONLY
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
#endif

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
   struct HgfsSuperInfo *si;
   HgfsReq *req;
   HgfsRequest *requestHeader;
   HgfsReplySetattr *reply;
   int result = 0;
   Bool changed = FALSE;
   Bool allowHandleReuse = TRUE;
   HgfsOp opUsed;

   ASSERT(dentry);
   ASSERT(dentry->d_inode);
   ASSERT(dentry->d_inode->i_mapping);
   ASSERT(dentry->d_sb);
   ASSERT(iattr);

   si = HGFS_SB_TO_COMMON(dentry->d_sb);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: out of memory while "
              "getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

   requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));

  retry:
   /* Fill out the request packet. */
   requestHeader->op = opUsed = atomic_read(&hgfsVersionSetattr);
   requestHeader->id = req->id;
   result = HgfsPackSetattrRequest(iattr, dentry, req, &changed,
                                   allowHandleReuse);
   if (result != 0 || !changed) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: no attrs changed\n"));
      goto out;
   }

   /*
    * Flush all dirty pages prior to sending the request if we're going to
    * modify the file size.
    */
   if (iattr->ia_valid & ATTR_SIZE) {
      compat_filemap_write_and_wait(dentry->d_inode->i_mapping);
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      reply = (HgfsReplySetattr *)(HGFS_REQ_PAYLOAD(req));
      result = HgfsStatusConvertToLinux(reply->header.status);

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
         /* Retry with Version 1 of Setattr. Set globally. */
         if (opUsed == HGFS_OP_SETATTR_V2) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetattr: Version 2 "
                    "not supported. Falling back to version 1.\n"));
            atomic_set(&hgfsVersionSetattr, HGFS_OP_SETATTR);
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
   HgfsAttrInfo attr;
   int error = 0;
   HgfsSuperInfo *si;
   unsigned long age;

   ASSERT(dentry);
   si = HGFS_SB_TO_COMMON(dentry->d_sb);

   if (!dentry->d_inode) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: null input\n"));
      return -EINVAL;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: name %s, "
           "inum %lu\n", dentry->d_name.name, dentry->d_inode->i_ino));

   age = jiffies - dentry->d_time;
   if (age > si->ttl) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: dentry is too old, "
              "getting new attributes\n"));
      /*
       * Sync unwritten file data so the file size on the server will
       * be current with our view of the file.
       */
      compat_filemap_write_and_wait(dentry->d_inode->i_mapping);
      attr.fileName = NULL;
      error = HgfsPrivateGetattr(dentry,
                                 &attr);
      if (!error) {
         /* No error, so update inode's attributes and reset the age. */
         HgfsChangeFileAttributes(dentry->d_inode, &attr);
         HgfsDentryAgeReset(dentry);
         kfree(attr.fileName);
      }
   } else {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsRevalidate: using cached dentry "
              "attributes\n"));
   }

   return error;
}
