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
 * unicodeTransforms.h --
 *
 *      Operations that transform all the characters in a string.
 *
 *      Transform operations like uppercase and lowercase are
 *      locale-sensitive (depending on the user's country and language
 *      preferences).
 */

#ifndef _UNICODE_TRANSFORMS_H_
#define _UNICODE_TRANSFORMS_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMNIXMOD
#include "includeCheck.h"

#include "unicodeTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Standardizes the case of the string by doing a locale-agnostic
 * uppercase operation, then a locale-agnostic lowercase operation.
 */
Unicode Unicode_FoldCase(ConstUnicode str);

/*
 * Trims whitespace from either side of the string.
 */
Unicode Unicode_Trim(ConstUnicode str);

/*
 * The following APIs require IBM's ICU library.
 */

#ifdef SUPPORT_UNICODE

/*
 * Transforms the case of the string using the given locale's rules.
 *
 * Pass in a NULL locale to use the process's default locale.
 *
 * Changing the case of a string can change its length, so don't
 * assume the string is the same length after calling these functions.
 */
Unicode Unicode_ToLower(ConstUnicode str, const char *locale);
Unicode Unicode_ToUpper(ConstUnicode str, const char *locale);
Unicode Unicode_ToTitle(ConstUnicode str, const char *locale);

typedef enum {
   UNICODE_NORMAL_FORM_C, // "e with acute accent" -> U+00E9
   UNICODE_NORMAL_FORM_D  // "e with acute accent" -> U+0065 U+0302
} UnicodeNormalizationForm;

/*
 * Normalizes Unicode characters composed of multiple parts into a
 * standard form.
 */
Unicode Unicode_Normalize(ConstUnicode str,
                          UnicodeNormalizationForm form);

#endif

#ifdef __cplusplus
}
#endif

#endif // _UNICODE_TRANSFORMS_H_
