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
 * dir.c --
 *
 * File operations for the hgfs driver.
 */
#include "module.h"


#define HGFS_CREATE_DIR_MASK (HGFS_CREATE_DIR_VALID_FILE_NAME | \
                              HGFS_CREATE_DIR_VALID_SPECIAL_PERMS | \
                              HGFS_CREATE_DIR_VALID_OWNER_PERMS | \
                              HGFS_CREATE_DIR_VALID_GROUP_PERMS | \
                              HGFS_CREATE_DIR_VALID_OTHER_PERMS)




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
HgfsPackDirOpenRequest(const char *path,    // IN: Path of the dir to open
                       HgfsOp opUsed,       // IN: Op to be used
                       HgfsReq *req)        // IN/OUT: Packet to write into
{
   size_t reqSize;

   ASSERT(path);
   ASSERT(req);
   LOG(4, ("Path = %s \n", path));
   switch (opUsed) {
   case HGFS_OP_SEARCH_OPEN_V3: {
      int result;
      HgfsRequestSearchOpenV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->dirName.flags = 0;
      requestV3->dirName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->dirName.fid = HGFS_INVALID_HANDLE;
      requestV3->reserved = 0;
      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (reqSize - 1),
                                requestV3->dirName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed\n"));
         return -EINVAL;
      }
      LOG(4, ("After conversion = %s\n", requestV3->dirName.name));
      requestV3->dirName.length = result;
      reqSize += result;
      break;
   }

   case HGFS_OP_SEARCH_OPEN: {
      int result;
      HgfsRequestSearchOpen *request;

      request = (HgfsRequestSearchOpen *)(HGFS_REQ_PAYLOAD(req));

      reqSize = sizeof *request;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (reqSize - 1),
                                request->dirName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed\n"));
         return -EINVAL;
      }
      LOG(4, ("After conversion = %s\n", request->dirName.name));
      request->dirName.length = result;
      reqSize += result;
      break;
   }

   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   req->payloadSize = reqSize;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

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
 *    We send a "Search Open" request to the server. If the Open
 *    succeeds, we store the search handle sent by the server in
 *     the handle parameter so it can be reused later.
 *
 * Results:
 *    Returns zero on success, error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsDirOpen(const char* path,       // IN: Path of dir to open
            HgfsHandle* handle)     // OUT: Handle to the dir
{
   HgfsReq *req;
   int result;
   HgfsOp opUsed;
   HgfsStatus replyStatus;

   ASSERT(path);
   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionSearchOpen;

   result = HgfsPackDirOpenRequest(path, opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply and check return status. */
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         if (opUsed == HGFS_OP_SEARCH_OPEN_V3) {
            HgfsReplySearchOpenV3 *requestV3 = HgfsGetReplyPayload(req);
            *handle = requestV3->search;
         } else {
            HgfsReplySearchOpen *request = (HgfsReplySearchOpen *)HGFS_REQ_PAYLOAD(req);
            *handle = request->search;
         }
         LOG(6, ("Set handle to %u\n", *handle));
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_SEARCH_OPEN_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionSearchOpen = HGFS_OP_SEARCH_OPEN;
            goto retry;
         }
         LOG(4, ("Server returned error: %d, opUsed = %d\n", result, opUsed));
         break;
      default:
         LOG(4, ("Server returned error: %d\n", result));
         break;
      }
   } else if (result == -EIO) {
      LOG(4, ("Timed out. error: %d\n", result));
   } else if (result == -EPROTO) {
      LOG(4, ("Server returned error: %d\n", result));
   } else {
      LOG(4, ("Unknown error: %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReadDirFromReply --
 *
 *    This function reads directory entries from the reply packet
 *    contained in the specified request structure. It calls filldir
 *    to copy each entry into the vfsDirent buffer.
 *
 *    For V1 and V2 search read reply, only one entry is returned from
 *    server, while for V3 we may have multiple directory entries. The
 *    number of entries can be read from the reply packet.
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
HgfsReadDirFromReply(uint32 *f_pos,     // IN/OUT: Offset
                     void *vfsDirent,   // OUT: Buffer to copy dentries into
                     fuse_fill_dir_t filldir, // IN:  Filler function
                     HgfsReq *req,      // IN:  The request containing reply
                     HgfsOp opUsed,     // IN:  request type
                     Bool *done)        // OUT: Set true when there are no
                                        //      more entries
{
   uint32 replyCount;
   HgfsAttrInfo attr;
   HgfsDirEntry *hgfsDirent = NULL; /* Only for V3. */
   char *escName = NULL;            /* Buffer for escaped version of name */
   size_t escNameLength = NAME_MAX + 1;
   int result = 0;

   ASSERT(req);

   escName = malloc(escNameLength);
   if (!escName) {
      LOG(4, ("Out of memory allocating escaped name buffer.\n"));
      return  -ENOMEM;
   }

   replyCount = 1;
   if (opUsed == HGFS_OP_SEARCH_READ_V3) {
      HgfsReplySearchReadV3 *replyV3 = HgfsGetReplyPayload(req);

      replyCount = replyV3->count;
      hgfsDirent = (HgfsDirEntry *)replyV3->payload;
      if (replyCount == 0) {
         /* We're at the end of the directory. */
         *done = TRUE;
         goto out;
      }
   }

   LOG(8, ("Reply counter %u, opUsed %d\n", replyCount, opUsed));
   while (replyCount-- > 0) {
      void *rawAttr;
      char *fileName;
      uint32 fileNameLength;
      ino_t ino;
      uint32 d_type;
      struct stat st;

      switch(opUsed) {
      case HGFS_OP_SEARCH_READ_V3: {
         rawAttr =  &hgfsDirent->attr;
         fileName = hgfsDirent->fileName.name;
         fileNameLength = hgfsDirent->fileName.length;
         break;
      }
      case HGFS_OP_SEARCH_READ_V2: {
         HgfsReplySearchReadV2 *replyV2;
         replyV2 = (HgfsReplySearchReadV2 *)(HGFS_REQ_PAYLOAD(req));
         rawAttr = &replyV2->attr;
         fileName = replyV2->fileName.name;
         fileNameLength = replyV2->fileName.length;
         break;
      }
      case HGFS_OP_SEARCH_READ: {
         HgfsReplySearchRead *replyV1;
         replyV1 = (HgfsReplySearchRead *)(HGFS_REQ_PAYLOAD(req));
         rawAttr = &replyV1->attr;
         fileName = replyV1->fileName.name;
         fileNameLength = replyV1->fileName.length;
         break;
      }
      default:
         LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
         result = -EPROTO;
         goto out;
      }

      /* Make sure name length is legal. */
      if (fileNameLength > NAME_MAX) {
         /*
          * Skip dentry if its name is too long. We don't try to read next
          * entry in this reply. It is ok since it happens rarely.
          */
         (*f_pos)++;
         result = -ENAMETOOLONG;
         goto out;
      } else if (fileNameLength == 0) {
         /* We're at the end of the directory. */
         *done = TRUE;
         goto out;
      }
      result = HgfsUnpackCommonAttr(rawAttr, opUsed, &attr);
      if (result != 0) {
         goto out;
      }

      /*
       * Escape all non-printable characters (which for linux is just
       * "/").
       *
       * Note that normally we would first need to convert from the
       * CP name format, but that is done implicitely here since we
       * are guaranteed to have just one path component per dentry.
       */
      result = HgfsEscape_Do(fileName,
                             fileNameLength,
			     escNameLength,
			     escName);

      /*
       * Check the filename length.
       *
       * If the name is too long to be represented in linux, we simply
       * skip it (i.e., that file is not visible to our filesystem) by
       * incrementing file->f_pos and repeating the loop to get the
       * next dentry.
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
         LOG(4, ("HgfsEscape_Do() returns %d\n", result));
         (*f_pos)++;
         continue;
      }

      /* Reuse fileNameLength to store the filename length after escape. */
      fileNameLength = result;

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

      ino = attr.hostFileId;
      memset(&st, 0, sizeof(st));
      st.st_blksize = HGFS_BLOCKSIZE;
      st.st_blocks = HgfsCalcBlockSize(attr.size);
      st.st_size = attr.size;
      st.st_ino = ino;
      st.st_mode = d_type << 12;
      result = filldir(vfsDirent, escName, &st, 0);

      if (result) {
         /*
          * This means that filldir ran out of room in the user buffer
          * it was copying into; we just break out and return, but
          * don't increment f_pos. So the next time the user calls
          * getdents, this dentry will be requested again, will get
          * retrieved again, and get copied properly to the user.
          *
          * The filldir errors are normal when the user buffer is small,
          * so we return ENOSPC and let the caller treat it specially.
          */
         LOG(4, ("filldir() returns %d\n", result));
         result = -ENOSPC;
         break;
      }
      (*f_pos)++;

      /* For V3, there may be remaining entries to process. */
      if (opUsed == HGFS_OP_SEARCH_READ_V3) {
         if (hgfsDirent->nextEntry > 0) {
            ASSERT(replyCount > 0);
         }
         hgfsDirent = (HgfsDirEntry *)((unsigned long)hgfsDirent +
                                       hgfsDirent->nextEntry);
      }
   }

out:
   free(escName);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRequestDirEntries --
 *
 *    Get the directory entries with the given offset from the server.
 *    The server may return 0, 1, or more than 2 entries depending on
 *    the protocol version.
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
HgfsRequestDirEntries(HgfsHandle searchHandle, // IN: Handle to dir
                      uint32 offset,           // IN: Offset of next dentry to get
                      HgfsReq *req,            // IN/OUT: the request
                      HgfsOp *opUsed)          // OUT: request type
{
   HgfsStatus replyStatus;
   int result = 0;

  retry:
   *opUsed = hgfsVersionSearchRead;
   if (*opUsed == HGFS_OP_SEARCH_READ_V3) {
      HgfsRequestSearchReadV3 *request = HgfsGetRequestPayload(req);

      request->search = searchHandle;
      request->offset = offset;
      request->reserved = 0;
      request->flags = 0 /* HGFS_SEARCH_READ_FLAG_MULTIPLE_REPLY */;
      req->payloadSize = sizeof(*request) + HgfsGetRequestHeaderSize();

   } else {
      HgfsRequestSearchRead *request;

      request = (HgfsRequestSearchRead *)(HGFS_REQ_PAYLOAD(req));
      request->search = searchHandle;
      request->offset = offset;
      req->payloadSize = sizeof *request;
   }

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, *opUsed);

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      /* Retry with older version(s). Set globally. */
      if (result == -EPROTO) {
         if (*opUsed == HGFS_OP_SEARCH_READ_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 2.\n"));
            hgfsVersionSearchRead = HGFS_OP_SEARCH_READ_V2;
            goto retry;
         } else if (*opUsed == HGFS_OP_SEARCH_READ_V2) {
            LOG(4, ("Version 2 not supported. Falling back to version 1.\n"));
            hgfsVersionSearchRead = HGFS_OP_SEARCH_READ;
            goto retry;
         }
      }
   } else if (result == -EIO) {
      LOG(4, ("Timed out. error: %d\n", result));
   } else if (result == -EPROTO) {
      LOG(4, ("Server returned error: %d\n", result));
   } else {
      LOG(4, ("Unknown error: %d\n", result));
   }

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

int
HgfsReaddir(HgfsHandle handle,        // IN:  Directory handle to read from
            void *dirent,             // OUT: Buffer to copy dentries into
            fuse_fill_dir_t filldir)  // IN:  Filler function
{
   Bool done = FALSE;
   HgfsReq *request;
   int result = 0;
   uint32 f_pos = 0;

   ASSERT(dirent);

   request = HgfsGetNewRequest();
   if (!request) {
      LOG(4, ("Out of memory while getting new request\n"));
      return -ENOMEM;
   }
   while (!done) {
      HgfsOp opUsed;
      /* Nonzero result = we failed to get valid reply from server. */
      result = HgfsRequestDirEntries(handle,
                                     f_pos,
                                     request,
                                     &opUsed);
      if (result) {
         LOG(4, ("Error getting dentries from server\n"));
         break;
      }

      result = HgfsReadDirFromReply(&f_pos, dirent, filldir, request, opUsed,
                                    &done);

      LOG(4, ("f_pos = %d\n", f_pos));
      if (result == -ENAMETOOLONG) {
         continue;
      } else if (result == -ENOSPC) {
         result = 0;
         break;
      } else if (result < 0) {
         LOG(4, ("Error reading dentries from reply packet. Return %d\n", result));
         break;
      }
   }

   if (done == TRUE) {
      LOG(6, ("End of dir reached.\n"));
   }
   HgfsFreeRequest(request);
   return result;
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
HgfsPackCreateDirRequest(const char *path,
                         int permsMode,         // IN: Mode to assign dir
                         HgfsOp opUsed,         // IN: Op to be used.
                         HgfsReq *req)          // IN/OUT: Packet to write into
{
   size_t reqSize;


   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_CREATE_DIR_V3: {
      int result;
      HgfsRequestCreateDirV3 *requestV3 = HgfsGetRequestPayload(req);

      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      requestV3->fileName.flags = 0;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (reqSize - 1),
                                requestV3->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV3->fileName.length = result;
      reqSize += result;
      requestV3->mask = HGFS_CREATE_DIR_MASK;

      /* Set permissions. */
      requestV3->specialPerms = (permsMode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
      requestV3->ownerPerms = (permsMode & S_IRWXU) >> 6;
      requestV3->groupPerms = (permsMode & S_IRWXG) >> 3;
      requestV3->otherPerms = (permsMode & S_IRWXO);
      requestV3->fileAttr = 0;
      break;
   }
   case HGFS_OP_CREATE_DIR_V2: {
      int result;
      HgfsRequestCreateDirV2 *requestV2;

      requestV2 = (HgfsRequestCreateDirV2 *)(HGFS_REQ_PAYLOAD(req));

      reqSize = sizeof *requestV2;

      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (reqSize - 1),
                                requestV2->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV2->fileName.length = result;
      reqSize += result;
      requestV2->mask = HGFS_CREATE_DIR_MASK;

      /* Set permissions. */
      requestV2->specialPerms = (permsMode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
      requestV2->ownerPerms = (permsMode & S_IRWXU) >> 6;
      requestV2->groupPerms = (permsMode & S_IRWXG) >> 3;
      requestV2->otherPerms = (permsMode & S_IRWXO);
      break;
   }
   case HGFS_OP_CREATE_DIR: {
      int result;
      HgfsRequestCreateDir *request;

      request = (HgfsRequestCreateDir *)(HGFS_REQ_PAYLOAD(req));

      reqSize = sizeof *request;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (reqSize - 1),
                                request->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      request->fileName.length = result;
      reqSize += result;
      /* Set permissions. */
      request->permissions = (permsMode & S_IRWXU) >> 6;
      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   req->payloadSize = reqSize;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   return 0;
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

int
HgfsMkdir(const char *path,     // IN: Path to directory
          int permsMode)        // IN: Mode to set
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   HgfsOp opUsed;
   int result = 0;

   ASSERT(path);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionCreateDir;
   result = HgfsPackCreateDirRequest(path, permsMode, opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   /*
    * Send the request and process the reply. Since HgfsReplyCreateDirV2 and
    * HgfsReplyCreateDir are identical, we need no special logic here.
    */
   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply.\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         LOG(6, ("Directory created successfully, instantiating dentry.\n"));
         /*
          * XXX: When we support hard links, this is a good place to
          * increment link count of parent dir.
          */
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CREATE_DIR_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 2.\n"));
            hgfsVersionCreateDir = HGFS_OP_CREATE_DIR_V2;
            goto retry;
         } else if (opUsed == HGFS_OP_CREATE_DIR_V2) {
            LOG(4, ("Version 2 not supported. Falling back to version 1.\n"));
            hgfsVersionCreateDir = HGFS_OP_CREATE_DIR;
            goto retry;
         }

         /* Fallthrough. */
         default:
            LOG(6, ("Directory was not created, error %d\n", result));
            break;
         }
   } else if (result == -EIO) {
      LOG(4, ("Timed out. error: %d\n", result));
   } else if (result == -EPROTO) {
      LOG(4, ("Server returned error: %d\n", result));
   } else {
      LOG(4, ("Unknown error: %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
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

int
HgfsDelete(const char* path,       // IN: Path to file
           HgfsOp op)              // IN: Opcode for file type (file or dir)*/
{
   HgfsReq *req = NULL;
   int result = 0;
   HgfsStatus replyStatus;
   uint32 reqSize;
   HgfsOp opUsed;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *clearReadOnlyAttr = &newAttr;
   Bool clearedReadOnly = FALSE;

   if ((op != HGFS_OP_DELETE_FILE) &&
       (op != HGFS_OP_DELETE_DIR)) {
      LOG(4, ("Invalid opcode. op = %d\n", op));
      result = -EINVAL;
      goto out;
   }

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
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
      HgfsRequestDeleteV3 *request = HgfsGetRequestPayload(req);

      reqSize = sizeof(*request) + HgfsGetRequestHeaderSize();
      request->hints = 0;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize),
                                request->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         result = -EINVAL;
         goto out;
      }
      request->fileName.length = result;
      reqSize += result;
      request->fileName.fid = HGFS_INVALID_HANDLE;
      request->fileName.flags = 0;
      request->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
      request->reserved = 0;

   } else {
      HgfsRequestDelete *request;

      request = (HgfsRequestDelete *)(HGFS_REQ_PAYLOAD(req));
      /* Fill out the request packet. */
      reqSize = sizeof *request;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize),
                                request->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         result = -EINVAL;
         goto out;
      }
      request->fileName.length = result;
      reqSize += result;
   }

   req->payloadSize = reqSize;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   result = HgfsSendRequest(req);
   switch (result) {
   case 0:
      LOG(6, ("Got reply\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {

      case -EACCES:
      case -EPERM:
         /*
          * It's possible that we're talking to a Windows server with
          * a file marked read-only. Let's try again, after removing
          * the read-only bit from the file.
          *
          * Note, currently existing Windows HGFS servers that are running
          * shares against NTFS volumes do NOT handle the ACLs when setting
          * a file as writable. Only the attribute for read only is cleared.
          * This maybe okay but could be inadequate if the ACLs are read only
          * for the current user. For those cases, the second attempt will
          * still fail. The server code should be fixed to address this failing
          * the set attributes for read only if it cannot do both.
          *
          * XXX: I think old servers will send -EPERM here. Is this entirely
          * safe?
          */
         if (!clearedReadOnly) {
            result = HgfsClearReadOnly(path, clearReadOnlyAttr);
            if (result == 0) {
               clearedReadOnly = TRUE;
               LOG(4, ("removed read-only, retrying delete\n"));
               goto retry;
            }
            LOG(4, ("failed to remove read-only attribute\n"));
         } else {
            (void)HgfsRestoreReadOnly(path, clearReadOnlyAttr);
            LOG(4, ("second attempt failed\n"));
         }
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_DELETE_DIR_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionDeleteDir = HGFS_OP_DELETE_DIR;
            goto retry;
         } else if (opUsed == HGFS_OP_DELETE_FILE_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionDeleteFile = HGFS_OP_DELETE_FILE;
            goto retry;
         }

         LOG(4, ("Server returned error: %d\n", result));
         break;
      default:
         break;
      }
      break;
   default:
      LOG(4, ("Send returned error: %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   return result;
}
