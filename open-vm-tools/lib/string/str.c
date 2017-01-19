/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * str.c --
 *
 *    User level string wrappers
 *
 * WARNING:
 *    Do not call any variadic functions - those that use "..." repeatedly
 *    with the same va_list or memory corruption and/or crashes will occur.
 *    The suggested way deal with repeated calls is to use a va_copy:
 *
 *    va_list tmpArgs;
 *
 *    va_copy(tmpArgs, ap);
 *    // Call the variadic function
 *    va_end(tmpArgs);
 *
 */

#ifdef _WIN32
#include <windows.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "vmware.h"
#include "str.h"
#ifdef HAS_BSD_PRINTF
#include "bsd_output.h"
#endif
#include "codeset.h"

#if defined _WIN32 && !defined HAS_BSD_PRINTF
#define vsnprintf _vsnprintf
#endif

#ifndef _WIN32
extern int vasprintf(char **ptr, const char *f, va_list arg);
/*
 * Declare vswprintf on platforms where it's not known to exist.  We know
 * it's available on glibc >= 2.2, FreeBSD >= 5.0, and all versions of
 * Solaris.
 * (Re: Solaris, vswprintf has been present since Solaris 8, and we only
 * support Solaris 9 and above, since that was the first release available
 * for x86, so we just assume it's already there.)
 *
 * XXX Str_Vsnwprintf and friends are still protected by _WIN32 and
 * glibc >= 2.2.  I.e., even though they should be able to work on
 * FreeBSD 5.0+ and Solaris 8+, they aren't made available there.
 */
#   if !(defined(__linux__) ||                                          \
         (defined(__FreeBSD__) && (__FreeBSD_version >= 500000)) ||     \
         defined(sun))
extern int vswprintf(wchar_t *wcs, size_t maxlen, const wchar_t *format, va_list args);
#   endif
#endif // _WIN32


/*
 *----------------------------------------------------------------------
 *
 * Str_Vsnprintf --
 *
 *      Compatibility wrapper b/w different libc versions
 *
 * Results:
 *
 *      int - number of bytes stored in 'str' (not including NUL
 *      terminate character), -1 on overflow (insufficient space for
 *      NUL terminate is considered overflow)
 *
 *      NB: on overflow the buffer WILL be NUL terminated at the last
 *      UTF-8 code point boundary within the buffer's bounds.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Str_Vsnprintf(char *str,          // OUT
              size_t size,        // IN
              const char *format, // IN
              va_list ap)         // IN
{
   int retval;

   ASSERT(str != NULL);
   ASSERT(format != NULL);

#if defined HAS_BSD_PRINTF && !defined __ANDROID__
   retval = bsd_vsnprintf(&str, size, format, ap);
#else
   retval = vsnprintf(str, size, format, ap);
#endif

   /*
    * Linux glibc 2.0.x returns -1 and NUL terminates (which we shouldn't
    * be linking against), but glibc 2.1.x follows c99 and returns
    * characters that would have been written.
    *
    * In the case of Win32 and !HAS_BSD_PRINTF, we are using
    * _vsnprintf(), which returns -1 on overflow, returns size
    * when result fits exactly, and does not NUL terminate in
    * those cases.
    */

   if ((retval < 0) || (retval >= size)) {
      if (size > 0) {
         /* Find UTF-8 code point boundary and place NUL termination there */
         int trunc = CodeSet_Utf8FindCodePointBoundary(str, size - 1);

         str[trunc] = '\0';
      }
   }
   if (retval >= size) {
      return -1;
   }
   return retval;
}


