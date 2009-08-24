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
 * os.h --
 *
 *      Definitions for OS-specific wrapper functions required by "vmmemctl".
 */

#ifndef	OS_H
#define	OS_H

/*
 * Types
 */

typedef void OSTimerHandler(void *clientData);
typedef int  OSStatusHandler(char *buf, size_t size);

/*
 * Operations
 */

extern void OS_Init(const char *name,
                    const char *nameVerbose,
                    OSStatusHandler *handler);
extern void OS_Cleanup(void);
extern const char *OS_Identity(void);

extern void OS_MemZero(void *ptr, size_t size);
extern void OS_MemCopy(void *dest, const void *src, size_t size);
extern int  OS_Snprintf(char *buf, size_t size, const char *format, ...);

extern void *OS_Malloc(size_t size);
extern void OS_Free(void *ptr, size_t size);

extern void OS_Yield(void);

extern unsigned int OS_TimerHz(void);
extern void OS_TimerInit(OSTimerHandler *handler, void *clientData, int period);
extern void OS_TimerStart(void);
extern void OS_TimerStop(void);

extern unsigned int  OS_PredictMaxReservedPages(void);
extern unsigned long OS_AddrToPPN(unsigned long addr);
extern unsigned long OS_AllocReservedPage(int canSleep);
extern void          OS_FreeReservedPage(unsigned long page);

#endif  /* OS_H */
