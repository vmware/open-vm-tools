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
 * escape.h --
 *
 *    Buffer escaping --hpreg
 */

#ifndef __ESCAPE_H__
#   define __ESCAPE_H__

#include "vmware.h"

void *
Escape_DoString(const char *escStr,    // IN
                int const *bytesToEsc, // IN
                void const *bufIn,     // IN
                size_t sizeIn,         // IN
                size_t *sizeOut);      // OUT

void *
Escape_Do(char escByte,          // IN
          int const *bytesToEsc, // IN
          void const *bufIn,     // IN
          size_t sizeIn,         // IN
          size_t *sizeOut);      // OUT

void *
Escape_Undo(char escByte,      // IN
            void const *bufIn, // IN
            size_t sizeIn,     // IN
            size_t *sizeOut);  // OUT

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
                  size_t *sizeOut);  // OUT

void *
Escape_Sh(void const *bufIn, // IN
          size_t sizeIn,     // IN
          size_t *sizeOut);  // OUT

void
Escape_UnescapeCString(char *buf); // IN/OUT

char *
Escape_Comma(const char *string); // IN


#endif /* __ESCAPE_H__ */
