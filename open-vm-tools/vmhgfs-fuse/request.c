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
 * request.c --
 *
 * Functions dealing with the creation, deletion, and sending of HGFS
 * requests are defined here.
 */

#include "linux/list.h"
#include "module.h"
#include "request.h"
#include "transport.h"
#include "fsutil.h"
#include "vm_assert.h"

static HgfsHandle hgfsIdCounter;
pthread_mutex_t hgfsIdLock = PTHREAD_MUTEX_INITIALIZER;


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetNewRequest --
 *
 *    Get a new request structure off the free list and initialize it.
 *
 * Results:
 *    On success the new struct is returned with all fields
 *    initialized. Returns NULL on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsReq *
HgfsGetNewRequest(void)
{
   HgfsReq *req = (HgfsReq*) malloc(sizeof(HgfsReq));
   if (req == NULL) {
      LOG(4, ("Can't allocate memory.\n"));
      return NULL;
   }
   INIT_LIST_HEAD(&req->list);
   req->payloadSize = 0;
   req->state = HGFS_REQ_STATE_ALLOCATED;
   /* Setup the packet prefix. */
   memcpy(req->packet, HGFS_SYNC_REQREP_CLIENT_CMD,
          HGFS_SYNC_REQREP_CLIENT_CMD_LEN);
   pthread_mutex_lock(&hgfsIdLock);
   req->id = hgfsIdCounter;
   hgfsIdCounter++;
   pthread_mutex_unlock(&hgfsIdLock);

   return req;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsPackHeader --
 *
 *    Fill the header fields for the request. If session is enabled,
 *    use new header.
 *
 * Results:
 *    HGFS_STATUS_SUCCESS for a valid header
 *    else HGFS_STATUS_PROTOCOL_ERROR
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsStatus
HgfsPackHeader(HgfsReq *req,  // IN/OUT:
               HgfsOp opUsed) // IN
{
   if (gState->sessionEnabled) { /* use new header */
      HgfsHeader *header = (HgfsHeader*)HGFS_REQ_PAYLOAD(req);

      LOG(4, ("sessionEnabled, use HgfsHeader. opUsed = %d\n", opUsed));
      header->version = gState->headerVersion;
      header->dummy = HGFS_OP_NEW_HEADER;
      header->headerSize = sizeof *header;
      header->packetSize = req->payloadSize;
      header->requestId = req->id;
      header->op = opUsed;
      header->sessionId = gState->sessionId;
      header->flags = HGFS_PACKET_FLAG_REQUEST;
      /*
       * Currently unused fields which can be used in version 2 and later.
       * Version 1 didn't zero these fields, hence the server cannot determine
       * their validity.
       */
      header->status = 0;
      header->information = 0;
      header->reserved = 0;
      memset(&header->reserved1[0], 0, sizeof header->reserved1);

   } else {
      HgfsRequest *header = (HgfsRequest*)HGFS_REQ_PAYLOAD(req);

      LOG(4, ("not sessionEnabled, use HgfsRequest. opUsed = %d\n", opUsed));
      header->id = req->id;
      header->op = opUsed;
   }

   return HGFS_STATUS_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackHeader --
 *
 *    Validate that server reply contains valid HgfsHeader that matches
 *    current session.
 *    Extract various useful fields from the protocol header of the reply.
 *
 * Results:
 *    HGFS_STATUS_SUCCESS for a valid header
 *    else HGFS_STATUS_PROTOCOL_ERROR
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

HgfsStatus
HgfsUnpackHeader(void *serverReply,           // IN:  server reply
                 size_t replySize,            // IN:  reply data size
                 uint8 *headerVersion,        // OUT: version
                 uint64 *sessionId,           // OUT: unique session id
                 uint32 *requestId,           // OUT: unique request id
                 uint32 *headerFlags,         // OUT: header flags
                 uint32 *information,         // OUT: generic information
                 HgfsOp *opcode,              // OUT: request opcode
                 HgfsStatus *replyStatus,     // OUT: reply status
                 void **payload,              // OUT: pointer to the payload
                 size_t *payloadSize)         // OUT: size of the payload
{
   HgfsHeader *header = (HgfsHeader *)serverReply;

   /* First some sanity checking. */
   if ((replySize < sizeof (HgfsHeader)) ||
       (header->dummy != HGFS_OP_NEW_HEADER) ||
       (header->packetSize > replySize) ||
       (header->headerSize > header->packetSize)) {
      return HGFS_STATUS_PROTOCOL_ERROR;
   }

   *headerVersion = header->version;
   *sessionId = header->sessionId;
   *requestId = header->requestId;
   *opcode = header->op;
   *headerFlags = header->flags;
   *information = header->information;
   *payloadSize = header->packetSize - header->headerSize;
   if (*payloadSize) {
      *payload = (char *)serverReply + header->headerSize;
   } else {
      *payload = NULL;
   }

   *replyStatus = header->status;

   return HGFS_STATUS_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetRequestPayload --
 *
 * Results:
 *    Returns a pointer to the start of the payload data.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

INLINE void*
HgfsGetRequestPayload(HgfsReq *req)   // IN:
{
   if (gState->sessionEnabled) {
      return (void*) HGFS_REQ_GET_PAYLOAD_HDRV2(req);
   } else {
      return (void*) HGFS_REQ_PAYLOAD_V3(req);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetReplyPayload --
 *
 * Results:
 *    Returns a pointer to the start of the payload data.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

INLINE void*
HgfsGetReplyPayload(HgfsReq *rep)   // IN:
{
   if (gState->sessionEnabled) {
      return (void*) HGFS_REP_GET_PAYLOAD_HDRV2(rep);
   } else {
      return (void*) HGFS_REP_PAYLOAD_V3(rep);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetRequestHeaderSize --
 *
 * Results:
 *    The size of request message header.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

INLINE size_t
HgfsGetRequestHeaderSize(void)
{
   return (gState->sessionEnabled ?
           sizeof(HgfsHeader) :
           sizeof(HgfsRequest));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetReplyHeaderSize --
 *
 * Results:
 *    The size of reply message header.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

INLINE size_t
HgfsGetReplyHeaderSize(void)
{
   return (gState->sessionEnabled ?
           sizeof(HgfsHeader) :
           sizeof(HgfsReply));
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsSendRequest --
 *
 *    Send out an HGFS request via transport layer, and wait for the reply.
 *
 * Results:
 *    Returns zero on success, negative number on error.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
HgfsSendRequest(HgfsReq *req)       // IN/OUT: Outgoing request
{
   int ret;

   ASSERT(req);
   ASSERT(req->payloadSize <= HgfsLargePacketMax(FALSE));

   req->state = HGFS_REQ_STATE_UNSENT;

   LOG(8, ("Sending request id %d\n", req->id));
   LOG(4, ("Before sending \n"));

   ret = HgfsTransportSendRequest(req);
   LOG(4, ("After sending \n"));

   LOG(8, ("Request finished, return %d\n", ret));
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsFreeRequest --
 *
 *    Free an HGFS request.
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
HgfsFreeRequest(HgfsReq *req) // IN: Request to free
{
   free(req);
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsReplyStatus --
 *
 *    Return reply status.
 *
 * Results:
 *    Returns reply status as per the protocol.
 *
 * Side effects:
 *    reestablish session when encounter session stale failure.
 *
 *----------------------------------------------------------------------
 */

HgfsStatus
HgfsGetReplyStatus(HgfsReq *req)  // IN
{
   HgfsStatus status;

   if (req->payloadSize < sizeof (HgfsReply)) {
      LOG(4, ("Malformed packet received.\n"));
      status = HGFS_STATUS_PROTOCOL_ERROR;
      goto out;
   }

   if (gState->sessionEnabled &&
      req->payloadSize < sizeof (HgfsHeader)) {
      /*
       * We have enabled an HGFS protocol session which uses the new header
       * format. And an HGFS protocol session uses the new header format only.
       * A reply without the new header indicates a message with the
       * old reply header format.
       */
      gState->sessionEnabled = FALSE;
   }

   if (gState->sessionEnabled) {
      HgfsHeader *header = (HgfsHeader *)(HGFS_REQ_PAYLOAD(req));
      status = header->status;

      if (status == HGFS_STATUS_STALE_SESSION) {
         LOG(4, ("Session stale! Try to recreate session ...\n"));
         HgfsCreateSession();
         /*
          * XXX: User might want to retry it later, and status will not be
          * changed here. But due to the fail safe directory access like
          * searching for dynamic library path, user hardly ever notice
          * the failure.
          */
      }

   } else {
      HgfsReply *reply = (HgfsReply *)(HGFS_REQ_PAYLOAD(req));
      status = reply->status;
   }

out:
   LOG(4, ("Exit(status = %d)\n", status));
   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsCompleteReq --
 *
 *    Copies the reply packet into the request structure and wakes up
 *    the associated client.
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
HgfsCompleteReq(HgfsReq *req,       // IN: Request
                char const *reply,  // IN: Reply packet
                size_t replySize)   // IN: Size of reply packet
{
   ASSERT(req);
   ASSERT(reply);
   ASSERT(replySize <= HgfsLargePacketMax(FALSE));

   memcpy(HGFS_REQ_PAYLOAD(req), reply, replySize);
   req->payloadSize = replySize;
   req->state = HGFS_REQ_STATE_COMPLETED;
   if (!list_empty(&req->list)) {
      list_del_init(&req->list);
   }
}
