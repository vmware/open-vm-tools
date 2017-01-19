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
 * fsutil.c --
 *
 * Functions used in more than one type of filesystem operation will be
 * exported from this file.
 */

/* Must come before any kernel header file. */
#include "driver-config.h"

#include <linux/signal.h>

/* Must come before compat_dcache. */
#include "compat_fs.h"
#include "compat_dcache.h"
#include "compat_kernel.h"
#include "compat_mm.h"
#include "compat_sched.h"
#include "compat_slab.h"
#include "compat_spinlock.h"

#include "vm_assert.h"
#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsUtil.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "hgfsProto.h"
#include "vm_basic_types.h"

/*
 * The get inode descriptor object.
 */
typedef struct HgfsInodeAttrDesc {
   uint32 flags;
   const HgfsAttrInfo *attr;
} HgfsInodeAttrDesc;

#define HGFS_INO_DESC_INO_FAKE           (1 << 0)
#define HGFS_INO_DESC_INO_COLLISION      (1 << 1)

static void HgfsSetFileType(struct inode *inode,
                            HgfsAttrInfo const *attr);
static int HgfsUnpackGetattrReply(HgfsReq *req,
                                  HgfsAttrInfo *attr,
                                  char **fileName);
static int HgfsBuildRootPath(char *buffer,
                             size_t bufferLen,
                             HgfsSuperInfo *si);
static int HgfsBuildFullPath(char *buffer,
                             size_t bufferLen,
                             HgfsSuperInfo *si,
                             struct dentry *dentry);
static struct inode *HgfsGetInode(struct super_block *sb,
                                  ino_t ino,
                                  HgfsInodeAttrDesc *iattrDesc);
static void HgfsDoReadInode(struct inode *inode);
static int HgfsInitInode(struct inode *inode,
                         void *opaque);
static int HgfsFindInode(struct inode *inode,
                         void *opaque);

/*
 * For kernels that are older than 2.6.32 there is no truncate_pagecache call
 * so we set an empty macro.
 * If we have the call then we check for kernel 3.12 compatibility.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 32)
#define HGFS_TRUNCATE_PAGE_CACHE(inode, oldSize, newSize)
#elif defined VMW_PAGECACHE_312 || LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0)
#define HGFS_TRUNCATE_PAGE_CACHE(inode, oldSize, newSize)    truncate_pagecache(inode, newSize)
#else
#define HGFS_TRUNCATE_PAGE_CACHE(inode, oldSize, newSize)    truncate_pagecache(inode, oldSize, newSize)
#endif

/*
 * Private function implementations.
 */

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 2, 0)
/*
 *----------------------------------------------------------------------------
 *
 * set_nlink --
 *
 *    Set an inode's link count.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static inline void
set_nlink(struct inode *inode, unsigned int nlink)
{
   inode->i_nlink = nlink;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetFileType --
 *
 *    Get file type from the inode mode.
 *
 * Results:
 *    The file type.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static HgfsFileType
HgfsGetFileType(struct inode *inode)     // IN: Attrs to use
{
   HgfsFileType type;

   ASSERT(inode != NULL);

   switch (inode->i_mode & S_IFMT) {
   case S_IFLNK:
      type = HGFS_FILE_TYPE_SYMLINK;
      break;

   case S_IFREG:
      type = HGFS_FILE_TYPE_REGULAR;
      break;

   case S_IFDIR:
      type = HGFS_FILE_TYPE_DIRECTORY;
      break;

   default:
      /*
       * XXX Should never happen. Since there aren't any other HGFS supported type.
       */
      LOG(4, (KERN_DEBUG LGPFX "%s: UNSUPPORTED inode type %d\n",
              __func__, inode->i_mode & S_IFMT));
      type = 0;
      break;
   }

   LOG(10, (KERN_DEBUG LGPFX "%s: return %d\n", __func__, type));
   return type;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSetFileType --
 *
 *    Set file type in inode according to the hgfs attributes.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static void
