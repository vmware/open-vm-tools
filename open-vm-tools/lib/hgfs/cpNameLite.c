/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * cpNameLite.c --
 *
 *    Shared portions of cross-platform name conversion routines used
 *    by hgfs. Unlike the real CP name conversion routines, these ones
 *    just convert path separators to nul characters and vice versa.
 *
 */

#include "cpNameLite.h"
#include "vm_assert.h"

/*
 *----------------------------------------------------------------------
 *
 * CPNameLite_ConvertTo --
 *
 *    Makes a cross-platform lite name representation from the input
 *    string.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
CPNameLite_ConvertTo(char *bufIn,      // IN/OUT: Input to convert
                     size_t inSize,    // IN: Size of input buffer
                     char pathSep)     // IN: Path separator
{
   size_t pos;
   ASSERT(bufIn);

   for (pos = 0; pos < inSize; pos++) {
      if (bufIn[pos] == pathSep) {
         bufIn[pos] = '\0';
      }
   }
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameLite_ConvertFrom --
 *
 *    Converts a cross-platform lite name representation into a string for
 *    use in the local filesystem. This is a cross-platform
 *    implementation and takes the path separator as an
 *    argument.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
CPNameLite_ConvertFrom(char *bufIn,     // IN/OUT: Input to convert
                       size_t inSize,   // IN: Size of input buffer
                       char pathSep)    // IN: Path separator

{
   size_t pos;
   ASSERT(bufIn);

   for (pos = 0; pos < inSize; pos++) {
      if (bufIn[pos] == '\0') {
         bufIn[pos] = pathSep;
      }
   }
}
