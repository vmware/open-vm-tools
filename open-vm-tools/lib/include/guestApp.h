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

typedef struct GuestApp_Dict GuestApp_Dict;

uint32
GuestApp_OldGetOptions(void);

void
GuestApp_OldSetOptions(uint32 options); // IN

Bool
GuestApp_SetOptionInVMX(const char *option,     // IN
                        const char *currentVal, // IN
                        const char *newVal);    // IN

GuestApp_Dict *
GuestApp_ConstructDict(char *fileName); // IN

void
GuestApp_SetDictEntry(GuestApp_Dict *dict, // IN
                      const char *name,    // IN
                      const char *value);  // IN

void
GuestApp_SetDictEntryDefault(GuestApp_Dict *dict,     // IN
                             const char *name,        // IN
                             const char *defaultVal); // IN
const char *
GuestApp_GetDictEntry(GuestApp_Dict *dict, // IN
                      const char *name);   // IN

const char *
GuestApp_GetDictEntryDefault(GuestApp_Dict *dict, // IN
                             const char *name);   // IN

Bool
GuestApp_GetDictEntryInt(GuestApp_Dict *dict, // IN
                         const char *name,    // IN
                         int32 *value);       // OUT

Bool
GuestApp_GetDictEntryBool(GuestApp_Dict *dict, // IN
                          const char *name);   // IN

Bool
GuestApp_WasDictFileChanged(GuestApp_Dict *dict); // IN

void
GuestApp_FreeDict(GuestApp_Dict *dict); // IN

Bool
GuestApp_LoadDict(GuestApp_Dict *dict); // IN

Bool
GuestApp_WriteDict(GuestApp_Dict *dict); // IN

const char *
GuestApp_GetDefaultScript(const char *confName); // IN

Bool
GuestApp_GetUnifiedLoopCap(const char *channel); // IN

Bool
GuestApp_GetPtrGrabCap(const char *channel); // IN

Bool
GuestApp_Log(const char *s); // IN

char *
GuestApp_GetInstallPath(void);

char *
GuestApp_GetConfPath(void);

char *
GuestApp_GetLogPath(void);

Bool
GuestApp_IsHgfsCapable(void);

Bool
GuestApp_IsDiskShrinkEnabled(void);

Bool
GuestApp_IsDiskShrinkCapable(void);

Bool
GuestApp_DiskShrink(void);

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

uint32
GuestApp_HostCopyStep(uint8 c);  // IN

Bool
GuestApp_RpcSendOneArgCPName(char const *cmd,       // IN: RPCI command
                             char const *arg,       // IN: UTF-8 encoded string
                             size_t argSize,        // IN: size of arg
                             char delimiter,        // IN: delimiter
                             char const *cpNameArg, // IN: UTF-8 encoded CPName
                             size_t cpNameArgSize); // IN: size of cpNameArg
Bool
GuestApp_RpcSendOneCPName(char const *cmd, // IN: RPCI command
                          char delimiter,  // IN: delimiter
                          char const *arg, // IN: string to be Utf8/CPName encoded
                          size_t argSize); // IN: size of arg

Bool GuestApp_OpenUrl(const char *url, Bool maximize);

typedef enum {
   GUESTAPP_ABSMOUSE_UNAVAILABLE,
   GUESTAPP_ABSMOUSE_AVAILABLE,
   GUESTAPP_ABSMOUSE_UNKNOWN
} GuestAppAbsoluteMouseState;

GuestAppAbsoluteMouseState GuestApp_GetAbsoluteMouseState(void);

#if defined(_WIN32)
void GuestApp_SetDictEntryW(GuestApp_Dict *dict,
                            const WCHAR *name,
                            const WCHAR *value);

void GuestApp_SetDictEntryDefaultW(GuestApp_Dict *dict,
                                   const WCHAR *name,
                                   const WCHAR *defaultVal);

WCHAR *GuestApp_GetDictEntryW(GuestApp_Dict *dict,
                              const WCHAR *name);

WCHAR *GuestApp_GetDictEntryDefaultW(GuestApp_Dict *dict,
                                     const WCHAR *name);
#endif

#ifndef _WIN32
void GuestApp_SetSpawnEnviron(const char **spawnEnviron);
Bool GuestApp_FindProgram(const char *program);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __GUESTAPP_H__ */
