/*
 * Copyright (c) 1996, 1998 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1995 by International Business Machines, Inc.
 *
 * International Business Machines, Inc. (hereinafter called IBM) grants
 * permission under its copyrights to use, copy, modify, and distribute this
 * Software with or without fee, provided that the above copyright notice and
 * all paragraphs of this notice appear in all copies, and that the name of IBM
 * not be used in connection with the marketing of any product incorporating
 * the Software or modifications thereof, without specific, written prior
 * permission.
 *
 * To the extent it has a right to do so, IBM grants an immunity from suit
 * under its patents, if any, for the use, sale or manufacture of products to
 * the extent that such products are used for performing Domain Name System
 * dynamic updates in TCP/IP networks by means of the Software.  No immunity is
 * granted for any product per se or for any other function of any product.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", AND IBM DISCLAIMS ALL WARRANTIES,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE.  IN NO EVENT SHALL IBM BE LIABLE FOR ANY SPECIAL,
 * DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE, EVEN
 * IF IBM IS APPRISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/types.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vm_basic_types.h"
#include "vm_assert.h"
#include "base64.h"

static const char Base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char Pad64 = '=';

/* (From RFC1521 and draft-ietf-dnssec-secext-03.txt)
   The following encoding technique is taken from RFC 1521 by Borenstein
   and Freed.  It is reproduced here in a slightly edited form for
   convenience.

   A 65-character subset of US-ASCII is used, enabling 6 bits to be
   represented per printable character. (The extra 65th character, "=",
   is used to signify a special processing function.)

   The encoding process represents 24-bit groups of input bits as output
   strings of 4 encoded characters. Proceeding from left to right, a
   24-bit input group is formed by concatenating 3 8-bit input groups.
   These 24 bits are then treated as 4 concatenated 6-bit groups, each
   of which is translated into a single digit in the base64 alphabet.

   Each 6-bit group is used as an index into an array of 64 printable
   characters. The character referenced by the index is placed in the
   output string.

   Table 1: The Base64 Alphabet

   Value Encoding  Value Encoding  Value Encoding  Value Encoding
   0 A            17 R            34 i            51 z
   1 B            18 S            35 j            52 0
   2 C            19 T            36 k            53 1
   3 D            20 U            37 l            54 2
   4 E            21 V            38 m            55 3
   5 F            22 W            39 n            56 4
   6 G            23 X            40 o            57 5
   7 H            24 Y            41 p            58 6
   8 I            25 Z            42 q            59 7
   9 J            26 a            43 r            60 8
   10 K            27 b            44 s            61 9
   11 L            28 c            45 t            62 +
   12 M            29 d            46 u            63 /
   13 N            30 e            47 v
   14 O            31 f            48 w         (pad) =
   15 P            32 g            49 x
   16 Q            33 h            50 y

   Special processing is performed if fewer than 24 bits are available
   at the end of the data being encoded.  A full encoding quantum is
   always completed at the end of a quantity.  When fewer than 24 input
   bits are available in an input group, zero bits are added (on the
   right) to form an integral number of 6-bit groups.  Padding at the
   end of the data is performed using the '=' character.

   Since all base64 input is an integral number of octets, only the
   -------------------------------------------------
   following cases can arise:

   (1) the final quantum of encoding input is an integral
   multiple of 24 bits; here, the final unit of encoded
   output will be an integral multiple of 4 characters
   with no "=" padding,
   (2) the final quantum of encoding input is exactly 8 bits;
   here, the final unit of encoded output will be two
   characters followed by two "=" padding characters, or
   (3) the final quantum of encoding input is exactly 16 bits;
   here, the final unit of encoded output will be three
   characters followed by one "=" padding character.
*/

/*
 *----------------------------------------------------------------------------
 *
 * Base64_Encode --
 *
 *      Base64-encodes srcLength bytes from src and stores result in dst.
 *
 * Results:
 *      TRUE if the destination held enough space for the decoded result,
 *      FALSE otherwise.
 *
 * Side effects:
 *      Updates dstSize with the number of encoded bytes (excluding the
 *      terminating '\0').
 *
 *----------------------------------------------------------------------------
 */

