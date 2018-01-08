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

#ifndef _PREFERENCE_H_
#define _PREFERENCE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

void Preference_DisableUserPreferences(void);
Bool Preference_Init(void);
void Preference_Exit(void);
Bool Preference_GetBool(Bool defaultValue, const char *fmt);
int32 Preference_GetTriState(int32 defaultValue, const char *fmt);
int32 Preference_GetLong(int32 defaultValue, const char *fmt);
int64 Preference_GetInt64(int64 defaultvalue, const char *fmt);
double Preference_GetDouble(double defaultValue, const char *fmt);
char *Preference_GetString(const char *defaultValue, const char *fmt);

void Preference_Log(void);
char *Preference_GetPathName(const char *defaultValue, const char *fmt);
void Preference_SetFromString(const char *string, Bool overwrite);
Bool Preference_NotSet(const char *fmt);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
