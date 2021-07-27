/*********************************************************
 * Copyright (C) 1998-2019, 2021 VMware, Inc. All rights reserved.
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
 * strutil.c --
 *
 *    String utility functions.
 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#include <wchar.h>
#else
#include <strings.h> /* For strncasecmp */
#include <stdint.h>
#endif
#include "vmware.h"
#include "strutil.h"
#include "str.h"
#include "dynbuf.h"
#include "vm_ctype.h"
#include "util.h"

#ifndef SIZE_MAX /* SIZE_MAX is new in C99 */
#define SIZE_MAX ((size_t) -1)
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_IsEmpty --
 *
 *      Test if a non-NULL string is empty.
 *
 * Results:
 *      TRUE if the string has length 0, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

#ifdef VMX86_DEBUG
static INLINE Bool
StrUtil_IsEmpty(const char *str)  // IN:
{
   ASSERT(str != NULL);
   return str[0] == '\0';
}
#endif

/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetNextToken --
 *
 *      Get the next token from a string after a given index w/o modifying the
 *      original string.
 *
 * Results:
 *      An allocated, NUL-terminated string containing the token. 'index' is
 *         updated to point after the returned token
 *      NULL if no tokens are left
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
StrUtil_GetNextToken(unsigned int *index,    // IN/OUT: Index to start at
                     const char *str,        // IN    : String to parse
                     const char *delimiters) // IN    : Chars separating tokens
{
   unsigned int startIndex;
   unsigned int length;
   char *token;

   ASSERT(index);
   ASSERT(str);
   ASSERT(delimiters);
   ASSERT(*index <= strlen(str));

#define NOT_DELIMITER (Str_Strchr(delimiters, str[*index]) == NULL)

   /* Skip leading delimiters. */
   for (; ; (*index)++) {
      if (str[*index] == '\0') {
         return NULL;
      }

      if (NOT_DELIMITER) {
         break;
      }
   }
   startIndex = *index;

   /*
    * Walk the string until we reach the end of it, or we find a
    * delimiter.
    */
   for ((*index)++; str[*index] != '\0' && NOT_DELIMITER; (*index)++) {
   }

#undef NOT_DELIMITER

   length = *index - startIndex;
   ASSERT(length);
   token = Util_SafeMalloc(length + 1 /* NUL */);
   memcpy(token, str + startIndex, length);
   token[length] = '\0';

   return token;
}


#if defined(_WIN32)
/*
 ******************************************************************************
 * StrUtil_GetNextTokenW --
 *
 *      Get the next token from a UTF-16 string after a given index w/o
 *      modifying the original string. This routine is based on the
 *      StrUtil_GetNextToken() function but convert it from ASCII to Unicode
 *      version.
 *
 * Results:
 *      An allocated, NULL-terminated string containing the token. 'index'
 *      is updated to point after the returned token
 *      NULL if no tokens are left
 *
 * Side effects:
 *      None
 *
 ******************************************************************************
 */

