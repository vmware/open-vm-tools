/*********************************************************
 * Copyright (C) 2000-2012,2014,2018-2019 VMware, Inc. All rights reserved.
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
 * vmballoon.h: Definitions and macros for vmballoon driver.
 */

#ifndef	VMBALLOON_H
#define	VMBALLOON_H

#include "balloonInt.h"
#include "vm_basic_types.h"
#include "dbllnklst.h"
#include "os.h"

/*
 * Page allocation flags
 */
typedef enum BalloonPageAllocType {
   BALLOON_PAGE_ALLOC_LPAGE = 0,
   BALLOON_PAGE_ALLOC_NOSLEEP = 1,
   BALLOON_PAGE_ALLOC_CANSLEEP = 2,
   BALLOON_PAGE_ALLOC_TYPES_NR,	// total number of alloc types
} BalloonPageAllocType;


/*
 * Types
 */

typedef struct {
   /* current status */
   uint64 nPages;
   uint64 nPagesTarget;

   /* adjustment rates */
   uint32 rateNoSleepAlloc;
   uint32 rateAlloc;
   uint32 rateFree;

   /* high-level operations */
   uint32 timer;

   /* primitives */
   uint32 primAlloc[BALLOON_PAGE_ALLOC_TYPES_NR];
   uint32 primAllocFail[BALLOON_PAGE_ALLOC_TYPES_NR];
   uint32 primFree[2];
   uint32 primErrorPageAlloc[2];
   uint32 primErrorPageFree[2];

   /* monitor operations */
   uint32 lock[2];
   uint32 lockFail[2];
   uint32 unlock[2];
   uint32 unlockFail[2];
   uint32 target;
   uint32 targetFail;
   uint32 start;
   uint32 startFail;
   uint32 guestType;
   uint32 guestTypeFail;
} BalloonStats;

#define BALLOON_ERROR_PAGES             16

typedef struct {
   PageHandle entries[BALLOON_ERROR_PAGES];
   uint32 nEntries;
} BalloonErrorPages;

#define BALLOON_CHUNK_ENTRIES             1000

typedef struct BalloonChunk {
   PageHandle entries[BALLOON_CHUNK_ENTRIES];
   uint32 nEntries;
   DblLnkLst_Links node;
} BalloonChunk;

struct BalloonOps;

typedef struct {
   DblLnkLst_Links chunks;
   int nChunks;
} BalloonChunkList;

typedef struct {
   /* sets of reserved physical pages */
   BalloonChunkList pages[2];

   /* transient list of non-balloonable pages */
   BalloonErrorPages errors[2];

   BalloonGuest guestType;

   /* balloon size (in small pages) */
   uint64 nPages;

   /* target balloon size (in small pages) */
   uint64 nPagesTarget;

   /* reset flag */
   int resetFlag;

   /* adjustment rates (pages per second) */
   int rateAlloc;
   int rateFree;

   /* slowdown page allocations for next few cycles */
   int slowPageAllocationCycles;

   /* statistics */
   BalloonStats stats;

   /* hypervisor exposed capabilities */
   BalloonCapabilities hypervisorCapabilities;

   /* balloon operations, tied to the capabilities */
   const struct BalloonOps *balloonOps;

   /* Either the batch page handle, or the page to lock on v2 */
   PageHandle pageHandle;
   Mapping batchPageMapping;
   BalloonBatchPage *batchPage;
   uint16 batchMaxEntries;

   BalloonChunk *fallbackChunk;
} Balloon;

typedef struct BalloonOps {
   void (*addPage)(Balloon *b, uint16 idx, PageHandle entries);
   int (*lock)(Balloon *b, uint16 nPages, int isLargePages, uint64 *target);
   int (*unlock)(Balloon *b, uint16 nPages, int isLargePages, uint64 *target);
} BalloonOps;

/*
 * Operations
 */

Bool Balloon_Init(BalloonGuest guestType);
void Balloon_Cleanup(void);

void Balloon_QueryAndExecute(void);

const BalloonStats *Balloon_GetStats(void);

#endif	/* VMBALLOON_H */
