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
 * cpNameInt.h --
 *
 *    Cross-platform name format used by hgfs.
 *
 */


#ifndef __CP_NAME_INT_H__
#define __CP_NAME_INT_H__


#include "vm_basic_types.h"

/*
 * Used by CPName_ConvertFrom
 */
int
CPNameConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                  size_t *inSize,     // IN/OUT: Size of input
                  size_t *outSize,    // IN/OUT: Size of output buffer
                  char **bufOut,      // IN/OUT: Output buffer
                  char pathSep);      // IN: Path separator character

int
CPNameEscapeAndConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                           size_t *inSize,     // IN/OUT: Size of input
                           size_t *outSize,    // IN/OUT: Size of output buffer
                           char **bufOut,      // IN/OUT: Output buffer
                           char pathSep);      // IN: Path separator character

/*
 * Common code for CPName_ConvertTo
 */
int
CPNameConvertTo(char const *nameIn, // IN:  Buf to convert
                size_t bufOutSize,  // IN:  Size of the output buffer
                char *bufOut,       // OUT: Output buffer
                char pathSep);      // IN:  path separator to use

#endif /* __CP_NAME_INT_H__ */
