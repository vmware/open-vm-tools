/* **********************************************************
 * Copyright (c) 2015-2021 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
 * See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */


#include "vmware.h"
#include "codeset.h"

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const unsigned char utf8d[] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
  8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
  0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
  0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
  0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
  1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
  1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
  1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
  1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static INLINE uint32
CodeSetDecode(uint32 *state,  // IN:
              uint32 byte)    // IN:
{
  uint32 type = utf8d[byte];

  *state = utf8d[256 + *state*16 + type];

  return *state;
}


/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_IsStringValidUTF8 --
 *
 *      Check if the given buffer contains a valid UTF-8 string.
 *      This function will stop at first '\0' it sees.
 *
 * Results:
 *      TRUE if the given buffer contains a valid UTF-8 string, or FALSE.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
CodeSet_IsStringValidUTF8(const char *bufIn)  // IN:
{
   uint32 state = UTF8_ACCEPT;

   while (*bufIn != '\0') {
      CodeSetDecode(&state, (unsigned char) *bufIn++);
   }

   return state == UTF8_ACCEPT;
}


/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_IsValidUTF8 --
 *
 *      Check if the given buffer with given size, is UTF-8 encoded.
 *      This function will return TRUE even if there is '\0' in the buffer.
 *
 * Results:
 *      TRUE if the buffer is UTF-8 encoded, or FALSE.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
CodeSet_IsValidUTF8(const char *bufIn,  // IN:
                    size_t sizeIn)      // IN:
{
   size_t i;
   uint32 state = UTF8_ACCEPT;

   for (i = 0; i < sizeIn; i++) {
      CodeSetDecode(&state, (unsigned char) *bufIn++);
   }

   return state == UTF8_ACCEPT;
}


/*
 *----------------------------------------------------------------------------
 *
 * CodeSet_IsValidUTF8String --
 *
 *      Check if the given buffer with given size, is a valid UTF-8 string,
 *      and without '\0' in it.
 *
 * Results:
 *      TRUE if passed, or FALSE.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
CodeSet_IsValidUTF8String(const char *bufIn,  // IN:
                          size_t sizeIn)      // IN:
{
   size_t i;
   uint32 state = UTF8_ACCEPT;

   for (i = 0; i < sizeIn; i++) {
      unsigned char c = (unsigned char) *bufIn++;

      if (UNLIKELY(c == '\0')) {
         return FALSE;
      }

      CodeSetDecode(&state, c);
   }

   /* If everything went well we should have proper UTF8, the data
    * might instead have ended in the middle of a UTF8 codepoint.
    */
   return state == UTF8_ACCEPT;
}
