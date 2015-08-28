/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
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
 * vmballoon_kstats.c --
 *
 * 	Functions reporting status for vmballoon driver in the form of
 *	kstats.
 */

/*
 * Compile-Time Options
 */

#include <sys/types.h>
#include <sys/kstat.h>
#include <sys/errno.h>
#include "os.h"
#include "vmballoon.h"
#include "vmballoon_kstats.h"

/*
 * Information to be reported to user level through kstats.  This
 * table should be kept in sync with the corresponding info reported
 * through procfs by the linux driver.  To display this information
 * on a Solaris system with the driver loaded, run "kstat -m vmmemctl".
 */
typedef struct {
   kstat_named_t nPagesTarget;
   kstat_named_t nPages;
   kstat_named_t rateAlloc;
   kstat_named_t rateFree;
   kstat_named_t timer;
   kstat_named_t start;
   kstat_named_t startFail;
   kstat_named_t guestType;
   kstat_named_t guestTypeFail;
   kstat_named_t lock;
   kstat_named_t lockFail;
   kstat_named_t unlock;
   kstat_named_t unlockFail;
   kstat_named_t target;
   kstat_named_t targetFail;
   kstat_named_t primAlloc[BALLOON_PAGE_ALLOC_TYPES_NR];
   kstat_named_t primAllocFail[BALLOON_PAGE_ALLOC_TYPES_NR];
   kstat_named_t primFree;
   kstat_named_t primErrorPageAlloc;
   kstat_named_t primErrorPageFree;
} BalloonKstats;

/*
 *----------------------------------------------------------------------
 *
 * BalloonKstatUpdate --
 *
 *      Ballon driver status reporting routine.
 *
 * Results:
 *	Copies current driver status and statistics into kstat structure
 *	for reporting to user level.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */
static int
BalloonKstatUpdate(kstat_t *ksp, int rw)
{
   int i;
   const BalloonStats *stats;
   BalloonKstats *bkp = ksp->ks_data;

   if (rw == KSTAT_WRITE)
      return (EACCES);

   stats = Balloon_GetStats();

   /* size info */
   bkp->nPagesTarget.value.ui32 = stats->nPagesTarget;
   bkp->nPages.value.ui32 = stats->nPages;

   /* rate info */
   bkp->rateAlloc.value.ui32 = stats->rateAlloc;
   bkp->rateFree.value.ui32 = stats->rateFree;

   /* statistics */
   bkp->timer.value.ui32 = stats->timer;
   bkp->start.value.ui32 = stats->start;
   bkp->startFail.value.ui32 = stats->startFail;
   bkp->guestType.value.ui32 = stats->guestType;
   bkp->guestTypeFail.value.ui32 = stats->guestTypeFail;
   bkp->lock.value.ui32 = stats->lock[FALSE];
   bkp->lockFail.value.ui32 = stats->lockFail[FALSE];
   bkp->unlock.value.ui32 = stats->unlock[FALSE];
   bkp->unlockFail.value.ui32 = stats->unlockFail[FALSE];
   bkp->target.value.ui32 = stats->target;
   bkp->targetFail.value.ui32 = stats->targetFail;
   for (i = 0; i < BALLOON_PAGE_ALLOC_TYPES_NR; i++) {
      bkp->primAlloc[i].value.ui32 = stats->primAlloc[i];
      bkp->primAllocFail[i].value.ui32 = stats->primAllocFail[i];
   }
   bkp->primFree.value.ui32 = stats->primFree[FALSE];
   bkp->primErrorPageAlloc.value.ui32 = stats->primErrorPageAlloc[FALSE];
   bkp->primErrorPageFree.value.ui32 = stats->primErrorPageFree[FALSE];

   return 0;
}

/*
 * Set up statistics for the balloon driver.  Creates and initializes
 * the kstats structure.
 */
kstat_t *
BalloonKstatCreate(void)
{
   kstat_t *ksp;
   BalloonKstats *bkp;

   ksp = kstat_create("vmmemctl", 0, "vmmemctl", "vm", KSTAT_TYPE_NAMED,
		      sizeof (BalloonKstats) / sizeof (kstat_named_t), 0);

   if (ksp == NULL)
      return (NULL);	/* can't allocate space, give up (no kstats) */

   bkp = ksp->ks_data;
   kstat_named_init(&bkp->nPagesTarget, "targetPages", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->nPages, "currentPages", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->rateAlloc, "rateAlloc", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->rateFree, "rateFree", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->timer, "timer", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->start, "start", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->startFail, "startFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->guestType, "guestType", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->guestTypeFail, "guestTypeFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->lock, "lock", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->lockFail, "lockFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->unlock, "unlock", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->unlockFail, "unlockFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->target, "target", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->targetFail, "targetFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAlloc[BALLOON_PAGE_ALLOC_LPAGE],
                        "primAllocLPage", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAlloc[BALLOON_PAGE_ALLOC_NOSLEEP], 
			"primAllocNoSleep", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAlloc[BALLOON_PAGE_ALLOC_CANSLEEP],
			"primAllocCanSleep", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAllocFail[BALLOON_PAGE_ALLOC_LPAGE],
                        "primAllocLPageFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAllocFail[BALLOON_PAGE_ALLOC_NOSLEEP],
			"primAllocNoSleepFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primAllocFail[BALLOON_PAGE_ALLOC_CANSLEEP],
			"primAllocCanSleepFail", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primFree, "primFree", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primErrorPageAlloc, "errAlloc", KSTAT_DATA_UINT32);
   kstat_named_init(&bkp->primErrorPageFree, "errFree", KSTAT_DATA_UINT32);

   /* set update function to be run when kstats are read */
   ksp->ks_update = BalloonKstatUpdate;

   kstat_install(ksp);

   return ksp;
}

void BalloonKstatDelete(kstat_t *ksp)
{
   if (ksp != NULL)
      kstat_delete(ksp);
}
