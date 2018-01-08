/*********************************************************
 * Copyright (C) 1998-2017 VMware, Inc. All rights reserved.
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
 * dictll.c --
 *
 *    Low-level dictionary format --hpreg
 */


#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vmware.h"
#include "vmstdio.h"
#include "escape.h"
#include "dictll.h"
#include "util.h"

#define UTF8_BOM "\xEF\xBB\xBF"


/* Duplicate a buffer --hpreg. The result is NUL-terminated */
static void *
BufDup(void const * const bufIn,  // IN: buffer to duplicate
       unsigned int const sizeIn) // IN: buffer size in bytes
{
   char *bufOut;

   ASSERT(bufIn);

   bufOut = Util_SafeMalloc(sizeIn + 1);
   memcpy(bufOut, bufIn, sizeIn);
   bufOut[sizeIn] = '\0';

   return bufOut;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Walk --
 *
 *    While 'bufIn' points to a byte in 'sentinel', increment it --hpreg
 *
 * Note:
 *    If your 'bufIn' is a NUL-terminated C string, you should rather make sure
 *    that the NUL byte is not in your 'sentinel'
 *
 * Results:
 *    The new 'buf'
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void const *
Walk(void const * const bufIn,   // IN
     int const * const sentinel) // IN
{
   char const *buf;

   buf = (char const *)bufIn;
   ASSERT(buf);

   /* Unsigned does matter --hpreg */
   while (sentinel[(unsigned char)*buf]) {
      buf++;
   }

   return buf;
}

/*
 * XXX Document the escaping/unescaping process: rationale for which chars we
 *     escape, and how we escape --hpreg
 *
 * The dictionary line format:
 *
 *    <ws> <name> <ws> = <ws> <value> <ws> <comment>
 * or
 *    <ws> <name> <ws> = <ws> " <quoted-value> " <ws> <comment>
 * or
 *    <ws> <name> <ws> = <ws> <comment> (Implied value of empty string)
 * or
 *    <ws> <comment>
 *
 * where
 *    <name> does not contain any whitespace or = or #
 *    <value> does not contain any double-quote or #
 *    <quoted-value> does not contain any double-quote
 *    <comment> begins with # and ends at the end of the line
 *    <ws> is a sequence spaces and/or tabs
 *    <comment> and <ws> are optional
 */


/*
 *-----------------------------------------------------------------------------
 *
 * DictLL_UnmarshalLine --
 *
 *      Reads a line from the bufSize-byte buffer buf, which holds one or more
 *      new-line delimited lines.  The buffer is not necessarily
 *      NUL-terminated.
 *
 * Results:
 *      The beginning of the next line if a line was successfully parsed.  In
 *      that case, '*line' is the allocated line.  If the line is well-formed,
 *      then '*name' and '*value' are allocated strings. Otherwise they are
 *      both NULL.
 *
 *      A null pointer at the end of the buffer, in which case *line, *name,
 *      and *value are set to null pointers.
 *
 * Side effects:
 *      Advances *buf to the beginning of the next line.
 *
 *-----------------------------------------------------------------------------
 */

const char *
DictLL_UnmarshalLine(const char *buf,   // IN: buffer to parse
                     size_t bufSize,    // IN: buffer size in bytes
                     char **line,       // OUT: malloc()'d entire line
                     char **name,       // OUT: malloc()'d name or NULL
                     char **value)      // OUT: malloc()'d value or NULL
{
   /* Space and tab --hpreg */
   static int const ws_in[] = {
      0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
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
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
   };
   /* Everything but NUL, space, tab and pound --hpreg */
   static int const wsp_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   /* Everything but NUL, space, tab, pound and equal --hpreg */
   static int const wspe_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   /* Everything but NUL and double quote --hpreg */
   static int const q_out[] = {
      0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
   };
   char const *nBegin;
   char const *nEnd;
   char const *vBegin;
   char const *vEnd;
   char const *tmp;
   char *myLine;
   char *myName;
   char *myValue;
   const char *lineEnd;
   const char *nextLine;

   ASSERT(buf);
   ASSERT(line);
   ASSERT(name);
   ASSERT(value);

   /* Check for end of buffer. */
   if (bufSize == 0) {
      *line = NULL;
      *name = NULL;
      *value = NULL;
      return NULL;
   }

   /* Find end of this line, beginning of next. */
   lineEnd = memchr(buf, '\n', bufSize);
   if (lineEnd != NULL) {
      nextLine = lineEnd + 1;
   } else {
      nextLine = lineEnd = buf + bufSize;
   }

   /* Make local copy of line. */
   myLine = BufDup(buf, lineEnd - buf);

   /* Check if the line is well-formed --hpreg */
   nBegin = Walk(myLine, ws_in);
   nEnd = Walk(nBegin, wspe_out);
   tmp = Walk(nEnd, ws_in);
   if (nBegin == nEnd || *tmp != '=') {
      goto weird;
   }
   tmp++;
   tmp = Walk(tmp, ws_in);
   if (*tmp == '"') {
      tmp++;
      vBegin = tmp;
      vEnd = Walk(vBegin, q_out);
      tmp = vEnd;
      if (*tmp != '"') {
         goto weird;
      }
      tmp++;
   } else {
      vBegin = tmp;
      vEnd = Walk(vBegin, wsp_out);
      tmp = vEnd;
   }
   tmp = Walk(tmp, ws_in);
   if (*tmp != '\0' && *tmp != '#') {
      goto weird;
   }

   /* The line is well-formed. Extract the name and value --hpreg */

   myName = BufDup(nBegin, nEnd - nBegin);
   myValue = Escape_Undo('|', vBegin, vEnd - vBegin, NULL);
   VERIFY(myValue);

   *line = myLine;
   *name = myName;
   *value = myValue;

   return nextLine;

weird:
   /* The line is not well-formed. Let the upper layers handle it --hpreg */

   *line = myLine;
   *name = NULL;
   *value = NULL;

   return nextLine;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DictLL_ReadLine --
 *
 *    Read the next line from a dictionary file --hpreg
 *
 * Results:
 *    2 on success: '*line' is the allocated line
 *                  If the line is well-formed, then '*name' and '*value' are
 *                  allocated strings. Otherwise they are both NULL.
 *    1 if there is no next line (end of stream)
 *    0 on failure: errno is set accordingly
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */
 
int
DictLL_ReadLine(FILE *stream, // IN: stream to read
                char **line,  // OUT: malloc()'d line or null pointer
                char **name,  // OUT: malloc()'d name or null pointer
                char **value) // OUT: malloc()'d value or null pointer
{
   char *myLine;
   size_t myLineLen;

   ASSERT(stream);
   ASSERT(line);
   ASSERT(name);
   ASSERT(value);

   *line = NULL;
   *name = NULL;
   *value = NULL;

   switch (StdIO_ReadNextLine(stream, &myLine, 0, &myLineLen)) {
   case StdIO_Error:
      return 0;

   case StdIO_EOF:
      return 1;

   case StdIO_Success:
      if (DictLL_UnmarshalLine(myLine, myLineLen,
                               line, name, value) == NULL) {
         *line = BufDup("", 0);
      }
      free(myLine);
      return 2;

   default:
      NOT_IMPLEMENTED();
   }
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * DictLL_MarshalLine --
 *
 *    Marshals a line, appending the data to a DynBuf.  If 'name' is NULL,
 *    '*value' contains the whole line to write verbatim.  Otherwise a proper
 *    name/value pair is written.
 *
 * Results:
 *    TRUE on success, FALSE on failure (caused by memory allocation failure).
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DictLL_MarshalLine(DynBuf *output,    // IN/OUT: output buffer
                   char const *name,  // IN/OPT: name to marshal
                   char const *value) // IN: value to marshal
{
   size_t size;

   if (name) {
      /*
       * Double quote, pipe, 0x7F, and all control characters but
       * tab --hpreg
       * 0x80 to 0xff are unescaped so characters in encodings
       * like UTF-8 will be displayed normally.
       */
      static int const toEscape[] = {
         1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1,
         1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
         0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      };
      char *evalue;

      /* Write a well-formed line --hpreg */

      evalue = Escape_Do('|', toEscape, value, (uint32)strlen(value), &size);
      if (   !DynBuf_Append(output, name, (uint32)strlen(name))
          || !DynBuf_Append(output, " = \"", 4)
          || (size && !DynBuf_Append(output, evalue, size))
          || !DynBuf_Append(output, "\"", 1)) {
         free(evalue);

         return FALSE;
      }
      free(evalue);
   } else {
      /* Write the line as passed from the upper layers --hpreg */

      size = (uint32)strlen(value);
      if (size && !DynBuf_Append(output, value, size)) {
         return FALSE;
      }
   }

   /*
    * Win32 takes care of adding the \r (XXX this assumes that the stream
    * is opened in ascii mode) --hpreg
    */
   if (!DynBuf_Append(output, "\n", 1)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DictLL_WriteLine --
 *
 *    Marshals a line, writing the data to a file.  If 'name' is NULL, '*value'
 *    contains the whole line to write verbatim.  Otherwise a proper name/value
 *    pair is written.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure: errno is set accordingly
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
DictLL_WriteLine(FILE *stream,      // IN: stream to write
                 char const *name,  // IN: name to write
                 char const *value) // IN: value to write
{
   DynBuf buf;

   DynBuf_Init(&buf);
   if (!DictLL_MarshalLine(&buf, name, value)) {
      DynBuf_Destroy(&buf);
      errno = ENOMEM;
      return FALSE;
   }
   if (fwrite(DynBuf_Get(&buf), DynBuf_GetSize(&buf), 1, stream) != 1) {
      DynBuf_Destroy(&buf);
      return FALSE;
   }
   DynBuf_Destroy(&buf);
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DictLL_ReadUTF8BOM --
 *
 *    Reads a UTF-8 BOM from a file.
 *
 * Results:
 *    If successful, returns TRUE and updates the file position.
 *    Returns FALSE if a UTF-8 BOM was not found.
 *
 * Side effects:
 *    Might clears the error indicator of the file stream.
 *
 *-----------------------------------------------------------------------------
 */

Bool
DictLL_ReadUTF8BOM(FILE *file) // IN/OUT
{
   Bool found;

   // sizeof on a string literal counts NUL.  Exclude it.
   char buf[sizeof UTF8_BOM - 1] = { 0 };

   if (file == stdin) {
      return FALSE;
   }

   found =    fread(buf, sizeof buf, 1, file) == 1
           && memcmp(UTF8_BOM, buf, sizeof buf) == 0;

   if (!found) {
      rewind(file);
   }

   return found;
}
