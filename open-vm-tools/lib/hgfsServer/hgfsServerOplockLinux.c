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
 * hgfsServerOplockLinux.c --
 *
 *      HGFS server opportunistic lock support for the Linux platform.
 */


#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "vmware.h"
#include "err.h"
#include "hgfsServerInt.h"
#include "hgfsServerOplockInt.h"

#ifdef HGFS_OPLOCKS
#   include <signal.h>
#   include "sig.h"
#endif


/*
 * Local data
 */

/*
 * Global data
 */


/*
 * Local functions
 */

#ifdef HGFS_OPLOCKS
static void HgfsServerSigOplockBreak(int sigNum,
                                     siginfo_t *info,
                                     ucontext_t *u,
                                     void *clientData);
#endif



/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPlatformOplockInit --
 *
 *      Set up any state needed to start Linux HGFS server oplock support.
 *
 * Results:
 *      TRUE always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsPlatformOplockInit(void)
{
#ifdef HGFS_OPLOCKS
   /* Register a signal handler to catch oplock break signals. */
   Sig_Callback(SIGIO, SIG_SAFE, HgfsServerSigOplockBreak, NULL);
#endif
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsPlatformOplockDestroy --
 *
 *      Tear down any state used for Linux HGFS server.
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
HgfsPlatformOplockDestroy(void)
{
#ifdef HGFS_OPLOCKS
   /* Tear down oplock state, so we no longer catch signals. */
   Sig_Callback(SIGIO, SIG_NOHANDLER, NULL, NULL);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsRemoveAIOServerLock --
 *
 *      Remove an oplock for an open file.
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
HgfsRemoveAIOServerLock(fileDesc fileDesc)  // IN:
{
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAcquireAIOServerLock --
 *
 *    Acquire an oplock for an open file and register the break oplock event.
 *
 * Results:
 *    TRUE on success. serverLock contains the type of the lock acquired.
 *    FALSE on failure. serverLock is HGFS_LOCK_NONE.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsAcquireAIOServerLock(fileDesc fileDesc,            // IN:
                         HgfsSessionInfo *session,     // IN: Session info
                         HgfsLockType *serverLock,     // IN/OUT: Oplock asked for/granted
                         HgfsOplockCallback callback,  // IN: call back
                         void *data)                   // IN: parameter for call back
{
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAcquireServerLock --
 *
 *    Acquire a lease for the open file. Typically we try and get the exact
 *    lease desired, but if the client asked for HGFS_LOCK_OPPORTUNISTIC, we'll
 *    take the "best" lease we can get.
 *
 * Results:
 *    TRUE on success. serverLock contains the type of the lock acquired.
 *    FALSE on failure. serverLock is HGFS_LOCK_NONE.
 *
 *    XXX: This function has the potential to return per-platform error codes,
 *    but since it is opportunistic by nature, it isn't necessary to do so.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
HgfsAcquireServerLock(fileDesc fileDesc,            // IN: OS handle
                      HgfsSessionInfo *session,     // IN: session info
                      HgfsLockType *serverLock)     // IN/OUT: Oplock asked for/granted
{
#ifdef HGFS_OPLOCKS
   HgfsLockType desiredLock;
   int leaseType, error;

   ASSERT(serverLock);
   ASSERT(session);

   desiredLock = *serverLock;

   if (desiredLock == HGFS_LOCK_NONE) {
      return TRUE;
   }

   if (!HgfsIsServerLockAllowed(session)) {
      return FALSE;
   }

   /*
    * First tell the kernel which signal to send us. SIGIO is already the
    * default, but if we skip this step, we won't get the siginfo_t when
    * a lease break occurs.
    *
    * XXX: Do I need to do fcntl(fileDesc, F_SETOWN, getpid())?
    */
   if (fcntl(fileDesc, F_SETSIG, SIGIO)) {
      error = errno;
      Log("%s: Could not set SIGIO as the desired lease break signal for "
          "fd %d: %s\n", __FUNCTION__, fileDesc, Err_Errno2String(error));

      return FALSE;
   }

   /*
    * If the client just wanted the best lock possible, start off with a write
    * lease and move down to a read lease if that was unavailable.
    */
   if ((desiredLock == HGFS_LOCK_OPPORTUNISTIC) ||
       (desiredLock == HGFS_LOCK_EXCLUSIVE)) {
      leaseType = F_WRLCK;
   } else if (desiredLock  == HGFS_LOCK_SHARED) {
      leaseType = F_RDLCK;
   } else {
      LOG(4, "%s: Unknown server lock\n", __FUNCTION__);

      return FALSE;
   }
   if (fcntl(fileDesc, F_SETLEASE, leaseType)) {
      /*
       * If our client was opportunistic and we failed to get his lease because
       * someone else is already writing or reading to the file, try again with
       * a read lease.
       */
      if (desiredLock == HGFS_LOCK_OPPORTUNISTIC &&
          (errno == EAGAIN || errno == EACCES)) {
         leaseType = F_RDLCK;
         if (fcntl(fileDesc, F_SETLEASE, leaseType)) {
            error = errno;
            LOG(4, "%s: Could not get any opportunistic lease for fd %d: %s\n",
                __FUNCTION__, fileDesc, Err_Errno2String(error));

            return FALSE;
         }
      } else {
         error = errno;
         LOG(4, "%s: Could not get %s lease for fd %d: %s\n",
             __FUNCTION__, leaseType == F_WRLCK ? "write" : "read",
             fileDesc, Err_Errno2String(errno));

         return FALSE;
      }
   }

   /* Got a lease of some kind. */
   LOG(4, "%s: Got %s lease for fd %d\n", __FUNCTION__,
       leaseType == F_WRLCK ? "write" : "read", fileDesc);
   *serverLock = leaseType == F_WRLCK ? HGFS_LOCK_EXCLUSIVE : HGFS_LOCK_SHARED;
   return TRUE;
#else
   return FALSE;
#endif
}


#ifdef HGFS_OPLOCKS
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAckOplockBreak --
 *
 *    Platform-dependent implementation of oplock break acknowledgement.
 *    This function gets called when the oplock break rpc command is completed.
 *    The rpc oplock break command (HgfsServerOplockBreak) is in hgfsServer.c
 *
 *    On Linux, we use fcntl() to downgrade the lease. Then we update the node
 *    cache, free the clientData, and call it a day.
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
HgfsAckOplockBreak(ServerLockData *lockData, // IN: server lock info
                   HgfsLockType replyLock)   // IN: client has this lock
{
   int fileDesc, newLock;
   HgfsLockType actualLock;

   ASSERT(lockData);
   fileDesc = lockData->fileDesc;
   LOG(4, "%s: Acknowledging break on fd %d\n", __FUNCTION__, fileDesc);

   /*
    * The Linux server supports lock downgrading. We only downgrade to a shared
    * lock if our previous call to fcntl() said we could, and if the client
    * wants to downgrade to a shared lock. Otherwise, we break altogether.
    */
   if (lockData->serverLock == HGFS_LOCK_SHARED &&
       replyLock == HGFS_LOCK_SHARED) {
      newLock = F_RDLCK;
      actualLock = replyLock;
   } else {
      newLock = F_UNLCK;
      actualLock = HGFS_LOCK_NONE;
   }

   /* Downgrade or acknowledge the break altogether. */
   if (fcntl(fileDesc, F_SETLEASE, newLock) == -1) {
      int error = errno;
      Log("%s: Could not break lease on fd %d: %s\n",
          __FUNCTION__, fileDesc, Err_Errno2String(error));
   }

   /* Cleanup. */
   HgfsUpdateNodeServerLock(fileDesc, actualLock);
   free(lockData);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsServerSigOplockBreak --
 *
 *      Handle a pending oplock break. Called from the VMX poll loop context.
 *      All we really do is set up the state for an oplock break and call
 *      HgfsServerOplockBreak which will do the rest of the work.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
HgfsServerSigOplockBreak(int sigNum,       // IN: Signal number
                         siginfo_t *info,  // IN: Additional info about signal
                         ucontext_t *u,    // IN: Interrupted context (regs etc)
                         void *clientData) // IN: Ignored
{
   ServerLockData *lockData;
   int newLease, fd;
   HgfsLockType newServerLock;

   ASSERT(sigNum == SIGIO);
   ASSERT(info);
   ASSERT(clientData == NULL);

   fd = info->si_fd;
   LOG(4, "%s: Received SIGIO for fd %d\n", __FUNCTION__, fd);

   /*
    * We've got all we need from the signal handler, let it continue handling
    * signals of this type.
    */
   Sig_Continue(sigNum);

   /*
    * According to locks.c in kernel source, doing F_GETLEASE when a lease
    * break is pending will return the new lease we should use. It'll be
    * F_RDLCK if we can downgrade, or F_UNLCK if we should break altogether.
    */
   newLease = fcntl(fd, F_GETLEASE);
   if (newLease == F_RDLCK) {
      newServerLock = HGFS_LOCK_SHARED;
   } else if (newLease == F_UNLCK) {
      newServerLock = HGFS_LOCK_NONE;
   } else if (newLease == -1) {
      int error = errno;
      Log("%s: Could not get old lease for fd %d: %s\n", __FUNCTION__,
          fd, Err_Errno2String(error));
      goto error;
   } else {
      Log("%s: Unexpected reply to get lease for fd %d: %d\n",
          __FUNCTION__, fd, newLease);
      goto error;
   }

   /*
    * Setup a ServerLockData struct so that we can make use of
    * HgfsServerOplockBreak which does the heavy lifting of discovering which
    * HGFS handle we're interested in breaking, sending the break, receiving
    * the acknowledgement, and firing the platform-specific acknowledgement
    * function (where we'll downgrade the lease).
    */
   lockData = malloc(sizeof *lockData);
   if (lockData) {
      lockData->fileDesc = fd;
      lockData->serverLock = newServerLock;
      lockData->event = 0; // not needed

      /*
       * Relinquish control of this data. It'll get freed later, when the RPC
       * command completes.
       */
      HgfsServerOplockBreak(lockData);
      return;
   } else {
      Log("%s: Could not allocate memory for lease break on behalf of fd %d\n",
          __FUNCTION__, fd);
   }

  error:
   /* Clean up as best we can. */
   fcntl(fd, F_SETLEASE, F_UNLCK);
   HgfsUpdateNodeServerLock(fd, HGFS_LOCK_NONE);
}
#endif /* HGFS_OPLOCKS */
