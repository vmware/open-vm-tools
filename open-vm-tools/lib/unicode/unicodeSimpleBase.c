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
 * unicodeSimpleBase.cc --
 *
 *      Simple implementation of unicodeBase.h interface using char *
 *      containing NUL-terminated UTF-8 bytes as the typedef for
 *      Unicode.
 *
 *      This implementation is a short-term shim to start work on the
 *      Unicode development project, and to introduce people to the
 *      lib/unicode interface without requiring them to change all
 *      layers of calling code.
 *
 *      It does not generically handle all encodings; if an encoding
 *      that is not US-ASCII, UTF-8, UTF-16, or UTF-16 LE is passed to
 *      any function, then the process-default encoding is used instead.
 *
 *      Basic Unicode string creation, encoding conversion, and
 *      access to cached UTF-8 and UTF-16 representations.
 *
 *      The thread-safety of ConstUnicode functions is the same as
 *      that for standard const char * functions: multiple threads can
 *      call ConstUnicode functions on the same string simultaneously.
 *
 *      However, a non-const Unicode function (like Unicode_Free) must
 *      not be called concurrently with any other Unicode or
 *      ConstUnicode function on the same string.
 */

#include "vmware.h"

#include "util.h"
#include "codeset.h"
#include "hashTable.h"
#include "str.h"
#include "syncMutex.h"
#include "unicodeBase.h"
#include "unicodeInt.h"
#include "unicodeSimpleUTF16.h"

// Initial number of buckets for the UTF-16 hash table.
static const uint32 UNICODE_UTF16_STRING_TABLE_BUCKETS = 4096;

/*
 * Padding for initial and final bytes used by an encoding.  The value
 * comes from ICU's UCNV_GET_MAX_BYTES_FOR_STRING macro and accounts
 * for leading and trailing bytes and NUL.
 */
static const size_t UNICODE_UTF16_CODE_UNITS_PADDING = 10;

static HashTable *UnicodeUTF16StringTable;
static Atomic_Ptr UnicodeUTF16StringTableLockStorage;


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
      /*
       * XXX TODO: This is not correct.  We don't yet have a
       * cross-platform library that can convert from an arbitrary
       * encoding to UTF-8, so this is a stopgap measure to get us
       * going in the meantime.
       */
      if (lengthInBytes == -1) {
         /*
          * TODO: This doesn't work for UTF-16 BE, UTF-32, and other
          * encodings with embedded NULs.
          */
         lengthInBytes = strlen((const char *)buffer);
      }

      CodeSet_CurrentToUtf8((const char *)buffer,
                            lengthInBytes,
                            &utf8Result,
                            NULL);
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
 *      Deletes the cached UTF-16 representation of the string if it's set.
 *
 *-----------------------------------------------------------------------------
 */

