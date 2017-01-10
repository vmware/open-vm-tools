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
 * stub-user-util.c --
 *
 *   Stubs for Util_* functions in lib/user.
 *
 */

#if defined(_WIN32)
#  include <windows.h>
#endif

#include <stdlib.h>
#include "vm_assert.h"
#include "util.h"

void
Util_Backtrace(int bugNr)
{
   NOT_IMPLEMENTED();
}


void
Util_ExitProcessAbruptly(int code) // IN
{
#if defined(_WIN32)
   TerminateProcess(GetCurrentProcess(),code);
#else
   exit(code);
#endif
}

int
Util_HasAdminPriv()
{
   return 1;
}
