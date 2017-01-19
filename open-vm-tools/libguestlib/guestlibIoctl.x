/*********************************************************
 * Copyright (C) 2010-2016 VMware, Inc. All rights reserved.
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
 * guestlibIoctl.x --
 *
 *    Data structures that encode the information exchanged over guestlib
 *    client ioctl and VMX .
 */

/* The GuestRpc command string. */
const VMGUESTLIB_IOCTL_COMMAND_STRING = "guestlib.ioctl";

const GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE_LENGTH = 256;

enum GuestLibAtomicUpdateStatus {
   GUESTLIB_ATOMIC_UPDATE_OK_SUCCESS   = 0,
   GUESTLIB_ATOMIC_UPDATE_OK_FAIL      = 1,
   GUESTLIB_ATOMIC_UPDATE_EBADPARAM    = 2,
   GUESTLIB_ATOMIC_UPDATE_EUNAVAILABLE = 3,
   GUESTLIB_ATOMIC_UPDATE_EUNKNOWN     = 4
};

/* All supported ioctl ids */
enum GuestLibIoctlId {
   GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE = 1,

   GUESTLIB_IOCTL_MAX = 2
};

/* Parameters for AtomicUpdateCookie ioctl. */
struct GuestLibIoctlAtomicUpdateCookie {
   string src<GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE_LENGTH>;
   string dst<GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE_LENGTH>;
};

/* Struct for all supported ioctls */
union GuestLibIoctlParam switch (GuestLibIoctlId d) {
   case GUESTLIB_IOCTL_ATOMIC_UPDATE_COOKIE:
      struct GuestLibIoctlAtomicUpdateCookie atomicUpdateCookie;
};
