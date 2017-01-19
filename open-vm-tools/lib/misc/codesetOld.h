/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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

#include "dynbuf.h"
#include "codeset.h"  // for CURRENT_IS_UTF8


/*
 * These systems use iconv and nl_langinfo.
 * See the definition of CURRENT_IS_UTF8.
 */

#if !defined(CURRENT_IS_UTF8) && !defined(_WIN32)
   #define USE_ICONV
#endif


Bool
CodeSetOld_GenericToGenericDb(char const *codeIn,  // IN
                              char const *bufIn,   // IN
                              size_t sizeIn,       // IN
                              char const *codeOut, // IN
                              unsigned int flags,  // IN
                              DynBuf *db);         // IN/OUT

Bool
CodeSetOld_GenericToGeneric(const char *codeIn,  // IN
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
CodeSetOld_CurrentToUtf8(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSetOld_Utf16leToUtf8Db(char const *bufIn,   // IN
                           size_t sizeIn,	// IN
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

Bool
CodeSetOld_Utf16beToUtf8(char const *bufIn, // IN
                         size_t sizeIn,     // IN
                         char **bufOut,     // OUT
                         size_t *sizeOut);  // OUT

Bool
CodeSetOld_Utf16beToUtf8Db(char const *bufIn, // IN
                           size_t sizeIn,     // IN
                           DynBuf *db);       // IN

Bool
CodeSetOld_AsciiToUtf8(const char *bufIn,   // IN
                       size_t sizeIn,       // IN
                       unsigned int flags,  // IN
                       char **bufOut,       // OUT
                       size_t *sizeOut);    // OUT

Bool
CodeSetOld_AsciiToUtf8Db(char const *bufIn,   // IN
                         size_t sizeIn,       // IN
                         unsigned int flags,  // IN
                         DynBuf *db);         // OUT

Bool
CodeSetOld_Utf8ToAscii(const char *bufIn,   // IN
                       size_t sizeIn,       // IN
                       unsigned int flags,  // IN
                       char **bufOut,       // OUT
                       size_t *sizeOut);    // OUT

Bool
CodeSetOld_Utf8ToAsciiDb(char const *bufIn,   // IN
                         size_t sizeIn,       // IN
                         unsigned int flags,  // IN
                         DynBuf *db);         // OUT

const char *
CodeSetOld_GetCurrentCodeSet(void);

Bool
CodeSetOld_IsEncodingSupported(const char *name); // IN

Bool
CodeSetOld_Validate(const char *buf,   // IN: the string
                    size_t size,       // IN: length of string
                    const char *code); // IN: encoding

Bool
CodeSetOld_Init(const char *dataDir);  // UNUSED

#endif /* __CODESET_OLD_H__ */
