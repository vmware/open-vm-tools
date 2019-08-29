/*********************************************************
 * Copyright (C) 2015-2016,2019 VMware, Inc. All rights reserved.
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
 * session.c --
 *
 * Hgfs session related operations.
 */

#include "module.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackCreateSessionRequestV4 --
 *
 *    Packs create session request to be sent to the server.
 *    This is the first request that is sent after a connection
 *    to a server had been established.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsPackCreateSessionRequest(HgfsOp opUsed, // IN: Op to be used
                             HgfsReq *req)  // IN/OUT: Packet to write into
{
   switch (opUsed) {
   case HGFS_OP_CREATE_SESSION_V4: {
      HgfsRequestCreateSessionV4 *requestV4 = HgfsGetRequestPayload(req);

      requestV4->numCapabilities = 0;
      requestV4->maxPacketSize = HgfsLargePacketMax(FALSE);
      requestV4->reserved = 0;

      req->payloadSize = sizeof(*requestV4) + HgfsGetRequestHeaderSize();

      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   /* Fill in header here as payloadSize needs to be there. */
   HgfsPackHeader(req, opUsed);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCreateSessionProcessResult --
 *
 *    Process create session server reply.
 *
 * Results:
 *    HGFS_STATUS_SUCCESS if everything is right,
 *    else HGFS_STATUS_PROTOCOL_ERROR
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static HgfsStatus
HgfsCreateSessionProcessResult(const char *result,  // IN: Reply packet
                               size_t resultSize)   // IN: packet size
{
   HgfsStatus status = HGFS_STATUS_SUCCESS;
   uint64 sessionId = HGFS_INVALID_SESSION_ID;
   uint8 headerVersion = HGFS_HEADER_VERSION_1;
   Bool sessionIdPresent = FALSE;
   uint32 maxPacketSize = HgfsLargePacketMax(TRUE);

   uint32 information;
   HgfsHandle requestId;
   HgfsStatus tmpStatus;
   uint32 headerFlags = 0;
   HgfsOp op;
   void *replyPayload;
   size_t replyPayloadSize;
   HgfsReplyCreateSessionV4 *createSessionReply;

   tmpStatus = HgfsUnpackHeader((void *) result,
				resultSize,
				&headerVersion,
				&sessionId,
				&requestId,
				&headerFlags,
				&information,
				&op,
				&status,
				&replyPayload,
				&replyPayloadSize);
   /* Older servers set the header flags to zero. */
   if (0 != headerFlags) {
      if (0 == (headerFlags & HGFS_PACKET_FLAG_REPLY)) {
	 /* The server request flag must be set for these packets. */
	 status = HGFS_STATUS_PROTOCOL_ERROR;
	 goto out;
      }
   }
   if (tmpStatus != HGFS_STATUS_SUCCESS) {
      LOG(4, ("Malformed packet received. status=%d\n", tmpStatus));
      status = HGFS_STATUS_PROTOCOL_ERROR;
      goto out;
   }

   if (status == HGFS_STATUS_SUCCESS) {
      createSessionReply = (HgfsReplyCreateSessionV4 *) replyPayload;
      ASSERT(createSessionReply);

      LOG(4, ("Successfully created the session.\n"));

      /*
       * XXX: HgfsServer returns other properties like capabilities etc.
       * in the reply. In the future, we should extend the
       * structure to hold these properties returned from the HgfsServer
       * for CreateSession request.
       */
      sessionId = createSessionReply->sessionId;
      sessionIdPresent = TRUE;
      maxPacketSize = createSessionReply->maxPacketSize;
   }

out:
   gState->sessionId = sessionId;
   gState->headerVersion = headerVersion;
   gState->sessionEnabled = sessionIdPresent;
   gState->maxPacketSize = maxPacketSize;

   LOG(4, ("Exit(%d)\n", status));
   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCreateSession --
 *
 *    Sends a Create session request to the HGFS Server.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

int
HgfsCreateSession(void)
{
   HgfsReq *req;
   int result;
   HgfsStatus status;
   HgfsOp opUsed;

   LOG(4, ("Entry()\n"));
   gState->sessionEnabled = TRUE;
   gState->headerVersion = HGFS_HEADER_VERSION;

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

   opUsed = hgfsVersionCreateSession;
   result = HgfsPackCreateSessionRequest(opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply.\n"));
      status = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(status);
      switch (result) {
      case 0:
         status = HgfsCreateSessionProcessResult(HGFS_REQ_PAYLOAD(req),
                                                 req->payloadSize);
         ASSERT(status == HGFS_STATUS_SUCCESS);
         break;
      case -EPROTO:
         /* Fallthrough. */
      default:
         LOG(6, ("Session was not created, error %d\n", result));
         gState->sessionEnabled = FALSE;
         break;
      }

   } else {
      gState->sessionEnabled = FALSE;

      if (result == -EIO) {
         LOG(4, ("Timed out. error: %d\n", result));
      } else if (result == -EPROTO) {
         LOG(4, ("Server returned error: %d\n", result));
      } else {
         LOG(4, ("Unknown error: %d\n", result));
      }
   }

out:
   HgfsFreeRequest(req);
   LOG(4, ("Exit(%d)\n", result));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackDestroySessionRequestV4 --
 *
 *    Packs destroy session request to be sent to the server.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsPackDestroySessionRequest(HgfsOp opUsed, // IN: Op to be used
                             HgfsReq *req)   // IN/OUT: Packet to write into
{
   switch (opUsed) {
   case HGFS_OP_DESTROY_SESSION_V4: {
      HgfsRequestDestroySessionV4 *requestV4 =
         (HgfsRequestDestroySessionV4*)HgfsGetRequestPayload(req);

      requestV4->reserved = 0;

      req->payloadSize = sizeof(HgfsHeader) + sizeof(*requestV4);
      /* Fill in header here as payloadSize needs to be there. */
      HgfsPackHeader(req, opUsed);

      break;
   }
   default:
      LOG(4, ("Unexpected OP type encountered. opUsed = %d\n", opUsed));
      return -EPROTO;
   }

   return 0;
}



/*
 *----------------------------------------------------------------------------
 *
 * HgfsDestroySessionProcessResult --
 *
 *    Process destroy session server reply.
 *
 * Results:
 *    HGFS_STATUS_SUCCESS if everything is right,
 *    else HGFS_STATUS_PROTOCOL_ERROR
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

static HgfsStatus
HgfsDestroySessionProcessResult(const char *result,  // IN: Reply packet
                                size_t resultSize)   // IN: packet size
{
   uint8 headerVersion = HGFS_HEADER_VERSION_1;
   uint64 sessionId;
   HgfsHandle requestId;
   uint32 headerFlags = 0;
   uint32 information;
   HgfsOp op;
   size_t payloadSize;
   void *replyPayload;
   HgfsStatus status = HGFS_STATUS_SUCCESS;
   HgfsStatus tmpStatus;

   tmpStatus = HgfsUnpackHeader((void *) result,
				resultSize,
				&headerVersion,
				&sessionId,
				&requestId,
				&headerFlags,
				&information,
				&op,
				&status,
				&replyPayload,
				&payloadSize);
   if (tmpStatus != HGFS_STATUS_SUCCESS) {
      LOG(4, ("%s: Malformed packet received.\n", __FUNCTION__));
      status = HGFS_STATUS_PROTOCOL_ERROR;
      goto out;
   }

   if(status == HGFS_STATUS_SUCCESS) {
      LOG(4, ("Successfully destroyed the session.\n"));
   }

out:
   gState->sessionId = HGFS_INVALID_SESSION_ID;
   gState->sessionEnabled = FALSE;
   gState->maxPacketSize = HgfsLargePacketMax(TRUE);

   LOG(4, ("Exit(%d)\n", status));
   return status;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDestroySession --
 *
 *    Sends a Create session request to the HGFS Server in the guest.
 *
 * Results:
 *    Returns zero on success, or negative error on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

int
HgfsDestroySession(void)
{
   HgfsReq *req;
   int result;
   HgfsStatus status;
   HgfsOp opUsed;

   LOG(4, ("Entry()\n"));
   if (!gState->sessionEnabled) {
     return 0;
   }

   req = HgfsGetNewRequest();
   if (!req) {
      LOG(4, ("Out of memory while getting new request.\n"));
      result = -ENOMEM;
      goto out;
   }

   opUsed = hgfsVersionDestroySession;
   result = HgfsPackDestroySessionRequest(opUsed, req);
   if (result != 0) {
      LOG(4, ("Error packing request.\n"));
      goto out;
   }

   result = HgfsSendRequest(req);
   if (result == 0) {
      LOG(6, ("Got reply.\n"));
      status = HgfsGetReplyStatus(req);
      result = HgfsStatusConvertToLinux(status);
      switch (result) {
      case 0:
         status = HgfsDestroySessionProcessResult(HGFS_REQ_PAYLOAD(req),
                                                  req->payloadSize);
         ASSERT(status == HGFS_STATUS_SUCCESS);
         break;
      case -EPROTO:
         /* Fallthrough. */
      default:
         LOG(6, ("Session was not created, error %d\n", result));
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
   LOG(4, ("Exit(%d)\n", result));
   return result;
}

