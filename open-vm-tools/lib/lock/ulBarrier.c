/*********************************************************
 * Copyright (C) 2010-2019 VMware, Inc. All rights reserved.
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

struct BarrierContext
{
   uint32           count;    // Number of threads currently in this context
   MXUserCondVar   *condVar;  // Threads within this context are parked here
};

typedef struct BarrierContext BarrierContext;

struct MXUserBarrier
{
   MXUserHeader     header;        // Barrier's ID information
   MXUserExclLock  *lock;          // Barrier's (internal) lock
   uint32           configCount;   // Hold until this many threads arrive
   volatile uint32  curContext;    // Arrivals go to this context
   BarrierContext   contexts[2];   // The normal and abnormal contexts
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
   uint32 curContext;
   MXUserBarrier *barrier = (MXUserBarrier *) header;

   Warning("%s: Barrier @ 0x%p\n", __FUNCTION__, barrier);

   Warning("\tsignature 0x%X\n", barrier->header.signature);
   Warning("\tname %s\n", barrier->header.name);
   Warning("\trank 0x%X\n", barrier->header.rank);
   Warning("\tserial number %"FMT64"u\n", barrier->header.serialNumber);

   Warning("\tlock 0x%p\n", barrier->lock);
   Warning("\tconfigured count %u\n", barrier->configCount);
   curContext = barrier->curContext;

   Warning("\tcurrent context %u\n", curContext);

   Warning("\tcontext[%u] count %u\n", curContext,
           barrier->contexts[curContext].count);
   Warning("\tcontext[%u] condVar 0x%p\n", curContext,
           &barrier->contexts[curContext].condVar);

   curContext = (curContext + 1) & 0x1;

   Warning("\tcontext[%u] count %u\n", curContext,
           barrier->contexts[curContext].count);
   Warning("\tcontext[%u] condVar 0x%p\n", curContext,
           &barrier->contexts[curContext].condVar);
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
MXUser_CreateBarrier(const char *userName,  // IN: shall be known as
                     MX_Rank rank,          // IN: internal lock's rank
                     uint32 count)          // IN:
{
   char *properName;
   MXUserBarrier *barrier;

   ASSERT(count);

   barrier = Util_SafeCalloc(1, sizeof *barrier);

   if (userName == NULL) {
      properName = Str_SafeAsprintf(NULL, "Barrier-%p", GetReturnAddress());
   } else {
      properName = Util_SafeStrdup(userName);
   }

   barrier->lock = MXUser_CreateExclLock(properName, rank);
   barrier->contexts[0].condVar = MXUser_CreateCondVarExclLock(barrier->lock);
   barrier->contexts[1].condVar = MXUser_CreateCondVarExclLock(barrier->lock);

   barrier->configCount = count;
   barrier->curContext = 0;

   barrier->header.signature = MXUserGetSignature(MXUSER_TYPE_BARRIER);
   barrier->header.name = properName;
   barrier->header.rank = rank;
   barrier->header.serialNumber = MXUserAllocSerialNumber();
   barrier->header.dumpFunc = MXUserDumpBarrier;
   barrier->header.statsFunc = NULL;

   MXUserAddToList(&barrier->header);

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
MXUser_DestroyBarrier(MXUserBarrier *barrier)  // IN/OUT:
{
   if (LIKELY(barrier != NULL)) {
      MXUserValidateHeader(&barrier->header, MXUSER_TYPE_BARRIER);

      if ((barrier->contexts[0].count != 0) ||
          (barrier->contexts[1].count != 0)) {
         MXUserDumpAndPanic(&barrier->header,
                            "%s: Attempted destroy on barrier while in use\n",
                            __FUNCTION__);
      }

      barrier->header.signature = 0;  // just in case...

      MXUserRemoveFromList(&barrier->header);

      MXUser_DestroyCondVar(barrier->contexts[0].condVar);
      MXUser_DestroyCondVar(barrier->contexts[1].condVar);
      MXUser_DestroyExclLock(barrier->lock);

      free(barrier->header.name);
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
 *      "Nobody comes out until everyone goes in."
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
   BarrierContext *ptr;
   uint32 context;

   ASSERT(barrier);
   MXUserValidateHeader(&barrier->header, MXUSER_TYPE_BARRIER);

   MXUser_AcquireExclLock(barrier->lock);

   context = barrier->curContext;
   ptr = &barrier->contexts[context];

   ptr->count++;

   if (ptr->count == barrier->configCount) {
      /*
       * The last thread has entered; release the other threads
       *
       * Flip the current context. Should a thread leave the barrier and
       * enter the barrier while the barrier is "emptying" the thread will
       * park on the condVar that is not "emptying". Eventually everything
       * will "work out" and all of the threads will be parked on the opposite
       * context's condVar.
       */

      barrier->curContext = (context + 1) & 0x1;
      ASSERT(barrier->contexts[barrier->curContext].count == 0);

      /* Wake up all of the waiting threads. */
      MXUser_BroadcastCondVar(ptr->condVar);
   } else {
      /*
       * Not the last thread in... wait/sleep until the last thread appears.
       * Protect against spurious wakeups.
       */

      while (barrier->curContext == context) {
         MXUser_WaitCondVarExclLock(barrier->lock, ptr->condVar);
      }
   }

   ptr->count--;

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

   barrier = Atomic_ReadPtr(barrierStorage);

   if (UNLIKELY(barrier == NULL)) {
      MXUserBarrier *newBarrier = MXUser_CreateBarrier(name, rank, count);

      barrier = Atomic_ReadIfEqualWritePtr(barrierStorage, NULL,
                                           (void *) newBarrier);

      if (barrier) {
         MXUser_DestroyBarrier(newBarrier);
      } else {
         barrier = Atomic_ReadPtr(barrierStorage);
      }
   }

   return barrier;
}

