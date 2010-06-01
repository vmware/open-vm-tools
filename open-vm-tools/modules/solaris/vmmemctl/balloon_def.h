/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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
 * balloon_def.h -- 
 *
 *      Definitions for server "balloon" mechanism for reclaiming
 *      physical memory from a VM.
 */

#ifndef _BALLOON_DEF_H
#define _BALLOON_DEF_H

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

/* guest identities */
#define BALLOON_GUEST_UNKNOWN           (0)
#define BALLOON_GUEST_LINUX             (1)
#define BALLOON_GUEST_BSD               (2)
#define BALLOON_GUEST_WINDOWS_NT4       (3)
#define BALLOON_GUEST_WINDOWS_NT5       (4)
#define BALLOON_GUEST_SOLARIS           (5)

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