wchar_t *
StrUtil_GetNextTokenW(unsigned int  *index,      // IN/OUT: Index to start at
                      const wchar_t *str,        // IN    : String to parse
                      const wchar_t *delimiters) // IN    : Separating tokens
{
   unsigned int startIndex;
   unsigned int length;
   wchar_t *token;

   ASSERT(index != NULL);
   ASSERT(str != NULL);
   ASSERT(delimiters != NULL);
   ASSERT(*index <= wcslen(str));

#define NOT_DELIMITER (wcschr(delimiters, str[*index]) == NULL)

   /* Skip leading delimiters. */
   for (;; (*index)++) {
      if (str[*index] == L'\0') {
         return NULL;
      }

      if (NOT_DELIMITER) {
         break;
      }
   }
   startIndex = *index;

   /*
    * Walk the string until we reach the end of it, or we find a
    * delimiter.
    */
   for ((*index)++; str[*index] != L'\0' && NOT_DELIMITER; (*index)++) {
   }

#undef NOT_DELIMITER

   length = *index - startIndex;
   ASSERT(length);
   token = Util_SafeMalloc(sizeof *token * (length + 1));
   wmemcpy(token, str + startIndex, length);
   token[length] = L'\0';

   return token;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetNextIntToken --
 *
 *      Acts like StrUtil_GetNextToken except it returns an int32.
 *
 * Results:
 *      TRUE if a valid int was parsed and 'out' contains the parsed int.
 *      FALSE otherwise. Contents of 'out' are undefined.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_GetNextIntToken(int32 *out,             // OUT   : parsed int
                        unsigned int *index,    // IN/OUT: Index to start at
                        const char *str,        // IN    : String to parse
                        const char *delimiters) // IN    : Chars separating tokens
{
   char *resultStr;
   Bool valid = FALSE;

   ASSERT(out);
   ASSERT(index);
   ASSERT(str);
   ASSERT(delimiters);

   resultStr = StrUtil_GetNextToken(index, str, delimiters);
   if (resultStr == NULL) {
      return FALSE;
   }

   valid = StrUtil_StrToInt(out, resultStr);
   free(resultStr);

   return valid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetNextUintToken --
 *
 *      Acts like StrUtil_GetNextIntToken except it returns an uint32.
 *
 * Results:
 *      TRUE if a valid int was parsed and 'out' contains the parsed int.
 *      FALSE otherwise. Contents of 'out' are undefined.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_GetNextUintToken(uint32 *out,            // OUT   : parsed int
                         unsigned int *index,    // IN/OUT: Index to start at
                         const char *str,        // IN    : String to parse
                         const char *delimiters) // IN    : Chars separating tokens
{
   char *resultStr;
   Bool valid = FALSE;

   ASSERT(out);
   ASSERT(index);
   ASSERT(str);
   ASSERT(delimiters);

   resultStr = StrUtil_GetNextToken(index, str, delimiters);
   if (resultStr == NULL) {
      return FALSE;
   }

   valid = StrUtil_StrToUint(out, resultStr);
   free(resultStr);

   return valid;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetNextInt64Token --
 *
 *      Acts like StrUtil_GetNextToken except it returns an int64.
 *
 * Results:
 *      TRUE on a successful retrieval. FALSE otherwise.
 *      Token is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_GetNextInt64Token(int64 *out,             // OUT: The output value
                          unsigned int *index,    // IN/OUT: Index to start at
                          const char *str,        // IN    : String to parse
                          const char *delimiters) // IN    : Chars separating tokens
{
   char *resultStr;
   Bool result;

   ASSERT(out);
   ASSERT(index);
   ASSERT(str);
   ASSERT(delimiters);

   resultStr = StrUtil_GetNextToken(index, str, delimiters);
   result = resultStr ? StrUtil_StrToInt64(out, resultStr) : FALSE;
   free(resultStr);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_DecimalStrToUint --
 *
 *      Convert a string into an integer.
 *
 * Results:
 *      TRUE if the conversion was successful, and 'out' contains the converted
 *      result, and 'str' is updated to point to new place after last processed
 *      digit.
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_DecimalStrToUint(unsigned int *out, // OUT
                         const char **str)  // IN/OUT : String to parse
{
   unsigned long val;
   char *ptr;

   ASSERT(out);
   ASSERT(str);

   errno = 0;
   val = strtoul(*str, &ptr, 10);
   if (ptr == *str || errno == ERANGE || val != (unsigned int)val) {
      return FALSE;
   }
   *str = ptr;
   *out = (unsigned int)val;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToInt --
 *
 *      Convert a string into an integer.
 *
 * Results:
 *      TRUE if the conversion was successful and 'out' contains the converted
 *      result.
 *      FALSE otherwise. 'out' is undefined.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToInt(int32 *out,      // OUT
                 const char *str) // IN : String to parse
{
   char *ptr;
   long val;

   ASSERT(out);
   ASSERT(str);

   errno = 0;

   val = strtol(str, &ptr, 0);
   *out = (int32)val;

   /*
    * Input must be non-empty, complete, no overflow, and value read must fit
    * into 32 bits - both signed and unsigned values are accepted.
    */

   return ptr != str && *ptr == '\0' && errno != ERANGE &&
          (val == (int32)val || val == (uint32)val);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToUint --
 *
 *      Convert a string into unsigned integer.
 *
 * Results:
 *      TRUE if the conversion succeeded and 'out' contains the result.
 *      FALSE otherwise. 'out' is undefined.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToUint(uint32 *out,     // OUT
                  const char *str) // IN : String to parse
{
   char *ptr;
   unsigned long val;

   ASSERT(out);
   ASSERT(str);

   errno = 0;

   val = strtoul(str, &ptr, 0);
   *out = (uint32)val;

   /*
    * Input must be non-empty, complete, no overflow, and value read must fit
    * into 32 bits - both signed and unsigned values are accepted.
    */

   return ptr != str && *ptr == '\0' && errno != ERANGE &&
          (val == (uint32)val || val == (int32)val);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToInt64 --
 *
 *      Convert a string into a 64bit integer.
 *
 * Results:
 *      TRUE if conversion was successful, FALSE otherwise.
 *      Value is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToInt64(int64 *out,      // OUT: The output value
                   const char *str) // IN : String to parse
{
   char *ptr;

   ASSERT(out);
   ASSERT(str);

   errno = 0;

#if defined(_WIN32)
   *out = _strtoi64(str, &ptr, 0);
#elif defined(__FreeBSD__)
   *out = strtoq(str, &ptr, 0);
#else
   *out = strtoll(str, &ptr, 0);
#endif

   return ptr != str && ptr[0] == '\0' && errno != ERANGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToUint64 --
 *
 *      Convert a string into an unsigned 64bit integer.
 *
 * Results:
 *      TRUE if conversion was successful, FALSE otherwise.
 *      Value is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToUint64(uint64 *out,     // OUT: The output value
                    const char *str) // IN : String to parse
{
   char *ptr;

   ASSERT(out);
   ASSERT(str);

   errno = 0;

#if defined(_WIN32)
   *out = _strtoui64(str, &ptr, 0);
#elif defined(__FreeBSD__)
   *out = strtouq(str, &ptr, 0);
#else
   *out = strtoull(str, &ptr, 0);
#endif

   return ptr != str && ptr[0] == '\0' && errno != ERANGE && errno != EINVAL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToSizet --
 *
 *      Convert a string into an unsigned integer that is either 32-bits or
 *      64-bits long, depending on the underlying architecture.
 *
 * Results:
 *      TRUE if conversion was successful, FALSE otherwise.
 *      Value is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToSizet(size_t *out,     // OUT: The output value
                   const char *str) // IN : String to parse
{
   char *ptr;

   ASSERT(out);
   ASSERT(str);

   errno = 0;
#if defined VM_64BIT
   ASSERT_ON_COMPILE(sizeof *out == sizeof(uint64));
#   if defined(_WIN32)
   *out = _strtoui64(str, &ptr, 0);
#   elif defined(__FreeBSD__)
   *out = strtouq(str, &ptr, 0);
#   else
   *out = strtoull(str, &ptr, 0);
#   endif
#else
   ASSERT_ON_COMPILE(sizeof *out == sizeof(uint32));
   *out = strtoul(str, &ptr, 0);
#endif

   return ptr != str && *ptr == '\0' && errno != ERANGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_StrToDouble --
 *
 *      Convert a string into a double.
 *
 * Results:
 *      TRUE if the conversion was successful and 'out' contains the converted
 *      result.
 *      FALSE otherwise. 'out' is undefined.
 *
 * Side effects:
 *      Modifies errno.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_StrToDouble(double *out,      // OUT: The output value
                    const char *str)  // IN : String to parse
{
   char *ptr = NULL;

   ASSERT(out);
   ASSERT(str);

   errno = 0;

   *out = strtod(str, &ptr);

   /*
    * Input must be complete and no overflow.
    */

   return ptr != str && *ptr == '\0' && errno != ERANGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_CapacityToBytes --
 *
 *      Converts a string containing a measure of capacity (such as
 *      "100MB" or "1.5k") into an unadorned and primitive quantity of bytes
 *      capacity. The comment before the switch statement describes the kinds
 *      of capacity expressible.
 *
 * Results:
 *      TRUE if conversion was successful, FALSE otherwise.
 *      Value is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_CapacityToBytes(uint64 *out,        // OUT: The output value
                        const char *str,    // IN: String to parse
                        unsigned int bytes) // IN: Bytes per unit in an
                                            //     unadorned string

{
   double quantity;
   char *rest;

   ASSERT(out);
   ASSERT(str);

   errno = 0;
   quantity = strtod(str, &rest);
   if (errno == ERANGE) {
      return FALSE;
   }

   /* Skip over any whitespace in the suffix. */
   while (*rest == ' ' || *rest == '\t') {
      rest++;
   }
   if (*rest != '\0') {
      uint64 shift;
      Bool suffixOK = TRUE;

      /*
       * [kK], [mM], [gG], and [tT] represent kilo, mega, giga, and tera
       * byte quantities respectively. [bB] represents a singular byte
       * quantity. [sS] represents a sector quantity.
       *
       * For kilo, mega, giga, and tera we're OK with an additional byte
       * suffix. Otherwise, the presence of an additional suffix is an error.
       */
      switch (*rest) {
      case 'b': case 'B': shift = 0;  suffixOK = FALSE; break;
      case 's': case 'S': shift = 9;  suffixOK = FALSE; break;
      case 'k': case 'K': shift = 10;                   break;
      case 'm': case 'M': shift = 20;                   break;
      case 'g': case 'G': shift = 30;                   break;
      case 't': case 'T': shift = 40;                   break;
      default :                                         return FALSE;
      }
      switch(*++rest) {
      case '\0':
         break;
      case 'b': case 'B':
         if (suffixOK && !*++rest) {
            break;
         }
         /* FALLTHRU */
      default:
         return FALSE;
      }
      quantity *= CONST64U(1) << shift;
   } else {
      /*
       * No suffix, so multiply by the number of bytes per unit as specified
       * by the caller.
       */
      quantity *= bytes;
   }

   *out = quantity;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_CapacityToSectorType --
 *
 *      Converts a string containing a measure of disk capacity (such as
 *      "100MB" or "1.5k") into an unadorned and primitive quantity of sector
 *      capacity.
 *
 * Results:
 *      TRUE if conversion was successful, FALSE otherwise.
 *      Value is stored in 'out', which is left undefined in the FALSE case.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_CapacityToSectorType(SectorType *out,    // OUT: The output value
                             const char *str,    // IN: String to parse
                             unsigned int bytes) // IN: Bytes per unit in an
                                                 //     unadorned string

{
   uint64 quantityInBytes;

   if (StrUtil_CapacityToBytes(&quantityInBytes, str, bytes) == FALSE) {
      return FALSE;
   }

   /*
    * Convert from "number of bytes" to "number of sectors", rounding up or
    * down appropriately.
    *
    * XXX: We should use DISKLIB_SECTOR_SIZE, but do we really want the
    * disklib header dependencies in this file?
    *
    */
   *out = (SectorType)((quantityInBytes + 256) / 512);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_FormatSizeInBytesUnlocalized --
 *
 *      Format a size (in bytes) to a string in a user-friendly way.
 *
 *      Example: 160041885696 -> "149.1 GB"
 *
 * Results:
 *      The allocated, NUL-terminated string (not localized).
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
StrUtil_FormatSizeInBytesUnlocalized(uint64 size) // IN
{
   /*
    * XXX TODO, BUG 199661:
    * This is a direct copy of Msg_FormatSizeInBytes without localization.
    * These two functions should ideally share the basic functionality, and
    * just differ in the string localization
    */

   char const *fmt;
   double sizeInSelectedUnit;
   unsigned int precision;
   char *sizeFormat;
   char *sizeString;
   char *result;
   static const double epsilon = 0.01;
   double delta;

   if (size >= CONST64U(1) << 40 /* 1 TB */) {
      fmt = "%s TB";
      sizeInSelectedUnit = (double)size / (CONST64U(1) << 40);
      precision = 1;
   } else if (size >= CONST64U(1) << 30 /* 1 GB */) {
      fmt = "%s GB";
      sizeInSelectedUnit = (double)size / (CONST64U(1) << 30);
      precision = 1;
   } else if (size >= CONST64U(1) << 20 /* 1 MB */) {
      fmt = "%s MB";
      sizeInSelectedUnit = (double)size / (CONST64U(1) << 20);
      precision = 1;
   } else if (size >= CONST64U(1) << 10 /* 1 KB */) {
      fmt = "%s KB";
      sizeInSelectedUnit = (double)size / (CONST64U(1) << 10);
      precision = 1;
   } else if (size >= CONST64U(2) /* 2 bytes */) {
      fmt = "%s bytes";
      sizeInSelectedUnit = (double)size;
      precision = 0; // No fractional byte.
   } else if (size >= CONST64U(1) /* 1 byte */) {
      fmt = "%s byte";
      sizeInSelectedUnit = (double)size;
      precision = 0; // No fractional byte.
   } else {
      ASSERT(size == CONST64U(0) /* 0 bytes */);
      fmt = "%s bytes";
      sizeInSelectedUnit = (double)size;
      precision = 0; // No fractional byte.
   }

   /*
    * We cast to uint32 instead of uint64 here because of a problem with the
    * NetWare Tools build. However, it's safe to cast to uint32 since we have
    * already reduced the range of sizeInSelectedUnit above.
    */
   // If it would display with .0, round it off and display the integer value.
   delta = (uint32)(sizeInSelectedUnit + 0.5) - sizeInSelectedUnit;
   if (delta < 0) {
      delta = -delta;
   }
   if (delta <= epsilon) {
      precision = 0;
      sizeInSelectedUnit = (double)(uint32)(sizeInSelectedUnit + 0.5);
   }

   sizeFormat = Str_Asprintf(NULL, "%%.%uf", precision);
   sizeString = Str_Asprintf(NULL, sizeFormat, sizeInSelectedUnit);
   result = Str_Asprintf(NULL, fmt, sizeString);
   free(sizeFormat);
   free(sizeString);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * StrUtil_GetLongestLineLength --
 *
 *      Given a buffer with one or more lines
 *      this function computes the length of the
 *      longest line in a buffer.  Input buffer is an array of
 *      arbitrary bytes (including NUL character), line separator
 *      is '\n', and is counted in line length.  Like:
 *        "", 0     => 0
 *        "\n", 1   => 1
 *        "X", 1    => 1
 *        "XX\n", 3 => 3
 *        "X\nY", 3 => 2
 *        "\n\n", 2 => 1
 *
 * Results:
 *      Returns the length of the longest line in the 'buf'.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

size_t
StrUtil_GetLongestLineLength(const char *buf,   //IN
                             size_t bufLength)  //IN
{
    size_t longest = 0;

    while (bufLength) {
       const char *next;
       size_t len;

       ASSERT(buf != NULL);
       /* coverity[var_deref_model] */
       next = memchr(buf, '\n', bufLength);
       if (next) {
          next++;
          len = next - buf;
       } else {
          len = bufLength;
       }
       if (len > longest) {
          longest = len;
       }
       bufLength -= len;
       buf = next;
    }

    return longest;
}


/*
 *----------------------------------------------------------------------
 *
 * StrUtil_StartsWith --
 *
 *      Determines if a string starts with another string.
 *
 * Results:
 *      TRUE if 's' starts with 'prefix', FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
StrUtil_StartsWith(const char *s,      // IN
                   const char *prefix) // IN
{
   ASSERT(s != NULL);
   ASSERT(prefix != NULL);

   while (*prefix && *prefix == *s) {
      prefix++;
      s++;
   }

   return *prefix == '\0';
}


/*
 *----------------------------------------------------------------------
 *
 * StrUtil_CaselessStartsWith --
 *
 *      A case-insensitive version of StrUtil_StartsWith.
 *
 * Results:
 *      TRUE if 's' starts with 'prefix', FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
StrUtil_CaselessStartsWith(const char *s,      // IN
                           const char *prefix) // IN
{
   ASSERT(s != NULL);
   ASSERT(prefix != NULL);

   return Str_Strncasecmp(s, prefix, strlen(prefix)) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_EndsWith --
 *
 *      Detects if a string ends with another string.
 *
 * Results:
 *      TRUE  if string 'suffix' is found at the end of string 's'
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_EndsWith(const char *s,      // IN
                 const char *suffix) // IN
{
   size_t slen;
   size_t suffixlen;

   ASSERT(s != NULL);
   ASSERT(suffix != NULL);

   slen = strlen(s);
   suffixlen = strlen(suffix);

   if (suffixlen > slen) {
      return FALSE;
   }

   return strcmp(s + slen - suffixlen, suffix) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_CaselessEndsWith --
 *
 *      A case-insensitive version of StrUtil_EndsWith.
 *
 * Results:
 *      TRUE  if string 'suffix' is found at the end of string 's'
 *      FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_CaselessEndsWith(const char *s,      // IN
                         const char *suffix) // IN
{
   size_t slen;
   size_t suffixlen;

   ASSERT(s != NULL);
   ASSERT(suffix != NULL);
   ASSERT(StrUtil_IsASCII(suffix));

   slen = strlen(s);
   suffixlen = strlen(suffix);

   if (suffixlen > slen) {
      return FALSE;
   }

   return Str_Strcasecmp(s + (slen - suffixlen), suffix) == 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_IsASCII --
 *
 * Results:
 *      Returns TRUE if the string contains only ASCII characters, FALSE
 *      otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_IsASCII(const char *s) // IN
{
   ASSERT(s != NULL);

   while (*s != '\0') {
      if (!CType_IsAscii(*s)) {
         return FALSE;
      }
      s++;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_VDynBufPrintf --
 *
 *      This is a vprintf() variant which appends directly into a
 *      DynBuf.  Does NOT visibly NUL-terminate the DynBuf.
 *
 *      This function does not use any temporary buffer. The printf()
 *      result can be arbitrarily large. This function automatically
 *      grows the DynBuf as necessary.
 *
 * Results:
 *      TRUE on success, FALSE on memory allocation failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_VDynBufPrintf(DynBuf *b,        // IN/OUT
                      const char *fmt,  // IN
                      va_list args)     // IN
{
   /*
    * Arbitrary lower-limit on buffer size allocation, to avoid doing
    * many tiny enlarge operations.
    */

   const size_t minAllocSize = 128;

   while (1) {
      int i;
      size_t size = DynBuf_GetSize(b);
      size_t allocSize = DynBuf_GetAllocatedSize(b);

      /* Make sure the buffer isn't still unallocated */
      if (allocSize < minAllocSize) {
         Bool success = DynBuf_Enlarge(b, minAllocSize);
         if (!success) {
            return FALSE;
         }
         continue;
      }

      /*
       * Is there any allocated-but-not-occupied space? If so, try the printf.
       * If there was no space to begin with, or Str_Vsnprintf() ran out of
       * space, this will fail.
       */

      if (allocSize - size > 0) {
         va_list tmpArgs;

         va_copy(tmpArgs, args);

         /*
          * We actually do NUL-terminate the buffer internally, but this is not
          * visible to callers, and they should not rely on this.
          */
         i = Str_Vsnprintf((char *) DynBuf_Get(b) + size, allocSize - size,
                           fmt, tmpArgs);
         va_end(tmpArgs);
      } else {
         i = -1;
      }

      if (i >= 0) {
         /*
          * Success. Enlarge the buffer.
          *
          * The ASSERT here is to verify that printf() isn't lying
          * about the length of the string it wrote. This actually
          * happens, believe it or not. See bug 253674.
          */

         ASSERT(i + size == allocSize ||
                ((char *)DynBuf_Get(b))[i + size] == '\0');

         DynBuf_SetSize(b, size + i);
         break;

      } else {
         /*
          * Failure. We must grow the buffer first.  Note that this is
          * just a minimum size- dynbuf will probably decide to double
          * the actual reallocated buffer size.
          */

         Bool success = DynBuf_Enlarge(b, size + minAllocSize);

         if (!success) {
            return FALSE;
         }
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_DynBufPrintf --
 *
 *      A StrUtil_VDynBufPrintf() wrapper which takes a variadic argument list.
 *
 * Results:
 *      TRUE on success, FALSE on memory allocation failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_DynBufPrintf(DynBuf *b,        // IN/OUT
                     const char *fmt,  // IN
                     ...)              // IN
{
   va_list args;
   Bool success;

   va_start(args, fmt);
   success = StrUtil_VDynBufPrintf(b, fmt, args);
   va_end(args);

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_SafeDynBufPrintf --
 *
 *      A 'safe' variant of StrUtil_DynBufPrintf(), which catches
 *      memory allocation failures and panics.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_SafeDynBufPrintf(DynBuf *b,        // IN/OUT
                         const char *fmt,  // IN
                         ...)              // IN
{
   va_list args;
   Bool success;

   va_start(args, fmt);
   success = StrUtil_VDynBufPrintf(b, fmt, args);
   va_end(args);

   VERIFY(success);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_SafeStrcat --
 *
 *      Given an input buffer, append another string and return the resulting
 *      string. The input buffer is freed along the way. A fancy strcat().
 *
 * Results:
 *      New buffer returned via 'prefix'
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_SafeStrcat(char **prefix,    // IN/OUT:
                   const char *str)  // IN:
{
   char *tmp;
   size_t plen = (*prefix == NULL) ? 0 : strlen(*prefix);
   size_t slen = strlen(str);

   /*
    * If we're manipulating strings that are anywhere near max(size_t)/2 in
    * length we're doing something very wrong. Avoid potential overflow by
    * checking for "insane" operations. Prevent the problem before it gets
    * started.
    */

   VERIFY((plen < (SIZE_MAX/2)) && (slen < (SIZE_MAX/2)));

   tmp = Util_SafeRealloc(*prefix, plen + slen + 1 /* NUL */);

   memcpy(tmp + plen, str, slen + 1 /* NUL */);
   *prefix = tmp;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_SafeStrcatFV --
 *
 *      Given an input buffer, append another string and return the resulting
 *      string. The input buffer is freed along the way. A fancy vasprintf().
 *
 * Results:
 *      New buffer returned via 'prefix'
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_SafeStrcatFV(char **prefix,    // IN/OUT
                     const char *fmt,  // IN
                     va_list args)     // IN
{
   char *str = Str_SafeVasprintf(NULL, fmt, args);
   StrUtil_SafeStrcat(prefix, str);
   free(str);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_SafeStrcatF --
 *
 *      Given an input buffer, append another string and return the resulting
 *      string. The input buffer is freed along the way. A fancy asprintf().
 *
 * Results:
 *      New buffer returned via 'prefix'
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_SafeStrcatF(char **prefix,    // IN/OUT
                    const char *fmt,  // IN
                    ...)              // IN
{
   va_list args;

   va_start(args, fmt);
   StrUtil_SafeStrcatFV(prefix, fmt, args);
   va_end(args);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_TrimWhitespace --
 *
 *      Return a copy of the input string with leading and trailing
 *      whitespace removed.
 *
 * Results:
 *      See above. Caller should free with free().
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
StrUtil_TrimWhitespace(const char *str)  // IN
{
   char *cur = (char *)str;
   char *res = NULL;
   size_t len;

   /* Skip leading whitespace. */
   while (*cur && isspace(*cur)) {
      cur++;
   }

   /* Copy the remaining string. */
   res = Util_SafeStrdup(cur);

   /* Find the beginning of the trailing whitespace. */
   len = strlen(res);
   if (len == 0) {
      return res;
   }

   cur = res + len - 1;
   while (cur > res && isspace(*cur)) {
      cur--;
   }

   /* Truncate it. */
   cur++;
   *cur = 0;

   return res;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_ReplaceAll --
 *
 *    Replaces all occurrences of a non-empty substring with non-NULL pattern
 *    in non-NULL string.
 *
 * Results:
 *    Returns pointer to the allocated resulting string. The caller is
 *    responsible for freeing it.
 *
 *-----------------------------------------------------------------------------
 */

char *
StrUtil_ReplaceAll(const char *orig, // IN
                   const char *what, // IN
                   const char *with) // IN
{
    char *result;
    const char *current;
    char *tmp;
    size_t lenWhat;
    size_t lenWith;
    size_t occurrences = 0;
    size_t lenNew;

    ASSERT(orig != NULL);
    ASSERT(!StrUtil_IsEmpty(what));
    ASSERT(with != NULL);

    lenWhat = strlen(what);
    lenWith = strlen(with);

    current = orig;
    while ((tmp = strstr(current, what)) != NULL) {
       current = tmp + lenWhat;
       ++occurrences;
    }

    lenNew = strlen(orig) + (lenWith - lenWhat) * occurrences;
    tmp = Util_SafeMalloc(lenNew + 1);
    result = tmp;

    while (occurrences--) {
       size_t lenBefore;

       current = strstr(orig, what);
       lenBefore = current - orig;
       tmp = memcpy(tmp, orig, lenBefore);
       tmp += lenBefore;
       tmp = memcpy(tmp, with, lenWith);
       tmp += lenWith;
       orig += lenBefore + lenWhat;
    }
    memcpy(tmp, orig, strlen(orig));

    result[lenNew] = '\0';

    return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilStrcmp --
 *
 *      Wraps around the Str_Strcmp macro to provide a function that we
 *      can take the pointer for.
 *
 * Results:
 *      Same as Str_Strcmp.
 *
 * Side effects:
 *      Same as Str_Strcmp.
 *
 *-----------------------------------------------------------------------------
 */

static int
StrUtilStrcmp(const char *s1, const char *s2)
{
   return Str_Strcmp(s1, s2);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilStrncmp --
 *
 *      Wraps around the Str_Strncmp macro to provide a function that we
 *      can take the pointer for.
 *
 * Results:
 *      Same as Str_Strncmp.
 *
 * Side effects:
 *      Same as Str_Strncmp.
 *
 *-----------------------------------------------------------------------------
 */

static int
StrUtilStrncmp(const char *s1, const char *s2, size_t n)
{
   return Str_Strncmp(s1, s2, n);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilStrcasecmp --
 *
 *      Wraps around the Str_Strcasecmp macro to provide a function that we
 *      can take the pointer for.
 *
 * Results:
 *      Same as Str_Strcasecmp.
 *
 * Side effects:
 *      Same as Str_Strcasecmp.
 *
 *-----------------------------------------------------------------------------
 */

static int
StrUtilStrcasecmp(const char *s1, const char *s2)
{
   return Str_Strcasecmp(s1, s2);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilStrncasecmp --
 *
 *      Wraps around the Str_Strncasecmp macro to provide a function that we
 *      can take the pointer for.
 *
 * Results:
 *      Same as Str_Strncasecmp.
 *
 * Side effects:
 *      Same as Str_Strncasecmp.
 *
 *-----------------------------------------------------------------------------
 */

static int
StrUtilStrncasecmp(const char *s1, const char *s2, size_t n)
{
   return Str_Strncasecmp(s1, s2, n);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetNextItem --
 *
 *      Extract the next item from a list of items delimited by delim. It
 *      behaves like strsep except it doesn't accept a string of delimiters.
 *
 * Results:
 *      Returns a pointer to the first item and makes list point to the rest of
 *      the list or NULL if list was NULL or there was only one item.
 *
 * Side effects:
 *      The first delimiter is changed to '\0'.
 *
 *-----------------------------------------------------------------------------
 */

char*
StrUtil_GetNextItem(char **list, // IN/OUT:
                    char delim)  // IN:
{
   char *token = *list;
   char *foundDelim;

   if (*list == NULL) {
      return NULL;
   }

   foundDelim = strchr(*list, delim);
   if (foundDelim != NULL) {
      foundDelim[0] = '\0';
      *list = foundDelim + 1;
   } else {
      *list = NULL;
   }

   return token;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_GetLastItem --
 *
 *      Extract the last item from a list of items delimited by delim.
 *
 * Results:
 *      Returns a pointer to the last item and makes list point to the rest of
 *      the list or NULL if list was NULL or there was only one item.
 *
 * Side effects:
 *      The last delimiter is changed to '\0'.
 *
 *-----------------------------------------------------------------------------
 */

char*
StrUtil_GetLastItem(char **list, // IN/OUT:
                    char delim)  // IN:
{
   char *token = *list;
   char *foundDelim;

   if (*list == NULL) {
      return NULL;
   }

   foundDelim = strrchr(*list, delim);
   if (foundDelim != NULL) {
      foundDelim[0] = '\0';
      token = foundDelim + 1;
   } else {
      *list = NULL;
   }

   return token;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilHasListItem --
 *
 *      Checks whether an item is a part of a list of tokens
 *      separated by a delimiter.
 *
 * Results:
 *      Whether the item exists in the list.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
StrUtilHasListItem(char const *list,                               // IN:
                   char delim,                                     // IN:
                   char const *item,                               // IN:
                   int (*ncmp)(char const *, char const*, size_t)) // IN:
{
   char *foundDelim;
   int itemLen = strlen(item);

   if (list == NULL) {
      return FALSE;
   }

   do {
      int tokenLen;

      foundDelim = strchr(list, delim);
      if (foundDelim == NULL) { // either single or last element
         tokenLen = strlen(list);
      } else { // ! last element
         tokenLen = foundDelim - list;
      }

      if (itemLen == tokenLen && ncmp(item, list, itemLen) == 0) {
         return TRUE;
      } else if (foundDelim != NULL) {
         // ! last element
         list = foundDelim + 1;
      }
   } while (foundDelim != NULL);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_HasListItem --
 *
 *      Checks whether an item is a part of a list of tokens
 *      separated by a delimiter.
 *
 * Results:
 *      Whether the item exists in the list.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_HasListItem(char const *list,   // IN:
                    char delim,         // IN:
                    char const *item)   // IN:
{
   return StrUtilHasListItem(list, delim, item,
                             &StrUtilStrncmp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_HasListItemCase --
 *
 *      Checks whether an item is a part of a list of tokens
 *      separated by a delimiter. Case insensitive.
 *
 * Results:
 *      Whether the item exists in the list.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
StrUtil_HasListItemCase(char const *list,   // IN:
                        char delim,         // IN:
                        char const *item)   // IN:
{
   return StrUtilHasListItem(list, delim, item,
                             &StrUtilStrncasecmp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_AppendListItem --
 *
 *      Insert an item into a list of tokens separated by a delimiter.
 *
 * Results:
 *      A pointer to a new list with the item appended at the end.
 *
 * Side effects:
 *      Allocates a new list.
 *
 *-----------------------------------------------------------------------------
 */

char *
StrUtil_AppendListItem(char const *list,  // IN:
                       char delim,        // IN:
                       char const *item)  // IN:
{
   if (list == NULL) {
      return Str_Asprintf(NULL, "%s", item);
   } else {
      return Str_Asprintf(NULL, "%s%c%s", list, delim, item);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtilRemoveListItem --
 *
 *      Removes first occurence of an item from a list of tokens separated by
 *      a delimiter.
 *
 * Results:
 *      The list is modified in-place and the item is removed.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

static void
StrUtilRemoveListItem(char * const list,                    // IN/OUT:
                      char delim,                           // IN:
                      char const *item,                     // IN:
                      int (*cmp)(char const*, char const*)) // IN:
{
   char *tok;
   char *work = list;
   int maxSize = strlen(list) + 1;

   while ((tok = StrUtil_GetNextItem(&work, delim)) != NULL) {
      if (cmp(tok, item) == 0) { // found the item
         if (work != NULL) { // in the middle of the list
            // overwrite it with the rest of the list
            Str_Strcpy(tok, work, maxSize);
         } else if (tok == list) {
            tok[0] = '\0'; // only item in the list
         } else {
            tok[-1] = '\0'; // or the last element in the list
         }

         return;
      } else if (work != NULL) {
         // restore delimiter that was replaced by Str_GetNextItem
         work[-1] = delim;
      }
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_RemoveListItem --
 *
 *      Removes first occurence of an item from a list of tokens separated by
 *      a delimiter.
 *
 * Results:
 *      The list is modified in-place and the item is removed.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_RemoveListItem(char * const list,  // IN/OUT:
                       char delim,         // IN:
                       char const *item)   // IN:
{
   StrUtilRemoveListItem(list, delim, item, &StrUtilStrcmp);
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_RemoveListItemCase --
 *
 *      Remove first occurence of an item from a list of tokens separated by a
 *      delimiter. Case insensitive.
 *
 * Results:
 *      The list is modified in-place and the item is removed.
 *
 * Side effects:
 *      See above.
 *
 *-----------------------------------------------------------------------------
 */

void
StrUtil_RemoveListItemCase(char * const list,  // IN/OUT:
                           char delim,         // IN:
                           char const *item)   // IN:
{
   StrUtilRemoveListItem(list, delim, item, &StrUtilStrcasecmp);
}

