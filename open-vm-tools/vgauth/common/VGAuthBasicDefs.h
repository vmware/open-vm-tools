/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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

#ifndef _VGAUTH_BASIC_DEFS_H_
#define _VGAUTH_BASIC_DEFS_H_
/*
 * @file VGAuthBasicDefs.h
 *
 *       Shared types and macros for the VGAuth project.
 */

#include "VGAuthError.h"
#include "VGAuthUtil.h"

#ifndef _WIN32
#include <stdint.h>
#endif

#ifdef _WIN32
#define WIN32_ONLY(x) x
#define POSIX_ONLY(x)
#else
#define WIN32_ONLY(x)
#define POSIX_ONLY(x) x
#endif

#if defined(__GNUC__)
# define PRINTF_DECL(fmtPos, varPos)                                  \
   __attribute__((__format__(__printf__, fmtPos, varPos)))
#else
# define PRINTF_DECL(fmtPos, varPos)
#endif

#ifdef __cplusplus
#define ASSERT(cond) ((!(cond)) ? Util_Assert(#cond, __FILE__, __LINE__) : (void) 0)
#else
#define ASSERT(cond) ((!(cond)) ? Util_Assert(#cond, __FILE__, __LINE__) : 0)
#endif

/*
 * UNUSED_PARAM should surround the parameter name and type declaration,
 * e.g. "int MyFunction(int var1, UNUSED_PARAM(int var2))"
 *
 */

#ifndef UNUSED_PARAM
# if defined(__GNUC__)
#  define UNUSED_PARAM(_parm) _parm  __attribute__((__unused__))
# else
#  define UNUSED_PARAM(_parm) _parm
# endif
#endif

#ifndef UNUSED_TYPE
// XXX _Pragma would better but doesn't always work right now.
#  define UNUSED_TYPE(_parm) UNUSED_PARAM(_parm)
#endif

#define ASSERT_ON_COMPILE(e) \
   do { \
      enum { AssertOnCompileMisused = ((e) ? 1: -1) }; \
      UNUSED_TYPE(typedef char AssertOnCompileFailed[AssertOnCompileMisused]); \
   } while (0)

#ifndef _WIN32
/*
 * Some common platform interface may require a HANDLE parameter for Windows
 * Define it for other platforms as well to get a better type safety and
 * cleaner code.
 */

typedef void* HANDLE;
#endif

/*
 * Printf format specifiers for size_t and 64-bit number.
 * Use them like this:
 *    printf("%"FMT64"d\n", big);
 *
 * This transforms the bora/lib version into the glib names so
 * we can easily port code.
 */
#define FMT64 G_GINT64_MODIFIER
#define FMTSZ G_GSIZE_MODFIER

#define VGAUTH_ERROR_SET_SYSTEM_ERRNO(err, syserr) \
   do { \
      ((VGAuthErrorFields *) &err)->error = VGAUTH_E_SYSTEM_ERRNO; \
      ((VGAuthErrorFields *) &err)->extraError = syserr; \
   } while (0)

#define VGAUTH_ERROR_SET_SYSTEM_WINDOWS(err, syserr) \
   do { \
      ((VGAuthErrorFields *) &err)->error = VGAUTH_E_SYSTEM_WINDOWS; \
      ((VGAuthErrorFields *) &err)->extraError = syserr; \
   } while (0)


#endif // _VGAUTH_BASIC_DEFS_H_
