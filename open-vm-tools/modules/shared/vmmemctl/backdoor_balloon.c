/*********************************************************
 * Copyright (C) 2012,2014,2018-2019 VMware, Inc. All rights reserved.
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
#include "backdoor_balloon.h"
#include "backdoor.h"
#include "balloon_def.h"
#include "os.h"

/*
 *----------------------------------------------------------------------
 *
 * BackdoorCmd --
 *
 *      Do the balloon hypercall to the vmkernel.
 *
 * Results:
 *      Hypercall status will be returned and out will be filled
 *      if it's not NULL, either by bx or cx depending on the command.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static int
BackdoorCmd(uint16 cmd,     // IN
            uint64 arg1,    // IN
            uint32 arg2,    // IN
            uint64 *out,    // OUT
            int *resetFlag) // OUT
{
   Backdoor_proto bp;
   int status;

   /* prepare backdoor args */
   bp.in.cx.halfs.low = cmd;
   bp.in.size = (size_t)arg1;
   ASSERT(bp.in.size == arg1);
   bp.in.si.word = arg2;

   /* invoke backdoor */
   bp.in.ax.word = BALLOON_BDOOR_MAGIC;
   bp.in.dx.halfs.low = BALLOON_BDOOR_PORT;
   Backdoor_InOut(&bp);

   status = bp.out.ax.word;
   /* set flag if reset requested */
   if (status == BALLOON_ERROR_RESET) {
      *resetFlag = 1;
   }

   if (out) {
#ifdef VM_X86_64
      if (cmd == BALLOON_BDOOR_CMD_START) {
         *out = bp.out.cx.quad;
      } else {
         *out = bp.out.bx.quad;
      }
#else
      if (cmd == BALLOON_BDOOR_CMD_START) {
         *out = bp.out.cx.word;
      } else {
         *out = bp.out.bx.word;
      }
#endif
   }

   return status;
}


/*
 *----------------------------------------------------------------------
 *
 * Backdoor_MonitorStart --
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

int
Backdoor_MonitorStart(Balloon *b,               // IN/OUT
                      uint32 protoVersion)      // IN
{
   uint64 capabilities;
   int status = BackdoorCmd(BALLOON_BDOOR_CMD_START, protoVersion, 0,
                            &capabilities, &b->resetFlag);
   /*
    * If return code is BALLOON_SUCCESS_WITH_CAPABILITY, then ESX is
    * sending the common capabilities supported by the monitor and the
    * guest in cx.
    */
   if (status == BALLOON_SUCCESS_WITH_CAPABILITIES) {
      b->hypervisorCapabilities = capabilities;
      status = BALLOON_SUCCESS;
   } else if (status == BALLOON_SUCCESS) {
      b->hypervisorCapabilities = BALLOON_BASIC_CMDS;
   }

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
 * Backdoor_MonitorGuestType --
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

int
Backdoor_MonitorGuestType(Balloon *b) // IN/OUT
{
   int status = BackdoorCmd(BALLOON_BDOOR_CMD_GUEST_ID, b->guestType, 0,
                            NULL, &b->resetFlag);

   /* update stats */
   STATS_INC(b->stats.guestType);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.guestTypeFail);
   }

   return status;
}


static Bool
BackdoorHasCapability(Balloon *b,
                      uint32 capability)
{
   return (b->hypervisorCapabilities & capability) == capability;
}


