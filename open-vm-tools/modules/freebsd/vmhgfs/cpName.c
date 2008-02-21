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
 * cpName.c --
 *
 *    Shared portions of cross-platform name conversion routines used
 *    by hgfs. [bac]
 *
 */

#ifdef sun
#include <string.h>
#endif

#include "cpName.h"
#include "cpNameInt.h"
#include "vm_assert.h"

/*
 *----------------------------------------------------------------------
 *
 * CPName_GetComponentGeneric --
 *
 *    Get the next component of the CP name.
 *
 *    Returns the length of the component starting with the begin
 *    pointer, and a pointer to the next component in the buffer, if
 *    any. The "next" pointer is set to "end" if there is no next
 *    component.
 *
 *    'illegal' is a string of characters that are not allowed to
 *    be present in the pre-converted CP name.
 *
 * Results:
 *    length (not including NUL termination) >= 0 of next
 *    component on success.
 *    error < 0 on failure (invalid component).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPName_GetComponentGeneric(char const *begin,   // IN: Beginning of buffer
                           char const *end,     // IN: End of buffer
                           char const *illegal, // IN: Illegal characters
                           char const **next)   // OUT: Start of next component
{
   char const *walk;
   char const *myNext;
   size_t len;

   ASSERT(begin);
   ASSERT(end);
   ASSERT(next);
   ASSERT(illegal);
   ASSERT(begin <= end);

   for (walk = begin; ; walk++) {
      if (walk == end) {
         /* End of buffer. No NUL was found */

         myNext = end;
         break;
      }

      if (*walk == '\0') {
         /* Found a NUL */

         if (walk == begin) {
            Log("CPName_GetComponentGeneric: error: first char can't be NUL\n");
            return -1;
         }

         myNext = walk + 1;
         if (myNext == end) {
            /* Last character in the buffer is not allowed to be NUL */
            return -1;
         }

         break;
      }

      /*
       * Make sure the input buffer does not contain any illegal
       * characters. In particular, we want to make sure that there
       * are no path separator characters in the name. Since the
       * cross-platform name format by definition does not use path
       * separators, this is an error condition, and is likely the
       * sign of an attack. See bug 27926. [bac]
       *
       * The test above ensures that *walk != NUL here, so we don't
       * need to test it again before calling strchr().
       */
      if (strchr(illegal, *walk) != NULL) {
         Log("CPName_GetComponentGeneric: error: Illegal char \"%c\" found in "
             "input\n", *walk);
         return -1;
      }
   }

   len = walk - begin;

   /* 
    * We're only interested in looking for dot/dotdot if the illegal character
    * string isn't empty. These characters are only relevant when the resulting
    * string is to be passed down to the filesystem. Some callers (such as the
    * HGFS server, when dealing with actual filenames) do care about this 
    * validation, but others (like DnD, hgFileCopy, and the HGFS server when
    * converting share names) just want to convert a CPName down to a 
    * nul-terminated string. 
    */
   if (strcmp(illegal, "") != 0 &&
       ((len == 1 && memcmp(begin, ".", 1) == 0) ||
        (len == 2 && memcmp(begin, "..", 2) == 0))) {
      Log("CPName_GetComponentGeneric: error: found dot/dotdot\n");
      return -1;
   }

   *next = myNext;
   return ((int) len);
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameConvertFrom --
 *
 *    Converts a cross-platform name representation into a string for
 *    use in the local filesystem. This is a cross-platform
 *    implementation and takes the path separator argument as an
 *    argument. The path separator is prepended before each additional
 *    path component, so this function never adds a trailing path
 *    separator.
 *
 * Results:
 *    0 on success.
 *    error < 0 on failure (the converted string did not fit in
 *    the buffer provided or the input was invalid).
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPNameConvertFrom(char const **bufIn, // IN/OUT: Input to convert
                  size_t *inSize,     // IN/OUT: Size of input
                  size_t *outSize,    // IN/OUT: Size of output buffer
                  char **bufOut,      // IN/OUT: Output buffer
                  char pathSep)       // IN: Path separator character
{
   char const *in;
   char const *inEnd;
   size_t myOutSize;
   char *out;

   ASSERT(bufIn);
   ASSERT(inSize);
   ASSERT(outSize);
   ASSERT(bufOut);

   in = *bufIn;
   inEnd = in + *inSize;
   myOutSize = *outSize;
   out = *bufOut;

   for (;;) {
      char const *next;
      int len;
      int newLen;

      len = CPName_GetComponent(in, inEnd, &next);
      if (len < 0) {
         Log("CPNameConvertFrom: error: get next component failed\n");
         return len;
      }

      if (len == 0) {
         /* No more component */
         break;
      }

      newLen = ((int) myOutSize) - len - 1;
      if (newLen < 0) {
         Log("CPNameConvertFrom: error: not enough room\n");
         return -1;
      }
      myOutSize = (size_t) newLen;

      *out++ = pathSep;
      memcpy(out, in, len);
      out += len;

      in = next;
   }

   /* NUL terminate */
   if (myOutSize < 1) {
      Log("CPNameConvertFrom: error: not enough room\n");
      return -1;
   }
   *out = '\0';

   /* Path name size should not require more than 4 bytes. */
   ASSERT((in - *bufIn) <= 0xFFFFFFFF);

   /* Update pointers. */
   *inSize -= (in - *bufIn);
   *outSize = myOutSize;
   *bufIn = in;
   *bufOut = out;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_Print --
 *
 *    Converts a CPName formatted string to a valid, NUL-terminated string by
 *    replacing all embedded NUL characters with '|'.
 *
 * Results:
 *    Pointer to a static buffer containing the converted string.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

char const *
CPName_Print(char const *in, // IN: Name to print
             size_t size)    // IN: Size of name
{
   /* Static so it does not go on a kernel stack --hpreg */
   static char out[128];
   size_t i;

   ASSERT(in);

   ASSERT(sizeof out >= 4);
   if (size > sizeof out - 1) {
      size = sizeof out - 4;
      out[size] = '.';
      out[size + 1] = '.';
      out[size + 2] = '.';
      out[size + 3] = '\0';
   } else {
      out[size] = '\0';
   }

   for (i = 0; i < size; i++) {
      out[i] = in[i] != '\0' ? in[i] : '|';
   }

   return out;
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_LinuxConvertTo --
 *
 *    Wrapper function that calls CPNameConvertTo() with the correct arguments
 *    for Linux path conversions.
 *
 *    Makes a cross-platform name representation from the Linux path input
 *    string and writes it into the output buffer.
 *
 * Results:
 *    On success, returns the number of bytes used in the cross-platform name,
 *    NOT including the final terminating NUL character.  On failure, returns
 *    a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPName_LinuxConvertTo(char const *nameIn, // IN:  Buf to convert
                      size_t bufOutSize,  // IN:  Size of the output buffer
                      char *bufOut)       // OUT: Output buffer
{
   return CPNameConvertTo(nameIn, bufOutSize, bufOut, '/', NULL);
}


/*
 *----------------------------------------------------------------------------
 *
 * CPName_WindowsConvertTo --
 *
 *    Wrapper function that calls CPNameConvertTo() with the correct arguments
 *    for Windows path conversions.
 *
 *    Makes a cross-platform name representation from the Linux path input
 *    string and writes it into the output buffer.
 *
 * Results:
 *    On success, returns the number of bytes used in the cross-platform name,
 *    NOT including the final terminating NUL character.  On failure, returns
 *    a negative error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
CPName_WindowsConvertTo(char const *nameIn, // IN:  Buf to convert
                        size_t bufOutSize,  // IN:  Size of the output buffer
                        char *bufOut)       // OUT: Output buffer
{
   return CPNameConvertTo(nameIn, bufOutSize, bufOut, '\\', ":");
}


/*
 *----------------------------------------------------------------------
 *
 * CPNameConvertTo --
 *
 *    Makes a cross-platform name representation from the input string
 *    and writes it into the output buffer.
 *
 * Results:
 *    On success, returns the number of bytes used in the
 *    cross-platform name, NOT including the final terminating NUL
 *    character. On failure, returns a negative error.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

int
CPNameConvertTo(char const *nameIn, // IN:  Buf to convert
                size_t bufOutSize,  // IN:  Size of the output buffer
                char *bufOut,       // OUT: Output buffer
                char pathSep,       // IN:  path separator to use
                char *ignores)      // IN:  chars to not transfer to output
{
   char const *origOut = bufOut;
   char const *endOut = bufOut + bufOutSize;
   size_t cpNameLength = 0;

   ASSERT(nameIn);
   ASSERT(bufOut);

   /* Skip any path separators at the beginning of the input string */
   while (*nameIn == pathSep) {
      nameIn++;
   }

   /*
    * Copy the string to the output buf, converting all path separators into
    * '\0' and ignoring the specified characters.
    */
   for (; *nameIn != '\0' && bufOut < endOut; nameIn++) {
      if (ignores) {
         char *currIgnore = ignores;
         Bool ignore = FALSE;

         while (*currIgnore != '\0') {
            if (*nameIn == *currIgnore) {
               ignore = TRUE;
               break;
            }
            currIgnore++;
         }

         if (!ignore) {
            *bufOut = (*nameIn == pathSep) ? '\0' : *nameIn;
            bufOut++;
         }
      } else {
         *bufOut = (*nameIn == pathSep) ? '\0' : *nameIn;
         bufOut++;
      }
   }

   /*
    * NUL terminate. XXX This should go away.
    *
    * When we get rid of NUL termination here, this test should
    * also change to "if (*nameIn != '\0')".
    */
   if (bufOut == endOut) {
      return -1;
   }
   *bufOut = '\0';

   /* Path name size should not require more than 4 bytes. */
   ASSERT((bufOut - origOut) <= 0xFFFFFFFF);

   /* If there were any trailing path separators, dont count them [krishnan] */
   cpNameLength = bufOut - origOut;
   while ((cpNameLength >= 1) && (origOut[cpNameLength - 1] == 0)) {
      cpNameLength--;
   }

   /* Return number of bytes used */
   return (int) cpNameLength;
}