void
Unicode_Free(Unicode str) // IN
{
   char *utf8String = (char *)str;
   SyncMutex *lck;

   lck = SyncMutex_CreateSingleton(&UnicodeUTF16StringTableLockStorage);
   SyncMutex_Lock(lck);

   if (UnicodeUTF16StringTable) {
      HashTable_Delete(UnicodeUTF16StringTable, utf8String);
   }

   SyncMutex_Unlock(lck);

   free(utf8String);
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
 * Unicode_GetUTF16 --
 *
 *      Returns the contents of the string encoded as a NUL-terminated
 *      UTF-16 array in host byte order.
 *
 * Results:
 *      A NUL-terminated UTF-16 string in host byte order; lifetime is
 *      valid until the next non-const Unicode function is called on
 *      the string.  Caller should duplicate if storing the return value
 *      long-term.
 *
 *      Caller does not need to free; the memory is managed inside the
 *      Unicode object.
 *
 * Side effects:
 *      Creates UnicodeUTF16StringTable if it doesn't yet exist.
 *      If the string hasn't yet been converted to UTF-16, converts
 *      the string to UTF-16 and stores the result in
 *      UnicodeUTF16StringTable.
 *
 *-----------------------------------------------------------------------------
 */

const utf16_t *
Unicode_GetUTF16(ConstUnicode str) // IN
{
   const char *utf8Str = (const char *)str;
   utf16_t *utf16Str = NULL;
   SyncMutex *lck;

   lck = SyncMutex_CreateSingleton(&UnicodeUTF16StringTableLockStorage);
   SyncMutex_Lock(lck);

   if (!UnicodeUTF16StringTable) {
      /*
       * We use HASH_INT_KEY so we can hash on the pointer value
       * rather than the contents of the string.  (This lets us simply
       * delete the hashtable entry in Unicode_Free() without affecting
       * other strings with the same content).
       *
       * If we want to consolidate all UTF-16 representations of
       * identical strings that might have different pointer values,
       * we can use a struct rather than storing utf16Str in the hash
       * directly, and reference count the UTF-16 representations,
       * freeing when the last reference is gone.
       */
      UnicodeUTF16StringTable = 
                             HashTable_Alloc(UNICODE_UTF16_STRING_TABLE_BUCKETS,
                                             HASH_INT_KEY,
                                             (HashTableFreeEntryFn)free);
   }

   /*
    * TODO: In the current implementation (where Unicode is char *),
    * we have no way of ensuring the caller doesn't manipulate the
    * bytes in the input value after creating it.
    *
    * Until we make the type opaque, we must recreate the UTF-16 cache
    * each time it's asked for.
    */
   HashTable_Delete(UnicodeUTF16StringTable, utf8Str);

   if (CodeSet_Utf8ToUtf16le(utf8Str,
                             strlen(utf8Str),
                             (char **)&utf16Str,
                             NULL)) {
      HashTable_Insert(UnicodeUTF16StringTable, utf8Str, utf16Str);
   }

   SyncMutex_Unlock(lck);

   return utf16Str;
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
   const utf16_t *utf16;
   size_t maxUTF16CodeUnitSize;

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
   case STRING_ENCODING_ISO_8859_1:
   case STRING_ENCODING_WINDOWS_1252:
      // TODO: Lots more encodings can be added here.
      maxUTF16CodeUnitSize = 1;
      break;
   case STRING_ENCODING_UTF8:
      /*
       * One UTF-16 code unit takes 3 bytes of UTF-8.  Unicode code
       * points > U+FFFF are encoded in two UTF-16 code units, and 4
       * bytes of UTF-8.  2*3 = 6 > 4, which gives us more than enough
       * space to encode such a code point.
       */
      maxUTF16CodeUnitSize = 3;
      break;
   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
   case STRING_ENCODING_UTF16_BE:
      maxUTF16CodeUnitSize = 2;
      break;
   case STRING_ENCODING_UTF32:
   case STRING_ENCODING_UTF32_LE:
   case STRING_ENCODING_UTF32_BE:
      maxUTF16CodeUnitSize = 4;
      break;
   default:
      /*
       * Assume the worst: ISO-2022-JP takes up to 7 bytes per UTF-16
       * code unit.
       */
      maxUTF16CodeUnitSize = 7;
      break;
   }

   // Accounts for leading and trailing bytes and NUL.
   utf16 = Unicode_GetUTF16(str);
   return (Unicode_UTF16Strlen(utf16) + UNICODE_UTF16_CODE_UNITS_PADDING) *
           maxUTF16CodeUnitSize;
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
 *      Returns -1 if the Unicode string requires more than
 *      maxLengthInBytes bytes to be encoded in the specified
 *      encoding, including NUL termination.  (Call
 *      Unicode_BytesRequired(str, encoding) to get the correct length.)
 *
 *      Otherwise, returns the number of bytes written to buffer, not
 *      including the NUL termination.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

ssize_t
Unicode_CopyBytes(ConstUnicode str,        // IN
                  void *buffer,            // OUT
                  size_t maxLengthInBytes, // IN
                  StringEncoding encoding) // IN
{
   const char *utf8Str = (const char *)str;
   ssize_t ret = -1;

   switch (encoding) {
   case STRING_ENCODING_US_ASCII:
   case STRING_ENCODING_UTF8:
      Str_Strcpy((char *) buffer, utf8Str, maxLengthInBytes);
      ret = strlen((const char *)buffer);
      break;
   case STRING_ENCODING_UTF16:
   case STRING_ENCODING_UTF16_LE:
      {
         size_t bytesNeeded;
         const utf16_t *utf16;
         UnicodeIndex length;

         utf16 = Unicode_GetUTF16(str);
         length = Unicode_UTF16Strlen(utf16);
         // Add 1 for NUL.
         bytesNeeded = (length + 1) * 2;

         if (bytesNeeded <= maxLengthInBytes) {
            memcpy(buffer, utf16, bytesNeeded);
            ret = bytesNeeded;
         }
         break;
      }
   default:
      {
         // XXX TODO: This is not correct; it's just a stopgap measure.
         char *currentBuf;
         size_t currentBufSize;

         if (CodeSet_Utf8ToCurrent(utf8Str,
                                   strlen(utf8Str),
                                   &currentBuf,
                                   &currentBufSize)) {
            if (currentBufSize < maxLengthInBytes) {
               // TODO: NUL is not necessarily 1 byte in all encodings.
               memcpy(buffer, currentBuf, currentBufSize + 1);
               ret = currentBufSize;
               free(currentBuf);
            }
         } else {
            // XXX: What to do here?
         }
      }
   }

   return ret;
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
