/*********************************************************
 * Copyright (C) 2013,2019 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * fsutil.c --
 *
 * Functions used in more than one type of filesystem operation will be
 * exported from this file.
 */

#include <limits.h>
#include "module.h"
#include "cache.h"

typedef unsigned short umode_t;


static int
HgfsPackGetattrRequest(HgfsReq *req,
                       HgfsHandle handle,
                       const char* path,
                       Bool allowHandleReuse,
                       HgfsOp opUsed,
                       HgfsAttrInfo *attr);



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
                       HgfsAttrInfo *attr)  // IN/OUT: Attributes
{
   int result;
   char *name = NULL;
   uint32 length = 0;
   void *rawAttr;
   HgfsOp opUsed;

   ASSERT(req);
   ASSERT(attr);

   opUsed = attr->requestType;
   switch (opUsed) {
   case HGFS_OP_GETATTR_V3: {
      HgfsReplyGetattrV3 *getattrReplyV3 = HgfsGetReplyPayload(req);

      rawAttr = &getattrReplyV3->attr;
      break;
   }
   case HGFS_OP_GETATTR_V2: {
      HgfsReplyGetattrV2 *getattrReplyV2 =
         (HgfsReplyGetattrV2 *)(HGFS_REQ_PAYLOAD(req));

      rawAttr = &getattrReplyV2->attr;
      break;
   }
   case HGFS_OP_GETATTR: {
      HgfsReplyGetattr *getattrReplyV1;
      getattrReplyV1 = (HgfsReplyGetattr *)(HGFS_REQ_PAYLOAD(req));
      rawAttr = &getattrReplyV1->attr;
      break;
   }
   default:
      LOG(4, ("Unexpected op in reply packet. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   result = HgfsUnpackCommonAttr(rawAttr, opUsed, attr);
   if (result != 0) {
      return result;
   }

   /* GetattrV2+ also wants a symlink target if it exists. */
   if (attr->requestType == HGFS_OP_GETATTR_V3) {
      HgfsReplyGetattrV3 *replyV3 = HgfsGetReplyPayload(req);

      name = replyV3->symlinkTarget.name;
      length = replyV3->symlinkTarget.length;

      /* Skip the symlinkTarget if it's too long. */
      if (length > HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX,
                                          sizeof *replyV3 + sizeof (HgfsReply))) {
         LOG(4, ("symlink target name too long, ignoring\n"));
         return -ENAMETOOLONG;
      }
   } else if (attr->requestType == HGFS_OP_GETATTR_V2) {
      HgfsReplyGetattrV2 *replyV2 = (HgfsReplyGetattrV2 *)
         (HGFS_REQ_PAYLOAD(req));
      name = replyV2->symlinkTarget.name;
      length = replyV2->symlinkTarget.length;

      /* Skip the symlinkTarget if it's too long. */
      if (length > HGFS_NAME_BUFFER_SIZE(HGFS_LARGE_PACKET_MAX, replyV2)) {
         LOG(4, ("symlink target name too long, ignoring\n"));
         return -ENAMETOOLONG;
      }
   }

   if (length != 0) {
      attr->fileName = (char*)malloc(length + 1);
      if (attr->fileName == NULL) {
         LOG(4, ("Out of memory allocating symlink target name, ignoring\n"));
         return -ENOMEM;
      }

      /* Copy and convert. From now on, the symlink target is in UTF8. */
      memcpy(attr->fileName, name, length);
      CPNameLite_ConvertFrom(attr->fileName, length, '/');
      attr->fileName[length] = '\0';
   }

   /*
    * Set the access mode. For hosts that don't give us group or other
    * bits (Windows), we use the owner bits in their stead.
    */
   ASSERT(attr->mask & HGFS_ATTR_VALID_OWNER_PERMS);
   if ((attr->mask & HGFS_ATTR_VALID_GROUP_PERMS) == 0) {
      attr->groupPerms = attr->ownerPerms;
      attr->mask |= HGFS_ATTR_VALID_GROUP_PERMS;
   }
   if ((attr->mask & HGFS_ATTR_VALID_OTHER_PERMS) == 0) {
      attr->otherPerms = attr->ownerPerms;
      attr->mask |= HGFS_ATTR_VALID_OTHER_PERMS;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackGetattrRequest --
 *
 *    Setup the getattr request
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
HgfsPackGetattrRequest(HgfsReq *req,            // IN/OUT: Request buffer
                       HgfsHandle handle,       // IN: file handle
                       const char* path,        // IN: path to a file
                       Bool handleReuse,        // IN: Can we use a handle?
                       HgfsOp opUsed,           // IN: Op to be used
                       HgfsAttrInfo *attr)      // OUT: Attrs to update
{
   size_t reqBufferSize;
   size_t reqSize;
   int result = 0;
   ASSERT(attr);
   ASSERT(req);
   ASSERT(path);
   attr->requestType = opUsed;
   (void) handle;
   (void) handleReuse;

   switch (opUsed) {
   case HGFS_OP_GETATTR_V3: {
      HgfsRequestGetattrV3 *requestV3 = HgfsGetRequestPayload(req);

      /* Fill out the request packet. */
      requestV3->hints = 0;
      requestV3->fileName.flags = 0;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;

      requestV3->reserved = 0;
      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize);

      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                requestV3->fileName.name);
      LOG(8, ("Converted path %s\n", requestV3->fileName.name));
      if (result < 0) {
         LOG(8, ("CP conversion failed.\n"));
         result = -EINVAL;
         goto out;
      }
      requestV3->fileName.length = result;
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsRequestGetattrV2 *requestV2;

      LOG(8, ("Version 2 OP type encountered\n"));

      requestV2 = (HgfsRequestGetattrV2 *)(HGFS_REQ_PAYLOAD(req));
      requestV2->hints = 0;
      reqSize = sizeof *requestV2;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(HGFS_LARGE_PACKET_MAX, requestV2);

      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                requestV2->fileName.name);
      LOG(8, ("Converted path %s\n", requestV2->fileName.name));
      if (result < 0) {
         LOG(8, ("CP conversion failed.\n"));
         result = -EINVAL;
         goto out;
      }
      requestV2->fileName.length = result;
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsRequestGetattr *requestV1;
      requestV1 = (HgfsRequestGetattr *)(HGFS_REQ_PAYLOAD(req));
      reqSize = sizeof *requestV1;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(HGFS_LARGE_PACKET_MAX, requestV1);

      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                requestV1->fileName.name);
      LOG(8, ("Converted path %s\n", requestV1->fileName.name));
      if (result < 0) {
         LOG(8, ("CP conversion failed.\n"));
         result = -EINVAL;
         goto out;
      }
      requestV1->fileName.length = result;
      break;
   }

   default:
      LOG(8, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      result = -EPROTO;
      goto out;
   }

   req->payloadSize = reqSize + result;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   result = 0;

out:
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsUnpackCommonAttr --
 *
 *    This function abstracts the HgfsAttr struct behind HgfsAttrInfo.
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
HgfsUnpackCommonAttr(void *rawAttr,          // IN: Attr in reply packet
                     HgfsOp requestType,     // IN: request type
                     HgfsAttrInfo *attrInfo) // OUT: Attributes
{
   HgfsAttrV2 *attrV2 = NULL;
   HgfsAttr *attrV1 = NULL;

   ASSERT(rawAttr);
   ASSERT(attrInfo);

   switch (requestType) {
   case HGFS_OP_GETATTR_V3:
      attrV2 = rawAttr;
      break;
   case HGFS_OP_GETATTR_V2:
      attrV2 = rawAttr;
      break;
   case HGFS_OP_GETATTR:
      attrV1 = rawAttr;
      break;
   case HGFS_OP_SEARCH_READ_V3:
      attrV2 = rawAttr;
      break;
   case HGFS_OP_SEARCH_READ_V2:
      attrV2 = rawAttr;
      break;
   case HGFS_OP_SEARCH_READ:
      attrV1 = rawAttr;
      break;
   default:
      LOG(4, ("Unexpected op in reply packet: requestType = %d\n", requestType));
      return -EPROTO;
   }

   attrInfo->requestType = requestType;
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

      if (attrV2->mask & HGFS_ATTR_VALID_FILEID) {
         attrInfo->hostFileId = attrV2->hostFileId;
         attrInfo->mask |= HGFS_ATTR_VALID_FILEID;
      }
      /* Windows Host */
      if (attrV2->mask & HGFS_ATTR_VALID_NON_STATIC_FILEID) {
         attrInfo->hostFileId = attrV2->hostFileId;
         attrInfo->mask |= HGFS_ATTR_VALID_NON_STATIC_FILEID;
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
 * HgfsPrivateGetattr --
 *
 *    Internal getattr routine. Send a getattr request to the server
 *    for the indicated remote name, and if it succeeds copy the
 *    results of the getattr into the provided HgfsAttrInfo.
 *
 *    attr->fileName will be allocated on success if the file is a
 *    symlink; it's the caller's duty to free it.
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
HgfsPrivateGetattr(HgfsHandle handle,      // IN: file handle
                   const char* path,       // IN: path
                   HgfsAttrInfo *attr)     // OUT: Attr to copy into
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   HgfsOp opUsed;
   int result = 0;
   Bool allowHandleReuse = TRUE;

   ASSERT(attr);
   LOG( 4,("path = %s, handle = %u\n", path, handle));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(8, ("Out of memory while getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionGetattr;
   LOG(4, ("Before  HgfsPackGetattrRequest\n"));
   result = HgfsPackGetattrRequest(req, handle,
                       path, allowHandleReuse, opUsed, attr);

   LOG(4, ("Before Send, Path = %s result = %d \n", path, result));

   if (result != 0) {
      LOG(8, ("No attrs.\n"));
      goto out;
   }

   result = HgfsSendRequest(req);

   LOG( 4,("After Send, path = %s result = %d \n", path, result));

   if (result == 0) {
      LOG(8, ("Got reply\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      /*
       * If the getattr succeeded on the server, copy the stats
       * into the HgfsAttrInfo, otherwise return an error.
       */
      switch (result) {
      case 0:
         result = HgfsUnpackGetattrReply(req, attr);
         break;

      case -EBADF:
         /*
          * There's no reason why the server should have sent us this error
          * when we haven't used a handle. But to prevent an infinite loop in
          * the driver, let's make sure that we don't retry again.
          */
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (attr->requestType == HGFS_OP_GETATTR_V3) {
            LOG(8, ("Version 3 not supported. Falling back to version 2.\n"));
            hgfsVersionGetattr = HGFS_OP_GETATTR_V2;
            goto retry;
         } else if (attr->requestType == HGFS_OP_GETATTR_V2) {
            LOG(8, ("Version 2 not supported. Falling back to version 1.\n"));
            hgfsVersionGetattr = HGFS_OP_GETATTR;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(8, ("Timed out. error: %d\n", result));
   } else if (result == -EPROTO) {
      LOG(8, ("Server returned error: %d\n", result));
   } else {
      LOG(8, ("Unknown error: %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
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

   LOG(6, ("entered\n"));

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
      LOG(4, ("invalid open flags %o\n", flags));
      result = -1;
      break;
   }

   return result;
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

   default:
      LOG(10, ("Unknown error: %u\n", hgfsStatus));
      return -EIO;
   }
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

unsigned long
HgfsCalcBlockSize(uint64 tsize)
{
   loff_t used = (tsize + 511) >> 9;
   return (used > ULONG_MAX) ? ULONG_MAX : used;
}


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

int
HgfsClearReadOnly(const char* path,           // IN: file/dir to remove read only
                  HgfsAttrInfo *enableWrite)  // OUT: attributes
{
   int result;

   LOG(4, ("Entry(path = %s)\n", path));

   result = HgfsGetAttrCache(path, enableWrite);
   LOG(4, ("Retrieve attr from cache. result = %d \n", result));
   if (result != 0) {
      result = HgfsPrivateGetattr(HGFS_INVALID_HANDLE,
                                  path,
                                  enableWrite);
   }

   if (result != 0) {
      LOG(4, ("error: attributes for read-only file\n"));
      goto out;
   }

   LOG(4, ("%s perms %#o %#o %#o\n", path, enableWrite->ownerPerms,
           enableWrite->groupPerms, enableWrite->otherPerms));

   /*
    * Use only the permissions bits and add write for the owner.
    */
   enableWrite->mask &= (HGFS_ATTR_VALID_SPECIAL_PERMS |
                         HGFS_ATTR_VALID_OWNER_PERMS |
                         HGFS_ATTR_VALID_GROUP_PERMS |
                         HGFS_ATTR_VALID_OTHER_PERMS);
   enableWrite->ownerPerms |= HGFS_PERM_WRITE;

   result = HgfsSetattr(path, enableWrite);

out:
   LOG(4, ("Exit(%d)\n", result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRestoreReadOnly --
 *
 *    Try to restore the file/dir read only attribute.
 *
 *    Note This is the store for the above clear operation and expects
 *    the attributes to have valid owner permissons at a minimum.
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
HgfsRestoreReadOnly(const char* path,           // IN: file/dir to remove read only
                    HgfsAttrInfo *enableWrite)  // IN: attributes
{
   int result;

   LOG(4, ("Entry(path = %s)\n", path));

   /*
    * Clear the write permissions bit for the owner.
    */
   ASSERT((enableWrite->mask & HGFS_ATTR_VALID_OWNER_PERMS) != 0);

   enableWrite->ownerPerms &= ~HGFS_PERM_WRITE;
   result = HgfsSetattr(path, enableWrite);
   LOG(4, ("Exit(%d)\n", result));
   return result;
}
