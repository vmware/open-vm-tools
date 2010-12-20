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
 * strutil.h --
 *
 *    String utility functions.
 */


#ifndef STRUTIL_H
#define STRUTIL_H

#include <stdarg.h>

#include "fileIO.h"
#include "dynbuf.h"

char * StrUtil_GetNextToken(unsigned int *index, const char *str,
                            const char *delimiters);
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
Bool StrUtil_StrToSizet(size_t *out, const char *str);
Bool StrUtil_StrToDouble(double *out, const char *str);
Bool StrUtil_CapacityToSectorType(SectorType *out, const char *str,
                                  unsigned int bytes);
char * StrUtil_FormatSizeInBytesUnlocalized(uint64 size);

size_t StrUtil_GetLongestLineLength(const char *buf, size_t bufLength);

Bool StrUtil_StartsWith(const char *s, const char *prefix);
Bool StrUtil_CaselessStartsWith(const char *s, const char *prefix);
Bool StrUtil_EndsWith(const char *s, const char *suffix);

Bool StrUtil_VDynBufPrintf(DynBuf *b, const char *fmt, va_list args);
Bool StrUtil_DynBufPrintf(DynBuf *b, const char *fmt, ...);
void StrUtil_SafeDynBufPrintf(DynBuf *b, const char *fmt, ...);

#endif /* STRUTIL_H */
