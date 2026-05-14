/*********************************************************
 * Copyright (c) 1998-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * vmstdio.c --
 *
 *    Functions that operate on FILE objects --hpreg
 */


#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "vmware.h"
#include "dynbuf.h"
#include "vmstdio.h"
#include "posixInt.h"


#ifdef _WIN32
   #include <io.h>
   #include <fcntl.h>
   #include <wchar.h>

   typedef utf16_t tchar;

   #define T(x) L ## x

   #define fgetts fgetws
   #define tcslen wcslen

   #define AllocUTF8(utf16Str) Unicode_AllocWithUTF16(utf16Str)

   #define fileno(stream) _fileno(stream)
   #define isatty(fd) _isatty(fd)
#else
   #include <signal.h>
   #include <termios.h>
   #include <unistd.h>

   typedef char tchar;

   #define T(x) x

   #define fgetts fgets
   #define tcslen strlen

   #define AllocUTF8(utf8Str) Unicode_Alloc(utf8Str, STRING_ENCODING_DEFAULT)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * SuperFgets --
 *
 *      The fgets() API is poorly designed: it doesn't return how many bytes
 *      were written to the buffer. Hence this function --hpreg
 *
 *      In addition, this function recognizes three variants of end-of-line
 *      markers regardless of the platform: \n, \r\n and \r.  The exception
 *      is that on Windows, \r\n is recognized only if the file is opened
 *      in text mode.  In binary mode, the \r and \n are interpreted as two
 *      separate end-of-line markers.
 *
 *      As input, *count is the size of bufIn.
 *
 * Results:
 *      It returns bufIn on success and NULL on error.  The line terminator
 *      and NUL terminator are not stored in bufIn.  On success, '*count' is
 *      the number of bytes written to the buffer.
 *
 * Side effects:
 *      If the line read is terminated by a standalone '\r' (legacy Mac), the
 *      next character is pushed back using ungetc.  Thus, that character may
 *      be lost if the caller performs any write operation on the stream
 *      (including fseek, fflush, etc.) subsequently without an intervening
 *      read operation.
 *
 *-----------------------------------------------------------------------------
 */

