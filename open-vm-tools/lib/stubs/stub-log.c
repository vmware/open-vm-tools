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
 * stub-log.c --
 *
 *   Stub for lib/log.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include "str.h"
#include "log.h"


/*
 * XXX: the check is a hack to work around stupid libraries, like
 * bora/lib/install, that provide implementations for only some of
 * the functions of the real library, but not all.
 */
#if !defined(NO_LOG_STUB)

void
LogV(uint32 unused,
     const char *fmt,
     va_list args)
{
   char *str;

   str = Str_Vasprintf(NULL, fmt, args);
   if (str != NULL) {
      fputs(str, stderr);
      free(str);
   }
}


void
Log(const char *fmt,
    ...)
{
   va_list args;

   va_start(args, fmt);
   LogV(VMW_LOG_INFO, fmt, args);
   va_end(args);
}
#endif


void
Log_DisableThrottling(void)
{

}


const char *
Log_GetFileName(void)
{
   return NULL;
}

