/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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


static SnowMotionLogger snowMotionLogger = NULL;

/*
 *-----------------------------------------------------------------------------
 *
 * StdIO_ToggleSnowMotionLogging --
 *
 *    SnowMotion-specific log toggling - as per PR#2121674 and 2108730, vmx
 *    corruption appears to occur at some arbitrary point prior to VM powerOn,
 *    but only in CAT - it has not been reproducible locally or via manually
 *    triggered Nimbus testruns.
 *
 *    To try to isolate the corruption point, add a toggled logging mechanism
 *    that will dump the results of SuperFGets for the interval between
 *    main initializing and VM powerOn.
 *
 *    This is a strictly temporary mechanism to provide the necessary logging
 *    to debug this issue.
 *
 *-----------------------------------------------------------------------------
 */

void
StdIO_ToggleSnowMotionLogging(SnowMotionLogger logger)   // IN:
{
   snowMotionLogger = logger;
}


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

   if (snowMotionLogger != NULL) {
      snowMotionLogger(buf, *count);
   }

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
