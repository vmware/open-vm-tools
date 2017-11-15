/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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

#if defined(__cplusplus)
extern "C" {
#endif

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

#undef PAGE_MASK
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

   return alignedResult;
}


/*
 *-----------------------------------------------------------------------------
 *
 * AlignedReallocImpl --
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
AlignedReallocImpl(void *oldbuf,    // IN
                   size_t newsize)  // IN
{
   size_t paddedSize;
   void **buf;
   void **alignedResult;
   void *oldptr = NULL;

   if (oldbuf) {
      oldptr = (*((void **)oldbuf - 1));
   }

   paddedSize = PAGE_SIZE + sizeof *buf + newsize;

   // Check for overflow.
   if (paddedSize < newsize) {
      return NULL;
   }

   buf = (void **)realloc(oldptr, paddedSize);
   if (!buf) {
      return NULL;
   }

   alignedResult = (void **)PAGE_ROUND_UP(buf + 1);
   *(alignedResult - 1) = buf;

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
   VERIFY(buf);
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
   VERIFY(buf);
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


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_UnsafeRealloc --
 *
 *      This function is not implemented because it cannot be done safely and
 *      portably.  See https://reviewboard.eng.vmware.com/r/284303/ for
 *      discussion.
 *
 *---------------------------------------------------------------------------
 */


/*
 *---------------------------------------------------------------------------
 *
 * Aligned_Realloc --
 *
 *      Realloc a chunk of memory aligned on a page boundary, potentially
 *      copying the previous data to a new buffer if necessary.  Resulting
 *      pointer needs to be freed with Aligned_Free.  You should never use this
 *      function.  Especially if size was derived from guest provided data.
 *
 * Result:
 *      A pointer.
 *
 * Side effects:
 *      Old buf may be freed.
 *
 *---------------------------------------------------------------------------
 */

static INLINE void*
Aligned_Realloc(void *buf,   // IN
                size_t size) // IN
{
#if defined MEMALIGNED_USE_INTERNAL_IMPL
   return AlignedReallocImpl(buf, size);
#elif defined _WIN32
   return _aligned_realloc(buf, size, PAGE_SIZE);
#else
   /*
    * Some valloc(3) manpages claim that realloc(3) on a buffer allocated by
    * valloc() will return an aligned buffer.  If so, we have a fast path;
    * simply realloc, validate the alignment, and return.  For realloc()s that
    * do not maintain the alignment (such as glibc 2.13 x64 for allocations of
    * 16 pages or less) then we fall back to a slowpath and copy the data.
    * Note that we can't avoid the realloc overhead in this case: on entry to
    * Aligned_Realloc we have no way to find out how big the source buffer is!
    * Only after the realloc do we know a safe range to copy.  We may copy more
    * data than necessary -- consider the case of resizing from one page to
    * 100 pages -- but that is safe, just slow.
    */
   buf = realloc(buf, size);
   if (((uintptr_t)buf % PAGE_SIZE) != 0) {
      void *newbuf;

      newbuf = Aligned_UnsafeMalloc(size);
      VERIFY(newbuf);
      memcpy(newbuf, buf, size);
      free(buf);
      return newbuf;
   }
   return buf;
#endif
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif
