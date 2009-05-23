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
#ifdef _WIN32
   buf = _aligned_malloc(size, PAGE_SIZE);
#elif __linux__
#  if defined(N_PLAT_NLM)
   // Netware does not support valloc or memalign.  Fall back to malloc.
   buf = malloc(size);
#  else
   buf = memalign(PAGE_SIZE, size);
#  endif
#else // Apple, BSD, Solaris (tools)
   buf = valloc(size); 
#endif
#if !defined(N_PLAT_NLM)  // Netware won't be aligned necessarily.
   ASSERT(((uintptr_t)buf % PAGE_SIZE) == 0);
#endif
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
#ifdef _WIN32
   _aligned_free(buf);
#else
   free(buf);
#endif
}

#endif

