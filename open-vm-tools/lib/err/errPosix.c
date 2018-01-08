/*********************************************************
 * Copyright (C) 2007-2017 VMware, Inc. All rights reserved.
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
 * errPosix.c --
 *
 *      Posix error handling library
 *
 */

#if defined __linux__
/* Force GNU strerror_r prototype instead of Posix prototype */
#  define _GNU_SOURCE
#endif

#include <errno.h>
#include <string.h>
#include <locale.h>

#include "vmware.h"
#include "errInt.h"
#include "util.h"
#include "str.h"


/*
 *----------------------------------------------------------------------
 *
 * ErrErrno2String --
 *
 *      Convert an error number to a string in English.
 *	The returned string may use the supplied buffer or may be
 *	a static string.
 *
 * Results:
 *      The error string.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
ErrErrno2String(Err_Number errorNumber, // IN
                char *buf,		// OUT: return buffer
		size_t bufSize)		// IN: size of buffer
{
   char *p;

#if defined(__linux__) && !defined(__ANDROID__)
   p = strerror_r(errorNumber, buf, bufSize);
#else
   p = strerror(errorNumber);
#endif
   ASSERT(p != NULL);
   return p;
}
