/*********************************************************
 * Copyright (C) 2007-2016,2020 VMware, Inc. All rights reserved.
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


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_EscapeBuffer --
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
Unicode_EscapeBuffer(const void *buffer,      // IN
                     ssize_t lengthInBytes,   // IN
                     StringEncoding encoding) // IN
{
   encoding = Unicode_ResolveEncoding(encoding);

   if (lengthInBytes == -1) {
      lengthInBytes = Unicode_LengthInBytes(buffer, encoding);
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
   ASSERT(Unicode_IsEncodingValid(encoding));

   /*
    * Sanity check US-ASCII here, so we can fast-path its conversion
    * to Unicode later.
    */

   if (encoding == STRING_ENCODING_US_ASCII) {
      const uint8 *asciiBytes = (const uint8 *) buffer;

      if (lengthInBytes == -1) {
	 for (; *asciiBytes != '\0'; asciiBytes++) {
	    if (*asciiBytes >= 0x80) {
	       return FALSE;
	    }
	 }
      } else {
	 ssize_t i;

	 for (i = 0; i < lengthInBytes; i++) {
	    if (asciiBytes[i] >= 0x80) {
	       return FALSE;
	    }
	 }
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_LengthInBytes --
 *
 *      Compute the length in bytes of a string in a given encoding.
 *
 * Results:
 *      The number of bytes.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ssize_t
Unicode_LengthInBytes(const void *buffer,      // IN
                      StringEncoding encoding) // IN
{
   ssize_t len;

   encoding = Unicode_ResolveEncoding(encoding);

   switch (encoding) {
   case STRING_ENCODING_UTF32_LE:
   case STRING_ENCODING_UTF32_BE:
   case STRING_ENCODING_UTF32_XE:
   {
      const int32 *p;

      for (p = buffer; *p != 0; p++) {
      }
      len = (const char *) p - (const char *) buffer;
      break;
   }
   case STRING_ENCODING_UTF16_LE:
   case STRING_ENCODING_UTF16_BE:
   case STRING_ENCODING_UTF16_XE:
   {
      const utf16_t *p;

      for (p = buffer; *p != 0; p++) {
      }
      len = (const char *) p - (const char *) buffer;
      break;
   }
   default:
      // XXX assume 8-bit encoding with no embedded null
      len = strlen(buffer);
   }

   return len;
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
 * Unicode_UTF16Strdup --
 *
 *      Duplicates a UTF-16 string.
 *
 * Results:
 *      Returns an allocated copy of the input UTF-16 string.  The caller
 *      is responsible for freeing it.
 *
 *      Panics on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

utf16_t *
Unicode_UTF16Strdup(const utf16_t *utf16) // IN: May be NULL.
{
   utf16_t *copy;
   ssize_t numBytes;

   // Follow Util_SafeStrdup semantics.
   if (utf16 == NULL) {
      return NULL;
   }

   numBytes = (Unicode_UTF16Strlen(utf16) + 1 /* NUL */) * sizeof *copy;
   copy = Util_SafeMalloc(numBytes);
   memcpy(copy, utf16, numBytes);

   return copy;
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
 *	If buffer is NULL, then NULL is returned.
 *	In this case, lengthInBytes must be 0 or -1, consistent with
 *	an empty string.
 *
 *      Note that regardless of the encoding of the buffer passed to this
 *      function, the returned string can hold any Unicode characters.
 *
 *      If the buffer contains an invalid sequence of the specified
 *      encoding or memory could not be allocated, logs the buffer,
 *      and panics.
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL if input is NULL.
 *	Caller must pass the string to free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_AllocWithLength(const void *buffer,       // IN:
                        ssize_t lengthInBytes,    // IN:
                        StringEncoding encoding)  // IN:
{
   char *result;

   ASSERT(lengthInBytes >= 0 || lengthInBytes == -1);

   if (buffer == NULL) {
      ASSERT(lengthInBytes <= 0);
      return NULL;
   }

   encoding = Unicode_ResolveEncoding(encoding);

   if (lengthInBytes == -1) {
      lengthInBytes = Unicode_LengthInBytes(buffer, encoding);
   }

   result = UnicodeAllocInternal(buffer, lengthInBytes, encoding, FALSE);

   if (result == NULL) {
      char *escapedBuffer = Unicode_EscapeBuffer(buffer, lengthInBytes,
                                                 encoding);

      Panic("%s: Couldn't convert invalid buffer [%s] from %s to Unicode.\n",
            __FUNCTION__,
            escapedBuffer ? escapedBuffer : "(couldn't escape bytes)",
            Unicode_EncodingEnumToName(encoding));
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_CanGetBytesWithEncoding --
 *
 *      Tests if the given Unicode string can be converted
 *      losslessly to the specified encoding.
 *
 * Results:
 *      TRUE if the string can be converted, FALSE if it cannot.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unicode_CanGetBytesWithEncoding(const char *ustr,         // IN:
                                StringEncoding encoding)  // IN:
{
   void *tmp;

   if (ustr == NULL) {
      return TRUE;
   }

   tmp = UnicodeGetAllocBytesInternal(ustr, encoding, -1, NULL);

   if (tmp == NULL) {
      return FALSE;
   }
   free(tmp);

   return TRUE;
}
