/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * unicodeBase.h --
 *
 *      Basic Unicode string creation and encoding conversion.
 */

#ifndef _UNICODE_BASE_H_
#define _UNICODE_BASE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#define INCLUDE_ALLOW_VMKERNEL
#include "includeCheck.h"

#include <stdlib.h>
#include <errno.h>
#include "util.h"
#include "unicodeTypes.h"
#include "err.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define UNICODE_SUBSTITUTION_CHAR "\xEF\xBF\xBD"

/*
 * Unescapes \\uABCD in string literals to Unicode code point
 * U+ABCD, and \\U001FABCD to Unicode code point U+1FABCD.
 *
 * The resulting string is never freed, so this is not to be used for
 * general runtime Unicode string creation.
 *
 * Use to replace:
 *
 *   const char *utf8Copyright = "Copyright \302\251 COMPANY_NAME";
 *
 * with:
 *
 *   const char *copyright = U_UNESCAPE("Copyright \\u00A9 COMPANY_NAME");
 */

#define U_UNESCAPE(x) Unicode_GetStatic(x, TRUE)


/*
 * In contexts where an errno makes sense, use this
 * to report conversion failure.
 */

#ifndef _WIN32
#define UNICODE_CONVERSION_ERRNO EINVAL 
#endif


/*
 * Allocates a Unicode string given a buffer of the specified length
 * (not necessarily NUL-terminated) in the specified encoding.
 *
 * Returns NULL if the buffer was invalid or memory could not be
 * allocated.
 */

char *Unicode_AllocWithLength(const void *buffer,
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
 *	If buffer is NULL, then NULL is returned.
 *
 *      Note that regardless of the encoding of the buffer passed to this
 *      function, the returned string can hold any Unicode characters.
 *
 * Results:
 *      An allocated Unicode string containing the decoded characters
 *      in buffer, or NULL if input is NULL.  Caller must pass to
 *      free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char *
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
 *	If utf8String is NULL, then NULL is returned.
 *
 * Results:
 *      An allocated Unicode string containing the characters in
 *      utf8String, or NULL if utf8String is NULL.  Caller must pass to
 *      free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char *
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
 *	If utf16String is NULL, then NULL is returned.
 *
 * Results:
 *      An allocated Unicode string containing the characters in
 *      utf16String, or NULL if utf16String is NULL.  Caller must pass to
 *      free to free.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char *
Unicode_AllocWithUTF16(const utf16_t *utf16String) // IN
{
   return Unicode_AllocWithLength(utf16String, -1, STRING_ENCODING_UTF16);
}


char *Unicode_Duplicate(const char *str);

char **Unicode_AllocList(char **srcList, ssize_t length,
                         StringEncoding encoding);

char **Unicode_GetAllocList(char *const srcList[],
                            ssize_t length,
                            StringEncoding encoding);

/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_AllocListWithUTF16 --
 *
 *      Allocates a list (actually a vector) of Unicode strings from a list
 *      (vector) of UTF-16 strings.  The input list has a specified length or
 *      can be an argv-style NULL-terminated list (if length is negative).
 *
 * Results:
 *      An allocated list (vector) of Unicode strings.
 *      The result must be freed with Util_FreeStringList.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char **
Unicode_AllocListWithUTF16(utf16_t **utf16list, // IN:
                           ssize_t length)      // IN:
{
   return Unicode_AllocList((char **) utf16list, length,
			    STRING_ENCODING_UTF16);
}

/*
 * Compute the number of bytes in a string.
 */
ssize_t Unicode_LengthInBytes(const void *buffer,
                              StringEncoding encoding);

/*
 * Gets the number of UTF-16 code units in the NUL-terminated UTF-16 array.
 */
ssize_t Unicode_UTF16Strlen(const utf16_t *utf16);

/*
 * Duplicates a UTF-16 string.
 */
utf16_t *Unicode_UTF16Strdup(const utf16_t *utf16);

/*
 * Tests if the buffer's bytes are valid in the specified encoding.
 */
Bool Unicode_IsBufferValid(const void *buffer,
                           ssize_t lengthInBytes,
                           StringEncoding encoding);

Bool Unicode_IsStringValidUTF8(const char *str);

/*
 * Tests if the buffer's unicode contents can be converted to the
 * specified encoding.
 */
Bool Unicode_CanGetBytesWithEncoding(const char *ustr,
                                     StringEncoding encoding);

/*
 * Escape non-printable bytes of the buffer with \xAB, where 0xAB
 * is the non-printable byte value.
 */
char *Unicode_EscapeBuffer(const void *buffer,
                           ssize_t lengthInBytes,
                           StringEncoding encoding);

/*
 * Returns the length in number of native code units (UTF-8 bytes or
 * UTF-16 words) of the string.
 */
UnicodeIndex Unicode_LengthInCodeUnits(const char *str);

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
Unicode_IsEmpty(const char *str)  // IN:
{
   ASSERT(str != NULL);
   return str[0] == '\0';
}


/*
 * Efficiently returns the upper bound on the number of bytes required
 * to encode the Unicode string in the specified encoding, including
 * NUL termination.
 */
size_t Unicode_BytesRequired(const char *str,
                             StringEncoding encoding);

/*
 * Extracts the contents of the Unicode string into a NUL-terminated
 * buffer using the specified encoding. Copies at most
 * maxLengthInBytes including NUL termination. Returns FALSE if
 * truncation occurred, TRUE otherwise. If retLength is not NULL,
 * *retLength holds the number of bytes actually copied, not including
 * the NUL termination, upon return.
 */
Bool Unicode_CopyBytes(void *destBuffer,
                       const char *srcBuffer,
                       size_t maxLengthInBytes,
                       size_t *retLength,
                       StringEncoding encoding);

void *Unicode_GetAllocBytes(const char *str,
                            StringEncoding encoding);

void *Unicode_GetAllocBytesWithLength(const char *str,
                                      StringEncoding encoding,
                                      ssize_t lengthInBytes);


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_GetAllocUTF16 --
 *
 *      Allocates and returns a NUL terminated buffer into which the contents
 *      of the unicode string are extracted using the (host native) UTF-16
 *      encoding.  (Note that UTF-16 NUL is two bytes: "\0\0".)
 *
 *      NULL is returned for NULL argument.
 *
 * Results:
 *      Pointer to the dynamically allocated memory,
 *      or NULL on NULL argument.
 *
 *      Caller is responsible to free the memory allocated by this routine.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE utf16_t *
Unicode_GetAllocUTF16(const char *str)  // IN:
{
   /* Cast for compatibility with C++ compilers. */
   return (utf16_t *) Unicode_GetAllocBytes(str, STRING_ENCODING_UTF16);
}


/*
 * Helper function for Unicode string literal macros.
 */
const char *Unicode_GetStatic(const char *asciiBytes,
                              Bool unescape);

/*
 * Helper macros for Win32 Unicode string transition.
 */
#if defined(_WIN32)
   #define UNICODE_GET_UTF16(s)     Unicode_GetAllocUTF16(s)
   #define UNICODE_RELEASE_UTF16(s) WITH_ERRNO_FREE(s)
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _UNICODE_BASE_H_
