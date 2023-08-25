/*********************************************************
 * Copyright (C) 2006-2016,2023 VMware, Inc. All rights reserved.
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
 * compat_stdarg.h --
 *
 *    Compatibility defines for systems that need the stdarg features. If your program
 *    needs va_init, va_copy, va_end, etc. then include this file instead of including
 *    stdarg.h directly.
 *
 *    Note that the header guard for this file does not follow typical naming
 *    convention as _COMPAT_STDARG_H conflicts with emscripten's compat/stdarg.h
 */

#ifndef _VMWARE_COMPAT_STDARG_H
#define _VMWARE_COMPAT_STDARG_H 1

#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_VMMON
#define INCLUDE_ALLOW_VMKERNEL
#define INCLUDE_ALLOW_VMKDRIVERS
#define INCLUDE_ALLOW_VMK_MODULE
#define INCLUDE_ALLOW_DISTRIBUTE
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdarg.h>

#if !defined(va_copy)
#   if defined(__va_copy)
#      define va_copy __va_copy
#   elif defined(_WIN32)
#      define va_copy(ap1, ap2) (*(ap1) = *(ap2))
#   elif defined(VA_LIST_IS_ARRAY)
#      define va_copy(ap1, ap2) memcpy(ap1, ap2, sizeof(va_list))
#   else
#      define va_copy(ap1, ap2) ((ap1) = (ap2))
#   endif
#endif

#endif /* _VMWARE_COMPAT_STDARG_H */
