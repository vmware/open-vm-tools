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

#if defined(__APPLE__)
/*
 * DirectoryEntry type that is used in common code is
 * defined as dirent for Mac OS.
 * _DARWIN_USE_64_BIT_INODE definition is needed to make dirent
 * structure definitions in this file and in HgfsServerLinux.c
 * consistent.
 */
#define _DARWIN_USE_64_BIT_INODE
#endif

#include <string.h>
#include <stdlib.h>

#include "vmware.h"
#include "str.h"
#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsServerInt.h"
#include "hgfsServerPolicy.h"
#include "hgfsUtil.h"
#include "hgfsVirtualDir.h"
#include "hgfsEscape.h"
#include "codeset.h"
#include "config.h"
#include "dbllnklst.h"
#include "file.h"
#include "util.h"
#include "wiper.h"
#include "hgfsServer.h"
#include "hgfsDirNotify.h"
#include "hgfsTransport.h"
#include "userlock.h"
#include "poll.h"
#include "mutexRankLib.h"
#include "vm_basic_asm.h"
#include "unicodeOperations.h"

#if defined(_WIN32)
#include <io.h>
#define HGFS_PARENT_DIR "..\\"
#else
#include <unistd.h>
#define stricmp strcasecmp
#define HGFS_PARENT_DIR "../"
#endif // _WIN32
#define HGFS_PARENT_DIR_LEN 3

#define LOGLEVEL_MODULE hgfs
#include "loglevel_user.h"


/*
 * Define this to enable an ASSERT on HGFS_STATUS_PROTOCOL_ERROR.
 * This is useful if client is to be guaranteed to work with the server
 * without falling back to older protocol versions and to ensure that
 * clients don't send op value greater than HGFS_OP_MAX.
 *
 * NOTE: This flag is only meant to be used while testing. This should
 *       _always_ be undefined when checking code in.
 */
#if 0
#define HGFS_ASSERT_CLIENT(op) \
   do { \
   LOG(4, ("%s: op: %u.\n", __FUNCTION__, op)); \
   ASSERT(status != HGFS_STATUS_PROTOCOL_ERROR); \
   } while(0)
#else
#define HGFS_ASSERT_CLIENT(op)
#endif

#define HGFS_ASSERT_INPUT(input) ASSERT(input && input->packet && input->metaPacket && \
                                        ((!input->v4header && input->session) || \
                                         (input->v4header && \
                                          (input->op == HGFS_OP_CREATE_SESSION_V4 || input->session))) && \
                                        (!input->payloadSize || input->payload))

/*
 * Define this to enable an ASSERT if server gets an op lower than
 * this value. This is useful if client is to be guaranteed to work with
 * the server without falling back to older protocol versions.
 *
 * NOTE: This flag is only meant to be used while testing. This should
 *       _always_ be undefined when checking code in.
 */
#if 0
#define HGFS_ASSERT_MINIMUM_OP(op) \
   do { \
      LOG(4, ("%s: op received - %u.\n", __FUNCTION__, op)); \
      ASSERT(op >= HGFS_OP_OPEN_V3); \
   } while(0)
#else
#define HGFS_ASSERT_MINIMUM_OP(op)
#endif

#ifdef VMX86_TOOLS
#define Config_GetBool(defaultValue,fmt) (defaultValue)
#endif

/*
 * This ensures that the hgfs name conversion code never fails on long
 * filenames by using a buffer that is too small. If anything, we will
 * fail first elsewhere because the name is too big to fit in one hgfs
 * packet. [bac]
 */
#define HGFS_PATH_MAX HGFS_PACKET_MAX

/*
 * Array of FileNodes for opening files.
 */
#define NUM_FILE_NODES 100
#define NUM_SEARCHES 100

/* Default maximum number of open nodes. */
#define MAX_CACHED_FILENODES 30

/* Default maximun number of open nodes that have server locks. */
#define MAX_LOCKED_FILENODES 10

/* Maximum number of cached open nodes. */
static unsigned int maxCachedOpenNodes;

/* Value of config option to require using host timestamps */
Bool alwaysUseHostTime = FALSE;

/*
 * Monotonically increasing handle counter used to dish out HgfsHandles.
 * This value is checkpointed.
 */
static Atomic_uint32 hgfsHandleCounter = {0};

/*
 * Number of outstanding asynchronous operations.
 */
static Atomic_uint32 gHgfsAsyncCounter = {0};
static MXUserExclLock *gHgfsAsyncLock;
static MXUserCondVar  *gHgfsAsyncVar;

static HgfsServerStateLogger *hgfsMgrData = NULL;

/*
 * Session usage and locking.
 *
 * The channel will serialize callbacks to connect, disconnect, close
 * and invalidate objects for sessions.
 * The receives will also be serialized with the above when received through
 * the backdoor channel.
 * However, when requests are received from a socket, they will be from a
 * worker thread. It is the responsibility of the socket channel to keep
 * the session alive when processing the receive request which it does by
 * an additional reference for the session. This means even if a disconnect
 * occurs and the socket is closed, the channel will not call the session
 * close until the hgfs server returns from the receive processing. Thus
 * the hgfs server session data will remain valid.
 * When the hgfs server processes requests asynchronously, or returns from
 * receive request prior to sending the reply to be done at a later time,
 * a reference on the session is taken out while processing the message,
 * and not removed until the reply is processed. This reference will ensure
 * the session is not torndown until the final reference is removed, even
 * if the close session is called from the channel.
 */

/* Session related callbacks. */
static void HgfsServerSessionReceive(HgfsPacket *packet,
                                     void *clientData);
static Bool HgfsServerSessionConnect(void *transportData,
                                     HgfsServerChannelCallbacks *channelCbTable,
                                     uint32 channelCapabililies,
                                     void **clientData);
static void HgfsServerSessionDisconnect(void *clientData);
static void HgfsServerSessionClose(void *clientData);
static void HgfsServerSessionInvalidateObjects(void *clientData,
                                               DblLnkLst_Links *shares);
static uint32 HgfsServerSessionInvalidateInactiveSessions(void *clientData);
static void HgfsServerSessionSendComplete(HgfsPacket *packet, void *clientData);

/*
 * Callback table passed to transport and any channels.
 */
HgfsServerSessionCallbacks hgfsServerSessionCBTable = {
   HgfsServerSessionConnect,
   HgfsServerSessionDisconnect,
   HgfsServerSessionClose,
   HgfsServerSessionReceive,
   HgfsServerSessionInvalidateObjects,
   HgfsServerSessionInvalidateInactiveSessions,
   HgfsServerSessionSendComplete,
};

/* Lock that protects shared folders list. */
static MXUserExclLock *gHgfsSharedFoldersLock = NULL;

/* List of shared folders nodes. */
static DblLnkLst_Links gHgfsSharedFoldersList;

static Bool gHgfsInitialized = FALSE;

/*
 * Number of active sessions that support change directory notification. HGFS server
 * needs to maintain up-to-date shared folders list when there is
 * at least one such session.
 */
static Bool gHgfsDirNotifyActive = FALSE;

typedef struct HgfsSharedFolderProperties {
   DblLnkLst_Links links;
   char *name;                                /* Name of the share. */
   HgfsSharedFolderHandle notificationHandle; /* Directory notification handle. */
   Bool markedForDeletion;
} HgfsSharedFolderProperties;

static void HgfsServerTransportRemoveSessionFromList(HgfsTransportSessionInfo *transportSession,
                                                     HgfsSessionInfo *sessionInfo);

/*
 *    Limit payload to 16M + header.
 *    This limit ensures that list of shared pages fits into VMCI datagram.
 *    Client may impose a lower limit in create session request.
 */
#define MAX_SERVER_PACKET_SIZE_V4         (0x1000000 + sizeof(HgfsHeader))

/* Local functions. */
static void HgfsInvalidateSessionObjects(DblLnkLst_Links *shares,
                                         HgfsSessionInfo *session);
static Bool HgfsAddToCacheInternal(HgfsHandle handle,
                                   HgfsSessionInfo *session);
static Bool HgfsIsCachedInternal(HgfsHandle handle,
                                 HgfsSessionInfo *session);
static Bool HgfsRemoveLruNode(HgfsSessionInfo *session);
static Bool HgfsRemoveFromCacheInternal(HgfsHandle handle,
                                        HgfsSessionInfo *session);
static void HgfsRemoveSearchInternal(HgfsSearch *search,
                                     HgfsSessionInfo *session);
static HgfsSearch *HgfsSearchHandle2Search(HgfsHandle handle,
                                           HgfsSessionInfo *session);
static HgfsHandle HgfsSearch2SearchHandle(HgfsSearch const *search);
static HgfsSearch *HgfsAddNewSearch(char const *utf8Dir,
                                    DirectorySearchType type,
                                    char const *utf8ShareName,
                                    char const *rootDir,
                                    HgfsSessionInfo *session);
static void HgfsDumpAllSearches(HgfsSessionInfo *session);
static void HgfsDumpAllNodes(HgfsSessionInfo *session);
static void HgfsFreeFileNode(HgfsHandle handle,
                             HgfsSessionInfo *session);
static void HgfsFreeFileNodeInternal(HgfsHandle handle,
                                     HgfsSessionInfo *session);
static HgfsFileNode *HgfsAddNewFileNode(HgfsFileOpenInfo *openInfo,
                                        HgfsLocalId const *localId,
                                        fileDesc fileDesc,
                                        Bool append,
                                        size_t shareNameLen,
                                        char const *shareName,
                                        Bool sharedFolderOpen,
                                        HgfsSessionInfo *session);
static void HgfsRemoveFileNode(HgfsFileNode *node,
                               HgfsSessionInfo *session);
static HgfsFileNode *HgfsGetNewNode(HgfsSessionInfo *session);
static HgfsHandle HgfsFileNode2Handle(HgfsFileNode const *fileNode);
static HgfsFileNode *HgfsHandle2FileNode(HgfsHandle handle,
                                         HgfsSessionInfo *session);
static void HgfsServerExitSessionInternal(HgfsSessionInfo *session);
static Bool HgfsIsShareRoot(char const *cpName, size_t cpNameSize);
static void HgfsServerCompleteRequest(HgfsInternalStatus status,
                                      size_t replyPayloadSize,
                                      HgfsInputParam *input);
static Bool HgfsHandle2NotifyInfo(HgfsHandle handle,
                                  HgfsSessionInfo *session,
                                  char **fileName,
                                  size_t *fileNameSize,
                                  HgfsSharedFolderHandle *folderHandle);
static void HgfsServerDirWatchEvent(HgfsSharedFolderHandle sharedFolder,
                                    HgfsSubscriberHandle subscriber,
                                    char* fileName,
                                    uint32 mask,
                                    struct HgfsSessionInfo *session);
static void HgfsFreeSearchDirents(HgfsSearch *search);


/*
 * Opcode handlers
 */