HgfsSetFileType(struct inode *inode,          // IN/OUT: Inode to update
                HgfsAttrInfo const *attr)     // IN: Attrs to use to update
{
   ASSERT(inode);
   ASSERT(attr);

   switch (attr->type) {
   case HGFS_FILE_TYPE_DIRECTORY:
      inode->i_mode = S_IFDIR;
      inode->i_op = &HgfsDirInodeOperations;
      inode->i_fop = &HgfsDirFileOperations;
      break;

   case HGFS_FILE_TYPE_SYMLINK:
      inode->i_mode = S_IFLNK;
      inode->i_op = &HgfsLinkInodeOperations;
      break;

   case HGFS_FILE_TYPE_REGULAR:
      inode->i_mode = S_IFREG;
      inode->i_op = &HgfsFileInodeOperations;
      inode->i_fop = &HgfsFileFileOperations;
      inode->i_data.a_ops = &HgfsAddressSpaceOperations;
      break;

   default:
      /*
       * XXX Should never happen. I'd put NOT_IMPLEMENTED() here
       * but if the driver ever goes in the host it's probably not
       * a good idea for an attacker to be able to hang the host
       * simply by using a bogus file type in a reply. [bac]
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsSetFileType: UNSUPPORTED "
              "inode type\n"));
      inode->i_mode = 0;
//      NOT_IMPLEMENTED();
      break;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsUnpackGetattrReply --
 *
 *    This function abstracts the differences between a GetattrV1 and
 *    a GetattrV2. The caller provides the packet containing the reply
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
HgfsUnpackGetattrReply(HgfsReq *req,        // IN: Reply packet
                       HgfsAttrInfo *attr,  // IN/OUT: Attributes
                       char **fileName)     // OUT: file name
{
   int result;
   char *name = NULL;
   uint32 length = 0;

   ASSERT(req);
   ASSERT(attr);

   result = HgfsUnpackCommonAttr(req, attr);
   if (result != 0) {
      return result;
   }

   /* GetattrV2+ also wants a symlink target if it exists. */
   if (attr->requestType == HGFS_OP_GETATTR_V3) {
      HgfsReplyGetattrV3 *replyV3 = (HgfsReplyGetattrV3 *)(HGFS_REP_PAYLOAD_V3(req));
      name = replyV3->symlinkTarget.name;
      length = replyV3->symlinkTarget.length;

      /* Skip the symlinkTarget if it's too long. */
      if (length > HGFS_NAME_BUFFER_SIZET(req->bufferSize, sizeof *replyV3 + sizeof(HgfsReply))) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackGetattrReply: symlink "
                 "target name too long, ignoring\n"));
         return -ENAMETOOLONG;
      }
   } else if (attr->requestType == HGFS_OP_GETATTR_V2) {
      HgfsReplyGetattrV2 *replyV2 = (HgfsReplyGetattrV2 *)
         (HGFS_REQ_PAYLOAD(req));
      name = replyV2->symlinkTarget.name;
      length = replyV2->symlinkTarget.length;

      /* Skip the symlinkTarget if it's too long. */
      if (length > HGFS_NAME_BUFFER_SIZE(req->bufferSize, replyV2)) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackGetattrReply: symlink "
                 "target name too long, ignoring\n"));
         return -ENAMETOOLONG;
      }
   }

   if (fileName) {
      if (length != 0) {

         *fileName = kmalloc(length + 1, GFP_KERNEL);
         if (*fileName == NULL) {
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackGetattrReply: out of "
                    "memory allocating symlink target name, ignoring\n"));
            return -ENOMEM;
         }

         /* Copy and convert. From now on, the symlink target is in UTF8. */
         memcpy(*fileName, name, length);
         CPNameLite_ConvertFrom(*fileName, length, '/');
         (*fileName)[length] = '\0';
         LOG(4, (KERN_DEBUG "VMware hgfs: %s: symlink name %s\n",
                 __func__, *fileName));
      } else {
         *fileName = NULL;
      }
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackCommonattr --
 *
 *    This function abstracts the HgfsAttr struct behind HgfsAttrInfo.
 *    Callers can pass one of four replies into it and receive back the
 *    attributes for those replies.
 *
 *    Callers must populate attr->requestType so that we know whether to
 *    expect a V1 or V2 Attr struct.
 *
 * Results:
 *    Zero on success, non-zero otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsPackCommonattr(HgfsReq *req,            // IN/OUT: request buffer
                   HgfsOp opUsed,           // IN: Op to be used
                   HgfsHandle handle,       // IN: file handle to use if valid
                   size_t *reqSize,         // OUT: request size
                   size_t *reqBufferSize,   // OUT: request buffer size
                   char **fileName,         // OUT: pointer to request file name
                   uint32 **fileNameLength, // OUT: pointer to request file name length
                   HgfsAttrInfo *attr)      // OUT: Attrs to update
{
   int result = 0;

   attr->requestType = opUsed;

   switch (opUsed) {
   case HGFS_OP_GETATTR_V3: {
      HgfsRequest *requestHeader;
      HgfsRequestGetattrV3 *requestV3;

      /* Fill out the request packet. */
      requestHeader = (HgfsRequest *)(HGFS_REQ_PAYLOAD(req));
      requestHeader->op = opUsed;
      requestHeader->id = req->id;

      requestV3 = (HgfsRequestGetattrV3 *)HGFS_REQ_PAYLOAD_V3(req);

      /*
       * When possible, issue a getattr using an existing handle. This will
       * give us slightly better performance on a Windows server, and is more
       * correct regardless. If we don't find a handle, fall back on getattr
       * by name.
       */
      requestV3->hints = 0;
      if (handle != HGFS_INVALID_HANDLE) {
         requestV3->fileName.flags = HGFS_FILE_NAME_USE_FILE_DESC;
         requestV3->fileName.fid = handle;
         requestV3->fileName.length = 0;
         requestV3->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
         *fileName = NULL;
         *fileNameLength = NULL;
      } else {
         *fileName = requestV3->fileName.name;
         *fileNameLength = &requestV3->fileName.length;
         requestV3->fileName.flags = 0;
         requestV3->fileName.fid = HGFS_INVALID_HANDLE;
         requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      }
      requestV3->reserved = 0;
      *reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(requestV3);
      *reqBufferSize = HGFS_NAME_BUFFER_SIZET(req->bufferSize, *reqSize);
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsRequestGetattrV2 *requestV2;

      requestV2 = (HgfsRequestGetattrV2 *)(HGFS_REQ_PAYLOAD(req));
      requestV2->header.op = opUsed;
      requestV2->header.id = req->id;

      /*
       * When possible, issue a getattr using an existing handle. This will
       * give us slightly better performance on a Windows server, and is more
       * correct regardless. If we don't find a handle, fall back on getattr
       * by name.
       */
      if (handle != HGFS_INVALID_HANDLE) {
         requestV2->hints = HGFS_ATTR_HINT_USE_FILE_DESC;
         requestV2->file = handle;
         *fileName = NULL;
         *fileNameLength = NULL;
      } else {
         requestV2->hints = 0;
         *fileName = requestV2->fileName.name;
         *fileNameLength = &requestV2->fileName.length;
      }
      *reqSize = sizeof *requestV2;
      *reqBufferSize = HGFS_NAME_BUFFER_SIZE(req->bufferSize, requestV2);
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsRequestGetattr *requestV1;

      requestV1 = (HgfsRequestGetattr *)(HGFS_REQ_PAYLOAD(req));
      requestV1->header.op = opUsed;
      requestV1->header.id = req->id;

      *fileName = requestV1->fileName.name;
      *fileNameLength = &requestV1->fileName.length;
      *reqSize = sizeof *requestV1;
      *reqBufferSize = HGFS_NAME_BUFFER_SIZE(req->bufferSize, requestV1);
      break;
   }

   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: unexpected OP type encountered\n", __func__));
      result = -EPROTO;
      break;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackGetattrRequestInt --
 *
 *    Setup the getattr request, depending on the op version. When possible,
 *    we will issue the getattr using an existing open HGFS handle.
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
HgfsPackGetattrRequestInt(HgfsReq *req,            // IN/OUT: Request buffer
                          HgfsOp opUsed,           // IN: Op to be used
                          Bool allowHandleReuse,   // IN: Can we use a handle?
                          HgfsSuperInfo *si,       // IN: super block info
                          struct dentry *dentry,   // IN: Optional dentry containing name/handle
                          HgfsAttrInfo *attr)      // OUT: Attrs to update
{
   size_t reqBufferSize;
   size_t reqSize;
   char *fileName = NULL;
   uint32 *fileNameLength = NULL;
   HgfsHandle handle = HGFS_INVALID_HANDLE;
   int result = 0;

   ASSERT(attr != NULL);
   ASSERT(req != NULL);
   ASSERT(si != NULL);

   if (allowHandleReuse) {
      /* The dentry must be valid if the caller wants to use a file handle. */
      ASSERT(dentry != NULL);
      /* Errors are dropped getting the file handle, as we will use the name instead. */
      (void)HgfsGetHandle(dentry->d_inode, 0, &handle);
   }

   result = HgfsPackCommonattr(req,
                               opUsed,
                               handle,
                               &reqSize,
                               &reqBufferSize,
                               &fileName,
                               &fileNameLength,
                               attr);
   if (0 > result) {
      goto out;
   }

   /* Avoid all this extra work when we're doing a getattr by handle. */
   if (fileName != NULL) {

      /* Build full name to send to server. */
      if (HgfsBuildFullPath(fileName,
                            reqBufferSize,
                            si,
                            dentry) < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: %s: build path failed\n", __func__));
         result = -EINVAL;
         goto out;
      }
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: getting attrs for \"%s\"\n",
              __func__, fileName));

      /* Convert to CP name. */
      result = CPName_ConvertTo(fileName,
                                reqBufferSize,
                                fileName);
      if (result < 0) {
         LOG(4, (KERN_DEBUG "VMware hgfs: %s: CP conversion failed\n", __func__));
         result = -EINVAL;
         goto out;
      }

      *fileNameLength = result;
   }

   req->payloadSize = reqSize + result;
   result = 0;

out:
   return result;
}


/*
 * Public function implementations.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsUnpackCommonAttr --
 *
 *    This function abstracts the HgfsAttr struct behind HgfsAttrInfo.
 *    Callers can pass one of four replies into it and receive back the
 *    attributes for those replies.
 *
 *    Callers must populate attr->requestType so that we know whether to
 *    expect a V1 or V2 Attr struct.
 *
 * Results:
 *    Zero on success, non-zero otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */
