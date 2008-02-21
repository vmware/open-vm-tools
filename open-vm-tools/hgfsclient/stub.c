/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * stub.c --
 *
 *   Stub for unuseful functions. Stolen from the Tools upgrader.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdarg.h>
#include <string.h>

#ifdef _MSC_VER
#   include <io.h>
#   include <windows.h>
#endif


#include "vm_assert.h"
#include "str.h"
#include "debug.h"

typedef int MsgSeverity;


Bool
Config_GetBool(Bool defaultValue,
               const char *fmt,
               ...)
{
   return defaultValue;
}


int32
Config_GetLong(int32 defaultValue,
               const char *fmt,
               ...)
{
   return defaultValue;
}


void 
Msg_Append(const char *id, 
           const char *fmt,
           ...)
{
   static char buf[1000];
   
   va_list args;
   va_start(args, fmt);
   Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   Warning("Msg_Append: %s\n", buf);
}


unsigned int 
Msg_Question(void *buttons,
	     char const *id,
             char const *fmt,
             ...)
{
   static char buf[1000];
   
   va_list args;
   va_start(args, fmt);
   Str_Vsnprintf(buf, sizeof buf, fmt, args);
   va_end(args);

   Warning("Msg_Question: %s\n", buf);

   return 0;
}


Bool
Preference_GetBool(Bool defaultValue, 
                   const char *name) 
{
   return defaultValue;
}


char *
Preference_GetString(char *defaultValue, 
                     const char *name) 
{
   return defaultValue;
}

const char *
Hostinfo_NameGet(void)
{
   NOT_IMPLEMENTED();
}

char *
Util_ExpandString(const char *fileName)
{
   NOT_IMPLEMENTED();
}
