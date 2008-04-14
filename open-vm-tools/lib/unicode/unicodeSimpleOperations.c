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
 * unicodeSimpleOperations.c --
 *
 *      Simple UTF-8 implementation of unicodeOperations.h interface.
 */

#include "vmware.h"

#include "util.h"
#include "str.h"
#include "unicodeBase.h"
#include "unicodeInt.h"
#include "unicodeOperations.h"
#include "codeset.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_CompareRange --
 *
 *      Compares ranges of two Unicode strings for canonical
 *      equivalence in code point order.
 *
 *      Canonical equivalence means the two strings represent the same
 *      Unicode code points, regardless of the order of combining
 *      characters or use of compatibility singletons.
 *
 *      See Unicode Standard Annex #15 (Unicode Normalization Forms)
 *      for more on canonical equivalence and composition.
 *
 *      If ignoreCase is TRUE, then the two strings are case-folded
 *      (converted to upper-case, then converted to lower-case) in a
 *      locale-agnostic manner before comparing.
 *
 *      Indices and lengths that are out of bounds are pinned to the
 *      edges of the string.
 *
 *      Pass -1 for any length parameter to indicate "from start until
 *      end of string".
 *
 * Results:
 *      -1 if str1 < str2, 0 if str1 == str2, 1 if str1 > str2.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
