/*********************************************************
 * Copyright (C) 2010 VMware, Inc. All rights reserved.
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

#define MXUSER_BARRIER_SIGNATURE 0x52524142 // 'BARR' in memory

struct MXUserBarrier
{
   MXUserHeader     header;
   MXUserExclLock  *lock;
   Bool             emptying;
   MXUserCondVar   *condVar;
   uint32           configCount;   // Hold until this many threads arrive.
   uint32           checkInCount;  // Number of threads currently in barrier
};


/*
 *-----------------------------------------------------------------------------
 *
 * MXUserDumpBarrier --
 *
 *      Dump a barrier.
 *
 * Results:
 *      A dump.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
MXUserDumpBarrier(MXUserHeader *header)  // IN:
{
   MXUserBarrier *barrier = (MXUserBarrier *) header;

   Warning("%s: Barrier @ 0x%p\n", __FUNCTION__, barrier);

   Warning("\tsignature 0x%X\n", barrier->header.signature);
   Warning("\tname %s\n", barrier->header.name);
   Warning("\trank 0x%X\n", barrier->header.rank);

   Warning("\tlock %p\n", barrier->lock);
   Warning("\tcondVar %p\n", barrier->condVar);

   Warning("\tconfig count %u\n", barrier->configCount);
   Warning("\tcheck in count %u\n", barrier->checkInCount);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateBarrier --
 *
 *      Create a computational barrier.
 *
 *      The barriers are self regenerating - they do not need to be
 *      initialized or reset after creation.
 *
 * Results:
 *      A pointer to a barrier.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

MXUserBarrier *
MXUser_CreateBarrier(const char *userName,  // IN:
                     MX_Rank rank,          // IN:
                     uint32 count)          // IN:
{
   char *properName;
   MXUserBarrier *barrier;

   ASSERT(count);

   barrier = Util_SafeCalloc(1, sizeof(*barrier));

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "Barrier-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   barrier->lock = MXUser_CreateExclLock(properName, rank);

   if (barrier->lock == NULL) {
      free(properName);
      free(barrier);

      return NULL;
   }

   barrier->condVar = MXUser_CreateCondVarExclLock(barrier->lock);

   if (barrier->condVar == NULL) {
      MXUser_DestroyExclLock(barrier->lock);

      free(properName);
      free(barrier);

      return NULL;
   }

   barrier->configCount = count;
   barrier->emptying = FALSE;

   barrier->header.name = properName;
   barrier->header.signature = MXUSER_BARRIER_SIGNATURE;
   barrier->header.rank = rank;
   barrier->header.dumpFunc = MXUserDumpBarrier;

#if defined(MXUSER_STATS)
   barrier->header.statsFunc = NULL;
   barrier->header.identifier = MXUserAllocID();
#endif

   return barrier;
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_DestroyBarrier --
 *
 *      Destroy a barrier.
 *
 * Results:
 *      The barrier is destroyed. Don't try to use the pointer again.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_DestroyBarrier(MXUserBarrier *barrier)  // IN:
{
   if (LIKELY(barrier != NULL)) {
      ASSERT(barrier->header.signature == MXUSER_BARRIER_SIGNATURE);

      if (barrier->checkInCount != 0) {
         MXUserDumpAndPanic(&barrier->header,
                            "%s: Attempted destroy on barrier while in use\n",
                            __FUNCTION__);
      }

      MXUser_DestroyCondVar(barrier->condVar);
      MXUser_DestroyExclLock(barrier->lock);

      barrier->header.signature = 0;  // just in case...
      free((void *) barrier->header.name);  // avoid const warnings
      barrier->header.name = NULL;
      free(barrier);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_EnterBarrier --
 *
 *      Enter a barrier
 *
 *      All threads entering the barrier will be suspended until the number
 *      threads that have entered reaches the configured number upon which
 *      time all of the threads will return from this routine.
 *
 *      "Nobody comes out until everone goes in."
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The caller may sleep.
 *
 *-----------------------------------------------------------------------------
 */

void
MXUser_EnterBarrier(MXUserBarrier *barrier)  // IN/OUT:
{
   ASSERT(barrier && (barrier->header.signature == MXUSER_BARRIER_SIGNATURE));
   ASSERT(!barrier->emptying);

   MXUser_AcquireExclLock(barrier->lock);

   barrier->checkInCount++;

   barrier->emptying = (barrier->checkInCount == barrier->configCount);

   if (barrier->emptying) {
      /* The last thread has entered; release the other threads */
      MXUser_BroadcastCondVar(barrier->condVar);
   } else {
      /* Not the last thread in... sleep until the last thread appears */
      MXUser_WaitCondVarExclLock(barrier->lock, barrier->condVar);
   }

   barrier->checkInCount--;

   if (barrier->checkInCount == 0) {
      barrier->emptying = FALSE;
   }

   MXUser_ReleaseExclLock(barrier->lock);
}


/*
 *-----------------------------------------------------------------------------
 *
 * MXUser_CreateSingletonBarrier --
 *
 *      Ensures that the specified backing object (Atomic_Ptr) contains a
 *      barrier. This is useful for modules that need to protect something
 *      with a barrier but don't have an existing Init() entry point where a
 *      barrier can be created.
 *
 * Results:
 *      A pointer to the requested barrier.
 *
 * Side effects:
 *      Generally the barrier's resources are intentionally leaked (by design).
 *
 *-----------------------------------------------------------------------------
 */

MXUserBarrier *
MXUser_CreateSingletonBarrier(Atomic_Ptr *barrierStorage,  // IN/OUT:
                              const char *name,            // IN:
                              MX_Rank rank,                // IN:
                              uint32 count)                // IN:
{
   MXUserBarrier *barrier;

   ASSERT(barrierStorage);

   barrier = (MXUserBarrier *) Atomic_ReadPtr(barrierStorage);

   if (UNLIKELY(barrier == NULL)) {
      MXUserBarrier *before;

      barrier = MXUser_CreateBarrier(name, rank, count);

      before = (MXUserBarrier *) Atomic_ReadIfEqualWritePtr(barrierStorage,
                                                            NULL,
                                                            (void *) barrier);

      if (before) {
         MXUser_DestroyBarrier(barrier);

         barrier = before;
      }
   }

   return barrier;
}

