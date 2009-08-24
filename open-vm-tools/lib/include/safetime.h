/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 *      In Windows, the user is allowed to set the time past the
 *      32-bit overflow date (in 2038), which can cause crashes
 *      and security problems.  In Linux, the time can't be set
 *      to overflow, so we do nothing.
 *
 *      NB: We do not know if one can set the time past 2038 in
 *      64-bit versions of Linux, and, if so, what happens when
 *      one does.  This requires further investigation sometime 
 *      in the future.
 *
 *      The stat types and functions must be defined manually,
 *      since they contain time_ts, and we can't use the macro
 *      trick, since the struct stat and the function stat have
 *      the same 32 bit name (but different 64 bit names).
 *
 */

#ifndef _SAFETIME_H_
#define _SAFETIME_H_

#ifdef _WIN32

#if (defined(_STAT_DEFINED) || defined(_INC_TIME) || defined(_INC_TYPES))
#error Use safetime.h instead of time.h, stat.h, and types.h
#endif

#undef  FMTTIME
#define FMTTIME FMT64"d"

#if (_MSC_VER < 1400)

#define _STAT_DEFINED

#include <time.h>
#include <sys/utime.h>
#include <sys/timeb.h>

#define time_t __time64_t
#define time(a) _time64(a)
#define localtime(a) _localtime64((a))

#define _ctime(a) _ctime64((a))
#define ctime(a) _ctime64((a))

#define _ftime(a) _ftime64((a))
#define ftime(a) _ftime64((a))
#define _timeb __timeb64

#define _gmtime(a) _gmtime64((a))
#define gmtime(a) _gmtime64((a))

#define _mktime(a) _mktime64((a))
#define mktime(a) _mktime64((a))

#define _utime(a,b) _utime64((a),(b))
#define utime(a,b) _utime64((a),(b))
#define _utimbuf __utimbuf64
#define utimbuf __utimbuf64

#define _wctime(a) _wctime64((a),(b))
#define wctime(a) _wctime64((a),(b))

#define _futime(a,b) _futime64((a),(b))
#define futime(a,b) _futime64((a),(b))

#define _wutime(a,b) _wutime64((a),(b))
#define wutime(a,b) _wutime64((a),(b))

#include <sys/types.h>

#ifdef  _MSC_VER
#pragma pack(push,8)
#endif  

struct _stat {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        __int64 st_size;
        __time64_t st_atime;
        __time64_t st_mtime;
        __time64_t st_ctime;
        };

struct stat {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        __int64 st_size;
        __time64_t st_atime;
        __time64_t st_mtime;
        __time64_t st_ctime;
        };

struct __stat64 {
        _dev_t st_dev;
        _ino_t st_ino;
        unsigned short st_mode;
        short st_nlink;
        short st_uid;
        short st_gid;
        _dev_t st_rdev;
        __int64 st_size;
        __time64_t st_atime;
        __time64_t st_mtime;
        __time64_t st_ctime;
        };

#ifdef  _MSC_VER
#pragma pack(pop)
#endif  

#include <sys/stat.h>

#define stat(a,b) _stat64((a),(struct __stat64*)(b))
#define _stat(a,b) _stat64((a),(struct __stat64*)(b))

#define fstat(a,b) _fstat64((a),(struct __stat64*)(b))
#define _fstat(a,b) _fstat64((a),(struct __stat64*)(b))

#define wstat(a,b) _wstat64((a),(struct __stat64*)(b))
#define _wstat(a,b) _wstat64((a),(struct __stat64*)(b))

#else /* (_MSC_VER < 1400) */

/* 
 * Starting with VC80, we can pick between 32-bit and 64-bit time_t
 * by defining or not defining _USE_32BIT_TIME_T.  Don't define it.
 */

#include <time.h>
#include <sys/utime.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>

/* Make sure that the headers didn't revert to 32-bit. */
#ifdef _USE_32BIT_TIME_T
#error Refusing to use 32-bit time_t in safetime.h
#endif

#endif /* (_MSC_VER < 1400) */

#else

#include <sys/types.h>
#include <time.h>
#include <sys/stat.h>
#include <utime.h>

#endif

#endif
