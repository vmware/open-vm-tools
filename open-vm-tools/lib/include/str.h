/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * str.h --
 *
 *    string wrapping functions
 */

#ifndef _STR_H_
#define _STR_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#if defined(GLIBC_VERSION_22)
#include <wchar.h>
#elif defined(_WIN32)
#include <tchar.h>
#elif __APPLE__
#include <stdlib.h>
#endif
#include <stdarg.h>

#include "vm_basic_types.h"


/*
 * These platforms use bsd_vsnprintf().
 * This does not mean it has bsd_vsnwprintf().
 */
#if defined _WIN32 && !defined STR_NO_WIN32_LIBS || \
    defined __linux__ && !defined N_PLAT_NLM || __APPLE__
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
 */

EXTERN int Str_Sprintf(char *buf, size_t max,
                       const char *fmt, ...) PRINTF_DECL(3, 4);
EXTERN int Str_Snprintf(char *buf, size_t len,
			const char *fmt, ...) PRINTF_DECL(3, 4);
EXTERN int Str_Vsnprintf(char *buf, size_t len,
			 const char *fmt, va_list args);
EXTERN char *Str_Strnstr(const char *src, const char *sub, size_t n);
EXTERN char *Str_Strcpy(char *dst, const char *src, size_t maxLen);
EXTERN char *Str_Strcat(char *dst, const char *src, size_t maxLen);
EXTERN char *Str_Strncat(char *buf, size_t bufSize, const char *src, size_t n);
EXTERN char *Str_Asprintf(size_t *length,
                          const char *format, ...) PRINTF_DECL(2, 3);
EXTERN char *Str_Vasprintf(size_t *length, const char *format,
                           va_list arguments);

#if defined(_WIN32) || defined(GLIBC_VERSION_22)

/*
 * wchar_t versions
 *
 * NOTE: All size_t arguments and integer return values are in
 * wchar_ts, not bytes.
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

#ifdef _WIN32
#ifdef UNICODE
   #define  Str_Stprintf Str_Swprintf
   #define  Str_Sntprintf Str_Snwprintf
   #define  Str_Vsntprintf Str_Vsnwprintf
   #define  Str_Tcscpy Str_Wcscpy
   #define  Str_Tcscat Str_Wcscat
   #define  Str_Tcsncat Str_Wcsncat
   #define  Str_Astprintf Str_Aswprintf
   #define  Str_Vastprintf Str_Vaswprintf
#else
   #define  Str_Stprintf Str_Sprintf
   #define  Str_Sntprintf Str_Snprintf
   #define  Str_Vsntprintf Str_Vsnprintf
   #define  Str_Tcscpy Str_Strcpy
   #define  Str_Tcscat Str_Strcat
   #define  Str_Tcsncat Str_Strncat
   #define  Str_Astprintf Str_Asprintf
   #define  Str_Vastprintf Str_Vasprintf
#endif
#endif

#endif // defined(_WIN32) || defined(GLIBC_VERSION_22)


/*
 * MsgFmt version of vsnprintf
 */

#ifdef HAS_BSD_PRINTF
struct MsgFmt_Arg;
int Str_MsgFmtSnprintfWork(char **outbuf, size_t bufSize, const char *fmt0,
                           const struct MsgFmt_Arg *args, int numArgs);
#endif


/*
 *----------------------------------------------------------------------
 *
 *    Str_Strchr --
 *    Str_Strrchr --
 *    Str_Strspn  --
 *    Str_Strcspn --
 *
 *    Str_Strchr, Str_Strrchr:
 *       const char *str   - null-terminated string to search
 *       int   c           - character to be located
 *    Str_Strspn, Str_Strcspn:
 *       const char *str1  - null-terminated string to search
 *       const char *str2  - null-terminated string of chars to search for
 *
 *    Macros for MBCS implementation.
 *
 *    All Str_xxxx functions work exactly the same way as the
 *    corresponding run-time library strxxxx functions.
 *
 *    For Windows implementation, they are mapped to the generic international
 *    string function names, so that they are compiled with multi-byte (MBCS).
 *    The inline functions here are necessary for the type-casting. We are unable to
 *    call those MBCS functions directly due to type mismatch.
 *
 *    When SUPPORT_UNICODE is on, we need to make those functionS back to ASCII version,
 *    so that they won't screw up the UTF-8 encoding.
 *
 *    The reason for this work is to mainly take care of filename parsing
 *    involving DBCS, especially in cases where a DBCS consists of the
 *    backslash '\' as the 2nd character.
 *    
 *    '.' & '/' characters are not affected in DBCS parsing because they
 *    are defined to be illegal 2nd byte character. 
 *    
 *----------------------------------------------------------------------
 */
#if _WIN32
   #ifdef SUPPORT_UNICODE
      #define  Str_Strchr(str, c)  strchr(str, c)
      #define  Str_Strrchr(str, c) strrchr(str, c)
      #define  Str_Strspn(str1, str2)  strspn(str1, str2)
      #define  Str_Strcspn(str1, str2) strcspn(str1, str2)
      #define  Str_ToUpper(str) _strupr(str)
      #define  Str_ToLower(str) _strlwr(str)         
   #else
      #include <mbstring.h>

      __inline char *Str_Strchr(const char *_s1, unsigned int _c)
      {
         return (char *)_mbschr((const unsigned char *)_s1, _c);
      }

      __inline char *Str_Strrchr(const char *_s1, unsigned int _c)
      {
         return (char *)_mbsrchr((const unsigned char *)_s1, _c);
      }

      __inline size_t Str_Strspn(const char *_s1, const char *_s2)
      {
         return _mbsspn((const unsigned char *)_s1, (const unsigned char *)_s2);
      }

      __inline size_t Str_Strcspn(const char *_s1, const char *_s2)
      {
         return _mbscspn((const unsigned char *)_s1, (const unsigned char *)_s2);
      }

      __inline char *Str_ToUpper(char *_String)
      {
      #pragma warning(push)
      #pragma warning(disable:4996)           
         return (char *)_mbsupr((unsigned char *)_String);
      #pragma warning(pop)
      }

      __inline char *Str_ToLower(char *_String)
      {
      #pragma warning(push)
      #pragma warning(disable:4996)           
         return (char *)_mbslwr((unsigned char *)_String);
      #pragma warning(pop)
      }
   #endif

   // To-do: These functions should be MBCS aware too when SUPPORT_UNICODE is not defined.   
   #define  Str_Strcasecmp(_s1,_s2) _stricmp((_s1),(_s2))
   #define  Str_Strncasecmp(_s1,_s2,_n) _strnicmp((_s1),(_s2),(_n))
   #define  Str_Strncmp(_s1,_s2,_n) strncmp((_s1),(_s2),(_n))
#else
   #define  Str_Strchr(str, c)  strchr(str, c)
   #define  Str_Strrchr(str, c) strrchr(str, c)
   #define  Str_Strspn(str1, str2)  strspn(str1, str2)
   #define  Str_Strcspn(str1, str2) strcspn(str1, str2)
   #define  Str_Strcasecmp(_s1,_s2) strcasecmp((_s1),(_s2))
   #define  Str_Strncasecmp(_s1,_s2,_n) strncasecmp((_s1),(_s2),(_n))
   #define  Str_Strncmp(_s1,_s2,_n) strncmp((_s1),(_s2),(_n))

   EXTERN char * _Str_ToUpper(char *str);
   #define  Str_ToUpper(str) _Str_ToUpper((str))

   EXTERN char * _Str_ToLower(char *str);
   #define  Str_ToLower(str) _Str_ToLower((str))
#endif   // _WIN32

#endif /* _STR_H_ */
