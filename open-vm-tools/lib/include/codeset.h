/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * codeset.h --
 *
 *    Character set and encoding conversion functions --hpreg
 */

#ifndef __CODESET_H__
#   define __CODESET_H__


#include "vm_basic_types.h"
#include "dynbuf.h"


Bool
CodeSet_Utf8ToCurrent(char const *bufIn,      // IN
                      size_t sizeIn,    // IN
                      char **bufOut,          // OUT
                      size_t *sizeOut); // OUT

Bool
CodeSet_Utf8ToCurrentTranslit(char const *bufIn,      // IN
                              size_t sizeIn,    // IN
                              char **bufOut,          // OUT
                              size_t *sizeOut); // OUT

Bool
CodeSet_CurrentToUtf8(char const *bufIn,      // IN
                      size_t sizeIn,    // IN
                      char **bufOut,          // OUT
                      size_t *sizeOut); // OUT

Bool
CodeSet_Utf16leToUtf8_Db(char const *bufIn,   // IN
                         size_t sizeIn, // IN
                         DynBuf *db);         // IN

Bool
CodeSet_Utf16leToUtf8(char const *bufIn,      // IN
                      size_t sizeIn,    // IN
                      char **bufOut,          // OUT
                      size_t *sizeOut); // OUT

Bool
CodeSet_Utf8ToUtf16le(char const *bufIn,      // IN
                      size_t sizeIn,    // IN
                      char **bufOut,          // OUT
                      size_t *sizeOut); // OUT

Bool
CodeSet_CurrentToUtf16le(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSet_Utf16leToCurrent(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSet_Utf16beToCurrent(char const *bufIn,      // IN
                         size_t sizeIn,    // IN
                         char **bufOut,          // OUT
                         size_t *sizeOut); // OUT

Bool
CodeSet_Utf8FormDToUtf8FormC(char const *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut);      // OUT

Bool
CodeSet_Utf8FormCToUtf8FormD(char const *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut);      // OUT

#endif /* __CODESET_H__ */
