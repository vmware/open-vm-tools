/*********************************************************
 * Copyright (C) 2009 VMware, Inc. All rights reserved.
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
 * unity.x --
 *
 *    Definition of the data structures used in the GuestRpc commands to
 *    enable/disable and manipulate unity.
 *
 *    XXX: We should move unityActive.x into this file to avoid a 'plethora' of XDR
 *    sources for Unity.
 */

/*
 * Include unityCommon for the definitions of the types of operations.
 */
%#include "unityCommon.h"

/*
 * Enumerates the different versions of the messages.
 */
enum UnityOptionsVersion {
  UNITY_OPTIONS_V1 = 1
};

/*
 * The structure used for version 1 of the message.
 */
struct UnityOptionsV1 {
  int featureMask;
};

/*
 * This defines the protocol for a 'unityOptions' message.
 *
 * The union allows us to introduce new versions of the protocol later by
 * creating new values in the enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail if
 * an unknown version is provided on the wire.
 */
union UnityOptions switch (UnityOptionsVersion ver) {
case UNITY_OPTIONS_V1:
   struct UnityOptionsV1 *unityOptionsV1;
};


/*
 * Unity Request, Confirm and Acknowledge XDR structures.
 *
 * The guest will request (from the host) that it be allowed to perform certain types
 * of window operations - for example minimize, the host will later confirm that the
 * guest can (or cannot) go ahead with the operation. Once the guest has performed
 * the requested operation it will acknowledge its completion back to the host.
 * In many ways this is analagous to the three way handshaking used by TCP.
 */

/*
 * Enumerates the different versions of the messages. Note that all three types of
 * messages (request, confirm, acknowledge) share the same version value. They must
 * be managed together when updating versions.
 */
enum UnityOperationVersion {
  UNITY_OP_V1 = 1
};

/*
 * The structure used to distinguish the operations of the message.
 */
union UnityOperationDetails switch (int op) {
case MINIMIZE:
   int dummy;        /* Dummy value to avoid empty union */
};

struct UnityRequestOperationV1 {
  /*
   * sequence should be used to associate a request with a later confirmation so that
   * state can be maintained within the guest as to oustanding requests (or to set
   * an error for requests that must maintain order and do not reflect the order
   * back correctly.).
   */
   int sequence;
   int windowId;
   UnityOperationDetails details;
};

struct UnityConfirmOperationV1 {
   int sequence;
   int windowId;
   UnityOperationDetails details;
   bool allow;
};

/*
 * This defines the protocol for a 'unityRequestOperation' message.
 *
 * The union allows us to introduce new versions of the protocol later by
 * creating new values in the enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail if
 * an unknown version is provided on the wire.
 */
union UnityRequestOperation switch (UnityOperationVersion ver) {
case UNITY_OP_V1:
   struct UnityRequestOperationV1 *unityRequestOpV1;
};

union UnityConfirmOperation switch (UnityOperationVersion ver) {
case UNITY_OP_V1:
   struct UnityConfirmOperationV1 *unityConfirmOpV1;
};

/*
 * Protocol to send the scraped/grabbed contents of a window to the host.
 */
enum UnityWindowContentsVersion {
   UNITY_WINDOW_CONTENTS_V1 = 1
};


/*
 * Message used to begin the transfer of the scraped window contents to the
 * host.
 */
struct UnityWindowContentsStartV1
{
   uint32 windowID;       /* The UnityWindowId of the window. */
   uint32 imageWidth;     /* The width of the image. */
   uint32 imageHeight;    /* The height of the image. */
   uint32 imageLength;    /* The total length of the data for the image. */
};


/*
 * Message used to signal the end of the transfer of the scraped window contents
 * to the host.
 */
struct UnityWindowContentsEndV1
{
   uint32 windowID;       /* The UnityWindowId of the window. */
};


/*
 * The maximum size of the image data in a UnityWindowContentsChunk.
 */
const UNITY_WINDOW_CONTENTS_MAX_CHUNK_SIZE = 49152;

/*
 * Message used to transfer a portion of the scraped window contents to the
 * host.
 */
struct UnityWindowContentsChunkV1
{
   uint32 windowID;       /* The UnityWindowId of the window. */
   uint32 chunkID;        /* The sequence number of this chunk. */
   opaque data<UNITY_WINDOW_CONTENTS_MAX_CHUNK_SIZE>;
};


union UnityWindowContentsChunk switch (UnityWindowContentsVersion ver) {
case UNITY_WINDOW_CONTENTS_V1:
   struct UnityWindowContentsChunkV1 *chunkV1;
};


union UnityWindowContentsStart switch (UnityWindowContentsVersion ver) {
case UNITY_WINDOW_CONTENTS_V1:
   struct UnityWindowContentsStartV1 *startV1;
};


union UnityWindowContentsEnd switch (UnityWindowContentsVersion ver) {
case UNITY_WINDOW_CONTENTS_V1:
   struct UnityWindowContentsEndV1 *endV1;
};


/*
 * Protocol to request the contents for a list of Unity windows.
 */
const UNITY_MAX_NUM_WINDOWS_PER_REQUEST = 256;

struct UnityWindowContentsRequestV1 {
   uint32 windowID<UNITY_MAX_NUM_WINDOWS_PER_REQUEST>;
};

union UnityWindowContentsRequest switch (UnityWindowContentsVersion ver) {
case UNITY_WINDOW_CONTENTS_V1:
   struct UnityWindowContentsRequestV1 *requestV1;
};

/*
 * Message used to register the presence of a PBRPC server in the guest for
 * handling Unity & GHI operations. This message is sent by the guest to
 * bootstrap the process of talking via a 'non-backdoor' channel between guest
 * and host.
 */

const UNITY_REGISTER_PBRPCSERVER_ADDRESS_LEN = 256;

enum UnityRegisterPbrpcServerVersion {
   UNITY_REGISTER_PBRPCSERVER_V1 = 1
};

struct UnityRegisterPbrpcServerV1 {
   uint32 addressFamily;
   string address<UNITY_REGISTER_PBRPCSERVER_ADDRESS_LEN>;
   uint32 port;
};

union UnityRegisterPbrpcServer switch (UnityRegisterPbrpcServerVersion ver) {
case UNITY_REGISTER_PBRPCSERVER_V1:
   struct UnityRegisterPbrpcServerV1* registerV1;
};


/*
 * Mouse wheel.
 */

enum UnityMouseWheelVersion {
   UNITY_MOUSE_WHEEL_V1 = 1
};

struct UnityMouseWheelV1
{
   int32 deltaX;
   int32 deltaY;
   int32 deltaZ;
   uint32 modifierFlags;
};

union UnityMouseWheel switch (UnityMouseWheelVersion ver) {
case UNITY_MOUSE_WHEEL_V1:
   struct UnityMouseWheelV1 *mouseWheelV1;
};
