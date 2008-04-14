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
#include "hgfsServerManager.h"
#include "codeset.h"
#include "config.h"
#include "file.h"
#include "util.h"
#include "wiper.h"
#include "syncMutex.h"

#if defined(_WIN32)
#include <io.h>
#else 
#include <unistd.h>
#define stricmp strcasecmp
#endif // _WIN32

#define LOGLEVEL_MODULE hgfs
#include "loglevel_user.h"


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

static HgfsFileNode *nodeArray = NULL;
uint32 numNodes;

/* Free list of file nodes. LIFO to be cache-friendly */
DblLnkLst_Links nodeFreeList;

#define NUM_SEARCHES 100

static HgfsSearch *searchArray = NULL;
uint32 numSearches;

/* Free list of searches. LIFO to be cache-friendly */
DblLnkLst_Links searchFreeList;

/* Default maximum number of open nodes. */
#define MAX_CACHED_FILENODES 30

/* Default maximun number of open nodes that have server locks. */
#define MAX_LOCKED_FILENODES 10

/* Maximum number of cached open nodes. */
static unsigned int maxCachedOpenNodes;

/* List of cached open nodes. */
static DblLnkLst_Links nodeCachedList;

/* Current number of open nodes. */
static unsigned int numCachedOpenNodes;

/* Current number of open nodes that have server locks. */
static unsigned int numCachedLockedNodes;

/* Value of config option to require using host timestamps */
Bool alwaysUseHostTime = FALSE;

/* 
 * Monotonically increasing handle counter used to dish out HgfsHandles. Not
 * static so that it can be used in the VMX checkpointing code.
 */
uint32 hgfsHandleCounter = 0;

/*
 * HGFS requests can now be executed asynchronously. As a result, we need to
 * protect our shared data structures: the node and search caches.
 *
 * We also need to protect certain cases of read/write IO. Where possible,
 * we try to combine the seek and read/write steps into one atomic syscall or
 * win32 API call. But in some cases (Win9x, non-Linux server), the two steps
 * are split, and thus are not atomic. We need a lock to protect against those
 * cases.
 */
static SyncMutex hgfsNodeArrayLock;
static SyncMutex hgfsSearchArrayLock;

/* Not static because it's used in per-platform server code. */
SyncMutex hgfsIOLock;

/* Local functions. */

