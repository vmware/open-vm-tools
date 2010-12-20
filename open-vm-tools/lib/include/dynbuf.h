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
 * dynbuf.h --
 *
 *    Dynamic buffers
 */

#ifndef __DYNBUF_H__
#   define __DYNBUF_H__

#include <string.h>
#include "vm_basic_types.h"
#include "vm_assert.h"


typedef struct DynBuf {
   char *data;
   size_t size;
   size_t allocated;
} DynBuf;


void
DynBuf_Init(DynBuf *b); // IN

void
DynBuf_Destroy(DynBuf *b); // IN

void *
DynBuf_AllocGet(DynBuf const *b); // IN

void
DynBuf_Attach(DynBuf *b,    // IN
              size_t size,  // IN
              void *data);  // IN

void *
DynBuf_Detach(DynBuf *b); // IN

Bool
DynBuf_Enlarge(DynBuf *b,        // IN
               size_t min_size); // IN

Bool
DynBuf_Append(DynBuf *b,        // IN
              void const *data, // IN
              size_t size);     // IN

Bool
DynBuf_Trim(DynBuf *b); // IN

Bool
DynBuf_Copy(DynBuf *src,    // IN
            DynBuf *dest);  // OUT

void
DynBuf_SafeInternalAppend(DynBuf *b,            // IN
                          void const *data,     // IN
                          size_t size,          // IN
                          char const *file,     // IN
                          unsigned int lineno); // IN

#define DynBuf_SafeAppend(_buf, _data, _size) \
   DynBuf_SafeInternalAppend(_buf, _data, _size, __FILE__, __LINE__)


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

#if defined(SWIG)
static void *
#else
static INLINE void *
#endif
DynBuf_Get(DynBuf const *b) // IN
{
   ASSERT(b);

   return b->data;
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

#if defined(SWIG)
static size_t
#else
static INLINE size_t
#endif
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

#if defined(SWIG)
static void
#else
static INLINE void
#endif
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

#if defined(SWIG)
static size_t
#else
static INLINE size_t
#endif
DynBuf_GetAllocatedSize(DynBuf const *b) // IN
{
   ASSERT(b);

   return b->allocated;
}


/*
 *----------------------------------------------------------------------------
 *
 * DynBuf_AppendString --
 *
 *     Append the string to the specified DynBuf object.  Basically a
 *     fancy strcat().
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 *
 * Side effects:
 *     DynBuf may change its size or allocate additional memory.
 *
 *----------------------------------------------------------------------------
 */

#if defined(SWIG)
static Bool
#else
static INLINE Bool
#endif
DynBuf_AppendString(DynBuf *buf,         // IN
                    const char *string)  // IN
{
   /*
    * Make sure to copy the NULL.
    */
   return DynBuf_Append(buf, string, strlen(string) + 1);
}


#endif /* __DYNBUF_H__ */
