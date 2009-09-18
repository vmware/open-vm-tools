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
#define BALLOON_STATS_PROCFS

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

#define BALLOON_NAME                    "vmmemctl"
#define BALLOON_NAME_VERBOSE            "VMware memory control driver"

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

// Maximum number of page allocations without yielding processor
#define BALLOON_ALLOC_YIELD_THRESHOLD   1024


/*
 * Types
 */

typedef struct BalloonChunk {
   PageHandle page[BALLOON_CHUNK_PAGES];
   uint32 nextPage;
   struct BalloonChunk *prev;
   struct BalloonChunk *next;
} BalloonChunk;

typedef struct {
   PageHandle page[BALLOON_ERROR_PAGES];
   uint32 nextPage;
} BalloonErrorPages;

typedef struct {
   /* sets of reserved physical pages */
   BalloonChunk *chunks;
   int nChunks;

   /* transient list of non-balloonable pages */
   BalloonErrorPages errors;

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
static Bool timerStarted;

/*
 * Balloon operations
 */
static int  BalloonPageAlloc(Balloon *b, BalloonPageAllocType allocType);
static int  BalloonPageFree(Balloon *b, int monitorUnlock);
static int  BalloonAdjustSize(Balloon *b, uint32 target);
static void BalloonReset(Balloon *b);
static void BalloonTimerHandler(void *clientData);

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
 * Macros to generate operations for simple lists of OBJ.
 * OBJ must contain next and prev fields.
 */

#define GENERATE_LIST_INSERT(OBJ)                       \
static void OBJ ## _Insert(OBJ **head, OBJ *obj)        \
{                                                       \
   OBJ *h = *head;                                      \
                                                        \
   /* add element to head of list */                    \
   obj->next = h;                                       \
   if (h != NULL) {                                     \
      h->prev = obj;                                    \
   }                                                    \
   *head = obj;                                         \
   obj->prev = NULL;                                    \
}

#define GENERATE_LIST_REMOVE(OBJ)                       \
static void OBJ ## _Remove(OBJ **head, OBJ *obj)        \
{                                                       \
    /* splice element out of list */                    \
    if (obj->prev != NULL) {                            \
      obj->prev->next = obj->next;                      \
    } else {                                            \
      *head = obj->next;                                \
    }                                                   \
    if (obj->next != NULL) {                            \
      obj->next->prev = obj->prev;                      \
    }                                                   \
}

/*
 * List Operations
 */

GENERATE_LIST_INSERT(BalloonChunk);
GENERATE_LIST_REMOVE(BalloonChunk);


/*
 *----------------------------------------------------------------------
 *
 * BalloonProcRead --
 *
 *      Ballon driver status reporting routine.  Note that this is only
 *      used for Linux.
 *
 * Results:
 *      Writes ASCII status information into "buf".
 *      Returns number of bytes written.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BalloonProcRead(char *buf,   // OUT
                size_t size) // IN
{
   int len = 0;
   BalloonStats stats;

   Balloon_GetStats(&stats);

   /* format size info */
   len += OS_Snprintf(buf + len, size - len,
                     "target:             %8d pages\n"
                     "current:            %8d pages\n",
                     stats.nPagesTarget,
                     stats.nPages);

   /* format rate info */
   len += OS_Snprintf(buf + len, size - len,
                     "rateNoSleepAlloc:   %8d pages/sec\n"
                     "rateSleepAlloc:     %8d pages/sec\n"
                     "rateFree:           %8d pages/sec\n",
                     BALLOON_NOSLEEP_ALLOC_MAX,
                     stats.rateAlloc,
                     stats.rateFree);

#ifdef BALLOON_STATS_PROCFS
   len += OS_Snprintf(buf + len, size - len,
                     "\n"
                     "timer:              %8u\n"
                     "start:              %8u (%4u failed)\n"
                     "guestType:          %8u (%4u failed)\n"
                     "lock:               %8u (%4u failed)\n"
                     "unlock:             %8u (%4u failed)\n"
                     "target:             %8u (%4u failed)\n"
                     "primNoSleepAlloc:   %8u (%4u failed)\n"
                     "primCanSleepAlloc:  %8u (%4u failed)\n"
                     "primFree:           %8u\n"
                     "errAlloc:           %8u\n"
                     "errFree:            %8u\n",
                     stats.timer,
                     stats.start, stats.startFail,
                     stats.guestType, stats.guestTypeFail,
                     stats.lock,  stats.lockFail,
                     stats.unlock, stats.unlockFail,
                     stats.target, stats.targetFail,
                     stats.primAlloc[BALLOON_PAGE_ALLOC_NOSLEEP],
                     stats.primAllocFail[BALLOON_PAGE_ALLOC_NOSLEEP],
                     stats.primAlloc[BALLOON_PAGE_ALLOC_CANSLEEP],
                     stats.primAllocFail[BALLOON_PAGE_ALLOC_CANSLEEP],
                     stats.primFree,
                     stats.primErrorPageAlloc,
                     stats.primErrorPageFree);
#endif

   return len;
}


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

