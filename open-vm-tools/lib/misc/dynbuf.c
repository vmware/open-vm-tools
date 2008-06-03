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
 * dynbuf.c --
 *
 *    Dynamic buffers --hpreg
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "dynbuf.h"


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Init --
 *
 *      Dynamic buffer constructor --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DynBuf_Init(DynBuf *b) // IN
{
   ASSERT(b);

   b->data = NULL;
   b->size = 0;
   b->allocated = 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Destroy --
 *
 *      Dynamic buffer destructor --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DynBuf_Destroy(DynBuf *b) // IN
{
   ASSERT(b);

   free(b->data);
   b->data = NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Get --
 *
 *      Retrieve a pointer to the data contained in a dynamic buffer --hpreg
 *
 * Results:
 *      The pointer to the data
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
DynBuf_Get(DynBuf const *b) // IN
{
   ASSERT(b);

   return b->data;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_AllocGet --
 *
 *      Retrieve a pointer to the data contained in a dynamic buffer.  Return
 *      a copy of that data.
 *
 * Results:
 *      The pointer to the data.  NULL on out of memory failure.
 *
 * Side effects:
 *      Allocates memory.
 *
 *-----------------------------------------------------------------------------
 */

void *
DynBuf_AllocGet(DynBuf const *b) // IN
{
   void *new_data;
   ASSERT(b);

   new_data = malloc(b->size);
   if (new_data) {
      memcpy(new_data, b->data, b->size);
   }

   return new_data;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Attach --
 *
 *      Grants ownership of the specified buffer to the DynBuf
 *      object. If there is an existing buffer, it is freed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DynBuf_Attach(DynBuf *b,    // IN
              size_t size,  // IN
              void *data)   // IN
{
   ASSERT(b);

   free(b->data);
   b->data = data;
   b->size = b->allocated = size;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Detach --
 *
 *      Releases ownership of the buffer stored in the DynBuf object,
 *      and returns a pointer to it.
 *
 * Results:
 *      The pointer to the data.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
DynBuf_Detach(DynBuf *b) // IN
{
   void *data;

   ASSERT(b);

   data = b->data;
   b->data = NULL;

   return data;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBufRealloc --
 *
 *      Reallocate a dynamic buffer --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
DynBufRealloc(DynBuf *b,            // IN
              size_t new_allocated) // IN
{
   void *new_data;

   ASSERT(b);

   new_data = realloc(b->data, new_allocated);
   if (new_data == NULL) {
      /* Not enough memory */
      return FALSE;
   }

   b->data = new_data;
   b->allocated = new_allocated;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Enlarge --
 *
 *      Enlarge a dynamic buffer. The resulting dynamic buffer is guaranteed to
 *      be larger than the one you passed, and at least 'size' bytes
 *      large --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynBuf_Enlarge(DynBuf *b,       // IN
               size_t min_size) // IN
{
   size_t new_allocated;

   ASSERT(b);

   new_allocated = b->allocated
                      ?
#if defined(DYNBUF_DEBUG)
                        b->allocated + 1
#else
                        /* 
                         * Double the previously allocated size if it is less
                         * than 256KB; otherwise grow it linearly by 256KB
                         */
                        (b->allocated < 256 * 1024 ? b->allocated * 2
                                                   : b->allocated + 256 * 1024)
#endif
                      :
#if defined(DYNBUF_DEBUG)
                        1
#else
                        /*
                         * Initial size: 1 KB. Most buffers are smaller than
                         * that --hpreg
                         */
                        1 << 10
#endif
                      ;

   if (min_size > new_allocated) {
      new_allocated = min_size;
   }

   return DynBufRealloc(b, new_allocated);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Append --
 *
 *      Append data at the end of a dynamic buffer. 'size' is the size of the
 *      data. If it is <= 0, no operation is performed --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynBuf_Append(DynBuf *b,        // IN
              void const *data, // IN
              size_t size)      // IN
{
   size_t new_size;
   
   ASSERT(b);

   if (size <= 0) {
      return TRUE;
   }

   ASSERT(data);

   new_size = b->size + size;
   if (new_size > b->allocated) {
      /* Not enough room */
      if (DynBuf_Enlarge(b, new_size) == FALSE) {
         return FALSE;
      }
   }

   memcpy(b->data + b->size, data, size);
   b->size = new_size;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Trim --
 *
 *      Reallocate a dynamic buffer to the exact size it occupies --hpreg
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynBuf_Trim(DynBuf *b) // IN
{
   ASSERT(b);

   return DynBufRealloc(b, b->size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_GetSize --
 *
 *      Returns the current size of the dynamic buffer --hpreg
 *
 * Results:
 *      The current size of the dynamic buffer
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

size_t
DynBuf_GetSize(DynBuf const *b) // IN
{
   ASSERT(b);

   return b->size;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_SetSize --
 *
 *      Set the current size of a dynamic buffer --hpreg
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
DynBuf_SetSize(DynBuf *b,   // IN
               size_t size) // IN
{
   ASSERT(b);
   ASSERT(size <= b->allocated);

   b->size = size;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_GetAllocatedSize --
 *
 *      Returns the current allocated size of the dynamic buffer --hpreg
 *
 * Results:
 *      The current allocated size of the dynamic buffer
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

size_t
DynBuf_GetAllocatedSize(DynBuf const *b) // IN
{
   ASSERT(b);

   return b->allocated;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynBuf_Copy --
 *
 *      Copies all data and metadata from src dynbuff to dest dynbuf.
 *
 *      Dest should be an initialized DynBuf of alloced length zero
 *      to prevent memory leaks.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynBuf_Copy(DynBuf *src,   // IN
            DynBuf *dest)  // OUT
{
   ASSERT(src);
   ASSERT(dest);
   ASSERT(!dest->data);

   dest->size      = src->size;
   dest->allocated = src->allocated;
   dest->data      = malloc(src->allocated);

   if (!dest->data) {
      return FALSE;
   }

   memcpy(dest->data, src->data, src->size);
   
   return TRUE;
}
