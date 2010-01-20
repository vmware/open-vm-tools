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
#include "hgfsDirNotify.h"

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

#ifdef VMX86_TOOLS
/* We need to have a static session for use of HGFS server inside Tools. */
struct HgfsStaticSession {
   HgfsSessionInfo *session;        /* Session. */
   char *bufferOut;                 /* Reply buffer. */
   size_t bufferOutLen;             /* Reply buffer length. */
} hgfsStaticSession;
#endif

/* Session related callbacks. */
static void HgfsServerSessionReceive(char const *packetIn,
                                     size_t packetSize,
                                     void *clientData,
                                     HgfsReceiveFlags flags);
static Bool HgfsServerSessionConnect(void *transportData,
                                     HgfsSessionSendFunc *send,
                                     void **clientData);
static void HgfsServerSessionDisconnect(void *clientData);
static void HgfsServerSessionClose(void *clientData);
static void HgfsServerSessionInvalidateObjects(void *clientData,
                                               DblLnkLst_Links *shares);
static void HgfsServerSessionSendComplete(void *clientData, char *buffer);

/*
 * Callback table passed to transport and any channels.
 */
HgfsServerSessionCallbacks hgfsServerSessionCBTable = {
   HgfsServerSessionConnect,
   HgfsServerSessionDisconnect,
   HgfsServerSessionClose,
   HgfsServerSessionReceive,
   HgfsServerSessionInvalidateObjects,
   HgfsServerSessionSendComplete,
};

static Bool hgfsChangeNotificationSupported = FALSE;


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
static Bool HgfsValidatePacket(char const *packetIn, size_t packetSize);
static void HgfsPackReplyHeaderV4(HgfsInternalStatus status,
                                  uint32 payloadSize,
                                  HgfsHeader const *packetIn,
                                  HgfsHeader *header);

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

static void
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
 *    The session's nodeArrayLock should be acquired prior to calling this function.
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
 *    The session's nodeArrayLock should be acquired prior to calling this function.
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
 *    The session's nodeArrayLock should be acquired prior to calling this function.
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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
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
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   *appendFlag = fileNode->flags & HGFS_FILE_NODE_APPEND_FL;
   found = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   localId->volumeId = fileNode->localId.volumeId;
   localId->fileId = fileNode->localId.fileId;

   found = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle, session);
   if (fileNode == NULL) {
      goto exit;
   }

   *lock = fileNode->serverLock;
   found = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   for (i = 0; i < session->numNodes; i++) {
      existingFileNode = &session->nodeArray[i];
      if ((existingFileNode->state == FILENODE_STATE_IN_USE_CACHED) &&
          (existingFileNode->fileDesc == fd)) {
         *handle = HgfsFileNode2Handle(existingFileNode);
         found = TRUE;
         break;
      }
   }

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   existingFileNode = HgfsHandle2FileNode(handle, session);
   if (existingFileNode == NULL) {
      goto exit_unlock;
   }

   nameStatus = HgfsServerPolicy_GetShareMode(existingFileNode->shareName,
                                              existingFileNode->shareNameLen,
                                              shareMode);
   found = (nameStatus == HGFS_NAME_STATUS_COMPLETE);

exit_unlock:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

   *fileName = name;
   *fileNameSize = nameSize;

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   *sequentialOpen = node->flags & HGFS_FILE_NODE_SEQUENTIAL_FL;
   success = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   *sharedFolderOpen = node->flags & HGFS_FILE_NODE_SHARED_FOLDER_OPEN_FL;
   success = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   node->fileDesc = fd;
   node->fileCtx = fileCtx;
   updated = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   node = HgfsHandle2FileNode(handle, session);
   if (node == NULL) {
      goto exit;
   }

   if (appendFlag) {
      node->flags |= HGFS_FILE_NODE_APPEND_FL;
   }
   updated = TRUE;

