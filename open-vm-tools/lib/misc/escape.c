/*********************************************************
 * Copyright (C) 1998-2017,2020 VMware, Inc. All rights reserved.
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
 * escape.c --
 *
 *    Buffer escaping --hpreg
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "dynbuf.h"
#include "escape.h"


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
 * Escape_DoString --
 *
 *    Escape a buffer --hpreg
 *
 * Results:
 *    The escaped, allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_DoString(const char *escStr,    // IN
                int const *bytesToEsc, // IN
                void const *bufIn,     // IN
                size_t sizeIn,         // IN
                size_t *sizeOut)       // OUT/OPT
{
   char const *buf;
   DynBuf b;
   size_t startUnescaped;
   size_t index;
   size_t escStrLen;

   ASSERT(escStr);
   escStrLen = strlen(escStr);
   ASSERT(escStrLen > 0);

   ASSERT(bytesToEsc);
   /* Unsigned does matter --hpreg */
   ASSERT(bytesToEsc[(unsigned char)escStr[0]]);

   buf = (char const *)bufIn;
   ASSERT(buf);

   DynBuf_Init(&b);
   startUnescaped = 0;

   for (index = 0; index < sizeIn; index++) {
      /* Unsigned does matter --hpreg */
      unsigned char ubyte;
      char escSeq[2];

      ubyte = buf[index];
      if (bytesToEsc[ubyte]) {
         /* We must escape that byte --hpreg */

         escSeq[0] = Dec2Hex[ubyte >> 4];
         escSeq[1] = Dec2Hex[ubyte & 0xF];
         if (DynBuf_Append(&b, &buf[startUnescaped],
                           index - startUnescaped) == FALSE ||
             DynBuf_Append(&b, escStr, escStrLen) == FALSE ||
             DynBuf_Append(&b, escSeq, sizeof escSeq) == FALSE) {
            goto nem;
         }
         startUnescaped = index + 1;
      }
   }

   if (/* Last unescaped chunk (if any) --hpreg */
       DynBuf_Append(&b, &buf[startUnescaped],
                     index - startUnescaped) == FALSE ||
       /* NUL terminator --hpreg */
       DynBuf_Append(&b, "", 1) == FALSE ||
       DynBuf_Trim(&b) == FALSE) {
      goto nem;
   }

   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(&b) - 1;
   }

   return DynBuf_Get(&b);

nem:
   DynBuf_Destroy(&b);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_Do --
 *
 *    Escape a buffer
 *
 * Results:
 *    The escaped, allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_Do(char escByte,          // IN
          int const *bytesToEsc, // IN
          void const *bufIn,     // IN
          size_t sizeIn,         // IN
          size_t *sizeOut)       // OUT/OPT
{
   const char escStr[] = { escByte, '\0' };

   return Escape_DoString(escStr, bytesToEsc, bufIn, sizeIn, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_Undo --
 *
 *    Unescape a buffer --hpreg
 *
 * Results:
 *    The unescaped, allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_Undo(char escByte,      // IN
            void const *bufIn, // IN
            size_t sizeIn,     // IN
            size_t *sizeOut)   // OUT/OPT
{
   char const *buf;
   DynBuf b;
   unsigned int state;
   size_t startUnescaped;
   size_t index;
   int h = 0; /* Compiler warning --hpreg */
   int l;

   buf = (char const *)bufIn;
   ASSERT(buf);

   DynBuf_Init(&b);
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
            if (DynBuf_Append(&b, &buf[startUnescaped],
                              index - 2 - startUnescaped) == FALSE ||
                DynBuf_Append(&b, &escaped, 1) == FALSE) {
               goto nem;
            }
            startUnescaped = index + 1;
         }
         state = 0;
         break;

      default:
         NOT_IMPLEMENTED();
         break;
      }
   }

   if (/* Last unescaped chunk (if any) --hpreg */
       DynBuf_Append(&b, &buf[startUnescaped],
                     index - startUnescaped) == FALSE ||
       /* NUL terminator --hpreg */
       DynBuf_Append(&b, "", 1) == FALSE ||
       DynBuf_Trim(&b) == FALSE) {
      goto nem;
   }

   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(&b) - 1;
   }

   return DynBuf_Get(&b);

