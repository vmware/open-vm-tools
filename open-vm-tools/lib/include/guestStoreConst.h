/*********************************************************
 * Copyright (c) 2019-2020 VMware, Inc. All rights reserved.
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
 * guestStoreConst.h --
 *
 *    GuestStore common constant definitions.
 */

#ifndef _GUESTSTORE_CONST_H_
#define _GUESTSTORE_CONST_H_

/*
 * GuestStore maximum content path length, set it
 * to be less than USERLINUX_PATH_MAX.
 */
#define GUESTSTORE_CONTENT_PATH_MAX  1024

/*
 * Buffer size to handle GuestStore content request. The request is
 * a content path encoded in HTTP protocol or DataMap.
 */
#define GUESTSTORE_REQUEST_BUFFER_SIZE   (1024 * 4)

/*
 * Buffer size to receive and forward GuestStore content bytes, set to
 * the maximum size of an IP packet.
 */
#define GUESTSTORE_RESPONSE_BUFFER_SIZE  (1024 * 64)

/*
 * GuestStore vmx to guest connection pending timeout value.
 */
#define GUESTSTORE_VMX_TO_GUEST_CONN_TIMEOUT  5 // seconds

/*
 * GuestStore default connection inactivity timeout value. This value shall
 * be greater than gstored timeout value which is currently 60 seconds.
 */
#define GUESTSTORE_DEFAULT_CONN_TIMEOUT  900 // seconds


/*
 * NOTE: changing the following IDs may break data map encoding compatibility.
 */

/* Tools to VMX field IDs */
enum {
   GUESTSTORE_REQ_FLD_CMD           = 1,
   GUESTSTORE_REQ_FLD_PATH          = 2,
   GUESTSTORE_REQ_FLD_MAX
};

/* Command Types */
enum {
   GUESTSTORE_REQ_CMD_GET           = 1,
   GUESTSTORE_REQ_CMD_CLOSE         = 2,
};

/* VMX to tools field IDs */
enum {
   GUESTSTORE_RES_FLD_ERROR_CODE    = 1,
   GUESTSTORE_RES_FLD_CONTENT_SIZE  = 2,
};

#endif  /* _GUESTSTORE_CONST_H_ */