static Bool HgfsAddToCacheInternal(HgfsHandle handle);
static Bool HgfsIsCachedInternal(HgfsHandle handle);
static Bool HgfsRemoveLruNode(void);
static Bool HgfsRemoveFromCacheInternal(HgfsHandle handle);
static void HgfsRemoveSearchInternal(HgfsSearch *search);
static HgfsSearch *HgfsSearchHandle2Search(HgfsHandle handle);
static HgfsHandle HgfsSearch2SearchHandle(HgfsSearch const *search);
static HgfsSearch *HgfsAddNewSearch(char const *utf8Dir,
                                    DirectorySearchType type);


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsHandle2FileNode --
 *
 *    Retrieve the file node a handle refers to.
 *
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsHandle2FileNode(HgfsHandle handle)    // IN: Hgfs file handle
{
   unsigned int i;
   HgfsFileNode *fileNode = NULL;

   ASSERT(nodeArray);

   /* XXX: This O(n) lookup can and should be optimized. */
   for (i = 0; i < numNodes; i++) {
      if (nodeArray[i].state != FILENODE_STATE_UNUSED &&
          nodeArray[i].handle == handle) {
         fileNode = &nodeArray[i];
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsDumpAllNodes(void)
{
   unsigned int i;

   Log("Dumping all nodes\n");
   for (i = 0; i < numNodes; i++) {
      Log("handle %u, name \"%s\", localdev %u, localInum %"FMT64"u %u\n",
          nodeArray[i].handle,
          nodeArray[i].utf8Name ? nodeArray[i].utf8Name : "NULL",
          nodeArray[i].localId.volumeId,
          nodeArray[i].localId.fileId,
          nodeArray[i].fileDesc);
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
HgfsHandle2FileDesc(HgfsHandle handle,    // IN: Hgfs file handle
                    fileDesc *fd)         // OUT: OS handle (file descriptor)
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle);
   if (fileNode == NULL) {
      goto exit;
   }

   *fd = fileNode->fileDesc;
   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
HgfsHandle2AppendFlag(HgfsHandle handle,   // IN: Hgfs file handle
                      Bool *appendFlag)    // OUT: append flag
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle);
   if (fileNode == NULL) {
      goto exit;
   }

   *appendFlag = fileNode->flags & FILE_NODE_APPEND_FL;
   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
HgfsHandle2LocalId(HgfsHandle handle,     // IN: Hgfs file handle
                   HgfsLocalId *localId)  // OUT: local id info
{
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   ASSERT(localId);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle);
   if (fileNode == NULL) {
      goto exit;
   }

   localId->volumeId = fileNode->localId.volumeId;
   localId->fileId = fileNode->localId.fileId;

   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
HgfsHandle2ServerLock(HgfsHandle handle,     // IN: Hgfs file handle
                      HgfsServerLock *lock)  // OUT: Server lock
{
#ifdef HGFS_OPLOCKS
   Bool found = FALSE;
   HgfsFileNode *fileNode = NULL;

   ASSERT(lock);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   fileNode = HgfsHandle2FileNode(handle);
   if (fileNode == NULL) {
      goto exit;
   }

   *lock = fileNode->serverLock;
   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
HgfsFileDesc2Handle(fileDesc fd,          // IN: OS handle (file descriptor)
                    HgfsHandle *handle)   // OUT: Hgfs file handle
{
   unsigned int i;
   Bool found = FALSE;
   HgfsFileNode *existingFileNode = NULL;
   
   SyncMutex_Lock(&hgfsNodeArrayLock);
   for (i = 0; i < numNodes; i++) {
      existingFileNode = &nodeArray[i];
      if ((existingFileNode->state == FILENODE_STATE_IN_USE_CACHED) &&
          (existingFileNode->fileDesc == fd)) {
         *handle = HgfsFileNode2Handle(existingFileNode);
         found = TRUE;
         break;
      }
   }

   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
                     HgfsOpenMode *shareMode)   // OUT:share access mode
{
   Bool found = FALSE;
   HgfsFileNode *existingFileNode = NULL;
   HgfsNameStatus nameStatus;

   if (shareMode == NULL) {
      return found;
   }

   SyncMutex_Lock(&hgfsNodeArrayLock);
   existingFileNode = HgfsHandle2FileNode(handle);
   if (existingFileNode == NULL) {
      goto exit_unlock;
   }
   
   nameStatus = HgfsServerPolicy_GetShareMode(existingFileNode->shareName,
                                              existingFileNode->shareNameLen,
                                              shareMode);
   found = (nameStatus == HGFS_NAME_STATUS_COMPLETE);

exit_unlock:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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

   SyncMutex_Lock(&hgfsNodeArrayLock);
   existingFileNode = HgfsHandle2FileNode(handle);
   if (existingFileNode == NULL) {
      goto exit_unlock;
   }

   name = malloc(existingFileNode->utf8NameLen + 1);
   if (name == NULL) {
      goto exit_unlock;
   }
   nameSize = existingFileNode->utf8NameLen; 
   memcpy(name, existingFileNode->utf8Name, nameSize);
   name[nameSize] = '\0';
   found = TRUE;

exit_unlock:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
                      HgfsServerLock *serverLock,       // OUT: Existing oplock
                      fileDesc   *fileDesc)             // OUT: Existing fd
{
#ifdef HGFS_OPLOCKS
   unsigned int i;
   Bool found = FALSE;
   ASSERT(utf8Name);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   for (i = 0; i < numNodes; i++) {
      HgfsFileNode *existingFileNode = &nodeArray[i];
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

   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
                Bool copyName,            // IN: Should we copy the name?
                HgfsFileNode *copy)       // IN/OUT: Copy of the node
{
   HgfsFileNode *original = NULL;
   Bool found = FALSE;

   ASSERT(copy);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   original = HgfsHandle2FileNode(handle);
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
   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
HgfsHandleIsSequentialOpen(HgfsHandle handle,     // IN:  Hgfs file handle
                           Bool *sequentialOpen)  // OUT: If open was sequential
{
   HgfsFileNode *node;
   Bool success = FALSE;

   ASSERT(sequentialOpen);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      goto exit;
   }

   *sequentialOpen = node->flags & FILE_NODE_SEQUENTIAL_FL;
   success = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
HgfsHandleIsSharedFolderOpen(HgfsHandle handle,       // IN:  Hgfs file handle
                             Bool *sharedFolderOpen)  // OUT: If shared folder
{
   HgfsFileNode *node;
   Bool success = FALSE;

   ASSERT(sharedFolderOpen);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      goto exit;
   }

   *sharedFolderOpen = node->flags & FILE_NODE_SHARED_FOLDER_OPEN_FL;
   success = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
HgfsUpdateNodeFileDesc(HgfsHandle handle, // IN: Hgfs file handle
                       fileDesc fd)       // OUT: OS handle (file desc)
{
   HgfsFileNode *node;
   Bool updated = FALSE;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      goto exit;
   }

   node->fileDesc = fd;
   updated = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
                         HgfsServerLock serverLock)  // IN: new oplock
{
   unsigned int i;
   HgfsFileNode *existingFileNode = NULL;
   Bool updated = FALSE;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   for (i = 0; i < numNodes; i++) {
      existingFileNode = &nodeArray[i];
      if (existingFileNode->state != FILENODE_STATE_UNUSED) {
         if (existingFileNode->fileDesc == fd) {
            existingFileNode->serverLock = serverLock;
            updated = TRUE;
            break;
         }
      }
   }

   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
HgfsUpdateNodeAppendFlag(HgfsHandle handle, // IN: Hgfs file handle
                         Bool appendFlag)   // OUT: Append flag
{
   HgfsFileNode *node;
   Bool updated = FALSE;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      goto exit;
   }

   if (appendFlag) {
      node->flags |= FILE_NODE_APPEND_FL;
   }
   updated = TRUE;

exit:
   SyncMutex_Unlock(&hgfsNodeArrayLock);
   return updated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsDumpAllSearches --
 *
 *    Debugging routine; print all searches in the searchArray.
 *
 *    Caller should hold hgfsSearchArrayLock.
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
HgfsDumpAllSearches(void)
{
   unsigned int i;

   Log("Dumping all searches\n");
   for (i = 0; i < numSearches; i++) {
      Log("handle %u, baseDir \"%s\"\n", 
          searchArray[i].handle, 
          searchArray[i].utf8Dir ? 
          searchArray[i].utf8Dir : "(NULL)");
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsGetNewNode(void)
{
   HgfsFileNode *node;
   HgfsFileNode *newMem;
   unsigned int newNumNodes;
   unsigned int i;

   LOG(4, ("HgfsGetNewNode: entered\n"));

   if (nodeFreeList.next == &nodeFreeList) {
      unsigned int ptrDiff;

      if (DOLOG(4)) {
         Log("Dumping nodes before realloc\n");
         HgfsDumpAllNodes();
      }

      /* Try to get twice as much memory as we had */
      newNumNodes = 2 * numNodes;
      newMem = (HgfsFileNode *)realloc(nodeArray, 
                                       newNumNodes * sizeof *nodeArray);
      if (!newMem) {
         LOG(4, ("HgfsGetNewNode: can't realloc more nodes\n"));
         return NULL;
      }

      ptrDiff = (char *)newMem - (char *)nodeArray;
      if (ptrDiff) {
         unsigned int const oldSize = numNodes * sizeof *nodeArray;

         /*
          * The portion of memory that contains all our file nodes moved.
          * All pointers that pointed inside the previous portion of memory
          * must be updated to point to the new portion of memory.
          *
          * We'll need to lock this if we multithread.
          */
         LOG(4, ("Rebasing pointers, diff is %d, sizeof node is %"FMTSZ"u\n",
                  ptrDiff, sizeof(HgfsFileNode)));
         LOG(4, ("old: %p new: %p\n", nodeArray, newMem));
         ASSERT(newMem == (HgfsFileNode*)((char*)nodeArray + ptrDiff));

#define HgfsServerRebase(_ptr, _type)                                         \
   if ((unsigned int)((char *)_ptr - (char *)nodeArray) < oldSize) {          \
      _ptr = (_type *)((char *)_ptr + ptrDiff);                               \
   }

         /*
          * Rebase the links of all file nodes
          */
         for (i = 0; i < numNodes; i++) {
            HgfsServerRebase(newMem[i].links.prev, DblLnkLst_Links)
            HgfsServerRebase(newMem[i].links.next, DblLnkLst_Links)
         }

         /*
          * There is no need to rebase the anchor of the file node free list
          * because if we are here, it is empty.
          */

         /* Rebase the anchor of the cached file nodes list. */
         HgfsServerRebase(nodeCachedList.prev, DblLnkLst_Links)
         HgfsServerRebase(nodeCachedList.next, DblLnkLst_Links)

#undef HgfsServerRebase
      }

      /* Initialize the new nodes */
      LOG(4, ("numNodes was %u, now is %u\n", numNodes, newNumNodes));
      for (i = numNodes; i < newNumNodes; i++) {
         DblLnkLst_Init(&newMem[i].links);

         newMem[i].state = FILENODE_STATE_UNUSED;
         newMem[i].utf8Name = NULL;
         newMem[i].utf8NameLen = 0;

         /* Append at the end of the list */
         DblLnkLst_LinkLast(&nodeFreeList, &newMem[i].links);
      }
      nodeArray = newMem;
      numNodes = newNumNodes;

      if (DOLOG(4)) {
         Log("Dumping nodes after pointer changes\n");
         HgfsDumpAllNodes();
      }
   }

   /* Remove the first item from the list */
   node = DblLnkLst_Container(nodeFreeList.next, HgfsFileNode, links);
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsRemoveFileNode(HgfsFileNode *node) // IN
{
   ASSERT(node);

   LOG(4, ("HgfsRemoveFileNode: handle %u, name %s, fileId %"FMT64"u\n",
           HgfsFileNode2Handle(node),
           node->utf8Name,
           node->localId.fileId));

   if (node->shareName) {
      free(node->shareName);
   }
   node->shareName = NULL;

   if (node->utf8Name) {
      free(node->utf8Name);
   }
   node->utf8Name = NULL;
   node->state = FILENODE_STATE_UNUSED;

   /* Prepend at the beginning of the list */
   DblLnkLst_LinkFirst(&nodeFreeList, &node->links);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsFreeFileNodeInternal --
 *
 *    Free its localname, clear its fields, return it to the free list.
 *
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsFreeFileNodeInternal(HgfsHandle handle) // IN: Handle to free
{
   HgfsFileNode *node = HgfsHandle2FileNode(handle);
   ASSERT(node);
   HgfsRemoveFileNode(node);
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
HgfsFreeFileNode(HgfsHandle handle) // IN: Handle to free
{
   SyncMutex_Lock(&hgfsNodeArrayLock);
   HgfsFreeFileNodeInternal(handle);
   SyncMutex_Unlock(&hgfsNodeArrayLock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddNewFileNode --
 *
 *    Gets a free node off the free list, sets its name, localId info, 
 *    file descriptor and permissions.
 * 
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
                   Bool sharedFolderOpen)       // IN: shared folder only open
{
   HgfsFileNode *newNode;

   ASSERT(openInfo);
   ASSERT(localId);

   /* This was already verified in HgfsUnpackOpenRequest... */
   ASSERT(openInfo->mask & HGFS_OPEN_VALID_FILE_NAME);

   /* Get an unused node */
   newNode = HgfsGetNewNode();
   if (!newNode) {
      LOG(4, ("HgfsAddNewFileNode: out of memory\n"));
      return NULL;
   }

   /* Set new node's fields */
   if (!HgfsServerGetOpenMode(openInfo, &newNode->mode)) {
      HgfsRemoveFileNode(newNode);
      return NULL;
   }

   /* 
    * Save a copy of the share name so we can look up its
    * access mode at various times over the node's lifecycle.
    */
   newNode->shareName = malloc(shareNameLen + 1);
   if (newNode->shareName == NULL) {
      LOG(4, ("HgfsAddNewFileNode: out of memory\n"));
      HgfsRemoveFileNode(newNode);
      return NULL;
   }
   memcpy(newNode->shareName, shareName, shareNameLen);
   newNode->shareName[shareNameLen] = '\0';
   newNode->shareNameLen = shareNameLen;

   newNode->utf8NameLen = strlen(openInfo->utf8Name);
   newNode->utf8Name = malloc(newNode->utf8NameLen + 1);
   if (newNode->utf8Name == NULL) {
      LOG(4, ("HgfsAddNewFileNode: out of memory\n"));
      HgfsRemoveFileNode(newNode);
      return NULL;
   }
   memcpy(newNode->utf8Name, openInfo->utf8Name, newNode->utf8NameLen);
   newNode->utf8Name[newNode->utf8NameLen] = '\0';

   newNode->handle = hgfsHandleCounter++;
   newNode->localId = *localId;
   newNode->fileDesc = fileDesc;
   newNode->shareAccess = (openInfo->mask & HGFS_OPEN_VALID_SHARE_ACCESS) ?
      openInfo->shareAccess : HGFS_DEFAULT_SHARE_ACCESS;
   newNode->flags = 0;
   if (append) {
      newNode->flags |= FILE_NODE_APPEND_FL;
   }
   if (sharedFolderOpen) {
      newNode->flags |= FILE_NODE_SHARED_FOLDER_OPEN_FL;
   }
   if (HGFS_OPEN_MODE_FLAGS(openInfo->mode) & HGFS_OPEN_SEQUENTIAL) {
      newNode->flags |= FILE_NODE_SEQUENTIAL_FL; 
   }

   newNode->serverLock = openInfo->acquiredLock;
   newNode->state = FILENODE_STATE_IN_USE_NOT_CACHED;

   LOG(4, ("HgfsAddNewFileNode: got new node, handle %u\n",
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsAddToCacheInternal(HgfsHandle handle) // IN: HGFS file handle
{
   HgfsFileNode *node;

   /* Check if the node is already cached. */
   if (HgfsIsCachedInternal(handle)) {
      ASSERT((node = HgfsHandle2FileNode(handle)) && 
             node->state == FILENODE_STATE_IN_USE_CACHED);
      return TRUE;
   }

   /* Remove the LRU node if the list is full. */
   if (numCachedOpenNodes == maxCachedOpenNodes) {
      if (!HgfsRemoveLruNode()) {
         LOG(4, ("HgfsAddToCacheInternal: Unable to remove LRU node from cache.\n"));
         return FALSE;
      }
   }

   ASSERT_BUG(36244, numCachedOpenNodes < maxCachedOpenNodes);

   node = HgfsHandle2FileNode(handle);
   ASSERT(node);
   /* Append at the end of the list. */
   DblLnkLst_LinkLast(&nodeCachedList, &node->links);

   node->state = FILENODE_STATE_IN_USE_CACHED;
   numCachedOpenNodes++;

   /* 
    * Keep track of how many open nodes we have with
    * server locks on them. The locked file should
    * always be present in the node cache. So we keep
    * the number of the files that have locks on them
    * limited, and smaller than the number of maximum
    * nodes in the cache.
    */

   if (node->serverLock != HGFS_LOCK_NONE) {
      numCachedLockedNodes++;
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsRemoveFromCacheInternal(HgfsHandle handle)	// IN: Hgfs handle to the node
{
   HgfsFileNode *node;

   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      LOG(4, ("HgfsRemoveFromCacheInternal: invalid handle.\n"));
      return FALSE;
   }

   if (node->state == FILENODE_STATE_IN_USE_CACHED) {
      /* Unlink the node from the list of cached fileNodes. */
      DblLnkLst_Unlink1(&node->links);
      node->state = FILENODE_STATE_IN_USE_NOT_CACHED;
      numCachedOpenNodes--;

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
      if (HgfsCloseFile(node->fileDesc)) {
         LOG(4, ("HgfsRemoveFromCacheInternal: Could not close fd %u\n", 
                 node->fileDesc));
         return FALSE;
      }

     /*
      * If we have just removed the node then the number of used nodes better
      * be less than the max. If we didn't remove a node, it means the
      * node we tried to remove was not in the cache to begin with, and
      * we have a problem (see bug 36244).
      */

      ASSERT(numCachedOpenNodes < maxCachedOpenNodes);
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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsIsCachedInternal(HgfsHandle handle) // IN: Structure representing file node
{
   HgfsFileNode *node;

   node = HgfsHandle2FileNode(handle);
   if (node == NULL) {
      LOG(4, ("HgfsIsCached: invalid handle.\n"));
      return FALSE;
   }
   if (node->state == FILENODE_STATE_IN_USE_CACHED) {
      /*
       * Move this node to the end of the list.
       */
      DblLnkLst_Unlink1(&node->links);
      DblLnkLst_LinkLast(&nodeCachedList, &node->links);
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
HgfsIsServerLockAllowed()
{
   Bool allowed;
   
   SyncMutex_Lock(&hgfsNodeArrayLock);
   allowed = numCachedLockedNodes < MAX_LOCKED_FILENODES;
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
 *    Caller should hold hgfsSearchArrayLock.
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
HgfsGetNewSearch(void)
{
   HgfsSearch *search;
   HgfsSearch *newMem;
   unsigned int newNumSearches;
   unsigned int i;

   LOG(4, ("HgfsGetNewSearch: entered\n"));

   if (searchFreeList.next == &searchFreeList) {
      unsigned int ptrDiff;

      if (DOLOG(4)) {
         Log("Dumping searches before realloc\n");
         HgfsDumpAllSearches();
      }

      /* Try to get twice as much memory as we had */
      newNumSearches = 2 * numSearches;
      newMem = (HgfsSearch *)realloc(searchArray, 
                                     newNumSearches * sizeof *searchArray);
      if (!newMem) {
         LOG(4, ("HgfsGetNewSearch: can't realloc more searches\n"));
         return NULL;
      }

      ptrDiff = (char *)newMem - (char *)searchArray;
      if (ptrDiff) {
         unsigned int const oldSize = numSearches * sizeof *searchArray;

         /*
          * The portion of memory that contains all our searches moved.
          * All pointers that pointed inside the previous portion of memory
          * must be updated to point to the new portion of memory.
          */

         LOG(4, ("Rebasing pointers, diff is %d, sizeof search is %"FMTSZ"u\n",
                 ptrDiff, sizeof(HgfsSearch)));
         LOG(4, ("old: %p new: %p\n", searchArray, newMem));
         ASSERT(newMem == (HgfsSearch*)((char*)searchArray + ptrDiff));

#define HgfsServerRebase(_ptr, _type)                                         \
   if ((unsigned int)((char *)_ptr - (char *)searchArray) < oldSize) {        \
      _ptr = (_type *)((char *)_ptr + ptrDiff);                               \
   }

         /*
          * Rebase the links of all searches
          */

         for (i = 0; i < numSearches; i++) {
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
      LOG(4, ("numSearches was %u, now is %u\n", numSearches, newNumSearches));
      for (i = numSearches; i < newNumSearches; i++) {
         DblLnkLst_Init(&newMem[i].links);
         newMem[i].utf8Dir = NULL;
         newMem[i].utf8DirLen = 0;
         newMem[i].dents = NULL;
         newMem[i].numDents = 0;

         /* Append at the end of the list */
         DblLnkLst_LinkLast(&searchFreeList, &newMem[i].links);
      }
      searchArray = newMem;
      numSearches = newNumSearches;

      if (DOLOG(4)) {
         Log("Dumping searches after pointer changes\n");
         HgfsDumpAllSearches();
      }
   }

   /* Remove the first item from the list */
   search = DblLnkLst_Container(searchFreeList.next, HgfsSearch, links);
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
 *    Caller should hold hgfsSearchArrayLock.
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
                  HgfsSearch *copy)         // IN/OUT: Copy of the search
{
   HgfsSearch *original = NULL;
   Bool found = FALSE;

   ASSERT(copy);

   SyncMutex_Lock(&hgfsSearchArrayLock);
   original = HgfsSearchHandle2Search(handle);
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

   /* No dents for the copy, they consume too much memory and aren't needed. */
   copy->dents = NULL;
   copy->numDents = 0;

   copy->handle = original->handle;
   copy->type = original->type;
   found = TRUE;

exit:
   SyncMutex_Unlock(&hgfsSearchArrayLock);
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
 *    Caller should hold hgfsSearchArrayLock.
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
HgfsAddNewSearch(char const *utf8Dir,      // IN: UTF8 name of dir to search in
                 DirectorySearchType type) // IN: What kind of search is this?
{
   HgfsSearch *newSearch;

   ASSERT(utf8Dir);

   /* Get an unused search */
   newSearch = HgfsGetNewSearch();
   if (!newSearch) {
      LOG(4, ("HgfsAddNewSearch: out of memory\n"));
      return NULL;
   }

   newSearch->dents = NULL;
   newSearch->numDents = 0;
   newSearch->type = type;
   newSearch->handle = hgfsHandleCounter++;

   newSearch->utf8DirLen = strlen(utf8Dir);
   newSearch->utf8Dir = malloc(newSearch->utf8DirLen + 1);
   if (newSearch->utf8Dir == NULL) {
      HgfsRemoveSearchInternal(newSearch);
      return NULL;
   }
   memcpy(newSearch->utf8Dir, utf8Dir, newSearch->utf8DirLen);
   newSearch->utf8Dir[newSearch->utf8DirLen] = '\0';

   LOG(4, ("HgfsAddNewSearch: got new search, handle %u\n",
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
 *    Caller should hold hgfsSearchArrayLock.
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
HgfsRemoveSearchInternal(HgfsSearch *search) // IN
{
   ASSERT(search);

   LOG(4, ("HgfsRemoveSearchInternal: handle %u, dir %s\n",
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

   /* Prepend at the beginning of the list */
   DblLnkLst_LinkFirst(&searchFreeList, &search->links);
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
HgfsRemoveSearch(HgfsHandle handle) // IN
{
   HgfsSearch *search;
   Bool success = FALSE;

   SyncMutex_Lock(&hgfsSearchArrayLock);
   search = HgfsSearchHandle2Search(handle);
   if (search != NULL) {
      HgfsRemoveSearchInternal(search);
      success = TRUE;
   }
   SyncMutex_Unlock(&hgfsSearchArrayLock);

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
HgfsGetSearchResult(HgfsHandle handle, // IN: Handle to search
                    uint32 offset,     // IN: Offset to retrieve at
                    Bool remove)       // IN: If true, removes the result
{
   HgfsSearch *search;
   DirectoryEntry *dent = NULL;

   SyncMutex_Lock(&hgfsSearchArrayLock);   
   search = HgfsSearchHandle2Search(handle);
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
      ASSERT(originalDent->d_name + nameLen <
             (char *)originalDent + originalDent->d_reclen);

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
   SyncMutex_Unlock(&hgfsSearchArrayLock);
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
 *    Caller should hold hgfsSearchArrayLock.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

HgfsSearch *
HgfsSearchHandle2Search(HgfsHandle handle) // IN
{
   unsigned int i;
   HgfsSearch *search = NULL;

   ASSERT(searchArray);
  
   /* XXX: This O(n) lookup can and should be optimized. */
   for (i = 0; i < numSearches; i++) {
      if (!DblLnkLst_IsLinked(&searchArray[i].links) &&
          searchArray[i].handle == handle) {
         search = &searchArray[i];
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
                    const char *newLocalName)  // IN: Name to replace with
{
   HgfsFileNode *fileNode;
   unsigned int i;
   char *newBuffer;
   size_t newBufferLen;

   ASSERT(oldLocalName);
   ASSERT(newLocalName);

   newBufferLen = strlen(newLocalName);

   SyncMutex_Lock(&hgfsNodeArrayLock);
   ASSERT(nodeArray);
   for (i = 0; i < numNodes; i++) {
      fileNode = &nodeArray[i];

      /* If the node is on the free list, skip it. */
      if (fileNode->state == FILENODE_STATE_UNUSED) {
         continue;
      }

      if (strcmp(fileNode->utf8Name, oldLocalName) == 0) {
         newBuffer = malloc(newBufferLen + 1);
         if (!newBuffer) {
            LOG(4, ("HgfsUpdateNodeNames: Failed to update a node name.\n"));
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
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
HgfsServerClose(char const *packetIn, // IN: incoming packet
                char *packetOut,      // OUT: outgoing packet
                size_t *packetSize)   // IN/OUT: size of packet
{
   HgfsRequestClose *request;
   HgfsReplyClose *reply;

   request = (HgfsRequestClose *)packetIn;
   ASSERT(request);
   reply = (HgfsReplyClose *)packetOut;
   ASSERT(reply);
   ASSERT(packetSize);

   LOG(4, ("HgfsServerClose: close fh %u\n", request->file));

   if (!HgfsRemoveFromCache(request->file)) {
      LOG(4, ("HgfsServerClose: Could not remove the node from cache.\n"));
      return HGFS_INTERNAL_STATUS_ERROR;
   } else {
      HgfsFreeFileNode(request->file);
   }

   *packetSize = sizeof *reply;
   return 0;
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
HgfsServerSearchClose(char const *packetIn, // IN: incoming packet
                      char *packetOut,      // OUT: outgoing packet
                      size_t *packetSize)   // IN/OUT: size of packet
{
   HgfsRequestSearchClose *request;
   HgfsReplySearchClose *reply;

   request = (HgfsRequestSearchClose *)packetIn;
   ASSERT(request);
   reply = (HgfsReplySearchClose *)packetOut;
   ASSERT(reply);
   ASSERT(packetSize);

   LOG(4, ("HgfsServerSearchClose: close search #%u\n", request->search));

   if (!HgfsRemoveSearch(request->search)) {
      /* Invalid handle */
      LOG(4, ("HgfsServerSearchClose: invalid handle %u\n", request->search));
      return HGFS_INTERNAL_STATUS_ERROR;
   }

   *packetSize = sizeof *reply;
   return 0;
}


/* Opcode handlers, indexed by opcode */
static struct {
   HgfsInternalStatus
   (*handler)(char const *packetIn,
              char *packetOut,
              size_t *packetSize);

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

   { HgfsServerOpen,             (sizeof (HgfsRequestOpenV3) + sizeof (HgfsRequest))          },
   { HgfsServerRead,             (sizeof (HgfsRequestReadV3) + sizeof (HgfsRequest))          },
   { HgfsServerWrite,            (sizeof (HgfsRequestWriteV3) + sizeof (HgfsRequest))         },
   { HgfsServerClose,            (sizeof (HgfsRequestCloseV3) + sizeof (HgfsRequest))         },
   { HgfsServerSearchOpen,       (sizeof (HgfsRequestSearchOpenV3) + sizeof (HgfsRequest))    },
   { HgfsServerSearchRead,       (sizeof (HgfsRequestSearchReadV3) + sizeof (HgfsRequest))    },
   { HgfsServerSearchClose,      (sizeof (HgfsRequestSearchCloseV3) + sizeof (HgfsRequest))   },
   { HgfsServerGetattr,          (sizeof (HgfsRequestGetattrV3) + sizeof (HgfsRequest))       },
   { HgfsServerSetattr,          (sizeof (HgfsRequestSetattrV3) + sizeof (HgfsRequest))       },
   { HgfsServerCreateDir,        (sizeof (HgfsRequestCreateDirV3) + sizeof (HgfsRequest))     },
   { HgfsServerDeleteFile,       (sizeof (HgfsRequestDeleteV3) + sizeof (HgfsRequest))        },
   { HgfsServerDeleteDir,        (sizeof (HgfsRequestDeleteV3) + sizeof (HgfsRequest))        },
   { HgfsServerRename,           (sizeof (HgfsRequestRenameV3) + sizeof (HgfsRequest))        },
   { HgfsServerQueryVolume,      (sizeof (HgfsRequestQueryVolumeV3) + sizeof (HgfsRequest))   },
   { HgfsServerSymlinkCreate,    (sizeof (HgfsRequestSymlinkCreateV3) + sizeof (HgfsRequest)) },
   { HgfsServerServerLockChange, sizeof (HgfsRequestServerLockChange)  },

};



/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_DispatchPacket --
 *
 *    Dispatch an incoming packet (in packetIn) to a handler function.
 *
 *    The handler function should place a reply packet in
 *    packetOut. packetSize contains the size of the incoming
 *    packet initially, and the handler function should reset it to
 *    the size of the outgoing packet before returning. The same
 *    buffer can be used for both packetIn and packetOut.
 *
 *    This function cannot fail; if something goes wrong, it returns
 *    a packet containing only a reply header with error code.
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
HgfsServer_DispatchPacket(char const *packetIn, // IN:     Incoming request packet
                          char *packetOut,      // OUT:    Outgoing reply packet
                          size_t *packetSize)   // IN/OUT: Size of packet
{
   HgfsRequest *request;
   HgfsReply *reply;
   HgfsHandle id;
   HgfsOp op;
   HgfsStatus status;

   request = (HgfsRequest *)packetIn;
   ASSERT(request);
   reply = (HgfsReply *)packetOut;
   ASSERT(reply);
   ASSERT(packetSize);

   if (*packetSize < sizeof *request) {
      /*
       * The input packet is smaller than a request. Because we can't read the
       * request ID, we can't send a reply. We can only drop the request on the
       * floor.
       */
      *packetSize = 0;
      return;
   }

   id = request->id;
   op = request->op;

   if (op < sizeof handlers / sizeof handlers[0]) {
      if (*packetSize >= handlers[op].minReqSize) {
         HgfsInternalStatus internalStatus;
         internalStatus = (*handlers[op].handler)(packetIn, 
                                                  packetOut, 
                                                  packetSize);
         status = HgfsConvertFromInternalStatus(internalStatus);
      } else {
         /*
          * The input packet is smaller than the minimal size needed for the
          * operation.
          */
         status = HGFS_STATUS_PROTOCOL_ERROR;
      }
   } else {
      /* Unknown opcode */
      status = HGFS_STATUS_PROTOCOL_ERROR;
   }

   /*
    * If the status isn't success, set the packetSize to the
    * size of the reply struct. This saves handler functions
    * from having to bother setting packetSize on error paths,
    * and minimizes the number of bytes copied in error cases.
    */
   if (status != HGFS_STATUS_SUCCESS) {
      *packetSize = sizeof *reply;
   }

   ASSERT(*packetSize >= sizeof *reply && *packetSize <= HGFS_PACKET_MAX);
   reply->id = id;
   reply->status = status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_InvalidateObjects --
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

void
HgfsServer_InvalidateObjects(DblLnkLst_Links *shares) // IN: List of new shares
{
   unsigned int i;

   ASSERT(shares);
   LOG(4, ("HgfsServer_InvalidateObjects: Beginning\n"));

   /* 
    * Iterate over each node, skipping those that are unused. For each node,
    * if its filename is no longer within a share, remove it.
    */
   SyncMutex_Lock(&hgfsNodeArrayLock);
   for (i = 0; i < numNodes; i++) {
      HgfsHandle handle;
      DblLnkLst_Links *l;

      if (nodeArray[i].state == FILENODE_STATE_UNUSED) {
         continue;
      }
       
      handle = HgfsFileNode2Handle(&nodeArray[i]);
      LOG(4, ("HgfsServer_InvalidateObjects: Examining node with fd %d (%s)\n",
              handle, nodeArray[i].utf8Name));
      
      /* 
       * For each share, is the node within the share? The answer is yes if the
       * share's path is a prefix for the node's path. To make sure we don't 
       * get any false positives, check for a path separator (or nul 
       * terminator) right after the matched prefix.
       */
      for (l = shares->next; l != shares; l = l->next) {
         HgfsSharedFolder *share;

         share = DblLnkLst_Container(l, HgfsSharedFolder, links);
         ASSERT(share);
         if ((strncmp(nodeArray[i].utf8Name, share->path, share->pathLen) == 0)
             && (*(nodeArray[i].utf8Name + share->pathLen) == DIRSEPC || 
                 *(nodeArray[i].utf8Name + share->pathLen) == '\0')) {
            LOG(4, ("HgfsServer_InvalidateObjects: Node is still valid\n"));
            break;
         }
      }
      
      /* If the node wasn't found in any share, remove it. */
      if (l == shares) {
         LOG(4, ("HgfsServer_InvalidateObjects: Node is invalid, removing\n"));
         if (!HgfsRemoveFromCacheInternal(handle)) {
            LOG(4, ("HgfsServer_InvalidateObjects: Could not remove node with "
                    "fh %d from the cache.\n", handle));
         } else {
            HgfsFreeFileNodeInternal(handle);
         }
      }
   }
   SyncMutex_Unlock(&hgfsNodeArrayLock);

   /* 
    * Iterate over each search, skipping those that are on the free list. For 
    * each search, if its base name is no longer within a share, remove it.
    */
   SyncMutex_Lock(&hgfsSearchArrayLock);
   for (i = 0; i < numSearches; i++) {
      HgfsHandle handle;
      DblLnkLst_Links *l;

      if (DblLnkLst_IsLinked(&searchArray[i].links)) {
         continue;
      }

      handle = HgfsSearch2SearchHandle(&searchArray[i]);
      LOG(4, ("HgfsServer_InvalidateObjects: Examining search (%s)\n",
              searchArray[i].utf8Dir));

      /* 
       * For each share, is the search within the share? We apply the same 
       * heuristic as was used for the nodes above.
       */
      for (l = shares->next; l != shares; l = l->next) {
         HgfsSharedFolder *share;

         share = DblLnkLst_Container(l, HgfsSharedFolder, links);
         ASSERT(share);
         if ((strncmp(searchArray[i].utf8Dir, share->path, share->pathLen) == 0)
             && (*(searchArray[i].utf8Dir + share->pathLen) == DIRSEPC || 
                 *(searchArray[i].utf8Dir + share->pathLen) == '\0')) {
            LOG(4, ("HgfsServer_InvalidateObjects: Search is still valid\n"));
            break;
         }
      }

      /* If the node wasn't found in any share, remove it. */
      if (l == shares) {
         LOG(4, ("HgfsServer_InvalidateObjects: Search is invalid, removing"
                 "\n"));
         HgfsRemoveSearchInternal(&searchArray[i]);
      }
   }
   SyncMutex_Unlock(&hgfsSearchArrayLock);

   LOG(4, ("HgfsServer_InvalidateObjects: Ending\n"));
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
      LOG(4, ("HgfsServerStatFs: could not get the volume name\n"));
      return FALSE;
   }
         
   /* Now call the wiper lib to get space information. */
   Str_Strcpy(p.mountPoint, pathName, sizeof p.mountPoint);
   wiperError = WiperSinglePartition_GetSpace(&p, freeBytes, totalBytes);
   if (strlen(wiperError) > 0) {
      LOG(4, ("HgfsServerQueryVolume: error using wiper lib: %s\n", 
              wiperError));
      return FALSE;
   }
   
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerGetAccess --
 *
 *    Test a name for access permission and construct its local name
 *    if access is allowed. The name returned is allocated and must be
 *    freed by the caller.
 *
 *    outLen can be NULL, in which case the length is not returned.
 *
 * Results:
 *    A status code indicating either success (access is allowed) or
 *    a failure status.
 *
 * Side effects:
 *    Memory allocation in the success case
 *
 *-----------------------------------------------------------------------------
 */

HgfsNameStatus
HgfsServerGetAccess(char *cpName,                  // IN:  Cross-platform filename to check
                    size_t cpNameSize,             // IN:  Size of name cpName
                    HgfsOpenMode mode,             // IN:  Requested access mode
		    uint32 caseFlags,              // IN:  Case-sensitivity flags
                    char **bufOut,                 // OUT: File name in local fs
                    size_t *outLen,                // OUT: Length of name out
                    HgfsSharedFolder **hgfsShare)  // OUT: Share
{
   HgfsNameStatus nameStatus;
   char const *sharePath;
   char const *inEnd;
   char *next;
   char *myBufOut;
   char *myBufOutCurrent;
   char *out;
   size_t outSize;
   size_t sharePathLen; /* Length of share's path */
   int len;
   uint32 pathNameLen;
   char tempBuf[HGFS_PATH_MAX];
   size_t tempSize;
   char *tempPtr;
   char *ansiName;
   size_t ansiLen;
   Bool result;
   char *savedPathSepPos;
   HgfsSharedFolder *share;
   uint32 startIndex = 0;

   ASSERT(cpName);
   ASSERT(bufOut);

   inEnd = cpName + cpNameSize;

   /* 
    * Get first component. We bypass the higher level CPName_GetComponent
    * function so we'll have more control over the illegal characters, which,
    * for the share name, should be none.
    */
   len = CPName_GetComponentGeneric(cpName, inEnd, "", (char const **) &next);
   if (len < 0) {
      LOG(4, ("HgfsServerGetAccess: get first component failed\n"));
      return HGFS_NAME_STATUS_FAILURE;
   }

   /* See if we are dealing with the base of the namespace */
   if (!len) {
      return HGFS_NAME_STATUS_INCOMPLETE_BASE;
   }

   /*
    * VMDB stores the ANSI name so convert from UTF8 before looking up a share.
    * XXX: Ideally, we should store share names in UTF16 so that these 
    * conversions can be done away with.
    */
   if (!CodeSet_Utf8ToCurrent(cpName,
                              len,
                              &ansiName,
                              &ansiLen)) {
      LOG(4, ("HgfsServerGetAccess: ANSI conversion failed\n"));
      return HGFS_NAME_STATUS_FAILURE;
   }

   /* Check permission on the share and get the share path */
   nameStatus = HgfsServerPolicy_GetSharePath(ansiName,
                                              ansiLen,
                                              mode,
                                              &sharePathLen,
                                              &sharePath,
                                              &share);
   if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
      LOG(4, ("HgfsServerGetAccess: No such share (%s) or access denied\n",
              ansiName));
      free(ansiName);
      return nameStatus;
   }

   free(ansiName);

   /* Point to the next component, if any */
   cpNameSize -= next - cpName;
   cpName = next;

   /*
    * Allocate space for the string. We trim the unused space later.
    */
   outSize = HGFS_PATH_MAX;
   myBufOut = (char *)malloc(outSize * sizeof *myBufOut);
   if (!myBufOut) {
      LOG(4, ("HgfsServerGetAccess: out of memory allocating string\n"));
      return HGFS_NAME_STATUS_OUT_OF_MEMORY;
   }

   out = myBufOut;

   /*
    * See if we are dealing with a "root" share or regular share
    */
   if (strcmp(sharePath, "") == 0) {
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
                                          &cpNameSize,
                                          &tempSize,
                                          &tempPtr);
      if (nameStatus != HGFS_NAME_STATUS_COMPLETE) {
         LOG(4, ("HgfsServerGetAccess: ConvertFromRoot not complete\n"));
         goto error;
      }

      prefixLen = tempPtr - tempBuf;

      /* Copy the UTF8 prefix to the output buffer. */
      if (prefixLen >= HGFS_PATH_MAX) {
         Log("HgfsServerGetAccess: error: prefix too long\n");
         nameStatus = HGFS_NAME_STATUS_TOO_LONG;
         goto error;
      }

      memcpy(out, tempBuf, prefixLen);
      out += prefixLen;
      *out = 0;
      outSize -= prefixLen;
   } else {
      size_t utf8ShareLen;
      char *utf8SharePath;

      /*
       * This is a regular share. Append the UTF8 path to the out buffer.
       */
      if (!CodeSet_CurrentToUtf8(sharePath,
                                 sharePathLen,
                                 &utf8SharePath,
                                 &utf8ShareLen)) {
         LOG(4, ("HgfsServerGetAccess: share name UTF8 conversion failed\n"));
         nameStatus = HGFS_NAME_STATUS_FAILURE;
         goto error;
      }

      if (outSize < utf8ShareLen + 1) {
         LOG(4, ("HgfsServerGetAccess: share path too big\n"));
         free(utf8SharePath);
         nameStatus = HGFS_NAME_STATUS_TOO_LONG;
         goto error;
      }

      memcpy(out, utf8SharePath, utf8ShareLen + 1);
      out += utf8ShareLen;
      outSize -= utf8ShareLen;
      free(utf8SharePath);
   }

   /* Convert the rest of the input name (if any) to a local name */
   tempSize = sizeof tempBuf;
   tempPtr = tempBuf;

   if (CPName_ConvertFrom((char const **) &cpName, 
                          &cpNameSize,
                          &tempSize,
                          &tempPtr) < 0) {
      LOG(4, ("HgfsServerGetAccess: CP name conversion failed\n"));
      nameStatus = HGFS_NAME_STATUS_FAILURE;
      goto error;
   }

   /* 
    * For volume root directory shares the prefix will have a trailing
    * separator and since our remaining paths start with a separator, we
    * will skip over the second separator for this case. Bug 166755.
    */
   if ((out != myBufOut) && 
       (*(out - 1) == DIRSEPC) &&
       (tempBuf[0] == DIRSEPC)) {
      startIndex++;
   }
   pathNameLen = tempPtr - &tempBuf[startIndex];

   /* Copy UTF8 to the output buffer. */
   if (pathNameLen >= outSize) {
      LOG(4, ("HgfsServerGetAccess: pathname too long\n"));
      nameStatus = HGFS_NAME_STATUS_TOO_LONG;
      goto error;
   }

   memcpy(out, &tempBuf[startIndex], pathNameLen);
   outSize -= pathNameLen;
   out += pathNameLen;
   *out = 0;

   /* Convert file name to proper case as per the policy. */
   LOG(4, ("HgfsServerGetAccess: %u\n", caseFlags));
   HgfsServerConvertCase(share, caseFlags, myBufOut);

   /*
    * Verify that our path has no symlinks. We will only check up to the
    * parent, because some ops that call us expect to operate on a symlink
    * final component.
    */
   savedPathSepPos = Str_Strrchr(myBufOut, DIRSEPC);

   /*
    * Since cpName is user-supplied, it's possible that the name was invalid
    * and did not contain any DIRSEPC characters. If that's the case, fail
    * gracefully.
    */

   if (savedPathSepPos == NULL) {
      LOG(4, ("HgfsServerGetAccess: no valid path separator in the name\n"));
      nameStatus = HGFS_NAME_STATUS_FAILURE;
      goto error;
   }

   /* If the path starts with a DIRSEPC. */
   if (savedPathSepPos != myBufOut) {
      *savedPathSepPos = '\0';
   }

   if (!CodeSet_Utf8ToCurrent(myBufOut,
                              strlen(myBufOut),
			      &myBufOutCurrent,
			      NULL)) {
         LOG(4, ("HgfsServerGetAccess: share name UTF8 to current conversion failed\n"));
         nameStatus = HGFS_NAME_STATUS_FAILURE;
	 goto error;
   }

   result = HgfsServerHasSymlink(myBufOutCurrent, sharePath);
   *savedPathSepPos = DIRSEPC;
   free(myBufOutCurrent);
   if (result) {
      LOG(4, ("HgfsServerGetAccess: parent path contains a symlink\n"));
      nameStatus = HGFS_NAME_STATUS_FAILURE;
      goto error;
   }

#if defined(__APPLE__)
   {
      size_t nameLen;
      /* 
       * For Mac hosts the unicode format is decomposed (form D)
       * so there is a need to convert the incoming name from HGFS clients
       * which is assumed to be in the normalized form C (precomposed).
       */
      if (!CodeSet_Utf8FormCToUtf8FormD(myBufOut, 
                                        out - myBufOut, 
                                        &tempPtr, 
                                        &nameLen)) {
         LOG(4, ("HgfsServerGetAccess: unicode conversion to form D failed.\n"));
         nameStatus = HGFS_NAME_STATUS_FAILURE;
         goto error;
      }

      free(myBufOut);
      LOG(4, ("HgfsServerGetAccess: name is \"%s\"\n", myBufOut));

      /* Save returned pointers for memory trim. */
      myBufOut = tempPtr;
      out = tempPtr + nameLen;
   }
#endif /* defined(__APPLE__) */

   {
      char *p;
      size_t len;

      /* Trim unused memory */

      len = out - myBufOut;

      /* Enough space for resulting string + NUL termination */
      p = realloc(myBufOut, (len + 1) * sizeof *p);
      if (!p) {
         LOG(4, ("HgfsServerGetAccess: failed to trim memory\n"));
      } else {
         myBufOut = p;
      }

      if (outLen) {
         *outLen = len;
      }
   }

   LOG(4, ("HgfsServerGetAccess: name is \"%s\"\n", myBufOut));

   if (hgfsShare) {
      *hgfsShare = share;
   }
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
 *    HgfsServerGetAccess() that returns HGFS_NAME_STATUS_COMPLETE.
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

   /* 
    * Get first component. We bypass the higher level CPName_GetComponent
    * function so we'll have more control over the illegal characters, which,
    * for the share name, should be none.
    */
   len = CPName_GetComponentGeneric(cpName, inEnd, "", &next);
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
HgfsServerDumpDents(HgfsHandle searchHandle) // IN: Handle to dump dents from
{
#ifdef VMX86_LOG
   unsigned int i;
   HgfsSearch *search;

   SyncMutex_Lock(&hgfsSearchArrayLock);
   search = HgfsSearchHandle2Search(searchHandle);
   if (search != NULL) {
      Log("HgfsServerDumpDents: %u dents in \"%s\"\n",
          search->numDents, search->utf8Dir);
      Log("Dumping dents:\n");
      for (i = 0; i < search->numDents; i++) {
         Log("\"%s\"\n", search->dents[i]->d_name);
      }
   }
   SyncMutex_Unlock(&hgfsSearchArrayLock);
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
      LOG(4, ("HgfsServerGetDents: Couldn't init state\n"));
      goto error_free;
   }

   for (;;) {
      DirectoryEntry *pDirEntry;
      char const *name;
      size_t len;
      Bool done = FALSE;
      char *utf8Name;
      size_t utf8NameLen;
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
            LOG(4, ("HgfsServerGetDents: Couldn't get next name\n"));
            goto error;
         }
      }

      if (done) {
         LOG(4, ("HgfsServerGetDents: No more names\n"));
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
         Log("HgfsServerGetDents: Error: Name \"%s\" is too long.\n", name);
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
            LOG(4, ("HgfsServerGetDents: Couldn't reallocate array memory\n"));
            goto error;
         }
         myDents = (DirectoryEntry **)p;
      }

      /* This file/directory can be added to the list. Convert to UTF8 first. */
      LOG(4, ("HgfsServerGetDents: Nextfilename = \"%s\"\n", name));
      if (!CodeSet_CurrentToUtf8((const char *)name,
                                 len,
                                 &utf8Name,
                                 &utf8NameLen)) {
         LOG(4, ("HgfsServerGetDents: Unable to convert \"%s\" to utf-8\n", 
                 name));
         goto error;
      }

      /*
       * Start with the size of the DirectoryEntry struct, subtract the static
       * length of the d_name buffer (256 in Linux, 1 in Solaris, etc) and add back
       * just enough space for the UTF-8 name and nul terminator.
       */
      newDirEntryLen =
         sizeof *pDirEntry - sizeof pDirEntry->d_name + utf8NameLen + 1;
      pDirEntry = (DirectoryEntry *)malloc(newDirEntryLen);
      if (!pDirEntry) {
         LOG(4, ("HgfsServerGetDents: Couldn't allocate dentry memory\n"));
         free(utf8Name);
         goto error;
      }
      pDirEntry->d_reclen = (unsigned short)newDirEntryLen;
      memcpy(pDirEntry->d_name, utf8Name, utf8NameLen);
      pDirEntry->d_name[utf8NameLen] = 0;

      myDents[numDents] = pDirEntry;
      numDents++;
      free(utf8Name);
   }

   /* We are done; cleanup the state */
   if (!cleanupName(state)) {
      LOG(4, ("HgfsServerGetDents: Non-error cleanup failed\n"));
      goto error_free;
   }

   /* Trim extra memory off of dents */
   {
      void *p;

      p = realloc(myDents, numDents * sizeof *myDents);
      if (!p) {
         LOG(4, ("HgfsServerGetDents: Couldn't realloc less array memory\n"));
         *dents = myDents;
      } else {
         *dents = (DirectoryEntry **)p;
      }
   }

   return numDents;

error:
   /* Cleanup the callback state */
   if (!cleanupName(state)) {
      LOG(4, ("HgfsServerGetDents: Error cleanup failed\n"));
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
                        DirectorySearchType type, // IN: Kind of search
                        HgfsHandle *handle)       // OUT: Search handle
{
   HgfsSearch *search = NULL;
   HgfsInternalStatus status = 0;
   int numDents;

   ASSERT(baseDir);
   ASSERT(handle);
   ASSERT(type == DIRECTORY_SEARCH_TYPE_DIR);

   SyncMutex_Lock(&hgfsSearchArrayLock);
   search = HgfsAddNewSearch(baseDir, type);
   if (!search) {
      LOG(4, ("HgfsServerSearchRealDir: failed to get new search\n"));
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   status = HgfsServerScandir(baseDir, baseDirLen, &search->dents, &numDents);
   if (status != 0) {
      LOG(4, ("HgfsServerSearchRealDir: couldn't scandir\n"));
      HgfsRemoveSearchInternal(search);
      goto out;
   }

   search->numDents = numDents;
   *handle = HgfsSearch2SearchHandle(search);
  out:
   SyncMutex_Unlock(&hgfsSearchArrayLock);
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
                           HgfsHandle *handle)           // OUT: Search handle
{
   HgfsInternalStatus status = 0;
   HgfsSearch *search = NULL;
   int result = 0;

   ASSERT(getName);
   ASSERT(initName);
   ASSERT(cleanupName);
   ASSERT(handle);

   SyncMutex_Lock(&hgfsSearchArrayLock);
   search = HgfsAddNewSearch("", type);
   if (!search) {
      LOG(4, ("HgfsServerSearchVirtualDir: failed to get new search\n"));
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   result = HgfsServerGetDents(getName,
                               initName,
                               cleanupName,
                               &search->dents);
   if (result < 0) {
      LOG(4, ("HgfsServerSearchVirtualDir: couldn't get dents\n"));
      HgfsRemoveSearchInternal(search);
      status = HGFS_INTERNAL_STATUS_ERROR;
      goto out;
   }

   search->numDents = result;
   *handle = HgfsSearch2SearchHandle(search);
  out:
   SyncMutex_Unlock(&hgfsSearchArrayLock);
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
HgfsRemoveFromCache(HgfsHandle handle)	// IN: Hgfs handle to the node
{
   Bool removed = FALSE;
   SyncMutex_Lock(&hgfsNodeArrayLock);
   removed = HgfsRemoveFromCacheInternal(handle);
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
HgfsIsCached(HgfsHandle handle) // IN: Structure representing file node
{
   Bool cached = FALSE;

   SyncMutex_Lock(&hgfsNodeArrayLock);
   cached = HgfsIsCachedInternal(handle);
   SyncMutex_Unlock(&hgfsNodeArrayLock);

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
 *    hgfsNodeArrayLock should be acquired prior to calling this function.
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
HgfsRemoveLruNode(void)
{
   HgfsFileNode *lruNode = NULL;
   HgfsHandle handle;
   Bool found = FALSE;

   ASSERT(numCachedOpenNodes > 0);
   /* Remove the first item from the list that does not have a server lock. */
   while (!found) {
      lruNode = DblLnkLst_Container(nodeCachedList.next, HgfsFileNode, links);
      ASSERT(lruNode->state == FILENODE_STATE_IN_USE_CACHED);
      if (lruNode->serverLock != HGFS_LOCK_NONE) {
         /* Move this node with the server lock to the beginning of the list. */
         DblLnkLst_Unlink1(&lruNode->links);
         DblLnkLst_LinkLast(&nodeCachedList, &lruNode->links);
      } else {
         found = TRUE;
      }
   }
   handle = HgfsFileNode2Handle(lruNode);
   if (!HgfsRemoveFromCacheInternal(handle)) {
      LOG(4, ("HgfsRemoveLruNode: Could not remove the node from cache.\n"));
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
HgfsAddToCache(HgfsHandle handle) // IN: HGFS file handle
{
   Bool added = FALSE;
   SyncMutex_Lock(&hgfsNodeArrayLock);
   added = HgfsAddToCacheInternal(handle);
   SyncMutex_Unlock(&hgfsNodeArrayLock);
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
                           Bool append)                // IN: flag to append
{
   HgfsHandle handle;
   HgfsFileNode *node = NULL;
   char const *inEnd;
   char const *next;
   char *shareName = NULL;
   size_t shareLen = 0;
   uint32 len;
   Bool sharedFolderOpen = FALSE;

   ASSERT(openInfo);
   ASSERT(localId);

   inEnd = openInfo->cpName + openInfo->cpNameSize;

   /* 
    * Get first component. We bypass the higher level CPName_GetComponent
    * function so we'll have more control over the illegal characters, which,
    * for the share name, should be none.
    */
   len = CPName_GetComponentGeneric(openInfo->cpName, inEnd, "", &next);
   if (len < 0) {
      LOG(4, ("HgfsServerGetAccess: get first component failed\n"));
      return FALSE;
   }

   /* See if we are dealing with the base of the namespace */
   if (!len) {
      return FALSE;
   }

   if (!next) {
      sharedFolderOpen = TRUE;
   }

   /*
    * VMDB stores the ANSI name so convert from UTF8 before looking up a share.
    * XXX: Ideally, we should store share names in UTF16 so that these 
    * conversions can be done away with.
    */
   if (!CodeSet_Utf8ToCurrent(openInfo->cpName,
                              len,
                              &shareName,
                              &shareLen)) {
      LOG(4, ("HgfsServerGetAccess: ANSI conversion failed\n"));
      return FALSE;
   }

   SyncMutex_Lock(&hgfsNodeArrayLock);
   node = HgfsAddNewFileNode(openInfo,
                             localId,
                             fileDesc,
                             append,
                             shareLen,
                             shareName,
                             sharedFolderOpen);
   free(shareName);
   if (node == NULL) {
      LOG(4, ("HgfsCreateAndCacheFileNode: Failed to add new node.\n"));
      SyncMutex_Unlock(&hgfsNodeArrayLock);
      return FALSE;
   }
   handle = HgfsFileNode2Handle(node);

   if (!HgfsAddToCacheInternal(handle)) {
      LOG(4, ("HgfsCreateAndCacheFileNode: Failed to add node to the cache.\n"));
      SyncMutex_Unlock(&hgfsNodeArrayLock);
      return FALSE;
   }
   SyncMutex_Unlock(&hgfsNodeArrayLock);

   /* Only after everything is successful, save the handle in the open info. */
   openInfo->file = handle;

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
   HgfsRequest *request;
   size_t extra;

   ASSERT(packetIn);
   ASSERT(openInfo);
   request = (HgfsRequest *)packetIn;

   openInfo->requestType = request->op;
   openInfo->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (openInfo->requestType) {
   case HGFS_OP_OPEN_V3:
      {
          HgfsRequestOpenV3 *requestV3 =
            (HgfsRequestOpenV3 *)(packetIn + sizeof *request);
          LOG(4, ("HgfsUnpackOpenRequest: HGFS_OP_OPEN_V3\n"));
      

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV3 + sizeof *request);
         extra = packetSize - sizeof *requestV3 - sizeof *request;

         if (!(requestV3->mask & HGFS_OPEN_VALID_FILE_NAME)) {
            /* We do not support open requests without a valid file name. */
            return FALSE;
         }

         /*
          * requestV3->fileName.length is user-provided, so this test must be
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
	 openInfo->caseFlags = requestV3->fileName.flags;
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
         break;
      }
   case HGFS_OP_OPEN_V2:
      {
         HgfsRequestOpenV2 *requestV2 =
            (HgfsRequestOpenV2 *)packetIn;

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV2);
         extra = packetSize - sizeof *requestV2;

         if (!(requestV2->mask & HGFS_OPEN_VALID_FILE_NAME)) {
            /* We do not support open requests without a valid file name. */
            return FALSE;
         }

         /*
          * requestV2->fileName.length is user-provided, so this test must be
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
         break;
      }
   case HGFS_OP_OPEN:
      {
         HgfsRequestOpen *requestV1 = 
            (HgfsRequestOpen *)packetIn;

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV1);
         extra = packetSize - sizeof *requestV1;

         /*
          * requestV1->fileName.length is user-provided, so this test must be 
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
HgfsPackOpenReply(HgfsFileOpenInfo *openInfo,   // IN: open info struct
                  char *packetOut,              // IN/OUT: outgoing packet
                  size_t *packetSize)           // IN/OUT: size of packet
{
   ASSERT(packetOut);
   ASSERT(packetSize);
   ASSERT(openInfo);

   if (openInfo->requestType == HGFS_OP_OPEN) {
      HgfsReplyOpen *reply = (HgfsReplyOpen *)packetOut;
      reply->file = openInfo->file;
      *packetSize = sizeof *reply;
   } else {
      /* Identical reply packet for V2 and V3. */
      HgfsReplyOpenV2 *reply = (HgfsReplyOpenV2 *)packetOut;
      reply->file = openInfo->file;
      if (openInfo->mask & HGFS_OPEN_VALID_SERVER_LOCK) {
         reply->acquiredLock = openInfo->acquiredLock;
      }
      *packetSize = sizeof *reply;
   }

   return TRUE;
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
HgfsUnpackDeleteRequest(char const *packetIn,        // IN: request packet
                         size_t packetSize,          // IN: request packet size
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsDeleteHint *hints,      // OUT: delete hints
                         HgfsHandle *file,           // OUT: file handle
			 uint32 *caseFlags)          // OUT: case-sensitivity flags
{
   HgfsRequest *request;
   size_t extra;

   ASSERT(packetIn);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(hints);
   ASSERT(caseFlags);

   request = (HgfsRequest *)packetIn;
   *caseFlags = HGFS_FILE_NAME_CASE_SENSITIVE;

   switch (request->op) {
   case HGFS_OP_DELETE_FILE_V3:
   case HGFS_OP_DELETE_DIR_V3: {
      HgfsRequestDeleteV3 *requestV3;
      
      requestV3 = (HgfsRequestDeleteV3 *)(packetIn + sizeof *request);
      LOG(4, ("HgfsUnpackDeleteRequest: HGFS_OP_DELETE_DIR_V3\n"));
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV3 + sizeof *request);

      *file = HGFS_INVALID_HANDLE;
      *hints = requestV3->hints;

      /* 
       * If we've been asked to reuse a handle, we don't need to look at, let
       * alone test the filename or its length.
       */
      if (*hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
         *file = requestV3->file;
         *cpName = NULL;
         *cpNameSize = 0;
      } else {
         extra = packetSize - sizeof *requestV3 - sizeof *request;

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV3->fileName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }
         *cpName = requestV3->fileName.name;
         *cpNameSize = requestV3->fileName.length;
	 *caseFlags = requestV3->fileName.flags;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE_V2:
   case HGFS_OP_DELETE_DIR_V2: {
      HgfsRequestDeleteV2 *requestV2;
      requestV2 = (HgfsRequestDeleteV2 *)packetIn;
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV2);

      *file = HGFS_INVALID_HANDLE;
      *hints = requestV2->hints;

      /* 
       * If we've been asked to reuse a handle, we don't need to look at, let
       * alone test the filename or its length.
       */
      if (*hints & HGFS_DELETE_HINT_USE_FILE_DESC) {
         *file = requestV2->file;
         *cpName = NULL;
         *cpNameSize = 0;
      } else {
         extra = packetSize - sizeof *requestV2;

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV2->fileName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }
         *cpName = requestV2->fileName.name;
         *cpNameSize = requestV2->fileName.length;
      }
      break;
   }
   case HGFS_OP_DELETE_FILE:
   case HGFS_OP_DELETE_DIR: {
      HgfsRequestDelete *requestV1;
      
      requestV1 = (HgfsRequestDelete *)packetIn;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV1);
      extra = packetSize - sizeof *requestV1;

      /*
       * request->fileName.length is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */

      if (requestV1->fileName.length > extra) {
         /* The input packet is smaller than the request. */
         return FALSE;
      }
      *cpName = requestV1->fileName.name;
      *cpNameSize = requestV1->fileName.length;
      break;
   }
   default:
      return FALSE;
      break;
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
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackDeleteReply(char *packetOut,           // IN/OUT: outgoing packet
                    size_t *packetSize)        // IN/OUT: size of packet

{
   HgfsReplyDelete *reply = (HgfsReplyDelete *)packetOut;

   ASSERT(packetOut);
   ASSERT(packetSize);

   *packetSize = sizeof *reply;

   return TRUE;
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
                        char **cpOldName,           // OUT: rename src
                        uint32 *cpOldNameLen,       // OUT: rename src size
                        char **cpNewName,           // OUT: rename dst
                        uint32 *cpNewNameLen,       // OUT: rename dst size
                        HgfsRenameHint *hints,      // OUT: rename hints
                        HgfsHandle *srcFile,        // OUT: src file handle
                        HgfsHandle *targetFile,     // OUT: target file handle
			uint32 *oldCaseFlags,       // OUT: source case-sensitivity flags
			uint32 *newCaseFlags)       // OUT: dest. case-sensitivity flags
{
   HgfsRequestRename *requestV1;
   HgfsRequestRenameV2 *requestV2;
   HgfsRequestRenameV3 *requestV3;
   HgfsRequest *request;
   HgfsFileName *newName;
   size_t extra;

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

   request = (HgfsRequest *)packetIn;

   /*
    * Get the old and new filenames from the request, V1 and for V2
    * we get the handle or old filename and the new filename.
    *
    * Getting the new filename is somewhat inconvenient, because we
    * don't know where request->newName actually starts, thanks to the
    * fact that request->oldName is of variable length. We get around
    * this by using an HgfsFileName*, assigning it to the correct address
    * just after request->oldName ends, and using that to access the
    * new name.
    */

   switch (request->op) {
   case HGFS_OP_RENAME_V3:
      requestV3 = (HgfsRequestRenameV3 *)packetIn;
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV3);
      extra = packetSize - sizeof *requestV3;

      *hints = requestV3->hints;

      /* 
       * If we've been asked to reuse a handle, we don't need to look at, let
       * alone test the filename or its length. This applies to the source
       * and the target.
       */
      if (*hints & HGFS_RENAME_HINT_USE_SRCFILE_DESC) {
         *srcFile = requestV3->srcFile;
         *cpOldName = NULL;
         *cpOldNameLen = 0;
	 *oldCaseFlags = 0;
      } else {

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV3->oldName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }

         /* It is now safe to use the old file name. */
         *cpOldName = requestV3->oldName.name;
         *cpOldNameLen = requestV3->oldName.length;
	 *oldCaseFlags = requestV3->oldName.flags;
      }
      extra -= *cpOldNameLen;

      if (*hints & HGFS_RENAME_HINT_USE_TARGETFILE_DESC) {
         *targetFile = requestV3->targetFile;
         *cpNewName = NULL;
         *cpNewNameLen = 0;
	 *newCaseFlags = 0;
      } else {

         newName = (HgfsFileName *)((char *)(&requestV3->oldName + 1)
                                       + *cpOldNameLen);
         if (newName->length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }

         /* It is now safe to use the new file name. */
         *cpNewName = newName->name;
         *cpNewNameLen = newName->length;
	 *newCaseFlags = requestV3->newName.flags;
      }
      break;

   case HGFS_OP_RENAME_V2:
      requestV2 = (HgfsRequestRenameV2 *)packetIn;
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV2);
      extra = packetSize - sizeof *requestV2;

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

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV2->oldName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }

         /* It is now safe to use the old file name. */
         *cpOldName = requestV2->oldName.name;
         *cpOldNameLen = requestV2->oldName.length;
      }
      extra -= *cpOldNameLen;

      if (*hints & HGFS_RENAME_HINT_USE_TARGETFILE_DESC) {
         *targetFile = requestV2->targetFile;
         *cpNewName = NULL;
         *cpNewNameLen = 0;
      } else {

         newName = (HgfsFileName *)((char *)(&requestV2->oldName + 1)
                                       + *cpOldNameLen);
         if (newName->length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }

         /* It is now safe to use the new file name. */
         *cpNewName = newName->name;
         *cpNewNameLen = newName->length;
      }
      break;

   case HGFS_OP_RENAME:
      requestV1 = (HgfsRequestRename *)packetIn;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV1);
      extra = packetSize - sizeof *requestV1;

      /*
       * request->fileName.length is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */
      if (requestV1->oldName.length > extra) {
         /* The input packet is smaller than the request. */
         return FALSE;
      }

      /* It is now safe to use the old file name. */
      *cpOldName = requestV1->oldName.name;
      *cpOldNameLen = requestV1->oldName.length;
      extra -= requestV1->oldName.length;

      newName = (HgfsFileName *)((char *)(&requestV1->oldName + 1)
                                 + requestV1->oldName.length);
      /*
       * newName->length is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */
      if (newName->length > extra) {
         /* The input packet is smaller than the request. */
         return FALSE;
      }

      /* It is now safe to use the new file name. */
      *cpNewName = newName->name;
      *cpNewNameLen = newName->length;
      break;

   default:
      return FALSE;
      break;
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
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackRenameReply(char *packetOut,           // IN/OUT: outgoing packet
                    size_t *packetSize)        // IN/OUT: size of packet

{
   HgfsReplyRename *reply = (HgfsReplyRename *)packetOut;

   ASSERT(packetOut);
   ASSERT(packetSize);

   *packetSize = sizeof *reply;

   return TRUE;
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
			 uint32 *caseFlags)           // OUT: case-sensitivity flags
{
   HgfsRequestGetattr *requestV1;
   HgfsRequestGetattrV2 *requestV2;
   HgfsRequestGetattrV3 *requestV3;
   HgfsRequest *request;
   size_t extra;

   ASSERT(packetIn);
   ASSERT(attrInfo);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseFlags);

   request = (HgfsRequest *)packetIn;
   attrInfo->requestType = request->op;
   *caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (request->op) {
   case HGFS_OP_GETATTR_V3:
      requestV3 = (HgfsRequestGetattrV3 *)(packetIn + sizeof(struct HgfsRequest));
      LOG(4, ("HgfsUnpackGetattrRequest: HGFS_OP_GETATTR_V3\n"));
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV3 + sizeof(struct HgfsRequest));

      /* 
       * If we've been asked to reuse a handle, we don't need to look at, let
       * alone test the filename or its length.
       */
      *hints = requestV3->hints;
      if (*hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
         *file = requestV3->file;
         *cpName = NULL;
         *cpNameSize = 0;
      } else {
         extra = packetSize - sizeof *requestV3 - sizeof(struct HgfsRequest);

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV3->fileName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }
         *cpName = requestV3->fileName.name;
         *cpNameSize = requestV3->fileName.length;
	 *caseFlags = requestV3->fileName.flags;
         LOG(4, ("HgfsUnpackGetattrRequest: HGFS_OP_GETATTR_V3: %u\n", *caseFlags));
      }
      break;

   case HGFS_OP_GETATTR_V2:
      requestV2 = (HgfsRequestGetattrV2 *)packetIn;
      
      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV2);


      /* 
       * If we've been asked to reuse a handle, we don't need to look at, let
       * alone test the filename or its length.
       */
      *hints = requestV2->hints;
      if (*hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
         *file = requestV2->file;
         *cpName = NULL;
         *cpNameSize = 0;
      } else {
         extra = packetSize - sizeof *requestV2;

         /*
          * request->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */
         if (requestV2->fileName.length > extra) {
            /* The input packet is smaller than the request */
            return FALSE;
         }
         *cpName = requestV2->fileName.name;
         *cpNameSize = requestV2->fileName.length;
      }
      break;

   case HGFS_OP_GETATTR:
      requestV1 = (HgfsRequestGetattr *)packetIn;

      /* Enforced by the dispatch function. */
      ASSERT(packetSize >= sizeof *requestV1);
      extra = packetSize - sizeof *requestV1;

      /*
       * request->fileName.length is user-provided, so this test must be carefully
       * written to prevent wraparounds.
       */

      if (requestV1->fileName.length > extra) {
         /* The input packet is smaller than the request. */
         return FALSE;
      }
      *cpName = requestV1->fileName.name;
      *cpNameSize = requestV1->fileName.length;
      break;

   default:
      return FALSE;
   }


   /* Initialize the rest of the fields. */
   attrInfo->mask = HGFS_ATTR_VALID_NONE;
   attrInfo->type = 0;
   attrInfo->size = 0;
   attrInfo->creationTime = 0;
   attrInfo->accessTime = 0;
   attrInfo->writeTime = 0;
   attrInfo->attrChangeTime = 0;
   attrInfo->specialPerms = 0;
   attrInfo->ownerPerms = 0;
   attrInfo->groupPerms = 0;
   attrInfo->otherPerms = 0;
   attrInfo->flags = 0;
   attrInfo->allocationSize = 0;
   attrInfo->userId = 0;
   attrInfo->groupId = 0;
   attrInfo->hostFileId = 0;
   return TRUE;
}



/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPackGetattrReply --
 *
 *    Pack hgfs getattr reply to the HgfsReplyGetattr{V2} structure.
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
HgfsPackGetattrReply(HgfsFileAttrInfo *attr,     // IN: attr stucture
                     const char *utf8TargetName, // IN: optional target name
                     uint32 utf8TargetNameLen,   // IN: file name length
                     char *packetOut,            // IN/OUT: outgoing packet
                     size_t *packetSize)         // IN/OUT: size of packet
{
   ASSERT(packetOut);
   ASSERT(packetSize);
   ASSERT(attr);

   if (attr->requestType == HGFS_OP_GETATTR_V3) {
      HgfsReplyGetattrV3 *reply = (HgfsReplyGetattrV3 *)(packetOut + 
                                  sizeof(struct HgfsRequest));
      reply->attr.mask = attr->mask;
      reply->attr.type = attr->type;

      /*
       * Is there enough space in the request packet for the utf8 name?
       * Our goal is to write the entire name, with nul terminator, into
       * the buffer, but set the length to not include the nul termination.
       * This is what clients expect.
       *
       * Also keep in mind that sizeof *reply already contains one character,
       * which we'll consider the nul terminator.
       */
      if (utf8TargetNameLen > HGFS_PACKET_MAX - sizeof *reply - sizeof(struct HgfsReply)) {
         return FALSE;
      }
      if (utf8TargetName) {
         memcpy(reply->symlinkTarget.name, utf8TargetName, utf8TargetNameLen);
         CPNameLite_ConvertTo(reply->symlinkTarget.name, utf8TargetNameLen,
                              DIRSEPC);
      } else {
         ASSERT(utf8TargetNameLen == 0);
      }
      reply->symlinkTarget.length = utf8TargetNameLen;
      reply->symlinkTarget.name[utf8TargetNameLen] = '\0';
      reply->symlinkTarget.flags = HGFS_FILE_NAME_DEFAULT_CASE;

      reply->attr.size = attr->size;
      reply->attr.creationTime = attr->creationTime;
      reply->attr.accessTime = attr->accessTime;
      reply->attr.writeTime = attr->writeTime;
      reply->attr.attrChangeTime = attr->attrChangeTime;
      reply->attr.specialPerms = attr->specialPerms;
      reply->attr.ownerPerms = attr->ownerPerms;
      reply->attr.groupPerms = attr->groupPerms;
      reply->attr.otherPerms = attr->otherPerms;
      reply->attr.flags = attr->flags;
      reply->attr.allocationSize = attr->allocationSize;
      reply->attr.userId = attr->userId;
      reply->attr.groupId = attr->groupId;
      reply->attr.hostFileId = attr->hostFileId;
      *packetSize = sizeof *reply + utf8TargetNameLen + sizeof(struct HgfsReply);
   } else if (attr->requestType == HGFS_OP_GETATTR_V2) {
      HgfsReplyGetattrV2 *reply = (HgfsReplyGetattrV2 *)packetOut;
      reply->attr.mask = attr->mask;
      reply->attr.type = attr->type;

      /*
       * Is there enough space in the request packet for the utf8 name?
       * Our goal is to write the entire name, with nul terminator, into
       * the buffer, but set the length to not include the nul termination.
       * This is what clients expect.
       *
       * Also keep in mind that sizeof *reply already contains one character,
       * which we'll consider the nul terminator.
       */
      if (utf8TargetNameLen > HGFS_PACKET_MAX - sizeof *reply) {
         return FALSE;
      }
      if (utf8TargetName) {
         memcpy(reply->symlinkTarget.name, utf8TargetName, utf8TargetNameLen);
         CPNameLite_ConvertTo(reply->symlinkTarget.name, utf8TargetNameLen,
                              DIRSEPC);
      } else {
         ASSERT(utf8TargetNameLen == 0);
      }
      reply->symlinkTarget.length = utf8TargetNameLen;
      reply->symlinkTarget.name[utf8TargetNameLen] = '\0';

      reply->attr.size = attr->size;
      reply->attr.creationTime = attr->creationTime;
      reply->attr.accessTime = attr->accessTime;
      reply->attr.writeTime = attr->writeTime;
      reply->attr.attrChangeTime = attr->attrChangeTime;
      reply->attr.specialPerms = attr->specialPerms;
      reply->attr.ownerPerms = attr->ownerPerms;
      reply->attr.groupPerms = attr->groupPerms;
      reply->attr.otherPerms = attr->otherPerms;
      reply->attr.flags = attr->flags;
      reply->attr.allocationSize = attr->allocationSize;
      reply->attr.userId = attr->userId;
      reply->attr.groupId = attr->groupId;
      reply->attr.hostFileId = attr->hostFileId;
      *packetSize = sizeof *reply + utf8TargetNameLen;
   } else {
      HgfsReplyGetattr *reply = (HgfsReplyGetattr *)packetOut;

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
      *packetSize = sizeof *reply;
   }

   return TRUE;
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
   HgfsRequestSearchRead *request;

   ASSERT(packetIn);
   ASSERT(attr);
   ASSERT(hgfsSearchHandle);
   ASSERT(offset);

   /* XXX: Changes required for VMCI. */
   request = (HgfsRequestSearchRead *)packetIn;

   /* Enforced by the dispatch function. */
   ASSERT(packetSize >= sizeof *request);

   *hgfsSearchHandle = request->search;
   *offset = request->offset;

   /* Initialize the rest of the fields. */
   attr->requestType = request->header.op;

   if (request->header.op == HGFS_OP_SEARCH_READ_V3) {
      LOG(4, ("HgfsUnpackSearchReadRequest: HGFS_OP_SEARCH_READ_V3\n"));
   }

   attr->mask = HGFS_ATTR_VALID_NONE;
   attr->type = 0;
   attr->size = 0;
   attr->creationTime = 0;
   attr->accessTime = 0;
   attr->writeTime = 0;
   attr->attrChangeTime = 0;
   attr->specialPerms = 0;
   attr->ownerPerms = 0;
   attr->groupPerms = 0;
   attr->otherPerms = 0;
   attr->flags = 0;
   attr->allocationSize = 0;
   attr->userId = 0;
   attr->groupId = 0;
   attr->hostFileId = 0;

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
HgfsPackSearchReadReply(const char *utf8Name,      // IN: file name
                        size_t utf8NameLen,        // IN: file name length
                        HgfsFileAttrInfo *attr,    // IN: file attr struct
                        char *packetOut,           // IN/OUT: outgoing packet
                        size_t *packetSize)        // IN/OUT: size of packet
{
   if (attr->requestType == HGFS_OP_SEARCH_READ_V3) {
      HgfsReplySearchReadV3 *reply = (HgfsReplySearchReadV3 *)packetOut + 
                                  sizeof(struct HgfsRequest);
      HgfsDirEntry *dirent = (HgfsDirEntry *)reply->payload;

      /*
       * Is there enough space in the request packet for the utf8 name?
       * Our goal is to write the entire name, with nul terminator, into
       * the buffer, but set the length to not include the nul termination.
       * This is what clients expect.
       *
       * Also keep in mind that sizeof *reply already contains one character,
       * which we'll consider the nul terminator.
       */
      if (utf8NameLen > HGFS_PACKET_MAX - sizeof *reply - sizeof(struct HgfsReply)
                                        - sizeof(struct HgfsDirEntry)) {
         return FALSE;
      }

      *packetSize = sizeof *reply + utf8NameLen + sizeof(struct HgfsReply)
                                  + sizeof(struct HgfsDirEntry);
      reply->count = 1;
      dirent->fileName.length = (uint32)utf8NameLen;
      dirent->fileName.flags = HGFS_FILE_NAME_DEFAULT_CASE;
      dirent->nextEntry = 0;

      if (utf8NameLen == 0) {
         /* No entry. */
         return TRUE;
      }

      memcpy(dirent->fileName.name, utf8Name, utf8NameLen);
      dirent->fileName.name[utf8NameLen] = 0;

      dirent->attr.mask = attr->mask;
      dirent->attr.type = attr->type;
      dirent->attr.size = attr->size;
      dirent->attr.creationTime = attr->creationTime;
      dirent->attr.accessTime = attr->accessTime;
      dirent->attr.writeTime = attr->writeTime;
      dirent->attr.attrChangeTime = attr->attrChangeTime;
      dirent->attr.specialPerms = attr->specialPerms;
      dirent->attr.ownerPerms = attr->ownerPerms;
      dirent->attr.groupPerms = attr->groupPerms;
      dirent->attr.otherPerms = attr->otherPerms;
      dirent->attr.flags = attr->flags;
      dirent->attr.allocationSize = attr->allocationSize;
      dirent->attr.userId = attr->userId;
      dirent->attr.groupId = attr->groupId;
      dirent->attr.hostFileId = attr->hostFileId;
   } else if (attr->requestType == HGFS_OP_SEARCH_READ_V2) {
      HgfsReplySearchReadV2 *reply = (HgfsReplySearchReadV2 *)packetOut;

      /*
       * Is there enough space in the request packet for the utf8 name?
       * Our goal is to write the entire name, with nul terminator, into
       * the buffer, but set the length to not include the nul termination.
       * This is what clients expect.
       *
       * Also keep in mind that sizeof *reply already contains one character,
       * which we'll consider the nul terminator.
       */
      if (utf8NameLen > HGFS_PACKET_MAX - sizeof *reply) {
         return FALSE;
      }

      *packetSize = sizeof *reply + utf8NameLen;
      reply->fileName.length = (uint32)utf8NameLen;

      if (utf8NameLen == 0) {
         /* No entry. */
         return TRUE;
      }

      memcpy(reply->fileName.name, utf8Name, utf8NameLen);
      reply->fileName.name[utf8NameLen] = 0;

      reply->attr.mask = attr->mask;
      reply->attr.type = attr->type;
      reply->attr.size = attr->size;
      reply->attr.creationTime = attr->creationTime;
      reply->attr.accessTime = attr->accessTime;
      reply->attr.writeTime = attr->writeTime;
      reply->attr.attrChangeTime = attr->attrChangeTime;
      reply->attr.specialPerms = attr->specialPerms;
      reply->attr.ownerPerms = attr->ownerPerms;
      reply->attr.groupPerms = attr->groupPerms;
      reply->attr.otherPerms = attr->otherPerms;
      reply->attr.flags = attr->flags;
      reply->attr.allocationSize = attr->allocationSize;
      reply->attr.userId = attr->userId;
      reply->attr.groupId = attr->groupId;
      reply->attr.hostFileId = attr->hostFileId;

   } else {
      HgfsReplySearchRead *reply = (HgfsReplySearchRead *)packetOut;

      /*
       * Is there enough space in the request packet for the utf8 name?
       * Our goal is to write the entire name, with nul terminator, into
       * the buffer, but set the length to not include the nul termination.
       * This is what clients expect.
       *
       * Also keep in mind that sizeof *reply already contains one character,
       * which we'll consider the nul terminator.
       */
      if (utf8NameLen > HGFS_PACKET_MAX - sizeof *reply) {
         return FALSE;
      }

      *packetSize = sizeof *reply + utf8NameLen;
      reply->fileName.length = (uint32)utf8NameLen;

      if (utf8NameLen == 0) {
         /* No entry. */
         return TRUE;
      }
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

   return TRUE;
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
                         HgfsFileAttrInfo *attr,     // IN/OUT: getattr info
                         HgfsAttrHint *hints,        // OUT: setattr hints
                         char **cpName,              // OUT: cpName
                         size_t *cpNameSize,         // OUT: cpName size
                         HgfsHandle *file,           // OUT: server file ID
                         uint32 *caseFlags)          // OUT: case-sensitivity flags 
{
   HgfsRequest *request;
   size_t extra;

   ASSERT(packetIn);
   ASSERT(attr);
   ASSERT(cpName);
   ASSERT(cpNameSize);
   ASSERT(file);
   ASSERT(caseFlags);
   request = (HgfsRequest *)packetIn;


   /* Initialize the rest of the fields. */
   attr->requestType = request->op;

   switch (attr->requestType) {
   case HGFS_OP_SETATTR_V3:
      {
         HgfsRequestSetattrV3 *requestV3 = 
            (HgfsRequestSetattrV3 *)(packetIn + sizeof(struct HgfsRequest));

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV3 + sizeof(struct HgfsRequest));

         attr->mask = requestV3->attr.mask;
         attr->type = requestV3->attr.type;
         attr->size = requestV3->attr.size;
         attr->creationTime = requestV3->attr.creationTime;
         attr->accessTime = requestV3->attr.accessTime;
         attr->writeTime = requestV3->attr.writeTime;
         attr->attrChangeTime = requestV3->attr.attrChangeTime;
         attr->specialPerms = requestV3->attr.specialPerms;
         attr->ownerPerms = requestV3->attr.ownerPerms;
         attr->groupPerms = requestV3->attr.groupPerms;
         attr->otherPerms = requestV3->attr.otherPerms;
         attr->flags = requestV3->attr.flags;
         attr->allocationSize = requestV3->attr.allocationSize;
         attr->userId = requestV3->attr.userId;
         attr->groupId = requestV3->attr.groupId;
         attr->hostFileId = requestV3->attr.hostFileId;
         
         *hints = requestV3->hints;

         /* 
          * If we've been asked to reuse a handle, we don't need to look at, 
          * let alone test the filename or its length.
          */
         if (*hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
            *file = requestV3->file;
            *cpName = NULL;
            *cpNameSize = 0;
	    *caseFlags = 0;
         } else {
            extra = packetSize - sizeof *requestV3 - sizeof(struct HgfsRequest);

            if (requestV3->fileName.length > extra) {
               /* The input packet is smaller than the request. */
               return FALSE;
            }
            /* It is now safe to read the file name. */
            *cpName = requestV3->fileName.name;
            *cpNameSize = requestV3->fileName.length;
            *caseFlags = requestV3->fileName.flags;
         }
         LOG(4, ("HgfsUnpackSetattrRequest: unpacking HGFS_OP_SETATTR_V3, %u\n", *caseFlags));
         break;
      }

   case HGFS_OP_SETATTR_V2:
      {
         HgfsRequestSetattrV2 *requestV2 = 
            (HgfsRequestSetattrV2 *)packetIn;

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV2);

         attr->mask = requestV2->attr.mask;
         attr->type = requestV2->attr.type;
         attr->size = requestV2->attr.size;
         attr->creationTime = requestV2->attr.creationTime;
         attr->accessTime = requestV2->attr.accessTime;
         attr->writeTime = requestV2->attr.writeTime;
         attr->attrChangeTime = requestV2->attr.attrChangeTime;
         attr->specialPerms = requestV2->attr.specialPerms;
         attr->ownerPerms = requestV2->attr.ownerPerms;
         attr->groupPerms = requestV2->attr.groupPerms;
         attr->otherPerms = requestV2->attr.otherPerms;
         attr->flags = requestV2->attr.flags;
         attr->allocationSize = requestV2->attr.allocationSize;
         attr->userId = requestV2->attr.userId;
         attr->groupId = requestV2->attr.groupId;
         attr->hostFileId = requestV2->attr.hostFileId;
         
         *hints = requestV2->hints;

         /* 
          * If we've been asked to reuse a handle, we don't need to look at, 
          * let alone test the filename or its length.
          */
         if (*hints & HGFS_ATTR_HINT_USE_FILE_DESC) {
            *file = requestV2->file;
            *cpName = NULL;
            *cpNameSize = 0;
         } else {
            extra = packetSize - sizeof *requestV2;

            if (requestV2->fileName.length > extra) {
               /* The input packet is smaller than the request. */
               return FALSE;
            }
            /* It is now safe to read the file name. */
            *cpName = requestV2->fileName.name;
            *cpNameSize = requestV2->fileName.length;
         }
         LOG(4, ("HgfsUnpackSetattrRequest: unpacking HGFS_OP_SETATTR_V2\n"));
         break;
      }
   case HGFS_OP_SETATTR:
      {
         HgfsRequestSetattr *requestV1 = 
            (HgfsRequestSetattr *)packetIn;
         
         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV1);
         extra = packetSize - sizeof *requestV1;
         
         /*
          * requestV1->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */

         if (requestV1->fileName.length > extra) {
            /* The input packet is smaller than the request. */
            return FALSE;
         }
         
         /* It is now safe to read the file name. */
         *cpName = requestV1->fileName.name;
         *cpNameSize = requestV1->fileName.length;
         
         attr->mask = 0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_SIZE ?
            HGFS_ATTR_VALID_SIZE :
            0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_CREATE_TIME ?
            HGFS_ATTR_VALID_CREATE_TIME :
            0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_ACCESS_TIME ?
            HGFS_ATTR_VALID_ACCESS_TIME :
            0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_WRITE_TIME ?
            HGFS_ATTR_VALID_WRITE_TIME :
            0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_CHANGE_TIME ?
            HGFS_ATTR_VALID_CHANGE_TIME :
            0;
         attr->mask |=
            requestV1->update & HGFS_ATTR_PERMISSIONS ?
            HGFS_ATTR_VALID_OWNER_PERMS :
            0;
         
         *hints     |=
            requestV1->update & HGFS_ATTR_ACCESS_TIME_SET ?
            HGFS_ATTR_HINT_SET_ACCESS_TIME :
            0;
         
         *hints     |=
            requestV1->update & HGFS_ATTR_WRITE_TIME_SET ?
            HGFS_ATTR_HINT_SET_WRITE_TIME :
            0;
         
         attr->type = requestV1->attr.type;
         attr->size = requestV1->attr.size;
         attr->creationTime = requestV1->attr.creationTime;
         attr->accessTime = requestV1->attr.accessTime;
         attr->writeTime = requestV1->attr.writeTime;
         attr->attrChangeTime = requestV1->attr.attrChangeTime;
         attr->ownerPerms = requestV1->attr.permissions;
         LOG(4, ("HgfsUnpackSetattrRequest: unpacking HGFS_OP_SETATTR\n"));
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
 *    Always TRUE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPackSetattrReply(char *packetOut,           // IN/OUT: outgoing packet
                     size_t *packetSize)        // IN/OUT: size of packet

{
   HgfsReplySetattr *reply = (HgfsReplySetattr *)packetOut;

   ASSERT(packetOut);
   ASSERT(packetSize);

   *packetSize = sizeof *reply;

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
   HgfsRequest *request;
   size_t extra;
   
   ASSERT(packetIn);
   ASSERT(info);
   request = (HgfsRequest *)packetIn;

   info->requestType = request->op;
   info->caseFlags = HGFS_FILE_NAME_DEFAULT_CASE;

   switch (info->requestType) {
   case HGFS_OP_CREATE_DIR_V3:
      {
         HgfsRequestCreateDirV3 *requestV3 = 
            (HgfsRequestCreateDirV3 *)(packetIn + sizeof *request);

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV3 + sizeof *request);
         extra = packetSize - sizeof *requestV3 - sizeof *request;

         if (!(requestV3->mask & HGFS_CREATE_DIR_VALID_FILE_NAME)) {
            /* We do not support requests without a valid file name. */
            return FALSE;
         }

         /*
          * requestV3->fileName.length is user-provided, so this test must be 
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
         info->mask = requestV3->mask;
         info->cpName = requestV3->fileName.name;
         info->cpNameSize = requestV3->fileName.length;
	 info->caseFlags = requestV3->fileName.flags;
         info->specialPerms = requestV3->specialPerms;
         info->ownerPerms = requestV3->ownerPerms;
         info->groupPerms = requestV3->groupPerms;
         info->otherPerms = requestV3->otherPerms;
	 LOG(4, ("HgfsUnpackCreateDirRequest: HGFS_OP_CREATE_DIR_V3\n"));
         break;
      }
   case HGFS_OP_CREATE_DIR_V2:
      {
         HgfsRequestCreateDirV2 *requestV2 = 
            (HgfsRequestCreateDirV2 *)packetIn;

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV2);
         extra = packetSize - sizeof *requestV2;

         if (!(requestV2->mask & HGFS_CREATE_DIR_VALID_FILE_NAME)) {
            /* We do not support requests without a valid file name. */
            return FALSE;
         }

         /*
          * requestV2->fileName.length is user-provided, so this test must be 
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
         info->mask = requestV2->mask;
         info->cpName = requestV2->fileName.name;
         info->cpNameSize = requestV2->fileName.length;
         info->specialPerms = requestV2->specialPerms;
         info->ownerPerms = requestV2->ownerPerms;
         info->groupPerms = requestV2->groupPerms;
         info->otherPerms = requestV2->otherPerms;
         break;
      }
   case HGFS_OP_CREATE_DIR:
      {
         HgfsRequestCreateDir *requestV1;
         requestV1 = (HgfsRequestCreateDir *)packetIn;

         /* Enforced by the dispatch function. */
         ASSERT(packetSize >= sizeof *requestV1);
         extra = packetSize - sizeof *requestV1;

         /*
          * requestV1->fileName.length is user-provided, so this test must be 
          * carefully written to prevent wraparounds.
          */

         if (requestV1->fileName.length > extra) {
            /* The input packet is smaller than the request. */
            return FALSE;
         }

         /* For CreateDirV1 requests, we know exactly what fields we expect. */
         info->mask = 
            HGFS_CREATE_DIR_VALID_OWNER_PERMS |
            HGFS_CREATE_DIR_VALID_FILE_NAME;
         info->cpName = requestV1->fileName.name;
         info->cpNameSize = requestV1->fileName.length;
         info->ownerPerms = requestV1->permissions;
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
 *    Since the structure of the create dir reply packet hasn't changed in
 *    version 2 of the protocol, HgfsReplyCreateDirV2 is identical to
 *    HgfsReplyCreateDir. So use HgfsReplyCreateDir type to access packetIn to
 *    keep the code simple.
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
HgfsPackCreateDirReply(char *packetOut,           // IN/OUT: outgoing packet
                       size_t *packetSize)        // IN/OUT: size of packet

{
   HgfsReplyCreateDir *reply = (HgfsReplyCreateDir *)packetOut;

   ASSERT(packetOut);
   ASSERT(packetSize);

   *packetSize = sizeof *reply;

   return TRUE;
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
HgfsServer_InitState(void)
{
   unsigned int i;

   /* Initialize filenode freelist */
   DblLnkLst_Init(&nodeFreeList);

   /* Initialize filenode cachelist. */
   DblLnkLst_Init(&nodeCachedList);

   maxCachedOpenNodes = Config_GetLong(MAX_CACHED_FILENODES,
                                       "hgfs.fdCache.maxNodes");

   /* Allocate array of FileNodes and add them to free list */
   numNodes = NUM_FILE_NODES;
   ASSERT(nodeArray == NULL);
   nodeArray = (HgfsFileNode *)calloc(numNodes, sizeof(HgfsFileNode));
   if (!nodeArray) {
      LOG(4, ("No memory allocating file nodes\n"));
      goto error;
   }

   for (i = 0; i < numNodes; i++) {
      DblLnkLst_Init(&nodeArray[i].links);
      /* Append at the end of the list */
      DblLnkLst_LinkLast(&nodeFreeList, &nodeArray[i].links);
   }

   /* Initialize search freelist */
   DblLnkLst_Init(&searchFreeList);

   /* Allocate array of searches and add them to free list */
   numSearches = NUM_SEARCHES;
   ASSERT(searchArray == NULL);
   searchArray = (HgfsSearch *)calloc(numSearches, sizeof(HgfsSearch));
   if (!searchArray) {
      LOG(4, ("No memory allocating searches\n"));
      goto error;
   }

   for (i = 0; i < numSearches; i++) {
      DblLnkLst_Init(&searchArray[i].links);
      /* Append at the end of the list */
      DblLnkLst_LinkLast(&searchFreeList, &searchArray[i].links);
   }

#ifndef VMX86_TOOLS
   if (Config_GetBool(FALSE, "hgfs.alwaysUseHostTime")) {
      alwaysUseHostTime = TRUE;
   }
#endif  // !defined(VMX86_TOOLS)

   if (!SyncMutex_Init(&hgfsNodeArrayLock, NULL)) {
      LOG(4, ("Could not create mutex for node array\n"));
      goto error;
   }
   if (!SyncMutex_Init(&hgfsSearchArrayLock, NULL)) {
      LOG(4, ("Could not create mutex for search array\n"));
      SyncMutex_Destroy(&hgfsNodeArrayLock);
      goto error;
   }
   if (!SyncMutex_Init(&hgfsIOLock, NULL)) {
      LOG(4, ("Could not create mutex for IO protection\n"));
      SyncMutex_Destroy(&hgfsNodeArrayLock);
      SyncMutex_Destroy(&hgfsSearchArrayLock);
      goto error;
   }

   if (!HgfsServerPlatformInit()) {
      LOG(4, ("Could not initialize server platform specific \n"));
      SyncMutex_Destroy(&hgfsIOLock);
      SyncMutex_Destroy(&hgfsNodeArrayLock);
      SyncMutex_Destroy(&hgfsSearchArrayLock);
      goto error;
   }

   return TRUE;

  error:
   free(searchArray);
   free(nodeArray);
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServer_ExitState --
 *
 *    Cleanup the global server state.
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
   unsigned int i;

   /* Recycle all objects that are still in use, then destroy object pools */
   for (i = 0; i < numNodes; i++) {
      HgfsHandle handle;

      if (nodeArray[i].state == FILENODE_STATE_UNUSED) {
         continue;
      }
      
      handle = HgfsFileNode2Handle(&nodeArray[i]);
      HgfsRemoveFromCacheInternal(handle);
      HgfsFreeFileNodeInternal(handle);
   }
   free(nodeArray);
   nodeArray = NULL;

   for (i = 0; i < numSearches; i++) {
      if (DblLnkLst_IsLinked(&searchArray[i].links)) {
         continue;
      }
      HgfsRemoveSearchInternal(&searchArray[i]);
   }
   free(searchArray);
   searchArray = NULL;
   
   SyncMutex_Destroy(&hgfsIOLock);
   SyncMutex_Destroy(&hgfsSearchArrayLock);
   SyncMutex_Destroy(&hgfsNodeArrayLock);

   HgfsServerPlatformDestroy();
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

   LOG(4, ("HgfsServerOplockBreak: entered\n"));

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
      LOG(4, ("HgfsServerOplockBreak: file is not in the cache\n"));
      goto free_and_exit;
   }

   if (!HgfsHandle2ServerLock(hgfsHandle, &lock)) {
      LOG(4, ("HgfsServerOplockBreak: could not retrieve node's lock info.\n"));
      goto free_and_exit;
   }

   if (lock == HGFS_LOCK_NONE) {
      LOG(4, ("HgfsServerOplockBreak: the file does not have a server lock.\n"));
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
      LOG(4, ("HgfsServerOplockBreak: could not allocate memory.\n"));
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
   if (HgfsServerManager_SendRequest(requestBuffer,
                                     sizeof *request,
                                     HgfsServerOplockBreakReply,
                                     lockData)) {
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

   printf("TestNodeFreeList: begin >>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

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
   printf("TestNodeFreeList: end <<<<<<<<<<<<<<<<<<<<<<<<<< \n");
}


void
TestSearchFreeList(void)
{
   HgfsHandle array[10 * NUM_SEARCHES];
   HgfsSearch *search;
   unsigned int i;

   printf("TestSearchFreeList: begin >>>>>>>>>>>>>>>>>>>>>>>>>>>\n");

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
   printf("TestSearchFreeList: end <<<<<<<<<<<<<<<<<<<<<<<<<< \n");
}
#endif
