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
 * cpLiteName.h --
 *
 *    Cross-platform "lite" name format used by hgfs.
 *
 */

#ifndef __CP_NAME_LITE_H__
#define __CP_NAME_LITE_H__

#if defined __KERNEL__ && defined __linux__
#  include "driver-config.h"
#  include <linux/string.h>
#elif defined _KERNEL && defined __FreeBSD__
#  include <sys/libkern.h>
#  define strchr(s,c)       index(s,c)
#else
#  include <string.h>
#endif

#include "vm_basic_types.h"

void
CPNameLite_ConvertTo(char *bufIn,      // IN/OUT: Input to convert
                     size_t inSize,    // IN: Size of input buffer
                     char pathSep);    // IN: Path separator

void
CPNameLite_ConvertFrom(char *bufIn,    // IN/OUT: Input to convert
                       size_t inSize,  // IN: Size of input buffer
                       char pathSep);  // IN: Path separator



#endif /* __CP_NAME_LITE_H__ */
