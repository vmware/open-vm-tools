/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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

#ifndef _LOCALCONFIG_H_
#define _LOCALCONFIG_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "preference.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define LocalConfig_GetBool Preference_GetBool
#define LocalConfig_GetTriState Preference_GetTriState
#define LocalConfig_GetLong Preference_GetLong
#define LocalConfig_GetString Preference_GetString
#define LocalConfig_GetPathName Preference_GetPathName

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
