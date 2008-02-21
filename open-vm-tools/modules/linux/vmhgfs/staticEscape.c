/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * staticEscape.c --
 *
 *    Buffer escaping, stolen from hpreg's buffer escaping 
 *    in lib/string, but modified to use bit vectors instead
 *    of arrays, and static buffers instead of dynbufs. [bac]
 *
 */

#if defined(sun)
#   include <string.h>
#elif defined(__FreeBSD__)
#   if defined(_KERNEL)
#      include <sys/libkern.h>
#      define memmove(dst0, src0, len)  bcopy(src0, dst0, len)
#   else
#      include <string.h>
#   endif
#endif

#include "staticEscape.h"
#include "vm_assert.h"


/*
 * Table to use to quickly convert an ASCII hexadecimal digit character into a
 * decimal number. If the input is not an hexadecimal digit character, the
 * output is -1 --hpreg
 */
static int const Hex2Dec[] = {
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9, -1, -1, -1, -1, -1, -1,
   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, 10, 11, 12, 13, 14, 15, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
   -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};


/*
 * Table to use to quickly convert a decimal number into an ASCII hexadecimal
 * digit character --hpreg
 */
static char const Dec2Hex[] = {
   '0', '1', '2', '3', '4', '5', '6', '7',
   '8', '9', 'A', 'B', 'C', 'D', 'E', 'F',
};


/*
 *-----------------------------------------------------------------------------
 *
 * StaticEscape_Do --
 *
 *    Escape a buffer. Expects sizeBufOut to account for the NUL terminator.
 *
 *    You can calculate the required size of the output buffer as follows:    
 *    sizeBufOut >= (((sizeIn - # of chars to be escaped) * sizeof *sizeIn) +
 *                   (sizeof escSeq * # of chars to be escaped) +
 *                    sizeof '\0')
 *
 *    Or, in English, "the number of non-escaped characters times each
 *    non-escaped character's size, plus the number of escaped characters times
 *    each escaped character's size, plus the size of the NUL terminator" (not
 *    that this is very useful, since most callers won't take the time to
 *    determine the number of characters to be escaped up front).
 *
 *    Note that this function assumes one to one mapping between characters
 *    and bytes. This works for any ASCII-transparent encodings (such as UTF8).
 *
 *    XXX: An interface with an input size in characters and an output size in
 *    bytes is broken (especially when the the return value is in bytes, but
 *    _without_ the NUL terminator). We do it to maintain consistency with
 *    the StaticEscapeW interface, where the distinction between characters
 *    and bytes is actually important.
 *
 * Results: 
 *    On success, the size (excluding the NUL terminator) of the
 *    escaped, NUL terminated buffer.
 *    On failure (bufOut not big enough to hold result), negative value.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
StaticEscape_Do(char escByte,                   // IN: The escape character
                EscBitVector const *bytesToEsc, // IN: Chars we must escape
                void const *bufIn,              // IN: Input buffer
                uint32 sizeIn,                  // IN: Size of bufIn (chars)
                uint32 sizeBufOut,              // IN: Size of bufOut (bytes)
                char *bufOut)                   // OUT: Output buffer
{
   char const *buf;
   unsigned int startUnescaped;
   unsigned int index;
   char escSeq[3];
   int copySize;
   int outPos;

   /* Make sure we won't obviously overflow the bufOut [bac] */
   if (sizeIn > sizeBufOut) {
      return -1;
   }

   ASSERT(bytesToEsc);
   /* Unsigned does matter --hpreg */
   ASSERT(EscBitVector_Test(bytesToEsc, (unsigned char)escByte));
   buf = (char const *)bufIn;
   ASSERT(buf);

   escSeq[0] = escByte;
   startUnescaped = 0;
   outPos = 0;

   for (index = 0; index < sizeIn; index++) {
      /* Unsigned does matter --hpreg */
      unsigned char ubyte;

      ubyte = buf[index];
      if (EscBitVector_Test(bytesToEsc, ubyte)) {
         /* We must escape that byte --hpreg */

         escSeq[1] = Dec2Hex[ubyte >> 4];
	 escSeq[2] = Dec2Hex[ubyte & 0xF];
         copySize = index - startUnescaped;
         if (outPos + copySize + sizeof(escSeq) > sizeBufOut) {
            /*
             * Make sure that both the first chunk and the
             * escape sequence will fit in the bufOut. [bac]
             */
            return -1;
         }
         memcpy(&bufOut[outPos], &buf[startUnescaped], copySize);
         outPos += copySize;
         copySize = sizeof(escSeq);
         memcpy(&bufOut[outPos], escSeq, copySize);
         outPos += copySize;
         startUnescaped = index + 1;
      }
   }

   copySize = index - startUnescaped;
   if (outPos + copySize + 1 > sizeBufOut) {
      /* 
       * Make sure the terminating NUL will fit too, so we don't have
       * to check again below. [bac]
       */
      return -1;
   }
   memcpy(&bufOut[outPos], &buf[startUnescaped], copySize);
   outPos += copySize;
   memcpy(&bufOut[outPos], "", 1);

   return outPos; /* Size of the output buf, not counting NUL terminator */
}


/*
 *-----------------------------------------------------------------------------
 *
 * StaticEscape_Undo --
 *
 *    Unescape a buffer --hpreg
 *
 *    The unescaping is done in place in the input buffer, and
 *    thus can not fail.
 * 
 * Results:
 *    The size (excluding the NUL terminator) of the unescaped, NUL
 *    terminated buffer.
 *    
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
StaticEscape_Undo(char escByte,  // IN
                  void *bufIn,   // IN
                  uint32 sizeIn) // IN
{
   char *buf;
   unsigned int state;
   unsigned int startUnescaped;
   unsigned int index;
   int outPos;
   int copySize;
   int h = 0; /* Compiler warning --hpreg */
   int l;

   buf = (char *)bufIn;
   ASSERT(buf);

   outPos = 0;
   startUnescaped = 0;
   state = 0;

   for (index = 0; index < sizeIn; index++) {
      /* Unsigned does matter --hpreg */
      unsigned char ubyte;

      ubyte = buf[index];
      switch (state) {
      case 0: /* Found <byte> --hpreg */
         if (ubyte == escByte) {
            state = 1;
         }
         break;

      case 1: /* Found <escByte><byte> --hpreg */
         h = Hex2Dec[ubyte];
         state = h >= 0 ? 2 : 0;
         break;

      case 2: /* Found <escByte><hexa digit><byte> --hpreg */
         l = Hex2Dec[ubyte];
         if (l >= 0) {
            char escaped;

            escaped = h << 4 | l;

            copySize = index - 2 - startUnescaped;
            memmove(&buf[outPos], &buf[startUnescaped], copySize);
            outPos += copySize;
            memcpy(&buf[outPos], &escaped, 1);
            outPos++;

            startUnescaped = index + 1;
         }
         state = 0;
         break;

      default:
         NOT_IMPLEMENTED();
         break;
      }
   }

   /* Last unescaped chunk (if any) --hpreg */
   copySize = index - startUnescaped;
   memmove(&buf[outPos], &buf[startUnescaped], copySize);
   outPos += copySize;
   memcpy(&buf[outPos], "", 1);

   return outPos;
}
