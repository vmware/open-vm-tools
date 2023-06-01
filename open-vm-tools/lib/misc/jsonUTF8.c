/*********************************************************
 * Copyright (c) 2020-2022 VMware, Inc. All rights reserved.
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

#include <stdlib.h>
#include "vmware.h"
#include "codeset.h"
#include "vm_ctype.h"
#include "dynbuf.h"
#include "strutil.h"
#include "unicodeBase.h"


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_JsonEscape --
 *
 *      Escape a unicode string following JSON rules.
 *
 *      From https://www.rfc-editor.org/rfc/rfc8259.html#section-7:
 *
 *      ... All Unicode characters may be placed within the
 *      quotation marks, except for the characters that MUST be escaped:
 *      quotation mark, reverse solidus, and the control characters (U+0000
 *      through U+001F).
 *
 *      ... If the character is in the Basic
 *      Multilingual Plane (U+0000 through U+FFFF), then it may be
 *      represented as a six-character sequence: a reverse solidus, followed
 *      by the lowercase letter u, followed by four hexadecimal digits that
 *      encode the character's code point....
 *
 *      Alternatively, there are two-character sequence escape
 *      representations of some popular characters.  So, for example, a
 *      string containing only a single reverse solidus character may be
 *      represented more compactly as "\\"
 *
 *      ...
 *
 *                  %x62 /          ; b    backspace       U+0008
 *                  %x66 /          ; f    form feed       U+000C
 *                  %x6E /          ; n    line feed       U+000A
 *                  %x72 /          ; r    carriage return U+000D
 *                  %x74 /          ; t    tab             U+0009
 *
 * Results:
 *      NULL Failure!
 *     !NULL Success! The escaped string. The caller is responsible to free
 *                    this.
 *
 * Side effects:
 *      Memory is allocated
 *
 *-----------------------------------------------------------------------------
 */

char *
CodeSet_JsonEscape(const char *utf8)                   // IN:
{
   DynBuf b;
   char *res;
   const char *p;
   const char *end;
   Bool success = TRUE;

   ASSERT(utf8 != NULL);
   if (utf8 == NULL) {
      return NULL;
   }

   DynBuf_Init(&b);

   p = utf8;
   end = p + strlen(utf8);

   while (p < end) {
      uint32 len = CodeSet_GetUtf8(p, end, NULL);

      if (len == 0) {
         success = FALSE;
         break;
      }

      if (len > 1 || (*p > 0x001F && *p != '"' && *p != '\\')) {
         DynBuf_SafeAppend(&b, p, len);
      } else {
         DynBuf_SafeAppend(&b, "\\", 1);
         switch (*p) {
         case '"':
         case '\\':
            DynBuf_SafeAppend(&b, p, 1);
            break;
         case '\b':
            DynBuf_SafeAppend(&b, "b", 1);
            break;
         case '\f':
            DynBuf_SafeAppend(&b, "f", 1);
            break;
         case '\n':
            DynBuf_SafeAppend(&b, "n", 1);
            break;
         case '\r':
            DynBuf_SafeAppend(&b, "r", 1);
            break;
         case '\t':
            DynBuf_SafeAppend(&b, "t", 1);
            break;
         default:
            StrUtil_SafeDynBufPrintf(&b, "u%04x", *p);
            break;
         }
      }
      p += len;
   }

   if (success) {
      res = DynBuf_DetachString(&b);
   } else {
      res = NULL;
   }

   DynBuf_Destroy(&b);

   return res;
}


/* Constants used by json unescape routines. */

/* Number of hex digits in a "\u" escape sequence. */
#define JSON_UESC_NDIGITS 4

/*
 * Maximum number of UTF-8 code units (bytes) per Unicode code point.
 * From bora/lib/unicode/unicode/utf8.h.
 */
#ifndef U8_MAX_LENGTH
#define U8_MAX_LENGTH 4
#endif

/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_JsonGetHex --
 *
 *      Retrieve and convert to an integer the four hex digits that are
 *      part of the six character escape sequence that starts with "\u".
 *
 *      On entry, p points to the first code point following "\u."
 *
 * Results:
 *      TRUE on success, with *value set to the integer value.
 *
 *      FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
