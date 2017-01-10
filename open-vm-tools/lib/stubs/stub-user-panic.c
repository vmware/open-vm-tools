/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
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
 * stub-user-panic.c --
 *
 *   Stubs for Panic_* functions in lib/user.
 *
 */

#include <stdarg.h>
#include <stdio.h>

#include "vmware.h"
#include "panic.h"
#include "str.h"


void
Panic_PostPanicMsg(const char *fmt, ...)
{
   char *str;
   va_list args;

   va_start(args, fmt);
   str = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   if (str != NULL) {
      fputs(str, stderr);
   }
}

