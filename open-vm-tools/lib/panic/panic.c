/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * panic.c --
 *
 *	Module to encapsulate common Panic behaviors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#ifdef _WIN32
#  include <process.h>
#  include <Windows.h>
#else // Posix
#  include <unistd.h>
#endif // Win32 vs Posix

#include "vmware.h"
#include "productState.h"
#include "vm_version.h"
#include "log.h"
#include "panic.h"
#include "msg.h"
#include "str.h"
#include "config.h"
#include "util.h"
#if defined(_WIN32) || !defined(VMX86_TOOLS)
#include "coreDump.h"
#endif
#ifdef _WIN32
#include "win32u.h"
#endif

typedef enum {
   PanicBreakLevel_Never,
   PanicBreakLevel_IfDebuggerAttached,
   PanicBreakLevel_Always
} PanicBreakLevel;

static struct PanicState {
   Bool msgPostOnPanic;
   Bool coreDumpOnPanic;
   Bool loopOnPanic;
   int coreDumpFlags; /* Memorize for clients without init func */
   PanicBreakLevel breakOnPanic; /* XXX: should this be DEVEL only? */
   char *coreDumpFile;
} panicState = { TRUE, TRUE }; /* defaults in lieu of Panic_Init() */


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_Init --
 *
 *    Inits the panic module.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    Sets panic state.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_Init(void)
{
   panicState.coreDumpOnPanic = Config_GetBool(TRUE, "coreDumpOnPanic");
   panicState.loopOnPanic = Config_GetBool(FALSE, "panic.loopOnPanic");
   panicState.breakOnPanic = Config_GetLong(PanicBreakLevel_Never,
                                                 "panic.breakOnPanic");
   panicState.coreDumpFlags = Config_GetLong(0, "coreDumpFlags");
}


/*
 *----------------------------------------------------------------------
 *
 * Panic_SetPanicMsgPost --
 *
 *	Allow the Msg_Post() on panic to be suppressed.  If passed FALSE,
 *	then any subsequent Panics will refrain from posting the "VMWARE
 *	Panic:" message.
 *
 * Results:
 *	void.
 *
 * Side effects:
 *	Enables/Disables Msg_Post on Panic().
 *
 *----------------------------------------------------------------------
 */

void
Panic_SetPanicMsgPost(Bool postMsg)
{
   panicState.msgPostOnPanic = postMsg;
}


/*
 *----------------------------------------------------------------------
 *
 * Panic_GetPanicMsgPost --
 *	Returns panicState.msgPostOnPanic
 *
 * Results:
 *
 * Side effects:
 * 	None.
 *
 *----------------------------------------------------------------------
 */

Bool
Panic_GetPanicMsgPost(void)
{
   return panicState.msgPostOnPanic;
}


/*
 *----------------------------------------------------------------------
 *
 * Panic_SetCoreDumpOnPanic --
 *
 *      Allow the core dump on panic to be suppressed.  If passed FALSE,
 *      then any subsequent Panics will not attempt to dump core.
 *
 * Results:
 *      void.
 *
 * Side effects:
 *      Enables/Disables core dump on Panic().
 *
 * Bugs:
 *      This really should act like Panic_Loop and Panic_Break, and just
 *      be Panic_CoreDumpOnPanic, and not have to export the state back
 *      out.  That requires this module being the one that knows how to
 *      actually carry out the core dump, which seems like a good thing
 *      anyway.  (Then Panic_Panic can do it too.)
 *
 *----------------------------------------------------------------------
 */

