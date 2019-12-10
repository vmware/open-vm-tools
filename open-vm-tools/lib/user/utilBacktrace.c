/*********************************************************
 * Copyright (C) 2013-2019 VMware, Inc. All rights reserved.
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
 * utilBacktrace.c --
 *
 *    misc util functions
 */

#undef WIN32_LEAN_AND_MEAN

#if defined(__linux__) && !defined(VMX86_TOOLS)
#define _GNU_SOURCE
#endif

#include "vm_ctype.h"

#if defined(_WIN32)
# include <winsock2.h> // also includes windows.h
# include <io.h>
# include <process.h>
# include "coreDump.h"
#endif

#if !defined(_WIN32)
#  include <unistd.h>
#  include <pwd.h>
#  include <dlfcn.h>
#endif

#if defined(__linux__) && !defined(VMX86_TOOLS) && !defined(__ANDROID__)
#  include <link.h>
#endif

#include "vmware.h"
#include "util.h"
#include "str.h"

#ifdef VM_X86_64
#   if defined(__GNUC__) && (!defined(USING_AUTOCONF) || defined(HAVE_UNWIND_H))
#      define UTIL_BACKTRACE_USE_UNWIND
#   endif
#endif

#ifdef UTIL_BACKTRACE_USE_UNWIND
#include <unwind.h>

#define MAX_SKIPPED_FRAMES 10

struct UtilBacktraceFromPointerData {
   uintptr_t        basePtr;
   Util_OutputFunc  outFunc;
   void            *outFuncData;
   unsigned int     frameNr;
   unsigned int     skippedFrames;
};

struct UtilBacktraceToBufferData {
   uintptr_t        basePtr;
   uintptr_t       *buffer;
   size_t           len;
};
#endif /* UTIL_BACKTRACE_USE_UNWIND */


/*
 *-----------------------------------------------------------------------------
 *
 * UtilLogWrapper --
 *
 *      Adapts the Log function to meet the interface required by backtracing
 *      functions by adding an ignored void* argument.
 *
 *      NOTE: This function needs to be static on linux (and any other
 *      platform appLoader might be ported to).  See bug 403780.
 *
 * Results:
 *      Same effect as Log(fmt, ...)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
UtilLogWrapper(void *ignored,    // IN:
               const char *fmt,  // IN:
               ...)              // IN:
{
   uint32 len;
   va_list ap;
   char thisLine[UTIL_BACKTRACE_LINE_LEN];

   va_start(ap, fmt);
   len = Str_Vsnprintf(thisLine, UTIL_BACKTRACE_LINE_LEN - 2, fmt, ap);
   va_end(ap);

   if (len >= UTIL_BACKTRACE_LINE_LEN - 2) {
      len = UTIL_BACKTRACE_LINE_LEN - 3;
   }

   if (thisLine[len - 1] != '\n') {
      thisLine[len] = '\n';
      thisLine[len + 1] = '\0';
   }

   Log("%s", thisLine);
}


#ifdef UTIL_BACKTRACE_USE_UNWIND
/*
 *-----------------------------------------------------------------------------
 *
 * UtilBacktraceToBufferCallback --
 *
 *      Callback from _Unwind_Backtrace to add one entry to the backtrace
 *      buffer.
 *
 * Results:
 *      _URC_NO_REASON : Please continue with backtrace.
 *      _URC_END_OF_STACK : Abort backtrace, we run out of space (*).
 *
 *          (*) Caller does not care.  Anything else than NO_REASON is fatal
 *              and forces _Unwind_Backtrace to report PHASE1 error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static _Unwind_Reason_Code
UtilBacktraceToBufferCallback(struct _Unwind_Context *ctx, // IN: Unwind context
                              void *cbData)                // IN/OUT: Our data
{
   struct UtilBacktraceToBufferData *data = cbData;
   uintptr_t cfa = _Unwind_GetCFA(ctx);

   /*
    * Stack grows down.  So if we are below basePtr, do nothing...
    */
   if (cfa >= data->basePtr) {
      if (data->len) {
         *data->buffer++ = _Unwind_GetIP(ctx);
         data->len--;
      } else {
         return _URC_END_OF_STACK;
      }
   }
   return _URC_NO_REASON;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilBacktraceFromPointerCallback --
 *
 *      Callback from _Unwind_Backtrace to print one backtrace entry
 *      to the backtrace output.
 *
 * Results:
 *      _URC_NO_REASON : Please continue with backtrace.
 *      _URC_END_OF_STACK : Abort backtrace.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static _Unwind_Reason_Code
