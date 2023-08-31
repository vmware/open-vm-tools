/*********************************************************
 * Copyright (c) 1998-2016,2022 VMware, Inc. All rights reserved.
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
 * cpName.c --
 *
 *    Shared portions of cross-platform name conversion routines used
 *    by hgfs. [bac]
 *
 */

#ifdef sun
#include <string.h>
#endif

#include "cpName.h"
#include "cpNameInt.h"
#include "vm_assert.h"
#include "hgfsEscape.h"

/*
 *----------------------------------------------------------------------
 *
 * CPName_GetComponent --
 *
 *    Get the next component of the CP name.
 *
 *    Returns the length of the component starting with the begin
 *    pointer, and a pointer to the next component in the buffer, if
 *    any. The "next" pointer is set to "end" if there is no next
 *    component.
 *
 * Results:
 *    length (not including NUL termination) >= 0 of next
 *    component on success.
 *    error < 0 on failure (invalid component).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPName_GetComponent(char const *begin,   // IN: Beginning of buffer
                    char const *end,     // IN: End of buffer
                    char const **next)   // OUT: Start of next component
{
   char const *walk;
   char const *myNext;
   size_t len;

   ASSERT(begin);
   ASSERT(end);
   ASSERT(next);
   ASSERT(begin <= end);

   for (walk = begin; ; walk++) {
      if (walk == end) {
         /* End of buffer. No NUL was found */

         myNext = end;
         break;
      }

      if (*walk == '\0') {
         /* Found a NUL */

         if (walk == begin) {
            Log("%s: error: first char can't be NUL\n", __FUNCTION__);
            return -1;
         }

         myNext = walk + 1;
         /* Skip consecutive path delimiters. */
         while ((*myNext == '\0') && (myNext != end)) {
            myNext++;
         }
         if (myNext == end) {
            /* Last character in the buffer is not allowed to be NUL */
            Log("%s: error: last char can't be NUL\n", __FUNCTION__);
            return -1;
         }

         break;
      }
   }

   len = walk - begin;

   *next = myNext;
   return (int) len;
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameEscapeAndConvertFrom --
 *
 *    Converts a cross-platform name representation into a string for
 *    use in the local filesystem.
 *    Escapes illegal characters as a part of convertion.
 *    This is a cross-platform implementation and takes the path separator
 *    argument as an argument. The path separator is prepended before each
 *    additional path component, so this function never adds a trailing path
 *    separator.
 *
 * Results:
 *    0 on success.
 *    error < 0 on failure (the converted string did not fit in
 *    the buffer provided or the input was invalid).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPNameEscapeAndConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                           size_t *inSize,     // IN/OUT: Size of input
                           size_t *outSize,    // IN/OUT: Size of output buffer
                           char **bufOut,      // IN/OUT: Output buffer
                           char pathSep)       // IN: Path separator character
{
   int result;
   int inputSize;
   inputSize = HgfsEscape_GetSize(*bufIn, *inSize);
   if (inputSize < 0) {
      result = -1;
   } else if (inputSize != 0) {
      char *savedBufOut = *bufOut;
      char const *savedOutConst = savedBufOut;
      size_t savedOutSize = *outSize;
      if (inputSize > *outSize) {
         Log("%s: error: not enough room for escaping\n", __FUNCTION__);
         return -1;
      }

      /* Leaving space for the leading path separator, thus output to savedBufOut + 1. */
      result = HgfsEscape_Do(*bufIn, *inSize, savedOutSize - 1, savedBufOut + 1);
      if (result < 0) {
         Log("%s: error: not enough room to perform escape: %d\n",
             __FUNCTION__, result);
          return -1;
      }
      *inSize = (size_t) result;

      result = CPNameConvertFrom(&savedOutConst, inSize, outSize, bufOut, pathSep);
      *bufIn += *inSize;
      *inSize = 0;
   } else {
      result = CPNameConvertFrom(bufIn, inSize, outSize, bufOut, pathSep);
   }
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameConvertFrom --
 *
 *    Converts a cross-platform name representation into a string for
 *    use in the local filesystem. This is a cross-platform
 *    implementation and takes the path separator argument as an
 *    argument. The path separator is prepended before each additional
 *    path component, so this function never adds a trailing path
 *    separator.
 *
 * Results:
 *    0 on success.
 *    error < 0 on failure (the converted string did not fit in
 *    the buffer provided or the input was invalid).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPNameConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                  size_t *inSize,     // IN/OUT: Size of input
                  size_t *outSize,    // IN/OUT: Size of output buffer
                  char **bufOut,      // IN/OUT: Output buffer
                  char pathSep)       // IN: Path separator character
{
   char const *in;
   char const *inEnd;
   size_t myOutSize;
   char *out;
   Bool inPlaceConvertion = (*bufIn == *bufOut);

   ASSERT(bufIn);
   ASSERT(inSize);
   ASSERT(outSize);
   ASSERT(bufOut);

   in = *bufIn;
   inEnd = in + *inSize;
   if (inPlaceConvertion) {
      in++; // Skip place for the leading path separator.
   }
   myOutSize = *outSize;
   out = *bufOut;

   for (;;) {
      char const *next;
      int len;
      int newLen;

      len = CPName_GetComponent(in, inEnd, &next);
      if (len < 0) {
         Log("%s: error: get next component failed\n", __FUNCTION__);
         return len;
      }

      /* Bug 27926 - preventing escaping from shared folder. */
      if ((len == 1 && *in == '.') ||
          (len == 2 && in[0] == '.' && in[1] == '.')) {
         Log("%s: error: found dot/dotdot\n", __FUNCTION__);
         return -1;
      }

      if (len == 0) {
         /* No more component */
         break;
      }

      newLen = ((int) myOutSize) - len - 1;
      if (newLen < 0) {
         Log("%s: error: not enough room\n", __FUNCTION__);
         return -1;
      }
      myOutSize = (size_t) newLen;

      *out++ = pathSep;
      if (!inPlaceConvertion) {
         memcpy(out, in, len);
      }
      out += len;

      in = next;
   }

   /* NUL terminate */
   if (myOutSize < 1) {
      Log("%s: error: not enough room\n", __FUNCTION__);
      return -1;
   }
   *out = '\0';

   /* Path name size should not require more than 4 bytes. */
   ASSERT((in - *bufIn) <= 0xFFFFFFFF);

   /* Update pointers. */
   *inSize -= (in - *bufIn);
   *outSize = myOutSize;
   *bufIn = in;
   *bufOut = out;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_Print --
 *
 *    Converts a CPName formatted string to a valid, NUL-terminated string by
 *    replacing all embedded NUL characters with '|'.
 *
 * Results:
 *    Pointer to a static buffer containing the converted string.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

char const *
CPName_Print(char const *in, // IN: Name to print
             size_t size)    // IN: Size of name
{
   /* Static so it does not go on a kernel stack --hpreg */
   static char out[128];
   size_t i;

   ASSERT(in);

   ASSERT(sizeof out >= 4);
   if (size > sizeof out - 1) {
      size = sizeof out - 4;
      out[size] = '.';
      out[size + 1] = '.';
      out[size + 2] = '.';
      out[size + 3] = '\0';
   } else {
      out[size] = '\0';
   }

   for (i = 0; i < size; i++) {
      out[i] = in[i] != '\0' ? in[i] : '|';
   }

   return out;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_LinuxConvertTo --
 *
 *    Wrapper function that calls CPNameConvertTo() with the correct arguments
 *    for Linux path conversions.
 *
 *    Makes a cross-platform name representation from the Linux path input
 *    string and writes it into the output buffer.
 *
 * Results:
 *    On success, returns the number of bytes used in the cross-platform name,
 *    NOT including the final terminating NUL character.  On failure, returns
 *    a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPName_LinuxConvertTo(char const *nameIn, // IN:  Buf to convert
                      size_t bufOutSize,  // IN:  Size of the output buffer
                      char *bufOut)       // OUT: Output buffer
{
   return CPNameConvertTo(nameIn, bufOutSize, bufOut, '/');
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_WindowsConvertTo --
 *
 *    Wrapper function that calls CPNameConvertTo() with the correct arguments
 *    for Windows path conversions.
 *
 *    Makes a cross-platform name representation from the Linux path input
 *    string and writes it into the output buffer.
 *
 * Results:
 *    On success, returns the number of bytes used in the cross-platform name,
 *    NOT including the final terminating NUL character.  On failure, returns
 *    a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPName_WindowsConvertTo(char const *nameIn, // IN:  Buf to convert
                        size_t bufOutSize,  // IN:  Size of the output buffer
                        char *bufOut)       // OUT: Output buffer
{
   return CPNameConvertTo(nameIn, bufOutSize, bufOut, '\\');
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameConvertTo --
 *
 *    Makes a cross-platform name representation from the input string
 *    and writes it into the output buffer.
 *    HGFS convention is to echange names between guest and host in uescaped form.
 *    Both ends perform necessary name escaping according to its own rules
 *    to avoid presenitng invalid file names to OS. Thus the name needs to be unescaped
 *    as a part of conversion to host-independent format.
 *
 * Results:
 *    On success, returns the number of bytes used in the
 *    cross-platform name, NOT including the final terminating NUL
 *    character. On failure, returns a negative error.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPNameConvertTo(char const *nameIn, // IN:  Buf to convert
                size_t bufOutSize,  // IN:  Size of the output buffer
                char *bufOut,       // OUT: Output buffer
                char pathSep)       // IN:  path separator to use
{
   char *origOut = bufOut;
   char const *endOut = bufOut + bufOutSize;
   size_t cpNameLength = 0;

   ASSERT(nameIn);
   ASSERT(bufOut);

   /* Skip any path separators at the beginning of the input string */
   while (*nameIn == pathSep) {
      nameIn++;
   }

    /*
     * Copy the string to the output buf, converting all path separators into '\0'.
     * Collapse multiple consecutive path separators into a single one since
     * CPName_GetComponent can't handle consecutive path separators.
     */
   while (*nameIn != '\0' && bufOut < endOut) {
      if (*nameIn == pathSep) {
         *bufOut = '\0';
         do {
            nameIn++;
         } while (*nameIn == pathSep);
      } else {
         *bufOut = *nameIn;
         nameIn++;
      }
      bufOut++;
   }

   /*
    * NUL terminate. XXX This should go away.
    *
    * When we get rid of NUL termination here, this test should
    * also change to "if (*nameIn != '\0')".
    */
   if (bufOut == endOut) {
      return -1;
   }
   *bufOut = '\0';

   /* Path name size should not require more than 4 bytes. */
   ASSERT((bufOut - origOut) <= 0xFFFFFFFF);

   /* If there were any trailing path separators, dont count them [krishnan] */
   cpNameLength = bufOut - origOut;
   while ((cpNameLength >= 1) && (origOut[cpNameLength - 1] == 0)) {
      cpNameLength--;
   }
   cpNameLength = HgfsEscape_Undo(origOut, cpNameLength);

   /* Return number of bytes used */
   return (int) cpNameLength;
}
