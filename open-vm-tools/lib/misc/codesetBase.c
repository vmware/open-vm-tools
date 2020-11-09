/*********************************************************
 * Copyright (C) 2010-2020 VMware, Inc. All rights reserved.
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
 * codesetBase.c --
 *
 *    Character set and encoding conversion functions - unentangled from ICU,
 *    Unicode, codesetOld or other dependencies. Routines here can be used
 *    "anywhere" without fear of linking entanglements.
 */

#include <stdlib.h>
#include "vmware.h"
#include "codeset.h"
#include "util.h"


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GetUtf8 --
 *
 *      Parse the next UTF-8 sequence.
 *
 * Results:
 *      0 on failure.
 *      Length of sequence and Unicode character in *uchar on success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
CodeSet_GetUtf8(const char *string,  // IN: string
                const char *end,     // IN: end of string
                uint32 *uchar)       // OUT/OPT: the Unicode character
{
   uint8 *p = (uint8 *) string;
   uint8 *e;
   uint32 c;
   int len;

   ASSERT(string < end);

   c = *p;

   if (c < 0x80) {
      // ASCII: U+0000 - U+007F: 1 byte of UTF-8.
      len = 1;
      goto out;
   }

   if ((c < 0xC2) || (c > 0xF4)) {
      // 0x81 to 0xBF are not valid first bytes
      // 0xC0 and 0xC1 cannot appear in UTF-8, see below
      // leading char cannot be > 0xF4, illegal as well
      return 0;
   }

   if (c < 0xE0) {
      // U+0080 - U+07FF: 2 bytes of UTF-8.
      c -= 0xC0;
      len = 2;
   } else if (c < 0xF0) {
      // U+0800 - U+FFFF: 3 bytes of UTF-8.
      c -= 0xE0;
      len = 3;
   } else {
      // U+10000 - U+10FFFF: 4 bytes of UTF-8.
      c -= 0xF0;
      len = 4;
   }

   if ((e = p + len) > (uint8 *) end) {
      // input too short
      return 0;
   }

   while (++p < e) {
      if ((*p & 0xC0) != 0x80) {
         // bad trailing byte
         return 0;
      }
      c <<= 6;
      c += *p - 0x80;
   }

   /*
    * Enforce shortest encoding.
    * UTF-8 mandates that shortest possible encoding is used,
    * as otherwise doing UTF-8 => anything => UTF-8 could bypass some
    * important tests, like '/' for path separator or \0 for string
    * termination.
    *
    * This test does not work for len == 2, but that case is handled
    * by requiring the first byte to be 0xC2 or greater (see above).
    */

   if (c < 1U << (len * 5 - 4)) {
      return 0;
   }

out:
   if (uchar != NULL) {
      *uchar = c;
   }

   return len;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_LengthInCodePoints --
 *
 *    Return the length of a UTF8 string in code points (the number of
 *    unicode characters present in the string, not the length of the
 *    string in bytes).
 *
 *    Like strlen, the length returned does not include the terminating NUL.
 *
 * Results:
 *    -1 on error
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
CodeSet_LengthInCodePoints(const char *utf8)  // IN:
{
   char *p;
   char *end;
   uint32 codePoints = 0;

   ASSERT(utf8 != NULL);

   p = (char *) utf8;
   end = p + strlen(utf8);

   while (p < end) {
      uint32 len = CodeSet_GetUtf8(p, end, NULL);

      if (len == 0) {
         return -1;
      }

      p += len;
      codePoints++;
   }

   return codePoints;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_CodePointOffsetToByteOffset --
 *
 *    Return the byte offset of the character at the given codepoint
 *    offset.
 *
 * Results:
 *    -1 on error
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
CodeSet_CodePointOffsetToByteOffset(const char *utf8,    // IN:
                                    int codePointOffset) // IN:
{
   const char *p;
   const char *end;

   ASSERT(utf8 != NULL);

   p = utf8;
   end = p + strlen(utf8);

   while (p < end && codePointOffset > 0) {
      uint32 utf32;
      uint32 len = CodeSet_GetUtf8(p, end, &utf32);

      if (len == 0) {
         return -1;
      }

      p += len;
      codePointOffset--;
   }

   if (codePointOffset == 0) {
      return p - utf8;
   } else {
      return -1;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_UTF8ToUTF32 --
 *
 *    Convert a UTF8 string into a UTF32 string. The result is returned as a
 *    dynamically allocated string that the caller is responsible for.
 *
 * Results:
 *    TRUE   Input string was valid, converted string in *utf32
 *    FALSE  Input string was invalid or internal error
 *
 * Side effects:
 *    Allocates memory
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_UTF8ToUTF32(const char *utf8,  // IN:
                    char **utf32)      // OUT:
{
   char *p;
   char *end;
   uint32 *ptr;
   int codePoints;

   ASSERT(utf32 != NULL);

   if (utf8 == NULL) {  // NULL is not an error
      *utf32 = NULL;

      return TRUE;
   }

   codePoints = CodeSet_LengthInCodePoints(utf8);
   if (codePoints == -1) {
      *utf32 = NULL;

      return FALSE;
   }

   p = (char *) utf8;
   end = p + strlen(utf8);

   ptr = Util_SafeMalloc(sizeof *ptr * (codePoints + 1));
   *utf32 = (char *) ptr;

   while (p < end) {
      p += CodeSet_GetUtf8(p, end, ptr++);
   }

   *ptr = 0;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_UTF32ToUTF8 --
 *
 *    Convert a UTF32 string into a UTF8 string. The result is returned as a
 *    dynamically allocated string that the caller is responsible for.
 *
 * Results:
 *    TRUE   Input string was valid, converted string in *utf8
 *    FALSE  Input string was invalid or internal error
 *
 * Side effects:
 *    Allocates memory
 *
 *-----------------------------------------------------------------------------
 */


Bool
CodeSet_UTF32ToUTF8(const char *utf32,  // IN:
                    char **utf8)        // OUT:
{
   uint32 i;
   uint8 *p;
   uint8 *q;
   uint32 len;
   union {
      uint32  word;
      uint8   bytes[4];
   } value;

   ASSERT(utf8 != NULL);

   if (utf32 == NULL) {  // NULL is not an error
      *utf8 = NULL;

      return TRUE;
   }

   /*
    * Determine the length of the UTF32 string. A UTF32 string terminates
    * with four (4) bytes of zero (0).
    */

   len = 0;
   p = (uint8 *) utf32;

   while (TRUE) {
      value.bytes[0] = *p++;
      value.bytes[1] = *p++;
      value.bytes[2] = *p++;
      value.bytes[3] = *p++;

      if (value.word == 0) {
         break;
      }

      len++;
   }

   /*
    * Now that we know the length, allocate the memory for the UTF8 string.
    * The UTF8 string length calculation ensures that there will always be
    * sufficient space to represent the UTF32 string. Most of the time this
    * will involved allocating too much memory however the memory wastage
    * will be very short lived and very small.
    */

   *utf8 = Util_SafeMalloc((4 * len) + 1);  // cover the NUL byte

   /*
    * Process the UTF32 string, converting each code point into its
    * UTF8 equivalent.
    */

   p = (uint8 *) utf32;
   q = (uint8 *) *utf8;

   for (i = 0; i < len; i++) {
      value.bytes[0] = *p++;
      value.bytes[1] = *p++;
      value.bytes[2] = *p++;
      value.bytes[3] = *p++;

      if (value.word < 0x80) {                      // One byte case (ASCII)
         *q++ = value.word;
      } else if (value.word < 0x800) {              // Two byte case
         *q++ = 0xC0 | (value.word >> 6);
         *q++ = 0x80 | (value.word & 0x3F);
      } else if (value.word < 0x10000) {            // Three byte case
         *q++ = 0xE0 | (value.word >> 12);
         *q++ = 0x80 | ((value.word >> 6) & 0x3F);
         *q++ = 0x80 | (value.word & 0x3F);
      } else if (value.word < 0x110000) {           // Four byte case
         *q++ = 0xF0 | (value.word >> 18);
         *q++ = 0x80 | ((value.word >> 12) & 0x3F);
         *q++ = 0x80 | ((value.word >> 6) & 0x3F);
         *q++ = 0x80 | (value.word & 0x3F);
      } else {  // INVALID VALUE!
         free(*utf8);
         *utf8 = NULL;

         return FALSE;
      }
   }

   *q = '\0';

   return TRUE;
}
