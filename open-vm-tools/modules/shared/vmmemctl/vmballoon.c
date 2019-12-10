/*********************************************************
 * Copyright (C) 2000-2019 VMware, Inc. All rights reserved.
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

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * vmballoon.c --
 *
 *      VMware physical memory management driver for Unix-ish
 *      (Linux, FreeBSD, Solaris, Mac OS) guests. The driver acts like
 *      a "balloon" that can be inflated to reclaim physical pages by
 *      reserving them in the guest and invalidating them in the
 *      monitor, freeing up the underlying machine pages so they can
 *      be allocated to other guests.  The balloon can also be
 *      deflated to allow the guest to use more physical memory.
 *      Higher level policies can control the sizes of balloons in VMs
 *      in order to manage physical memory resources.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Includes
 */

#include "os.h"
#include "backdoor.h"
#include "backdoor_balloon.h"
#include "vmballoon.h"

/*
 * Constants
 */

#ifndef NULL
#define NULL 0
#endif

/*
 * When guest is under memory pressure, use a reduced page allocation
 * rate for next several cycles.
 */
#define SLOW_PAGE_ALLOCATION_CYCLES     4

/* Maximum number of page allocations without yielding processor */
#define BALLOON_ALLOC_YIELD_THRESHOLD   1024

/*
 * Balloon operations
 */
static void BalloonPageFree(Balloon *b, int isLargePage);
static void BalloonAdjustSize(Balloon *b, uint64 target);
static void BalloonReset(Balloon *b);

static void BalloonAddPage(Balloon *b, uint16 idx, PageHandle page);
static void BalloonAddPageBatched(Balloon *b, uint16 idx, PageHandle page);
static int  BalloonLock(Balloon *b, uint16 nPages, int isLargePage,
                        uint64 *target);
static int  BalloonLockBatched(Balloon *b, uint16 nPages, int isLargePages,
                               uint64 *target);
static int  BalloonUnlock(Balloon *b, uint16 nPages, int isLargePages,
                          uint64 *target);
static int  BalloonUnlockBatched(Balloon *b, uint16 nPages, int IsLargePages,
                                 uint64 *target);

/*
 * Globals
 */

static Balloon globalBalloon;

static const BalloonOps balloonOps = {
   .addPage = BalloonAddPage,
   .lock = BalloonLock,
   .unlock = BalloonUnlock
};

static const struct BalloonOps balloonOpsBatched = {
   .addPage = BalloonAddPageBatched,
   .lock = BalloonLockBatched,
   .unlock = BalloonUnlockBatched
};

