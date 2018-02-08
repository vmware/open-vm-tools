/*********************************************************
 * Copyright (C) 2004-2018 VMware, Inc. All rights reserved.
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
 * safetime.h --
 *
 *      This header file defines wrappers so that we use the
 *      64-bit versions of the C time calls.  This file is
 *      temporary until we switch to a newer version of
 *      Visual Studio that uses the 64-bit verisions by default.
 *
 *      NB: We have switched; existence of this header is temporary
 *      until include sites can be updated.
 *
 */

#ifndef _SAFETIME_H_
#define _SAFETIME_H_

#ifdef _WIN32

#include <time.h>
#include <sys/utime.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

#else

#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <utime.h>

#endif

#endif
