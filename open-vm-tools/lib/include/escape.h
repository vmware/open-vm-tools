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

/*
 * escape.h --
 *
 *    Buffer escaping --hpreg
 */

#ifndef __ESCAPE_H__
#   define __ESCAPE_H__

#include "vmware.h"

#if defined(__cplusplus)
extern "C" {
#endif

void *
Escape_DoString(const char *escStr,    // IN
                int const *bytesToEsc, // IN
                void const *bufIn,     // IN
                size_t sizeIn,         // IN
                size_t *sizeOut);      // OUT/OPT

void *
Escape_Do(char escByte,          // IN
          int const *bytesToEsc, // IN
          void const *bufIn,     // IN
          size_t sizeIn,         // IN
          size_t *sizeOut);      // OUT/OPT

void *
Escape_Undo(char escByte,      // IN
            void const *bufIn, // IN
            size_t sizeIn,     // IN
            size_t *sizeOut);  // OUT/OPT

Bool
Escape_UndoFixed(char escByte,        // IN
                 void const *bufIn,   // IN
                 size_t sizeIn,       // IN
                 void *bufOut,        // IN/OUT
                 size_t bufOutSize);  // IN

const char *
Escape_Strchr(char escByte,      // IN
              const char *bufIn, // IN
              char c);           // IN

char *
Escape_Unescape(char escByte,       // IN
                const char *bufIn); // IN

void *
Escape_AnsiToUnix(void const *bufIn, // IN
                  size_t sizeIn,     // IN
                  size_t *sizeOut);  // OUT/OPT

void *
Escape_Sh(void const *bufIn, // IN
          size_t sizeIn,     // IN
          size_t *sizeOut);  // OUT/OPT

void *
Escape_BRE(void const *bufIn, // IN
           size_t sizeIn,     // IN
           size_t *sizeOut);  // OUT/OPT

void
Escape_UnescapeCString(char *buf); // IN/OUT

char *
Escape_Comma(const char *string); // IN

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __ESCAPE_H__ */
