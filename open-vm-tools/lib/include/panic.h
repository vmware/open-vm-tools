/*********************************************************
 * Copyright (C) 2003-2022 VMware, Inc. All rights reserved.
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

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Initialize module to read custom behavior from config files.
 */

void Panic_Init(void);

/*
 * If you want the Panic module to just handle everything: implement Panic()
 * as a call to Panic_Panic().  If you want to implement your own Panic(),
 * you can still use functions below to query whether certain features are
 * desired and get the default implementation of each feature.
 */

NORETURN void Panic_Panic(const char *format, va_list args);

/*
 * On panic, post a UI dialog about the panic and how to diagnose it:
 */

void Panic_SetPanicMsgPost(Bool postMsg);
Bool Panic_GetPanicMsgPost(void);
void Panic_PostPanicMsg(const char *format, ...);

/*
 * On panic, break into a debugger or enter an infinite loop until a
 * debugger stops it:
 */

typedef enum {
   PanicBreakAction_Never,
   PanicBreakAction_IfDebuggerAttached,
   PanicBreakAction_Always
} PanicBreakAction;

void Panic_SetBreakAction(PanicBreakAction action);
PanicBreakAction Panic_GetBreakAction(void);
Bool Panic_GetBreakOnPanic(void);
void Panic_BreakOnPanic(void);
void Panic_LoopOnPanic(void);


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_SetBreakOnPanic --
 *
 *      Allow the debug breakpoint on panic to be suppressed.  If passed FALSE,
 *      then any subsequent Panics will not attempt to attract a debugger.
 *
 * Results:
 *      void.
 *
 * Side effects:
 *      Enables/Disables break into debugger on Panic().
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Panic_SetBreakOnPanic(Bool breakOnPanic)  // IN:
{
   Panic_SetBreakAction(breakOnPanic ? PanicBreakAction_Always
                                     : PanicBreakAction_Never);
}


/*
 * On panic, dump core; Panic is also the place where various pieces of
 * back end stash information about the core dump.
 */

void Panic_SetCoreDumpOnPanic(Bool dumpCore);
Bool Panic_GetCoreDumpOnPanic(void);
int  Panic_GetCoreDumpFlags(void);
void Panic_SetCoreDumpFlags(int flags);

/*
 * Extra debugging information that Panic module knows how to dump.
 */
void Panic_DumpGuiResources(void);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif //  _PANIC_H_
