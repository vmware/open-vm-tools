/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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
 *      The thread-safety of const char *functions is the same as
 *      that for standard const char * functions: multiple threads can
 *      call const char *functions on the same string simultaneously.
 *
 *      However, a non-const Unicode function (like free) must
 *      not be called concurrently with any other Unicode or
 *      const char *function on the same string.
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
 *      Return NULL on memory allocation failure.
 *
 *      Return NULL if strict is true and the buffer contains an invalid
 *      sequence in the specified encoding.
 *
 *      If strict is false, then an invalid sequence is replaced by
 *      a substitution character, which is either the Unicode
 *      substitution character (U+FFFD or \xef\xbf\xbd in UTF-8)
 *      or subchar1 (ASCII SUB or control-z, value 0x1a).
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL on failure.  Caller must pass to
 *      free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
UnicodeAllocInternal(const void *buffer,      // IN
                     ssize_t lengthInBytes,   // IN
                     StringEncoding encoding, // IN
                     Bool strict)             // IN
{
   char *utf8Result = NULL;

   ASSERT(buffer != NULL);
   ASSERT(lengthInBytes >= 0);
   ASSERT(Unicode_IsEncodingValid(encoding));

   if (!strict) {
      CodeSet_GenericToGeneric(Unicode_EncodingEnumToName(encoding),
                               buffer, lengthInBytes,
                               "UTF-8", CSGTG_TRANSLIT, &utf8Result, NULL);
      return utf8Result;
   }

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
   case STRING_ENCODING_UTF8:
      if (Unicode_IsBufferValid(buffer, lengthInBytes, encoding)) {
         utf8Result = Util_SafeStrndup(buffer, lengthInBytes);
      }
      break;
   case STRING_ENCODING_UTF16_LE:
      // utf8Result will be left NULL on failure.
      CodeSet_Utf16leToUtf8((const char *)buffer,
                            lengthInBytes,
                            &utf8Result,
                            NULL);
      break;
   default:
      CodeSet_GenericToGeneric(Unicode_EncodingEnumToName(encoding),
                               buffer, lengthInBytes,
                               "UTF-8", 0, &utf8Result, NULL);
      break;
   }

   return utf8Result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsStringValidUTF8 --
 *
 *      Tests if the specified string (NUL terminated) is valid UTF8.
 *
 * Results:
 *      TRUE   The string is valid UTF8.
 *      FALSE  The string is not valid UTF8.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Unicode_IsStringValidUTF8(const char *str)  // IN:
{
   return CodeSet_IsStringValidUTF8(str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsBufferValid --
 *
 *      Tests if the given buffer is valid in the specified encoding.
 *
 *      If lengthInBytes is -1, then buffer must be NUL-terminated.
 *      Otherwise, buffer must be of the specified length, but does
 *      not need to be NUL-terminated.
 *
 *      This function should not be used for heuristic determination of
 *      encodings.  Since the test looks for bit patterns in the buffer
 *      that are invalid in the specified encoding, negative results
 *      guarantee the buffer is not in the specified encoding, but positive
 *      results are inconclusive.  Source buffers containing pure ASCII
 *      will pass all 8-bit encodings, and all source buffers will pass 
 *      a windows-1252 test since win-1252 maps all 256 8-bit combinations.
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
   if (buffer == NULL) {
      ASSERT(lengthInBytes <= 0);
      return TRUE;
   }

   encoding = Unicode_ResolveEncoding(encoding);

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
      return UnicodeSanityCheck(buffer, lengthInBytes, encoding);

   case STRING_ENCODING_UTF8:
      if (lengthInBytes == -1) {
         return CodeSet_IsStringValidUTF8(buffer);
      } else {
         return CodeSet_IsValidUTF8(buffer, lengthInBytes);
      }

   default:
      if (lengthInBytes == -1) {
         lengthInBytes = Unicode_LengthInBytes(buffer, encoding);
      }

      return CodeSet_Validate(buffer, lengthInBytes,
                              Unicode_EncodingEnumToName(encoding));
   }
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
 *      string.  Caller must pass to free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_Duplicate(const char *str) // IN
{
   return Util_SafeStrdup(str);
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
 *      The individual strings must be freed with free,
 *      or the entire list can be free with Util_FreeStringList.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char **
Unicode_AllocList(char **srcList,          // IN: list of strings
                  ssize_t length,          // IN: list 
                  StringEncoding encoding) // IN:
{
   char **dstList = NULL;
   ssize_t i;

   ASSERT(srcList != NULL);

   encoding = Unicode_ResolveEncoding(encoding);

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
 * Unicode_GetAllocList --
 *
 *      Given a list of Unicode strings, converts them to a list of
 *      buffers in the specified encoding.
 *
 *      The input list has a specified length or can be an argv-style
 *      NULL-terminated list (if length is negative).
 *
 * Results:
 *      An allocated list (vector) of NUL terminated buffers in the specified
 *      encoding
 *      or NULL on conversion failure.
 *      The caller is responsible to free the memory allocated by
 *      this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char **
Unicode_GetAllocList(char *const srcList[],   // IN: list of strings
                     ssize_t length,          // IN: length (-1 for NULL term.)
                     StringEncoding encoding) // IN: Encoding of returned list
{
   char **dstList = NULL;
   ssize_t i;

   ASSERT(srcList != NULL);

   encoding = Unicode_ResolveEncoding(encoding);

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
      if (dstList[i] == NULL && srcList[i] != NULL) {
         while (--i >= 0) {
            free(dstList[i]);
         }
         free(dstList);
         return NULL;
      }
   }

   return dstList;
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
Unicode_LengthInCodeUnits(const char *str) // IN
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
Unicode_BytesRequired(const char *str,         // IN
                      StringEncoding encoding) // IN
{
   const uint8 *utf8 = (const uint8 *)str;

   // Number of bytes needed for a code point [U+0000, U+FFFF].
   size_t basicCodePointSize;

   // Number of bytes needed for a code point [U+10000, U+10FFFF].
   size_t supplementaryCodePointSize;

   size_t result = 0;

   encoding = Unicode_ResolveEncoding(encoding);

   switch (encoding) {
   case STRING_ENCODING_UTF8:
      return strlen((const char *)utf8) + 1;
   case STRING_ENCODING_US_ASCII:
   case STRING_ENCODING_ISO_8859_1:
   case STRING_ENCODING_WINDOWS_1252:
      // TODO: Lots more encodings can be added here.
      basicCodePointSize = supplementaryCodePointSize = 1;
      break;
   case STRING_ENCODING_UTF16_LE:
   case STRING_ENCODING_UTF16_BE:
   case STRING_ENCODING_UTF16_XE:
      basicCodePointSize = 2;
      supplementaryCodePointSize = 4;
      break;
   case STRING_ENCODING_UTF32_LE:
   case STRING_ENCODING_UTF32_BE:
   case STRING_ENCODING_UTF32_XE:
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
 *      FALSE on conversion failure or if the Unicode string requires
 *      more than maxLengthInBytes bytes to be encoded in the specified
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
                  const char *srcBuffer,   // IN
                  size_t maxLengthInBytes, // IN
                  size_t *retLength,       // OUT
                  StringEncoding encoding) // IN
{
   const char *utf8Str = (const char *)srcBuffer;
   Bool success = FALSE;
   size_t copyBytes = 0;

   encoding = Unicode_ResolveEncoding(encoding);

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
         if (copyBytes >= len) {
            success = TRUE;
         } else {
            if (encoding == STRING_ENCODING_UTF8) {
               copyBytes =
                  CodeSet_Utf8FindCodePointBoundary(destBuffer, copyBytes);
            }
         }

         ((char*)destBuffer)[copyBytes] = '\0';
      }
      break;
   case STRING_ENCODING_UTF16_LE:
      {
         char *utf16Buf;
         size_t utf16BufLen;

         if (!CodeSet_Utf8ToUtf16le(utf8Str,
                                    strlen(utf8Str),
                                    &utf16Buf,
                                    &utf16BufLen)) {
            /* input should be valid UTF-8, no conversion error possible */
            NOT_IMPLEMENTED();
            break;
         }
         copyBytes = MIN(utf16BufLen, maxLengthInBytes - 2);
         memcpy(destBuffer, utf16Buf, copyBytes);
         copyBytes = CodeSet_Utf16FindCodePointBoundary(destBuffer, copyBytes);
         ((utf16_t*)destBuffer)[copyBytes / 2] = 0;
         free(utf16Buf);

         if (copyBytes >= utf16BufLen) {
            success = TRUE;
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
            /* XXX can't distinguish error cause */
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

         if (copyBytes >= currentBufSize) {
            success = TRUE;
         }
      }
      break;
   }

   if (retLength) {
      *retLength = copyBytes;
   }
   return success;
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
 *      NULL if argument is NULL.
 *      Otherwise, pointer to the dynamically allocated memory
 *      or NULL on conversion failure.
 *      The caller is responsible to free the memory allocated
 *      by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
Unicode_GetAllocBytes(const char *str,         // IN:
                      StringEncoding encoding) // IN:
{
   if (str == NULL) {
      return NULL;
   }

   return UnicodeGetAllocBytesInternal(str, encoding, -1, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocBytesWithLength --
 *
 *      Allocates and returns a buffer into which the contents of the unicode
 *      string of the specified length are extracted using the specified
 *      encoding.
 *
 *      NOTE: The buffer returned is always NUL terminated.  The length of
 *            the NUL can depend on the encoding.  UTF-16 NUL is "\0\0";
 *            UTF-32 NUL is "\0\0\0\0".
 *
 *      NULL is returned for NULL argument.
 *
 * Results:
 *      NULL if argument is NULL.
 *      Otherwise, pointer to the dynamically allocated memory
 *      or NULL on conversion failure.
 *      The caller is responsible to free the memory allocated
 *      by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
Unicode_GetAllocBytesWithLength(const char *str,         // IN:
                                StringEncoding encoding, // IN:
                                ssize_t lengthInBytes)   // IN:
{
   if (str == NULL) {
      return NULL;
   }
   ASSERT(lengthInBytes >= 0);

   return UnicodeGetAllocBytesInternal(str, encoding, lengthInBytes, NULL);
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
 *      or NULL on conversion failure.
 *
 *      The length of the result (in bytes, without termination) in retLength.
 *
 * Side effects:
 *      Panic on memory allocation failure.
 *
 *-----------------------------------------------------------------------------
 */

void *
UnicodeGetAllocBytesInternal(const char *ustr,        // IN
                             StringEncoding encoding, // IN
                             ssize_t lengthInBytes,   // IN
                             size_t *retLength)       // OUT: optional
{
   const char *utf8Str = ustr;
   char *result = NULL;

   ASSERT(ustr != NULL);

   encoding = Unicode_ResolveEncoding(encoding);

   if (lengthInBytes == -1) {
      lengthInBytes = Unicode_LengthInBytes(ustr, STRING_ENCODING_UTF8);
   }

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
      if (!UnicodeSanityCheck(utf8Str, lengthInBytes, encoding)) {
         break;
      }
      // fall through
   case STRING_ENCODING_UTF8:
      result = Util_SafeMalloc(lengthInBytes + 1);
      memcpy(result, utf8Str, lengthInBytes + 1);
      if (retLength != NULL) {
         *retLength = lengthInBytes;
      }
      break;

   case STRING_ENCODING_UTF16_LE:
      if (!CodeSet_Utf8ToUtf16le(utf8Str, lengthInBytes, &result, retLength)) {
         /* input should be valid UTF-8, no conversion error possible */
         NOT_IMPLEMENTED();
      }
      break;

   default:
      if (!CodeSet_GenericToGeneric("UTF-8", utf8Str, lengthInBytes,
                                    Unicode_EncodingEnumToName(encoding),
                                    CSGTG_NORMAL,
                                    &result, retLength)) {
         /* XXX can't distinguish error cause */
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

char *
UnicodeAllocStatic(const char *asciiBytes, // IN
                   Bool unescape)          // IN
{
   utf16_t *utf16;
   // Explicitly use int8 so we don't depend on whether char is signed.
   const int8 *byte = (const int8 *)asciiBytes;
   size_t utf16Offset = 0;
   char *result;

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
      VERIFY(*byte > 0);

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

      VERIFY(*byte > 0);

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
         VERIFY(escapedCodePoint <= 0x10FFFF);

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
