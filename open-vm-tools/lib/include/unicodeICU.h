/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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
 * unicodeICU.h --
 *
 *      Unicode operations that depend on the third-party ICU support
 *      library.
 */

#ifndef _UNICODE_ICU_H_
#define _UNICODE_ICU_H_

#define INCLUDE_ALLOW_USERLEVEL

#ifndef USE_ICU
#error These interfaces require the ICU library (define USE_ICU).
#endif

#include "includeCheck.h"

#include "unicodeBase.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
   UNICODE_COMPARE_DEFAULT = 0,
   UNICODE_COMPARE_IGNORE_ACCENTS,
   UNICODE_COMPARE_IGNORE_CASE,
   UNICODE_COMPARE_IGNORE_PUNCTUATION
} UnicodeCompareOption;


/*
 * Different languages and cultures have unique rules for how strings
 * are compared and sorted.  For example:
 *
 *   Swedish: z < "o with umlaut"
 *   German:  "o with umlaut" < z
 *
 * When producing a result visible to the user (like a sorted list of
 * virtual machine names) string comparsion must obey the rules set by
 * the user's language and culture, collectively called the "locale".
 */

int Unicode_CompareWithLocale(const char *str1,
                              const char *str2,
                              const char *locale,
                              UnicodeCompareOption compareOption);

/*
 * Transforms the case of the string using the given locale's rules.
 *
 * Pass in a NULL locale to use the process's default locale.
 *
 * Changing the case of a string can change its length, so don't
 * assume the string is the same length after calling these functions.
 */
char *Unicode_ToLower(const char *str, const char *locale);
char *Unicode_ToUpper(const char *str, const char *locale);

#ifdef HAVE_ICU_38
char *Unicode_ToTitle(const char *str, const char *locale);
#endif

typedef enum {
   UNICODE_NORMAL_FORM_C, // "e with acute accent" -> U+00E9
   UNICODE_NORMAL_FORM_D  // "e with acute accent" -> U+0065 U+0302
} UnicodeNormalizationForm;

/*
 * Normalizes Unicode characters composed of multiple parts into a
 * standard form.
 */
char *Unicode_Normalize(const char *str,
                          UnicodeNormalizationForm form);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _UNICODE_ICU_H_
