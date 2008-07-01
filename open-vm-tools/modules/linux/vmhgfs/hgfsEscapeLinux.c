/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * hgfsEscapeLinux.c --
 *
 *    Escape and unescape filenames that are not legal on linux.
 *
 */

#include "staticEscape.h"
#include "vmware.h"
#include "hgfsEscape.h"


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscape_Do --
 *
 *    Escape any characters that are not legal in a linux filename,
 *    which is just the character "/". We also of course have to
 *    escape the escape character, which is "%".
 *
 *    sizeBufOut must account for the NUL terminator.
 *
 *    XXX: See the comments in staticEscape.c and staticEscapeW.c to understand
 *    why this interface sucks.
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
HgfsEscape_Do(char const *bufIn, // IN:  Buffer with unescaped input
	      uint32 sizeIn,     // IN:  Size of input buffer (chars)
	      uint32 sizeBufOut, // IN:  Size of output buffer (bytes)
	      char *bufOut)      // OUT: Buffer for escaped output
{
   /*
    * This is just a wrapper around the more general escape
    * routine; we pass it the correct bitvector and the
    * buffer to escape. [bac]
    */
   EscBitVector bytesToEsc;

   ASSERT(bufIn);
   ASSERT(bufOut);

   /* Set up the bitvector for "/" and "%" */
   EscBitVector_Init(&bytesToEsc);
   EscBitVector_Set(&bytesToEsc, (unsigned char)'%');
   EscBitVector_Set(&bytesToEsc, (unsigned char)'/');

   return StaticEscape_Do('%',
                          &bytesToEsc,
                          bufIn,
                          sizeIn,
                          sizeBufOut,
                          bufOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscape_Undo --
 *
 *    Unescape a buffer that was escaped using HgfsEscapeBuffer.
 *
 *    The unescaping is done in place in the input buffer, and
 *    can not fail.
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
HgfsEscape_Undo(char *bufIn,   // IN: Buffer to be unescaped
		uint32 sizeIn) // IN: Size of input buffer
{
   /*
    * This is just a wrapper around the more general unescape
    * routine; we pass it the correct escape character and the
    * buffer to unescape. [bac]
    */
   ASSERT(bufIn);
   return StaticEscape_Undo('%', bufIn, sizeIn);
}
