/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#include <string.h>
#include <stdlib.h>

#include "vmware.h"
#include "str.h"
#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsServerInt.h"
#include "hgfsServerPolicy.h"
#include "codeset.h"
#include "config.h"
#include "file.h"
#include "util.h"
#include "wiper.h"
#include "vm_basic_asm.h"

#define LOGLEVEL_MODULE hgfs
#include "loglevel_user.h"

#ifdef _WIN32
#define HGFS_REQUEST_WIN32_SUPPORTED  HGFS_REQUEST_SUPPORTED
#define HGFS_REQUEST_POSIX_SUPPORTED  HGFS_REQUEST_NOT_SUPPORTED
#else
#define HGFS_REQUEST_WIN32_SUPPORTED  HGFS_REQUEST_NOT_SUPPORTED
#define HGFS_REQUEST_POSIX_SUPPORTED  HGFS_REQUEST_SUPPORTED
#endif

#define HGFS_ASSERT_PACK_PARAMS \
   do { \
      ASSERT(packet); \
      ASSERT(packetHeader); \
      ASSERT(session); \
      ASSERT(payloadSize); \
   } while(0)

/*
 * This is the default/minimal set of capabilities which is supported by every transport.
 * Every transport and session may have additional capabilities in addition to these.
 */
static HgfsCapability hgfsDefaultCapabilityTable[] =
{
   {HGFS_OP_OPEN,                  HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_READ,                  HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_WRITE,                 HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CLOSE,                 HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_OPEN,           HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_READ,           HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_CLOSE,          HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_GETATTR,               HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SETATTR,               HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CREATE_DIR,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_FILE,           HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_DIR,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_RENAME,                HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_QUERY_VOLUME_INFO,     HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_OPEN_V2,               HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_GETATTR_V2,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SETATTR_V2,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_READ_V2,        HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CREATE_SYMLINK,        HGFS_REQUEST_POSIX_SUPPORTED},
   {HGFS_OP_SERVER_LOCK_CHANGE,    HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_CREATE_DIR_V2,         HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_FILE_V2,        HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_DIR_V2,         HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_RENAME_V2,             HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_OPEN_V3,               HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_READ_V3,               HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_WRITE_V3,              HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CLOSE_V3,              HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_OPEN_V3,        HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_READ_V3,        HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SEARCH_CLOSE_V3,       HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_GETATTR_V3,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_SETATTR_V3,            HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CREATE_DIR_V3,         HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_FILE_V3,        HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DELETE_DIR_V3,         HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_RENAME_V3,             HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_QUERY_VOLUME_INFO_V3,  HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_CREATE_SYMLINK_V3,     HGFS_REQUEST_POSIX_SUPPORTED},
   {HGFS_OP_SERVER_LOCK_CHANGE_V3, HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_WRITE_WIN32_STREAM_V3, HGFS_REQUEST_WIN32_SUPPORTED},
   {HGFS_OP_CREATE_SESSION_V4,     HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_DESTROY_SESSION_V4,    HGFS_REQUEST_SUPPORTED},
   {HGFS_OP_READ_FAST_V4,          HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_WRITE_FAST_V4,         HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_SET_WATCH_V4,          HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_REMOVE_WATCH_V4,       HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_NOTIFY_V4,             HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_SEARCH_READ_V4,        HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_OPEN_V4,               HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_ENUMERATE_STREAMS_V4,  HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_GETATTR_V4,            HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_SETATTR_V4,            HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_DELETE_V4,             HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_LINKMOVE_V4,           HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_FSCTL_V4,              HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_ACCESS_CHECK_V4,       HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_FSYNC_V4,              HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_QUERY_VOLUME_INFO_V4,  HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_OPLOCK_ACQUIRE_V4,     HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_OPLOCK_BREAK_V4,       HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_LOCK_BYTE_RANGE_V4,    HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_UNLOCK_BYTE_RANGE_V4,  HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_QUERY_EAS_V4,          HGFS_REQUEST_NOT_SUPPORTED},
   {HGFS_OP_SET_EAS_V4,            HGFS_REQUEST_NOT_SUPPORTED},
};

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsValidatePacket --
 *
 *    Validates that packet is not malformed. Checks consistency of various
 *    fields and sizes.
 *
 * Results:
 *    TRUE if the packet is correct.
 *    FALSE if the packet is malformed.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsValidatePacket(char const *packetIn,  // IN: request packet
                   size_t packetSize,     // IN: request packet size
                   Bool v4header)         // IN: HGFS header type
{
   HgfsRequest *request = (HgfsRequest *)packetIn;
   Bool result = TRUE;
   if (packetSize < sizeof *request) {
      LOG(4, ("%s: Malformed HGFS packet received - packet too small!\n", __FUNCTION__));
      return FALSE;
   }
   if (v4header) {
      HgfsHeader *header = (HgfsHeader *)packetIn;
      ASSERT(packetSize >= header->packetSize);
      ASSERT(header->packetSize >= header->headerSize);
      result = packetSize >= offsetof(HgfsHeader, requestId) &&
               header->headerSize >= offsetof(HgfsHeader, reserved) &&
               header->packetSize >= header->headerSize &&
               packetSize >= header->packetSize;
   } else {
      result = packetSize >= sizeof *request;
   }
   if (!result) {
      LOG(4, ("%s: Malformed HGFS packet received!\n", __FUNCTION__));
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsValidateReplySize --
 *
 *    Verify if the size of a reply does not exceed maximum supported size.
 *
 * Results:
 *    TRUE if the packet size is acceptable, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsValidateReplySize(char const *packetIn,
                      HgfsOp op,
                      size_t packetSize)
{
   Bool result;
   HgfsRequest *request = (HgfsRequest *)packetIn;

   if (HGFS_V4_LEGACY_OPCODE != request->op) {
      if (HGFS_OP_READ_V3 == op) {
         result = packetSize <= HGFS_LARGE_PACKET_MAX;
      } else {
         result = packetSize <= HGFS_PACKET_MAX;
      }
   } else {
      result = TRUE;
   }
   if (!result) {
      LOG(4, ("%s: Reply exceeded maximum support size!\n", __FUNCTION__));
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetPayloadSize --
 *
 *    Returns size of the payload based on incoming packet and total
 *    packet size.
 *
 * Results:
 *    Size of the payload in bytes.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

size_t
HgfsGetPayloadSize(char const *packetIn,        // IN: request packet
                   size_t packetSize)           // IN: request packet size
{
   HgfsRequest *request = (HgfsRequest *)packetIn;
   size_t result;
   ASSERT(packetSize >= sizeof *request);
   if (request->op < HGFS_OP_CREATE_SESSION_V4) {
      result = packetSize - sizeof *request;
   } else {
      HgfsHeader *header = (HgfsHeader *)packetIn;
      ASSERT(packetSize >= header->packetSize);
      ASSERT(header->packetSize >= header->headerSize);
      result = header->packetSize - header->headerSize;
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsParseRequest --
 *
 *    Returns requested operation and pointer to the payload based on
 *    incoming packet and total packet size.
 *
 * Results:
 *    TRUE if a reply can be sent.
 *    FALSE if incoming packet does not allow sending any response.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsParseRequest(HgfsPacket *packet,         // IN: request packet
                 HgfsTransportSessionInfo *transportSession,   // IN: current session
                 HgfsInputParam **input,     // OUT: request parameters
                 HgfsInternalStatus *status) // OUT: error code
{
   HgfsRequest *request;
   size_t packetSize;
   HgfsInternalStatus result = HGFS_ERROR_SUCCESS;
   HgfsInputParam *localInput;
   HgfsSessionInfo *session = NULL;

   request = (HgfsRequest *) HSPU_GetMetaPacket(packet, &packetSize, transportSession);
   ASSERT_DEVEL(request);

   LOG(4, ("%s: Recieved a request with opcode %d.\n", __FUNCTION__, (int) request->op));

   if (!request) {
      /*
       * How can I return error back to the client, clearly the client is either broken or
       * malicious? We cannot continue from here.
       */
      return FALSE;
   }

   *input = Util_SafeMalloc(sizeof *localInput);
   localInput = *input;

   memset(localInput, 0, sizeof *localInput);
   localInput->metaPacket = (char *)request;
   localInput->metaPacketSize = packetSize;
   localInput->transportSession = transportSession;
   localInput->packet = packet;
   localInput->session = NULL;

   /*
    * Error out if less than HgfsRequest size.
    */
   if (packetSize < sizeof *request) {
      if (packetSize >= sizeof request->id) {
         localInput->id = request->id;
      }
      ASSERT_DEVEL(0);
      return FALSE;
   }

   if (request->op < HGFS_OP_OPEN_V3) {
      /* Legacy requests do not have a separate header. */
      localInput->payload = request;
      localInput->op = request->op;
      localInput->payloadSize = packetSize;
      localInput->id = request->id;
   } else if (request->op < HGFS_OP_CREATE_SESSION_V4) {
      /* V3 header. */
      if (packetSize > sizeof *request) {
         localInput->payload = HGFS_REQ_GET_PAYLOAD_V3(request);
         localInput->payloadSize = packetSize -
                                      ((char *)localInput->payload - (char *)request);
      }
      localInput->op = request->op;
      localInput->id = request->id;
   } else if (HGFS_V4_LEGACY_OPCODE == request->op) {
      /* V4 header. */
      HgfsHeader *header = (HgfsHeader *)request;
      localInput->v4header = TRUE;
      localInput->id = header->requestId;
      localInput->op = header->op;

      if (packetSize >= offsetof(HgfsHeader, sessionId) + sizeof header->sessionId) {
         if (packetSize < header->packetSize ||
            header->packetSize < header->headerSize) {
            LOG(4, ("%s: Malformed HGFS packet received - inconsistent header"
               " and packet sizes!\n", __FUNCTION__));
            result = HGFS_ERROR_PROTOCOL;
         }

         if ((HGFS_ERROR_SUCCESS == result) &&
             (header->op != HGFS_OP_CREATE_SESSION_V4)) {
            session = HgfsServerTransportGetSessionInfo(transportSession,
                                                        header->sessionId);
            if (!session || session->state != HGFS_SESSION_STATE_OPEN) {
               LOG(4, ("%s: HGFS packet with invalid session id!\n", __FUNCTION__));
               result = HGFS_ERROR_STALE_SESSION;
            }
         }
      } else {
         LOG(4, ("%s: Malformed HGFS packet received - header is too small!\n",
            __FUNCTION__));
         result = HGFS_ERROR_PROTOCOL;
      }

      if (HGFS_ERROR_SUCCESS == result) { // Passed all tests
         localInput->payload = (char *)request + header->headerSize;
         localInput->payloadSize = header->packetSize - header->headerSize;
      }
   } else {
      LOG(4, ("%s: Malformed HGFS packet received - invalid legacy opcode!\n",
              __FUNCTION__));
      result = HGFS_ERROR_PROTOCOL;
   }

   if (HGFS_ERROR_SUCCESS != result) {
      LOG(4, ("%s: Malformed HGFS packet received!\n", __FUNCTION__));
   } else if ((NULL == session) && (!localInput->v4header)) {
      session = HgfsServerTransportGetSessionInfo(transportSession,
                                                  transportSession->defaultSessionId);
      if (NULL == session) {
         /*
          * Create a new session if the default session doesn't exist.
          */
         if (!HgfsServerAllocateSession(transportSession,
                                        transportSession->channelCapabilities,
                                        &session)) {
            result = HGFS_ERROR_NOT_ENOUGH_MEMORY;
         } else {
            result = HgfsServerTransportAddSessionToList(transportSession,
                                                         session);
            if (HGFS_ERROR_SUCCESS != result) {
               LOG(4, ("%s: Could not add session to the list.\n", __FUNCTION__));
            } else {
               transportSession->defaultSessionId = session->sessionId;
               HgfsServerSessionGet(session);
            }
         }
      }
   }

   if (session) {
      session->isInactive = FALSE;
   }
   localInput->session = session;
   localInput->payloadOffset = (char *)localInput->payload -
                               (char *)localInput->metaPacket;
   *status = result;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackOpenPayloadV1 --
 *
 *    Unpack and validate payload for hgfs open request V1 to the HgfsFileOpenInfo
 *    structure that is used to pass around open request information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackOpenPayloadV1(HgfsRequestOpen *requestV1, // IN: request payload
                        size_t payloadSize,         // IN: request payload size
                        HgfsFileOpenInfo *openInfo) // IN/OUT: open info struct
{
   size_t extra;

   /* Enforced by the dispatch function. */
   if (payloadSize < sizeof *requestV1) {
      LOG(4, ("%s: Malformed HGFS packet received - payload too small\n", __FUNCTION__));
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV1;

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV1->fileName.length > extra) {
      /* The input packet is smaller than the request. */
      LOG(4, ("%s: Malformed HGFS packet received - payload too small to hold file name\n", __FUNCTION__));
      return FALSE;
   }

   /* For OpenV1 requests, we know exactly what fields we expect. */
   openInfo->mask = HGFS_OPEN_VALID_MODE |
                    HGFS_OPEN_VALID_FLAGS |
                    HGFS_OPEN_VALID_OWNER_PERMS |
                    HGFS_OPEN_VALID_FILE_NAME;
   openInfo->mode = requestV1->mode;
   openInfo->cpName = requestV1->fileName.name;
   openInfo->cpNameSize = requestV1->fileName.length;
   openInfo->flags = requestV1->flags;
   openInfo->ownerPerms = requestV1->permissions;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackOpenPayloadV2 --
 *
 *    Unpack and validate payload for hgfs open request V2 to the HgfsFileOpenInfo
 *    structure that is used to pass around open request information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackOpenPayloadV2(HgfsRequestOpenV2 *requestV2, // IN: request payload
                        size_t payloadSize,           // IN: request payload size
                        HgfsFileOpenInfo *openInfo)   // IN/OUT: open info struct
{
   size_t extra;

   /* Enforced by the dispatch function. */
   if (payloadSize < sizeof *requestV2) {
      LOG(4, ("%s: Malformed HGFS packet received - payload too small\n", __FUNCTION__));
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV2;

   if (!(requestV2->mask & HGFS_OPEN_VALID_FILE_NAME)) {
      /* We do not support open requests without a valid file name. */
      LOG(4, ("%s: Malformed HGFS packet received - invalid mask\n", __FUNCTION__));
      return FALSE;
   }

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV2->fileName.length > extra) {
      /* The input packet is smaller than the request. */
      LOG(4, ("%s: Malformed HGFS packet received - payload too small to hold file name\n", __FUNCTION__));
      return FALSE;
   }

   /*
    * Copy all the fields into our carrier struct. Some will probably be
    * garbage, but it's simpler to copy everything now and check the
    * valid bits before reading later.
    */

   openInfo->mask = requestV2->mask;
   openInfo->mode = requestV2->mode;
   openInfo->cpName = requestV2->fileName.name;
   openInfo->cpNameSize = requestV2->fileName.length;
   openInfo->flags = requestV2->flags;
   openInfo->specialPerms = requestV2->specialPerms;
   openInfo->ownerPerms = requestV2->ownerPerms;
   openInfo->groupPerms = requestV2->groupPerms;
   openInfo->otherPerms = requestV2->otherPerms;
   openInfo->attr = requestV2->attr;
   openInfo->allocationSize = requestV2->allocationSize;
   openInfo->desiredAccess = requestV2->desiredAccess;
   openInfo->shareAccess = requestV2->shareAccess;
   openInfo->desiredLock = requestV2->desiredLock;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackOpenPayloadV3 --
 *
 *    Unpack and validate payload for hgfs open request V3 to the HgfsFileOpenInfo
 *    structure that is used to pass around open request information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackOpenPayloadV3(HgfsRequestOpenV3 *requestV3, // IN: request payload
                        size_t payloadSize,           // IN: request payload size
                        HgfsFileOpenInfo *openInfo)   // IN/OUT: open info struct
{
   size_t extra;

   /* Enforced by the dispatch function. */
   if (payloadSize < sizeof *requestV3) {
      LOG(4, ("%s: Malformed HGFS packet received - payload too small\n", __FUNCTION__));
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV3;

   if (!(requestV3->mask & HGFS_OPEN_VALID_FILE_NAME)) {
      /* We do not support open requests without a valid file name. */
      LOG(4, ("%s: Malformed HGFS packet received - incorrect mask\n", __FUNCTION__));
      return FALSE;
   }

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV3->fileName.length > extra) {
      /* The input packet is smaller than the request. */
      LOG(4, ("%s: Malformed HGFS packet received - payload too small to hold file name\n", __FUNCTION__));
      return FALSE;
   }

   /*
    * Copy all the fields into our carrier struct. Some will probably be
    * garbage, but it's simpler to copy everything now and check the
    * valid bits before reading later.
    */
   openInfo->mask = requestV3->mask;
   openInfo->mode = requestV3->mode;
   openInfo->cpName = requestV3->fileName.name;
   openInfo->cpNameSize = requestV3->fileName.length;
   openInfo->caseFlags = requestV3->fileName.caseType;
   openInfo->flags = requestV3->flags;
   openInfo->specialPerms = requestV3->specialPerms;
   openInfo->ownerPerms = requestV3->ownerPerms;
   openInfo->groupPerms = requestV3->groupPerms;
   openInfo->otherPerms = requestV3->otherPerms;
   openInfo->attr = requestV3->attr;
   openInfo->allocationSize = requestV3->allocationSize;
   openInfo->desiredAccess = requestV3->desiredAccess;
   openInfo->shareAccess = requestV3->shareAccess;
   openInfo->desiredLock = requestV3->desiredLock;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackOpenRequest --
 *
 *    Unpack hgfs open request to the HgfsFileOpenInfo structure that is used
 *    to pass around open request information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackOpenRequest(void const *packet,         // IN: HGFS packet
                      size_t packetSize,          // IN: packet size
                      HgfsOp op,                  // IN: requested operation
                      HgfsFileOpenInfo *openInfo) // IN/OUT: open info structure
{
   Bool result;

   ASSERT(packet);
   ASSERT(openInfo);

   openInfo->requestType = op;
   openInfo->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (op) {
   case HGFS_OP_OPEN_V3: {
         HgfsRequestOpenV3 *requestV3 = (HgfsRequestOpenV3 *)packet;
         LOG(4, ("%s: HGFS_OP_OPEN_V3\n", __FUNCTION__));

         result = HgfsUnpackOpenPayloadV3(requestV3, packetSize, openInfo);
         break;
      }
   case HGFS_OP_OPEN_V2: {
         HgfsRequestOpenV2 *requestV2 = (HgfsRequestOpenV2 *)packet;
         LOG(4, ("%s: HGFS_OP_OPEN_V2\n", __FUNCTION__));

         result = HgfsUnpackOpenPayloadV2(requestV2, packetSize, openInfo);
         break;
      }
   case HGFS_OP_OPEN: {
         HgfsRequestOpen *requestV1 = (HgfsRequestOpen *)packet;
         LOG(4, ("%s: HGFS_OP_OPEN\n", __FUNCTION__));

         result = HgfsUnpackOpenPayloadV1(requestV1, packetSize, openInfo);
         break;
      }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   if (!result) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
   }
   return result;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackReplyHeaderV4 --
 *
 *    Pack hgfs header that corresponds an incoming packet.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackReplyHeaderV4(HgfsInternalStatus status,    // IN: reply status
                      uint32 payloadSize,           // IN: size of the reply payload
                      HgfsOp op,                    // IN: request type
                      uint64 sessionId,             // IN: session id
                      uint32 requestId,             // IN: request id
                      HgfsHeader *header)           // OUT: outgoing packet header
{
   memset(header, 0, sizeof *header);
   header->version = 1;
   header->dummy = HGFS_V4_LEGACY_OPCODE;
   header->packetSize = payloadSize + sizeof *header;
   header->headerSize = sizeof *header;
   header->requestId = requestId;
   header->op = op;
   header->status = HgfsConvertFromInternalStatus(status);
   header->flags = 0;
   header->information = status;
   header->sessionId = sessionId;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackLegacyReplyHeader --
 *
 *    Pack pre-V4 reply header.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackLegacyReplyHeader(HgfsInternalStatus status,    // IN: reply status
                          HgfsHandle id,                // IN: original packet id
                          HgfsReply *header)            // OUT: outgoing packet header
{
   memset(header, 0, sizeof *header);
   header->status = HgfsConvertFromInternalStatus(status);
   header->id = id;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackOpenReplyV3 --
 *
 *    Pack hgfs open V3 reply payload to the HgfsReplyOpenV3 structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackOpenReplyV3(HgfsFileOpenInfo *openInfo,   // IN: open info struct
                    HgfsReplyOpenV3 *reply)       // OUT: size of packet
{
   reply->file = openInfo->file;
   reply->reserved = 0;
   if (openInfo->mask & HGFS_OPEN_VALID_SERVER_LOCK) {
      reply->acquiredLock = openInfo->acquiredLock;
   } else {
      reply->acquiredLock = HGFS_LOCK_NONE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackOpenV2Reply --
 *
 *    Pack hgfs open V2 reply payload to the HgfsReplyOpenV3 structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackOpenV2Reply(HgfsFileOpenInfo *openInfo,   // IN: open info struct
                    HgfsReplyOpenV2 *reply)       // OUT: reply payload
{
   reply->file = openInfo->file;
   if (openInfo->mask & HGFS_OPEN_VALID_SERVER_LOCK) {
      reply->acquiredLock = openInfo->acquiredLock;
   } else {
      reply->acquiredLock = HGFS_LOCK_NONE;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackOpenV1Reply --
 *
 *    Pack hgfs open V1 reply payload to the HgfsReplyOpenV3 structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackOpenV1Reply(HgfsFileOpenInfo *openInfo,   // IN: open info struct
                    HgfsReplyOpen *reply)         // OUT: reply payload
{
   reply->file = openInfo->file;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackOpenReply --
 *
 *    Pack hgfs open reply to the HgfsReplyOpen{V2} structure.
 *
 * Results:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackOpenReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                  char const *packetHeader,     // IN: packet header
                  HgfsFileOpenInfo *openInfo,   // IN: open info struct
                  size_t *payloadSize,          // OUT: size of packet
                  HgfsSessionInfo *session)     // IN: Session info
{
   Bool result;

   ASSERT(openInfo);
   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (openInfo->requestType) {
   case HGFS_OP_OPEN_V3: {
      HgfsReplyOpenV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         HgfsPackOpenReplyV3(openInfo, reply);
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_OPEN_V2: {
      HgfsReplyOpenV2 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         HgfsPackOpenV2Reply(openInfo, reply);
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_OPEN: {
      HgfsReplyOpen *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                 (void **)&reply, session);
      if (result) {
         HgfsPackOpenV1Reply(openInfo, reply);
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackClosePayload --
 *
 *    Unpack hgfs close payload to get the handle which need to be closed.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackClosePayload(HgfsRequestClose *request,   // IN: payload
                       size_t payloadSize,          // IN: payload size
                       HgfsHandle* file)            // OUT: HGFS handle to close
{
   LOG(4, ("%s: HGFS_OP_CLOSE\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      *file = request->file;
      return TRUE;
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackClosePayloadV3 --
 *
 *    Unpack hgfs close payload V3 to get the handle which need to be closed.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackClosePayloadV3(HgfsRequestCloseV3 *requestV3, // IN: payload
                         size_t payloadSize,            // IN: payload size
                         HgfsHandle* file)              // OUT: HGFS handle to close
{
   LOG(4, ("%s: HGFS_OP_CLOSE_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      *file = requestV3->file;
      return TRUE;
   }
   LOG(4, ("%s: Too small HGFS packet\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCloseRequest --
 *
 *    Unpack hgfs close request to get the handle to close.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCloseRequest(void const *packet,  // IN: request packet
                       size_t packetSize,   // IN: request packet size
                       HgfsOp op,           // IN: request type
                       HgfsHandle *file)    // OUT: Handle to close
{
   ASSERT(packet);

   switch (op) {
   case HGFS_OP_CLOSE_V3: {
         HgfsRequestCloseV3 *requestV3 = (HgfsRequestCloseV3 *)packet;

         if (!HgfsUnpackClosePayloadV3(requestV3, packetSize, file)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_CLOSE: {
         HgfsRequestClose *requestV1 = (HgfsRequestClose *)packet;

         if (!HgfsUnpackClosePayload(requestV1, packetSize, file)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   default:
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackCloseReply --
 *
 *    Pack hgfs close reply to the HgfsReplyClose(V3) structure.
 *
 * Results:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackCloseReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                   char const *packetHeader,   // IN: packet header
                   HgfsOp op,                  // IN: request type
                   size_t *payloadSize,        // OUT: size of packet excluding header
                   HgfsSessionInfo *session)   // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_CLOSE_V3: {
      HgfsReplyCloseV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply consists of a reserved field only. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_CLOSE: {
      HgfsReplyClose *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                 (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchClosePayload --
 *
 *    Unpack hgfs search close payload to get the search handle which need to be closed.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchClosePayload(HgfsRequestSearchClose *request, // IN: payload
                             size_t payloadSize,              // IN: payload size
                             HgfsHandle* search)              // OUT: search to close
{
   LOG(4, ("%s: HGFS_OP_SEARCH_CLOSE\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      *search = request->search;
      return TRUE;
   }
   LOG(4, ("%s: Too small HGFS packet\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchClosePayloadV3 --
 *
 *    Unpack hgfs search close payload V3 to get the search handle which need to
 *    be closed.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchClosePayloadV3(HgfsRequestSearchCloseV3 *requestV3, // IN: payload
                               size_t payloadSize,                  // IN: payload size
                               HgfsHandle* search)                  // OUT: search
{
   LOG(4, ("%s: HGFS_OP_SEARCH_CLOSE_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      *search = requestV3->search;
      return TRUE;
   }
   LOG(4, ("%s: Too small HGFS packet\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchCloseRequest --
 *
 *    Unpack hgfs search close request to get the search handle.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchCloseRequest(void const *packet,   // IN: request packet
                             size_t packetSize,    // IN: request packet size
                             HgfsOp op,            // IN: request type
                             HgfsHandle *search)   // OUT: search to close
{
   ASSERT(packet);

   switch (op) {
   case HGFS_OP_SEARCH_CLOSE_V3: {
         HgfsRequestSearchCloseV3 *requestV3 = (HgfsRequestSearchCloseV3 *)packet;

         if (!HgfsUnpackSearchClosePayloadV3(requestV3, packetSize, search)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_SEARCH_CLOSE: {
         HgfsRequestSearchClose *requestV1 = (HgfsRequestSearchClose *)packet;

         if (!HgfsUnpackSearchClosePayload(requestV1, packetSize, search)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   default:
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchCloseReply --
 *
 *    Pack hgfs SearchClose reply into a HgfsReplySearchClose(V3) structure.
 *
 * Results:
 *    Always TRUE, except when it is called with a
 *    wrong op or insufficient output buffer (which is a programming error).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSearchCloseReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                         char const *packetHeader,   // IN: packet header
                         HgfsOp op,                  // IN: request type
                         size_t *payloadSize,        // OUT: size of packet
                         HgfsSessionInfo *session)   // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_SEARCH_CLOSE_V3: {
      HgfsReplyCloseV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply consists of only a reserved field. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_SEARCH_CLOSE: {
      HgfsReplyClose *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackFileName --
 *
 *    Unpack HgfsFileName into a pointer to a CPName and size of the name.
 *    Verifies that input buffer has enough space to hold the name.
 *
 * Results:
 *    TRUE on success, FALSE on failure (buffer too small).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackFileName(HgfsFileName *name,     // IN: file name
                   size_t maxNameSize,     // IN: space allocated for the name
                   char **cpName,          // OUT: CP name
                   size_t *cpNameSize)     // OUT: CP name size
{
   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (name->length > maxNameSize) {
      /* The input packet is smaller than the request. */
      return FALSE;
   }
   *cpName = name->name;
   *cpNameSize = name->length;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackFileNameV3 --
 *
 *    Unpack HgfsFileNameV3 into a pointer to a CPName and size of the name
 *    or into file handle.
 *    Verifies that input buffer has enough space to hold the name.
 *
 * Results:
 *    TRUE on success, FALSE on failure (buffer too small).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackFileNameV3(HgfsFileNameV3 *name,   // IN: file name
                     size_t maxNameSize,     // IN: space allocated for the name
                     Bool *useHandle,        // OUT: file name or handle returned?
                     char **cpName,          // OUT: CP name
                     size_t *cpNameSize,     // OUT: CP name size
                     HgfsHandle *file,       // OUT: HGFS file handle
                     uint32 *caseFlags)      // OUT: case-sensitivity flags
{
   /*
    * If we've been asked to reuse a handle, we don't need to look at, let
    * alone test the filename or its length.
    */
   if (name->flags & HGFS_FILE_NAME_USE_FILE_DESC) {
      *file = name->fid;
      *cpName = NULL;
      *cpNameSize = 0;
      *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
      *useHandle = TRUE;
   } else {
      /*
       * The request file name length is user-provided, so this test must be
       * carefully written to prevent wraparounds.
       */
      if (name->length > maxNameSize) {
         /* The input packet is smaller than the request */
         LOG(4, ("%s: Error unpacking file name - buffer too small\n", __FUNCTION__));
         return FALSE;
      }
      *file = HGFS_INVALID_HANDLE;
      *cpName = name->name;
      *cpNameSize = name->length;
      *caseFlags = name->caseType;
      *useHandle = FALSE;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackDeletePayloadV3 --
 *
 *    Unpack hgfs delete request V3 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which to delete. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to delete.
 *
 *    Since the structure of the get delete request packet is the same
 *    for Delete File or Directory of the protocol, code is identical for
 *    both operations.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackDeletePayloadV3(HgfsRequestDeleteV3 *requestV3, // IN: request payload
                          size_t payloadSize,             // IN: payload size
                          char **cpName,                  // OUT: cpName
                          size_t *cpNameSize,             // OUT: cpName size
                          HgfsDeleteHint *hints,          // OUT: delete hints
                          HgfsHandle *file,               // OUT: file handle
                          uint32 *caseFlags)              // OUT: case-sensitivity flags
{
   Bool result;
   Bool useHandle;

   if (payloadSize < sizeof *requestV3) {
      return FALSE;
   }

   *hints = requestV3->hints;

   result = HgfsUnpackFileNameV3(&requestV3->fileName,
                                 payloadSize - sizeof *requestV3,
                                 &useHandle,
                                 cpName,
                                 cpNameSize,
                                 file,
                                 caseFlags);
   if (useHandle) {
      *hints |= HGFS_DELETE_HINT_USE_FILE_DESC;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackDeletePayloadV2 --
 *
 *    Unpack hgfs delete request V2 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which to delete. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to delete.
 *
 *    Since the structure of the get delete request packet is the same
 *    for Delete File or Directory of the protocol, code is identical for
 *    both operations.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackDeletePayloadV2(HgfsRequestDeleteV2 *requestV2, // IN: request payload
                          size_t payloadSize,             // IN: payload size
                          char **cpName,                  // OUT: cpName
                          size_t *cpNameSize,             // OUT: cpName size
                          HgfsDeleteHint *hints,          // OUT: delete hints
                          HgfsHandle *file)               // OUT: file handle
{
   Bool result = TRUE;

   /* Enforced by the dispatch function. */
   ASSERT(payloadSize >= sizeof *requestV2);

   *file = HGFS_INVALID_HANDLE;
   *hints = requestV2->hints;

   /*
    * If we've been asked to reuse a handle, we don't need to look at, let
    * alone test the filename or its length.
    */

   if (requestV2->hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
      *file = requestV2->file;
      *cpName = NULL;
      *cpNameSize = 0;
   } else {
      result = HgfsUnpackFileName(&requestV2->fileName,
                                  payloadSize - sizeof *requestV2,
                                  cpName,
                                  cpNameSize);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackDeletePayloadV1 --
 *
 *    Unpack hgfs delete request V1 payload and initialize a corresponding
 *    file name to tell us which to delete.
 *
 *    Since the structure of the get delete request packet is the same
 *    for Delete File or Directory of the protocol, code is identical for
 *    both operations.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackDeletePayloadV1(HgfsRequestDelete *requestV1,   // IN: request payload
                          size_t payloadSize,             // IN: payload size
                          char **cpName,                  // OUT: cpName
                          size_t *cpNameSize)             // OUT: cpName size
{
   return HgfsUnpackFileName(&requestV1->fileName,
                             payloadSize - sizeof *requestV1,
                             cpName,
                             cpNameSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackDeleteRequest --
 *
 *    Unpack hgfs delete request and initialize a corresponding
 *    HgfsHandle or file name to tell us which to delete. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to delete.
 *
 *    Since the structure of the get delete request packet is the same
 *    for Delete File or Directory of the protocol, code is identical for
 *    both operations.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackDeleteRequest(void const *packet,      // IN: HGFS packet
                        size_t packetSize,       // IN: request packet size
                        HgfsOp op,               // IN: requested operation
                        char **cpName,           // OUT: cpName
                        size_t *cpNameSize,      // OUT: cpName size
                        HgfsDeleteHint *hints,   // OUT: delete hints
                        HgfsHandle *file,        // OUT: file handle
                        uint32 *caseFlags)       // OUT: case-sensitivity flags
{
   ASSERT(packet);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(hints);
   ASSERT(caseFlags);

   *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (op) {
   case HGFS_OP_DELETE_FILE_V3:
   case HGFS_OP_DELETE_DIR_V3: {
      HgfsRequestDeleteV3 *requestV3 = (HgfsRequestDeleteV3 *)packet;

      if (!HgfsUnpackDeletePayloadV3(requestV3,
                                     packetSize,
                                     cpName,
                                     cpNameSize,
                                     hints,
                                     file,
                                     caseFlags)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE_V2:
   case HGFS_OP_DELETE_DIR_V2: {
      HgfsRequestDeleteV2 *requestV2 = (HgfsRequestDeleteV2 *)packet;

      if (!HgfsUnpackDeletePayloadV2(requestV2,
                                     packetSize,
                                     cpName,
                                     cpNameSize,
                                     hints,
                                     file)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE:
   case HGFS_OP_DELETE_DIR: {
      HgfsRequestDelete *requestV1 = (HgfsRequestDelete *)packet;

      if (!HgfsUnpackDeletePayloadV1(requestV1,
                                     packetSize,
                                     cpName,
                                     cpNameSize)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }
   default:
      NOT_REACHED();
      LOG(4, ("%s: Invalid opcode\n", __FUNCTION__));
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackDeleteReply --
 *
 *    Pack hgfs delete reply.
 *    Since the structure of the delete reply packet hasn't changed in
 *    version 2 of the protocol, HgfsReplyDeleteV2 is identical to
 *    HgfsReplyDelete. So use HgfsReplyDelete type to access packetIn to
 *    keep the code simple.
 *
 * Results:
 *    TRUE if valid op version reply filled, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackDeleteReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                    char const *packetHeader,  // IN: packet header
                    HgfsOp op,                 // IN: requested operation
                    size_t *payloadSize,       // OUT: size of packet
                    HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   /* No reply payload, just header. */
   switch (op) {
   case HGFS_OP_DELETE_FILE_V3:
   case HGFS_OP_DELETE_DIR_V3: {
      HgfsReplyDeleteV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE_V2:
   case HGFS_OP_DELETE_FILE:
   case HGFS_OP_DELETE_DIR_V2:
   case HGFS_OP_DELETE_DIR: {
      HgfsReplyDelete *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                 (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      result = FALSE;
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRenamePayloadV3 --
 *
 *    Unpack hgfs rename request V3 payload and initialize a corresponding
 *    HgfsHandles or file names to tell us old and new names/handles. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to rename.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackRenamePayloadV3(HgfsRequestRenameV3 *requestV3, // IN: request payload
                          size_t payloadSize,             // IN: payload size
                          char **cpOldName,               // OUT: rename src
                          size_t *cpOldNameLen,           // OUT: rename src size
                          char **cpNewName,               // OUT: rename dst
                          size_t *cpNewNameLen,           // OUT: rename dst size
                          HgfsRenameHint *hints,          // OUT: rename hints
                          HgfsHandle *srcFile,            // OUT: src file handle
                          HgfsHandle *targetFile,         // OUT: target file handle
                          uint32 *oldCaseFlags,           // OUT: source case flags
                          uint32 *newCaseFlags)           // OUT: dest. case flags
{
   size_t extra;
   HgfsFileNameV3 *newName;
   Bool useHandle;

   LOG(4, ("%s: HGFS_OP_RENAME_V3\n", __FUNCTION__));

   if (payloadSize < sizeof *requestV3) {
      return FALSE;
   }
   extra = payloadSize - sizeof *requestV3;

   *hints = requestV3->hints;

   /*
    * Get the old and new filenames from the request.
    *
    * Getting the new filename is somewhat inconvenient, because we
    * don't know where request->newName actually starts, thanks to the
    * fact that request->oldName is of variable length. We get around
    * this by using an HgfsFileName*, assigning it to the correct address
    * just after request->oldName ends, and using that to access the
    * new name.
    */

   /*
    * If we've been asked to reuse a handle, we don't need to look at, let
    * alone test the filename or its length. This applies to the source
    * and the target.
    */
   if (!HgfsUnpackFileNameV3(&requestV3->oldName,
                             extra,
                             &useHandle,
                             cpOldName,
                             cpOldNameLen,
                             srcFile,
                             oldCaseFlags)) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
      return FALSE;
   }
   if (useHandle) {
      *hints |= HGFS_RENAME_HINT_USE_SRCFILE_DESC;
      newName = &requestV3->newName;
   } else {
      newName = (HgfsFileNameV3 *)(requestV3->oldName.name + 1 + *cpOldNameLen);
      extra -= *cpOldNameLen;
   }
   if (!HgfsUnpackFileNameV3(newName,
                             extra,
                             &useHandle,
                             cpNewName,
                             cpNewNameLen,
                             targetFile,
                             newCaseFlags)) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
      return FALSE;
   }
   if (useHandle) {
      *hints |= HGFS_RENAME_HINT_USE_TARGETFILE_DESC;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRenamePayloadV2 --
 *
 *    Unpack hgfs rename request V2 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which to delete. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to rename.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackRenamePayloadV2(HgfsRequestRenameV2 *requestV2, // IN: request payload
                          size_t payloadSize,             // IN: payload size
                          char **cpOldName,               // OUT: rename src
                          size_t *cpOldNameLen,           // OUT: rename src size
                          char **cpNewName,               // OUT: rename dst
                          size_t *cpNewNameLen,           // OUT: rename dst size
                          HgfsRenameHint *hints,          // OUT: rename hints
                          HgfsHandle *srcFile,            // OUT: src file handle
                          HgfsHandle *targetFile)         // OUT: target file handle
{
   HgfsFileName *newName;
   size_t extra;

   /* Enforced by the dispatch function. */
   if (payloadSize < sizeof *requestV2) {
      LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
      return FALSE;
   }
   extra = payloadSize - sizeof *requestV2;

   *hints = requestV2->hints;

   /*
    * If we've been asked to reuse a handle, we don't need to look at, let
    * alone test the filename or its length. This applies to the source
    * and the target.
    */

   if (*hints & HGFS_RENAME_HINT_USE_SRCFILE_DESC) {
      *srcFile = requestV2->srcFile;
      *cpOldName = NULL;
      *cpOldNameLen = 0;
   } else {
      if (!HgfsUnpackFileName(&requestV2->oldName,
                              extra,
                              cpOldName,
                              cpOldNameLen)) {
         LOG(4, ("%s: Error decoding HGFS packet - not enough room for file name\n", __FUNCTION__));
         return FALSE;
      }
      extra -= *cpOldNameLen;
   }

   if (*hints & HGFS_RENAME_HINT_USE_TARGETFILE_DESC) {
      *targetFile = requestV2->targetFile;
      *cpNewName = NULL;
      *cpNewNameLen = 0;
   } else {
      newName = (HgfsFileName *)((char *)(&requestV2->oldName + 1)
                                             + *cpOldNameLen);
      if (!HgfsUnpackFileName(newName,
                              extra,
                              cpNewName,
                              cpNewNameLen)) {
        LOG(4, ("%s: Error decoding HGFS packet - not enough room for file name\n", __FUNCTION__));
        return FALSE;
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRenamePayloadV1 --
 *
 *    Unpack hgfs rename request V1 payload and initialize a corresponding
 *    old and new file names.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackRenamePayloadV1(HgfsRequestRename *requestV1, // IN: request payload
                          size_t payloadSize,           // IN: payload size
                          char **cpOldName,             // OUT: rename src
                          size_t *cpOldNameLen,         // OUT: rename src size
                          char **cpNewName,             // OUT: rename dst
                          size_t *cpNewNameLen)         // OUT: rename dst size
{
   HgfsFileName *newName;
   uint32 extra;

   if (payloadSize < sizeof *requestV1) {
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV1;

   if (!HgfsUnpackFileName(&requestV1->oldName,
                           extra,
                           cpOldName,
                           cpOldNameLen)) {
      LOG(4, ("%s: Error decoding HGFS packet - not enough room for file name\n", __FUNCTION__));
      return FALSE;
   }

   extra -= requestV1->oldName.length;
   newName = (HgfsFileName *)((char *)(&requestV1->oldName + 1)
                              + requestV1->oldName.length);

   return HgfsUnpackFileName(newName, extra, cpNewName, cpNewNameLen);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRenameRequest --
 *
 *    Unpack hgfs rename request and initialize a corresponding
 *    HgfsHandle or file name to tell us which to rename. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to rename.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackRenameRequest(void const *packet,       // IN: HGFS packet
                        size_t packetSize,        // IN: request packet size
                        HgfsOp op,                // IN: requested operation
                        char **cpOldName,         // OUT: rename src
                        size_t *cpOldNameLen,     // OUT: rename src size
                        char **cpNewName,         // OUT: rename dst
                        size_t *cpNewNameLen,     // OUT: rename dst size
                        HgfsRenameHint *hints,    // OUT: rename hints
                        HgfsHandle *srcFile,      // OUT: src file handle
                        HgfsHandle *targetFile,   // OUT: target file handle
                        uint32 *oldCaseFlags,     // OUT: source case-sensitivity flags
                        uint32 *newCaseFlags)     // OUT: dest. case-sensitivity flags
{
   ASSERT(packet);
   ASSERT(cpOldName);
   ASSERT(cpOldNameLen);
   ASSERT(cpNewName);
   ASSERT(cpNewNameLen);
   ASSERT(srcFile);
   ASSERT(targetFile);
   ASSERT(hints);
   ASSERT(oldCaseFlags);
   ASSERT(newCaseFlags);

   /* Default values for legacy requests. */
   *oldCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *newCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;

   switch (op) {
   case HGFS_OP_RENAME_V3:
   {
      HgfsRequestRenameV3 *requestV3 = (HgfsRequestRenameV3 *)packet;

      if (!HgfsUnpackRenamePayloadV3(requestV3,
                                     packetSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen,
                                     hints,
                                     srcFile,
                                     targetFile,
                                     oldCaseFlags,
                                     newCaseFlags)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }
   case HGFS_OP_RENAME_V2:
   {
      HgfsRequestRenameV2 *requestV2 = (HgfsRequestRenameV2 *)packet;

      if (!HgfsUnpackRenamePayloadV2(requestV2,
                                     packetSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen,
                                     hints,
                                     srcFile,
                                     targetFile)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }

   case HGFS_OP_RENAME:
   {
      HgfsRequestRename *requestV1 = (HgfsRequestRename *)packet;

      if (!HgfsUnpackRenamePayloadV1(requestV1,
                                     packetSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }

   default:
      LOG(4, ("%s: Invalid opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackRenameReply --
 *
 *    Pack hgfs rename reply.
 *    Since the structure of the rename reply packet hasn't changed in
 *    version 2 of the protocol, HgfsReplyRenameV2 is identical to
 *    HgfsReplyRename. So use HgfsReplyRename type to access packetIn to
 *    keep the code simple.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackRenameReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                    char const *packetHeader,  // IN: packet header
                    HgfsOp op,                 // IN: requested operation
                    size_t *payloadSize,       // OUT: size of packet
                    HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_RENAME_V3: {
      HgfsReplyRenameV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply consists of only a reserved field. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_RENAME_V2:
   case HGFS_OP_RENAME: {
      HgfsReplyRename *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      result = FALSE;
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackGetattrPayloadV3 --
 *
 *    Unpack hgfs get attr request V3 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which file to get attributes. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to get attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackGetattrPayloadV3(HgfsRequestGetattrV3 *requestV3,// IN: request payload
                           size_t payloadSize,             // IN: payload size
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize,             // OUT: cpName size
                           HgfsAttrHint *hints,            // OUT: getattr hints
                           HgfsHandle *file,               // OUT: file handle
                           uint32 *caseFlags)              // OUT: case-sensitivity flags
{
   Bool result;
   Bool useHandle;

   if (payloadSize < sizeof *requestV3) {
      return FALSE;
   }

   *hints = requestV3->hints;

   result = HgfsUnpackFileNameV3(&requestV3->fileName,
                                 payloadSize - sizeof *requestV3,
                                 &useHandle,
                                 cpName,
                                 cpNameSize,
                                 file,
                                 caseFlags);
   if (useHandle) {
      *hints |= HGFS_ATTR_HINT_USE_FILE_DESC;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackGetattrPayloadV2 --
 *
 *    Unpack hgfs Getattr request V2 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which to get attributes. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to get attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackGetattrPayloadV2(HgfsRequestGetattrV2 *requestV2,// IN: request payload
                           size_t payloadSize,             // IN: payload size
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize,             // OUT: cpName size
                           HgfsAttrHint *hints,            // OUT: delete hints
                           HgfsHandle *file)               // OUT: file handle
{
   Bool result = TRUE;

   if (payloadSize < sizeof *requestV2) {
      return FALSE;
   }


   *file = HGFS_INVALID_HANDLE;
   *hints = requestV2->hints;

   /*
    * If we've been asked to reuse a handle, we don't need to look at, let
    * alone test the filename or its length.
    */

   if (requestV2->hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
      *file = requestV2->file;
      *cpName = NULL;
      *cpNameSize = 0;
   } else {
      result = HgfsUnpackFileName(&requestV2->fileName,
                                  payloadSize - sizeof *requestV2,
                                  cpName,
                                  cpNameSize);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackGetattrPayloadV1 --
 *
 *    Unpack hgfs getattr request V1 payload and initialize a corresponding
 *    file name to tell us which to get attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackGetattrPayloadV1(HgfsRequestGetattr *requestV1,  // IN: request payload
                           size_t payloadSize,             // IN: payload size
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize)             // OUT: cpName size
{
   return HgfsUnpackFileName(&requestV1->fileName,
                             payloadSize - sizeof *requestV1,
                             cpName,
                             cpNameSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackAttrV2 --
 *
 *    Packs attr version 2 reply structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackAttrV2(HgfsFileAttrInfo *attr,     // IN: attr stucture
              HgfsAttrV2 *attr2)          // OUT: attr in payload
{
   attr2->mask = attr->mask;
   attr2->type = attr->type;
   attr2->size = attr->size;
   attr2->creationTime = attr->creationTime;
   attr2->accessTime = attr->accessTime;
   attr2->writeTime = attr->writeTime;
   attr2->attrChangeTime = attr->attrChangeTime;
   attr2->specialPerms = attr->specialPerms;
   attr2->ownerPerms = attr->ownerPerms;
   attr2->groupPerms = attr->groupPerms;
   attr2->otherPerms = attr->otherPerms;
   attr2->flags = attr->flags;
   attr2->allocationSize = attr->allocationSize;
   attr2->userId = attr->userId;
   attr2->groupId = attr->groupId;
   attr2->hostFileId = attr->hostFileId;
   attr2->volumeId = attr->volumeId;
   attr2->effectivePerms = attr->effectivePerms;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackAttrV2 --
 *
 *    Unpacks attr version 2 reply structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsUnpackAttrV2(HgfsAttrV2 *attr2,          // IN: attr in payload
                HgfsFileAttrInfo *attr)     // OUT: attr stucture
{
   attr->mask = attr2->mask;
   attr->type = attr2->type;
   attr->size = attr2->size;
   attr->creationTime = attr2->creationTime;
   attr->accessTime = attr2->accessTime;
   attr->writeTime = attr2->writeTime;
   attr->attrChangeTime = attr2->attrChangeTime;
   attr->specialPerms = attr2->specialPerms;
   attr->ownerPerms = attr2->ownerPerms;
   attr->groupPerms = attr2->groupPerms;
   attr->otherPerms = attr2->otherPerms;
   attr->flags = attr2->flags;
   attr->allocationSize = attr2->allocationSize;
   attr->userId = attr2->userId;
   attr->groupId = attr2->groupId;
   attr->hostFileId = attr2->hostFileId;
   attr->volumeId = attr2->volumeId;
   attr->effectivePerms = attr2->effectivePerms;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInitFileAttr --
 *
 *    Initializes HgfsFileAttrInfo structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsInitFileAttr(HgfsOp op,                // IN: request type
                 HgfsFileAttrInfo *attr)   // OUT: attr stucture
{
   /* Initialize all fields with 0. */
   memset(attr, 0, sizeof *attr);

   /* Explicitly initialize fields which need it. */
   attr->requestType = op;
   attr->mask = HGFS_ATTR_VALID_NONE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackGetattrReplyPayloadV3 --
 *
 *    Packs Getattr V3 reply payload.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackGetattrReplyPayloadV3(HgfsFileAttrInfo *attr,     // IN: attr stucture
                              const char *utf8TargetName, // IN: optional target name
                              uint32 utf8TargetNameLen,   // IN: file name length
                              HgfsReplyGetattrV3 *reply) // OUT: payload
{
   LOG(4, ("%s: attr type: %u\n", __FUNCTION__, reply->attr.type));

   HgfsPackAttrV2(attr, &reply->attr);
   reply->reserved = 0;

   if (utf8TargetName) {
      memcpy(reply->symlinkTarget.name, utf8TargetName, utf8TargetNameLen);
      CPNameLite_ConvertTo(reply->symlinkTarget.name, utf8TargetNameLen,
                           DIRSEPC);
   } else {
      ASSERT(utf8TargetNameLen == 0);
   }
   reply->symlinkTarget.length = utf8TargetNameLen;
   reply->symlinkTarget.name[utf8TargetNameLen] = '\0';
   reply->symlinkTarget.flags = 0;
   reply->symlinkTarget.fid = 0;
   reply->symlinkTarget.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackGetattrReplyPayloadV2 --
 *
 *    Packs rename reply payload V2 requests.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackGetattrReplyPayloadV2(HgfsFileAttrInfo *attr,       // IN: attr stucture
                              const char *utf8TargetName,   // IN: optional target name
                              uint32 utf8TargetNameLen,     // IN: file name length
                              HgfsReplyGetattrV2 *reply)    // OUT: payload
{
   HgfsPackAttrV2(attr, &reply->attr);

   if (utf8TargetName) {
      memcpy(reply->symlinkTarget.name, utf8TargetName, utf8TargetNameLen);
      CPNameLite_ConvertTo(reply->symlinkTarget.name, utf8TargetNameLen,
                           DIRSEPC);
   } else {
      ASSERT(utf8TargetNameLen == 0);
   }
   reply->symlinkTarget.length = utf8TargetNameLen;
   reply->symlinkTarget.name[utf8TargetNameLen] = '\0';
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackGetattrReplyPayloadV1 --
 *
 *    Packs rename reply payload for V1 requests.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsPackGetattrReplyPayloadV1(HgfsFileAttrInfo *attr,       // IN: attr stucture
                              HgfsReplyGetattr *reply)      // OUT: reply info
{
   /* In GetattrV1, symlinks are treated as regular files. */
   if (attr->type == HGFS_FILE_TYPE_SYMLINK) {
      reply->attr.type = HGFS_FILE_TYPE_REGULAR;
   } else {
      reply->attr.type = attr->type;
   }

   reply->attr.size = attr->size;
   reply->attr.creationTime = attr->creationTime;
   reply->attr.accessTime = attr->accessTime;
   reply->attr.writeTime =  attr->writeTime;
   reply->attr.attrChangeTime = attr->attrChangeTime;
   reply->attr.permissions = attr->ownerPerms;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackGetattrRequest --
 *
 *    Unpack hgfs getattr request and initialize a corresponding
 *    HgfsFileAttrInfo structure that is used to pass around getattr request
 *    information.
 *
 *    Since the structure of the get attributes request packet hasn't changed
 *    in version 2 of the protocol, HgfsRequestGetattrV2 is identical to
 *    HgfsRequestGetattr. So use HgfsRequestGetattr type to access packetIn to
 *    keep the code simple.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackGetattrRequest(void const *packet,         // IN: HGFS packet
                         size_t packetSize,          // IN: request packet size
                         HgfsOp op,                  // IN request type
                         HgfsFileAttrInfo *attrInfo, // IN/OUT: getattr info
                         HgfsAttrHint *hints,        // OUT: getattr hints
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: file handle
                         uint32 *caseType)           // OUT: case-sensitivity flags
{
   ASSERT(packet);
   ASSERT(attrInfo);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseType);

   HgfsInitFileAttr(op, attrInfo);

   /* Default values for legacy requests. */
   *caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (op) {
   case HGFS_OP_GETATTR_V3: {
      HgfsRequestGetattrV3 *requestV3 = (HgfsRequestGetattrV3 *)packet;

      if (!HgfsUnpackGetattrPayloadV3(requestV3,
                                      packetSize,
                                      cpName,
                                      cpNameSize,
                                      hints,
                                      file,
                                      caseType)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      LOG(4, ("%s: HGFS_OP_GETATTR_V3: %u\n", __FUNCTION__, *caseType));
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsRequestGetattrV2 *requestV2 = (HgfsRequestGetattrV2 *)packet;

      if (!HgfsUnpackGetattrPayloadV2(requestV2,
                                      packetSize,
                                      cpName,
                                      cpNameSize,
                                      hints,
                                      file)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsRequestGetattr *requestV1 = (HgfsRequestGetattr *)packet;

      if (!HgfsUnpackGetattrPayloadV1(requestV1, packetSize, cpName, cpNameSize)) {
         LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
         return FALSE;
      }
      break;
   }

   default:
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackGetattrReply --
 *
 *    Pack hgfs getattr reply to the HgfsReplyGetattr structure.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackGetattrReply(HgfsPacket *packet,         // IN/OUT: Hgfs Packet
                     char const *packetHeader,   // IN: packet header
                     HgfsFileAttrInfo *attr,     // IN: attr stucture
                     const char *utf8TargetName, // IN: optional target name
                     uint32 utf8TargetNameLen,   // IN: file name length
                     size_t *payloadSize,        // OUT: size of packet
                     HgfsSessionInfo *session)   // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (attr->requestType) {
   case HGFS_OP_GETATTR_V3: {
      HgfsReplyGetattrV3 *reply;

      *payloadSize = sizeof *reply + utf8TargetNameLen;
      result = HgfsAllocInitReply(packet, packetHeader, *payloadSize,
                                  (void **)&reply, session);
      if (result) {
         HgfsPackGetattrReplyPayloadV3(attr, utf8TargetName, utf8TargetNameLen, reply);
      }
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsReplyGetattrV2 *reply;
      *payloadSize = sizeof *reply + utf8TargetNameLen;

      result = HgfsAllocInitReply(packet, packetHeader, *payloadSize,
                                  (void **)&reply, session);
      if (result) {
         HgfsPackGetattrReplyPayloadV2(attr,
                                       utf8TargetName,
                                       utf8TargetNameLen,
                                       reply);
      }
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsReplyGetattr *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         HgfsPackGetattrReplyPayloadV1(attr, reply);
         *payloadSize = sizeof *reply;
      }
      break;
   }

   default:
      LOG(4, ("%s: Invalid GetAttr op.\n", __FUNCTION__));
      NOT_REACHED();

      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyHeaderV4 --
 *
 *    Packs SearchRead V4 reply header part for all entry records returned.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyHeaderV4(HgfsSearchReadInfo *info,     // IN: reply info
                                HgfsReplySearchReadV4 *reply, // OUT: payload
                                size_t *headerSize)           // OUT: size written
{
   reply->numberEntriesReturned = info->numberRecordsWritten;
   reply->offsetToContinue = info->currentIndex;
   reply->flags = info->replyFlags;
   reply->reserved = 0;

   *headerSize = offsetof(HgfsReplySearchReadV4, entries);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyRecordV4 --
 *
 *    Packs SearchRead V4 reply record.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyRecordV4(HgfsSearchReadEntry *entry,        // IN: entry info
                                HgfsDirEntryV4 *replylastEntry,    // IN/OUT: payload
                                HgfsDirEntryV4 *replyCurrentEntry) // OUT: reply buffer for dirent
{
   HgfsFileAttrInfo *attr = &entry->attr;

   memset(replyCurrentEntry, 0, sizeof *replyCurrentEntry);

   if (NULL != replylastEntry) {
      replylastEntry->nextEntryOffset = ((char*)replyCurrentEntry -
                                         (char*)replylastEntry);
   }

   /* Set the valid data mask for the entry. */
   replyCurrentEntry->mask = entry->mask;

   if (0 != (entry->mask & HGFS_SEARCH_READ_NAME)) {

      replyCurrentEntry->nextEntryOffset = 0;
      replyCurrentEntry->fileIndex = entry->fileIndex;

      if (0 != (replyCurrentEntry->mask & HGFS_SEARCH_READ_FILE_NODE_TYPE)) {
         replyCurrentEntry->fileType = attr->type;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_FILE_SIZE)) {
         replyCurrentEntry->fileSize = attr->size;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_ALLOCATION_SIZE)) {
         replyCurrentEntry->allocationSize = attr->allocationSize;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_TIME_STAMP)) {
         replyCurrentEntry->creationTime = attr->creationTime;
         replyCurrentEntry->accessTime = attr->accessTime;
         replyCurrentEntry->writeTime = attr->writeTime;
         replyCurrentEntry->attrChangeTime = attr->attrChangeTime;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_FILE_ATTRIBUTES)) {
         replyCurrentEntry->attrFlags = attr->flags;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_FILE_ID)) {
         replyCurrentEntry->hostFileId = attr->hostFileId;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_EA_SIZE)) {
         replyCurrentEntry->eaSize = attr->eaSize;
      }
      if (0 != (entry->mask & HGFS_SEARCH_READ_REPARSE_TAG)) {
         replyCurrentEntry->reparseTag = attr->reparseTag;
      }

      if (0 != (entry->mask & HGFS_SEARCH_READ_SHORT_NAME)) {
         ASSERT(attr->shortName.length > 0);
         memcpy(replyCurrentEntry->shortName.name,
                attr->shortName.name,
                attr->shortName.length);
         replyCurrentEntry->shortName.length = attr->shortName.length;
      }

      memcpy(replyCurrentEntry->fileName.name, entry->name, entry->nameLength);
      replyCurrentEntry->fileName.name[entry->nameLength] = 0;
      replyCurrentEntry->fileName.length = entry->nameLength;

      replyCurrentEntry->reserved = 0;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyHeaderV3 --
 *
 *    Packs SearchRead V3 reply record.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyHeaderV3(HgfsSearchReadInfo *info,     // IN: reply info
                                HgfsReplySearchReadV3 *reply, // OUT: payload
                                size_t *headerSize)           // OUT: size written
{
   ASSERT(info->numberRecordsWritten <= 1 &&
          0 != (info->flags & HGFS_SEARCH_READ_SINGLE_ENTRY));
   reply->count = info->numberRecordsWritten;
   reply->reserved = 0;
   /*
    * Previous shipping tools expect to account for a whole reply,
    * which is not strictly correct, but we are stuck with it.
    */
   *headerSize = sizeof *reply;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyRecordV3 --
 *
 *    Packs SearchRead V3 reply record.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyRecordV3(HgfsFileAttrInfo *attr,       // IN: attr stucture
                                const char *utf8Name,         // IN: file name
                                uint32 utf8NameLen,           // IN: file name length
                                HgfsDirEntry *replyDirent)    // OUT: reply buffer for dirent
{
   replyDirent->fileName.length = (uint32)utf8NameLen;
   replyDirent->fileName.flags = 0;
   replyDirent->fileName.fid = 0;
   replyDirent->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   replyDirent->nextEntry = 0;

   if (utf8NameLen != 0) {
      memcpy(replyDirent->fileName.name, utf8Name, utf8NameLen);
      replyDirent->fileName.name[utf8NameLen] = 0;

      HgfsPackAttrV2(attr, &replyDirent->attr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyHeaderV2 --
 *
 *    Packs SearchRead V2 reply header (common) part for all records.
 *    V2 replies only contain a single record, so there is nothing to do here.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyHeaderV2(HgfsSearchReadInfo *info,     // IN: unused
                                HgfsReplySearchReadV2 *reply, // OUT: unused
                                size_t *headerSize)           // OUT: size written
{
   /* The header has already been accounted for. */
   *headerSize = sizeof *reply;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyRecordV2 --
 *
 *    Packs SearchRead V2 reply record.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyRecordV2(HgfsFileAttrInfo *attr,       // IN: attr stucture
                                const char *utf8Name,         // IN: file name
                                uint32 utf8NameLen,           // IN: file name length
                                HgfsReplySearchReadV2 *reply) // OUT: reply buffer
{
   reply->fileName.length = (uint32)utf8NameLen;

   if (utf8NameLen != 0) {
      memcpy(reply->fileName.name, utf8Name, utf8NameLen);
      reply->fileName.name[utf8NameLen] = 0;
      HgfsPackAttrV2(attr, &reply->attr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyHeaderV1 --
 *
 *    Packs SearchRead V1 reply header (common) part for all records.
 *    V1 replies only contain a single record, so there is nothing to do here.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyHeaderV1(HgfsSearchReadInfo *info,   // IN: unused
                                HgfsReplySearchRead *reply, // OUT: unused
                                size_t *headerSize)         // OUT: size written
{
   /* The header has already been accounted for. */
   *headerSize = sizeof *reply;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyRecordV1 --
 *
 *    Packs SearchRead V1 reply record.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSearchReadReplyRecordV1(HgfsFileAttrInfo *attr,     // IN: attr stucture
                                const char *utf8Name,       // IN: file name
                                uint32 utf8NameLen,         // IN: file name length
                                HgfsReplySearchRead *reply) // OUT: reply buffer
{
   reply->fileName.length = (uint32)utf8NameLen;

   if (utf8NameLen != 0) {
      memcpy(reply->fileName.name, utf8Name, utf8NameLen);
      reply->fileName.name[utf8NameLen] = 0;

      /* In SearchReadV1, symlinks are treated as regular files. */
      if (attr->type == HGFS_FILE_TYPE_SYMLINK) {
         reply->attr.type = HGFS_FILE_TYPE_REGULAR;
      } else {
         reply->attr.type = attr->type;
      }
      reply->attr.size = attr->size;
      reply->attr.creationTime = attr->creationTime;
      reply->attr.accessTime = attr->accessTime;
      reply->attr.writeTime =  attr->writeTime;
      reply->attr.attrChangeTime = attr->attrChangeTime;
      reply->attr.permissions = attr->ownerPerms;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchReadRequest --
 *
 *    Unpack hgfs search read request and initialize a corresponding
 *    HgfsFileAttrInfo structure that is used to pass around attribute
 *    information.
 *
 *    Since the structure of the search read request packet hasn't changed in
 *    version 2 of the protocol, HgfsRequestSearchReadV2 is identical to
 *    HgfsRequestSearchRead. So use HgfsRequestSearchRead type to access
 *    packetIn to keep the code simple.
 *
 * Results:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchReadRequest(const void *packet,           // IN: request packet
                            size_t packetSize,            // IN: packet size
                            HgfsOp op,                    // IN: reqest type
                            HgfsSearchReadInfo *info,     // OUT: search info
                            size_t *baseReplySize,        // OUT: op base reply size
                            size_t *inlineReplyDataSize,  // OUT: size of inline reply data
                            HgfsHandle *hgfsSearchHandle) // OUT: hgfs search handle
{
   Bool result = TRUE;
   uint32 *startIndex;
   HgfsSearchReadMask *mask;
   HgfsSearchReadFlags *flags;
   size_t *replyPayloadSize;

   ASSERT(packet);
   ASSERT(info);
   ASSERT(baseReplySize);
   ASSERT(inlineReplyDataSize);
   ASSERT(hgfsSearchHandle);

   info->requestType = op;
   info->searchPattern = NULL;
   startIndex = &info->startIndex;
   replyPayloadSize = &info->payloadSize;
   mask = &info->requestedMask;
   flags = &info->flags;
   *mask = 0;
   *flags = 0;

   switch (op) {
   case HGFS_OP_SEARCH_READ_V4: {
      HgfsRequestSearchReadV4 *request = (HgfsRequestSearchReadV4 *)packet;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *request);

      if (0 != (request->flags & HGFS_SEARCH_READ_FID_OPEN_V4)) {
         /*
          * XXX - When this is implemented, the handle will get us a node,
          * (of directory type) and then with the node, we can look up a
          * search handle, if the data is cached in the search array.
          */
         NOT_IMPLEMENTED();
      }

      *hgfsSearchHandle = request->fid;
      *startIndex = request->restartIndex;
      *mask = request->mask;
      *flags = request->flags;
      *baseReplySize = offsetof(HgfsReplySearchReadV4, entries);
      *replyPayloadSize = request->replyDirEntryMaxSize;
      *inlineReplyDataSize = 0;
      ASSERT(*replyPayloadSize > 0);

      LOG(4, ("%s: HGFS_OP_SEARCH_READ_V4\n", __FUNCTION__));
      break;
   }

   case HGFS_OP_SEARCH_READ_V3: {
      HgfsRequestSearchReadV3 *request = (HgfsRequestSearchReadV3 *)packet;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *request);

      *hgfsSearchHandle = request->search;
      *startIndex = request->offset;
      *flags = HGFS_SEARCH_READ_SINGLE_ENTRY;
      *mask = (HGFS_SEARCH_READ_FILE_NODE_TYPE |
               HGFS_SEARCH_READ_NAME |
               HGFS_SEARCH_READ_FILE_SIZE |
               HGFS_SEARCH_READ_TIME_STAMP |
               HGFS_SEARCH_READ_FILE_ATTRIBUTES |
               HGFS_SEARCH_READ_FILE_ID);
      *baseReplySize = offsetof(HgfsReplySearchReadV3, payload);
      *replyPayloadSize = HGFS_PACKET_MAX - *baseReplySize;
      *inlineReplyDataSize = *replyPayloadSize;

      LOG(4, ("%s: HGFS_OP_SEARCH_READ_V3\n", __FUNCTION__));
      break;
   }

   case HGFS_OP_SEARCH_READ_V2:
   /*
    * Currently, the HgfsRequestSearchReadV2 is the same as
    * HgfsRequestSearchRead, so drop through.
    */
   case HGFS_OP_SEARCH_READ: {
      HgfsRequestSearchRead *request = (HgfsRequestSearchRead *)packet;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *request);

      *hgfsSearchHandle = request->search;
      *startIndex = request->offset;
      *flags = HGFS_SEARCH_READ_SINGLE_ENTRY;
      *mask = (HGFS_SEARCH_READ_FILE_NODE_TYPE |
               HGFS_SEARCH_READ_NAME |
               HGFS_SEARCH_READ_FILE_SIZE |
               HGFS_SEARCH_READ_TIME_STAMP |
               HGFS_SEARCH_READ_FILE_ATTRIBUTES);
      *baseReplySize = 0;
      *replyPayloadSize = HGFS_PACKET_MAX;
      *inlineReplyDataSize = *replyPayloadSize;
      break;
   }

   default:
      /* Should never occur. */
      NOT_REACHED();
      result = FALSE;
      Log("%s: ERROR Invalid OP %u\n", __FUNCTION__, op);
      break;
   }

   ASSERT(result);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyRecord --
 *
 *    Pack hgfs search read reply record to the current entry record.
 *    If the last record is not NULL then update its offset to the current
 *    entry field.
 *
 * Results:
 *    TRUE on success and number of bytes written in replyRecordSize.
 *    FALSE on failure, nothing written.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSearchReadReplyRecord(HgfsOp requestType,           // IN: search read request
                              HgfsSearchReadEntry *entry,   // IN: entry info
                              size_t bytesRemaining,        // IN: space in bytes for record
                              void *lastSearchReadRecord,   // IN/OUT: last packed entry
                              void *currentSearchReadRecord,// OUT: currrent entry to pack
                              size_t *replyRecordSize)      // OUT: size of packet
{
   Bool result = TRUE;
   size_t recordSize = 0;

   switch (requestType) {
   case HGFS_OP_SEARCH_READ_V4: {
      HgfsDirEntryV4 *replyCurrentEntry = currentSearchReadRecord;
      HgfsDirEntryV4 *replyLastEntry = lastSearchReadRecord;

      /* Skip the final empty record, it is not needed for V4.*/
      if (0 == entry->nameLength) {
         break;
      }

      recordSize = offsetof(HgfsDirEntryV4, fileName.name) +
                   entry->nameLength + 1;

      if (recordSize > bytesRemaining) {
         result = FALSE;
         break;
      }

      HgfsPackSearchReadReplyRecordV4(entry,
                                      replyLastEntry,
                                      replyCurrentEntry);
      break;
   }

   case HGFS_OP_SEARCH_READ_V3: {
      HgfsDirEntry *replyCurrentEntry = currentSearchReadRecord;

      /*
       * Previous shipping tools expect to account for a whole reply,
       * which is not strictly correct, it should be using
       * offsetof(HgfsDirEntry, fileName.name) + entry->nameLength + 1
       * but we are stuck with it.
       */
      recordSize = sizeof *replyCurrentEntry + entry->nameLength;

      if (recordSize > bytesRemaining) {
         result = FALSE;
         break;
      }

      HgfsPackSearchReadReplyRecordV3(&entry->attr,
                                      entry->name,
                                      entry->nameLength,
                                      replyCurrentEntry);
      break;
   }

   case HGFS_OP_SEARCH_READ_V2: {
      HgfsReplySearchReadV2 *replyV2 = currentSearchReadRecord;

      /* We have already accounted for the fixed part of the record. */
      recordSize = entry->nameLength;

      if (recordSize > bytesRemaining) {
         result = FALSE;
         break;
      }

      HgfsPackSearchReadReplyRecordV2(&entry->attr,
                                      entry->name,
                                      entry->nameLength,
                                      replyV2);
      break;
   }

   case HGFS_OP_SEARCH_READ: {
      HgfsReplySearchRead *replyV1 = currentSearchReadRecord;

      /* We have already accounted for the fixed part of the record. */
      recordSize = entry->nameLength;

      if (recordSize > bytesRemaining) {
         result = FALSE;
         break;
      }

      HgfsPackSearchReadReplyRecordV1(&entry->attr,
                                      entry->name,
                                      entry->nameLength,
                                      replyV1);
      break;
   }

   default: {
      Log("%s: Invalid SearchRead Op.", __FUNCTION__);
      NOT_REACHED();
      result = FALSE;
   }
   }

   if (result) {
      *replyRecordSize = recordSize;
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyHeader --
 *
 *    Pack hgfs search read reply header (common) part to all the
 *    entries returned in the search read reply.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSearchReadReplyHeader(HgfsSearchReadInfo *info,    // IN: request info
                              size_t *payloadSize)         // OUT: size of packet
{
   Bool result = TRUE;

   *payloadSize = 0;

   switch (info->requestType) {
   case HGFS_OP_SEARCH_READ_V4: {
      HgfsReplySearchReadV4 *reply = info->reply;

      HgfsPackSearchReadReplyHeaderV4(info, reply, payloadSize);
      break;
   }

   case HGFS_OP_SEARCH_READ_V3: {
      HgfsReplySearchReadV3 *reply = info->reply;

      HgfsPackSearchReadReplyHeaderV3(info, reply, payloadSize);
      break;
   }

   case HGFS_OP_SEARCH_READ_V2: {
      HgfsReplySearchReadV2 *reply = info->reply;

      HgfsPackSearchReadReplyHeaderV2(info, reply, payloadSize);
      break;
   }

   case HGFS_OP_SEARCH_READ: {
      HgfsReplySearchRead *reply = info->reply;

      HgfsPackSearchReadReplyHeaderV1(info, reply, payloadSize);
      break;
   }

   default: {
      LOG(4, ("%s: Invalid SearchRead Op.", __FUNCTION__));
      NOT_REACHED();
      result = FALSE;
   }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetattrPayloadV3 --
 *
 *    Unpack hgfs set attr request V3 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which file to set attributes. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to set attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSetattrPayloadV3(HgfsRequestSetattrV3 *requestV3,// IN: request payload
                           size_t payloadSize,             // IN: payload size
                           HgfsFileAttrInfo *attr,         // OUT: setattr info
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize,             // OUT: cpName size
                           HgfsAttrHint *hints,            // OUT: getattr hints
                           HgfsHandle *file,               // OUT: file handle
                           uint32 *caseFlags)              // OUT: case-sensitivity flags
{
   Bool result;
   Bool useHandle;

   if (payloadSize < sizeof *requestV3) {
      return FALSE;
   }

   *hints = requestV3->hints;

   HgfsUnpackAttrV2(&requestV3->attr, attr);

   result = HgfsUnpackFileNameV3(&requestV3->fileName,
                                 payloadSize - sizeof *requestV3,
                                 &useHandle,
                                 cpName,
                                 cpNameSize,
                                 file,
                                 caseFlags);
   if (useHandle) {
      *hints |= HGFS_ATTR_HINT_USE_FILE_DESC;
   }

   LOG(4, ("%s: unpacking HGFS_OP_SETATTR_V3, %u\n", __FUNCTION__,
       *caseFlags));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetattrPayloadV2 --
 *
 *    Unpack hgfs Setattr request V2 payload and initialize a corresponding
 *    HgfsHandle or file name to tell us which to set attributes. Hints
 *    holds flags to specify a handle or name for the file or
 *    directory to set attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSetattrPayloadV2(HgfsRequestSetattrV2 *requestV2,// IN: request payload
                           size_t payloadSize,             // IN: payload size
                           HgfsFileAttrInfo *attr,         // OUT: setattr info
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize,             // OUT: cpName size
                           HgfsAttrHint *hints,            // OUT: delete hints
                           HgfsHandle *file)               // OUT: file handle
{
   Bool result = TRUE;

   /* Enforced by the dispatch function. */
   if (payloadSize < sizeof *requestV2) {
      return FALSE;
   }

   LOG(4, ("%s: unpacking HGFS_OP_SETATTR_V2\n", __FUNCTION__));

   *file = HGFS_INVALID_HANDLE;
   *hints = requestV2->hints;

   HgfsUnpackAttrV2(&requestV2->attr, attr);

   if (requestV2->hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
      *file = requestV2->file;
      *cpName = NULL;
      *cpNameSize = 0;
   } else {
      result = HgfsUnpackFileName(&requestV2->fileName,
                                  payloadSize - sizeof *requestV2,
                                  cpName,
                                  cpNameSize);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetattrPayloadV1 --
 *
 *    Unpack hgfs setattr request V1 payload and initialize a corresponding
 *    file name to tell us which to set attributes.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSetattrPayloadV1(HgfsRequestSetattr *requestV1,  // IN: request payload
                           size_t payloadSize,             // IN: payload size
                           HgfsFileAttrInfo *attr,         // OUT: setattr info
                           char **cpName,                  // OUT: cpName
                           size_t *cpNameSize,             // OUT: cpName size
                           HgfsAttrHint *hints)            // OUT: setattr hints
{
   LOG(4, ("%s: unpacking HGFS_OP_SETATTR\n", __FUNCTION__));

   attr->mask = 0;
   attr->mask |= requestV1->update & HGFS_ATTR_SIZE ? HGFS_ATTR_VALID_SIZE : 0;
   attr->mask |= requestV1->update & HGFS_ATTR_CREATE_TIME ?
                                               HGFS_ATTR_VALID_CREATE_TIME : 0;
   attr->mask |= requestV1->update & HGFS_ATTR_ACCESS_TIME ?
                                               HGFS_ATTR_VALID_ACCESS_TIME : 0;
   attr->mask |= requestV1->update & HGFS_ATTR_WRITE_TIME ?
                                               HGFS_ATTR_VALID_WRITE_TIME : 0;
   attr->mask |= requestV1->update & HGFS_ATTR_CHANGE_TIME ?
                                               HGFS_ATTR_VALID_CHANGE_TIME : 0;
   attr->mask |= requestV1->update & HGFS_ATTR_PERMISSIONS ?
                                               HGFS_ATTR_VALID_OWNER_PERMS : 0;
   *hints     |= requestV1->update & HGFS_ATTR_ACCESS_TIME_SET ?
                                               HGFS_ATTR_HINT_SET_ACCESS_TIME : 0;
   *hints     |= requestV1->update & HGFS_ATTR_WRITE_TIME_SET ?
                                               HGFS_ATTR_HINT_SET_WRITE_TIME : 0;

   attr->type = requestV1->attr.type;
   attr->size = requestV1->attr.size;
   attr->creationTime = requestV1->attr.creationTime;
   attr->accessTime = requestV1->attr.accessTime;
   attr->writeTime = requestV1->attr.writeTime;
   attr->attrChangeTime = requestV1->attr.attrChangeTime;
   attr->ownerPerms = requestV1->attr.permissions;
   return HgfsUnpackFileName(&requestV1->fileName,
                             payloadSize - sizeof *requestV1,
                             cpName,
                             cpNameSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetattrRequest --
 *
 *    Unpack hgfs setattr request and initialize a corresponding
 *    HgfsFileAttrInfo structure that is used to pass around setattr request
 *    information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSetattrRequest(void const *packet,       // IN: HGFS packet
                         size_t packetSize,        // IN: request packet size
                         HgfsOp op,                // IN: request type
                         HgfsFileAttrInfo *attr,   // OUT: setattr info
                         HgfsAttrHint *hints,      // OUT: setattr hints
                         char **cpName,            // OUT: cpName
                         size_t *cpNameSize,       // OUT: cpName size
                         HgfsHandle *file,         // OUT: server file ID
                         uint32 *caseType)         // OUT: case-sensitivity flags
{
   ASSERT(packet);
   ASSERT(attr);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseType);

   attr->requestType = op;

   /* Default values for legacy requests. */
   *caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (op) {
   case HGFS_OP_SETATTR_V3:
      {
         HgfsRequestSetattrV3 *requestV3 = (HgfsRequestSetattrV3 *)packet;
         if (!HgfsUnpackSetattrPayloadV3(requestV3,
                                         packetSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints,
                                         file,
                                         caseType)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }

   case HGFS_OP_SETATTR_V2:
      {
         HgfsRequestSetattrV2 *requestV2 = (HgfsRequestSetattrV2 *)packet;
         if (!HgfsUnpackSetattrPayloadV2(requestV2,
                                         packetSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints,
                                         file)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_SETATTR:
      {
         HgfsRequestSetattr *requestV1 = (HgfsRequestSetattr *)packet;
         if (!HgfsUnpackSetattrPayloadV1(requestV1,
                                         packetSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSetattrReply --
 *
 *    Pack hgfs setattr reply.
 *    Since the structure of the set attributes reply packet hasn't changed in
 *    version 2 of the protocol, HgfsReplySetattrV2 is identical to
 *    HgfsReplySetattr. So use HgfsReplySetattr type to access packetIn to
 *    keep the code simple.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSetattrReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                     char const *packetHeader,  // IN: packet header
                     HgfsOp op,                 // IN: request type
                     size_t *payloadSize,       // OUT: size of packet
                     HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_SETATTR_V3: {
      HgfsReplySetattrV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply consists of only a reserved field. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_SETATTR_V2:
   case HGFS_OP_SETATTR: {
      HgfsReplySetattr *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateDirPayloadV3 --
 *
 *    Unpack hgfs create directory request V3 payload and initialize a corresponding
 *    file name to tell us which directory to create.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateDirPayloadV3(HgfsRequestCreateDirV3 *requestV3, // IN: request payload
                             size_t payloadSize,                // IN: payload size
                             HgfsCreateDirInfo *info)           // IN/OUT: info struct
{
   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */

   LOG(4, ("%s: HGFS_OP_CREATE_DIR_V3\n", __FUNCTION__));
   ASSERT(payloadSize >= sizeof *requestV3);
   if (requestV3->fileName.length > payloadSize - sizeof *requestV3) {
      /* The input packet is smaller than the request. */
      return FALSE;
   }
   if (!(requestV3->mask & HGFS_CREATE_DIR_VALID_FILE_NAME)) {
      /* We do not support requests without a valid file name. */
      LOG(4, ("%s: Incorrect mask %x\n", __FUNCTION__, (uint32)requestV3->mask));
      return FALSE;
   }

   /*
    * Copy all the fields into our carrier struct. Some will probably be
    * garbage, but it's simpler to copy everything now and check the
    * valid bits before reading later.
    */

   info->mask = requestV3->mask;
   info->cpName = requestV3->fileName.name;
   info->cpNameSize = requestV3->fileName.length;
   info->caseFlags = requestV3->fileName.caseType;
   info->specialPerms = requestV3->specialPerms;
   info->fileAttr = requestV3->fileAttr;
   info->ownerPerms = requestV3->ownerPerms;
   info->groupPerms = requestV3->groupPerms;
   info->otherPerms = requestV3->otherPerms;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateDirPayloadV2 --
 *
 *    Unpack hgfs create directory request V2 payload and initialize a corresponding
 *    file name to tell us which directory to create.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateDirPayloadV2(HgfsRequestCreateDirV2 *requestV2, // IN: request payload
                             size_t payloadSize,                // IN: payload size
                             HgfsCreateDirInfo *info)           // IN/OUT: info struct
{
   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */

   LOG(4, ("%s: HGFS_OP_CREATE_DIR_V2\n", __FUNCTION__));
   ASSERT(payloadSize >= sizeof *requestV2);
   if (requestV2->fileName.length > payloadSize - sizeof *requestV2) {
      /* The input packet is smaller than the request. */
      return FALSE;
   }
   if (!(requestV2->mask & HGFS_CREATE_DIR_VALID_FILE_NAME)) {
      /* We do not support requests without a valid file name. */
      LOG(4, ("%s: Incorrect mask %x\n", __FUNCTION__, (uint32)requestV2->mask));
      return FALSE;
   }

   /*
    * Copy all the fields into our carrier struct. Some will probably be
    * garbage, but it's simpler to copy everything now and check the
    * valid bits before reading later.
    */

   info->mask = requestV2->mask;
   info->cpName = requestV2->fileName.name;
   info->cpNameSize = requestV2->fileName.length;
   info->specialPerms = requestV2->specialPerms;
   info->ownerPerms = requestV2->ownerPerms;
   info->groupPerms = requestV2->groupPerms;
   info->otherPerms = requestV2->otherPerms;
   info->fileAttr = 0;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateDirPayloadV1 --
 *
 *    Unpack hgfs create directory request V1 payload and initialize a corresponding
 *    file name to tell us which directory to create.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateDirPayloadV1(HgfsRequestCreateDir *requestV1, // IN: request payload
                             size_t payloadSize,              // IN: payload size
                             HgfsCreateDirInfo *info)         // IN/OUT: info struct
{
   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */

   LOG(4, ("%s: HGFS_OP_CREATE_DIR_V1\n", __FUNCTION__));
   ASSERT(payloadSize >= sizeof *requestV1);
   if (requestV1->fileName.length > payloadSize - sizeof *requestV1) {
      /* The input packet is smaller than the request. */
      LOG(4, ("%s: HGFS packet too small for the file name\n", __FUNCTION__));
      return FALSE;
   }

   /* For CreateDirV1 requests, we know exactly what fields we expect. */
   info->mask = HGFS_CREATE_DIR_VALID_OWNER_PERMS | HGFS_CREATE_DIR_VALID_FILE_NAME;
   info->cpName = requestV1->fileName.name;
   info->cpNameSize = requestV1->fileName.length;
   info->ownerPerms = requestV1->permissions;
   info->fileAttr = 0;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateDirRequest --
 *
 *    Unpack hgfs CreateDir request and initialize a corresponding
 *    HgfsCreateDirInfo structure that is used to pass around CreateDir request
 *    information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateDirRequest(void const *packet,      // IN: incoming packet
                           size_t packetSize,       // IN: size of packet
                           HgfsOp op,               // IN: request type
                           HgfsCreateDirInfo *info) // IN/OUT: info struct
{
   ASSERT(packet);
   ASSERT(info);

   info->requestType = op;
   /* Default value for legacy requests. */
   info->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (op) {
   case HGFS_OP_CREATE_DIR_V3:
      {
         HgfsRequestCreateDirV3 *requestV3 = (HgfsRequestCreateDirV3 *)packet;
         if (!HgfsUnpackCreateDirPayloadV3(requestV3,
                                           packetSize,
                                           info)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }

   case HGFS_OP_CREATE_DIR_V2:
      {
         HgfsRequestCreateDirV2 *requestV2 = (HgfsRequestCreateDirV2 *)packet;
         if (!HgfsUnpackCreateDirPayloadV2(requestV2,
                                           packetSize,
                                           info)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_CREATE_DIR:
      {
         HgfsRequestCreateDir *requestV1 = (HgfsRequestCreateDir *)packet;
         if (!HgfsUnpackCreateDirPayloadV1(requestV1,
                                           packetSize,
                                           info)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackCreateDirReply --
 *
 *    Pack hgfs CreateDir reply.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackCreateDirReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                       char const *packetHeader,  // IN: packet header
                       HgfsOp op,                 // IN: request type
                       size_t *payloadSize,        // OUT: size of packet
                       HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_CREATE_DIR_V3: {
      HgfsReplyCreateDirV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply consists of only a reserved field. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_CREATE_DIR_V2: {
      HgfsReplyCreateDirV2 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_CREATE_DIR: {
      HgfsReplyCreateDir *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWriteWin32StreamPayloadV3 --
 *
 *    Unpack hgfs write stream request V3 payload and initialize a corresponding
 *    file name to tell us which directory to create.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWriteWin32StreamPayloadV3(HgfsRequestWriteWin32StreamV3 *requestV3, // IN:
                                    size_t payloadSize,                       // IN:
                                    HgfsHandle *file,                         // OUT:
                                    char **data,                              // OUT:
                                    size_t *dataSize,                         // OUT:
                                    Bool *doSecurity)                         // OUT:
{
   LOG(4, ("%s: HGFS_OP_WRITE_WIN32_STREAM_V3\n", __FUNCTION__));
   if (payloadSize < sizeof *requestV3) {
      LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
      return FALSE;
   }

   if (payloadSize >= requestV3->requiredSize + sizeof *requestV3) {
      *file = requestV3->file;
      *data = requestV3->payload;
      *dataSize = requestV3->requiredSize;
      *doSecurity = (requestV3->flags & HGFS_WIN32_STREAM_IGNORE_SECURITY) == 0;
      return TRUE;
   }

   LOG(4, ("%s: HGFS packet too small - user data do not fit\n", __FUNCTION__));
   return FALSE;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWriteWin32StreamRequest --
 *
 *    Unpack hgfs SendFileUsingReader request. Returns file to write to, data
 *    and whether to restore the security stream.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWriteWin32StreamRequest(void const *packet, // IN: incoming packet
                                  size_t packetSize,  // IN: size of packet
                                  HgfsOp op,          // IN: request type
                                  HgfsHandle *file,   // OUT: file to write to
                                  char **data,        // OUT: data to write
                                  size_t *dataSize,   // OUT: size of data
                                  Bool *doSecurity)   // OUT: restore sec.str.
{
   ASSERT(packet);
   ASSERT(file);
   ASSERT(data);
   ASSERT(dataSize);
   ASSERT(doSecurity);

   if (op != HGFS_OP_WRITE_WIN32_STREAM_V3) {
      /* The only supported version for the moment is V3. */
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return HgfsUnpackWriteWin32StreamPayloadV3((HgfsRequestWriteWin32StreamV3 *)packet,
                                              packetSize,
                                              file,
                                              data,
                                              dataSize,
                                              doSecurity);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackWriteWin32StreamReply --
 *
 *    Pack hgfs SendFileUsingReader reply.
 *    Returns the actual amount of data written in the reply.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackWriteWin32StreamReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                              char const *packetHeader,  // IN: packet header
                              HgfsOp op,                 // IN: request type
			                     uint32 actualSize,         // IN: amount written
			                     size_t *payloadSize,       // OUT: size of packet
                              HgfsSessionInfo *session)  // IN: Session info
{
   HgfsReplyWriteWin32StreamV3 *reply;
   Bool result;

   *payloadSize = 0;

   if (HGFS_OP_WRITE_WIN32_STREAM_V3 == op) {
      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                 (void **)&reply, session);
      if (result) {
         reply->reserved = 0;
         reply->actualSize = actualSize;
         *payloadSize = sizeof *reply;
      }
   } else {
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackReadPayload --
 *
 *    Unpack hgfs read payload to get the file handle and file offset to read from and
 *    the length of data to read.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackReadPayload(HgfsRequestRead *request,    // IN: payload
                      size_t payloadSize,          // IN: payload size
                      HgfsHandle* file,            // OUT: HGFS handle to close
                      uint64 *offset,              // OUT: offset to read from
                      uint32 *length)              // OUT: length of data to read
{
   LOG(4, ("%s: HGFS_OP_READ\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      *file = request->file;
      *offset = request->offset;
      *length = request->requiredSize;
      return TRUE;
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackReadPayloadV3 --
 *
 *    Unpack hgfs read payload V3 to get parameters needed to perform read.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackReadPayloadV3(HgfsRequestReadV3 *requestV3,  // IN: payload
                        size_t payloadSize,            // IN: payload size
                        HgfsHandle* file,              // OUT: HGFS handle to close
                        uint64 *offset,                // OUT: offset to read from
                        uint32 *length)                // OUT: length of data to read
{
   LOG(4, ("%s: HGFS_OP_READ_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      *file = requestV3->file;
      *offset = requestV3->offset;
      *length = requestV3->requiredSize;
      return TRUE;
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackReadRequest --
 *
 *    Unpack hgfs read request.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackReadRequest(void const *packet,     // IN: HGFS request
                      size_t packetSize,      // IN: request packet size
                      HgfsOp  op,             // IN: request type
                      HgfsHandle *file,       // OUT: Handle to close
                      uint64 *offset,         // OUT: offset to read from
                      uint32 *length)         // OUT: length of data to read
{
   Bool result;

   ASSERT(packet);

   switch (op) {
   case HGFS_OP_READ_FAST_V4:
   case HGFS_OP_READ_V3: {
         HgfsRequestReadV3 *requestV3 = (HgfsRequestReadV3 *)packet;

         result = HgfsUnpackReadPayloadV3(requestV3, packetSize, file, offset, length);
         break;
      }
   case HGFS_OP_READ: {
         HgfsRequestRead *requestV1 = (HgfsRequestRead *)packet;

         result = HgfsUnpackReadPayload(requestV1, packetSize, file, offset, length);
         break;
      }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   if (!result) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWritePayload --
 *
 *    Unpack hgfs write payload to get the file handle, file offset, of data to write,
 *    write flags and pointer to the data to write.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWritePayload(HgfsRequestWrite *request,    // IN: request payload
                       size_t payloadSize,           // IN: request payload size
                       HgfsHandle* file,             // OUT: HGFS handle to write to
                       uint64 *offset,               // OUT: offset to read from
                       uint32 *length,               // OUT: length of data to write
                       HgfsWriteFlags *flags,        // OUT: write flags
                       char **data)                  // OUT: data to be written
{
   LOG(4, ("%s: HGFS_OP_WRITE\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      if (sizeof *request + request->requiredSize - 1 <= payloadSize) {
         *file = request->file;
         *flags = request->flags;
         *offset = request->offset;
         *data = request->payload;
         *length = request->requiredSize;
         return TRUE;
      }
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWritePayloadV3 --
 *
 *    Unpack hgfs write payload V3.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWritePayloadV3(HgfsRequestWriteV3 *requestV3, // IN: payload
                         size_t payloadSize,            // IN: request payload size
                         HgfsHandle* file,              // OUT: HGFS handle write to
                         uint64 *offset,                // OUT: offset to read from
                         uint32 *length,                // OUT: length of data to write
                         HgfsWriteFlags *flags,         // OUT: write flags
                         char **data)                   // OUT: data to be written
{
   LOG(4, ("%s: HGFS_OP_WRITE_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      if (sizeof *requestV3 + requestV3->requiredSize - 1 <= payloadSize) {
         *file = requestV3->file;
         *flags = requestV3->flags;
         *offset = requestV3->offset;
         *data = requestV3->payload;
         *length = requestV3->requiredSize;
         return TRUE;
      }
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWriteFastPayloadV4 --
 *
 *    Unpack hgfs write fast payload V4.
 *    The only difference from V3 payload is that data to write are
 *    provided in the payload but located in a separate buffer.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWriteFastPayloadV4(HgfsRequestWriteV3 *requestV3, // IN: payload
                             size_t payloadSize,            // IN: request payload size
                             HgfsHandle* file,              // OUT: HGFS handle write to
                             uint64 *offset,                // OUT: offset to write to
                             uint32 *length,                // OUT: size of data to write
                             HgfsWriteFlags *flags)         // OUT: write flags
{
   LOG(4, ("%s: HGFS_OP_WRITE_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      *file = requestV3->file;
      *flags = requestV3->flags;
      *offset = requestV3->offset;
      *length = requestV3->requiredSize;
      return TRUE;
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackWriteRequest --
 *
 *    Unpack hgfs write request to get parameters and data to write.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackWriteRequest(HgfsInputParam *input,   // IN: Input params
                       HgfsHandle *file,        // OUT: Handle to write to
                       uint64 *offset,          // OUT: offset to write to
                       uint32 *length,          // OUT: length of data to write
                       HgfsWriteFlags *flags,   // OUT: write flags
                       char **data)             // OUT: data to be written
{
   Bool result;

   ASSERT(input);

   switch (input->op) {
   case HGFS_OP_WRITE_FAST_V4: {
         HgfsRequestWriteV3 *requestV3 = (HgfsRequestWriteV3 *)input->payload;

         result = HgfsUnpackWriteFastPayloadV4(requestV3, input->payloadSize, file,
                                               offset, length, flags);
         if (result) {
            *data = HSPU_GetDataPacketBuf(input->packet,
                                          BUF_READABLE,
                                          input->transportSession);
            if (NULL == *data) {
               LOG(4, ("%s: Failed to get data in guest memory\n", __FUNCTION__));
               result = FALSE;
            }
         }
         break;
      }
   case HGFS_OP_WRITE_V3: {
         HgfsRequestWriteV3 *requestV3 = (HgfsRequestWriteV3 *)input->payload;

         result = HgfsUnpackWritePayloadV3(requestV3, input->payloadSize, file, offset,
                                           length, flags, data);
         break;
      }
   case HGFS_OP_WRITE: {
         HgfsRequestWrite *requestV1 = (HgfsRequestWrite *)input->payload;

         result = HgfsUnpackWritePayload(requestV1, input->payloadSize, file, offset,
                                         length, flags, data);
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, input->op));
      NOT_REACHED();
      result = FALSE;
   }

   if (!result) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackWriteReply --
 *
 *    Pack hgfs write reply to the HgfsReplyWrite structure.
 *
 * Results:
 *    TRUE is there are no bugs in the code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackWriteReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                   char const *packetHeader,     // IN: packet header
                   HgfsOp op,                    // IN: request type
                   uint32 actualSize,            // IN: number of bytes that were written
                   size_t *payloadSize,          // OUT: size of packet
                   HgfsSessionInfo *session)     // IN: Session info
{
   Bool result;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_WRITE_FAST_V4:
   case HGFS_OP_WRITE_V3: {
      HgfsReplyWriteV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->reserved = 0;
         reply->actualSize = actualSize;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_WRITE: {
      HgfsReplyWrite *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->actualSize = actualSize;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackQueryVolumePayload --
 *
 *    Unpack hgfs query volume payload to get the file name which must be used to query
 *    volume information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackQueryVolumePayload(HgfsRequestQueryVolume *request, // IN: request payload
                             size_t payloadSize,              // IN: request payload size
                             char **fileName,                 // OUT: volume name
                             size_t *nameLength)              // OUT: volume name length
{
   LOG(4, ("%s: HGFS_OP_QUERY_VOLUME_INFO\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      return HgfsUnpackFileName(&request->fileName,
                                payloadSize - sizeof *request + 1,
                                fileName,
                                nameLength);
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackQueryVolumePayloadV3 --
 *
 *    Unpack hgfs query volume payload V3.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackQueryVolumePayloadV3(HgfsRequestQueryVolumeV3 *requestV3, // IN: payload
                               size_t payloadSize,                  // IN: payload size
                               Bool *useHandle,                     // OUT: use handle
                               HgfsHandle* file,                    // OUT: HGFS handle
                               char **fileName,                     // OUT: volume name
                               size_t *nameLength,                  // OUT: name length
                               uint32 * caseFlags)                  // OUT: case flags
{
   LOG(4, ("%s: HGFS_OP_QUERY_VOLUME_INFO_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      return HgfsUnpackFileNameV3(&requestV3->fileName,
                                  payloadSize - sizeof *requestV3 + 1,
                                  useHandle,
                                  fileName,
                                  nameLength,
                                  file,
                                  caseFlags);
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackQueryVolumeRequest --
 *
 *    Unpack hgfs query volume information request to get parameters related to
 *    query volume operation.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackQueryVolumeRequest(void const *packet,     // IN: HGFS packet
                             size_t packetSize,      // IN: request packet size
                             HgfsOp op,              // IN: request type
                             Bool *useHandle,        // OUT: use handle
                             char **fileName,        // OUT: file name
                             size_t *fileNameLength, // OUT: file name length
                             uint32 *caseFlags,      // OUT: case sensitivity
                             HgfsHandle *file)       // OUT: Handle to the volume
{
   ASSERT(packet);

   switch (op) {
   case HGFS_OP_QUERY_VOLUME_INFO_V3: {
         HgfsRequestQueryVolumeV3 *requestV3 = (HgfsRequestQueryVolumeV3 *)packet;

         if (!HgfsUnpackQueryVolumePayloadV3(requestV3, packetSize, useHandle, file,
                                             fileName, fileNameLength, caseFlags)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_QUERY_VOLUME_INFO: {
         HgfsRequestQueryVolume *requestV1 = (HgfsRequestQueryVolume *)packet;

         if (!HgfsUnpackQueryVolumePayload(requestV1, packetSize, fileName,
                                           fileNameLength)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         *file = HGFS_INVALID_HANDLE;
         *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
         *useHandle = FALSE;
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackQueryVolumeReply --
 *
 *    Pack hgfs query volume reply.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackQueryVolumeReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                         char const *packetHeader,  // IN: packet header
                         HgfsOp op,                 // IN: request type
                         uint64 freeBytes,          // IN: volume free space
                         uint64 totalBytes,         // IN: volume capacity
                         size_t *payloadSize,       // OUT: size of packet
                         HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_QUERY_VOLUME_INFO_V3: {
      HgfsReplyQueryVolumeV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->reserved = 0;
         reply->freeBytes = freeBytes;
         reply->totalBytes = totalBytes;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_QUERY_VOLUME_INFO: {
      HgfsReplyQueryVolume *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->freeBytes = freeBytes;
         reply->totalBytes = totalBytes;
          *payloadSize = sizeof *reply;
     }
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSymlinkCreatePayload --
 *
 *    Unpack hgfs symbolic link payload to get symbolic link file name
 *    and symbolic link target.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSymlinkCreatePayload(HgfsRequestSymlinkCreate *request, // IN: request payload
                               size_t payloadSize,                // IN: payload size
                               char **srcFileName,                // OUT: link file name
                               size_t *srcNameLength,             // OUT: file name length
                               char **tgFileName,                 // OUT: target file name
                               size_t *tgNameLength)              // OUT: target name length
{
   uint32 prefixSize;

   LOG(4, ("%s: HGFS_OP_CREATE_SYMLINK_V3\n", __FUNCTION__));
   prefixSize = offsetof(HgfsRequestSymlinkCreate, symlinkName.name);
   if (payloadSize >= prefixSize) {
      if (HgfsUnpackFileName(&request->symlinkName,
                             payloadSize - prefixSize,
                             srcFileName,
                             srcNameLength)) {
         HgfsFileName *targetName = (HgfsFileName *)(*srcFileName + 1 + *srcNameLength);
         prefixSize = ((char *)targetName - (char *)request) + offsetof(HgfsFileName, name);

         return HgfsUnpackFileName(targetName,
                                   payloadSize - prefixSize,
                                   tgFileName,
                                   tgNameLength);
      };
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSymlinkCreatePayloadV3 --
 *
 *    Unpack hgfs create symbolic link payload V3.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSymlinkCreatePayloadV3(HgfsRequestSymlinkCreateV3 *requestV3, // IN:
                                 size_t payloadSize,                    // IN:
                                 Bool *srcUseHandle,                    // OUT:
                                 HgfsHandle* srcFile,                   // OUT:
                                 char **srcFileName,                    // OUT:
                                 size_t *srcNameLength,                 // OUT:
                                 uint32 *srcCaseFlags,                  // OUT:
                                 Bool *tgUseHandle,                     // OUT:
                                 HgfsHandle* tgFile,                    // OUT:
                                 char **tgFileName,                     // OUT:
                                 size_t *tgNameLength,                  // OUT:
                                 uint32 * tgCaseFlags)                  // OUT:
{
   uint32 prefixSize;

   LOG(4, ("%s: HGFS_OP_CREATE_SYMLINK_V3\n", __FUNCTION__));
   prefixSize = offsetof(HgfsRequestSymlinkCreateV3, symlinkName.name);
   if (payloadSize >= prefixSize) {
      if (HgfsUnpackFileNameV3(&requestV3->symlinkName,
                               payloadSize - prefixSize,
                               srcUseHandle,
                               srcFileName,
                               srcNameLength,
                               srcFile,
                               srcCaseFlags)) {
         HgfsFileNameV3 *targetName = (HgfsFileNameV3 *)(*srcFileName + 1 +
                                                         *srcNameLength);
         prefixSize = ((char *)targetName - (char *)requestV3) +
                       offsetof(HgfsFileNameV3, name);

         return HgfsUnpackFileNameV3(targetName,
                                     payloadSize - prefixSize,
                                     tgUseHandle,
                                     tgFileName,
                                     tgNameLength,
                                     tgFile,
                                     tgCaseFlags);
      }
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSymlinkCreateRequest --
 *
 *    Unpack hgfs symbolic link creation request to get parameters related to
 *    creating the symbolic link.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSymlinkCreateRequest(void const *packet,        // IN: HGFS packet
                               size_t packetSize,         // IN: request packet size
                               HgfsOp op,                 // IN: request type
                               Bool *srcUseHandle,        // OUT: use source handle
                               char **srcFileName,        // OUT: source file name
                               size_t *srcFileNameLength, // OUT: source file name length
                               uint32 *srcCaseFlags,      // OUT: source case sensitivity
                               HgfsHandle *srcFile,       // OUT: source file handle
                               Bool *tgUseHandle,         // OUT: use target handle
                               char **tgFileName,         // OUT: target file name
                               size_t *tgFileNameLength,  // OUT: target file name length
                               uint32 *tgCaseFlags,       // OUT: target case sensitivity
                               HgfsHandle *tgFile)        // OUT: target file handle
{
   ASSERT(packet);

   switch (op) {
   case HGFS_OP_CREATE_SYMLINK_V3: {
         HgfsRequestSymlinkCreateV3 *requestV3 = (HgfsRequestSymlinkCreateV3 *)packet;

         if (!HgfsUnpackSymlinkCreatePayloadV3(requestV3, packetSize,
                                               srcUseHandle, srcFile,
                                               srcFileName, srcFileNameLength, srcCaseFlags,
                                               tgUseHandle, tgFile,
                                               tgFileName, tgFileNameLength, tgCaseFlags)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_CREATE_SYMLINK: {
         HgfsRequestSymlinkCreate *requestV1 = (HgfsRequestSymlinkCreate *)packet;

         if (!HgfsUnpackSymlinkCreatePayload(requestV1, packetSize, srcFileName,
                                             srcFileNameLength, tgFileName, tgFileNameLength)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         *srcFile = HGFS_INVALID_HANDLE;
         *srcCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
         *srcUseHandle = FALSE;
         *tgFile = HGFS_INVALID_HANDLE;
         *tgCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
         *tgUseHandle = FALSE;
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSymlinkCreateReply --
 *
 *    Pack hgfs symbolic link creation reply.
 *
 * Results:
 *    TRUE if valid op and reply set, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSymlinkCreateReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                           char const *packetHeader,  // IN: packet header
                           HgfsOp op,                 // IN: request type
                           size_t *payloadSize,       // OUT: size of packet
                           HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_CREATE_SYMLINK_V3: {
      HgfsReplySymlinkCreateV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         /* Reply only consists of a reserved field. */
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_CREATE_SYMLINK: {
      HgfsReplySymlinkCreate *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchOpenPayload --
 *
 *    Unpack hgfs search open payload to get name of directory to open.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchOpenPayload(HgfsRequestSearchOpen *request, // IN: payload
                            size_t payloadSize,             // IN: payload size
                            char **dirName,                 // OUT: directory name
                            uint32 *dirNameLength)          // OUT: name length
{
   LOG(4, ("%s: HGFS_OP_SEARCH_OPEN\n", __FUNCTION__));
   if (payloadSize >= sizeof *request) {
      if (sizeof *request + request->dirName.length - 1 <= payloadSize) {
         *dirName = request->dirName.name;
         *dirNameLength = request->dirName.length;
         return TRUE;
      }
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchOpenPayloadV3 --
 *
 *    Unpack hgfs search open payload V3 to get name of directory to open and
 *    case flags.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchOpenPayloadV3(HgfsRequestSearchOpenV3 *requestV3, // IN: payload
                              size_t payloadSize,                 // IN: payload size
                              char **dirName,                     // OUT: directory name
                              uint32 *dirNameLength,              // OUT: name length
                              uint32 *caseFlags)                  // OUT: case flags
{
   LOG(4, ("%s: HGFS_OP_SEARCH_OPEN_V3\n", __FUNCTION__));
   if (payloadSize >= sizeof *requestV3) {
      if (sizeof *requestV3 + requestV3->dirName.length - 1 <= payloadSize) {
         *dirName = requestV3->dirName.name;
         *dirNameLength = requestV3->dirName.length;
         *caseFlags = requestV3->dirName.flags;
         return TRUE;
      }
   }
   LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSearchOpenRequest --
 *
 *    Unpack hgfs search open request to get directory name and case flags.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSearchOpenRequest(void const *packet,      // IN: HGFS packet
                            size_t packetSize,       // IN: request packet size
                            HgfsOp op,               // IN: request type
                            char **dirName,          // OUT: directory name
                            uint32 *dirNameLength,   // OUT: name length
                            uint32 *caseFlags)       // OUT: case flags
{
   ASSERT(packet);

   switch (op) {
   case HGFS_OP_SEARCH_OPEN_V3: {
         HgfsRequestSearchOpenV3 *requestV3 = (HgfsRequestSearchOpenV3 *)packet;

         if (!HgfsUnpackSearchOpenPayloadV3(requestV3, packetSize, dirName,
                                            dirNameLength, caseFlags)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         break;
      }
   case HGFS_OP_SEARCH_OPEN: {
         HgfsRequestSearchOpen *requestV1 = (HgfsRequestSearchOpen *)packet;

         if (!HgfsUnpackSearchOpenPayload(requestV1, packetSize, dirName,
                                            dirNameLength)) {
            LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
            return FALSE;
         }
         *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
         break;
      }
   default:
      LOG(4, ("%s: Incorrect opcode %d\n", __FUNCTION__, op));
      NOT_REACHED();
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchOpenReply --
 *
 *    Pack hgfs search open reply.
 *
 * Results:
 *    TRUE unless it is invoked for a wrong op (which indicates a bug in the code).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSearchOpenReply(HgfsPacket *packet,          // IN/OUT: Hgfs Packet
                        char const *packetHeader,    // IN: packet header
                        HgfsOp op,                   // IN: request type
                        HgfsHandle search,           // IN: search handle
                        size_t *payloadSize,         // OUT: size of packet
                        HgfsSessionInfo *session)    // IN: Session info
{
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   switch (op) {
   case HGFS_OP_SEARCH_OPEN_V3: {
      HgfsReplySearchOpenV3 *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->reserved = 0;
         reply->search = search;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   case HGFS_OP_SEARCH_OPEN: {
      HgfsReplySearchOpen *reply;

      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->search = search;
         *payloadSize = sizeof *reply;
      }
      break;
   }
   default:
      NOT_REACHED();
      result = FALSE;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateSessionPayloadV4 --
 *
 *    Unpack hgfs create session request V4 payload.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateSessionPayloadV4(HgfsRequestCreateSessionV4 *requestV4, // IN: payload
                                 size_t payloadSize,                    // IN:
                                 HgfsCreateSessionInfo *info)           // IN/OUT: info
{
   LOG(4, ("%s: HGFS_OP_CREATE_SESSION_V4\n", __FUNCTION__));
   if (payloadSize  < offsetof(HgfsRequestCreateSessionV4, reserved)) {
      /* The input packet is smaller than the request. */
      return FALSE;
   }

   if (requestV4->numCapabilities) {
      if (payloadSize < offsetof(HgfsRequestCreateSessionV4, capabilities) +
         requestV4->numCapabilities * sizeof(HgfsCapability)) {
         LOG(4, ("%s: HGFS packet too small\n", __FUNCTION__));
         return FALSE;
      }
   }

   info->maxPacketSize = requestV4->maxPacketSize;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackCreateSessionRequest --
 *
 *    Unpack hgfs CreateSession request and initialize a corresponding
 *    HgfsCreateDirInfo structure that is used to pass around CreateDir request
 *    information.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackCreateSessionRequest(void const *packet,          // IN: HGFS packet
                               size_t packetSize,           // IN: size of packet
                               HgfsOp op,                   // IN: request type
                               HgfsCreateSessionInfo *info) // IN/OUT: info struct
{
   HgfsRequestCreateSessionV4 *requestV4;

   ASSERT(packet);
   ASSERT(info);

   ASSERT(op == HGFS_OP_CREATE_SESSION_V4);

   requestV4 = (HgfsRequestCreateSessionV4 *)packet;
   if (!HgfsUnpackCreateSessionPayloadV4(requestV4, packetSize, info)) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackCreateSessionReply --
 *
 *    Pack hgfs CreateSession reply.
 *
 * Results:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackCreateSessionReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                           char const *packetHeader,  // IN: packet header
                           size_t *payloadSize,       // OUT: size of packet
                           HgfsSessionInfo *session)  // IN: Session info
{
   Bool result;
   HgfsReplyCreateSessionV4 *reply;
   uint32 numCapabilities = session->numberOfCapabilities;
   uint32 capabilitiesLen = numCapabilities * sizeof *session->hgfsSessionCapabilities;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = offsetof(HgfsReplyCreateSessionV4, capabilities) + capabilitiesLen;

   result = HgfsAllocInitReply(packet, packetHeader, *payloadSize, (void **)&reply,
                               session);
   if (result) {
      reply->sessionId = session->sessionId;
      reply->numCapabilities = numCapabilities;
      reply->maxPacketSize = session->maxPacketSize;
      reply->identityOffset = 0;
      reply->reserved = 0;
      memcpy(reply->capabilities, session->hgfsSessionCapabilities, capabilitiesLen);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackDestroySessionReply --
 *
 *    Pack hgfs CreateSession reply.
 *
 * Results:
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackDestorySessionReply(HgfsPacket *packet,        // IN/OUT: Hgfs Packet
                            char const *packetHeader,  // IN: packet header
                            size_t *payloadSize,        // OUT: size of packet
                            HgfsSessionInfo *session)  // IN: Session info
{
   HgfsReplyDestroySessionV4 *reply;
   Bool result;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                               (void **)&reply, session);
   if (result) {
      /* Reply only consists of a reserved field. */
      *payloadSize = sizeof *reply;
      reply->reserved = 0;
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetDefaultCapabilities --
 *
 *    Returns list capabilities that are supported by all sessions.
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
HgfsServerGetDefaultCapabilities(HgfsCapability *capabilities,  // OUT: capabilities
                                 uint32 *numberOfCapabilities)  // OUT: number of items
{
   *numberOfCapabilities = ARRAYSIZE(hgfsDefaultCapabilityTable);
   ASSERT(*numberOfCapabilities <= HGFS_OP_MAX);
   memcpy(capabilities, hgfsDefaultCapabilityTable, sizeof hgfsDefaultCapabilityTable);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSetWatchReplyV4 --
 *
 *    Pack hgfs set watch V4 reply payload to the HgfsReplySetWatchV4 structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsPackSetWatchReplyV4(HgfsSubscriberHandle watchId, // IN: host id of thee new watch
                        HgfsReplySetWatchV4 *reply)   // OUT: reply buffer to fill
{
   reply->watchId = watchId;
   reply->reserved = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSetWatchReply --
 *
 *    Pack hgfs set watch reply to the HgfsReplySetWatchV4 structure.
 *
 * Results:
 *    TRUE if successfully allocated reply request, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSetWatchReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                      char const *packetHeader,     // IN: packet header
                      HgfsOp     op,                // IN: operation code
                      HgfsSubscriberHandle watchId, // IN: id of the new watch
                      size_t *payloadSize,          // OUT: size of packet
                      HgfsSessionInfo *session)     // IN: Session info
{
   Bool result;
   HgfsReplySetWatchV4 *reply;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   if (HGFS_OP_SET_WATCH_V4 != op) {
      NOT_REACHED();
      result = FALSE;
   } else {
      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                               (void **)&reply, session);
      if (result) {
         HgfsPackSetWatchReplyV4(watchId, reply);
         *payloadSize = sizeof *reply;
      }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetWatchPayloadV4 --
 *
 *    Unpack HGFS set directory notication watch payload version 4 and initializes
 *    a corresponding HgfsHandle or file name to tell us which directory to watch.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsUnpackSetWatchPayloadV4(HgfsRequestSetWatchV4 *requestV4, // IN: request payload
                            size_t payloadSize,               // IN: payload size
                            Bool *useHandle,                  // OUT: handle or cpName
                            uint32 *flags,                    // OUT: watch flags
                            uint32 *events,                   // OUT: event filter
                            char **cpName,                    // OUT: cpName
                            size_t *cpNameSize,               // OUT: cpName size
                            HgfsHandle *dir,                  // OUT: directory handle
                            uint32 *caseFlags)                // OUT: case-sensitivity
{
   if (payloadSize < sizeof *requestV4) {
      return FALSE;
   }

   *flags = requestV4->flags;
   *events = requestV4->events;

   return HgfsUnpackFileNameV3(&requestV4->fileName,
                               payloadSize - sizeof *requestV4,
                               useHandle,
                               cpName,
                               cpNameSize,
                               dir,
                               caseFlags);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackSetWatchRequest --
 *
 *    Unpack hgfs set directory notication watch request and initialize a corresponding
 *    HgfsHandle or directory name to tell us which directory to monitor.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackSetWatchRequest(void const *packet,      // IN: HGFS packet
                          size_t packetSize,       // IN: request packet size
                          HgfsOp op,               // IN: requested operation
                          Bool *useHandle,         // OUT: handle or cpName
                          char **cpName,           // OUT: cpName
                          size_t *cpNameSize,      // OUT: cpName size
                          uint32 *flags,           // OUT: flags for the new watch
                          uint32 *events,          // OUT: event filter
                          HgfsHandle *dir,         // OUT: direrctory handle
                          uint32 *caseFlags)       // OUT: case-sensitivity flags
{
   HgfsRequestSetWatchV4 *requestV4 = (HgfsRequestSetWatchV4 *)packet;
   Bool result;

   ASSERT(packet);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(dir);
   ASSERT(flags);
   ASSERT(events);
   ASSERT(caseFlags);
   ASSERT(useHandle);

   if (HGFS_OP_SET_WATCH_V4 != op) {
      NOT_REACHED();
      result = FALSE;
   } else {
      result = HgfsUnpackSetWatchPayloadV4(requestV4, packetSize, useHandle, flags,
                                           events, cpName, cpNameSize, dir, caseFlags);
   }

   if (!result) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackRemoveWatchReply --
 *
 *    Pack hgfs remove watch reply to the HgfsReplyRemoveWatchV4 structure.
 *
 * Results:
 *    TRUE if successfully allocated reply request, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackRemoveWatchReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                         char const *packetHeader,     // IN: packet header
                         HgfsOp     op,                // IN: operation code
                         size_t *payloadSize,          // OUT: size of packet
                         HgfsSessionInfo *session)     // IN: Session info
{
   Bool result;
   HgfsReplyRemoveWatchV4 *reply;

   HGFS_ASSERT_PACK_PARAMS;

   *payloadSize = 0;

   if (HGFS_OP_REMOVE_WATCH_V4 != op) {
      NOT_REACHED();
      result = FALSE;
   } else {
      result = HgfsAllocInitReply(packet, packetHeader, sizeof *reply,
                                  (void **)&reply, session);
      if (result) {
         reply->reserved = 0;
         *payloadSize = sizeof *reply;
      }
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRemoveWatchPayload --
 *
 *    Unpack HGFS remove directory notication watch payload version 4.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsUnpackRemoveWatchPayloadV4(HgfsRequestRemoveWatchV4 *requestV4, // IN: request payload
                               size_t payloadSize,                  // IN: payload size
                               HgfsSubscriberHandle *watchId)       // OUT: watch id
{
   if (payloadSize < sizeof *requestV4) {
      return FALSE;
   }

   *watchId = requestV4->watchId;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackRemoveWatchRequest --
 *
 *    Unpack hgfs remove directory notication watch request.
 *
 * Results:
 *    TRUE on success.
 *    FALSE on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUnpackRemoveWatchRequest(void const *packet,            // IN: HGFS packet
                             size_t packetSize,             // IN: request packet size
                             HgfsOp op,                     // IN: requested operation
                             HgfsSubscriberHandle *watchId) // OUT: watch Id to remove
{
   HgfsRequestRemoveWatchV4 *requestV4 = (HgfsRequestRemoveWatchV4 *)packet;

   ASSERT(packet);
   ASSERT(watchId);

   ASSERT(HGFS_OP_REMOVE_WATCH_V4 == op);

   if (HGFS_OP_REMOVE_WATCH_V4 != op) {
      return FALSE;
   } else if (!HgfsUnpackRemoveWatchPayloadV4(requestV4, packetSize, watchId)) {
      LOG(4, ("%s: Error decoding HGFS packet\n", __FUNCTION__));
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackCalculateNotificationSize --
 *
 *    Calculates size needed for change notification packet.
 *
 * Results:
 *    TRUE if successfully allocated reply request, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

size_t
HgfsPackCalculateNotificationSize(char const *shareName, // IN: shared folder name
                                  char *fileName)        // IN: relative file path
{
   size_t result = sizeof(HgfsRequestNotifyV4);

   if (NULL != fileName) {
      size_t shareNameLen = strlen(shareName);
      result += strlen(fileName) + 1 + shareNameLen;
   }
   result += sizeof(HgfsHeader);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBuildCPName --
 *
 *    Build crossplatform name out of share name and relative to the shared folder
 *    file path.
 *
 * Results:
 *    Length of the output crossplatform name.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static size_t
HgfsBuildCPName(char const *shareName,  // IN: utf8 share name
                char *fileName,         // IN: utf8 file path
                char **cpName)          // OUT: full name in cp format
{
   size_t shareNameLen = strlen(shareName) + 1;
   size_t fileNameLen = strlen(fileName) + 1;
   char *fullName = Util_SafeMalloc(shareNameLen + fileNameLen);
   size_t result;

   *cpName = Util_SafeMalloc(shareNameLen + fileNameLen);
   Str_Strcpy(fullName, shareName, shareNameLen);
   fullName[shareNameLen - 1] = DIRSEPC;
   Str_Strcpy(fullName + shareNameLen, fileName, fileNameLen);

   result = CPName_ConvertTo(fullName, shareNameLen + fileNameLen, *cpName);
   ASSERT(result > 0); // Unescaped name can't be longer then escaped thus it must fit.
   free(fullName);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackHgfsName --
 *
 *    Pack cpName into HgfsFileName structure.
 *
 * Results:
 *    TRUE if there is enough space in the buffer,
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsPackHgfsName(char *cpName,            // IN: cpName to pack
                 size_t cpNameLen,        // IN: length of the cpName
                 size_t availableSpace,   // IN: space available for HgfsFileName
                 size_t *nameSize,        // OUT: space consumed by HgfsFileName
                 HgfsFileName *fileName)  // OUT: structure to pack cpName into
{
   if (availableSpace < offsetof(HgfsFileName, name) + cpNameLen) {
      return FALSE;
   }
   fileName->length = cpNameLen;
   memcpy(fileName->name, cpName, cpNameLen);
   *nameSize = offsetof(HgfsFileName, name) + cpNameLen;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackChangeNotifyEventV4 --
 *
 *    Pack single change directory notification event information.
 *
 * Results:
 *    Length of the packed structure or 0 if the structure does not fit in the
 *    the buffer.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static size_t
HgfsPackChangeNotifyEventV4(uint32 mask,              // IN: event mask
                            char const *shareName,    // IN: share name
                            char *fileName,           // IN: file name
                            size_t bufferSize,        // IN: available space
                            HgfsNotifyEventV4 *reply) // OUT: notificaiton buffer
{
   size_t totalLength;

   if (sizeof *reply > bufferSize) {
      return 0;
   }

   reply->nextOffset = 0;
   reply->mask = mask;
   if (NULL != fileName) {
      char *fullPath;
      size_t remainingSize;
      size_t nameSize;
      size_t hgfsNameSize;

      nameSize = HgfsBuildCPName(shareName, fileName, &fullPath);
      remainingSize = bufferSize - offsetof(HgfsNotifyEventV4, fileName);
      if (HgfsPackHgfsName(fullPath, nameSize, remainingSize, &hgfsNameSize,
                           &reply->fileName)) {
          remainingSize -= hgfsNameSize;
          totalLength = bufferSize - remainingSize;
      } else {
         totalLength = 0;
      }
      free(fullPath);
   } else {
      reply->fileName.length = 0;
      totalLength = sizeof *reply;
   }
   return totalLength;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackChangeNotifyRequestV4 --
 *
 *    Pack hgfs directory change notification request to be sent to the guest.
 *
 * Results:
 *    Length of the packed structure or 0 if the structure does not fit in the
 *    the buffer.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static size_t
HgfsPackChangeNotifyRequestV4(HgfsSubscriberHandle watchId,  // IN: watch
                              uint32 flags,                  // IN: notify flags
                              uint32 mask,                   // IN: event mask
                              char const *shareName,         // IN: share name
                              char *fileName,                // IN: relative file path
                              size_t bufferSize,             // IN: available space
                              HgfsRequestNotifyV4 *reply)    // OUT: notification buffer
{
   size_t size;
   size_t notificationOffset;

   if (bufferSize < sizeof *reply) {
      return 0;
   }
   reply->watchId = watchId;
   reply->flags = flags;
   if ((flags & HGFS_NOTIFY_FLAG_OVERFLOW) == HGFS_NOTIFY_FLAG_OVERFLOW) {
      size = sizeof *reply;
      reply->count = 0;
      reply->flags = HGFS_NOTIFY_FLAG_OVERFLOW;
   } else {
      /*
       * For the moment server sends only one notification at a time and it relies
       * on transport to coalesce requests.
       * Later on we may consider supporting multiple notifications.
       */
      reply->count = 1;
      notificationOffset = offsetof(HgfsRequestNotifyV4, events);
      size = HgfsPackChangeNotifyEventV4(mask, shareName, fileName,
                                          bufferSize - notificationOffset,
                                          reply->events);
      if (size != 0) {
         size += notificationOffset;
      } else {
         /*
         * Set event flag to tell guest that some events were dropped
         * when filling out notification details failed.
         */
         size = sizeof *reply;
         reply->count = 0;
         reply->flags = HGFS_NOTIFY_FLAG_OVERFLOW;
      }
   }
   return size;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackChangeNotificationRequest --
 *
 *    Pack hgfs directory change notification request to the
 *    HgfsRequestNotifyV4 structure.
 *
 * Results:
 *    TRUE if successfully allocated reply request, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackChangeNotificationRequest(void *packet,                    // IN/OUT: Hgfs Packet
                                  HgfsSubscriberHandle subscriber, // IN: watch
                                  char const *shareName,           // IN: share name
                                  char *fileName,                  // IN: relative name
                                  uint32 mask,                     // IN: event mask
                                  uint32 flags,                    // IN: notify flags
                                  HgfsSessionInfo *session,        // IN: session
                                  size_t *bufferSize)              // INOUT: size of packet
{
   size_t notifyRequestSize;
   HgfsRequestNotifyV4 *notifyRequest;
   HgfsHeader *header = packet;
   Bool result;

   ASSERT(packet);
   ASSERT(shareName);
   ASSERT(NULL != fileName ||
          (flags & HGFS_NOTIFY_FLAG_OVERFLOW) == HGFS_NOTIFY_FLAG_OVERFLOW);
   ASSERT(session);
   ASSERT(bufferSize);

   if (*bufferSize < sizeof *header) {
      return FALSE;
   }

   /*
    *  Initialize notification header.
    *  Set status and requestId to 0 since these fields are not relevant for
    *  notifications.
    *  Initialize payload size to 0 - it is not known yet and will be filled later.
    */
   notifyRequest = (HgfsRequestNotifyV4 *)((char *)header + sizeof *header);
   notifyRequestSize = HgfsPackChangeNotifyRequestV4(subscriber,
                                                     flags,
                                                     mask,
                                                     shareName,
                                                     fileName,
                                                     *bufferSize - sizeof *header,
                                                     notifyRequest);
   if (0 != notifyRequestSize) {
      HgfsPackReplyHeaderV4(0, notifyRequestSize, HGFS_OP_NOTIFY_V4, session->sessionId, 0, header);
      result = TRUE;
   } else {
      result = FALSE;
   }

   return result;
}
