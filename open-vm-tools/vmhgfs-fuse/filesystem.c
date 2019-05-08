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
 * filesystem.c --
 *
 * High-level filesystem operations for the filesystem portion of
 * the vmhgfs driver.
 */

#include "filesystem.h"
#include "transport.h"
#include "hgfsProto.h"
#include "hgfsUtil.h"
#include "module.h"
#include "request.h"
#include "fsutil.h"
#include "vm_assert.h"
#include "vm_basic_types.h"
#include "rpcout.h"
#include "hgfs.h"



/* Synchronization primitives. */
pthread_mutex_t hgfsBigLock;


/* Global protocol version switch. */
HgfsOp hgfsVersionCreateSession;
HgfsOp hgfsVersionDestroySession;
HgfsOp hgfsVersionOpen;
HgfsOp hgfsVersionRead;
HgfsOp hgfsVersionWrite;
HgfsOp hgfsVersionClose;
HgfsOp hgfsVersionSearchOpen;
HgfsOp hgfsVersionSearchRead;
HgfsOp hgfsVersionSearchClose;
HgfsOp hgfsVersionGetattr;
HgfsOp hgfsVersionSetattr;
HgfsOp hgfsVersionCreateDir;
HgfsOp hgfsVersionDeleteFile;
HgfsOp hgfsVersionDeleteDir;
HgfsOp hgfsVersionRename;
HgfsOp hgfsVersionQueryVolumeInfo;
HgfsOp hgfsVersionCreateSymlink;

HgfsFuseState HFState;
HgfsFuseState *gState = &HFState;

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsResetOps --
 *
 *      Reset ops with more than one opcode back to the desired opcode.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsResetOps(void)
{
   hgfsVersionCreateSession   = HGFS_OP_CREATE_SESSION_V4;
   hgfsVersionDestroySession  = HGFS_OP_DESTROY_SESSION_V4;
   hgfsVersionOpen            = HGFS_OP_OPEN_V3;
   hgfsVersionRead            = HGFS_OP_READ_V3;
   hgfsVersionWrite           = HGFS_OP_WRITE_V3;
   hgfsVersionClose           = HGFS_OP_CLOSE_V3;
   hgfsVersionSearchOpen      = HGFS_OP_SEARCH_OPEN_V3;
   hgfsVersionSearchRead      = HGFS_OP_SEARCH_READ_V3;
   hgfsVersionSearchClose     = HGFS_OP_SEARCH_CLOSE_V3;
   hgfsVersionGetattr         = HGFS_OP_GETATTR_V3;
   hgfsVersionSetattr         = HGFS_OP_SETATTR_V3;
   hgfsVersionCreateDir       = HGFS_OP_CREATE_DIR_V3;
   hgfsVersionDeleteFile      = HGFS_OP_DELETE_FILE_V3;
   hgfsVersionDeleteDir       = HGFS_OP_DELETE_DIR_V3;
   hgfsVersionRename          = HGFS_OP_RENAME_V3;
   hgfsVersionQueryVolumeInfo = HGFS_OP_QUERY_VOLUME_INFO_V3;
   hgfsVersionCreateSymlink   = HGFS_OP_CREATE_SYMLINK_V3;
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
HgfsPackQueryVolumeRequest(const char *path,        // IN: File pointer for this open
                           HgfsOp opUsed,           // IN: Op to be used.
                           HgfsReq *req)            // IN/OUT: Packet to write into
{
   size_t requestSize;


   ASSERT(req);

   switch (opUsed) {
   case HGFS_OP_QUERY_VOLUME_INFO_V3: {
      int result;
      HgfsRequestQueryVolumeV3 *requestV3 = HgfsGetRequestPayload(req);

      requestV3->fileName.flags = 0;
      requestV3->fileName.fid = HGFS_INVALID_HANDLE;
      requestV3->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->reserved = 0;
      requestSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (requestSize - 1),
                                requestV3->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV3->fileName.length = result;
      requestSize += result;
      break;
   }
   case HGFS_OP_QUERY_VOLUME_INFO: {
      int result;
      HgfsRequestQueryVolume *request;

      request = (HgfsRequestQueryVolume *)(HGFS_REQ_PAYLOAD(req));

      requestSize = sizeof *request;
      /* Convert to CP name. */
      result = CPName_ConvertTo(path,
                                HGFS_LARGE_PACKET_MAX - (requestSize - 1),
                                request->fileName.name);
      if (result < 0) {
         LOG(4, ("CP conversion failed.\n"));
         return -EINVAL;
      }
      request->fileName.length = result;
      requestSize += result;
      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   req->payloadSize = requestSize;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatfs --
 *
 *    Hgfs 'statfs' method. Called when statfs(2) is invoked on the
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

int
HgfsStatfs(const char* path,            // IN : Path to the file
           struct statvfs *stat)        // OUT: Stat to fill in
{
   HgfsReq *req;
   int result = 0;
   HgfsOp opUsed;
   HgfsStatus replyStatus;
   uint64 freeBytes;
   uint64 totalBytes;

   LOG(6, ("Entered.\n"));
   memset(stat, 0, sizeof *stat);

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = hgfsVersionQueryVolumeInfo;
   result = HgfsPackQueryVolumeRequest(path, opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply.\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);

      /*
       * If the statfs succeeded on the server, copy the stats
       * into the statvfs struct, otherwise return an error.
       */
      switch (result) {
      case 0:
         stat->f_bsize = HGFS_BLOCKSIZE;
         if (opUsed == HGFS_OP_QUERY_VOLUME_INFO_V3) {
            HgfsReplyQueryVolumeV3 * replyV3 = HgfsGetReplyPayload(req);

            totalBytes = replyV3->totalBytes;
            freeBytes = replyV3->freeBytes;

         } else {
            totalBytes = ((HgfsReplyQueryVolume *)HGFS_REQ_PAYLOAD(req))->totalBytes;
            freeBytes = ((HgfsReplyQueryVolume *)HGFS_REQ_PAYLOAD(req))->freeBytes;
         }
         stat->f_blocks = (totalBytes + HGFS_BLOCKSIZE - 1) / HGFS_BLOCKSIZE;
         stat->f_bfree = (freeBytes + HGFS_BLOCKSIZE - 1) / HGFS_BLOCKSIZE;
         stat->f_bavail = stat->f_bfree;

         /*
          * Files application requires this field see bug 2287577.
          * This is using the same as the GNU C Library defined NAME_MAX
          */
         stat->f_namemax = NAME_MAX;
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
            LOG(4, ("Version 3 not supported. Falling back to version 1.\n"));
            hgfsVersionQueryVolumeInfo = HGFS_OP_QUERY_VOLUME_INFO;
            goto retry;
         }
         break;

      default:
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