Bool
Base64_Encode(uint8 const *src,        // IN
              size_t       srcSize,    // IN
              char        *dst,        // OUT
              size_t       dstMax,     // IN: max result length, including NUL byte
              size_t      *dstSize)    // OUT: result length, may be NULL
{
   char *dst0 = dst;
   ASSERT(src || srcSize == 0);
   ASSERT(dst);

   if (4 * ((srcSize+2) / 3) >= dstMax) {
      if (dstSize) {
         *dstSize = 0;
      }
      return FALSE;
   }

   while (LIKELY(srcSize > 2)) {
      dst[0] = Base64[src[0] >> 2];
      dst[1] = Base64[(src[0] & 0x03) << 4 | src[1] >> 4];
      dst[2] = Base64[(src[1] & 0x0f) << 2 | src[2] >> 6];
      dst[3] = Base64[src[2] & 0x3f];

      srcSize -= 3;
      src += 3;
      dst += 4;
   }

   /* Now we worry about padding. */
   if (LIKELY(srcSize--)) {
      uint8 src1 = srcSize ? src[1] : 0;

      dst[0] = Base64[src[0] >> 2];
      dst[1] = Base64[(src[0] & 0x03) << 4 | src1 >> 4];
      dst[2] = srcSize ? Base64[(src1 & 0x0f) << 2] : Pad64;
      dst[3] = Pad64;
      dst += 4;
   }

   dst[0] = '\0';	/* Returned value doesn't count \0. */
   if (dstSize) {
      *dstSize = dst - dst0;
   }
   return TRUE;
}

#ifdef __I_WANT_TO_TEST_THIS__
main()
{
   struct {
      char *in, *out;
   } tests[] = {
      {"", ""},
      {"MQ==", "1"},
      {"MTI=", "12"},
      {"MTIz", "123"},
      {"MTIzNA==", "1234"},
      {"SGVsbG8gRWR3YXJkIGFuZCBKb2huIQ==","Hello Edward and John!"},
      {NULL, NULL}
   }, *test;

   size_t bufMax;
   if (1) {
      for (bufMax = 0; bufMax < 7; ++bufMax) {
         char buf[999];
         size_t bufSize;

         if (bufMax == 6) {
            bufMax = sizeof buf;
         }

         printf("\nBuffer size %d:\n", bufMax);

         test = tests;
         for (; test->in; ++test) {
            Bool r;

            r = Base64_Decode(test->in, buf, bufMax, &bufSize);

            if (bufSize != strlen(test->out) ||
                strncmp(test->out, buf, bufSize) != 0) {
               printf("Decoding of %s failed.  Got %s (%d), not %s\n",
                      test->in, buf, bufSize, test->out);
            } else {
               printf("Good: %s -> %s (%d)\n", test->in, buf, bufSize);
            }

            r = Base64_Encode(test->out, strlen(test->out),
                              buf, bufMax, &bufSize);
            buf[bufMax] = 0;

            if (!r || bufSize != strlen(test->in) ||
                strncmp(test->in, buf, bufSize) != 0) {
               printf("Encoding of %s failed.  Got %s (%d), not %s\n",
                      test->out, buf, bufSize, test->in);
            } else {
               printf("Good: %s -> %s (%d)\n", test->out, buf, bufSize);
            }
         }
      }
   }

   for (bufMax = 0; bufMax < 100000; ++bufMax) {
      char random_in[8000];
      char random_out[16000];
      size_t bufSize;

      Bool r = Base64_Encode(random_in, sizeof random_in,
                             random_out, sizeof random_out, &bufSize);

      if (!r) {
            printf("Encoding failed.\n");
      }
   }
}
#endif

