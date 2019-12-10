/*********************************************************
 * Copyright (C) 2000-2012,2014,2017-2019 VMware, Inc. All rights reserved.
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
 * balloon_def.h --
 *
 *      Definitions for server "balloon" mechanism for reclaiming
 *      physical memory from a VM.
 */

#ifndef _BALLOON_DEF_H
#define _BALLOON_DEF_H

#define INCLUDE_ALLOW_VMX
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "vm_basic_defs.h"
#include "vm_assert.h"

#if defined __cplusplus
extern "C" {
#endif


/*
 * constants
 */

/* backdoor port */
#define BALLOON_BDOOR_PORT              (0x5670)
#define BALLOON_BDOOR_MAGIC             (0x456c6d6f)

/*
 * Backdoor commands availability:
 *
 * +====================+======================+
 * |    CMD             | Capabilities         |
 * +--------------------+----------------------+
 * | START              | Always available (*) |
 * | TARGET             | Always available     |
 * | LOCK               | BASIC_CMDS           |
 * | UNLOCK             | BASIC_CMDS           |
 * | GUEST_ID           | Always available     |
 * | BATCHED_LOCK       | BATCHED_CMDS         |
 * | BATCHED_UNLOCK     | BATCHED_CMDS         |
 * | BATCHED_2M_LOCK    | BATCHED_2M_CMDS      |
 * | BATCHED_2M_UNLOCK  | BATCHED_2M_CMDS      |
 * | VMCI_DOORBELL_SET  | SIGNALED_WAKEUP_CMD  |
 * +====================+======================+
 *
 * (*) The START command has been slightly modified when more than the
 * basic commands are available: It returns
 * BALLOON_SUCCESS_WITH_CAPABILITIES with the available capabilities
 * stored in %ecx. Previously, a versioned protocol was used, and the
 * protocol that should be used was also returned in %ecx. Protocol
 * version 2 was the initial version and the only one shipped. Version 3
 * was temporary used internally but has caused several issue due to
 * protocol mismatch between monitor and guest.
 *
 */

/* backdoor command numbers */
#define BALLOON_BDOOR_CMD_START             (0)
#define BALLOON_BDOOR_CMD_TARGET            (1)
#define BALLOON_BDOOR_CMD_LOCK              (2)
#define BALLOON_BDOOR_CMD_UNLOCK            (3)
#define BALLOON_BDOOR_CMD_GUEST_ID          (4)
/* The command 5 was shortly used between 1881144 and 1901153. */
#define BALLOON_BDOOR_CMD_BATCHED_LOCK      (6)
#define BALLOON_BDOOR_CMD_BATCHED_UNLOCK    (7)
#define BALLOON_BDOOR_CMD_BATCHED_2M_LOCK   (8)
#define BALLOON_BDOOR_CMD_BATCHED_2M_UNLOCK (9)
#define BALLOON_BDOOR_CMD_VMCI_DOORBELL_SET (10)

/* balloon capabilities */
typedef enum {
   /*
    * Bit 0 is not used and shouldn't be used, due to issue with
    * protocol v3, to avoid ambiguity between protocol v3 and
    * capabilities, leave this bit as 0. That way, by masking guest
    * capabilities with monitor capabilities, bit 0 will always be set
    * to 0, and buggy v3 tool will automatically switch to unbatched
    * LOCK and UNLOCK.
    */
   BALLOON_BASIC_CMDS           = (1 << 1),
   BALLOON_BATCHED_CMDS         = (1 << 2),
   BALLOON_BATCHED_2M_CMDS      = (1 << 3),
   BALLOON_SIGNALED_WAKEUP_CMD  = (1 << 4),
   BALLOON_64_BIT_TARGET        = (1 << 5),
} BalloonCapabilities;

/* use config value for max balloon size */
#define BALLOON_MAX_SIZE_USE_CONFIG     (0)

/*
 * Guest identities
 *
 *      Note : all values should fit in 32 bits
 */
typedef enum {
   BALLOON_GUEST_UNKNOWN     = 0,
   BALLOON_GUEST_LINUX       = 1,
   BALLOON_GUEST_BSD         = 2,
   BALLOON_GUEST_WINDOWS_NT4 = 3,
   BALLOON_GUEST_WINDOWS_NT5 = 4,
   BALLOON_GUEST_SOLARIS     = 5,
   BALLOON_GUEST_MACOS       = 6,
   BALLOON_GUEST_FROBOS      = 7,
} BalloonGuest;

/* error codes */
#define BALLOON_SUCCESS                         (0)
#define BALLOON_FAILURE                        (-1)
#define BALLOON_ERROR_CMD_INVALID               (1)
#define BALLOON_ERROR_PPN_INVALID               (2)
#define BALLOON_ERROR_PPN_LOCKED                (3)
#define BALLOON_ERROR_PPN_UNLOCKED              (4)
#define BALLOON_ERROR_PPN_PINNED                (5)
#define BALLOON_ERROR_PPN_NOTNEEDED             (6)
#define BALLOON_ERROR_RESET                     (7)
#define BALLOON_ERROR_BUSY                      (8)

#define BALLOON_SUCCESS_WITH_CAPABILITIES       (0x03000000)

/*
 * BatchPage.
 */
#define BALLOON_BATCH_MAX_ENTRIES       (PAGE_SIZE / sizeof(PA64))

/*
 * We are using the fact that for 4k pages, the 12LSB are set to 0, so
 * we can use them and mask those bit when we need the real PA.
 *
 * +=============+==========+========+
 * |             |          |        |
 * | Page number | Reserved | Status |
 * |             |          |        |
 * +=============+==========+========+
 * 64  PAGE_SHIFT          6         0
 *
 * The reserved field should be set to 0.
 *
 */


#define BALLOON_BATCH_STATUS_MASK       MASK64(5)
#define BALLOON_BATCH_PAGE_MASK         (~MASK64(PAGE_SHIFT))

typedef struct BalloonBatchPage {
   PA64 entries[BALLOON_BATCH_MAX_ENTRIES];
} BalloonBatchPage;


/*
 *-----------------------------------------------------------------------------
 *
 * Balloon_BatchGetPA --
 *
 *      Get the page stored in the batch page at idx.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE PA64
Balloon_BatchGetPA(BalloonBatchPage *batchPage,         // IN
                   uint16 idx)                          // IN
{
   ASSERT(idx < BALLOON_BATCH_MAX_ENTRIES);
   return batchPage->entries[idx] & BALLOON_BATCH_PAGE_MASK;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Balloon_BatchGetStatus --
 *
 *      Get the error code associated with a page.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE uint8
Balloon_BatchGetStatus(BalloonBatchPage *batchPage,     // IN
                       uint16 idx)                      // IN
{
   ASSERT(idx < BALLOON_BATCH_MAX_ENTRIES);
   return (uint8)(batchPage->entries[idx] & BALLOON_BATCH_STATUS_MASK);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Balloon_BatchSetPA --
 *
 *      Store the page in the batch page at idx.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE void
Balloon_BatchSetPA(BalloonBatchPage *batchPage,            // IN
                   uint16 idx,                             // IN
                   PA64 pa)                                // IN
{
   ASSERT(idx < BALLOON_BATCH_MAX_ENTRIES);
   ASSERT((pa & ~BALLOON_BATCH_PAGE_MASK) == 0);
   batchPage->entries[idx] = pa;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Balloon_BatchSetStatus --
 *
 *      Set the error code associated with a page.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */
static INLINE void
Balloon_BatchSetStatus(BalloonBatchPage *batchPage,      // IN
                       uint16 idx,                       // IN
                       int error)                        // IN
{
   PA64 pa = Balloon_BatchGetPA(batchPage, idx);
   ASSERT(idx < BALLOON_BATCH_MAX_ENTRIES);
   ASSERT(error <= BALLOON_ERROR_BUSY && error >= BALLOON_FAILURE);
   batchPage->entries[idx] = pa | (PPN)error;
}

MY_ASSERTS(BALLOON_BATCH_SIZE,
           ASSERT_ON_COMPILE(sizeof(BalloonBatchPage) == PAGE_SIZE);
)


#if defined __cplusplus
} // extern "C"
#endif

#endif  /* _BALLOON_DEF_H */