static void HgfsServerOpen(HgfsInputParam *input);
static void HgfsServerRead(HgfsInputParam *input);
static void HgfsServerWrite(HgfsInputParam *input);
static void HgfsServerSearchOpen(HgfsInputParam *input);
static void HgfsServerSearchRead(HgfsInputParam *input);
static void HgfsServerGetattr(HgfsInputParam *input);
static void HgfsServerSetattr(HgfsInputParam *input);
static void HgfsServerCreateDir(HgfsInputParam *input);
static void HgfsServerDeleteFile(HgfsInputParam *input);
static void HgfsServerDeleteDir(HgfsInputParam *input);
static void HgfsServerRename(HgfsInputParam *input);
static void HgfsServerQueryVolume(HgfsInputParam *input);
static void HgfsServerSymlinkCreate(HgfsInputParam *input);
static void HgfsServerServerLockChange(HgfsInputParam *input);
static void HgfsServerWriteWin32Stream(HgfsInputParam *input);
static void HgfsServerCreateSession(HgfsInputParam *input);
static void HgfsServerDestroySession(HgfsInputParam *input);
static void HgfsServerClose(HgfsInputParam *input);
static void HgfsServerSearchClose(HgfsInputParam *input);
static void HgfsServerSetDirNotifyWatch(HgfsInputParam *input);
static void HgfsServerRemoveDirNotifyWatch(HgfsInputParam *input);


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerSessionGet --
 *
 *      Increment session reference count.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsServerSessionGet(HgfsSessionInfo *session)   // IN: session context
{
   ASSERT(session);
   Atomic_Inc(&session->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerSessionPut --
 *
 *      Decrement session reference count.
 *
 *      Free session info data if no reference.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsServerSessionPut(HgfsSessionInfo *session)   // IN: session context
{
   ASSERT(session);

   if (Atomic_FetchAndDec(&session->refCount) == 1) {
      HgfsServerExitSessionInternal(session);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerTransportSessionGet --
 *
 *      Increment transport session reference count.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsServerTransportSessionGet(HgfsTransportSessionInfo *transportSession)   // IN: session context
{
   ASSERT(transportSession);
   Atomic_Inc(&transportSession->refCount);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerTransportSessionPut --
 *
 *      Decrement transport session reference count.
 *
 *      Free session info data if no reference.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsServerTransportSessionPut(HgfsTransportSessionInfo *transportSession)   // IN: transport session context
{
   ASSERT(transportSession);
   if (Atomic_FetchAndDec(&transportSession->refCount) == 1) {
      DblLnkLst_Links *curr, *next;

      MXUser_AcquireExclLock(transportSession->sessionArrayLock);

      DblLnkLst_ForEachSafe(curr, next,  &transportSession->sessionArray) {
         HgfsSessionInfo *session = DblLnkLst_Container(curr, HgfsSessionInfo, links);
         HgfsServerTransportRemoveSessionFromList(transportSession, session);
         HgfsServerSessionPut(session);
      }

      MXUser_ReleaseExclLock(transportSession->sessionArrayLock);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerInitHandleCounter --
 *
 *    Initialize the file handle counter to the new value passed.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerInitHandleCounter(uint32 newHandleCounter)
{
   Atomic_Write(&hgfsHandleCounter, newHandleCounter);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetHandleCounter --
 *
 *    Return file handle counter. This is used by the checkpointing code to
 *    checkpoint this value so we avoid the risk of handle collision.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static uint32
HgfsServerGetHandleCounter(void)
{
   return Atomic_Read(&hgfsHandleCounter);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetNextHandleCounter --
 *
 *    Return file handle counter. This is used by the checkpointing code to
 *    checkpoint this value so we avoid the risk of handle collision.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static uint32
HgfsServerGetNextHandleCounter(void)
{
   uint32 count = Atomic_FetchAndInc(&hgfsHandleCounter);
   /*
    * Call server manager for logging state updates.
    * XXX - This will have to be reworked when the server is
    * more concurrent than with the current access.
    */
   if (hgfsMgrData != NULL &&
       hgfsMgrData->logger != NULL) {
      hgfsMgrData->logger(hgfsMgrData->loggerData, count + 1);
   }
   return count;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileNode --
 *
 *    Retrieve the file node a handle refers to.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    The file node if the handle is valid (i.e. it refers to an existing file
 *    node that is currently in use).
 *    NULL if the handle is invalid.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static HgfsFileNode *
HgfsHandle2FileNode(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session) // IN: Session info
{
   unsigned int i;
   HgfsFileNode *fileNode = NULL;

   ASSERT(session);
   ASSERT(session->nodeArray);

   /* XXX: This O(n) lookup can and should be optimized. */
   for (i = 0; i < session->numNodes; i++) {
      if (session->nodeArray[i].state != FILENODE_STATE_UNUSED &&
          session->nodeArray[i].handle == handle) {
         fileNode = &session->nodeArray[i];
         break;
      }
   }

   return fileNode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFileNode2Handle --
 *
 *    Retrieve the handle that represents a file node outside of the server.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    The handle
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsHandle
HgfsFileNode2Handle(HgfsFileNode const *fileNode) // IN
{
   ASSERT(fileNode);

   return fileNode->handle;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDumpAllNodes --
 *
 *    Debugging routine; print all nodes in the nodeArray.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsDumpAllNodes(HgfsSessionInfo *session)  // IN: session info
{
   unsigned int i;

   ASSERT(session);
   ASSERT(session->nodeArray);

   Log("Dumping all nodes\n");
   for (i = 0; i < session->numNodes; i++) {
      Log("handle %u, name \"%s\", localdev %"FMT64"u, localInum %"FMT64"u %u\n",
          session->nodeArray[i].handle,
          session->nodeArray[i].utf8Name ? session->nodeArray[i].utf8Name : "NULL",
          session->nodeArray[i].localId.volumeId,
          session->nodeArray[i].localId.fileId,
          session->nodeArray[i].fileDesc);
   }
   Log("Done\n");
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileDesc --
 *
 *    Retrieve the file descriptor (host OS file handle) based on the hgfs
 *    handle.
 *
 * Results:
 *    TRUE if the handle is valid and the file desc was retrieved successfully.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2FileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                    HgfsSessionInfo *session, // IN: Session info
                    fileDesc *fd,             // OUT: OS handle (file descriptor)
                    void **fileCtx)           // OUT: OS file context
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   *fd = fileNode->fileDesc;
   if (fileCtx) {
      *fileCtx = fileNode->fileCtx;
   }
   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2AppendFlag --
 *
 *    Retrieve the append flag for the file node that corresponds to
 *    the specified hgfs handle.
 *
 * Results:
 *    TRUE if the handle is valid and append flag was retrieved successfully.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2AppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                      HgfsSessionInfo *session, // IN: Session info
                      Bool *appendFlag)         // OUT: append flag
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   *appendFlag = fileNode->flags & HGFS_FILE_NODE_APPEND_FL;
   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2LocalId --
 *
 *    Retrieve the local id for the file node that corresponds to
 *    the specified hgfs handle.
 *
 * Results:
 *    TRUE if the hgfs handle is valid and local id was retrieved successfully.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2LocalId(HgfsHandle handle,        // IN: Hgfs file handle
                   HgfsSessionInfo *session, // IN: Session info
                   HgfsLocalId *localId)     // OUT: local id info
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   ASSERT(localId);

   MXUser_AcquireExclLock(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   localId->volumeId = fileNode->localId.volumeId;
   localId->fileId = fileNode->localId.fileId;

   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2ServerLock --
 *
 *    Retrieve the serverlock information for the file node that corresponds to
 *    the specified hgfs handle. If the server is not compiled with oplock
 *    support, we always return TRUE and HGFS_LOCK_NONE.
 *
 * Results:
 *    TRUE if the hgfs handle is valid and the lock was retrieved successfully.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2ServerLock(HgfsHandle handle,        // IN: Hgfs file handle
                      HgfsSessionInfo *session, // IN: Session info
                      HgfsServerLock *lock)     // OUT: Server lock
{
#ifdef HGFS_OPLOCKS
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   ASSERT(lock);

   MXUser_AcquireExclLock(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   *lock = fileNode->serverLock;
   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
#else
   *lock = HGFS_LOCK_NONE;
   return TRUE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFileDesc2Handle --
 *
 *    Given an OS handle/fd, return file's hgfs handle.
 *
 * Results:
 *    TRUE if the node was found.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsFileDesc2Handle(fileDesc fd,              // IN: OS handle (file descriptor)
                    HgfsSessionInfo *session, // IN: Session info
                    HgfsHandle *handle)       // OUT: Hgfs file handle
{
   unsigned int i;
   Bool found = FALSE;
   HgfsFileNode *existingFileNode = NULL;

   ASSERT(session);
   ASSERT(session->nodeArray);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   for (i = 0; i < session->numNodes; i++) {
      existingFileNode = &session->nodeArray[i];
      if ((existingFileNode->state == FILENODE_STATE_IN_USE_CACHED) &&
          (existingFileNode->fileDesc == fd)) {
         *handle = HgfsFileNode2Handle(existingFileNode);
         found = TRUE;
         break;
      }
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2ShareMode --
 *
 *    Given an OS handle/fd, return the share access mode.
 *
 * Results:
 *    TRUE if the node was found.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2ShareMode(HgfsHandle handle,         // IN: Hgfs file handle
                     HgfsSessionInfo *session,  // IN: Session info
                     HgfsOpenMode *shareMode)   // OUT:share access mode
{
   Bool found = FALSE;
   HgfsFileNode *existingFileNode = NULL;
   HgfsNameStatus nameStatus;

   if (shareMode == NULL) {
      return found;
   }

   MXUser_AcquireExclLock(session->nodeArrayLock);

   existingFileNode = HgfsHandle2FileNode(handle, session);
   if (existingFileNode == NULL) {
      goto exit_unlock;
   }

   nameStatus = HgfsServerPolicy_GetShareMode(existingFileNode->shareName,
                                              existingFileNode->shareNameLen,
                                              shareMode);
   found = (nameStatus == HGFS_NAME_STATUS_COMPLETE);

exit_unlock:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileName --
 *
 *    Given an OS handle/fd, return file's hgfs name.
 *
 * Results:
 *    TRUE if the node was found.
 *    FALSE otherwise.
 *
 * Side effects:
 *    Allocates memory and makes a copy of the file name.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2FileName(HgfsHandle handle,       // IN: Hgfs file handle
                    HgfsSessionInfo *session,// IN: Session info
                    char **fileName,         // OUT: UTF8 file name
                    size_t *fileNameSize)    // OUT: UTF8 file name size
{
   Bool unused1, unused2;
   return HgfsHandle2FileNameMode(handle, session, &unused1, &unused2, fileName,
                                  fileNameSize);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileNameMode --
 *
 *    Given an OS handle/fd, return file's hgfs name and permissions
 *    associated with the corresponding shared folder.
 *
 * Results:
 *    TRUE if the node was found.
 *    FALSE otherwise.
 *
 * Side effects:
 *    Allocates memory and makes a copy of the file name.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2FileNameMode(HgfsHandle handle,       // IN: Hgfs file handle
                        HgfsSessionInfo *session,// IN: Session info
                        Bool *readPermissions,   // OUT: shared folder permissions
                        Bool *writePermissions,  // OUT: shared folder permissions
                        char **fileName,         // OUT: UTF8 file name
                        size_t *fileNameSize)    // OUT: UTF8 file name size
{
   Bool found = FALSE;
   HgfsFileNode *existingFileNode = NULL;
   char *name = NULL;
   size_t nameSize = 0;

   if ((fileName == NULL) || (fileNameSize == NULL)) {
      return found;
   }

   MXUser_AcquireExclLock(session->nodeArrayLock);

   existingFileNode = HgfsHandle2FileNode(handle, session);
   if (existingFileNode == NULL) {
      goto exit_unlock;
   }

   name = malloc(existingFileNode->utf8NameLen + 1);
   if (name == NULL) {
      goto exit_unlock;
   }
   *readPermissions = existingFileNode->shareInfo.readPermissions;
   *writePermissions = existingFileNode->shareInfo.writePermissions;
   nameSize = existingFileNode->utf8NameLen;
   memcpy(name, existingFileNode->utf8Name, nameSize);
   name[nameSize] = '\0';
   found = TRUE;

exit_unlock:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   *fileName = name;
   *fileNameSize = nameSize;

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileNameMode --
 *
 *    Given an OS handle/fd, return information needed for directory
 *    notification package: relative to the root share file name and
 *    shared folder notification handle.
 *
 * Results:
 *    TRUE if the node was found.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsHandle2NotifyInfo(HgfsHandle handle,                    // IN: Hgfs file handle
                      HgfsSessionInfo *session,             // IN: Session info
                      char **fileName,                      // OUT: UTF8 file name
                      size_t *fileNameSize,                 // OUT: UTF8 file name size
                      HgfsSharedFolderHandle *folderHandle) // OUT: shared folder handle
{
   Bool found = FALSE;
   HgfsFileNode *existingFileNode;
   char *name;
   size_t nameSize;

   ASSERT(fileName != NULL && fileNameSize != NULL);
   MXUser_AcquireExclLock(session->nodeArrayLock);

   existingFileNode = HgfsHandle2FileNode(handle, session);
   if (NULL != existingFileNode) {
      nameSize = existingFileNode->utf8NameLen - existingFileNode->shareInfo.rootDirLen;
      name = Util_SafeMalloc(nameSize + 1);
      *folderHandle = existingFileNode->shareInfo.handle;
      memcpy(name, existingFileNode->utf8Name, nameSize);
      name[nameSize] = '\0';
      *fileName = name;
      *fileNameSize = nameSize;
      found = TRUE;
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFileHasServerLock --
 *
 *    Check if the file with the given name is already opened with a server
 *    lock on it. If the server is compiled without oplock support, we always
 *    return FALSE.
 *
 * Results:
 *    TRUE if the node was found and has an oplock.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsFileHasServerLock(const char *utf8Name,             // IN: Name in UTF8
                      HgfsSessionInfo *session,         // IN: Session info
                      HgfsServerLock *serverLock,       // OUT: Existing oplock
                      fileDesc   *fileDesc)             // OUT: Existing fd
{
#ifdef HGFS_OPLOCKS
   unsigned int i;
   Bool found = FALSE;
   ASSERT(utf8Name);

   ASSERT(session);
   ASSERT(session->nodeArray);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   for (i = 0; i < session->numNodes; i++) {
      HgfsFileNode *existingFileNode = &session->nodeArray[i];

      if ((existingFileNode->state == FILENODE_STATE_IN_USE_CACHED) &&
          (existingFileNode->serverLock != HGFS_LOCK_NONE) &&
          (!stricmp(existingFileNode->utf8Name, utf8Name))) {
         LOG(4, ("Found file with a lock: %s\n", utf8Name));
         *serverLock = existingFileNode->serverLock;
         *fileDesc = existingFileNode->fileDesc;
         found = TRUE;
         break;
      }
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetNodeCopy --
 *
 *    Make a copy of the node. The node should not be kept around for long, as
 *    the data might become stale. This is mostly a convenience function to get
 *    node fields more efficiently.
 *
 * Results:
 *    TRUE if the hgfs handle is valid and the copy was successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    Allocates memory for node.utf8Name if copyName was set to TRUE.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsGetNodeCopy(HgfsHandle handle,        // IN: Hgfs file handle
                HgfsSessionInfo *session, // IN: Session info
                Bool copyName,            // IN: Should we copy the name?
                HgfsFileNode *copy)       // IN/OUT: Copy of the node
{
   HgfsFileNode *original = NULL;
   Bool found = FALSE;

   ASSERT(copy);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   original = HgfsHandle2FileNode(handle, session);
   if (original == NULL) {
      goto exit;
   }

   if (copyName) {
      copy->utf8Name = malloc(original->utf8NameLen + 1);
      if (copy->utf8Name == NULL) {
         goto exit;
      }
      copy->utf8NameLen = original->utf8NameLen;
      memcpy(copy->utf8Name, original->utf8Name, copy->utf8NameLen);
      copy->utf8Name[copy->utf8NameLen] = '\0';
   } else {
      copy->utf8Name = NULL;
      copy->utf8NameLen = 0;
   }

   copy->localId = original->localId;
   copy->fileDesc = original->fileDesc;
   copy->mode = original->mode;
   copy->shareAccess = original->shareAccess;
   copy->flags = original->flags;
   copy->state = original->state;
   copy->handle = original->handle;
   copy->fileCtx = original->fileCtx;
   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return found;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsHandleIsSequentialOpen --
 *
 *    Get the Hgfs open mode this handle was originally opened with.
 *
 * Results:
 *    TRUE on success, FALSE on failure.  sequentialOpen is filled in on
 *    success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsHandleIsSequentialOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                           HgfsSessionInfo *session, // IN: Session info
                           Bool *sequentialOpen)     // OUT: If open was sequential
{
   HgfsFileNode *node;
   Bool success = FALSE;

   ASSERT(sequentialOpen);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   *sequentialOpen = node->flags & HGFS_FILE_NODE_SEQUENTIAL_FL;
   success = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return success;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsHandleIsSharedFolderOpen --
 *
 *    Find if this is a shared folder open.
 *
 * Results:
 *    TRUE on success, FALSE on failure.  sharedFolderOpen is filled in on
 *    success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsHandleIsSharedFolderOpen(HgfsHandle handle,        // IN:  Hgfs file handle
                             HgfsSessionInfo *session, // IN: Session info
                             Bool *sharedFolderOpen)   // OUT: If shared folder
{
   HgfsFileNode *node;
   Bool success = FALSE;

   ASSERT(sharedFolderOpen);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   *sharedFolderOpen = node->flags & HGFS_FILE_NODE_SHARED_FOLDER_OPEN_FL;
   success = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUpdateNodeFileDesc --
 *
 *    Given a hgfs file handle, update the node with the new file desc (OS
 *    handle) information.
 *
 * Results:
 *    TRUE if the update is successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUpdateNodeFileDesc(HgfsHandle handle,        // IN: Hgfs file handle
                       HgfsSessionInfo *session, // IN: Session info
                       fileDesc fd,              // IN: OS handle (file desc)
                       void *fileCtx)            // IN: OS file context
{
   HgfsFileNode *node;
   Bool updated = FALSE;

   MXUser_AcquireExclLock(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   node->fileDesc = fd;
   node->fileCtx = fileCtx;
   updated = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return updated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUpdateNodeServerLock --
 *
 *    Given a file desc (OS handle), update the node with the new oplock
 *    information.
 *
 * Results:
 *    TRUE if the update is successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUpdateNodeServerLock(fileDesc fd,                // IN: OS handle
                         HgfsSessionInfo *session,   // IN: Session info
                         HgfsServerLock serverLock)  // IN: new oplock
{
   unsigned int i;
   HgfsFileNode *existingFileNode = NULL;
   Bool updated = FALSE;

   ASSERT(session);
   ASSERT(session->nodeArray);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   for (i = 0; i < session->numNodes; i++) {
      existingFileNode = &session->nodeArray[i];
      if (existingFileNode->state != FILENODE_STATE_UNUSED) {
         if (existingFileNode->fileDesc == fd) {
            existingFileNode->serverLock = serverLock;
            updated = TRUE;
            break;
         }
      }
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return updated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUpdateNodeAppendFlag --
 *
 *    Given a hgfs file handle, update the node with the append flag info.
 *
 * Results:
 *    TRUE if the update is successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsUpdateNodeAppendFlag(HgfsHandle handle,        // IN: Hgfs file handle
                         HgfsSessionInfo *session, // IN: Session info
                         Bool appendFlag)          // OUT: Append flag
{
   HgfsFileNode *node;
   Bool updated = FALSE;

   MXUser_AcquireExclLock(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   if (appendFlag) {
      node->flags |= HGFS_FILE_NODE_APPEND_FL;
   }
   updated = TRUE;

exit:
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return updated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCheckOpenFlagsForShare --
 *
 *    Given an open mode check this is compatible with the mode for
 *    the share upon which the open file resides.
 *
 *    If the share is read only and mode is HGFS_OPEN_CREATE we remap
 *    it to HGFS_OPEN which is allowed if the file exists.
 *
 * Results:
 *    TRUE if the mode is compatible.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerCheckOpenFlagsForShare(HgfsFileOpenInfo *openInfo,// IN: Hgfs file handle
                                 HgfsOpenFlags *flags)      // IN/OUT: open mode
{
   Bool status = TRUE;
   HgfsNameStatus nameStatus;
   HgfsOpenMode shareMode;
   char const *inEnd;
   char const *next;
   int len;

   ASSERT(openInfo);
   ASSERT(flags);

   inEnd = openInfo->cpName + openInfo->cpNameSize;

   /* The share name is the first component of the cross-platform name. */
   len = CPName_GetComponent(openInfo->cpName, inEnd, &next);
   if (len < 0) {
      LOG(4, ("%s: get first component failed\n", __FUNCTION__));
      status = FALSE;
      goto exit;
   }

   nameStatus = HgfsServerPolicy_GetShareMode(openInfo->cpName, len,
                                              &shareMode);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      status = FALSE;
      goto exit;
   }

   if (shareMode == HGFS_OPEN_MODE_READ_ONLY) {
      /* Read only share we may have work to do. */
      if (*flags != HGFS_OPEN && *flags != HGFS_OPEN_CREATE) {
         status = FALSE;
         goto exit;
      }
      if (*flags == HGFS_OPEN_CREATE) {
         /*
          * Map open or create, to just open, which will fail if
          * if the file does not exist, which it is okay, as creating
          * a new file is not allowed and should be failed.
          */
         *flags = HGFS_OPEN;
      }
   }

exit:
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDumpAllSearches --
 *
 *    Debugging routine; print all searches in the searchArray.
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsDumpAllSearches(HgfsSessionInfo *session)   // IN: session info
{
   unsigned int i;

   ASSERT(session);
   ASSERT(session->searchArray);

   Log("Dumping all searches\n");
   for (i = 0; i < session->numSearches; i++) {
      Log("handle %u, baseDir \"%s\"\n",
          session->searchArray[i].handle,
          session->searchArray[i].utf8Dir ?
          session->searchArray[i].utf8Dir : "(NULL)");
   }
   Log("Done\n");
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetNewNode --
 *
 *    Remove a node from the free list and return it. Nodes on
 *    the free list should already be initialized.
 *
 *    If the free list is empty, reallocates more memory,
 *    initializes it appropriately, adds the new entries to the
 *    free list, and then returns one off the free list.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    An unused file node on success
 *    NULL on failure
 *
 * Side effects:
 *    Memory allocation (potentially).
 *
 *-----------------------------------------------------------------------------
 */

static HgfsFileNode *
HgfsGetNewNode(HgfsSessionInfo *session)  // IN: session info
{
   HgfsFileNode *node;
   HgfsFileNode *newMem;
   unsigned int newNumNodes;
   unsigned int i;

   ASSERT(session);
   ASSERT(session->nodeArray);

   LOG(4, ("%s: entered\n", __FUNCTION__));

   if (!DblLnkLst_IsLinked(&session->nodeFreeList)) {
      /*
       * This has to be unsigned and with maximum bit length. This is
       * required to take care of "negative" differences as well.
       */
      uintptr_t ptrDiff;

      if (DOLOG(4)) {
         Log("Dumping nodes before realloc\n");
         HgfsDumpAllNodes(session);
      }

      /* Try to get twice as much memory as we had */
      newNumNodes = 2 * session->numNodes;
      newMem = (HgfsFileNode *)realloc(session->nodeArray,
                                       newNumNodes * sizeof *(session->nodeArray));
      if (!newMem) {
         LOG(4, ("%s: can't realloc more nodes\n", __FUNCTION__));

         return NULL;
      }

      ptrDiff = (char *)newMem - (char *)session->nodeArray;
      if (ptrDiff) {
         size_t const oldSize = session->numNodes * sizeof *(session->nodeArray);

         /*
          * The portion of memory that contains all our file nodes moved.
          * All pointers that pointed inside the previous portion of memory
          * must be updated to point to the new portion of memory.
          *
          * We'll need to lock this if we multithread.
          */

         LOG(4, ("Rebasing pointers, diff is %"FMTSZ"u, sizeof node is "
                  "%"FMTSZ"u\n", ptrDiff, sizeof(HgfsFileNode)));
         LOG(4, ("old: %p new: %p\n", session->nodeArray, newMem));
         ASSERT(newMem == (HgfsFileNode *)((char*)session->nodeArray + ptrDiff));

#define HgfsServerRebase(_ptr, _type)                                   \
   if ((size_t)((char *)_ptr - (char *)session->nodeArray) < oldSize) { \
      _ptr = (_type *)((char *)_ptr + ptrDiff);                         \
   }

         /*
          * Rebase the links of all file nodes
          */
         for (i = 0; i < session->numNodes; i++) {
            HgfsServerRebase(newMem[i].links.prev, DblLnkLst_Links)
            HgfsServerRebase(newMem[i].links.next, DblLnkLst_Links)
         }

         /*
          * There is no need to rebase the anchor of the file node free list
          * because if we are here, it is empty.
          */

         /* Rebase the anchor of the cached file nodes list. */
         HgfsServerRebase(session->nodeCachedList.prev, DblLnkLst_Links)
         HgfsServerRebase(session->nodeCachedList.next, DblLnkLst_Links)

#undef HgfsServerRebase
      }

      /* Initialize the new nodes */
      LOG(4, ("numNodes was %u, now is %u\n", session->numNodes, newNumNodes));
      for (i = session->numNodes; i < newNumNodes; i++) {
         DblLnkLst_Init(&newMem[i].links);

         newMem[i].state = FILENODE_STATE_UNUSED;
         newMem[i].utf8Name = NULL;
         newMem[i].utf8NameLen = 0;
         newMem[i].fileCtx = NULL;

         /* Append at the end of the list */
         DblLnkLst_LinkLast(&session->nodeFreeList, &newMem[i].links);
      }
      session->nodeArray = newMem;
      session->numNodes = newNumNodes;

      if (DOLOG(4)) {
         Log("Dumping nodes after pointer changes\n");
         HgfsDumpAllNodes(session);
      }
   }

   /* Remove the first item from the list */
   node = DblLnkLst_Container(session->nodeFreeList.next, HgfsFileNode, links);
   DblLnkLst_Unlink1(&node->links);

   return node;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveFileNode --
 *
 *    Free its localname, clear its fields, return it to the free list.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    node->utf8Name is freed.
 *    node->state is set to FILENODE_STATE_UNUSED.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsRemoveFileNode(HgfsFileNode *node,        // IN: file node
                   HgfsSessionInfo *session)  // IN: session info
{
   ASSERT(node);

   LOG(4, ("%s: handle %u, name %s, fileId %"FMT64"u\n", __FUNCTION__,
           HgfsFileNode2Handle(node), node->utf8Name, node->localId.fileId));

   if (node->shareName) {
      free(node->shareName);
   }
   node->shareName = NULL;

   if (node->utf8Name) {
      free(node->utf8Name);
   }
   node->utf8Name = NULL;
   node->state = FILENODE_STATE_UNUSED;
   ASSERT(node->fileCtx == NULL);
   node->fileCtx = NULL;

   /* Prepend at the beginning of the list */
   DblLnkLst_LinkFirst(&session->nodeFreeList, &node->links);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFreeFileNodeInternal --
 *
 *    Free its localname, clear its fields, return it to the free list.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    node->utf8Name is freed.
 *    node->state is set to FILENODE_STATE_UNUSED.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsFreeFileNodeInternal(HgfsHandle handle,        // IN: Handle to free
                         HgfsSessionInfo *session) // IN: Session info
{
   HgfsFileNode *node = HgfsHandle2FileNode(handle, session);
   ASSERT(node);
   HgfsRemoveFileNode(node, session);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFreeFileNode --
 *
 *    Free its localname, clear its fields, return it to the free list.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    node->utf8Name is freed.
 *    node->state is set to FILENODE_STATE_UNUSED.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsFreeFileNode(HgfsHandle handle,         // IN: Handle to free
                 HgfsSessionInfo *session)  // IN: Session info
{
   MXUser_AcquireExclLock(session->nodeArrayLock);
   HgfsFreeFileNodeInternal(handle, session);
   MXUser_ReleaseExclLock(session->nodeArrayLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddNewFileNode --
 *
 *    Gets a free node off the free list, sets its name, localId info,
 *    file descriptor and permissions.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    A pointer to the newly added node on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsFileNode *
HgfsAddNewFileNode(HgfsFileOpenInfo *openInfo,  // IN: open info struct
                   HgfsLocalId const *localId,  // IN: Local unique file ID
                   fileDesc fileDesc,           // IN: File Handle
                   Bool append,                 // IN: open with append flag
                   size_t shareNameLen,         // IN: share name byte length
                   char const *shareName,       // IN: share name
                   Bool sharedFolderOpen,       // IN: shared folder only open
                   HgfsSessionInfo *session)    // IN: session info
{
   HgfsFileNode *newNode;
   char* rootDir;

   ASSERT(openInfo);
   ASSERT(localId);
   ASSERT(session);

   /* This was already verified in HgfsUnpackOpenRequest... */
   ASSERT(openInfo->mask & HGFS_OPEN_VALID_FILE_NAME);

   /* Get an unused node */
   newNode = HgfsGetNewNode(session);
   if (!newNode) {
      LOG(4, ("%s: out of memory\n", __FUNCTION__));

      return NULL;
   }

   /* Set new node's fields */
   if (!HgfsServerGetOpenMode(openInfo, &newNode->mode)) {
      HgfsRemoveFileNode(newNode, session);
      return NULL;
   }

   /*
    * Save a copy of the share name so we can look up its
    * access mode at various times over the node's lifecycle.
    */
   newNode->shareName = malloc(shareNameLen + 1);
   if (newNode->shareName == NULL) {
      LOG(4, ("%s: out of memory\n", __FUNCTION__));
      HgfsRemoveFileNode(newNode, session);
      return NULL;
   }
   memcpy(newNode->shareName, shareName, shareNameLen);
   newNode->shareName[shareNameLen] = '\0';
   newNode->shareNameLen = shareNameLen;

   newNode->utf8NameLen = strlen(openInfo->utf8Name);
   newNode->utf8Name = malloc(newNode->utf8NameLen + 1);
   if (newNode->utf8Name == NULL) {
      LOG(4, ("%s: out of memory\n", __FUNCTION__));
      HgfsRemoveFileNode(newNode, session);
      return NULL;
   }
   memcpy(newNode->utf8Name, openInfo->utf8Name, newNode->utf8NameLen);
   newNode->utf8Name[newNode->utf8NameLen] = '\0';

   newNode->shareInfo.rootDirLen = strlen(openInfo->shareInfo.rootDir);
   rootDir = malloc(newNode->shareInfo.rootDirLen + 1);
   if (rootDir == NULL) {
      LOG(4, ("HgfsAddNewFileNode: out of memory\n"));
      HgfsRemoveFileNode(newNode, session);
      return NULL;
   }
   memcpy(rootDir, openInfo->shareInfo.rootDir, newNode->shareInfo.rootDirLen);
   rootDir[newNode->shareInfo.rootDirLen] = '\0';
   newNode->shareInfo.rootDir = rootDir;

   newNode->handle = HgfsServerGetNextHandleCounter();
   newNode->localId = *localId;
   newNode->fileDesc = fileDesc;
   newNode->shareAccess = (openInfo->mask & HGFS_OPEN_VALID_SHARE_ACCESS) ?
      openInfo->shareAccess : HGFS_DEFAULT_SHARE_ACCESS;
   newNode->flags = 0;

   if (append) {
      newNode->flags |= HGFS_FILE_NODE_APPEND_FL;
   }
   if (sharedFolderOpen) {
      newNode->flags |= HGFS_FILE_NODE_SHARED_FOLDER_OPEN_FL;
   }
   if (HGFS_OPEN_MODE_FLAGS(openInfo->mode) & HGFS_OPEN_SEQUENTIAL) {
      newNode->flags |= HGFS_FILE_NODE_SEQUENTIAL_FL;
   }

   newNode->serverLock = openInfo->acquiredLock;
   newNode->state = FILENODE_STATE_IN_USE_NOT_CACHED;
   newNode->shareInfo.readPermissions = openInfo->shareInfo.readPermissions;
   newNode->shareInfo.writePermissions = openInfo->shareInfo.writePermissions;
   newNode->shareInfo.handle = openInfo->shareInfo.handle;

   LOG(4, ("%s: got new node, handle %u\n", __FUNCTION__,
           HgfsFileNode2Handle(newNode)));
   return newNode;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddToCacheInternal --
 *
 *    Adds the node to cache. If the number of nodes in the cache exceed
 *    the maximum number of entries then the first node is removed. The
 *    first node should be the least recently used.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsAddToCacheInternal(HgfsHandle handle,         // IN: HGFS file handle
                       HgfsSessionInfo *session)  // IN: Session info
{
   HgfsFileNode *node;

   /* Check if the node is already cached. */
   if (HgfsIsCachedInternal(handle, session)) {
      ASSERT((node = HgfsHandle2FileNode(handle, session)) &&
             node->state == FILENODE_STATE_IN_USE_CACHED);
      return TRUE;
   }

   /* Remove the LRU node if the list is full. */
   if (session->numCachedOpenNodes == maxCachedOpenNodes) {
      if (!HgfsRemoveLruNode(session)) {
         LOG(4, ("%s: Unable to remove LRU node from cache.\n",
                 __FUNCTION__));

         return FALSE;
      }
   }

   ASSERT_BUG(36244, session->numCachedOpenNodes < maxCachedOpenNodes);

   node = HgfsHandle2FileNode(handle, session);
   ASSERT(node);
   /* Append at the end of the list. */
   DblLnkLst_LinkLast(&session->nodeCachedList, &node->links);

   node->state = FILENODE_STATE_IN_USE_CACHED;
   session->numCachedOpenNodes++;

   /*
    * Keep track of how many open nodes we have with
    * server locks on them. The locked file should
    * always be present in the node cache. So we keep
    * the number of the files that have locks on them
    * limited, and smaller than the number of maximum
    * nodes in the cache.
    */

   if (node->serverLock != HGFS_LOCK_NONE) {
      session->numCachedLockedNodes++;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveFromCacheInternal --
 *
 *    Remove the specified node from the cache and close the associated
 *    file descriptor. If the node was not already in the cache then nothing
 *    is done.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsRemoveFromCacheInternal(HgfsHandle handle,        // IN: Hgfs handle to the node
                            HgfsSessionInfo *session) // IN: Session info
{
   HgfsFileNode *node;

   ASSERT(session);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      LOG(4, ("%s: invalid handle.\n", __FUNCTION__));

      return FALSE;
   }

   if (node->state == FILENODE_STATE_IN_USE_CACHED) {
      /* Unlink the node from the list of cached fileNodes. */
      DblLnkLst_Unlink1(&node->links);
      node->state = FILENODE_STATE_IN_USE_NOT_CACHED;
      session->numCachedOpenNodes--;
      LOG(4, ("%s: cache entries %u remove node %s id %"FMT64"u fd %u .\n",
              __FUNCTION__, session->numCachedOpenNodes, node->utf8Name,
              node->localId.fileId, node->fileDesc));

      /*
       * XXX: From this point and up in the call chain (i.e. this function and
       * all callers), Bool is returned instead of the HgfsInternalStatus.
       * HgfsCloseFile returns HgfsInternalStatus, which is far more granular,
       * but modifying this stack to use HgfsInternalStatus instead of Bool is
       * not worth it, as we'd have to #define per-platform error codes for
       * things like "ran out of memory", "bad file handle", etc.
       *
       * Instead, we'll just await the lobotomization of the node cache to
       * really fix this.
       */
      if (HgfsCloseFile(node->fileDesc, node->fileCtx)) {
         LOG(4, ("%s: Could not close fd %u\n", __FUNCTION__, node->fileDesc));

         return FALSE;
      }
      node->fileCtx = NULL;

     /*
      * If we have just removed the node then the number of used nodes better
      * be less than the max. If we didn't remove a node, it means the
      * node we tried to remove was not in the cache to begin with, and
      * we have a problem (see bug 36244).
      */

      ASSERT(session->numCachedOpenNodes < maxCachedOpenNodes);
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsCachedInternal --
 *
 *    Check if the node exists in the cache. If the node is found in
 *    the cache then move it to the end of the list. Most recently
 *    used nodes move towards the end of the list.
 *
 *    The session nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    TRUE if the node is found in the cache.
 *    FALSE if the node is not in the cache.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsIsCachedInternal(HgfsHandle handle,         // IN: Structure representing file node
                     HgfsSessionInfo *session)  // IN: Session info
{
   HgfsFileNode *node;

   ASSERT(session);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      LOG(4, ("%s: invalid handle.\n", __FUNCTION__));

      return FALSE;
   }

   if (node->state == FILENODE_STATE_IN_USE_CACHED) {
      /*
       * Move this node to the end of the list.
       */
      DblLnkLst_Unlink1(&node->links);
      DblLnkLst_LinkLast(&session->nodeCachedList, &node->links);

      return TRUE;
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsServerLockAllowed --
 *
 *    Check if there's room for another file node with the server lock.
 *    If there's no room in the cache for the file with the server lock,
 *    then the file will be opened without the lock even if the client
 *    asked for the lock.
 *
 *
 * Results:
 *    TRUE if the node is found in the cache.
 *    FALSE if the node is not in the cache.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsIsServerLockAllowed(HgfsSessionInfo *session)  // IN: session info
{
   Bool allowed;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   allowed = session->numCachedLockedNodes < MAX_LOCKED_FILENODES;
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return allowed;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetNewSearch --
 *
 *    Remove a search from the free list and return it. Searches on
 *    the free list should already be initialized.
 *
 *    If the free list is empty, reallocates more memory,
 *    initializes it appropriately, adds the new entries to the
 *    free list, and then returns one off the free list.
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Results:
 *    An unused search on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsSearch *
HgfsGetNewSearch(HgfsSessionInfo *session)  // IN: session info
{
   HgfsSearch *search;
   HgfsSearch *newMem;
   unsigned int newNumSearches;
   unsigned int i;

   ASSERT(session);
   ASSERT(session->searchArray);

   LOG(4, ("%s: entered\n", __FUNCTION__));

   if (!DblLnkLst_IsLinked(&session->searchFreeList)) {
      /*
       * This has to be unsigned and with maximum bit length. This is
       * required to take care of "negative" differences as well.
       */

      uintptr_t ptrDiff;

      if (DOLOG(4)) {
         Log("Dumping searches before realloc\n");
         HgfsDumpAllSearches(session);
      }

      /* Try to get twice as much memory as we had */
      newNumSearches = 2 * session->numSearches;
      newMem = (HgfsSearch *)realloc(session->searchArray,
                                     newNumSearches * sizeof *(session->searchArray));
      if (!newMem) {
         LOG(4, ("%s: can't realloc more searches\n", __FUNCTION__));

         return NULL;
      }

      ptrDiff = (char *)newMem - (char *)session->searchArray;
      if (ptrDiff) {
         size_t const oldSize = session->numSearches * sizeof *(session->searchArray);

         /*
          * The portion of memory that contains all our searches moved.
          * All pointers that pointed inside the previous portion of memory
          * must be updated to point to the new portion of memory.
          */

         LOG(4, ("Rebasing pointers, diff is %"FMTSZ"u, sizeof search is "
                 "%"FMTSZ"u\n", ptrDiff, sizeof(HgfsSearch)));
         LOG(4, ("old: %p new: %p\n", session->searchArray, newMem));
         ASSERT(newMem == (HgfsSearch*)((char*)session->searchArray + ptrDiff));

#define HgfsServerRebase(_ptr, _type)                                     \
   if ((size_t)((char *)_ptr - (char *)session->searchArray) < oldSize) { \
      _ptr = (_type *)((char *)_ptr + ptrDiff);                           \
   }

         /*
          * Rebase the links of all searches
          */

         for (i = 0; i < session->numSearches; i++) {
            HgfsServerRebase(newMem[i].links.prev, DblLnkLst_Links)
            HgfsServerRebase(newMem[i].links.next, DblLnkLst_Links)
         }

         /*
          * There is no need to rebase the links of the search free list
          * because if we are here, it is empty
          */

#undef HgfsServerRebase
      }

      /* Initialize the new searches */
      LOG(4, ("numSearches was %u, now is %u\n", session->numSearches,
               newNumSearches));

      for (i = session->numSearches; i < newNumSearches; i++) {
         DblLnkLst_Init(&newMem[i].links);
         newMem[i].utf8Dir = NULL;
         newMem[i].utf8DirLen = 0;
         newMem[i].utf8ShareName = NULL;
         newMem[i].utf8ShareNameLen = 0;
         newMem[i].shareInfo.rootDir = NULL;
         newMem[i].shareInfo.rootDirLen = 0;
         newMem[i].dents = NULL;
         newMem[i].numDents = 0;

         /* Append at the end of the list */
         DblLnkLst_LinkLast(&session->searchFreeList, &newMem[i].links);
      }
      session->searchArray = newMem;
      session->numSearches = newNumSearches;

      if (DOLOG(4)) {
         Log("Dumping searches after pointer changes\n");
         HgfsDumpAllSearches(session);
      }
   }

   /* Remove the first item from the list */
   search = DblLnkLst_Container(session->searchFreeList.next, HgfsSearch, links);
   DblLnkLst_Unlink1(&search->links);

   return search;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSearch2SearchHandle --
 *
 *    Retrieve the handle that represents a search outside of the server.
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Results:
 *    The handle
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsHandle
HgfsSearch2SearchHandle(HgfsSearch const *search) // IN
{
   ASSERT(search);

   return search->handle;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSearchIsBaseNameSpace --
 *
 *    Check if the search is the base of our name space, i.e. the dirents are
 *    the shares themselves.
 *
 * Results:
 *    TRUE if the search is the base of the name space, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsSearchIsBaseNameSpace(HgfsSearch const *search) // IN
{
   ASSERT(search);

   return search->type == DIRECTORY_SEARCH_TYPE_BASE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetSearchCopy --
 *
 *    Make a copy of the search. It should not be kept around for long, as the
 *    data might become stale. This is mostly a convenience function to get
 *    search fields more efficiently.
 *
 *    Note that unlike HgfsGetNodeCopy, we always copy the name, and we never
 *    copy the dents.
 *
 * Results:
 *    TRUE if the hgfs handle is valid and the copy was successful.
 *    FALSE otherwise.
 *
 * Side effects:
 *    Allocates memory for search.utf8Dir
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsGetSearchCopy(HgfsHandle handle,        // IN: Hgfs search handle
                  HgfsSessionInfo *session, // IN: Session info
                  HgfsSearch *copy)         // IN/OUT: Copy of the search
{
   HgfsSearch *original = NULL;
   Bool found = FALSE;

   ASSERT(copy);

   MXUser_AcquireExclLock(session->searchArrayLock);
   original = HgfsSearchHandle2Search(handle, session);
   if (original == NULL) {
      goto exit;
   }

   copy->utf8Dir = malloc(original->utf8DirLen + 1);
   if (copy->utf8Dir == NULL) {
      goto exit;
   }
   copy->utf8DirLen = original->utf8DirLen;
   memcpy(copy->utf8Dir, original->utf8Dir, copy->utf8DirLen);
   copy->utf8Dir[copy->utf8DirLen] = '\0';

   copy->utf8ShareName = malloc(original->utf8ShareNameLen + 1);
   if (copy->utf8ShareName == NULL) {
      goto exit;
   }
   copy->utf8ShareNameLen = original->utf8ShareNameLen;
   memcpy(copy->utf8ShareName, original->utf8ShareName, copy->utf8ShareNameLen);
   copy->utf8ShareName[copy->utf8ShareNameLen] = '\0';

   /* No dents for the copy, they consume too much memory and aren't needed. */
   copy->dents = NULL;
   copy->numDents = 0;

   copy->handle = original->handle;
   copy->type = original->type;
   found = TRUE;

exit:
   MXUser_ReleaseExclLock(session->searchArrayLock);

   return found;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddNewSearch --
 *
 *    Gets a free search off the free list, sets its base directory, dents,
 *    and type.
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Results:
 *    A pointer to the newly added search on success
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSearch *
HgfsAddNewSearch(char const *utf8Dir,       // IN: UTF8 name of dir to search in
                 DirectorySearchType type,  // IN: What kind of search is this?
                 char const *utf8ShareName, // IN: Share name containing the directory
                 char const *rootDir,       // IN: Root directory for the share
                 HgfsSessionInfo *session)  // IN: Session info
{
   HgfsSearch *newSearch;

   ASSERT(utf8Dir);

   /* Get an unused search */
   newSearch = HgfsGetNewSearch(session);
   if (!newSearch) {
      LOG(4, ("%s: out of memory\n", __FUNCTION__));

      return NULL;
   }

   newSearch->dents = NULL;
   newSearch->numDents = 0;
   newSearch->type = type;
   newSearch->handle = HgfsServerGetNextHandleCounter();

   newSearch->utf8DirLen = strlen(utf8Dir);
   newSearch->utf8Dir = Util_SafeStrdup(utf8Dir);

   newSearch->utf8ShareNameLen = strlen(utf8ShareName);
   newSearch->utf8ShareName = Util_SafeStrdup(utf8ShareName);

   newSearch->shareInfo.rootDirLen = strlen(rootDir);
   newSearch->shareInfo.rootDir = Util_SafeStrdup(rootDir);

   LOG(4, ("%s: got new search, handle %u\n", __FUNCTION__,
           HgfsSearch2SearchHandle(newSearch)));
   return newSearch;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFreeSearchDirents --
 *
 *    Frees all dirents and dirents pointer array.
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsFreeSearchDirents(HgfsSearch *search)       // IN/OUT: search
{
   unsigned int i;

   if (search->dents) {
      for (i = 0; i < search->numDents; i++) {
         free(search->dents[i]);
      }
      free(search->dents);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveSearchInternal --
 *
 *    Destroy a search object and recycle it to the free list
 *
 *    Caller should hold the session's searchArrayLock.
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
HgfsRemoveSearchInternal(HgfsSearch *search,       // IN: search
                         HgfsSessionInfo *session) // IN: session info
{
   ASSERT(search);
   ASSERT(session);

   LOG(4, ("%s: handle %u, dir %s\n", __FUNCTION__,
           HgfsSearch2SearchHandle(search), search->utf8Dir));

   HgfsFreeSearchDirents(search);
   free(search->utf8Dir);
   free(search->utf8ShareName);
   free((char*)search->shareInfo.rootDir);

   /* Prepend at the beginning of the list */
   DblLnkLst_LinkFirst(&session->searchFreeList, &search->links);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveSearch --
 *
 *    Wrapper around HgfsRemoveSearchInternal that first takes the lock and
 *    converts the handle to the search itself.
 *
 * Results:
 *    TRUE if the search was freed successfully.
 *    FALSE if the search could not be found.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsRemoveSearch(HgfsHandle handle,        // IN: search
                 HgfsSessionInfo *session) // IN: session info
{
   HgfsSearch *search;
   Bool success = FALSE;

   MXUser_AcquireExclLock(session->searchArrayLock);

   search = HgfsSearchHandle2Search(handle, session);
   if (search != NULL) {
      HgfsRemoveSearchInternal(search, session);
      success = TRUE;
   }

   MXUser_ReleaseExclLock(session->searchArrayLock);

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetSearchResult --
 *
 *    Returns a copy of the search result at the given offset. If remove is set
 *    to TRUE, the existing result is also pruned and the remaining results
 *    are shifted up in the result array.
 *
 * Results:
 *    NULL if there was an error or no search results were left.
 *    Non-NULL if result was found. Caller must free it.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

DirectoryEntry *
HgfsGetSearchResult(HgfsHandle handle,         // IN: Handle to search
                    HgfsSessionInfo *session,  // IN: Session info
                    uint32 offset,             // IN: Offset to retrieve at
                    Bool remove)               // IN: If true, removes the result
{
   HgfsSearch *search;
   DirectoryEntry *dent = NULL;

   MXUser_AcquireExclLock(session->searchArrayLock);

   search = HgfsSearchHandle2Search(handle, session);
   if (search == NULL || search->dents == NULL) {
      goto out;
   }

   if (offset >= search->numDents) {
      goto out;
   }

   /* If we're not removing the result, we need to make a copy of it. */
   if (remove) {
      /*
       * We're going to shift the dents array, overwriting the dent pointer at
       * offset, so first we need to save said pointer so that we can return it
       * later to the caller.
       */
      dent = search->dents[offset];

      /* Shift up the remaining results */
      memmove(&search->dents[offset], &search->dents[offset + 1],
              (search->numDents - (offset + 1)) * sizeof search->dents[0]);

      /* Decrement the number of results */
      search->numDents--;
   } else {
      DirectoryEntry *originalDent;
      size_t nameLen;

      originalDent = search->dents[offset];
      ASSERT(originalDent);

      nameLen = strlen(originalDent->d_name);
      /*
       * Make sure the name will not overrun the d_name buffer, the end of
       * which is also the end of the DirectoryEntry.
       */
      ASSERT(offsetof(DirectoryEntry, d_name) + nameLen < originalDent->d_reclen);

      dent = malloc(originalDent->d_reclen);
      if (dent == NULL) {
         goto out;
      }

      /*
       * Yes, there are more members than this in a dirent. But if you look
       * at the top of hgfsServerInt.h, you'll see that on Windows we only
       * define d_reclen and d_name, as those are the only fields we need.
       */
      dent->d_reclen = originalDent->d_reclen;
      memcpy(dent->d_name, originalDent->d_name, nameLen);
      dent->d_name[nameLen] = 0;
   }

  out:
   MXUser_ReleaseExclLock(session->searchArrayLock);

   return dent;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSearchHandle2Search --
 *
 *    Retrieve the search a handle refers to.
 *
 * Results:
 *    The search if the handle is valid (i.e. it refers to an existing search
 *     that is currently in use)
 *    NULL if the handle is invalid
 *
 *    Caller should hold the session's searchArrayLock.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSearch *
HgfsSearchHandle2Search(HgfsHandle handle,         // IN: handle
                        HgfsSessionInfo *session)  // IN: session info
{
   unsigned int i;
   HgfsSearch *search = NULL;

   ASSERT(session);
   ASSERT(session->searchArray);

   /* XXX: This O(n) lookup can and should be optimized. */
   for (i = 0; i < session->numSearches; i++) {
      if (!DblLnkLst_IsLinked(&session->searchArray[i].links) &&
          session->searchArray[i].handle == handle) {
         search = &session->searchArray[i];
         break;
      }
   }

   return search;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUpdateNodeNames --
 *
 *    Walk the node array and update all nodes that have the old file name to
 *    store the new file name.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    If there isnt enough memory to accomodate the new names, those file nodes
 *    that couldnt be updated are deleted.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsUpdateNodeNames(const char *oldLocalName,  // IN: Name of file to look for
                    const char *newLocalName,  // IN: Name to replace with
                    HgfsSessionInfo *session)  // IN: Session info
{
   HgfsFileNode *fileNode;
   unsigned int i;
   char *newBuffer;
   size_t newBufferLen;

   ASSERT(oldLocalName);
   ASSERT(newLocalName);
   ASSERT(session);
   ASSERT(session->nodeArray);

   newBufferLen = strlen(newLocalName);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   for (i = 0; i < session->numNodes; i++) {
      fileNode = &session->nodeArray[i];

      /* If the node is on the free list, skip it. */
      if (fileNode->state == FILENODE_STATE_UNUSED) {
         continue;
      }

      if (strcmp(fileNode->utf8Name, oldLocalName) == 0) {
         newBuffer = malloc(newBufferLen + 1);
         if (!newBuffer) {
            LOG(4, ("%s: Failed to update a node name.\n", __FUNCTION__));
            continue;
         }
         memcpy(newBuffer, newLocalName, newBufferLen);
         newBuffer[newBufferLen] = '\0';

         /* Update this name to the new name. */
         free(fileNode->utf8Name);
         fileNode->utf8Name = newBuffer;
         fileNode->utf8NameLen = newBufferLen;
      }
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerClose --
 *
 *    Handle a Close request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerClose(HgfsInputParam *input)  // IN: Input params
{
   HgfsHandle file;
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackCloseRequest(input->payload, input->payloadSize,
                              input->op, &file)) {
      LOG(4, ("%s: close fh %u\n", __FUNCTION__, file));

      if (!HgfsRemoveFromCache(file, input->session)) {
         LOG(4, ("%s: Could not remove the node from cache.\n", __FUNCTION__));
         status = HGFS_ERROR_INTERNAL;
      } else {
         HgfsFreeFileNode(file, input->session);
         if (!HgfsPackCloseReply(input->packet, input->metaPacket, input->op,
                                 &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_INTERNAL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchClose --
 *
 *    Handle a "Search Close" request.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSearchClose(HgfsInputParam *input)  // IN: Input params
{
   HgfsHandle search;
   HgfsInternalStatus status;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackSearchCloseRequest(input->payload, input->payloadSize,
                                    input->op, &search)) {
      LOG(4, ("%s: close search #%u\n", __FUNCTION__, search));

      if (HgfsRemoveSearch(search, input->session)) {
         if (HgfsPackSearchCloseReply(input->packet, input->metaPacket,
                                      input->op,
                                      &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_SUCCESS;
         } else {
            status = HGFS_ERROR_INTERNAL;
         }
      } else {
         /* Invalid handle */
         LOG(4, ("%s: invalid handle %u\n", __FUNCTION__, search));
         status = HGFS_ERROR_INTERNAL;
      }
   } else {
      status = HGFS_ERROR_INTERNAL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


#define HGFS_SIZEOF_OP(type) (sizeof (type) + sizeof (HgfsRequest))

/* Opcode handlers, indexed by opcode */
static struct {
   void (*handler)(HgfsInputParam *input);
   /* Minimal size of the request packet */
   unsigned int minReqSize;

   /* How do you process the request {sync, async} ? */
   RequestHint reqType;

} const handlers[] = {
   { HgfsServerOpen,             sizeof (HgfsRequestOpen),              REQ_SYNC },
   { HgfsServerRead,             sizeof (HgfsRequestRead),              REQ_SYNC },
   { HgfsServerWrite,            sizeof (HgfsRequestWrite),             REQ_SYNC },
   { HgfsServerClose,            sizeof (HgfsRequestClose),             REQ_SYNC },
   { HgfsServerSearchOpen,       sizeof (HgfsRequestSearchOpen),        REQ_SYNC },
   { HgfsServerSearchRead,       sizeof (HgfsRequestSearchRead),        REQ_SYNC },
   { HgfsServerSearchClose,      sizeof (HgfsRequestSearchClose),       REQ_SYNC },
   { HgfsServerGetattr,          sizeof (HgfsRequestGetattr),           REQ_SYNC },
   { HgfsServerSetattr,          sizeof (HgfsRequestSetattr),           REQ_SYNC },
   { HgfsServerCreateDir,        sizeof (HgfsRequestCreateDir),         REQ_SYNC },
   { HgfsServerDeleteFile,       sizeof (HgfsRequestDelete),            REQ_SYNC },
   { HgfsServerDeleteDir,        sizeof (HgfsRequestDelete),            REQ_SYNC },
   { HgfsServerRename,           sizeof (HgfsRequestRename),            REQ_SYNC },
   { HgfsServerQueryVolume,      sizeof (HgfsRequestQueryVolume),       REQ_SYNC },

   { HgfsServerOpen,             sizeof (HgfsRequestOpenV2),            REQ_SYNC },
   { HgfsServerGetattr,          sizeof (HgfsRequestGetattrV2),         REQ_SYNC },
   { HgfsServerSetattr,          sizeof (HgfsRequestSetattrV2),         REQ_SYNC },
   { HgfsServerSearchRead,       sizeof (HgfsRequestSearchReadV2),      REQ_SYNC },
   { HgfsServerSymlinkCreate,    sizeof (HgfsRequestSymlinkCreate),     REQ_SYNC },
   { HgfsServerServerLockChange, sizeof (HgfsRequestServerLockChange),  REQ_SYNC },
   { HgfsServerCreateDir,        sizeof (HgfsRequestCreateDirV2),       REQ_SYNC },
   { HgfsServerDeleteFile,       sizeof (HgfsRequestDeleteV2),          REQ_SYNC },
   { HgfsServerDeleteDir,        sizeof (HgfsRequestDeleteV2),          REQ_SYNC },
   { HgfsServerRename,           sizeof (HgfsRequestRenameV2),          REQ_SYNC },

   { HgfsServerOpen,             HGFS_SIZEOF_OP(HgfsRequestOpenV3),             REQ_SYNC },
   { HgfsServerRead,             HGFS_SIZEOF_OP(HgfsRequestReadV3),             REQ_SYNC },
   { HgfsServerWrite,            HGFS_SIZEOF_OP(HgfsRequestWriteV3),            REQ_SYNC },
   { HgfsServerClose,            HGFS_SIZEOF_OP(HgfsRequestCloseV3),            REQ_SYNC },
   { HgfsServerSearchOpen,       HGFS_SIZEOF_OP(HgfsRequestSearchOpenV3),       REQ_SYNC },
   { HgfsServerSearchRead,       HGFS_SIZEOF_OP(HgfsRequestSearchReadV3),       REQ_SYNC },
   { HgfsServerSearchClose,      HGFS_SIZEOF_OP(HgfsRequestSearchCloseV3),      REQ_SYNC },
   { HgfsServerGetattr,          HGFS_SIZEOF_OP(HgfsRequestGetattrV3),          REQ_SYNC },
   { HgfsServerSetattr,          HGFS_SIZEOF_OP(HgfsRequestSetattrV3),          REQ_SYNC },
   { HgfsServerCreateDir,        HGFS_SIZEOF_OP(HgfsRequestCreateDirV3),        REQ_SYNC },
   { HgfsServerDeleteFile,       HGFS_SIZEOF_OP(HgfsRequestDeleteV3),           REQ_SYNC },
   { HgfsServerDeleteDir,        HGFS_SIZEOF_OP(HgfsRequestDeleteV3),           REQ_SYNC },
   { HgfsServerRename,           HGFS_SIZEOF_OP(HgfsRequestRenameV3),           REQ_SYNC },
   { HgfsServerQueryVolume,      HGFS_SIZEOF_OP(HgfsRequestQueryVolumeV3),      REQ_SYNC },
   { HgfsServerSymlinkCreate,    HGFS_SIZEOF_OP(HgfsRequestSymlinkCreateV3),    REQ_SYNC },
   { HgfsServerServerLockChange, sizeof (HgfsRequestServerLockChange),          REQ_SYNC },
   { HgfsServerWriteWin32Stream, HGFS_SIZEOF_OP(HgfsRequestWriteWin32StreamV3), REQ_SYNC },
   /*
    * Starting from HGFS_OP_CREATE_SESSION_V4 (all V4 commands and above) the
    * second field is the minimum size for actual HGFS operational request
    * and not the minimum size of operational request with a header.
    */
   { HgfsServerCreateSession,    offsetof(HgfsRequestCreateSessionV4, reserved),   REQ_SYNC},
   { HgfsServerDestroySession,   offsetof(HgfsRequestDestroySessionV4, reserved),  REQ_SYNC},
   { HgfsServerRead,             sizeof (HgfsRequestReadV3),                       REQ_SYNC},
   { HgfsServerWrite,            sizeof (HgfsRequestWriteV3),                      REQ_SYNC},
   { HgfsServerSetDirNotifyWatch,    sizeof (HgfsRequestSetWatchV4),               REQ_SYNC},
   { HgfsServerRemoveDirNotifyWatch, sizeof (HgfsRequestRemoveWatchV4),            REQ_SYNC},
   { NULL,                       0,                                                REQ_SYNC}, // No Op notify
   { HgfsServerSearchRead,       sizeof (HgfsRequestSearchReadV4),                 REQ_SYNC},

};


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCompleteRequest --
 *
 *    Performs all necessary action which needed for completing HGFS request:
 *       1. Sends reply to the guest.
 *       2. Release allocated objects, mapped guest memory.
 *       3. Dereference objects that were referenced.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Reference to Session is dropped.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerCompleteRequest(HgfsInternalStatus status,   // IN: Status of the request
                          size_t replyPayloadSize,     // IN: sizeof the reply payload
                          HgfsInputParam *input)       // INOUT: request context
{
   char *packetOut;
   size_t replyPacketSize;
   size_t replySize;

   if (HGFS_ERROR_SUCCESS == status) {
      HGFS_ASSERT_INPUT(input);
   } else {
      ASSERT(input);
   }

   if (input->v4header) {
      HgfsHeader *header;
      replySize = sizeof *header + replyPayloadSize;
      replyPacketSize = replySize;
      header = HSPU_GetReplyPacket(input->packet, &replyPacketSize,
                                   input->transportSession);
      packetOut = (char *)header;

      ASSERT_DEVEL(header && (replySize <= replyPacketSize));
      if (header && (sizeof *header <= replyPacketSize)) {
         uint64 replySessionId = HGFS_INVALID_SESSION_ID;

         if (NULL != input->session) {
            replySessionId = input->session->sessionId;
         }
         HgfsPackReplyHeaderV4(status, replyPayloadSize, input->op,
                               replySessionId, input->id, header);
      }
   } else {
      HgfsReply *reply;

      /*
       * Starting from HGFS V3 header is not included in the payload size.
       */
      if (input->op < HGFS_OP_OPEN_V3) {
         replySize = MAX(replyPayloadSize, sizeof *reply);
      } else {
         replySize = sizeof *reply + replyPayloadSize;
      }
      replyPacketSize = replySize;
      reply = HSPU_GetReplyPacket(input->packet, &replyPacketSize,
                                  input->transportSession);
      packetOut = (char *)reply;

      ASSERT_DEVEL(reply && (replySize <= replyPacketSize));
      if (reply && (sizeof *reply <= replyPacketSize)) {
         HgfsPackLegacyReplyHeader(status, input->id, reply);
      }
   }
   if (!HgfsPacketSend(input->packet, packetOut, replySize,
                       input->transportSession, 0)) {
      /* Send failed. Drop the reply. */
      LOG(4, ("Error sending reply\n"));
   }

   if (NULL != input->session) {
      HgfsServerSessionPut(input->session);
   }
   HgfsServerTransportSessionPut(input->transportSession);
   free(input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerProcessRequest --
 *
 *    Dispatch an incoming packet (in packetIn) to a handler function.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerProcessRequest(void *context)
{
   HgfsInputParam *input = (HgfsInputParam *)context;
   if (!input->metaPacket) {
      input->metaPacket = HSPU_GetMetaPacket(input->packet,
                                             &input->metaPacketSize,
                                             input->transportSession);
   }

   input->payload = (char *)input->metaPacket + input->payloadOffset;
   (*handlers[input->op].handler)(input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionReceive --
 *
 *    Dispatch an incoming packet (in packetIn) to a handler function.
 *
 *    This function cannot fail; if something goes wrong, it returns
 *    a packet containing only a reply header with error code.
 *
 *    The handler function can send the reply packet either using
 *    HgfsPacketSend helper functions. This function would return error
 *    as a reply if the op handler do not return HGFS_ERROR_SUCCESS.
 *
 *    NOTE: If any op handler needs to keep packetIn around for sending replies
 *    at a later point (possibly in a different thread context), it should
 *    make a copy of it. The validity of packetIn for the HGFS server is only
 *    within the scope of this function.
 *
 *    Definitions of Meta Packet, Data packet can be looked up in
 *    hgfsChannelVmci.c
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSessionReceive(HgfsPacket *packet,      // IN: Hgfs Packet
                         void *clientData)        // IN: session info
{
   HgfsTransportSessionInfo *transportSession = (HgfsTransportSessionInfo *)clientData;
   HgfsInternalStatus status;
   HgfsInputParam *input = NULL;

   ASSERT(transportSession);

   if (transportSession->state == HGFS_SESSION_STATE_CLOSED) {
      LOG(4, ("%s: %d: Received packet after disconnected.\n", __FUNCTION__,
              __LINE__));
      return;
   }

   HgfsServerTransportSessionGet(transportSession);

   if (!HgfsParseRequest(packet, transportSession, &input, &status)) {
      LOG(4, ("%s: %d: Can't generate any response for the guest, just exit.\n ",
              __FUNCTION__, __LINE__));
      HgfsServerTransportSessionPut(transportSession);
      return;
   }

   packet->id = input->id;
   HGFS_ASSERT_MINIMUM_OP(input->op);
   if (HGFS_ERROR_SUCCESS == status) {
      HGFS_ASSERT_INPUT(input);
      if (HgfsValidatePacket(input->metaPacket, input->metaPacketSize,
                             input->v4header) &&
          (input->op < ARRAYSIZE(handlers)) &&
          (handlers[input->op].handler != NULL) &&
          (input->metaPacketSize >= handlers[input->op].minReqSize)) {
         /* Initial validation passed, process the client request now. */
         packet->processedAsync = packet->supportsAsync &&
                                         (handlers[input->op].reqType == REQ_ASYNC);
         if (packet->processedAsync) {
            LOG(4, ("%s: %d: @@Async\n", __FUNCTION__, __LINE__));
#ifndef VMX86_TOOLS
            /*
             * Asynchronous processing is supported by the transport.
             * We can release mappings here and reacquire when needed.
             */
            HSPU_PutMetaPacket(packet, transportSession);
            input->metaPacket = NULL;
            Atomic_Inc(&gHgfsAsyncCounter);

            /* Remove pending requests during poweroff. */
            Poll_Callback(POLL_CS_MAIN,
                          POLL_FLAG_REMOVE_AT_POWEROFF,
                          HgfsServerProcessRequest,
                          input,
                          POLL_REALTIME,
                          1000,
                          NULL);
#else
            /* Tools code should never process request async. */
            ASSERT(0);
#endif
         } else {
            LOG(4, ("%s: %d: ##Sync\n", __FUNCTION__, __LINE__));
            HgfsServerProcessRequest(input);
         }
      } else {
         /*
          * The input packet is smaller than the minimal size needed for the
          * operation.
          */
         status = HGFS_ERROR_PROTOCOL;
         LOG(4, ("%s: %d: Possible BUG! Malformed packet.\n", __FUNCTION__,
                 __LINE__));
      }
   }
   HGFS_ASSERT_CLIENT(input->op);

   /* Send error if we fail to process the op. */
   if (HGFS_ERROR_SUCCESS != status) {
      LOG(4, ("Error %d occured parsing the packet\n", (uint32)status));
      HgfsServerCompleteRequest(status, 0, input);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerTransportGetSessionInfo --
 *
 *   Scans the list of sessions and return the session with the specified
 *   session id.
 *
 * Results:
 *    A valid pointer to HgfsSessionInfo if there is a session with the
 *    specified session id. NULL, otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSessionInfo *
HgfsServerTransportGetSessionInfo(HgfsTransportSessionInfo *transportSession,       // IN: transport session info
                                  uint64 sessionId)                                 // IN: session id
{
   DblLnkLst_Links *curr;
   HgfsSessionInfo *session = NULL;

   ASSERT(transportSession);

   if (HGFS_INVALID_SESSION_ID == sessionId) {
      return NULL;
   }

   MXUser_AcquireExclLock(transportSession->sessionArrayLock);

   DblLnkLst_ForEach(curr, &transportSession->sessionArray) {
      session = DblLnkLst_Container(curr, HgfsSessionInfo, links);
      if (session->sessionId == sessionId) {
         HgfsServerSessionGet(session);
         break;
      }
      session = NULL;
   }

   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);

   return session;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerTransportRemoveSessionFromList --
 *
 *   Unlinks the specified session info from the list.
 *
 *   Note: The caller must acquire the sessionArrayLock in transportSession
 *   before calling this function.
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
HgfsServerTransportRemoveSessionFromList(HgfsTransportSessionInfo *transportSession,   // IN: transport session info
                                         HgfsSessionInfo *session)                     // IN: session info
{
   ASSERT(transportSession);
   ASSERT(session);

   DblLnkLst_Unlink1(&session->links);
   transportSession->numSessions--;
   HgfsServerSessionPut(session);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerTransportAddSessionToList --
 *
 *    Links the specified session info to the list.
 *
 * Results:
 *    HGFS_ERROR_SUCCESS if the session is successfully added to the list,
 *    HGFS_ERROR_TOO_MANY_SESSIONS if maximum number of sessions were already
 *                                 added to the list.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerTransportAddSessionToList(HgfsTransportSessionInfo *transportSession,       // IN: transport session info
                                    HgfsSessionInfo *session)                         // IN: session info
{
   HgfsInternalStatus status = HGFS_ERROR_TOO_MANY_SESSIONS;

   ASSERT(transportSession);
   ASSERT(session);

   MXUser_AcquireExclLock(transportSession->sessionArrayLock);

   if (transportSession->numSessions == MAX_SESSION_COUNT) {
      goto abort;
   }

   DblLnkLst_LinkLast(&transportSession->sessionArray, &session->links);
   transportSession->numSessions++;
   HgfsServerSessionGet(session);
   status = HGFS_ERROR_SUCCESS;

abort:
   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCleanupDeletedFolders --
 *
 *    This function iterates through all shared folders and removes all
 *    deleted shared folders, removes them from notification package and
 *    from the folders list.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerCleanupDeletedFolders(void)
{
   DblLnkLst_Links *link, *nextElem;

   MXUser_AcquireExclLock(gHgfsSharedFoldersLock);
   DblLnkLst_ForEachSafe(link, nextElem, &gHgfsSharedFoldersList) {
      HgfsSharedFolderProperties *folder =
         DblLnkLst_Container(link, HgfsSharedFolderProperties, links);
      if (folder->markedForDeletion) {
         LOG(8, ("%s: removing shared folder handle %#x\n",
                 __FUNCTION__, folder->notificationHandle));
         if (!HgfsNotify_RemoveSharedFolder(folder->notificationHandle)) {
            LOG(4, ("Problem removing %d shared folder handle\n",
                    folder->notificationHandle));
         }
         DblLnkLst_Unlink1(link);
         free(folder);
      }
   }
   MXUser_ReleaseExclLock(gHgfsSharedFoldersLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_RegisterSharedFolder --
 *
 *    This is a callback function which is invoked by hgfsServerManagement
 *    for every shared folder when something changed in shared folders
 *    configuration. The function iterates through the list of existing
 *    shared folders trying to locate an entry with the shareName. If the
 *    entry is found the function returns corresponding handle. Otherwise
 *    it creates a new entry and assigns a new handle to it.
 *
 *    Currently there is no notification that a shared folder has been
 *    deleted. The only way to find out that a shred folder is deleted
 *    is to notice that it is not enumerated any more. Thus an explicit
 *    "end of list" notification is needed. "sharedFolder == NULL" notifies
 *    that enumeration is completed which allows to delete all shared
 *    folders that were not mentioned during current enumeration.
 *
 * Results:
 *    HgfsSharedFolderHandle for the entry.
 *
 * Side effects:
 *    May add an entry to known shared folders list.
 *
 *-----------------------------------------------------------------------------
 */

HgfsSharedFolderHandle
HgfsServer_RegisterSharedFolder(const char *shareName,   // IN: shared folder name
                                const char *sharePath,   // IN: shared folder path
                                Bool addFolder)          // IN: add or remove folder
{
   DblLnkLst_Links *link, *nextElem;
   HgfsSharedFolderHandle result = HGFS_INVALID_FOLDER_HANDLE;

   LOG(8, ("%s: %s, %s, %s\n", __FUNCTION__,
           (shareName ? shareName : "NULL"), (sharePath ? sharePath : "NULL"),
           (addFolder ? "add" : "remove")));

   if (!gHgfsDirNotifyActive) {
      LOG(8, ("%s: notification disabled\n", __FUNCTION__));
      goto exit;
   }

   LOG(8, ("%s: %s, %s, %s - active notification\n", __FUNCTION__,
           (shareName ? shareName : "NULL"), (sharePath ? sharePath : "NULL"),
           (addFolder ? "add" : "remove")));

   if (NULL == shareName) {
      /*
       * The function is invoked with shareName NULL when all shares has been
       * enumerated.
       * Need to delete all shared folders that were marked for deletion.
       */
      HgfsServerCleanupDeletedFolders();
      goto exit;
   }

   MXUser_AcquireExclLock(gHgfsSharedFoldersLock);

   DblLnkLst_ForEachSafe(link, nextElem, &gHgfsSharedFoldersList) {
      HgfsSharedFolderProperties *folder =
         DblLnkLst_Container(link, HgfsSharedFolderProperties, links);
      if (strcmp(folder->name, shareName) == 0) {
         result = folder->notificationHandle;
         folder->markedForDeletion = !addFolder;
         break;
      }
   }
   if (addFolder && HGFS_INVALID_FOLDER_HANDLE == result) {
      result = HgfsNotify_AddSharedFolder(sharePath, shareName);
      if (HGFS_INVALID_FOLDER_HANDLE != result) {
         HgfsSharedFolderProperties *folder =
            (HgfsSharedFolderProperties *)Util_SafeMalloc(sizeof *folder);
         folder->notificationHandle = result;
         folder->name = Util_SafeStrdup(shareName);
         folder->markedForDeletion = FALSE;
         DblLnkLst_Init(&folder->links);
         DblLnkLst_LinkLast(&gHgfsSharedFoldersList, &folder->links);
      }
   }
   MXUser_ReleaseExclLock(gHgfsSharedFoldersLock);

exit:
   LOG(8, ("%s: %s, %s, %s exit %#x\n",__FUNCTION__,
           (shareName ? shareName : "NULL"), (sharePath ? sharePath : "NULL"),
           (addFolder ? "add" : "remove"), result));
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetShareHandle --
 *
 *    The function returns shared folder notification handle for the specified
 *    shared folder.
 *
 * Results:
 *    HgfsSharedFolderHandle that corresponds to the shared folder.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static HgfsSharedFolderHandle
HgfsServerGetShareHandle(const char *shareName)  // IN: name of the shared folder
{
   DblLnkLst_Links *link;
   HgfsSharedFolderHandle result = HGFS_INVALID_FOLDER_HANDLE;

   if (!gHgfsDirNotifyActive) {
      return HGFS_INVALID_FOLDER_HANDLE;
   }

   MXUser_AcquireExclLock(gHgfsSharedFoldersLock);

   DblLnkLst_ForEach(link, &gHgfsSharedFoldersList) {
      HgfsSharedFolderProperties *folder =
         DblLnkLst_Container(link, HgfsSharedFolderProperties, links);
      if (strcmp(folder->name, shareName) == 0) {
         result = folder->notificationHandle;
         break;
      }
   }
   MXUser_ReleaseExclLock(gHgfsSharedFoldersLock);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetShareName --
 *
 *    Get the share name for a shared folder handle by looking at the
 *    requested handle, finding the matching share (if any), and returning
 *    the share's name.
 *
 * Results:
 *    An Bool value indicating if the result is returned.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerGetShareName(HgfsSharedFolderHandle sharedFolder, // IN: Notify handle
                       size_t *shareNameLen,                // OUT: Name length
                       char **shareName)                    // OUT: Share name
{
   Bool result = FALSE;
   DblLnkLst_Links *link;

   if (!gHgfsDirNotifyActive) {
      return FALSE;
   }

   MXUser_AcquireExclLock(gHgfsSharedFoldersLock);

   DblLnkLst_ForEach(link, &gHgfsSharedFoldersList) {
      HgfsSharedFolderProperties *folder =
         DblLnkLst_Container(link, HgfsSharedFolderProperties, links);
      if (folder->notificationHandle == sharedFolder) {
         *shareName = Util_SafeStrdup(folder->name);
         result = TRUE;
         *shareNameLen = strlen(*shareName);
         break;
      }
   }
   MXUser_ReleaseExclLock(gHgfsSharedFoldersLock);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_InitState --
 *
 *    Initialize the global server state
 *
 * Results:
 *    TRUE if succeeded, FALSE if failed.
 *
 * Side effects:
 *    Memory allocation.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServer_InitState(HgfsServerSessionCallbacks **callbackTable,  // IN/OUT: our callbacks
                     HgfsServerStateLogger *serverMgrData)        // IN: mgr callback
{
   Bool result = TRUE;

   ASSERT(callbackTable);

   /* Save any server manager data for logging state updates.*/
   hgfsMgrData = serverMgrData;

   maxCachedOpenNodes = Config_GetLong(MAX_CACHED_FILENODES,
                                       "hgfs.fdCache.maxNodes");

   alwaysUseHostTime = Config_GetBool(FALSE, "hgfs.alwaysUseHostTime");

   /*
    * Initialize the globals for handling the active shared folders.
    */

   gHgfsAsyncLock = NULL;
   gHgfsAsyncVar = NULL;
   Atomic_Write(&gHgfsAsyncCounter, 0);

   DblLnkLst_Init(&gHgfsSharedFoldersList);
   gHgfsSharedFoldersLock = MXUser_CreateExclLock("sharedFoldersLock",
                                                  RANK_hgfsSharedFolders);
   if (NULL != gHgfsSharedFoldersLock) {
      gHgfsAsyncLock = MXUser_CreateExclLock("asyncLock",
                                             RANK_hgfsSharedFolders);
      if (NULL != gHgfsAsyncLock) {
         gHgfsAsyncVar = MXUser_CreateCondVarExclLock(gHgfsAsyncLock);
         if (NULL != gHgfsAsyncVar) {
            if (!HgfsServerPlatformInit()) {
               LOG(4, ("Could not initialize server platform specific \n"));
               result = FALSE;
            }
         } else {
            LOG(4, ("%s: Could not create async counter cond var.\n",
                    __FUNCTION__));
            result = FALSE;
         }
      } else {
         LOG(4, ("%s: Could not create async counter mutex.\n", __FUNCTION__));
         result = FALSE;
      }
   } else {
      LOG(4, ("%s: Could not create shared folders mutex.\n", __FUNCTION__));
      result = FALSE;
   }

   if (result) {
      *callbackTable = &hgfsServerSessionCBTable;
      if (Config_GetBool(TRUE, "isolation.tools.hgfs.notify.enable")) {
         gHgfsDirNotifyActive = HgfsNotify_Init() == HGFS_STATUS_SUCCESS;
         Log("%s: initialized notification %s.\n", __FUNCTION__,
             (gHgfsDirNotifyActive ? "active" : "inactive"));
      }
      gHgfsInitialized = TRUE;
   } else {
      HgfsServer_ExitState(); // Cleanup partially initialized state
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_ExitState --
 *
 *    Cleanup the global server state.
 *
 *    This function should be called when all other HGFS threads stopped
 *    running. Otherwise we'll be in trouble because this is where we delete
 *    the node array lock.
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
HgfsServer_ExitState(void)
{
   gHgfsInitialized = FALSE;

   if (gHgfsDirNotifyActive) {
      HgfsNotify_Exit();
      gHgfsDirNotifyActive = FALSE;
      Log("%s: exit notification - inactive.\n", __FUNCTION__);
   }

   if (NULL != gHgfsSharedFoldersLock) {
      MXUser_DestroyExclLock(gHgfsSharedFoldersLock);
      gHgfsSharedFoldersLock = NULL;
   }

   if (NULL != gHgfsAsyncLock) {
      MXUser_DestroyExclLock(gHgfsAsyncLock);
      gHgfsAsyncLock = NULL;
   }

   if (NULL != gHgfsAsyncVar) {
      MXUser_DestroyCondVar(gHgfsAsyncVar);
      gHgfsAsyncVar = NULL;
   }

   HgfsServerPlatformDestroy();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGenerateSessionId --
 *
 *    Generates unique session id.
 *
 * Results:
 *    Unique 64-bit value.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static uint64
HgfsGenerateSessionId(void)
{
   return RDTSC();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetSessionCapability --
 *
 *    Sets session capability for a specific operation code.
 *
 * Results:
 *    TRUE is the capability for the operation has been changed.
 *    FALSE if the operation is not represented in the capabilities array.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerSetSessionCapability(HgfsOp op,                  // IN: operation code
                               uint32 flags,               // IN: flags that describe level of support
                               HgfsSessionInfo *session)   // INOUT: session to update
{
   int i;
   Bool result = FALSE;

   for ( i = 0; i < ARRAYSIZE(session->hgfsSessionCapabilities); i++) {
      if (session->hgfsSessionCapabilities[i].op == op) {
         session->hgfsSessionCapabilities[i].flags = flags;
         result = TRUE;
      }
   }
   LOG(4, ("%s: Setting capabilitiy flags %x for op code %d %s\n",
           __FUNCTION__, flags, op, result ? "succeeded" : "failed"));

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerEnumerateSharedFolders --
 *
 *    Enumerates all exisitng shared folders and registers shared folders with
 *    directory notification package.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerEnumerateSharedFolders(void)
{
   void *state;
   Bool success = FALSE;

   LOG(8, ("%s: entered\n", __FUNCTION__));
   state = HgfsServerPolicy_GetSharesInit();
   if (NULL != state) {
      Bool done;

      do {
         char const *shareName;
         size_t len;

         success = HgfsServerPolicy_GetShares(state, &shareName, &len, &done);
         if (success && !done) {
            HgfsSharedFolderHandle handle;
            char const *sharePath;
            size_t sharePathLen;
            HgfsNameStatus nameStatus;

            nameStatus = HgfsServerPolicy_GetSharePath(shareName, len,
                                                       HGFS_OPEN_MODE_READ_ONLY,
                                                       &sharePathLen, &sharePath);
            if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
               LOG(8, ("%s: registering share %s path %s\n", __FUNCTION__, shareName, sharePath));
               handle = HgfsServer_RegisterSharedFolder(shareName, sharePath,
                                                        TRUE);
               success = handle != HGFS_INVALID_FOLDER_HANDLE;
               LOG(8, ("%s: registering share %s hnd %#x\n", __FUNCTION__, shareName, handle));
            }
         }
      } while (!done && success);

      HgfsServerPolicy_GetSharesCleanup(state);
   }
   LOG(8, ("%s: exit %d\n", __FUNCTION__, success));
   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionConnect --
 *
 *    Initialize a new client session.
 *
 *    Allocate HgfsTransportSessionInfo and initialize it.
 *
 * Results:
 *    TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsServerSessionConnect(void *transportData,                         // IN: transport session context
                         HgfsServerChannelCallbacks *channelCbTable,  // IN: Channel callbacks
                         uint32 channelCapabilities,                  // IN: channel capabilities
                         void **transportSessionData)                 // OUT: server session context
{
   HgfsTransportSessionInfo *transportSession;

   ASSERT(transportSessionData);

   LOG(4, ("%s: initting.\n", __FUNCTION__));

   transportSession = Util_SafeCalloc(1, sizeof *transportSession);
   transportSession->transportData = transportData;
   transportSession->channelCbTable = channelCbTable;
   transportSession->maxPacketSize = MAX_SERVER_PACKET_SIZE_V4;
   transportSession->type = HGFS_SESSION_TYPE_REGULAR;
   transportSession->state = HGFS_SESSION_STATE_OPEN;
   transportSession->channelCapabilities = channelCapabilities;
   transportSession->numSessions = 0;

   transportSession->sessionArrayLock =
         MXUser_CreateExclLock("HgfsSessionArrayLock",
                               RANK_hgfsSessionArrayLock);
   if (transportSession->sessionArrayLock == NULL) {
      LOG(4, ("%s: Could not create session sync mutex.\n", __FUNCTION__));
      free(transportSession);
      return FALSE;
   }

   DblLnkLst_Init(&transportSession->sessionArray);

   transportSession->defaultSessionId = HGFS_INVALID_SESSION_ID;

   Atomic_Write(&transportSession->refCount, 0);

   /* Give our session a reference to hold while we are open. */
   HgfsServerTransportSessionGet(transportSession);

   *transportSessionData = transportSession;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerAllocateSession --
 *
 *    Initialize a new Hgfs session.
 *
 *    Allocate HgfsSessionInfo and initialize it. Create the nodeArray and
 *    searchArray for the session.
 *
 * Results:
 *    TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *    Allocates and initializes new session info.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerAllocateSession(HgfsTransportSessionInfo *transportSession, // IN:
                          uint32 channelCapabilities,                 // IN:
                          HgfsSessionInfo **sessionData)              // OUT:
{
   int i;
   HgfsSessionInfo *session;

   LOG(8, ("%s: entered\n", __FUNCTION__));

   ASSERT(transportSession);

   session = Util_SafeCalloc(1, sizeof *session);

   /*
    * Initialize all our locks first as these can fail.
    */

   session->fileIOLock = MXUser_CreateExclLock("HgfsFileIOLock",
                                               RANK_hgfsFileIOLock);
   if (session->fileIOLock == NULL) {
      LOG(4, ("%s: Could not create node array sync mutex.\n", __FUNCTION__));
      free(session);
      return FALSE;
   }

   session->nodeArrayLock = MXUser_CreateExclLock("HgfsNodeArrayLock",
                                                  RANK_hgfsNodeArrayLock);
   if (session->nodeArrayLock == NULL) {
      MXUser_DestroyExclLock(session->fileIOLock);
      LOG(4, ("%s: Could not create node array sync mutex.\n", __FUNCTION__));
      free(session);
      return FALSE;
   }

   session->searchArrayLock = MXUser_CreateExclLock("HgfsSearchArrayLock",
                                                    RANK_hgfsSearchArrayLock);
   if (session->searchArrayLock == NULL) {
      MXUser_DestroyExclLock(session->fileIOLock);
      MXUser_DestroyExclLock(session->nodeArrayLock);
      LOG(4, ("%s: Could not create search array sync mutex.\n",
              __FUNCTION__));
      free(session);
      return FALSE;
   }

   session->sessionId = HgfsGenerateSessionId();
   session->state = HGFS_SESSION_STATE_OPEN;
   DblLnkLst_Init(&session->links);
   session->maxPacketSize = MAX_SERVER_PACKET_SIZE_V4;
   session->activeNotification = FALSE;
   session->isInactive = TRUE;
   session->transportSession = transportSession;
   session->numInvalidationAttempts = 0;

   /*
    * Initialize the node handling components.
    */

   DblLnkLst_Init(&session->nodeFreeList);
   DblLnkLst_Init(&session->nodeCachedList);

   /* Allocate array of FileNodes and add them to free list. */
   session->numNodes = NUM_FILE_NODES;
   session->nodeArray = Util_SafeCalloc(session->numNodes,
                                        sizeof (HgfsFileNode));
   session->numCachedOpenNodes = 0;
   session->numCachedLockedNodes = 0;

   for (i = 0; i < session->numNodes; i++) {
      DblLnkLst_Init(&session->nodeArray[i].links);
      /* Append at the end of the list. */
      DblLnkLst_LinkLast(&session->nodeFreeList, &session->nodeArray[i].links);
   }

   /*
    * Initialize the search handling components.
    */

   /* Initialize search freelist. */
   DblLnkLst_Init(&session->searchFreeList);

   Atomic_Write(&session->refCount, 0);

   /* Give our session a reference to hold while we are open. */
   HgfsServerSessionGet(session);

   /* Allocate array of searches and add them to free list. */
   session->numSearches = NUM_SEARCHES;
   session->searchArray = Util_SafeCalloc(session->numSearches,
                                          sizeof (HgfsSearch));

   for (i = 0; i < session->numSearches; i++) {
      DblLnkLst_Init(&session->searchArray[i].links);
      /* Append at the end of the list. */
      DblLnkLst_LinkLast(&session->searchFreeList,
                         &session->searchArray[i].links);
   }

   /* Get common to all sessions capabiities. */
   HgfsServerGetDefaultCapabilities(session->hgfsSessionCapabilities,
                                    &session->numberOfCapabilities);

   if (channelCapabilities & HGFS_CHANNEL_SHARED_MEM) {
      HgfsServerSetSessionCapability(HGFS_OP_READ_FAST_V4,
                                     HGFS_REQUEST_SUPPORTED, session);
      HgfsServerSetSessionCapability(HGFS_OP_WRITE_FAST_V4,
                                     HGFS_REQUEST_SUPPORTED, session);
      if (gHgfsDirNotifyActive) {
         LOG(8, ("%s: notify is enabled\n", __FUNCTION__));
         if (HgfsServerEnumerateSharedFolders()) {
            HgfsServerSetSessionCapability(HGFS_OP_SET_WATCH_V4,
                                           HGFS_REQUEST_SUPPORTED, session);
            HgfsServerSetSessionCapability(HGFS_OP_REMOVE_WATCH_V4,
                                           HGFS_REQUEST_SUPPORTED, session);
            session->activeNotification = TRUE;
         } else {
            HgfsServerSetSessionCapability(HGFS_OP_SET_WATCH_V4,
                                           HGFS_REQUEST_NOT_SUPPORTED, session);
            HgfsServerSetSessionCapability(HGFS_OP_REMOVE_WATCH_V4,
                                           HGFS_REQUEST_NOT_SUPPORTED, session);
         }
         LOG(8, ("%s: session notify capability is %s\n", __FUNCTION__,
                 (session->activeNotification ? "enabled" : "disabled")));
      }
      HgfsServerSetSessionCapability(HGFS_OP_SEARCH_READ_V4,
                                     HGFS_REQUEST_SUPPORTED, session);
   }

   *sessionData = session;

   LOG(8, ("%s: exit TRUE\n", __FUNCTION__));
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDisconnectSessionInt --
 *
 *    Disconnect a client session.
 *
 *    Mark the session as closed as we are in the process of teardown
 *    of the session. No more new requests should be processed. We would
 *    start draining any outstanding pending operations at this point.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsDisconnectSessionInt(HgfsSessionInfo *session)    // IN: session context
{
   LOG(8, ("%s: entered\n", __FUNCTION__));

   ASSERT(session);
   ASSERT(session->nodeArray);
   ASSERT(session->searchArray);

   if (session->activeNotification) {
      LOG(8, ("%s: calling notify component to disconnect\n", __FUNCTION__));
      HgfsNotify_RemoveSessionSubscribers(session);
   }
   LOG(8, ("%s: exit\n", __FUNCTION__));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionDisconnect --
 *
 *    Disconnect a client session.
 *
 *    Mark the session as closed as we are in the process of teardown
 *    of the session. No more new requests should be processed. We would
 *    start draining any outstanding pending operations at this point.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSessionDisconnect(void *clientData)    // IN: session context
{
   HgfsTransportSessionInfo *transportSession = (HgfsTransportSessionInfo *)clientData;
   DblLnkLst_Links *curr, *next;

   LOG(8, ("%s: entered\n", __FUNCTION__));

   ASSERT(transportSession);

   MXUser_AcquireExclLock(transportSession->sessionArrayLock);

   DblLnkLst_ForEachSafe(curr, next, &transportSession->sessionArray) {
      HgfsSessionInfo *session = DblLnkLst_Container(curr, HgfsSessionInfo, links);

      HgfsDisconnectSessionInt(session);
   }

   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);

   transportSession->state = HGFS_SESSION_STATE_CLOSED;
   LOG(8, ("%s: exit\n", __FUNCTION__));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionClose --
 *
 *    Closes a client session.
 *
 *    Remvoing the final reference will free the session's nodeArray
 *    and seachArrary, and finally free the session object.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSessionClose(void *clientData)    // IN: session context
{
   HgfsTransportSessionInfo *transportSession = (HgfsTransportSessionInfo *)clientData;

   ASSERT(transportSession);
   ASSERT(transportSession->state == HGFS_SESSION_STATE_CLOSED);

   /* Remove, typically, the last reference, will teardown everything. */
   HgfsServerTransportSessionPut(transportSession);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerExitSessionInternal --
 *
 *    Destroys a session.
 *
 *    Free the session's nodeArray and seachArrary. Free the session.
 *
 *    The caller must have previously acquired the global sessions lock.
 *
 * Results:
 *    TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *    Allocates new session info.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerExitSessionInternal(HgfsSessionInfo *session)    // IN: session context
{
   int i;

   ASSERT(session);
   ASSERT(session->nodeArray);
   ASSERT(session->searchArray);

   MXUser_AcquireExclLock(session->nodeArrayLock);

   LOG(4, ("%s: exiting.\n", __FUNCTION__));
   /* Recycle all nodes that are still in use, then destroy the node pool. */
   for (i = 0; i < session->numNodes; i++) {
      HgfsHandle handle;

      if (session->nodeArray[i].state == FILENODE_STATE_UNUSED) {
         continue;
      }

      handle = HgfsFileNode2Handle(&session->nodeArray[i]);
      HgfsRemoveFromCacheInternal(handle, session);
      HgfsFreeFileNodeInternal(handle, session);
   }
   free(session->nodeArray);
   session->nodeArray = NULL;

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   /*
    * Recycle all searches that are still in use, then destroy the
    * search pool.
    */

   MXUser_AcquireExclLock(session->searchArrayLock);

   for (i = 0; i < session->numSearches; i++) {
      if (DblLnkLst_IsLinked(&session->searchArray[i].links)) {
         continue;
      }
      HgfsRemoveSearchInternal(&session->searchArray[i], session);
   }
   free(session->searchArray);
   session->searchArray = NULL;

   MXUser_ReleaseExclLock(session->searchArrayLock);

   /* Teardown the locks for the sessions and destroy itself. */
   MXUser_DestroyExclLock(session->nodeArrayLock);
   MXUser_DestroyExclLock(session->searchArrayLock);
   MXUser_DestroyExclLock(session->fileIOLock);

   free(session);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_GetHandleCounter --
 *
 *    Return file handle counter. This is used by the checkpointing code to
 *    checkpoint this value so we avoid the risk of handle collision.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

uint32
HgfsServer_GetHandleCounter(void)
{
   return HgfsServerGetHandleCounter();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_SetHandleCounter --
 *
 *    Set the file handle counter. This is used by the checkpointing code to
 *    restore this value so we avoid the risk of handle collision.
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
HgfsServer_SetHandleCounter(uint32 newHandleCounter)
{
   HgfsServerInitHandleCounter(newHandleCounter);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServerSessionSendComplete --
 *
 *    This is called by the Transport when it is done sending the packet.
 *    Free the buffer. If we allocate buffers per session we have the session
 *    that the buffer belongs too.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Frees the packet buffer.
 *
 *---------------------------------------------------------------------------
 */

void
HgfsServerSessionSendComplete(HgfsPacket *packet,   // IN/OUT: Hgfs packet
                              void *clientData)     // IN: session info
{
   HgfsTransportSessionInfo *transportSession =
         (HgfsTransportSessionInfo *)clientData;

   if (packet->guestInitiated) {
      HSPU_PutMetaPacket(packet, transportSession);
      HSPU_PutReplyPacket(packet, transportSession);
      HSPU_PutDataPacketBuf(packet, transportSession);
   } else {
      free(packet->metaPacket);
      free(packet);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsServer_Quiesce --
 *
 *    The function is called when VM is about to take a snapshot and
 *    when creation of the snapshot completed. When the freeze is TRUE the
 *    function quiesces all asynchronous and background activity to prevent
 *    interactions with snapshots and waits until there is no such activity.
 *    When freeze is FALSE the function restarts background activity that
 *    has been suspended previously.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsServer_Quiesce(Bool freeze)  // IN:
{
   if (!gHgfsInitialized) {
      return;
   }

   if (freeze) {
      /* Suspend background activity. */
      if (gHgfsDirNotifyActive) {
         HgfsNotify_Deactivate(HGFS_NOTIFY_REASON_SERVER_SYNC);
      }
      /* Wait for outstanding asynchronous requests to complete. */
      MXUser_AcquireExclLock(gHgfsAsyncLock);
      while (Atomic_Read(&gHgfsAsyncCounter)) {
         MXUser_WaitCondVarExclLock(gHgfsAsyncLock, gHgfsAsyncVar);
      }
      MXUser_ReleaseExclLock(gHgfsAsyncLock);
   } else {
      /* Resume background activity. */
      if (gHgfsDirNotifyActive) {
         HgfsNotify_Activate(HGFS_NOTIFY_REASON_SERVER_SYNC);
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsNotifyPacketSent --
 *
 *    Decrements counter of outstanding asynchronous packets
 *    and signal conditional variable when the counter
 *    becomes 0.
 *
 * Results:
 *    TRUE on success, FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
HgfsNotifyPacketSent(void)
{
   if (Atomic_FetchAndDec(&gHgfsAsyncCounter) == 1) {
      MXUser_AcquireExclLock(gHgfsAsyncLock);
      MXUser_BroadcastCondVar(gHgfsAsyncVar);
      MXUser_ReleaseExclLock(gHgfsAsyncLock);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsPacketSend --
 *
 *    Send the packet.
 *
 * Results:
 *    TRUE on success, FALSE on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
HgfsPacketSend(HgfsPacket *packet,            // IN/OUT: Hgfs Packet
               char *packetOut,               // IN: output buffer
               size_t packetOutLen,           // IN: packet size
               HgfsTransportSessionInfo *transportSession,      // IN: session info
               HgfsSendFlags flags)           // IN: flags for how to process
{
   Bool result = FALSE;
   Bool notificationNeeded = packet->guestInitiated && packet->processedAsync;

   ASSERT(packet);
   ASSERT(transportSession);

   if (transportSession->state == HGFS_SESSION_STATE_OPEN) {
      packet->replyPacketSize = packetOutLen;
      ASSERT(transportSession->type == HGFS_SESSION_TYPE_REGULAR);
      result = transportSession->channelCbTable->send(transportSession->transportData,
                                             packet, packetOut,
                                             packetOutLen, flags);
   }

   if (notificationNeeded) {
      HgfsNotifyPacketSent();
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsInvalidateSessionObjects --
 *
 *      Iterates over all nodes and searches, invalidating and removing those
 *      that are no longer within a share.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsInvalidateSessionObjects(DblLnkLst_Links *shares,  // IN: List of new shares
                             HgfsSessionInfo *session) // IN: Session info
{
   unsigned int i;

   ASSERT(shares);
   ASSERT(session);
   ASSERT(session->nodeArray);
   ASSERT(session->searchArray);
   LOG(4, ("%s: Beginning\n", __FUNCTION__));

   MXUser_AcquireExclLock(session->nodeArrayLock);

   /*
    * Iterate over each node, skipping those that are unused. For each node,
    * if its filename is no longer within a share, remove it.
    */
   for (i = 0; i < session->numNodes; i++) {
      HgfsHandle handle;
      DblLnkLst_Links *l;

      if (session->nodeArray[i].state == FILENODE_STATE_UNUSED) {
         continue;
      }

      handle = HgfsFileNode2Handle(&session->nodeArray[i]);
      LOG(4, ("%s: Examining node with fd %d (%s)\n", __FUNCTION__,
              handle, session->nodeArray[i].utf8Name));

      /* For each share, is the node within the share? */
      for (l = shares->next; l != shares; l = l->next) {
         HgfsSharedFolder *share;

         share = DblLnkLst_Container(l, HgfsSharedFolder, links);
         ASSERT(share);
         if (strcmp(session->nodeArray[i].shareInfo.rootDir, share->path) == 0) {
            LOG(4, ("%s: Node is still valid\n", __FUNCTION__));
            break;
         }
      }

      /* If the node wasn't found in any share, remove it. */
      if (l == shares) {
         LOG(4, ("%s: Node is invalid, removing\n", __FUNCTION__));
         if (!HgfsRemoveFromCacheInternal(handle, session)) {
            LOG(4, ("%s: Could not remove node with "
                    "fh %d from the cache.\n", __FUNCTION__, handle));
         } else {
            HgfsFreeFileNodeInternal(handle, session);
         }
      }
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   MXUser_AcquireExclLock(session->searchArrayLock);

   /*
    * Iterate over each search, skipping those that are on the free list. For
    * each search, if its base name is no longer within a share, remove it.
    */
   for (i = 0; i < session->numSearches; i++) {
      DblLnkLst_Links *l;

      if (DblLnkLst_IsLinked(&session->searchArray[i].links)) {
         continue;
      }

      if (HgfsSearchIsBaseNameSpace(&session->searchArray[i])) {
         /* Skip search of the base name space. Maybe stale but it is okay. */
         continue;
      }

      LOG(4, ("%s: Examining search (%s)\n", __FUNCTION__,
              session->searchArray[i].utf8Dir));

      /* For each share, is the search within the share? */
      for (l = shares->next; l != shares; l = l->next) {
         HgfsSharedFolder *share;

         share = DblLnkLst_Container(l, HgfsSharedFolder, links);
         ASSERT(share);
         if (strcmp(session->searchArray[i].shareInfo.rootDir, share->path) == 0) {
            LOG(4, ("%s: Search is still valid\n", __FUNCTION__));
            break;
         }
      }

      /* If the node wasn't found in any share, remove it. */
      if (l == shares) {
         LOG(4, ("%s: Search is invalid, removing\n", __FUNCTION__));
         HgfsRemoveSearchInternal(&session->searchArray[i], session);
      }
   }

   MXUser_ReleaseExclLock(session->searchArrayLock);

   LOG(4, ("%s: Ending\n", __FUNCTION__));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionInvalidateObjects --
 *
 *      Iterates over all sessions and invalidate session objects for the shares
 *      removed.
 *
 *      Caller guarantees that the sessions won't go away under us, so no locks
 *      needed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsServerSessionInvalidateObjects(void *clientData,         // IN:
                                   DblLnkLst_Links *shares)  // IN: List of new shares
{
   HgfsTransportSessionInfo *transportSession =
         (HgfsTransportSessionInfo *)clientData;
   DblLnkLst_Links *curr;

   ASSERT(transportSession);
   MXUser_AcquireExclLock(transportSession->sessionArrayLock);

   DblLnkLst_ForEach(curr, &transportSession->sessionArray) {
      HgfsSessionInfo *session = DblLnkLst_Container(curr, HgfsSessionInfo, links);
      HgfsServerSessionGet(session);
      HgfsInvalidateSessionObjects(shares, session);
      HgfsServerSessionPut(session);
   }

   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionInvalidateInactiveSessions --
 *
 *      Iterates over all sessions and invalidate all inactive session objects.
 *
 *      Following clock algorithm is used to determine whether the session object
 *      is inactive or not.
 *
 *      When this function is called, the HGFS server manager will iterate
 *      over all the sessions belonging to this manager. Each session is marked
 *      as inactive. Whenever a message is processed for a session, that
 *      session is marked as active. When this function is called the next time,
 *      any sessions that are still inactive will be invalidated.
 *
 *      Caller guarantees that the sessions won't go away under us, so no locks
 *      needed.
 *
 * Results:
 *      Number of active sessions remaining inside the HGFS server.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

uint32
HgfsServerSessionInvalidateInactiveSessions(void *clientData)         // IN:
{
   HgfsTransportSessionInfo *transportSession =
         (HgfsTransportSessionInfo *)clientData;
   uint32 numActiveSessionsLeft = 0;
   DblLnkLst_Links shares, *curr, *next;

   ASSERT(transportSession);
   MXUser_AcquireExclLock(transportSession->sessionArrayLock);

   DblLnkLst_Init(&shares);

   DblLnkLst_ForEachSafe(curr, next,  &transportSession->sessionArray) {
      HgfsSessionInfo *session = DblLnkLst_Container(curr, HgfsSessionInfo, links);
      HgfsServerSessionGet(session);

      session->numInvalidationAttempts++;
      numActiveSessionsLeft++;

      /*
       * Check if the session is inactive. If the session is inactive, then
       * invalidate the session objects.
       */
      if (session->isInactive) {

         if (session->numInvalidationAttempts == MAX_SESSION_INVALIDATION_ATTEMPTS) {
            HgfsServerTransportRemoveSessionFromList(transportSession,
                                                     session);
            /*
             * We need to reduce the refcount by 1 since we want to
             * destroy the session.
             */
            numActiveSessionsLeft--;
            HgfsServerSessionPut(session);
         } else {
            HgfsInvalidateSessionObjects(&shares, session);
         }
      } else {
         session->isInactive = TRUE;
         session->numInvalidationAttempts = 0;
      }

      HgfsServerSessionPut(session);
   }

   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);

   return numActiveSessionsLeft;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerStatFs --
 *
 *      Calls on the wiper library to return the number of free bytes and
 *      total bytes on the filesystem underlying the given pathname.
 *
 * Results:
 *      TRUE if successful: freeBytes and totalBytes have been written to.
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerStatFs(const char *pathName, // IN: Path we're interested in
                 size_t pathLength,    // IN: Length of path
                 uint64 *freeBytes,    // OUT: Free bytes on volume
                 uint64 *totalBytes)   // OUT: Total bytes on volume
{
   WiperPartition p;
   unsigned char *wiperError;

   ASSERT(pathName);
   ASSERT(freeBytes);
   ASSERT(totalBytes);

   Wiper_Init(NULL);

   /*
    * Sanity checks. If length is good, assume well-formed drive path
    * (i.e. "C:\..." or "\\abc..."). Note that we throw out shares that
    * exactly equal p.mountPoint's size because we won't have room for a null
    * delimiter on copy. Allow 0 length drives so that hidden feature "" can
    * work.
    */
   if (pathLength >= sizeof p.mountPoint) {
      LOG(4, ("%s: could not get the volume name\n", __FUNCTION__));

      return FALSE;
   }

   /* Now call the wiper lib to get space information. */
   Str_Strcpy(p.mountPoint, pathName, sizeof p.mountPoint);
   wiperError = WiperSinglePartition_GetSpace(&p, freeBytes, totalBytes);
   if (strlen(wiperError) > 0) {
      LOG(4, ("%s: error using wiper lib: %s\n", __FUNCTION__, wiperError));

      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetShareInfo --
 *
 *    Construct local name based on the crossplatform CPName for the file.
 *    The name returned is allocated and must be freed by the caller.
 *
 *    outLen can be NULL, in which case the length is not returned.
 *
 * Results:
 *    A status code indicating either success (correspondent share exists) or
 *    a failure status.
 *
 * Side effects:
 *    Memory allocation in the success case
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerGetShareInfo(char *cpName,            // IN:  Cross-platform filename to check
                       size_t cpNameSize,       // IN:  Size of name cpName
                       uint32 caseFlags,        // IN:  Case-sensitivity flags
                       HgfsShareInfo *shareInfo,// OUT: properties of the shared folder
                       char **bufOut,           // OUT: File name in local fs
                       size_t *outLen)          // OUT: Length of name out
{
   HgfsNameStatus nameStatus;
   char const *inEnd;
   char *next;
   char *myBufOut;
   char *convertedMyBufOut;
   char *out;
   size_t outSize;
   size_t myBufOutLen;
   size_t convertedMyBufOutLen;
   int len;
   uint32 pathNameLen;
   char tempBuf[HGFS_PATH_MAX];
   size_t tempSize;
   char *tempPtr;
   uint32 startIndex = 0;
   HgfsShareOptions shareOptions;

   ASSERT(cpName);
   ASSERT(bufOut);

   inEnd = cpName + cpNameSize;

   if (!Unicode_IsBufferValid(cpName, cpNameSize, STRING_ENCODING_UTF8)) {
      LOG(4, ("%s: invalid UTF8 string @ %p\n", __FUNCTION__, cpName));
      return HGFS_NAME_STATUS_FAILURE;
   }

   /*
    * Get first component.
    */
   len = CPName_GetComponent(cpName, inEnd, (char const **) &next);
   if (len < 0) {
      LOG(4, ("%s: get first component failed\n", __FUNCTION__));

      return HGFS_NAME_STATUS_FAILURE;
   }

   /* See if we are dealing with the base of the namespace */
   if (!len) {
      return HGFS_NAME_STATUS_INCOMPLETE_BASE;
   }

   /* Check permission on the share and get the share path */
   nameStatus = HgfsServerPolicy_ProcessCPName(cpName,
                                               len,
                                               &shareInfo->readPermissions,
                                               &shareInfo->writePermissions,
                                               &shareInfo->handle,
                                               &shareInfo->rootDir);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: No such share (%s)\n", __FUNCTION__, cpName));
      return nameStatus;
   }
   shareInfo->rootDirLen = strlen(shareInfo->rootDir);

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(cpName, len, &shareOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpName));
      return nameStatus;
   }

   /* Point to the next component, if any */
   cpNameSize -= next - cpName;
   cpName = next;

   /*
    * Allocate space for the string. We trim the unused space later.
    */

   outSize = HGFS_PATH_MAX;
   myBufOut = (char *) malloc(outSize * sizeof *myBufOut);
   if (!myBufOut) {
      LOG(4, ("%s: out of memory allocating string\n", __FUNCTION__));

      return HGFS_NAME_STATUS_OUT_OF_MEMORY;
   }

   out = myBufOut;

   /*
    * See if we are dealing with a "root" share or regular share
    */
   if (shareInfo->rootDirLen == 0) {
      size_t prefixLen;

      /*
       * This is a "root" share. Interpret the input appropriately as
       * either a drive letter or UNC name and append it to the output
       * buffer (for Win32) or simply get the prefix for root (for
       * linux).
       */
      tempSize = sizeof tempBuf;
      tempPtr = tempBuf;
      nameStatus = CPName_ConvertFromRoot((char const **) &cpName,
                                          &cpNameSize, &tempSize, &tempPtr);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: ConvertFromRoot not complete\n", __FUNCTION__));
         goto error;
      }

      prefixLen = tempPtr - tempBuf;

      /* Copy the UTF8 prefix to the output buffer. */
      if (prefixLen >= HGFS_PATH_MAX) {
         Log("%s: error: prefix too long\n", __FUNCTION__);
         nameStatus = HGFS_NAME_STATUS_TOO_LONG;
         goto error;
      }

      memcpy(out, tempBuf, prefixLen);
      out += prefixLen;
      *out = 0;
      outSize -= prefixLen;
   } else {
      /*
       * This is a regular share. Append the path to the out buffer.
       */
      if (outSize < shareInfo->rootDirLen + 1) {
         LOG(4, ("%s: share path too big\n", __FUNCTION__));
         nameStatus = HGFS_NAME_STATUS_TOO_LONG;
         goto error;
      }

      memcpy(out, shareInfo->rootDir, shareInfo->rootDirLen + 1);
      out += shareInfo->rootDirLen;
      outSize -= shareInfo->rootDirLen;
   }

   /* Convert the rest of the input name (if any) to a local name */
   tempSize = sizeof tempBuf;
   tempPtr = tempBuf;


   if (CPName_ConvertFrom((char const **) &cpName, &cpNameSize, &tempSize,
                          &tempPtr) < 0) {
      LOG(4, ("%s: CP name conversion failed\n", __FUNCTION__));
      nameStatus = HGFS_NAME_STATUS_FAILURE;
      goto error;
   }

   /*
    * For volume root directory shares the prefix will have a trailing
    * separator and since our remaining paths start with a separator, we
    * will skip over the second separator for this case. Bug 166755.
    */

   if ((out != myBufOut) && (*(out - 1) == DIRSEPC) && (tempBuf[0] == DIRSEPC)) {
      startIndex++;
   }
   pathNameLen = tempPtr - &tempBuf[startIndex];

   /* Copy UTF8 to the output buffer. */
   if (pathNameLen >= outSize) {
      LOG(4, ("%s: pathname too long\n", __FUNCTION__));
      nameStatus = HGFS_NAME_STATUS_TOO_LONG;
      goto error;
   }

   memcpy(out, &tempBuf[startIndex], pathNameLen);
   outSize -= pathNameLen;
   out += pathNameLen;
   *out = 0;
   myBufOutLen = out - myBufOut;

#if defined(__APPLE__)
   {
      size_t nameLen;
      /*
       * For Mac hosts the unicode format is decomposed (form D)
       * so there is a need to convert the incoming name from HGFS clients
       * which is assumed to be in the normalized form C (precomposed).
       */

      if (!CodeSet_Utf8FormCToUtf8FormD(myBufOut, myBufOutLen, &tempPtr,
                                        &nameLen)) {
         LOG(4, ("%s: unicode conversion to form D failed.\n", __FUNCTION__));
         nameStatus = HGFS_NAME_STATUS_FAILURE;
         goto error;
      }

      free(myBufOut);
      LOG(4, ("%s: name is \"%s\"\n", __FUNCTION__, tempPtr));

      /* Save returned pointers, update buffer length. */
      myBufOut = tempPtr;
      out = tempPtr + nameLen;
      myBufOutLen = nameLen;
   }
#endif /* defined(__APPLE__) */

   /*
    * Convert file name to proper case if host default config option is not set
    * and case conversion is required for this platform.
    */

   if (!HgfsServerPolicy_IsShareOptionSet(shareOptions,
                                          HGFS_SHARE_HOST_DEFAULT_CASE) &&
       HgfsServerCaseConversionRequired()) {
      nameStatus = HgfsServerConvertCase(shareInfo->rootDir, shareInfo->rootDirLen,
                                         myBufOut, myBufOutLen, caseFlags,
                                         &convertedMyBufOut,
                                         &convertedMyBufOutLen);

      /*
       * On success, use the converted file names for further operations.
       */

      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: HgfsServerConvertCase failed.\n", __FUNCTION__));
         goto error;
      }

      free(myBufOut);
      myBufOut = convertedMyBufOut;
      myBufOutLen = convertedMyBufOutLen;
      ASSERT(myBufOut);
   }

   /* Check for symlinks if the followSymlinks option is not set. */
   if (!HgfsServerPolicy_IsShareOptionSet(shareOptions,
                                          HGFS_SHARE_FOLLOW_SYMLINKS)) {
      /*
       * Verify that either the path is same as share path or the path until the
       * parent directory is within the share.
       *
       * XXX: Symlink check could become susceptible to TOCTOU (time-of-check,
       * time-of-use) attack when we move to asynchrounous HGFS operations.
       * We should use the resolved file path for further file system
       * operations, instead of using the one passed from the client.
       */
      nameStatus = HgfsServerHasSymlink(myBufOut, myBufOutLen, shareInfo->rootDir,
                                        shareInfo->rootDirLen);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("%s: parent path failed to be resolved: %d\n",
                 __FUNCTION__, nameStatus));
         goto error;
      }
   }

   {
      char *p;

      /* Trim unused memory */

      /* Enough space for resulting string + NUL termination */
      p = realloc(myBufOut, (myBufOutLen + 1) * sizeof *p);
      if (!p) {
         LOG(4, ("%s: failed to trim memory\n", __FUNCTION__));
      } else {
         myBufOut = p;
      }

      if (outLen) {
         *outLen = myBufOutLen;
      }
   }

   LOG(4, ("%s: name is \"%s\"\n", __FUNCTION__, myBufOut));

   *bufOut = myBufOut;

   return HGFS_NAME_STATUS_COMPLETE;

error:
   free(myBufOut);

   return nameStatus;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerIsSharedFolderOnly --
 *
 *    Test a name if it is a shared folder only or not
 *
 *    This function assumes that CPName_GetComponent() will always succeed
 *    with a size greater than 0, so it must ONLY be called after a call to
 *    HgfsServerGetShareInfo() that returns HGFS_NAME_STATUS_COMPLETE.
 *
 * Results:
 *    True if it is a shared folder only, otherwise false
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerIsSharedFolderOnly(char const *cpName,// IN:  Cross-platform filename to check
                             size_t cpNameSize) // IN:  Size of name cpName
{
   char const *inEnd;
   char const *next;
   int len;

   ASSERT(cpName);

   inEnd = cpName + cpNameSize;
   len = CPName_GetComponent(cpName, inEnd, &next);

   ASSERT(len > 0);
   (void) len; /* Shuts up gcc's -Werror=unused-but-set-variable. */

   return (next == inEnd);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDumpDents --
 *
 *    Dump a set of directory entries (debugging code)
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
HgfsServerDumpDents(HgfsHandle searchHandle,  // IN: Handle to dump dents from
                    HgfsSessionInfo *session) // IN: Session info
{
#ifdef VMX86_LOG
   unsigned int i;
   HgfsSearch *search;

   MXUser_AcquireExclLock(session->searchArrayLock);

   search = HgfsSearchHandle2Search(searchHandle, session);
   if (search != NULL) {
      Log("%s: %u dents in \"%s\"\n", __FUNCTION__, search->numDents,
          search->utf8Dir);

      Log("Dumping dents:\n");
      for (i = 0; i < search->numDents; i++) {
         Log("\"%s\"\n", search->dents[i]->d_name);
      }
   }

   MXUser_ReleaseExclLock(session->searchArrayLock);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetDents --
 *
 *    Get directory entry names from the given callback function, and
 *    build an array of DirectoryEntrys of all the names. Somewhat similar to
 *    scandir(3) on linux, but more general.
 *
 * Results:
 *    On success, the number of directory entries found.
 *    On failure, negative error.
 *
 * Side effects:
 *    Memory allocation.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsServerGetDents(HgfsGetNameFunc getName,     // IN: Function to get name
                   HgfsInitFunc initName,       // IN: Setup function
                   HgfsCleanupFunc cleanupName, // IN: Cleanup function
                   DirectoryEntry ***dents)     // OUT: Array of DirectoryEntrys
{
   uint32 totalDents = 0;   // Number of allocated dents
   uint32 numDents = 0;     // Current actual number of dents
   DirectoryEntry **myDents = NULL; // So realloc is happy w/ zero numDents
   void *state;

   state = initName();
   if (!state) {
      LOG(4, ("%s: Couldn't init state\n", __FUNCTION__));
      goto error_free;
   }

   for (;;) {
      DirectoryEntry *pDirEntry;
      char const *name;
      size_t len;
      Bool done = FALSE;
      size_t newDirEntryLen;
      size_t maxLen;

      /* Add '.' and ".." as the first dents. */
      if (numDents == 0) {
         name = ".";
         len = 1;
      } else if (numDents == 1) {
         name = "..";
         len = 2;
      } else {
         if (!getName(state, &name, &len, &done)) {
            LOG(4, ("%s: Couldn't get next name\n", __FUNCTION__));
            goto error;
         }
      }

      if (done) {
         LOG(4, ("%s: No more names\n", __FUNCTION__));
         break;
      }

#if defined(sun)
      /*
       * Solaris lacks a single definition of NAME_MAX and using pathconf(), to
       * determine NAME_MAX for the current directory, is too cumbersome for
       * our purposes, so we use PATH_MAX as a reasonable upper bound on the
       * length of the name.
       */
      maxLen = PATH_MAX;
#else
      maxLen = sizeof pDirEntry->d_name;
#endif
      if (len >= maxLen) {
         Log("%s: Error: Name \"%s\" is too long.\n", __FUNCTION__, name);
         continue;
      }

      /* See if we need to allocate more memory */
      if (numDents == totalDents) {
         void *p;

         if (totalDents != 0) {
            totalDents *= 2;
         } else {
            totalDents = 100;
         }
         p = realloc(myDents, totalDents * sizeof *myDents);
         if (!p) {
            LOG(4, ("%s: Couldn't reallocate array memory\n", __FUNCTION__));
            goto error;
         }
         myDents = (DirectoryEntry **)p;
      }

      /* This file/directory can be added to the list. */
      LOG(4, ("%s: Nextfilename = \"%s\"\n", __FUNCTION__, name));

      /*
       * Start with the size of the DirectoryEntry struct, subtract the static
       * length of the d_name buffer (256 in Linux, 1 in Solaris, etc) and add
       * back just enough space for the UTF-8 name and nul terminator.
       */

      newDirEntryLen = sizeof *pDirEntry - sizeof pDirEntry->d_name + len + 1;
      pDirEntry = (DirectoryEntry *)malloc(newDirEntryLen);
      if (!pDirEntry) {
         LOG(4, ("%s: Couldn't allocate dentry memory\n", __FUNCTION__));
         goto error;
      }
      pDirEntry->d_reclen = (unsigned short)newDirEntryLen;
      memcpy(pDirEntry->d_name, name, len);
      pDirEntry->d_name[len] = 0;

      myDents[numDents] = pDirEntry;
      numDents++;
   }

   /* We are done; cleanup the state */
   if (!cleanupName(state)) {
      LOG(4, ("%s: Non-error cleanup failed\n", __FUNCTION__));
      goto error_free;
   }

   /* Trim extra memory off of dents */
   {
      void *p;

      p = realloc(myDents, numDents * sizeof *myDents);
      if (!p) {
         LOG(4, ("%s: Couldn't realloc less array memory\n", __FUNCTION__));
         *dents = myDents;
      } else {
         *dents = (DirectoryEntry **)p;
      }
   }

   return numDents;

error:
   /* Cleanup the callback state */
   if (!cleanupName(state)) {
      LOG(4, ("%s: Error cleanup failed\n", __FUNCTION__));
   }

error_free:
   /* Free whatever has been allocated so far */
   {
      unsigned int i;

      for (i = 0; i < numDents; i++) {
         free(myDents[i]);
      }

      free(myDents);
   }

   return -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchRealDir --
 *
 *    Handle a search on a real directory. Takes a pointer to an enumerator
 *    for the directory's contents and returns a handle to a search that is
 *    correctly set up with the real directory's entries.
 *
 *    The casual reader will notice that the "type" of this search is obviously
 *    always DIRECTORY_SEARCH_TYPE_DIR, but the caller is nonetheless required
 *    to pass it in, for completeness' sake with respect to
 *    HgfsServerSearchVirtualDir.
 *
 * Results:
 *    Zero on success, returns a handle to the created search.
 *    Non-zero on failure.
 *
 * Side effects:
 *    Memory allocation on success
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSearchRealDir(char const *baseDir,      // IN: Directory to search
                        size_t baseDirLen,        // IN: Length of directory
                        char const *shareName,    // IN: Share name containing the directory
                        char const *rootDir,      // IN: Shared folder root directory
                        HgfsSessionInfo *session, // IN: Session info
                        HgfsHandle *handle)       // OUT: Search handle
{
   HgfsSearch *search = NULL;
   HgfsInternalStatus status = 0;
   HgfsNameStatus nameStatus;
   int numDents;
   Bool followSymlinks;
   HgfsShareOptions configOptions;

   ASSERT(baseDir);
   ASSERT(handle);
   ASSERT(shareName);

   MXUser_AcquireExclLock(session->searchArrayLock);

   search = HgfsAddNewSearch(baseDir, DIRECTORY_SEARCH_TYPE_DIR, shareName,
                             rootDir, session);
   if (!search) {
      LOG(4, ("%s: failed to get new search\n", __FUNCTION__));
      status = HGFS_ERROR_INTERNAL;
      goto out;
   }

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(shareName, strlen(shareName),
                                                 &configOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, shareName));
      status = HGFS_ERROR_INTERNAL;
      HgfsRemoveSearchInternal(search, session);
      goto out;
   }

   followSymlinks = HgfsServerPolicy_IsShareOptionSet(configOptions,
                                                      HGFS_SHARE_FOLLOW_SYMLINKS);

   status = HgfsServerScandir(baseDir, baseDirLen, followSymlinks,
                              &search->dents, &numDents);
   if (status != 0) {
      LOG(4, ("%s: couldn't scandir\n", __FUNCTION__));
      HgfsRemoveSearchInternal(search, session);
      goto out;
   }

   search->numDents = numDents;
   *handle = HgfsSearch2SearchHandle(search);

  out:
   MXUser_ReleaseExclLock(session->searchArrayLock);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchVirtualDir --
 *
 *    Handle a search on a virtual directory (i.e. one that does not
 *    really exist on the server). Takes a pointer to an enumerator
 *    for the directory's contents and returns a handle to a search that is
 *    correctly set up with the virtual directory's entries.
 *
 * Results:
 *    Zero on success, returns a handle to the created search.
 *    Non-zero on failure.
 *
 * Side effects:
 *    Memory allocation on success
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsServerSearchVirtualDir(HgfsGetNameFunc *getName,     // IN: Name enumerator
                           HgfsInitFunc *initName,       // IN: Init function
                           HgfsCleanupFunc *cleanupName, // IN: Cleanup function
                           DirectorySearchType type,     // IN: Kind of search
                           HgfsSessionInfo *session,     // IN: Session info
                           HgfsHandle *handle)           // OUT: Search handle
{
   HgfsInternalStatus status = 0;
   HgfsSearch *search = NULL;
   int result = 0;

   ASSERT(getName);
   ASSERT(initName);
   ASSERT(cleanupName);
   ASSERT(handle);

   MXUser_AcquireExclLock(session->searchArrayLock);

   search = HgfsAddNewSearch("", type, "", "", session);
   if (!search) {
      LOG(4, ("%s: failed to get new search\n", __FUNCTION__));
      status = HGFS_ERROR_INTERNAL;
      goto out;
   }

   result = HgfsServerGetDents(getName, initName, cleanupName, &search->dents);
   if (result < 0) {
      LOG(4, ("%s: couldn't get dents\n", __FUNCTION__));
      HgfsRemoveSearchInternal(search, session);
      status = HGFS_ERROR_INTERNAL;
      goto out;
   }

   search->numDents = result;
   *handle = HgfsSearch2SearchHandle(search);

  out:
   MXUser_ReleaseExclLock(session->searchArrayLock);

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveFromCache --
 *
 *    Grab a node cache lock and call HgfsRemoveFromCacheInternal.
 *
 *    If the node was not already in the cache then nothing is done.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsRemoveFromCache(HgfsHandle handle,	      // IN: Hgfs handle to the node
                    HgfsSessionInfo *session) // IN: Session info
{
   Bool removed = FALSE;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   removed = HgfsRemoveFromCacheInternal(handle, session);
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return removed;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsCached --
 *
 *    Grab a lock and call HgfsIsCachedInternal.
 *
 * Results:
 *    TRUE if the node is found in the cache.
 *    FALSE if the node is not in the cache.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsIsCached(HgfsHandle handle,         // IN: Structure representing file node
             HgfsSessionInfo *session)  // IN: Session info
{
   Bool cached = FALSE;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   cached = HgfsIsCachedInternal(handle, session);
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return cached;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveLruNode--
 *
 *    Removes the least recently used node in the cache. The first node is
 *    removed since most recently used nodes are moved to the end of the
 *    list.
 *
 *    XXX: Right now we do not remove nodes that have server locks on them
 *         This is not correct and should be fixed before the release.
 *         Instead we should cancel the server lock (by calling IoCancel)
 *         notify client of the lock break, and close the file.
 *
 *    Assumes that there is at least one node in the cache.
 *
 *    The session's nodeArrayLock should be acquired prior to calling this
 *    function.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsRemoveLruNode(HgfsSessionInfo *session)   // IN: session info
{
   HgfsFileNode *lruNode = NULL;
   HgfsHandle handle;
   Bool found = FALSE;
   uint32 numOpenNodes = session->numCachedOpenNodes;

   ASSERT(session);
   ASSERT(session->numCachedOpenNodes > 0);

   /*
    * Remove the first item from the list that does not have a server lock,
    * file context or is open in sequential mode.
    */
   while (!found && (numOpenNodes-- > 0)) {
      lruNode = DblLnkLst_Container(session->nodeCachedList.next,
                                    HgfsFileNode, links);

      ASSERT(lruNode->state == FILENODE_STATE_IN_USE_CACHED);
      if (lruNode->serverLock != HGFS_LOCK_NONE || lruNode->fileCtx != NULL
          || (lruNode->flags & HGFS_FILE_NODE_SEQUENTIAL_FL) != 0) {
         /*
	  * Move this node with the server lock to the beginning of the list.
	  * Also, prevent files opened in HGFS_FILE_NODE_SEQUENTIAL_FL mode
	  * from being closed. -- On some platforms, this mode does not
	  * allow files to be closed/re-opened (eg: When restoring a file
	  * into a Windows guest you cannot use BackupWrite, then close and
	  * re-open the file and continue to use BackupWrite.
	  */
         DblLnkLst_Unlink1(&lruNode->links);
         DblLnkLst_LinkLast(&session->nodeCachedList, &lruNode->links);
      } else {
         found = TRUE;
      }
   }
   if (found) {
      handle = HgfsFileNode2Handle(lruNode);
      if (!HgfsRemoveFromCacheInternal(handle, session)) {
         LOG(4, ("%s: Could not remove the node from cache.\n", __FUNCTION__));
         return FALSE;
      }
   } else {
      LOG(4, ("%s: Could not find a node to remove from cache.\n", __FUNCTION__));
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddToCache --
 *
 *    Grabs the cache lock and calls HgfsAddToCacheInternal.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsAddToCache(HgfsHandle handle,        // IN: HGFS file handle
               HgfsSessionInfo *session) // IN: Session info
{
   Bool added = FALSE;

   MXUser_AcquireExclLock(session->nodeArrayLock);
   added = HgfsAddToCacheInternal(handle, session);
   MXUser_ReleaseExclLock(session->nodeArrayLock);

   return added;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCreateAndCacheFileNode --
 *
 *    Get a node from the free node list and cache it.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsCreateAndCacheFileNode(HgfsFileOpenInfo *openInfo, // IN: Open info struct
                           HgfsLocalId const *localId, // IN: Local unique file ID
                           fileDesc fileDesc,          // IN: Handle to the fileopenInfo,
                           Bool append,                // IN: flag to append
                           HgfsSessionInfo *session)   // IN: session info
{
   HgfsHandle handle;
   HgfsFileNode *node = NULL;
   char const *inEnd;
   char const *next;
   int len;
   Bool sharedFolderOpen = FALSE;

   ASSERT(openInfo);
   ASSERT(localId);
   ASSERT(session);

   inEnd = openInfo->cpName + openInfo->cpNameSize;

   /*
    * Get first component.
    */

   len = CPName_GetComponent(openInfo->cpName, inEnd, &next);
   if (len < 0) {
      LOG(4, ("%s: get first component failed\n", __FUNCTION__));
      HgfsCloseFile(fileDesc, NULL);
      return FALSE;
   }

   /* See if we are dealing with the base of the namespace */
   if (!len) {
      HgfsCloseFile(fileDesc, NULL);
      return FALSE;
   }

   if (!next) {
      sharedFolderOpen = TRUE;
   }

   MXUser_AcquireExclLock(session->nodeArrayLock);

   node = HgfsAddNewFileNode(openInfo, localId, fileDesc, append, len,
                             openInfo->cpName, sharedFolderOpen, session);

   if (node == NULL) {
      LOG(4, ("%s: Failed to add new node.\n", __FUNCTION__));
      MXUser_ReleaseExclLock(session->nodeArrayLock);

      HgfsCloseFile(fileDesc, NULL);
      return FALSE;
   }
   handle = HgfsFileNode2Handle(node);

   if (!HgfsAddToCacheInternal(handle, session)) {
      HgfsFreeFileNodeInternal(handle, session);
      HgfsCloseFile(fileDesc, NULL);

      LOG(4, ("%s: Failed to add node to the cache.\n", __FUNCTION__));
      MXUser_ReleaseExclLock(session->nodeArrayLock);

      return FALSE;
   }

   MXUser_ReleaseExclLock(session->nodeArrayLock);

   /* Only after everything is successful, save the handle in the open info. */
   openInfo->file = handle;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAllocInitReply --
 *
 *    Allocates hgfs reply packet and calculates pointer to HGFS payload.
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
HgfsAllocInitReply(HgfsPacket *packet,           // IN/OUT: Hgfs Packet
                   char const *packetHeader,     // IN: packet header
                   size_t payloadSize,           // IN: payload size
                   void **payload,               // OUT: pointer to the reply payload
                   HgfsSessionInfo *session)     // IN: Session Info
{
   HgfsRequest *request = (HgfsRequest *)packetHeader;
   size_t replyPacketSize;
   size_t headerSize = 0; /* Replies prior to V3 do not have a header. */
   Bool result = FALSE;
   char *reply;

   if (HGFS_V4_LEGACY_OPCODE == request->op) {
      headerSize = sizeof(HgfsHeader);
   } else if (request->op < HGFS_OP_CREATE_SESSION_V4 &&
              request->op > HGFS_OP_RENAME_V2) {
      headerSize = sizeof(HgfsReply);
   }
   replyPacketSize = headerSize + payloadSize;
   reply = HSPU_GetReplyPacket(packet, &replyPacketSize, session->transportSession);

   if (reply && (replyPacketSize >= headerSize + payloadSize)) {
      memset(reply, 0, headerSize + payloadSize);
      result = TRUE;
      if (payloadSize > 0) {
         *payload = reply + headerSize;
      } else {
         *payload = NULL;
      }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerRead --
 *
 *    Handle a Read request.
 *
 * Results:
 *    HGFS_ERROR_SUCCESS on success.
 *    HGFS error code on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerRead(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   HgfsHandle file;
   uint64 offset;
   uint32 requiredSize;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (!HgfsUnpackReadRequest(input->payload, input->payloadSize, input->op, &file,
                              &offset, &requiredSize)) {
      LOG(4, ("%s: Failed to unpack a valid packet -> PROTOCOL_ERROR.\n", __FUNCTION__));
      status = HGFS_ERROR_PROTOCOL;
   } else {
      switch(input->op) {
      case HGFS_OP_READ_FAST_V4:
      case HGFS_OP_READ_V3: {
            HgfsReplyReadV3 *reply;
            void *payload;
            uint32 inlineDataSize =
               (HGFS_OP_READ_FAST_V4 == input->op) ? 0 : requiredSize;

            if (!HgfsAllocInitReply(input->packet, input->metaPacket,
                                    sizeof *reply + inlineDataSize, (void **)&reply,
                                    input->session)) {
               status = HGFS_ERROR_PROTOCOL;
               LOG(4, ("%s: V3/V4 Failed to alloc reply -> PROTOCOL_ERROR.\n", __FUNCTION__));
            } else {
               if (HGFS_OP_READ_V3 == input->op) {
                  payload = &reply->payload[0];
               } else {
                  payload = HSPU_GetDataPacketBuf(input->packet, BUF_WRITEABLE,
                                                  input->transportSession);
               }
               if (payload) {
                  status = HgfsPlatformReadFile(file, input->session, offset,
                                                requiredSize, payload,
                                                &reply->actualSize);
                  if (HGFS_ERROR_SUCCESS == status) {
                     reply->reserved = 0;
                     replyPayloadSize = sizeof *reply +
                                         ((inlineDataSize > 0) ? reply->actualSize : 0);
                  }
               } else {
                  status = HGFS_ERROR_PROTOCOL;
                  LOG(4, ("%s: V3/V4 Failed to get payload -> PROTOCOL_ERROR.\n", __FUNCTION__));
               }
            }
            break;
         }
      case HGFS_OP_READ: {
            HgfsReplyRead *reply;

            if (HgfsAllocInitReply(input->packet, input->metaPacket,
                                   sizeof *reply + requiredSize, (void **)&reply,
                                   input->session)) {
               status = HgfsPlatformReadFile(file, input->session, offset, requiredSize,
                                             reply->payload, &reply->actualSize);
               if (HGFS_ERROR_SUCCESS == status) {
                  replyPayloadSize = sizeof *reply + reply->actualSize;
               } else {
                  LOG(4, ("%s: V1 Failed to read-> %d.\n", __FUNCTION__, status));
               }
            } else {
               status = HGFS_ERROR_PROTOCOL;
               LOG(4, ("%s: V1 Failed to alloc reply -> PROTOCOL_ERROR.\n", __FUNCTION__));
            }
            break;
         }
      default:
         NOT_IMPLEMENTED();
         status = HGFS_ERROR_PROTOCOL;
         LOG(4, ("%s: Unsupported protocol version passed %d -> PROTOCOL_ERROR.\n",
                 __FUNCTION__, input->op));
      }
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerWrite --
 *
 *    Handle a Write request.
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
HgfsServerWrite(HgfsInputParam *input)  // IN: Input params
{
   uint32 numberBytesToWrite;
   HgfsInternalStatus status;
   HgfsWriteFlags flags;
   uint64 offset;
   char *dataToWrite;
   uint32 replyActualSize;
   size_t replyPayloadSize = 0;
   HgfsHandle file;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackWriteRequest(input, &file, &offset, &numberBytesToWrite,
                              &flags, &dataToWrite)) {

      status = HgfsPlatformWriteFile(file, input->session, offset, numberBytesToWrite,
                                     flags, dataToWrite, &replyActualSize);
      if (HGFS_ERROR_SUCCESS == status) {
          if (!HgfsPackWriteReply(input->packet, input->metaPacket, input->op,
                                  replyActualSize, &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
          }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsQueryVolume --
 *
 *    Performs actual work to get free space and capacity for a volume or
 *    a group of volumes.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *
 *-----------------------------------------------------------------------------
 */
HgfsInternalStatus
HgfsQueryVolume(HgfsSessionInfo *session,   // IN: session info
                char *fileName,             // IN: cpName for the volume
                size_t fileNameLength,      // IN: cpName length
                uint32 caseFlags,           // IN: case sensitive/insensitive name
                uint64 *freeBytes,          // OUT: free space in bytes
                uint64 *totalBytes)         // OUT: capacity in bytes
{
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;
   uint64 outFreeBytes = 0;
   uint64 outTotalBytes = 0;
   char *utf8Name = NULL;
   size_t utf8NameLen;
   HgfsNameStatus nameStatus;
   Bool firstShare = TRUE;
   HgfsShareInfo shareInfo;
   HgfsHandle handle;
   VolumeInfoType infoType;
   DirectoryEntry *dent;
   size_t failed = 0;
   size_t shares = 0;
   int offset = 0;
   HgfsInternalStatus firstErr = HGFS_ERROR_SUCCESS;
   Bool success;

   /* It is now safe to read the file name field. */
   nameStatus = HgfsServerGetShareInfo(fileName,
                                       fileNameLength,
                                       caseFlags,
                                       &shareInfo,
                                       &utf8Name,
                                       &utf8NameLen);

   switch (nameStatus) {
   case HGFS_NAME_STATUS_INCOMPLETE_BASE:
      /*
       * This is the base of our namespace. Clients can request a
       * QueryVolumeInfo on it, on individual shares, or on just about
       * any pathname.
       */

      LOG(4,("%s: opened search on base\n", __FUNCTION__));
      status = HgfsServerSearchVirtualDir(HgfsServerPolicy_GetShares,
                                          HgfsServerPolicy_GetSharesInit,
                                          HgfsServerPolicy_GetSharesCleanup,
                                          DIRECTORY_SEARCH_TYPE_BASE,
                                          session,
                                          &handle);
      if (status != 0) {
         return status;
      }

      /*
       * If we're outside the Tools, find out if we're to compute the minimum
       * values across all shares, or the maximum values.
       */
      infoType = VOLUME_INFO_TYPE_MIN;
#ifndef VMX86_TOOLS
      {
         char *volumeInfoType = Config_GetString("min",
                                                 "tools.hgfs.volumeInfoType");
         if (!Str_Strcasecmp(volumeInfoType, "max")) {
            infoType = VOLUME_INFO_TYPE_MAX;
         }
         free(volumeInfoType);
      }
#endif

      /*
       * Now go through all shares and get share paths on the server.
       * Then retrieve space info for each share's volume.
       */
      offset = 0;
      while ((dent = HgfsGetSearchResult(handle, session, offset,
                                         TRUE)) != NULL) {
         char const *sharePath;
         size_t sharePathLen;
         uint64 currentFreeBytes  = 0;
         uint64 currentTotalBytes = 0;
         size_t length;

         length = strlen(dent->d_name);

         /*
          * Now that the server is passing '.' and ".." around as dents, we
          * need to make sure to handle them properly. In particular, they
          * should be ignored within QueryVolume, as they're not real shares.
          */
         if (!strcmp(dent->d_name, ".") || !strcmp(dent->d_name, "..")) {
            LOG(4, ("%s: Skipping fake share %s\n", __FUNCTION__,
                    dent->d_name));
            free(dent);
            continue;
         }

         /*
          * The above check ignores '.' and '..' so we do not include them in
          * the share count here.
          */
         shares++;

         /*
          * Check permission on the share and get the share path.  It is not
          * fatal if these do not succeed.  Instead we ignore the failures
          * (apart from logging them) until we have processed all shares.  Only
          * then do we check if there were any failures; if all shares failed
          * to process then we bail out with an error code.
          */

         nameStatus = HgfsServerPolicy_GetSharePath(dent->d_name, length,
                                                    HGFS_OPEN_MODE_READ_ONLY,
                                                    &sharePathLen,
                                                    &sharePath);
         free(dent);
         if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
            LOG(4, ("%s: No such share or access denied\n", __FUNCTION__));
            if (0 == firstErr) {
               firstErr = HgfsPlatformConvertFromNameStatus(nameStatus);
            }
            failed++;
            continue;
         }

         /*
          * Pick the drive with amount of space available and return that
          * according to different volume info type.
          */


         if (!HgfsServerStatFs(sharePath, sharePathLen,
                               &currentFreeBytes, &currentTotalBytes)) {
            LOG(4, ("%s: error getting volume information\n",
                    __FUNCTION__));
            if (0 == firstErr) {
               firstErr = HGFS_ERROR_IO;
            }
            failed++;
            continue;
         }

         /*
          * Pick the drive with amount of space available and return that
          * according to different volume info type.
          */
         switch (infoType) {
         case VOLUME_INFO_TYPE_MIN:
            if ((outFreeBytes > currentFreeBytes) || firstShare) {
               firstShare = FALSE;
               outFreeBytes  = currentFreeBytes;
               outTotalBytes = currentTotalBytes;
            }
            break;
         case VOLUME_INFO_TYPE_MAX:
            if ((outFreeBytes < currentFreeBytes)) {
               outFreeBytes  = currentFreeBytes;
               outTotalBytes = currentTotalBytes;
            }
            break;
         default:
            NOT_IMPLEMENTED();
         }
      }
      if (!HgfsRemoveSearch(handle, session)) {
         LOG(4, ("%s: could not close search on base\n", __FUNCTION__));
      }
      if (shares == failed) {
         if (firstErr != 0) {
            /*
             * We failed to query any of the shares.  We return the error]
             * from the first share failure.
             */
            status = firstErr;
         }
         /* No shares but no error, return zero for sizes and success. */
      }
      break;
   case HGFS_NAME_STATUS_COMPLETE:
      ASSERT(utf8Name);
      LOG(4,("%s: querying path %s\n", __FUNCTION__, utf8Name));
      success = HgfsServerStatFs(utf8Name, utf8NameLen,
                                 &outFreeBytes, &outTotalBytes);
      free(utf8Name);
      if (!success) {
         LOG(4, ("%s: error getting volume information\n", __FUNCTION__));
         status = HGFS_ERROR_IO;
      }
      break;
   default:
      LOG(4,("%s: file access check failed\n", __FUNCTION__));
      status = HgfsPlatformConvertFromNameStatus(nameStatus);
   }

   *freeBytes  = outFreeBytes;
   *totalBytes = outTotalBytes;
   LOG(4, ("%s: return %"FMT64"u bytes Free %"FMT64"u bytes\n", __FUNCTION__,
          outTotalBytes, outFreeBytes));

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerQueryVolume --
 *
 *    Handle a Query Volume request.
 *
 *    Right now we only handle the volume space request. Call Wiper library
 *    to get the volume information.
 *    It is possible that shared folders can belong to different volumes on
 *    the server. If this is the case, default to return the space information
 *    of the volume that has the least amount of the available space, but it's
 *    configurable with a config option (tools.hgfs.volumeInfoType). 2 possible
 *    options, min and max.
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
HgfsServerQueryVolume(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   size_t replyPayloadSize = 0;
   HgfsHandle file;
   char *fileName;
   size_t fileNameLength;
   uint32 caseFlags;
   Bool useHandle;
   uint64 freeBytes;
   uint64 totalBytes;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackQueryVolumeRequest(input->payload, input->payloadSize, input->op,
                                    &useHandle, &fileName,
                                    &fileNameLength, &caseFlags, &file)) {
      /*
       * We don't yet support file handle for this operation.
       * Clients should retry using the file name.
       */
      if (useHandle) {
         LOG(4, ("%s: Doesn't support file handle.\n", __FUNCTION__));
         status = HGFS_ERROR_INVALID_PARAMETER;
      } else {
         status = HgfsQueryVolume(input->session, fileName, fileNameLength, caseFlags,
                                  &freeBytes, &totalBytes);
         if (HGFS_ERROR_SUCCESS == status) {
            if (!HgfsPackQueryVolumeReply(input->packet, input->metaPacket,
                                          input->op, freeBytes, totalBytes,
                                          &replyPayloadSize, input->session)) {
               status = HGFS_ERROR_INTERNAL;
            }
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsSymlinkCreate --
 *
 *    Platform independent function that verifies whether symbolic link creation
 *    is allowed for the specific shared folder and then calls platform specific
 *    HgfsPlatformSymlinkCreate to do the actual job.
 *
 * Results:
 *    Zero on success.
 *    Non-zero on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsSymlinkCreate(HgfsSessionInfo *session, // IN: session info,
                  char *srcFileName,        // IN: symbolic link file name
                  uint32 srcFileNameLength, // IN: symbolic link name length
                  uint32 srcCaseFlags,      // IN: symlink case flags
                  char *trgFileName,        // IN: symbolic link target name
                  uint32 trgFileNameLength, // IN: target name length
                  uint32 trgCaseFlags)      // IN: target case flags
{
   HgfsShareInfo shareInfo;
   HgfsInternalStatus status = 0;
   HgfsNameStatus nameStatus;
   HgfsShareOptions configOptions;
   char *localSymlinkName = NULL;
   size_t localSymlinkNameLen;
   char localTargetName[HGFS_PACKET_MAX];

   /*
    * It is now safe to read the symlink file name and the
    * "targetName" field
    */

   nameStatus = HgfsServerGetShareInfo(srcFileName,
                                       srcFileNameLength,
                                       srcCaseFlags,
                                       &shareInfo,
                                       &localSymlinkName,
                                       &localSymlinkNameLen);
   if (nameStatus == HGFS_NAME_STATUS_COMPLETE) {
      if (shareInfo.writePermissions ) {
         /* Get the config options. */
         nameStatus = HgfsServerPolicy_GetShareOptions(srcFileName, srcFileNameLength,
                                                       &configOptions);
         if (nameStatus == HGFS_NAME_STATUS_COMPLETE) {
            /* Prohibit symlink ceation if symlink following is enabled. */
            if (HgfsServerPolicy_IsShareOptionSet(configOptions, HGFS_SHARE_FOLLOW_SYMLINKS)) {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
         } else {
            LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, srcFileName));
            status = HgfsPlatformConvertFromNameStatus(nameStatus);
         }
      } else {
         status = HgfsPlatformFileExists(localSymlinkName);
         if (status != 0) {
            if (status == HGFS_ERROR_FILE_NOT_FOUND) {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
         } else {
            status = HGFS_ERROR_FILE_EXIST;
         }
         LOG(4, ("%s: failed access check, error %d\n", __FUNCTION__, status));
      }
   } else {
      LOG(4, ("%s: symlink name access check failed\n", __FUNCTION__));
      status = HgfsPlatformConvertFromNameStatus(nameStatus);
   }
   if (HGFS_ERROR_SUCCESS == status) {
      /* Convert from CPName-lite to normal and NUL-terminate. */
      memcpy(localTargetName, trgFileName, trgFileNameLength);
      CPNameLite_ConvertFrom(localTargetName, trgFileNameLength, DIRSEPC);
      localTargetName[trgFileNameLength] = '\0';

      status = HgfsPlatformSymlinkCreate(localSymlinkName, localTargetName);
   }

   free(localSymlinkName);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSymlinkCreate --
 *
 *    Handle a SymlinkCreate request.
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
HgfsServerSymlinkCreate(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   HgfsHandle srcFile;
   char *srcFileName;
   size_t srcFileNameLength;
   uint32 srcCaseFlags;
   Bool srcUseHandle;
   HgfsHandle trgFile;
   char *trgFileName;
   size_t trgFileNameLength;
   uint32 trgCaseFlags;
   Bool trgUseHandle;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackSymlinkCreateRequest(input->payload, input->payloadSize, input->op,
                                      &srcUseHandle, &srcFileName,
                                      &srcFileNameLength, &srcCaseFlags, &srcFile,
                                      &trgUseHandle, &trgFileName,
                                      &trgFileNameLength, &trgCaseFlags, &trgFile)) {
      /*
       * We don't yet support file handle for this operation.
       * Clients should retry using the file name.
       */
      if (srcUseHandle || trgUseHandle) {
         LOG(4, ("%s: Doesn't support file handle.\n", __FUNCTION__));
         status = HGFS_ERROR_INVALID_PARAMETER;
      } else {
         status = HgfsSymlinkCreate(input->session, srcFileName, srcFileNameLength,
                                    srcCaseFlags, trgFileName, trgFileNameLength,
                                    trgCaseFlags);
         if (HGFS_ERROR_SUCCESS == status) {
            if (!HgfsPackSymlinkCreateReply(input->packet, input->metaPacket, input->op,
                                            &replyPayloadSize, input->session)) {
               status = HGFS_ERROR_INTERNAL;
            }
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchOpen --
 *
 *    Handle a search open request.
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
HgfsServerSearchOpen(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   size_t replyPayloadSize = 0;
   char *dirName;
   uint32 dirNameLength;
   uint32 caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   HgfsHandle search;
   HgfsNameStatus nameStatus;
   HgfsShareInfo shareInfo;
   char *baseDir = NULL;
   size_t baseDirLen;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackSearchOpenRequest(input->payload, input->payloadSize, input->op,
                                   &dirName, &dirNameLength, &caseFlags)) {
      nameStatus = HgfsServerGetShareInfo(dirName, dirNameLength, caseFlags, &shareInfo,
                                          &baseDir, &baseDirLen);
      status = HgfsPlatformSearchDir(nameStatus, dirName, dirNameLength, caseFlags,
                                     &shareInfo, baseDir, baseDirLen,
                                     input->session, &search);
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackSearchOpenReply(input->packet, input->metaPacket, input->op, search,
                                      &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsValidateRenameFile --
 *
 *    Validates if the file can can participate in rename process either as
 *    as a source or as a target.
 *
 * Results:
 *    HGFS_ERROR_SUCCESS if rename operation is allowed.
 *    Appropriate error code otherwise.
 *
 * Side effects:
 *    Allcates locaFileName which must be freed by the caller.
 *
 *-----------------------------------------------------------------------------
 */

HgfsInternalStatus
HgfsValidateRenameFile(Bool useHandle,            // IN:
                       HgfsHandle fileHandle,     // IN:
                       char *cpName,              // IN:
                       size_t cpNameLength,       // IN:
                       uint32 caseFlags,          // IN:
                       HgfsSessionInfo *session,  // IN: Session info
                       fileDesc* descr,           // OUT:
                       HgfsShareInfo *shareInfo,  // OUT:
                       char **localFileName,      // OUT:
                       size_t *localNameLength)   // OUT:
{
   HgfsInternalStatus status;
   Bool sharedFolderOpen = FALSE;
   HgfsServerLock serverLock = HGFS_LOCK_NONE;
   HgfsNameStatus nameStatus;


   if (useHandle) {
      status = HgfsPlatformGetFd(fileHandle, session, FALSE, descr);

      if (HGFS_ERROR_SUCCESS != status) {
         LOG(4, ("%s: could not map cached handle %d, error %u\n",
                 __FUNCTION__, fileHandle, status));
      } else if (!HgfsHandle2FileNameMode(fileHandle, session, &shareInfo->writePermissions,
                                          &shareInfo->readPermissions, localFileName,
                                          localNameLength)) {
         /*
          * HgfsPlatformRename requires valid source file name even when file handle
          * is specified.
          * Also the name will be required to update the nodes on a successful
          * rename operation.
          */
        LOG(4, ("%s: could not get file name for fd %d\n", __FUNCTION__,
                *descr));
        status = HGFS_ERROR_INVALID_HANDLE;
      } else if (HgfsHandleIsSharedFolderOpen(fileHandle, session, &sharedFolderOpen) &&
                                              sharedFolderOpen) {
         LOG(4, ("%s: Cannot rename shared folder\n", __FUNCTION__));
         status = HGFS_ERROR_ACCESS_DENIED;
      }
   } else {
      nameStatus = HgfsServerGetShareInfo(cpName,
                                          cpNameLength,
                                          caseFlags,
                                          shareInfo,
                                          localFileName,
                                          localNameLength);
      if (HGFS_NAME_STATUS_COMPLETE != nameStatus) {
         LOG(4, ("%s: access check failed\n", __FUNCTION__));
         status = HgfsPlatformConvertFromNameStatus(nameStatus);
      } else if (HgfsServerIsSharedFolderOnly(cpName, cpNameLength)) {
         /* Guest OS is not allowed to rename shared folder. */
         LOG(4, ("%s: Cannot rename shared folder\n", __FUNCTION__));
         status = HGFS_ERROR_ACCESS_DENIED;
      } else {
         status = HGFS_ERROR_SUCCESS;
      }
   }

   ASSERT(*localFileName != NULL || HGFS_ERROR_SUCCESS != status);

   if (HGFS_ERROR_SUCCESS == status) {
      if (HgfsFileHasServerLock(*localFileName, session, &serverLock, descr)) {
         /*
          * XXX: Before renaming the file, check to see if we are holding
          * an oplock on both the old and new files. If one of them is oplocked, and
          * we commence with the rename, we'll trigger an oplock break that'll
          * deadlock us. The client should be smart enough to break necessary oplocks
          * on the source and target files before calling rename, so we'll return
          * an error.
          */

         LOG (4, ("%s: File has an outstanding oplock. Client "
            "should remove this oplock and try again.\n", __FUNCTION__));
         status = HGFS_ERROR_PATH_BUSY;
      }
   }

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerRename --
 *
 *    Handle a Rename request.
 *
 *    Simply converts the new and old names to local filenames, calls
 *    platform specific function to rename/move the file, and returns an
 *    appropriate response to the driver.
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
HgfsServerRename(HgfsInputParam *input)  // IN: Input params
{
   char *utf8OldName = NULL;
   size_t utf8OldNameLen;
   char *utf8NewName = NULL;
   size_t utf8NewNameLen;
   char *cpOldName;
   size_t cpOldNameLen;
   char *cpNewName;
   size_t cpNewNameLen;
   HgfsInternalStatus status;
   fileDesc srcFileDesc;
   fileDesc targetFileDesc;
   HgfsHandle srcFile;
   HgfsHandle targetFile;
   HgfsRenameHint hints;
   uint32 oldCaseFlags;
   uint32 newCaseFlags;
   HgfsShareInfo shareInfo;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackRenameRequest(input->payload, input->payloadSize, input->op, &cpOldName,
                               &cpOldNameLen, &cpNewName, &cpNewNameLen,
                               &hints, &srcFile, &targetFile, &oldCaseFlags,
                               &newCaseFlags)) {
      status = HgfsValidateRenameFile((hints & HGFS_RENAME_HINT_USE_SRCFILE_DESC) != 0,
                                      srcFile,
                                      cpOldName,
                                      cpOldNameLen,
                                      oldCaseFlags,
                                      input->session,
                                      &srcFileDesc,
                                      &shareInfo,
                                      &utf8OldName,
                                      &utf8OldNameLen);
      if (HGFS_ERROR_SUCCESS == status) {
         /*
          * Renaming a file requires both read and write permssions for the
          * original file.
          * However the error code must be different depending on the existence
          * of the file with the same name.
          */
         if (!shareInfo.writePermissions || !shareInfo.readPermissions) {
            status = HgfsPlatformFileExists(utf8OldName);
            if (HGFS_ERROR_SUCCESS == status) {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
            LOG(4, ("HgfsServerRename: failed access check, error %d\n", status));
         } else {
            status =
               HgfsValidateRenameFile((hints & HGFS_RENAME_HINT_USE_TARGETFILE_DESC) != 0,
                                      targetFile,
                                      cpNewName,
                                      cpNewNameLen,
                                      newCaseFlags,
                                      input->session,
                                      &targetFileDesc,
                                      &shareInfo,
                                      &utf8NewName,
                                      &utf8NewNameLen);
            if (HGFS_ERROR_SUCCESS == status) {
               /*
                * Renaming a file requires both read and write permssions for
                * the target directory.
                * However the error code must be different depending on the existence
                * of the target directory - if the destination directory exists then
                * ERROR_ACCESS_DENIED should be returned regardless if the destination
                * file exists.
                */
               if (!shareInfo.writePermissions || !shareInfo.readPermissions) {
                  status = HgfsPlatformFileExists(utf8NewName);
                  if (HGFS_ERROR_SUCCESS == status ||
                      HGFS_ERROR_FILE_NOT_FOUND == status) {
                     status = HGFS_ERROR_ACCESS_DENIED;
                  }
                  LOG(4, ("HgfsServerRename: failed access check, error %d\n", status));
               }
            }
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   /* If all pre-conditions are met go ahead with actual rename. */
   if (HGFS_ERROR_SUCCESS == status) {
      status = HgfsPlatformRename(utf8OldName, srcFileDesc, utf8NewName,
         targetFileDesc, hints);
      if (HGFS_ERROR_SUCCESS == status) {
         /* Update all file nodes that refer to this file to contain the new name. */
         HgfsUpdateNodeNames(utf8OldName, utf8NewName, input->session);
         if (!HgfsPackRenameReply(input->packet, input->metaPacket, input->op,
                                  &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   }

   free(utf8OldName);
   free(utf8NewName);

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCreateDir --
 *
 *    Handle a CreateDir request.
 *
 *    Simply converts to the local filename, calls platform specific
 *    code to create a directory, and returns an appropriate response to the driver.
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
HgfsServerCreateDir(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   HgfsCreateDirInfo info;
   char *utf8Name;
   size_t utf8NameLen;
   size_t replyPayloadSize = 0;
   HgfsShareInfo shareInfo;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackCreateDirRequest(input->payload, input->payloadSize,
                                  input->op, &info)) {
      nameStatus = HgfsServerGetShareInfo(info.cpName, info.cpNameSize, info.caseFlags,
                                          &shareInfo, &utf8Name, &utf8NameLen);
      if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
         ASSERT(utf8Name);

         LOG(4, ("%s: making dir \"%s\"", __FUNCTION__, utf8Name));
         /*
          * For read-only shares we must never attempt to create a directory.
          * However the error code must be different depending on the existence
          * of the file with the same name.
          */
         if (shareInfo.writePermissions) {
            status = HgfsPlatformCreateDir(&info, utf8Name);
            if (HGFS_ERROR_SUCCESS == status) {
               if (!HgfsPackCreateDirReply(input->packet, input->metaPacket, info.requestType,
                                           &replyPayloadSize, input->session)) {
                  status = HGFS_ERROR_PROTOCOL;
               }
            }
         } else {
            status = HgfsPlatformFileExists(utf8Name);
            if (HGFS_ERROR_SUCCESS == status) {
               status = HGFS_ERROR_FILE_EXIST;
            } else if (HGFS_ERROR_FILE_NOT_FOUND == status) {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
         }
      } else {
         /*
          * Check if the name does not exist - the share was not found.
          * Then it could one of two things: the share was removed/disabled;
          * or we could be in the root share itself and have a new name.
          * To return the correct error, if we are in the root share,
          * we must check the open mode too - creation of new files/folders
          * should fail access denied, for anything else "not found" is acceptable.
          */
         if (nameStatus == HGFS_NAME_STATUS_DOES_NOT_EXIST) {
             if (HgfsServerIsSharedFolderOnly(info.cpName,
                                              info.cpNameSize)) {
               nameStatus = HGFS_NAME_STATUS_ACCESS_DENIED;
               LOG(4, ("%s: New file creation in share root not allowed\n", __FUNCTION__));
            } else {
               LOG(4, ("%s: Shared folder not found\n", __FUNCTION__));
            }
         } else {
            LOG(4, ("%s: Shared folder access error %u\n", __FUNCTION__, nameStatus));
         }

         status = HgfsPlatformConvertFromNameStatus(nameStatus);
      }

   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDeleteFile --
 *
 *    Handle a Delete File request.
 *
 *    Simply converts to the local filename, calls DeleteFile on the
 *    file or calls the Windows native API Delete, and returns an
 *    appropriate response to the driver.
 *
 *    Enforcing read-only shares restrictions
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
HgfsServerDeleteFile(HgfsInputParam *input)  // IN: Input params
{
   char *cpName;
   size_t cpNameSize;
   HgfsServerLock serverLock = HGFS_LOCK_NONE;
   fileDesc fileDesc;
   HgfsHandle file;
   HgfsDeleteHint hints = 0;
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   uint32 caseFlags;
   size_t replyPayloadSize = 0;
   HgfsShareInfo shareInfo;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackDeleteRequest(input->payload, input->payloadSize, input->op, &cpName,
                               &cpNameSize, &hints, &file, &caseFlags)) {
      if (hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
         status = HgfsPlatformDeleteFileByHandle(file, input->session);
      } else {
         char *utf8Name = NULL;
         size_t utf8NameLen;

         nameStatus = HgfsServerGetShareInfo(cpName, cpNameSize, caseFlags, &shareInfo,
                                             &utf8Name, &utf8NameLen);
         if (nameStatus == HGFS_NAME_STATUS_COMPLETE) {
            /*
             * Deleting a file needs both read and write permssions.
             * However the error code must be different depending on the existence
             * of the file with the same name.
             */
            if (!shareInfo.writePermissions || !shareInfo.readPermissions) {
               status = HgfsPlatformFileExists(utf8Name);
               if (HGFS_ERROR_SUCCESS == status) {
                  status = HGFS_ERROR_ACCESS_DENIED;
               }
               LOG(4, ("HgfsServerDeleteFile: failed access check, error %d\n", status));
            } else if (HgfsFileHasServerLock(utf8Name, input->session, &serverLock,
                       &fileDesc)) {
               /*
                * XXX: If the file has an oplock, the client should have broken it on
                * its own by now. Sorry!
                */
               LOG (4, ("%s: File has an outstanding oplock. Client should "
                  "remove this oplock and try again.\n", __FUNCTION__));
               status = HGFS_ERROR_PATH_BUSY;
            } else {
               LOG(4, ("%s: deleting \"%s\"\n", __FUNCTION__, utf8Name));
               status = HgfsPlatformDeleteFileByName(utf8Name);
            }
            free(utf8Name);
         } else {
            LOG(4, ("%s: Shared folder does not exist.\n", __FUNCTION__));
            status = HgfsPlatformConvertFromNameStatus(nameStatus);
         }
      }
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackDeleteReply(input->packet, input->metaPacket, input->op,
                                  &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDeleteDir --
 *
 *    Handle a Delete Dir request.
 *
 *    Simply converts to the local filename, calls RemoveDirectory on the
 *    directory or Windows native API delete if we have a valid handle,
 *    and returns an appropriate response to the driver.
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
HgfsServerDeleteDir(HgfsInputParam *input)  // IN: Input params
{
   char *cpName;
   size_t cpNameSize;
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   HgfsHandle file;
   HgfsDeleteHint hints = 0;
   fileDesc fileDesc;
   Bool sharedFolderOpen = FALSE;
   uint32 caseFlags;
   size_t replyPayloadSize = 0;
   HgfsShareInfo shareInfo;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackDeleteRequest(input->payload, input->payloadSize, input->op, &cpName,
                               &cpNameSize, &hints, &file, &caseFlags)) {
      if (hints & HGFS_DELETE_HINT_USE_FILE_DESC) {

         status = HgfsPlatformGetFd(file, input->session, FALSE, &fileDesc);

         if (HGFS_ERROR_SUCCESS == status) {
            if (HgfsHandleIsSharedFolderOpen(file, input->session, &sharedFolderOpen) &&
               sharedFolderOpen) {
               LOG(4, ("%s: Cannot delete shared folder\n", __FUNCTION__));
               status = HGFS_ERROR_ACCESS_DENIED;
            } else {
               status = HgfsPlatformDeleteDirByHandle(file, input->session);
               if (HGFS_ERROR_SUCCESS != status) {
                  LOG(4, ("%s: error deleting directory %d: %d\n", __FUNCTION__,
                     file, status));
               }
            }
         } else {
            LOG(4, ("%s: could not map cached handle %u, error %u\n",
               __FUNCTION__, file, status));
         }
      } else {
         char *utf8Name = NULL;
         size_t utf8NameLen;

         nameStatus = HgfsServerGetShareInfo(cpName, cpNameSize, caseFlags, &shareInfo,
                                             &utf8Name, &utf8NameLen);
         if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
            ASSERT(utf8Name);
            /* Guest OS is not allowed to delete shared folder. */
            if (HgfsServerIsSharedFolderOnly(cpName, cpNameSize)){
               LOG(4, ("%s: Cannot delete shared folder\n", __FUNCTION__));
               status = HGFS_ERROR_ACCESS_DENIED;
            } else if (!shareInfo.writePermissions || !shareInfo.readPermissions) {
               /*
                * Deleting a directory requires both read and write permissions.
                * However the error code must be different depending on the existence
                * of the file with the same name.
                */
               status = HgfsPlatformFileExists(utf8Name);
               if (HGFS_ERROR_SUCCESS == status) {
                  status = HGFS_ERROR_ACCESS_DENIED;
               }
               LOG(4, ("HgfsServerDeleteDir: failed access check, error %d\n", status));
            } else {
               LOG(4, ("%s: removing \"%s\"\n", __FUNCTION__, utf8Name));
               status = HgfsPlatformDeleteDirByName(utf8Name);
            }
            free(utf8Name);
         } else {
            LOG(4, ("%s: access check failed\n", __FUNCTION__));
            status = HgfsPlatformConvertFromNameStatus(nameStatus);
         }
      }
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackDeleteReply(input->packet, input->metaPacket, input->op,
                                  &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerServerLockChange --
 *
 *    Called by the client when it wants to either acquire an oplock on a file
 *    that was previously opened, or when it wants to release/downgrade an
 *    oplock on a file that was previously oplocked.
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
HgfsServerServerLockChange(HgfsInputParam *input)  // IN: Input params
{

   HGFS_ASSERT_INPUT(input);

   HgfsServerCompleteRequest(HGFS_ERROR_NOT_SUPPORTED, 0, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerWriteWin32Stream --
 *
 *    Handle a write request in the WIN32_STREAM_ID format.
 *
 * Results:
 *    ERROR_SUCCESS or an appropriate Win32 error code.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerWriteWin32Stream(HgfsInputParam *input)  // IN: Input params
{
   uint32  actualSize;
   HgfsInternalStatus status;
   HgfsHandle file;
   char *dataToWrite;
   Bool doSecurity;
   size_t replyPayloadSize = 0;
   size_t requiredSize;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackWriteWin32StreamRequest(input->payload, input->payloadSize, input->op, &file,
                                         &dataToWrite, &requiredSize, &doSecurity)) {
      status = HgfsPlatformWriteWin32Stream(file, dataToWrite, (uint32)requiredSize,
                                            doSecurity, &actualSize, input->session);
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackWriteWin32StreamReply(input->packet, input->metaPacket, input->op,
                                            actualSize, &replyPayloadSize,
                                            input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetDirWatchByHandle --
 *
 *    Sets directory notification watch request using directory handle.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsServerSetDirWatchByHandle(HgfsInputParam *input,         // IN: Input params
                              HgfsHandle dir,                // IN: directory handle
                              uint32 events,                 // IN: event types to report
                              Bool watchTree,                // IN: recursive watch
                              HgfsSubscriberHandle *watchId) // OUT: watch id
{
   HgfsInternalStatus status;
   char *fileName = NULL;
   size_t fileNameSize;
   HgfsSharedFolderHandle sharedFolder = HGFS_INVALID_FOLDER_HANDLE;

   LOG(8, ("%s: entered\n", __FUNCTION__));

   ASSERT(watchId != NULL);

   if (HgfsHandle2NotifyInfo(dir, input->session, &fileName, &fileNameSize,
                             &sharedFolder)) {
      LOG(4, ("%s: adding a subscriber on shared folder handle %#x\n", __FUNCTION__,
               sharedFolder));
      *watchId = HgfsNotify_AddSubscriber(sharedFolder, fileName, events, watchTree,
                                          HgfsServerDirWatchEvent, input->session);
      status = (HGFS_INVALID_SUBSCRIBER_HANDLE == *watchId) ? HGFS_ERROR_INTERNAL :
                                                              HGFS_ERROR_SUCCESS;
      LOG(4, ("%s: result of add subscriber id %"FMT64"x status %u\n", __FUNCTION__,
               *watchId, status));
   } else {
      status = HGFS_ERROR_INTERNAL;
   }
   free(fileName);
   LOG(8, ("%s: exit %u\n", __FUNCTION__, status));
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetDirWatchByName --
 *
 *    Sets directory notification watch request using directory name.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsServerSetDirWatchByName(HgfsInputParam *input,         // IN: Input params
                            char *cpName,                  // IN: directory name
                            uint32 cpNameSize,             // IN: directory name length
                            uint32 caseFlags,              // IN: case flags
                            uint32 events,                 // IN: event types to report
                            Bool watchTree,                // IN: recursive watch
                            HgfsSubscriberHandle *watchId) // OUT: watch id
{
   HgfsInternalStatus status;
   HgfsNameStatus nameStatus;
   char *utf8Name = NULL;
   size_t utf8NameLen;
   HgfsShareInfo shareInfo;
   HgfsSharedFolderHandle sharedFolder = HGFS_INVALID_FOLDER_HANDLE;

   ASSERT(cpName != NULL);
   ASSERT(watchId != NULL);

   LOG(8, ("%s: entered\n",__FUNCTION__));

   nameStatus = HgfsServerGetShareInfo(cpName, cpNameSize, caseFlags, &shareInfo,
                                       &utf8Name, &utf8NameLen);
   if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
      char const *inEnd = cpName + cpNameSize;
      char const *next;
      int len;

      ASSERT(utf8Name);
      /*
       * Get first component.
       */
      len = CPName_GetComponent(cpName, inEnd, (char const **) &next);
      if (len < 0) {
         LOG(4, ("%s: get first component failed\n", __FUNCTION__));
         nameStatus = HGFS_NAME_STATUS_FAILURE;
      } else if (0 == len) {
         /* See if we are dealing with the base of the namespace */
         nameStatus = HGFS_NAME_STATUS_INCOMPLETE_BASE;
      } else {
         sharedFolder = HgfsServerGetShareHandle(cpName);
      }

      if (HGFS_NAME_STATUS_COMPLETE == nameStatus &&
          HGFS_INVALID_FOLDER_HANDLE != sharedFolder) {
         if (cpNameSize > len + 1) {
            size_t nameSize = cpNameSize - len - 1;
            char tempBuf[HGFS_PATH_MAX];
            char *tempPtr = tempBuf;
            size_t tempSize = sizeof tempBuf;

            nameStatus = CPName_ConvertFrom((char const **) &next, &nameSize,
                                            &tempSize, &tempPtr);
            if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
               LOG(8, ("%s: adding subscriber on share hnd %#x\n", __FUNCTION__, sharedFolder));
               *watchId = HgfsNotify_AddSubscriber(sharedFolder, tempBuf, events,
                                                   watchTree, HgfsServerDirWatchEvent,
                                                   input->session);
                status = (HGFS_INVALID_SUBSCRIBER_HANDLE == *watchId) ?
                                              HGFS_ERROR_INTERNAL : HGFS_ERROR_SUCCESS;
               LOG(8, ("%s: adding subscriber on share hnd %#x result %u\n", __FUNCTION__,
                       sharedFolder, status));
            } else {
               LOG(4, ("%s: Conversion to platform specific name failed\n",
                       __FUNCTION__));
               status = HgfsPlatformConvertFromNameStatus(nameStatus);
            }
         } else {
            LOG(8, ("%s: adding subscriber on share hnd %#x\n", __FUNCTION__, sharedFolder));
            *watchId = HgfsNotify_AddSubscriber(sharedFolder, "", events, watchTree,
                                                HgfsServerDirWatchEvent,
                                                input->session);
            status = (HGFS_INVALID_SUBSCRIBER_HANDLE == *watchId) ? HGFS_ERROR_INTERNAL :
                                                                    HGFS_ERROR_SUCCESS;
            LOG(8, ("%s: adding subscriber on share hnd %#x result %u\n", __FUNCTION__,
                     sharedFolder, status));
         }
      } else if (HGFS_NAME_STATUS_INCOMPLETE_BASE == nameStatus) {
         LOG(4, ("%s: Notification for root share is not supported yet\n",
                 __FUNCTION__));
         status = HGFS_ERROR_INVALID_PARAMETER;
      } else {
         LOG(4, ("%s: file not found.\n", __FUNCTION__));
         status = HgfsPlatformConvertFromNameStatus(nameStatus);
      }
   } else {
      LOG(4, ("%s: file not found.\n", __FUNCTION__));
      status = HgfsPlatformConvertFromNameStatus(nameStatus);
   }
   free(utf8Name);
   LOG(8, ("%s: exit %u\n",__FUNCTION__, status));
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetDirNotifyWatch --
 *
 *    Handle a set directory notification watch request.
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
HgfsServerSetDirNotifyWatch(HgfsInputParam *input)  // IN: Input params
{
   char *cpName;
   size_t cpNameSize;
   HgfsInternalStatus status;
   HgfsHandle dir;
   uint32 caseFlags;
   size_t replyPayloadSize = 0;
   uint32 flags;
   uint32 events;
   HgfsSubscriberHandle watchId = HGFS_INVALID_SUBSCRIBER_HANDLE;
   Bool useHandle;

   HGFS_ASSERT_INPUT(input);

   LOG(8, ("%s: entered\n", __FUNCTION__));

   /*
    * If the active session does not support directory change notification - bail out
    * with an error immediately. Otherwise setting watch may succeed but no notification
    * will be delivered when a change occurs.
    */
   if (!input->session->activeNotification) {
      HgfsServerCompleteRequest(HGFS_ERROR_PROTOCOL, 0, input);
      return;
   }

   if (HgfsUnpackSetWatchRequest(input->payload, input->payloadSize, input->op,
                                 &useHandle, &cpName, &cpNameSize, &flags, &events,
                                 &dir, &caseFlags)) {
      Bool watchTree = 0 != (flags & HGFS_NOTIFY_FLAG_WATCH_TREE);
      if (useHandle) {
         status = HgfsServerSetDirWatchByHandle(input, dir, events, watchTree, &watchId);
      } else {
         status = HgfsServerSetDirWatchByName(input, cpName, cpNameSize, caseFlags,
                                              events, watchTree, &watchId);
      }
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackSetWatchReply(input->packet, input->metaPacket, input->op,
                                    watchId, &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
   LOG(8, ("%s: exit %u\n", __FUNCTION__, status));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerRemoveDirNotifyWatch --
 *
 *    Handle a remove directory notification watch request.
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
HgfsServerRemoveDirNotifyWatch(HgfsInputParam *input)  // IN: Input params
{
   HgfsSubscriberHandle watchId;
   HgfsInternalStatus status;
   size_t replyPayloadSize = 0;

   LOG(8, ("%s: entered\n", __FUNCTION__));
   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackRemoveWatchRequest(input->payload, input->payloadSize, input->op,
                                    &watchId)) {
      LOG(8, ("%s: remove subscriber on subscr id %"FMT64"x\n", __FUNCTION__, watchId));
      if (HgfsNotify_RemoveSubscriber(watchId)) {
         status = HGFS_ERROR_SUCCESS;
      } else {
         status = HGFS_ERROR_INTERNAL;
      }
      LOG(8, ("%s: remove subscriber on subscr id %"FMT64"x result %u\n", __FUNCTION__,
               watchId, status));
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }
   if (HGFS_ERROR_SUCCESS == status) {
      if (!HgfsPackRemoveWatchReply(input->packet, input->metaPacket, input->op,
         &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
      }
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
   LOG(8, ("%s: exit result %u\n", __FUNCTION__, status));
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetattr --
 *
 *    Handle a Getattr request.
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
HgfsServerGetattr(HgfsInputParam *input)  // IN: Input params
{
   char *localName;
   HgfsAttrHint hints = 0;
   HgfsFileAttrInfo attr;
   HgfsInternalStatus status = 0;
   HgfsNameStatus nameStatus;
   char *cpName;
   size_t cpNameSize;
   char *targetName = NULL;
   uint32 targetNameLen = 0;
   HgfsHandle file; /* file handle from driver */
   uint32 caseFlags = 0;
   HgfsShareOptions configOptions;
   size_t localNameLen;
   HgfsShareInfo shareInfo;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackGetattrRequest(input->payload, input->payloadSize, input->op, &attr,
                                &hints, &cpName, &cpNameSize, &file, &caseFlags)) {
      /* Client wants us to reuse an existing handle. */
      if (hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
         fileDesc fd;

         targetNameLen = 0;
         status = HgfsPlatformGetFd(file, input->session, FALSE, &fd);
         if (HGFS_ERROR_SUCCESS == status) {
            status = HgfsPlatformGetattrFromFd(fd, input->session, &attr);
         } else {
            LOG(4, ("%s: Could not get file descriptor\n", __FUNCTION__));
         }

      } else {
         /*
          * Depending on whether this file/dir is real or virtual, either
          * forge its attributes or look them up in the actual filesystem.
          */
         nameStatus = HgfsServerGetShareInfo(cpName, cpNameSize, caseFlags, &shareInfo,
                                             &localName, &localNameLen);
         switch (nameStatus) {
         case HGFS_NAME_STATUS_INCOMPLETE_BASE:
            /*
             * This is the base of our namespace; make up fake status for
             * this directory.
             */

            LOG(4, ("%s: getting attrs for base dir\n", __FUNCTION__));
            HgfsPlatformGetDefaultDirAttrs(&attr);
            break;

         case HGFS_NAME_STATUS_COMPLETE:
            /* This is a regular lookup; proceed as usual */
            ASSERT(localName);

            /* Get the config options. */
            nameStatus = HgfsServerPolicy_GetShareOptions(cpName, cpNameSize,
                                                          &configOptions);
            if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
               status = HgfsPlatformGetattrFromName(localName, configOptions, cpName, &attr,
                                                    &targetName);
            } else {
               LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpName));
               status = HGFS_ERROR_FILE_NOT_FOUND;
            }
            free(localName);

            if (HGFS_ERROR_SUCCESS == status &&
                !HgfsServerPolicy_CheckMode(HGFS_OPEN_MODE_READ_ONLY,
                                            shareInfo.writePermissions,
                                            shareInfo.readPermissions)) {
               status = HGFS_ERROR_ACCESS_DENIED;
            } else if (status != HGFS_ERROR_SUCCESS) {
               /*
                * If it is a dangling share server should not return
                * HGFS_ERROR_FILE_NOT_FOUND
                * to the client because it causes confusion: a name that is returned
                * by directory enumeration should not produce "name not found"
                * error.
                * Replace it with a more appropriate error code: no such device.
                */
               if (status == HGFS_ERROR_FILE_NOT_FOUND &&
                   HgfsIsShareRoot(cpName, cpNameSize)) {
                  status = HGFS_ERROR_IO;
               }
            }
            break;

         default:
            status = HgfsPlatformHandleIncompleteName(nameStatus, &attr);
         }
         targetNameLen = targetName ? strlen(targetName) : 0;

      }
      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackGetattrReply(input->packet, input->metaPacket, &attr, targetName,
                                   targetNameLen, &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   free(targetName);

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSetattr --
 *
 *    Handle a Setattr request.
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
HgfsServerSetattr(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;
   HgfsNameStatus nameStatus;
   HgfsFileAttrInfo attr;
   char *cpName;
   size_t cpNameSize = 0;
   HgfsAttrHint hints = 0;
   HgfsOpenMode shareMode;
   uint32 caseFlags = 0;
   HgfsShareInfo shareInfo;
   HgfsHandle file;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackSetattrRequest(input->payload, input->payloadSize, input->op, &attr,
                                &hints, &cpName, &cpNameSize, &file, &caseFlags)) {
      /* Client wants us to reuse an existing handle. */
      if (hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
         if (HgfsHandle2ShareMode(file, input->session, &shareMode)) {
            if (HGFS_OPEN_MODE_READ_ONLY != shareMode) {
               status = HgfsPlatformSetattrFromFd(file, input->session, &attr, hints);
            } else {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
         } else {
            LOG(4, ("%s: could not get share mode fd %d\n", __FUNCTION__,
                file));
            status = HGFS_ERROR_INVALID_HANDLE;
         }
      } else { /* Client wants us to open a new handle for this operation. */
         char *utf8Name = NULL;
         size_t utf8NameLen;

         nameStatus = HgfsServerGetShareInfo(cpName, cpNameSize, caseFlags, &shareInfo,
                                             &utf8Name, &utf8NameLen);
         if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
            fileDesc hFile;
            HgfsServerLock serverLock = HGFS_LOCK_NONE;
            HgfsShareOptions configOptions;

            /*
             * XXX: If the client has an oplock on this file, it must reuse the
             * handle for the oplocked node (or break the oplock) prior to making
             * a setattr request. Fail this request.
             */
            if (!HgfsServerPolicy_CheckMode(HGFS_OPEN_MODE_WRITE_ONLY,
                                            shareInfo.writePermissions,
                                            shareInfo.readPermissions)) {
               status = HGFS_ERROR_ACCESS_DENIED;
            } else if (HGFS_NAME_STATUS_COMPLETE !=
                       HgfsServerPolicy_GetShareOptions(cpName, cpNameSize,
                       &configOptions)) {
               LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, cpName));
               status = HGFS_ERROR_FILE_NOT_FOUND;
            } else if (HgfsFileHasServerLock(utf8Name, input->session, &serverLock, &hFile)) {
               LOG(4, ("%s: An open, oplocked handle exists for "
                      "this file. The client should retry with that handle\n",
                      __FUNCTION__));
               status = HGFS_ERROR_PATH_BUSY;
            } else {
               status = HgfsPlatformSetattrFromName(utf8Name, &attr, configOptions, hints);
            }
            free(utf8Name);
         } else {
            LOG(4, ("%s: file not found.\n", __FUNCTION__));
            status = HgfsPlatformConvertFromNameStatus(nameStatus);
         }
      }

      if (HGFS_ERROR_SUCCESS == status) {
         if (!HgfsPackSetattrReply(input->packet, input->metaPacket, attr.requestType,
                                   &replyPayloadSize, input->session)) {
            status = HGFS_ERROR_INTERNAL;
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerValidateOpenParameters --
 *
 *    Performs sanity check of the input parameters.
 *
 * Results:
 *    HGFS_ERROR_SUCCESS if the parameters are valid.
 *    Appropriate error code otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsServerValidateOpenParameters(HgfsFileOpenInfo *openInfo, // IN/OUT: openfile info
                                 Bool *denyCreatingFile,     // OUT: No new files
                                 int *followSymlinks)        // OUT: Host resolves link
{
   size_t utf8NameLen;
   HgfsInternalStatus status;

   *followSymlinks = 0;
   *denyCreatingFile = FALSE;

   if ((openInfo->mask & HGFS_OPEN_VALID_MODE)) {
      HgfsNameStatus nameStatus;
      /* It is now safe to read the file name. */
      nameStatus = HgfsServerGetShareInfo(openInfo->cpName,
                                          openInfo->cpNameSize,
                                          openInfo->caseFlags,
                                          &openInfo->shareInfo,
                                          &openInfo->utf8Name,
                                          &utf8NameLen);
      if (HGFS_NAME_STATUS_COMPLETE == nameStatus) {
         if (openInfo->mask & HGFS_OPEN_VALID_FLAGS) {
            HgfsOpenFlags savedOpenFlags = openInfo->flags;

            if (HgfsServerCheckOpenFlagsForShare(openInfo, &openInfo->flags)) {
               HgfsShareOptions configOptions;

               /* Get the config options. */
               nameStatus = HgfsServerPolicy_GetShareOptions(openInfo->cpName,
                                                             openInfo->cpNameSize,
                                                             &configOptions);
               if (nameStatus == HGFS_NAME_STATUS_COMPLETE) {
                  *followSymlinks =
                     HgfsServerPolicy_IsShareOptionSet(configOptions,
                                                       HGFS_SHARE_FOLLOW_SYMLINKS);
                  *denyCreatingFile = savedOpenFlags != openInfo->flags;
                  status = HGFS_ERROR_SUCCESS;
               } else {
                  LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, openInfo->cpName));
                  *denyCreatingFile = TRUE;
                  status = HGFS_ERROR_FILE_NOT_FOUND;
               }
            } else {
               /* Incompatible open mode with share mode. */
               status = HGFS_STATUS_ACCESS_DENIED;
            }
         } else {
            status = HGFS_ERROR_PROTOCOL;
         }
      } else {
         /*
          * Check if the name does not exist - the share was not found.
          * Then it could one of two things: the share was removed/disabled;
          * or we could be in the root share itself and have a new name.
          * To return the correct error, if we are in the root share,
          * we must check the open mode too - creation of new files/folders
          * should fail access denied, for anything else "not found" is acceptable.
          */
         if (nameStatus == HGFS_NAME_STATUS_DOES_NOT_EXIST) {
            if ((openInfo->mask & HGFS_OPEN_VALID_FLAGS &&
                 (openInfo->flags == HGFS_OPEN_CREATE ||
                  openInfo->flags == HGFS_OPEN_CREATE_SAFE ||
                  openInfo->flags == HGFS_OPEN_CREATE_EMPTY)) &&
                HgfsServerIsSharedFolderOnly(openInfo->cpName,
                                             openInfo->cpNameSize)) {
               nameStatus = HGFS_NAME_STATUS_ACCESS_DENIED;
               LOG(4, ("%s: New file creation in share root not allowed\n", __FUNCTION__));
            } else {
               LOG(4, ("%s: Shared folder not found\n", __FUNCTION__));
            }
         } else {
            LOG(4, ("%s: Shared folder access error %u\n", __FUNCTION__, nameStatus));
         }
         status = HgfsPlatformConvertFromNameStatus(nameStatus);
      }
   } else {
      LOG(4, ("%s: filename or mode not provided\n", __FUNCTION__));
      status = HGFS_ERROR_PROTOCOL;
   }
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOpen --
 *
 *    Handle an Open request.
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
HgfsServerOpen(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status;
   fileDesc newHandle;
   HgfsLocalId localId;
   HgfsFileOpenInfo openInfo;
   fileDesc fileDesc;
   HgfsServerLock serverLock = HGFS_LOCK_NONE;
   size_t replyPayloadSize = 0;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackOpenRequest(input->payload, input->payloadSize, input->op, &openInfo)) {
      int followSymlinks;
      Bool denyCreatingFile;

      status = HgfsServerValidateOpenParameters(&openInfo, &denyCreatingFile,
                                                &followSymlinks);
      if (HGFS_ERROR_SUCCESS == status) {
         ASSERT(openInfo.utf8Name);
         LOG(4, ("%s: opening \"%s\", mode %u, flags %u, perms %u%u%u%u attr %u\n",
             __FUNCTION__, openInfo.utf8Name, openInfo.mode,
             openInfo.mask & HGFS_OPEN_VALID_FLAGS       ? openInfo.flags      : 0,
             (openInfo.mask & HGFS_OPEN_VALID_SPECIAL_PERMS) ?
             openInfo.specialPerms : 0,
             (openInfo.mask & HGFS_OPEN_VALID_OWNER_PERMS) ?
             openInfo.ownerPerms : 0,
             (openInfo.mask & HGFS_OPEN_VALID_GROUP_PERMS) ?
             openInfo.groupPerms : 0,
             (openInfo.mask & HGFS_OPEN_VALID_OTHER_PERMS) ?
             openInfo.otherPerms : 0,
             openInfo.mask & HGFS_OPEN_VALID_FILE_ATTR   ? (uint32)openInfo.attr : 0));
         /*
          * XXX: Before opening the file, see if we already have this file opened on
          * the server with an oplock on it. If we do, we must fail the new open
          * request, otherwise we will trigger an oplock break that the guest cannot
          * handle at this time (since the HGFS server is running in the context of
          * the vcpu thread), and we'll deadlock.
          *
          * Until we overcome this limitation via Crosstalk, we will be extra smart
          * in the client drivers so as to prevent open requests on handles that
          * already have an oplock. And the server will protect itself like so.
          *
          * XXX: With some extra effort, we could allow a second open for read here,
          * since that won't break a shared oplock, but the clients should already
          * realize that the second open can be avoided via sharing handles, too.
          */
         if (!HgfsFileHasServerLock(openInfo.utf8Name, input->session, &serverLock,
                                    &fileDesc)) {
            /* See if the name is valid, and if so add it and return the handle. */
            status = HgfsPlatformValidateOpen(&openInfo, followSymlinks, input->session,
                                              &localId, &newHandle);
            if (status == HGFS_ERROR_SUCCESS) {
               ASSERT(newHandle >= 0);

               /*
                * Open succeeded, so make new node and return its handle. If we fail,
                * it's almost certainly an internal server error.
                */

               if (HgfsCreateAndCacheFileNode(&openInfo, &localId, newHandle,
                                              FALSE, input->session)) {
                  if (!HgfsPackOpenReply(input->packet, input->metaPacket, &openInfo,
                                         &replyPayloadSize, input->session)) {
                     status = HGFS_ERROR_INTERNAL;
                  }
               }
            } else if (denyCreatingFile && HGFS_ERROR_FILE_NOT_FOUND == status) {
               status = HGFS_ERROR_ACCESS_DENIED;
            }
         } else {
            status = HGFS_ERROR_PATH_BUSY;
         }
         free(openInfo.utf8Name);
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchReadAttrToMask --
 *
 *    Sets a search read information mask from the retrieved attribute
 *    information.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSearchReadAttrToMask(HgfsFileAttrInfo *attr,     // IN/OUT: attributes for entry
                               HgfsSearchReadMask *mask)   // IN/OUT: what info is required/returned
{
   if (0 != (attr->mask & HGFS_ATTR_VALID_TYPE)) {
      *mask |= (HGFS_SEARCH_READ_FILE_NODE_TYPE);
   }
   if (0 != (attr->mask & HGFS_ATTR_VALID_SIZE)) {
      *mask |= (HGFS_SEARCH_READ_FILE_SIZE);
   }
   if (0 != (attr->mask & HGFS_ATTR_VALID_ALLOCATION_SIZE)) {
      *mask |= (HGFS_SEARCH_READ_ALLOCATION_SIZE);
   }
   if (0 != (attr->mask & (HGFS_ATTR_VALID_CREATE_TIME |
                           HGFS_ATTR_VALID_ACCESS_TIME |
                           HGFS_ATTR_VALID_WRITE_TIME |
                           HGFS_ATTR_VALID_CHANGE_TIME))) {
      *mask |= (HGFS_SEARCH_READ_TIME_STAMP);
   }
   if (0 != (attr->mask & (HGFS_ATTR_VALID_FLAGS |
                           HGFS_ATTR_VALID_OWNER_PERMS |
                           HGFS_ATTR_VALID_GROUP_PERMS |
                           HGFS_ATTR_VALID_OTHER_PERMS))) {
      Bool isReadOnly = TRUE;

      *mask |= (HGFS_SEARCH_READ_FILE_ATTRIBUTES);
      /*
       * For V4 we don't return the permissions as they are really not
       * used. Only used to see if the entry is read only. So set the
       * attribute flag if the entry is read only.
       */
      if (attr->mask & HGFS_ATTR_VALID_OWNER_PERMS &&
          attr->ownerPerms & HGFS_PERM_WRITE) {
          isReadOnly = FALSE;
      }
      if (attr->mask & HGFS_ATTR_VALID_GROUP_PERMS &&
          attr->groupPerms & HGFS_PERM_WRITE) {
          isReadOnly = FALSE;
      }
      if (attr->mask & HGFS_ATTR_VALID_OTHER_PERMS &&
          attr->otherPerms & HGFS_PERM_WRITE) {
          isReadOnly = FALSE;
      }
      if (isReadOnly) {
         attr->flags |= HGFS_ATTR_READONLY;
         attr->mask |= HGFS_ATTR_VALID_FLAGS;
      }
   }
   if (0 != (attr->mask & (HGFS_ATTR_VALID_FILEID |
                           HGFS_ATTR_VALID_NON_STATIC_FILEID))) {
      *mask |= (HGFS_SEARCH_READ_FILE_ID);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsGetDirEntry --
 *
 *    Gets a directory entry at specified offset.
 *
 * Results:
 *    A platform specific error or success.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsGetDirEntry(HgfsHandle hgfsSearchHandle,     // IN: ID for search data
                HgfsSearch *search,              // IN: search data
                HgfsShareOptions configOptions,  // IN: share configuration settings
                HgfsSessionInfo *session,        // IN: session we are called in
                HgfsSearchReadInfo *info,        // IN/OUT: request details
                HgfsSearchReadEntry *entry,      // OUT: directory entry
                Bool *moreEntries)               // OUT: any more entries
{
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;
   DirectoryEntry *dent;
   HgfsSearchReadMask infoRetrieved;
   HgfsSearchReadMask infoRequested;
   HgfsFileAttrInfo *attr;
   char **entryName;
   uint32 *nameLength;
   Bool getAttrs;
   Bool unescapeName = TRUE;
   uint32 requestedIndex;

   infoRequested = info->requestedMask;

   attr = &entry->attr;
   entryName = &entry->name;
   nameLength = &entry->nameLength;

   requestedIndex = info->currentIndex;

   getAttrs = (0 != (infoRequested & (HGFS_SEARCH_READ_FILE_SIZE |
                                      HGFS_SEARCH_READ_ALLOCATION_SIZE |
                                      HGFS_SEARCH_READ_TIME_STAMP |
                                      HGFS_SEARCH_READ_FILE_ATTRIBUTES |
                                      HGFS_SEARCH_READ_FILE_ID |
                                      HGFS_SEARCH_READ_FILE_NODE_TYPE)));

   /* Clear out what we will return. */
   infoRetrieved = 0;
   memset(attr, 0, sizeof *attr);

   dent = HgfsGetSearchResult(hgfsSearchHandle, session, requestedIndex, FALSE);
   if (dent) {
      unsigned int length;
      char *fullName;
      char *sharePath;
      size_t sharePathLen;
      size_t fullNameLen;
      HgfsServerLock serverLock = HGFS_LOCK_NONE;
      fileDesc fileDesc;

      length = strlen(dent->d_name);

      /* Each type of search gets a dent's attributes in a different way. */
      switch (search->type) {
      case DIRECTORY_SEARCH_TYPE_DIR:

         /*
          * Construct the UTF8 version of the full path to the file, and call
          * HgfsGetattrFromName to get the attributes of the file.
          */
         fullNameLen = search->utf8DirLen + 1 + length;
         fullName = (char *)malloc(fullNameLen + 1);
         if (fullName) {
            memcpy(fullName, search->utf8Dir, search->utf8DirLen);
            fullName[search->utf8DirLen] = DIRSEPC;
            memcpy(&fullName[search->utf8DirLen + 1], dent->d_name, length + 1);

            LOG(4, ("%s: about to stat \"%s\"\n", __FUNCTION__, fullName));

            /* Do we need to query the attributes information? */
            if (getAttrs) {
               /*
                * XXX: It is unreasonable to make the caller either 1) pass existing
                * handles for directory objects as part of the SearchRead, or 2)
                * prior to calling SearchRead on a directory, break all oplocks on
                * that directory's objects.
                *
                * To compensate for that, if we detect that this directory object
                * has an oplock, we'll quietly reuse the handle. Note that this
                * requires clients who take out an exclusive oplock to open a
                * handle with read as well as write access, otherwise we'll fail
                * further down in HgfsStat.
                *
                * XXX: We could open a new handle safely if its a shared oplock.
                * But isn't this handle sharing always desirable?
                */
               if (HgfsFileHasServerLock(fullName, session, &serverLock, &fileDesc)) {
                  LOG(4, ("%s: Reusing existing oplocked handle "
                          "to avoid oplock break deadlock\n", __FUNCTION__));
                  status = HgfsPlatformGetattrFromFd(fileDesc, session, attr);
               } else {
                  status = HgfsPlatformGetattrFromName(fullName, configOptions,
                                                       search->utf8ShareName, attr, NULL);
               }

               if (HGFS_ERROR_SUCCESS != status) {
                  HgfsOp savedOp = attr->requestType;
                  LOG(4, ("%s: stat FAILED %s (%d)\n", __FUNCTION__, fullName, status));
                  memset(attr, 0, sizeof *attr);
                  attr->requestType = savedOp;
                  attr->type = HGFS_FILE_TYPE_REGULAR;
                  attr->mask = HGFS_ATTR_VALID_TYPE;
                  status = HGFS_ERROR_SUCCESS;
               }
            }
            /*
             * Update the search read mask for the attributes information.
             */
            HgfsServerSearchReadAttrToMask(attr, &infoRetrieved);

            free(fullName);
         } else {
            LOG(4, ("%s: could not allocate space for \"%s\\%s\"\n",
                    __FUNCTION__, search->utf8Dir, dent->d_name));
            status = HGFS_ERROR_NOT_ENOUGH_MEMORY;
         }
         break;

      case DIRECTORY_SEARCH_TYPE_BASE:

         /*
          * We only want to unescape names that we could have escaped.
          * This cannot apply to our shares since they are created by the user.
          * The client will take care of escaping anything it requires.
          */
         unescapeName = FALSE;
         if (getAttrs) {
            /*
             * For a search enumerating all shares, give the default attributes
             * for '.' and ".." (which aren't really shares anyway). Each real
             * share gets resolved into its full path, and gets its attributes
             * via HgfsGetattrFromName.
             */
            if (strcmp(dent->d_name, ".") == 0 ||
                strcmp(dent->d_name, "..") == 0) {
               LOG(4, ("%s: assigning %s default attributes\n",
                       __FUNCTION__, dent->d_name));
               HgfsPlatformGetDefaultDirAttrs(attr);
            } else {
               HgfsNameStatus nameStatus;

               /* Check permission on the share and get the share path */
               nameStatus =
                  HgfsServerPolicy_GetSharePath(dent->d_name, length,
                                                HGFS_OPEN_MODE_READ_ONLY,
                                                &sharePathLen,
                                                (char const **)&sharePath);
               if (nameStatus == HGFS_NAME_STATUS_COMPLETE) {

                  /*
                   * Server needs to produce list of shares that is consistent with
                   * the list defined in UI. If a share can't be accessed because of
                   * problems on the host, the server still enumerates it and
                   * returns to the client.
                   */
                  /*
                   * XXX: We will open a new handle for this, but it should be safe
                   * from oplock-induced deadlock because these are all directories,
                   * and thus cannot have oplocks placed on them.
                   */
                  status = HgfsPlatformGetattrFromName(sharePath, configOptions,
                                                       dent->d_name, attr, NULL);

                  /*
                   * For some reason, Windows marks drives as hidden and system. So
                   * if one of the top level shared folders is mapped to a drive
                   * letter (like C:\), then GetFileAttributesEx() will return hidden
                   * and system attributes for that drive. We don't want that
                   * since we want the users to see all top level shared folders.
                   * Even in the case when the top level shared folder is mapped
                   * to a non-drive hidden/system directory, we still want to display
                   * it to the user. So make sure that system and hidden attributes
                   * are not set.
                   * Note, that for network shares this doesn't apply, since each
                   * top level network share is a separate mount point that doesn't
                   * have such attributes. So when we finally have per share
                   * mounting, this hack will go away.
                   *
                   * See bug 125065.
                   */
                  attr->flags &= ~(HGFS_ATTR_HIDDEN | HGFS_ATTR_SYSTEM);

                  if (HGFS_ERROR_SUCCESS != status) {
                     /*
                      * The dent no longer exists. Log the event.
                      */

                     LOG(4, ("%s: stat FAILED\n", __FUNCTION__));
                     status = HGFS_ERROR_SUCCESS;
                  }
               } else {
                  LOG(4, ("%s: No such share or access denied\n", __FUNCTION__));
                  status = HgfsPlatformConvertFromNameStatus(nameStatus);
               }
            }
            if (HGFS_ERROR_SUCCESS == status) {
               /*
                * Update the search read mask for the attributes information.
                */
               HgfsServerSearchReadAttrToMask(attr, &infoRetrieved);
            }
         }
         break;
      case DIRECTORY_SEARCH_TYPE_OTHER:

         /*
          * The POSIX implementation of HgfsSearchOpen could not have created
          * this kind of search.
          */
#if !defined(_WIN32)
         NOT_IMPLEMENTED();
#endif
         /*
          * We only want to unescape names that we could have escaped.
          * This cannot apply to these entries.
          */
         unescapeName = FALSE;
         if (getAttrs) {
            /*
             * All "other" searches get the default attributes. This includes
             * an enumeration of drive, and the root enumeration (which contains
             * a "drive" dent and a "unc" dent).
             */
            HgfsPlatformGetDefaultDirAttrs(attr);
            /*
             * Update the search read mask for the attributes information.
             */
            HgfsServerSearchReadAttrToMask(attr, &infoRetrieved);
         }
         break;
      default:
         NOT_IMPLEMENTED();
         break;
      }

      /*
       * We need to unescape the name before sending it back to the client
       */
      if (HGFS_ERROR_SUCCESS == status) {
         *entryName = Util_SafeStrdup(dent->d_name);
         if (unescapeName) {
            *nameLength = HgfsEscape_Undo(*entryName, length + 1);
         } else {
            *nameLength = length;
         }
         infoRetrieved |= HGFS_SEARCH_READ_NAME;
         LOG(4, ("%s: dent name is \"%s\" len = %u\n", __FUNCTION__,
                 *entryName, *nameLength));
      } else {
         *entryName = NULL;
         *nameLength = 0;
         LOG(4, ("%s: error %d getting dent\n", __FUNCTION__, status));
      }

      if (HGFS_ERROR_SUCCESS == status) {
         /* Update the entry fields for valid data and index for the dent. */
         entry->fileIndex = requestedIndex;
         entry->mask = infoRetrieved;
         /* Retrieve any platform specific information from the dent. */
      }
      free(dent);
      *moreEntries = TRUE;
   } else {
      /* End of directory entries marker. */
      *entryName = NULL;
      *nameLength = 0;
      *moreEntries = FALSE;
      info->replyFlags |= HGFS_SEARCH_READ_REPLY_FINAL_ENTRY;
   }
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDoSearchRead --
 *
 *    Gets all the directory entries that remain or as many that will
 *    fit into the reply buffer from the specified index. Fill in the
 *    reply with the records and complete the reply details.
 *
 * Results:
 *    A platform specific error or success.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static HgfsInternalStatus
HgfsDoSearchRead(HgfsHandle hgfsSearchHandle,     // IN: ID for search data
                 HgfsSearch *search,              // IN: search data
                 HgfsShareOptions configOptions,  // IN: share configuration settings
                 HgfsSessionInfo *session,        // IN: session we are called in
                 HgfsSearchReadInfo *info,        // IN/OUT: request details
                 size_t *replyHeaderSize,         // OUT: reply info written size
                 size_t *replyDirentSize)         // OUT: reply dirent written size
{
   HgfsSearchReadEntry entry;
   size_t bytesWritten = 0;
   size_t bytesRemaining = 0;
   char *currentSearchReadRecord = NULL;
   char *lastSearchReadRecord = NULL;
   Bool moreEntries = TRUE;
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;

   info->currentIndex = info->startIndex;
   *replyHeaderSize = 0;
   *replyDirentSize = 0;


   while (moreEntries) {
      size_t offsetInBuffer = ROUNDUP(*replyDirentSize, sizeof (uint64));

      if (info->payloadSize <= offsetInBuffer) {
         break;
      }

      memset(&entry, 0, sizeof entry);

      currentSearchReadRecord = (char*)info->replyPayload + offsetInBuffer;
      bytesRemaining = info->payloadSize - offsetInBuffer;
      bytesWritten = 0;

      status = HgfsGetDirEntry(hgfsSearchHandle,
                               search,
                               configOptions,
                               session,
                               info,
                               &entry,
                               &moreEntries);
      if (HGFS_ERROR_SUCCESS != status) {
         /* Failed to retrieve an entry record, bail. */
         break;
      }

      if (!HgfsPackSearchReadReplyRecord(info->requestType,
                                         &entry,
                                         bytesRemaining,
                                         lastSearchReadRecord,
                                         currentSearchReadRecord,
                                         &bytesWritten)) {
         /*
          * The current entry is too large to be contained in the reply.
          * If this is the first entry returned then we have an error.
          * Otherwise, we return success for what is already in the reply.
          */
         if (0 == info->numberRecordsWritten) {
            status = HGFS_ERROR_INTERNAL;
         }
         moreEntries = FALSE;
      }

      if (NULL != entry.name) {
         free(entry.name);
      }

      if (HGFS_ERROR_SUCCESS != status) {
         /* Failed to pack any entry records, bail. */
         break;
      }

      /*
       * Only count records actually written to the reply.
       * (The final, empty record is not written for all protocol versions.)
       */
      if (0 < bytesWritten) {

         if (0 != (info->flags & HGFS_SEARCH_READ_SINGLE_ENTRY)) {
            moreEntries = FALSE;
         }

         *replyDirentSize = ROUNDUP(*replyDirentSize, sizeof (uint64)) + bytesWritten;
         lastSearchReadRecord = currentSearchReadRecord;
         info->currentIndex++;
         info->numberRecordsWritten++;
      }
   }

   /* Now pack the search read reply common reply part. */
   if (HgfsPackSearchReadReplyHeader(info,
                                     &bytesWritten)) {
      /* The search read reply common reply part size was already done so should be 0. */
      *replyHeaderSize = bytesWritten;
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSearchRead --
 *
 *    Handle a "Search Read" request.
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
HgfsServerSearchRead(HgfsInputParam *input)  // IN: Input params
{
   HgfsInternalStatus status = HGFS_ERROR_SUCCESS;
   HgfsNameStatus nameStatus;
   HgfsHandle hgfsSearchHandle;
   HgfsSearch search;
   HgfsShareOptions configOptions = 0;
   size_t replyInfoSize = 0;
   size_t replyDirentSize = 0;
   size_t replyPayloadSize = 0;
   size_t inlineDataSize = 0;
   size_t baseReplySize;
   HgfsSearchReadInfo info;

   HGFS_ASSERT_INPUT(input);

   memset(&info, 0, sizeof info);

   /*
    * For search read V4 we use the whole packet buffer available to pack
    * as many replies as can fit into that size. For all previous versions
    * only one record is going to be returned, so we allow the old packet
    * max for the reply.
    */
   if (HgfsUnpackSearchReadRequest(input->payload, input->payloadSize, input->op,
                                   &info, &baseReplySize, &inlineDataSize,
                                   &hgfsSearchHandle)) {

      LOG(4, ("%s: read search #%u, offset %u\n", __FUNCTION__,
              hgfsSearchHandle, info.startIndex));

      if (!HgfsAllocInitReply(input->packet, input->metaPacket,
                              baseReplySize + inlineDataSize, &info.reply,
                              input->session)) {
         status = HGFS_ERROR_PROTOCOL;
      } else {

         if (inlineDataSize == 0) {
            info.replyPayload = HSPU_GetDataPacketBuf(input->packet, BUF_WRITEABLE,
                                                      input->transportSession);
         } else {
            info.replyPayload = (char *)info.reply + baseReplySize;
         }

         if (info.replyPayload == NULL) {
            LOG(4, ("%s: Op %d reply buffer failure\n", __FUNCTION__, input->op));
            status = HGFS_ERROR_PROTOCOL;
         } else {

            if (HgfsGetSearchCopy(hgfsSearchHandle, input->session, &search)) {
               /* Get the config options. */
               if (search.utf8ShareNameLen != 0) {
                  nameStatus = HgfsServerPolicy_GetShareOptions(search.utf8ShareName,
                                                                search.utf8ShareNameLen,
                                                                &configOptions);
                  if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
                     LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__,
                             search.utf8ShareName));
                     status = HGFS_ERROR_FILE_NOT_FOUND;
                  }
               } else if (0 == info.startIndex) {
                  HgfsSearch *rootSearch;

                  MXUser_AcquireExclLock(input->session->searchArrayLock);

                  rootSearch = HgfsSearchHandle2Search(hgfsSearchHandle, input->session);
                  ASSERT(NULL != rootSearch);
                  HgfsFreeSearchDirents(rootSearch);

                  rootSearch->numDents =
                     HgfsServerGetDents(HgfsServerPolicy_GetShares,
                                        HgfsServerPolicy_GetSharesInit,
                                        HgfsServerPolicy_GetSharesCleanup,
                                        &rootSearch->dents);
                  if (((int) (rootSearch->numDents)) < 0) {
                     ASSERT_DEVEL(0);
                     LOG(4, ("%s: couldn't get root dents\n", __FUNCTION__));
                     rootSearch->numDents = 0;
                     status = HGFS_ERROR_INTERNAL;
                  }

                  MXUser_ReleaseExclLock(input->session->searchArrayLock);
               }

               if (HGFS_ERROR_SUCCESS == status) {
                  status = HgfsDoSearchRead(hgfsSearchHandle,
                                            &search,
                                            configOptions,
                                            input->session,
                                            &info,
                                            &replyInfoSize,
                                            &replyDirentSize);
               }

               if (HGFS_ERROR_SUCCESS == status) {
                  replyPayloadSize = replyInfoSize +
                                     ((inlineDataSize == 0) ? 0 : replyDirentSize);
               }

               free(search.utf8Dir);
               free(search.utf8ShareName);

            } else {
               LOG(4, ("%s: handle %u is invalid\n", __FUNCTION__, hgfsSearchHandle));
               status = HGFS_ERROR_INVALID_HANDLE;
            }
         }
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerCreateSession --
 *
 *    Handle a "Create session" request.
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
HgfsServerCreateSession(HgfsInputParam *input)  // IN: Input params
{
   size_t replyPayloadSize = 0;
   HgfsCreateSessionInfo info;
   HgfsInternalStatus status;

   HGFS_ASSERT_INPUT(input);

   if (HgfsUnpackCreateSessionRequest(input->payload, input->payloadSize,
                                      input->op, &info)) {
      HgfsSessionInfo *session;
      LOG(4, ("%s: create session\n", __FUNCTION__));

      if (!HgfsServerAllocateSession(input->transportSession,
                                     input->transportSession->channelCapabilities,
                                     &session)) {
         status = HGFS_ERROR_NOT_ENOUGH_MEMORY;
         goto abort;
      } else {
         status = HgfsServerTransportAddSessionToList(input->transportSession,
                                                      session);
         if (HGFS_ERROR_SUCCESS != status) {
            LOG(4, ("%s: Could not add session to the list.\n", __FUNCTION__));
            HgfsServerSessionPut(session);
            goto abort;
         }
      }

      if (info.maxPacketSize < session->maxPacketSize) {
         session->maxPacketSize = info.maxPacketSize;
      }
      if (HgfsPackCreateSessionReply(input->packet, input->metaPacket,
                                     &replyPayloadSize, session)) {
         status = HGFS_ERROR_SUCCESS;
      } else {
         status = HGFS_ERROR_INTERNAL;
      }
   } else {
      status = HGFS_ERROR_PROTOCOL;
   }

abort:
   HgfsServerCompleteRequest(status, replyPayloadSize, input);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDestroySession --
 *
 *    Handle a "Destroy session" request.
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
HgfsServerDestroySession(HgfsInputParam *input)  // IN: Input params
{
   HgfsTransportSessionInfo *transportSession;
   HgfsSessionInfo *session;

   HGFS_ASSERT_INPUT(input);

   transportSession = input->transportSession;
   session = input->session;

   session->state = HGFS_SESSION_STATE_CLOSED;

   if (session->sessionId == transportSession->defaultSessionId) {
      transportSession->defaultSessionId = HGFS_INVALID_SESSION_ID;
   }

   /*
    * Remove the session from the list. By doing that, the refcount of
    * the session will be decremented. Later, we will be invoking
    * HgfsServerCompleteRequest which will decrement the session's
    * refcount and cleanup the session
    */
   MXUser_AcquireExclLock(transportSession->sessionArrayLock);
   HgfsServerTransportRemoveSessionFromList(transportSession, session);
   MXUser_ReleaseExclLock(transportSession->sessionArrayLock);
   HgfsServerCompleteRequest(HGFS_ERROR_SUCCESS, 0, input);
   HgfsServerSessionPut(session);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsBuildRelativePath --
 *
 *    Generates relative file path which need to be used a symbolic link
 *    target which would generate target name defined in "target" if the path
 *    to symbolic link file defined in the "source".
 *    Both source and target parameters represent absolute paths.
 *
 * Results:
 *    Allocated path that caller must free.
 *    NULL if there is a low memory condition.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char*
HgfsBuildRelativePath(const char* source,    // IN: source file name
                      const char* target)    // IN: target file name
{
   const char *relativeSource = source;
   const char *relativeTarget = target;
   const char* sourceSep;
   const char* targetSep;
   int level = 0;
   size_t targetSize;
   char *result;
   char *currentPosition;

   /*
    * First remove the part of the path which is common between source and
    * target
    */

   while (*relativeSource != '\0' && *relativeTarget != '\0') {
      sourceSep = strchr(relativeSource, DIRSEPC);
      targetSep = strchr(relativeTarget, DIRSEPC);
      if (sourceSep == NULL || targetSep == NULL) {
         break;
      }
      if ((sourceSep - relativeSource) != (targetSep - relativeTarget)) {
         break;
      }
      if (strncmp(relativeSource, relativeTarget,
                  (targetSep - relativeTarget)) != 0) {
         break;
      }
      relativeSource = sourceSep + 1;
      relativeTarget = targetSep + 1;
   };

   /*
    * Find out how many directories deep the source file is from the common
    * part of the  path.
    */
   while(*relativeSource != '\0') {
      sourceSep = strchr(relativeSource, DIRSEPC);
      if (sourceSep != NULL) {
         /* Several consecutive separators mean only one level. */
         while (*sourceSep == DIRSEPC) {
            sourceSep++;
         }
         if (*sourceSep != '\0') {
            level++;
            relativeSource = sourceSep;
         } else {
            break;
         }
      } else {
         break;
      }
   }

   /*
    * Consruct relative path by adding level number of "../"
    * to the relative target path.
    */

   targetSize = level * HGFS_PARENT_DIR_LEN + strlen(relativeTarget) + sizeof '\0';
   result = malloc(targetSize);
   currentPosition = result;
   if (result != NULL) {
      while (level != 0) {
         memcpy(currentPosition, HGFS_PARENT_DIR, HGFS_PARENT_DIR_LEN);
         level--;
         currentPosition += HGFS_PARENT_DIR_LEN;
      }
      memcpy(currentPosition, relativeTarget, strlen(relativeTarget) + sizeof '\0');
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerDirWatchEvent --
 *
 *    The callback is invoked by the file system change notification component
 *    in response to a change event when the client has set at least one watch
 *    on a directory.
 *
 *    The function builds directory notification packet and queues it to be sent
 *    to the client. It processes one notification at a time. Any consolidation of
 *    packets is expected to occur at the transport layer.
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
HgfsServerDirWatchEvent(HgfsSharedFolderHandle sharedFolder, // IN: shared folder
                        HgfsSubscriberHandle subscriber,     // IN: subsciber
                        char* fileName,                      // IN: name of the file
                        uint32 mask,                         // IN: event type
                        struct HgfsSessionInfo *session)     // IN: session info
{
   HgfsPacket *packet = NULL;
   HgfsHeader *packetHeader = NULL;
   char *shareName = NULL;
   size_t shareNameLen;
   size_t sizeNeeded;
   uint32 flags;

   LOG(4, ("%s:Entered shr hnd %u hnd %"FMT64"x file %s mask %u\n",
         __FUNCTION__, sharedFolder, subscriber, fileName, mask));

   if (!HgfsServerGetShareName(sharedFolder, &shareNameLen, &shareName)) {
      LOG(4, ("%s: failed to find shared folder for a handle %x\n",
              __FUNCTION__, sharedFolder));
      goto exit;
   }

   sizeNeeded = HgfsPackCalculateNotificationSize(shareName, fileName);

   packetHeader = Util_SafeCalloc(1, sizeNeeded);
   packet = Util_SafeCalloc(1, sizeof *packet);
   packet->guestInitiated = FALSE;
   packet->metaPacketSize = sizeNeeded;
   packet->metaPacket = packetHeader;
   packet->dataPacketIsAllocated = TRUE;
   flags = 0;
   if (mask & HGFS_NOTIFY_EVENTS_DROPPED) {
      flags |= HGFS_NOTIFY_FLAG_OVERFLOW;
   }

   if (!HgfsPackChangeNotificationRequest(packetHeader, subscriber, shareName, fileName, mask,
                                          flags, session, &sizeNeeded)) {
      LOG(4, ("%s: failed to pack notification request\n", __FUNCTION__));
      goto exit;
   }

   if (!HgfsPacketSend(packet, (char *)packetHeader,  sizeNeeded, session->transportSession, 0)) {
      LOG(4, ("%s: failed to send notification to the host\n", __FUNCTION__));
      goto exit;
   }

   /* The transport will call the server send complete callback to release the packets. */
   packet = NULL;
   packetHeader = NULL;

   LOG(4, ("%s: Sent notify for: %u index: %"FMT64"u file name %s mask %x\n",
           __FUNCTION__, sharedFolder, subscriber, fileName, mask));

exit:
   if (shareName) {
      free(shareName);
   }
   if (packet) {
      free(packet);
   }
   if (packetHeader) {
      free(packetHeader);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsShareRoot --
 *
 *    Checks if the cpName represents the root directory for a share.
 *    Components in CPName format are separated by NUL characters.
 *    CPName for the root of a share contains only one component thus
 *    it does not have any embedded '\0' characters in the name.
 *
 * Results:
 *    TRUE if it is the root directory, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsShareRoot(char const *cpName,         // IN: name to test
                size_t cpNameSize)          // IN: length of the name
{
   size_t i;
   for (i = 0; i < cpNameSize; i++) {
      if (cpName[i] == '\0') {
         return FALSE;
      }
   }
   return TRUE;
}

#ifdef HGFS_OPLOCKS
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOplockBreakReply --
 *
 *      The client was sent an oplock break request, and responded with this
 *      reply. It contains the oplock status that the client is now in. Since
 *      the break could have actually been a degrade, it is well within the
 *      client's rights to transition to a non-broken state. We need to make
 *      sure that such a transition was legal, acknowledge the brea
 *      appropriately, and update our own state.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsServerOplockBreakReply(const unsigned char *packetIn, // IN: Reply packet
                           unsigned int packetSize,       // IN: Size of packet
                           void *clientData)              // IN: From request
{
   HgfsReplyServerLockChange *reply;
   ServerLockData *lockData;
   ASSERT(packetIn);
   ASSERT(clientData);

   if (packetSize < sizeof *reply) {
      return;
   }
   reply = (HgfsReplyServerLockChange *)packetIn;
   lockData = (ServerLockData *)clientData;

   /*
    * XXX: It should be safe to ignore the status and id from the actual
    * HgfsReply. The only information we need to properly acknowledge the break
    * is the original fd and the new lease, which, in the case of a degrade,
    * is double checked in HgfsAckOplockBreak, so we'd be safe from a garbage
    * value.
    */
   HgfsAckOplockBreak(lockData, reply->serverLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOplockBreak --
 *
 *      When the host FS needs to break the oplock so that another client
 *      can open the file, it signals the event in the overlapped structure
 *      that we used to request an oplock.
 *      This sets off the following chains of events:
 *      1. Send the oplock break request to the guest.
 *      2. Once the guest acknowledges the oplock break, the completion
 *      routine GuestRpcServerRequestCallback will fire, causing
 *      HgfsServerOplockBreakReply to also fire, which will break the oplock
 *      on the host FS.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      If successful, allocates memory for the rpc request.
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsServerOplockBreak(ServerLockData *lockData)
{
   HgfsHandle hgfsHandle;
   char *requestBuffer = NULL;
   HgfsRequestServerLockChange *request;
   HgfsServerLock lock;

   LOG(4, ("%s: entered\n", __FUNCTION__));

   /*
    * XXX: Just because the file in not in the cache on the server,
    * does not mean it was closed on the client. It is possible that
    * we closed the file on the server because we ran out of space
    * in cache. That's why for now as long as a file has a lock,
    * we don't remove it from the node cache. This should be fixed.
    *
    * In any case, none of these cache-related failures should cause us to ack
    * the oplock break locally. That is because if the file wasn't in the
    * cache, or it had no lock, chances are someone else (maybe the VCPU
    * thread) broke the oplock and/or closed the file.
    */

   if (!HgfsFileDesc2Handle(lockData->fileDesc, &hgfsHandle)) {
      LOG(4, ("%s: file is not in the cache\n", __FUNCTION__));
      goto free_and_exit;
   }

   if (!HgfsHandle2ServerLock(hgfsHandle, &lock)) {
      LOG(4, ("%s: could not retrieve node's lock info.\n", __FUNCTION__));
      goto free_and_exit;
   }

   if (lock == HGFS_LOCK_NONE) {
      LOG(4, ("%s: the file does not have a server lock.\n", __FUNCTION__));
      goto free_and_exit;
   }

   /*
    * We need to setup the entire request here. The command prefix will be
    * added later, so save some space for it.
    *
    * XXX: This should probably go into a common allocation function that
    * other out-of-band requests can use.
    */

   requestBuffer = malloc(sizeof *request + HGFS_CLIENT_CMD_LEN);
   if (requestBuffer == NULL) {
      LOG(4, ("%s: could not allocate memory.\n", __FUNCTION__));
      goto ack_and_exit;
   }

   /* Save space for the command prefix. */
   request = (HgfsRequestServerLockChange *)
      (requestBuffer + HGFS_CLIENT_CMD_LEN);
   request->header.op = HGFS_OP_SERVER_LOCK_CHANGE;
   request->header.id = 0; /* XXX */
   request->file = hgfsHandle;
   request->newServerLock = lockData->serverLock;

   /*
    * Just send the request size for our actual request; our callee will
    * write in the command prefix and modify the request size appropriately.
    *
    * If for some reason we fail, we'll acknowledge the oplock break
    * immediately.
    */

   if (HgfsServerManager_SendRequest(requestBuffer, sizeof *request,
                                     HgfsServerOplockBreakReply, lockData)) {
      return;
   }
   free(requestBuffer);

  ack_and_exit:
   HgfsAckOplockBreak(lockData, HGFS_LOCK_NONE);

   return;

  free_and_exit:
   free(lockData);
}
#endif

/*
 * more testing
 */
#if 0
void
TestNodeFreeList(void)
{
   HgfsHandle array[10 * NUM_FILE_NODES];
   HgfsFileNode *node;
   unsigned int i;

   printf("%s: begin >>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __FUNCTION__);

   for (i = 0; i < sizeof array / sizeof array[0]; i++) {
      char tempName[20];
      HgfsLocalId localId;

      Str_Sprintf(tempName, sizeof tempName, "name%u", i);
      printf("\nadding node with name: %s\n", tempName);
      localId.volumeId = 0;
      localId.fileId = i + 1000;
      node = HgfsAddNewFileNode(Util_SafeStrdup(tempName), &localId);
      array[i] = HgfsFileNode2Handle(node);
   }

   HgfsDumpAllNodes();

   printf("done getting nodes, now freeing\n");

   for (i = 0; i < sizeof array / sizeof array[0]; i++) {
      printf("removing node #%u\n", i);
      HgfsRemoveFileNode(&nodeArray[array[i]]);
   }

   HgfsDumpAllNodes();
   printf("%s: end <<<<<<<<<<<<<<<<<<<<<<<<<< \n", __FUNCTION__);
}


void
TestSearchFreeList(void)
{
   HgfsHandle array[10 * NUM_SEARCHES];
   HgfsSearch *search;
   unsigned int i;

   printf("%s: begin >>>>>>>>>>>>>>>>>>>>>>>>>>>\n", __FUNCTION__);

   for (i = 0; i < sizeof array / sizeof array[0]; i++) {
      char tempName[20];

      Str_Sprintf(tempName, sizeof tempName, "baseDir%u", i);
      printf("\nadding search with baseDir: \"%s\"\n", tempName);
      search = HgfsAddNewSearch(Util_SafeStrdup(tempName));
      array[i] = HgfsSearch2SearchHandle(search);
   }

   HgfsDumpAllSearches();

   printf("done getting searches, now freeing\n");

   for (i = 0; i < sizeof array / sizeof array[0]; i++) {
      printf("removing search #%u\n", i);
      HgfsRemoveSearch(&searchArray[array[i]]);
   }

   HgfsDumpAllSearches();
   printf("%s: end <<<<<<<<<<<<<<<<<<<<<<<<<< \n", __FUNCTION__);
}
#endif
