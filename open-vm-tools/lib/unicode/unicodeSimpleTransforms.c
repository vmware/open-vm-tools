/*********************************************************
 * Copyright (C) 2007-2016 VMware, Inc. All rights reserved.
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
 * unicodeSimpleTransforms.c --
 *
 *      Simple UTF-8 implementation of unicodeTransforms.h interface.
 */

#include "vmware.h"

#include "unicodeBase.h"
#include "unicodeInt.h"
#include "unicodeTransforms.h"

static INLINE Bool UnicodeSimpleIsWhiteSpace(utf16_t codeUnit);

/*
 * Tables generated from bora/lib/unicode/makeWhitespacePages.c,
 * running against ICU 3.8 (which implements Unicode 5.0.0).
 *
 * The table wspg_XY denotes the whitespace characters in the Unicode
 * range U+XY00 to U+XYFF (where XY is the hexadecimal upper byte of
 * the 16-bit Unicode code point value).
 */

static const Bool wspg_00[256] =
{
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   /*
    * TODO: U+00A0 (no-break space) is not treated as whitespace by
    * ICU's UnicodeString::trim().  Do we want to do the same?
    */
   TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

static const Bool wspg_16[256] =
{
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

static const Bool wspg_18[256] =
{
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,  FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

static const Bool wspg_20[256] =
{
   TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,  TRUE,
   TRUE,  TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   TRUE,  TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, TRUE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

static const Bool wspg_30[256] =
{
   TRUE,  FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
   FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE,
};

static const Bool *whitespacePages[256] =
{
   wspg_00, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    wspg_16, NULL,
   wspg_18, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   wspg_20, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   wspg_30, NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
   NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,    NULL,
};

typedef enum {
   UNICODE_TRIMLEFT  = 0x1,
   UNICODE_TRIMRIGHT = 0x2,
   UNICODE_TRIMBOTH  = (UNICODE_TRIMLEFT | UNICODE_TRIMRIGHT),
} UnicodeTrimSide;


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeSimpleIsWhiteSpace --
 *
 *      Checks if the UTF-16 code unit represents white space.
 *
 * Results:
 *      TRUE if the code unit represents white space, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE Bool
UnicodeSimpleIsWhiteSpace(utf16_t codeUnit) // IN
{
   const Bool *page = whitespacePages[codeUnit >> 8];

   if (!page) {
      return FALSE;
   }

   return page[codeUnit & 0xFF];
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_FoldCase --
 *
 *      Creates a Unicode string with standardized case by performing
 *      simple case folding (upper-case, then lower-case) on the
 *      input string.
 *
 * Results:
 *      The allocated Unicode string.  Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_FoldCase(const char *str) // IN
{
   char *folded;
   utf16_t *utf16;
   utf16_t *utf16Current;

   ASSERT(str);

   utf16 = Unicode_GetAllocBytes(str, STRING_ENCODING_UTF16);

   utf16Current = utf16;
   while (*utf16Current) {
      *utf16Current = UnicodeSimpleCaseFold(*utf16Current);
      utf16Current++;
   }

   folded = Unicode_AllocWithUTF16(utf16);
   free(utf16);

   return folded;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UnicodeTrimInternal --
 *
 *      Creates a Unicode string by trimming whitespace from the beginning
 *      and/or end of the input string, depending on the input parameter "side".
 *
 * Results:
 *      The allocated Unicode string.  Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
UnicodeTrimInternal(const char *str,       // IN
                    UnicodeTrimSide side)  // IN
{
   char *trimmed;
   utf16_t *utf16;
   utf16_t *utf16Start;
   utf16_t *utf16End;

   ASSERT(str);

   utf16 = Unicode_GetAllocBytes(str, STRING_ENCODING_UTF16);
   utf16Start = utf16;
   utf16End = utf16 + Unicode_UTF16Strlen(utf16);

   if (side & UNICODE_TRIMLEFT) {
      while (utf16Start != utf16End && UnicodeSimpleIsWhiteSpace(*utf16Start)) {
         utf16Start++;
      }
   }

   if (side & UNICODE_TRIMRIGHT) {
      while (utf16End != utf16Start && UnicodeSimpleIsWhiteSpace(*(utf16End - 1))) {
         utf16End--;
      }
   }

   *utf16End = 0;

   trimmed = Unicode_AllocWithUTF16(utf16Start);
   free(utf16);

   return trimmed;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_Trim --
 *
 *      Creates a Unicode string by trimming whitespace from the beginning
 *      and end of the input string.
 *
 * Results:
 *      The allocated Unicode string.  Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_Trim(const char *str) // IN
{
   return UnicodeTrimInternal(str, UNICODE_TRIMBOTH);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_TrimLeft --
 *
 *      Creates a Unicode string by trimming whitespace from the beginning of
 *      the input string.
 *
 * Results:
 *      The allocated Unicode string.  Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_TrimLeft(const char *str) // IN
{
   return UnicodeTrimInternal(str, UNICODE_TRIMLEFT);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Unicode_TrimRight --
 *
 *      Creates a Unicode string by trimming whitespace from the end of the 
 *      input string.
 *
 * Results:
 *      The allocated Unicode string.  Caller must free with free().
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Unicode_TrimRight(const char *str) // IN
{
   return UnicodeTrimInternal(str, UNICODE_TRIMRIGHT);
}
