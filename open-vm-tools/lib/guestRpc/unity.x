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
 * Enumerates the different versions of the messages.
 */
enum UnityOptionsVersion {
  UNITY_OPTIONS_V1 = 1
};

/*
 * List of features (as a bitmask) which may be optionally enabled when entering
 * Unity mode. By default all these features are disabled.
 */
enum UnityFeatures {
   UNITY_ADD_HIDDEN_WINDOWS_TO_TRACKER = 1,
   UNITY_INTERLOCK_MINIMIZE_OPERATION = 2,
   UNITY_SEND_WINDOW_CONTENTS = 4
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
