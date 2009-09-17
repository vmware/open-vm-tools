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

typedef void OSTimerHandler(void *clientData);
typedef int  OSStatusHandler(char *buf, size_t size);
typedef uintptr_t PageHandle;

#define PAGE_HANDLE_INVALID 0

/*
 * Operations
 */

extern void OS_Init(const char *name,
                    const char *nameVerbose,
                    OSStatusHandler *handler);
extern void OS_Cleanup(void);
extern BalloonGuest OS_Identity(void);

extern void OS_MemZero(void *ptr, size_t size);
extern void OS_MemCopy(void *dest, const void *src, size_t size);
extern int  OS_Snprintf(char *buf, size_t size, const char *format, ...);

extern void *OS_Malloc(size_t size);
extern void OS_Free(void *ptr, size_t size);

extern void OS_Yield(void);

extern Bool OS_TimerStart(OSTimerHandler *handler, void *clientData);
extern void OS_TimerStop(void);

extern unsigned long OS_ReservedPageGetLimit(void);
extern unsigned long OS_ReservedPageGetPPN(PageHandle handle);
extern PageHandle    OS_ReservedPageAlloc(int canSleep);
extern void          OS_ReservedPageFree(PageHandle handle);

#endif  /* OS_H */
