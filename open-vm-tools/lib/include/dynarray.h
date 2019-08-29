/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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
 * dynarray.h --
 *
 *    Dynamic array of objects.
 *
 *    Use a DynArray to hold a dynamically resizable array
 *    of objects with a fixed width.
 */

#ifndef _DYNARRAY_H_
#define _DYNARRAY_H_

#include "dynbuf.h"
#include "vm_basic_types.h"
#include "vm_assert.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct DynArray {
   DynBuf buf;
   size_t width;
} DynArray;

/*
 * The SVGA drivers require the __cdecl calling convention.
 * The qsort comparison function is compiled with the __stdecl
 * convention by default, so if we are compiling SVGA (which defines
 * STD_CALL) we need to explicitly declare the function with __cdecl.
 */
#if defined(STD_CALL)
#define CDECLCONV __cdecl
#else
#define CDECLCONV
#endif

typedef int (CDECLCONV *DynArrayCmp)(const void *, const void *);

Bool
DynArray_Init(DynArray *a, unsigned int count, size_t width);

void
DynArray_Destroy(DynArray *a);

Bool
DynArray_SetCount(DynArray *a, unsigned int c);

void
DynArray_QSort(DynArray *a, DynArrayCmp compare);

unsigned int
DynArray_AllocCount(const DynArray *a);


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

