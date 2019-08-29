/*********************************************************
 * Copyright (C) 2009-2019 VMware, Inc. All rights reserved.
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

Bool UtilConstTimeMemDiff(const void *secret, const void *guess, size_t len, size_t *diffCount);
Bool UtilConstTimeStrDiff(const char *secret, const char *guess, size_t *diffCount);


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
 *
 *      Helper functions for Util_SafeMalloc.
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
 *
 *      Helper functions for Util_SafeRealloc.
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
 *
 *      Helper functions for Util_SafeCalloc.
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
 * UtilSafeStrdup0 --
 * UtilSafeStrdup1 --
 *
 *      Helper functions for Util_SafeStrdup.
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
 * UtilSafeStrndup0 --
 * UtilSafeStrndup1 --
 *
 *      Helper functions for Util_SafeStrndup.
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

   return memcpy(copy, s, size);
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

   return memcpy(copy, s, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Memdup --
 *
 *      Allocates a copy of data.
 *
 * Results:
 *      Returns a pointer to the allocated copy.  The caller is responsible for
 *      freeing it with free().  Returns NULL on failure or if the input size
 *      is 0.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
Util_Memdup(const void *src, // IN:
            size_t size)     // IN:
{
   void *dest;

   if (size == 0) {
      return NULL;
   }

   ASSERT(src != NULL);

   dest = malloc(size);
   if (dest != NULL) {
      Util_Memcpy(dest, src, size);
   }
   return dest;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Memcpy --
 *
 *      Version of memcpy intended to accelerate aligned copies.
 *
 *      Expected benefits:
 *      2-4x performance improvemenet for small buffers (count <= 256 bytes)
 *      Equivalent performance on mid-sized buffers (256 bytes < count < 4K)
 *      ~25% performance improvement on large buffers (4K < count)
 *
 *      Has a drawback that falling through to standard memcpy has overhead
 *      of 5 instructions and 2 branches.
 *
 * Results:
 *      Returns a pointer to the destination buffer.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void *
Util_Memcpy(void *dest,      // OUT:
            const void *src, // IN:
            size_t count)    // IN:
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


/*
 *-----------------------------------------------------------------------------
 *
 * Util_Memfree --
 *
 *      Frees the memory space pointed to by ptr.
 *
 *      The reason why this function is externally visible (not static)
 *      is to avoid freeing memory across dll boundary.
 *      In vmwarebase, we have many API that return newly allocated memory
 *      to the caller. If the caller linked against a different msvc runtime
 *      (for example, vmrest linked against msvcrt.dll), we will crash.
 *      Using Util_Memfree() can avoid this kind of problem, since it sits
 *      inside vmwarebase too. It will call the right free(), the one that
 *      match the malloc() used in vmwarebase.
 *
 * Results:
 *      The memory space pointed to by ptr will be freed.
 *      If ptr is NULL, no operation is performed.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
Util_Memfree(void *ptr) // IN:
{
   free(ptr);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilConstTimeMemDiff --
 *
 *       The implementation of a constant time memory comparison.  Unlike
 *       memcmp, this function does not return early if it finds a mismatch.
 *       It always examines the entire 'secret' and 'guess' buffers, so that
 *       the time spent in this function is constant for buffers of the same
 *       given 'len'.  (We don't attempt to make the time invariant for
 *       different buffer lengths.)
 *
 *       The reason why this function is externally visible (not static)
 *       and has a 'diffCount' argument is to try to prevent aggressive
 *       compiler optimization levels from short-circuiting the inner loop.
 *       The possibility of a call from outside this module with a non-NULL
 *       diffCount pointer prevents that optimization.  If we didn't have
 *       to worry about that then we wouldn't need this function; we could
 *       have put the implementation directly into Util_ConstTimeMemDiff.
 *
 * Results:
 *       Returns true if the buffers differ, false if they are identical.
 *       If diffCount is non-NULL, sets *diffCount to the total number of
 *       differences between the buffers.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UtilConstTimeMemDiff(const void *secret,    // IN
                     const void *guess,     // IN
                     size_t len,            // IN
                     size_t *diffCount)     // OUT: optional
{
   const char *secretChar = secret;
   const char *guessChar = guess;

   size_t numDiffs = 0;

   while (len--) {
      numDiffs += !!(*secretChar ^ *guessChar);
      ++secretChar;
      ++guessChar;
   }

   if (diffCount != NULL) {
      *diffCount = numDiffs;
   }
   return numDiffs != 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ConstTimeMemDiff --
 *
 *       Performs a constant time memory comparison.
 *
 *       The return values are chosen to make this as close as possible to
 *       a drop-in replacement for memcmp, so we return false (0) if the
 *       buffers match (ie there are zero differences) and true (1) if the
 *       buffers differ.
 *
 * Results:
 *       Returns zero if the buffers are identical, 1 if they differ.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_ConstTimeMemDiff(const void *secret,  // IN
                      const void *guess,   // IN
                      size_t len)          // IN
{
   return UtilConstTimeMemDiff(secret, guess, len, NULL);
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilConstTimeStrDiff --
 *
 *       The implementation of a constant time string comparison.  Unlike
 *       strcmp, this function does not return early if it finds a mismatch.
 *       It always compares the entire 'secret' string against however much
 *       of the 'guess' string is required for that comparison, so that the
 *       time spent in this function is constant for secrets of the same
 *       length.  (We don't attempt to make the time invariant for secrets
 *       of different lengths.)
 *
 *       The reason why this function is externally visible (not static)
 *       and has a 'diffCount' argument is to try to prevent aggressive
 *       compiler optimization levels from short-circuiting the inner
 *       loop.  The possibility of a call from outside this module with a
 *       non-NULL diffCount pointer prevents that optimization.  If we
 *       didn't have to worry about that then we wouldn't need this
 *       function; we could have put the implementation directly into
 *       Util_ConstTimeStrDiff.
 *
 * Results:
 *       Returns true if the strings differ, false if they are identical.
 *       If diffCount is non-NULL, sets *diffCount to the total number of
 *       differences between the strings.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
UtilConstTimeStrDiff(const char *secret,    // IN
                     const char *guess,     // IN
                     size_t *diffCount)     // OUT: optional
{
   size_t numDiffs = 0;

   do {
      numDiffs += !!(*secret ^ *guess);
      guess += !!(*guess);
   } while (*secret++);

   if (diffCount != NULL) {
      *diffCount = numDiffs;
   }
   return numDiffs != 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_ConstTimeStrDiff --
 *
 *       The implementation of a constant time string comparison.
 *
 *       The return values are chosen to make this as close as possible
 *       to a drop-in replacement for strcmp, so we return 0 if
 *       the buffers match (ie there are zero differences) and 1
 *       if the buffers differ.
 *
 * Results:
 *       Returns zero if the strings are identical, 1 if they differ.
 *
 * Side Effects:
 *       None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_ConstTimeStrDiff(const char *secret,  // IN
                      const char *guess)   // IN
{
   return UtilConstTimeStrDiff(secret, guess, NULL);
}
