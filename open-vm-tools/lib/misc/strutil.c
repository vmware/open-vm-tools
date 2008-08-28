/*********************************************************
 * Copyright (C) 1998 VMware, Inc. All rights reserved.
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
#if !defined(_WIN32) && !defined(N_PLAT_NLM)
#include <strings.h> /* For strncasecmp */
#endif
#include "vmware.h"
#include "strutil.h"
#include "str.h"



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
   token = (char *)malloc(length + 1 /* NUL */);
   ASSERT_MEM_ALLOC(token);
   memcpy(token, str + startIndex, length);
   token[length] = '\0';

   return token;
}


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
    * Input must be complete, no overflow, and value read must fit into 32 bits -
    * both signed and unsigned values are accepted.
    */
   return *ptr == '\0' && errno != ERANGE && (val == (int32)val || val == (uint32)val);
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
    * Input must be complete, no overflow, and value read must fit into 32 bits -
    * both signed and unsigned values are accepted.
    */
   return *ptr == '\0' && errno != ERANGE && (val == (uint32)val || val == (int32)val);
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
#elif defined(N_PLAT_NLM)
   /* Works for small values of str... */
   *out = (int64)strtol(str, &ptr, 0);
#else
   *out = strtoll(str, &ptr, 0);
#endif

   return ptr[0] == '\0' && errno != ERANGE;
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
#if defined VM_X86_64
   ASSERT_ON_COMPILE(sizeof *out == sizeof(uint64));
#   if defined(_WIN32)
   *out = _strtoui64(str, &ptr, 0);
#   elif defined(__FreeBSD__)
   *out = strtouq(str, &ptr, 0);
#   elif defined(N_PLAT_NLM)
   /* Works for small values of str... */
   *out = strtoul(str, &ptr, 0);
#   else
   *out = strtoull(str, &ptr, 0);
#   endif
#else
   ASSERT_ON_COMPILE(sizeof *out == sizeof(uint32));
   *out = strtoul(str, &ptr, 0);
#endif

   return *ptr == '\0' && errno != ERANGE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * StrUtil_FormatSizeInBytes --
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
    * These two functions should ideally share the basic functionality, and just
    * differ in the string localization
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
       const char* next;
       size_t len;

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
   return Str_Strncmp(s, prefix, strlen(prefix)) == 0;
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
 *      TRUE if string 'suffix' is found at the end of string 's', FALSE otherwise.
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

   ASSERT(s);
   ASSERT(suffix);

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
 * StrUtil_VDynBufPrintf --
 *
 *      This is a vprintf() variant which appends directly into a
 *      dynbuf. The dynbuf is not NUL-terminated: The printf() result
 *      is written immediately after the last byte in the DynBuf.
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
       * Is there any allocated-but-not-occupied space? If so,
       * try the printf. If there was no space to begin with, or
       * Str_Vsnprintf() ran out of space, this will fail.
       *
       * Note that we don't need to copy the argument list, it's
       * okay to iterate over the same va_list multiple times.
       * Str_Vsnprintf() already does a va_copy on platforms that
       * need it.
       */

      if (allocSize - size > 0) {
         i = Str_Vsnprintf((char*)DynBuf_Get(b) + size, allocSize - size, fmt, args);
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

         ASSERT(i + size == allocSize || ((char*)DynBuf_Get(b))[i + size] == '\0');
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
 *      A 'safe' variant of StrUtil_SafeDynBufPrintf(), which catches
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

   ASSERT_MEM_ALLOC(success);
}
