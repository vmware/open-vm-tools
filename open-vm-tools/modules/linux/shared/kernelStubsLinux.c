/*********************************************************
 * Copyright (C) 2006-2014 VMware, Inc. All rights reserved.
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
 * kernelStubsLinux.c
 *
 * This file contains implementations of common userspace functions in terms
 * that the Linux kernel can understand.
 */

/* Must come before any kernel header file */
#include "driver-config.h"
#include "kernelStubs.h"
#include "compat_kernel.h"
#include "compat_page.h"
#include "compat_sched.h"
#include <linux/slab.h>

#include "vm_assert.h"


/*
 *-----------------------------------------------------------------------------
 *
 * Panic --
 *
 *    Prints the debug message and stops the system.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...) // IN
{
   va_list args;
   char *result;

   va_start(args, fmt);
   result = Str_Vasprintf(NULL, fmt, args);
   va_end(args);

   if (result) {
      printk(KERN_EMERG "%s", result);
   }

   BUG();

   while (1); // Avoid compiler warning.
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Strcpy--
 *
 *    Wrapper for strcpy that checks for buffer overruns.
 *
 * Results:
 *    Same as strcpy.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

char *
Str_Strcpy(char *buf,       // OUT
           const char *src, // IN
           size_t maxSize)  // IN
{
   size_t len;

   len = strlen(src);
   if (len >= maxSize) {
#ifdef GetReturnAddress
      Panic("%s:%d Buffer too small 0x%p\n", __FILE__, __LINE__, GetReturnAddress());
#else
      Panic("%s:%d Buffer too small\n", __FILE__, __LINE__);
#endif
   }
   return memcpy(buf, src, len + 1);
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Vsnprintf --
 *
 *	Compatability wrapper b/w different libc versions
 *
 * Results:
 *	int - number of bytes written (not including NULL terminate character),
 *	      -1 on overflow (insufficient space for NULL terminate is considered
 *	      overflow)
 *
 *	NB: on overflow the buffer WILL be null terminated
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

int
Str_Vsnprintf(char *str,          // OUT
              size_t size,        // IN
              const char *format, // IN
              va_list arguments)  // IN
{
   int retval;
   retval = vsnprintf(str, size, format, arguments);

   /*
    * Linux glibc 2.0.x returns -1 and null terminates (which we shouldn't
    * be linking against), but glibc 2.1.x follows c99 and returns
    * characters that would have been written.
    */
   if (retval >= size) {
      return -1;
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
      va_list args2;

      bufSize *= 2;
      buf = realloc(buf, bufSize);

      if (!buf) {
         return NULL;
      }

      va_copy(args2, arguments);
      retval = Str_Vsnprintf(buf, bufSize, format, args2);
      va_end(args2);
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


/*
 *-----------------------------------------------------------------------------
 *
 * strdup --
 *
 *    Duplicates a string.
 *
 * Results:
 *    A pointer to memory containing the duplicated string or NULL if no
 *    memory was available.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

char *
strdup(const char *source) // IN
{
   char *target = NULL;
   if (source) {

      /*
       * We call our special implementation of malloc() because the users of
       * strdup() will call free(), and that'll decrement the pointer before
       * freeing it. Thus, we need to make sure that the allocated block
       * also stores the block length before the block itself (see malloc()
       * below).
       */
      unsigned int len = strlen(source);
      target = malloc(len + 1);
      if (target) {
         memcpy(target, source, len + 1);
      }
   }

   return target;
}


/*
 *----------------------------------------------------------------------------
 *
 * mallocReal --
 *
 *      Allocate memory using kmalloc. There is no realloc
 *      equivalent, so we roll our own by padding each allocation with
 *      4 (or 8 for 64 bit guests) extra bytes to store the block length.
 *
 * Results:
 *      Pointer to driver heap memory, offset by 4 (or 8)
 *      bytes from the real block pointer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static void *
mallocReal(size_t size) // IN
{
   size_t *ptr;
   ptr = kmalloc(size + sizeof size, GFP_KERNEL);

   if (ptr) {
      *ptr++ = size;
   }
   return ptr;
}


/*
 *----------------------------------------------------------------------------
 *
 * malloc --
 *
 *      Allocate memory using the common mallocReal.
 *
 * Note: This calls mallocReal and not malloc as the gcc 5.1.1 optimizer
 *       will replace the malloc and memset with a calloc call. This results
 *       in calloc calling itself and results in system crashes. See bug 1413226.
 *
 * Results:
 *      Pointer to driver heap memory, offset by 4 (or 8)
 *      bytes from the real block pointer.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void *
malloc(size_t size) // IN
{
   return mallocReal(size);
}

/*
 *---------------------------------------------------------------------------
 *
 * free --
 *
 *     Free memory allocated by a previous call to malloc, calloc or realloc.
 *
 * Results:
 *     None.
 *
 * Side effects:
 *     Calls kfree to free the real (base) pointer.
 *
 *---------------------------------------------------------------------------
 */

void
free(void *mem) // IN
{
   if (mem) {
      size_t *dataPtr = (size_t *)mem;
      kfree(--dataPtr);
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * calloc --
 *
 *      Malloc and zero.
 *
 * Note: This calls mallocReal and not malloc as the gcc 5.1.1 optimizer
 *       will replace the malloc and memset with a calloc call. This results
 *       for system crashes when used by kernel components. See bug 1413226.
 *
 * Results:
 *      Pointer to driver heap memory (see malloc, above).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void *
calloc(size_t num, // IN
       size_t len) // IN
{
   size_t size;
   void *ptr;

   size = num * len;
   ptr = mallocReal(size);
   if (ptr) {
      memset(ptr, 0, size);
   }
   return ptr;
}


/*
 *----------------------------------------------------------------------------
 *
 * realloc --
 *
 *      Since the driver heap has no realloc equivalent, we have to roll our
 *      own. Fortunately, we can retrieve the block size of every block we
 *      hand out since we stashed it at allocation time (see malloc above).
 *
 * Results:
 *      Pointer to memory block valid for 'newSize' bytes, or NULL if
 *      allocation failed.
 *
 * Side effects:
 *      Could copy memory around.
 *
 *----------------------------------------------------------------------------
 */

void *
realloc(void* ptr,      // IN
        size_t newSize) // IN
{
   void *newPtr;
   size_t *dataPtr;
   size_t length, lenUsed;

   dataPtr = (size_t *)ptr;
   length = ptr ? dataPtr[-1] : 0;
   if (newSize == 0) {
      if (ptr) {
         free(ptr);
         newPtr = NULL;
      } else {
         newPtr = malloc(newSize);
      }
   } else if (newSize == length) {
      newPtr = ptr;
   } else if ((newPtr = malloc(newSize))) {
      if (length < newSize) {
         lenUsed = length;
      } else {
         lenUsed = newSize;
      }
      memcpy(newPtr, ptr, lenUsed);
      free(ptr);
   }
   return newPtr;
}


