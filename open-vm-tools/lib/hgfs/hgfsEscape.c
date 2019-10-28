/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * hgfsEscape.c --
 *
 *    Escape and unescape illegal filenames for different platforms.
 *
 */

#ifdef __KERNEL__
#  include "driver-config.h"
#  include <linux/string.h>
#elif defined __FreeBSD__
#   if defined _KERNEL
#      include <sys/libkern.h>
#      define strchr(s,c)       index(s,c)
#   else
#      include <string.h>
#   endif
#   define memmove(s1,s2,n) bcopy(s2,s1,n)
#elif defined __APPLE__ && defined KERNEL
#  include <string.h>
#elif !defined sun
#  include <stdlib.h>
#  include <string.h>
#else
#  include <string.h>
#endif

#include "vmware.h"
#include "hgfsEscape.h"
#include "cpName.h"

#ifdef _WIN32

#define UNREFERENCED_PARAMETER(P) (P)

/* These characters are illegal in Windows file names. */
const char* HGFS_ILLEGAL_CHARS = "/\\*?:\"<>|";
const char* HGFS_SUBSTITUTE_CHARS = "!@#$^&(){";
/* Last character of a file name in Windows can be neither dot nor space. */
const char* HGFS_ILLEGAL_LAST_CHARS = ". ";

/* http://msdn.microsoft.com/en-us/library/aa365247.aspx */
char *HgfsReservedNames[] = {"CON", "PRN", "AUX", "NUL"};
char *HgfsReservedNamesWithNumber[] = {"COM", "LPT"};

#define HGFS_RESERVED_NAME_CHARS_LENGTH 3
#define HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH (HGFS_RESERVED_NAME_CHARS_LENGTH + 1)
/* Check for special escaping cases - reserved names and illegal last characters. */
#define IS_SPECIAL_CASE_ESCAPE(b,o,l) HgfsIsSpecialCaseEscape(b,o,l)
/* Process Windows reserved names. */
#define PROCESS_RESERVED_NAME(b,s,p,o,c) \
if (!HgfsProcessReservedName(b,s,p,o,c)) \
{ \
 return FALSE; \
}
/* Process Windows reserved names. */
#define PROCESS_LAST_CHARACTER(b,s,p,c) \
if (!HgfsProcessLastCharacter(b,s,p,c)) \
{ \
 return FALSE; \
}

#else // _WIN32

#define UNREFERENCED_PARAMETER(P)
/* There is no special escape sequences on other than Windows platforms. */
#define IS_SPECIAL_CASE_ESCAPE(b,o,l) FALSE
/* There is no reserved names on other then Windows platforms. */
#define PROCESS_RESERVED_NAME(b,s,p,o,c)
/* There is no special processing for the last character on non-Windows platforms. */
#define PROCESS_LAST_CHARACTER(b,s,p,c)

#if defined __APPLE__
/* These characters are illegal in MAC OS file names. */
const char* HGFS_ILLEGAL_CHARS = "/:";
const char* HGFS_SUBSTITUTE_CHARS = "!&";
#else   // __APPLE__
/* These characters are illegal in Linux file names. */
const char* HGFS_ILLEGAL_CHARS = "/";
const char* HGFS_SUBSTITUTE_CHARS = "!";
#endif  // __APPLE__

#endif  // _WIN32

#define HGFS_ESCAPE_CHAR '%'
#define HGFS_ESCAPE_SUBSTITUE_CHAR ']'

typedef enum {
   HGFS_ESCAPE_ILLEGAL_CHARACTER,
   HGFS_ESCAPE_RESERVED_NAME,
   HGFS_ESCAPE_ILLEGAL_LAST_CHARACTER,
   HGFS_ESCAPE_ESCAPE_SEQUENCE,
   HGFS_ESCAPE_COMPLETE
} HgfsEscapeReason;


typedef Bool (*HgfsEnumCallback)(char const *bufIn,
                                 uint32 offset,
                                 HgfsEscapeReason reason,
                                 void* context);

