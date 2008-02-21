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
 * unicodeSimpleTypes.c --
 *
 *      Basic types and cache handling for simple UTF-8 implementation
 *      of Unicode library interface.
 */

#include <ctype.h>
#include <string.h>

#include "unicodeBase.h"
#include "unicodeInt.h"

#include "vm_assert.h"
#include "util.h"

static char *UnicodeNormalizeEncodingName(const char *encoding);

/*
 * Table generated from selected encodings supported by ICU:
 *
 * http://source.icu-project.org/repos/icu/icu/trunk/source/data/mappings/convrtrs.txt
 *
 * If you update this, you must keep the StringEncoding enum in
 * lib/public/unicodeTypes.h in sync!
 */
static const char *SIMPLE_ENCODING_LIST[STRING_ENCODING_MAX_SPECIFIED] = {
   /*
    * Byte string encodings that support all characters in Unicode.
    */
   "UTF-8",    // STRING_ENCODING_UTF8
   "UTF-16",   // STRING_ENCODING_UTF16
   "UTF-16LE", // STRING_ENCODING_UTF16_LE
   "UTF-16BE", // STRING_ENCODING_UTF16_BE

   "UTF-32",   // STRING_ENCODING_UTF32
   "UTF-32LE", // STRING_ENCODING_UTF32_LE
   "UTF-32BE", // STRING_ENCODING_UTF32_BE

   /*
    * Legacy byte string encodings that only support a subset of Unicode.
    */

   /*
    * Latin encodings
    */
   "US-ASCII",   // STRING_ENCODING_US_ASCII
   "ISO-8859-1", // STRING_ENCODING_ISO_8859_1
   "ISO-8859-2", // STRING_ENCODING_ISO_8859_2
   "ISO-8859-3", // STRING_ENCODING_ISO_8859_3
   "ISO-8859-4", // STRING_ENCODING_ISO_8859_4
   "ISO-8859-5", // STRING_ENCODING_ISO_8859_5
   "ISO-8859-6", // STRING_ENCODING_ISO_8859_6
   "ISO-8859-7", // STRING_ENCODING_ISO_8859_7
   "ISO-8859-8", // STRING_ENCODING_ISO_8859_8
   "ISO-8859-9", // STRING_ENCODING_ISO_8859_9
   "ISO-8859-10", // STRING_ENCODING_ISO_8859_10
   "ISO-8859-11", // STRING_ENCODING_ISO_8859_11
                  // Oddly, there is no ISO-8859-12.
   "ISO-8859-13", // STRING_ENCODING_ISO_8859_13
   "ISO-8859-14", // STRING_ENCODING_ISO_8859_14
   "ISO-8859-15", // STRING_ENCODING_ISO_8859_15

   /*
    * Chinese encodings
    */
   "GB18030",     // STRING_ENCODING_GB_18030
   "Big5",        // STRING_ENCODING_BIG_5
   "Big5-HKSCS",  // STRING_ENCODING_BIG_5_HK
   "GBK",         // STRING_ENCODING_GBK
   "GB2312",      // STRING_ENCODING_GB_2312
   "ISO-2022-CN", // STRING_ENCODING_ISO_2022_CN

   /*
    * Japanese encodings
    */
   "Shift_JIS",     // STRING_ENCODING_SHIFT_JIS
   "EUC-JP",        // STRING_ENCODING_EUC_JP
   "ISO-2022-JP",   // STRING_ENCODING_ISO_2022_JP
   "ISO-2022-JP-1", // STRING_ENCODING_ISO_2022_JP_1
   "ISO-2022-JP-2", // STRING_ENCODING_ISO_2022_JP_2

   /*
    * Korean encodings
    */
   "EUC-KR",      // STRING_ENCODING_EUC_KR
   "ISO-2022-KR", // STRING_ENCODING_ISO_2022_KR

   /*
    * Windows encodings
    */
   "windows-1250", // STRING_ENCODING_WINDOWS_1250
   "windows-1251", // STRING_ENCODING_WINDOWS_1251
   "windows-1252", // STRING_ENCODING_WINDOWS_1252
   "windows-1253", // STRING_ENCODING_WINDOWS_1253
   "windows-1254", // STRING_ENCODING_WINDOWS_1254
   "windows-1255", // STRING_ENCODING_WINDOWS_1255
   "windows-1256", // STRING_ENCODING_WINDOWS_1256
   "windows-1257", // STRING_ENCODING_WINDOWS_1257
   "windows-1258", // STRING_ENCODING_WINDOWS_1258
};


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeNormalizeEncodingName --
 *
 *      Normalizes a US-ASCII encoding name by discarding all
 *      non-alphanumeric characters and converting to lower-case.
 *
 * Results:
 *      The allocated, normalized encoding name in NUL-terminated
 *      US-ASCII bytes.  Caller must free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
