/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#ifndef _HGFS_SERVER_H_
#define _HGFS_SERVER_H_

#include "hgfs.h"             /* for HGFS_PACKET_MAX */
#include "dbllnklst.h"

/*
 * Function used for sending replies to the client for a session.
 * Passed by the caller at session connect time.
 */

/*
 * Send flags.
 *
 * Contains a bitwise OR of a combination of the following flags:
 * HGFS_SEND_CAN_DELAY - directs the channel to try and optimize
 * otherwise it will send the data immediately.
 * HGFS_SEND_NO_COMPLETE - directs the channel to not call the
 * send complete callback. Caller does not call completion notification
 * callback, for example to free buffers.
 */

typedef uint32 HgfsSendFlags;

#define HGFS_SEND_CAN_DELAY         (1 << 0)
#define HGFS_SEND_NO_COMPLETE       (1 << 1)

/*
 * Receive flags.
 *
 * Contains a bitwise OR of a combination of the following flags:
 * HGFS_RECEIVE_CAN_DELAY - directs the server to handle the message
 * asynchronously.
 */

typedef uint32 HgfsReceiveFlags;

#define HGFS_RECEIVE_CAN_DELAY      (1 << 0)

typedef Bool
HgfsSessionSendFunc(void *opaqueSession,  // IN
                    char *buffer,         // IN
                    size_t bufferLen,     // IN
                    HgfsSendFlags flags); // IN

typedef struct HgfsServerSessionCallbacks {
   Bool (*connect)(void *, HgfsSessionSendFunc *, void **);
   void (*disconnect)(void *);
   void (*close)(void *);
   void (*receive)(char const *,size_t, void *, HgfsReceiveFlags);
   void (*invalidateObjects)(void *, DblLnkLst_Links *);
   void (*sendComplete)(void *, char *);
} HgfsServerSessionCallbacks;

Bool HgfsServer_InitState(HgfsServerSessionCallbacks **);
void HgfsServer_ExitState(void);

uint32 HgfsServer_GetHandleCounter(void);
void HgfsServer_SetHandleCounter(uint32 newHandleCounter);

#ifdef VMX86_TOOLS
void HgfsServer_ProcessPacket(char const *packetIn,
                              char *packetOut,
                              size_t *packetSize,
                              HgfsReceiveFlags flags);
#endif

/*
 * Function pointers used for getting names in HgfsServerGetDents
 *
 * Functions of this type are expected to return a NUL terminated
 * string and the length of that string.
 */
typedef Bool
HgfsGetNameFunc(void *data,        // IN
                char const **name, // OUT
                size_t *len,       // OUT
                Bool *done);       // OUT

/*
 * Associated setup and cleanup function types, which should be called
 * before and after (respectively) HgfsGetNameFunc.
 */
typedef void *
HgfsInitFunc(void);

typedef Bool
HgfsCleanupFunc(void *);  // IN

/*
 * Function used for invalidating nodes and searches that fall outside of a
 * share when the list of shares changes.
 */
typedef void
HgfsInvalidateObjectsFunc(DblLnkLst_Links *shares); // IN

#endif // _HGFS_SERVER_H_
