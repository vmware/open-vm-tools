/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * str.h --
 *
 *    string wrapping functions
 */

#ifndef _STR_H_
#define _STR_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(__linux__)
#include <wchar.h>
#elif defined(_WIN32)
#include <tchar.h>
#elif __APPLE__
#include <stdlib.h>
#endif
#include "compat/compat_stdarg.h" // Provides stdarg.h plus va_copy

#include "vm_basic_types.h"


/*
 * These platforms use bsd_vsnprintf().
 * This does not mean it has bsd_vsnwprintf().
 */
#if (defined _WIN32 && !defined STR_NO_WIN32_LIBS) || \
    defined __linux__ || defined __APPLE__
#define HAS_BSD_PRINTF 1
#endif

/*
 * And these platforms/setups use bsd_vsnwprintf()
 */
#if (defined _WIN32 && !defined STR_NO_WIN32_LIBS) || \
   (defined __GNUC__ && (__GNUC__ < 2                 \
                         || (__GNUC__ == 2            \
                             && __GNUC_MINOR__ < 96)))
#define HAS_BSD_WPRINTF 1
#endif

/*
 * ASCII/UTF-8 versions
 *
 * NOTE: All size_t arguments and integer returns values are in bytes.
 *
 * NOTE: Str_Asprintf/Str_Vasprintf return NULL on failure, while
 * Str_SafeAsprintf/Str_SafeVasprintf ASSERT_NOT_IMPLEMENTED.
 *
 * NOTE: "%s" refers to strings of "char" units, while "%S" refers to
 * strings of "wchar_t" units, regardless of platform.
 */

#ifdef HAS_BSD_PRINTF
EXTERN int Str_Sprintf_C_Locale(char *buf, size_t max,
                                const char *fmt, ...) PRINTF_DECL(3, 4);
#endif

EXTERN int Str_Sprintf(char *buf, size_t max,
                       const char *fmt, ...) PRINTF_DECL(3, 4);
EXTERN int Str_Snprintf(char *buf, size_t len,
			const char *fmt, ...) PRINTF_DECL(3, 4);
EXTERN int Str_Vsnprintf(char *buf, size_t len,
			 const char *fmt, va_list args);
EXTERN size_t Str_Strlen(const char *src, size_t maxLen);
EXTERN char *Str_Strnstr(const char *src, const char *sub, size_t n);
EXTERN char *Str_Strcpy(char *dst, const char *src, size_t maxLen);
EXTERN char *Str_Strcat(char *dst, const char *src, size_t maxLen);
EXTERN char *Str_Strncat(char *buf, size_t bufSize, const char *src, size_t n);

EXTERN char *Str_Asprintf(size_t *length,
                          const char *format, ...) PRINTF_DECL(2, 3);
EXTERN char *Str_Vasprintf(size_t *length, const char *format,
                           va_list arguments);
EXTERN char *Str_SafeAsprintf(size_t *length,
                              const char *format, ...) PRINTF_DECL(2, 3);
EXTERN char *Str_SafeVasprintf(size_t *length, const char *format,
                               va_list arguments);

#if defined(_WIN32) || defined(__linux__) // {

/*
 * wchar_t versions
 *
 * NOTE: All size_t arguments and integer return values are in
 * wchar_ts, not bytes.
 *
 * NOTE: Str_Aswprintf/Str_Vaswprintf return NULL on failure, while
 * Str_SafeAswprintf/Str_SafeVaswprintf ASSERT_NOT_IMPLEMENTED.
 *
 * NOTE: "%s" refers to strings of "char" units, while "%S" refers to
 * strings of "wchar_t" units, regardless of platform.
 */

EXTERN int Str_Swprintf(wchar_t *buf, size_t max,
                        const wchar_t *fmt, ...);
EXTERN int Str_Snwprintf(wchar_t *buf, size_t len,
                         const wchar_t *fmt, ...);
EXTERN int Str_Vsnwprintf(wchar_t *buf, size_t len,
                          const wchar_t *fmt, va_list args);
