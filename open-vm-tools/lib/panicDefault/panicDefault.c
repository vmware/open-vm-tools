/*********************************************************
 * Copyright (C) 2006-2016, 2021 VMware, Inc. All rights reserved.
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
 * panic.c --
 *
 *      Basic Panic()
 */


#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "vmware.h"
#include "panic.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Panic --
 *
 *      Minimal panic.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Print message, force everything to quit.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic(const char *fmt,   // IN: message format
      ...)               // IN: message format arguments
{
   va_list ap;

   va_start(ap, fmt);
   Panic_Panic(fmt, ap);
   va_end(ap);
}
