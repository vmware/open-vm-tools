/*********************************************************
 * Copyright (C) 2009-2016 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * kernelStubsSolaris.c
 *
 * This file contains implementations of common userspace functions in terms
 * that the Solaris kernel can understand.
 */

#include "kernelStubs.h"
#include <sys/types.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>

#ifdef SOL9
# define compat_va_start(arg, fmt)  arg = ((char *)__builtin_next_arg(fmt))
# define compat_va_end(arg)
# define compat_va_copy(arg1, arg2) va_copy(arg1, arg2)
#else
# define compat_va_start(arg, fmt)  va_start(arg, fmt)
# define compat_va_end(arg)         va_end(arg)
# define compat_va_copy(arg1, arg2) va_copy(arg1, arg2)
#endif

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
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
Panic(const char *fmt, ...) // IN
{
   va_list args;
   char *result;

   compat_va_start(args, fmt);
   result = Str_Vasprintf(NULL, fmt, args);
   compat_va_end(args);

   cmn_err(CE_PANIC, "%s",
           result ? result : "Unable to format PANIC message");
}

/*
 *----------------------------------------------------------------------
 *
 * Str_Strcpy --
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
      Panic("%s:%d Buffer too small %p\n", __FILE__, __LINE__, buf);
   }
   bcopy(src, buf, len + 1);
   return buf;
}


/*
 *----------------------------------------------------------------------
 *
 * Str_Vsnprintf --
 *
 * Compatability wrapper b/w different libc versions
 *
 * Results:
 *    int - number of bytes written (not including NULL terminator),
 *          -1 on overflow (insufficient space for NULL terminator
 *          is considered overflow).
 *
 *    NB: on overflow the buffer WILL be null terminated.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------
 */

int
Str_Vsnprintf(char *str,          // OUT
              size_t size,        // IN
              const char *format, // IN
              va_list arguments)  // IN
{
   int retval = vsnprintf(str, size, format, arguments);

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
 *    format (i.e. optionally allow the use of positional parameters).
 *
 * Results:
 *    The allocated string on success (if 'length' is not NULL, *length
 *    is set to the length of the allocated string), NULL on failure.
 *
 * Side effects:
 *    None.
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

      compat_va_copy(args2, arguments);
      retval = Str_Vsnprintf(buf, bufSize, format, args2);
      compat_va_end(args2);
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

   compat_va_start(arguments, format);
   result = Str_Vasprintf(length, format, arguments);
   compat_va_end(arguments);

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * malloc --
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

void *
malloc(size_t size) // IN
{
   size_t *ptr = kmem_alloc(size + sizeof(size), KM_SLEEP);

   if (ptr) {
      *ptr++ = size;
   }
   return ptr;
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
 *     Calls kmem_free to free the real (base) pointer.
 *
 *---------------------------------------------------------------------------
 */

void
free(void *mem) // IN
{
   size_t *dataPtr = mem;

   if (mem) {
      --dataPtr;
      kmem_free(dataPtr, *dataPtr + sizeof(*dataPtr));
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * calloc --
 *
 *      Malloc and zero.
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
   ptr = malloc(size);
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
realloc(void *ptr,      // IN
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


