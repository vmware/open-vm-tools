/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#include "dbllnklst.h"
#include "hgfs.h"             /* for HGFS_PACKET_MAX */
#include "vm_basic_defs.h"    /* for vmx86_debug */

#if defined(__cplusplus)
extern "C" {
#endif

#define HGFS_VMX_IOV_CONTEXT_SIZE (vmx86_debug ? 112 : 96)
typedef struct HgfsVmxIov {
   void *va;           /* Virtual addr */
   uint64 pa;          /* Physical address passed by the guest */
   uint32 len;         /* length of data; should be <= PAGE_SIZE for VMCI; arbitrary for backdoor */
   union {
      void *ptr;
      char clientStorage[HGFS_VMX_IOV_CONTEXT_SIZE];
   } context;         /* Mapping context */
} HgfsVmxIov;

typedef enum {
   BUF_READABLE,      /* Establish readable mappings */
   BUF_WRITEABLE,     /* Establish writeable mappings */
   BUF_READWRITEABLE, /* Establish read-writeable mappings */
} MappingType;

typedef uint64 HgfsStateFlags;
#define HGFS_STATE_CLIENT_REQUEST         (1 << 0)
#define HGFS_STATE_ASYNC_REQUEST          (1 << 1)
typedef struct HgfsPacket {
   uint64 id;

   HgfsStateFlags state;

   /* For metapacket we always establish writeable mappings */
   void *metaPacket;
   size_t metaPacketSize;
   uint32 metaPacketMappedIov;
   size_t metaPacketDataSize;
   Bool metaPacketIsAllocated;
   MappingType metaMappingType;

   void *dataPacket;
   size_t dataPacketSize;
   uint32 dataPacketMappedIov;
   size_t dataPacketDataSize;
   uint32 dataPacketIovIndex;
   Bool dataPacketIsAllocated;
   /* What type of mapping was established - readable/ writeable ? */
   MappingType dataMappingType;

   void *replyPacket;
   size_t replyPacketSize;
   size_t replyPacketDataSize;
   Bool replyPacketIsAllocated;

   /* Iov for the packet private to the channel. */
   HgfsVmxIov channelIov[2];

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
typedef uint32 HgfsChannelFlags;
#define HGFS_CHANNEL_SHARED_MEM     (1 << 0)
#define HGFS_CHANNEL_ASYNC          (1 << 1)

typedef struct HgfsServerChannelData {
   HgfsChannelFlags flags;
   uint32 maxPacketSize;
}HgfsServerChannelData;


/* Default maximum number of open nodes. */
#define HGFS_MAX_CACHED_FILENODES   30

typedef uint32 HgfsConfigFlags;
#define HGFS_CONFIG_USE_HOST_TIME                    (1 << 0)
#define HGFS_CONFIG_NOTIFY_ENABLED                   (1 << 1)
#define HGFS_CONFIG_VOL_INFO_MIN                     (1 << 2)
#define HGFS_CONFIG_OPLOCK_ENABLED                   (1 << 3)
#define HGFS_CONFIG_SHARE_ALL_HOST_DRIVES_ENABLED    (1 << 4)

typedef struct HgfsServerConfig {
   HgfsConfigFlags flags;
   uint32 maxCachedOpenNodes;
}HgfsServerConfig;

/*
 * Function used to notify HGFS server that a shared folder has been created or updated.
 * It allows HGFS server to maintain up-to-date list of shared folders and its
 * properties.
 */
typedef uint32 HgfsSharedFolderHandle;
#define HGFS_INVALID_FOLDER_HANDLE         ((HgfsSharedFolderHandle)~((HgfsSharedFolderHandle)0))

/*
 * Callback functions to enumerate the share resources.
 * Filled in by the HGFS server policy and passed in to the HGFS server
 * so that it can call out to them to enumerate the shares.
 */
typedef void * (*HgfsServerResEnumInitFunc)(void);
typedef Bool (*HgfsServerResEnumGetFunc)(void *data,
                                         char const **name,
                                         size_t *len,
                                         Bool *done);
typedef Bool (*HgfsServerResEnumExitFunc)(void *);

typedef struct HgfsServerResEnumCallbacks {
   HgfsServerResEnumInitFunc init;
   HgfsServerResEnumGetFunc get;
   HgfsServerResEnumExitFunc exit;
} HgfsServerResEnumCallbacks;


/*
 * Server Manager callback functions to enumerate the share resources and state logging.
 * Passed to the HGFS server on initialization.
 */
typedef struct HgfsServerMgrCallbacks {
   HgfsServerResEnumCallbacks enumResources;
} HgfsServerMgrCallbacks;

typedef enum {
   HGFS_QUIESCE_FREEZE,
   HGFS_QUIESCE_THAW,
} HgfsQuiesceOp;

/*
 * Function used for invalidating nodes and searches that fall outside of a
 * share when the list of shares changes.
 */
typedef void (*HgfsInvalidateObjectsFunc)(DblLnkLst_Links *shares);

typedef Bool (*HgfsChannelSendFunc)(void *opaqueSession,
                                    HgfsPacket *packet,
                                    HgfsSendFlags flags);
typedef void * (*HgfsChannelMapVirtAddrFunc)(HgfsVmxIov *iov);
typedef void (*HgfsChannelUnmapVirtAddrFunc)(void *context);
typedef void (*HgfsChannelRegisterThreadFunc)(void);
typedef void (*HgfsChannelUnregisterThreadFunc)(void);

typedef struct HgfsServerChannelCallbacks {
   HgfsChannelRegisterThreadFunc registerThread;
   HgfsChannelUnregisterThreadFunc unregisterThread;
   HgfsChannelMapVirtAddrFunc getReadVa;
   HgfsChannelMapVirtAddrFunc getWriteVa;
   HgfsChannelUnmapVirtAddrFunc putVa;
   HgfsChannelSendFunc send;
}HgfsServerChannelCallbacks;

typedef struct HgfsServerSessionCallbacks {
   Bool (*connect)(void *, HgfsServerChannelCallbacks *, HgfsServerChannelData *,void **);
   void (*disconnect)(void *);
   void (*close)(void *);
   void (*receive)(HgfsPacket *packet, void *);
   void (*invalidateObjects)(void *, DblLnkLst_Links *);
   uint32 (*invalidateInactiveSessions)(void *);
   void (*sendComplete)(HgfsPacket *, void *);
   void (*quiesce)(void *, HgfsQuiesceOp);
} HgfsServerSessionCallbacks;

/* XXX: TODO delete this layer if no other non-session callbacks are required. */
typedef struct HgfsServerCallbacks {
   HgfsServerSessionCallbacks session;
} HgfsServerCallbacks;

Bool HgfsServer_InitState(const HgfsServerCallbacks **,
                          HgfsServerConfig *,
                          HgfsServerMgrCallbacks *);
void HgfsServer_ExitState(void);

Bool HgfsServer_ShareAccessCheck(HgfsOpenMode accessMode,
                                 Bool shareWriteable,
                                 Bool shareReadable);

uint32 HgfsServer_GetHandleCounter(void);
void HgfsServer_SetHandleCounter(uint32 newHandleCounter);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _HGFS_SERVER_H_
