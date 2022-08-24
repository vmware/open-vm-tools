/* **********************************************************
 * Copyright (c) 2007-2022 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * codeset.h --
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
#ifndef __CODESET_H__
#   define __CODESET_H__

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "dynbuf.h"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * These platforms use UTF-8 (or pretend to):
 *   FreeBSD: really UTF-8
 *   ESX: UTF-8 by policy decree
 *   Mac: really UTF-8
 */

#if defined(__FreeBSD__) || \
    defined(VMX86_SERVER) || \
    defined(__APPLE__) || \
    defined __ANDROID__
#define CURRENT_IS_UTF8
#endif

/*
 * Guard these defines, borrowed from ICU's utf16.h, so that source files
 * can include both.
 */

#ifndef __UTF16_H__

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
#define U16_IS_SINGLE(c) (!U_IS_SURROGATE(c))

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
#define U16_LEAD(supplementary) ((utf16_t)(((supplementary)>>10)+0xd7c0))

/**
 * Get the trail surrogate (0xdc00..0xdfff) for a
 * supplementary code point (0x10000..0x10ffff).
 * @param supplementary 32-bit code point (U+10000..U+10ffff)
 * @return trail surrogate (U+dc00..U+dfff) for supplementary
 * @stable ICU 2.4
 */
#define U16_TRAIL(supplementary) ((utf16_t)(((supplementary)&0x3ff)|0xdc00))

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

#endif // __UTF16_H__


/*
 * Use this instead of "UTF-16" to specify UTF-16 in native byte order.
 */

#define CODESET_NATIVE_UTF16 "UTF-16LE"


/*
 * Flags for conversion functions
 */

#define CSGTG_NORMAL    0x0000  /* Without any information loss. */
#define CSGTG_TRANSLIT  0x0001  /* Transliterate unknown characters. */
#define CSGTG_IGNORE    0x0002  /* Skip over untranslatable characters. */

/*
 * XXX -- this function is a temporary fix. It should be removed once we fix
 *        the 3rd party library pathname issue.
 */
char *
CodeSet_GetAltPathName(const utf16_t *pathW); // IN


Bool
CodeSet_Init(const char *icuDataDir); // IN: ICU datafile directory in current page,
                                      // can be NULL.

void
CodeSet_DontUseIcu(void);

Bool
CodeSet_GenericToGenericDb(const char *codeIn,  // IN
                           const char *bufIn,   // IN
                           size_t sizeIn,       // IN
                           const char *codeOut, // IN
                           unsigned int flags,  // IN
                           DynBuf *db);         // IN/OUT

Bool
CodeSet_GenericToGeneric(const char *codeIn,  // IN
                         const char *bufIn,   // IN
                         size_t sizeIn,       // IN
                         const char *codeOut, // IN
                         unsigned int flags,  // IN
                         char **bufOut,       // IN/OUT
                         size_t *sizeOut);    // IN/OUT

Bool
CodeSet_Utf8ToCurrent(const char *bufIn,   // IN
                      size_t sizeIn,       // IN
                      char **bufOut,       // OUT
                      size_t *sizeOut);    // OUT

Bool
CodeSet_CurrentToUtf8(const char *bufIn,  // IN
                      size_t sizeIn,      // IN
                      char **bufOut,      // OUT
                      size_t *sizeOut);   // OUT

Bool
CodeSet_Utf16leToUtf8Db(const char *bufIn,   // IN
                        size_t sizeIn,       // IN
                        DynBuf *db);         // IN

Bool
CodeSet_Utf16leToUtf8(const char *bufIn,   // IN
                      size_t sizeIn,       // IN
                      char **bufOut,       // OUT
                      size_t *sizeOut);    // OUT/OPT

Bool
CodeSet_Utf8ToUtf16le(const char *bufIn,   // IN
                      size_t sizeIn,       // IN
                      char **bufOut,       // OUT
                      size_t *sizeOut);    // OUT/OPT

Bool
CodeSet_CurrentToUtf16le(const char *bufIn,   // IN
                         size_t sizeIn,       // IN
                         char **bufOut,       // OUT
                         size_t *sizeOut);    // OUT/OPT

Bool
CodeSet_Utf16leToCurrent(const char *bufIn,   // IN
                         size_t sizeIn,       // IN
                         char **bufOut,       // OUT
                         size_t *sizeOut);    // OUT/OPT

Bool
CodeSet_Utf16beToCurrent(const char *bufIn,   // IN
                         size_t sizeIn,       // IN
                         char **bufOut,       // OUT
                         size_t *sizeOut);    // OUT/OPT

Bool
CodeSetOld_Utf8Normalize(const char *bufIn,     // IN
                         size_t sizeIn,         // IN
                         Bool precomposed,      // IN
                         DynBuf *db);           // OUT

Bool
CodeSet_Utf8FormDToUtf8FormC(const char *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut);      // OUT/OPT

Bool
CodeSet_Utf8FormCToUtf8FormD(const char *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut);      // OUT/OPT

const char *
CodeSet_GetCurrentCodeSet(void);

Bool
CodeSet_IsEncodingSupported(const char *name);  // IN:

Bool
CodeSet_Validate(const char *buf,   // IN: the string
                 size_t size,	    // IN: length of string
                 const char *code); // IN: encoding

Bool CodeSet_UTF8ToUTF32(const char *utf8,  // IN:
                         char **utf32);     // OUT:

