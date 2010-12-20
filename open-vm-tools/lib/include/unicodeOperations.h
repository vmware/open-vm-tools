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
 * unicodeOperations.h --
 *
 *      Basic Unicode string operations.
 *
 *      UnicodeIndex index and length arguments are in terms of code
 *      units, not characters.  The size of a code unit depends on the
 *      implementation (one byte for UTF-8, one 16-bit word for
 *      UTF-16).  Do not store these values on disk, modify them, or
 *      do arithmetic operations on them.
 *
 *      Instead of iterating over the code units in a string to do
 *      character operations, use the library functions provided to
 *      search and transform strings.
 *
 *      If the functionality you need is not present, email the
 *      i18n-dev mailing list.
 */

#ifndef _UNICODE_OPERATIONS_H_
#define _UNICODE_OPERATIONS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include <string.h>

#include "unicodeBase.h"
#include "vm_assert.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Primitive operations.  All other Unicode operations are implemented
 * in terms of these.
 *
 * Pass -1 for any length parameter to indicate "from start until end
 * of string".
 */

int Unicode_CompareRange(ConstUnicode str1,
                         UnicodeIndex str1Start,
                         UnicodeIndex str1Length,
                         ConstUnicode str2,
                         UnicodeIndex str2Start,
                         UnicodeIndex str2Length,
                         Bool ignoreCase);

UnicodeIndex Unicode_FindSubstrInRange(ConstUnicode str,
                                       UnicodeIndex strStart,
                                       UnicodeIndex strLength,
                                       ConstUnicode strToFind,
                                       UnicodeIndex strToFindStart,
                                       UnicodeIndex strToFindLength);

UnicodeIndex Unicode_FindLastSubstrInRange(ConstUnicode str,
                                           UnicodeIndex strStart,
                                           UnicodeIndex strLength,
                                           ConstUnicode strToFind,
                                           UnicodeIndex strToFindStart,
                                           UnicodeIndex strToFindLength);
Unicode Unicode_Substr(ConstUnicode str,
                       UnicodeIndex start,
                       UnicodeIndex length);

Unicode Unicode_ReplaceRange(ConstUnicode destination,
                             UnicodeIndex destinationStart,
                             UnicodeIndex destinationLength,
                             ConstUnicode source,
                             UnicodeIndex sourceStart,
                             UnicodeIndex sourceLength);

Unicode Unicode_Join(ConstUnicode first,
                     ...);

Unicode Unicode_Format(const char *fmt, ...);

UnicodeIndex Unicode_LengthInCodePoints(ConstUnicode str);

