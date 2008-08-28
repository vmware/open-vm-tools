/*********************************************************
 * Copyright (C) 2003 VMware, Inc. All rights reserved.
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

/*
 * panic.h --
 *
 *	Module to encapsulate common Panic behavior.
 */

#ifndef _PANIC_H_
#define _PANIC_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

/*
 * Initialize module to read custom behavior from config files.
 */

EXTERN void Panic_Init(void);

/*
 * If you want the Panic module to just handle everything: implement Panic()
 * as a call to Panic_Panic().  If you want to implement your own Panic(),
 * you can still use functions below to query whether certain features are
 * desired and get the default implementation of each feature.
 */

EXTERN NORETURN void Panic_Panic(const char *format, va_list args);

/*
 * On panic, post a UI dialog about the panic and how to diagnose it:
 */

EXTERN void Panic_SetPanicMsgPost(Bool postMsg);
EXTERN Bool Panic_GetPanicMsgPost(void);
EXTERN void Panic_PostPanicMsg(const char *format, ...);

/*
 * On panic, break into a debugger or enter an infinite loop until a
 * debugger stops it:
 */

EXTERN void Panic_SetBreakOnPanic(Bool breakOnPanic);
EXTERN Bool Panic_GetBreakOnPanic(void);
EXTERN void Panic_BreakOnPanic(void);
EXTERN void Panic_LoopOnPanic(void);

/*
 * On panic, dump core; Panic is also the place where various pieces of
 * back end stash information about the core dump.
 */

EXTERN void Panic_SetCoreDumpOnPanic(Bool dumpCore);
EXTERN Bool Panic_GetCoreDumpOnPanic(void);
EXTERN void Panic_SetCoreDumpFileName(const char *fileName);
EXTERN const char *Panic_GetCoreDumpFileName(void);
EXTERN int  Panic_GetCoreDumpFlags(void);
EXTERN void Panic_SetCoreDumpFlags(int flags);

/*
 * Extra debugging information that Panic module knows how to dump.
 */
EXTERN void Panic_DumpGuiResources(void);


#endif //  _PANIC_H_
