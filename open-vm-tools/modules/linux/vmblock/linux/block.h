/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
