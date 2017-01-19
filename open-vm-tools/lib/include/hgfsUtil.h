/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
 * hgfsUtil.h --
 *
 *    Utility functions and macros used by hgfs.
 */


#ifndef _HGFSUTIL_H_
#   define _HGFSUTIL_H_

#   if defined __linux__ && defined __KERNEL__
#      include "driver-config.h"
#      include <linux/time.h> // for time_t and timespec
    /* Include time.h in userspace code, but not in Solaris kernel code. */
#   elif defined __FreeBSD__ && defined _KERNEL
    /* Do nothing. */
#   elif defined __APPLE__ && defined KERNEL
#      include <sys/time.h>
#   else
#      include <time.h>
#   endif
#   include "vm_basic_types.h"
#   if !defined _STRUCT_TIMESPEC &&   \
       !defined _TIMESPEC_DECLARED && \
       !defined __timespec_defined && \
       !defined sun && \
       !defined __FreeBSD__ && \
       !__APPLE__ && \
       !defined _WIN32
struct timespec {
   time_t tv_sec;
   long   tv_nsec;
};
#   endif

#   include "hgfs.h"

/* Cross-platform representation of a platform-specific error code. */
#ifndef _WIN32
#   if defined __KERNEL__ || defined _KERNEL || defined KERNEL
#      if defined __linux__
#         include <linux/errno.h>
#      elif defined sun || defined __FreeBSD__ || defined __APPLE__
#         include <sys/errno.h>
#      endif
#   else
#      include <errno.h>
#   endif
    typedef int HgfsInternalStatus;
    /*
     * There is no internal error in Linux.
     * Define a const that is converted to HGFS_INTERNAL_STATUS_ERROR.
     */
#   define EINTERNAL                    1001
#else
#   include <windows.h>
    typedef DWORD HgfsInternalStatus;
#endif

#if defined _WIN32
#define HGFS_ERROR_SUCCESS           ERROR_SUCCESS
#define HGFS_ERROR_IO                ERROR_IO_DEVICE
#define HGFS_ERROR_ACCESS_DENIED     ERROR_ACCESS_DENIED
#define HGFS_ERROR_INVALID_PARAMETER ERROR_INVALID_PARAMETER
#define HGFS_ERROR_INVALID_HANDLE    ERROR_INVALID_HANDLE
#define HGFS_ERROR_PROTOCOL          RPC_S_PROTOCOL_ERROR
#define HGFS_ERROR_STALE_SESSION     ERROR_CONNECTION_INVALID
#define HGFS_ERROR_BUSY              ERROR_RETRY
#define HGFS_ERROR_PATH_BUSY         ERROR_RETRY
#define HGFS_ERROR_FILE_NOT_FOUND    ERROR_FILE_NOT_FOUND
#define HGFS_ERROR_FILE_EXIST        ERROR_ALREADY_EXISTS
#define HGFS_ERROR_NOT_SUPPORTED     ERROR_NOT_SUPPORTED
#define HGFS_ERROR_NOT_ENOUGH_MEMORY ERROR_NOT_ENOUGH_MEMORY
#define HGFS_ERROR_TOO_MANY_SESSIONS ERROR_MAX_SESSIONS_REACHED
#define HGFS_ERROR_INTERNAL          ERROR_INTERNAL_ERROR
#else
#define HGFS_ERROR_SUCCESS           0
#define HGFS_ERROR_IO                EIO
#define HGFS_ERROR_ACCESS_DENIED     EACCES
#define HGFS_ERROR_INVALID_PARAMETER EINVAL
#define HGFS_ERROR_INVALID_HANDLE    EBADF
#define HGFS_ERROR_PROTOCOL          EPROTO
#define HGFS_ERROR_STALE_SESSION     ENETRESET
#define HGFS_ERROR_BUSY              EBUSY
#define HGFS_ERROR_PATH_BUSY         EBUSY
#define HGFS_ERROR_FILE_NOT_FOUND    ENOENT
#define HGFS_ERROR_FILE_EXIST        EEXIST
#define HGFS_ERROR_NOT_SUPPORTED     EOPNOTSUPP
#define HGFS_ERROR_NOT_ENOUGH_MEMORY ENOMEM
#define HGFS_ERROR_TOO_MANY_SESSIONS ECONNREFUSED
#define HGFS_ERROR_INTERNAL          EINTERNAL
#endif // _WIN32

/*
 * Unfortunately, we need a catch-all "generic error" to use with
 * HgfsInternalStatus, because there are times when cross-platform code needs
 * to return its own errors along with errors from platform specific code.
 *
 * Using -1 should be safe because we expect our platforms to use zero as
 * success and a positive range of numbers as error values.
 */
#define HGFS_INTERNAL_STATUS_ERROR (-1)

#ifndef _WIN32
/*
 * This error code is used to notify the client that some of the parameters passed
 * (e.g. file handles) are not supported. Clients are expected to correct
 * the parameter (e.g. pass file name instead) and retry.
 *
 * Note that this error code is artificially made up and in future may conflict
 * with an "official" error code when added.
 */
#define EPARAMETERNOTSUPPORTED  (MAX_INT32 - 1)
#endif

/*
 * FreeBSD (pre-6.0) does not define EPROTO, so we'll define our own error code.
 */
#if defined __FreeBSD__ && !defined EPROTO
#define EPROTO (ELAST + 1)
#endif

#define HGFS_NAME_BUFFER_SIZE(packetSize, request) (packetSize - (sizeof *request - 1))
#define HGFS_NAME_BUFFER_SIZET(packetSize, sizet) (packetSize - ((sizet) - 1))

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

