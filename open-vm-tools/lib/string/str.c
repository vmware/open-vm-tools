/*********************************************************
 * Copyright (C) 1998-2018, 2023 VMware, Inc. All rights reserved.
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
#   if !(defined(__linux__) || defined(__FreeBSD__) || defined(sun))
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
 *      int - number of bytes stored in 'str' (not including NUL terminator),
 *      -1 on overflow (insufficient space for NUL terminator is considered
 *      overflow).
 *
 *      Guaranteed to NUL-terminate if 'size' > 0.
 *
 *      NB: on overflow the buffer WILL be NUL terminated at the last
 *      UTF-8 code point boundary within the buffer's bounds.
 *
 * WARNING:
 *      Behavior of this function is guaranteed only if HAS_BSD_PRINTF is
 *      enabled.
 *
 *      See the warning at the top of this file for proper va_list usage.
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

#if defined HAS_BSD_PRINTF
   retval = bsd_vsnprintf(&str, size, format, ap);
#else
   /*
    * Linux glibc 2.0.x (which we shouldn't be linking against) returns -1, but
    * glibc 2.1.x follows c99 and returns the number characters (excluding NUL)
    * that would have been written if given a sufficiently large buffer.
    *
    * In the case of Win32, this path uses _vsnprintf(), which returns -1 on
    * overflow, returns size when result fits exactly, and does not NUL
    * terminate in those cases.
    */
   retval = vsnprintf(str, size, format, ap);