nem:
   DynBuf_Destroy(&b);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_UndoFixed --
 *
 *    Unescape a buffer into a fixed sized, preallocated buffer.
 *
 * Results:
 *    TRUE   The unescaped NUL terminated data is in bufOut upon success.
 *    FALSE  Escape or memory allocation failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
Escape_UndoFixed(char escByte,       // IN: escape byte
                 void const *bufIn,  // IN: string to be escaped
                 size_t bufInSize,   // IN: uint32 - fixed size for Python
                 void *bufOut,       // IN/OUT: preallocated output buffer
                 size_t bufOutSize)  // IN: uint32 - fixed size for Python
{
   Bool success;
   size_t sizeOut = 0;
   void *result = Escape_Undo(escByte, bufIn, bufInSize, &sizeOut);

   if (result == NULL) {
      success = FALSE;
   } else {
      size_t strLen = sizeOut + 1;  // Include NUL

      success = (strLen <= bufOutSize);

      if (success) {
         memcpy(bufOut, result, strLen);
      }

      free(result);
   }

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_AnsiToUnix --
 *
 *    Convert any occurrence of \r\n into \n --hpreg
 *
 * Results:
 *    The allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_AnsiToUnix(void const *bufIn, // IN
                  size_t sizeIn,     // IN
                  size_t *sizeOut)   // OUT/OPT
{
   char const *buf;
   DynBuf b;
   unsigned int state;
   size_t startUnescaped;
   size_t index;

   buf = (char const *)bufIn;
   ASSERT(buf);

   DynBuf_Init(&b);
   startUnescaped = 0;
   state = 0;

   /*
    * Identify all chunks in buf (\r\n being the chunk separator), and copy
    * them into b --hpreg
    */

   for (index = 0; index < sizeIn; index++) {
      char byte;

      byte = buf[index];
      switch (state) {
      case 1: /* Found \r<byte> --hpreg */
         state = 0;
         if (byte == '\n') {
            if (DynBuf_Append(&b, &buf[startUnescaped],
                              index - 1 - startUnescaped) == FALSE) {
               goto nem;
            }
            startUnescaped = index;
            break;
         }
         /* Fall through --hpreg */

      case 0: /* Found <byte> --hpreg */
         if (byte == '\r') {
            state = 1;
         }
         break;

      default:
         NOT_IMPLEMENTED();
         break;
      }
   }

   if (/* Last unescaped chunk (if any) --hpreg */
       DynBuf_Append(&b, &buf[startUnescaped],
                     index - startUnescaped) == FALSE ||
       /* NUL terminator --hpreg */
       DynBuf_Append(&b, "", 1) == FALSE ||
       DynBuf_Trim(&b) == FALSE) {
      goto nem;
   }

   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(&b) - 1;
   }

   return DynBuf_Get(&b);

nem:
   DynBuf_Destroy(&b);

   return NULL;
}


#if 0
/* Unit test suite for Escape_AnsiToUnix() --hpreg */
int
main(int argc,
     char **argv)
{
   static struct {
      const char *in;
      const char *out;
   } tests[] = {
      { "", "", },
      { "a", "a", },
      { "\ra", "\ra", },
      { "\na", "\na", },
      { "\r\na", "\na", },
      { "\n\ra", "\n\ra", },
      { "\r\r\na", "\r\na", },
      { "\r\na\r", "\na\r", },
      { "\r\na\r\n", "\na\n", },
   };
   unsigned int i;

   for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
      char *out;

      out = Escape_AnsiToUnix(tests[i].in, strlen(tests[i].in), NULL);
      if (strcmp(out, tests[i].out) != 0) {
         printf("test %u failed: %s\n", i, out);
         exit(1);
      }
      free(out);
   }

   printf("all tests passed\n");

   return 0;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_Sh --
 *
 *    Escape a buffer so that it can be passed verbatim as part of an argument
 *    on a shell command line.
 *
 * Results:
 *    The escaped, allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_Sh(void const *bufIn, // IN
          size_t sizeIn,     // IN
          size_t *sizeOut)   // OUT/OPT
{
   static const char be[] = { '\'', };
   static const char escSeq[] = { '\'', '"', '\'', '"', };
   char const *buf;
   DynBuf b;
   size_t startUnescaped;
   size_t index;

   buf = (char const *)bufIn;
   ASSERT(buf);

   DynBuf_Init(&b);

   if (DynBuf_Append(&b, be, sizeof(be)) == FALSE) {
      goto nem;
   }

   startUnescaped = 0;
   for (index = 0; index < sizeIn; index++) {
      if (buf[index] == '\'') {
         /* We must escape that byte --hpreg */

         if (DynBuf_Append(&b, &buf[startUnescaped],
                           index - startUnescaped) == FALSE ||
             DynBuf_Append(&b, escSeq, sizeof(escSeq)) == FALSE) {
            goto nem;
         }
         startUnescaped = index;
      }
   }

   if (/* Last unescaped chunk (if any) --hpreg */
       DynBuf_Append(&b, &buf[startUnescaped],
                     index - startUnescaped) == FALSE ||
       DynBuf_Append(&b, be, sizeof(be)) == FALSE ||
       /* NUL terminator --hpreg */
       DynBuf_Append(&b, "", 1) == FALSE ||
       DynBuf_Trim(&b) == FALSE) {
      goto nem;
   }

   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(&b) - 1;
   }

   return DynBuf_Get(&b);

