/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * staticEscape.h --
 *
 *    Buffer escaping using bit vectors instead of arrays
 *    and static buffers instead of dynbufs. [bac]
 *
 *    - Unescaping is done in place and cannot fail. 
 *    - Escaping's results are put into the caller-provided static
 *    buffer, and it fails if the buffer is too small.
 */

#ifndef __STATIC_ESCAPE_H__
#define __STATIC_ESCAPE_H__

#include "escBitvector.h"
int
StaticEscape_Do(char escByte,                   // IN
                EscBitVector const *bytesToEsc, // IN
                void const *bufIn,              // IN
                unsigned int sizeIn,            // IN
                unsigned int sizeBufout,        // IN
                char *bufOut);                  // OUT

int
StaticEscape_Undo(char escByte,          // IN
                  void *bufIn,           // IN
                  unsigned int sizeIn);  // IN

#if defined(_WIN32)
/* Wide character versions of the escape routines. */

int
StaticEscape_DoW(wchar_t escByte,                // IN
                 wchar_t const *bytesToEsc,      // IN
                 void const *bufIn,              // IN
                 unsigned int sizeIn,            // IN
                 unsigned int sizeBufout,        // IN
                 void *bufOut);                  // OUT

int
StaticEscape_UndoW(wchar_t escByte,          // IN
                   wchar_t *bufIn,           // IN
                   unsigned int sizeIn);     // IN

int
StaticEscape_UndoWToA(char escChar,     // IN
                      char *bufIn,      // IN
                      uint32 sizeIn);   // IN
#endif
#endif /* __STATIC_ESCAPE_H__ */
