/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * unicodeSimpleBase.cc --
 *
 *      Simple implementation of unicodeBase.h interface using char *
 *      containing NUL-terminated UTF-8 bytes as the typedef for
 *      Unicode.
 *
 *      Basic Unicode string creation and encoding conversion.
 *
 *      The thread-safety of ConstUnicode functions is the same as
 *      that for standard const char * functions: multiple threads can
 *      call ConstUnicode functions on the same string simultaneously.
 *
 *      However, a non-const Unicode function (like Unicode_Free) must
 *      not be called concurrently with any other Unicode or
 *      ConstUnicode function on the same string.
 */

#include <string.h>

#include "vmware.h"
#include "util.h"
#include "codeset.h"
#include "str.h"
#include "unicodeBase.h"
#include "unicodeInt.h"

/*
 * Padding for initial and final bytes used by an encoding.  The value
 * comes from ICU's UCNV_GET_MAX_BYTES_FOR_STRING macro and accounts
 * for leading and trailing bytes and NUL.
 */
static const size_t UNICODE_UTF16_CODE_UNITS_PADDING = 10;

/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeAllocInternal --
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
 *      encoding or memory could not be allocated, returns NULL.
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL on failure.  Caller must pass to
 *      Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
UnicodeAllocInternal(const void *buffer,      // IN
                     ssize_t lengthInBytes,   // IN
                     StringEncoding encoding) // IN
{
   char *utf8Result = NULL;

   ASSERT(buffer != NULL);

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
      /*
       * Fall through and treat as a special case of UTF-8.
       * Unicode_AllocWithLength() has already ensured we've gotten
       * only 7-bit bytes in 'buffer'.
       */
   case STRING_ENCODING_UTF8:
      {
         char *utf16Str;
         const char *utf8Str = (const char *)buffer;

         if (lengthInBytes == -1) {
            lengthInBytes = strlen(utf8Str);
         }

         // Ensure the input is valid UTF-8.
         if (CodeSet_Utf8ToUtf16le(utf8Str,
                                   lengthInBytes,
                                   &utf16Str,
                                   NULL)) {
            free(utf16Str);
            utf8Result = Util_SafeStrndup(utf8Str, lengthInBytes);
         }
         break;
      }
   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
      if (lengthInBytes == -1) {
         lengthInBytes = Unicode_UTF16Strlen((const utf16_t *)buffer) * 2;
      }

      // utf8Result will be left NULL on failure.
      CodeSet_Utf16leToUtf8((const char *)buffer,
                            lengthInBytes,
                            &utf8Result,
                            NULL);
      break;
   default:
      if (lengthInBytes == -1) {
         /*
          * TODO: This doesn't work for UTF-16 BE, UTF-32, and other
          * encodings with embedded NULs.
          */
         lengthInBytes = strlen((const char *)buffer);
      }
      CodeSet_GenericToGeneric(Unicode_EncodingEnumToName(encoding),
                               buffer, lengthInBytes,
			       "UTF-8", CSGTG_NORMAL, &utf8Result, NULL);
      break;
   }

   return (Unicode)utf8Result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Duplicate --
 *
 *      Allocates and returns a copy of the passed-in Unicode string.
 *
 * Results:
 *      An allocated Unicode string containing a duplicate of the passed-in
 *      string.  Caller must pass to Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_Duplicate(ConstUnicode str) // IN
{
   return (Unicode)Util_SafeStrdup((const char *)str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Free --
 *
 *      Frees the memory for the specified Unicode string and invalidates it.
 *
 *      Not thread-safe when other functions are concurrently
 *      operating on the same string.
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
Unicode_Free(Unicode str) // IN
{
   free(str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AllocList --
 *
 *      Allocates a list (actually a vector) of Unicode strings from a list
 *      (vector) of strings of specified encoding.
 *      The input list has a specified length or can be an argv-style
 *      NULL-terminated list (if length is negative).
 *
 * Results:
 *      An allocated list (vector) of Unicode strings.
 *      The individual strings must be freed with Unicode_Free,
 *      or the entire list can be free with Unicode_FreeList.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode *
Unicode_AllocList(char **srcList,          // IN: list of strings
                  ssize_t length,          // IN: list 
                  StringEncoding encoding) // IN:
{
   Unicode *dstList = NULL;
   ssize_t i;

   ASSERT(srcList != NULL);

   if (length < 0) {
      length = 0;
      while (srcList[length] != NULL) {
         length++;
      }
      
      /* Include the sentinel element. */
      length++;
   }

   dstList = Util_SafeMalloc(length * sizeof *dstList);

   for (i = 0; i < length; i++) {
      dstList[i] = Unicode_Alloc(srcList[i], encoding);
   }

   return dstList;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FreeList --
 *
 *      Free a list (actually a vector) of Unicode strings.
 *      The list (vector) itself is also freed.
 *
 *      The list either has a specified length or is
 *      argv-style NULL terminated (if length is negative).
 *
 * Results:
 *      None
 *
 * Side effects:
 *      errno or Windows last error is preserved.
 *
 *-----------------------------------------------------------------------------
 */

void
Unicode_FreeList(Unicode *list,    // IN: the list to free
                 ssize_t length)   // IN: the length
{
   Util_FreeStringList(list, length);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocList --
 *
 *      Allocates a list (actually a vector) of NUL terminated buffers from a 
 *      list (vector) of strings of specified encoding.
 *      The input list has a specified length or can be an argv-style
 *      NULL-terminated list (if length is negative).
 *
 * Results:
 *      An allocated list (vector) of NUL terminated buffers.
 *      The caller is responsible to free the memory allocated by
 *      this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode *
Unicode_GetAllocList(Unicode const srcList[], // IN: list of strings
		     ssize_t length,          // IN: list 
		     StringEncoding encoding) // IN:
{
   Unicode *dstList = NULL;
   ssize_t i;

   ASSERT(srcList != NULL);

   if (length < 0) {
      length = 0;
      while (srcList[length] != NULL) {
         length++;
      }
      
      /* Include the sentinel element. */
      length++;
   }

   dstList = Util_SafeMalloc(length * sizeof *dstList);

   for (i = 0; i < length; i++) {
      dstList[i] = Unicode_GetAllocBytes(srcList[i], encoding);
   }

   return dstList;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetUTF8 --
 *
 *      Returns the contents of the string encoded as a NUL-terminated UTF-8
 *      byte array.
 *
 * Results:
 *      A NUL-terminated UTF-8 string; lifetime is valid until the next
 *      non-const Unicode function is called on the string.  Caller should
 *      strdup if storing the return value long-term.
 *
 *      Caller does not need to free; the memory is managed inside the
 *      Unicode object.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
Unicode_GetUTF8(ConstUnicode str) // IN
{
   return (const char *)str;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_LengthInCodeUnits --
 *
 *      Gets the length of the Unicode string in UTF-8 code units.
 *
 * Results:
 *      The length of the string in UTF-8 code units.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

UnicodeIndex
Unicode_LengthInCodeUnits(ConstUnicode str) // IN
{
   return strlen((const char *)str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_BytesRequired --
 *
 *      Gets the number of bytes needed to encode the Unicode string in
 *      the specified encoding, including NUL-termination.
 *
 *      Use this to find the size required for the byte array passed
 *      to Unicode_CopyBytes.
 *
 * Results:
 *      The number of bytes needed to encode the Unicode string in the
 *      specified encoding.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

size_t
Unicode_BytesRequired(ConstUnicode str,        // IN
                      StringEncoding encoding) // IN
{
   const uint8 *utf8 = (const uint8 *)str;

   // Number of bytes needed for a code point [U+0000, U+FFFF].
   size_t basicCodePointSize;

   // Number of bytes needed for a code point [U+10000, U+10FFFF].
   size_t supplementaryCodePointSize;

   size_t result = 0;

   switch (encoding) {
   case STRING_ENCODING_UTF8:
      return strlen(utf8) + 1;
   case STRING_ENCODING_US_ASCII:
   case STRING_ENCODING_ISO_8859_1:
   case STRING_ENCODING_WINDOWS_1252:
      // TODO: Lots more encodings can be added here.
      basicCodePointSize = supplementaryCodePointSize = 1;
      break;
   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
   case STRING_ENCODING_UTF16_BE:
      basicCodePointSize = 2;
      supplementaryCodePointSize = 4;
      break;
   case STRING_ENCODING_UTF32:
   case STRING_ENCODING_UTF32_LE:
   case STRING_ENCODING_UTF32_BE:
      basicCodePointSize = 4;
      supplementaryCodePointSize = 4;
      break;
   default:
      /*
       * Assume the worst: ISO-2022-JP takes up to 7 bytes per code point.
       */
      basicCodePointSize = 7;
      supplementaryCodePointSize = 7;
      break;
   }

   /*
    * Do a simple check of how many bytes are needed to convert the
    * UTF-8 to the target encoding.  This doesn't do UTF-8 validity
    * checking, but will not overrun the end of the buffer.
    */
   while (*utf8) {
      size_t utf8NumBytesRemaining;

      // Advance one code point forward in the UTF-8 input.
      if (*utf8 <= 0x7F) {
         utf8NumBytesRemaining = 1;
         result += basicCodePointSize;
      } else if (*utf8 & 0xC0) {
         utf8NumBytesRemaining = 2;
         result += basicCodePointSize;
      } else if (*utf8 & 0xE0) {
         utf8NumBytesRemaining = 3;
         result += basicCodePointSize;
      } else if (*utf8 & 0xF0) {
         utf8NumBytesRemaining = 4;
         result += supplementaryCodePointSize;
      } else {
         // Invalid input; nothing we can do.
         break;
      }

      while (*utf8 && utf8NumBytesRemaining) {
         utf8NumBytesRemaining--;
         utf8++;
      }

      if (utf8NumBytesRemaining > 0) {
         // Invalid input; nothing we can do.
         break;
      }
   }

   // Add enough for NUL expressed in the target encoding.
   result += UNICODE_UTF16_CODE_UNITS_PADDING * basicCodePointSize;

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_CopyBytes --
 *
 *      Encodes the Unicode string using the specified encoding into
 *      the specified buffer and NUL-terminates it, writing at most
 *      maxLengthInBytes bytes in total to the buffer.
 *
 * Results:
 *      Returns FALSE if the Unicode string requires more than
 *      maxLengthInBytes bytes to be encoded in the specified
 *      encoding, including NUL termination. (Call
 *      Unicode_BytesRequired(str, encoding) to get the correct
 *      length.). Returns TRUE if no truncation was required. In
 *      either case, if retLength is not NULL, *retLength contains the
 *      number of bytes actually written to the buffer upon return.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unicode_CopyBytes(void *destBuffer,        // OUT
                  ConstUnicode srcBuffer,  // IN
                  size_t maxLengthInBytes, // IN
                  size_t *retLength,       // OUT
                  StringEncoding encoding) // IN
{
   const char *utf8Str = (const char *)srcBuffer;
   Bool notTruncated = TRUE;
   size_t copyBytes = 0;

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
      if (!UnicodeSanityCheck(utf8Str, -1, encoding)) {
	 break;
      }
      // fall through
   case STRING_ENCODING_UTF8:
      {
         size_t len = strlen(utf8Str);
         copyBytes = MIN(len, maxLengthInBytes - 1);
         memcpy(destBuffer, utf8Str, copyBytes);

         /*
          * If we truncated, force a null termination in a UTF-8 safe
          * manner.
          */
         if (copyBytes < len) {
            notTruncated = FALSE;
            if (encoding == STRING_ENCODING_UTF8) {
               copyBytes =
                  CodeSet_Utf8FindCodePointBoundary(destBuffer, copyBytes);
            }
         }

         ((char*)destBuffer)[copyBytes] = '\0';
      }
      break;
   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
      {
         char *utf16Buf;
         size_t utf16BufLen;

         if (!CodeSet_Utf8ToUtf16le(utf8Str,
                                    strlen(utf8Str),
                                    &utf16Buf,
                                    &utf16BufLen)) {
            break;
         }
         copyBytes = MIN(utf16BufLen, maxLengthInBytes - 2);
         memcpy(destBuffer, utf16Buf, copyBytes);
         copyBytes = CodeSet_Utf16FindCodePointBoundary(destBuffer, copyBytes);
         ((utf16_t*)destBuffer)[copyBytes / 2] = 0;
         free(utf16Buf);

         if (copyBytes < utf16BufLen) {
            notTruncated = FALSE;
         }

         break;
      }
   default:
      {
         char *currentBuf;
         size_t currentBufSize;

	 if (!CodeSet_GenericToGeneric("UTF-8", utf8Str, strlen(utf8Str),
				       Unicode_EncodingEnumToName(encoding),
				       CSGTG_NORMAL,
				       &currentBuf, &currentBufSize)) {
	    break;
	 }
         copyBytes = MIN(currentBufSize, maxLengthInBytes - 1);
         memcpy(destBuffer, currentBuf, copyBytes);
         free(currentBuf);

         /* 
          * XXX this isn't quite correct, we still need to truncate on
          * a code point boundary, based on the current encoding type,
          * rather than just null terminate blindly.
          */

         ((char*)destBuffer)[copyBytes] = 0;

         if (copyBytes < currentBufSize) {
            notTruncated = FALSE;
         }
      }
      break;
   }

   if (retLength) {
      *retLength = copyBytes;
   }
   return notTruncated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocBytes --
 *
 *      Allocates and returns a NUL terminated buffer into which the contents
 *      of the unicode string are extracted using the specified encoding.
 *
 *      NOTE: The length of the NUL can depend on the encoding.
 *            UTF-16 NUL is "\0\0"; UTF-32 NUL is "\0\0\0\0".
 *
 *      NULL is returned for NULL argument.
 *
 * Results:
 *      Pointer to the dynamically allocated memory,
 *      or NULL on NULL argument.
 *      The caller is responsible to free the memory allocated
 *      by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
Unicode_GetAllocBytes(ConstUnicode str,        // IN:
                      StringEncoding encoding) // IN:
{
   void *result;

   if (str == NULL) {
      return NULL;
   }

   result = UnicodeGetAllocBytesInternal(str, encoding, NULL);
   ASSERT_NOT_IMPLEMENTED(result != NULL);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeGetAllocBytesInternal --
 *
 *      Encodes the Unicode string using the specified encoding into
 *      an allocated, null-terminated buffer.
 *
 * Results:
 *      The converted string in an allocated buffer,
 *      or NULL on conversion or memory allocation failure.
 *
 *	The length of the result (in bytes, without termination)
 *	in retLength.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
UnicodeGetAllocBytesInternal(ConstUnicode ustr,       // IN
                             StringEncoding encoding, // IN
                             size_t *retLength)       // OUT: optional
{
   const char *utf8Str = ustr;
   size_t len;
   char *result = NULL;

   ASSERT(ustr != NULL);

   len = strlen(utf8Str);

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
      if (!UnicodeSanityCheck(utf8Str, len, encoding)) {
	 break;
      }
      // fall through
   case STRING_ENCODING_UTF8:
      if ((result = malloc(len + 1)) == NULL) {
	 break;
      }
      memcpy(result, utf8Str, len + 1);
      if (retLength != NULL) {
	 *retLength = len;
      }
      break;

   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
      if (!CodeSet_Utf8ToUtf16le(utf8Str, len, &result, retLength)) {
	 ASSERT(result == NULL);
      }
      break;

   default:
      if (!CodeSet_GenericToGeneric("UTF-8", utf8Str, len,
				    Unicode_EncodingEnumToName(encoding),
				    CSGTG_NORMAL,
				    &result, retLength)) {
	 ASSERT(result == NULL);
      }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeAllocStatic --
 *
 *      Internal helper function to allocate a new Unicode string
 *      given an array of bytes in US-ASCII encoding.
 *
 *      If 'unescape' is TRUE, unescapes \\uABCD to U+ABCD, and
 *      \\U001FABCD to U+1FABCD.
 *
 * Results:
 *      The allocated Unicode string.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
UnicodeAllocStatic(const char *asciiBytes, // IN
                   Bool unescape)          // IN
{
   utf16_t *utf16;
   // Explicitly use int8 so we don't depend on whether char is signed.
   const int8 *byte = (const int8 *)asciiBytes;
   size_t utf16Offset = 0;
   Unicode result;

   ASSERT(asciiBytes);

   if (!unescape) {
      DEBUG_ONLY(
         while (*byte > 0) {
            byte++;
         }
         // All input must be 7-bit US-ASCII.
         ASSERT(*byte == 0);
      );
      return Util_SafeStrdup(asciiBytes);
   }

   utf16 = Util_SafeMalloc(sizeof *utf16 * (strlen(asciiBytes) + 1));

   while (TRUE) {
      size_t hexDigitsRemaining;
      uint32 escapedCodePoint = 0;
      Bool foundEscapedCodePoint = FALSE;

      if (*byte == '\0') {
         utf16[utf16Offset] = 0;
         break;
      }

      // Only US-ASCII bytes are allowed as input.
      ASSERT_NOT_IMPLEMENTED(*byte > 0);

      if (*byte != '\\') {
         utf16[utf16Offset++] = *byte;
         byte++;
         continue;
      }

      // Handle the backslash.
      byte++;

      if (*byte == '\0') {
         // We'll fall out at the top of the loop.
         continue;
      }

      ASSERT_NOT_IMPLEMENTED(*byte > 0);

      switch (*byte) {
      case 'u':
         /*
          * \\uABCD must have exactly four hexadecimal digits after
          * the escape, denoting the Unicode code point U+ABCD.
          */
         foundEscapedCodePoint = TRUE;
         hexDigitsRemaining = 4;
         break;
      case 'U':
         /*
          * \\U0010CDEF must have exactly eight hexadecimal digits
          * after the escape, denoting the Unicode code point U+10CDEF.
          */
         foundEscapedCodePoint = TRUE;
         hexDigitsRemaining = 8;
         break;
      default:
         // Unsupported escape; treat the next byte literally.
         hexDigitsRemaining = 0;
         utf16[utf16Offset++] = *byte;
         break;
      }

      byte++;

      while (hexDigitsRemaining) {
         uint8 hexValue;

         if (*byte >= '0' && *byte <= '9') {
            hexValue = *byte - '0';
         } else if (*byte >= 'A' && *byte <= 'F') {
            hexValue = *byte - 'A' + 0xA;
         } else if (*byte >= 'a' && *byte <= 'f') {
            hexValue = *byte - 'a' + 0xA;
         } else {
            NOT_IMPLEMENTED();
         }

         escapedCodePoint = (escapedCodePoint << 4) | hexValue;

         byte++;
         hexDigitsRemaining--;
      }

      if (foundEscapedCodePoint) {
         ASSERT_NOT_IMPLEMENTED(escapedCodePoint <= 0x10FFFF);

         if (U16_LENGTH(escapedCodePoint) == 1) {
            utf16[utf16Offset++] = (utf16_t)escapedCodePoint;
         } else {
            utf16[utf16Offset++] = U16_LEAD(escapedCodePoint);
            utf16[utf16Offset++] = U16_TRAIL(escapedCodePoint);
         }
      }
   }

   result = Unicode_AllocWithUTF16(utf16);
   free(utf16);

   return result;
}
