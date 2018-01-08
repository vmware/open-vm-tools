/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * Unicode_LengthInCodePoints --
 *
 *      Returns the length of the unicode string in code points
 *      ("unicode characters").
 *
 * Results:
 *      As above
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

UnicodeIndex
Unicode_LengthInCodePoints(const char *str)  // IN:
{
   return CodeSet_LengthInCodePoints(str);
}


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
 *      The start and length arguments are in code points - unicode
 *      "characters" - not bytes!
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
Unicode_CompareRange(const char *str1,         // IN:
                     UnicodeIndex str1Start,   // IN:
                     UnicodeIndex str1Length,  // IN:
                     const char *str2,         // IN:
                     UnicodeIndex str2Start,   // IN:
                     UnicodeIndex str2Length,  // IN:
                     Bool ignoreCase)          // IN:
{
   int result = -1;
   char *substr1 = NULL;
   char *substr2 = NULL;
   utf16_t *str1UTF16 = NULL;
   utf16_t *str2UTF16 = NULL;
   UnicodeIndex i = 0;
   UnicodeIndex utf16Index;
   utf16_t codeUnit1;
   utf16_t codeUnit2;
   uint32 codePoint1;
   uint32 codePoint2;

   /*
    * TODO: Allocating substrings is a performance hit.  We should do this
    * search in-place.  (However, searching UTF-8 requires tender loving
    * care, and it's just easier to search UTF-16.)
    */

   if (str1Start != 0 || str1Length != -1) {
      substr1 = Unicode_Substr(str1, str1Start, str1Length);
      if (!substr1) {
         goto out;
      }
      str1 = substr1;
   }

   if (str2Start != 0 || str2Length != -1) {
      substr2 = Unicode_Substr(str2, str2Start, str2Length);
      if (!substr2) {
         goto out;
      }
      str2 = substr2;
   }

   /*
    * XXX TODO: Need to normalize the incoming strings to NFC or NFD.
    */

   str1UTF16 = Unicode_GetAllocUTF16(str1);
   if (!str1UTF16) {
      goto out;
   }

   str2UTF16 = Unicode_GetAllocUTF16(str2);
   if (!str2UTF16) {
      goto out;
   }

   /*
    * TODO: This is the naive string search algorithm, which is O(n * m). We
    * can do better with KMP or Boyer-Moore if this proves to be a bottleneck.
    */

   while (TRUE) {
      codeUnit1 = *(str1UTF16 + i);
      codeUnit2 = *(str2UTF16 + i);

      /*
       * TODO: Simple case folding doesn't handle the situation where more
       * than one code unit is needed to store the result of the case folding.
       *
       * This means that German "straBe" (where B = sharp S, U+00DF) will not
       * match "STRASSE", even though the two strings are the same.
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
    * The two UTF-16 code units differ. If they're the first code unit of a
    * surrogate pair (for Unicode values past U+FFFF), decode the surrogate
    * pair into a full Unicode code point.
    */

   if (U16_IS_SURROGATE(codeUnit1)) {
      ssize_t strUTF16Len = Unicode_UTF16Strlen(str1UTF16);

      // U16_NEXT modifies the index, so let it work on a copy.
      utf16Index = i;

      // Decode the surrogate if needed.
      U16_NEXT(str1UTF16, utf16Index, strUTF16Len, codePoint1);
   } else {
      // Not a surrogate?  Then the code point value is the code unit.
      codePoint1 = codeUnit1;
   }

   if (U16_IS_SURROGATE(codeUnit2)) {
      ssize_t strUTF16Len = Unicode_UTF16Strlen(str2UTF16);

      utf16Index = i;
      U16_NEXT(str2UTF16, utf16Index, strUTF16Len, codePoint2);
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
   free(str1UTF16);
   free(str2UTF16);

   free(substr1);
   free(substr2);

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
 *      The start and length arguments are in code points - unicode
 *      "characters" - not bytes!
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
Unicode_FindSubstrInRange(const char *str,               // IN:
                          UnicodeIndex strStart,         // IN:
                          UnicodeIndex strLength,        // IN:
                          const char *strToFind,         // IN:
                          UnicodeIndex strToFindStart,   // IN:
                          UnicodeIndex strToFindLength)  // IN:
{
   UnicodeIndex index;
   uint32 *utf32Source = NULL;
   uint32 *utf32Search = NULL;

   ASSERT(str);
   ASSERT(strStart >= 0);
   ASSERT((strLength >= 0) || (strLength == -1));

   ASSERT(strToFind);
   ASSERT(strToFindStart >= 0);
   ASSERT((strToFindLength >= 0) || (strToFindLength == -1));

   /*
    * Convert the string to be searched and the search string to UTF32.
    */

   if (!CodeSet_UTF8ToUTF32(str, (char **) &utf32Source)) {
      Panic("%s: invalid UTF8 string @ %p\n", __FUNCTION__, str);
   }

   if (!CodeSet_UTF8ToUTF32(strToFind, (char **) &utf32Search)) {
      Panic("%s: invalid UTF8 string @ %p\n", __FUNCTION__, strToFind);
   }

   /*
    * Do any bounds cleanup and checking that is necessary...
    */

   if (strLength < 0) {
      strLength = Unicode_LengthInCodePoints(str) - strStart;
   }

   if (strToFindLength < 0) {
      strToFindLength = Unicode_LengthInCodePoints(strToFind) - strToFindStart;
   }

   if (strLength < strToFindLength) {
      index = UNICODE_INDEX_NOT_FOUND;
      goto bail;
   }

   /*
    * Yes, this may be viewed as a bit strange but this is what strstr does.
    */

   if (strToFindLength == 0) {
      index = strStart;
      goto bail;
   }

   /*
    * Attempt to find the first occurence of the search string in the string
    * to be searched.
    */

   for (index = strStart;
        index <= (strStart + strLength - strToFindLength);
        index++) {
      UnicodeIndex i;
      Bool match = FALSE;
      UnicodeIndex indexSrc = index;
      UnicodeIndex indexSrch = strToFindStart;

      for (i = 0; i < strToFindLength; i++) {
         match = (utf32Source[indexSrc++] == utf32Search[indexSrch++]);

         if (!match) {
            break;
         }
      }

      if (match) {
         goto bail;
      }
   }

   index = UNICODE_INDEX_NOT_FOUND;

bail:

   free(utf32Source);
   free(utf32Search);

   return index;
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
 *      The start and length arguments are in code points - unicode
 *      "characters" - not bytes!
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
Unicode_FindLastSubstrInRange(const char *str,               // IN:
                              UnicodeIndex strStart,         // IN:
                              UnicodeIndex strLength,        // IN:
                              const char *strToFind,         // IN:
                              UnicodeIndex strToFindStart,   // IN:
                              UnicodeIndex strToFindLength)  // IN:
{
   UnicodeIndex index;
   uint32 *utf32Source = NULL;
   uint32 *utf32Search = NULL;

   ASSERT(str);
   ASSERT(strStart >= 0);
   ASSERT((strLength >= 0) || (strLength == -1));

   ASSERT(strToFind);
   ASSERT(strToFindStart >= 0);
   ASSERT((strToFindLength >= 0) || (strToFindLength == -1));

   /*
    * Convert the string to be searched and the search string to UTF32.
    */

   if (!CodeSet_UTF8ToUTF32(str, (char **) &utf32Source)) {
      Panic("%s: invalid UTF8 string @ %p\n", __FUNCTION__, str);
   }

   if (!CodeSet_UTF8ToUTF32(strToFind, (char **) &utf32Search)) {
      Panic("%s: invalid UTF8 string @ %p\n", __FUNCTION__, strToFind);
   }

   /*
    * Do any bounds cleanup and checking that is necessary...
    */

   if (strLength < 0) {
      strLength = Unicode_LengthInCodePoints(str) - strStart;
   }

   if (strToFindLength < 0) {
      strToFindLength = Unicode_LengthInCodePoints(strToFind) - strToFindStart;
   }

   if (strLength < strToFindLength) {
      index = UNICODE_INDEX_NOT_FOUND;
      goto bail;
   }

   /*
    * Yes, this may be viewed as a bit strange but this is what strstr does.
    */

   if (strToFindLength == 0) {
      index = strStart;
      goto bail;
   }

   /*
    * Attempt to find the last occurence of the search string in the string
    * to be searched.
    */

   for (index = strStart + strLength - strToFindLength;
        index >= strStart;
        index--) {
      UnicodeIndex i;
      Bool match = FALSE;
      UnicodeIndex indexSrc = index;
      UnicodeIndex indexSrch = strToFindStart;

      for (i = 0; i < strToFindLength; i++) {
         match = (utf32Source[indexSrc++] == utf32Search[indexSrch++]);

         if (!match) {
            break;
         }
      }

      if (match) {
         goto bail;
      }
   }

   index = UNICODE_INDEX_NOT_FOUND;

bail:

   free(utf32Source);
   free(utf32Search);

   return index;
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
 *      Pass -1 for the length parameter to indicate "from start until
 *      end of string".
 *
 *      The start and length arguments are in code points - unicode
 *      "characters" - not bytes!
 *
 * Results:
 *      The newly-allocated substring of 'str' in the range [index,
 *      index + length). Caller must free with free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_Substr(const char *str,      // IN:
               UnicodeIndex start,   // IN:
               UnicodeIndex length)  // IN:
{
   char *substr;
   uint32 codePointLen;
   uint32 *utf32 = NULL;

   ASSERT(str);
   ASSERT((start >= 0) || (start == -1));
   ASSERT((length >= 0) || (length == -1));

   if (!CodeSet_UTF8ToUTF32(str, (char **) &utf32)) {
      Panic("%s: invalid UTF8 string @ %p\n", __FUNCTION__, str);
   }

   codePointLen = 0;
   while (utf32[codePointLen] != 0) {
      codePointLen++;
   }

   if ((start < 0) || (start > codePointLen)) {
      start = codePointLen;
   }

   if ((length < 0) || ((start + length) > codePointLen)) {
      length = codePointLen - start;
   }

   utf32[start + length] = 0;

   CodeSet_UTF32ToUTF8((char *) &utf32[start], &substr);

   free(utf32);

   return substr;
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
 *      The start and length arguments are in code points - unicode
 *      "characters" - not bytes!
 *
 * Results:
 *      A newly-allocated string containing the results of the replace
 *      operation.  Caller must free with free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_ReplaceRange(const char *dest,         // IN:
                     UnicodeIndex destStart,   // IN:
                     UnicodeIndex destLength,  // IN:
                     const char *src,          // IN:
                     UnicodeIndex srcStart,    // IN:
                     UnicodeIndex srcLength)   // IN:
{
   char *result;
   char *stringOne;
   char *stringTwo;
   char *stringThree;

   ASSERT(dest);
   ASSERT((destStart >= 0) || (destStart == -1));
   ASSERT((destLength >= 0) || (destLength == -1));

   ASSERT(src);
   ASSERT((srcStart >= 0) || (srcStart == -1));
   ASSERT((srcLength >= 0) || (srcLength == -1));

   stringOne = Unicode_Substr(dest, 0, destStart);
   stringTwo = Unicode_Substr(src, srcStart, srcLength);
   stringThree = Unicode_Substr(dest, destStart + destLength, -1);

   result = Unicode_Join(stringOne, stringTwo, stringThree, NULL);

   free(stringOne);
   free(stringTwo);
   free(stringThree);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Join --
 *
 *      Allocates and returns a new string containing the 'first' followed by
 *      all the unicode strings specified as optional arguments. Appending
 *      ceases when a NULL pointer is detected.
 *
 * Results:
 *      The newly-allocated string.  Caller must free with free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_Join(const char *first,  // IN:
             ...)                // IN:
{
   char *result;

   if (first == NULL) {
      result = NULL;
   } else {
      va_list args;
      const char *cur;

      result = Unicode_Duplicate(first);

      va_start(args, first);

      while ((cur = va_arg(args, const char *)) != NULL) {
         char *temp;

         temp = Unicode_Format("%s%s", result, cur);
         free(result);
         result = temp;
      }

      va_end(args);
   }

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

char *
Unicode_Format(const char *fmt, // IN: the format
               ...)             // IN: the arguments
{
   va_list args;
   char *p;

   va_start(args, fmt);
   p = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   return p;
}
