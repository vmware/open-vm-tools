/*********************************************************
 * Copyright (C) 2008-2018 VMware, Inc. All rights reserved.
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
 * ghIntegrationCommon.h --
 *
 *	Common data structures and definitions used by Guest/Host Integration.
 */

#ifndef _GHINTEGRATIONCOMMON_H_
#define _GHINTEGRATIONCOMMON_H_

/*
 * Common data structures and definitions used by Guest/Host Integration.
 */
#define GHI_HGFS_SHARE_URL_SCHEME_UTF8 "x-vmware-share"
#define GHI_HGFS_SHARE_URL_UTF8 "x-vmware-share://"
#define GHI_HGFS_SHARE_URL      _T(GHI_HGFS_SHARE_URL_UTF8)

/*
 * A GHI/Unity request from the host to the guest can be sent to different
 * TCLO channels of guestRPC (refer to tclodefs.h). Mostly we use TOOLS_DND_NAME
 * channel and only a few may use TOOLS_DAEMON_NAME.
 */
#define GHI_CHANNEL_USER_SERVICE  0  // TOOLS_DND_NAME TCLO channel
#define GHI_CHANNEL_MAIN_SERVICE  1  // TOOLS_DAEMON_NAME TCLO channel
#define GHI_CHANNEL_MAX           2
typedef uint32 GHIChannelType;

#define GHI_REQUEST_SUCCESS_OK       0  // Guest returns OK.
#define GHI_REQUEST_SUCCESS_ERROR    1  // Guest returns ERROR.
#define GHI_REQUEST_GUEST_RPC_FAILED 2  // Not sent to guest
                                        // or guest failed to return,
                                        // including timeout.
#define GHI_REQUEST_GENERAL_ERROR    3  // General error, can be any
                                        // situation except
                                        // MKSCONTROL_GHI_REQUEST_SUCCESS_OK.
typedef uint32 GHIRequestResult;

#endif // ifndef _GHINTEGRATIONCOMMON_H_
