/*********************************************************
 * Copyright (C) 2008-2016,2019 VMware, Inc. All rights reserved.
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
#include <string.h>
#include <ctype.h>
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


void
Log_Level(uint32 routing,   // IN:
          const char *fmt,  // IN:
          ...)              // IN or OUT: depends on 'fmt'
{
   va_list ap;

   va_start(ap, fmt);
   LogV(routing, fmt, ap);
   va_end(ap);
}


void
Log_HexDumpLevel(uint32 routing,      // IN:
                 const char *prefix,  // IN: prefix for each log line
                 const void *data,    // IN: data to log
                 size_t size)         // IN: number of bytes
{
   size_t i = 0;

   while (i < size) {
      char hex[16 * 3 + 1];
      char ascii[16 + 1];
      unsigned j;

      memset(hex, ' ', sizeof hex - 1);
      hex[sizeof hex - 1] = 0;
      memset(ascii, ' ', sizeof ascii - 1);
      ascii[sizeof ascii - 1] = 0;

      for (j = 0; j < 16 && i < size; j++, i++) {
         uint8 c = ((const uint8 *)data)[i];

         hex[j * 3 + 0] = "0123456789abcdef"[c >> 4];
         hex[j * 3 + 1] = "0123456789abcdef"[c & 0xf];
         ascii[j] = isprint(c) ? c : '.';
      }

      Log_Level(routing, "%s %03"FMTSZ"x: %s%s\n", prefix, i - j, hex, ascii);
   }
}


void
Log_HexDump(const char *prefix,  // IN: prefix for each log line
            const void *data,    // IN: data to log
            size_t size)         // IN: number of bytes
{
   Log_HexDumpLevel(VMW_LOG_INFO, prefix, data, size);
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
