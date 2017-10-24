/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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

#define UNICODE_BUILDING_POSIX_WRAPPERS
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

#include "vmware.h"
#include "posixInt.h"


#if !defined(_WIN32)
/*
 *----------------------------------------------------------------------
 *
 * Posix_Dlopen --
 *
 *      POSIX dlopen()
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

void *
Posix_Dlopen(const char *pathName,  // IN:
             int flag)              // IN:
{
   char *path;
   void *ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   ret = dlopen(path, flag);

   Posix_Free(path);
   return ret;
}
#endif
