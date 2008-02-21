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
 * unicodeBase.h --
 *
 *      Basic Unicode string creation and encoding conversion.
 */

#ifndef _UNICODE_BASE_H_
#define _UNICODE_BASE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMNIXMOD
#include "includeCheck.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <errno.h>
#include "util.h"
#include "unicodeTypes.h"

/*
 * Static Unicode string literal macros.
 *
 * Given an ASCII const char * literal ("abcd"), returns a static
 * ConstUnicode string.
 *
 * These strings are never freed, so this is not to be used for
 * general runtime Unicode string creation. Instead, use it to
 * replace:
 *
 *   const char *extension = ".vmdk";
 *
 * with:
 *
 *   ConstUnicode extension = U(".vmdk");
 */
#define U(x) Unicode_GetStatic(x, FALSE)

/*
 * Same as U(), but also unescapes \\uABCD to Unicode code point
 * U+ABCD, and \\U001FABCD to Unicode code point U+1FABCD.
 *
 * Use to replace:
 *
 *   const char *utf8Copyright = "Copyright \302\251 COMPANY_NAME";
 *
 * with:
 *
 *   ConstUnicode copyright = U_UNESCAPE("Copyright \\u00A9 COMPANY_NAME");
 */
#define U_UNESCAPE(x) Unicode_GetStatic(x, TRUE)


/*
 * Allocates a Unicode string given a buffer of the specified length
 * (not necessarily NUL-terminated) in the specified encoding.
 *
 * Returns NULL if the buffer was invalid or memory could not be
 * allocated.
 */