#ifdef HAS_BSD_PRINTF
/*
 *----------------------------------------------------------------------
 *
 * Str_Sprintf_C_Locale --
 *
 *      sprintf wrapper that fails on overflow. Enforces numeric C locale.
 *
 * Results:
 *      Returns the number of bytes stored in 'buf'.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Str_Sprintf_C_Locale(char *buf,        // OUT:
                     size_t maxSize,   // IN:
                     const char *fmt,  // IN:
                     ...)              // IN:
{
   va_list args;
   int retval;

   ASSERT(buf);
   ASSERT(fmt);

   va_start(args, fmt);
   retval = bsd_vsnprintf_c_locale(&buf, maxSize, fmt, args);
   va_end(args);

   if ((retval < 0) || (retval >= maxSize)) {
      if (maxSize > 0) {
         /* Find UTF-8 code point boundary and place NUL termination there */
         int trunc = CodeSet_Utf8FindCodePointBoundary(buf, maxSize - 1);

         buf[trunc] = '\0';
      }
   }

   if (retval >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   return retval;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * Str_Sprintf --
 *
 *      sprintf wrapper that fails on overflow
 *
 * Results:
 *      Returns the number of bytes stored in 'buf'.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Str_Sprintf(char *buf,       // OUT
            size_t maxSize,  // IN
            const char *fmt, // IN
            ...)             // IN
{
   va_list args;
   int i;

   va_start(args, fmt);
   i = Str_Vsnprintf(buf, maxSize, fmt, args);
   va_end(args);
   if (i < 0) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }
   return i;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Snprintf --
 *
 *      Compatibility wrapper b/w different libc versions
 *
 * Results:
 *
 *      int - number of bytes stored in 'str' (not including NUL
 *      terminate character), -1 on overflow (insufficient space for
 *      NUL terminate is considered overflow)
 *
 *      NB: on overflow the buffer WILL be NUL terminated
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Str_Snprintf(char *str,          // OUT
             size_t size,        // IN
             const char *format, // IN
             ...)                // IN
{
   int retval;
   va_list args;

   ASSERT(str != NULL);
   ASSERT(format != NULL);

   va_start(args, format);
   retval = Str_Vsnprintf(str, size, format, args);
   va_end(args);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strcpy --
 *
 *    Wrapper for strcpy that checks for buffer overruns.
 *
 * Results:
 *    Same as strcpy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

char *
Str_Strcpy(char *buf,       // OUT
           const char *src, // IN
           size_t maxSize)  // IN
{
   size_t len;

   ASSERT(buf != NULL);
   ASSERT(src != NULL);

   len = strlen(src);
   if (len >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }
   return memcpy(buf, src, len + 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strlen --
 *
 *      Calculate length of the string.
 *
 * Results:
 *      Length of s not including the terminating '\0' character.
 *      If there is no '\0' for first maxLen bytes, then it
 *      returns maxLen.
 *
 * Side Effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

size_t
Str_Strlen(const char *s,  // IN:
           size_t maxLen)  // IN:

{
   const char *end;

   ASSERT(s != NULL);

   if ((end = memchr(s, '\0', maxLen)) == NULL) {
      return maxLen;
   }
   return end - s;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strnstr --
 *
 *      Find a substring within a string of length at most n. 'sub' must be
 *      NUL-terminated. 'n' is interpreted as an unsigned int.
 *
 * Results:
 *      A pointer to the beginning of the substring, or NULL if not found.
 *
 * Side Effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

char *
Str_Strnstr(const char *src,  // IN:
            const char *sub,  // IN:
            size_t n)         // IN:
{
   size_t subLen;
   const char *end;

   ASSERT(src != NULL);
   ASSERT(sub != NULL);

   if ((subLen = strlen(sub)) == 0) {
      return (char *) src;
   }
   if ((end = memchr(src, '\0', n)) == NULL) {
      end = src + n;
   }
   end -= subLen - 1;
   if (end <= src) {
      return NULL;
   }
   for (;
       (src = memchr(src, sub[0], end - src)) != NULL &&
        memcmp(src, sub, subLen) != 0;
        src++) {
   }
   return (char *) src;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strcat --
 *
 *    Wrapper for strcat that checks for buffer overruns.
 *
 * Results:
 *    Same as strcat.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

char *
Str_Strcat(char *buf,       // IN/OUT
           const char *src, // IN
           size_t maxSize)  // IN
{
   size_t bufLen;
   size_t srcLen;

   ASSERT(buf != NULL);
   ASSERT(src != NULL);

   bufLen = strlen(buf);
   srcLen = strlen(src);

   /* The first comparison checks for numeric overflow */
   if (bufLen + srcLen < srcLen || bufLen + srcLen >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   memcpy(buf + bufLen, src, srcLen + 1);

   return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strncat --
 *
 *    Wrapper for strncat that checks for buffer overruns.
 *
 *    Specifically, this function will Panic if a buffer overrun would
 *    have occurred.
 *
 * Results:
 *    Same as strncat.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

char *
Str_Strncat(char *buf,       // IN/OUT
            size_t bufSize,  // IN: Size of buf
            const char *src, // IN: String to append
            size_t n)        // IN: Max chars of src to append
{
   size_t bufLen;

   ASSERT(buf != NULL);
   ASSERT(src != NULL);

   /*
    * Check bufLen + n first so we can avoid the second call to strlen
    * if possible.
    *
    * The reason the test with bufLen and n is >= rather than just >
    * is that strncat always NUL-terminates the resulting string, even
    * if it reaches the length limit n. This means that if it happens that
    * bufLen + n == bufSize, strncat will write a NUL terminator that
    * is outside of the buffer. Therefore, we make sure this does not
    * happen by adding the == case to the Panic test.
    */

   bufLen = strlen(buf);

   if (bufLen + n >= bufSize &&
       bufLen + strlen(src) >= bufSize) {
      Panic("%s:%d Buffer too small\n", __FILE__,__LINE__);
   }

   /*
    * We don't need to worry about NUL termination, because it's only
    * needed on overflow and we Panic above in that case.
    */

   return strncat(buf, src, n);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Asprintf --
 *
 *    Same as Str_Vasprintf(), but parameters are passed inline
 *
 * Results:
 *    Same as Str_Vasprintf()
 *
 * Side effects:
 *    Same as Str_Vasprintf()
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_Asprintf(size_t *length,       // OUT/OPT
             const char *format,   // IN
             ...)                  // IN
{
   va_list arguments;
   char *result;

   va_start(arguments, format);
   result = Str_Vasprintf(length, format, arguments);
   va_end(arguments);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_SafeAsprintf --
 *
 *    Same as Str_SafeVasprintf(), but parameters are passed inline
 *
 * Results:
 *    Same as Str_SafeVasprintf()
 *
 * Side effects:
 *    Same as Str_SafeVasprintf()
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_SafeAsprintf(size_t *length,       // OUT/OPT
                 const char *format,   // IN
                 ...)                  // IN
{
   va_list arguments;
   char *result;

   va_start(arguments, format);
   result = Str_SafeVasprintf(length, format, arguments);
   va_end(arguments);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrVasprintfInternal --
 *
 *    Allocate and format a string, using the GNU libc way to specify the
 *    format (i.e. optionally allow the use of positional parameters)
 *
 * Results:
 *
 *    The allocated string on success (if 'length' is not NULL, *length
 *    is set to the length of the allocated string).
 *
 *    ASSERTs or returns NULL on failure, depending on the value of
 *    'assertOnFailure'.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
StrVasprintfInternal(size_t *length,       // OUT/OPT:
                     const char *format,   // IN:
                     va_list arguments,    // IN:
                     Bool assertOnFailure) // IN:
{
   char *buf = NULL;
   int ret;

#if defined HAS_BSD_PRINTF && !defined __ANDROID__
   ret = bsd_vsnprintf(&buf, 0, format, arguments);

#elif !defined sun && !defined STR_NO_WIN32_LIBS
   ret = vasprintf(&buf, format, arguments);

#else
   /*
    * Simple implementation of Str_Vasprintf when we we have vsnprintf
    * but not vasprintf (e.g. in Win32 or in drivers). We just fallback
    * to vsnprintf, doubling if we didn't have enough space.
    */
   size_t bufSize = strlen(format);

   do {
      /*
       * Initial allocation of strlen(format) * 2. Should this be tunable?
       * XXX Yes, this could overflow and spin forever when you get near 2GB
       *     allocations. I don't care. --rrdharan
       */

      char *newBuf;
      va_list tmpArgs;

      bufSize *= 2;
      newBuf = realloc(buf, bufSize);
      if (!newBuf) {
         free(buf);
         buf = NULL;
         goto exit;
      }

      buf = newBuf;

      va_copy(tmpArgs, arguments);
      ret = Str_Vsnprintf(buf, bufSize, format, tmpArgs);
      va_end(tmpArgs);
   } while (ret < 0);
#endif

   if (ret < 0) {
      buf = NULL;
      goto exit;
   }
   if (length != NULL) {
      *length = ret;
   }

  exit:
   if (assertOnFailure) {
      VERIFY(buf);
   }
   return buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Vasprintf --
 *
 *    See StrVasprintfInternal.
 *
 * Results:
 *    Returns NULL on failure.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *-----------------------------------------------------------------------------
 */

char *
Str_Vasprintf(size_t *length,       // OUT/OPT
              const char *format,   // IN
              va_list arguments)    // IN
{
   return StrVasprintfInternal(length, format, arguments, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_SafeVasprintf --
 *
 *    See StrVasprintfInternal.
 *
 * Results:
 *    Calls VERIFY on failure.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_SafeVasprintf(size_t *length,       // OUT/OPT
                  const char *format,   // IN
                  va_list arguments)    // IN
{
   return StrVasprintfInternal(length, format, arguments, TRUE);
}

#if defined(_WIN32) || defined(__linux__)

/*
 *----------------------------------------------------------------------
 *
 * Str_Swprintf --
 *
 *      wsprintf wrapper that fails on overflow
 *
 * Results:
 *      Returns the number of wchar_ts stored in 'buf'.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Str_Swprintf(wchar_t *buf,       // OUT
             size_t maxSize,     // IN: Size of buf, in wide-characters.
             const wchar_t *fmt, // IN
             ...)                // IN
{
   va_list args;
   int i;

   va_start(args,fmt);
   i = Str_Vsnwprintf(buf, maxSize, fmt, args);
   va_end(args);
   if (i < 0) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }
   return i;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Vsnwprintf --
 *
 *      Compatibility wrapper b/w different libc versions
 *
 * Results:
 *
 *      int - number of wchar_ts stored in 'str' (not including NUL
 *      terminate character), -1 on overflow (insufficient space for
 *      NUL terminate is considered overflow)
 *
 *      NB: on overflow the buffer WILL be NUL terminated
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Str_Vsnwprintf(wchar_t *str,          // OUT
               size_t size,           // IN: Size of str, in wide-characters.
               const wchar_t *format, // IN
               va_list ap)            // IN
{
   int retval;

#if defined(HAS_BSD_WPRINTF) && HAS_BSD_WPRINTF
   retval = bsd_vsnwprintf(&str, size, format, ap);
#elif defined(_WIN32)
   retval = _vsnwprintf(str, size, format, ap);
#else
   retval = vswprintf(str, size, format, ap);
#endif

   /*
    * Linux glibc 2.0.x returns -1 and NUL terminates (which we shouldn't
    * be linking against), but glibc 2.1.x follows c99 and returns
    * characters that would have been written.
    *
    * In the case of Win32 and !HAS_BSD_PRINTF, we are using
    * _vsnwprintf(), which returns -1 on overflow, returns size
    * when result fits exactly, and does not NUL terminate in
    * those cases.
    */

#if defined _WIN32 && !defined HAS_BSD_PRINTF
   if ((retval < 0 || retval >= size) && size > 0) {
      str[size - 1] = L'\0';
   }
#endif
   if (retval >= size) {
      return -1;
   }

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Snwprintf --
 *
 *      Compatibility wrapper b/w different libc versions
 *
 * Results:
 *
 *      int - number of wchar_ts stored in 'str' (not including NUL
 *      terminate character), -1 on overflow (insufficient space for
 *      NUL terminate is considered overflow)
 *
 *      NB: on overflow the buffer WILL be NUL terminated
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Str_Snwprintf(wchar_t *str,          // OUT
              size_t size,           // IN: Size of str, in wide-characters.
              const wchar_t *format, // IN
              ...)                   // IN
{
   int retval;
   va_list args;

   va_start(args, format);
   retval = Str_Vsnwprintf(str, size, format, args);
   va_end(args);
   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Wcscpy --
 *
 *      Wrapper for wcscpy that checks for buffer overruns.
 *
 * Results:
 *    Same as wcscpy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

wchar_t *
Str_Wcscpy(wchar_t *buf,       // OUT
           const wchar_t *src, // IN
           size_t maxSize)     // IN: Size of buf, in wide-characters.
{
   size_t len;

   len = wcslen(src);
   if (len >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }
   return memcpy(buf, src, (len + 1)*sizeof(wchar_t));
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Wcscat --
 *
 *      Wrapper for wcscat that checks for buffer overruns.
 *
 * Results:
 *    Same as wcscat.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

wchar_t *
Str_Wcscat(wchar_t *buf,       // IN/OUT
           const wchar_t *src, // IN
           size_t maxSize)     // IN: Size of buf, in wide-characters.
{
   size_t bufLen;
   size_t srcLen;

   bufLen = wcslen(buf);
   srcLen = wcslen(src);

   /* The first comparison checks for numeric overflow */
   if (bufLen + srcLen < srcLen || bufLen + srcLen >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   memcpy(buf + bufLen, src, (srcLen + 1)*sizeof(wchar_t));

   return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Wcsncat --
 *
 *    Wrapper for wcsncat that checks for buffer overruns.
 *
 *    Specifically, this function will Panic if a buffer overrun would
 *    have occurred.
 *
 * Results:
 *    Same as wcsncat.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

wchar_t *
Str_Wcsncat(wchar_t *buf,       // IN/OUT
            size_t bufSize,     // IN: Size of buf, in wide-characters.
            const wchar_t *src, // IN: String to append
            size_t n)           // IN: Max chars of src to append
{
   size_t bufLen = wcslen(buf);

   /*
    * Check bufLen + n first so we can avoid the second call to wcslen
    * if possible.
    *
    * The reason the test with bufLen and n is >= rather than just >
    * is that wcsncat always NUL-terminates the resulting string, even
    * if it reaches the length limit n. This means that if it happens that
    * bufLen + n == bufSize, wcsncat will write a NUL terminator that
    * is outside of the buffer. Therefore, we make sure this does not
    * happen by adding the == case to the Panic test.
    */

   if (bufLen + n >= bufSize &&
       bufLen + wcslen(src) >= bufSize) {
      Panic("%s:%d Buffer too small\n", __FILE__,__LINE__);
   }

   /*
    * We don't need to worry about NUL termination, because it's only
    * needed on overflow and we Panic above in that case.
    */

   return wcsncat(buf, src, n);
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Mbscpy --
 *
 *    Wrapper for _mbscpy that checks for buffer overruns.
 *
 * Results:
 *    Same as strcpy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

unsigned char *
Str_Mbscpy(char *buf,        // OUT
           const char *src,  // IN
           size_t maxSize)   // IN
{
   size_t len;

   len = strlen(src);
   if (len >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }
   return memcpy(buf, src, len + 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Mbscat --
 *
 *    Wrapper for _mbscat that checks for buffer overruns.
 *
 *    The Microsoft _mbscat may or may not deal with tailing
 *    partial multibyte sequence in buf.  We don't.
 *
 * Results:
 *    Same as strcat.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

unsigned char *
Str_Mbscat(char *buf,        // IN/OUT
           const char *src,  // IN
           size_t maxSize)   // IN
{
   size_t bufLen;
   size_t srcLen;

   bufLen = strlen(buf);
   srcLen = strlen(src);

   /* The first comparison checks for numeric overflow */
   if (bufLen + srcLen < srcLen || bufLen + srcLen >= maxSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   memcpy(buf + bufLen, src, srcLen + 1);

   return (unsigned char *)buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrVaswprintfInternal --
 *
 *    Allocate and format a string.
 *
 * Results:
 *    The allocated string on success (if 'length' is not NULL, *length
 *    is set to the length of the allocated string, in wchat_ts)
 *
 *    ASSERTs or returns NULL on failure, depending on the value of
 *    'assertOnFailure'.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static wchar_t *
StrVaswprintfInternal(size_t *length,         // OUT/OPT:
                      const wchar_t *format,  // IN:
                      va_list arguments,      // IN
                      Bool assertOnFailure)   // IN
{
   size_t bufSize;
   wchar_t *buf = NULL;
   int retval;

   bufSize = wcslen(format);

   do {
      /*
       * Initial allocation of wcslen(format) * 2. Should this be tunable?
       * XXX Yes, this could overflow and spin forever when you get near 2GB
       *     allocations. I don't care. --rrdharan
       */

      va_list tmpArgs;
      wchar_t *newBuf;

      bufSize *= 2;
      newBuf = realloc(buf, bufSize * sizeof(wchar_t));
      if (!newBuf) {
         free(buf);
         buf = NULL;
         goto exit;
      }

      buf = newBuf;

      va_copy(tmpArgs, arguments);
      retval = Str_Vsnwprintf(buf, bufSize, format, tmpArgs);
      va_end(tmpArgs);
   } while (retval == -1);

   if (length) {
      *length = retval;
   }

   /*
    * Try to trim the buffer here to save memory?
    */

  exit:
   if (assertOnFailure) {
      VERIFY(buf);
   }
   return buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Aswprintf --
 *
 *    Same as Str_Vaswprintf(), but parameters are passed inline.
 *
 * Results:
 *    Same as Str_Vaswprintf()
 *
 * Side effects:
 *    Same as Str_Vaswprintf()
 *
 *-----------------------------------------------------------------------------
 */

wchar_t *
Str_Aswprintf(size_t *length,         // OUT/OPT
              const wchar_t *format,  // IN
              ...)                    // IN
{
   va_list arguments;
   wchar_t *result;

   va_start(arguments, format);
   result = Str_Vaswprintf(length, format, arguments);
   va_end(arguments);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Vaswprintf --
 *
 *    See StrVaswprintfInternal.
 *
 * Results:
 *    Returns NULL on failure.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

wchar_t *
Str_Vaswprintf(size_t *length,         // OUT/OPT
               const wchar_t *format,  // IN
               va_list arguments)      // IN
{
   return StrVaswprintfInternal(length, format, arguments, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_SafeAswprintf --
 *
 *    Same as Str_SafeVaswprintf(), but parameters are passed inline.
 *
 * Results:
 *    Same as Str_SafeVaswprintf()
 *
 * Side effects:
 *    Same as Str_SafeVaswprintf()
 *
 *-----------------------------------------------------------------------------
 */

wchar_t *
Str_SafeAswprintf(size_t *length,         // OUT/OPT
                  const wchar_t *format,  // IN
                  ...)                    // IN
{
   va_list arguments;
   wchar_t *result;

   va_start(arguments, format);
   result = Str_SafeVaswprintf(length, format, arguments);
   va_end(arguments);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_SafeVaswprintf --
 *
 *    See StrVaswprintfInternal.
 *
 * Results:
 *    Calls VERIFY on failure.
 *
 * WARNING: See warning at the top of this file.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

wchar_t *
Str_SafeVaswprintf(size_t *length,         // OUT/OPT
                   const wchar_t *format,  // IN
                   va_list arguments)      // IN
{
   return StrVaswprintfInternal(length, format, arguments, TRUE);
}

#endif // defined(_WIN32) || defined(__linux__)

#ifndef _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * Str_ToLower --
 *
 *      Convert a string to lowercase, in-place. Hand-rolled, for non-WIN32.
 *
 * Results:
 *
 *      Returns the same pointer that was passed in.
 *
 * Side effects:
 *
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_ToLower(char *string)  // IN
{
   char *c = string;

   while (*c) {
      *c = (*c >= 'A' && *c <= 'Z') ? *c + ('a' - 'A') : *c;
      c++;
   }

   return string;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_ToUpper --
 *
 *      Convert a string to uppercase, in-place. Hand-rolled, for non-WIN32.
 *
 * Results:
 *
 *      Returns the same pointer that was passed in.
 *
 * Side effects:
 *
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_ToUpper(char *string)  // IN
{
   char *c = string;

   while (*c) {
      *c = (*c >= 'a' && *c <= 'z') ? *c - ('a' - 'A') : *c;
      c++;
   }

   return string;
}

#endif // !_WIN32

#if 0

/*
 * Unit tests. Compares our bsd_vs*printf code output to C-library
 * code output, where possible.
 */

static Bool bCompare;

#define FAIL(s) \
   do { \
      printf("FAIL: %s\n", s); \
      exit(1); \
   } while (0);

static void
PrintAndCheck(char *fmt,  // IN:
              ...)        // IN:
{
   char buf1[1024], buf2[1024];
   int count;
   va_list args;

   va_start(args, fmt);
   count = Str_Vsnprintf(buf1, sizeof buf1, fmt, args);
   va_end(args);

   if (count < 0) {
      FAIL("PrintAndCheck new code count off");
   }

   va_start(args, fmt);

#ifdef _WIN32
   count = _vsnprintf(buf2, sizeof buf2, fmt, args);
#else
   count = vsnprintf(buf2, sizeof buf2, fmt, args);
#endif

   va_end(args);

   if (count < 0) {
      FAIL("PrintAndCheck old code count off");
   }

   if (bCompare && (0 != strcmp(buf1, buf2))) {
      printf("Format string: %s\n", fmt);
      printf("Our code: %s\n", buf1);
      printf("Sys code: %s\n", buf2);

      FAIL("PrintAndCheck compare failed");
   }

   printf(buf1);
}

static void
PrintAndCheckW(wchar_t *fmt, ...)
{
   wchar_t buf1[1024], buf2[1024];
   int count;
   va_list args;

   va_start(args, fmt);
   count = Str_Vsnwprintf(buf1, sizeof buf1, fmt, args);
   va_end(args);

   if (count < 0) {
      FAIL("PrintAndCheckW new code count off");
   }

   va_start(args, fmt);

#ifdef _WIN32
   count = _vsnwprintf(buf2, sizeof buf2, fmt, args);
#else
   count = vswprintf(buf2, sizeof buf2, fmt, args);
#endif

   va_end(args);

   if (count < 0) {
      FAIL("PrintAndCheckW old code count off");
   }

   if (bCompare && (0 != wcscmp(buf1, buf2))) {
      printf("Format string: %S", fmt);
      printf("Our code: %S", buf1);
      printf("Sys code: %S", buf2);

      FAIL("PrintAndCheckW compare failed");
   }

#ifndef _WIN32
   printf("%S", buf1);
#endif // _WIN32
}

void
Str_UnitTests(void)
{
   char buf[1024];
   wchar_t bufw[1024];
   int count;
   int32 num1 = 0xDEADBEEF;
   int32 num2 = 0x927F82CD;
   int64 num3 = CONST64U(0xCAFEBABE42439021);
#ifdef _WIN32
   double num4 = 5.1923843;
   double num5 = 0.000482734;
   double num6 = 8274102.3872;
#endif
   int numChars;
   char empty[1] = {'\0'};
   wchar_t wempty[1] = {L'\0'};

   /* test empty string */
   count = Str_Snprintf(buf, 1, empty);

   if (0 != count) {
      FAIL("Failed empty string test");
   }

   count = Str_Snwprintf(bufw, 1, wempty);

   if (0 != count) {
      FAIL("Failed empty string test (W)");
   }

   /* test borderline overflow */
   count = Str_Snprintf(buf, 2, "ba");

   if (-1 != count) {
      FAIL("Failed borderline overflow test - count");
   }

   if (buf[1]) {
      FAIL("Failed borderline overflow test - NULL term");
   }

   count = Str_Snwprintf(bufw, 2, L"ba");

   if (-1 != count) {
      FAIL("Failed borderline overflow test - count (W)");
   }

   if (bufw[1]) {
      FAIL("Failed borderline overflow test - NULL term (W)");
   }

   /* test egregious overflow */
   count = Str_Snprintf(buf, 2, "baabaa");

   if (-1 != count) {
      FAIL("Failed egregious overflow test - count");
   }

   if (buf[1]) {
      FAIL("Failed egregious overflow test - NULL term");
   }

   count = Str_Snwprintf(bufw, 2, L"baabaa");

   if (-1 != count) {
      FAIL("Failed egregious overflow test - count (W)");
   }

   if (bufw[1]) {
      FAIL("Failed egregious overflow test - NULL term (W)");
   }

   /* test 'n' argument */
   count = Str_Snprintf(buf, 1024, "foo %n\n", &numChars);

   if (-1 == count) {
      FAIL("Failed 'n' arg test - count");
   }

   if (4 != numChars) {
      FAIL("Failed 'n' arg test - numChars");
   }

   count = Str_Snwprintf(bufw, 1024, L"foo %n\n", &numChars);

   if (-1 == count) {
      FAIL("Failed 'n' arg test - count (W)");
   }

   if (4 != numChars) {
      FAIL("Failed 'n' arg test - numChars (W)");
   }

   bCompare = TRUE;

   // simple
   PrintAndCheck("hello\n");
   PrintAndCheckW(L"hello\n");

   // string arguments
   PrintAndCheck("whazz %s up %S doc\n", "hello", L"hello");
   PrintAndCheckW(L"whazz %s up %S doc\n", L"hello", "hello");

   // character arguments
   PrintAndCheck("whazz %c up %C doc\n", 'a', L'a');
   PrintAndCheckW(L"whazz %c up %C doc\n", L'a', 'a');

   // 32-bit integer arguments
   PrintAndCheck("%d %i %o %u %x %X\n", num1, num1, num1, num1, num1,
                 num1);
   PrintAndCheckW(L"%d %i %o %u %x %X\n", num1, num1, num1, num1, num1,
                  num1);

   // 'p' argument
   bCompare = FALSE;
   PrintAndCheck("%p\n", buf);
   PrintAndCheckW(L"%p\n", buf);
   bCompare = TRUE;

   // 64-bit
   bCompare = FALSE;
   PrintAndCheck("%LX %llX %qX\n", num3, num3, num3);
   PrintAndCheckW(L"%LX %llX %qX\n", num3, num3, num3);
   bCompare = TRUE;

   // more 64-bit
#ifdef _WIN32
   PrintAndCheck("%I64X\n", num3);
   PrintAndCheckW(L"%I64X\n", num3);
#else
   PrintAndCheck("%LX\n", num3);
   PrintAndCheckW(L"%LX\n", num3);
#endif

#ifdef _WIN32 // exponent digits printed differs vs. POSIX
   // floating-point
   PrintAndCheck("%e %E %f %g %G\n", num4, num5, num6);
   PrintAndCheckW(L"%e %E %f %g %G\n", num4, num5, num6);
#endif

   // positional arguments
   bCompare = FALSE;
   PrintAndCheck("%3$LX %1$x %2$x\n", num1, num2, num3);
   PrintAndCheckW(L"%3$LX %1$x %2$x\n", num1, num2, num3);
   bCompare = TRUE;

#ifdef _WIN32 // exponent digits printed differs vs. POSIX
   // width and precision
   PrintAndCheck("%15.1g %20.2f %*.*f\n", num6, num6, 15, 3, num6);
   PrintAndCheckW(L"%15.1g %20.2f %*.*f\n", num6, num6, 15, 3, num6);
#endif

#ifdef _WIN32 // exponent digits printed differs vs. POSIX
   // flags
   PrintAndCheck("%-15e %+f %015g\n", num4, num5, num6);
   PrintAndCheckW(L"%-15e %+f %015g\n", num4, num5, num6);
#endif

#ifdef _WIN32 // exponent digits printed differs vs. POSIX
   // more flags
   PrintAndCheck("%#X %#E %#G\n", num1, num1, num1);
   PrintAndCheckW(L"%#X %#E %#G\n", num1, num1, num1);
#endif
}

#endif // 0
