/*********************************************************
 * Copyright (C) 2008-2020 VMware, Inc. All rights reserved.
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
 * stub-config.c --
 *
 *   Stubs for lib/config.
 *
 */

#include <string.h>

#include "vm_assert.h"
#include "config.h"
#include "preference.h"


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


double
Config_GetDouble(double defaultValue,
                 const char *fmt,
                 ...)
{
   return defaultValue;
}


char *
Config_GetString(const char *defaultValue,
                 const char *fmt, ...)
{
   return (defaultValue == NULL) ? NULL : strdup(defaultValue);
}

Bool
Preference_Init(void)
{
   return TRUE;
}

void
Preference_Exit(void)
{
}

Bool
Preference_GetBool(Bool defaultValue,
                   const char *name)
{
   return defaultValue;
}


char *
Preference_GetString(const char *defaultValue,
                     const char *name)
{
   return (defaultValue == NULL) ? NULL : strdup(defaultValue);
}


char *
Preference_GetPathName(const char *defaultValue,
                       const char *fmt)
{
   return (defaultValue == NULL) ? NULL : strdup(defaultValue);
}

int32
Config_GetTriState(int32 defaultValue,
                   const char *fmt, ...)
{
   return defaultValue;
}

char *
Config_GetPathName(const char *defaultValue,
                   const char *format, ...)
{
   return defaultValue ? strdup(defaultValue) : NULL;
}

Bool
Config_NotSet(const char *fmt, ...)
{
   return FALSE;
}

int32
Preference_GetLong(int32 defaultValue,
                   const char *name)
{
   return defaultValue;
}

