/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * block.h --
 *
 *   Blocking operations for the vmblock driver.
 */

#ifndef __BLOCK_H__
#define __BLOCK_H__

#include "os.h"

typedef struct BlockInfo * BlockHandle;

/*
 * Global functions
 */

int BlockInit(void);
void BlockCleanup(void);
int BlockAddFileBlock(const char *filename, const os_blocker_id_t blocker);
int BlockRemoveFileBlock(const char *filename, const os_blocker_id_t blocker);
unsigned int BlockRemoveAllBlocks(const os_blocker_id_t blocker);
int BlockWaitOnFile(const char *filename, BlockHandle cookie);
BlockHandle BlockLookup(const char *filename, const os_blocker_id_t blocker);
#ifdef VMX86_DEVEL
void BlockListFileBlocks(void);
#endif

#endif /* __BLOCK_H__ */
