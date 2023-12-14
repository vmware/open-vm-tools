/*********************************************************
 * Copyright (c) 1998-2018, 2021-2022 VMware, Inc. All rights reserved.
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
 * strutil.h --
 *
 *    String utility functions.
 */


#ifndef STRUTIL_H
#define STRUTIL_H

#include <stdarg.h>
#include "vm_basic_types.h"

#if defined(__cplusplus)
extern "C" {
#endif

struct DynBuf;

char *StrUtil_GetNextToken(unsigned int *index, const char *str,
                           const char *delimiters);
#if defined(_WIN32)
wchar_t *StrUtil_GetNextTokenW(unsigned int *index, const wchar_t *str,
                               const wchar_t *delimiters);
#endif
Bool StrUtil_GetNextIntToken(int32 *out, unsigned int *index, const char *str,
                             const char *delimiters);
Bool StrUtil_GetNextUintToken(uint32 *out, unsigned int *index, const char *str,
                              const char *delimiters);
Bool StrUtil_GetNextInt64Token(int64 *out, unsigned int *index, const char *str,
                              const char *delimiters);
Bool StrUtil_DecimalStrToUint(unsigned int *out, const char **str);
Bool StrUtil_StrToInt(int32 *out, const char *str);
Bool StrUtil_StrToUint(uint32 *out, const char *str);
Bool StrUtil_StrToInt64(int64 *out, const char *str);
Bool StrUtil_StrToUint64(uint64 *out, const char *str);
Bool StrUtil_StrToSizet(size_t *out, const char *str);
Bool StrUtil_StrToDouble(double *out, const char *str);
Bool StrUtil_CapacityToBytes(SectorType *out, const char *str,
                             unsigned int bytes);
Bool StrUtil_CapacityToSectorType(SectorType *out, const char *str,
                                  unsigned int bytes);
char *StrUtil_FormatSizeInBytesUnlocalized(uint64 size);

size_t StrUtil_GetLongestLineLength(const char *buf, size_t bufLength);

Bool StrUtil_StartsWith(const char *s, const char *prefix);
Bool StrUtil_CaselessStartsWith(const char *s, const char *prefix);
Bool StrUtil_EndsWith(const char *s, const char *suffix);
Bool StrUtil_CaselessEndsWith(const char *s, const char *suffix);
const char * StrUtil_CaselessStrstr(const char *str, const char *strSearch);
Bool StrUtil_IsASCII(const char *s);

Bool StrUtil_VDynBufPrintf(struct DynBuf *b, const char *fmt, va_list args);
Bool StrUtil_DynBufPrintf(struct DynBuf *b, const char *fmt, ...) PRINTF_DECL(2, 3);
void StrUtil_SafeDynBufPrintf(struct DynBuf *b, const char *fmt, ...) PRINTF_DECL(2, 3);

void StrUtil_SafeStrcat(char **prefix, const char *str);
void StrUtil_SafeStrcatFV(char **prefix, const char *fmt, va_list args);
void StrUtil_SafeStrcatF(char **prefix, const char *fmt, ...) PRINTF_DECL(2, 3);

char *StrUtil_TrimWhitespace(const char *str);

char *StrUtil_ReplaceAll(const char *orig, const char *what, const char *with);

char *StrUtil_GetNextItem(char **list, char delim);

char *StrUtil_GetLastItem(char **list, char delim);

Bool StrUtil_HasListItem(char const *list, char delim, char const *item);

Bool StrUtil_HasListItemCase(char const *list, char delim, char const *item);

char *StrUtil_AppendListItem(char const *list, char delim, char const *item);

void StrUtil_RemoveListItem(char * const list, char delim, char const *item);

void StrUtil_RemoveListItemCase(char * const list, char delim,
                                char const *item);

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* STRUTIL_H */
