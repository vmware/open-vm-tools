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
 * cpNameLinux.c --
 *
 *    Linux implementation of cross-platform name conversion
 *    routines used by hgfs. [bac]
 *
 */

#if defined sun && !defined SOL9
#include <memory.h>
#endif

#include "cpName.h"
#include "cpNameInt.h"
#include "vm_assert.h"
#include "hgfsEscape.h"


/*
 *----------------------------------------------------------------------
 *
 * CPName_ConvertFrom --
 *
 *    Converts a cross-platform name representation into a string for
 *    use in the local filesystem.
 *
 * Results:
 *    Length (not including NUL termination) >= 0 of resulting
 *    string on success.
 *    Negative error on failure (the converted string did not fit in
 *    the buffer provided or the input was invalid).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPName_ConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                   size_t *inSize,     // IN/OUT: Size of input
                   size_t *outSize,    // IN/OUT: Size of output buffer
                   char **bufOut)      // IN/OUT: Output buffer
{
   ASSERT(bufIn);
   ASSERT(inSize);
   ASSERT(outSize);
   ASSERT(bufOut);

   return CPNameEscapeAndConvertFrom(bufIn, inSize, outSize, bufOut, '/');
}


/*
 *----------------------------------------------------------------------
 *
 * CPName_ConvertFromRoot --
 *
 *    Append the appropriate prefix to the output buffer for accessing
 *    the root of the local filesystem. CPName_ConvertFrom prepends
 *    leading path separators before each path component, but only
 *    when the next component has nonzero length, so we still need to
 *    special case this for Linux.
 *
 *    The pointers and sizes are updated appropriately.
 *
 * Results:
 *    Status of name conversion
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

HgfsNameStatus
CPName_ConvertFromRoot(char const **bufIn, // IN/OUT: Input to convert
                       size_t *inSize,     // IN/OUT: Size of input
                       size_t *outSize,    // IN/OUT: Size of output buffer
                       char **bufOut)      // IN/OUT: Output buffer
{
   char const *next;
   char *out;
   int len;

   ASSERT(bufIn);
   ASSERT(inSize);
   ASSERT(outSize);
   ASSERT(bufOut);

   out = *bufOut;

   /*
    * Get first component
    */
   len = CPName_GetComponent(*bufIn, *bufIn + *inSize, &next);
   if (len < 0) {
      Log("%s: get first component failed\n", __FUNCTION__);
      return HGFS_NAME_STATUS_FAILURE;
   }

   /* Space for leading '/' plus NUL termination */
   if (*outSize < len + 2) {
      return HGFS_NAME_STATUS_FAILURE;
   }

   /* Put a leading '/' in the output buffer either way */
   *out++ = '/';

   memcpy(out, *bufIn, len);
   out += len;

   /* NUL terminate */
   *out = '\0';

   *inSize -= next - *bufIn;
   *outSize -= out - *bufOut;
   *bufIn = next;
   *bufOut = out;

   return HGFS_NAME_STATUS_COMPLETE;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_ConvertTo --
 *
 *    Wrapper function that calls the Linux implementation of _ConvertTo().
 *
 *    Makes a cross-platform name representation from the Linux path input
 *    string and writes it into the output buffer.
 *    If the name being converter may be a result a of name escaping the
 *    function performs unescaping. HGFS convention is to always exchange
 *    unescaped names between guest and host and to perform necessary
 *    name escaping on both ends.
 *
 * Results:
 *    On success, returns the number of bytes used in the
 *    cross-platform name, NOT including the final terminating NUL
 *    character. On failure, returns a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPName_ConvertTo(char const *nameIn, // IN:  Buf to convert
                 size_t bufOutSize,  // IN:  Size of the output buffer
                 char *bufOut)       // OUT: Output buffer
{
   int result;
   result = CPName_LinuxConvertTo(nameIn, bufOutSize, bufOut);
   return result;
}