Unicode_CompareRange(ConstUnicode str1,       // IN
                     UnicodeIndex str1Start,  // IN
                     UnicodeIndex str1Length, // IN
                     ConstUnicode str2,       // IN
                     UnicodeIndex str2Start,  // IN
                     UnicodeIndex str2Length, // IN
                     Bool ignoreCase)         // IN
{
   int result = -1;
   Unicode substr1 = NULL;
   Unicode substr2 = NULL;
   utf16_t *substr1UTF16 = NULL;
   utf16_t *substr2UTF16 = NULL;
   UnicodeIndex i = 0;
   UnicodeIndex utf16Index;
   utf16_t codeUnit1;
   utf16_t codeUnit2;
   uint32 codePoint1;
   uint32 codePoint2;

   UnicodePinIndices(str1, &str1Start, &str1Length);
   UnicodePinIndices(str2, &str2Start, &str2Length);

   /*
    * TODO: Allocating substrings is a performance hit.  We should do
    * this search in-place.  (However, searching UTF-8 requires tender loving
    * care, and it's just easier to search UTF-16.)
    */
   substr1 = Unicode_Substr(str1, str1Start, str1Length);
   if (!substr1) {
      goto out;
   }

   substr2 = Unicode_Substr(str2, str2Start, str2Length);
   if (!substr2) {
      goto out;
   }

   /*
    * XXX TODO: Need to normalize the incoming strings to NFC or NFD.
    */
   substr1UTF16 = Unicode_GetAllocUTF16(substr1);
   if (!substr1UTF16) {
      goto out;
   }

   substr2UTF16 = Unicode_GetAllocUTF16(substr2);
   if (!substr2UTF16) {
      goto out;
   }

   /*
    * TODO: This is the naive string search algorithm, which is
    * O(n * m).  We can do better with KMP or Boyer-Moore if this
    * proves to be a bottleneck.
    */
   while (TRUE) {
      codeUnit1 = *(substr1UTF16 + i);
      codeUnit2 = *(substr2UTF16 + i);

      /*
       * TODO: Simple case folding doesn't handle the situation where
       * more than one code unit is needed to store the result of the
       * case folding.
       *
       * This means that German "straBe" (where B = sharp S, U+00DF)
       * will not match "STRASSE", even though the two strings are the
       * same.
       */
      if (ignoreCase) {
         codeUnit1 = UnicodeSimpleCaseFold(codeUnit1);
         codeUnit2 = UnicodeSimpleCaseFold(codeUnit2);
      }

      if (codeUnit1 != codeUnit2) {
         break;
      }

      if (codeUnit1 == 0) {
         // End of both strings reached: strings are equal.
         result = 0;
         goto out;
      }

      i++;
   }

   /*
    * The two UTF-16 code units differ.  If they're the first code unit
    * of a surrogate pair (for Unicode values past U+FFFF), decode the
    * surrogate pair into a full Unicode code point.
    */
   if (U16_IS_SURROGATE(codeUnit1)) {
      ssize_t substrUTF16Len = Unicode_UTF16Strlen(substr1UTF16);

      // U16_NEXT modifies the index, so let it work on a copy.
      utf16Index = i;

      // Decode the surrogate if needed.
      U16_NEXT(substr1UTF16, utf16Index, substrUTF16Len, codePoint1);
   } else {
      // Not a surrogate?  Then the code point value is the code unit.
      codePoint1 = codeUnit1;
   }

   if (U16_IS_SURROGATE(codeUnit2)) {
      ssize_t substrUTF16Len = Unicode_UTF16Strlen(substr2UTF16);

      utf16Index = i;
      U16_NEXT(substr2UTF16, utf16Index, substrUTF16Len, codePoint2);
   } else {
      codePoint2 = codeUnit2;
   }

   if (codePoint1 < codePoint2) {
      result = -1;
   } else if (codePoint1 > codePoint2) {
      result = 1;
   } else {
      // If we hit the end of the string, we've already gone to 'out'.
      NOT_REACHED();
   }

  out:
   free(substr1UTF16);
   free(substr2UTF16);

   Unicode_Free(substr1);
   Unicode_Free(substr2);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindSubstrInRange --
 *
 *      Searches the string 'str' in the range [strStart, strStart+strLength)
 *      for the first occurrence of the code units of 'strToFind'
 *      in the range [strToFindStart, strToFindStart+strToFindLength).
 *
 *      Indices and lengths that are out of bounds are pinned to the
 *      edges of the string.
 *
 *      Pass -1 for any length parameter to indicate "from start until
 *      end of string".
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

UnicodeIndex
Unicode_FindSubstrInRange(ConstUnicode str,             // IN
                          UnicodeIndex strStart,        // IN
                          UnicodeIndex strLength,       // IN
                          ConstUnicode strToFind,       // IN
                          UnicodeIndex strToFindStart,  // IN
                          UnicodeIndex strToFindLength) // IN
{
   const char *strUTF8 = (const char *)str;
   const char *strToFindUTF8 = (const char *)strToFind;
   UnicodeIndex strUTF8Offset;

   UnicodePinIndices(str, &strStart, &strLength);
   UnicodePinIndices(strToFind, &strToFindStart, &strToFindLength);

   if (strLength < strToFindLength) {
      return UNICODE_INDEX_NOT_FOUND;
   }

   if (strToFindLength == 0) {
      return strStart;
   }

   /*
    * TODO: This loop is quite similar to the one in
    * Unicode_FindLastSubstrInRange.  We might be able to refactor the
    * two into a common helper function.
    */
   for (strUTF8Offset = strStart;
        strUTF8Offset < strStart + strLength;
        strUTF8Offset++) {
      char byte = strUTF8[strUTF8Offset];

      if (byte == strToFindUTF8[strToFindStart]) {
         UnicodeIndex strSubOffset = strUTF8Offset;
         UnicodeIndex strToFindSubOffset = strToFindStart;
         UnicodeIndex strToFindEnd = strToFindStart + strToFindLength - 1;

         while (TRUE) {
            if (strToFindSubOffset == strToFindEnd) {
               // Found the substring.
               return strUTF8Offset;
            }

            strToFindSubOffset++;
            strSubOffset++;

            if (strUTF8[strSubOffset] != strToFindUTF8[strToFindSubOffset]) {
               break;
            }
         }
      }
   }

   return UNICODE_INDEX_NOT_FOUND;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FindLastSubstrInRange --
 *
 *      Searches the string 'str' in the range [strStart, strStart+strLength)
 *      for the last occurrence of the code units of 'strToFind'
 *      in the range [strToFindStart, strToFindStart+strToFindLength).
 *
 *      Indices and lengths that are out of bounds are pinned to the
 *      edges of the string.
 *
 *      Pass -1 for any length parameter to indicate "from start until
 *      end of string".
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

UnicodeIndex
Unicode_FindLastSubstrInRange(ConstUnicode str,             // IN
                              UnicodeIndex strStart,        // IN
                              UnicodeIndex strLength,       // IN
                              ConstUnicode strToFind,       // IN
                              UnicodeIndex strToFindStart,  // IN
                              UnicodeIndex strToFindLength) // IN
{
   const char *strUTF8 = (const char *)str;
   const char *strToFindUTF8 = (const char *)strToFind;
   UnicodeIndex strUTF8Offset;

   UnicodePinIndices(str, &strStart, &strLength);
   UnicodePinIndices(strToFind, &strToFindStart, &strToFindLength);

   if (strLength < strToFindLength) {
      return UNICODE_INDEX_NOT_FOUND;
   }

   if (strToFindLength == 0) {
      return strStart;
   }

   for (strUTF8Offset = strStart + strLength - 1;
        strUTF8Offset >= strStart;
        strUTF8Offset--) {
      char byte = strUTF8[strUTF8Offset];
      UnicodeIndex strToFindEnd = strToFindStart + strToFindLength - 1;

      if (byte == strToFindUTF8[strToFindEnd]) {
         UnicodeIndex strSubOffset = strUTF8Offset;
         UnicodeIndex strToFindSubOffset = strToFindEnd;

         while (TRUE) {
            if (strToFindSubOffset == strToFindStart) {
               // Found the substring.
               return strSubOffset;
            }

            strToFindSubOffset--;
            strSubOffset--;

            if (strUTF8[strSubOffset] != strToFindUTF8[strToFindSubOffset]) {
               break;
            }
         }
      }
   }

   return UNICODE_INDEX_NOT_FOUND;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Substr --
 *
 *      Allocates and returns a substring of 'str'.
 *
 *      Indices and lengths that are out of bounds are pinned to the
 *      edges of the string.
 *
 *      Pass -1 for any length parameter to indicate "from start until
 *      end of string".
 *
 * Results:
 *      The newly-allocated substring of 'str' in the range [index,
 *      index + length). Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_Substr(ConstUnicode str,    // IN
               UnicodeIndex start,  // IN
               UnicodeIndex length) // IN
{
   UnicodePinIndices(str, &start, &length);

   return Util_SafeStrndup(((const char *)str) + start, length);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_ReplaceRange --
 *
 *      Core operation upon which append, insert, replace, and remove
 *      are based.
 *
 *      Replaces the code units of destination in the range
 *      [destinationStart, destinationLength) with the code units of
 *      source in the range [sourceStart, sourceLength).
 *
 *      Indices and lengths that are out of bounds are pinned to the
 *      edges of the string.
 *
 *      Pass -1 for any length parameter to indicate "from start until
 *      end of string".
 *
 * Results:
 *      A newly-allocated string containing the results of the replace
 *      operation.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_ReplaceRange(ConstUnicode destination,       // IN
                     UnicodeIndex destinationStart,  // IN
                     UnicodeIndex destinationLength, // IN
                     ConstUnicode source,            // IN
                     UnicodeIndex sourceStart,       // IN
                     UnicodeIndex sourceLength)      // IN
{
   UnicodeIndex destNumCodeUnits;
   UnicodeIndex resultLength;
   char *result;

   UnicodePinIndices(destination, &destinationStart, &destinationLength);
   UnicodePinIndices(source, &sourceStart, &sourceLength);

   destNumCodeUnits = Unicode_LengthInCodeUnits(destination);

   resultLength = destNumCodeUnits - destinationLength + sourceLength;

   result = Util_SafeMalloc(resultLength + 1);

   // Start with the destination bytes before the substring to be replaced.
   memcpy(result,
          destination,
          destinationStart);

   // Insert the substring of source in place of the destination substring.
   memcpy(result + destinationStart,
          (const char *)source + sourceStart,
          sourceLength);

   // Append the remaining bytes of destination after the replaced substring.
   memcpy(result + destinationStart + sourceLength,
          (const char *)destination + destinationStart + destinationLength,
          destNumCodeUnits - destinationStart - destinationLength);

   result[resultLength] = '\0';

   return (Unicode)result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Join --
 *
 *      Allocates and returns a new string containing the 'first' followed by
 *      all the unicode strings specified as optional arguments (which must
 *      be of type ConstUnicode). Appending ceases when a NULL pointer is
 *      detected.
 * 
 * Results:
 *      The newly-allocated string.  Caller must free with Unicode_Free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_Join(ConstUnicode first,  // IN:
             ...)                 // IN
{
   va_list args;
   Unicode result;
   ConstUnicode cur;

   if (first == NULL) {
      return NULL;
   }

   result = Unicode_Duplicate(first);

   va_start(args, first);

   while ((cur = va_arg(args, ConstUnicode)) != NULL) {
      Unicode temp;

      temp = Unicode_Append(result, cur);
      Unicode_Free(result);
      result = temp;
   }

   va_end(args);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Format --
 *
 *      Format a Unicode string (roughly equivalent to Str_Asprintf()).
 *
 * Results:
 *      The formatted string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Unicode
Unicode_Format(const char *fmt,	// IN: the format
	       ...)		// IN: the arguments
{
   va_list args;
   char *p;
   
   va_start(args, fmt);
   p = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   return p;
}
