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

#ifndef _CONFIG_H_
#define _CONFIG_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "vm_basic_types.h"
#include "preference.h"


/*
 * Well-known configuration variable names
 */

#define	CONFIG_VMWAREDIR	"libdir"

struct CryptoKey;
struct KeySafeUserRing;

EXTERN void Config_SetAny(const char *value,
                          const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetString(const char *value,
                             const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetStringPlain(const char *value,
                                  const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetBool(Bool value, const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetBoolPlain(Bool value, const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetLong(int32 value,
                           const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetInt64(int64 value,
                            const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetLongPlain(int32 value,
                                const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN void Config_SetDouble(double value,
                             const char *fmt, ...) PRINTF_DECL(2, 3);

EXTERN char *Config_GetString(const char *defaultValue,
                              const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN char *Config_GetStringPlain(const char *defaultValue,
                                   const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN char *Config_GetAsString(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN char *Config_GetStringEnum(const char *defaultValue,
                                  const char **choices,
                                  const char *fmt, ...) PRINTF_DECL(3, 4);

EXTERN int Config_CompareVersion(const char *version);
EXTERN int Config_CompareVersions(const char *version1, const char *version2);
EXTERN char *Config_GetPathName(const char *defaultValue,
				const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN Bool Config_GetBool(Bool defaultValue,
                           const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN Bool Config_GetBoolPlain(Bool defaultValue,
                                const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN int32 Config_GetLong(int32 defaultValue,
                            const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN int64 Config_GetInt64(int64 defaultValue,
                             const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN int32 Config_GetLongPlain(int32 defaultValue,
                                 const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN int32 Config_GetTriState(int32 defaultValue,
                                const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN double Config_GetDouble(double defaultValue,
                               const char *fmt, ...) PRINTF_DECL(2, 3);
EXTERN Bool Config_NotSet(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN void Config_Unset(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN void Config_UnsetWithPrefix(const char *fmt, ...) PRINTF_DECL(1, 2);

EXTERN void Config_Set(void *value, int type,
                       const char *fmt, ...) PRINTF_DECL(3, 4);

/*
 * This is tricky to call because it returns allocated storage. Use
 * the typed wrappers instead (Config_Get*).
 */
EXTERN void *Config_Get(const void *pDefaultValue, int type,
                        const char *fmt, ...) PRINTF_DECL(3, 4);

EXTERN void Config_MarkModified(const char *fmt, ...) PRINTF_DECL(1, 2);
EXTERN Bool Config_Load(const char *filename);
EXTERN Bool Config_Write(const char *dummy);
EXTERN Bool Config_WriteNoMsg(void);

EXTERN Bool  Config_FileIsPresent(void);
EXTERN Bool  Config_FileIsWritable(void);

EXTERN uint32 Config_GetMask(uint32 defaultMask, const char *optionName);
EXTERN uint64 Config_GetMask64(uint64 defaultMask, const char *optionName);

EXTERN Bool Config_GetDataFileKey(struct CryptoKey **key,
                                  struct KeySafeUserRing **userRing);

EXTERN Bool Config_GetDataFileKeys(struct KeySafeUserRing **parentKeys,
                                   struct KeySafeUserRing **allKeys);

#endif // _CONFIG_H_