void
Balloon_GetStats(BalloonStats *stats) // OUT
{
   Balloon *b = &globalBalloon;

   /*
    * Copy statistics out of global structure.
    */
   OS_MemCopy(stats, &b->stats, sizeof *stats);

   /*
    * Fill in additional information about size and rates, which is
    * normally kept in the Balloon structure itself.
    */
   stats->nPages = b->nPages;
   stats->nPagesTarget = b->nPagesTarget;
   stats->rateAlloc = b->rateAlloc;
   stats->rateFree = b->rateFree;
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
 * Balloon_Init --
 *
 *      Initializes device state for balloon "b".
 *
 * Results:
 *      Returns BALLOON_SUCCESS.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
Balloon_Init(Balloon *b) // IN
{
   /* clear state */
   OS_MemZero(b, sizeof *b);

   /* initialize rates */
   b->rateAlloc = BALLOON_RATE_ALLOC_MAX;
   b->rateFree  = BALLOON_RATE_FREE_MAX;

   /* initialize reset flag */
   b->resetFlag = 1;

   return BALLOON_SUCCESS;
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
 * BalloonTimerHandler --
 *
 *      Balloon bottom half handler.  Contacts monitor via backdoor
 *      to obtain balloon size target, and starts adjusting balloon
 *      size to achieve target by allocating or deallocating pages.
 *      Resets balloon if requested by the monitor.
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
BalloonTimerHandler(void *clientData) // IN
{
   Balloon *b = (Balloon *) clientData;
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
 * BalloonErrorPagesAlloc --
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
BalloonErrorPagesAlloc(Balloon *b,      // IN
                       PageHandle page) // IN
{
   /* fail if list already full */
   if (b->errors.nextPage >= BALLOON_ERROR_PAGES) {
      return(BALLOON_FAILURE);
   }

   /* add page to list */
   b->errors.page[b->errors.nextPage] = page;
   b->errors.nextPage++;
   STATS_INC(b->stats.primErrorPageAlloc);
   return(BALLOON_SUCCESS);
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
   for (i = 0; i < b->errors.nextPage; i++) {
      OS_ReservedPageFree(b->errors.page[i]);
      b->errors.page[i] = PAGE_HANDLE_INVALID;
      STATS_INC(b->stats.primErrorPageFree);
   }
   b->errors.nextPage = 0;
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
   BalloonChunk *chunk;
   PageHandle page;
   int status;

 retry:

   /* allocate page, fail if unable */
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

   /* find chunk with space, create if necessary */
   chunk = b->chunks;
   if ((chunk == NULL) || (chunk->nextPage >= BALLOON_CHUNK_PAGES)) {
      /* create new chunk */
      chunk = BalloonChunk_Create();
      if (chunk == NULL) {
         /* reclaim storage, fail */
         OS_ReservedPageFree(page);
         return BALLOON_PAGE_ALLOC_FAILURE;
      }
      BalloonChunk_Insert(&b->chunks, chunk);

      /* update stats */
      b->nChunks++;
   }

   /* inform monitor via backdoor */
   status = BalloonMonitorLockPage(b, page);
   if (status != BALLOON_SUCCESS) {
      /* place on list of non-balloonable pages, retry allocation */
      if ((status != BALLOON_ERROR_RESET) &&
          (BalloonErrorPagesAlloc(b, page) == BALLOON_SUCCESS)) {
         goto retry;
      }

      /* reclaim storage, fail */
      OS_ReservedPageFree(page);
      return status;
   }

   /* track allocated page */
   chunk->page[chunk->nextPage] = page;
   chunk->nextPage++;

   /* update balloon size */
   b->nPages++;

   return BALLOON_SUCCESS;
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
   BalloonChunk *chunk;
   PageHandle page;
   int status;

   chunk = b->chunks;

   while ((chunk != NULL) && (chunk->nextPage == 0)) {
      /* destroy empty chunk */
      BalloonChunk_Remove(&b->chunks, chunk);
      BalloonChunk_Destroy(chunk);

      /* update stats */
      b->nChunks--;

      chunk = b->chunks;
   }

   if (chunk == NULL) {
      return(BALLOON_FAILURE);
   }

   /* select page to deallocate */
   chunk->nextPage--;
   page = chunk->page[chunk->nextPage];

   /* inform monitor via backdoor */
   if (monitorUnlock) {
      status = BalloonMonitorUnlockPage(b, page);
      if (status != BALLOON_SUCCESS) {
         /* reset next pointer, fail */
         chunk->nextPage++;
         return(status);
      }
   }

   /* deallocate page */
   OS_ReservedPageFree(page);
   STATS_INC(b->stats.primFree);

   /* update balloon size */
   b->nPages--;

   /* reclaim chunk, if empty */
   if (chunk->nextPage == 0) {
      /* destroy empty chunk */
      BalloonChunk_Remove(&b->chunks, chunk);
      BalloonChunk_Destroy(chunk);

      /* update stats */
      b->nChunks--;
   }

   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonDecreaseRateAlloc --
 *
 *      Wrapper to quickly reduce the page allocation rate. This function
 *      is called only when a CANSLEEP allocation fails. This implies severe
 *      memory pressure inside the guest, so quickly decrease the rateAlloc.
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
BalloonDecreaseRateAlloc(Balloon *b) // IN
{
   if (BALLOON_RATE_ADAPT) {
      b->rateAlloc = MAX(b->rateAlloc / 2, BALLOON_RATE_ALLOC_MIN);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * BalloonIncreaseRateAlloc --
 *
 *      Wrapper to increase the page allocation rate.
 *
 *      This function is called when the balloon target is met or
 *      b->rateAlloc (or more) pages have been successfully allocated.
 *      This implies that the guest may not be under high memory
 *      pressure. So let us increase the rateAlloc.
 *
 *      If meeting balloon target requires less than b->rateAlloc
 *      pages, then we do not change the page allocation rate.
 *
 *      If the number of pages successfully allocated (nAlloc) is far
 *      higher than b->rateAlloc, then it implies that NOSLEEP
 *      allocations are highly successful. Therefore, we predict that
 *      the guest is under no memory pressure, and so increase
 *      b->rateAlloc quickly.
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
BalloonIncreaseRateAlloc(Balloon *b,    // IN
                         uint32 nAlloc) // IN
{
   if (BALLOON_RATE_ADAPT) {
      if (nAlloc >= b->rateAlloc) {
         uint32 mult = nAlloc / b->rateAlloc;
         b->rateAlloc = MIN(b->rateAlloc + mult * BALLOON_RATE_ALLOC_INC,
                            BALLOON_RATE_ALLOC_MAX);
      }
   }
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
   int status, allocations = 0;
   uint32 i, nAllocNoSleep, nAllocCanSleep;

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
   if (b->slowPageAllocationCycles > 0) {
      nAllocNoSleep = MIN(target - b->nPages, b->rateAlloc);
   } else {
      nAllocNoSleep = MIN(target - b->nPages, BALLOON_NOSLEEP_ALLOC_MAX);
   }

   for (i = 0; i < nAllocNoSleep; i++) {
      /* Try NOSLEEP allocation */
      status = BalloonPageAlloc(b, BALLOON_PAGE_ALLOC_NOSLEEP);
      if (status != BALLOON_SUCCESS) {
         if (status != BALLOON_PAGE_ALLOC_FAILURE) {
            /*
             * Not a page allocation failure, so release non-balloonable
             * pages, and fail.
             */
            BalloonErrorPagesFree(b);
            return(status);
         }
         /*
          * NOSLEEP page allocation failed, so the guest is under memory
          * pressure. Let us slowdown page allocations for next few
          * cycles so that the guest gets out of memory pressure.
          */
         b->slowPageAllocationCycles = SLOW_PAGE_ALLOCATION_CYCLES;
         break;
      }

      if (++allocations > BALLOON_ALLOC_YIELD_THRESHOLD) {
         OS_Yield();
         allocations = 0;
      }
   }

   /*
    * Check whether nosleep allocation successfully zapped nAllocNoSleep
    * pages.
    */
   if (i == nAllocNoSleep) {
      BalloonIncreaseRateAlloc(b, nAllocNoSleep);
      /* release non-balloonable pages, succeed */
      BalloonErrorPagesFree(b);
      return(BALLOON_SUCCESS);
   } else {
      /*
       * NOSLEEP allocation failed, so the guest is under memory pressure.
       * If already b->rateAlloc pages were zapped, then succeed. Otherwise,
       * try CANSLEEP allocation.
       */
      if (i > b->rateAlloc) {
         BalloonIncreaseRateAlloc(b, i);
         /* release non-balloonable pages, succeed */
         BalloonErrorPagesFree(b);
         return(BALLOON_SUCCESS);
      } else {
         /* update successful NOSLEEP allocations, and proceed */
         nAllocNoSleep = i;
      }
   }

   /*
    * Use CANSLEEP page allocation to inflate balloon if below target.
    *
    * Sleep allocations are required only when nosleep allocation fails.
    * This implies that the guest is already under memory pressure, so
    * let us always throttle canSleep allocations. The total number pages
    * allocated using noSleep and canSleep methods is throttled at
    * b->rateAlloc per second when the guest is under memory pressure.
    */
   nAllocCanSleep = target - b->nPages;
   nAllocCanSleep = MIN(nAllocCanSleep, b->rateAlloc - nAllocNoSleep);

   for (i = 0; i < nAllocCanSleep; i++) {
      /* Try CANSLEEP allocation */
      status = BalloonPageAlloc(b, BALLOON_PAGE_ALLOC_CANSLEEP);
      if(status != BALLOON_SUCCESS) {
         if (status == BALLOON_PAGE_ALLOC_FAILURE) {
            /*
             * CANSLEEP page allocation failed, so guest is under severe
             * memory pressure. Quickly decrease rateAlloc.
             */
            BalloonDecreaseRateAlloc(b);
         }
         /* release non-balloonable pages, fail */
         BalloonErrorPagesFree(b);
         return(status);
      }

      if (++allocations > BALLOON_ALLOC_YIELD_THRESHOLD) {
         OS_Yield();
         allocations = 0;
      }
   }

   /*
    * Either met the balloon target or b->rateAlloc pages have been
    * successfully zapped.
    */
   BalloonIncreaseRateAlloc(b, nAllocNoSleep + nAllocCanSleep);

   /* release non-balloonable pages, succeed */
   BalloonErrorPagesFree(b);
   return(BALLOON_SUCCESS);
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
         return(status);
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
   /* done if already at target */
   if (b->nPages == target) {
      return(BALLOON_SUCCESS);
   }

   /* inflate balloon if below target */
   if (b->nPages < target) {
      return BalloonInflate(b, target);
   }

   /* deflate balloon if above target */
   if (b->nPages > target) {
      return BalloonDeflate(b, target);
   }

   /* not reached */
   return(BALLOON_FAILURE);
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
   BalloonGuest identity;
   Backdoor_proto bp;

   identity = OS_Identity();
   ASSERT(identity == (uint32) identity);

   /* prepare backdoor args */
   bp.in.cx.halfs.low = BALLOON_BDOOR_CMD_GUEST_ID;
   bp.in.size = identity;

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
 * Balloon_ModuleInit --
 *
 *      Startup the balloon module.
 *
 * Results:
 *      On success: BALLOON_SUCCESS
 *      On failure: BALLOON_FAILURE
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Balloon_ModuleInit(void)
{
   static int initialized = 0;
   Balloon *b = &globalBalloon;

   /* initialize only once */
   if (initialized++) {
      return BALLOON_FAILURE;
   }

   /* os-specific initialization */
   if (!OS_Init(BALLOON_NAME, BALLOON_NAME_VERBOSE, BalloonProcRead)) {
      return BALLOON_FAILURE;
   }

   /* initialize global state */
   Balloon_Init(b);

   /* start timer */
   timerStarted = FALSE;
   if (!OS_TimerStart(BalloonTimerHandler, b)) {
      Balloon_ModuleCleanup();
      return BALLOON_FAILURE;
   }

   timerStarted = TRUE;
   return BALLOON_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * Balloon_ModuleCleanup --
 *
 *      Terminate and cleanup the balloon module.
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
Balloon_ModuleCleanup(void)
{
   Balloon *b = &globalBalloon;

   /* stop timer */
   if (timerStarted) {
      OS_TimerStop();
   }

   /*
    * Deallocate all reserved memory, and reset connection with monitor.
    * Reset connection before deallocating memory to avoid potential for
    * additional spurious resets from guest touching deallocated pages.
    */
   BalloonMonitorStart(b);
   Balloon_Deallocate(b);

   /* os-specific cleanup */
   OS_Cleanup();
}

#ifdef __cplusplus
}
#endif