UtilBacktraceFromPointerCallback(struct _Unwind_Context *ctx, // IN: Unwind context
                                 void *cbData)                // IN/OUT: Our status
{
   struct UtilBacktraceFromPointerData *data = cbData;
   uintptr_t cfa = _Unwind_GetCFA(ctx);

   /*
    * Stack grows down.  So if we are below basePtr, do nothing...
    */

   if (cfa >= data->basePtr && data->frameNr < 500) {
#ifndef VM_X86_64
#   error You should not build this on 32bit - there is no eh_frame there.
#endif
      /* bump basePtr for glibc unwind bug, see [302237] */
      data->basePtr = cfa + 8;
      /* Do output without leading '0x' to save some horizontal space... */
      data->outFunc(data->outFuncData,
                    "Backtrace[%u] %016lx rip=%016lx rbx=%016lx rbp=%016lx "
                    "r12=%016lx r13=%016lx r14=%016lx r15=%016lx\n",
                    data->frameNr, cfa, _Unwind_GetIP(ctx),
                    _Unwind_GetGR(ctx, 3), _Unwind_GetGR(ctx, 6),
                    _Unwind_GetGR(ctx, 12), _Unwind_GetGR(ctx, 13),
                    _Unwind_GetGR(ctx, 14), _Unwind_GetGR(ctx, 15));
      data->frameNr++;
      return _URC_NO_REASON;
   } else if (data->skippedFrames < MAX_SKIPPED_FRAMES && !data->frameNr) {
      /*
       * Skip over the frames before the specified starting point of the
       * backtrace.
       */

      data->skippedFrames++;
      return _URC_NO_REASON;
   }
   return _URC_END_OF_STACK;
}


#if !defined(_WIN32) && !defined(VMX86_TOOLS)
/*
 *-----------------------------------------------------------------------------
 *
 * UtilSymbolBacktraceFromPointerCallback --
 *
 *      Callback from _Unwind_Backtrace to print one backtrace entry
 *      to the backtrace output.  This version includes symbol information,
 *      if available.
 *
 * Results:
 *      _URC_NO_REASON : Please continue with backtrace.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static _Unwind_Reason_Code
UtilSymbolBacktraceFromPointerCallback(struct _Unwind_Context *ctx, // IN: Unwind context
                                       void *cbData)                // IN/OUT: Our status
{
   struct UtilBacktraceFromPointerData *data = cbData;
   uintptr_t cfa = _Unwind_GetCFA(ctx);

   /*
    * Stack grows down.  So if we are below basePtr, do nothing...
    */

   if (cfa >= data->basePtr && data->frameNr < 500) {
#ifndef VM_X86_64
#   error You should not build this on 32bit - there is no eh_frame there.
#endif
      void *enclFuncAddr;
      Dl_info dli;

      /* bump basePtr for glibc unwind bug, see [302237] */
      data->basePtr = cfa + 8;
#ifdef __linux__
      enclFuncAddr = _Unwind_FindEnclosingFunction((void *)_Unwind_GetIP(ctx));
#else
      enclFuncAddr = NULL;
#endif
      if (dladdr(enclFuncAddr, &dli) ||
          dladdr((void *)_Unwind_GetIP(ctx), &dli)) {
         data->outFunc(data->outFuncData,
                      "SymBacktrace[%u] %016lx rip=%016lx in function %s "
                      "in object %s loaded at %016lx\n",
                      data->frameNr, cfa, _Unwind_GetIP(ctx),
                      dli.dli_sname, dli.dli_fname, dli.dli_fbase);
      } else {
         data->outFunc(data->outFuncData,
                      "SymBacktrace[%u] %016lx rip=%016lx \n",
                      data->frameNr, cfa, _Unwind_GetIP(ctx));
      }
      data->frameNr++;
      return _URC_NO_REASON;
   } else if (data->skippedFrames < MAX_SKIPPED_FRAMES && !data->frameNr) {
      /*
       * Skip over the frames before the specified starting point of the
       * backtrace.
       */

      data->skippedFrames++;
      return _URC_NO_REASON;
   }
   return _URC_END_OF_STACK;
}
#endif
#endif


/*
 *----------------------------------------------------------------------
 *
 * Util_BacktraceFromPointer --
 *
 *      log the stack backtrace given a frame pointer
 *
 * Results:
 *
 *      void
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Util_BacktraceFromPointer(uintptr_t *basePtr)  // IN:
{
   Util_BacktraceFromPointerWithFunc(basePtr, UtilLogWrapper, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_BacktraceFromPointerWithFunc --
 *
 *      Output a backtrace from the given frame porinter, using
 *      "outputFunc" as the logging function. For each line of the backtrace,
 *      this will call "outputFunc(outputFuncData, fmt, ...)"
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls outFunc repeatedly.
 *
 *-----------------------------------------------------------------------------
 */

