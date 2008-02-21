/* **********************************************************
 * Copyright 2007 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * unicodeSimpleUTF16.h --
 *
 *      UTF-16 handling macros. Based on utf16.h from ICU 1.8.1.
 *
 *      ICU 1.8.1 license follows:
 *
 *      ICU License - ICU 1.8.1 and later
 *
 *      COPYRIGHT AND PERMISSION NOTICE
 *
 *      Copyright (c) 1995-2006 International Business Machines Corporation
 *      and others
 *
 *      All rights reserved.
 *
 *           Permission is hereby granted, free of charge, to any
 *      person obtaining a copy of this software and associated
 *      documentation files (the "Software"), to deal in the Software
 *      without restriction, including without limitation the rights
 *      to use, copy, modify, merge, publish, distribute, and/or sell
 *      copies of the Software, and to permit persons to whom the
 *      Software is furnished to do so, provided that the above
 *      copyright notice(s) and this permission notice appear in all
 *      copies of the Software and that both the above copyright
 *      notice(s) and this permission notice appear in supporting
 *      documentation.
 *
 *           THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *      KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *      WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *      PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
 *      BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR
 *      CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 *      FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 *      CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *      OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *           Except as contained in this notice, the name of a
 *      copyright holder shall not be used in advertising or otherwise
 *      to promote the sale, use or other dealings in this Software
 *      without prior written authorization of the copyright holder.
 */

#ifndef _UNICODE_SIMPLE_UTF16_H
#define _UNICODE_SIMPLE_UTF16_H

#include "unicodeTypes.h"

/**
 * Is this code point a surrogate (U+d800..U+dfff)?
 * @param c 32-bit code point
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U_IS_SURROGATE(c) (((c)&0xfffff800)==0xd800)

/**
 * Does this code unit alone encode a code point (BMP, not a surrogate)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SINGLE(c) !U_IS_SURROGATE(c)

/**
 * Is this code unit a lead surrogate (U+d800..U+dbff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_LEAD(c) (((c)&0xfffffc00)==0xd800)

/**
 * Is this code unit a trail surrogate (U+dc00..U+dfff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_TRAIL(c) (((c)&0xfffffc00)==0xdc00)

/**
 * Is this code unit a surrogate (U+d800..U+dfff)?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SURROGATE(c) U_IS_SURROGATE(c)

/**
 * Assuming c is a surrogate code point (U16_IS_SURROGATE(c)),
 * is it a lead surrogate?
 * @param c 16-bit code unit
 * @return TRUE or FALSE
 * @stable ICU 2.4
 */
#define U16_IS_SURROGATE_LEAD(c) (((c)&0x400)==0)

/**
 * Helper constant for U16_GET_SUPPLEMENTARY.
 * @internal
 */
#define U16_SURROGATE_OFFSET ((0xd800<<10UL)+0xdc00-0x10000)

/**
 * Get a supplementary code point value (U+10000..U+10ffff)
 * from its lead and trail surrogates.
 * The result is undefined if the input values are not
 * lead and trail surrogates.
 *
 * @param lead lead surrogate (U+d800..U+dbff)
 * @param trail trail surrogate (U+dc00..U+dfff)
 * @return supplementary code point (U+10000..U+10ffff)
 * @stable ICU 2.4
 */
#define U16_GET_SUPPLEMENTARY(lead, trail) \
   (((uint32)(lead)<<10UL)+(uint32)(trail)-U16_SURROGATE_OFFSET)


/**
 * Get the lead surrogate (0xd800..0xdbff) for a
 * supplementary code point (0x10000..0x10ffff).
 * @param supplementary 32-bit code point (U+10000..U+10ffff)
 * @return lead surrogate (U+d800..U+dbff) for supplementary
 * @stable ICU 2.4
 */
#define U16_LEAD(supplementary) (utf16_t)(((supplementary)>>10)+0xd7c0)

/**
 * Get the trail surrogate (0xdc00..0xdfff) for a
 * supplementary code point (0x10000..0x10ffff).
 * @param supplementary 32-bit code point (U+10000..U+10ffff)
 * @return trail surrogate (U+dc00..U+dfff) for supplementary
 * @stable ICU 2.4
 */
#define U16_TRAIL(supplementary) (utf16_t)(((supplementary)&0x3ff)|0xdc00)

/**
 * How many 16-bit code units are used to encode this Unicode code point? (1 or 2)
 * The result is not defined if c is not a Unicode code point (U+0000..U+10ffff).
 * @param c 32-bit code point
 * @return 1 or 2
 * @stable ICU 2.4
 */
#define U16_LENGTH(c) ((uint32)(c)<=0xffff ? 1 : 2)

/**
 * The maximum number of 16-bit code units per Unicode code point (U+0000..U+10ffff).
 * @return 2
 * @stable ICU 2.4
 */
#define U16_MAX_LENGTH 2

/**
 * Get a code point from a string at a code point boundary offset,
 * and advance the offset to the next code point boundary.
 * (Post-incrementing forward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The offset may point to the lead surrogate unit
 * for a supplementary code point, in which case the macro will read
 * the following trail surrogate as well.
 * If the offset points to a trail surrogate or
 * to a single, unpaired lead surrogate, then that itself
 * will be returned as the code point.
 *
 * @param s const utf16_t * string
 * @param i string offset, must be i<length
 * @param length string length
 * @param c output uint32 variable
 * @see U16_NEXT_UNSAFE
 * @stable ICU 2.4
 */
#define U16_NEXT(s, i, length, c) { \
      (c)=(s)[(i)++]; \
      if(U16_IS_LEAD(c)) { \
         utf16_t __c2; \
         if((i)<(length) && U16_IS_TRAIL(__c2=(s)[(i)])) { \
            ++(i); \
            (c)=U16_GET_SUPPLEMENTARY((c), __c2); \
         } \
      } \
   }

/**
 * Move the string offset from one code point boundary to the previous one
 * and get the code point between them.
 * (Pre-decrementing backward iteration.)
 * "Safe" macro, handles unpaired surrogates and checks for string boundaries.
 *
 * The input offset may be the same as the string length.
 * If the offset is behind a trail surrogate unit
 * for a supplementary code point, then the macro will read
 * the preceding lead surrogate as well.
 * If the offset is behind a lead surrogate or behind a single, unpaired
 * trail surrogate, then that itself
 * will be returned as the code point.
 *
 * @param s const UChar * string
 * @param start starting string offset (usually 0)
 * @param i string offset, must be start<i
 * @param c output UChar32 variable
 * @see U16_PREV_UNSAFE
 * @stable ICU 2.4
 */
#define U16_PREV(s, start, i, c) { \
      (c)=(s)[--(i)]; \
      if(U16_IS_TRAIL(c)) { \
         utf16_t __c2; \
         if((i)>(start) && U16_IS_LEAD(__c2=(s)[(i)-1])) { \
            --(i); \
            (c)=U16_GET_SUPPLEMENTARY(__c2, (c)); \
         } \
      } \
   }

#endif // _UNICODE_SIMPLE_UTF16_H