/*
 * Simple in-line functions that may be used below.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsIndexAtCodePointBoundary --
 *
 *      Check a string index (in bytes) for code point boundary.
 *
 *	The index must be valid (>= 0 and <= string length).
 *	The end of the string is considered a valid boundary.
 *
 * Results:
 *      TRUE if index is at a code point boundary.
 *
 * Side effects:
 *      Panic if index is not valid.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Unicode_IsIndexAtCodePointBoundary(ConstUnicode str,    // IN:
                                   UnicodeIndex index)  // IN:
{
   ASSERT(index >= 0 && index <= Unicode_LengthInCodeUnits(str));

#ifdef SUPPORT_UNICODE_OPAQUE
   NOT_IMPLEMENTED();
#else
   return (str[index] & 0xc0) != 0x80;
#endif
}


/*
 * Other operations, each based upon calls to primitives.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Append --
 *
 *      Allocates and returns a new string containing 'destination'
 *      followed by 'source'.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_Append(ConstUnicode destination, // IN
               ConstUnicode source)      // IN
{
   return Unicode_ReplaceRange(destination,
                               -1,
                               0,
                               source,
                               0,
                               -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AppendRange --
 *
 *      Allocates and returns a new string containing 'destination'
 *      followed by the specified range of 'source'.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_AppendRange(ConstUnicode dest,       // IN:
                    ConstUnicode src,        // IN:
                    UnicodeIndex srcStart,   // IN:
                    UnicodeIndex srcLength)  // IN:
{
   return Unicode_ReplaceRange(dest,
                               Unicode_LengthInCodePoints(dest),
                               0,
                               src,
                               srcStart,
                               srcLength);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Compare --
 *
 *      Compares two Unicode strings for canonical equivalence in code
 *      point order.
 *
 *      If the result is to be visible in a user interface, use
 *      Unicode_CompareWithLocale to support language and
 *      culture-specific comparison and sorting rules.
 *
 * Results:
 *      -1 if str1 < str2, 0 if str1 == str2, 1 if str1 > str2.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
Unicode_Compare(ConstUnicode str1, // IN
                ConstUnicode str2) // IN
{
   return Unicode_CompareRange(str1,
                               0,
                               -1,
                               str2,
                               0,
                               -1,
                               FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_CompareIgnoreCase --
 *
 *      Compares two Unicode strings for case-insensitive canonical
 *      equivalence in code point order.
 *
 *      If the result is to be visible in a user interface, use
 *      Unicode_CompareWithLocale to support language and
 *      culture-specific comparison and sorting rules.
 *
 * Results:
 *      -1 if str1 < str2, 0 if str1 == str2, 1 if str1 > str2.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE int
Unicode_CompareIgnoreCase(ConstUnicode str1, // IN
                          ConstUnicode str2) // IN
{
   return Unicode_CompareRange(str1,
                               0,
                               -1,
                               str2,
                               0,
                               -1,
                               TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeEndsWith --
 * Unicode_EndsWith --
 * Unicode_EndsWithIgnoreCase --
 *
 *      Tests if 'str' ends with 'suffix'.
 *
 * Results:
 *      TRUE if 'str' ends with 'suffix', FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
UnicodeEndsWith(ConstUnicode str,     // IN:
                ConstUnicode suffix,  // IN:
                Bool ignoreCase)      // IN:
{
   UnicodeIndex strLength = Unicode_LengthInCodePoints(str);
   UnicodeIndex suffixLength = Unicode_LengthInCodePoints(suffix);
   UnicodeIndex offset = strLength - suffixLength;

   if (suffixLength > strLength) {
      return FALSE;
   }

   return Unicode_CompareRange(str,
                               offset,
                               suffixLength,
                               suffix,
                               0,
                               suffixLength,
                               ignoreCase) == 0;
}


static INLINE Bool
Unicode_EndsWith(ConstUnicode str,    // IN
                 ConstUnicode suffix) // IN
{
   return UnicodeEndsWith(str, suffix, FALSE);
}


static INLINE Bool
Unicode_EndsWithIgnoreCase(ConstUnicode str,    // IN
                           ConstUnicode suffix) // IN
{
   return UnicodeEndsWith(str, suffix, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Find --
 *
 *      Finds the first occurrence of 'strToFind' inside 'str'.
 *
 * Results:
 *      If 'strToFind' exists inside 'str', returns the first starting
 *      index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_Find(ConstUnicode str,       // IN
             ConstUnicode strToFind) // IN
{
   return Unicode_FindSubstrInRange(str,
                                    0,
                                    -1,
                                    strToFind,
                                    0,
                                    -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindFromIndex --
 *
 *      Finds the first occurrence of 'strToFind' inside 'str' in the range
 *      [fromIndex, lengthOfStr).
 *
 * Results:
 *      If 'strToFind' exists inside 'str' in the specified range,
 *      returns the first starting index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_FindFromIndex(ConstUnicode str,       // IN
                      ConstUnicode strToFind, // IN
                      UnicodeIndex fromIndex) // IN
{
   return Unicode_FindSubstrInRange(str,
                                    fromIndex,
                                    -1,
                                    strToFind,
                                    0,
                                    -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindInRange --
 *
 *      Finds the first occurrence of 'strToFind' inside 'str' in the range
 *      [start, start+length).
 *
 * Results:
 *      If 'strToFind' exists inside 'str' in the specified range,
 *      returns the first starting index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_FindInRange(ConstUnicode str,       // IN
                    ConstUnicode strToFind, // IN
                    UnicodeIndex start,     // IN
                    UnicodeIndex length)    // IN
{
   return Unicode_FindSubstrInRange(str,
                                    start,
                                    length,
                                    strToFind,
                                    0,
                                    -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindLast --
 *
 *      Finds the last occurrence of 'strToFind' inside 'str'.
 *
 * Results:
 *      If 'strToFind' exists inside 'str', returns the last starting
 *      index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_FindLast(ConstUnicode str,       // IN
                 ConstUnicode strToFind) // IN
{
   return Unicode_FindLastSubstrInRange(str,
                                        0,
                                        -1,
                                        strToFind,
                                        0,
                                        -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindLastFromIndex --
 *
 *      Finds the last occurrence of 'strToFind' inside 'str' in the range
 *      [fromIndex, lengthOfStr).
 *
 * Results:
 *      If 'strToFind' exists inside 'str' in the specified range,
 *      returns the last starting index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_FindLastFromIndex(ConstUnicode str,       // IN
                          ConstUnicode strToFind, // IN
                          UnicodeIndex fromIndex) // IN
{
   return Unicode_FindLastSubstrInRange(str,
                                        fromIndex,
                                        -1,
                                        strToFind,
                                        0,
                                        -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindLastInRange --
 *
 *      Finds the last occurrence of 'strToFind' inside 'str' in the range
 *      [start, start+length).
 *
 * Results:
 *      If 'strToFind' exists inside 'str' in the specified range,
 *      returns the last starting index of 'strToFind' in that range.
 *
 *      Otherwise, returns UNICODE_INDEX_NOT_FOUND.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE UnicodeIndex
Unicode_FindLastInRange(ConstUnicode str,       // IN
                        ConstUnicode strToFind, // IN
                        UnicodeIndex start,     // IN
                        UnicodeIndex length)    // IN
{
   return Unicode_FindLastSubstrInRange(str,
                                        start,
                                        length,
                                        strToFind,
                                        0,
                                        -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Insert --
 *
 *      Allocates and returns a new copy of 'destination', with the
 *      string 'source' inserted at the index 'destinationStart'.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_Insert(ConstUnicode destination,      // IN
               UnicodeIndex destinationStart, // IN
               ConstUnicode source)           // IN
{
   return Unicode_ReplaceRange(destination,
                               destinationStart,
                               0,
                               source,
                               0,
                               -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_InsertRange --
 *
 *      Allocates and returns a new copy of 'destination', with the
 *      specified range of the string 'source' inserted at the index
 *      'destinationStart'.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_InsertRange(ConstUnicode destination,
                    UnicodeIndex destinationStart,
                    ConstUnicode source,
                    UnicodeIndex sourceStart,
                    UnicodeIndex sourceLength)
{
   return Unicode_ReplaceRange(destination,
                               destinationStart,
                               0,
                               source,
                               sourceStart,
                               sourceLength);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsEqual --
 *
 *      Tests two strings for canonical equivalence.
 *
 * Results:
 *      TRUE if the two strings are canonically equivalent, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Unicode_IsEqual(ConstUnicode str1, // IN
                ConstUnicode str2) // IN
{
   return Unicode_CompareRange(str1,
                               0,
                               -1,
                               str2,
                               0,
                               -1,
                               FALSE) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_RemoveRange --
 *
 *      Allocates and returns a new string that contains a copy of
 *      'destination' with the code units in the range [start, start + length)
 *      removed.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_RemoveRange(ConstUnicode destination,
                    UnicodeIndex start,
                    UnicodeIndex length)
{
   return Unicode_ReplaceRange(destination, start, length, "", 0, 0);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Replace --
 *
 *      Allocates and returns a new string that contains a copy of
 *      'destination' with the code units in the range
 *      [destinationStart, destinarionStart + destinationLength) replaced
 *      with 'source'.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_Replace(ConstUnicode destination,
                UnicodeIndex destinationStart,
                UnicodeIndex destinationLength,
                ConstUnicode source)
{
   return Unicode_ReplaceRange(destination,
                               destinationStart,
                               destinationLength,
                               source,
                               0,
                               -1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeStartsWith --
 * Unicode_StartsWith --
 * Unicode_StartsWithIgnoreCase --
 *
 *      Tests if 'str' starts with 'prefix'.
 *
 * Results:
 *      TRUE if 'str' starts with 'prefix', FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
UnicodeStartsWith(ConstUnicode str,     // IN:
                  ConstUnicode prefix,  // IN:
                  Bool ignoreCase)      // IN:
{
   UnicodeIndex strLength = Unicode_LengthInCodePoints(str);
   UnicodeIndex prefixLength = Unicode_LengthInCodePoints(prefix);

   if (prefixLength > strLength) {
      return FALSE;
   }

   return Unicode_CompareRange(str,
                               0,
                               prefixLength,
                               prefix,
                               0,
                               prefixLength,
                               ignoreCase) == 0;
}


static INLINE Bool
Unicode_StartsWith(ConstUnicode str,    // IN
                   ConstUnicode prefix) // IN
{
   return UnicodeStartsWith(str, prefix, FALSE);
}


static INLINE Bool
Unicode_StartsWithIgnoreCase(ConstUnicode str,    // IN
                             ConstUnicode prefix) // IN
{
   return UnicodeStartsWith(str, prefix, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Truncate --
 *
 *      Allocates and returns a new copy of 'str' truncated to the
 *      specified length in code units.
 *
 * Results:
 *      The newly-allocated truncated copy of 'str'.  Caller must free
 *      with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_Truncate(ConstUnicode str,    // IN
                 UnicodeIndex length) // IN
{
   return Unicode_Substr(str, 0, length);
}


#ifdef __cplusplus
}
#endif

#endif // _UNICODE_OPERATIONS_H_
