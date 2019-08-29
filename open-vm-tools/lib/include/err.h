/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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

/*
 * err.h --
 *
 *      General error handling library
 */

#ifndef _ERR_H_
#define _ERR_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <errno.h>
#include "vm_basic_defs.h"

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(_WIN32)
   typedef DWORD Err_Number;
#else
   typedef int Err_Number;
#endif

#define ERR_INVALID ((Err_Number) -1)

const char *Err_ErrString(void);

const char *Err_Errno2String(Err_Number errorNumber);

Err_Number Err_String2Errno(const char *string);

#if defined(VMX86_DEBUG)
Err_Number Err_String2ErrnoDebug(const char *string);
#endif

#if defined(_WIN32)
char *Err_SanitizeMessage(const char *msg);
#endif

/*
 *----------------------------------------------------------------------
 *
 * Err_Errno --
 *
 *      Gets last error in a platform independent way.
 *
 * Results:
 *      Last error.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

#if defined(_WIN32)
   #define Err_Errno() GetLastError()
#else
   #define Err_Errno() errno
#endif


/*
 *----------------------------------------------------------------------
 *
 * Err_SetErrno --
 *
 *      Set the last error in a platform independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Yes.
 *
 *----------------------------------------------------------------------
 */

#if defined(_WIN32)
   #define Err_SetErrno(e) SetLastError(e)
#else
   #define Err_SetErrno(e) (errno = (e))
#endif


/*
 *----------------------------------------------------------------------
 *
 * WITH_ERRNO --
 *
 *      Execute "body" with "e" bound to the last error number
 *	and preserving the last error in surrounding code.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *	Yes.
 *
 *----------------------------------------------------------------------
 */

#if defined(_WIN32)
#define WITH_ERRNO(e, body) do { \
      Err_Number e = Err_Errno(); \
      int __win__##e = errno; \
      body; \
      Err_SetErrno(e); \
      errno = __win__##e; \
   } while (0)
#else
#define WITH_ERRNO(e, body) do { \
      Err_Number e = Err_Errno(); \
      body; \
      Err_SetErrno(e); \
   } while (0)
#endif

#define WITH_ERRNO_FREE(p) WITH_ERRNO(__errNum__, free((void *)p))

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
