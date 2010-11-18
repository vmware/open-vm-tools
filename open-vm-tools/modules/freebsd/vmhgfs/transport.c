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
 * transport.c --
 *
 * Functions that prepare HGFS packages and send them to the host.
 * Implementations are shared between both Mac OS and FreeBSD.
 */

#include <sys/param.h>          // for everything
#include <sys/vnode.h>          // for struct vnode
#include <sys/dirent.h>         // for struct dirent

#include "fsutil.h"
#include "debug.h"
#include "transport.h"
#include "cpName.h"
#include "os.h"

#define HGFS_FILE_OPEN_MASK (HGFS_OPEN_VALID_MODE | \
                             HGFS_OPEN_VALID_FLAGS | \
                             HGFS_OPEN_VALID_SPECIAL_PERMS | \
                             HGFS_OPEN_VALID_OWNER_PERMS | \
                             HGFS_OPEN_VALID_GROUP_PERMS | \
                             HGFS_OPEN_VALID_OTHER_PERMS | \
                             HGFS_OPEN_VALID_FILE_NAME)

/*
 *----------------------------------------------------------------------------
 *
 * HgfsSendOpenDirRequest --
 *
 *       Sends a SEARCH_OPEN request to the Hgfs server.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSendOpenDirRequest(HgfsSuperInfo *sip,   // IN: Superinfo pointer
                       char *fullPath,       // IN: Full path for the file
                       uint32 fullPathLen,   // IN: Length of the full path
                       HgfsHandle *handle)   // OUT: HGFS handle for the opened file
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestSearchOpenV3 *request;
   HgfsReplySearchOpenV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;
   int ret;

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      return ret;
   }

   /* Set the correct header values */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestSearchOpenV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_SEARCH_OPEN_V3);

   request->dirName.flags = 0;
   request->dirName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   request->dirName.fid = HGFS_INVALID_HANDLE;
   request->reserved = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(fullPath, fullPathLen + 1,
                                request->dirName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
      HgfsKReq_ReleaseRequest(sip->reqs, req);
      return -ret;
   }

   request->dirName.length = ret;
   reqSize += ret;

   HgfsKReq_SetPayloadSize(req, reqSize);

   /* Submit the request to the Hgfs server */
   ret = HgfsSubmitRequest(sip, req);
   if (ret == 0) {

      /* Our reply is in the request packet */
      replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
      reply = (HgfsReplySearchOpenV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);

      repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);

      ret = HgfsGetStatus(req, repSize);
      if (ret == 0) {
         *handle = reply->search;
      } else {
         DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
      }

      DEBUG(VM_DEBUG_COMM, "received reply for ID %d\n", replyHeader->id);
      DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", replyHeader->status);
      DEBUG(VM_DEBUG_COMM, " handle: %d\n", reply->search);

      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * HgfsSendOpenRequest --
 *
 *      Sends an OPEN request to the Hgfs server to open an existing file.
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSendOpenRequest(HgfsSuperInfo *sip,   // IN: Superinfo pointer
                    int openMode,         // IN: HGFS mode of open
                    int openFlags,        // IN: HGFS open flags
                    int permissions,      // IN: Permissions of open (only when creating)
                    char *fullPath,       // IN: Full path for the file
                    uint32 fullPathLen,   // IN: Length of the full path
                    HgfsHandle *handle)   // OUT: HGFS handle for the opened file
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestOpenV3 *request;
   HgfsReplyOpenV3 *reply;
   int ret;
   uint32 reqSize;
   uint32 repSize;
   uint32 reqBufferSize;

   DEBUG(VM_DEBUG_LOG, "Trace enter.\n");
   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      DEBUG(VM_DEBUG_FAIL, "HgfsKReq_AllocateRequest failed.\n");
      return ret;
   }

   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestOpenV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_OPEN_V3);

   request->mask = HGFS_FILE_OPEN_MASK;
   request->reserved1 = 0;
   request->reserved2 = 0;

   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);
   reqBufferSize = HGFS_NAME_BUFFER_SIZET(HGFS_PACKET_MAX, reqSize);
   request->mode = openMode;
   request->flags = openFlags;
   DEBUG(VM_DEBUG_COMM, "open flags are %x\n", request->flags);

   request->specialPerms = (permissions & (S_ISUID | S_ISGID | S_ISVTX)) >>
                              HGFS_ATTR_SPECIAL_PERM_SHIFT;
   request->ownerPerms = (permissions & S_IRWXU) >> HGFS_ATTR_OWNER_PERM_SHIFT;
   request->groupPerms = (permissions & S_IRWXG) >> HGFS_ATTR_GROUP_PERM_SHIFT;
   request->otherPerms = permissions & S_IRWXO;
   DEBUG(VM_DEBUG_COMM, "permissions are %o\n", permissions);

   request->fileName.flags = 0;
   request->fileName.caseType = HGFS_FILE_NAME_CASE_SENSITIVE;
   request->fileName.fid = HGFS_INVALID_HANDLE;

   /*
    * Convert an input string to utf8 precomposed form, convert it to
    * the cross platform name format and finally unescape any illegal
    * filesystem characters.
    */
   ret = HgfsNameToWireEncoding(fullPath, fullPathLen + 1,
                                request->fileName.name,
                                reqBufferSize);

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "Could not encode to wire format");
      ret = -ret;
      goto out;
   }
   request->fileName.length = ret;
   reqSize += ret;

   /* Packet size includes the request and its payload. */
   HgfsKReq_SetPayloadSize(req, reqSize);

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      /* HgfsSubmitRequest will destroy the request if necessary. */
      DEBUG(VM_DEBUG_FAIL, "could not submit request.\n");
      req = NULL; // Request has been deallocated by SubmitRequest
   } else {
      replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
      reply = (HgfsReplyOpenV3 *)HGFS_REP_GET_PAYLOAD_V3(replyHeader);
      repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);
      ret = HgfsGetStatus(req, repSize);
      if (ret == 0) {
         *handle = reply->file;
      }
   }