static INLINE Bool
DynArray_Trim(DynArray *a)    // IN/OUT
{
   ASSERT(a);

   return DynBuf_Trim(&a->buf);
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

static INLINE void *
DynArray_AddressOf(const DynArray *a,   // IN
                   unsigned int i)      // IN
{
   uintptr_t offset = i * a->width;

   ASSERT(a);

   if (offset + a->width <= DynBuf_GetSize(&a->buf)) {
      return offset + (uint8 *)DynBuf_Get(&a->buf);
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_AddressOfUnsafe --
 *
 *      Fetch a pointer to the address of the ith element. Only call
 *      this if you already know that 'i' is valid. The index is not
 *      checked against the size of the array.
 *
 * Results:
 *      Always returns a pointer to the ith element.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void *
DynArray_AddressOfUnsafe(const DynArray *a,   // IN
                         unsigned int i)      // IN
{
   uintptr_t offset = i * a->width;

   ASSERT(a);
   ASSERT(offset + a->width <= DynBuf_GetSize(&a->buf));

   return offset + (uint8 *)DynBuf_Get(&a->buf);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DynArray_Count --
 *
 *      Returns the number of elements in the array.
 *
 *      XXX: This is relatively slow, since we do an integer division.
 *           Avoid calling this in inner loops.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE unsigned int
DynArray_Count(const DynArray *a)       // IN
{
   ASSERT(a);

   return (unsigned int) (DynBuf_GetSize(&a->buf) / a->width);
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

static INLINE Bool
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


/*
 * Use the following macros to define your own DynArray type to
 * make its usage less cumbersome.  You also get type-checking
 * for free, as demonstrated by this example:
 *
 * Assume:
 *
 *    typedef struct { int n, d; } Fraction;
 *    typedef struct { float r, i; } Complex;
 *
 * Without DEFINE_DYNARRAY_TYPE:
 *
 *    DynArray a1, a2;
 *    DynArray_Init(&a1, 4, sizeof(Fraction));
 *    DynArray_Init(&a2, 16, sizeof(Complex));
 *
 *    Fraction *f2 = (Fraction *)DynArray_AddressOf(&a2, 3); // Runtime Error
 *
 *
 * With DEFINE_DYNARRAY_TYPE:
 *
 *    DEFINE_DYNARRAY_TYPE(Fraction)
 *    DEFINE_DYNARRAY_TYPE(Complex)
 *    FractionArray a1;
 *    ComplexArray a2;
 *    FractionArray_Init(&a1, 4);
 *    ComplexArray_Init(&a2, 16);
 *
 *    Fraction *f2 = FractionArray_AddressOf(&a2, 3); // Compile Error
 *
 * Yes, it's a poor man's template (but better than nothing).
 *
 */

#define DEFINE_DYNARRAY_TYPE(T)     DEFINE_DYNARRAY_NAMED_TYPE(T, T)

#define DEFINE_DYNARRAY_NAMED_TYPE(T, TYPE)                             \
   DECLARE_DYNARRAY_TYPE(T)                                             \
   DEFINEONLY_DYNARRAY_NAMED_TYPE(T, TYPE)

#define DECLARE_DYNARRAY_TYPE(T)                                        \
   typedef DynArray T##Array;

#define DEFINEONLY_DYNARRAY_NAMED_TYPE(T, TYPE)                         \
                                                                        \
   typedef int (CDECLCONV *DynArray##T##Cmp)(const TYPE *,              \
                                             const TYPE *);             \
                                                                        \
   static INLINE Bool                                                   \
   T##Array_Init(T##Array *a, unsigned int count)                       \
   {                                                                    \
      return DynArray_Init((DynArray *)a, count, sizeof(TYPE));         \
   }                                                                    \
                                                                        \
   static INLINE void                                                   \
   T##Array_Destroy(T##Array *a)                                        \
   {                                                                    \
      DynArray_Destroy((DynArray *)a);                                  \
   }                                                                    \
                                                                        \
   static INLINE TYPE*                                                  \
   T##Array_AddressOf(T##Array *a, unsigned int i)                      \
   {                                                                    \
      return (TYPE*)DynArray_AddressOf((DynArray *)a, i);               \
   }                                                                    \
                                                                        \
   static INLINE TYPE*                                                  \
   T##Array_AddressOfUnsafe(T##Array *a, unsigned int i)                \
   {                                                                    \
      return (TYPE*)DynArray_AddressOfUnsafe((DynArray *)a, i);         \
   }                                                                    \
                                                                        \
   static INLINE unsigned int                                           \
   T##Array_Count(const T##Array *a)                                    \
   {                                                                    \
      return DynArray_Count((const DynArray *)a);                       \
   }                                                                    \
                                                                        \
   static INLINE Bool                                                   \
   T##Array_SetCount(T##Array *a, unsigned int c)                       \
   {                                                                    \
      return DynArray_SetCount((DynArray *)a, c);                       \
   }                                                                    \
                                                                        \
   static INLINE Bool                                                   \
   T##Array_Push(T##Array *a, TYPE val)                                 \
   {                                                                    \
      unsigned int count = T##Array_Count(a);                           \
      if (!T##Array_SetCount(a, count + 1)) {                           \
         return FALSE;                                                  \
      }                                                                 \
      *T##Array_AddressOf(a, count) = val;                              \
      return TRUE;                                                      \
   }                                                                    \
                                                                        \
   static INLINE Bool                                                   \
   T##Array_PushFront(T##Array *a, TYPE val)                            \
   {                                                                    \
      unsigned int count = T##Array_Count(a);                           \
      if (!T##Array_SetCount(a, count + 1)) {                           \
         return FALSE;                                                  \
      } else {                                                          \
         unsigned int i;                                                \
         for (i = count; i > 0; --i) {                                  \
            *T##Array_AddressOf(a, i) = *T##Array_AddressOf(a, i-1);    \
         }                                                              \
         *T##Array_AddressOf(a, 0) = val;                               \
         return TRUE;                                                   \
      }                                                                 \
   }                                                                    \
                                                                        \
   static INLINE unsigned int                                           \
   T##Array_AllocCount(T##Array *a)                                     \
   {                                                                    \
      return DynArray_AllocCount((DynArray *)a);                        \
   }                                                                    \
                                                                        \
   static INLINE Bool                                                   \
   T##Array_Trim(T##Array *a)                                           \
   {                                                                    \
      return DynArray_Trim((DynArray *)a);                              \
   }                                                                    \
   static INLINE void                                                   \
   T##Array_QSort(T##Array *a, DynArray##T##Cmp compare)                \
   {                                                                    \
      DynArray_QSort((DynArray *)a, (DynArrayCmp)compare);              \
   }                                                                    \
   static INLINE Bool                                                   \
   T##Array_Copy(T##Array *src, T##Array *dest)                         \
   {                                                                    \
      return DynArray_Copy((DynArray *)src, (DynArray *)dest);          \
   }
/* Define DynArray of DynBuf. */
DEFINE_DYNARRAY_TYPE(DynBuf)

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* _DYNARRAY_H_ */
