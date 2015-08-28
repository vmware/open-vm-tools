/*********************************************************
 * Copyright (C) 2000,2014 VMware, Inc. All rights reserved.
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
 * os.h --
 *
 *      Definitions for OS-specific wrapper functions required by "vmmemctl".
 */

#ifndef	OS_H
#define	OS_H

#include "vm_basic_types.h"
#include "balloon_def.h"

/*
 * Types
 */
#if defined __APPLE__
typedef uint64 PageHandle;
#else
typedef uintptr_t PageHandle;
#endif
typedef uintptr_t Mapping;

#define PAGE_HANDLE_INVALID     0
#define MAPPING_INVALID         0

#define OS_SMALL_PAGE_ORDER (0) // 4 KB small pages
#define OS_LARGE_PAGE_ORDER (9) // 2 MB large pages
#define OS_LARGE_2_SMALL_PAGES (1 << OS_LARGE_PAGE_ORDER)

/*
 * Operations
 */

extern void OS_MemZero(void *ptr, size_t size);
extern void OS_MemCopy(void *dest, const void *src, size_t size);

extern void *OS_Malloc(size_t size);
extern void OS_Free(void *ptr, size_t size);

extern void OS_Yield(void);

extern unsigned long OS_ReservedPageGetLimit(void);
extern PA64          OS_ReservedPageGetPA(PageHandle handle);
extern PageHandle    OS_ReservedPageGetHandle(PA64 pa);
extern PageHandle    OS_ReservedPageAlloc(int canSleep, int isLargePage);
extern void          OS_ReservedPageFree(PageHandle handle, int isLargePage);

extern Mapping       OS_MapPageHandle(PageHandle handle);
extern void          *OS_Mapping2Addr(Mapping mapping);
extern void          OS_UnmapPage(Mapping mapping);

#endif  /* OS_H */
