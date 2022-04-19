/*********************************************************
 * Copyright (C) 1998-2016,2022 VMware, Inc. All rights reserved.
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
 * hgfsUtil.c --
 *
 *    Utility routines used by both HGFS servers and clients, such as
 *    conversion routines between Unix time and Windows NT time.
 *    The former is in units of seconds since midnight 1/1/1970, while the
 *    latter is in units of 100 nanoseconds since midnight 1/1/1601.
 */

/*
 * hgfsUtil.h must be included before vm_basic_asm.h, as hgfsUtil.h
 * includes kernel headers on Linux.  That is, vmware.h must come after
 * hgfsUtil.h.
 */
#include "hgfsUtil.h"
#include "vmware.h"
#include "vm_basic_asm.h"

#ifndef _WIN32
/*
 * NT time of the Unix epoch:
 * midnight January 1, 1970 UTC
 */
#define UNIX_EPOCH ((((uint64)369 * 365) + 89) * 24 * 3600 * 10000000)

/*
 * NT time of the Unix 32 bit signed time_t wraparound:
 * 03:14:07 January 19, 2038 UTC
 */
#define UNIX_S32_MAX (UNIX_EPOCH + (uint64)0x80000000 * 10000000)


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertToNtTime --
 *
 *    Convert from Unix time to Windows NT time.
 *
 * Results:
 *    The time in Windows NT format.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