Unicode Unicode_AllocWithLength(const void *buffer,
                                ssize_t lengthInBytes,
                                StringEncoding encoding);


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Alloc --
 *
 *      Allocates a new Unicode string given a NUL-terminated buffer
 *      of bytes in the specified string encoding.
 *
 *      Note that regardless of the encoding of the buffer passed to this
 *      function, the returned string can hold any Unicode characters.
 *
 *      If the buffer contains an invalid sequence of the specified
 *      encoding or memory could not be allocated, returns NULL.
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL on failure.  Caller must pass to
 *      Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_Alloc(const void *buffer,      // IN
              StringEncoding encoding) // IN
{
   return Unicode_AllocWithLength(buffer, -1, encoding);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AllocWithUTF8 --
 *
 *      Allocates a new Unicode string given a NUL-terminated UTF-8 string.
 *
 *      If the input contains an invalid UTF-8 byte sequence or memory could
 *      not be allocated, returns NULL.
 *
 * Results:
 *      An allocated Unicode string containing the characters in
 *      utf8String, or NULL on failure.  Caller must pass to
 *      Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_AllocWithUTF8(const char *utf8String) // IN
{
   return Unicode_AllocWithLength(utf8String, -1, STRING_ENCODING_UTF8);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AllocWithUTF16 --
 *
 *      Allocates a new Unicode string given a NUL-terminated UTF-16
 *      string in host-endian order.
 *
 *      If the input contains an invalid UTF-16 sequence or memory could
 *      not be allocated, returns NULL.
 *
 * Results:
 *      An allocated Unicode string containing the characters in
 *      utf16String, or NULL on failure.  Caller must pass to
 *      Unicode_Free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Unicode
Unicode_AllocWithUTF16(const utf16_t *utf16String) // IN
{
   return Unicode_AllocWithLength(utf16String, -1, STRING_ENCODING_UTF16);
}


Unicode Unicode_Duplicate(ConstUnicode str);
void Unicode_Free(Unicode str);

/*
 * Accessors for NUL-terminated UTF-8 and UTF-16 host-endian
 * representations of this Unicode string.
 *
 * Memory is managed inside this string, so callers do not need to
 * free.
 */
const char *Unicode_GetUTF8(ConstUnicode str);
const utf16_t *Unicode_GetUTF16(ConstUnicode str);

static INLINE const char *
UTF8(ConstUnicode str)
{
   const char *result;

   int errP = errno;
#if defined(_WIN32)
   DWORD errW = GetLastError();
#endif

   result = Unicode_GetUTF8(str);

#if defined(_WIN32)
   SetLastError(errW);
#endif
   errno = errP;

   return result;
}

static INLINE const utf16_t *
UTF16(ConstUnicode str)
{
   const utf16_t *result;

   int errP = errno;
#if defined(_WIN32)
   DWORD errW = GetLastError();
#endif

   result = Unicode_GetUTF16(str);

#if defined(_WIN32)
   SetLastError(errW);
#endif

   errno = errP;

   return result;
}

/*
 * Gets the number of UTF-16 code units in the NUL-terminated UTF-16 array.
 */
ssize_t Unicode_UTF16Strlen(const utf16_t *utf16);

/*
 * Tests if the buffer's bytes are valid in the specified encoding.
 */
Bool Unicode_IsBufferValid(const void *buffer,
                           ssize_t lengthInBytes,
                           StringEncoding encoding);

/*
 * Returns the length in number of native code units (UTF-8 bytes or
 * UTF-16 words) of the string.
 */
UnicodeIndex Unicode_LengthInCodeUnits(ConstUnicode str);

/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_IsEmpty --
 *
 *      Test if the Unicode string is empty.
 *
 * Results:
 *      TRUE if the string has length 0, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
Unicode_IsEmpty(ConstUnicode str)  // IN:
{
   return Unicode_LengthInCodeUnits(str) == 0;
}


/*
 * Efficiently returns the upper bound on the number of bytes required
 * to encode the Unicode string in the specified encoding, including
 * NUL termination.
 */
size_t Unicode_BytesRequired(ConstUnicode str,
                             StringEncoding encoding);

/*
 * Extracts the contents of the Unicode string into a NUL-terminated
 * buffer using the specified encoding.  Returns -1 if
 * maxLengthInBytes isn't enough to hold the encoded string.
 */
ssize_t Unicode_CopyBytes(ConstUnicode str,
                          void *buffer,
                          size_t maxLengthInBytes,
                          StringEncoding encoding);


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocBytes --
 *
 *      Allocates and returns a NUL terminated buffer into which the contents
 *      of the unicode string are extracted using the specified encoding.
 *
 *      NOTE: The length of the NUL can depend on the encoding.
 *            UTF-16 NUL is "\0\0"; UTF-32 NUL is "\0\0\0\0".
 *
 * Results:
 *      Pointer to the dynamically allocated memory. The caller is
 *      responsible to free the memory allocated by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
Unicode_GetAllocBytes(ConstUnicode str,        // IN:
                      StringEncoding encoding) // IN:
{
   void *memory;
   ssize_t result;

   size_t length = Unicode_BytesRequired(str, encoding);

   memory = Util_SafeMalloc(length);

   result = Unicode_CopyBytes(str, memory, length, encoding);

   ASSERT_NOT_IMPLEMENTED(result != -1);

   return memory;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocUTF16 --
 *
 *      Allocates and returns a NUL terminated buffer into which the contents
 *      of the unicode string are extracted using the (host native) UTF-16
 *      encoding.
 *
 *      NOTE: The length of the NUL can depend on the encoding.
 *            UTF-16 NUL is "\0\0"; UTF-32 NUL is "\0\0\0\0".
 *
 * Results:
 *      Pointer to the dynamically allocated memory or NULL on failure.
 *
 *      Caller is responsible to free the memory allocated by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE utf16_t *
Unicode_GetAllocUTF16(ConstUnicode str)  // IN:
{
   /* Cast for compatibility with C++ compilers. */
   return (utf16_t *) Unicode_GetAllocBytes(str, STRING_ENCODING_UTF16);
}


/*
 * Helper function for Unicode string literal macros.
 */
ConstUnicode Unicode_GetStatic(const char *asciiBytes,
                               Bool unescape);


/*
 * XXX Helper macros for win32 Unicode string transition stage.
 *     Those macros should be removed after we turn on SUPPORT_UNICODE.
 *     We need to call free() when !SUPPORT_UNICODE because Unicode_GetAllocUTF16()
 *     creates a new copy of 's' in UTF-16. But when SUPPORT_UNICODE, Unicode_GetUTF16()
 *     just returns a const representation of the string 's'.
 */

#if defined(_WIN32)
   #if defined(SUPPORT_UNICODE)
      #define UNICODE_GET_UTF16(s)     Unicode_GetUTF16(s)
      #define UNICODE_RELEASE_UTF16(s)
   #else
      #define UNICODE_GET_UTF16(s)     Unicode_GetAllocUTF16(s)
      #define UNICODE_RELEASE_UTF16(s) free((utf16_t *)s)
   #endif
#endif

#ifdef __cplusplus
}
#endif

#endif // _UNICODE_BASE_H_
