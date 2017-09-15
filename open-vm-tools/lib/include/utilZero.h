/*********************************************************
 * Copyright (C) 2017 VMware, Inc. All rights reserved.
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
 * utilZero.h --
 *
 *    Utility functions for zeroing memory, and verifying memory is
 *    zeroed.
 */

#ifndef UTIL_ZERO_H
#define UTIL_ZERO_H

#define INCLUDE_ALLOW_MODULE
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <string.h>
#ifndef VMKBOOT
#include <stdlib.h>
#endif
#ifndef _WIN32
   #include <unistd.h>
   #include <sys/types.h>
   #include <errno.h>
#endif

#include "vm_assert.h"
#include "vm_basic_defs.h"
#include "unicodeTypes.h"

#if defined(__cplusplus)
extern "C" {
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ValidateBytes --
 *
 *      Check that memory is filled with the specified value.
 *
 * Results:
 *      NULL   No error
 *      !NULL  First address that doesn't have the proper value
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
Util_ValidateBytes(const void *ptr,  // IN: ptr to check
                   size_t size,      // IN: size of ptr
                   uint8 byteValue)  // IN: memory must be filled with this
{
   uint8 *p;
   uint8 *end;
   uint64 bigValue;

   ASSERT(ptr);

   if (size == 0) {
      return NULL;
   }

   p = (uint8 *) ptr;
   end = p + size;

   /* Compare bytes until a "nice" boundary is achieved. */
   while ((uintptr_t) p % sizeof bigValue) {
      if (*p != byteValue) {
         return p;
      }

      p++;

      if (p == end) {
         return NULL;
      }
   }

   /* Compare using a "nice sized" chunk for a long as possible. */
   memset(&bigValue, (int) byteValue, sizeof bigValue);

   while (p + sizeof bigValue <= end) {
      if (*((uint64 *) p) != bigValue) {
         /* That's not right... let the loop below report the exact address. */
         break;
      }

      size -= sizeof bigValue;
      p += sizeof bigValue;
   }

   /* Handle any trailing bytes. */
   while (p < end) {
      if (*p != byteValue) {
         return p;
      }

      p++;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_BufferIsEmpty --
 *
 *    Determine if the specified buffer of 'len' bytes starting at 'base'
 *    is empty (i.e. full of zeroes).
 *
 * Results:
 *    TRUE  Yes
 *    FALSE No
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
Util_BufferIsEmpty(void const *base,  // IN:
                   size_t len)        // IN:
{
   return Util_ValidateBytes(base, len, '\0') == NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Zero --
 *
 *      Zeros out bufSize bytes of buf. NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_Zero(void *buf,       // OUT
          size_t bufSize)  // IN
{
   if (buf != NULL) {
#if defined _WIN32 && defined USERLEVEL
      /*
       * Simple memset calls might be optimized out.  See CERT advisory
       * MSC06-C.
       */
      SecureZeroMemory(buf, bufSize);
#else
      memset(buf, 0, bufSize);
#if !defined _WIN32
      /*
       * Memset calls before free might be optimized out.  See PR1248269.
       */
      __asm__ __volatile__("" : : "r"(&buf) : "memory");
#endif
#endif
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroString --
 *
 *      Zeros out a NULL-terminated string. NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroString(char *str)  // IN/OUT/OPT
{
   if (str != NULL) {
      Util_Zero(str, strlen(str));
   }
}


#ifndef VMKBOOT
/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFree --
 *
 *      Zeros out bufSize bytes of buf, and then frees it. NULL is
 *      legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      buf is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFree(void *buf,       // OUT/OPT
              size_t bufSize)  // IN
{
   if (buf != NULL) {
      // See Posix_Free.
      int err = errno;
      Util_Zero(buf, bufSize);
      free(buf);
      errno = err;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFreeString --
 *
 *      Zeros out a NULL-terminated string, and then frees it. NULL is
 *      legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      str is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFreeString(char *str)  // IN/OUT/OPT
{
   if (str != NULL) {
      // See Posix_Free.
      int err = errno;
      Util_ZeroString(str);
      free(str);
      errno = err;
   }
}
#endif /* VMKBOOT */


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * Util_ZeroFreeStringW --
 *
 *      Zeros out a NUL-terminated wide-character string, and then frees it.
 *      NULL is legal.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      str is zeroed, and then free() is called on it.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
Util_ZeroFreeStringW(wchar_t *str)  // IN/OUT/OPT
{
   if (str != NULL) {
      // See Posix_Free.
      int err = errno;
      Util_Zero(str, wcslen(str) * sizeof *str);
      free(str);
      errno = err;
   }
}
#endif /* _WIN32 */

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* UTIL_ZERO_H */
