/*********************************************************
 * Copyright (C) 1998-2020 VMware, Inc. All rights reserved.
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

#ifndef _LOGLEVEL_DEFS_H_
#define _LOGLEVEL_DEFS_H_

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL

#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#ifndef LOGLEVEL_MODULE
#error "loglevel_defs.h must be included with LOGLEVEL_MODULE defined"
#endif

#ifndef LOGLEVEL_EXTENSION
#error "loglevel_defs.h must be included with LOGLEVEL_EXTENSION defined"
#endif

#include "vm_basic_types.h"
#include "vm_basic_defs.h"

#if defined __cplusplus
#define LOGLEVEL_EXTERN_C_BEGIN extern "C" {
#define LOGLEVEL_EXTERN_C_END }
#else
#define LOGLEVEL_EXTERN_C_BEGIN
#define LOGLEVEL_EXTERN_C_END
#endif

LOGLEVEL_EXTERN_C_BEGIN


/*
 * CPP variable name hacks
 */

#define LOGLEVEL_EXTOFFSET(ext) XCONC(_loglevel_offset_, ext)
#define LOGLEVEL_EXTNAME(ext) XSTR(ext)
#define LOGLEVEL_MODULEVAR(mod) XCONC(_loglevel_mod_, mod) 


/*
 * LogLevel declaration
 */

#define LOGLEVEL_EXTENSION_DECLARE(list) \
   LOGLEVEL_EXTERN_C_BEGIN \
   VMX86_EXTERN_DATA const int8 *logLevelPtr; \
   VMX86_EXTERN_DATA int LOGLEVEL_EXTOFFSET(LOGLEVEL_EXTENSION); \
   LOGLEVEL_EXTERN_C_END \
   enum { list(LOGLEVEL_MODULEVAR) }


#ifdef VMX86_LOG

/*
 * Cross extension
 */

int LogLevel_Set(const char *extension, const char *module, int val);

/*
 * Intra extension
 */

#define LOGLEVEL_BYNAME(_mod) \
        logLevelPtr[LOGLEVEL_EXTOFFSET(LOGLEVEL_EXTENSION) + \
                    LOGLEVEL_MODULEVAR(_mod)]

#define DOLOG_BYNAME(_mod, _min) \
        UNLIKELY(LOGLEVEL_BYNAME(_mod) >= (_min))

/*
 * Variadic macro wrinkle: C99 says "one or more arguments"; some compilers
 * (gcc+clang+msvc) support zero arguments, but differ in tolerating a trailing
 * comma. Solution: include format string in variadic arguments.
 *
 * C++2a introduces __VA_OPT__, which would allow this definition instead:
 * define LOG_BYNAME(_mod, _min, _fmt, ...) \
 *        (DOLOG_BYNAME(_mod, _min) ? Log(_fmt __VA_OPT__(,) __VA_ARGS__) \
 *                                  : (void) 0)
 * MSVC has always ignored a spurious trailing comma, so does not need this.
 */
#define LOG_BYNAME(_mod, _min, ...) \
        (DOLOG_BYNAME(_mod, _min) ? Log(__VA_ARGS__) : (void) 0)

/*
 * Default
 */

#define LOGLEVEL()     LOGLEVEL_BYNAME(LOGLEVEL_MODULE)
#define DOLOG(_min)    DOLOG_BYNAME(LOGLEVEL_MODULE, _min)
#define LOG(_min, ...) LOG_BYNAME(LOGLEVEL_MODULE, _min, __VA_ARGS__)

#else /* VMX86_LOG */

#define LOGLEVEL_BYNAME(_mod)           0
#define DOLOG_BYNAME(_mod, _min)        (FALSE)
#define LOG_BYNAME(_mod, _min, ...)

#define LOGLEVEL()      0
#define DOLOG(_min)     (FALSE)
#define LOG(_min, ...)


#endif /* VMX86_LOG */


#ifdef VMX86_DEVEL
   #define LOG_DEVEL(...) Log(__VA_ARGS__)
#else
   #define LOG_DEVEL(...)
#endif


LOGLEVEL_EXTERN_C_END

#endif /* _LOGLEVEL_DEFS_H_ */
