/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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
 * 3. Neither the name of VMware Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission of VMware Inc.
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
 * Compile-Time Options
 */

#define BALLOON_RATE_ADAPT      1

#define BALLOON_DEBUG           1
#define BALLOON_DEBUG_VERBOSE   0

#define BALLOON_STATS

/*
 * Includes
 */

#include "os.h"
#include "backdoor.h"
#include "backdoor_balloon.h"
#include "dbllnklst.h"
#include "vmballoon.h"

/*
 * Constants
 */

#ifndef NULL
#define NULL 0
#endif

#define BALLOON_PROTOCOL_VERSION        2

#define BALLOON_CHUNK_PAGES             1000

#define BALLOON_NOSLEEP_ALLOC_MAX       16384

#define BALLOON_RATE_ALLOC_MIN          512
#define BALLOON_RATE_ALLOC_MAX          2048
#define BALLOON_RATE_ALLOC_INC          16

#define BALLOON_RATE_FREE_MIN           512
#define BALLOON_RATE_FREE_MAX           16384
#define BALLOON_RATE_FREE_INC           16

#define BALLOON_ERROR_PAGES             16

/*
 * When guest is under memory pressure, use a reduced page allocation
 * rate for next several cycles.
 */
#define SLOW_PAGE_ALLOCATION_CYCLES     4

/*
 * Move it to bora/public/balloon_def.h later, if needed. Note that
 * BALLOON_PAGE_ALLOC_FAILURE is an internal error code used for
 * distinguishing page allocation failures from monitor-backdoor errors.
 * We use value 1000 because all monitor-backdoor error codes are < 1000.
 */
#define BALLOON_PAGE_ALLOC_FAILURE      1000

/* Maximum number of page allocations without yielding processor */
#define BALLOON_ALLOC_YIELD_THRESHOLD   1024


/*
 * Types
 */

typedef struct BalloonChunk {
   PageHandle page[BALLOON_CHUNK_PAGES];
   uint32 pageCount;
   DblLnkLst_Links node;
} BalloonChunk;

typedef struct {
   PageHandle page[BALLOON_ERROR_PAGES];
   uint32 pageCount;
} BalloonErrorPages;

typedef struct {
   /* sets of reserved physical pages */
   DblLnkLst_Links chunks;
   int nChunks;

   /* transient list of non-balloonable pages */
   BalloonErrorPages errors;

   BalloonGuest guestType;

   /* balloon size */
   int nPages;
   int nPagesTarget;

   /* reset flag */
   int resetFlag;

   /* adjustment rates (pages per second) */
   int rateAlloc;
   int rateFree;

   /* slowdown page allocations for next few cycles */
   int slowPageAllocationCycles;

   /* statistics */
   BalloonStats stats;
} Balloon;

/*
 * Globals
 */

static Balloon globalBalloon;

/*
 * Balloon operations
 */
static int  BalloonPageAlloc(Balloon *b, BalloonPageAllocType allocType);
static int  BalloonPageFree(Balloon *b, int monitorUnlock);
static int  BalloonAdjustSize(Balloon *b, uint32 target);
static void BalloonReset(Balloon *b);

/*
 * Backdoor Operations
 */
static int BalloonMonitorStart(Balloon *b);
static int BalloonMonitorGuestType(Balloon *b);
static int BalloonMonitorGetTarget(Balloon *b, uint32 *nPages);
static int BalloonMonitorLockPage(Balloon *b, PageHandle handle);
static int BalloonMonitorUnlockPage(Balloon *b, PageHandle handle);

/*
 * Macros
 */

