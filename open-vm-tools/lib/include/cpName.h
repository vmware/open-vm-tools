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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * cpName.h --
 *
 *    Cross-platform name format used by hgfs.
 *
 */

#ifndef __CP_NAME_H__
#define __CP_NAME_H__


#ifdef __KERNEL__
#  include "driver-config.h"
#  include <linux/string.h>
#elif defined __FreeBSD__
#   if defined _KERNEL
#      include <sys/libkern.h>
#      define strchr(s,c)       index(s,c)
#   else
#      include <string.h>
#   endif
#elif defined __APPLE__ && defined KERNEL
#  include <string.h>
#elif !defined sun
#  include <stdlib.h>
#  include <string.h>
#endif

#include "vm_basic_types.h"


/* Status codes for processing share names */
typedef enum {
   HGFS_NAME_STATUS_COMPLETE,            /* Name is complete */
   HGFS_NAME_STATUS_FAILURE,             /* Name processing failed */
   HGFS_NAME_STATUS_INCOMPLETE_BASE,     /* Name is base of namespace */
   HGFS_NAME_STATUS_INCOMPLETE_ROOT,     /* Name is "root" only */
   HGFS_NAME_STATUS_INCOMPLETE_DRIVE,    /* Name is "root drive" only */
   HGFS_NAME_STATUS_INCOMPLETE_UNC,      /* Name is "root unc" only */
   HGFS_NAME_STATUS_INCOMPLETE_UNC_MACH, /* Name is "root unc <x>" only */
   HGFS_NAME_STATUS_DOES_NOT_EXIST,      /* Name does not exist */
   HGFS_NAME_STATUS_ACCESS_DENIED,       /* Desired access to share denied */
   HGFS_NAME_STATUS_SYMBOLIC_LINK,       /* Name contains a symbolic link */
   HGFS_NAME_STATUS_OUT_OF_MEMORY,       /* Out of memory while processing */
   HGFS_NAME_STATUS_TOO_LONG,            /* Name has overly long component */
   HGFS_NAME_STATUS_NOT_A_DIRECTORY,     /* Name has path component not a dir */
} HgfsNameStatus;


int
CPName_ConvertTo(char const *nameIn, // IN:  The buf to convert
                 size_t bufOutSize,  // IN:  The size of the output buffer
                 char *bufOut);      // OUT: The output buffer

int
CPName_LinuxConvertTo(char const *nameIn, // IN:  buf to convert
                      size_t bufOutSize,  // IN:  size of the output buffer
                      char *bufOut);      // OUT: output buffer

int
CPName_WindowsConvertTo(char const *nameIn, // IN:  buf to convert
                        size_t bufOutSize,  // IN:  size of the output buffer
                        char *bufOut);      // OUT: output buffer

int
CPName_ConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                   size_t *inSize,     // IN/OUT: Size of input buffer
                   size_t *outSize,    // IN/OUT: Size of output buffer
                   char **bufOut);     // IN/OUT: Output buffer

HgfsNameStatus
CPName_ConvertFromRoot(char const **bufIn, // IN/OUT: Input to convert
                       size_t *inSize,     // IN/OUT: Size of input
                       size_t *outSize,    // IN/OUT: Size of output buf
                       char **bufOut);     // IN/OUT: Output buffer

int
CPName_GetComponent(char const *begin,  // IN: Beginning of buffer
                    char const *end,    // IN: End of buffer
                    char const **next); // OUT: Next component

char const *
CPName_Print(char const *in, // IN: Name to print
             size_t size);   // IN: Size of name


#endif /* __CP_NAME_H__ */
