/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * unicodeCommon.c --
 *
 *      Functions common to all implementations of lib/unicode.
 */

#include <stdlib.h>
#include <string.h>

#include "vmware.h"

#include "escape.h"
#include "vm_assert.h"
#include "unicodeBase.h"
#include "unicodeInt.h"
#include "unicodeTypes.h"


// Array of byte values we want Escape_Do() to escape when logging.
static const int NonPrintableBytesToEscape[256] = {
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // Control characters.
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, // More control characters.
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, // Backslash
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, // DEL
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};


static char * UnicodeEscapeBuffer(const void *buffer,
                                  ssize_t lengthInBytes,
                                  StringEncoding encoding);
static Bool UnicodeSanityCheck(const void *buffer,
                               ssize_t lengthInBytes,
                               StringEncoding encoding);

/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeEscapeBuffer --
 *
 *      Escape non-printable bytes of the buffer with \xAB, where 0xAB
 *      is the non-printable byte value.
 *
 * Results:
 *      The allocated, NUL-terminated, US-ASCII string containing the
 *      escaped buffer.  Caller must free the buffer.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
UnicodeEscapeBuffer(const void *buffer,      // IN
                    ssize_t lengthInBytes,   // IN
                    StringEncoding encoding) // IN
{
   if (lengthInBytes == -1) {
      switch (encoding) {
      case STRING_ENCODING_UTF16:
      case STRING_ENCODING_UTF16_LE:
      case STRING_ENCODING_UTF16_BE:
         lengthInBytes = Unicode_UTF16Strlen((const utf16_t *)buffer) * 2;
         break;
      case STRING_ENCODING_UTF32:
      case STRING_ENCODING_UTF32_LE:
      case STRING_ENCODING_UTF32_BE:
         {
            const uint32 *utf32 = (const uint32 *)buffer;
            ssize_t numCodeUnits;

            for (numCodeUnits = 0; utf32[numCodeUnits]; numCodeUnits++) {
               // Count the number of code units until we hit NUL.
            }

            lengthInBytes = numCodeUnits * 4;
         }
      default:
         lengthInBytes = strlen((const char *)buffer);
         break;
      }
   }

   /*
    * The buffer could have NULs or 8-bit values inside.  Escape it.
    */
   return Escape_DoString("\\x",
                          NonPrintableBytesToEscape,
                          buffer,
                          lengthInBytes,
                          NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeSanityCheck --
 *
 *      Simple sanity checks on buffers of specified encodings.
 *
 * Results:
 *      TRUE if the buffer passed the sanity check for the specified encoding,
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
UnicodeSanityCheck(const void *buffer,      // IN
                   ssize_t lengthInBytes,   // IN
                   StringEncoding encoding) // IN
{
   /*
    * Sanity check US-ASCII here, so we can fast-path its conversion
    * to Unicode later.
    */
   if (encoding == STRING_ENCODING_US_ASCII) {
      const uint8 *asciiBytes = (const uint8 *)buffer;
      Bool nulTerminated = (lengthInBytes == -1);
      ssize_t i;

      for (i = 0; nulTerminated ? asciiBytes[i] : i < lengthInBytes; i++) {
         if (asciiBytes[i] >= 0x80) {
            return FALSE;
         }
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodePinIndices --
 *
 *      Given a string, a start index, and a length in code units,
 *      pins the index and length so that they're within the
 *      boundaries of the string.
 *
 *      numCodeUnits is the length of str in code units.
 *
 *      If startIndex == -1, sets startIndex to numCodeUnits.
 *
 *      If length == -1, sets length to (numCodeUnits - startIndex).
 *
 *      If startIndex > numCodeUnits, sets startIndex to numCodeUnits.
 *
 *      If startIndex + length > numCodeUnits, sets length to
 *         (numCodeUnits - startIndex).
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
UnicodePinIndices(ConstUnicode str,         // IN
                  UnicodeIndex *startIndex, // IN/OUT
                  UnicodeIndex *length)     // IN/OUT
{
   UnicodeIndex numCodeUnits;

   ASSERT(str);
   ASSERT(startIndex);
   ASSERT(*startIndex >= 0 || *startIndex == -1);
   ASSERT(length);
   ASSERT(*length >= 0 || *length == -1);

   numCodeUnits = Unicode_LengthInCodeUnits(str);

   if (   *startIndex < 0
       || *startIndex > numCodeUnits) {
      // Start on the NUL at the end of the string.
      *startIndex = numCodeUnits;
   }

   if (   *length < 0
       || *startIndex + *length > numCodeUnits) {
      *length = numCodeUnits - *startIndex;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_UTF16Strlen --
 *
 *      Gets the number of code units in NUL-terminated UTF-16 array.
 *
 * Results:
 *      The number of code units in the array.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ssize_t
Unicode_UTF16Strlen(const utf16_t *utf16) // IN
{
   ssize_t length;

   for (length = 0; utf16[length]; length++) {
      // Count the number of code units until we hit NUL.
   }

   return length;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AllocWithLength --
 *
 *      Allocates a new Unicode string given a buffer with both length
 *      in bytes and string encoding specified.
 *
 *      If lengthInBytes is -1, then buffer must be NUL-terminated.
 *      Otherwise, buffer must be of the specified length, but does
 *      not need to be NUL-terminated.
 *
 *      Note that regardless of the encoding of the buffer passed to this
 *      function, the returned string can hold any Unicode characters.
 *
 *      If the buffer contains an invalid sequence of the specified
 *      encoding or memory could not be allocated, logs the buffer,
 *      ASSERTs and returns NULL (for non-debug builds).
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL on failure.  Caller must pass the string to
 *      Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_AllocWithLength(const void *buffer,      // IN
                        ssize_t lengthInBytes,   // IN
                        StringEncoding encoding) // IN
{
   Unicode result = NULL;
   char *escapedBuffer;

   ASSERT(lengthInBytes >= 0 || lengthInBytes == -1);

   if (lengthInBytes < 0 && lengthInBytes != -1) {
      return NULL;
   }

   if (UnicodeSanityCheck(buffer, lengthInBytes, encoding)) {
      result = UnicodeAllocInternal(buffer, lengthInBytes, encoding);
   }

   if (result) {
      // Success!  We've allocated the Unicode string and can return.
      return result;
   }

   // Failure. Log the buffer, ASSERT, and return (in non-debug builds).
   escapedBuffer = UnicodeEscapeBuffer(buffer,
                                       lengthInBytes,
                                       encoding);

   Log("%s: Error: Couldn't convert invalid buffer [%s] from %s to Unicode.\n",
       __FUNCTION__,
       escapedBuffer ? escapedBuffer : "(couldn't escape bytes)",
       Unicode_EncodingEnumToName(encoding));

   free(escapedBuffer);

   /*
    * Invalid input bytes are fatal in debug builds.  If you need
    * to sanity check an input array for validity, call
    * Unicode_IsBufferValid() before allocating this string.
    */
   ASSERT(FALSE);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsBufferValid --
 *
 *      Tests if the given buffer is valid in the specified encoding.
 *
 * Results:
 *      TRUE if the buffer is valid, FALSE if it's not.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unicode_IsBufferValid(const void *buffer,      // IN
                      ssize_t lengthInBytes,   // IN
                      StringEncoding encoding) // IN
{
   Unicode result;

   if (!UnicodeSanityCheck(buffer, lengthInBytes, encoding)) {
      return FALSE;
   }

   result = UnicodeAllocInternal(buffer, lengthInBytes, encoding);

   if (!result) {
      return FALSE;
   } else {
      Unicode_Free(result);
      return TRUE;
   }
}
