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
* ghiGetExecInfoHash.x --
*
*    Definition of the data structures used in the GuestRpc commands to
*    get the hash of information returned by the get.binary.info RPC.
*/

#include "ghiCommonDefines.h"

#define GHI_EXEC_INFO_HASH_MAX_LEN 1024

/*
* Enumerates the different versions of the messages.
*/
enum GHIGetExecInfoHashVersion {
  GHI_GET_EXEC_INFO_HASH_V1 = 1
};

/*
* The structures used for version 1 of the messages.
*/
struct GHIGetExecInfoHashRequestV1 {
  /*
    * A string identifier for the executable path.
    */
   string execPath<GHI_HANDLERS_ACTIONURI_MAX_PATH>;
};
struct GHIGetExecInfoHashReplyV1 {
  /*
    * A string identifier for the hash.
    */
   string execHash<GHI_EXEC_INFO_HASH_MAX_LEN>;
};

/*
 * This defines the protocol for 'getExecInfoHash' request and reply messages.
 *
 * The union allows us to introduce new versions of the protocol later by
 * creating new values in the enumeration, without having to change much of
 * the code calling the (de)serialization functions.
 *
 * Since the union doesn't have a default case, de-serialization will fail if
 * an unknown version is provided on the wire.
 */
union GHIGetExecInfoHashRequest switch (GHIGetExecInfoHashVersion ver) {
case GHI_GET_EXEC_INFO_HASH_V1:
   struct GHIGetExecInfoHashRequestV1 *requestV1;
};
union GHIGetExecInfoHashReply switch (GHIGetExecInfoHashVersion ver) {
case GHI_GET_EXEC_INFO_HASH_V1:
   struct GHIGetExecInfoHashReplyV1 *replyV1;
};

