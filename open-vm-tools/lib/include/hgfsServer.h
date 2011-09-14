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
 * Function used for sending updates of server state to the manager.
 * Passed by the caller at session connect time.
 */
typedef void
HgfsServerStateLoggerFunc(void *data,     // IN
                          uint64 cookie); // IN

typedef struct HgfsServerStateLogger {
   HgfsServerStateLoggerFunc  *logger;       // logger callback
   void                       *loggerData;   // logger callback private data
} HgfsServerStateLogger;

typedef
struct HgfsVmxIov {
   void *va;           /* Virtual addr */
   uint64 pa;          /* Physical address passed by the guest */
   uint32 len;         /* length of data; should be <= PAGE_SIZE for VMCI; arbitrary for backdoor */
   char *token;        /* Token for physMem_ APIs */
} HgfsVmxIov;

typedef
struct HgfsVaIov {
   void *va;
   uint32 len;
}HgfsVaIov;

typedef enum {
   BUF_READABLE,      /* Establish readable mappings */
   BUF_WRITEABLE,     /* Establish writeable mappings */
   BUF_READWRITEABLE, /* Establish read-writeable mappings */
} MappingType;

typedef
struct HgfsPacket {
   uint64 id;

   /* Does the transport support Async operations ? */
   Bool supportsAsync;

   /* Does transport need to send Async reply ? */
   Bool processedAsync;

   /* Is the packet guest initiated ? */
   Bool guestInitiated;

   /* For metapacket we always establish writeable mappings */
   void *metaPacket;
   size_t metaPacketSize;
   Bool metaPacketIsAllocated;

   void *dataPacket;
   size_t dataPacketSize;
   uint32 dataPacketIovIndex;
   Bool dataPacketIsAllocated;
   /* What type of mapping was established - readable/ writeable ? */
   MappingType dataMappingType;

   void *replyPacket;
   size_t replyPacketSize;
   Bool replyPacketIsAllocated;

   uint32 iovCount;
   HgfsVmxIov iov[1];

} HgfsPacket;


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

// Channel capability flags
#define HGFS_CHANNEL_SHARED_MEM     (1 << 0)
#define HGFS_CHANNEL_ASYNC          (1 << 1)

typedef Bool
HgfsSessionSendFunc(void *opaqueSession,  // IN
                    char *buffer,         // IN
                    size_t bufferLen,     // IN
                    HgfsSendFlags flags); // IN

typedef struct HgfsServerChannelCallbacks {
    void* (*getReadVa)(uint64 pa, uint32 size, char **token);
    void* (*getWriteVa)(uint64 pa, uint32 size, char **token);
    void (*putVa)(char **token);
    Bool (*send)(void *opaqueSession, HgfsPacket *packet, char *buffer,
                 size_t bufferLen, HgfsSendFlags flags);
}HgfsServerChannelCallbacks;

typedef struct HgfsServerSessionCallbacks {
   Bool (*connect)(void *, HgfsServerChannelCallbacks *, uint32 ,void **);
   void (*disconnect)(void *);
   void (*close)(void *);
   void (*receive)(HgfsPacket *packet, void *);
   void (*invalidateObjects)(void *, DblLnkLst_Links *);
   void (*sendComplete)(HgfsPacket *, void *);
} HgfsServerSessionCallbacks;

Bool HgfsServer_InitState(HgfsServerSessionCallbacks **, HgfsServerStateLogger *);
void HgfsServer_ExitState(void);

uint32 HgfsServer_GetHandleCounter(void);
void HgfsServer_SetHandleCounter(uint32 newHandleCounter);

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

/*
 * Function used to notify HGFS server that a shared folder has been created or updated.
 * It allows HGFS server to maintain up-to-date list of shared folders and its
 * properties.
 */
typedef uint32 HgfsSharedFolderHandle;
#define HGFS_INVALID_FOLDER_HANDLE         ((HgfsSharedFolderHandle)~((HgfsSharedFolderHandle)0))

typedef HgfsSharedFolderHandle
HgfsRegisterSharedFolderFunc(const char *shareName,
                             const char *sharePath,
                             Bool addFolder);
HgfsSharedFolderHandle HgfsServer_RegisterSharedFolder(const char *shareName,
                                                       const char *sharePath,
                                                       Bool addFolder);
void HgfsServer_Quiesce(Bool freeze);

#endif // _HGFS_SERVER_H_
