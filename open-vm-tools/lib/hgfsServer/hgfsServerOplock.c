/*********************************************************
 * Copyright (C) 2012-2020 VMware, Inc. All rights reserved.
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
 * hgfsServerOplock.c --
 *
 *      HGFS server opportunistic lock support that is common to all platforms.
 */

#if defined(__APPLE__)
#define _DARWIN_USE_64_BIT_INODE
#endif


#include <stdlib.h>
#include <stdio.h>

#include "vmware.h"
#include "str.h"
#include "cpName.h"
#include "cpNameLite.h"
#include "hgfsServerInt.h"
#include "hgfsServerOplockInt.h"



/*
 * Local data
 */

/* Indicates if the oplock module is initialized. */
static Bool gOplockInit = FALSE;

/*
 * Global data
 */


/*
 * Local functions
 */


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOplockInit --
 *
 *      Set up any oplock related state used for HGFS server.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerOplockInit(void)
{
   if (gOplockInit) {
      return TRUE;
   }

   gOplockInit = HgfsPlatformOplockInit();
   return gOplockInit;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOplockDestroy --
 *
 *      Tear down any oplock related state used for HGFS server.
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
HgfsServerOplockDestroy(void)
{
   if (!gOplockInit) {
      return;
   }

   /* Tear down oplock state, so we no longer catch signals. */
   HgfsPlatformOplockDestroy();

   gOplockInit = FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerOplockIsInited --
 *
 *      Check if the oplock related state is set up.
 *
 * Results:
 *      TRUE if the oplock related state is set up.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsServerOplockIsInited(void)
{
   return gOplockInit;
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
                      HgfsLockType *lock)       // OUT: Server lock
{
#ifdef HGFS_OPLOCKS
   Bool found = FALSE;
   HgfsFileNode *fileNode;

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
                      HgfsLockType *serverLock,         // OUT: Existing oplock
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
         LOG(4, "Found file with a lock: %s\n", utf8Name);
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
   ServerLockData *lockData = clientData;

   ASSERT(packetIn);
   ASSERT(clientData);

   if (packetSize < sizeof *reply) {
      return;
   }
   reply = (HgfsReplyServerLockChange *)packetIn;

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
   HgfsLockType lock;

   LOG(4, "%s: entered\n", __FUNCTION__);

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
      LOG(4, "%s: file is not in the cache\n", __FUNCTION__);
      goto free_and_exit;
   }

   if (!HgfsHandle2ServerLock(hgfsHandle, &lock)) {
      LOG(4, "%s: could not retrieve node's lock info.\n", __FUNCTION__);
      goto free_and_exit;
   }

   if (lock == HGFS_LOCK_NONE) {
      LOG(4, "%s: the file does not have a server lock.\n", __FUNCTION__);
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
      LOG(4, "%s: could not allocate memory.\n", __FUNCTION__);
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
