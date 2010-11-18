/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * vnopscommon.h --
 *
 * Common VFS vfsop implementations that are shared between both Mac OS and FreeBSD.
 */

#include <sys/param.h>          // for everything
#include <sys/vnode.h>          // for struct vnode

#include "fsutil.h"
#include "state.h"
#include "debug.h"
#include "request.h"
#include "vnopscommon.h"
#include "vfsopscommon.h"

/*
 *----------------------------------------------------------------------------
 *
 * HgfsStatfsInt --
 * 
 *      Hgfs statfs method. Called by HgfsVfsStatfs on FreeBSD and
 *      HgfsVfsGetattr on Mac OS.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsStatfsInt(struct vnode *vp,          // IN: vnode
              HgfsStatfs *stat)          // IN: statfs structure to fill in
{
   HgfsSuperInfo *sip;
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestQueryVolumeV3 *request;
   HgfsReplyQueryVolumeV3 *reply;
   uint32 reqSize;
   uint32 reqBufferSize;
   uint32 repSize;
   char *fullPath = NULL;
   uint32 fullPathLen;
   int ret = 0;

   /* Get pointer to the superinfo. */
   sip = HGFS_VP_TO_SIP(vp);
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "couldn't acquire superinfo\n");
      return ENOTSUP;
   }

   /* Prepare the request */
   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      return ret;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestQueryVolumeV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   /* Initialize the request header */
   requestHeader->id = HgfsKReq_GetId(req);
   requestHeader->op = HGFS_OP_QUERY_VOLUME_INFO_V3;

   request->fileName.flags = 0;
   request->fileName.fid = HGFS_INVALID_HANDLE;
   request->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   fullPath = HGFS_VP_TO_FILENAME(vp);
   fullPathLen = HGFS_VP_TO_FILENAME_LENGTH(vp);

   ret = HgfsNameToWireEncoding(fullPath, fullPathLen + 1,
                                request->fileName.name,
                                reqBufferSize);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
      ret = -ret;
      goto destroyOut;
   }
   request->fileName.length = ret;
   reqSize += ret;

   /* The request size includes header, request and file length */
   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);

   if (ret) {
      /* HgfsSubmitRequest() destroys the request if necessary */

      goto out;
   }

   replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
   reply = (HgfsReplyQueryVolumeV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);
   repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

   ret = HgfsGetStatus(req, repSize);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "reply was invalid");
      goto destroyOut;
   }

   ret = HgfsStatusToBSD(replyHeader->status);

   if (ret) {
      goto destroyOut;
   }

   stat->f_bsize = HGFS_BLOCKSIZE;
   stat->f_iosize = HGFS_BLOCKSIZE;
   stat->f_blocks = HGFS_CONVERT_TO_BLOCKS(reply->totalBytes);
   stat->f_bfree = HGFS_CONVERT_TO_BLOCKS(reply->freeBytes);
   stat->f_bavail = stat->f_bfree;

destroyOut:
   HgfsKReq_ReleaseRequest(sip->reqs, req);
out:
   return ret;
}