UnicodeNormalizeEncodingName(const char *encodingName) // IN
{
   char *result;
   char *currentResult;

   ASSERT(encodingName);

   result = Util_SafeMalloc(strlen(encodingName) + 1);
   currentResult = result;

   for (currentResult = result; *encodingName != '\0'; encodingName++) {
      if (isalnum((int) *encodingName)) {
         *currentResult = tolower(*encodingName);
         currentResult++;
      }
   }

   *currentResult = '\0';

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_EncodingEnumToName --
 *
 *      Converts a StringEncoding enum value to the equivalent
 *      encoding name.
 *
 * Results:
 *      A NUL-terminated US-ASCII string containing the name of the
 *      encoding.  Encodings follow the preferred MIME encoding name
 *      from IANA's Character Sets standard.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
Unicode_EncodingEnumToName(StringEncoding encoding) // IN
{
   // STRING_ENCODING_UNKNOWN shouldn't be hit at runtime.
   ASSERT(encoding != STRING_ENCODING_UNKNOWN);

   switch (encoding) {
   case STRING_ENCODING_UNKNOWN:
      /*
       * We will only get here in non-debug builds.  Fall through to
       * the process-default encoding name.
       */
   case STRING_ENCODING_DEFAULT:
      /*
       * XXX TODO: Need CodeSetGetCurrentCodeSet() from lib/misc/codeset.c,
       * and need to extend with Win32 implementation.
       */
      return "UTF-8";
      break;
   default:
      ASSERT(encoding >= STRING_ENCODING_FIRST);
      ASSERT(encoding < STRING_ENCODING_MAX_SPECIFIED);

      if (   encoding < STRING_ENCODING_FIRST
          || encoding >= STRING_ENCODING_MAX_SPECIFIED) {
         const char *defaultEncoding = "UTF-8";
         Log("%s: Unknown encoding %d, returning default (%s).",
             __FUNCTION__, encoding, defaultEncoding);
         return defaultEncoding;
      }

      return SIMPLE_ENCODING_LIST[encoding];
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_EncodingNameToEnum --
 *
 *      Converts a NUL-terminated US-ASCII string encoding name
 *      to the equivalent enum.
 *
 * Results:
 *      The StringEncoding enum value corresponding to the name, or
 *      STRING_ENCODING_UNKNOWN if the encoding name wasn't found.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

StringEncoding
Unicode_EncodingNameToEnum(const char *encodingName) // IN
{
   StringEncoding result = STRING_ENCODING_UNKNOWN;
   char *normalizedEncodingName;
   size_t encoding;

   normalizedEncodingName = UnicodeNormalizeEncodingName(encodingName);

   for (encoding = STRING_ENCODING_FIRST;
        encoding < STRING_ENCODING_MAX_SPECIFIED;
        encoding++) {
      char *normalizedEncodingListEntry;

      ASSERT(SIMPLE_ENCODING_LIST[encoding]);

      /*
       * TODO: We can optimize this by pulling this code out into
       * a once-on-startup initialization, where we generate and
       * cache the normalized encoding name list upon first use.
       */
      normalizedEncodingListEntry =
         UnicodeNormalizeEncodingName(SIMPLE_ENCODING_LIST[encoding]);

      if (0 == strcmp(normalizedEncodingName, normalizedEncodingListEntry)) {
         result = (StringEncoding)encoding;
      }

      free(normalizedEncodingListEntry);

      if (result != STRING_ENCODING_UNKNOWN) {
         break;
      }
   }

   free(normalizedEncodingName);

   return result;
}
