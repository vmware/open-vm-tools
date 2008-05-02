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
 * codesetOld.h --
 *
 *    The old codeset implementation that depends on system libraries.
 *    Used for fallback if ICU isn't available.
 *
 */

#ifndef __CODESET_OLD_H__
#   define __CODESET_OLD_H__

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "dynbuf.h"

/*
 * NO_CORE_ICU is currently only used by oddball Tools builds
 * (non-Linux, non-Windows, non-Mac, source). It means "don't compile
 * our borrowed ICU source". Even if we don't do that, however, it's
 * still possible to use the system ICU libraries if USE_ICU is
 * defined (currently only for open-source tools builds.
 *
 * Since codeset.c is the ICU layer over codesetOld.c, if we are NOT
 * using either variety of ICU, then we won't be compiling codeset.c,
 * and thus need to transparently convert codeset calls to old codeset
 * calls.
 */

#if defined(NO_CORE_ICU) && !defined(USE_ICU)
#define CodeSetOld_GenericToGenericDb CodeSet_GenericToGenericDb
#define CodeSetOld_GenericToGeneric CodeSet_GenericToGeneric
#define CodeSetOld_Utf8ToCurrent CodeSet_Utf8ToCurrent
#define CodeSetOld_Utf8ToCurrentTranslit CodeSet_Utf8ToCurrentTranslit
#define CodeSetOld_CurrentToUtf8 CodeSet_CurrentToUtf8
#define CodeSetOld_Utf16leToUtf8_Db CodeSet_Utf16leToUtf8_Db
#define CodeSetOld_Utf16leToUtf8 CodeSet_Utf16leToUtf8
#define CodeSetOld_Utf8ToUtf16le CodeSet_Utf8ToUtf16le
#define CodeSetOld_CurrentToUtf16le CodeSet_CurrentToUtf16le
#define CodeSetOld_Utf16leToCurrent CodeSet_Utf16leToCurrent
#define CodeSetOld_Utf16beToCurrent CodeSet_Utf16beToCurrent
#define CodeSetOld_Utf8FormDToUtf8FormC CodeSet_Utf8FormDToUtf8FormC
#define CodeSetOld_Utf8FormCToUtf8FormD CodeSet_Utf8FormCToUtf8FormD
#define CodeSetOld_GetCurrentCodeSet CodeSet_GetCurrentCodeSet
#define CodeSetOld_IsEncodingSupported CodeSet_IsEncodingSupported
#define CodeSetOld_Init CodeSet_Init
#endif

Bool
CodeSetOld_GenericToGenericDb(char const *codeIn,  // IN
                              char const *bufIn,   // IN
                              size_t sizeIn,       // IN
                              char const *codeOut, // IN
                              unsigned int flags,  // IN
                              DynBuf *db);         // IN/OUT

Bool
CodeSet_OldGenericToGeneric(const char *codeIn,  // IN
                            const char *bufIn,   // IN
                            size_t sizeIn,       // IN
                            const char *codeOut, // IN
                            unsigned int flags,  // IN
                            char **bufOut,       // IN/OUT
                            size_t *sizeOut);    // IN/OUT

Bool
CodeSetOld_Utf8ToCurrent(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf8ToCurrentTranslit(char const *bufIn,      // IN
                                 size_t sizeIn,    // IN
                                 char **bufOut,          // OUT
                                 size_t *sizeOut); // OUT

Bool
CodeSetOld_CurrentToUtf8(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf16leToUtf8_Db(char const *bufIn,   // IN
                            size_t sizeIn, // IN
                            DynBuf *db);         // IN

Bool
CodeSetOld_Utf16leToUtf8(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf8ToUtf16le(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSetOld_CurrentToUtf16le(char const *bufIn,      // IN
                            size_t sizeIn,    // IN
                            char **bufOut,          // OUT
                            size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf16leToCurrent(char const *bufIn,      // IN
                            size_t sizeIn,    // IN
                            char **bufOut,          // OUT
                            size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf16beToCurrent(char const *bufIn,      // IN
                            size_t sizeIn,    // IN
                            char **bufOut,          // OUT
                            size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf8Normalize(const char *bufIn,     // IN
                         size_t sizeIn,         // IN
                         Bool precomposed,      // IN
                         DynBuf *db);           // OUT

Bool
CodeSetOld_Utf8FormDToUtf8FormC(char const *bufIn,     // IN
                                size_t sizeIn,         // IN
                                char **bufOut,         // OUT
                                size_t *sizeOut);      // OUT

Bool
CodeSetOld_Utf8FormCToUtf8FormD(char const *bufIn,     // IN
                                size_t sizeIn,         // IN
                                char **bufOut,         // OUT
                                size_t *sizeOut);      // OUT

const char *
CodeSetOld_GetCurrentCodeSet(void);

Bool
CodeSetOld_IsEncodingSupported(const char *name); // IN

Bool
CodeSetOld_Init(void);

#endif /* __CODESET_OLD_H__ */
