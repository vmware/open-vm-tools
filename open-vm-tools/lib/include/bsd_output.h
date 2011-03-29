/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * bsd_output.h --
 *
 *    Declaration of BSD-borrowed formatted string output functions.
 */

#ifndef _BSD_OUTPUT_H_
#define _BSD_OUTPUT_H_

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"

#include "compat/compat_stdarg.h"

/*
 * Equivalents to the Windows vs*printf functions, except backed by code
 * borrowed from FreeBSD. This is necessary to provide certain
 * functionality missing from Windows formatted output APIs - namely
 * support for both positional arguments and 64-bit integers on all
 * platforms.
 *
 * Where feasible the BSD code has been altered to match what the
 * VisualC libc versions of vs*printf expect and do, as opposed to what
 * the GNU libc or FreeBSD versions do. Regarding 64-bit arguments, this
 * code supports all four of these prefixes: 'L', 'll', 'q', or 'I64'.
 */

int
bsd_vsnprintf(char **outbuf, size_t bufSize, const char *fmt0,
              va_list ap);

int
bsd_vsnprintf_c_locale(char **outbuf, size_t bufSize, const char *fmt0,
                       va_list ap);

int
bsd_vsnwprintf(wchar_t **outbuf, size_t bufSize, const wchar_t *fmt0,
               va_list ap);

#endif // _BSD_OUTPUT_H_
