/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * util.c --
 *
 *    misc util functions
 */

#undef WIN32_LEAN_AND_MEAN

#if defined(__linux__) && !defined(VMX86_TOOLS)
#define _GNU_SOURCE
#endif

#include "vm_ctype.h"
#include "safetime.h"

#if defined(_WIN32)
# include <winsock2.h> // also includes windows.h
# include <io.h>
# include <process.h>
# include "coreDump.h"
# include "getoptWin32.h"
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <getopt.h>
#  include <pwd.h>
#  include <dlfcn.h>
#endif

#if defined(__linux__) && !defined(VMX86_TOOLS)
#  include <link.h>
#endif

#include "vmware.h"
#include "msg.h"
#include "util.h"
#include "str.h"
/* For HARD_EXPIRE --hpreg */
#include "vm_version.h"
#include "su.h"
#include "posix.h"
#include "file.h"
#include "util_shared.h"
#include "escape.h"
#include "base64.h"
#include "unicode.h"
#include "posix.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * Win32 doesn't have an iovec type, so do this here to avoid messy games in
 * the header files.  --Jeremy.
 */

struct UtilVector {
   void *base;
   int len;
};


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
 *----------------------------------------------------------------------
 *
 * Util_Init --
 *
 *	Opportunity to sanity check things
 *
 * Results:
 *	Bool - TRUE (this should never fail)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Init(void)
{
#ifdef VMX86_DEVEL
   /*
    * Sanity check Str_Snprintf so that we're never thrown off guard
    * by a change in the underlying libraries that Str_Snprintf doesn't
    * catch and wrap properly.
    */
   {
      char buf[2] = { 'x', 'x' };
      int rv;

      rv = Str_Snprintf(buf, sizeof(buf), "a");
      ASSERT(rv == 1);
      ASSERT(!strcmp(buf, "a"));

      rv = Str_Snprintf(buf, sizeof(buf), "ab");
      ASSERT(rv == -1);
      ASSERT(!strcmp(buf, "a"));
   }
#endif
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksum32 --
 *
 * Checksums a uint32 aligned block by progressive XOR.  Basically parity
 * checking of each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksum32(const uint32 *buf, int len)
{
   uint32 checksum = 0;
   int i;

   ASSERT((len % 4) == 0);
   for (i = 0; i < len; i+=4) checksum ^= *(buf++);
   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksum --
 *
 * Checksums a block by progressive XOR.  Basically parity
 * checking of each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksum(const uint8 *buf, int len)
{
   uint32 checksum;
   int remainder, shift;

   remainder = len % 4;
   len -= remainder;

   checksum = Util_Checksum32((uint32 *)buf, len);

   buf += len;
   shift = 0;
   while (remainder--) {
      /*
       * Note: this is little endian.
       */
      checksum ^= (*buf++ << shift);
      shift += 8;
   }

   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksumv --
 *
 * Checksums an iovector by progressive XOR.  Basically parity checking of
 * each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksumv(void *iov,      // IN
               int numEntries) // IN
{
   uint32 checksum = 0;
   struct UtilVector *vector = (struct UtilVector *) iov;
   uint32 partialChecksum;
   int bytesSoFar = 0;
   int rotate;

   while (numEntries-- > 0) {
      partialChecksum = Util_Checksum(vector->base, vector->len);
      rotate = (bytesSoFar & 3) * 8;
      checksum ^= ((partialChecksum << rotate) |
                   (partialChecksum >> (32 - rotate)));
      bytesSoFar += vector->len;
      vector++;
   }

   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_HashString --
 *
 *      Get a hash of the given NUL terminated string using the djb2
 *      hash algorithm.
 *
 * Results:
 *      The hashed value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_HashString(const char *str)  // IN:
{
   uint32 hash = 5381;
   int c;

   while ((c = *str++) != 0) {
      hash = ((hash << 5) + hash) + c;
   }

   return hash;
}


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
Util_BacktraceFromPointer(uintptr_t *basePtr)
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
Util_BacktraceFromPointerWithFunc(uintptr_t *basePtr,
                                  Util_OutputFunc outFunc,
                                  void *outFuncData)
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
#if !defined(_WIN32) && !defined(VMX86_TOOLS)
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

#if !defined(_WIN32) && !defined(VMX86_TOOLS)
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
Util_BacktraceToBuffer(uintptr_t *basePtr,
                       uintptr_t *buffer,
                       int len)
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
 * Util_Data2Buffer --
 *
 *	Format binary data for printing
 *
 * Results:
 *	TRUE if all data fits into buffer, FALSE otherwise.
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Data2Buffer(char *buf,         // OUT
                 size_t bufSize,    // IN
                 const void *data0, // IN
                 size_t dataSize)   // IN
{
   size_t n;

   /* At least 1 byte (for NUL) must be available. */
   if (!bufSize) {
      return FALSE;
   }

   bufSize = bufSize / 3;
   n = MIN(dataSize, bufSize);
   if (n != 0) {
      const uint8 *data = data0;

      while (n > 0) {
         static const char digits[] = "0123456789ABCDEF";

         *buf++ = digits[*data >> 4];
         *buf++ = digits[*data & 0xF];
         *buf++ = ' ';
         data++;
         n--;
      }
      buf--;
   }
   *buf = 0;
   return dataSize <= bufSize;
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
Util_BacktraceWithFunc(int bugNr, Util_OutputFunc outFunc, void *outFuncData)
{
#if !defined(_WIN32)
   uintptr_t *x = (uintptr_t *) &bugNr;
#endif

   if (bugNr == 0) {
      outFunc(outFuncData, "Backtrace:\n");
   } else {
      outFunc(outFuncData, "Backtrace for bugNr=%d\n",bugNr);
   }
#if defined(_WIN32)
   CoreDump_Backtrace(outFunc, outFuncData);
#else
   Util_BacktraceFromPointerWithFunc(&x[-2], outFunc, outFuncData);
#endif
}


/* XXX This should go in a separate utilPosix.c file --hpreg */
#if !defined(_WIN32)
/*
 *----------------------------------------------------------------------
 *
 * Util_MakeSureDirExists --
 *
 *    Make sure a directory exists
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_MakeSureDirExistsAndAccessible(char const *path,  // IN
				    unsigned int mode) // IN
{
   char *epath;
   struct stat statbuf;

   epath = Util_ExpandString(path);
   if (epath == NULL) {
      return FALSE;
   }

   if (Posix_Stat(epath, &statbuf) == 0) {
      if (! S_ISDIR(statbuf.st_mode)) {
	 Msg_Append(MSGID(util.msde.notDir)
		    "The path \"%s\" exists, but it is not a directory.\n",
		    epath);
	 free(epath);
	 return FALSE;
      }
   } else {
      if (Posix_Mkdir(epath, mode) != 0) {
	 Msg_Append(MSGID(util.msde.mkdir)
		    "Cannot create directory \"%s\": %s.\n",
		    epath, Msg_ErrString());
	 free(epath);
	 return FALSE;
      }
   }
   if (FileIO_Access(epath, FILEIO_ACCESS_READ | FILEIO_ACCESS_WRITE | FILEIO_ACCESS_EXEC | FILEIO_ACCESS_EXISTS) ==
       FILEIO_ERROR) {
      Msg_Append(MSGID(util.msde.noAccess)
		 "Directory \"%s\" is not accessible: %s.\n",
		 epath, Msg_ErrString());
      free(epath);
      return FALSE;
   }
   free(epath);

   return TRUE;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Util_ExitProcessAbruptly
 *
 *    On Win32, terminate the process and all of its threads, without
 *    calling any of the DLL termination handlers.

 *    On Linux, call _exit().
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
Util_ExitProcessAbruptly(int code) // IN
{
#if defined(_WIN32)
   TerminateProcess(GetCurrentProcess(), code);
#else
   _exit(code);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Util_ExitThread --
 *
 *    Terminate the running thread.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
Util_ExitThread(int code) // IN
{
#if defined(_WIN32)
   ExitThread(code);
#else
   exit(code);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Util_CompareDotted --
 *
 *      Compares two version numbers encoded as dotted strings.
 *
 * Results:
 *      0 if equal, -1 if s1 is less than s2, else 1.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Util_CompareDotted(const char *s1, const char *s2)
{
   int i, x[5], y[5];

   for (i = 0; i < 5; i++) {
      x[i] = 0;
      y[i] = 0;
   }

   if (sscanf(s1, "%d.%d.%d.%d.%d", &x[0], &x[1], &x[2], &x[3], &x[4]) < 1) {
      x[0] = 1;
   }
   if (sscanf(s2, "%d.%d.%d.%d.%d", &y[0], &y[1], &y[2], &y[3], &y[4]) < 1) {
      y[0] = 1;
   }

   for (i = 0; i < 5; i++) {
      if (x[i] < y[i]) {
         return -1;
      }
      if (x[i] > y[i]) {
         return 1;
      }
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetOpt --
 *
 *      A wrapper around getopt_long that avoids needing separate long and
 *      short option lists.
 *
 *      To use this, the array of option structs must:
 *      * Store the short option name in the 'val' member.
 *      * Set the 'name' member to NULL if the option has a short name but no
 *        long name.
 *      * For options that have only a long name, 'val' should be set to a
 *        unique value greater than UCHAR_MAX.
 *      * Terminate the array with a sentinel value that zero-initializes both
 *        'name' and 'val'.
 *
 * Results:
 *      See getopt_long.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
Util_GetOpt(int argc,                  // IN
            char * const *argv,        // IN
            const struct option *opts, // IN
            Util_NonOptMode mode)      // IN
{
   int ret = -1;

   struct option *longOpts = NULL;
   char *shortOptString = NULL;

   /*
    * In the worst case, each character needs "::" to indicate that it takes
    * an optional argument.
    */
   const size_t maxCharsPerShortOption = 3;
   const size_t modePrefixSize = 1;

   size_t n = 0;
   size_t shortOptStringSize;

   while (!(opts[n].name == NULL && opts[n].val == 0)) {
      if (UNLIKELY(n == SIZE_MAX)) {
         /*
          * Avoid integer overflow.  If you have this many options, you're
          * doing something wrong.
          */
         ASSERT(FALSE);
         goto exit;
      }
      n++;
   }

   if (UNLIKELY(n > SIZE_MAX / sizeof *longOpts - 1)) {
      /* Avoid integer overflow. */
      ASSERT(FALSE);
      goto exit;
   }
   longOpts = malloc((n + 1) * sizeof *longOpts);
   if (longOpts == NULL) {
      goto exit;
   }

   if (UNLIKELY(n > (SIZE_MAX - modePrefixSize - 1 /* NUL */) /
                maxCharsPerShortOption)) {
      /* Avoid integer overflow. */
      ASSERT(FALSE);
      goto exit;
   }
   shortOptStringSize = n * maxCharsPerShortOption + modePrefixSize + 1 /* NUL */;
   shortOptString = malloc(shortOptStringSize);
   if (shortOptString == NULL) {
      goto exit;
   } else {
      struct option empty = { 0 };

      size_t i;
      struct option *longOptOut = longOpts;
      char *shortOptOut = shortOptString;

      switch (mode) {
         case UTIL_NONOPT_STOP:
            *shortOptOut++ = '+';
            break;
         case UTIL_NONOPT_ALL:
            *shortOptOut++ = '-';
            break;
         default:
            break;
      }

      for (i = 0; i < n; i++) {
         int val = opts[i].val;

         if (opts[i].name != NULL) {
            *longOptOut++ = opts[i];
         }

         if (val > 0 && val <= UCHAR_MAX) {
            int argSpec = opts[i].has_arg;

            *shortOptOut++ = (char) val;

            if (argSpec != no_argument) {
               *shortOptOut++ = ':';

               if (argSpec == optional_argument) {
                  *shortOptOut++ = ':';
               }
            }
         }
      }

      ASSERT(longOptOut - longOpts <= n);
      *longOptOut = empty;

      ASSERT(shortOptOut - shortOptString < shortOptStringSize);
      *shortOptOut = '\0';
   }

   ret = getopt_long(argc, argv, shortOptString, longOpts, NULL);

exit:
   free(longOpts);
   free(shortOptString);
   return ret;
}




/*
 *-----------------------------------------------------------------------------
 *
 * Util_HasAdminPriv --
 *
 *    Determine if the calling code has administrator privileges --hpreg
 *
 * Results:
 *    1 if yes
 *    0 if no
 *    <0 on error
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
Util_HasAdminPriv(void)
{
#if defined(_WIN32)
   HANDLE token = INVALID_HANDLE_VALUE;
   int ret = -1;

   /*
    * Retrieve the access token of the calling thread --hpreg
    *
    * On some machines OpenThreadToken with openAsSelf set to FALSE fails.
    * Empirically, it seems that, in the security context of another user
    * (even when the impersonation token is at SecurityImpersonation level)
    * it is not able to obtain the thread token with TOKEN_DUPLICATE access.
    * Calling OpenThreadToken to open as self is more reliable and does not
    * seem to hurt. -- vui
    */

   if (OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE, TRUE,
          &token) == 0) {
      if (GetLastError() != ERROR_NO_TOKEN) {
         ret = -1;
         goto end;
      }

      if (OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE,
             &token) == 0) {
         ret = -2;
         goto end;
      }
   }

   ret = Util_TokenHasAdminPriv(token);

end:

   if (token != INVALID_HANDLE_VALUE) {
      if (CloseHandle(token) == 0 && ret >= 0) {
         ret = -13;
      }
   }

   return ret;
#else
   return Id_IsSuperUser() ? 1 : 0;
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * Util_DeriveFileName --
 *
 *      This function is admittedly weird.  The basic idea is that we
 *      have a path to a dictionary file, and we need to make a path to
 *      a another file that's named in a similar way to that dictionary
 *      file (e.g., only difference is extension, or filename and
 *      extension).
 *
 *      This function returns a pointer to the result
 *
 * Results:
 *      Pointer to string (on success, caller should free string),
 *      otherwise NULL.
 *
 * Side effects:
 *      Allocates memory to be freed by caller.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_DeriveFileName(const char *source, // IN: path to dict file (incl filename)
                    const char *name,   // IN: what to replace filename with (optional)
                    const char *ext)    // IN: what to replace extension with (optional)
{
   char *returnResult = NULL;
   char *path = NULL;
   char *base = NULL;

   if (source == NULL || (name == NULL && ext == NULL)) {
      Warning("invalid use of function\n");
      return NULL;
   }
   File_GetPathName(source, &path, &base);

   /* If replacing name and extension */
   if (name != NULL) {
      free(base);

      /*
       * If the "name" we have to append is a relative path (i.e., not an
       * absolute path), then we need to concatenate the "name" to the
       * path of "source". If the path of "source" doesn't exist or is
       * just ".", then we don't need to bother with concatenating results
       * together.
       */

      if (!Util_IsAbsolutePath(name) && strlen(path) > 0 &&
          strcmp(path, ".") != 0) {
	 if (ext == NULL) {
	    returnResult = Str_Asprintf(NULL, "%s%s%s",
					path, DIRSEPS, name);
	 } else {
	    returnResult = Str_Asprintf(NULL, "%s%s%s.%s",
				        path, DIRSEPS, name, ext);
	 }
      } else {

	 /*
          * Path is non-existent or is just the current directory (or the
	  * result from the dictionary is an absolute path), so we
	  * just need to use the filename (using the DIRSEPS method above
          * for a non-existent path might result in something undesireable
	  * like "\foobar.vmdk")
	  */

	 if (ext == NULL) {
	    returnResult = Str_Asprintf(NULL, "%s", name);
	 } else {
	    returnResult = Str_Asprintf(NULL, "%s.%s", name, ext);
	 }
      }
      free(path);
      return returnResult;
   }

   /* replacing only the file extension */

   /* strip off the existing file extension, if present */
   {
      char *p = Str_Strrchr(base, '.');
      if (p != NULL) {
	 *p = '\0';
      }
   }

   /* Combine disk path with parent path */
   if (strlen(path) > 0 && strcmp(path, ".") != 0) {
      returnResult = Str_Asprintf(NULL, "%s%s%s.%s",
				  path, DIRSEPS, base, ext);
   } else {

      /*
       * Path is non-existent or is just the current directory, so we
       * just need to use the filename (using the DIRSEPS method might
       * result in something undesireable like "\foobar.vmdk")
       */
      returnResult = Str_Asprintf(NULL, "%s.%s", base, ext);
   }
   free(path);
   free(base);
   return returnResult;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CombineStrings --
 *
 *      Takes a vector of strings, and combines them into one string,
 *      where each string is separated by a 0 (zero) byte.
 *
 *      The 0 bytes are then escaped out, and the result is returned.
 *
 * Results:
 *
 *      A NULL terminated string
 *
 * Side effects:
 *
 *      The result string is allocated
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_CombineStrings(char **sources,             // IN
                    int count)                  // IN
{
   size_t size = 0;
   int index = 0;

   char *combinedString = NULL;
   char *cursor = NULL;
   char *escapedString = NULL;

   int bytesToEsc[256];

   ASSERT(sources != NULL);

   memset(bytesToEsc, 0, sizeof bytesToEsc);
   bytesToEsc[0] = 1;
   bytesToEsc['#'] = 1;

   for (index = 0; index < count; index++) {
      /*
       * Count the size of each string + the delimeter
       */

      size += strlen(sources[index]) + 1;
   }

   combinedString = Util_SafeMalloc(size);

   cursor = combinedString;
   for (index = 0; index < count; index++) {
      memcpy(cursor, sources[index], strlen(sources[index]));
      cursor += strlen(sources[index]);
      cursor[0] = '\0';
      cursor++;
   }

   escapedString = Escape_Do('#', bytesToEsc, combinedString, size, NULL);

   free(combinedString);

   return escapedString;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_SeparateStrings --
 *
 *      Takes as input the result of a call to Util_CombineStrings, and
 *      separates the strings back onto a vector of strings.
 *
 * Results:
 *
 *      A vector of strings, the count will also reflect the number of
 *      entries in the vector
 *
 * Side effects:
 *
 *      The vector is allocated, and each string in the vector must be
 *      freed by the caller
 *
 *-----------------------------------------------------------------------------
 */

char **
Util_SeparateStrings(char *source,              // IN
                     int *count)                // OUT
{
   char *data = NULL;
   size_t dataSize = 0;

   int index = 0;
   char *cursor = NULL;
   char *endCursor = NULL;

   char **stringVector = NULL;

   ASSERT(count != NULL);

   *count = 0;

   data = Escape_Undo('#', source, strlen(source), &dataSize);
   ASSERT(data != NULL);

   endCursor = data + dataSize;
   ASSERT(endCursor[0] == '\0');

   cursor = data;
   while (cursor < endCursor) {
      (*count)++;
      cursor += strlen(cursor) + 1;
   }

   stringVector = Util_SafeMalloc(sizeof(char *) * (*count));

   cursor = data;
   for (index = 0; index < (*count); index++) {
      stringVector[index] = Util_SafeStrdup(cursor);
      cursor += strlen(cursor) + 1;
   }

   free(data);

   return stringVector;
}
