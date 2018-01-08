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
 * dictll.h --
 *
 *    Low-level dictionary format --hpreg
 */

#ifndef __DICTLL_H__
#   define __DICTLL_H__

#include "vm_basic_types.h"
#include "dynbuf.h"

#if defined(__cplusplus)
extern "C" {
#endif

int
DictLL_ReadLine(FILE *stream,  // IN
                char **line,   // OUT
                char **name,   // OUT
                char **value); // OUT

Bool
DictLL_WriteLine(FILE *stream,       // IN
                 char const *name,   // IN
                 char const *value); // IN

const char *
DictLL_UnmarshalLine(const char *buf,  // IN
                     size_t bufSize,   // IN
                     char **line,      // OUT
                     char **name,      // OUT
                     char **value);    // OUT

Bool
DictLL_MarshalLine(DynBuf *output,     // IN/OUT
                   char const *name,   // IN/OPT
                   char const *value); // IN


Bool
DictLL_ReadUTF8BOM(FILE *file); // IN/OUT

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __DICTLL_H__ */
