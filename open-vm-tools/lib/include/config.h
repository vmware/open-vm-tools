/*********************************************************
 * Copyright (C) 1998-2018 VMware, Inc. All rights reserved.
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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "preference.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * Well-known configuration variable names
 */

#define	CONFIG_VMWAREDIR	"libdir"

struct CryptoKey;
struct KeySafeUserRing;

void Config_SetAny(const char *value,
                   const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetString(const char *value,
                      const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetStringPlain(const char *value,
                           const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetStringSecure(const char *value,
                            const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetBool(Bool value, const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetBoolPlain(Bool value, const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetBoolSecure(Bool value, const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetLong(int32 value,
                    const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetInt64(int64 value,
                     const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetLongPlain(int32 value,
                         const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetLongSecure(int32 value,
                          const char *fmt, ...) PRINTF_DECL(2, 3);
void Config_SetDouble(double value,
                      const char *fmt, ...) PRINTF_DECL(2, 3);

char *Config_GetString(const char *defaultValue,
                       const char *fmt, ...) PRINTF_DECL(2, 3);
char *Config_GetStringPlain(const char *defaultValue,
                            const char *fmt, ...) PRINTF_DECL(2, 3);
char *Config_GetStringSecure(const char *defaultValue,
                             const char *fmt, ...) PRINTF_DECL(2, 3);
char *Config_GetAsString(const char *fmt, ...) PRINTF_DECL(1, 2);
char *Config_GetStringEnum(const char *defaultValue,
                           const char **choices,
                           const char *fmt, ...) PRINTF_DECL(3, 4);

int Config_CompareVersion(int version);
int Config_CompareVersions(int version1, int version2);
char *Config_GetPathName(const char *defaultValue,
                         const char *fmt, ...) PRINTF_DECL(2, 3);
Bool Config_GetBool(Bool defaultValue,
                    const char *fmt, ...) PRINTF_DECL(2, 3);
Bool Config_GetBoolPlain(Bool defaultValue,
                         const char *fmt, ...) PRINTF_DECL(2, 3);
Bool Config_GetBoolSecure(Bool defaultValue,
                          const char *fmt, ...) PRINTF_DECL(2, 3);
int32 Config_GetLong(int32 defaultValue,
                     const char *fmt, ...) PRINTF_DECL(2, 3);
int64 Config_GetInt64(int64 defaultValue,
                      const char *fmt, ...) PRINTF_DECL(2, 3);
int32 Config_GetLongPlain(int32 defaultValue,
                          const char *fmt, ...) PRINTF_DECL(2, 3);
int32 Config_GetLongSecure(int32 defaultValue,
                           const char *fmt, ...) PRINTF_DECL(2, 3);
int32 Config_GetTriState(int32 defaultValue,
                         const char *fmt, ...) PRINTF_DECL(2, 3);
double Config_GetDouble(double defaultValue,
                        const char *fmt, ...) PRINTF_DECL(2, 3);
Bool Config_NotSet(const char *fmt, ...) PRINTF_DECL(1, 2);
Bool Config_Unset(const char *fmt, ...) PRINTF_DECL(1, 2);
Bool Config_UnsetWithPrefix(const char *fmt, ...) PRINTF_DECL(1, 2);
Bool Config_NeedSave(void);

void Config_Set(void *value, int type,
                const char *fmt, ...) PRINTF_DECL(3, 4);

/*
 * This is tricky to call because it returns allocated storage. Use
 * the typed wrappers instead (Config_Get*).
 */
void *Config_Get(const void *pDefaultValue, int type,
                 const char *fmt, ...) PRINTF_DECL(3, 4);

Bool Config_Load(const char *filename);
Bool Config_Write(void);
Bool Config_WriteNoMsg(void);

Bool  Config_FileIsWritable(void);

uint32 Config_GetMask(uint32 defaultMask, const char *optionName);
uint64 Config_GetMask64(uint64 defaultMask, const char *optionName);

Bool Config_GetDataFileKey(struct CryptoKey **key,
                           struct KeySafeUserRing **userRing);

Bool Config_GetDataFileKeys(struct KeySafeUserRing **parentKeys,
                            struct KeySafeUserRing **allKeys);

Bool Config_TriToBool(Bool boolDefaultValue,
                      int32 triValue);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _CONFIG_H_