static void *
SuperFgets(FILE *stream,   // IN:
           size_t *count,  // IN/OUT:
           void *bufIn)    // OUT:
{
   char *buf = bufIn;
   size_t size = 0;

   ASSERT(stream);
   ASSERT(buf);
   ASSERT(count);
   ASSERT(*count);

   /*
    * Keep reading until a line terminator is found or *count is reached.
    * The line terminator itself is not written into the buffer.
    * Clear errno so that we can distinguish between end-of-file and error.
    */

   errno = 0;

   for (size = 0; size < *count; size++) {
      int c;

      c = getc(stream);

      if (c == EOF) {
         if (errno) {
            /* getc returned an error. */
            return NULL;
         } else {
            /* Found an end-of-file line terminator. */
            break;
         }
      }

      if (c == '\n') {
         /* Found a Unix line terminator */
         break;
      }

      if (c == '\r') {

#ifndef _WIN32
         /*
          * Look ahead to see if it is a \r\n line terminator.
          * On Windows platform, getc() returns one '\n' for a \r\n two-byte
          * sequence (for files opened in text mode), so we can skip the
          * look-ahead.
          */

         c = getc(stream);
         if (c != EOF && c != '\n') {
            /* Found a legacy Mac line terminator */

            if (ungetc(c, stream) == EOF) {
               return NULL;
            }
         }

         /* Forget that we peeked. */
         clearerr(stream);
#endif

         break;
      }

      buf[size] = c;
   }

   *count = size;

   return buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StdIO_ReadNextLine --
 *
 *      Read the next line from a stream.
 *
 *      A line is defined as an arbitrarily long sequence of arbitrary bytes,
 *      that ends with the first occurrence of one of these line terminators:
 *      . \r\n          (the Windows/DOS way, in text mode)
 *      . \n            (the UNIX way)
 *      . \r            (the legacy Mac (pre-OS X) way)
 *      . end-of-stream
 *
 *      Note that on Windows, getc() returns one '\n' for a \r\n two-byte
 *      sequence only if the file is opened in text mode.  Therefore \r\r\n
 *      will be interpreted as two newlines in text mode ('\r' followed by
 *      '\r\n'), but three newlines in binary mode ('\r', '\r', '\n').
 *
 *      If maxBufLength is non-zero at most maxBufLength bytes will be
 *      allocated.
 *
 * Results:
 *      StdIO_Success on success:
 *          '*buf' is an allocated, NUL terminated buffer that contains the
 *          line (excluding the line terminator).  If not NULL, '*count'
 *          contains the size of the buffer (excluding the NUL terminator).
 *
 *      StdIO_EOF if there is no next line (end of stream):
 *          '*buf' is left untouched.
 *
 *      StdIO_Error on failure:
 *          errno is set accordingly and '*buf' is left untouched.
 *
 * Side effects:
 *      If the line read is terminated by a standalone '\r' (legacy Mac), the
 *      next character is pushed back using ungetc.  Thus, that character may
 *      be lost if the caller performs any write operation on the stream
 *      (including fseek, fflush, etc.) subsequently without an intervening
 *      read operation.
 *
 *-----------------------------------------------------------------------------
 */

StdIO_Status
StdIO_ReadNextLine(FILE *stream,         // IN:
                   char **buf,           // OUT:
                   size_t maxBufLength,  // IN: May be 0.
                   size_t *count)        // OUT/OPT:
{
   DynBuf b;

   ASSERT(stream);
   ASSERT(buf);

   DynBuf_Init(&b);

   for (;;) {
      char *data;
      size_t size;
      size_t max;
      size_t nr;

      /*
       * The dynamic buffer must be at least 2 bytes large, so that at least
       * 1 stream byte and fgets()'s NUL byte can fit --hpreg
       */

      if (DynBuf_Enlarge(&b, 2) == FALSE) {
         errno = ENOMEM;
         goto error;
      }

      /* Read the next chunk of line directly in the dynamic buffer --hpreg */

      data = DynBuf_Get(&b);
      size = DynBuf_GetSize(&b);

      max = DynBuf_GetAllocatedSize(&b);
      nr = max - size;

      if (SuperFgets(stream, &nr, data + size) == NULL) {
         goto error;
      }

      size += nr;
      DynBuf_SetSize(&b, size);

      if (maxBufLength != 0 && size >= maxBufLength) {
         errno = E2BIG;
         goto error;
      }

      if (size < max) {
         /* SuperFgets() found end-of-line */

         if (size == 0 && feof(stream)) {
            /* Reached end-of-file before reading anything */
            DynBuf_Destroy(&b);

            return StdIO_EOF;
         }

         break;
      }

      /*
       * No line terminator found yet, we need a larger buffer to do the next
       * SuperFgets() --hpreg
       */

   }

   /* There is a line in the buffer --hpreg */

   /* NUL terminator --hpreg */
   if (DynBuf_Append(&b, "", 1) == FALSE) {
      errno = ENOMEM;
      goto error;
   }

   *buf = DynBuf_Get(&b);
   if (count) {
      *count = DynBuf_GetSize(&b) - 1;
   }

   return StdIO_Success;

error:
   DynBuf_Destroy(&b);

   return StdIO_Error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StdIOSignalCatcher --
 *
 *      Signal handler for `StdIOCatchSignals`.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Sets the `caughtSignal` global to `TRUE`.
 *
 *-----------------------------------------------------------------------------
 */

static Bool caughtSignal = FALSE;

static void
StdIOSignalCatcher(int signal)  // IN
{
   caughtSignal = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StdIOCatchTerminationSignals --
 *
 *      Registers a signal handler to catch SIGQUIT, SIGINT, and SIGTERM.
 *      If the signal handler is a null pointer, restores the default signal
 *      handler.
 *
 *      Does nothing on Windows.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static void
StdIOCatchTerminationSignals(void (*handler)(int))  // IN
{
#if !defined _WIN32
   struct sigaction act;

   /*
    * We can't use `= { 0 }` because the definition of `struct sigaction` is
    * implementation-dependent, and the first member could be an aggregate
    * type.
    */
   memset(&act, 0, sizeof act);

   if (handler == NULL) {
      act.sa_handler = SIG_DFL;
   } else {
      sigfillset(&act.sa_mask);
      act.sa_flags = SA_RESETHAND;
      act.sa_handler = handler;
   }

   sigaction(SIGQUIT, &act, NULL);
   sigaction(SIGINT,  &act, NULL);
   sigaction(SIGTERM, &act, NULL);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * StdIO_PromptUser --
 *
 *      Prompts the user for input from stdin.  Optionally disables echoing the
 *      input (when prompting for passwords, for example) if possible.
 *
 * Results:
 *      Returns an allocated UTF-8 string of the input.  Returns NULL on
 *      failure.
 *
 *      The caller is responsible for calling `Util_ZeroFreeString` on the
 *      result (or alternatively `free` if the input is non-sensitive).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
StdIO_PromptUser(FILE *out,           // IN
                 const char *prompt,  // IN
                 Bool echo)           // IN
{
   char *ret = NULL;
   Bool readSuccess = FALSE;
   Bool disabledEcho = FALSE;

   enum {
      bufferSize = 1024,
   };

   tchar buffer[bufferSize] = { 0 };
   size_t length;

   Bool isOutputTTY = isatty(fileno(out));

   // Try to disable echoing a typed password.
#ifdef _WIN32
   DWORD oldConsoleMode;
   HANDLE console = GetStdHandle(STD_INPUT_HANDLE);

   if (console == INVALID_HANDLE_VALUE) {
      goto exit;
   }

   if (!echo && GetConsoleMode(console, &oldConsoleMode)) {
      DWORD newConsoleMode = oldConsoleMode & ~ENABLE_ECHO_INPUT;
      disabledEcho = SetConsoleMode(console, newConsoleMode);
   }
#else
   struct termios oldTermiosInfo = { 0 };
   int stdInFd = fileno(stdin);

   if (!echo && isatty(stdInFd) && tcgetattr(stdInFd, &oldTermiosInfo) == 0) {
      struct termios tempTermiosInfo = oldTermiosInfo;
      tempTermiosInfo.c_lflag |= ICANON;
      tempTermiosInfo.c_lflag &= ~ECHO;
      disabledEcho =
         (tcsetattr(stdInFd, TCSAFLUSH, &tempTermiosInfo) == 0);
   }
#endif

   if (isOutputTTY) {
      Posix_Fprintf(out, "%s", prompt);
      fflush(out);
   }

   // Enable reading UTF-16.
   WIN32_ONLY(_setmode(_fileno(stdin), _O_U16TEXT));

   /*
    * It'd be nice to use `StdIO_ReadNextLine` instead of `fgets` and to not
    * use a fixed-size buffer, but buffers resized via `realloc` don't allow
    * us to zero the memory if `realloc` moves the buffer.
    */
   caughtSignal = FALSE;
   StdIOCatchTerminationSignals(StdIOSignalCatcher);
   readSuccess =
      (fgetts(buffer, ARRAYSIZE(buffer), stdin) != NULL) && !caughtSignal;
   StdIOCatchTerminationSignals(NULL);

   // We disabled echoing, so we didn't echo the newline.  Do that now.
   if (!echo && isOutputTTY) {
      Posix_Fprintf(out, "\n");
      fflush(out);
   }

   if (!readSuccess) {
      goto exit;
   }

   // fgets/fgetws might include the newline.  Get rid of it.
   length = tcslen(buffer);
   if (length == 0) {
   } else if (buffer[length - 1] == '\n') {
      buffer[length - 1] = T('\0');
   } else {
      // The buffer is too small.  Better to fail than to silently truncate.
      goto exit;
   }

   ret = AllocUTF8(buffer);

exit:
   if (disabledEcho) {
   #ifdef _WIN32
      SetConsoleMode(console, oldConsoleMode);
   #else
      /*
       * Bug 3509716: Use `TCSAFLUSH` so that any partially inputted line
       * (which might be a password) is discarded when pressing Ctrl+C instead
       * of being left as potential input to the parent process.
       */
      tcsetattr(stdInFd, TCSAFLUSH, &oldTermiosInfo);
   #endif
   }

   Util_Zero(buffer, sizeof buffer);

   return ret;
}
