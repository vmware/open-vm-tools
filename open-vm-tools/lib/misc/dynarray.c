/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * dynarray.c --
 *
 *    Dynamic array of objects -- tonyc
 */

#include <stdlib.h>

#include "vmware.h"
#include "dynarray.h"


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Init --
 *
 *      Initialize the dynamic array
 *
 * Results:
 *      TRUE on success.  FALSE on failure.
 *
 * Side effects:
 *      See above
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynArray_Init(DynArray *a,          // IN/OUT
              unsigned int count,   // IN
              size_t width)         // IN
{
   ASSERT(a);

   DynBuf_Init(&a->buf);
   a->width = width;
   return DynArray_SetCount(a, count);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Destroy --
 *
 *      Destroy the array
 *
 * Results:
 *      None
 *
 * Side effects:
 *      See above
 *
 *-----------------------------------------------------------------------------
 */

void
DynArray_Destroy(DynArray *a)    // IN/OUT
{
   ASSERT(a);

   DynBuf_Destroy(&a->buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_AddressOf --
 *
 *      Fetch a pointer to the address of the ith element.
 *
 * Results:
 *      The pointer to the ith element or NULL if the index is out of
 *      bounds.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
DynArray_AddressOf(const DynArray *a,   // IN
                   unsigned int i)      // IN
{
   uint8 *result = NULL;

   ASSERT(a);

   if (i < DynArray_Count(a)) {
      result = ((uint8 *)DynBuf_Get(&a->buf)) + (i * a->width);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Count --
 *
 *      Returns the number of elements in the array.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned int
DynArray_Count(const DynArray *a)       // IN
{
   ASSERT(a);

   return (unsigned int) (DynBuf_GetSize(&a->buf) / a->width);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_SetCount --
 *
 *      Sets the number of elements in the array.   This may enlarge
 *      the size of the array.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      May resize the array
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynArray_SetCount(DynArray *a,          // IN/OUT
                  unsigned int c)       // IN
{
   size_t needed, allocated;

   ASSERT(a);

   needed = c * a->width;
   allocated = DynBuf_GetAllocatedSize(&a->buf);

   if (allocated < needed) {
      if (!DynBuf_Enlarge(&a->buf, needed)) {
         return FALSE;
      }
   }
   DynBuf_SetSize(&a->buf, needed);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_AllocCount --
 *
 *      Returns the actual size of the array.  If you want the effective
 *      size, use DynArray_Count.  Technically, you don't need this API
 *      unless you're doing trimming (i.e. Don't bother trimming if
 *      DynArray_AllocCount is within some threshold of DynArray_Count.
 *      It won't buy you much).
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

unsigned int
DynArray_AllocCount(const DynArray *a)  // IN
{
   ASSERT(a);

   return (unsigned int) (DynBuf_GetAllocatedSize(&a->buf) / a->width);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Trim --
 *
 *      Resize the array to fit exactly DynArray_Count() elements.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (why?  who knows...)
 *
 * Side effects:
 *      Resizes the array
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynArray_Trim(DynArray *a)    // IN/OUT
{
   ASSERT(a);

   return DynBuf_Trim(&a->buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_QSort --
 *
 *      A wrapper for the quicksort function.  Sorts the DynArray
 *      according to the provided comparison function.
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
DynArray_QSort(DynArray *a,             // IN/OUT
               DynArrayCmp compare)     // IN
{
   uint8 *arrayBuf;

   ASSERT(a);
   ASSERT(compare);

   arrayBuf = DynBuf_Get(&a->buf);
   qsort(arrayBuf, DynArray_Count(a), a->width, compare);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Copy --
 *
 *      Copies all data and metadata from src Dynarray to dest DynArray.
 *      
 *      Dest should be an initialized DynArray of size zero.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DynArray_Copy(DynArray *src,        // IN
              DynArray *dest)       // OUT
{
   ASSERT(src);
   ASSERT(dest);
   ASSERT(dest->width);
   ASSERT(dest->width == src->width);
   ASSERT(DynArray_AllocCount(dest) == 0);

   return DynBuf_Copy(&src->buf, &dest->buf);
}

