/*********************************************************
 * Copyright (c) 1998-2025 Broadcom. All Rights Reserved.
 * The term "Broadcom" refers to Broadcom Inc. and/or its subsidiaries.
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
 * vmstdio.h --
 *
 *    Functions that operate on FILE objects --hpreg
 */

#ifndef VMSTDIO_H
#define VMSTDIO_H

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
   StdIO_Error,
   StdIO_EOF,
   StdIO_Success,
} StdIO_Status;

StdIO_Status
StdIO_ReadNextLine(FILE *stream,         // IN
                   char **buf,           // OUT
                   size_t maxBufLength,  // IN
                   size_t *count);       // OUT

char *
StdIO_PromptUser(FILE *out,           // IN
                 const char *prompt,  // IN
                 Bool echo);          // IN

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* VMSTDIO_H */