Bool CodeSet_UTF32ToUTF8(const char *utf32,  // IN:
                         char **utf8);       // OUT:

int CodeSet_LengthInCodePoints(const char *utf8);  // IN:

int CodeSet_CodePointOffsetToByteOffset(const char *utf8,      // IN:
                                        int codePointOffset);  // IN:

int CodeSet_GetUtf8(const char *string,  // IN:
                    const char *end,     // IN:
                    uint32 *uchar);      // OUT/OPT:

Bool CodeSet_IsValidUTF8(const char *bufIn,  // IN:
                         size_t sizeIn);     // IN:

Bool CodeSet_IsStringValidUTF8(const char *string);  // IN:

Bool CodeSet_IsValidUTF8String(const char *bufIn,  // IN:
                               size_t sizeIn);     // IN:

char *CodeSet_JsonEscape(const char *utf8);  // IN:

char *CodeSet_JsonUnescape(const char *utf8);  // IN:

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToUtf16 --
 *
 *      A convenience wrapper that accepts a NUL-terminated UTF-8 string
 *      and returns an allocated UTF-16 (LE) string. ASSERTs on failure.
 *
 * Results:
 *      The allocted UTF-16 (LE) string, free with free().
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE utf16_t *
CodeSet_Utf8ToUtf16(const char *str)  // IN:
{
   utf16_t *strW;

   if (!CodeSet_Utf8ToUtf16le(str, strlen(str), (char **) &strW, NULL)) {
      NOT_IMPLEMENTED();
   }

   return strW;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16ToUtf8 --
 *
 *      A convenience wrapper that accepts a NUL-terminated UTF-16 (LE)
 *      string and returns an allocated UTF-8 string. ASSERTs on failure.
 *
 * Results:
 *      The allocted UTF-8 string, free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE char *
CodeSet_Utf16ToUtf8(const utf16_t *strW)  // IN:
{
   char *str;
   size_t len;

   for (len = 0; strW[len]; len++)
      ;

   if (!CodeSet_Utf16leToUtf8((const char *) strW, len * sizeof strW[0],
                              (char **) &str, NULL)) {
      NOT_IMPLEMENTED();
   }

   return str;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8FindCodePointBoundary
 *
 *      Determine if buf[offset] is a valid UTF-8 code point boundary
 *      and find the previous boundary if it is not. The contents of
 *      buf[offset] need not be defined, only data prior to this
 *      location is examined. Useful for finding a suitable place to
 *      put a NUL terminator.
 *
 * Results:
 *
 *      Returns the offset of the byte immediately following the last
 *      complete UTF-8 code point in buf that is entirely within the
 *      range [0, offset-1]. Note that if the final UTF-8 code point
 *      is complete, the input offset will be returned unchanged.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE size_t
CodeSet_Utf8FindCodePointBoundary(const char *buf,   // IN
                                  size_t offset)     // IN
{
   size_t origOffset = offset;

   if (offset > 0) {

      signed char c;
      /*
       * Back up 1 byte and then find the start of the UTF-8 code
       * point occupying that location.
       */

      offset--;
      while (offset > 0 && (buf[offset] & 0xc0) == 0x80) {
         offset--;
      }

      /*
       * Maximum UTF-8 code point length is 4
       */

      ASSERT(origOffset - offset <= 4);

      c = buf[offset];

      /*
       * The first byte of a UTF-8 code point needs to be one of
       * 0b0XXXXXXX, 0b110XXXXX, 0b1110XXXX, 0b11110XXX
       */

      ASSERT(c >= 0 || (c >> 5) == -2 || (c >> 4) == -2 || (c >> 3) == -2);

      /*
       * offset now points to the start of a UTF-8 code point. If it
       * is a single byte or if the length, as encoded in the first
       * byte, matches the number of bytes we have backed up, then the
       * entire code point is present, so the original offset is a
       * valid code point starting offset.
       *
       * Length is encoded as
       * 2 bytes: 0b110XXXXX
       * 3 bytes: 0b1110XXXX
       * 4 bytes: 0b11110XXX
       * Thus the first byte is -2 when shifted right (signed) by
       * (7 - length).
       */

      if (c >= 0 || (c >> (7 - origOffset + offset)) == -2) {
         return origOffset;
      }

      /*
       * Else we truncated a code point. Return its starting point.
       */
   }
   return offset;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16FindCodePointBoundary
 *
 *      Determine if buf[offset] is a valid UTF-16 code point boundary
 *      and find the previous boundary if it is not. The contents of
 *      buf[offset] need not be defined, only data prior to this
 *      location is examined. Useful for finding a suitable place to
 *      put a NUL terminator.
 *
 * Results:
 *
 *      Returns the offset of the byte immediately following the last
 *      complete UTF-16 code point in buf that is entirely within the
 *      range [0, offset-1]. Note that if the final UTF-16 code point
 *      is complete, the input offset will be returned unchanged.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE size_t
CodeSet_Utf16FindCodePointBoundary(const char *buf, // IN
                                   size_t offset)   // IN
{
   size_t origOffset;
   const utf16_t *utf16Buf = (const utf16_t *)buf;

   origOffset = offset / 2;
   offset = origOffset - 1;

   if (origOffset > 0 && U16_IS_LEAD(utf16Buf[offset])) {
      return offset * 2;
   }

   return origOffset * 2;
}

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif /* __CODESET_H__ */