int
HgfsUnpackCommonAttr(HgfsReq *req,            // IN: Reply packet
                     HgfsAttrInfo *attrInfo)  // OUT: Attributes
{
   HgfsReplyGetattrV3 *getattrReplyV3;
   HgfsReplyGetattrV2 *getattrReplyV2;
   HgfsReplyGetattr *getattrReplyV1;
   HgfsReplySearchReadV3 *searchReadReplyV3;
   HgfsReplySearchReadV2 *searchReadReplyV2;
   HgfsReplySearchRead *searchReadReplyV1;
   HgfsDirEntry *dirent;
   HgfsAttrV2 *attrV2 = NULL;
   HgfsAttr *attrV1 = NULL;

   ASSERT(req);
   ASSERT(attrInfo);

   switch (attrInfo->requestType) {
   case HGFS_OP_GETATTR_V3:
      getattrReplyV3 = (HgfsReplyGetattrV3 *)(HGFS_REP_PAYLOAD_V3(req));
      attrV2 = &getattrReplyV3->attr;
      break;
   case HGFS_OP_GETATTR_V2:
      getattrReplyV2 = (HgfsReplyGetattrV2 *)(HGFS_REQ_PAYLOAD(req));
      attrV2 = &getattrReplyV2->attr;
      break;
   case HGFS_OP_GETATTR:
      getattrReplyV1 = (HgfsReplyGetattr *)(HGFS_REQ_PAYLOAD(req));
      attrV1 = &getattrReplyV1->attr;
      break;
   case HGFS_OP_SEARCH_READ_V3:
      searchReadReplyV3 = (HgfsReplySearchReadV3 *)(HGFS_REP_PAYLOAD_V3(req));
      dirent = (HgfsDirEntry *)searchReadReplyV3->payload;
      attrV2 = &dirent->attr;
      break;
   case HGFS_OP_SEARCH_READ_V2:
      searchReadReplyV2 = (HgfsReplySearchReadV2 *)(HGFS_REQ_PAYLOAD(req));
      attrV2 = &searchReadReplyV2->attr;
      break;
   case HGFS_OP_SEARCH_READ:
      searchReadReplyV1 = (HgfsReplySearchRead *)(HGFS_REQ_PAYLOAD(req));
      attrV1 = &searchReadReplyV1->attr;
      break;
   default:
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsUnpackCommonAttr: unexpected op "
              "in reply packet\n"));
      return -EPROTO;
   }

   if (attrV2 != NULL) {
      attrInfo->mask = 0;

      if (attrV2->mask & HGFS_ATTR_VALID_TYPE) {
         attrInfo->type = attrV2->type;
         attrInfo->mask |= HGFS_ATTR_VALID_TYPE;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_SIZE) {
         attrInfo->size = attrV2->size;
         attrInfo->mask |= HGFS_ATTR_VALID_SIZE;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_ALLOCATION_SIZE) {
         attrInfo->allocSize = attrV2->allocationSize;
         attrInfo->mask |= HGFS_ATTR_VALID_ALLOCATION_SIZE;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
         attrInfo->accessTime = attrV2->accessTime;
         attrInfo->mask |= HGFS_ATTR_VALID_ACCESS_TIME;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_WRITE_TIME) {
         attrInfo->writeTime = attrV2->writeTime;
         attrInfo->mask |= HGFS_ATTR_VALID_WRITE_TIME;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_CHANGE_TIME) {
         attrInfo->attrChangeTime = attrV2->attrChangeTime;
         attrInfo->mask |= HGFS_ATTR_VALID_CHANGE_TIME;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_SPECIAL_PERMS) {
         attrInfo->specialPerms = attrV2->specialPerms;
         attrInfo->mask |= HGFS_ATTR_VALID_SPECIAL_PERMS;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
         attrInfo->ownerPerms = attrV2->ownerPerms;
         attrInfo->mask |= HGFS_ATTR_VALID_OWNER_PERMS;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
         attrInfo->groupPerms = attrV2->groupPerms;
         attrInfo->mask |= HGFS_ATTR_VALID_GROUP_PERMS;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_OTHER_PERMS) {
         attrInfo->otherPerms = attrV2->otherPerms;
         attrInfo->mask |= HGFS_ATTR_VALID_OTHER_PERMS;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_USERID) {
         attrInfo->userId = attrV2->userId;
         attrInfo->mask |= HGFS_ATTR_VALID_USERID;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_GROUPID) {
         attrInfo->groupId = attrV2->groupId;
         attrInfo->mask |= HGFS_ATTR_VALID_GROUPID;
      }
      if (attrV2->mask & (HGFS_ATTR_VALID_FILEID |
                          HGFS_ATTR_VALID_NON_STATIC_FILEID)) {
         attrInfo->hostFileId = attrV2->hostFileId;
         attrInfo->mask |= HGFS_ATTR_VALID_FILEID;
      }
      if (attrV2->mask & HGFS_ATTR_VALID_EFFECTIVE_PERMS) {
         attrInfo->effectivePerms = attrV2->effectivePerms;
         attrInfo->mask |= HGFS_ATTR_VALID_EFFECTIVE_PERMS;
      }
   } else if (attrV1 != NULL) {
      /* Implicit mask for a Version 1 attr. */
      attrInfo->mask = HGFS_ATTR_VALID_TYPE |
         HGFS_ATTR_VALID_SIZE |
         HGFS_ATTR_VALID_ACCESS_TIME |
         HGFS_ATTR_VALID_WRITE_TIME |
         HGFS_ATTR_VALID_CHANGE_TIME |
         HGFS_ATTR_VALID_OWNER_PERMS |
         HGFS_ATTR_VALID_EFFECTIVE_PERMS;

      attrInfo->type = attrV1->type;
      attrInfo->size = attrV1->size;
      attrInfo->accessTime = attrV1->accessTime;
      attrInfo->writeTime = attrV1->writeTime;
      attrInfo->attrChangeTime = attrV1->attrChangeTime;
      attrInfo->ownerPerms = attrV1->permissions;
      attrInfo->effectivePerms = attrV1->permissions;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCalcBlockSize --
 *
 *    Calculate the number of 512 byte blocks used.
 *
 *    Round the size to the next whole block and divide by the block size
 *    to get the number of 512 byte blocks.
 *    Note, this is taken from the nfs client and is simply performing:
 *    (size + 512-1)/ 512)
 *
 * Results:
 *    The number of 512 byte blocks for the size.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 17)
static inline blkcnt_t
HgfsCalcBlockSize(uint64 tsize)
{
   blkcnt_t used = (tsize + 511) >> 9;
   return (used > ULONG_MAX) ? ULONG_MAX : used;
}
#else
static inline unsigned long
HgfsCalcBlockSize(uint64 tsize)
{
   loff_t used = (tsize + 511) >> 9;
   return (used > ULONG_MAX) ? ULONG_MAX : used;
}
#endif


static inline int
hgfs_timespec_compare(const struct timespec *lhs, const struct timespec *rhs)
{
   if (lhs->tv_sec < rhs->tv_sec)
      return -1;
   if (lhs->tv_sec > rhs->tv_sec)
      return 1;
   return lhs->tv_nsec - rhs->tv_nsec;
}