CodeSet_JSonGetHex(const char *p,       // IN:
                   const char *end,     // IN:
                   int32 *value)        // OUT:
{
   char hexBuf[JSON_UESC_NDIGITS + 1];   /* +1 for NUL */
   int numHexDigits = 0;

   ASSERT(p <= end);

   /*
    * Assumes called with p set to first code point following "\u" and looks
    * for four hex digits.   No need to call CodeSet_GetUtf8 to verify that
    * the code point length of these characters is one since it's always on
    * a code point boundary and it's OK to check directly for specific
    * ASCII characters in such a case, and if there's a match to an ASCII
    * character then advancing the pointer by a single character will advance
    * to the next code point.
    */
   while (numHexDigits < JSON_UESC_NDIGITS) {
      if (p >= end || !CType_IsXDigit(*p)) {
         return FALSE;
      }
      hexBuf[numHexDigits++] = *p++;
   }

   hexBuf[numHexDigits] = '\0';
   *value = strtol(hexBuf, NULL, 16);
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_JsonUnescapeU --
 *
 *      Handle a JSON escape sequence beginning with "\u", consisting either
 *      of:
 *         (1) "\u" followed by four hex digits; or
 *         (2) two such consecutive sequences encoding a character
 *             outside the Basic MultiLingual Plane as a UTF-16
 *             surrogate pair.
 *
 *      Note "\u0000" is not allowed and is considered an error if
 *      encountered.
 *
 *      On entry to the routine, p should be pointing at the backslash
 *      character that starts the (possible) escape sequence.
 *
 *      outBuf is the base of a char array of size >= U8_MAX_LENGTH + 1, i.e.,
 *      large enough to hold a NUL-terminated UTF-8 encoding of any Unicode
 *      code point.
 *
 * Results:
 *      On success, the length of the escape sequence, with the unescaped
 *      result plus a NUL terminator in outBuf.
 *
 *      0 on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
CodeSet_JsonUnescapeU(const char *p,        // IN:
                      const char *end,      // IN:
                      char *outBuf)         // OUT:
{
   uint32 w;
   uint32 utf32Buf[2];  /* code point value plus 0 terminator */
   char *utf8String;
   uint32 len;
   const char *start = p;

   /*
    * Assumes called only if starts with "\u".  No need to call
    * CodeSet_GetUtf8 in this ASSERT since this is checking for specific ASCII
    * characters - see comment preceding ASSERT in CodeSet_JsonUnescapeOne
    * below.
    */
   ASSERT(p < end && *p == '\\');
   ASSERT(&p[1] < end && p[1] == 'u');

   /* Code point of 0 ("\u0000") not allowed. */
   if (!CodeSet_JSonGetHex(&p[2], end, &w) || w == 0) {
      return 0;
   }

   /* Advance p past "\u" and the hex digits that follow. */
   p += 2 + JSON_UESC_NDIGITS;

   /* If the value is a leading surrogate, then handle the trailing one. */
   if (U16_IS_LEAD(w)) {
      uint32 trail;

      /*
       * Check for '\', 'u', and four digits representing a trailer.  As
       * elsewhere, no need to call CodeSet_GetUtf8 since this is checking for
       * specific ASCII characters, and bails out if any of the checks fail.
       */
      if (p < end && *p++ == '\\' && p < end && *p++ == 'u' &&
          CodeSet_JSonGetHex(p, end, &trail) && U16_IS_TRAIL(trail)) {
         w = U16_GET_SUPPLEMENTARY(w, trail);

         /* Advance p past the digits that follow "\u". */
         p += JSON_UESC_NDIGITS;
      } else {
         return 0;
      }
   } else if (U16_IS_TRAIL(w)) {
      return 0;
   }

   /*
    * To get the UTF-8 for this code point, create a UTF-32 string
    * and convert to UTF-8.
    */
   utf32Buf[0] = w;
   utf32Buf[1] = 0;   /* needs a 4-byte 0 terminator */

   if (!CodeSet_UTF32ToUTF8((char *)utf32Buf, &utf8String)) {
      return 0;
   }

   len = strlen(utf8String);
   ASSERT(Unicode_IsBufferValid(utf8String, len, STRING_ENCODING_UTF8));
   ASSERT(len <= U8_MAX_LENGTH);
   memcpy(outBuf, utf8String, len + 1);

   free(utf8String);
   return p - start;
}


/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_JsonUnescapeOne --
 *
 *      Handle a single JSON escape sequence.
 *
 *      On entry to the routine, p should be pointing at the backslash
 *      character that starts the (possible) escape sequence.
 *
 *      outBuf is the base of a char array of size >= U8_MAX_LENGTH + 1, i.e.,
 *      large enough to hold a NUL-terminated UTF-8 encoding of any Unicode
 *      code point.
 *
 * Results:
 *      On success, the length of the escape sequence, with the unescaped
 *      result plus a NUL terminator in outBuf.
 *
 *      0 on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
CodeSet_JsonUnescapeOne(const char *p,        // IN:
                        const char *end,      // IN:
                        char *outBuf)         // OUT:
{
   int len = 0;
   const char *start = p;

   /*
    * Assumes called only if first character is '\'.  Note that in the
    * ASSERT it's not necessary to call CodeSet_GetUtf8 to verify the
    * code point length is 1.  Since this is on a code point boundary,
    * if the byte matches a specific ASCII character (in this case, '\')
    * that is sufficient to verify the code point length of 1.
    */
   ASSERT(p < end && *p == '\\');

   /*
    * Advance p by a single char to get to the next code point since it's
    * known to be an ASCII character (i.e., '\') and therefore code point
    * length is 1.
    */
   if (++p < end) {
      /*
       * Preset len and outBuf for common case of valid two-character escape
       * sequence with one-character output; different values will be assigned
       * if the sequence turns out to start with "\u"  or is invalid.
       */
      len = 2;
      outBuf[1] = '\0';

      /*
       * As above, since this on a code point boundary and checking whether
       * it matches specific ASCII characters, it's not necessary to call
       * CodeSet_GetUtf8 to verify that the code point length is 1.  In the
       * event *p is the first byte of a multi-byte UTF-8 code point, we'll
       * end up in the default case of the switch and fail.
       */
      switch (*p) {
      case '\"':
      case '\\':
      case '/':
         outBuf[0] = *p;
         break;
      case 'b':
         outBuf[0] = '\b';
         break;
      case 'f':
         outBuf[0] = '\f';
         break;
      case 'r':
         outBuf[0] = '\r';
         break;
      case 'n':
         outBuf[0] = '\n';
         break;
      case 't':
         outBuf[0] = '\t';
         break;
      case 'u':
         len = CodeSet_JsonUnescapeU(start, end, outBuf);
         break;
      default:
         len = 0;
         break;
      }
   }
   return len;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_JsonUnescape --
 *
 *      Copy a UTF8 string, reverting any JSON escape sequences found within
 *      the string according to the STD-90 spec at
 *      https://tools.ietf.org/html/std90.  This processes the same
 *      escape sequences that are allowed by the jsmn parser, and generally
 *      tries to follow the same logic as the jsmn escape parsing.  Any
 *      strings passed in to this routine have likely been through jsmn, and
 *      any invalid escape sequences should have been rejected.  However, this
 *      routine and those it calls still check for the possibility of
 *      invalid escape sequences and return failure when running into one, as
 *      opposed to assuming and/or asserting they are valid.
 *
 *      A general unescape routine is difficult to do, so the logic here is
 *      specific to JSON (as opposed to CodeSet_JsonEscape, which relies on
 *      the more general CodeSet_Utf8Escape).
 *
 * Results:
 *      NULL Failure!
 *     !NULL Success! The un-escaped string. The caller is responsible to free
 *                    this.
 *
 * Side effects:
 *      Returns a dynamically allocated string that must be freed by the
 *      caller.
 *
 *-----------------------------------------------------------------------------
 */

char *
CodeSet_JsonUnescape(const char *utf8)   // IN:
{
   DynBuf b;
   char *res;
   const char *p;
   const char *end;
   Bool success = TRUE;

   ASSERT(utf8 != NULL);

   DynBuf_Init(&b);
   p = utf8;
   end = p + strlen(p);

   while (p < end && success) {
      char unescaped[U8_MAX_LENGTH + 1];  /* +1 for NUL */
      uint32 len = CodeSet_GetUtf8(p, end, NULL);

      if (len == 0) {
         success = FALSE;
      } else if (len > 1 || *p != '\\') {
         DynBuf_SafeAppend(&b, p, len);
      } else if ((len = CodeSet_JsonUnescapeOne(p, end, unescaped)) != 0) {
         DynBuf_SafeAppend(&b, unescaped, strlen(unescaped));
      } else {
         success = FALSE;
      }
      p += len;
   }

   if (success) {
      res = DynBuf_DetachString(&b);
   } else {
      res = NULL;
   }

   DynBuf_Destroy(&b);

   return res;
}