uint64
HgfsConvertToNtTime(time_t unixTime, // IN: Time in Unix format (seconds)
		    long   nsec)     // IN: nanoseconds
{
   return (uint64)unixTime * 10000000 + nsec / 100 + UNIX_EPOCH;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertFromNtTimeNsec --
 *
 *    Convert from Windows NT time to Unix time. If NT time is outside of
 *    UNIX time range (1970-2038), returned time is nearest time valid in
 *    UNIX.
 *
 * Results:
 *    0        on success
 *    non-zero if NT time is outside of valid range for UNIX
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsConvertFromNtTimeNsec(struct timespec *unixTime, // OUT: Time in UNIX format
			  uint64 ntTime) // IN: Time in Windows NT format
{
   ASSERT(unixTime);

   if (ntTime < UNIX_EPOCH) {
      unixTime->tv_sec = 0;
      unixTime->tv_nsec = 0;
      return -1;
   }

#ifdef __i386__
   if (sizeof unixTime->tv_sec == 4) {
      uint32 sec,nsec;

      /* Cap NT time values that are outside of Unix time's range */
      if (ntTime >= UNIX_S32_MAX) {
         unixTime->tv_sec = 0x7FFFFFFF;
         unixTime->tv_nsec = 0;
         return 1;
      }

      Div643232(ntTime - UNIX_EPOCH, 10000000, &sec, &nsec);
      unixTime->tv_sec = sec;
      unixTime->tv_nsec = nsec * 100;

      return 0;
   }
#endif

   unixTime->tv_sec = (ntTime - UNIX_EPOCH) / 10000000;
   unixTime->tv_nsec = ((ntTime - UNIX_EPOCH) % 10000000) * 100;

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertFromNtTime --
 *
 *    Convert from Windows NT time to Unix time.
 *
 * Results:
 *    0       on success
 *    nonzero if time is not representable on UNIX
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsConvertFromNtTime(time_t *unixTime, // OUT: Time in UNIX format
		      uint64 ntTime) // IN: Time in Windows NT format
{
   struct timespec tm;
   int ret;

   ret = HgfsConvertFromNtTimeNsec(&tm, ntTime);
   *unixTime = tm.tv_sec;
   return ret;
}
#endif /* !def(_WIN32) */


#undef UNIX_EPOCH
#undef UNIX_S32_MAX


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsConvertFromInternalStatus --
 *
 *    This function converts between a platform-specific status code and a
 *    cross-platform status code to be sent down the wire.
 *
 * Results:
 *    Converted status code.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

#ifdef _WIN32
HgfsStatus
HgfsConvertFromInternalStatus(HgfsInternalStatus status) // IN
{
   switch(status) {
   case ERROR_SUCCESS:
      return HGFS_STATUS_SUCCESS;
   case ERROR_FILE_NOT_FOUND:
   case ERROR_PATH_NOT_FOUND:
      return HGFS_STATUS_NO_SUCH_FILE_OR_DIR;
   case ERROR_INVALID_HANDLE:
      return HGFS_STATUS_INVALID_HANDLE;
   case ERROR_ALREADY_EXISTS:
   case ERROR_FILE_EXISTS:
      return HGFS_STATUS_FILE_EXISTS;
   case ERROR_DIR_NOT_EMPTY:
      return HGFS_STATUS_DIR_NOT_EMPTY;
   case RPC_S_PROTOCOL_ERROR:
      return HGFS_STATUS_PROTOCOL_ERROR;
   case ERROR_ACCESS_DENIED:
      return HGFS_STATUS_ACCESS_DENIED;
   case ERROR_INVALID_NAME:
      return HGFS_STATUS_INVALID_NAME;
   case ERROR_SHARING_VIOLATION:
      return HGFS_STATUS_SHARING_VIOLATION;
   case ERROR_DISK_FULL:
   case ERROR_HANDLE_DISK_FULL:
      return HGFS_STATUS_NO_SPACE;
   case ERROR_NOT_SUPPORTED:
      return HGFS_STATUS_OPERATION_NOT_SUPPORTED;
   case ERROR_INVALID_PARAMETER:
      return HGFS_STATUS_INVALID_PARAMETER;
   case ERROR_NOT_SAME_DEVICE:
      return HGFS_STATUS_NOT_SAME_DEVICE;
   case ERROR_FILENAME_EXCED_RANGE:
      return HGFS_STATUS_NAME_TOO_LONG;
   case ERROR_CONNECTION_INVALID:  // HGFS_ERROR_STALE_SESSION
      return HGFS_STATUS_STALE_SESSION;
   case ERROR_MAX_SESSIONS_REACHED:
      return HGFS_STATUS_TOO_MANY_SESSIONS;
   case ERROR_INTERNAL_ERROR:
   case HGFS_INTERNAL_STATUS_ERROR:
   default:
      return HGFS_STATUS_GENERIC_ERROR;
   }
}

#else /* Win32 */

HgfsStatus
HgfsConvertFromInternalStatus(HgfsInternalStatus status) // IN
{
   switch(status) {
   case 0:
      return HGFS_STATUS_SUCCESS;
   case ENOENT:
      return HGFS_STATUS_NO_SUCH_FILE_OR_DIR;
   case EBADF:
      return HGFS_STATUS_INVALID_HANDLE;
   case EPERM:
      return HGFS_STATUS_OPERATION_NOT_PERMITTED;
   case EISDIR:
   case EEXIST:
      return HGFS_STATUS_FILE_EXISTS;
   case ENOTDIR:
      return HGFS_STATUS_NOT_DIRECTORY;
   case ENOTEMPTY:
      return HGFS_STATUS_DIR_NOT_EMPTY;
   case EPROTO:
      return HGFS_STATUS_PROTOCOL_ERROR;
   case EACCES:
      return HGFS_STATUS_ACCESS_DENIED;
   case EINVAL:
      return HGFS_STATUS_INVALID_NAME;
   case ENOSPC:
      return HGFS_STATUS_NO_SPACE;
   case EOPNOTSUPP:
      return HGFS_STATUS_OPERATION_NOT_SUPPORTED;
   case ENAMETOOLONG:
      return HGFS_STATUS_NAME_TOO_LONG;
   case EPARAMETERNOTSUPPORTED:
      return HGFS_STATUS_INVALID_PARAMETER;
   case EXDEV:
      return HGFS_STATUS_NOT_SAME_DEVICE;
   case ENETRESET:  // HGFS_ERROR_STALE_SESSION
      return HGFS_STATUS_STALE_SESSION;
   case ECONNREFUSED:
      return HGFS_STATUS_TOO_MANY_SESSIONS;
   case EINTERNAL:
   case HGFS_INTERNAL_STATUS_ERROR:
   default:
      return HGFS_STATUS_GENERIC_ERROR;
   }
}
#endif
