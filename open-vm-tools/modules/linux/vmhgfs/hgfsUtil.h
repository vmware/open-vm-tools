/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * hgfsUtil.h --
 * 
 *    Utility functions and macros used by hgfs.
 */


#ifndef _HGFSUTIL_H_
#   define _HGFSUTIL_H_

#   if defined(__linux__) && defined(__KERNEL__)
#      include "driver-config.h"
#      include <linux/time.h> // for time_t and timespec  
    /* Include time.h in userspace code, but not in Solaris kernel code. */
#   elif defined(__FreeBSD__) && defined(_KERNEL)
    /* Do nothing. */
#   elif defined(__APPLE__) && defined(KERNEL)
#      include <sys/time.h>
#   else 
#      include <time.h>
#   endif 
#   include "vm_basic_types.h"
#   if !defined(_STRUCT_TIMESPEC) &&   \
       !defined(_TIMESPEC_DECLARED) && \
       !defined(__timespec_defined) && \
       !defined(sun) && \
       !defined(__FreeBSD__) && \
       !__APPLE__ && \
       !defined(_WIN32)
struct timespec {
   time_t tv_sec;
   long   tv_nsec;
};
#   endif

#   include "hgfs.h"

/* Cross-platform representation of a platform-specific error code. */
#ifndef _WIN32
#   if defined(__KERNEL__) || defined(_KERNEL) || defined(KERNEL)
#      if defined(__linux__)
#         include <linux/errno.h>
#      elif defined(sun) || defined(__FreeBSD__) || defined(__APPLE__)
#         include <sys/errno.h>
#      endif
#   else
#      include <errno.h>
#   endif
    typedef int HgfsInternalStatus;
#else
#   include <windows.h>
    typedef DWORD HgfsInternalStatus;
#endif

/* 
 * Unfortunately, we need a catch-all "generic error" to use with 
 * HgfsInternalStatus, because there are times when cross-platform code needs
 * to return its own errors along with errors from platform specific code. 
 *
 * Using -1 should be safe because we expect our platforms to use zero as
 * success and a positive range of numbers as error values.
 */
#define HGFS_INTERNAL_STATUS_ERROR -1

/*
 * FreeBSD (pre-6.0) does not define EPROTO, so we'll define our own error code.
 */
#if defined(__FreeBSD__) && !defined(EPROTO)
#define EPROTO (ELAST + 1)
#endif

#define HGFS_NAME_BUFFER_SIZE(request) (HGFS_PACKET_MAX - (sizeof *request - 1))

#ifndef _WIN32
/*
 * Routines for converting between Win NT and unix time formats. The
 * hgfs attributes use the NT time formats, so the linux driver and
 * server have to convert back and forth. [bac]
 */

uint64 HgfsConvertToNtTime(time_t unixTime, // IN
			   long   nsec);    // IN
static INLINE uint64
HgfsConvertTimeSpecToNtTime(const struct timespec *unixTime) // IN
{
   return HgfsConvertToNtTime(unixTime->tv_sec, unixTime->tv_nsec);
}

int HgfsConvertFromNtTime(time_t * unixTime, // OUT
			  uint64 ntTime);    // IN
int HgfsConvertFromNtTimeNsec(struct timespec *unixTime, // OUT
                              uint64 ntTime);            // IN
#endif /* !def(_WIN32) */

HgfsStatus HgfsConvertFromInternalStatus(HgfsInternalStatus status); // IN

#endif /* _HGFSUTIL_H_ */