#endif

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
 *      See Str_Vsnprintf.
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
 *-----------------------------------------------------------------------------
 *
 * Str_Strncpy --
 *
 *      Unlike strncpy:
 *      * Guaranteed to NUL-terminate.
 *      * If the src string is shorter than n bytes, does NOT zero-fill the
 *        remaining bytes.
 *      * Panics if a buffer overrun would have occurred.
 *
 * Results:
 *      Same as strncpy.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_Strncpy(char *dest,       // IN/OUT
            size_t destSize,  // IN: Size of dest
            const char *src,  // IN: String to copy
            size_t n)         // IN: Max chars of src to copy, not including NUL
{
   ASSERT(dest != NULL);
   ASSERT(src != NULL);

   n = Str_Strlen(src, n);

   if (n >= destSize) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   memcpy(dest, src, n);
   dest[n] = '\0';
   return dest;
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
 *    Guaranteed to NUL-terminate.
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
    * If bufLen + n is OK, we know things will fit (and avoids a strlen(src)).
    * If bufLen + n looks too big, they still might fit if the src is short
    * enough... so repeat the check using strlen(src).
    *
    * The "fit" means less than, as strncat always adds a terminating NUL.
    */

   bufLen = strlen(buf);
   bufLen = MIN(bufLen, bufSize);  // Prevent potential overflow

   if (!(bufLen + n < bufSize ||
         bufLen + strlen(src) < bufSize)) {
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
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

#if defined HAS_BSD_PRINTF
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

#if defined(_WIN32) // {

/*
 *----------------------------------------------------------------------
 *
 * Str_Swprintf --
 *
 *      swprintf wrapper that fails on overflow
 *
 * Results:
 *      Returns the number of wchar_ts stored in 'buf'.
 *
 * WARNING:
 *      Behavior of this function is guaranteed only if HAS_BSD_PRINTF is
 *      enabled.
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
 * WARNING:
 *      Behavior of this function is guaranteed only if HAS_BSD_PRINTF is
 *      enabled.
 *
 *      See the warning at the top of this file for proper va_list usage.
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

#if defined HAS_BSD_PRINTF
   retval = bsd_vsnwprintf(&str, size, format, ap);
#elif defined(_WIN32)
   /*
    * _vsnwprintf() returns -1 on overflow, returns size when result fits
    * exactly, and does not NUL terminate in those cases.
    */
   retval = _vsnwprintf(str, size, format, ap);
   if ((retval < 0 || retval >= size) && size > 0) {
      str[size - 1] = L'\0';
   }
#else
   /*
    * There is no standard vsnwprintf function.  vswprintf is close, but unlike
    * vsnprintf, vswprintf always returns a negative value if truncation
    * occurred.  Additionally, the state of the destination buffer on failure
    * is not specified.  Although the C99 specification says that [v]swprintf
    * should always NUL-terminate, glibc (as of 2.24) is non-conforming and
    * seems to leave the final element untouched.
    */
   retval = vswprintf(str, size, format, ap);
   if (retval < 0 && size > 0) {
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
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
   }

   /*
    * We don't need to worry about NUL termination, because it's only
    * needed on overflow and we Panic above in that case.
    */

   return wcsncat(buf, src, n);
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

#endif // } defined(_WIN32)

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
 * Unit tests.
 */

#define FAIL(s) \
   do { \
      printf("FAIL: %s\n", s); \
      exit(1); \
   } while (0);


static void
CheckPrintf(const char *expected,  // IN
            const char *fmt,       // IN
            ...)                   // IN
{
#if !defined HAS_BSD_PRINTF
   NOT_TESTED_ONCE();
#else
   char buf[1024] = "";
   int count;
   int expectedCount = strlen(expected);
   va_list args;

   va_start(args, fmt);
   count = Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   // Verify there's a NUL somewhere since we print the buffer on failure.
   VERIFY(buf[ARRAYSIZE(buf) - 1] == '\0');

   if (count == expectedCount && strcmp(buf, expected) == 0) {
      // Success.
      return;
   }

   printf("%s\n", buf);
   printf("Format string: %s\n", fmt);
   printf("Expected count: %d\n", expectedCount);
   printf("Expected output: %s\n", expected);
   printf("Actual count: %d\n", count);
   printf("Actual output: %s\n", buf);
   FAIL("CheckPrintf");
#endif
}


static void
CheckWPrintf(const wchar_t *expected, // IN
             const wchar_t *fmt,      // IN
             ...)                     // IN
{
#if !defined HAS_BSD_PRINTF
   NOT_TESTED_ONCE();
#else
   wchar_t buf[1024] = L"";
   int count;
   int expectedCount = wcslen(expected);
   va_list args;

   va_start(args, fmt);
   count = Str_Vsnwprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   // Verify there's a NUL somewhere since we print the buffer on failure.
   VERIFY(buf[ARRAYSIZE(buf) - 1] == L'\0');

   if (count == expectedCount && wcscmp(buf, expected) == 0) {
      // Success.
      return;
   }

   /*
    * %S isn't standard, and since we use the system printf here, we must use
    * %ls instead.
    */
   printf("%ls\n", buf);
   printf("Format string: %ls\n", fmt);
   printf("Expected count: %d\n", expectedCount);
   printf("Expected output: %ls\n", expected);
   printf("Actual count: %d\n", count);
   printf("Actual output: %ls\n", buf);
   FAIL("CheckWPrintf");
#endif
}


void
Str_UnitTests(void)
{
   char buf[1024];
   wchar_t bufw[1024];
   int count;
   const void *p = (void*) 0xFEEDFACE;
   int32 num1 = 0xDEADBEEF;
   int32 num2 = 0x927F82CD;
   int64 num3 = CONST64U(0xCAFEBABE42439021);
   const double d[] = { 5.0, 2017.0, 0.000482734, 8274102.3872 };
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

   // simple
   CheckPrintf("hello", "hello");
   CheckWPrintf(L"hello", L"hello");

   // string arguments
   CheckPrintf("whazz hello up hello doc",
               "whazz %s up %S doc", "hello", L"hello");
   CheckWPrintf(L"whazz hello up hello doc",
                L"whazz %s up %S doc", "hello", L"hello");

   // character arguments
   CheckPrintf("whazz a up a doc",
               "whazz %c up %C doc", 'a', L'a');
   CheckWPrintf(L"whazz a up a doc",
                L"whazz %c up %C doc", 'a', L'a');

   // 32-bit integer arguments
   CheckPrintf("-559038737 -559038737 33653337357 3735928559 deadbeef DEADBEEF",
               "%d %i %o %u %x %X", num1, num1, num1, num1, num1, num1);
   CheckWPrintf(L"-559038737 -559038737 33653337357 3735928559 deadbeef DEADBEEF",
                L"%d %i %o %u %x %X", num1, num1, num1, num1, num1, num1);

   // 'p' argument
   CheckPrintf("FEEDFACE", "%p", p);
   CheckWPrintf(L"FEEDFACE", L"%p", p);

   // 64-bit
   CheckPrintf("CAFEBABE42439021",
               "%LX", num3);
   CheckWPrintf(L"CAFEBABE42439021",
                L"%LX", num3);
   CheckPrintf("CAFEBABE42439021",
               "%llX", num3);
   CheckWPrintf(L"CAFEBABE42439021",
                L"%llX", num3);
   CheckPrintf("CAFEBABE42439021",
               "%qX", num3);
   CheckWPrintf(L"CAFEBABE42439021",
                L"%qX", num3);
   CheckPrintf("CAFEBABE42439021",
               "%I64X", num3);
   CheckWPrintf(L"CAFEBABE42439021",
                L"%I64X", num3);

   // floating-point
   {
      const char *expected[] = {
         "5.000000e+00 5.000000E+00 5.000000 5 5",
         "2.017000e+03 2.017000E+03 2017.000000 2017 2017",
         "4.827340e-04 4.827340E-04 0.000483 0.000482734 0.000482734",
         "8.274102e+06 8.274102E+06 8274102.387200 8.2741e+06 8.2741E+06",
      };
      const wchar_t *expectedW[] = {
         L"5.000000e+00 5.000000E+00 5.000000 5 5",
         L"2.017000e+03 2.017000E+03 2017.000000 2017 2017",
         L"4.827340e-04 4.827340E-04 0.000483 0.000482734 0.000482734",
         L"8.274102e+06 8.274102E+06 8274102.387200 8.2741e+06 8.2741E+06",
      };

      size_t i;

      ASSERT_ON_COMPILE(ARRAYSIZE(d) == ARRAYSIZE(expected));
      ASSERT_ON_COMPILE(ARRAYSIZE(d) == ARRAYSIZE(expectedW));

      for (i = 0; i < ARRAYSIZE(d); i++) {
         CheckPrintf(expected[i],
                     "%e %E %f %g %G", d[i], d[i], d[i], d[i], d[i]);
         CheckWPrintf(expectedW[i],
                      L"%e %E %f %g %G", d[i], d[i], d[i], d[i], d[i]);
      }
   }

   // positional arguments
   CheckPrintf("CAFEBABE42439021 deadbeef 927f82cd",
               "%3$LX %1$x %2$x", num1, num2, num3);
   CheckWPrintf(L"CAFEBABE42439021 deadbeef 927f82cd",
                L"%3$LX %1$x %2$x", num1, num2, num3);

   // width and precision
   CheckPrintf("          8e+06           8274102.39     8274102.387",
               "%15.1g %20.2f %*.*f", d[3], d[3], 15, 3, d[3]);
   CheckWPrintf(L"          8e+06           8274102.39     8274102.387",
                L"%15.1g %20.2f %*.*f", d[3], d[3], 15, 3, d[3]);

   // flags
   CheckPrintf("5.000000e+00    +0.000483 000008.2741e+06",
               "%-15e %+f %015g", d[0], d[2], d[3]);
   CheckWPrintf(L"5.000000e+00    +0.000483 000008.2741e+06",
                L"%-15e %+f %015g", d[0], d[2], d[3]);

   // more flags
   CheckPrintf("0XDEADBEEF 5.000000E+00 2017.00",
               "%#X %#E %#G", num1, d[0], d[1]);
   CheckWPrintf(L"0XDEADBEEF 5.000000E+00 2017.00",
                L"%#X %#E %#G", num1, d[0], d[1]);

   printf("%s succeeded.\n", __FUNCTION__);
}

#endif // 0