/*
 *----------------------------------------------------------------------
 *
 * Backdoor_MonitorGetTarget --
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

int
Backdoor_MonitorGetTarget(Balloon *b,     // IN/OUT
                          uint64 *target) // OUT
{
   uint64 limit;
   int status;

   limit = OS_ReservedPageGetLimit();

   if (!BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      if (limit != (uint32)limit) {
         limit = (uint32)limit;
      }
   }

   status = BackdoorCmd(BALLOON_BDOOR_CMD_TARGET, limit, 0, target,
                        &b->resetFlag);

   if (target != NULL && !BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      *target = MIN(MAX_UINT32, *target);
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
 * Backdoor_MonitorLockPage --
 *
 *      Attempts to contact monitor and add PPN corresponding to
 *      the page handle to set of "balloon locked" pages.
 *      If the current protocol support batching, it will balloon all
 *      PPNs listed in the batch page.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Backdoor_MonitorLockPage(Balloon *b,     // IN/OUT
                         PPN64 ppn,      // IN
                         uint64 *target) // OUT
{
   int status;

   if (!BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      uint32 ppn32 = (uint32)ppn;
      /* Ensure PPN fits in 32-bits, i.e. guest memory is limited to 16TB. */
      if (ppn32 != ppn) {
         return BALLOON_ERROR_PPN_INVALID;
      }
   }

   status = BackdoorCmd(BALLOON_BDOOR_CMD_LOCK, ppn, 0, target,
                        &b->resetFlag);
   if (target != NULL && !BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      *target = MIN(MAX_UINT32, *target);
   }

   /* update stats */
   STATS_INC(b->stats.lock[FALSE]);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.lockFail[FALSE]);
   }

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * Backdoor_MonitorUnlockPage --
 *
 *      Attempts to contact monitor and remove PPN corresponding to
 *      the page handle from set of "balloon locked" pages.
 *      If the current protocol support batching, it will remove all
 *      the PPNs listed in the batch page.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Backdoor_MonitorUnlockPage(Balloon *b,     // IN/OUT
                           PPN64 ppn,      // IN
                           uint64 *target) // OUT
{
   int status;

   if (!BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      uint32 ppn32 = (uint32)ppn;

      /* Ensure PPN fits in 32-bits, i.e. guest memory is limited to 16TB. */
      if (ppn32 != ppn) {
         return BALLOON_ERROR_PPN_INVALID;
      }
   }

   status = BackdoorCmd(BALLOON_BDOOR_CMD_UNLOCK, ppn, 0, target,
                        &b->resetFlag);
   if (target != NULL && !BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      *target = MIN(MAX_UINT32, *target);
   }

   /* update stats */
   STATS_INC(b->stats.unlock[FALSE]);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.unlockFail[FALSE]);
   }

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * Backdoor_MonitorLockPagesBatched --
 *
 *      Balloon all PPNs listed in the batch page.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Backdoor_MonitorLockPagesBatched(Balloon *b,      // IN/OUT
                                 PPN64 ppn,       // IN
                                 uint32 nPages,   // IN
                                 int isLargePage, // IN
                                 uint64 *target)  // OUT
{
   int status;
   uint16 cmd;

   if (isLargePage) {
      cmd = BALLOON_BDOOR_CMD_BATCHED_2M_LOCK;
   } else {
      cmd = BALLOON_BDOOR_CMD_BATCHED_LOCK;
   }

   status = BackdoorCmd(cmd, (size_t)ppn, nPages, target, &b->resetFlag);
   if (target != NULL && !BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      *target = MIN(MAX_UINT32, *target);
   }

   /* update stats */
   STATS_INC(b->stats.lock[isLargePage]);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.lockFail[isLargePage]);
   }

   return status;
}

/*
 *----------------------------------------------------------------------
 *
 * Backdoor_MonitorUnlockPagesBatched --
 *
 *      Unballoon all PPNs listed in the batch page.
 *
 * Results:
 *      Returns BALLOON_SUCCESS if successful, otherwise error code.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Backdoor_MonitorUnlockPagesBatched(Balloon *b,      // IN/OUT
                                   PPN64 ppn,       // IN
                                   uint32 nPages,   // IN
                                   int isLargePage, // IN
                                   uint64 *target)  // OUT
{
   int status;
   uint16 cmd;

   if (isLargePage) {
      cmd = BALLOON_BDOOR_CMD_BATCHED_2M_UNLOCK;
   } else {
      cmd = BALLOON_BDOOR_CMD_BATCHED_UNLOCK;
   }

   status = BackdoorCmd(cmd, (size_t)ppn, nPages, target, &b->resetFlag);
   if (target != NULL && !BackdoorHasCapability(b, BALLOON_64_BIT_TARGET)) {
      *target = MIN(MAX_UINT32, *target);
   }

   /* update stats */
   STATS_INC(b->stats.unlock[isLargePage]);
   if (status != BALLOON_SUCCESS) {
      STATS_INC(b->stats.unlockFail[isLargePage]);
   }

   return status;
}
