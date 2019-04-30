/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
 * unicodeICU.c --
 *
 *      Unicode functionality that depends on the third-party ICU
 *      library.
 */

#include <unicode/ucasemap.h>
#include <unicode/ucol.h>
#include <unicode/uiter.h>
#include <unicode/ustring.h>
#include <unicode/unorm.h>

#include "util.h"
#include "unicodeBase.h"
#include "unicodeICU.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_CompareWithLocale --
 *
 *      Compares two strings for equivalence under the collation rules
 *      of the specified locale.
 *
 *      The caller can specify ignoring differences in accents, case,
 *      or punctuation.
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
Unicode_CompareWithLocale(const char *str1,                   // IN
                          const char *str2,                   // IN
                          const char *locale,                 // IN
                          UnicodeCompareOption compareOption) // IN
{
   UCollationResult compareResult;
   UColAttributeValue comparisonStrength;
   UErrorCode status = U_ZERO_ERROR;
   int result;
   UCollator *coll;
   UCharIterator str1Iter;
   UCharIterator str2Iter;

   uiter_setUTF8(&str1Iter, (const char *)str1, -1);
   uiter_setUTF8(&str2Iter, (const char *)str2, -1);

   switch (compareOption) {
   case UNICODE_COMPARE_DEFAULT:
      comparisonStrength = UCOL_DEFAULT;
      break;
   case UNICODE_COMPARE_IGNORE_ACCENTS:
      comparisonStrength = UCOL_PRIMARY;
      break;
   case UNICODE_COMPARE_IGNORE_CASE:
      comparisonStrength = UCOL_SECONDARY;
      break;
   case UNICODE_COMPARE_IGNORE_PUNCTUATION:
      comparisonStrength = UCOL_TERTIARY;
      break;
   default:
      NOT_IMPLEMENTED();
   }

   coll = ucol_open(locale, &status);

   ASSERT(U_SUCCESS(status));
   ASSERT(coll);

   if (U_FAILURE(status) || !coll) {
      return -1;
   }

   // Normalize all strings to NFD before comparing.
   ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
   ucol_setAttribute(coll, UCOL_STRENGTH, comparisonStrength, &status);

   ASSERT(U_SUCCESS(status));

   compareResult = ucol_strcollIter(coll, &str1Iter, &str2Iter, &status);

   ucol_close(coll);

   if (U_FAILURE(status)) {
      // We'll probably only get here if the input wasn't UTF-8.
      ASSERT(U_SUCCESS(status));
      return -1;
   }

   switch (compareResult) {
   case UCOL_LESS:
      result = -1;
      break;
   case UCOL_EQUAL:
      result = 0;
      break;
   case UCOL_GREATER:
      result = 1;
      break;
   default:
      NOT_IMPLEMENTED();
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Normalize --
 *
 *      Creates a Unicode string by normalizing the input string
 *      into a Unicode normal form.
 *
 *      Normalization Form C ("precomposed") ensures that accented
 *      characters use as few Unicode code points as possible.  This
 *      form is often used on Windows and Linux hosts.
 *
 *      Example: small letter e with acute -> U+00E9
 *
 *      Normalization Form D ("decomposed") ensures that accented
 *      characters (e with accent acute) use separate Unicode code
 *      points for the base letter and accents.  This form is used on
 *      Mac OS hosts.
 *
 *      Example: small letter e with acute -> U+0065 U+0301
 *
 * Results:
 *      The allocated Unicode string, or NULL on failure.
 *      Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_Normalize(const char *str,               // IN
                  UnicodeNormalizationForm form) // IN
{
   UNormalizationMode mode;
   UChar *uchars;
   char *result;
   int32_t normalizedLen;
   UErrorCode status = U_ZERO_ERROR;
   UCharIterator strIter;
   UBool neededToNormalize = FALSE;

   uiter_setUTF8(&strIter, (const char *)str, -1);

   switch (form) {
   case UNICODE_NORMAL_FORM_C:
      mode = UNORM_NFC;
      break;
   case UNICODE_NORMAL_FORM_D:
      mode = UNORM_NFD;
      break;
   default:
      NOT_REACHED();
   }

   normalizedLen = unorm_next(&strIter,
                              NULL,
                              0,
                              mode,
                              0,
                              TRUE,
                              &neededToNormalize,
                              &status);

   if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
      // We expect U_BUFFER_OVERFLOW_ERROR here. Anything else is a problem.
      ASSERT(U_SUCCESS(status));
      return NULL;
   }

   uchars = Util_SafeMalloc(sizeof *uchars * normalizedLen);

   // Reset back to the beginning of the UTF-8 input.
   (*strIter.move)(&strIter, 0, UITER_START);

   status = U_ZERO_ERROR;
   normalizedLen = unorm_next(&strIter,
                              uchars,
                              normalizedLen,
                              mode,
                              0,
                              TRUE,
                              &neededToNormalize,
                              &status);

   if (U_FAILURE(status)) {
      ASSERT(U_SUCCESS(status));
      return NULL;
   }

   result = Unicode_AllocWithLength(uchars,
                                    normalizedLen * 2,
                                    STRING_ENCODING_UTF16);
   free(uchars);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_ToLower --
 *
 *      Creates a Unicode string by lower-casing the input string using
 *      the rules of the specified locale.
 *
 *      The resulting string may not be the same length as the input
 *      string.
 *
 *      Pass NULL for the locale to use the process's default locale.
 *
 * Results:
 *      The allocated Unicode string, or NULL on failure.
 *      Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_ToLower(const char *str,    // IN
                const char *locale) // IN
{
   UCaseMap *caseMap;
   UErrorCode status = U_ZERO_ERROR;
   char *utf8Dest;
   const char *utf8Src = (const char *)str;
   int32_t utf8SrcLen = strlen(utf8Src);
   int32_t destCapacity = utf8SrcLen + 1;
   int32_t destLen;
   char *result = NULL;

   /*
    * XXX TODO: This and the two following functions are substantially
    * identical.  Refactor them!  (Note that ucasemap_utf8ToTitle
    * takes a non-const UCaseMap, so we can't just use pointers to
    * functions unless we cast.)
    */

   // Most lower-case operations don't change the length of the string.
   utf8Dest = Util_SafeMalloc(destCapacity);

   caseMap = ucasemap_open(locale, 0, &status);
   if (U_FAILURE(status)) {
      goto out;
   }

   destLen = ucasemap_utf8ToLower(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

   if (status != U_BUFFER_OVERFLOW_ERROR) {
      goto out;
   }

   // If we need a bigger buffer, then reallocate and retry.
   destCapacity = destLen + 1;
   utf8Dest = Util_SafeRealloc(utf8Dest, destCapacity);

   status = U_ZERO_ERROR;
   destLen = ucasemap_utf8ToLower(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

  out:
   ucasemap_close(caseMap);

   if (U_SUCCESS(status) && status != U_STRING_NOT_TERMINATED_WARNING) {
      result = utf8Dest;
   } else {
      DEBUG_ONLY(Warning("%s: Invalid UTF-8 string detected.\n",
                         __FUNCTION__));
      free(utf8Dest);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_ToUpper --
 *
 *      Creates a Unicode string by upper-casing the input string using
 *      the rules of the specified locale.
 *
 *      The resulting string may not be the same length as the input
 *      string.
 *
 *      Pass NULL for the locale to use the process's default locale.
 *
 * Results:
 *      The allocated Unicode string, or NULL on failure.
 *      Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_ToUpper(const char *str,    // IN
                const char *locale) // IN
{
   UCaseMap *caseMap;
   UErrorCode status = U_ZERO_ERROR;
   char *utf8Dest;
   const char *utf8Src = (const char *)str;
   int32_t utf8SrcLen = strlen(utf8Src);
   int32_t destCapacity = utf8SrcLen + 1;
   int32_t destLen;
   char *result = NULL;

   // Most upper-case operations don't change the length of the string.
   utf8Dest = Util_SafeMalloc(destCapacity);

   caseMap = ucasemap_open(locale, 0, &status);
   if (U_FAILURE(status)) {
      goto out;
   }

   destLen = ucasemap_utf8ToUpper(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

   if (status != U_BUFFER_OVERFLOW_ERROR) {
      goto out;
   }

   // If we need a bigger buffer, then reallocate and retry.
   destCapacity = destLen + 1;
   utf8Dest = Util_SafeRealloc(utf8Dest, destCapacity);

   status = U_ZERO_ERROR;
   destLen = ucasemap_utf8ToUpper(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

  out:
   ucasemap_close(caseMap);

   if (U_SUCCESS(status) && status != U_STRING_NOT_TERMINATED_WARNING) {
      result = utf8Dest;
   } else {
      DEBUG_ONLY(Warning("%s: Invalid UTF-8 string detected.\n",
                         __FUNCTION__));
      free(utf8Dest);
   }

   return result;
}


/*
 * "ucasemap_utf8ToTitle" is not in version 3.6 of the ICU library,
 * which appears to be the default on many systems...
 *
 * XXX Currently HAVE_ICU_38 is only set by the open-source tools
 * build (based on what it detects on the current system) because that
 * is the only entity currently capable of compiling with USE_ICU.
 */

#ifdef HAVE_ICU_38

/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_ToTitle --
 *
 *      Creates a Unicode string by title-casing the input string using
 *      the rules of the specified locale.
 *
 *      The resulting string may not be the same length as the input
 *      string.
 *
 *      Pass NULL for the locale to use the process's default locale.
 *
 * Results:
 *      The allocated Unicode string, or NULL on failure.
 *      Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_ToTitle(const char *str,    // IN
                const char *locale) // IN
{
   UCaseMap *caseMap;
   UErrorCode status = U_ZERO_ERROR;
   char *utf8Dest;
   const char *utf8Src = (const char *)str;
   int32_t utf8SrcLen = strlen(utf8Src);
   int32_t destCapacity = utf8SrcLen + 1;
   int32_t destLen;
   char *result = NULL;

   // Most title-case operations don't change the length of the string.
   utf8Dest = Util_SafeMalloc(destCapacity);

   caseMap = ucasemap_open(locale, 0, &status);
   if (U_FAILURE(status)) {
      goto out;
   }

   destLen = ucasemap_utf8ToTitle(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

   if (status != U_BUFFER_OVERFLOW_ERROR) {
      goto out;
   }

   // If we need a bigger buffer, then reallocate and retry.
   destCapacity = destLen + 1;
   utf8Dest = Util_SafeRealloc(utf8Dest, destCapacity);

   status = U_ZERO_ERROR;
   destLen = ucasemap_utf8ToTitle(caseMap,
                                  utf8Dest,
                                  destCapacity,
                                  utf8Src,
                                  utf8SrcLen,
                                  &status);

  out:
   ucasemap_close(caseMap);

   if (U_SUCCESS(status) && status != U_STRING_NOT_TERMINATED_WARNING) {
      result = utf8Dest;
   } else {
      DEBUG_ONLY(Warning("%s: Invalid UTF-8 string detected.\n",
                         __FUNCTION__));
      free(utf8Dest);
   }

   return result;
}

#endif // HAVE_ICU_38