/*
 *----------------------------------------------------------------------------
 *
 * Base64_Decode --
 *
 *       Skips all whitespace anywhere. Converts characters, four at
 *       a time, starting at (or after) src from base - 64 numbers into three
 *       8 bit bytes in the target area. Returns the number of data bytes
 *       stored at the target in the provided out parameter.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Base64_Decode(char const *in,         // IN
              uint8      *out,        // OUT
              size_t      outSize,    // IN
              size_t     *dataLength) // OUT
{
   static char base64Reverse[256];
   static Bool base64ReverseInit = 0;

   uint32 b = 0;
   int n = 0;
   uintptr_t i = 0;

   // Special markers
   enum {
      ILLEGAL = -1, EOM = -2, WS = -3
   };

   ASSERT(in);
   ASSERT(out || outSize == 0);
   ASSERT(dataLength);

   *dataLength = 0;

   if (!base64ReverseInit) {
      // Default is illegal chars (or whitespace)
      for (i = 0; i < 256; ++i) {
         base64Reverse[i] = isspace(i) ? WS : ILLEGAL;
      }
      base64Reverse['\0']  = EOM;
      base64Reverse['=']   = EOM;

      for (i = 0; Base64[i]; ++i) {
         base64Reverse[(unsigned)Base64[i]] = (char) i;
      }
      base64ReverseInit = 1;
   }

   i = 0;
   for (;;) {
      int p = base64Reverse[*(unsigned char *)in++];

      if (UNLIKELY(p < 0)) {
         switch (p) {
         case ILLEGAL: return FALSE;
         case WS: continue;
         case EOM: *dataLength = i; return TRUE;
         }
      } else {
         if (UNLIKELY(i >= outSize)) {
            return FALSE;
         }
         b = (b << 6) | p;
         n += 6;
         if (LIKELY(n >= 8)) {
            n -= 8;
            out[i++] = b >> n;
         }
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * Base64_ValidEncoding --
 *
 *      Returns TRUE if the specified input buffer is valid Base64 input.
 *
 * Results:
 *      TRUE or FALSE.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Base64_ValidEncoding(char const *src, // IN
                     size_t srcLength) // IN
{
   size_t i;
   ASSERT(src);
   for (i = 0; i < srcLength; i++) {
      uint8 c = src[i]; /* MSVC CRT will die on negative arguments to is* */

      if (!isalpha(c) && !isdigit(c) &&
          c != '+' && c != '=' && c != '/') {
         return FALSE;
      }
   }
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * Base64_EncodedLength --
 *
 *      Given a binary buffer, how many bytes would it take to encode it.
 *
 * Results:
 *      Number of bytes needed to encode, including terminating NUL byte.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

size_t
Base64_EncodedLength(uint8 const *src, size_t srcLength) {
   return ((srcLength + 2) / 3 * 4) + 1;
}


/*
 *----------------------------------------------------------------------------
 *
 * Base64_DecodedLength --
 *
 *      Given a base64 encoded string, how many bytes do we need to decode it.
 *      Assumes no whitespace.  This is not necessarily the length of the
 *      decoded data (Base64_Decode requires a few extra bytes... don't blame
 *      me, I didn't write it).
 *
 * Results:
 *      Number of bytes needed to decode input.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

size_t
Base64_DecodedLength(char const *src, size_t srcLength) {
   size_t length;

   ASSERT(src);

   length = srcLength / 4 * 3;
   if (src[srcLength-1] == '=') {
      length--;
   }
   if (src[srcLength-2] == '=') {
      length--;
   }
   return length;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Base64_EasyEncode --
 *
 *      Base64-encode 'data' into a NUL-terminated string.
 *
 * Results:
 *      On success: TRUE. '*target' is set to an allocated string, that the
 *                  caller must eventually free().
 *      On failure: FALSE. '*target' is set to NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Base64_EasyEncode(const uint8 *src,  // IN: data to encode
                  size_t srcLength,  // IN: data size
                  char **target)     // OUT: encoded string
{
   Bool succeeded = FALSE;
   size_t size;

   ASSERT(src);
   ASSERT(target);

   size = Base64_EncodedLength(src, srcLength);

   *target = (char *) malloc(size);

   if (!*target) {
      goto exit;
   }

   if (!Base64_Encode(src, srcLength, *target, size, NULL)) {
      goto exit;
   }

   succeeded = TRUE;

exit:
   if (!succeeded) {
      free(*target);
      *target = NULL;
   }

   return succeeded;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Base64_EasyDecode --
 *
 *      Base64-decode 'src' into a buffer.
 *
 * Results:
 *      TRUE on success, FALSE otherwise, plus the decoded data on success.
 *      Caller must free 'target' with free().
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Base64_EasyDecode(const char *src,   // IN: data to decode
                  uint8 **target,    // OUT: decoded data
                  size_t *targSize)  // OUT: data size
{
   Bool succeeded = FALSE;
   size_t theDataSize;
   uint8 *theData;

   ASSERT(src);
   ASSERT(target);
   ASSERT(targSize);

   theDataSize = Base64_DecodedLength(src, strlen(src));

   theData = (uint8 *) malloc(theDataSize);

   if (!theData) {
      goto exit;
   }

   if (!Base64_Decode(src, theData, theDataSize, &theDataSize)) {
      free(theData);
      goto exit;
   }

   *target = theData;
   *targSize = theDataSize;

   succeeded = TRUE;

exit:
   if (!succeeded) {
      *target = NULL;
      *targSize = 0;
   }

   return succeeded;
}
