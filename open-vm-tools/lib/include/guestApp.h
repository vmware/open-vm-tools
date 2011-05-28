/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#   include "vm_basic_types.h"
#   include "removable_device.h"

uint32
GuestApp_OldGetOptions(void);

Bool
GuestApp_SetOptionInVMX(const char *option,     // IN
                        const char *currentVal, // IN
                        const char *newVal);    // IN

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

Bool
GuestApp_IsDiskShrinkEnabled(void);

Bool
GuestApp_IsDiskShrinkCapable(void);

void
GuestApp_GetPos(int16 *x,  // OUT
                int16 *y); // OUT

void
GuestApp_SetPos(uint16 x,  // IN
                uint16 y); // IN

int32
GuestApp_GetHostSelectionLen(void);

void
GuestApp_GetHostSelection(unsigned int size, // IN
                          char *data);       // OUT

void
GuestApp_SetSelLength(uint32 length); // IN

void
GuestApp_SetNextPiece(uint32 data); // IN

Bool
GuestApp_SetDeviceState(uint16 id,       // IN: Device ID
                        Bool connected); // IN

Bool
GuestApp_GetDeviceInfo(uint16 id,      // IN: Device ID
                       RD_Info *info); // OUT

typedef enum {
   GUESTAPP_ABSMOUSE_UNAVAILABLE,
   GUESTAPP_ABSMOUSE_AVAILABLE,
   GUESTAPP_ABSMOUSE_UNKNOWN
} GuestAppAbsoluteMouseState;

GuestAppAbsoluteMouseState GuestApp_GetAbsoluteMouseState(void);

#ifdef __cplusplus
}
#endif

#endif /* __GUESTAPP_H__ */
