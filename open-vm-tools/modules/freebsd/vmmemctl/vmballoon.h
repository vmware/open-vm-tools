/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/* 
 * vmballoon.h: Definitions and macros for vmballoon driver.
 */

#ifndef	VMBALLOON_H
#define	VMBALLOON_H

#include "vm_basic_types.h"

/*
 * Page allocation flags
 */
typedef enum BalloonPageAllocType {
   BALLOON_PAGE_ALLOC_NOSLEEP = 0,
   BALLOON_PAGE_ALLOC_CANSLEEP = 1,
   BALLOON_PAGE_ALLOC_TYPES_NR,	// total number of alloc types
} BalloonPageAllocType;

/*
 * Types
 */

typedef struct {
   /* current status */
   uint32 nPages;
   uint32 nPagesTarget;

   /* adjustment rates */
   uint32 rateAlloc;
   uint32 rateFree;

   /* high-level operations */
   uint32 timer;

   /* primitives */
   uint32 primAlloc[BALLOON_PAGE_ALLOC_TYPES_NR];
   uint32 primAllocFail[BALLOON_PAGE_ALLOC_TYPES_NR];
   uint32 primFree;
   uint32 primErrorPageAlloc;
   uint32 primErrorPageFree;

   /* monitor operations */
   uint32 lock;
   uint32 lockFail;
   uint32 unlock;
   uint32 unlockFail;
   uint32 target;
   uint32 targetFail;
   uint32 start;
   uint32 startFail;
   uint32 guestType;
   uint32 guestTypeFail;
} BalloonStats;

/*
 * Operations
 */

extern void BalloonGetStats(BalloonStats *stats);
extern int  init_module(void);
extern void cleanup_module(void);

#endif	/* VMBALLOON_H */
