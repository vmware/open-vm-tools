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

/*
 * constants
 */

/* backdoor port */
#define BALLOON_BDOOR_PORT              (0x5670)
#define BALLOON_BDOOR_MAGIC             (0x456c6d6f)

/* backdoor command numbers */
#define BALLOON_BDOOR_CMD_START         (0)
#define BALLOON_BDOOR_CMD_TARGET        (1)
#define BALLOON_BDOOR_CMD_LOCK          (2)
#define BALLOON_BDOOR_CMD_UNLOCK        (3)
#define BALLOON_BDOOR_CMD_GUEST_ID      (4)

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
#define BALLOON_SUCCESS                 (0)
#define BALLOON_FAILURE                (-1)
#define BALLOON_ERROR_CMD_INVALID       (1)
#define BALLOON_ERROR_PPN_INVALID       (2)
#define BALLOON_ERROR_PPN_LOCKED        (3)
#define BALLOON_ERROR_PPN_UNLOCKED      (4)
#define BALLOON_ERROR_PPN_PINNED        (5)
#define BALLOON_ERROR_PPN_NOTNEEDED     (6)
#define BALLOON_ERROR_RESET             (7)
#define BALLOON_ERROR_BUSY              (8)

#endif  /* _BALLOON_DEF_H */
