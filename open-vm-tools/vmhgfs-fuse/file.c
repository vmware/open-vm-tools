/*********************************************************
 * Copyright (C) 2013,2018-2019 VMware, Inc. All rights reserved.
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
 * file.c --
 *
 * File operations for the hgfs driver.
 */

#include "cpName.h"
#include "hgfsProto.h"
#include "module.h"
#include "request.h"
#include "hgfsUtil.h"
#include "fsutil.h"
#include "file.h"
#include "vm_assert.h"
#include "vm_basic_types.h"



static int
HgfsGetOpenFlags(uint32 flags);


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
HgfsPackOpenRequest(const char *path,          // IN: Path to file
                    struct fuse_file_info *fi, // IN: File info structure
                    mode_t permsMode,          // IN: Permissions, in this context
                    HgfsOpenValid mask,        // IN: Open validation mask
                    HgfsOp opUsed,             // IN: Op to use
                    HgfsReq *req)              // IN/OUT: Packet to write into
{
   size_t reqSize;
   int openMode, openFlags;

   ASSERT(path);
   ASSERT(req);

   openMode = HgfsGetOpenMode(fi->flags);
   if (openMode < 0) {
      LOG(4, ("Failed to get open mode.\n"));
      return -EINVAL;
   }
   openFlags = HgfsGetOpenFlags(fi->flags);
   if (openFlags < 0) {
      LOG(4, ("Failed to get open flags.\n"));
      return -EINVAL;
   }

   switch (opUsed) {
    case HGFS_OP_OPEN_V3: {
      int result;
      HgfsRequestOpenV3 *requestV3 = HgfsGetRequestPayload(req);

      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();

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
      /* Linux clients need case-sensitive lookups. */
      requestV3->fileName.flags = 0;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;

      requestV3->mask = mask;
      requestV3->mode = openMode;
      requestV3->flags = openFlags;

      /* Set permissions. */
      if (requestV3->mask & HGFS_FILE_OPEN_PERMS) {
         requestV3->specialPerms = (permsMode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
         requestV3->ownerPerms = (permsMode & S_IRWXU) >> 6;
         requestV3->groupPerms = (permsMode & S_IRWXG) >> 3;
         requestV3->otherPerms = (permsMode & S_IRWXO);
      }

      /* XXX: Request no lock for now. */
      requestV3->desiredLock = HGFS_LOCK_NONE;

      requestV3->reserved1 = 0;
      requestV3->reserved2 = 0;
      break;
   }

   case HGFS_OP_OPEN_V2: {
      int result;
      HgfsRequestOpenV2 *requestV2;

      requestV2 = (HgfsRequestOpenV2 *)(HGFS_REQ_PAYLOAD(req));

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
      requestV2->mask = mask;
      requestV2->mode = openMode;
      requestV2->flags = openFlags;

      /* Set permissions, requires discussion... default, will set max permission*/
      if (requestV2->mask & HGFS_FILE_OPEN_PERMS) {
         requestV2->specialPerms = (permsMode & (S_ISUID | S_ISGID | S_ISVTX)) >> 9;
         requestV2->ownerPerms = (permsMode & S_IRWXU) >> 6;
         requestV2->groupPerms = (permsMode & S_IRWXG) >> 3;
         requestV2->otherPerms = (permsMode & S_IRWXO);
      }

      /* XXX: Request no lock for now. */
      requestV2->desiredLock = HGFS_LOCK_NONE;
      break;
   }
   case HGFS_OP_OPEN: {
      int result;
      HgfsRequestOpen *request;

      request = (HgfsRequestOpen *)(HGFS_REQ_PAYLOAD(req));
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
      request->mode = openMode;
      request->flags = openFlags;

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
      replyV3 = HgfsGetReplyPayload(req);
      replySize = sizeof(*replyV3) + HgfsGetReplyHeaderSize();
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
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      ASSERT(FALSE);
      return -EPROTO;
   }

   if (opUsed < HGFS_OP_OPEN_V3 && req->payloadSize != replySize) {
      /*
       * The reply to Open is a fixed size. So the size of the payload
       * really ought to match the expected size of an HgfsReplyOpen[V2].
       */
      LOG(4, ("Wrong packet size.\n"));
      return -EPROTO;
   }

   return 0;
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

static int
HgfsGetOpenFlags(uint32 flags) // IN: Open flags
{
   uint32 mask = O_CREAT | O_TRUNC | O_EXCL;
   int result = -1;

   LOG(6, ("Entered\n"));

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
      LOG(4, ("Invalid open flags %o. Ignoring the O_TRUNC flag.\n", flags));
      result = HGFS_OPEN_CREATE_SAFE;
      break;
   }

   return result;
}


/*
 * HGFS file operations for files.
 */

/*
 *----------------------------------------------------------------------
 *
 * HgfsOpenInt --
 *
 *    We send an "Open" request to the server with the file path
 *    If the Open succeeds, we store the filehandle sent by the server
 *    in the file info struct so it can be accessed by
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

int
HgfsOpenInt(const char *path,           //IN: Path to a file
            struct fuse_file_info *fi,  //OUT: File info structure
            mode_t permsMode,           //IN: Permissions, in this context
            HgfsOpenValid mask)         //IN: Open validation mask
{
   HgfsReq *req;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   HgfsHandle replyFile;
   HgfsLockType replyLock;
   int result = 0;

   ASSERT(NULL != path);
   ASSERT(NULL != fi);

   LOG(4, ("Entry(%s)\n", path));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

   fi->fh = HGFS_INVALID_HANDLE;

retry:
   /*
    * Set up pointers using the proper struct This lets us check the
    * version exactly once and use the pointers later.
    */

   opUsed = hgfsVersionOpen;
   result = HgfsPackOpenRequest(path, fi, permsMode, mask, opUsed, req);
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
         result = HgfsUnpackOpenReply(req, opUsed, &replyFile, &replyLock);
         if (result != 0) {
            break;
         }

         fi->fh = (uint64_t)replyFile;
         LOG( 4,("Server file handle: %"FMT64"u\n", fi->fh));

         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_OPEN_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 2.\n"));
            hgfsVersionOpen = HGFS_OP_OPEN_V2;
            goto retry;
         }

         /* Retry with Version 1 of Open. Set globally. */
         if (opUsed == HGFS_OP_OPEN_V2) {
            LOG(4, ("Version 2 not supported. Falling back to version 1.\n"));
            hgfsVersionOpen = HGFS_OP_OPEN;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(8, ("Timed out. error: %d\n", result));
   } else if (result == -EPROTO) {
      LOG(4, ("Server returned error: %d\n", result));
   } else {
      LOG(4, ("Unknown error: %d\n", result));
   }
out:
   HgfsFreeRequest(req);
   LOG(4, ("Exit(0x%"FMT64"x -> %d)\n",fi->fh, result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsOpen --
 *
 *    Called whenever a process opens a file in our filesystem.
 *
 * Results:
 *    Returns zero if on success, error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsOpen(const char *path,             //IN: Path to a file
            struct fuse_file_info *fi) //OUT: File info structure
{
     return HgfsOpenInt(path, fi, 0, HGFS_FILE_OPEN_MASK);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCreate --
 *
 *    Called whenever a process request to create a file
 *
 * Results:
 *    Returns zero if on success, error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsCreate(const char *path,          //IN: Path to a file
           mode_t permsMode,          //IN: Permission to open the file
           struct fuse_file_info *fi) //OUT: File info structure
{
     return HgfsOpenInt(path, fi, permsMode, HGFS_FILE_CREATE_MASK);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoRead --
 *
 *    Do one read request. Called by HgfsRead, possibly multiple times
 *    if the size of the read is too big to be handled by one server request.
 *
 *    We send a "Read" request to the server with the given handle.
 *
 *
 * Results:
 *    Returns the number of bytes read on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoRead(HgfsHandle handle,  // IN:  Handle for this file
           char *buf,          // OUT: Buffer to copy data into
           size_t count,       // IN:  Number of bytes to read
           loff_t offset)      // IN:  Offset at which to read
{
   HgfsReq *req;
   HgfsOp opUsed;
   int result = 0;
   uint32 actualSize = 0;
   char *payload = NULL;
   HgfsStatus replyStatus;

   ASSERT(NULL != buf);

   LOG(4, ("Entry(handle = %u, 0x%"FMTSZ"x @ 0x%"FMT64"x)\n", handle, count, offset));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

 retry:
   opUsed = hgfsVersionRead;
   if (opUsed == HGFS_OP_READ_V3) {
      HgfsRequestReadV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->file = handle;
      requestV3->offset = offset;
      requestV3->requiredSize = count;
      requestV3->reserved = 0;

      req->payloadSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();

   } else {
      HgfsRequestRead *request;

      request = (HgfsRequestRead *)(HGFS_REQ_PAYLOAD(req));
      request->file = handle;
      request->offset = offset;
      request->requiredSize = count;
      req->payloadSize = sizeof *request;
   }

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         if (opUsed == HGFS_OP_READ_V3) {
            HgfsReplyReadV3 * replyV3 = HgfsGetReplyPayload(req);

            actualSize = replyV3->actualSize;
            payload = replyV3->payload;

         } else {
            actualSize = ((HgfsReplyRead *)HGFS_REQ_PAYLOAD(req))->actualSize;
            payload = ((HgfsReplyRead *)HGFS_REQ_PAYLOAD(req))->payload;
         }

         /* Sanity check on read size. */
         if (actualSize > count) {
            LOG(4, ("Server reply: read too big!\n"));
            result = -EPROTO;
            goto out;
         }

         if (0 == actualSize) {
            /* We got no bytes, so don't need to copy to user. */
            LOG(8, ("Server reply returned zero\n"));
            result = actualSize;
            goto out;
         }

         /* Return result. */
         memcpy(buf, payload, actualSize);
         LOG(8, ("Copied %u\n", actualSize));
         result = actualSize;
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_READ_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionRead = HGFS_OP_READ;
            goto retry;
         }
         break;

      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(8, ("Error: send request timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, ("Error: send request server returned error: %d\n", result));
   } else {
      LOG(4, ("Error: send request unknown : %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   LOG(4, ("Exit(%d)\n", result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRead --
 *
 *    Called whenever a process reads from a file in our filesystem.
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

ssize_t
HgfsRead(struct fuse_file_info *fi,  // IN:  File info struct
         char  *buf,                 // OUT: User buffer to copy data into
         size_t count,               // IN:  Number of bytes to read
         loff_t offset)              // IN:  Offset at which to read
{
   int result = 0;
   char *buffer = buf;
   loff_t curOffset = offset;
   size_t nextCount, remainingCount = count;

   ASSERT(NULL != fi);
   ASSERT(NULL != buf);

   LOG(4, ("Entry(0x%"FMT64"x 0x%"FMTSZ"x bytes @ 0x%"FMT64"x)\n",
           fi->fh, count, offset));

    do {
      nextCount = (remainingCount > HGFS_LARGE_IO_MAX) ?
                                     HGFS_LARGE_IO_MAX : remainingCount;
      LOG(4, ("Issue DoRead(0x%"FMT64"x 0x%"FMTSZ"x bytes @ 0x%"FMT64"x)\n",
              fi->fh, nextCount, curOffset));
      result = HgfsDoRead(fi->fh, buffer, nextCount, curOffset);
      if (result < 0) {
         LOG(8, ("Error: DoRead: -> %d\n", result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      buffer += result;

   } while ((result > 0) && (remainingCount > 0));

  memset(buffer, 0, remainingCount);

  out:
   LOG(4, ("Exit(%"FMTSZ"d)\n", count - remainingCount));
   return (count - remainingCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoWrite --
 *
 *    Do one write request. Called by HgfsWrite, possibly multiple
 *    times if the size of the write is too big to be handled by one server
 *    request.
 *
 *    We send a "Write" request to the server with the given handle.
 *
 * Results:
 *    Returns the number of bytes written on success, or an error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsDoWrite(HgfsHandle handle,       // IN: Handle for the file
            const char *buf,         // IN: Buffer containing data
            size_t count,            // IN: Number of bytes to write
            loff_t offset)           // IN: Offset to begin writing at
{
   HgfsReq *req;
   int result = 0;
   HgfsOp opUsed;
   uint32 requiredSize = 0;
   uint32 actualSize = 0;
   char *payload = NULL;
   uint32 reqSize;
   HgfsStatus replyStatus;

   ASSERT(buf);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request\n"));
      result = -ENOMEM;
      goto out;
   }
   LOG( 4,("handle = %u \n", handle));
 retry:
   opUsed = hgfsVersionWrite;
   if (opUsed == HGFS_OP_WRITE_V3) {
      HgfsRequestWriteV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->file = handle;
      requestV3->flags = 0;
      requestV3->offset = offset;
      requestV3->requiredSize = count;
      requestV3->reserved = 0;
      payload = requestV3->payload;
      requiredSize = requestV3->requiredSize;
      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();

   } else {
      HgfsRequestWrite *request;

      request = (HgfsRequestWrite *)(HGFS_REQ_PAYLOAD(req));
      request->file = handle;
      request->flags = 0;
      request->offset = offset;
      request->requiredSize = count;
      payload = request->payload;
      requiredSize = request->requiredSize;
      reqSize = sizeof *request;
   }

   memcpy(payload, buf, requiredSize);
   req->payloadSize = reqSize + requiredSize - 1;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         if (opUsed == HGFS_OP_WRITE_V3) {
            HgfsReplyWriteV3 * replyV3 = HgfsGetReplyPayload(req);

            actualSize = replyV3->actualSize;

         } else {
            actualSize = ((HgfsReplyWrite *)HGFS_REQ_PAYLOAD(req))->actualSize;
         }

         /* Return result. */
         LOG(6, ("wrote %u bytes\n", actualSize));
         result = actualSize;
         break;

      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_WRITE_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionWrite = HGFS_OP_WRITE;
            goto retry;
         }
         break;

      default:
         LOG(4, ("Server returned error: %d\n", result));
         break;
      }
   } else if (result == -EIO) {
      LOG(8, ("Timed out. error: %d\n", result));
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
 * HgfsWrite --
 *
 *    Called whenever a process writes to a file in our filesystem.
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

ssize_t
HgfsWrite(struct fuse_file_info *fi,  // IN: File info structure
         const char  *buf,            // OUT: User buffer to copy data into
         size_t count,                // IN:  Number of bytes to read
         loff_t offset)               // IN:  Offset at which to read
{
   int result;
   const char *buffer = buf;
   loff_t curOffset = offset;
   size_t nextCount, remainingCount = count;
   ssize_t bytesWritten = 0;

   ASSERT(NULL != buf);
   ASSERT(NULL != fi);

   LOG(6, ("Entry(0x%"FMT64"x off bytes 0x%"FMTSZ"x @ 0x%"FMT64"x)\n",
           fi->fh, count, offset));

   do {
      nextCount = (remainingCount > HGFS_LARGE_IO_MAX) ?
                                     HGFS_LARGE_IO_MAX : remainingCount;

      LOG(4, ("Issue DoWrite(0x%"FMT64"x 0x%"FMTSZ"x bytes @ 0x%"FMT64"x)\n",
              fi->fh, nextCount, curOffset));

      result = HgfsDoWrite(fi->fh, buffer, nextCount, curOffset);
      if (result < 0) {
         bytesWritten = result;
         LOG(4, ("Error: written 0x%"FMTSZ"x bytes DoWrite -> %d\n",
             count - remainingCount, result));
         goto out;
      }
      remainingCount -= result;
      curOffset += result;
      buffer += result;

   } while ((result > 0) && (remainingCount > 0));

   bytesWritten = count - remainingCount;

out:
   LOG(6, ("Exit(0x%"FMTSZ"x)\n", bytesWritten));
   return bytesWritten;
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

int
HgfsRename(const char* from, const char* to)
{
   HgfsReq *req = NULL;
   int result = 0;
   uint32 reqSize;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   HgfsAttrInfo newAttr = {0};
   HgfsAttrInfo *clearReadOnlyAttr = &newAttr;
   Bool clearedReadOnly = FALSE;

   ASSERT(from);
   ASSERT(to);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionRename;
   if (opUsed == HGFS_OP_RENAME_V3) {
      HgfsRequestRenameV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->hints = 0;
      requestV3->oldName.flags = 0;
      requestV3->oldName.fid = HGFS_INVALID_HANDLE;
      requestV3->oldName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->reserved = 0;
      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      /* Convert old name to CP format. */
      result = CPName_ConvertTo(from,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize),
                                requestV3->oldName.name);
      if (result < 0) {
         LOG(4, ("oldName CP conversion failed\n"));
         result = -EINVAL;
         goto out;
      }

      requestV3->oldName.length = result;
      reqSize += result;
   } else {
      HgfsRequestRename *request = (HgfsRequestRename *)HGFS_REQ_PAYLOAD(req);

      reqSize = sizeof *request;
      /* Convert old name to CP format. */
      result = CPName_ConvertTo(from,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize),
                                request->oldName.name);
      if (result < 0) {
         LOG(4, ("oldName CP conversion failed\n"));
         result = -EINVAL;
         goto out;
      }

      request->oldName.length = result;
      reqSize += result;
   }

   /*
    * Build full new name to send to server.
    * Note the different buffer length. This is because HgfsRequestRename
    * contains two filenames, and once we place the first into the packet we
    * must account for it when determining the amount of buffer available for
    * the second.
    */
   if (opUsed == HGFS_OP_RENAME_V3) {
      HgfsRequestRenameV3 *requestV3 = HgfsGetRequestPayload(req);
      HgfsFileNameV3 *newNameP;

      newNameP = (HgfsFileNameV3 *)((char *)&requestV3->oldName +
                                    sizeof requestV3->oldName + result);

      LOG(6, ("New name: \"%s\"\n", newNameP->name));

      /* Convert new name to CP format. */
      result = CPName_ConvertTo(to,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize) - result,
                                newNameP->name);
      if (result < 0) {
         LOG(4, ("newName CP conversion failed\n"));
         result = -EINVAL;
         goto out;
      }
      newNameP->length = result;
      reqSize += result;
      newNameP->flags = 0;
      newNameP->fid = HGFS_INVALID_HANDLE;
      newNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   } else {
      HgfsRequestRename *request = (HgfsRequestRename *)HGFS_REQ_PAYLOAD(req);
      HgfsFileName *newNameP;
      newNameP = (HgfsFileName *)((char *)&request->oldName +
                                  sizeof request->oldName + result);

      LOG(6, ("New name: \"%s\"\n", newNameP->name));

      /* Convert new name to CP format. */
      result = CPName_ConvertTo(to,
                                HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize) - result,
                                newNameP->name);
      if (result < 0) {
         LOG(4, ("newName CP conversion failed\n"));
         result = -EINVAL;
         goto out;
      }
      newNameP->length = result;
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
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_RENAME_V3) {
            hgfsVersionRename = HGFS_OP_RENAME;
            goto retry;
         } else {
            LOG(4, ("Server returned error: %d\n", result));
         }
         break;
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
          * We can receive EACCES or EPERM if we don't have the correct
          * permission on the source file. So lets not assume that we have
          * a target and only clear the target if there is one.
          */
         if (!clearedReadOnly) {
            result = HgfsClearReadOnly(to, clearReadOnlyAttr);
            if (result == 0) {
               clearedReadOnly = TRUE;
               LOG(4, ("removed read-only, retrying delete\n"));
               goto retry;
            }
            LOG(4, ("failed to remove read-only attribute\n"));
         } else {
            (void)HgfsRestoreReadOnly(to, clearReadOnlyAttr);
            LOG(4, ("second attempt failed\n"));
         }
         break;
      default:
         LOG(4, ("Server protocol result %d\n", result));
      }
      break;
   default:
      LOG(4, ("Send returned error: %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   LOG(6, ("Exit(%d)\n", result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackSetattrRequest --
 *
 *    Setup the Setattr request, depending on the op version.
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
HgfsPackSetattrRequest(const char *path,   // IN:  path to file
                       HgfsAttrInfo *attr, // IN: attributes to set
                       HgfsOp opUsed,      // IN: Op to be used
                       HgfsReq *req)       // IN/OUT: req packet
{
   HgfsAttrV2 *attrV2;
   HgfsAttr *attrV1;
   HgfsAttrChanges *update;
   size_t reqBufferSize;
   size_t reqSize;
   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_SETATTR_V3: {
      int result;
      HgfsRequestSetattrV3 *requestV3 = HgfsGetRequestPayload(req);

      attrV2 = &requestV3->attr;

      /*
       * Clear attributes, mask, and hints before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attrV2, 0, sizeof *attrV2);
      requestV3->hints = 0;

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
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.flags = 0;
      requestV3->reserved = 0;
      reqSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_LARGE_PACKET_MAX, reqSize);
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                requestV3->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV3->fileName.length = result;
      reqSize += result;

      attrV2->mask = attr->mask;
      if (attr->mask & (HGFS_ATTR_VALID_SPECIAL_PERMS |
                          HGFS_ATTR_VALID_OWNER_PERMS |
                          HGFS_ATTR_VALID_GROUP_PERMS |
                          HGFS_ATTR_VALID_OTHER_PERMS)) {
         attrV2->specialPerms = attr->specialPerms;
         attrV2->ownerPerms = attr->ownerPerms;
         attrV2->groupPerms = attr->groupPerms;
         attrV2->otherPerms = attr->otherPerms;
      }
      if (attr->mask & HGFS_ATTR_VALID_USERID) {
         attrV2->userId = attr->userId;
      }
      if (attr->mask & HGFS_ATTR_VALID_GROUPID) {
         attrV2->groupId = attr->groupId;
      }
      if (attr->mask & HGFS_ATTR_VALID_SIZE) {
         attrV2->size = attr->size;
      }
      if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
         attrV2->accessTime = attr->accessTime;
         requestV3->hints |= HGFS_ATTR_HINT_SET_ACCESS_TIME;
      }
      if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
         attrV2->writeTime = attr->writeTime;
         requestV3->hints |= HGFS_ATTR_HINT_SET_WRITE_TIME;
      }

      break;
   }
   case HGFS_OP_SETATTR_V2: {
      int result;
      HgfsRequestSetattrV2 *requestV2;

      requestV2 = (HgfsRequestSetattrV2 *)(HGFS_REQ_PAYLOAD(req));

      attrV2 = &requestV2->attr;

      /*
       * Clear attributes, mask, and hints before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attrV2, 0, sizeof *attrV2);
      requestV2->hints = 0;

      reqSize = sizeof *requestV2;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(HGFS_LARGE_PACKET_MAX, requestV2);
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                requestV2->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV2->fileName.length = result;
      reqSize += result;

      if (attr->mask & (HGFS_ATTR_VALID_SPECIAL_PERMS |
                          HGFS_ATTR_VALID_OWNER_PERMS |
                          HGFS_ATTR_VALID_GROUP_PERMS |
                          HGFS_ATTR_VALID_OTHER_PERMS)) {
         attrV2->specialPerms = attr->specialPerms;
         attrV2->ownerPerms = attr->ownerPerms;
         attrV2->groupPerms = attr->groupPerms;
         attrV2->otherPerms = attr->otherPerms;
      }
      if (attr->mask & HGFS_ATTR_VALID_USERID) {
         attrV2->userId = attr->userId;
      }
      if (attr->mask & HGFS_ATTR_VALID_GROUPID) {
         attrV2->groupId = attr->groupId;
      }
      if (attr->mask & HGFS_ATTR_VALID_SIZE) {
         attrV2->size = attr->size;
      }
      if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
         attrV2->accessTime = attr->accessTime;
         requestV2->hints |= HGFS_ATTR_HINT_SET_ACCESS_TIME;
      }
      if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
         attrV2->writeTime = attr->writeTime;
         requestV2->hints |= HGFS_ATTR_HINT_SET_WRITE_TIME;
      }

      break;
   }
   case HGFS_OP_SETATTR: {
      int result;
      HgfsRequestSetattr *request;

      request = (HgfsRequestSetattr *)(HGFS_REQ_PAYLOAD(req));

      attrV1 = &request->attr;
      update = &request->update;

      reqSize = sizeof *request;
      reqBufferSize = HGFS_NAME_BUFFER_SIZE(HGFS_LARGE_PACKET_MAX, request);
      result = CPName_ConvertTo(path,
                                reqBufferSize,
                                request->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      request->fileName.length = result;
      reqSize += result;

      /*
       * Clear attributes before touching them.
       * We can't rely on GetNewRequest() to zero our structures, so
       * make sure to zero them all here.
       */
      memset(attrV1, 0, sizeof *attrV1);
      memset(update, 0, sizeof *update);

      if (attr->mask & (HGFS_ATTR_VALID_SPECIAL_PERMS |
                          HGFS_ATTR_VALID_OWNER_PERMS |
                          HGFS_ATTR_VALID_GROUP_PERMS |
                          HGFS_ATTR_VALID_OTHER_PERMS)) {
         *update |= HGFS_ATTR_PERMISSIONS;
         attrV1->permissions = attr->effectivePerms;
      }
      if (attr->mask & HGFS_ATTR_VALID_SIZE) {
         *update |= HGFS_ATTR_SIZE;
         attrV1->size = attr->size;
      }
      if (attr->mask & HGFS_ATTR_VALID_ACCESS_TIME) {
         *update |= HGFS_ATTR_ACCESS_TIME |
         HGFS_ATTR_ACCESS_TIME_SET;
         attrV1->accessTime = attr->accessTime;
      }
      if (attr->mask & HGFS_ATTR_VALID_WRITE_TIME) {
         *update |= HGFS_ATTR_WRITE_TIME |
         HGFS_ATTR_WRITE_TIME_SET ;
         attrV1->writeTime = attr->writeTime;
      }

      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   req->payloadSize = reqSize;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   LOG(6, ("Exit(0)\n"));
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
 *    Handle a setattr request.
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
HgfsSetattr(const char* path,       //IN: Path to file
            HgfsAttrInfo *attr)     //IN: Attribute to set
{
   HgfsReq *req;
   HgfsStatus replyStatus;
   int result = 0;
   HgfsOp opUsed;

   LOG(4, ("Entry(%s)\n", path));

   req = HgfsGetNewRequest();
   if (!req) {
      result = -ENOMEM;
      LOG(4, ("Error: out of memory -> %d\n", result));
      goto out;
   }

retry:
   /* Fill out the request packet. */
   opUsed = hgfsVersionSetattr;
   result = HgfsPackSetattrRequest(path, attr, opUsed, req);

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_SETATTR_V3) {
            LOG(4, ("Error: reply EPROTO: Version 3 -> version 2.\n"));
            hgfsVersionSetattr = HGFS_OP_SETATTR_V2;
            goto retry;
         } else if (opUsed == HGFS_OP_SETATTR_V2) {
            LOG(4, ("Error: reply EPROTO: Version 2 -> version 1.\n"));
            hgfsVersionSetattr = HGFS_OP_SETATTR;
            goto retry;
         }

         /* Fallthrough. */
      default:
         break;
      }
   } else if (result == -EIO) {
      LOG(8, ("Error: EIO: send timed out\n"));
   } else if (result == -EPROTO) {
      LOG(4, ("Error: EPROTO: send -> %d\n", result));
   } else {
      LOG(4, ("Error: unknown: send -> %d\n", result));
   }

out:
   HgfsFreeRequest(req);
   LOG(6, ("Exit(%d)\n", result));
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsRelease --
 *
 *    Called when the last user of a file closes it.
 *
 * Results:
 *    Returns zero on success, or an error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsRelease(HgfsHandle handle)  //IN:File handle to close
{
   HgfsReq *req;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   int result = 0;

   LOG(6, ("Entry(handle = %u)\n", handle));

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request\n"));
      result = -ENOMEM;
      goto out;
   }

retry:
   opUsed = hgfsVersionClose;
   if (opUsed == HGFS_OP_CLOSE_V3) {
      HgfsRequestCloseV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->file = handle;
      requestV3->reserved = 0;
      req->payloadSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();

   } else {
      HgfsRequestClose *request;

      request = (HgfsRequestClose *)(HGFS_REQ_PAYLOAD(req));
      request->file = handle;
      req->payloadSize = sizeof *request;
   }

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   /* Send the request and process the reply. */
   result = HgfsSendRequest(req);
   if (result == 0) {
      /* Get the reply. */
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      switch (result) {
      case 0:
         LOG(4, ("Released handle %u\n", handle));
         break;
      case -EPROTO:
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CLOSE_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionClose = HGFS_OP_CLOSE;
            goto retry;
         }
         break;
      default:
         LOG(4, ("Failed. handle = %u\n", handle));
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
   LOG(6, ("Exit(%d)\n", result));
   return result;
}
