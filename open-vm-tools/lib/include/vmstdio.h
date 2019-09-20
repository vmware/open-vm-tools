/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#ifndef __VMSTDIO_H__
#   define __VMSTDIO_H__

#include <stdio.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
   StdIO_Error,
   StdIO_EOF,
   StdIO_Success,
} StdIO_Status;

typedef void (*SnowMotionLogger)(char *buf, size_t count);

void
StdIO_ToggleSnowMotionLogging(SnowMotionLogger logger);

StdIO_Status
StdIO_ReadNextLine(FILE *stream,         // IN
                   char **buf,           // OUT
                   size_t maxBufLength,  // IN
                   size_t *count);       // OUT

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __VMSTDIO_H__ */
