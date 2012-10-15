/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * kernelStubsBSD.c --
 *
 *      Stub functions for use by miscellaneous VMware code when brought into
 *      the FreeBSD kernel.
 */


#include <sys/types.h>
#include <sys/param.h>
#include <machine/stdarg.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include "kernelStubs.h"

MALLOC_DEFINE(M_VMWARE_TEMP, "VMwareTemp", "VMware: Temporary Allocations");

void Log (const char *fmt, ...) __attribute__ ((alias ("Debug")));


/*
 *-----------------------------------------------------------------------------
 *
 * Debug --
 *
 *      Send a debugging message to the system log and/or console.
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
Debug(const char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Panic --
 *
 *      Print a panic message & induce a kernel panic.
 *
 * Results:
 *      Does not return.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...)
{
   va_list ap;

   va_start(ap, fmt);
   vprintf(fmt, ap);
   va_end(ap);

   panic(" ");
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Strcpy --
 *
 *      Wrapper around strcpy that panics if the source is too long.
 *
 * Results:
 *      Returns a pointer to buf.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_Strcpy(char *buf,           // OUT: destination buffer
           const char *src,     // IN : source buffer
           size_t maxSize)      // IN : size of buf
{
   size_t srcLen = strlen(src);
   if (srcLen >= maxSize) {
      panic("%s:%d Buffer too small %p\n", __FILE__, __LINE__, buf);
   }
   return memcpy(buf, src, srcLen + 1);    // Extra byte = terminator
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Vsnprintf --
 *
 *      Compatibility wrapper for vsnprintf(1).
 *
 * Results:
 *
 *      int - number of bytes stored in 'str' (not including null
 *      terminate character), -1 on overflow (insufficient space for
 *      null terminate is considered overflow)
 *
 *      NB: on overflow the buffer WILL be null terminated
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Str_Vsnprintf(char *str,                // OUT: destination buffer
              size_t size,              // IN : size of str
              const char *format,       // IN : format for vsnprintf
              va_list arguments)        // IN : variadic args for vsnprintf
{
   int retval;

   retval = vsnprintf(str, size, format, arguments);
   if (retval >= size) {
      retval = -1;
   }
   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Vasprintf --
 *
 *    Allocate and format a string, using the GNU libc way to specify the
 *    format (i.e. optionally allow the use of positional parameters)
 *
 * Results:
 *    The allocated string on success (if 'length' is not NULL, *length
 *       is set to the length of the allocated string)
 *    NULL on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_Vasprintf(size_t *length,       // OUT
              const char *format,   // IN
              va_list arguments)    // IN
{
   /*
    * Simple implementation of Str_Vasprintf when userlevel libraries are not
    * available (e.g. for use in drivers). We just fallback to vsnprintf,
    * doubling if we didn't have enough space.
    */
   unsigned int bufSize;
   char *buf;
   int retval;

   bufSize = strlen(format);
   buf = NULL;

   do {
      /*
       * Initial allocation of strlen(format) * 2. Should this be tunable?
       * XXX Yes, this could overflow and spin forever when you get near 2GB
       *     allocations. I don't care. --rrdharan
       */

      va_list tmpArgs;

      bufSize *= 2;
      buf = realloc(buf, bufSize);

      if (!buf) {
         return NULL;
      }

      va_copy(tmpArgs, arguments);
      retval = Str_Vsnprintf(buf, bufSize, format, tmpArgs);
      va_end(tmpArgs);
   } while (retval == -1);

   if (length) {
      *length = retval;
   }

   /*
    * Try to trim the buffer here to save memory?
    */
   return buf;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Str_Asprintf --
 *
 *    Same as Str_Vasprintf(), but parameters are passed inline --hpreg
 *
 * Results:
 *    Same as Str_Vasprintf()
 *
 * Side effects:
 *    Same as Str_Vasprintf()
 *
 *-----------------------------------------------------------------------------
 */

char *
Str_Asprintf(size_t *length,       // OUT
             const char *format,   // IN
             ...)                  // IN
{
   va_list arguments;
   char *result;

   va_start(arguments, format);
   result = Str_Vasprintf(length, format, arguments);
   va_end(arguments);

   return result;
}
