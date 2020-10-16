/*********************************************************
 * Copyright (C) 2020 VMware, Inc. All rights reserved.
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

#ifndef _APPINFO_H_
#define _APPINFO_H_

/**
 * @file appInfo.h
 *
 * Common declarations that aid in sending application information
 * from 'appInfo' plugin in 'VMware Tools' to the host.
 */

/**
 * The guest Variable name for the application information.
 */
#define APP_INFO_GUESTVAR_KEY         "appInfo"
#define APP_INFO_GUESTINFO_KEY        "guestinfo." APP_INFO_GUESTVAR_KEY

#define APP_INFO_VERSION_1            1
#define APP_INFO_KEY_VERSION          "version"
#define APP_INFO_KEY_UPDATE_COUNTER   "updateCounter"
#define APP_INFO_KEY_PUBLISHTIME      "publishTime"
#define APP_INFO_KEY_APPS             "applications"
#define APP_INFO_KEY_APP_NAME         "a"
#define APP_INFO_KEY_APP_VERSION      "v"

#endif /* _APPINFO_H_ */