nem:
   DynBuf_Destroy(&b);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_BRE --
 *
 *    Escape a buffer so that it can be passed verbatim as part of a Basic
 *    (a.k.a. obsolete) Regular Expression.
 *
 * Results:
 *    The escaped, allocated, NUL terminated buffer on success. If not NULL,
 *     '*sizeOut' contains the size of the buffer (excluding the NUL
 *     terminator)
 *    NULL on failure (not enough memory)
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void *
Escape_BRE(void const *bufIn, // IN
           size_t sizeIn,     // IN
           size_t *sizeOut)   // OUT/OPT
{
   static const char escByte = '\\';
   /* Escape ] [ ^ . * $ and 'escByte'. */
   static const int bytesToEsc[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   };
   char const *buf;
   DynBuf b;
   size_t startUnescaped;
   size_t index;

   /* Unsigned does matter. */
   ASSERT(bytesToEsc[(unsigned char)escByte]);

   buf = (char const *)bufIn;
   ASSERT(buf);

   DynBuf_Init(&b);
   startUnescaped = 0;

   for (index = 0; index < sizeIn; index++) {
      /* Unsigned does matter. */
      unsigned char ubyte;

      ubyte = buf[index];
      if (bytesToEsc[ubyte]) {
         /* We must escape that byte. */

         if (DynBuf_Append(&b, &buf[startUnescaped],
                           index - startUnescaped) == FALSE ||
             DynBuf_Append(&b, &escByte, sizeof escByte) == FALSE) {
            goto nem;
         }
         startUnescaped = index;
      }
   }

   if (/* Last unescaped chunk (if any). */
       DynBuf_Append(&b, &buf[startUnescaped],
                     index - startUnescaped) == FALSE ||
       /* NUL terminator. */
       DynBuf_Append(&b, "", 1) == FALSE ||
       DynBuf_Trim(&b) == FALSE) {
      goto nem;
   }

   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(&b) - 1;
   }

   return DynBuf_Get(&b);

nem:
   DynBuf_Destroy(&b);

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_StrChr --
 *
 *      Find the first occurence of c in bufIn that is not preceded by
 *      escByte.
 *
 *      XXX Doesn't handle recursive escaping, so <escByte><escByte><c>
 *          will skip that occurence of <c>
 *
 * Results:
 *      A pointer to the character, if found, NULL if not found.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

