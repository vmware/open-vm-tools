/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
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

#if defined(__KERNEL__) && defined(__linux__)
#  include "driver-config.h"
#  include <linux/string.h>
#elif defined(_KERNEL) && defined(__FreeBSD__)
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
