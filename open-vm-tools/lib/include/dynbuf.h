/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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

#ifndef DYNBUF_H
#   define DYNBUF_H

#include <string.h>
#include "vm_basic_types.h"
#include "vm_assert.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct DynBuf {
   char   *data;
   size_t  size;
   size_t  allocated;
} DynBuf;


void
DynBuf_Init(DynBuf *b); // OUT

void
DynBuf_InitWithMemory(DynBuf *b,
                      size_t dataSize,
                      void *data);

void
DynBuf_InitWithString(DynBuf *b,
                      char *str);

void
DynBuf_Destroy(DynBuf *b); // IN

void
DynBuf_Attach(DynBuf *b,    // IN
              size_t size,  // IN
              void *data);  // IN

void *
DynBuf_Detach(DynBuf *b); // IN/OUT

char *
DynBuf_DetachString(DynBuf *b); // IN/OUT

Bool
DynBuf_Enlarge(DynBuf *b,        // IN/OUT
               size_t min_size); // IN

Bool
DynBuf_Append(DynBuf *b,        // IN/OUT
              void const *data, // IN
              size_t size);     // IN

Bool
DynBuf_Trim(DynBuf *b); // IN/OUT

Bool
DynBuf_Copy(DynBuf *src,    // IN
            DynBuf *dest);  // OUT

void
DynBuf_SafeInternalAppend(DynBuf *b,            // IN/OUT
                          void const *data,     // IN
                          size_t size,          // IN
                          char const *file,     // IN
                          unsigned int lineno); // IN

#define DynBuf_SafeAppend(_buf, _data, _size) \
   DynBuf_SafeInternalAppend(_buf, _data, _size, __FILE__, __LINE__)

void
DynBuf_SafeInternalEnlarge(DynBuf *b,            // IN/OUT
                           size_t min_size,      // IN
                           char const *file,     // IN
                           unsigned int lineno); // IN

#define DynBuf_SafeEnlarge(_buf, _min_size) \
   DynBuf_SafeInternalEnlarge(_buf,  _min_size, __FILE__, __LINE__)


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
 * DynBuf_GetString --
 *
 * Results:
 *      Returns a pointer to the dynamic buffer data as a NUL-terminated
 *      string.
 *
 * Side effects:
 *      DynBuf might allocate additional memory and will panic if it fails to.
 *
 *-----------------------------------------------------------------------------
 */

#if defined(SWIG)
static char *
#else
static INLINE char *
#endif
DynBuf_GetString(DynBuf *b) // IN
{
   ASSERT(b);

   if (b->size == b->allocated) {
      ASSERT_MEM_ALLOC(DynBuf_Enlarge(b, b->size + 1));
   }
   b->data[b->size] = '\0';
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
DynBuf_SetSize(DynBuf *b,   // IN/OUT:
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
 *     Appends the string to the specified DynBuf object, including its NUL
 *     terminator.  Note that this is NOT like strcat; repeated calls will
 *     leave embedded NULs in the middle of the buffer. (Compare to
 *     DynBuf_Strcat.)
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      DynBuf may change its size or allocate additional memory.
 *
 *----------------------------------------------------------------------------
 */

#if defined(SWIG)
static Bool
#else
static INLINE Bool
#endif
DynBuf_AppendString(DynBuf *buf,         // IN/OUT
                    const char *string)  // IN
{
   return DynBuf_Append(buf, string, strlen(string) + 1 /* NUL */);
}


/*
 *----------------------------------------------------------------------------
 *
 * DynBuf_SafeAppendString --
 *
 *     "Safe" version of the above that does not fail.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      DynBuf may change its size or allocate additional memory.
 *
 *----------------------------------------------------------------------------
 */

#if defined(SWIG)
static void
#else
static INLINE void
#endif
DynBuf_SafeAppendString(DynBuf *buf,         // IN/OUT
                        const char *string)  // IN
{
   DynBuf_SafeAppend(buf, string, strlen(string) + 1 /* NUL */);
}


/*
 *----------------------------------------------------------------------------
 *
 * DynBuf_Strcat --
 *
 *      A DynBuf version of strcat.  Unlike DynBuf_AppendString, does NOT
 *      visibly NUL-terminate the DynBuf, thereby allowing future appends to
 *      do proper string concatenation without leaving embedded NULs in the
 *      middle.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure (not enough memory)
 *
 * Side effects:
 *      DynBuf may change its size or allocate additional memory.
 *
 *----------------------------------------------------------------------------
 */

#if defined(SWIG)
static Bool
#else
static INLINE Bool
#endif
DynBuf_Strcat(DynBuf *buf,         // IN/OUT
              const char *string)  // IN
{
   Bool success;

   ASSERT(buf != NULL);
   ASSERT(string != NULL);

   /*
    * We actually do NUL-terminate the buffer internally, but this is not
    * visible to callers, and they should not rely on this.
    */
   success = DynBuf_AppendString(buf, string);
   if (LIKELY(success)) {
      ASSERT(buf->size > 0);
      buf->size--;
   }
   return success;
}


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* DYNBUF_H */
