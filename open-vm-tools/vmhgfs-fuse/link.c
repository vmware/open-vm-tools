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
 * link.c --
 *
 * Symlink-specific operations for HGFS driver
 */



#include "module.h"


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
HgfsPackSymlinkCreateRequest(const char* symlink,     // IN: path of the link
                             const char *symname,     // IN: Target name
                             HgfsOp opUsed,           // IN: Op to be used
                             HgfsReq *req)            // IN/OUT: Packet to write into
{
   HgfsRequestSymlinkCreateV3 *requestV3 = NULL;
   HgfsRequestSymlinkCreate *request = NULL;
   size_t targetNameBytes;
   size_t requestSize;

   targetNameBytes = strlen(symname) + 1;

   switch (opUsed) {
   case HGFS_OP_CREATE_SYMLINK_V3: {
      int result;
      HgfsFileNameV3 *fileNameP;
      requestV3 = HgfsGetRequestPayload(req);

      requestV3->symlinkName.flags = 0;
      requestV3->symlinkName.fid = HGFS_INVALID_HANDLE;
      requestV3->symlinkName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      requestV3->reserved = 0;
      requestSize = sizeof(*requestV3) + HgfsGetRequestHeaderSize();
      /* Convert symlink name to CP format. */
      result = CPName_ConvertTo(symlink,
                                HGFS_LARGE_PACKET_MAX - (requestSize - 1),
                                requestV3->symlinkName.name);
      if (result < 0) {
         LOG(4, ("SymlinkName CP conversion failed.\n"));
         return -EINVAL;
      }
      requestV3->symlinkName.length = result;
      requestSize += result;

      /* Copy target name into request packet. */
      if (targetNameBytes > HGFS_LARGE_PACKET_MAX - (requestSize - 1)) {
         LOG(4, ("Target name is too long.\n"));
         return -EINVAL;
      }

      fileNameP = (HgfsFileNameV3 *)((char *)&requestV3->symlinkName +
                                     sizeof requestV3->symlinkName + result);
      memcpy(fileNameP->name, symname, targetNameBytes);
      LOG(6, ("Target name: \"%s\"\n", fileNameP->name));
      /* Convert target name to CPName-lite format. */
      CPNameLite_ConvertTo(fileNameP->name, targetNameBytes - 1, '/');
      fileNameP->length = targetNameBytes - 1;
      fileNameP->flags = 0;
      fileNameP->fid = HGFS_INVALID_HANDLE;
      fileNameP->caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
      break;
   }
   case HGFS_OP_CREATE_SYMLINK: {
      int result;
      HgfsFileName *fileNameP;
      request = (HgfsRequestSymlinkCreate *)(HGFS_REQ_PAYLOAD(req));

      requestSize = sizeof *request;
      /* Convert symlink name to CP format. */
      result = CPName_ConvertTo(symlink,
                                HGFS_LARGE_PACKET_MAX - (requestSize - 1),
                                request->symlinkName.name);
      if (result < 0) {
         LOG(4, ("SymlinkName CP conversion failed.\n"));
         return -EINVAL;
      }
      request->symlinkName.length = result;
      requestSize += result;

      /* Copy target name into request packet. */
      if (targetNameBytes > HGFS_LARGE_PACKET_MAX - (requestSize - 1)) {
         LOG(4, ("Target name is too long.\n"));
         return -EINVAL;
      }

      fileNameP = (HgfsFileName *)((char *)&request->symlinkName +
                                   sizeof request->symlinkName + result);
      memcpy(fileNameP->name, symname, targetNameBytes);
      LOG(6, ("Target name: \"%s\"\n", fileNameP->name));
      /* Convert target name to CPName-lite format. */
      CPNameLite_ConvertTo(fileNameP->name, targetNameBytes - 1, '/');
      fileNameP->length = targetNameBytes - 1;
      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   req->payloadSize = requestSize;

   /*
    * targetNameBytes accounts for the NUL character. Once we've converted
    * it to CP name, it won't be NUL-terminated and the length of the string
    * in the packet itself won't account for it.
    */
   req->payloadSize += targetNameBytes - 1;

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

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

int
HgfsSymlink(const char* source,    // IN: Source name
            const char *symname)   // IN: Target name
{
   HgfsReq *req;
   int result = 0;
   HgfsOp opUsed;
   HgfsStatus replyStatus;

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

  retry:
   opUsed = hgfsVersionCreateSymlink;
   result = HgfsPackSymlinkCreateRequest(source, symname, opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply.\n"));
      replyStatus = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(replyStatus);
      if (result == 0) {
         LOG(6, ("Symlink created successfully, instantiating dentry.\n"));
      } else if (result == -EPROTO) {
         /* Retry with older version(s). Set globally. */
         if (opUsed == HGFS_OP_CREATE_SYMLINK_V3) {
            LOG(4, ("Version 3 not supported. Falling back to version 2.\n"));
            hgfsVersionCreateSymlink = HGFS_OP_CREATE_SYMLINK;
            goto retry;
         } else {
            LOG(6, ("Symlink was not created, error %d\n", result));
         }
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