void
Panic_SetCoreDumpOnPanic(Bool dumpCore)
{
   panicState.coreDumpOnPanic = dumpCore;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_GetCoreDumpOnPanic --
 *
 *      Returns whether panic should attempt to dump core.
 *
 * Results:
 *      TRUE if panics should dump core.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Panic_GetCoreDumpOnPanic(void)
{
   return panicState.coreDumpOnPanic;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_GetCoreDumpFlags --
 *
 *      Return the core dump flags.  The reason this module knows about this
 *      is because it's available to everyone who wants to know, and has an
 *      init function which is a convenient time to read configuration info.
 *      Putting the same functionality elsewhere is harder.
 *
 * Results:
 *      flags if set; 0 by default.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
Panic_GetCoreDumpFlags(void)
{
   return panicState.coreDumpFlags;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_SetCoreDumpFlags --
 *
 *      Although the core dump flags are read at init time, we may want to
 *      update this value later. This is especially true because the default
 *      value is read before the VM config is established.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Updates global struct panicState.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_SetCoreDumpFlags(int flags)   // IN
{
   panicState.coreDumpFlags = flags;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_LoopOnPanic --
 *
 *    Loop until debugger intervention, if so configured.
 *
 * Results:
 *    void, eventually.
 *
 * Side effects:
 *    Stall for time.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_LoopOnPanic(void)
{
   if (panicState.loopOnPanic) {
      fprintf(stderr, "Looping pid=%d\n", (int)getpid());
      while (panicState.loopOnPanic) {
         sleep(1);
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_BreakOnPanic --
 *
 *    Attract the attention of a nearby debugger.
 *
 * Results:
 *    void, eventually.
 *
 * Side effects:
 *    DebugBreak.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_BreakOnPanic(void)
{
#ifdef _WIN32
   if (Panic_GetBreakOnPanic()) {
      Warning("Panic: breaking into debugger\n");
      DebugBreak();
   }
#else
   /* XXX: Linux experts: is there any equivalent of attaching a JIT debugger? */
#endif
}


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

void
Panic_SetBreakOnPanic(Bool breakOnPanic)
{
   panicState.breakOnPanic = breakOnPanic ? PanicBreakLevel_Always
                                          : PanicBreakLevel_Never;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_GetBreakOnPanic --
 *
 *    Whether or not we should break into the debugger on the current
 *    panic iteration.
 *
 * Results:
 *    TRUE if a break is in order, FALSE otherwise
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Panic_GetBreakOnPanic(void)
{
   Bool shouldBreak = FALSE;

   switch (panicState.breakOnPanic) {
   case PanicBreakLevel_Never:
      break;
   case PanicBreakLevel_IfDebuggerAttached:
#ifdef _WIN32 
      {
         typedef BOOL (*pfnIsDebuggerPresent)(void);

         HMODULE kernelLibrary = Win32U_LoadLibrary("kernel32.dll");
         if (kernelLibrary != NULL) {
            pfnIsDebuggerPresent IsDebuggerPresentFn = 
               (pfnIsDebuggerPresent) GetProcAddress(kernelLibrary, "IsDebuggerPresent");
            if (IsDebuggerPresentFn != NULL) {
               shouldBreak = IsDebuggerPresentFn();
            }
            FreeLibrary(kernelLibrary);
         }
      }
#else
      /* XXX: Linux experts?  How do you know if you're being ptraced? */
#endif
      break;
   default:
   case PanicBreakLevel_Always:
      shouldBreak = TRUE;
      break;
   }
   return shouldBreak;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_SetCoreDumpFileName --
 *
 *    Record the filename of a core dump file so that a subsequent
 *    Panic_PostPanicMsg can mention it by name.
 *
 *    Pass NULL to say there's no core file; pass the empty string to
 *    say there's a core file but you don't know where; pass the name
 *    of the core file if you know it.
 *
 * Results:
 *    void
 *
 * Side effects:
 *    malloc; overwrites panicState.coreDumpFile
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_SetCoreDumpFileName(const char *fileName)
{
   if (panicState.coreDumpFile) {
      free(panicState.coreDumpFile);
   }

   if (fileName) {
      panicState.coreDumpFile = strdup(fileName);
   } else {
      panicState.coreDumpFile = NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_GetCoreDumpFileName --
 *
 *    Returns the core dump filename if set.
 *
 * Results:
 *    coredump filename if set, NULL otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

const char *
Panic_GetCoreDumpFileName(void)
{
   return panicState.coreDumpFile;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_Panic --
 *
 *      Panic, possibly core dump
 *
 *      A nice default implementation that basic Panic can call if you don't
 *      want to write your own.  The VMX of course has its own.
 *
 *      TODO: Figure out how to trigger the Mac OS X Crash Reporter.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Death.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_Panic(const char *format,
            va_list args)
{
   char buf[1024];
   static int count = 0;

   Str_Vsnprintf(buf, sizeof buf, format, args);

   /*
    * Write the message to stderr first, so there's always
    * some sort of record.
    * Don't try to do anything fancy, since this is before
    * panic loop detection.  In particular, try not to call
    * any of our functions (that may call Panic()).
    */

   fputs(buf, stderr);
#ifdef _WIN32
   Win32U_OutputDebugString(buf);
#endif

   /*
    * Make sure Panic gets logged
    */
   Log_DisableThrottling();

   /*
    * Panic loop detection:
    *	 first time - do the whole report and shutdown sequence
    *	 second time - log and exit
    *	 beyond second time - just exit
    */

   switch (count++) {
   case 0:
      break;
   case 1:
      Log("%s", buf);
      Log("Panic loop\n");
   default:
      fprintf(stderr, "Panic loop\n");
      Util_ExitProcessAbruptly(1);
      NOT_REACHED();
   }

   /*
    * Log panic information, and make sure we don't remove
    * the log file on exit.
    */

   Log("%s", buf);
   Util_Backtrace(0);
   Log_SetAlwaysKeep(TRUE);

   /*
    * Do the debugging steps early before we have a chance
    * to double panic.
    */

   Panic_DumpGuiResources();

#if defined(_WIN32) ||  !defined(VMX86_TOOLS)
   if (Panic_GetCoreDumpOnPanic()) {
      CoreDump_CoreDump();
   }
#endif

   Panic_LoopOnPanic();

   /*
    * Show pretty panic dialog.
    * This is where things can go badly wrong.
    */

   Panic_PostPanicMsg(buf);

   /* 
    * Bye
    */

   exit(-1);
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic_DumpGuiResources --
 *
 *      Dumps userlevel resources used by the current process.
 *
 * Results:
 *      void
 *
 * Side effects:
 *      Logs.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic_DumpGuiResources(void)
{
#ifdef _WIN32
   HANDLE hUser = Win32U_GetModuleHandle("user32.dll");
   if (hUser) {
      typedef DWORD (WINAPI *fnGetGuiResources)(HANDLE, DWORD);
      fnGetGuiResources pGetGuiResources =
         (fnGetGuiResources) GetProcAddress(hUser, "GetGuiResources");
      if (pGetGuiResources) {
         Warning("Win32 object usage: GDI %d, USER %d\n",
                 pGetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS),
                 pGetGuiResources(GetCurrentProcess(), GR_USEROBJECTS));
      }
   }
#endif
}
