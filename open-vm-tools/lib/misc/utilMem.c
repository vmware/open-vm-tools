/*********************************************************
 * Copyright (C) 2009-2015 VMware, Inc. All rights reserved.
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
 * utilMem.c --
 *
 *    misc util functions
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "vm_assert.h"
#include "util.h"

#ifdef __APPLE__
#include <TargetConditionals.h>
#if !defined TARGET_OS_IPHONE
#define TARGET_OS_IPHONE 0
#endif
#endif

static NORETURN void UtilAllocationFailure0(void);
static NORETURN void UtilAllocationFailure1(int bugNumber, 
                                            const char *file, int lineno);


static void
UtilAllocationFailure0(void)
{
   Panic("Unrecoverable memory allocation failure\n");
}


static void
UtilAllocationFailure1(int bugNumber, const char *file, int lineno)
{
   if (bugNumber == -1) {
      Panic("Unrecoverable memory allocation failure at %s:%d\n",
            file, lineno);
   } else {
      Panic("Unrecoverable memory allocation failure at %s:%d.  Bug "
            "number: %d\n", file, lineno, bugNumber);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilSafeMalloc0 --
 * UtilSafeMalloc1 --
 *      Helper function for malloc
 *
 * Results:
 *      Pointer to the dynamically allocated memory.
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

void *
UtilSafeMalloc0(size_t size)            // IN:
{
   void *result = malloc(size);
   if (result == NULL && size != 0) {
      UtilAllocationFailure0();
   }
   return result;
}


void *
UtilSafeMalloc1(size_t size,            // IN:
                int bugNumber,          // IN:
                const char *file,       // IN:
                int lineno)             // IN:
{
   void *result = malloc(size);
   if (result == NULL && size != 0) {
      UtilAllocationFailure1(bugNumber, file, lineno);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilSafeRealloc0 --
 * UtilSafeRealloc1 --
 *      Helper function for realloc
 *
 * Results:
 *      Pointer to the dynamically allocated memory.
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

void *
UtilSafeRealloc0(void *ptr,            // IN:
                 size_t size)          // IN:
{
   void *result = realloc(ptr, size);
   if (result == NULL && size != 0) {
      UtilAllocationFailure0();
   }
   return result;
}


void *
UtilSafeRealloc1(void *ptr,            // IN:
                 size_t size,          // IN:
                 int bugNumber,        // IN:
                 const char *file,     // IN:
                 int lineno)           // IN:
{
   void *result = realloc(ptr, size);
   if (result == NULL && size != 0) {
      UtilAllocationFailure1(bugNumber, file, lineno);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilSafeCalloc0 --
 * UtilSafeCalloc1 --
 *      Helper function for calloc
 *
 * Results:
 *      Pointer to the dynamically allocated memory.
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

void *
UtilSafeCalloc0(size_t nmemb,         // IN:
                size_t size)          // IN:
{
   void *result = calloc(nmemb, size);
   if (result == NULL && nmemb != 0 && size != 0) {
      UtilAllocationFailure0();
   }
   return result;
}


void *
UtilSafeCalloc1(size_t nmemb,         // IN:
                size_t size,          // IN:
                int bugNumber,        // IN:
                const char *file,     // IN:
                int lineno)           // IN:
{
   void *result = calloc(nmemb, size);
   if (result == NULL && nmemb != 0 && size != 0) {
      UtilAllocationFailure1(bugNumber, file, lineno);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_SafeStrdup --
 *      Helper function for strdup
 *
 * Results:
 *      Pointer to the dynamically allocated, duplicate string
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

char *
UtilSafeStrdup0(const char *s)        // IN:
{
   char *result;
   if (s == NULL) {
      return NULL;
   }
#if defined(_WIN32)
   if ((result = _strdup(s)) == NULL) {
#else
   if ((result = strdup(s)) == NULL) {
#endif
      UtilAllocationFailure0();
   }
   return result;
}


char *
UtilSafeStrdup1(const char *s,        // IN:
                int bugNumber,        // IN:
                const char *file,     // IN:
                int lineno)           // IN:
{
   char *result;
   if (s == NULL) {
      return NULL;
   }
#if defined(_WIN32)
   if ((result = _strdup(s)) == NULL) {
#else
   if ((result = strdup(s)) == NULL) {
#endif
      UtilAllocationFailure1(bugNumber, file, lineno);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_SafeStrndup --
 *
 *      Returns a string consisting of first n characters of 's' if 's' has
 *      length >= 'n', otherwise returns a string duplicate of 's'.
 *
 * Results:
 *      Pointer to the duplicated string.
 *
 * Side effects:
 *      May Panic if ran out of memory.
 *
 *-----------------------------------------------------------------------------
 */