/*
 * The structure is used by HgfsAddEscapeCharacter to keep context information between
 * invocations
 * All offsets defined in this structure are in characters, not bytes
 */
typedef  struct {
   uint32   processedOffset;     // Offset of the first unprocessed input character
   uint32   outputBufferLength;  // Number of characters in the output buffer
   uint32   outputOffset;        // Number of characters that are already in the output
   char    *outputBuffer;        // Pointer to the output buffer
} HgfsEscapeContext;

static void HgfsEscapeUndoComponent(char *bufIn, uint32 *totalLength);
static int HgfsEscapeGetComponentSize(char const *bufIn, uint32 sizeIn);
static int HgfsEscapeDoComponent(char const *bufIn, uint32 sizeIn, uint32 sizeBufOut,
                                 char *bufOut);

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsAddEscapeCharacter --
 *
 *    Callback function that is called by HgfsEnumerate to insert an escape sequence
 *    into the input name.
 *
 * Results:
 *    TRUE if successful, FALSE if there is an error like the output buffer is
 *    too small.
 *
 * Side effects:
 *    Updates the output buffer pointer (stored in the context variable).
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsAddEscapeCharacter(char const * bufIn,      // IN: input name
                       uint32 offset,           // IN: offset that requires escaping
                       HgfsEscapeReason reason, // IN: reason for esaping
                       void *context)           // IN/OUT: convertion context
{
   HgfsEscapeContext *escapeContext = (HgfsEscapeContext *)context;
   uint32 charactersToCopy;
   uint32 outputSpace;
   char* illegal;
   Bool result = TRUE;

   ASSERT(offset >= escapeContext->processedOffset); // Scanning forward
   charactersToCopy = offset - escapeContext->processedOffset;

   if (escapeContext->outputOffset + charactersToCopy >
       escapeContext->outputBufferLength) {
      return FALSE;
   }

   memcpy(escapeContext->outputBuffer + escapeContext->outputOffset,
          bufIn + escapeContext->processedOffset, charactersToCopy * sizeof *bufIn);
   escapeContext->outputOffset += charactersToCopy;
   escapeContext->processedOffset += charactersToCopy;

   outputSpace = escapeContext->outputBufferLength - escapeContext->outputOffset;

   switch(reason) {
   case HGFS_ESCAPE_ILLEGAL_CHARACTER:
      if (outputSpace < 2) {
         return FALSE;
      }
      illegal = strchr(HGFS_ILLEGAL_CHARS, bufIn[escapeContext->processedOffset]);
      escapeContext->processedOffset++;  // Skip illegal input character
      ASSERT(illegal != NULL);
      escapeContext->outputBuffer[escapeContext->outputOffset] =
         HGFS_SUBSTITUTE_CHARS[illegal - HGFS_ILLEGAL_CHARS];
      escapeContext->outputOffset++;
      escapeContext->outputBuffer[escapeContext->outputOffset] = HGFS_ESCAPE_CHAR;
      escapeContext->outputOffset++;
      break;

   case HGFS_ESCAPE_RESERVED_NAME:
      if (outputSpace < 1) {
         return FALSE;
      }
      escapeContext->outputBuffer[escapeContext->outputOffset] = HGFS_ESCAPE_CHAR;
      escapeContext->outputOffset++;
      break;

   case HGFS_ESCAPE_ILLEGAL_LAST_CHARACTER:
      if (outputSpace < 1) {
         return FALSE;
      }
      escapeContext->outputBuffer[escapeContext->outputOffset] = HGFS_ESCAPE_CHAR;
      escapeContext->outputOffset++;
      break;

   case HGFS_ESCAPE_ESCAPE_SEQUENCE:
      if (outputSpace < 2) {
         return FALSE;
      }
      escapeContext->processedOffset++; // Skip input esape character
      escapeContext->outputBuffer[escapeContext->outputOffset] = HGFS_ESCAPE_SUBSTITUE_CHAR;
      escapeContext->outputOffset++;
      escapeContext->outputBuffer[escapeContext->outputOffset] = HGFS_ESCAPE_CHAR;
      escapeContext->outputOffset++;
      break;

   case HGFS_ESCAPE_COMPLETE:
      if (outputSpace < 1) {
         return FALSE;
      }
      escapeContext->outputBuffer[escapeContext->outputOffset] = '\0';
      break;

   default:
      result = FALSE;
      ASSERT(FALSE);
   }
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsCountEscapeChars --
 *
 *    Callback function that is called by HgfsEnumerate to count additional characters
 *    that need to be inserted in the input name.
 *
 * Results:
 *    TRUE since it never fails.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsCountEscapeChars(char const *bufIn,       // IN: input name
                     uint32 offset,           // IN: offset where escape is needed
                     HgfsEscapeReason reason, // IN: reason for escaping
                     void *context)           // IN/OUT: context info
{
   UNREFERENCED_PARAMETER(bufIn);
   UNREFERENCED_PARAMETER(offset);
   if (reason != HGFS_ESCAPE_COMPLETE) {
      uint32 *counter = (uint32*)context;
      (*counter)++;
   }
   return TRUE;
}


#ifdef _WIN32
/*
 *-----------------------------------------------------------------------------
 *
 * HgfsLetterToUpper --
 *
 *    Converts lowercase English letters to uppercase.
 *    If the symbol is not a lowercase English letter returns the original character.
 *
 * Results:
 *    Converted character.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char
HgfsLetterToUpper(char letter)
{
   if (letter >= 'a' && letter <= 'z') {
      return letter - ('a' - 'A');
   }
   return letter;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsEqualPrefix --
 *
 *    Verifies if the string prefix is equal to the given prefix.
 *    It assumes that the prefix includes only uppercase English letters or numbers
 *    and it does not have any international characters.
 *    The string must be either NULL terminated or not shorter then the prefix.
 *
 * Results:
 *    TRUE if the uppcased string starts with the given prefix. False otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsEqualPrefix(char const *prefix,  // IN: prefix to check
                  char const *string,  // IN: input string
                  uint32 prefixLength) // IN: length of the prefix in characters
{
   int i;
   for (i = 0; i < prefixLength; i++) {
      ASSERT(prefix[i] > 0 && (prefix[i] < 'a' || prefix[i] > 'z' ));
      if (prefix[i] != HgfsLetterToUpper(string[i])) {
         return FALSE;
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsReservedPrefix --
 *
 *    Verifies if the name's prefix is one of the reserved names.
 *
 * Results:
 *    TRUE if the name's prefix is one of the reserved names.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsReservedPrefix(char const *bufIn)  // IN: input name
{
   uint32 i;
   for (i = 0; i < ARRAYSIZE(HgfsReservedNames); i++) {
      if (HgfsIsEqualPrefix(HgfsReservedNames[i], bufIn,
                            HGFS_RESERVED_NAME_CHARS_LENGTH)) {
         return TRUE;
      }
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsReservedPrefixWithNumber --
 *
 *    Verifies if the name's prefix is one of the reserved names with number:
 *    COM1-9 or LPT1-9.
 *
 * Results:
 *    TRUE if the name's prefix is one of the reserved names with number.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsReservedPrefixWithNumber(char const *bufIn)   // IN: input name
{
   uint32 i;
   for (i = 0; i < ARRAYSIZE(HgfsReservedNamesWithNumber); i++) {
      if (HgfsIsEqualPrefix(HgfsReservedNamesWithNumber[i], bufIn,
                            HGFS_RESERVED_NAME_CHARS_LENGTH) &&
          bufIn[HGFS_RESERVED_NAME_CHARS_LENGTH] >= '1' &&
          bufIn[HGFS_RESERVED_NAME_CHARS_LENGTH] <= '9') {
         return TRUE;
      }
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsSpecialCaseEscape --
 *
 *    Verifies if the escape character is a part of special case escape sequence
 *    that exists only in Windows - escaped reserved name or escaped illegal last
 *    character.
 *
 * Results:
 *    TRUE if the name's prefix is one of the reserved names with number.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsSpecialCaseEscape(char const *bufIn,   // IN: input name
                        uint32 offset,       // IN: offset of the escape character
                        uint32 length)       // IN: length of the name in characters
{
   if (offset + 1 == length &&
      strchr(HGFS_ILLEGAL_LAST_CHARS, bufIn[offset - 1]) != NULL) {
      return TRUE;
   }
   if (offset == HGFS_RESERVED_NAME_CHARS_LENGTH &&
      (length == HGFS_RESERVED_NAME_CHARS_LENGTH + 1 || bufIn[offset+1] == '.')) {
      return HgfsIsReservedPrefix(bufIn);
   }
   if (offset == HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH &&
      (length == HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH + 1 ||
      bufIn[offset+1] == '.')) {
      return HgfsIsReservedPrefixWithNumber(bufIn);
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsProcessReservedName --
 *
 *    Verifies if the name is one of reserved Windows file names.
 *    If it is a reserved name invokes callback that performs required
 *    processing.
 *
 * Results:
 *    TRUE if no processing is required of if processing succeeded,
 *    FALSE if processing failed.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsProcessReservedName(char const *bufIn,              // IN:  Unescaped input buffer
                        uint32 sizeIn,                  // IN:  Length of the input
                        HgfsEnumCallback processEscape, // IN:  Callack that is invoked
                                                        //      if input is reserved name
                        uint32 *offset,                 // OUT: New offset in the input
                        void *context)                  // IN/OUT: Context for callback
{
   /*  Look reserved names: CON, PRN, AUX, NUL. */
   if (sizeIn >= HGFS_RESERVED_NAME_CHARS_LENGTH && HgfsIsReservedPrefix(bufIn)) {
      if (HGFS_RESERVED_NAME_CHARS_LENGTH == sizeIn ||
         bufIn[HGFS_RESERVED_NAME_CHARS_LENGTH] == '.') {
         if (!processEscape(bufIn, HGFS_RESERVED_NAME_CHARS_LENGTH,
                            HGFS_ESCAPE_RESERVED_NAME, context)) {
            return FALSE;
         }
         *offset = HGFS_RESERVED_NAME_CHARS_LENGTH;
      }
   }

   /*  Look reserved names with numbers: COM1-9 and LPT1-9. */
   if (sizeIn >= HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH &&
       HgfsIsReservedPrefixWithNumber(bufIn)) {
      if (HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH == sizeIn ||
         bufIn[HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH] == '.') {
         if (!processEscape(bufIn, HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH,
                            HGFS_ESCAPE_RESERVED_NAME, context)) {
            return FALSE;
         }
         *offset = HGFS_RESERVED_NAME_WITH_NUMBER_CHARS_LENGTH;
      }
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsProcessLastCharacter --
 *
 *    Verifies if the trailing character in the name is a valid last character.
 *    In Windows it is illegal to have a file name that ends with dot ('.') or
 *    space (' '). The only exception is "." and ".." directory names.
 *    If the last character is invalid the function invokes a callback to process it.
 *
 * Results:
 *    TRUE if no processing is required of if processing succeeded,
 *    FALSE if processing failed.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsProcessLastCharacter(char const *bufIn,              // IN:  Unescaped input buffer
                         uint32 sizeIn,                  // IN:  Length of the input
                         HgfsEnumCallback processEscape, // IN:  Callack that is invoked
                                                         //      when escaping is needed
                         void *context)                  // IN/OUT: Callback context
{

   /* If the filename is '.' or '..' we shouldn't escape it. */
   if ((sizeIn == 1 && bufIn[0] == '.') ||
       (sizeIn == 2 && bufIn[0] == '.' && bufIn[1] == '.')) {
      return TRUE;
   }

   /* Invoke the callback if the last character is illegal. */
   if (strchr(HGFS_ILLEGAL_LAST_CHARS, bufIn[sizeIn - 1]) != NULL) {
      if (!processEscape(bufIn, sizeIn, HGFS_ESCAPE_ILLEGAL_LAST_CHARACTER, context)) {
         return FALSE;
      }
   }
   return TRUE;
}

#endif //  WIN32


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsIsEscapeSequence --
 *
 *    Verifies if input buffer has an escape sequence at the position
 *    defined by offset.
 *
 * Results:
 *    TRUE if there is an escape sequence at the position defined by offset.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsIsEscapeSequence(char const *bufIn,   // IN: input name
                     uint32 offset,       // IN: offset of the escape character
                     uint32 length)       // IN: length of the name in characters
{
   if (bufIn[offset] == HGFS_ESCAPE_CHAR && offset > 0) {
      char *substitute;
      if (bufIn[offset - 1] == HGFS_ESCAPE_SUBSTITUE_CHAR && offset > 1) {
         /*
          * Possibly a valid sequence, check it must be preceded with a substitute
          * character or another escape-escape character. Otherwise, HGFS did
          * not generate this sequence and should leave it alone.
          */
         if (bufIn[offset - 2] == HGFS_ESCAPE_SUBSTITUE_CHAR) {
            return TRUE;
         }
         substitute = strchr(HGFS_SUBSTITUTE_CHARS, bufIn[offset - 2]);
         if (substitute != NULL) {
            return TRUE;
         }
      }
      substitute = strchr(HGFS_SUBSTITUTE_CHARS, bufIn[offset - 1]);
      if (substitute != NULL) {
         return TRUE;
      }
      return IS_SPECIAL_CASE_ESCAPE(bufIn,offset,length);
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscapeEnumerate --
 *
 *    The function scans the input buffer and calls processEscape callback for every
 *    place in the input buffer which require escaping.
 *
 *    Callback does the required processing. There are two different callbacks -
 *    one counts extra symbols that are needed for escaping and another produces
 *    escaped output name based on input name.
 *
 *    1. The first function calculates number of extra characters. It just increments
 *    a counter which is passed to it in context variable every time it is called
 *    for the reason different from "complete processing" assuming that
 *    exactly one extra  character is required to escape any invalid input.
 *
 *    2. The second function produces output name by copying everything from input
 *    name into the output name up to the place which require escaping and
 *    then inserts appropriate escape sequence into the output. It keeps track of its
 *    progress and keeps pointer to the output buffer in the context variable.
 *    HgfsEscapeEnumerate calls calback function one more time at the end of the
 *    input buffer to let callback finish processing of the input (for example copy
 *    the rest of the name after the last escape sequence from input buffer to
 *    output buffer).
 *
 * Results:
 *    TRUE if the input has been processed successfully by the callback, false otherwise.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
HgfsEscapeEnumerate(char const *bufIn,              // IN:  Buffer with unescaped input
                    uint32 sizeIn,                  // IN:  Number of input *characters*
                    HgfsEnumCallback processEscape, // IN: Callack that is invoked every
                                                    //     time escaping is required
                    void *context)                  // IN/OUT: Context for processEscape
{
   /*  First look for invalid characters in the input name. */
   uint32 i, offset = 0;
   if (sizeIn == 0) {
      return TRUE;
   }

   ASSERT(processEscape);

   PROCESS_RESERVED_NAME(bufIn, sizeIn, processEscape, &offset, context);

   for (i = offset; i < sizeIn; i++) {
      if (strchr(HGFS_ILLEGAL_CHARS, bufIn[i]) != NULL) {
         if (!processEscape(bufIn, i, HGFS_ESCAPE_ILLEGAL_CHARACTER, context)) {
            return FALSE;
         }
      } else if (HgfsIsEscapeSequence(bufIn, i, sizeIn)) {
         if (!processEscape(bufIn, i, HGFS_ESCAPE_ESCAPE_SEQUENCE, context)) {
            return FALSE;
         }
      }
   }

   PROCESS_LAST_CHARACTER(bufIn, sizeIn, processEscape, context);

   if (!processEscape(bufIn, sizeIn, HGFS_ESCAPE_COMPLETE, context)) {
      return FALSE;
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscape_Do --
 *
 *    Escape any characters that are not legal in a windows filename.
 *    Escape reserved file names that can't be used in Windows.
 *    We also of course have to escape the escape character, which is "%",
 *    when it is part of a character sequence that would require unescaping
 *
 *    sizeBufOut must account for the NUL terminator.
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
              uint32 sizeIn,     // IN:  Size of input buffer
              uint32 sizeBufOut, // IN:  Size of output buffer
              char *bufOut)      // OUT: Buffer for escaped output
{
   const char *currentComponent = bufIn;
   uint32 sizeLeft = sizeBufOut;
   char *outPointer = bufOut;
   const char *end = bufIn + sizeIn;
   const char *next;
   ASSERT(sizeIn > 0);
   if (bufIn[sizeIn - 1] == '\0') {
      /*
       * In some cases a NUL terminated string is passed to HgfsEscape_Do
       * so it make sense to support such input even if CPName_GetComponent
       * does not. Detect this case and make the input compliant with
       * CPName_GetComponent by removing terminating NUL.
       */
      end--;
      sizeIn--;
   }
   /*
    * Absolute symbolic link name starts with the '\0'. HgfsEscapeDo needs to work
    * with such names. Leading NULL symbols should be skipped here since
    * CPName_GetComponent does not support such names.
    */
   while (*currentComponent == '\0' && currentComponent - bufIn < sizeIn) {
      currentComponent++;
      sizeLeft--;
      *outPointer++ = '\0';
   }
   while (currentComponent - bufIn < sizeIn) {
      int escapedLength;
      int componentSize = CPName_GetComponent(currentComponent, end, &next);
      if (componentSize < 0) {
         return componentSize;
      }

      escapedLength = HgfsEscapeDoComponent(currentComponent, componentSize,
                                            sizeLeft, outPointer);
      if (escapedLength < 0) {
         return escapedLength;
      }
      currentComponent = next;
      sizeLeft -= escapedLength + 1;
      outPointer += escapedLength + 1;
   }
   return (int) (outPointer - bufOut) - 1; // Do not count the last NUL terminator
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscape_GetSize --
 *
 *    Calculates required size in bytes for the buffer that is needed to hold escaped
 *    cross platform path name. Returns 0 if no escaping is required.
 *
 * Results:
 *    On success, the size (excluding the NUL terminator) of the
 *    escaped, NUL terminated buffer.
 *    Returns 0 if the name is a valid Windows file name.
 *    Returns -1 if the name is not a valid file name.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsEscape_GetSize(char const *bufIn,    // IN:  Buffer with unescaped input
                   uint32 sizeIn)        // IN:  Size of the input buffer
{
   uint32 result = 0;
   const char *currentComponent = bufIn;
   const char *end = bufIn + sizeIn;
   const char *next;

   if (sizeIn == 0) { // No need to escape an empty name.
      return 0;
   }
   if (bufIn[sizeIn - 1] == '\0') {
      /*
       * In some cases, a NUL-terminated string is passed to HgfsEscape_GetSize,
       * so it makes sense to support such input even if CPName_GetComponent
       * does not. Detect this case and make the input compliant with
       * CPName_GetComponent by removing the terminating NUL.
       */
      end--;
      sizeIn--;
   }
   /* Skip leading NULs to keep CPName_GetComponent happy. */
   while (*currentComponent == '\0' && currentComponent - bufIn < sizeIn) {
      currentComponent++;
   }
   while (currentComponent - bufIn < sizeIn) {
      int componentSize = CPName_GetComponent(currentComponent, end, &next);
      if (componentSize < 0) {
         Log("%s: failed to calculate escaped name size - name is invalid\n", __FUNCTION__);
         return -1;
      }
      result += HgfsEscapeGetComponentSize(currentComponent, componentSize);
      currentComponent = next;
   }
   return (result == 0) ? 0 : result + sizeIn;
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

uint32
HgfsEscape_Undo(char *bufIn,       // IN: Characters to be unescaped
                uint32 sizeIn)     // IN: Number of characters in bufIn
{
   uint32 componentSize;
   uint32 unprocessedSize = sizeIn + 1;
   uint32 result = 0;
   char *currentComponent = bufIn;

   ASSERT(bufIn != NULL);

   while (currentComponent != NULL) {
      HgfsEscapeUndoComponent(currentComponent, &unprocessedSize);
      componentSize = strlen(currentComponent) + 1; // Unescaped size
      result += componentSize;
      if (unprocessedSize > 1) {
         currentComponent = currentComponent + componentSize;
         componentSize = strlen(currentComponent) + 1; // Size of the next component
      } else {
         currentComponent = NULL;
      }
   }
   return result - 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscapeUndoComponent --
 *
 *    Unescape a buffer that was escaped using HgfsEscapeBuffer.
 *
 *    The unescaping is done in place in the input buffer, and
 *    can not fail.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

void
HgfsEscapeUndoComponent(char   *bufIn,             // IN: Characters to be unescaped
                        uint32 *unprocessedLength) // IN: Unprocessed characters
                                                   //     in the whole name
{
   size_t sizeIn;
   char* curOutBuffer;
   char* escapePointer;

   ASSERT(bufIn != NULL);

   curOutBuffer = bufIn;
   sizeIn = strlen(curOutBuffer);
   escapePointer = strchr(curOutBuffer, HGFS_ESCAPE_CHAR);
   while (escapePointer != NULL) {
      size_t offset = escapePointer - bufIn;

      if (HgfsIsEscapeSequence(bufIn, offset, sizeIn)) {
         char* substitute = strchr(HGFS_SUBSTITUTE_CHARS, bufIn[offset - 1]);
         if (substitute != NULL) {
            bufIn[offset - 1] = HGFS_ILLEGAL_CHARS[substitute - HGFS_SUBSTITUTE_CHARS];
         } else if (bufIn[offset - 1] == HGFS_ESCAPE_SUBSTITUE_CHAR) {
            bufIn[offset - 1] = HGFS_ESCAPE_CHAR;
         }
         memmove(escapePointer, escapePointer + 1, (*unprocessedLength) - offset - 1);
         (*unprocessedLength)--;
         sizeIn--;
         if (sizeIn > 0) {
            escapePointer = strchr(escapePointer, HGFS_ESCAPE_CHAR);
         } else {
            escapePointer = NULL;
         }
      } else {
         escapePointer = strchr(escapePointer + 1, HGFS_ESCAPE_CHAR);
      }
   }
   ASSERT((*unprocessedLength) > sizeIn);
   (*unprocessedLength) -= sizeIn + 1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscapeDoComponent --
 *
 *    Escape any characters that are not legal in a windows filename.
 *    Escape reserved file names that can't be used in Windows.
 *    We also of course have to escape the escape character, which is "%",
 *    when it is part of a character sequence that would require unescaping
 *
 *    sizeBufOut must account for the NUL terminator.
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
HgfsEscapeDoComponent(char const *bufIn, // IN:  Buffer with unescaped input
	                   uint32 sizeIn,     // IN:  Size of input buffer
	                   uint32 sizeBufOut, // IN:  Size of output buffer
	                   char *bufOut)      // OUT: Buffer for escaped output
{
   HgfsEscapeContext conversionContext;
   conversionContext.processedOffset = 0;
   conversionContext.outputBufferLength = sizeBufOut / sizeof *bufOut;
   conversionContext.outputOffset = 0;
   conversionContext.outputBuffer = bufOut;

   if (!HgfsEscapeEnumerate(bufIn, sizeIn, HgfsAddEscapeCharacter, &conversionContext)) {
      return -1;
   }
   return conversionContext.outputOffset * sizeof *bufOut;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsEscapeGetComponentSize --
 *
 *    Calculates number of addtitional characters that are needed to escape
 *    name for one NUL terminated component of the path.
 *
 * Results:
 *    Number of additional escape characters needed to escape the name.
 *    Returns 0 if no escaping is required.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
HgfsEscapeGetComponentSize(char const *bufIn, // IN:  Buffer with unescaped input
                           uint32 sizeIn)     // IN:  Size of the in input buffer
{
   int result = 0;
   HgfsEscapeEnumerate(bufIn, sizeIn, HgfsCountEscapeChars, &result);
   return result;
}
