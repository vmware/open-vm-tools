/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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
#include "util.h"
#include "dynbuf.h"
#include "strutil.h"


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


/*
 *-----------------------------------------------------------------------------
 *
 * Log_BufBegin --
 *
 *      Obtain a line accumulator.
 *
 * Results:
 *      A pointer to the line accumulator, a DynBuf.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
Log_BufBegin(void)
{
   DynBuf *b = Util_SafeCalloc(1, sizeof *b);

   DynBuf_Init(b);

   return b;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Log_BufAppend --
 *
 *      Append data to the specified line accumulator.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Log_BufAppend(void *acc,        // IN/OUT:
              const char *fmt,  // IN:
              ...)              // IN/OUT:
{
   va_list args;
   Bool success;

   ASSERT(acc != NULL);
   ASSERT(fmt != NULL);

   va_start(args, fmt);
   success = StrUtil_VDynBufPrintf((DynBuf *) acc, fmt, args);
   va_end(args);

   VERIFY(success);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Log_BufEndLevel --
 *
 *      Issue the contents of the specified line accumulator to the Log
 *      Facility using the specified routing information.
 *
 *      The line accumulator is destroyed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Log_BufEndLevel(void *acc,       // IN/OUT:
                uint32 routing)  // IN:
{
   ASSERT(acc != NULL);

   Log_Level(routing, "%s", (char *) DynBuf_GetString((DynBuf *) acc));

   DynBuf_Destroy((DynBuf *) acc);
   free(acc);
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
