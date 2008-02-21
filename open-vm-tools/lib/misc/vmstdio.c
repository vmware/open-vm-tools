/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
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


/*
 *-----------------------------------------------------------------------------
 *
 * SuperFgets --
 *
 *      The fgets() API is poorly designed: it doesn't return how many bytes
 *      were written to the buffer. Hence this function --hpreg
 *
 * Results:
 *      Like fgets(). On success, '*count' is the number of bytes written to the
 *      buffer (excluding the NUL terminator)
 *    
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void *
SuperFgets(FILE *stream,        // IN
           void *bufIn,         // OUT
           size_t *count)       // IN/OUT
{
   char *buf;

   /*
    * To compute how many bytes were written to the buffer, we fill our buffer
    * with non-NUL bytes prior to calling fgets(). Consequently, after calling
    * fgets(), the only NUL bytes that are in the buffer are:
    * . those that were read from the stream
    * . the one added by fgets() after all the other bytes that were read
    *   from the stream
    * So all we need to do is locate the last NUL byte in the buffer.
    *
    * Note that this may not work if your implementation of fgets() does funny
    * things (which is unlikely) like writing to buffer positions beyond the
    * terminating NUL byte --hpreg
    */

   ASSERT(stream);
   buf = (char *)bufIn;
   ASSERT(buf);
   ASSERT(count);
   ASSERT(*count);

   memset(buf, '\0' - 1, *count);

   /*
    * In the end of stream case, glibc's fgets() returns NULL but does not set
    * errno --hpreg
    */
   errno = 0;
   if (fgets(buf, *count, stream) == NULL) {
      if (errno) {
         return NULL;
      }

      /* End of stream and no byte was written --hpreg */
      *count = 0;
      return NULL;
   }

   do {
      (*count)--;
   } while (buf[*count] != '\0');

   return buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StdIO_ReadNextLine --
 *
 *      Read the next line from a stream.
 *
 *      A line is defined as an arbitrary long sequence of arbitrary bytes, that
 *      ends with the first occurrence of one of these line terminators:
 *      . \r\n          (the ANSI way)
 *      . \n            (the UNIX way)
 *      . end-of-stream
 *
 *     If maxBufLength is non-zero at most maxBufLength bytes will be allocated.
 *
 * Results:
 *      StdIO_Success on success: '*buf' is an allocated, NUL terminated buffer
 *                                that contains the line (excluding the line
 *                                terminator). If not NULL, '*count' contains the
 *                                size of the buffer (excluding the NUL
 *                                terminator)
 *      StdIO_EOF if there is no next line (end of stream)
 *      StdIO_Error on failure: errno is set accordingly
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

StdIO_Status
StdIO_ReadNextLine(FILE *stream,         // IN
                   char **buf,           // OUT
                   size_t maxBufLength,  // IN
                   size_t *count)        // OUT
{
   DynBuf b;

   ASSERT(stream);
   ASSERT(buf);

   DynBuf_Init(&b);

   for (;;) {
      char *data;
      size_t size;
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

      if (maxBufLength != 0 && size > maxBufLength) {
         errno = E2BIG;
         goto error;
      }

      nr = DynBuf_GetAllocatedSize(&b) - size;
      /*
       * XXX SuperFgets() is such a hack that I may replace it soon with a more
       *     reliable fgetc() loop --hpreg
       */
      if (SuperFgets(stream, data + size, &nr) == NULL) {
         if (errno) {
            goto error;
         }

         /* End of stream: there is no next chunk of line --hpreg */
         ASSERT(nr == 0);

         if (size) {
            /* Found an end-of-stream line terminator --hpreg */
            break;
         }

         DynBuf_Destroy(&b);

         return StdIO_EOF;
      }
      /*
       * fgets() wrote at least 1 stream byte, and NUL terminated the dynamic
       * buffer --hpreg
       */
      ASSERT(nr);
      size += nr;

      if (data[size - 1] == '\n') {
         /* Found a Unix or ANSI line terminator --hpreg */
         size--;

/* Win32 takes care of this in fgets() --hpreg */
#if !defined(_WIN32)
         if (size && data[size - 1] == '\r') {
            /* Found an ANSI line terminator --hpreg */
            size--;
         }
#endif

         /* Remove the line terminator from the dynamic buffer --hpreg */
         DynBuf_SetSize(&b, size);
         break;
      }

      /*
       * No line terminator found yet, we need a larger buffer to do the next
       * fgets() --hpreg
       */
      DynBuf_SetSize(&b, size);
   }

   /* There is a line in the buffer --hpreg */

   if (   /* NUL terminator --hpreg */
          DynBuf_Append(&b, "", 1) == FALSE
       || DynBuf_Trim(&b) == FALSE) {
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