/*
 *----------------------------------------------------------------------
 *
 * Balloon_GetStats --
 *
 *      Returns information about balloon state, including the current and
 *      target size, rates for allocating and freeing pages, and statistics
 *      about past activity.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

const BalloonStats *
Balloon_GetStats(void)
{
   Balloon *b = &globalBalloon;
   BalloonStats *stats = &b->stats;

   /*
    * Fill in additional information about size and rates, which is
    * normally kept in the Balloon structure itself.
    */
   stats->nPages = b->nPages;
   stats->nPagesTarget = b->nPagesTarget;
   stats->rateNoSleepAlloc = BALLOON_NOSLEEP_ALLOC_MAX;
   stats->rateAlloc = b->rateAlloc;
   stats->rateFree = b->rateFree;

   return stats;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonChunk_Create --
 *
 *      Creates a new BalloonChunk object capable of tracking
 *      BALLOON_CHUNK_PAGES PAs.
 *
 *      We do not bother to define two versions (NOSLEEP and CANSLEEP)
 *      of OS_Malloc because Chunk_Create does not require a new page
 *      often.
 *
 * Results:
 *      On success: initialized BalloonChunk
 *      On failure: NULL
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static BalloonChunk *
BalloonChunk_Create(void)
{
   BalloonChunk *chunk;

   /* allocate memory, fail if unable */
   chunk = OS_Malloc(sizeof *chunk);
   if (chunk == NULL) {
      return NULL;
   }

   /* initialize */
   OS_MemZero(chunk, sizeof *chunk);
   DblLnkLst_Init(&chunk->node);

   return chunk;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonChunk_Destroy --
 *
 *      Reclaims all storage associated with specified BalloonChunk.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonChunk_Destroy(BalloonChunk *chunk) // IN
{
   /* reclaim storage */
   OS_Free(chunk, sizeof *chunk);
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_DeallocateChunkList --
 *
 *      Frees all allocated pages of a size
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
Balloon_DeallocateChunkList(Balloon *b,       // IN/OUT
                            int isLargePages) // IN
{
   unsigned int cnt = 0;
   BalloonChunkList *chunkList;

   chunkList = &b->pages[isLargePages];

   /* free all pages, skipping monitor unlock */
   while (chunkList->nChunks > 0) {
      BalloonPageFree(b, isLargePages);
      if (++cnt >= b->rateFree) {
         cnt = 0;
         OS_Yield();
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_Deallocate --
 *
 *      Frees all allocated pages.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
Balloon_Deallocate(Balloon *b) // IN/OUT
{
   /* free all pages, skipping monitor unlock */
   Balloon_DeallocateChunkList(b, FALSE);
   Balloon_DeallocateChunkList(b, TRUE);

   /* Release the batch page */
   if (b->batchPageMapping != MAPPING_INVALID) {
      OS_UnmapPage(b->batchPageMapping);
      b->batchPageMapping = MAPPING_INVALID;
      b->batchPage = NULL;
   }

   if (b->pageHandle != PAGE_HANDLE_INVALID) {
      OS_ReservedPageFree(b->pageHandle, FALSE);
      b->pageHandle = PAGE_HANDLE_INVALID;
   }
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonInitBatching --
 *
 *      Allocate and map the batch page.
 *
 * Results:
 *      BALLOON_SUCCESS or an error code in case of failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonInitBatching(Balloon *b) // OUT
{
   b->batchMaxEntries = BALLOON_BATCH_MAX_ENTRIES;

   b->pageHandle = OS_ReservedPageAlloc(FALSE, FALSE);
   if (b->pageHandle == PAGE_HANDLE_INVALID) {
      return BALLOON_PAGE_ALLOC_FAILURE;
   }

   b->batchPageMapping = OS_MapPageHandle(b->pageHandle);
   if (b->batchPageMapping == MAPPING_INVALID) {
      OS_ReservedPageFree(b->pageHandle, FALSE);
      b->pageHandle = PAGE_HANDLE_INVALID;
      return BALLOON_PAGE_ALLOC_FAILURE;
   }
   b->batchPage = OS_Mapping2Addr(b->batchPageMapping);

   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonReset --
 *
 *      Resets balloon "b" to empty state.  Frees all allocated pages
 *      and attempts to reset contact with the monitor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Schedules next execution of balloon timer handler.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonReset(Balloon *b) // OUT
{
   int status;

   /* free all pages, skipping monitor unlock */
   Balloon_Deallocate(b);

   status = Backdoor_MonitorStart(b, BALLOON_CAPABILITIES);
   if (status != BALLOON_SUCCESS) {
      return;
   }

   if ((b->hypervisorCapabilities & BALLOON_BATCHED_CMDS) != 0) {
      status = BalloonInitBatching(b);
      if (status != BALLOON_SUCCESS) {
         /*
          * We failed to initialize the batching in the guest, inform
          * the monitor about that by sending a null capability.
          *
          * The guest will retry to init itself in one second.
          */
         Backdoor_MonitorStart(b, 0);
         return;
      }
   }

   if ((b->hypervisorCapabilities & BALLOON_BATCHED_CMDS) != 0) {
      b->balloonOps = &balloonOpsBatched;
   } else if ((b->hypervisorCapabilities & BALLOON_BASIC_CMDS) != 0) {
      b->balloonOps = &balloonOps;
      b->batchMaxEntries = 1;
   }

   /* clear flag */
   b->resetFlag = FALSE;

   /* report guest type */
   (void) Backdoor_MonitorGuestType(b);
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_QueryAndExecute --
 *
 *      Contacts monitor via backdoor to obtain balloon size target,
 *      and starts adjusting balloon size to achieve target by allocating
 *      or deallocating pages. Resets balloon if requested by the monitor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Balloon_QueryAndExecute(void)
{
   Balloon *b = &globalBalloon;
   uint64 target = 0; // Silence compiler warning.
   int status;

   /* update stats */
   STATS_INC(b->stats.timer);

   /* reset, if specified */
   if (b->resetFlag) {
      BalloonReset(b);
   }

   /* contact monitor via backdoor */
   status = Backdoor_MonitorGetTarget(b, &target);

   /* decrement slowPageAllocationCycles counter */
   if (b->slowPageAllocationCycles > 0) {
      b->slowPageAllocationCycles--;
   }

   if (status == BALLOON_SUCCESS) {
      /* update target, adjust size */
      b->nPagesTarget = target;
      BalloonAdjustSize(b, target);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonErrorPageStore --
 *
 *      Attempt to add "page" to list of non-balloonable pages
 *      associated with "b".
 *
 * Results:
 *      On success: BALLOON_SUCCESS
 *      On failure: BALLOON_FAILURE (non-balloonable page list is already full)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonErrorPageStore(Balloon *b,                     // IN/OUT
                      PageHandle page,                // IN
                      int isLargePage)                // IN
{
   BalloonErrorPages *errors = &b->errors[isLargePage];

   /* fail if list already full */
   if (errors->nEntries >= BALLOON_ERROR_PAGES) {
      return BALLOON_FAILURE;
   }

   /* add page to list */
   errors->entries[errors->nEntries++] = page;
   STATS_INC(b->stats.primErrorPageAlloc[isLargePage]);
   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonErrorPagesFreeInt --
 *
 *      Deallocates all pages of a size on the list of non-balloonable pages
 *      associated with "b".
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonErrorPagesFreeInt(Balloon *b,      // IN/OUT
                         int isLargePage) // IN
{
   unsigned int i;
   BalloonErrorPages *errors = &b->errors[isLargePage];

   /* free all non-balloonable "error" pages */
   for (i = 0; i < errors->nEntries; i++) {
      OS_ReservedPageFree(errors->entries[i], isLargePage);
      errors->entries[i] = PAGE_HANDLE_INVALID;
      STATS_INC(b->stats.primErrorPageFree[isLargePage]);
   }
   errors->nEntries = 0;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonErrorPagesFree --
 *
 *      Deallocates all pages on the list of non-balloonable pages
 *      associated with "b".
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonErrorPagesFree(Balloon *b) // IN/OUT
{
   BalloonErrorPagesFreeInt(b, FALSE);
   BalloonErrorPagesFreeInt(b, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonGetChunk --
 *
 *      Attempt to find a "chunk" with a free slot to store locked page.
 *      Try to allocate new chunk if all existing chunks are full.
 *
 * Results:
 *      Returns NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static BalloonChunk *
BalloonGetChunk(Balloon *b,         // IN/OUT
                int isLargePage)    // IN
{
   BalloonChunk *chunk;
   BalloonChunkList *chunkList = &b->pages[isLargePage];

   /* Get first chunk from the list */
   if (DblLnkLst_IsLinked(&chunkList->chunks)) {
      chunk = DblLnkLst_Container(chunkList->chunks.next, BalloonChunk, node);
      if (chunk->nEntries < BALLOON_CHUNK_ENTRIES) {
         /* This chunk has free slots, use it */
         return chunk;
      }
   }

   /* create new chunk */
   chunk = BalloonChunk_Create();
   if (chunk != NULL) {
      DblLnkLst_LinkFirst(&chunkList->chunks, &chunk->node);

      /* update stats */
      chunkList->nChunks++;
   }

   return chunk;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonGetChunkOrFallback --
 *
 *      Attempt to find a "chunk" with a free slot to store locked page.
 *      If the allocation fails, use the previously allocated
 *      fallbackChunk.
 *
 * Results:
 *      A valid chunk.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static BalloonChunk *
BalloonGetChunkOrFallback(Balloon *b,      // IN/OUT
                          int isLargePage) // IN
{
   BalloonChunk *chunk = BalloonGetChunk(b, isLargePage);
   if (chunk == NULL) {
      BalloonChunkList *chunkList = &b->pages[isLargePage];

      ASSERT(b->fallbackChunk != NULL);
      chunk = b->fallbackChunk;
      b->fallbackChunk = NULL;

      DblLnkLst_LinkFirst(&chunkList->chunks, &chunk->node);
      chunkList->nChunks++;
   }

   return chunk;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonPageStore --
 *
 *      Add "page" to the given "chunk".
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonPageStore(BalloonChunk *chunk,   // IN/OUT
                 PageHandle page)       // IN
{
   chunk->entries[chunk->nEntries++] = page;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonChunkDestroyEmpty --
 *
 *      Release the chunk if it contains no pages.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonChunkDestroyEmpty(Balloon *b,            // IN/OUT
                         BalloonChunk *chunk,   // IN/OUT
                         int isLargePage)       // IN
{
   if (chunk->nEntries == 0) {
      /* destroy empty chunk */
      DblLnkLst_Unlink1(&chunk->node);
      BalloonChunk_Destroy(chunk);

      /* update stats */
      b->pages[isLargePage].nChunks--;
   }
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonPageFree --
 *
 *      Attempts to deallocate a physical page, deflating balloon "b".
 *      Never informs monitor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonPageFree(Balloon *b,      // IN/OUT
                int isLargePage) // IN
{
   BalloonChunkList *chunkList = &b->pages[isLargePage];
   BalloonChunk *chunk;
   PageHandle page;

   ASSERT(DblLnkLst_IsLinked(&chunkList->chunks));
   chunk = DblLnkLst_Container(chunkList->chunks.next, BalloonChunk, node);

   /* deallocate last page */
   page = chunk->entries[--chunk->nEntries];

   /* deallocate page */
   OS_ReservedPageFree(page, isLargePage);

   STATS_INC(b->stats.primFree[isLargePage]);

   /* update balloon size */
   b->nPages--;

   /* reclaim chunk, if empty */
   BalloonChunkDestroyEmpty(b, chunk, isLargePage);
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonInflate --
 *
 *      Attempts to allocate physical pages to inflate balloon.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonInflate(Balloon *b,      // IN/OUT
               uint64 target)   // IN
{
   uint32 nEntries;
   unsigned int rate;
   unsigned int allocations = 0;
   int status = BALLOON_SUCCESS;
   BalloonPageAllocType allocType;
   Bool isLargePages;
   unsigned numPagesPerEntry;

   if ((b->hypervisorCapabilities & BALLOON_BATCHED_2M_CMDS) != 0) {
      allocType = BALLOON_PAGE_ALLOC_LPAGE;
      isLargePages = TRUE;
      numPagesPerEntry = OS_LARGE_2_SMALL_PAGES;
   } else {
      allocType = BALLOON_PAGE_ALLOC_NOSLEEP;
      isLargePages = FALSE;
      numPagesPerEntry = 1;
   }

   /*
    * We try allocating in the following order:
    *
    * First we try to allocate large pages without sleeping. If the
    * memory becomes too fragmented to allocate whole large pages at
    * once, switch to small pages - still without sleeping.
    *
    * If we do not throttle nosleep allocations, we can drain all
    * free pages in the guest quickly (if the balloon target is high).
    * As a side-effect, draining free pages helps to inform (force)
    * the guest to start swapping if balloon target is not met yet,
    * which is a desired behavior. However, balloon driver can consume
    * all available CPU cycles if too many pages are allocated in a
    * second. Therefore, we throttle nosleep allocations even when
    * the guest is not under memory pressure. OTOH, if we have already
    * predicted that the guest is under memory pressure, then we
    * slowdown page allocations considerably.
    */

   /*
    * Start with no sleep allocation rate which may be higher
    * than sleeping allocation rate.
    */
   rate = b->slowPageAllocationCycles ?
                b->rateAlloc : BALLOON_NOSLEEP_ALLOC_MAX;

   nEntries = 0;
   while (b->nPages < target &&
          nEntries * numPagesPerEntry < target - b->nPages) {
      PageHandle handle;

      STATS_INC(b->stats.primAlloc[allocType]);

      handle = OS_ReservedPageAlloc(allocType == BALLOON_PAGE_ALLOC_CANSLEEP,
                                    isLargePages);

      if (handle == PAGE_HANDLE_INVALID) {
         STATS_INC(b->stats.primAllocFail[allocType]);

         status = BALLOON_PAGE_ALLOC_FAILURE;

         if (allocType == BALLOON_PAGE_ALLOC_LPAGE) {
            /*
             * LPAGE allocation failed. This does mean that the guest is under
             * pressure, it just means that the memory is fragmented enough that
             * we cannot allocate any more large pages.
             *
             * Lock partial set of large pages now as we continue with
             * allocating small pages and we cannot have a lock call with
             * different entry sizes.
             */
            if (nEntries > 0) {
               status = b->balloonOps->lock(b, nEntries, TRUE, &target);
               nEntries = 0;
            }

            /* Continue with small pages */
            isLargePages = FALSE;
            numPagesPerEntry = 1;
            allocType = BALLOON_PAGE_ALLOC_NOSLEEP;
         } else if (allocType == BALLOON_PAGE_ALLOC_NOSLEEP) {
            /*
             * NOSLEEP page allocation failed, so the guest is under memory
             * pressure. Let us slow down page allocations for next few cycles
             * so that the guest gets out of memory pressure. Also, if we
             * already allocated b->rateAlloc pages, let's pause, otherwise
             * switch to sleeping allocations.
             */
            b->slowPageAllocationCycles = SLOW_PAGE_ALLOCATION_CYCLES;

            /* Lower rate for sleeping allocations. */
            rate = b->rateAlloc;
            allocType = BALLOON_PAGE_ALLOC_CANSLEEP;
         } else {
            ASSERT(allocType == BALLOON_PAGE_ALLOC_CANSLEEP);
            /*
             * CANSLEEP page allocation failed, so guest is under severe
             * memory pressure. Quickly decrease allocation rate.
             */
            b->rateAlloc = MAX(b->rateAlloc / 2, BALLOON_RATE_ALLOC_MIN);

            /* Stop allocating any more memory */
            break;
         }

         if (allocations >= b->rateAlloc) {
            break;
         }

         continue;
      }
      allocations++;

      b->balloonOps->addPage(b, nEntries++, handle);
      if (nEntries == b->batchMaxEntries) {
         status = b->balloonOps->lock(b, nEntries, isLargePages, &target);
         nEntries = 0;

         if (status != BALLOON_SUCCESS) {
            break;
         }
      }

      if (allocations % BALLOON_ALLOC_YIELD_THRESHOLD == 0) {
         OS_Yield();
      }

      if (allocations >= rate) {
         /* We allocated enough pages, let's take a break. */
         break;
      }
   }

   if (nEntries > 0) {
      b->balloonOps->lock(b, nEntries, isLargePages, NULL);
   }

   /*
    * We reached our goal without failures so try increasing
    * allocation rate.
    */
   if (status == BALLOON_SUCCESS && allocations >= b->rateAlloc) {
      unsigned int mult = allocations / b->rateAlloc;

      b->rateAlloc = MIN(b->rateAlloc + mult * BALLOON_RATE_ALLOC_INC,
                         BALLOON_RATE_ALLOC_MAX);
   }

   /* release non-balloonable pages, succeed */
   BalloonErrorPagesFree(b);
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonLockBatched --
 *
 *      Lock all the batched page, previously stored by
 *      BalloonAddPageBatched.
 *
 * Results:
 *      BALLOON_SUCCESS or an error code. On success, *target is filled
 *      with the balloon target.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int
BalloonLockBatched(Balloon *b,       // IN/OUT
                   uint16 nEntries,  // IN
                   int isLargePages, // IN
                   uint64 *target)   // OUT
{
   int          status;
   uint32       i;
   uint32       nLockedEntries;
   PageHandle   handle;
   PPN64        batchPagePPN;
   BalloonChunk *chunk = NULL;

   batchPagePPN = PA_2_PPN(OS_ReservedPageGetPA(b->pageHandle));

   /*
    * Make sure that we will always have an available chunk before doing
    * the LOCK_BATCHED call.
    */
   ASSERT(b->batchMaxEntries < BALLOON_CHUNK_ENTRIES);
   b->fallbackChunk = BalloonChunk_Create();
   if (b->fallbackChunk == NULL) {
      status = BALLOON_PAGE_ALLOC_FAILURE;
   } else {
      status = Backdoor_MonitorLockPagesBatched(b, batchPagePPN, nEntries,
                                                isLargePages, target);
   }

   if (status != BALLOON_SUCCESS) {
      for (i = 0; i < nEntries; i++) {
         PA64 pa = Balloon_BatchGetPA(b->batchPage, i);
         handle = OS_ReservedPageGetHandle(pa);

         OS_ReservedPageFree(handle, isLargePages);
      }

      goto out;
   }

   nLockedEntries = 0;
   for (i = 0; i < nEntries; i++) {
      PA64              pa;
      int               error;

      pa = Balloon_BatchGetPA(b->batchPage, i);
      handle = OS_ReservedPageGetHandle(pa);
      error = Balloon_BatchGetStatus(b->batchPage, i);
      if (error != BALLOON_SUCCESS) {
         switch (error) {
         case BALLOON_ERROR_PPN_PINNED:
         case BALLOON_ERROR_PPN_INVALID:
            if (BalloonErrorPageStore(b, handle, isLargePages)
                == BALLOON_SUCCESS) {
               break;
            }
            // Fallthrough.
         case BALLOON_ERROR_RESET:
         case BALLOON_ERROR_PPN_NOTNEEDED:
            OS_ReservedPageFree(handle, isLargePages);
            break;
         default:
            /*
             * If we fall here, there is definitively a bug in the
             * driver that needs to be fixed, I'm not sure if
             * PINNED and INVALID PPN can be seen as a bug in the
             * driver.
             */
            ASSERT(FALSE);
         }
         continue;
      }

      if (chunk == NULL) {
         chunk = BalloonGetChunkOrFallback(b, isLargePages);
      }

      BalloonPageStore(chunk, handle);
      if (chunk->nEntries == BALLOON_CHUNK_ENTRIES) {
         chunk = NULL;
      }
      nLockedEntries++;
   }

   if (isLargePages) {
      b->nPages += nLockedEntries * OS_LARGE_2_SMALL_PAGES;
   } else {
      b->nPages += nLockedEntries;
   }

out:
   if (b->fallbackChunk != NULL) {
      BalloonChunk_Destroy(b->fallbackChunk);
      b->fallbackChunk = NULL;
   }

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonUnlockBatched --
 *
 *      Unlock all the batched page, previously stored by
 *      BalloonAddPageBatched.
 *
 * Results:
 *      BALLOON_SUCCESS or an error code. On success, *target is filled
 *      with the balloon target.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int
BalloonUnlockBatched(Balloon *b,       // IN/OUT
                     uint16 nEntries,  // IN
                     int isLargePages, // IN
                     uint64 *target)   // OUT
{
   uint32 i;
   int status = BALLOON_SUCCESS;
   uint32 nUnlockedEntries;
   PPN64 batchPagePPN;
   BalloonChunk *chunk = NULL;

   batchPagePPN = PA_2_PPN(OS_ReservedPageGetPA(b->pageHandle));
   status = Backdoor_MonitorUnlockPagesBatched(b, batchPagePPN, nEntries,
                                               isLargePages, target);

   if (status != BALLOON_SUCCESS) {
      for (i = 0; i < nEntries; i++) {
         PA64 pa = Balloon_BatchGetPA(b->batchPage, i);
         PageHandle handle = OS_ReservedPageGetHandle(pa);

         chunk = BalloonGetChunkOrFallback(b, isLargePages);
         BalloonPageStore(chunk, handle);
      }
      goto out;
   }

   nUnlockedEntries = 0;
   for (i = 0; i < nEntries; i++) {
      int status = Balloon_BatchGetStatus(b->batchPage, i);
      PA64 pa = Balloon_BatchGetPA(b->batchPage, i);
      PageHandle handle = OS_ReservedPageGetHandle(pa);

      if (status != BALLOON_SUCCESS) {
         chunk = BalloonGetChunkOrFallback(b, isLargePages);
         BalloonPageStore(chunk, handle);
         continue;
      }

      OS_ReservedPageFree(handle, isLargePages);
      STATS_INC(b->stats.primFree[isLargePages]);

      nUnlockedEntries++;
   }

   if (isLargePages) {
      b->nPages -= nUnlockedEntries * OS_LARGE_2_SMALL_PAGES;
   } else {
      b->nPages -= nUnlockedEntries;
   }

out:
   if (b->fallbackChunk != NULL) {
      BalloonChunk_Destroy(b->fallbackChunk);
      b->fallbackChunk = NULL;
   }
   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonAddPageBatched --
 *
 *      Add a page to the batch page, that will be ballooned later.
 *
 * Results:
 *      Nothing.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonAddPageBatched(Balloon *b,            // IN
                      uint16 idx,            // IN
                      PageHandle page)       // IN
{
   PA64 pa = OS_ReservedPageGetPA(page);
   Balloon_BatchSetPA(b->batchPage, idx, pa);
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonLock --
 *
 *      Lock a page, previously stored with a call to BalloonAddPage,
 *      by notifying the monitor.
 *
 * Results:
 *      BALLOON_SUCCESS or an error code. On success, *target is filled
 *      with the balloon target.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonLock(Balloon *b,       // IN/OUT
            uint16 nPages,    // IN
            int isLargePage,  // IN
            uint64 *target)   // OUT
{
   PPN64 pagePPN;
   BalloonChunk *chunk;
   int status;

   ASSERT(!isLargePage);

   /* Get the chunk to store allocated page. */
   chunk = BalloonGetChunk(b, FALSE);
   if (chunk == NULL) {
      OS_ReservedPageFree(b->pageHandle, FALSE);
      status = BALLOON_PAGE_ALLOC_FAILURE;
      goto out;
   }

   /* inform monitor via backdoor */
   pagePPN = PA_2_PPN(OS_ReservedPageGetPA(b->pageHandle));
   status = Backdoor_MonitorLockPage(b, pagePPN, target);
   if (status != BALLOON_SUCCESS) {
      int old_status = status;

      /* We need to release the chunk if it was just allocated */
      BalloonChunkDestroyEmpty(b, chunk, isLargePage);

      if (status == BALLOON_ERROR_RESET ||
          status == BALLOON_ERROR_PPN_NOTNEEDED) {
         OS_ReservedPageFree(b->pageHandle, FALSE);
         goto out;
      }

      /* place on list of non-balloonable pages, retry allocation */
      status = BalloonErrorPageStore(b, b->pageHandle, FALSE);
      if (status != BALLOON_SUCCESS) {
         OS_ReservedPageFree(b->pageHandle, FALSE);
         goto out;
      }

      status = old_status;
      goto out;
   }

   /* track allocated page */
   BalloonPageStore(chunk, b->pageHandle);

   /* update balloon size */
   b->nPages++;

out:
   b->pageHandle = PAGE_HANDLE_INVALID;
   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonUnlock --
 *
 *      Unlock a page, previously stored with a call to BalloonAddPage,
 *      by notifying the monitor.
 *
 * Results:
 *      BALLOON_SUCCESS or an error code. On success, *target is filled
 *      with the balloon target.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonUnlock(Balloon *b,      // IN/OUT
              uint16 nPages,   // IN
              int isLargePage, // IN
              uint64 *target)  // OUT
{
   PPN64 pagePPN = PA_2_PPN(OS_ReservedPageGetPA(b->pageHandle));
   int status = Backdoor_MonitorUnlockPage(b, pagePPN, target);

   ASSERT(!isLargePage);

   if (status != BALLOON_SUCCESS) {
      BalloonChunk *chunk = BalloonGetChunkOrFallback(b, FALSE);
      BalloonPageStore(chunk, b->pageHandle);
      goto out;
   }

   OS_ReservedPageFree(b->pageHandle, FALSE);
   STATS_INC(b->stats.primFree[FALSE]);

   /* update balloon size */
   b->nPages--;

out:
   b->pageHandle = PAGE_HANDLE_INVALID;
   if (b->fallbackChunk != NULL) {
      BalloonChunk_Destroy(b->fallbackChunk);
      b->fallbackChunk = NULL;
   }
   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * BalloonAddPage --
 *
 *      Add a page to be ballooned later.
 *
 * Results:
 *      Nothing.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonAddPage(Balloon *b,            // IN/OUT
               uint16 idx,            // IN
               PageHandle page)       // IN
{
   ASSERT(b->pageHandle == PAGE_HANDLE_INVALID);
   b->pageHandle = page;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonDeflateInt --
 *
 *      Frees physical pages of a given size to deflate balloon.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonDeflateInt(Balloon *b,       // IN/OUT
                  uint64 target,    // IN
                  int isLargePages) // IN
{
   int                  status = BALLOON_SUCCESS;
   uint32               nPages, deallocations = 0;
   BalloonChunk         *chunk = NULL;
   BalloonChunkList     *chunkList = &b->pages[isLargePages];

   if (chunkList->nChunks == 0) {
      return;
   }

   nPages = 0;
   while (chunkList->nChunks > 0 && b->nPages > target
          && nPages < b->nPages - target) {
      PageHandle lockedHandle;

      if (chunk == NULL) {
         /*
          * The chunk should never be empty. If it is, then there is a
          * deviation between the guest balloon size, and tracked
          * pages...
          */
         ASSERT(DblLnkLst_IsLinked(&chunkList->chunks));
         chunk = DblLnkLst_Container(chunkList->chunks.next, BalloonChunk,
                                     node);
      }

      lockedHandle = chunk->entries[--chunk->nEntries];
      if (!chunk->nEntries) {
         DblLnkLst_Unlink1(&chunk->node);
         /*
          * Do not free the chunk, we may need it if the UNLOCK cmd fails
          */
         b->fallbackChunk = chunk;

         chunkList->nChunks--;
         chunk = NULL;
      }

      deallocations++;
      b->balloonOps->addPage(b, nPages++, lockedHandle);
      if (nPages == b->batchMaxEntries) {
         status = b->balloonOps->unlock(b, nPages, isLargePages, &target);
         nPages = 0;

         if (status != BALLOON_SUCCESS) {
            break;
         }
      }

      if (deallocations >= b->rateFree) {
         /* We released enough pages, let's take a break. */
         break;
      }
   }

   if (nPages) {
      b->balloonOps->unlock(b, nPages, isLargePages, NULL);
   }

   if (BALLOON_RATE_ADAPT) {
      if (status == BALLOON_SUCCESS) {
         /* slowly increase rate if no errors */
         b->rateFree = MIN(b->rateFree + BALLOON_RATE_FREE_INC,
                           BALLOON_RATE_FREE_MAX);
      } else {
         /* quickly decrease rate if error */
         b->rateFree = MAX(b->rateFree / 2, BALLOON_RATE_FREE_MIN);
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonDeflate --
 *
 *      Frees physical pages to deflate balloon.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonDeflate(Balloon *b,    // IN/OUT
               uint64 target) // IN
{
   /* Prefer to unlock small pages over unlocking large pages */
   BalloonDeflateInt(b, target, FALSE);
   BalloonDeflateInt(b, target, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonAdjustSize --
 *
 *      Attempts to allocate or deallocate physical pages in order
 *      to reach desired "target" size for balloon "b".
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static void
BalloonAdjustSize(Balloon *b,    // IN/OUT
                  uint64 target) // IN
{
   /*
    * When we only deal with large pages it can happen that we overshoot our
    * target by OS_LARGE_2_SMALL_PAGES - 1 pages. To prevent a constant balloon,
    * unballoon loop, allow the target to be OS_LARGE_2_SMALL_PAGES - 1 pages
    * lower than the actual ballooned amount before we do something.
    */
   if (b->nPages < target) {
      BalloonInflate(b, target);
   } else if (target == 0 || b->nPages > target + OS_LARGE_2_SMALL_PAGES - 1) {
      BalloonDeflate(b, target);
   }
}

/*
 *----------------------------------------------------------------------
 *
 * Balloon_Init --
 *
 *      Initializes state of the balloon.
 *
 * Results:
 *      Returns TRUE on success (always).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
Balloon_Init(BalloonGuest guestType)    // IN
{
   Balloon *b = &globalBalloon;

   DblLnkLst_Init(&b->pages[TRUE].chunks);
   DblLnkLst_Init(&b->pages[FALSE].chunks);

   b->guestType = guestType;

   /* initialize rates */
   b->rateAlloc = BALLOON_RATE_ALLOC_MAX;
   b->rateFree  = BALLOON_RATE_FREE_MAX;

   /* initialize reset flag */
   b->resetFlag = TRUE;

   b->hypervisorCapabilities = 0;

   b->pageHandle = PAGE_HANDLE_INVALID;
   b->batchPageMapping = MAPPING_INVALID;
   b->batchPage = NULL;

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_Cleanup --
 *
 *      Cleanup after ballooning.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
Balloon_Cleanup(void)
{
   Balloon *b = &globalBalloon;

   /*
    * Deallocate all reserved memory, and reset connection with monitor.
    * Reset connection before deallocating memory to avoid potential for
    * additional spurious resets from guest touching deallocated pages.
    */
   Backdoor_MonitorStart(b, BALLOON_CAPABILITIES);
   Balloon_Deallocate(b);
}

#ifdef __cplusplus
}
#endif
