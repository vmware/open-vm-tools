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

#include "vmware.h"
#include "str.h"
#include "util.h"
#include "userlock.h"
#include "ulInt.h"
#include "mutexInt.h"

#if defined(VMX86_DEBUG)
#define MXUSER_MAX_LOCKS_PER_THREAD 24

typedef struct {
   uint32         lockNum;
   MXUserHeader  *lockArray[MXUSER_MAX_LOCKS_PER_THREAD];
} MXUserPerThread;

static MXUserPerThread mxUserPerThread[VTHREAD_MAX_THREADS];
#endif


/*
 * Return an invalid thread ID until lib/thread is initialized.
 *
 * XXX
 *
 * VThread_CurID cannot be called before VThread_Init is called; doing so
 * causes assertion failures in some programs. This will go away when
 * lib/nothread goes away - we'll assign "dense", rationalized VMware
 * thread IDs without the distinction of lib/thread and lib/nothread.
 */

static VThreadID
MXUserDummyCurID(void)
{
   return VTHREAD_INVALID_ID;
}

VThreadID (*MXUserThreadCurID)(void) = MXUserDummyCurID;

void
MXUserIDHack(void)
{
   MXUserThreadCurID = VThread_CurID;
}


#if defined(VMX86_DEBUG)
/*
 *-----------------------------------------------------------------------------
 *
 * MXUserAcquireRankCheck
 *
 *      Perform rank checking for lock acquisition.
 *
 * Results:
 *      A panic on rank violation.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserAcquireRankCheck(MXUserHeader *header)  // IN:
{
   VThreadID tid = (*MXUserThreadCurID)();
   MXUserPerThread *perThread = &mxUserPerThread[tid];

   /* Hack until VThread_CurID is fixed */
   if ((tid == VTHREAD_INVALID_ID) || (tid == VTHREAD_OTHER_ID)) {
      return;
   }

   ASSERT_NOT_IMPLEMENTED(perThread->lockNum < MXUSER_MAX_LOCKS_PER_THREAD);

   perThread->lockArray[perThread->lockNum++] = header;

   if (header->lockRank != RANK_UNRANKED) {
      uint32 i;
      MX_Rank maxRank;

      /* Check MX locks when they are present */
      maxRank = (mxState) ? MX_CurrentRank() : RANK_UNRANKED;

      for (i = 0; i < perThread->lockNum; i++) {
         maxRank = MAX(perThread->lockArray[i]->lockRank, maxRank);
      }

      if (header->lockRank <= maxRank) {
         Warning("%s: lock rank violation by thread %s\n", __FUNCTION__,
                 VThread_CurName());
         Warning("%s: locks held:\n", __FUNCTION__);

         for (i = 0; i < perThread->lockNum; i++) {
            MXUserHeader *hdr = perThread->lockArray[i];

            Warning("\tMXUser lock %s (@%p) rank %d\n", hdr->lockName, hdr,
                    hdr->lockRank);
         }

         if (mxState) {
            MX_LockID lid;

            FOR_ALL_LOCKS_HELD(tid, lid) {
               MXPerLock *perLock = GetPerLock(lid);

               Warning("\tMX lock %s (@%u) rank %d\n", perLock->name,
                       lid, perLock->rank);
            }
         }

         MXUserDumpAndPanic(header, "%s: rank violation\n", __FUNCTION__);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserReleaseRankCheck
 *
 *      Perform rank checking for lock release.
 *
 * Results:
 *      A panic.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserReleaseRankCheck(MXUserHeader *header)  // IN:
{
   uint32 i;
   VThreadID tid = (*MXUserThreadCurID)();
   MXUserPerThread *perThread = &mxUserPerThread[tid];

   /* Hack until VThread_CurID is fixed */
   if ((tid == VTHREAD_INVALID_ID) || (tid == VTHREAD_OTHER_ID)) {
      return;
   }

   for (i = 0; i < perThread->lockNum; i++) {
      if (perThread->lockArray[i] == header) {
         break;
      }
   }

   ASSERT_NOT_IMPLEMENTED(i < perThread->lockNum);  // better find it

   if (i < perThread->lockNum - 1) {
      perThread->lockArray[i] = perThread->lockArray[perThread->lockNum - 1];
   }

   perThread->lockNum--;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpAndPanic
 *
 *      Dump a lock, print a message and die
 *
 * Results:
 *      A panic.
 *
 * Side effects:
 *      Manifold.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUserDumpAndPanic(MXUserHeader *header,  // IN:
                   const char *fmt,       // IN:
                   ...)                   // IN:
{
   char *msg;
   va_list ap;

   (*header->lockDumper)(header);

   va_start(ap, fmt);
   msg = Str_SafeVasprintf(NULL, fmt, ap);
   va_end(ap);

   Panic("%s", msg);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserIsAllUnlocked --
 *
 *      Is the lock currently completely unlocked?
 *
 * Results:
 *      The lock is acquired.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
MXUserIsAllUnlocked(const MXUserRWLock *lock)  // IN:
{
   uint32 i;

   for (i = 0; i < VTHREAD_MAX_THREADS; i++) {
      if (lock->lockTaken[i] != RW_UNLOCKED) {
         return FALSE;
      }
   }

   return TRUE;
}