const char *
Escape_Strchr(char escByte,      // IN
              const char *bufIn, // IN
              char c)            // IN
{
   size_t i;
   Bool escaped = FALSE;

   ASSERT(escByte != c);
   ASSERT(bufIn);

   for (i = 0; bufIn[i] != '\0'; i++) {
      if (escaped) {
         escaped = FALSE;
      } else {
         if (bufIn[i] == c) {
            return &bufIn[i];
         }

         if (bufIn[i] == escByte) {
            escaped = TRUE;
         }
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_Unescape --
 *
 *      Removes all occurences of an escape character from a string, unless
 *      the occurence is escaped.
 *
 * Results:
 *      An allocated string.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Escape_Unescape(char escByte,       // IN
                const char *bufIn)  // IN
{
   DynBuf result;
   Bool escaped = FALSE;
   char nulByte = '\0';
   int i;

   ASSERT(bufIn);

   DynBuf_Init(&result);

   for (i = 0; bufIn[i]; i++) {
      if (bufIn[i] != escByte || escaped) {
         DynBuf_Append(&result, &(bufIn[i]), sizeof(char));
         escaped = FALSE;
      } else {
         escaped = TRUE;
      }
   }

   DynBuf_Append(&result, &nulByte, sizeof nulByte);

   return DynBuf_Get(&result);
}


/*
 *----------------------------------------------------------------------------
 *
 * Escape_UnescapeCString --
 *
 *    Unescapes a C string that has been escaped using a '\n' -> "\n" or
 *    ' ' -> "\040" type conversion.  The former is a standard '\' escaping,
 *    while the latter is an octal representation escaping.  The unescaping is
 *    done in-place within the provided buffer.
 *
 *    This function assumes the string is NUL-terminated.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
Escape_UnescapeCString(char *buf) // IN/OUT
{
   uint32 read = 0;
   uint32 write = 0;

   ASSERT(buf);

   while (buf[read] != '\0') {
      if (buf[read] == '\\') {
         uint32 val;

         if (buf[read + 1] == 'n') {
            buf[write] = '\n';
            read++;
         } else if (buf[read + 1] == '\\') {
            buf[write] = '\\';
            read++;
         } else if (sscanf(&buf[read], "\\%03o", &val) == 1) {
            buf[write] = (char)val;
            read += 3;
         } else {
            buf[write] = buf[read];
         }
      } else {
         buf[write] = buf[read];
      }

      read++;
      write++;
   }
   buf[write] = '\0';
}


#if 0
/* Unit test suite for Escape_Sh() --hpreg */
int
main(int argc,
     char **argv)
{
   static struct {
      const char *in;
      const char *out;
   } tests[] = {
      { "", "''", },
      { "a", "'a'", },
      { "'a", "''\"'\"'a'", },
      { "'a'", "''\"'\"'a'\"'\"''", },
      { "a'a", "'a'\"'\"'a'", },
   };
   unsigned int i;

   for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
      char *out;

      out = Escape_Sh(tests[i].in, strlen(tests[i].in), NULL);
      if (strcmp(out, tests[i].out) != 0) {
         printf("test %u failed: %s\n", i, out);
         exit(1);
      }
      free(out);
   }

   printf("all tests passed\n");

   return 0;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Escape_Comma --
 *
 *       Use backslash character as an escape character to handle comma
 *       character escaping.
 *
 * Results:
 *       Returns a newly allocated string with comma and backslash characters
 *       escaped.
 *
 * Side effects:
 *       None.  It is a caller responsibility to deallocate the result.
 *
 *-----------------------------------------------------------------------------
 */

char *
Escape_Comma(const char *string) // IN
{
   DynBuf b;

   if (string == NULL) {
      return NULL;
   }

   DynBuf_Init(&b);

   for (; *string; ++string) {
      char c = *string;

      if (c == ',' || c == '\\') {
         if (!DynBuf_Append(&b, "\\", 1)) {
            goto out_of_memory;
         }
      }
      if (!DynBuf_Append(&b, string, 1)) {
         goto out_of_memory;
      }
   }

   DynBuf_Append(&b, string, 1);

   return DynBuf_Get(&b);

out_of_memory:
   DynBuf_Destroy(&b);
   return NULL;
}


#if 0
/* Unit test suite for Escape_Comma() */
int
main(int argc,
     char **argv)
{
   static struct {
      const char *in;
      const char *out;
   } tests[] = {
      { "123# ", "123# ", },
      { "123,", "123\\,", },
      { "'123\\", "'123\\\\", },
   };
   unsigned int i;

   for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
      char *out;

      out = Escape_Comma(tests[i].in);
      if (strcmp(out, tests[i].out) != 0) {
         printf("test %u failed: \"%s\" : \"%s\"\n", i, tests[i].in, out);
         exit(1);
      }
      free(out);
   }

   printf("all tests passed\n");

   return 0;
}

#endif