char *
UtilSafeStrndup0(const char *s,        // IN:
                 size_t n)             // IN:
{
   size_t size;
   char *copy;
   const char *null;
   size_t newSize;

   if (s == NULL) {
      return NULL;
   }

   null = memchr(s, '\0', n);
   size = null ? (size_t)(null - s) : n;
   newSize = size + 1;
   if (newSize < size) {  // Prevent integer overflow
      copy = NULL;
   } else {
      copy = malloc(newSize);
   }

   if (copy == NULL) {
      UtilAllocationFailure0();
   }

   copy[size] = '\0';

   return (char *) memcpy(copy, s, size);
}


char *
UtilSafeStrndup1(const char *s,        // IN:
                 size_t n,             // IN:
                 int bugNumber,        // IN:
                 const char *file,     // IN:
                 int lineno)           // IN:
{
   size_t size;
   char *copy;
   const char *null;
   size_t newSize;

   if (s == NULL) {
      return NULL;
   }

   null = memchr(s, '\0', n);
   size = null ? (size_t)(null - s) : n;
   newSize = size + 1;
   if (newSize < size) {  // Prevent integer overflow
      copy = NULL;
   } else {
      copy = malloc(newSize);
   }

   if (copy == NULL) {
      UtilAllocationFailure1(bugNumber, file, lineno);
   }

   copy[size] = '\0';

   return (char *) memcpy(copy, s, size);
}


void *
Util_Memcpy(void *dest,
            const void *src,
            size_t count)
{
#if defined(__x86_64__) || defined(__i386__)
   uintptr_t align = ((uintptr_t)dest | (uintptr_t)src | count);

#if defined __GNUC__

   #if defined(__x86_64__)

      size_t dummy0;
      void *dummy1;
      void *dummy2;

      if ((align & 7) == 0) {
         __asm__ __volatile__("\t"
                               "cld"            "\n\t"
                               "rep ; movsq"    "\n"
                               : "=c" (dummy0), "=D" (dummy1), "=S" (dummy2)
                               : "0" (count >> 3), "1" (dest), "2" (src)
                               : "memory", "cc"
            );
         return dest;
      } else if ((align & 3) == 0) {
         __asm__ __volatile__("\t"
                               "cld"            "\n\t"
                               "rep ; movsd"    "\n"
                               : "=c" (dummy0), "=D" (dummy1), "=S" (dummy2)
                               : "0" (count >> 2), "1" (dest), "2" (src)
                               : "memory", "cc"
            );
         return dest;
      }

   #elif defined(__i386__)

      size_t dummy0;
      void *dummy1;
      void *dummy2;

      if ((align & 3) == 0) {
         __asm__ __volatile__("\t"
                               "cld"            "\n\t"
                               "rep ; movsd"    "\n"
                               : "=c" (dummy0), "=D" (dummy1), "=S" (dummy2)
                               : "0" (count >> 2), "1" (dest), "2" (src)
                               : "memory", "cc"
            );
         return dest;
      }

   #endif
  
#elif defined _MSC_VER

   #if defined(__x86_64__)

      if ((align & 7) == 0) {
         __movsq((uint64 *)dest, (uint64 *)src, count >> 3);
         return dest;
      } else if ((align & 3) == 0) {
         __movsd((unsigned long *)dest, (unsigned long *)src, count >> 2);
         return dest;
      }

   #elif defined(__i386__)

      if ((((uintptr_t)dest | (uintptr_t)src | count) & 3) == 0) {
         __movsd((unsigned long *)dest, (unsigned long *)src, count >> 2);
         return dest;
      }

   #endif


#endif
#endif

   memcpy(dest, src, count);
   return dest;
}
            

