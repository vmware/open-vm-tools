/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
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
