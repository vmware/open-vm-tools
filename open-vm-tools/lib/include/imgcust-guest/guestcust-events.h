/*********************************************************
 * Copyright (C) 2005-2016 VMware, Inc. All rights reserved.
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
 * guestcust-events.h --
 *
 *      Definitions related to the GOSC events.
 */

#ifndef IMGCUST_COMMON_GOSC_EVENTS_H
#define IMGCUST_COMMON_GOSC_EVENTS_H

/*
 * Customization-specific events generated in the guest and handled by
 * hostd. They are sent via the vmx/guestTools/deployPkgState/ vmdb path.
 * We start these at 100 to avoid conflict with the deployPkg error
 * codes listed in vmx/public/toolsDeployPkg.h.
 */
typedef enum {
   GUESTCUST_EVENT_CUSTOMIZE_FAILED = 100,
   GUESTCUST_EVENT_NETWORK_SETUP_FAILED,
   GUESTCUST_EVENT_SYSPREP_FAILED,
   GUESTCUST_EVENT_ENABLE_NICS,
   GUESTCUST_EVENT_QUERY_NICS
} GuestCustEvent;

#endif // IMGCUST_COMMON_GOSC_EVENTS_H