#ifdef  BALLOON_STATS
#define STATS_INC(stat) (stat)++
#else
#define STATS_INC(stat)
#endif


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
 *      BALLOON_CHUNK_PAGES PPNs.
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
Balloon_Deallocate(Balloon *b) // IN
{
   unsigned int cnt = 0;

   /* free all pages, skipping monitor unlock */
   while (b->nChunks > 0) {
      (void) BalloonPageFree(b, FALSE);
      if (++cnt >= b->rateFree) {
         cnt = 0;
         OS_Yield();
      }
   }
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
BalloonReset(Balloon *b) // IN
{
   uint32 status;

   /* free all pages, skipping monitor unlock */
   Balloon_Deallocate(b);

   /* send start command */
   status = BalloonMonitorStart(b);
   if (status == BALLOON_SUCCESS) {
      /* clear flag */
      b->resetFlag = 0;

      /* report guest type */
      (void) BalloonMonitorGuestType(b);
   }
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
   uint32 target = 0; // Silence compiler warning.
   int status;

   /* update stats */
   STATS_INC(b->stats.timer);

   /* reset, if specified */
   if (b->resetFlag) {
      BalloonReset(b);
   }

   /* contact monitor via backdoor */
   status = BalloonMonitorGetTarget(b, &target);

   /* decrement slowPageAllocationCycles counter */
   if (b->slowPageAllocationCycles > 0) {
      b->slowPageAllocationCycles--;
   }

   if (status == BALLOON_SUCCESS) {
      /* update target, adjust size */
      b->nPagesTarget = target;
      (void) BalloonAdjustSize(b, target);
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
BalloonErrorPageStore(Balloon *b,      // IN
                      PageHandle page) // IN
{
   /* fail if list already full */
   if (b->errors.pageCount >= BALLOON_ERROR_PAGES) {
      return BALLOON_FAILURE;
   }

   /* add page to list */
   b->errors.page[b->errors.pageCount++] = page;
   STATS_INC(b->stats.primErrorPageAlloc);
   return BALLOON_SUCCESS;
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
BalloonErrorPagesFree(Balloon *b) // IN
{
   unsigned int i;

   /* free all non-balloonable "error" pages */
   for (i = 0; i < b->errors.pageCount; i++) {
      OS_ReservedPageFree(b->errors.page[i]);
      b->errors.page[i] = PAGE_HANDLE_INVALID;
      STATS_INC(b->stats.primErrorPageFree);
   }
   b->errors.pageCount = 0;
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
BalloonGetChunk(Balloon *b)         // IN
{
   BalloonChunk *chunk;

   /* Get first chunk from the list */
   if (DblLnkLst_IsLinked(&b->chunks)) {
      chunk = DblLnkLst_Container(b->chunks.next, BalloonChunk, node);
      if (chunk->pageCount < BALLOON_CHUNK_PAGES) {
         /* This chunk has free slots, use it */
         return chunk;
      }
   }

   /* create new chunk */
   chunk = BalloonChunk_Create();
   if (chunk != NULL) {
      DblLnkLst_LinkFirst(&b->chunks, &chunk->node);

      /* update stats */
      b->nChunks++;
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
BalloonPageStore(BalloonChunk *chunk, PageHandle page)
{
   chunk->page[chunk->pageCount++] = page;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonPageAlloc --
 *
 *      Attempts to allocate a physical page, inflating balloon "b".
 *      Informs monitor of PPN for allocated page via backdoor.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonPageAlloc(Balloon *b,                     // IN
                 BalloonPageAllocType allocType) // IN
{
   PageHandle page;
   BalloonChunk *chunk = NULL;
   Bool locked;
   int status;

   /* allocate page, fail if unable */
   do {
      STATS_INC(b->stats.primAlloc[allocType]);

      /*
       * Attempts to allocate and reserve a physical page.
       *
       * If canSleep == 1, i.e., BALLOON_PAGE_ALLOC_CANSLEEP:
       *      The allocation can wait (sleep) for page writeout (swap) by the guest.
       * otherwise canSleep == 0, i.e., BALLOON_PAGE_ALLOC_NOSLEEP:
       *      If allocation of a page requires disk writeout, then just fail. DON'T sleep.
       *
       * Returns the physical address of the allocated page, or 0 if error.
       */
      page = OS_ReservedPageAlloc(allocType);
      if (page == PAGE_HANDLE_INVALID) {
         STATS_INC(b->stats.primAllocFail[allocType]);
         return BALLOON_PAGE_ALLOC_FAILURE;
      }

      /* Get the chunk to store allocated page. */
      if (!chunk) {
         chunk = BalloonGetChunk(b);
         if (!chunk) {
            OS_ReservedPageFree(page);
            return BALLOON_PAGE_ALLOC_FAILURE;
         }
      }

      /* inform monitor via backdoor */
      status = BalloonMonitorLockPage(b, page);
      locked = status == BALLOON_SUCCESS;
      if (!locked) {
         if (status == BALLOON_ERROR_RESET ||
             status == BALLOON_ERROR_PPN_NOTNEEDED) {
            OS_ReservedPageFree(page);
            return status;
         }

         /* place on list of non-balloonable pages, retry allocation */
         status = BalloonErrorPageStore(b, page);
         if (status != BALLOON_SUCCESS) {
            OS_ReservedPageFree(page);
            return status;
         }
      }
   } while (!locked);

   /* track allocated page */
   BalloonPageStore(chunk, page);

   /* update balloon size */
   b->nPages++;

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonPageFree --
 *
 *      Attempts to deallocate a physical page, deflating balloon "b".
 *      Informs monitor of PPN for deallocated page via backdoor if
 *      "monitorUnlock" is specified.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonPageFree(Balloon *b,        // IN
                int monitorUnlock) // IN
{
   DblLnkLst_Links *node, *next;
   BalloonChunk *chunk = NULL;
   PageHandle page;
   int status;

   DblLnkLst_ForEachSafe(node, next, &b->chunks) {
      chunk = DblLnkLst_Container(node, BalloonChunk, node);
      if (chunk->pageCount > 0) {
         break;
      }

      /* destroy empty chunk */
      DblLnkLst_Unlink1(node);
      BalloonChunk_Destroy(chunk);
      chunk = NULL;

      /* update stats */
      b->nChunks--;
   }

   if (!chunk) {
      /* We could not find a single non-empty chunk. */
      return BALLOON_FAILURE;
   }

   /* deallocate last page */
   page = chunk->page[--chunk->pageCount];

   /* inform monitor via backdoor */
   if (monitorUnlock) {
      status = BalloonMonitorUnlockPage(b, page);
      if (status != BALLOON_SUCCESS) {
         /* reset next pointer, fail */
         chunk->pageCount++;
         return status;
      }
   }

   /* deallocate page */
   OS_ReservedPageFree(page);
   STATS_INC(b->stats.primFree);

   /* update balloon size */
   b->nPages--;

   /* reclaim chunk, if empty */
   if (chunk->pageCount == 0) {
      /* destroy empty chunk */
      DblLnkLst_Unlink1(&chunk->node);
      BalloonChunk_Destroy(chunk);

      /* update stats */
      b->nChunks--;
   }

   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonInflate--
 *
 *      Attempts to allocate physical pages to inflate balloon.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonInflate(Balloon *b,    // IN
               uint32 target) // IN
{
   unsigned int goal;
   unsigned int rate;
   unsigned int i;
   unsigned int allocations = 0;
   int status = 0;
   BalloonPageAllocType allocType = BALLOON_PAGE_ALLOC_NOSLEEP;

   /*
    * First try NOSLEEP page allocations to inflate balloon.
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

   goal = target - b->nPages;
   /*
    * Start with no sleep allocation rate which may be higher
    * than sleeping allocation rate.
    */
   rate = b->slowPageAllocationCycles ?
                b->rateAlloc : BALLOON_NOSLEEP_ALLOC_MAX;

   for (i = 0; i < goal; i++) {

      status = BalloonPageAlloc(b, allocType);
      if (status != BALLOON_SUCCESS) {
         if (status != BALLOON_PAGE_ALLOC_FAILURE) {
            /*
             * Not a page allocation failure, stop this cycle.
             * Maybe we'll get new target from the host soon.
             */
            break;
         }

         if (allocType == BALLOON_PAGE_ALLOC_CANSLEEP) {
            /*
             * CANSLEEP page allocation failed, so guest is under severe
             * memory pressure. Quickly decrease allocation rate.
             */
            b->rateAlloc = MAX(b->rateAlloc / 2, BALLOON_RATE_ALLOC_MIN);
            break;
         }

         /*
          * NOSLEEP page allocation failed, so the guest is under memory
          * pressure. Let us slow down page allocations for next few cycles
          * so that the guest gets out of memory pressure. Also, if we
          * already allocated b->rateAlloc pages, let's pause, otherwise
          * switch to sleeping allocations.
          */
         b->slowPageAllocationCycles = SLOW_PAGE_ALLOCATION_CYCLES;

         if (i >= b->rateAlloc)
            break;

         allocType = BALLOON_PAGE_ALLOC_CANSLEEP;
         /* Lower rate for sleeping allocations. */
         rate = b->rateAlloc;
      }

      if (++allocations > BALLOON_ALLOC_YIELD_THRESHOLD) {
         OS_Yield();
         allocations = 0;
      }

      if (i >= rate) {
         /* We allocated enough pages, let's take a break. */
         break;
      }
   }

   /*
    * We reached our goal without failures so try increasing
    * allocation rate.
    */
   if (status == BALLOON_SUCCESS && i >= b->rateAlloc) {
      unsigned int mult = i / b->rateAlloc;

      b->rateAlloc = MIN(b->rateAlloc + mult * BALLOON_RATE_ALLOC_INC,
                         BALLOON_RATE_ALLOC_MAX);
   }

   /* release non-balloonable pages, succeed */
   BalloonErrorPagesFree(b);
   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonDeflate --
 *
 *      Frees physical pages to deflate balloon.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonDeflate(Balloon *b,    // IN
               uint32 target) // IN
{
   int status, i;
   uint32 nFree = b->nPages - target;

   /* limit deallocation rate */
   nFree = MIN(nFree, b->rateFree);

   /* free pages to reach target */
   for (i = 0; i < nFree; i++) {
      status = BalloonPageFree(b, TRUE);
      if (status != BALLOON_SUCCESS) {
         if (BALLOON_RATE_ADAPT) {
            /* quickly decrease rate if error */
            b->rateFree = MAX(b->rateFree / 2, BALLOON_RATE_FREE_MIN);
         }
         return status;
      }
   }

   if (BALLOON_RATE_ADAPT) {
      /* slowly increase rate if no errors */
      b->rateFree = MIN(b->rateFree + BALLOON_RATE_FREE_INC,
                        BALLOON_RATE_FREE_MAX);
   }

   return BALLOON_SUCCESS;
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
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonAdjustSize(Balloon *b,    // IN
                  uint32 target) // IN
{
   if (b->nPages < target) {
      return BalloonInflate(b, target);
   } else if (b->nPages > target) {
      return BalloonDeflate(b, target);
   } else {
      /* already at target */
      return BALLOON_SUCCESS;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonMonitorStart --
 *
 *      Attempts to contact monitor via backdoor to begin operation.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonMonitorStart(Balloon *b) // IN
{
   uint32 status, target;
   Backdoor_proto bp;

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_START;
   bp.in.size = BALLOON_PROTOCOL_VERSION;

   /* invoke backdoor */
   Backdoor_Balloon(&bp);

   /* parse return values */
   status = bp.out.ax.word;
   target = bp.out.bx.word;

   /* update stats */
   STATS_INC(b->stats.start);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.startFail);
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonMonitorGuestType --
 *
 *      Attempts to contact monitor and report guest OS identity.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonMonitorGuestType(Balloon *b) // IN
{
   uint32 status, target;
   Backdoor_proto bp;

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_GUEST_ID;
   bp.in.size = b->guestType;

   /* invoke backdoor */
   Backdoor_Balloon(&bp);

   /* parse return values */
   status = bp.out.ax.word;
   target = bp.out.bx.word;

   /* set flag if reset requested */
   if (status == BALLOON_ERROR_RESET) {
      b->resetFlag = 1;
   }

   /* update stats */
   STATS_INC(b->stats.guestType);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.guestTypeFail);
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonMonitorGetTarget --
 *
 *      Attempts to contact monitor via backdoor to obtain desired
 *      balloon size.
 *
 *      Predicts the maximum achievable balloon size and sends it
 *      to vmm => vmkernel via vEbx register.
 *
 *      OS_ReservedPageGetLimit() returns either predicted max balloon
 *      pages or BALLOON_MAX_SIZE_USE_CONFIG. In the later scenario,
 *      vmkernel uses global config options for determining a guest's max
 *      balloon size. Note that older vmballoon drivers set vEbx to
 *      BALLOON_MAX_SIZE_USE_CONFIG, i.e., value 0 (zero). So vmkernel
 *      will fallback to config-based max balloon size estimation.
 *
 * Results:
 *      If successful, sets "target" to value obtained from monitor,
 *      and returns BALLOON_SUCCESS. Otherwise returns error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonMonitorGetTarget(Balloon *b,     // IN
                         uint32 *target) // OUT
{
   Backdoor_proto bp;
   unsigned long limit;
   uint32 limit32;
   uint32 status;

   limit = OS_ReservedPageGetLimit();

   /* Ensure limit fits in 32-bits */
   limit32 = (uint32)limit;
   if (limit32 != limit) {
      return BALLOON_FAILURE;
   }

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_TARGET;
   bp.in.size = limit;

   /* invoke backdoor */
   Backdoor_Balloon(&bp);

   /* parse return values */
   status  = bp.out.ax.word;
   *target = bp.out.bx.word;

   /* set flag if reset requested */
   if (status == BALLOON_ERROR_RESET) {
      b->resetFlag = 1;
   }

   /* update stats */
   STATS_INC(b->stats.target);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.targetFail);
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonMonitorLockPage --
 *
 *      Attempts to contact monitor and add PPN corresponding to
 *      the page handle to set of "balloon locked" pages.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonMonitorLockPage(Balloon *b,        // IN
                        PageHandle handle) // IN
{
   unsigned long ppn;
   uint32 ppn32;
   uint32 status, target;
   Backdoor_proto bp;

   ppn = OS_ReservedPageGetPPN(handle);

   /* Ensure PPN fits in 32-bits, i.e. guest memory is limited to 16TB. */
   ppn32 = (uint32)ppn;
   if (ppn32 != ppn) {
      return BALLOON_ERROR_PPN_INVALID;
   }

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_LOCK;
   bp.in.size = ppn32;

   /* invoke backdoor */
   Backdoor_Balloon(&bp);

   /* parse return values */
   status = bp.out.ax.word;
   target = bp.out.bx.word;

   /* set flag if reset requested */
   if (status == BALLOON_ERROR_RESET) {
      b->resetFlag = 1;
   }

   /* update stats */
   STATS_INC(b->stats.lock);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.lockFail);
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonMonitorUnlockPage --
 *
 *      Attempts to contact monitor and remove PPN corresponding to
 *      the page handle from set of "balloon locked" pages.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonMonitorUnlockPage(Balloon *b,        // IN
                          PageHandle handle) // IN
{
   unsigned long ppn;
   uint32 ppn32;
   uint32 status, target;
   Backdoor_proto bp;

   ppn = OS_ReservedPageGetPPN(handle);

   /* Ensure PPN fits in 32-bits, i.e. guest memory is limited to 16TB. */
   ppn32 = (uint32)ppn;
   if (ppn32 != ppn) {
      return BALLOON_ERROR_PPN_INVALID;
   }

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_UNLOCK;
   bp.in.size = ppn32;

   /* invoke backdoor */
   Backdoor_Balloon(&bp);

   /* parse return values */
   status = bp.out.ax.word;
   target = bp.out.bx.word;

   /* set flag if reset requested */
   if (status == BALLOON_ERROR_RESET) {
      b->resetFlag = 1;
   }

   /* update stats */
   STATS_INC(b->stats.unlock);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.unlockFail);
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_Init --
 *
 *      Initializes state oif the balloon.
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
Balloon_Init(BalloonGuest guestType)
{
   Balloon *b = &globalBalloon;

   DblLnkLst_Init(&b->chunks);

   b->guestType = guestType;

   /* initialize rates */
   b->rateAlloc = BALLOON_RATE_ALLOC_MAX;
   b->rateFree  = BALLOON_RATE_FREE_MAX;

   /* initialize reset flag */
   b->resetFlag = TRUE;

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
   BalloonMonitorStart(b);
   Balloon_Deallocate(b);
}

#ifdef __cplusplus
}
#endif
