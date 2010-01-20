/*********************************************************
 * Copyright (C) 1998-2003 VMware, Inc. All rights reserved.
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
	VMX86_EXTERN_DATA const int8 *logLevelPtr; \
        VMX86_EXTERN_DATA int LOGLEVEL_EXTOFFSET(LOGLEVEL_EXTENSION); \
        enum { list(LOGLEVEL_MODULEVAR) }


#ifdef VMX86_LOG


/*
 * Cross extension
 */

#define LOGLEVEL_BYEXTNAME(_ext, _mod) \
        (*LogLevel_LookUpVar(XSTR(_ext), XSTR(_mod)))

#define LOGLEVEL_BYEXTNAME_SET(_ext, _mod, _val) \
	LogLevel_Set(XSTR(_ext), XSTR(_mod), _val)

const int8 *LogLevel_LookUpVar(const char *extension, const char *module);
int LogLevel_Set(const char *extension, const char *module, int val);

#define DOLOG_BYEXTNAME(_ext, _mod, _min) \
        UNLIKELY(LOGLEVEL_BYEXTNAME(_ext, _mod) >= (_min))

#define LOG_BYEXTNAME(_ext, _mod, _min, _log) \
	(DOLOG_BYEXTNAME(_ext, _mod, _min) ? (Log _log) : (void) 0)

/*
 * Intra extension
 */

#define LOGLEVEL_BYNAME(_mod) \
        logLevelPtr[LOGLEVEL_EXTOFFSET(LOGLEVEL_EXTENSION) + \
		    LOGLEVEL_MODULEVAR(_mod)]

#ifdef VMM
#define LOGLEVEL_BYNAME_SET(_mod, _val) do { \
	   monitorLogLevels[LOGLEVEL_EXTOFFSET(LOGLEVEL_EXTENSION) + \
                            LOGLEVEL_MODULEVAR(_mod)] = _val;        \
        } while (0)
#endif

#define DOLOG_BYNAME(_mod, _min) \
        UNLIKELY(LOGLEVEL_BYNAME(_mod) >= (_min))

#define LOG_BYNAME(_mod, _min, _log) \
	(DOLOG_BYNAME(_mod, _min) ? (Log _log) : (void) 0)

/*
 * Default
 */

#define LOGLEVEL() LOGLEVEL_BYNAME(LOGLEVEL_MODULE)

#define DOLOG(_min) DOLOG_BYNAME(LOGLEVEL_MODULE, _min)

#define LOG(_min, _log) LOG_BYNAME(LOGLEVEL_MODULE, _min, _log)

#else /* VMX86_LOG */

#define LOGLEVEL_BYEXTNAME(_ext, _mod)           0
#define LOGLEVEL_BYEXTNAME_SET(_ext, _mod, _val) do {} while (0)
#define DOLOG_BYEXTNAME(_ext, _mod, _min)        (FALSE)
#define LOG_BYEXTNAME(_ext, _mod, _min, _log)    do {} while (0)

#define LOGLEVEL_BYNAME(_mod)           0
#ifdef VMM
#define LOGLEVEL_BYNAME_SET(_mod, _val) do {} while (0)
#endif
#define DOLOG_BYNAME(_mod, _min)        (FALSE)
#define LOG_BYNAME(_mod, _min, _log)    do {} while (0)

#define LOGLEVEL()      0
#define DOLOG(_min)     (FALSE)
#define LOG(_min, _log)


#endif /* VMX86_LOG */


#ifdef VMX86_DEVEL
   #define LOG_DEVEL(_x) (Log _x)
   #define LOG_DEVEL_DB(_x) (LogDB _x)
#else
   #define LOG_DEVEL(_x)
   #define LOG_DEVEL_DB(_x)
#endif


#endif /* _LOGLEVEL_DEFS_H_ */