EXTERN wchar_t *Str_Wcscpy(wchar_t *dst, const wchar_t *src, size_t maxLen);
EXTERN wchar_t *Str_Wcscat(wchar_t *dst, const wchar_t *src, size_t maxLen);
EXTERN wchar_t *Str_Wcsncat(wchar_t *buf, size_t bufSize, const wchar_t *src,
                            size_t n);

EXTERN wchar_t *Str_Aswprintf(size_t *length,
                              const wchar_t *format, ...);
EXTERN wchar_t *Str_Vaswprintf(size_t *length, const wchar_t *format,
                               va_list arguments);
EXTERN wchar_t *Str_SafeAswprintf(size_t *length,
                                  const wchar_t *format, ...);
EXTERN wchar_t *Str_SafeVaswprintf(size_t *length, const wchar_t *format,
                                   va_list arguments);

unsigned char *Str_Mbscpy(char *buf, const char *src,
                          size_t maxSize);
unsigned char *Str_Mbscat(char *buf, const char *src,
                          size_t maxSize);

/*
 * These are handly for Windows programmers.  They are like
 * the _tcs functions, but with Str_Strcpy-style bounds checking.
 *
 * We don't have Str_Mbsncat() because it has some odd semantic
 * ambiguity (whether to truncate in the middle of a multibyte
 * sequence) that I want to stay away from.  -- edward
 */

#ifdef _WIN32
#ifdef UNICODE
   #define Str_Tcscpy(s1, s2, n) Str_Wcscpy(s1, s2, n)
   #define Str_Tcscat(s1, s2, n) Str_Wcscat(s1, s2, n)
#else
   #define Str_Tcscpy(s1, s2, n) Str_Mbscpy(s1, s2, n)
   #define Str_Tcscat(s1, s2, n) Str_Mbscat(s1, s2, n)
#endif
#endif

#endif // } defined(_WIN32) || defined(__linux__)


/*
 * Wrappers for standard string functions
 *
 * These are either for Windows-Posix compatibility,
 * or just gratuitous wrapping for consistency.
 */

#define Str_Strcmp(s1, s2) strcmp(s1, s2)
#define Str_Strncmp(s1, s2, n) strncmp(s1, s2, n)

#define Str_Strchr(s, c) strchr(s, c)
#define Str_Strrchr(s, c) strrchr(s, c)
#define Str_Strspn(s1, s2) strspn(s1, s2)
#define Str_Strcspn(s1, s2) strcspn(s1, s2)

#if defined(_WIN32)
   #define Str_Strcasecmp(s1, s2) _stricmp(s1, s2)
   #define Str_Strncasecmp(s1, s2, n) _strnicmp(s1, s2, n)
   #define Str_ToUpper(s) _strupr(s)
   #define Str_ToLower(s) _strlwr(s)
#else
   #define Str_Strcasecmp(s1, s2) strcasecmp(s1, s2)
   #define Str_Strncasecmp(s1, s2, n) strncasecmp(s1, s2, n)
   char *Str_ToUpper(char *string);
   char *Str_ToLower(char *string);
#endif

#ifdef _WIN32
   #define Str_Tcscmp(s1, s2) _tcscmp(s1, s2)
   #define Str_Tcsncmp(s1, s2, n) _tcsncmp(s1, s2, n)
   #define Str_Tcsicmp(s1, s2) _tcsicmp(s1, s2)
   #define Str_Tcsnicmp(s1, s2, n) _tcsnicmp(s1, s2, n)
   #define Str_Tcschr(s, c) _tcschr(s, c)
   #define Str_Tcsrchr(s, c) _tcsrchr(s, c)
   #define Str_Tcsspn(s1, s2) _tcsspn(s1, s2)
   #define Str_Tcscspn(s1, s2) _tcscspn(s1, s2)
   #define Str_Tcsupr(s) _tcsupr(s)
   #define Str_Tcslwr(s) _tcslwr(s)
#endif

#endif /* _STR_H_ */