out:
   if (req != NULL) {
      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   return ret;
}

/*
 *----------------------------------------------------------------------------
 *
 * HgfsCloseServerDirHandle --
 *
 *      Prepares close handle request and sends it to the HGFS server
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCloseServerDirHandle(HgfsSuperInfo *sip,         // IN: Superinfo pointer
                         HgfsHandle handle)          // IN: Handle to close
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestSearchCloseV3 *request;
   HgfsReplySearchCloseV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   int ret;

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      return ret;
   }

   /*
    * Prepare the request structure.  Of note here is that the request is
    * always the same size so we just set the packetSize to that.
    */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestSearchCloseV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_SEARCH_CLOSE_V3);

   request->search = handle;
   request->reserved = 0;
   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);

   HgfsKReq_SetPayloadSize(req, reqSize);

   /* Submit the request to the Hgfs server */
   ret = HgfsSubmitRequest(sip, req);
   if (ret == 0) {
      replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
      repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);
      DEBUG(VM_DEBUG_COMM, "received reply for ID %d\n", replyHeader->id);
      DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", replyHeader->status);

      ret = HgfsGetStatus(req, repSize);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
         if (ret != EPROTO) {
            ret = EFAULT;
         }
      }

      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCloseServerFileHandle --
 *
 *      Prepares close handle request and sends it to the HGFS server
 *
 * Results:
 *      Returns zero on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCloseServerFileHandle(HgfsSuperInfo *sip,         // IN: Superinfo pointer
                          HgfsHandle handle)          // IN: Handle to close
{
   HgfsKReqHandle req;
   HgfsRequest *requestHeader;
   HgfsReply *replyHeader;
   HgfsRequestCloseV3 *request;
   HgfsReplyCloseV3 *reply;
   uint32 reqSize;
   uint32 repSize;
   int ret;

   req = HgfsKReq_AllocateRequest(sip->reqs, &ret);
   if (!req) {
      return ret;
   }

   /*
    * Prepare the request structure.  Of note here is that the request is
    * always the same size so we just set the packetSize to that.
    */
   requestHeader = (HgfsRequest *)HgfsKReq_GetPayload(req);
   request = (HgfsRequestCloseV3 *)HGFS_REQ_GET_PAYLOAD_V3(requestHeader);

   HGFS_INIT_REQUEST_HDR(requestHeader, req, HGFS_OP_CLOSE_V3);

   request->file = handle;
   request->reserved = 0;
   reqSize = HGFS_REQ_PAYLOAD_SIZE_V3(request);

   HgfsKReq_SetPayloadSize(req, reqSize);

   /* Submit the request to the Hgfs server */
   ret = HgfsSubmitRequest(sip, req);
   if (ret == 0) {
      replyHeader = (HgfsReply *)HgfsKReq_GetPayload(req);
      repSize = HGFS_REP_PAYLOAD_SIZE_V3(reply);
      DEBUG(VM_DEBUG_COMM, "received reply for ID %d\n", replyHeader->id);
      DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", replyHeader->status);

      ret = HgfsGetStatus(req, repSize);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "Error encountered with ret = %d\n", ret);
         if (ret != EPROTO) {
            ret = EFAULT;
         }
      }

      HgfsKReq_ReleaseRequest(sip->reqs, req);
   }
   return ret;
}