exit:
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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
 *    The session's nodeArrayLock should be acquired prior to calling this function.
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
 *    The session's nodeArrayLock should be acquired prior to calling this function.
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
   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   HgfsFreeFileNodeInternal(handle, session);
   HGFS_LOCK_RELEASE(session->nodeArrayLock);
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
 *    The session nodeArrayLock should be acquired prior to calling this function.
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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   allowed = session->numCachedLockedNodes < MAX_LOCKED_FILENODES;
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);
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
   HGFS_LOCK_RELEASE(session->searchArrayLock);

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
   newSearch->utf8Dir = strdup(utf8Dir);
   if (newSearch->utf8Dir == NULL) {
      HgfsRemoveSearchInternal(newSearch, session);

      return NULL;
   }

   newSearch->utf8ShareNameLen = strlen(utf8ShareName);
   newSearch->utf8ShareName = strdup(utf8ShareName);
   if (newSearch->utf8ShareName == NULL) {
      HgfsRemoveSearchInternal(newSearch, session);

      return NULL;
   }

   newSearch->shareInfo.rootDirLen = strlen(rootDir);
   newSearch->shareInfo.rootDir = strdup(rootDir);
   if (newSearch->shareInfo.rootDir == NULL) {
      HgfsRemoveSearchInternal(newSearch, session);
      return NULL;
   }

   LOG(4, ("%s: got new search, handle %u\n", __FUNCTION__,
           HgfsSearch2SearchHandle(newSearch)));
   return newSearch;
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

   /* Free all of the dirents */
   if (search->dents) {
      unsigned int i;

      for (i = 0; i < search->numDents; i++) {
         free(search->dents[i]);
      }

      free(search->dents);
   }

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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   search = HgfsSearchHandle2Search(handle, session);
   if (search != NULL) {
      HgfsRemoveSearchInternal(search, session);
      success = TRUE;
   }

   HGFS_LOCK_RELEASE(session->searchArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

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
       * Make sure the name will not overrun the d_name buffer, the end of which
       * is also the end of the DirectoryEntry.
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
   HGFS_LOCK_RELEASE(session->searchArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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

   HGFS_LOCK_RELEASE(session->nodeArrayLock);
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

static HgfsInternalStatus
HgfsServerClose(char const *packetIn,      // IN: incoming packet
                size_t packetSize,         // IN: size of packet
                HgfsSessionInfo *session)  // IN: session info
{
   HgfsHandle file;
   char *packetOut = NULL;
   size_t replySize;
   HgfsInternalStatus status = 0;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackCloseRequest(packetIn, packetSize, &op, &file)) {
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto error;
   }

   LOG(4, ("%s: close fh %u\n", __FUNCTION__, file));

   if (!HgfsRemoveFromCache(file, session)) {
      LOG(4, ("%s: Could not remove the node from cache.\n", __FUNCTION__));
      status = HGFS_INTERNAL_STATUS_ERROR;
   } else {
      HgfsFreeFileNode(file, session);
   }

   if (!HgfsPackCloseReply(packetIn, status, op, &packetOut, &replySize) ||
       !HgfsPacketSend(packetOut, replySize, session, 0)) {
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto error;
   }

   return 0;

error:
   free(packetOut);

   return status;
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

static HgfsInternalStatus
HgfsServerSearchClose(char const *packetIn,      // IN: incoming packet
                      size_t packetSize,         // IN: size of packet
                      HgfsSessionInfo *session)  // IN: session info
{
   HgfsHandle search;
   char *packetOut = NULL;
   size_t replySize;
   HgfsInternalStatus status = 0;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(session);

   if (!HgfsUnpackSearchCloseRequest(packetIn, packetSize, &op, &search)) {
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto error;
   }

   LOG(4, ("%s: close search #%u\n", __FUNCTION__, search));

   if (!HgfsRemoveSearch(search, session)) {
      /* Invalid handle */
      LOG(4, ("%s: invalid handle %u\n", __FUNCTION__, search));
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto error;
   }

   if (!HgfsPackSearchCloseReply(packetIn, status, op,  &packetOut, &replySize) ||
       !HgfsPacketSend(packetOut, replySize, session, 0)) {
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto error;
   }

   return 0;

error:
   free(packetOut);

   return status;
}


#define HGFS_SIZEOF_OP(type) (sizeof (type) + sizeof (HgfsRequest))

/* Opcode handlers, indexed by opcode */
static struct {
   HgfsInternalStatus
   (*handler)(const char *packetIn,
              size_t packetSize,
              HgfsSessionInfo *session);

   /* Minimal size of the request packet */
   unsigned int minReqSize;
} const handlers[] = {
   { HgfsServerOpen,             sizeof (HgfsRequestOpen)              },
   { HgfsServerRead,             sizeof (HgfsRequestRead)              },
   { HgfsServerWrite,            sizeof (HgfsRequestWrite)             },
   { HgfsServerClose,            sizeof (HgfsRequestClose)             },
   { HgfsServerSearchOpen,       sizeof (HgfsRequestSearchOpen)        },
   { HgfsServerSearchRead,       sizeof (HgfsRequestSearchRead)        },
   { HgfsServerSearchClose,      sizeof (HgfsRequestSearchClose)       },
   { HgfsServerGetattr,          sizeof (HgfsRequestGetattr)           },
   { HgfsServerSetattr,          sizeof (HgfsRequestSetattr)           },
   { HgfsServerCreateDir,        sizeof (HgfsRequestCreateDir)         },
   { HgfsServerDeleteFile,       sizeof (HgfsRequestDelete)            },
   { HgfsServerDeleteDir,        sizeof (HgfsRequestDelete)            },
   { HgfsServerRename,           sizeof (HgfsRequestRename)            },
   { HgfsServerQueryVolume,      sizeof (HgfsRequestQueryVolume)       },

   { HgfsServerOpen,             sizeof (HgfsRequestOpenV2)            },
   { HgfsServerGetattr,          sizeof (HgfsRequestGetattrV2)         },
   { HgfsServerSetattr,          sizeof (HgfsRequestSetattrV2)         },
   { HgfsServerSearchRead,       sizeof (HgfsRequestSearchReadV2)      },
   { HgfsServerSymlinkCreate,    sizeof (HgfsRequestSymlinkCreate)     },
   { HgfsServerServerLockChange, sizeof (HgfsRequestServerLockChange)  },
   { HgfsServerCreateDir,        sizeof (HgfsRequestCreateDirV2)       },
   { HgfsServerDeleteFile,       sizeof (HgfsRequestDeleteV2)          },
   { HgfsServerDeleteDir,        sizeof (HgfsRequestDeleteV2)          },
   { HgfsServerRename,           sizeof (HgfsRequestRenameV2)          },

   { HgfsServerOpen,             HGFS_SIZEOF_OP(HgfsRequestOpenV3)             },
   { HgfsServerRead,             HGFS_SIZEOF_OP(HgfsRequestReadV3)             },
   { HgfsServerWrite,            HGFS_SIZEOF_OP(HgfsRequestWriteV3)            },
   { HgfsServerClose,            HGFS_SIZEOF_OP(HgfsRequestCloseV3)            },
   { HgfsServerSearchOpen,       HGFS_SIZEOF_OP(HgfsRequestSearchOpenV3)       },
   { HgfsServerSearchRead,       HGFS_SIZEOF_OP(HgfsRequestSearchReadV3)       },
   { HgfsServerSearchClose,      HGFS_SIZEOF_OP(HgfsRequestSearchCloseV3)      },
   { HgfsServerGetattr,          HGFS_SIZEOF_OP(HgfsRequestGetattrV3)          },
   { HgfsServerSetattr,          HGFS_SIZEOF_OP(HgfsRequestSetattrV3)          },
   { HgfsServerCreateDir,        HGFS_SIZEOF_OP(HgfsRequestCreateDirV3)        },
   { HgfsServerDeleteFile,       HGFS_SIZEOF_OP(HgfsRequestDeleteV3)           },
   { HgfsServerDeleteDir,        HGFS_SIZEOF_OP(HgfsRequestDeleteV3)           },
   { HgfsServerRename,           HGFS_SIZEOF_OP(HgfsRequestRenameV3)           },
   { HgfsServerQueryVolume,      HGFS_SIZEOF_OP(HgfsRequestQueryVolumeV3)      },
   { HgfsServerSymlinkCreate,    HGFS_SIZEOF_OP(HgfsRequestSymlinkCreateV3)    },
   { HgfsServerServerLockChange, sizeof (HgfsRequestServerLockChange)          },
   { HgfsServerWriteWin32Stream, HGFS_SIZEOF_OP(HgfsRequestWriteWin32StreamV3) },

};


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
 *    The handler function can send the reply packet either using HgfsPacketSend
 *    or HgfsPackAndSendPacket helper functions. This function would return error
 *    as a reply if the op handler do not return HGFS_STATUS_SUCCESS.
 *
 *    NOTE: If any op handler needs to keep packetIn around for sending replies
 *    at a later point (possibly in a different thread context), it should
 *    make a copy of it. The validity of packetIn for the HGFS server is only
 *    within the scope of this function.
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
HgfsServerSessionReceive(char const *packetIn,    // IN: incoming packet
                          size_t packetSize,       // IN: size of packet
                          void *clientData,        // IN: session info
                          HgfsReceiveFlags flags)  // IN: flags to indicate processing
{
   HgfsSessionInfo *session = (HgfsSessionInfo *)clientData;
   HgfsRequest *request = (HgfsRequest *)packetIn;
   HgfsHandle id;
   HgfsOp op;
   HgfsStatus status;
   Bool v4header = FALSE;

   ASSERT(session);
   ASSERT(request);

   if (session->state == HGFS_SESSION_STATE_CLOSED) {
      LOG(4, ("%s: %d: Received packet after disconnected.\n", __FUNCTION__,
              __LINE__));

      return;
   }

   /* Error out if less than HgfsRequest size. */
   if (packetSize < sizeof *request) {
      if (packetSize >= sizeof id) {
         id = request->id;
      } else {
         id = 0;
      }
      status = HGFS_STATUS_PROTOCOL_ERROR;
      goto err;
   }

   /* Increment the session's reference count until we send the reply. */
   HgfsServerSessionGet(session);

   id = request->id;
   op = request->op;
   /* If it is a V4 packet then handle it appropriately. */
   if (HGFS_V4_LEGACY_OPCODE == op) {
      HgfsHeader *header = (HgfsHeader *)packetIn;
      if (packetSize < sizeof *header) {
         status = HGFS_STATUS_PROTOCOL_ERROR;
         goto err;
      }
      op = header->op;
      v4header = TRUE;
   }

   if (!HgfsValidatePacket(packetIn, packetSize)) {
      status = HGFS_STATUS_PROTOCOL_ERROR;
      LOG(4, ("%s: %d: Possible BUG! malformed packet.\n", __FUNCTION__,
               __LINE__));
      goto err;
   }

   HGFS_ASSERT_MINIMUM_OP(op);
   if (op < sizeof handlers / sizeof handlers[0]) {
      if (packetSize >= handlers[op].minReqSize) {
         HgfsInternalStatus internalStatus;
         internalStatus = (*handlers[op].handler)(packetIn, packetSize,
                                                  session);
         status = HgfsConvertFromInternalStatus(internalStatus);
      } else {
         /*
          * The input packet is smaller than the minimal size needed for the
          * operation.
          */

         status = HGFS_STATUS_PROTOCOL_ERROR;
         LOG(4, ("%s: %d: Possible BUG! Smaller packet.\n", __FUNCTION__,
                 __LINE__));
      }
   } else {
      /* Unknown opcode */
      status = HGFS_STATUS_PROTOCOL_ERROR;
      LOG(4, ("%s: %d: Possible BUG! Invalid opcode.\n", __FUNCTION__,
              __LINE__));
   }
   HGFS_ASSERT_CLIENT(op);

err:
   /* Send error if we fail to process the op. */
   if (status != HGFS_STATUS_SUCCESS) {
      char *packetOut;
      uint32 replySize;
      if (v4header) {
         HgfsHeader *header;

         header = Util_SafeMalloc(sizeof *header);
         HgfsPackReplyHeaderV4(status, 0, (HgfsHeader *)packetIn, header);
         packetOut = (char *)header;
         replySize = sizeof *header;
      } else {
         HgfsReply *reply;

         reply = Util_SafeMalloc(sizeof *reply);
         reply->id = id;
         reply->status = status;
         packetOut = (char *)reply;
         replySize = sizeof *reply;
      }

      if (!HgfsPacketSend(packetOut, replySize, session, 0)) {
         /* Send failed. Drop the reply. */
         free(packetOut);
      }
   }
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
   ASSERT(callbackTable);

   /* Save any server manager data for logging state updates.*/
   hgfsMgrData = serverMgrData;

   maxCachedOpenNodes = Config_GetLong(MAX_CACHED_FILENODES,
                                       "hgfs.fdCache.maxNodes");

#ifndef VMX86_TOOLS
   if (Config_GetBool(FALSE, "hgfs.alwaysUseHostTime")) {
      alwaysUseHostTime = TRUE;
   }
#endif


#ifdef VMX86_TOOLS
   hgfsStaticSession.session = NULL;
   hgfsStaticSession.bufferOut = NULL;
   hgfsStaticSession.bufferOutLen = 0;
#endif

   if (HgfsNotify_Init() == 0) {
      hgfsChangeNotificationSupported = TRUE;
   }

   if (!HgfsServerPlatformInit()) {
      LOG(4, ("Could not initialize server platform specific \n"));

      return FALSE;
   }

   *callbackTable = &hgfsServerSessionCBTable;

   return TRUE;
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

#ifdef VMX86_TOOLS
   if (hgfsStaticSession.session != NULL) {
      HgfsServerSessionPut(hgfsStaticSession.session);
   }
#endif

   if (hgfsChangeNotificationSupported) {
      HgfsNotify_Shutdown();
   }

   HgfsServerPlatformDestroy();
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSessionConnect --
 *
 *    Initialize a new client session.
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

static Bool
HgfsServerSessionConnect(void *transportData,        // IN: transport session context
                         HgfsSessionSendFunc *send,  // IN: send reply callback
                         void **sessionData)         // OUT: server session context
{
   int i;
   HgfsSessionInfo *session = Util_SafeMalloc(sizeof *session);

   ASSERT(sessionData);

   LOG(4, ("%s: initting.\n", __FUNCTION__));

   /*
    * Initialize all our locks first as these can fail.
    */

   session->fileIOLock = HGFS_LOCK_NEW("HgfsFileIOLock");
   if (session->fileIOLock == NULL) {
      free(session);
      LOG(4, ("%s: Could not create node array sync mutex.\n", __FUNCTION__));

      return FALSE;
   }

   session->nodeArrayLock = HGFS_LOCK_NEW("HgfsNodeArrayLock");
   if (session->nodeArrayLock == NULL) {
      HGFS_LOCK_DESTROY(session->fileIOLock);
      free(session);
      LOG(4, ("%s: Could not create node array sync mutex.\n", __FUNCTION__));

      return FALSE;
   }

   session->searchArrayLock = HGFS_LOCK_NEW("HgfsSearchArrayLock");
   if (session->searchArrayLock == NULL) {
      HGFS_LOCK_DESTROY(session->fileIOLock);
      HGFS_LOCK_DESTROY(session->nodeArrayLock);
      free(session);
      LOG(4, ("%s: Could not create search array sync mutex.\n", __FUNCTION__));

      return FALSE;
   }

   /*
    * Initialize the node handling components.
    */

   DblLnkLst_Init(&session->nodeFreeList);
   DblLnkLst_Init(&session->nodeCachedList);

   /* Allocate array of FileNodes and add them to free list. */
   session->numNodes = NUM_FILE_NODES;
   session->nodeArray = Util_SafeCalloc(session->numNodes, sizeof (HgfsFileNode));
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

   /* Allocate array of searches and add them to free list. */
   session->numSearches = NUM_SEARCHES;
   session->searchArray = Util_SafeCalloc(session->numSearches, sizeof (HgfsSearch));

   for (i = 0; i < session->numSearches; i++) {
      DblLnkLst_Init(&session->searchArray[i].links);
      /* Append at the end of the list. */
      DblLnkLst_LinkLast(&session->searchFreeList,
                         &session->searchArray[i].links);
   }

   /*
    * Initialize the general session stuff.
    */

   session->type = HGFS_SESSION_TYPE_REGULAR;
   session->state = HGFS_SESSION_STATE_OPEN;
   session->transportData = transportData;
   session->send = send;
   Atomic_Write(&session->refCount, 0);

   /* Give our session a reference to hold while we are open. */
   HgfsServerSessionGet(session);
   *sessionData = session;

   return TRUE;
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
   HgfsSessionInfo *session = (HgfsSessionInfo *)clientData;

   ASSERT(session);
   ASSERT(session->nodeArray);
   ASSERT(session->searchArray);

   session->state = HGFS_SESSION_STATE_CLOSED;
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
   HgfsSessionInfo *session = (HgfsSessionInfo *)clientData;

   ASSERT(session);
   ASSERT(session->nodeArray);
   ASSERT(session->searchArray);

   ASSERT(session->state == HGFS_SESSION_STATE_CLOSED);

   /* Remove, typically, the last reference, will teardown everything. */
   HgfsServerSessionPut(session);
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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

   /*
    * Recycle all searches that are still in use, then destroy the
    * search pool.
    */

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   for (i = 0; i < session->numSearches; i++) {
      if (DblLnkLst_IsLinked(&session->searchArray[i].links)) {
         continue;
      }
      HgfsRemoveSearchInternal(&session->searchArray[i], session);
   }
   free(session->searchArray);
   session->searchArray = NULL;

   HGFS_LOCK_RELEASE(session->searchArrayLock);

   /* Teardown the locks for the sessions and destroy itself. */
   HGFS_LOCK_DESTROY(session->nodeArrayLock);
   HGFS_LOCK_DESTROY(session->searchArrayLock);
   HGFS_LOCK_DESTROY(session->fileIOLock);
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


#ifdef VMX86_TOOLS
/*
 *----------------------------------------------------------------------------
 *
 * HgfsServer_ProcessPacket --
 *
 *    Process packet not associated with any session.
 *
 *    This function is used in the HGFS server inside Tools.
 *
 *    Create an internal session if not already created, and process the packet.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------------
 */

void
HgfsServer_ProcessPacket(char const *packetIn,   // IN: incoming packet
                         char *packetOut,        // OUT: outgoing packet
                         size_t *packetLen,      // IN/OUT: packet length
                         HgfsReceiveFlags flags) // IN: flags
{
   ASSERT(packetIn);
   ASSERT(packetOut);
   ASSERT(packetLen);

   if (*packetLen == 0) {
      return;
   }

   /*
    * Create the session if not already created.
    * This session is destroyed in HgfsServer_ExitState.
    */

   if (hgfsStaticSession.session == NULL) {
      if (!HgfsServerSessionConnect(NULL, NULL,
                                    (void **)&hgfsStaticSession.session)) {
         *packetLen = 0;

         return;
      }

      /* Mark the session as internal. */
      hgfsStaticSession.session->type = HGFS_SESSION_TYPE_INTERNAL;
   }

   HgfsServerSessionReceive(packetIn, *packetLen, hgfsStaticSession.session, 0);

   /*
    * At this point, all the HGFS ops send reply synchronously. So
    * we should have the reply by now.
    * XXX This should change if any async replies are expected.
    */

   ASSERT(hgfsStaticSession.bufferOut);

   memcpy(packetOut, hgfsStaticSession.bufferOut,
          hgfsStaticSession.bufferOutLen);
   *packetLen = hgfsStaticSession.bufferOutLen;

   HgfsServerSessionSendComplete(hgfsStaticSession.session,
                           hgfsStaticSession.bufferOut);
   hgfsStaticSession.bufferOut = NULL;
}
#endif


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
 *----------------------------------------------------------------------------
 */

void
HgfsServerSessionSendComplete(void *clientData, // IN: session currently unused
                              char *buffer)     // IN: sent buffer
{
   free(buffer);
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
HgfsPacketSend(char *packet,                // IN: packet buffer
               size_t packetSize,           // IN: packet size
               HgfsSessionInfo *session,    // IN: session info
               HgfsSendFlags flags)         // IN: flags for how to process
{
   Bool result = FALSE;

   ASSERT(packet);
   ASSERT(session);

   if (session->state == HGFS_SESSION_STATE_OPEN) {
#ifndef VMX86_TOOLS
      ASSERT(session->type == HGFS_SESSION_TYPE_REGULAR);
      result = session->send(session->transportData, packet, packetSize, flags);
#else
      /* This is internal session. */
      ASSERT(session->type == HGFS_SESSION_TYPE_INTERNAL);
      hgfsStaticSession.bufferOut = packet;
      hgfsStaticSession.bufferOutLen = packetSize;
      result = TRUE;
#endif
   }

   HgfsServerSessionPut(session);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackAndSendPacket --
 *
 *      Packs up the reply with id and status and sends the packet.
 *
 * Results:
 *      TRUE on success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackAndSendPacket(char *packet,               // IN: packet to send
                      size_t packetSize,          // IN: packet size
                      HgfsInternalStatus status,  // IN: status
                      HgfsHandle id,              // IN: id of the request packet
                      HgfsSessionInfo *session,   // IN: session info
                      HgfsSendFlags flags)        // IN: flags how to send
{
   HgfsReply *reply = (HgfsReply *)packet;

   ASSERT(packet);
   ASSERT(session);
   ASSERT(packetSize <= HGFS_LARGE_PACKET_MAX);

   reply->id = id;
   reply->status = HgfsConvertFromInternalStatus(status);

   return HgfsPacketSend(packet, packetSize, session, flags);
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


   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

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

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   /*
    * Iterate over each search, skipping those that are on the free list. For
    * each search, if its base name is no longer within a share, remove it.
    */
   for (i = 0; i < session->numSearches; i++) {
      HgfsHandle handle;
      DblLnkLst_Links *l;

      if (DblLnkLst_IsLinked(&session->searchArray[i].links)) {
         continue;
      }

      handle = HgfsSearch2SearchHandle(&session->searchArray[i]);
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

   HGFS_LOCK_RELEASE(session->searchArrayLock);

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
   HgfsSessionInfo *session = (HgfsSessionInfo *)clientData;

   HgfsInvalidateSessionObjects(shares, session);

#ifdef VMX86_TOOLS
   if (hgfsStaticSession.session != NULL) {
      HgfsInvalidateSessionObjects(shares, hgfsStaticSession.session);
   }
#endif
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
   if (pathLength < 0 || pathLength >= sizeof p.mountPoint) {
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
      LOG(4, ("%s: name is \"%s\"\n", __FUNCTION__, myBufOut));

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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   search = HgfsSearchHandle2Search(searchHandle, session);
   if (search != NULL) {
      Log("%s: %u dents in \"%s\"\n", __FUNCTION__, search->numDents,
          search->utf8Dir);

      Log("Dumping dents:\n");
      for (i = 0; i < search->numDents; i++) {
         Log("\"%s\"\n", search->dents[i]->d_name);
      }
   }

   HGFS_LOCK_RELEASE(session->searchArrayLock);
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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   search = HgfsAddNewSearch(baseDir, DIRECTORY_SEARCH_TYPE_DIR, shareName,
                             rootDir, session);
   if (!search) {
      LOG(4, ("%s: failed to get new search\n", __FUNCTION__));
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   /* Get the config options. */
   nameStatus = HgfsServerPolicy_GetShareOptions(shareName, strlen(shareName),
                                                 &configOptions);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("%s: no matching share: %s.\n", __FUNCTION__, shareName));
      status = HGFS_INTERNAL_STATUS_ERROR;
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
   HGFS_LOCK_RELEASE(session->searchArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->searchArrayLock);

   search = HgfsAddNewSearch("", type, "", "", session);
   if (!search) {
      LOG(4, ("%s: failed to get new search\n", __FUNCTION__));
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   result = HgfsServerGetDents(getName, initName, cleanupName, &search->dents);
   if (result < 0) {
      LOG(4, ("%s: couldn't get dents\n", __FUNCTION__));
      HgfsRemoveSearchInternal(search, session);
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   search->numDents = result;
   *handle = HgfsSearch2SearchHandle(search);

  out:
   HGFS_LOCK_RELEASE(session->searchArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   removed = HgfsRemoveFromCacheInternal(handle, session);
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   cached = HgfsIsCachedInternal(handle, session);
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

   ASSERT(session);
   ASSERT(session->numCachedOpenNodes > 0);

   /*
    * Remove the first item from the list that does not have a server lock,
    * file context or is open in sequential mode.
    */
   while (!found) {
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

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);
   added = HgfsAddToCacheInternal(handle, session);
   HGFS_LOCK_RELEASE(session->nodeArrayLock);

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

      return FALSE;
   }

   /* See if we are dealing with the base of the namespace */
   if (!len) {
      return FALSE;
   }

   if (!next) {
      sharedFolderOpen = TRUE;
   }

   HGFS_LOCK_ACQUIRE(session->nodeArrayLock);

   node = HgfsAddNewFileNode(openInfo, localId, fileDesc, append, len,
                             openInfo->cpName, sharedFolderOpen, session);

   if (node == NULL) {
      LOG(4, ("%s: Failed to add new node.\n", __FUNCTION__));
      HGFS_LOCK_RELEASE(session->nodeArrayLock);

      return FALSE;
   }
   handle = HgfsFileNode2Handle(node);

   if (!HgfsAddToCacheInternal(handle, session)) {
      LOG(4, ("%s: Failed to add node to the cache.\n", __FUNCTION__));
      HGFS_LOCK_RELEASE(session->nodeArrayLock);

      return FALSE;
   }

   HGFS_LOCK_RELEASE(session->nodeArrayLock);

   /* Only after everything is successful, save the handle in the open info. */
   openInfo->file = handle;

   return TRUE;
}


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

static Bool
HgfsValidatePacket(char const *packetIn,        // IN: request packet
                   size_t packetSize)           // IN: request packet size
{
   HgfsRequest *request = (HgfsRequest *)packetIn;
   Bool result = TRUE;
   if (packetSize < sizeof *request) {
      return FALSE;
   }
   if (request->op < HGFS_OP_CREATE_SESSION_V4) {
      result = packetSize >= sizeof *request;
   } else {
      HgfsHeader *header = (HgfsHeader *)packetIn;
      ASSERT(packetSize >= header->packetSize);
      ASSERT(header->packetSize >= header->headerSize);
      result = packetSize >= offsetof(HgfsHeader, requestId) &&
               header->headerSize >= offsetof(HgfsHeader, reserved) &&
               header->packetSize >= header->headerSize &&
               packetSize >= header->packetSize;
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
 *    TRUE if the packet has been successfully processed.
 *    FALSE if the incoming request is malformed.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsParseRequest(char const *packetIn,        // IN: request packet
                 size_t packetSize,           // IN: request packet size
                 void const **payload,        // OUT: payload
                 size_t *payloadSize,         // OUT: payloadSize
                 HgfsOp *op)                  // OUT: operation
{
   HgfsRequest *request = (HgfsRequest *)packetIn;
   Bool result = FALSE;
   ASSERT(packetSize >= sizeof *request);
   *payload = NULL;
   *payloadSize = 0;
   if (request->op < HGFS_OP_OPEN_V3) {
      /* Legacy requests do not have a separate header. */
      *payload = packetIn;
      *op = request->op;
      *payloadSize = packetSize;
      result = TRUE;
   } else if (request->op < HGFS_OP_CREATE_SESSION_V4) {
      /* V3 header. */
      if (packetSize > sizeof *request) {
         *payload = HGFS_REQ_GET_PAYLOAD_V3(packetIn);
         *payloadSize = packetSize - ((char *)*payload - packetIn);
      }
      *op = request->op;
      result = TRUE;
   } else {
      /* V4 header. */
      if (request->op == HGFS_V4_LEGACY_OPCODE) {
         HgfsHeader *header = (HgfsHeader *)packetIn;
         if (packetSize >= header->packetSize &&
            header->packetSize >= header->headerSize) {
            *op = header->op;
            result = TRUE;
            if (header->packetSize > header->headerSize) {
               *payload = packetIn + header->headerSize;
               *payloadSize = header->packetSize - header->headerSize;
            }
         }
      }
   }
   return result;
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
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV1;

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV1->fileName.length > extra) {
      /* The input packet is smaller than the request. */
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
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV2;

   if (!(requestV2->mask & HGFS_OPEN_VALID_FILE_NAME)) {
      /* We do not support open requests without a valid file name. */
      return FALSE;
   }

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV2->fileName.length > extra) {
      /* The input packet is smaller than the request. */
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
      return FALSE;
   }

   extra = payloadSize - sizeof *requestV3;

   if (!(requestV3->mask & HGFS_OPEN_VALID_FILE_NAME)) {
      /* We do not support open requests without a valid file name. */
      return FALSE;
   }

   /*
    * The request file name length is user-provided, so this test must be
    * carefully written to prevent wraparounds.
    */
   if (requestV3->fileName.length > extra) {
      /* The input packet is smaller than the request. */
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
HgfsUnpackOpenRequest(char const *packetIn,        // IN: request packet
                      size_t packetSize,           // IN: request packet size
                      HgfsFileOpenInfo *openInfo)  // IN/OUT: open info structure
{
   void const *payload;
   size_t payloadSize;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(openInfo);
   
   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, &op)) {
      return FALSE;
   }

   openInfo->requestType = op;
   openInfo->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (op) {
   case HGFS_OP_OPEN_V3: {
         HgfsRequestOpenV3 *requestV3 = (HgfsRequestOpenV3 *)payload;
         LOG(4, ("%s: HGFS_OP_OPEN_V3\n", __FUNCTION__));

         if (!HgfsUnpackOpenPayloadV3(requestV3, payloadSize, openInfo)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_OPEN_V2: {
         HgfsRequestOpenV2 *requestV2 = (HgfsRequestOpenV2 *)payload;

         if (!HgfsUnpackOpenPayloadV2(requestV2, payloadSize, openInfo)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_OPEN: {
         HgfsRequestOpen *requestV1 = (HgfsRequestOpen *)payload;

         if (!HgfsUnpackOpenPayloadV1(requestV1, payloadSize, openInfo)) {
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

static void
HgfsPackReplyHeaderV4(HgfsInternalStatus status,    // IN: reply status
                      uint32 payloadSize,           // IN: size of the reply payload
                      HgfsHeader const *packetIn,   // IN: original incoming packet
                      HgfsHeader *header)           // OUT: outgoing packet header
{
   memset(header, 0, sizeof *header);
   header->version = 1;
   header->dummy = HGFS_V4_LEGACY_OPCODE;
   header->packetSize = payloadSize + sizeof *header;
   header->headerSize = sizeof *header;
   header->requestId = packetIn->requestId;
   header->op = packetIn->op;
   header->status = HgfsConvertFromInternalStatus(status);
   header->flags = 0;
   header->information = status;
   header->sessionId = packetIn->sessionId;
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
 * HgfsAllocInitReply --
 *
 *    Allocates hgfs reply packet and initializes its header.
 *
 * Results:
 *    Size of the allocated packet.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsAllocInitReply(char const *packetIn,         // IN: incoming packet
                   size_t payloadSize,           // IN: payload size
                   HgfsInternalStatus status,    // IN: reply status
                   char **packetOut,             // OUT: allocated reply
                   void **payload,               // OUT: pointer to the reply payload
                   size_t *packetSize)           // OUT: size of the allocated packet
{
   HgfsRequest *request = (HgfsRequest *)packetIn;
   size_t headerSize = 0; /* Replies prior to V3 do not have a header. */
   if (HGFS_V4_LEGACY_OPCODE == request->op) {
      headerSize = sizeof(HgfsHeader);
   } else if (request->op < HGFS_OP_CREATE_SESSION_V4 &&
              request->op > HGFS_OP_RENAME_V2) {
      headerSize = sizeof(HgfsReply);
   }
   *packetSize = headerSize + payloadSize;
   *packetOut = Util_SafeCalloc(1, *packetSize);
   *payload = *packetOut + headerSize;
   if (HGFS_V4_LEGACY_OPCODE == request->op) {
      HgfsPackReplyHeaderV4(status,
                            payloadSize,
                            (HgfsHeader const *)packetIn,
                            (HgfsHeader *)*packetOut);
   } else if (request->op < HGFS_OP_CREATE_SESSION_V4) {
      HgfsRequest const *request = (HgfsRequest const *)packetIn;
      HgfsPackLegacyReplyHeader(status, request->id, (HgfsReply *)*packetOut);
   }
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
HgfsPackOpenReply(char const *packetIn,         // IN: incoming packet
                  HgfsInternalStatus status,    // IN: reply status
                  HgfsFileOpenInfo *openInfo,   // IN: open info struct
                  char **packetOut,             // OUT: outgoing packet
                  size_t *packetSize)           // OUT: size of packet
{
   ASSERT(packetIn);
   ASSERT(openInfo);
   ASSERT(packetSize);

   *packetOut = NULL;
   *packetSize = 0;

   switch (openInfo->requestType) {
   case HGFS_OP_OPEN_V3: {
      HgfsReplyOpenV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      HgfsPackOpenReplyV3(openInfo, reply);
      break;
   }
   case HGFS_OP_OPEN_V2: {
      HgfsReplyOpenV2 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      HgfsPackOpenV2Reply(openInfo, reply);
      break;
   }
   case HGFS_OP_OPEN: {
      HgfsReplyOpen *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      HgfsPackOpenV1Reply(openInfo, reply);
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
HgfsUnpackCloseRequest(char const *packetIn,        // IN: request packet
                       size_t packetSize,           // IN: request packet size
                       HgfsOp *op,                  // OUT: request type
                       HgfsHandle *file)            // OUT: Handle to close
{
   void const *payload;
   size_t payloadSize;

   ASSERT(packetIn);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, op)) {
      return FALSE;
   }

   switch (*op) {
   case HGFS_OP_CLOSE_V3: {
         HgfsRequestCloseV3 *requestV3 = (HgfsRequestCloseV3 *)payload;

         if (!HgfsUnpackClosePayloadV3(requestV3, payloadSize, file)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_CLOSE: {
         HgfsRequestClose *requestV1 = (HgfsRequestClose *)payload;

         if (!HgfsUnpackClosePayload(requestV1, payloadSize, file)) {
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
HgfsPackCloseReply(char const *packetIn,      // IN: incoming packet
                  HgfsInternalStatus status,  // IN: reply status
                  HgfsOp op,                  // IN: request type
                  char **packetOut,           // OUT: outgoing packet
                  size_t *packetSize)         // OUT: size of packet
{
   ASSERT(packetIn);
   ASSERT(packetSize);

   *packetOut = NULL;
   *packetSize = 0;

   switch (op) {
   case HGFS_OP_CLOSE_V3: {
      HgfsReplyCloseV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_CLOSE: {
      HgfsReplyClose *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
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
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsUnpackClosePayloadV3 --
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
HgfsUnpackSearchCloseRequest(char const *packetIn,        // IN: request packet
                             size_t packetSize,           // IN: request packet size
                             HgfsOp *op,                  // OUT: request type
                             HgfsHandle *search)          // OUT: search to close
{
   void const *payload;
   size_t payloadSize;

   ASSERT(packetIn);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, op)) {
      return FALSE;
   }

   switch (*op) {
   case HGFS_OP_SEARCH_CLOSE_V3: {
         HgfsRequestSearchCloseV3 *requestV3 = (HgfsRequestSearchCloseV3 *)payload;

         if (!HgfsUnpackSearchClosePayloadV3(requestV3, payloadSize, search)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_SEARCH_CLOSE: {
         HgfsRequestSearchClose *requestV1 = (HgfsRequestSearchClose *)payload;

         if (!HgfsUnpackSearchClosePayload(requestV1, payloadSize, search)) {
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
 *    wrong op (which is a programming error).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSearchCloseReply(char const *packetIn,       // IN: incoming packet
                         HgfsInternalStatus status,  // IN: reply status
                         HgfsOp op,                  // IN: request type
                         char **packetOut,           // OUT: outgoing packet
                         size_t *packetSize)         // OUT: size of packet
{
   ASSERT(packetIn);
   ASSERT(packetSize);

   *packetOut = NULL;
   *packetSize = 0;

   switch (op) {
   case HGFS_OP_SEARCH_CLOSE_V3: {
      HgfsReplyCloseV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_SEARCH_CLOSE: {
      HgfsReplyClose *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
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
HgfsUnpackDeleteRequest(char const *packetIn,       // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        HgfsOp *op,                 // OUT: requested operation
                        char **cpName,              // OUT: cpName
                        size_t *cpNameSize,         // OUT: cpName size
                        HgfsDeleteHint *hints,      // OUT: delete hints
                        HgfsHandle *file,           // OUT: file handle
                        uint32 *caseFlags)          // OUT: case-sensitivity flags
{
   void const *payload;
   size_t payloadSize;

   ASSERT(packetIn);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(hints);
   ASSERT(caseFlags);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, op)) {
      return FALSE;
   }

   *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (*op) {
   case HGFS_OP_DELETE_FILE_V3:
   case HGFS_OP_DELETE_DIR_V3: {
      HgfsRequestDeleteV3 *requestV3 = (HgfsRequestDeleteV3 *)payload;

      if (!HgfsUnpackDeletePayloadV3(requestV3,
                                     payloadSize,
                                     cpName,
                                     cpNameSize,
                                     hints,
                                     file,
                                     caseFlags)) {
         return FALSE;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE_V2:
   case HGFS_OP_DELETE_DIR_V2: {
      HgfsRequestDeleteV2 *requestV2 = (HgfsRequestDeleteV2 *)payload;

      if (!HgfsUnpackDeletePayloadV2(requestV2,
                                     payloadSize,
                                     cpName,
                                     cpNameSize,
                                     hints,
                                     file)) {
         return FALSE;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE:
   case HGFS_OP_DELETE_DIR: {
      HgfsRequestDelete *requestV1 = (HgfsRequestDelete *)payload;

      if (!HgfsUnpackDeletePayloadV1(requestV1,
                                     payloadSize,
                                     cpName,
                                     cpNameSize)) {
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
HgfsPackDeleteReply(char const *packetIn,      // IN: incoming packet
                    HgfsInternalStatus status, // IN: reply status
                    HgfsOp op,                 // IN: requested operation
                    char **packetOut,          // OUT: outgoing packet
                    size_t *packetSize)        // OUT: size of packet
{
   Bool result = TRUE;
   ASSERT(packetIn);
   ASSERT(packetSize);

   *packetOut = NULL;
   *packetSize = 0;

   /* No reply payload, just header. */
   switch (op) {
   case HGFS_OP_DELETE_FILE_V3:
   case HGFS_OP_DELETE_DIR_V3: {
      HgfsReplyDeleteV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_DELETE_FILE_V2:
   case HGFS_OP_DELETE_FILE:
   case HGFS_OP_DELETE_DIR_V2:
   case HGFS_OP_DELETE_DIR: {
      HgfsReplyDelete *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   default:
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      result = FALSE;
      NOT_REACHED();
      break;
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
HgfsUnpackRenameRequest(char const *packetIn,       // IN: request packet
                        size_t packetSize,          // IN: request packet size
                        HgfsOp *op,                 // OUT: requested operation
                        char **cpOldName,           // OUT: rename src
                        size_t *cpOldNameLen,       // OUT: rename src size
                        char **cpNewName,           // OUT: rename dst
                        size_t *cpNewNameLen,       // OUT: rename dst size
                        HgfsRenameHint *hints,      // OUT: rename hints
                        HgfsHandle *srcFile,        // OUT: src file handle
                        HgfsHandle *targetFile,     // OUT: target file handle
                        uint32 *oldCaseFlags,       // OUT: source case-sensitivity flags
                        uint32 *newCaseFlags)       // OUT: dest. case-sensitivity flags
{
   void const *payload;
   size_t payloadSize;

   ASSERT(packetIn);
   ASSERT(cpOldName);
   ASSERT(cpOldNameLen);
   ASSERT(cpNewName);
   ASSERT(cpNewNameLen);
   ASSERT(srcFile);
   ASSERT(targetFile);
   ASSERT(hints);
   ASSERT(oldCaseFlags);
   ASSERT(newCaseFlags);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, op)) {
      return FALSE;
   }

   /* Default values for legacy requests. */
   *oldCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *newCaseFlags = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;

   switch (*op) {
   case HGFS_OP_RENAME_V3:
   {
      HgfsRequestRenameV3 *requestV3 = (HgfsRequestRenameV3 *)payload;

      if (!HgfsUnpackRenamePayloadV3(requestV3,
                                     payloadSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen,
                                     hints,
                                     srcFile,
                                     targetFile,
                                     oldCaseFlags,
                                     newCaseFlags)) {
         return FALSE;
      }
      break;
   }
   case HGFS_OP_RENAME_V2:
   {
      HgfsRequestRenameV2 *requestV2 = (HgfsRequestRenameV2 *)payload;

      if (!HgfsUnpackRenamePayloadV2(requestV2,
                                     payloadSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen,
                                     hints,
                                     srcFile,
                                     targetFile)) {
         return FALSE;
      }
      break;
   }

   case HGFS_OP_RENAME:
   {
      HgfsRequestRename *requestV1 = (HgfsRequestRename *)payload;

      if (!HgfsUnpackRenamePayloadV1(requestV1,
                                     payloadSize,
                                     cpOldName,
                                     cpOldNameLen,
                                     cpNewName,
                                     cpNewNameLen)) {
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
HgfsPackRenameReply(char const *packetIn,      // IN: incoming packet
                    HgfsInternalStatus status, // IN: reply status
                    HgfsOp op,                 // IN: requested operation
                    char **packetOut,          // OUT: outgoing packet
                    size_t *packetSize)        // OUT: size of packet
{
   Bool result = TRUE;
   ASSERT(packetIn);
   ASSERT(packetSize);

   *packetOut = NULL;
   *packetSize = 0;

   switch (op) {
   case HGFS_OP_RENAME_V3: {
      HgfsReplyRenameV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_RENAME_V2:
   case HGFS_OP_RENAME: {
      HgfsReplyRename *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   default:
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      result = FALSE;
      NOT_REACHED();
      break;
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
HgfsPackGetattrReplyPayloadV2(HgfsHandle id,                // IN: id of the request
                              HgfsInternalStatus status,    // IN: error code
                              HgfsFileAttrInfo *attr,       // IN: attr stucture
                              const char *utf8TargetName,   // IN: optional target name
                              uint32 utf8TargetNameLen,     // IN: file name length
                              HgfsReplyGetattrV2 *reply)    // OUT: payload
{
   reply->header.status = HgfsConvertFromInternalStatus(status);
   reply->header.id = id;

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
HgfsPackGetattrReplyPayloadV1(HgfsHandle id,                // IN: id of the request
                              HgfsInternalStatus status,    // IN: error code
                              HgfsFileAttrInfo *attr,       // IN: attr stucture
                              HgfsReplyGetattr *reply)      // OUT: reply info
{
   reply->header.status = HgfsConvertFromInternalStatus(status);
   reply->header.id = id;

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
   HgfsRequest *request = (HgfsRequest *)packetIn;
   /*
    * XXX:
    * At the moment assume that V4 protocol does not impose any restrictions
    * on the packet size.
    * May need to review later.
    */
   if (HGFS_V4_LEGACY_OPCODE == request->op) {
      return TRUE;
   }

   if (HGFS_OP_READ_V3 == op) {
      return packetSize <= HGFS_LARGE_PACKET_MAX;
   }

   return packetSize <= HGFS_PACKET_MAX;
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
HgfsUnpackGetattrRequest(char const *packetIn,       // IN: request packet
                         size_t packetSize,          // IN: request packet size
                         HgfsFileAttrInfo *attrInfo, // IN/OUT: getattr info
                         HgfsAttrHint *hints,        // OUT: getattr hints
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: file handle
                         uint32 *caseType)           // OUT: case-sensitivity flags
{
   void const *payload;
   size_t payloadSize;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(attrInfo);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseType);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, &op)) {
      return FALSE;
   }

   HgfsInitFileAttr(op, attrInfo);

   /* Default values for legacy requests. */
   *caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (op) {
   case HGFS_OP_GETATTR_V3: {
      HgfsRequestGetattrV3 *requestV3 = (HgfsRequestGetattrV3 *)payload;

      if (!HgfsUnpackGetattrPayloadV3(requestV3,
                                      payloadSize,
                                      cpName,
                                      cpNameSize,
                                      hints,
                                      file,
                                      caseType)) {
         return FALSE;
      }
      LOG(4, ("%s: HGFS_OP_GETATTR_V3: %u\n", __FUNCTION__, *caseType));
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsRequestGetattrV2 *requestV2 = (HgfsRequestGetattrV2 *)payload;

      if (!HgfsUnpackGetattrPayloadV2(requestV2,
                                      payloadSize,
                                      cpName,
                                      cpNameSize,
                                      hints,
                                      file)) {
         return FALSE;
      }
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsRequestGetattr *requestV1 = (HgfsRequestGetattr *)payload;

      if (!HgfsUnpackGetattrPayloadV1(requestV1,payloadSize, cpName, cpNameSize)) {
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
HgfsPackGetattrReply(char const *packetIn,       // IN: incoming packet
                     HgfsInternalStatus status,  // IN: reply status
                     HgfsFileAttrInfo *attr,     // IN: attr stucture
                     const char *utf8TargetName, // IN: optional target name
                     uint32 utf8TargetNameLen,   // IN: file name length
                     char **packetOut,           // OUT: outgoing packet
                     size_t *packetSize)         // OUT: size of packet
{
   ASSERT(packetIn);
   ASSERT(attr);

   *packetOut = NULL;
   *packetSize = 0;

   switch (attr->requestType) {
   case HGFS_OP_GETATTR_V3: {
      HgfsReplyGetattrV3 *reply;
      uint32 payloadSize = sizeof *reply + utf8TargetNameLen;

      HgfsAllocInitReply(packetIn, payloadSize, status, packetOut,
                         (void **)&reply, packetSize);
      if (!HgfsValidateReplySize(packetIn, attr->requestType, *packetSize)) {
         free(reply);
         return FALSE;
      }
      HgfsPackGetattrReplyPayloadV3(attr, utf8TargetName, utf8TargetNameLen, reply);
      break;
   }

   case HGFS_OP_GETATTR_V2: {
      HgfsReplyGetattrV2 *reply;
      HgfsRequest *request = (HgfsRequest *)packetIn;
      uint32 payloadSize = sizeof *reply + utf8TargetNameLen;

      HgfsAllocInitReply(packetIn, payloadSize, status, packetOut,
                         (void **)&reply, packetSize);
      if (!HgfsValidateReplySize(packetIn, attr->requestType, *packetSize)) {
         free(reply);
         return FALSE;
      }
      HgfsPackGetattrReplyPayloadV2(request->id,
                                    status,
                                    attr,
                                    utf8TargetName,
                                    utf8TargetNameLen,
                                    reply);
      break;
   }

   case HGFS_OP_GETATTR: {
      HgfsReplyGetattr *reply;
      HgfsRequest *request = (HgfsRequest *)packetIn;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      HgfsPackGetattrReplyPayloadV1(request->id, status, attr, reply);
      break;
   }

   default:
      LOG(4, ("%s: Invalid GetAttr op.\n", __FUNCTION__));
      NOT_REACHED();

      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyPayloadV3 --
 *
 *    Packs SearchRead V3 reply payload.
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
HgfsPackSearchReadReplyPayloadV3(HgfsFileAttrInfo *attr,       // IN: attr stucture
                                 const char *utf8Name,         // IN: file name
                                 uint32 utf8NameLen,           // IN: file name length
                                 HgfsReplySearchReadV3 *reply) // OUT: payload
{
   HgfsDirEntry *dirent = (HgfsDirEntry *)reply->payload;

   reply->count = 1;
   reply->reserved = 0;

   dirent->fileName.length = (uint32)utf8NameLen;
   dirent->fileName.flags = 0;
   dirent->fileName.fid = 0;
   dirent->fileName.caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   dirent->nextEntry = 0;

   if (utf8NameLen != 0) {
      memcpy(dirent->fileName.name, utf8Name, utf8NameLen);
      dirent->fileName.name[utf8NameLen] = 0;

      HgfsPackAttrV2(attr, &dirent->attr);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReplyPayloadV2 --
 *
 *    Packs SearchRead V2 reply payload.
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
HgfsPackSearchReadReplyPayloadV2(HgfsFileAttrInfo *attr,       // IN: attr stucture
                                 const char *utf8Name,         // IN: file name
                                 uint32 utf8NameLen,           // IN: file name length
                                 HgfsReplySearchReadV2 *reply) // OUT: payload
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
 * HgfsPackSearchReadReplyPayloadV1 --
 *
 *    Packs SearchRead V1 reply payload.
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
HgfsPackSearchReadReplyPayloadV1(HgfsFileAttrInfo *attr,     // IN: attr stucture
                                 const char *utf8Name,       // IN: file name
                                 uint32 utf8NameLen,         // IN: file name length
                                 HgfsReplySearchRead *reply) // OUT: payload
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
HgfsUnpackSearchReadRequest(const char *packetIn,         // IN: request packet
                            size_t packetSize,            // IN: packet size
                            HgfsFileAttrInfo *attr,       // OUT: unpacked attr struct
                            HgfsHandle *hgfsSearchHandle, // OUT: hgfs search handle
                            uint32 *offset)               // OUT: entry offset
{
   void const *payload;
   size_t payloadSize;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(attr);
   ASSERT(hgfsSearchHandle);
   ASSERT(offset);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, &op)) {
      return FALSE;
   }

   HgfsInitFileAttr(op, attr);


   if (op == HGFS_OP_SEARCH_READ_V3) {
      HgfsRequestSearchReadV3 *request = (HgfsRequestSearchReadV3 *)payload;

      /* Enforced by the dispatch function. */
      ASSERT(payloadSize >= sizeof *request);

      *hgfsSearchHandle = request->search;
      *offset = request->offset;

      LOG(4, ("%s: HGFS_OP_SEARCH_READ_V3\n", __FUNCTION__));
   } else {
      HgfsRequestSearchRead *request = (HgfsRequestSearchRead *)payload;

      /* Enforced by the dispatch function. */
      ASSERT(payloadSize >= sizeof *request);

      *hgfsSearchHandle = request->search;
      *offset = request->offset;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackSearchReadReply --
 *
 *    Pack hgfs search read reply to the HgfsReplySearchRead{V2} structure.
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
HgfsPackSearchReadReply(char const *packetIn,      // IN: incoming packet
                        HgfsInternalStatus status, // IN: reply status
                        const char *utf8Name,      // IN: file name
                        size_t utf8NameLen,        // IN: file name length
                        HgfsFileAttrInfo *attr,    // IN: file attr struct
                        char **packetOut,          // OUT: outgoing packet
                        size_t *packetSize)        // OUT: size of packet
{
   ASSERT(packetIn);

   *packetOut = NULL;
   *packetSize = 0;

   switch (attr->requestType) {
   case HGFS_OP_SEARCH_READ_V3: {
      HgfsReplySearchReadV3 *reply;
      uint32 payloadSize = sizeof *reply + utf8NameLen + sizeof(HgfsDirEntry);

      HgfsAllocInitReply(packetIn, payloadSize, status, packetOut,
                         (void **)&reply, packetSize);
      if (!HgfsValidateReplySize(packetIn, attr->requestType, *packetSize)) {
         free(reply);
         return FALSE;
      }
      HgfsPackSearchReadReplyPayloadV3(attr, utf8Name, utf8NameLen, reply);
      break;
   }

   case HGFS_OP_SEARCH_READ_V2: {
      HgfsReplySearchReadV2 *reply;
      uint32 payloadSize = sizeof *reply + utf8NameLen;

      HgfsAllocInitReply(packetIn, payloadSize, status, packetOut,
                         (void **)&reply, packetSize);
      if (!HgfsValidateReplySize(packetIn, attr->requestType, *packetSize)) {
         free(reply);
         return FALSE;
      }
      HgfsPackSearchReadReplyPayloadV2(attr,
                                       utf8Name,
                                       utf8NameLen,
                                       reply);
      break;
   }

   case HGFS_OP_SEARCH_READ: {
      HgfsReplySearchRead *reply;
      uint32 payloadSize = sizeof *reply + utf8NameLen;

      HgfsAllocInitReply(packetIn, payloadSize, status, packetOut,
                         (void **)&reply, packetSize);
      if (!HgfsValidateReplySize(packetIn, attr->requestType, *packetSize)) {
         free(reply);
         return FALSE;
      }
      HgfsPackSearchReadReplyPayloadV1(attr,
                                       utf8Name,
                                       utf8NameLen,
                                       reply);
      break;
   }

   default: {
      LOG(4, ("%s: Invalid SearchRead Op.", __FUNCTION__));
      NOT_REACHED();
      return FALSE;
   }
   }

   return TRUE;
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
HgfsUnpackSetattrRequest(char const *packetIn,       // IN: request packet
                         size_t packetSize,          // IN: request packet size
                         HgfsFileAttrInfo *attr,     // OUT: setattr info
                         HgfsAttrHint *hints,        // OUT: setattr hints
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: server file ID
                         uint32 *caseType)           // OUT: case-sensitivity flags
{
   void const *payload;
   size_t payloadSize;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(attr);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseType);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, &op)) {
      return FALSE;
   }

   attr->requestType = op;

   /* Default values for legacy requests. */
   *caseType = HGFS_FILE_NAME_DEFAULT_CASE;
   *hints = 0;
   *file = HGFS_INVALID_HANDLE;

   switch (op) {
   case HGFS_OP_SETATTR_V3:
      {
         HgfsRequestSetattrV3 *requestV3 = (HgfsRequestSetattrV3 *)payload;
         if (!HgfsUnpackSetattrPayloadV3(requestV3,
                                         payloadSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints,
                                         file,
                                         caseType)) {
            return FALSE;
         }
         break;
      }

   case HGFS_OP_SETATTR_V2:
      {
         HgfsRequestSetattrV2 *requestV2 = (HgfsRequestSetattrV2 *)payload;
         if (!HgfsUnpackSetattrPayloadV2(requestV2,
                                         payloadSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints,
                                         file)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_SETATTR:
      {
         HgfsRequestSetattr *requestV1 = (HgfsRequestSetattr *)payload;
         if (!HgfsUnpackSetattrPayloadV1(requestV1,
                                         payloadSize,
                                         attr,
                                         cpName,
                                         cpNameSize,
                                         hints)) {
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
HgfsPackSetattrReply(char const *packetIn,      // IN: incoming packet
                     HgfsInternalStatus status, // IN: reply status
                     HgfsOp op,                 // IN: request type
                     char **packetOut,          // OUT: outgoing packet
                     size_t *packetSize)        // OUT: size of packet
{
   Bool result = TRUE;

   ASSERT(packetIn);

   *packetOut = NULL;
   *packetSize = 0;

   switch (op) {
   case HGFS_OP_SETATTR_V3: {
      HgfsReplySetattrV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_SETATTR_V2:
   case HGFS_OP_SETATTR: {
      HgfsReplySetattr *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
      break;
   }

   ASSERT(result);

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
HgfsUnpackCreateDirRequest(char const *packetIn,    // IN: incoming packet
                           size_t packetSize,       // IN: size of packet
                           HgfsCreateDirInfo *info) // IN/OUT: info struct
{
   void const *payload;
   size_t payloadSize;
   HgfsOp op;

   ASSERT(packetIn);
   ASSERT(info);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, &op)) {
      return FALSE;
   }

   info->requestType = op;
   /* Default value for legacy requests. */
   info->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (op) {
   case HGFS_OP_CREATE_DIR_V3:
      {
         HgfsRequestCreateDirV3 *requestV3 = (HgfsRequestCreateDirV3 *)payload;
         if (!HgfsUnpackCreateDirPayloadV3(requestV3,
                                           payloadSize,
                                           info)) {
            return FALSE;
         }
         break;
      }

   case HGFS_OP_CREATE_DIR_V2:
      {
         HgfsRequestCreateDirV2 *requestV2 = (HgfsRequestCreateDirV2 *)payload;
         if (!HgfsUnpackCreateDirPayloadV2(requestV2,
                                           payloadSize,
                                           info)) {
            return FALSE;
         }
         break;
      }
   case HGFS_OP_CREATE_DIR:
      {
         HgfsRequestCreateDir *requestV1 = (HgfsRequestCreateDir *)payload;
         if (!HgfsUnpackCreateDirPayloadV1(requestV1,
                                           payloadSize,
                                           info)) {
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
HgfsPackCreateDirReply(char const *packetIn,      // IN: create dir operation version
                       HgfsInternalStatus status, // IN: reply status
                       HgfsOp op,                 // IN: request type
                       char **packetOut,          // OUT: outgoing packet
                       size_t *packetSize)        // OUT: size of packet
{
   Bool result = TRUE;

   ASSERT(packetIn);

   *packetOut = NULL;
   *packetSize = 0;

   switch (op) {
   case HGFS_OP_CREATE_DIR_V3: {
      HgfsReplyCreateDirV3 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_CREATE_DIR_V2: {
      HgfsReplyCreateDirV2 *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   case HGFS_OP_CREATE_DIR: {
      HgfsReplyCreateDir *reply;

      HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                         (void **)&reply, packetSize);
      break;
   }
   default:
      result = FALSE;
      LOG(4, ("%s: invalid op code %d\n", __FUNCTION__, op));
      NOT_REACHED();
      break;
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
      return FALSE;
   }

   if (payloadSize >= requestV3->requiredSize + sizeof *requestV3) {
      *file = requestV3->file;
      *data = requestV3->payload;
      *dataSize = requestV3->requiredSize;
      *doSecurity = (requestV3->flags & HGFS_WIN32_STREAM_IGNORE_SECURITY) == 0;
      return TRUE;
   }

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
HgfsUnpackWriteWin32StreamRequest(char const *packetIn, // IN: incoming packet
				                      size_t packetSize,    // IN: size of packet
                                  HgfsOp *op,           // OUT: request type
				                      HgfsHandle *file,     // OUT: file to write to
				                      char **data,          // OUT: data to write
				                      size_t *dataSize,     // OUT: size of data
				                      Bool *doSecurity)     // OUT: restore sec.str.
{
   void const *payload;
   size_t payloadSize;

   ASSERT(packetIn);
   ASSERT(file);
   ASSERT(data);
   ASSERT(dataSize);
   ASSERT(doSecurity);

   if (!HgfsParseRequest(packetIn, packetSize, &payload, &payloadSize, op)) {
      return FALSE;
   }

   if (*op != HGFS_OP_WRITE_WIN32_STREAM_V3) {
      /* The only supported version for the moment is V3. */
      NOT_REACHED();
      return FALSE;
   }

   return HgfsUnpackWriteWin32StreamPayloadV3((HgfsRequestWriteWin32StreamV3 *)payload,
                                              payloadSize,
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
HgfsPackWriteWin32StreamReply(char const *packetIn,      // IN: incoming packet
			                     HgfsInternalStatus status, // IN: reply status
                              HgfsOp op,                 // IN: request type
			                     uint32 actualSize,         // IN: amount written
			                     char **packetOut,          // OUT: outgoing packet
			                     size_t *packetSize)        // OUT: size of packet
{
   HgfsReplyWriteWin32StreamV3 *reply;

   ASSERT(packetIn);

   *packetOut = NULL;
   *packetSize = 0;

   if (op != HGFS_OP_WRITE_WIN32_STREAM_V3) {
      NOT_REACHED();
      return FALSE;
   }

   HgfsAllocInitReply(packetIn, sizeof *reply, status, packetOut,
                      (void **)&reply, packetSize);
   reply->actualSize = actualSize;

   return TRUE;
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
 * Hgfs_NotificationCallback --
 *
 *    Callback which is called by directory notification package when in response
 *    to a event.
 *
 *    XXX:
 *    The function must build directory notification packet and send it to the
 *    client. At the moment it just logs a message, actual logic will be
 *    implemented later when required infrastructure is ready.
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
Hgfs_NotificationCallback(SharedFolderHandle sharedFolder,
                          SubscriberHandle subscriber,
                          char* name,
                          char* newName,
                          uint32 mask)
{
    LOG(4, ("%s: notification for folder: %d index: %d file name %s "
            "(new name %s) mask %x\n", __FUNCTION__, sharedFolder,
            (int)subscriber, name, (newName == NULL) ? "" : newName,
            mask));
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
      node = HgfsAddNewFileNode(strdup(tempName), &localId);
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
      search = HgfsAddNewSearch(strdup(tempName));
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
