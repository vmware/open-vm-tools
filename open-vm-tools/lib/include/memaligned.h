/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
 * memaligned.h --
 *
 *    misc util functions
 */

#ifndef _MEMALIGNED_H_
#define _MEMALIGNED_H_

#ifdef __linux__
#include <malloc.h>
#else
#include <stdlib.h>
#ifdef __FreeBSD__
#include <unistd.h>
#endif
#endif
#include "vmware.h"

#if defined __APPLE__ && !vm_x86_64
/*
 * Bug 471584: Mac OS X 10.6's valloc() implementation for 32-bit
 * processes can exhaust our process's memory space.
 *
 * Work around this by using our own simple page-aligned memory
 * allocation implementation based on malloc() for 32-bit processes.
 */
#define MEMALIGNED_USE_INTERNAL_IMPL 1
#endif


#ifdef MEMALIGNED_USE_INTERNAL_IMPL

/*
 *-----------------------------------------------------------------------------
 *
 * AlignedMallocImpl --
 *
 *      Internal implementation of page-aligned memory for operating systems
 *      that lack a working page-aligned allocation function.
 *
 *      Resulting pointer needs to be freed with AlignedFreeImpl.
 *
 * Result:
 *      A pointer.  NULL on out of memory condition.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
AlignedMallocImpl(size_t size) // IN
{
   size_t paddedSize;
   void **buf;
   void **alignedResult;

#define PAGE_MASK (PAGE_SIZE - 1)
#define PAGE_ROUND_DOWN(_value) ((uintptr_t)(_value) & ~PAGE_MASK)
#define PAGE_ROUND_UP(_value) PAGE_ROUND_DOWN((uintptr_t)(_value) + PAGE_MASK)

   /*
    * This implementation allocates PAGE_SIZE extra bytes with
    * malloc() to ensure the buffer spans a page-aligned memory
    * address, which we return.  (We could use PAGE_SIZE - 1 to save a
    * byte if we ensured 'size' was non-zero.)
    *
    * After padding, we allocate an extra pointer to hold the original
    * pointer returned by malloc() (stored immediately preceding the
    * page-aligned address).  We free this in AlignedFreeImpl().
    *
    * Finally, we allocate enough space to hold 'size' bytes.
    */
   paddedSize = PAGE_SIZE + sizeof *buf + size;

   // Check for overflow.
   if (paddedSize < size) {
      return NULL;
   }

   buf = (void **)malloc(paddedSize);
   if (!buf) {
      return NULL;
   }

   alignedResult = (void **)PAGE_ROUND_UP(buf + 1);
   *(alignedResult - 1) = buf;

#undef PAGE_MASK
#undef PAGE_ROUND_DOWN
#undef PAGE_ROUND_UP

   return alignedResult;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AlignedFreeImpl --
 *
 *      Internal implementation to free a page-aligned buffer allocated
 *      with AlignedMallocImpl.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
AlignedFreeImpl(void *buf) // IN
{
   if (!buf) {
      return;
   }

   free(*((void **)buf - 1));
}

#endif // MEMALIGNED_USE_INTERNAL_IMPL


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_UnsafeMalloc --
 *
 *      Alloc a chunk of memory aligned on a page boundary. Resulting pointer
 *      needs to be freed with Aligned_Free.
 *
 * Result:
 *      A pointer.  NULL on out of memory condition.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE void*
Aligned_UnsafeMalloc(size_t size) // IN
{
   void *buf;
#if defined MEMALIGNED_USE_INTERNAL_IMPL
   buf = AlignedMallocImpl(size);
#elif defined _WIN32
   buf = _aligned_malloc(size, PAGE_SIZE);
#elif __linux__
   buf = memalign(PAGE_SIZE, size);
#else // Apple, BSD, Solaris (tools)
   buf = valloc(size); 
#endif
   ASSERT(((uintptr_t)buf % PAGE_SIZE) == 0);

   return buf;
}


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_Malloc --
 *
 *      Alloc a chunk of memory aligned on a page boundary. Resulting pointer
 *      needs to be freed with Aligned_Free.  You should never use this
 *      function.  Especially if size was derived from guest provided data.
 *
 * Result:
 *      A pointer.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE void*
Aligned_Malloc(size_t size) // IN
{
   void *buf;

   buf = Aligned_UnsafeMalloc(size);
   ASSERT_MEM_ALLOC(buf);
   return buf;
}


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_Calloc --
 *
 *      Alloc a chunk of memory aligned on a page boundary. Resulting pointer
 *      needs to be freed with Aligned_Free.
 *
 * Result:
 *      A pointer.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE void*
Aligned_Calloc(size_t nmemb, // IN
               size_t size)  // IN
{
   void *buf = Aligned_Malloc(nmemb * size);
   ASSERT_MEM_ALLOC(buf);
   memset(buf, 0, nmemb * size);
   return buf;
}


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_Free --
 *
 *      Free a chunk of memory allocated using the 2 functions above.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

static INLINE void
Aligned_Free(void *buf)  // IN
{
#if defined MEMALIGNED_USE_INTERNAL_IMPL
   AlignedFreeImpl(buf);
#elif defined _WIN32
   _aligned_free(buf);
#else
   free(buf);
#endif
}

#endif