void
Util_BacktraceFromPointerWithFunc(uintptr_t *basePtr,       // IN:
                                  Util_OutputFunc outFunc,  // IN:
                                  void *outFuncData)        // IN:
{
#if defined(UTIL_BACKTRACE_USE_UNWIND)
   struct UtilBacktraceFromPointerData data;

   data.basePtr = (uintptr_t)basePtr;
   data.outFunc = outFunc;
   data.outFuncData = outFuncData;
   data.frameNr = 0;
   data.skippedFrames = 0;
   _Unwind_Backtrace(UtilBacktraceFromPointerCallback, &data);

#if !defined(_WIN32) && !defined(VMX86_TOOLS)
   /*
    * We do a separate pass here that includes symbols in order to
    * make sure the base backtrace that does not call dladdr() etc.
    * is safely produced.
    */
   data.basePtr = (uintptr_t)basePtr;
   data.outFunc = outFunc;
   data.outFuncData = outFuncData;
   data.frameNr = 0;
   data.skippedFrames = 0;
   _Unwind_Backtrace(UtilSymbolBacktraceFromPointerCallback, &data);
#endif

#elif !defined(VM_X86_64)
   uintptr_t *x = basePtr;
   int i;
#if !defined(_WIN32) && !defined(VMX86_TOOLS) && !defined(__ANDROID__)
   Dl_info dli;
#endif

   for (i = 0; i < 256; i++) {
      if (x < basePtr ||
	  (uintptr_t) x - (uintptr_t) basePtr > 0x8000) {
         break;
      }
      outFunc(outFuncData, "Backtrace[%d] %#08x eip %#08x \n", i, x[0], x[1]);
      x = (uintptr_t *) x[0];
   }

#if !defined(_WIN32) && !defined(VMX86_TOOLS) && !defined(__ANDROID__)
   /*
    * We do a separate pass here that includes symbols in order to
    * make sure the base backtrace that does not call dladdr() etc.
    * is safely produced.
    */
   x = basePtr;
   for (i = 0; i < 256; i++) {
      if (x < basePtr ||
	  (uintptr_t) x - (uintptr_t) basePtr > 0x8000) {
         break;
      }
      if (dladdr((uintptr_t *)x[1], &dli)) {
         outFunc(outFuncData, "SymBacktrace[%d] %#08x eip %#08x in function %s "
                              "in object %s loaded at %#08x\n",
                               i, x[0], x[1], dli.dli_sname, dli.dli_fname,
                                dli.dli_fbase);
      } else {
         outFunc(outFuncData, "SymBacktrace[%d] %#08x eip %#08x \n", i, x[0], x[1]);
      }
      x = (uintptr_t *) x[0];
   }
#endif
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_BacktraceToBuffer --
 *
 *      Output a backtrace from the given frame pointer to supplied buffer.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

void
Util_BacktraceToBuffer(uintptr_t *basePtr,  // IN:
                       uintptr_t *buffer,   // IN:
                       int len)             // IN:
{
#if defined(UTIL_BACKTRACE_USE_UNWIND)
   struct UtilBacktraceToBufferData data;

   data.basePtr = (uintptr_t)basePtr;
   data.buffer = buffer;
   data.len = len;
   _Unwind_Backtrace(UtilBacktraceToBufferCallback, &data);
#elif !defined(VM_X86_64)
   uintptr_t *x = basePtr;
   int i;

   for (i = 0; i < 256 && i < len; i++) {
      if (x < basePtr ||
	  (uintptr_t) x - (uintptr_t) basePtr > 0x8000) {
         break;
      }
      buffer[i] = x[1];
      x = (uintptr_t *) x[0];
   }
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Backtrace --
 *
 *      log the stack backtrace for a particular bug number
 *
 * Results:
 *
 *      void
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
Util_Backtrace(int bugNr) // IN
{
   Util_BacktraceWithFunc(bugNr, UtilLogWrapper, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_BacktraceWithFunc --
 *
 *      Outputs the stack backtrace for the bug "bugNr," using the
 *      log function "outFunc" as described in the comment for
 *      Util_BacktraceFromPointerWithFunc.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Calls outFunc several times.
 *
 *-----------------------------------------------------------------------------
 */

void
Util_BacktraceWithFunc(int bugNr,                // IN:
                       Util_OutputFunc outFunc,  // IN:
                       void *outFuncData)        // IN:
{
#if defined(_WIN32)
   CoreDumpFullBacktraceOptions options = {0};

   options.bugNumber = bugNr;
   CoreDump_LogFullBacktraceToFunc(&options, outFunc, outFuncData);
#else
   uintptr_t *x = (uintptr_t *) &bugNr;

   if (bugNr == 0) {
      outFunc(outFuncData, "Backtrace:\n");
   } else {
      outFunc(outFuncData, "Backtrace for bugNr=%d\n",bugNr);
   }
   Util_BacktraceFromPointerWithFunc(&x[-2], outFunc, outFuncData);
#endif
}
