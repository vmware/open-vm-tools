/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif



/*
 * These platforms use bsd_vsnprintf().
 *
 * XXX: open-vm-tools does not use bsd_vsnprintf because bsd_vsnprintf uses
 * convertutf.{h,c}, and the license for those files does not meet the
 * redistribution requirements for Debian.
 * <https://github.com/vmware/open-vm-tools/issues/148>
 */
#if !defined(VMX86_TOOLS) || defined(_WIN32)
#if (defined _WIN32 && !defined STR_NO_WIN32_LIBS) || \
    (defined __linux__ && !defined __UCLIBC__ && !defined __ANDROID__) || \
    defined __APPLE__
#define HAS_BSD_PRINTF 1
#endif
#endif

/*
 * ASCII/UTF-8 versions
 *
 * NOTE: All size_t arguments and integer returns values are in bytes.
 *
 * NOTE: Str_Asprintf/Str_Vasprintf return NULL on failure, while
 * Str_SafeAsprintf/Str_SafeVasprintf VERIFY.
 *
 * NOTE: "%s" refers to strings of "char" units, while "%S" refers to
 * strings of "wchar_t" units, regardless of platform.
 */

#ifdef HAS_BSD_PRINTF
int Str_Sprintf_C_Locale(char *buf,               // OUT:
                         size_t max,              // IN:
                         const char *fmt,         // IN:
                         ...) PRINTF_DECL(3, 4);  // IN:
#endif

int Str_Sprintf(char *buf,               // OUT:
                size_t max,              // IN:
                const char *fmt,         // IN:
                ...) PRINTF_DECL(3, 4);  // IN:
int Str_Snprintf(char *buf,               // OUT:
                 size_t len,              // IN:
                 const char *fmt,         // IN:
                 ...) PRINTF_DECL(3, 4);  // IN:
int Str_Vsnprintf(char *buf,        // OUT:
                  size_t len,       // IN:
                  const char *fmt,  // IN:
                  va_list args);    // IN:

size_t Str_Strlen(const char *src,  // IN:
                  size_t maxLen);   // IN:
char *Str_Strnstr(const char *src,  // IN:
                  const char *sub,  // IN:
                  size_t n);        // IN:
char *Str_Strcpy(char *dst,        // OUT:
                 const char *src,  // IN:
                 size_t maxLen);   // IN:
char *Str_Strncpy(char *dest,       // OUT:
                  size_t destSize,  // IN:
                  const char *src,  // IN:
                  size_t n);        // IN:
char *Str_Strcat(char *dst,        // IN/OUT:
                 const char *src,  // IN:
                 size_t maxLen);   // IN:
char *Str_Strncat(char *buf,        // IN/OUT:
                  size_t bufSize,   // IN:
                  const char *src,  // IN:
                  size_t n);        // IN:

char *Str_Asprintf(size_t *length,          // OUT/OPT:
                   const char *format,      // IN:
                   ...) PRINTF_DECL(2, 3);  // IN:
char *Str_Vasprintf(size_t *length,      // OUT/OPT:
                    const char *format,  // IN:
                    va_list arguments);  // IN:
char *Str_SafeAsprintf(size_t *length,          // OUT/OPT:
                       const char *format,      // IN:
                       ...) PRINTF_DECL(2, 3);  // IN:
char *Str_SafeVasprintf(size_t *length,      // OUT/OPT:
                        const char *format,  // IN:
                        va_list arguments);  // IN:

#if defined(_WIN32) // {

/*
 * wchar_t versions
 *
 * NOTE: All size_t arguments and integer return values are in
 * wchar_ts, not bytes.
 *
 * NOTE: Str_Aswprintf/Str_Vaswprintf return NULL on failure, while
 * Str_SafeAswprintf/Str_SafeVaswprintf VERIFY.
 *
 * NOTE: "%s" refers to strings of "char" units, while "%S" refers to
 * strings of "wchar_t" units, regardless of platform.
 */

int Str_Swprintf(wchar_t *buf,        // OUT:
                 size_t max,          // IN:
                 const wchar_t *fmt,  // IN:
                 ...);
int Str_Snwprintf(wchar_t *buf,        // OUT:
                  size_t len,          // IN:
                  const wchar_t *fmt,  // IN:
                  ...);
int Str_Vsnwprintf(wchar_t *buf,        // OUT:
                   size_t len,          // IN:
                   const wchar_t *fmt,  // IN:
                   va_list args);
wchar_t *Str_Wcscpy(wchar_t *dst,        // OUT:
                    const wchar_t *src,  // IN:
                    size_t maxLen);      // IN:
wchar_t *Str_Wcscat(wchar_t *dst,        // IN/OUT:
                    const wchar_t *src,  // IN:
                    size_t maxLen);      // IN:
wchar_t *Str_Wcsncat(wchar_t *buf,        // IN/OUT:
                     size_t bufSize,      // IN:
                     const wchar_t *src,  // IN:
                     size_t n);           // IN:

wchar_t *Str_Aswprintf(size_t *length,         // OUT/OPT:
                       const wchar_t *format,  // IN:
                       ...);                   // IN:
wchar_t *Str_Vaswprintf(size_t *length,         // OUT/OPT:
                        const wchar_t *format,  // IN:
                        va_list arguments);     // IN:
wchar_t *Str_SafeAswprintf(size_t *length,         // OUT/OPT:
                           const wchar_t *format,  // IN:
                           ...);                   // IN:
wchar_t *Str_SafeVaswprintf(size_t *length,         // OUT/OPT:
                            const wchar_t *format,  // IN:
                            va_list arguments);     // IN:

/*
 * These are handly for Windows programmers.  They are like
 * the _tcs functions, but with Str_Strcpy-style bounds checking.
 */

#ifdef UNICODE
   #define Str_Tcscpy(s1, s2, n) Str_Wcscpy(s1, s2, n)
   #define Str_Tcscat(s1, s2, n) Str_Wcscat(s1, s2, n)
#else
   #define Str_Tcscpy(s1, s2, n) Str_Strcpy(s1, s2, n)
   #define Str_Tcscat(s1, s2, n) Str_Strcat(s1, s2, n)
#endif

#endif // } defined(_WIN32)


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

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _STR_H_ */