/*
 *----------------------------------------------------------------------
 *
 * HgfsSetInodeUidGid --
 *
 *    Set the UID and GID of the inode.
 *
 *    Update an inode's UID and GID to match those of the HgfsAttr returned
 *    by the server.
 *
 * Results:
 *    The number of 512 byte blocks for the size.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsSetInodeUidGid(struct inode *inode,          // IN/OUT: Inode
                   HgfsSuperInfo *si,            // IN: New attrs
                   HgfsAttrInfo const *attr)     // IN: New attrs
{
   /*
    * Use the stored uid and gid if we were given them at mount-time, or if
    * the server didn't give us a uid or gid.
    */
   if ((si->mntFlags & HGFS_MNT_SET_UID) != 0 ||
       (attr->mask & HGFS_ATTR_VALID_USERID) == 0) {
      inode->i_uid = si->uid;
   } else {
      kuid_t attrUid = make_kuid(&init_user_ns, attr->userId);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
      if (uid_valid(attrUid)) {
         inode->i_uid = attrUid;
      } else {
         inode->i_uid = si->uid;
      }
#else
      inode->i_uid = attrUid;
#endif
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: inode uid %u\n",
               __func__, from_kuid(&init_user_ns, inode->i_uid)));
   }
   if ((si->mntFlags & HGFS_MNT_SET_GID) != 0 ||
       (attr->mask & HGFS_ATTR_VALID_GROUPID) == 0) {
      inode->i_gid = si->gid;
   } else {
      kgid_t attrGid = make_kgid(&init_user_ns, attr->groupId);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
      if (gid_valid(attrGid)) {
         inode->i_gid = attrGid;
      } else {
         inode->i_gid = si->gid;
      }
#else
      inode->i_gid = attrGid;
#endif
      LOG(6, (KERN_DEBUG "VMware hgfs: %s: inode gid %u\n",
               __func__, from_kgid(&init_user_ns, inode->i_gid)));
   }
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsInodeWritable --
 *
 *      Helper function for verifying if a file is under write access.
 *
 * Results:
 *      TRUE if file is writable, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsInodeWritable(struct inode *inode) // IN: File we're writing to
{
   HgfsInodeInfo *iinfo;
   struct list_head *cur;
   Bool isWritable = FALSE;

   iinfo = INODE_GET_II_P(inode);
   /*
    * Iterate over the open handles for this inode, and find if there
    * is one that allows the write mode.
    * Note, the mode is stored as incremented by one to prevent overload of
    * the zero value.
    */
   spin_lock(&hgfsBigLock);
   list_for_each(cur, &iinfo->files) {
      HgfsFileInfo *finfo = list_entry(cur, HgfsFileInfo, list);

      if (0 != (finfo->mode & (HGFS_OPEN_MODE_WRITE_ONLY + 1))) {
         isWritable = TRUE;
         break;
      }
   }
   spin_unlock(&hgfsBigLock);

   return isWritable;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsSafeToChange --
 *
 *      Helper function for verifying if a file inode size and time fields is safe
 *      to update. It is deemed safe only if there is not an open writer to the file.
 *
 * Results:
 *      TRUE if safe to change inode, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsSafeToChange(struct inode *inode) // IN: File we're writing to
{
   return !HgfsIsInodeWritable(inode);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsChangeFileAttributes --
 *
 *    Update an inode's attributes to match those of the HgfsAttr. May
 *    cause dirty pages to be flushed, and may invalidate cached pages,
 *    if there was a change in the file size or modification time in
 *    the server.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
HgfsChangeFileAttributes(struct inode *inode,          // IN/OUT: Inode
                         HgfsAttrInfo const *attr)     // IN: New attrs
{
   HgfsSuperInfo *si;
   HgfsInodeInfo *iinfo;
   loff_t fileSize = 0;
   Bool fileSizeChanged = FALSE;
   Bool needInvalidate = FALSE;
   Bool isSafeToChange;

   ASSERT(inode);
   ASSERT(inode->i_sb);
   ASSERT(attr);

   si = HGFS_SB_TO_COMMON(inode->i_sb);
   iinfo = INODE_GET_II_P(inode);

   /*
    * We do not want to update the file size from server or invalidate the inode
    * for inodes open for write. We need to avoid races with the write page
    * extending the file. This also will cause the server to possibly update the
    * server side file's mod time too. For those situations we do not want to blindly
    * go and invalidate the inode pages thus losing changes in flight and corrupting the
    * file.
    * We only need to invalidate the inode pages if the file has truly been modified
    * on the server side by another server side application, not by our writes.
    * If there are no writers it is safe to assume that newer mod time means the file
    * changed on the server side underneath us.
    */
   isSafeToChange = HgfsIsSafeToChange(inode);

   spin_lock(&inode->i_lock);

   iinfo = INODE_GET_II_P(inode);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: entered\n"));
   HgfsSetFileType(inode, attr);

   /*
    * Set the access mode. For hosts that don't give us group or other
    * bits (Windows), we use the owner bits in their stead.
    */
   inode->i_mode &= ~S_IALLUGO;
   if (attr->mask & HGFS_ATTR_VALID_SPECIAL_PERMS) {
      inode->i_mode |= (attr->specialPerms << 9);
   }
   if (attr->mask & HGFS_ATTR_VALID_OWNER_PERMS) {
      inode->i_mode |= (attr->ownerPerms << 6);
   }
   if (attr->mask & HGFS_ATTR_VALID_GROUP_PERMS) {
      inode->i_mode |= (attr->groupPerms << 3);
   } else {
      inode->i_mode |= ((inode->i_mode & S_IRWXU) >> 3);
   }
   if (attr->mask & HGFS_ATTR_VALID_OTHER_PERMS) {
      inode->i_mode |= (attr->otherPerms);
   } else {
      inode->i_mode |= ((inode->i_mode & S_IRWXU) >> 6);
   }

   /* Mask the access mode. */
   switch (attr->type) {
   case HGFS_FILE_TYPE_REGULAR:
      inode->i_mode &= ~si->fmask;
      break;
   case HGFS_FILE_TYPE_DIRECTORY:
      inode->i_mode &= ~si->dmask;
      break;
   default:
      /* Nothing else gets masked. */
      break;
   }

   /*
    * This field is used to represent the number of hard links. If the file is
    * really a file, this is easy; our filesystem doesn't support hard-linking,
    * so we just set it to 1. If the field is a directory, the number of links
    * represents the number of subdirectories, including '.' and "..".
    *
    * In either case, what we're doing isn't ideal. We've carefully tracked the
    * number of links through calls to HgfsMkdir and HgfsDelete, and now some
    * revalidate will make us trample on the number of links. But we have no
    * choice: someone on the server may have made our local view of the number
    * of links inconsistent (by, say, removing a directory) , and without the
    * ability to retrieve nlink via getattr, we have no way of knowing that.
    *
    * XXX: So in the future, adding nlink to getattr would be nice. At that
    * point we may as well just implement hard links anyway. Note that user
    * programs seem to have issues with a link count greater than 1 that isn't
    * accurate. I experimented with setting nlink to 2 for directories (to
    * account for '.' and ".."), and find printed a hard link error. So until
    * we have getattr support for nlink, everyone gets 1.
    */
   set_nlink(inode, 1);

   HgfsSetInodeUidGid(inode, si, attr);

   inode->i_rdev = 0;  /* Device nodes are not supported */
#if !defined VMW_INODE_2618
   inode->i_blksize = HGFS_BLOCKSIZE;
#endif

   /*
    * Invalidate cached pages if we didn't receive the file size, or if it has
    * changed on the server, and no writes in flight.
    */
   if (attr->mask & HGFS_ATTR_VALID_SIZE) {
      fileSize = compat_i_size_read(inode);
      LOG(8, (KERN_DEBUG "VMware hgfs: %s: srv size: %"FMT64"u, inode size: %Lu\n",
              __func__,  attr->size, fileSize));
      if (fileSize != attr->size) {
         if (iinfo->numWbPages == 0 && isSafeToChange) {
            fileSizeChanged = needInvalidate = TRUE;
            LOG(4, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: new file "
                    "size: %"FMT64"u, old file size: %Lu\n", attr->size, fileSize));
            compat_i_size_write(inode, attr->size);
         }
      }

      fileSize = compat_i_size_read(inode);
      if ((attr->mask & HGFS_ATTR_VALID_ALLOCATION_SIZE) != 0) {
         inode->i_blocks = HgfsCalcBlockSize(attr->allocSize);
      } else {
         uint64 allocSize = ROUNDUP(fileSize, HGFS_BLOCKSIZE);
         inode->i_blocks = HgfsCalcBlockSize(allocSize);
      }
      LOG(8, (KERN_DEBUG "VMware hgfs: %s: inode: size %Lu, blks %u\n",
              __func__,  fileSize, (uint32)inode->i_blocks));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: did not "
              "get file size\n"));
   }

   if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
      HGFS_SET_TIME(inode->i_atime, attr->accessTime);
   } else {
      HGFS_SET_TIME(inode->i_atime, HGFS_GET_CURRENT_TIME());
   }

   /*
    * Invalidate cached pages if we didn't receive the modification time, or if
    * it has changed on the server and we don't have writes in flight and any open
    * open writers.
    */
   if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
      HGFS_DECLARE_TIME(newTime);
      HGFS_SET_TIME(newTime, attr->writeTime);
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: server mod "
               "time: %ld:%lu, inode mod time: %ld:%lu\n", __func__,
               HGFS_PRINT_TIME(newTime), HGFS_PRINT_TIME(inode->i_mtime)));
      if (hgfs_timespec_compare(&newTime, &inode->i_mtime) > 0 &&
          iinfo->numWbPages == 0 &&
          isSafeToChange) {
         LOG(4, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: new mod "
                 "time: %ld:%lu, old mod time: %ld:%lu\n",
                 HGFS_PRINT_TIME(newTime), HGFS_PRINT_TIME(inode->i_mtime)));
         needInvalidate = TRUE;
      }
      HGFS_SET_TIME(inode->i_mtime, attr->writeTime);
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: did not "
              "get mod time\n"));
      HGFS_SET_TIME(inode->i_mtime, HGFS_GET_CURRENT_TIME());
   }

   /*
    * Windows doesn't know about ctime, and might send us something
    * bogus; if the ctime is invalid, use the mtime instead.
    */
   if (attr->mask & HGFS_ATTR_VALID_CHANGE_TIME) {
      if (HGFS_SET_TIME(inode->i_ctime, attr->attrChangeTime)) {
         inode->i_ctime = inode->i_mtime;
      }
   } else {
      HGFS_SET_TIME(inode->i_ctime, HGFS_GET_CURRENT_TIME());
   }

   spin_unlock(&inode->i_lock);

   if (fileSizeChanged) {
      HGFS_TRUNCATE_PAGE_CACHE(inode, fileSize, attr->size);
   }
   /*
    * Compare old size and write time with new size and write time. If there's
    * a difference (or if we didn't get a new size or write time), the file
    * must have been written to, and we need to invalidate our cached pages.
    */
   if (S_ISREG(inode->i_mode) && needInvalidate) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsChangeFileAttributes: file has "
              "changed on the server, invalidating pages.\n"));
      compat_filemap_write_and_wait(inode->i_mapping);
      if (inode->i_mapping && inode->i_mapping->nrpages != 0) {
         invalidate_inode_pages2(inode->i_mapping);
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCanRetryGetattrRequest --
 *
 *    Checks the getattr request version and downgrades the global getattr
 *    version if we can.
 *
 * Results:
 *    Returns TRUE on success and downgrades the global getattr protocol version,
 *    or FALSE if no retry is possible.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static Bool
HgfsCanRetryGetattrRequest(HgfsOp getattrOp)     // IN: getattrOp version used
{
   Bool canRetry = FALSE;

   /* Retry with older version(s). Set globally. */
   if (getattrOp == HGFS_OP_GETATTR_V3) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: Version 3 "
               "not supported. Falling back to version 2.\n", __func__));
      hgfsVersionGetattr = HGFS_OP_GETATTR_V2;
      canRetry = TRUE;
   } else if (getattrOp == HGFS_OP_GETATTR_V2) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: Version 2 "
               "not supported. Falling back to version 1.\n", __func__));
      hgfsVersionGetattr = HGFS_OP_GETATTR;
      canRetry = TRUE;
   }
   return canRetry;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSendGetattrRequest --
 *
 *    Send the getattr request and handle the reply.
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
HgfsSendGetattrRequest(HgfsReq *req,           // IN: getattr request
                       Bool *doRetry,          // OUT: Retry getattr request
                       Bool *allowHandleReuse, // IN/OUT: handle reuse
                       HgfsAttrInfo *attr,     // OUT: Attr to copy into
                       char **fileName)        // OUT: pointer to allocated file name
{
   int result;

   *doRetry = FALSE;

   result = HgfsSendRequest(req);
   if (result == 0) {
      HgfsStatus replyStatus = HgfsReplyStatus(req);

      result = HgfsStatusConvertToLinux(replyStatus);

      LOG(6, (KERN_DEBUG "VMware hgfs: %s: reply status %d -> %d\n",
              __func__, replyStatus, result));

      /*
       * If the getattr succeeded on the server, copy the stats
       * into the HgfsAttrInfo, otherwise return an error.
       */
      switch (result) {
      case 0:
         result = HgfsUnpackGetattrReply(req, attr, fileName);
         break;

      case -EIO:
	 /*
	  * Fix for bug 548177.
	  * When user deletes a share, we still show that share during directory
	  * enumeration to minimize user's surprise. Now when we get getattr on
	  * that share server returns EIO. Linux file manager doesn't like this,
	  * and it doesn't display any valid shares too. So as a workaround, we
	  * remap EIO to success and create minimal fake attributes.
	  */
         LOG(1, (KERN_DEBUG "Hgfs: %s: Server returned EIO on unknown file\n", __func__));
         /* Create fake attributes */
         attr->mask = HGFS_ATTR_VALID_TYPE | HGFS_ATTR_VALID_SIZE;
         attr->type = HGFS_FILE_TYPE_DIRECTORY;
         attr->size = 0;
         result = 0;
         break;

      case -EBADF:
         /*
          * This can happen if we attempted a getattr by handle and the handle
          * was closed. Because we have no control over the backdoor, it's
          * possible that an attacker closed our handle, in which case the
          * driver still thinks the handle is open. So a straight-up
          * "goto retry" would cause an infinite loop. Instead, let's retry
          * with a getattr by name.
          */
         if (*allowHandleReuse) {
            *allowHandleReuse = FALSE;
            *doRetry = TRUE;
         }

         /*
          * There's no reason why the server should have sent us this error
          * when we haven't used a handle. But to prevent an infinite loop in
          * the driver, let's make sure that we don't retry again.
          */
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (HgfsCanRetryGetattrRequest(attr->requestType)) {
            *doRetry = TRUE;
         }
         break;

      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: timed out\n", __func__));
   } else if (result == -EPROTO) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: protocol error: %d\n",
              __func__, result));
   } else {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: unknown error: %d\n",
              __func__, result));
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateGetattrCmn --
 *
 *    The common getattr request. Send a getattr request to the server
 *    for the indicated remote name, and if it succeeds copy the
 *    results of the getattr into the provided HgfsAttrInfo.
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
HgfsPrivateGetattrCmn(HgfsSuperInfo *si,      // IN: Super block info
                      struct dentry *dentry,  // IN: Optional dentry containing name/handle
                      Bool allowHandleReuse,  // IN: Use handle instead of name
                      HgfsAttrInfo *attr,     // OUT: Attr to copy into
                      char **fileName)        // OUT: Optional file name
{
   HgfsReq *req;
   HgfsOp opUsed;
   int result = 0;
   Bool doRetry;

   ASSERT(si != NULL);
   ASSERT(attr != NULL);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: out of memory "
              "while getting new request\n", __func__));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionGetattr;

   result = HgfsPackGetattrRequestInt(req,
                                      opUsed,
                                      allowHandleReuse,
                                      si,
                                      dentry,
                                      attr);
   if (result != 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: %s: no attrs\n", __func__));
      goto out;
   }

   result = HgfsSendGetattrRequest(req, &doRetry, &allowHandleReuse, attr, fileName);
   if (0 != result && doRetry) {
      goto retry;
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateGetattrRoot --
 *
 *    The getattr for the root. Send a getattr request to the server
 *    for the indicated remote name, and if it succeeds copy the
 *    results of the getattr into the provided HgfsAttrInfo.
 *
 *    fileName (of the root) will be set to a newly allocated string.
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
HgfsPrivateGetattrRoot(struct super_block *sb, // IN: Super block object
                       HgfsAttrInfo *attr)     // OUT: Attr to copy into
{
   HgfsSuperInfo *si;

   ASSERT(sb != NULL);

   si = HGFS_SB_TO_COMMON(sb);

   return HgfsPrivateGetattrCmn(si, NULL, FALSE, attr, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPrivateGetattr --
 *
 *    Internal getattr routine. Send a getattr request to the server
 *    for the indicated remote name, and if it succeeds copy the
 *    results of the getattr into the provided HgfsAttrInfo.
 *
 *    fileName (if supplied) will be set to a newly allocated string
 *    if the file is a symlink; it's the caller's duty to free it.
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
HgfsPrivateGetattr(struct dentry *dentry,  // IN: Dentry containing name
                   HgfsAttrInfo *attr,     // OUT: Attr to copy into
                   char **fileName)        // OUT: pointer to allocated file name
{
   HgfsSuperInfo *si;

   ASSERT(dentry);
   ASSERT(dentry->d_sb);

   si = HGFS_SB_TO_COMMON(dentry->d_sb);

   return HgfsPrivateGetattrCmn(si, dentry, TRUE, attr, fileName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIget --
 *
 *    Lookup or create an inode with the given attributes and remote filename.
 *
 *    If an inode number of zero is specified, we'll extract an inode number
 *    either from the attributes, or from calling iunique().
 *
 * Results:
 *    The inode on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

struct inode *
HgfsIget(struct super_block *sb,         // IN: Superblock of this fs
         ino_t ino,                      // IN: Inode number (optional)
         HgfsAttrInfo const *attr)       // IN: Attributes to create with
{
   HgfsInodeAttrDesc iattrDesc;
   HgfsSuperInfo *si;
   struct inode *inode;
   ino_t hashedId = ino;

   ASSERT(sb);
   ASSERT(attr);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsIget: entered\n"));

   si = HGFS_SB_TO_COMMON(sb);

   /* Initialize the descriptor to use for finding an inode. */
   iattrDesc.flags = 0;
   iattrDesc.attr = attr;

retry:
   /* No inode number? Use what's in the attributes, or call iunique(). */
   if (hashedId == 0) {
      /*
       * Try and use the server supplied ID if possible otherwise
       * failover to call iunique to generate one.
       */
      if ((si->mntFlags & HGFS_MNT_SERVER_INUM) != 0 &&
          (attr->mask & HGFS_ATTR_VALID_FILEID) != 0) {
         hashedId = HgfsUniqueidToIno(attr->hostFileId);

      } else {
         iattrDesc.flags |= HGFS_INO_DESC_INO_FAKE;
      }
   }

   if ((iattrDesc.flags & HGFS_INO_DESC_INO_FAKE) != 0) {
      hashedId = iunique(sb, HGFS_RESERVED_INO);
      hashedId = HgfsUniqueidToIno(hashedId);
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsIget: calling iget on inode number "
           "%lu\n", hashedId));

   /* Now try to find an inode for that number. */
   inode = HgfsGetInode(sb, hashedId, &iattrDesc);
   if (inode) {
      if ((iattrDesc.flags & HGFS_INO_DESC_INO_COLLISION) != 0) {
         LOG(6, (KERN_DEBUG LGPFX "%s: collision using %lu\n", __func__, hashedId));
         iput(inode);
         iattrDesc.flags &= ~HGFS_INO_DESC_INO_COLLISION;
         iattrDesc.flags |= HGFS_INO_DESC_INO_FAKE;
         goto retry;
      }
      if ((inode->i_state & I_NEW) != 0) {
         inode->i_ino = hashedId;
         HgfsDoReadInode(inode);
         unlock_new_inode(inode);
      }
      HgfsChangeFileAttributes(inode, attr);
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsIget: done\n"));
   return inode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInstantiateInode --
 *
 *    Instantiate an inode.
 *    Look up or create a new inode based on the attributes and inode number
 *    (if supplied).
 *    If an inode number of zero may be specified, in which case HgfsIget will
 *    get one from the server or, barring that, from iunique().
 *
 * Results:
 *    Zero on success, negative error otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsInstantiateInode(struct super_block *sb,   // IN: Super block
                     ino_t ino,                // IN: Inode number (optional)
                     HgfsAttrInfo const *attr, // IN: Attributes to use (optional)
                     struct inode **inode)     // OUT: instantiated inode
{
   int result = 0;

   ASSERT(sb != NULL);
   ASSERT(attr != NULL);
   ASSERT(inode != NULL);

   LOG(8, (KERN_DEBUG LGPFX "%s: entered\n", __func__));

   /*
    * Get the inode with this inode number and the attrs we got from
    * the server.
    */
   *inode = HgfsIget(sb, ino, attr);
   if (*inode == NULL) {
      LOG(4, (KERN_DEBUG LGPFX "%s: error getting inode\n", __func__));
      result = -ENOMEM;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInstantiateDentry --
 *
 *    Instantiate a dentry.
 *    Associate a dentry to a looked up or created inode.
 *
 * Results:
 *    Zero on success, negative error otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsInstantiateDentry(struct inode *inode,     // IN: inode
                      Bool rootDentry,         // IN: is root dentry or not
                      struct dentry **dentry)  // IN/OUT: dentry to instantiate
{
   int result = 0;

   ASSERT(dentry != NULL);
   ASSERT(inode != NULL);

   LOG(8, (KERN_DEBUG LGPFX "%s: instantiating dentry\n", __func__));

   if (rootDentry) {
      /*
       * Now the initialization of the inode is complete we can create
       * the root dentry which has flags initialized from the inode itself.
       */
      *dentry = d_make_root(inode);
      if (*dentry == NULL) {
         LOG(4, (KERN_WARNING LGPFX "%s: error make root dentry\n", __func__));
         result = -ENOMEM;
         goto exit;
      }
   }

   ASSERT(*dentry != NULL);

   HgfsDentryAgeReset(*dentry);
   (*dentry)->d_op = &HgfsDentryOperations;
   if (!rootDentry) {
      d_instantiate(*dentry, inode);
   }

exit:
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInstantiateRoot --
 *
 *    Gets the root dentry for a given super block.
 *
 * Results:
 *    zero and a valid root dentry on success
 *    negative value on failure
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsInstantiateRoot(struct super_block *sb,       // IN: Super block object
                    struct dentry **rootDentry)   // OUT: Root dentry
{
   int result;
   struct inode *rootInode = NULL;
   struct HgfsAttrInfo rootDentryAttr;

   ASSERT(sb != NULL);
   ASSERT(rootDentry != NULL);

   LOG(6, (KERN_DEBUG LGPFX "%s: entered\n", __func__));

   *rootDentry = NULL;

   LOG(8, (KERN_DEBUG LGPFX "%s: retrieve root attrs\n", __func__));
   result = HgfsPrivateGetattrRoot(sb, &rootDentryAttr);
   if (result) {
      LOG(4, (KERN_WARNING "VMware hgfs: %s: Could not the root attrs\n", __func__));
      goto exit;
   }

   result = HgfsInstantiateInode(sb, HGFS_ROOT_INO, &rootDentryAttr, &rootInode);
   if (result) {
      LOG(6, (KERN_DEBUG LGPFX "%s: Could not get the root inode\n", __func__));
      goto exit;
   }

   result = HgfsInstantiateDentry(rootInode, TRUE, rootDentry);
   /*
    * d_make_root() does iput() on failure; if d_make_root() completes
    * successfully then subsequent dput() will do iput() for us, so we
    * should just ignore root inode from now on.
    */
   rootInode = NULL;

exit:
   if (result) {
      iput(rootInode);
      dput(*rootDentry);
   }
   LOG(6, (KERN_DEBUG LGPFX "%s: return %d\n", __func__, result));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInstantiate --
 *
 *    Tie a dentry to a looked up or created inode. Callers may choose to
 *    supply their own attributes, or may leave attr NULL in which case the
 *    attributes will be queried from the server. Likewise, an inode number
 *    of zero may be specified, in which case HgfsIget will get one from the
 *    server or, barring that, from iunique().
 *
 * Results:
 *    Zero on success, negative error otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsInstantiate(struct dentry *dentry,    // IN: Dentry to use
                ino_t ino,                // IN: Inode number (optional)
                HgfsAttrInfo const *attr) // IN: Attributes to use (optional)
{
   struct inode *inode;
   HgfsAttrInfo newAttr;
   int result;

   ASSERT(dentry);

   LOG(8, (KERN_DEBUG LGPFX "%s: entered\n", __func__));

   /* If no specified attributes, get them from the server. */
   if (attr == NULL) {
      LOG(8, (KERN_DEBUG LGPFX "%s: issuing getattr\n", __func__));
      result = HgfsPrivateGetattr(dentry, &newAttr, NULL);
      if (result) {
         goto exit;
      }
      attr = &newAttr;
   }

   /*
    * Get the inode with this inode number and the attrs we got from
    * the server.
    */
   result = HgfsInstantiateInode(dentry->d_sb, ino, attr, &inode);
   if (result) {
      goto exit;
   }

   /* Everything worked out, instantiate the dentry. */
   result = HgfsInstantiateDentry(inode, FALSE, &dentry);

exit:
   LOG(8, (KERN_DEBUG LGPFX "%s: return %d\n", __func__, result));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBuildFullPath --
 *
 *    Constructs the full path given the super info and optional dentry.
 *    This is a wrapper to the functions to build root path, if no dentry
 *    is supplied, and the general build path routine.
 *
 * Results:
 *    If non-negative, the length of the buffer written.
 *    Otherwise, an error code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsBuildFullPath(char *buffer,           // IN/OUT: Buffer to write into
                  size_t bufferLen,       // IN: Size of buffer
                  HgfsSuperInfo *si,      // IN: Super block info
                  struct dentry *dentry)  // IN: Optional first dentry to walk
{
   int result = 0;

   if (dentry == NULL) {
      /* No dentry so we can construct only the root name. */
      result = HgfsBuildRootPath(buffer,
                                 bufferLen,
                                 si);
   } else {
      result = HgfsBuildPath(buffer,
                             bufferLen,
                             dentry);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBuildRootPath --
 *
 *    Constructs the root path given the super info.
 *
 * Results:
 *    If non-negative, the length of the buffer written.
 *    Otherwise, an error code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsBuildRootPath(char *buffer,           // IN/OUT: Buffer to write into
                  size_t bufferLen,       // IN: Size of buffer
                  HgfsSuperInfo *si)      // IN: First dentry to walk
{
   size_t shortestNameLength;
   /*
    * Buffer must hold at least the share name (which is already prefixed with
    * a forward slash), and nul.
    */
   shortestNameLength = si->shareNameLen + 1;
   if (bufferLen < shortestNameLength) {
      return -ENAMETOOLONG;
   }
   memcpy(buffer, si->shareName, shortestNameLength);

   /* Short-circuit if we're at the root already. */
   LOG(4, (KERN_DEBUG "VMware hgfs: %s: root path \"%s\"\n", __func__, buffer));
   return shortestNameLength;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBuildPath --
 *
 *    Constructs the full path given a dentry by walking the dentry and its
 *    parents back to the root. Adapted from d_path(), smb_build_path(), and
 *    build_path_from_dentry() implementations in Linux 2.6.16.
 *
 * Results:
 *    If non-negative, the length of the buffer written.
 *    Otherwise, an error code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsBuildPath(char *buffer,           // IN/OUT: Buffer to write into
              size_t bufferLen,       // IN: Size of buffer
              struct dentry *dentry)  // IN: First dentry to walk
{
   int retval;
   size_t shortestNameLength;
   HgfsSuperInfo *si;

   ASSERT(buffer);
   ASSERT(dentry);
   ASSERT(dentry->d_sb);

   si = HGFS_SB_TO_COMMON(dentry->d_sb);

   retval = HgfsBuildRootPath(buffer, bufferLen, si);
   if (0 > retval) {
      return retval;
   }

   /* Short-circuit if we're at the root already. */
   if (IS_ROOT(dentry)) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsBuildPath: Sending root \"%s\"\n",
              buffer));
      return retval;
   }

   /* Skip the share name, but overwrite our previous nul. */
   shortestNameLength = retval;
   buffer += shortestNameLength - 1;
   bufferLen -= shortestNameLength - 1;
   retval = 0;

   /*
    * Build the path string walking the tree backward from end to ROOT
    * and store it in reversed order.
    */
   dget(dentry);
   compat_lock_dentry(dentry);
   while (!IS_ROOT(dentry)) {
      struct dentry *parent;
      size_t nameLen;

      nameLen = dentry->d_name.len;
      bufferLen -= nameLen + 1;
      if (bufferLen < 0) {
         compat_unlock_dentry(dentry);
         dput(dentry);
	 LOG(4, (KERN_DEBUG "VMware hgfs: HgfsBuildPath: Ran out of space "
	         "while writing dentry name\n"));
         return -ENAMETOOLONG;
      }
      buffer[bufferLen] = '/';
      memcpy(buffer + bufferLen + 1, dentry->d_name.name, nameLen);
      retval += nameLen + 1;

      parent = dentry->d_parent;
      dget(parent);
      compat_unlock_dentry(dentry);
      dput(dentry);
      dentry = parent;
      compat_lock_dentry(dentry);
   }
   compat_unlock_dentry(dentry);
   dput(dentry);

   if (bufferLen == 0) {
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsBuildPath: Ran out of space while "
              "writing nul\n"));
      return -ENAMETOOLONG;
   }

   /* Shift the constructed string down to just past the share name. */
   memmove(buffer, buffer + bufferLen, retval);
   buffer[retval] = '\0';

   /* Don't forget the share name length (which also accounts for the nul). */
   retval += shortestNameLength;
   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsBuildPath: Built \"%s\"\n", buffer));

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDentryAgeReset --
 *
 *    Reset the age of this dentry by setting d_time to now.
 *
 *    XXX: smb_renew_times from smbfs claims it is safe to reset the time of
 *    all the parent dentries too, but how is that possible? If I stat a file
 *    using a relative path, only that relative path will be validated. Sure,
 *    it means that the parents still /exist/, but that doesn't mean their
 *    attributes are up to date.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsDentryAgeReset(struct dentry *dentry) // IN: Dentry whose age to reset
{
   ASSERT(dentry);

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsDentryAgeReset: entered\n"));
   dget(dentry);
   compat_lock_dentry(dentry);
   dentry->d_time = jiffies;
   compat_unlock_dentry(dentry);
   dput(dentry);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDentryAgeForce --
 *
 *    Set the dentry's time to 0. This makes the dentry's age "too old" and
 *    forces subsequent HgfsRevalidates to go to the server for attributes.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Subsequent HgfsRevalidate will not use cached attributes.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsDentryAgeForce(struct dentry *dentry) // IN: Dentry we want to force
{
   ASSERT(dentry);

   LOG(8, (KERN_DEBUG "VMware hgfs: HgfsDentryAgeForce: entered\n"));
   dget(dentry);
   compat_lock_dentry(dentry);
   dentry->d_time = 0;
   compat_unlock_dentry(dentry);
   dput(dentry);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenMode --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which open mode (access type) to request from
 *    the server.
 *
 * Results:
 *    Returns the correct HgfsOpenMode enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetOpenMode(uint32 flags) // IN: Open flags
{
   uint32 mask = O_RDONLY|O_WRONLY|O_RDWR;
   int result = -1;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: (%u) entered\n", __func__, flags));


   /*
    * Mask the flags to only look at the access type.
    */
   flags &= mask;

   /* Pick the correct HgfsOpenMode. */
   switch (flags) {

   case O_RDONLY:
      result = HGFS_OPEN_MODE_READ_ONLY;
      break;

   case O_WRONLY:
      result = HGFS_OPEN_MODE_WRITE_ONLY;
      break;

   case O_RDWR:
      result = HGFS_OPEN_MODE_READ_WRITE;
      break;

   default:
      /*
       * This should never happen, but it could if a userlevel program
       * is behaving poorly.
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenMode: invalid "
              "open flags %o\n", flags));
      result = -1;
      break;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: return %d\n", __func__, result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenFlags --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which flags to send to the server to open the
 *    file.
 *
 * Results:
 *    Returns the correct HgfsOpenFlags enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsGetOpenFlags(uint32 flags) // IN: Open flags
{
   uint32 mask = O_CREAT | O_TRUNC | O_EXCL;
   int result = -1;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: (%u) entered\n", __func__, flags));

   /*
    * Mask the flags to only look at O_CREAT, O_EXCL, and O_TRUNC.
    */

   flags &= mask;

   /* O_EXCL has no meaning if O_CREAT is not set. */
   if (!(flags & O_CREAT)) {
      flags &= ~O_EXCL;
   }

   /* Pick the right HgfsOpenFlags. */
   switch (flags) {

   case 0:
      /* Regular open; fails if file nonexistant. */
      result = HGFS_OPEN;
      break;

   case O_CREAT:
      /* Create file; if it exists already just open it. */
      result = HGFS_OPEN_CREATE;
      break;

   case O_TRUNC:
      /* Truncate existing file; fails if nonexistant. */
      result = HGFS_OPEN_EMPTY;
      break;

   case (O_CREAT | O_EXCL):
      /* Create file; fail if it exists already. */
      result = HGFS_OPEN_CREATE_SAFE;
      break;

   case (O_CREAT | O_TRUNC):
      /* Create file; if it exists already, truncate it. */
      result = HGFS_OPEN_CREATE_EMPTY;
      break;

   default:
      /*
       * This can only happen if all three flags are set, which
       * conceptually makes no sense because O_EXCL and O_TRUNC are
       * mutually exclusive if O_CREAT is set.
       *
       * However, the open(2) man page doesn't say you can't set all
       * three flags, and certain apps (*cough* Nautilus *cough*) do
       * so. To be friendly to those apps, we just silenty drop the
       * O_TRUNC flag on the assumption that it's safer to honor
       * O_EXCL.
       */
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenFlags: invalid open "
              "flags %o. Ignoring the O_TRUNC flag.\n", flags));
      result = HGFS_OPEN_CREATE_SAFE;
      break;
   }

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: return %d\n", __func__, result));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCreateFileInfo --
 *
 *    Create the HGFS-specific file information struct and store a pointer to
 *    it in the VFS file pointer. Also, link the file information struct in the
 *    inode's file list, so that we may find it when all we have is an inode
 *    (such as in writepage()).
 *
 * Results:
 *    Zero if success, non-zero if error.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsCreateFileInfo(struct file *file,  // IN: File pointer to attach to
                   HgfsHandle handle)  // IN: Handle returned from server
{
   HgfsFileInfo *fileInfo;
   HgfsInodeInfo *inodeInfo;
   int mode;

   ASSERT(file);

   inodeInfo = INODE_GET_II_P(file->f_dentry->d_inode);
   ASSERT(inodeInfo);

   /* Get the mode of the opened file. */
   mode = HgfsGetOpenMode(file->f_flags);
   if (mode < 0) {
      return -EINVAL;
   }

   /*
    * Store the file information for this open() in the file*.  This needs
    * to be freed on a close(). Note that we trim all flags from the open
    * mode and increment it so that it is guaranteed to be non-zero, because
    * callers of HgfsGetHandle may pass in zero as the desired mode if they
    * don't care about the mode of the opened handle.
    *
    * XXX: Move this into a slab allocator once HgfsFileInfo is large. One day
    * soon, the kernel will allow us to embed the vfs file into our file info,
    * like we currently do for inodes.
    */
   fileInfo = kmalloc(sizeof *fileInfo, GFP_KERNEL);
   if (!fileInfo) {
      return -ENOMEM;
   }
   fileInfo->handle = handle;
   fileInfo->mode = HGFS_OPEN_MODE_ACCMODE(mode) + 1;
   FILE_SET_FI_P(file, fileInfo);

   /* So that readdir() reissues open request */
   fileInfo->isStale = TRUE;
   fileInfo->direntPos = 0;

   /*
    * I don't think we need any VFS locks since we're only touching the HGFS
    * specific state. But we should still acquire our own lock.
    *
    * XXX: Better granularity on locks, etc.
    */
   spin_lock(&hgfsBigLock);
   list_add_tail(&fileInfo->list, &inodeInfo->files);
   spin_unlock(&hgfsBigLock);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsReleaseFileInfo --
 *
 *    Release HGFS-specific file information struct created in
 *    HgfsCreateFileInfo.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsReleaseFileInfo(struct file *file) // IN: File pointer to detach from
{
   HgfsFileInfo *fileInfo;
   ASSERT(file);

   fileInfo = FILE_GET_FI_P(file);
   ASSERT(fileInfo);

   spin_lock(&hgfsBigLock);
   list_del_init(&fileInfo->list);
   spin_unlock(&hgfsBigLock);

   kfree(fileInfo);
   FILE_SET_FI_P(file, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetHandle --
 *
 *    Retrieve an existing HGFS handle for this inode, assuming one exists.
 *    The handle retrieved satisfies the mode desired by the client.
 *
 *    The desired mode does not correspond directly to HgfsOpenMode. Callers
 *    should either increment the desired HgfsOpenMode, or, if any mode will
 *    do, pass zero instead. This is in line with the Linux kernel's behavior
 *    (see do_filp_open() and open_namei() for details).
 *
 * Results:
 *    Zero on success, non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsGetHandle(struct inode *inode,   // IN: Inode to search for handles
              HgfsOpenMode mode,     // IN: Mode to satisfy
              HgfsHandle *handle)    // OUT: Retrieved HGFS handle
{
   HgfsInodeInfo *iinfo;
   struct list_head *cur;
   Bool found = FALSE;

   ASSERT(handle);

   LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetHandle: desired mode %u\n", mode));

   /*
    * We may have been called from a dentry without an associated inode.
    * HgfsReadSuper is one such caller. No inode means no open files, so
    * return an error.
    */
   if (inode == NULL) {
      LOG(8, (KERN_DEBUG "VMware hgfs: HgfsGetHandle: NULL input\n"));
      return -EINVAL;
   }
   iinfo = INODE_GET_II_P(inode);

   /*
    * Unfortunately, we can't reuse handles belonging to directories. These
    * handles were created by a SearchOpen request, but the server itself
    * backed them with an artificial list of dentries populated via scandir. So
    * it can't actually use the handles for Getattr or Setattr requests, only
    * for subsequent SearchRead or SearchClose requests.
    */
   if (S_ISDIR(inode->i_mode)) {
      LOG(8, (KERN_DEBUG "VMware hgfs: HgfsGetHandle: Called on directory\n"));
      return -EINVAL;
   }

   /*
    * Iterate over the open handles for this inode, and find one that allows
    * the given mode. A desired mode of zero means "any mode will do".
    * Otherwise return an error;
    */
   spin_lock(&hgfsBigLock);
   list_for_each(cur, &iinfo->files) {
      HgfsFileInfo *finfo = list_entry(cur, HgfsFileInfo, list);

      if (mode == 0 || finfo->mode & mode) {
         *handle = finfo->handle;
         found = TRUE;
         break;
      }
   }
   spin_unlock(&hgfsBigLock);

   if (found) {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetHandle: Returning handle %d\n",
              *handle));
      return 0;
   } else {
      LOG(6, (KERN_DEBUG "VMware hgfs: HgfsGetHandle: Could not find matching "
              "handle\n"));
      return -ENOENT;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatusConvertToLinux --
 *
 *    Convert a cross-platform HGFS status code to its Linux-kernel specific
 *    counterpart.
 *
 *    Rather than encapsulate the status codes within an array indexed by the
 *    various HGFS status codes, we explicitly enumerate them in a switch
 *    statement, saving the reader some time when matching HGFS status codes
 *    against Linux status codes.
 *
 * Results:
 *    Zero if the converted status code represents success, negative error
 *    otherwise. Unknown status codes are converted to the more generic
 *    "protocol error" status code to maintain forwards compatibility.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsStatusConvertToLinux(HgfsStatus hgfsStatus) // IN: Status code to convert
{
   switch (hgfsStatus) {
   case HGFS_STATUS_SUCCESS:
      return 0;

   case HGFS_STATUS_NO_SUCH_FILE_OR_DIR:
   case HGFS_STATUS_INVALID_NAME:
      return -ENOENT;

   case HGFS_STATUS_INVALID_HANDLE:
      return -EBADF;

   case HGFS_STATUS_OPERATION_NOT_PERMITTED:
      return -EPERM;

   case HGFS_STATUS_FILE_EXISTS:
      return -EEXIST;

   case HGFS_STATUS_NOT_DIRECTORY:
      return -ENOTDIR;

   case HGFS_STATUS_DIR_NOT_EMPTY:
      return -ENOTEMPTY;

   case HGFS_STATUS_PROTOCOL_ERROR:
      return -EPROTO;

   case HGFS_STATUS_ACCESS_DENIED:
   case HGFS_STATUS_SHARING_VIOLATION:
      return -EACCES;

   case HGFS_STATUS_NO_SPACE:
      return -ENOSPC;

   case HGFS_STATUS_OPERATION_NOT_SUPPORTED:
      return -EOPNOTSUPP;

   case HGFS_STATUS_NAME_TOO_LONG:
      return -ENAMETOOLONG;

   case HGFS_STATUS_GENERIC_ERROR:
      return -EIO;

   case HGFS_STATUS_NOT_SAME_DEVICE:
      return -EXDEV;

   default:
      LOG(10, (KERN_DEBUG "VMware hgfs: HgfsStatusConvertToLinux: unknown "
               "error: %u\n", hgfsStatus));
      return -EIO;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetUidGid --
 *
 *    Sets the uid and gid of the host file represented by the provided
 *    dentry.
 *
 *    Note that this function assumes it is being called for a file that has
 *    been created on the host with the correct gid if the sgid bit is set for
 *    the parent directory.  That is, we treat the presence of the sgid bit in
 *    the parent direcory's mode as an indication not to set the gid manually
 *    ourselves here.  If we did, we would clobber the gid that the host file
 *    system chose for us automatically when the file was created.
 *
 *    Also note that the sgid bit itself would have been propagated to the new
 *    file by the host file system as well.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The host file's uid and gid are modified if the hgfs server has
 *    permission to do so.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsSetUidGid(struct inode *parent,     // IN: parent inode
              struct dentry *dentry,    // IN: dentry of file to update
              kuid_t uid,               // IN: uid to set
              kgid_t gid)               // IN: gid to set
{
   struct iattr setUidGid;

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: entered \n", __func__));

   setUidGid.ia_valid = ATTR_UID;
   setUidGid.ia_uid = uid;

   /*
    * Only set the gid if the host file system wouldn't have for us.  See the
    * comment in the function header.
    */
   if (!parent || !(parent->i_mode & S_ISGID)) {
      setUidGid.ia_valid |= ATTR_GID;
      setUidGid.ia_gid = gid;
   }

   /*
    * After the setattr, we desperately want a revalidate so we can
    * get the true attributes from the server. However, the setattr
    * may have done that for us. To prevent a spurious revalidate,
    * reset the dentry's time before the setattr. That way, if setattr
    * ends up revalidating the dentry, the subsequent call to
    * revalidate will do nothing.
    */
   HgfsDentryAgeForce(dentry);
   HgfsSetattr(dentry, &setUidGid);
   HgfsRevalidate(dentry);

   LOG(6, (KERN_DEBUG "VMware hgfs: %s: returns\n", __func__));
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetInode --
 *
 *    This function replaces iget() and should be called instead of it.
 *    HgfsGetInode() obtains an inode and, if it is a new one, initializes
 *    it calling HgfsDoReadInode().
 *
 * Results:
 *    A new inode object on success, NULL on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static struct inode *
HgfsGetInode(struct super_block *sb, // IN: file system superblock object
             ino_t ino,                    // IN: inode number to assign to new inode
             HgfsInodeAttrDesc *iattrDesc) // IN: Attributes to create with
{
   return iget5_locked(sb, ino, HgfsFindInode, HgfsInitInode, iattrDesc);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFindInode --
 *
 *    This function is a callback used for comparisons between inodes.
 *
 * Results:
 *    1 on success, 0 on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsFindInode(struct inode *inode,    // IN: compared inode
              void *opaque)           // IN: attribute data
{
   HgfsInodeAttrDesc *iattrDesc = opaque;
   HgfsInodeInfo *iinfo = INODE_GET_II_P(inode);
   int result = 1;

   ASSERT(iattrDesc != NULL);

   if (iinfo->hostFileId != iattrDesc->attr->hostFileId) {
      /* Don't match inode with different unique Ids. */
      result = 0;

   } else if (iinfo->isFakeInodeNumber ||
              iattrDesc->attr->type == HGFS_FILE_TYPE_DIRECTORY) {
      iattrDesc->flags |= HGFS_INO_DESC_INO_COLLISION;
      /* Deal with a collision and retry again. */
      result = 1;

   } else if (iattrDesc->attr->type != HgfsGetFileType(inode)) {
      /* Don't match inodes of different types. */
      result = 0;

   } else if (is_bad_inode(inode)) {
      result = 0;
   }

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitInode --
 *
 *    This function is a callback used to initialize a new struct inode.
 *
 * Results:
 *    Zero on success always.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsInitInode(struct inode *inode,    // IN: inode to init
              void *opaque)           // IN: attributes to init with
{
   HgfsInodeAttrDesc *iattrDesc = opaque;
   HgfsInodeInfo *iinfo = INODE_GET_II_P(inode);

   ASSERT(iattrDesc != NULL);

   if ((iattrDesc->attr->mask & HGFS_ATTR_VALID_FILEID) != 0) {
      iinfo->hostFileId = iattrDesc->attr->hostFileId;
   }

   iinfo->isFakeInodeNumber = (iattrDesc->flags & HGFS_INO_DESC_INO_FAKE) != 0;
   return 0;
}



/*
 *----------------------------------------------------------------------------
 *
 *  HgfsDoReadInode --
 *
 *    A filesystem wide function that is called to initialize a new inode.
 *    This is called from two different places depending on the kernel version.
 *    In older kernels that provide the iget() interface, this function is
 *    called by the kernel as part of inode initialization (from
 *    HgfsDoReadInode). In newer kernels that call iget_locked(), this
 *    function is called by filesystem code to initialize the new inode.
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
HgfsDoReadInode(struct inode *inode)  // IN: Inode to initialize
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
   INODE_SET_II_P(inode, iinfo);
   INIT_LIST_HEAD(&iinfo->files);
   iinfo->createdAndUnopened = FALSE;
   iinfo->numWbPages = 0;
   INIT_LIST_HEAD(&iinfo->listWbPages);

}
