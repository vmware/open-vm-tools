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
 * unicodeTypes.h --
 *
 *      Types used throughout the Unicode library.
 */

#ifndef _UNICODE_TYPES_H_
#define _UNICODE_TYPES_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMNIXMOD
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined(SUPPORT_UNICODE_OPAQUE)

/*
 * To assist with finding code that hasn't been internationalized, we
 * support building with an opaque Unicode type.  This catches users
 * passing a char * of unknown encoding to Unicode functions, and
 * assigning a Unicode to a char *.
 */

typedef struct UnicodeImpl UnicodeImpl;
typedef UnicodeImpl * Unicode;
typedef const UnicodeImpl * ConstUnicode;

#else

/*
 * As a transitionary development tactic to prevent code churn while the
 * Unicode libraries are being developed, we'll start with a simple
 * implementation of Unicode as UTF-8 char *.
 */
typedef char * Unicode;
typedef const char * ConstUnicode;

#endif

typedef ssize_t UnicodeIndex;

/*
 * Special UnicodeIndex value returned when a string is not found.
 */
enum {
   UNICODE_INDEX_NOT_FOUND = -1
};

/*
 * Encodings passed to convert encoded byte strings to and from
 * Unicode.
 *
 * Keep this enum synchronized with ICU_ENCODING_LIST in unicodeICU.cc!
 */
typedef enum {
   STRING_ENCODING_FIRST,

   /*
    * Byte string encodings that support all characters in Unicode.
    *
    * If you don't know what to use for bytes in a new system, use
    * STRING_ENCODING_UTF8.
    */
   STRING_ENCODING_UTF8 = STRING_ENCODING_FIRST,

   STRING_ENCODING_UTF16,    // Host-endian UTF-16.
   STRING_ENCODING_UTF16_LE, // Little-endian UTF-16.
   STRING_ENCODING_UTF16_BE, // Big-endian UTF-16.

   STRING_ENCODING_UTF32,    // Host-endian UTF-32.
   STRING_ENCODING_UTF32_LE, // Little-endian UTF-32.
   STRING_ENCODING_UTF32_BE, // Big-endian UTF-32.

   /*
    * Legacy byte string encodings that only support a subset of Unicode.
    */

   /*
    * Latin encodings
    */
   STRING_ENCODING_US_ASCII,
   STRING_ENCODING_ISO_8859_1,
   STRING_ENCODING_ISO_8859_2,
   STRING_ENCODING_ISO_8859_3,
   STRING_ENCODING_ISO_8859_4,
   STRING_ENCODING_ISO_8859_5,
   STRING_ENCODING_ISO_8859_6,
   STRING_ENCODING_ISO_8859_7,
   STRING_ENCODING_ISO_8859_8,
   STRING_ENCODING_ISO_8859_9,
   STRING_ENCODING_ISO_8859_10,
   // ISO-8859-11 is unused.
   // Oddly, there is no ISO-8859-12.
   STRING_ENCODING_ISO_8859_13,
   STRING_ENCODING_ISO_8859_14,
   STRING_ENCODING_ISO_8859_15,

   /*
    * Chinese encodings
    */
   STRING_ENCODING_GB_18030,
   STRING_ENCODING_BIG_5,
   STRING_ENCODING_BIG_5_HK,
   STRING_ENCODING_GBK,
   STRING_ENCODING_GB_2312,
   STRING_ENCODING_ISO_2022_CN,

   /*
    * Japanese encodings
    */
   STRING_ENCODING_SHIFT_JIS,
   STRING_ENCODING_EUC_JP,
   STRING_ENCODING_ISO_2022_JP,
   STRING_ENCODING_ISO_2022_JP_1,
   STRING_ENCODING_ISO_2022_JP_2,

   /*
    * Korean encodings
    */
   STRING_ENCODING_EUC_KR,
   STRING_ENCODING_ISO_2022_KR,

   /*
    * Windows encodings
    */
   STRING_ENCODING_WINDOWS_1250,
   STRING_ENCODING_WINDOWS_1251,
   STRING_ENCODING_WINDOWS_1252,
   STRING_ENCODING_WINDOWS_1253,
   STRING_ENCODING_WINDOWS_1254,
   STRING_ENCODING_WINDOWS_1255,
   STRING_ENCODING_WINDOWS_1256,
   STRING_ENCODING_WINDOWS_1257,
   STRING_ENCODING_WINDOWS_1258,

   // Add more encodings here.

   // Sentinel value after the last explicitly specified encoding.
   STRING_ENCODING_MAX_SPECIFIED,

   /*
    * The environment-specified "default" encoding for this process.
    */
   STRING_ENCODING_DEFAULT = -1,
   STRING_ENCODING_UNKNOWN = -2,
} StringEncoding;

const char *Unicode_EncodingEnumToName(StringEncoding encoding);
StringEncoding Unicode_EncodingNameToEnum(const char *encodingName);
StringEncoding Unicode_GetCurrentEncoding(void);
Bool Unicode_IsEncodingSupported(StringEncoding encoding);

#endif // _UNICODE_TYPES_H_
