/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

#include <stdlib.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "vmware.h"
#include "str.h"


/*
 * Stubs for symbols needed by vmGuestLib
 */

void
Debug(const char *fmt, ...) // IN
{
   // do nothing
}

void
Log(const char *fmt, ...) // IN
{
   // do nothing
}


void
Warning(const char *fmt, ...) // IN
{
   // do nothing
}


int64
File_GetModTime(const char *fileName)   // IN
{
   struct stat statBuf;
   if (stat(fileName, &statBuf) == -1) {
      return -1;
   }
   return (int64)statBuf.st_mtime;
}
