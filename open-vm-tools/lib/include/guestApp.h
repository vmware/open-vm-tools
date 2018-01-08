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


/*
 * guestApp.h --
 *
 *    Utility functions common to all guest applications
 */


#ifndef __GUESTAPP_H__
#   define __GUESTAPP_H__

#include "vm_basic_types.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

const char *
GuestApp_GetDefaultScript(const char *confName); // IN

#ifdef _WIN32
LPWSTR
GuestApp_GetInstallPathW(void);
#endif

char *
GuestApp_GetInstallPath(void);

char *
GuestApp_GetConfPath(void);

#ifdef __cplusplus
}
#endif

#endif /* __GUESTAPP_H__ */
