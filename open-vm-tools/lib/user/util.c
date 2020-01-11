/*********************************************************
 * Copyright (C) 1998-2019 VMware, Inc. All rights reserved.
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
 * util.c --
 *
 *    misc util functions
 */

#undef WIN32_LEAN_AND_MEAN

#if defined(__linux__) && !defined(VMX86_TOOLS)
#define _GNU_SOURCE
#endif

#include "vm_ctype.h"

#if defined(_WIN32)
# include <winsock2.h> // also includes windows.h
# include <io.h>
# include <process.h>
# include "getoptWin32.h"
#endif

#include <fcntl.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <getopt.h>
#  include <pwd.h>
#  include <dlfcn.h>
#endif

#if defined(__linux__) && !defined(VMX86_TOOLS) && !defined(__ANDROID__)
#  include <link.h>
#endif

#include "vmware.h"
#include "msg.h"
#include "util.h"
#include "str.h"
/* For HARD_EXPIRE --hpreg */
#include "vm_version.h"
#include "su.h"
#include "posix.h"
#include "file.h"
#include "escape.h"
#include "base64.h"
#include "unicode.h"
#include "posix.h"
#include "random.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

/*
 * Win32 doesn't have an iovec type, so do this here to avoid messy games in
 * the header files.  --Jeremy.
 */

struct UtilVector {
   void *base;
   int   len;
};



/*
 *----------------------------------------------------------------------
 *
 * Util_Init --
 *
 *      Opportunity to sanity check things
 *
 * Results:
 *	Bool - TRUE (this should never fail)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Init(void)
{
#ifdef VMX86_DEVEL
   /*
    * Sanity check Str_Snprintf so that we're never thrown off guard
    * by a change in the underlying libraries that Str_Snprintf doesn't
    * catch and wrap properly.
    */
   {
      char buf[2] = { 'x', 'x' };
      int rv;

      rv = Str_Snprintf(buf, sizeof buf, "a");
      ASSERT(rv == 1);
      ASSERT(!strcmp(buf, "a"));

      rv = Str_Snprintf(buf, sizeof buf, "ab");
      ASSERT(rv == -1);
      ASSERT(!strcmp(buf, "a"));
   }
#endif
   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksum32 --
 *
 * Checksums a uint32 aligned block by progressive XOR.  Basically parity
 * checking of each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksum32(const uint32 *buf, int len)
{
   uint32 checksum = 0;
   int i;

   ASSERT((len % 4) == 0);
   for (i = 0; i < len; i+=4) checksum ^= *(buf++);
   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksum --
 *
 * Checksums a block by progressive XOR.  Basically parity
 * checking of each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksum(const uint8 *buf, int len)
{
   uint32 checksum;
   int remainder, shift;

   remainder = len % 4;
   len -= remainder;

   checksum = Util_Checksum32((uint32 *)buf, len);

   buf += len;
   shift = 0;
   while (remainder--) {
      /*
       * Note: this is little endian.
       */
      checksum ^= (*buf++ << shift);
      shift += 8;
   }

   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Checksumv --
 *
 * Checksums an iovector by progressive XOR.  Basically parity checking of
 * each bit position.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_Checksumv(void *iov,      // IN
               int numEntries) // IN
{
   uint32 checksum = 0;
   struct UtilVector *vector = (struct UtilVector *) iov;
   uint32 partialChecksum;
   int bytesSoFar = 0;
   int rotate;

   while (numEntries-- > 0) {
      partialChecksum = Util_Checksum(vector->base, vector->len);
      rotate = (bytesSoFar & 3) * 8;
      checksum ^= ((partialChecksum << rotate) |
                   (partialChecksum >> (32 - rotate)));
      bytesSoFar += vector->len;
      vector++;
   }

   return checksum;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_HashString --
 *
 *      Get a hash of the given NUL terminated string using the djb2
 *      hash algorithm.
 *
 * Results:
 *      The hashed value.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_HashString(const char *str)  // IN:
{
   uint32 hash = 5381;
   int c;

   while ((c = *str++) != 0) {
      hash = ((hash << 5) + hash) + c;
   }

   return hash;
}


/*
 *----------------------------------------------------------------------
 *
 *  CRC_Compute --
 *
 *      computes the CRC of a block of data
 *
 * Results:
 *
 *      CRC code
 *
 * Side effects:
 *      Sets up the crc table if it hasn't already been computed.
 *
 *----------------------------------------------------------------------
 */

static void
UtilCRCMakeTable(uint32 crcTable[])
{
   uint32 c;
   int n, k;

   for (n = 0; n < 256; n++) {
      c = (uint32) n;
      for (k = 0; k < 8; k++) {
         if (c & 1) {
            c = 0xedb88320L ^ (c >> 1);
         } else {
            c = c >> 1;
         }
      }
      crcTable[n] = c;
   }
}

static INLINE_SINGLE_CALLER uint32
UtilCRCUpdate(uint32 crc,
              const uint8 *buf,
              int len)
{
   uint32 c = crc;
   int n;
   static uint32 crcTable[256];
   static int crcTableComputed = 0;

   if (!crcTableComputed) {
      UtilCRCMakeTable(crcTable);

      crcTableComputed = 1;
   }

   for (n = 0; n < len; n++) {
      c = crcTable[(c ^ buf[n]) & 0xff] ^ (c >> 8);
   }

   return c;
}

uint32
CRC_Compute(const uint8 *buf,
            int len)
{
   return UtilCRCUpdate(0xffffffffL, buf, len) ^ 0xffffffffL;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_FastRand --
 *
 *      Historical-name wrapper around Random_Simple.
 *      Deprecated: use Random_Fast, or Random_Simple directly.
 *
 * Results:
 *      A random number.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

uint32
Util_FastRand(uint32 seed)
{
   return Random_Simple(seed);
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Throttle --
 *
 *   Use for throttling of warnings.
 *
 * Results:
 *    Will return TRUE for an increasingly sparse set of counter values:
 *    1, 2, ..., 100, 200, 300, ..., 10000, 20000, 30000, ..., .
 *
 * Side effects:
 *   None.
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Throttle(uint32 count)  // IN:
{
   return count <     100                          ||
         (count <   10000 && count %     100 == 0) ||
         (count < 1000000 && count %   10000 == 0) ||
                             count % 1000000 == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Data2Buffer --
 *
 *    Format binary data for printing.
 *    Uses space as byte separator.
 *
 * Results:
 *    TRUE if all data fits into buffer, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Data2Buffer(char *buf,         // OUT
                 size_t bufSize,    // IN
                 const void *data0, // IN
                 size_t dataSize)   // IN
{
   return Util_Data2BufferEx(buf, bufSize, data0, dataSize, ' ');
}


/*
 *----------------------------------------------------------------------
 *
 * Util_Data2BufferEx --
 *
 *    Format binary data for printing.
 *    Uses custom byte separator. None if set to '\0'.
 *
 * Results:
 *    TRUE if all data fits into buffer, FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_Data2BufferEx(char *buf,         // OUT
                   size_t bufSize,    // IN
                   const void *data0, // IN
                   size_t dataSize,   // IN
                   char sep)          // IN
{
   size_t n; /* Chars to process from data0. */
   const Bool useSeparator = sep != 0;

   /* Number of processed chars that will fit in buf and leave space for trailing NUL. */
   const size_t outChars = useSeparator ? bufSize / 3 : (bufSize - 1) / 2;

   /* At least 1 byte (for NUL) must be available. */
   if (!bufSize) {
      return FALSE;
   }

   n = MIN(dataSize, outChars);
   if (n != 0) {
      const uint8 *data = data0;

      while (n > 0) {
         static const char digits[] = "0123456789ABCDEF";

         *buf++ = digits[*data >> 4];
         *buf++ = digits[*data & 0xF];
         if (useSeparator) {
            *buf++ = sep;
         }
         data++;
         n--;
      }
      if (useSeparator) {
         buf--; /* Overwrite the last separator with NUL. */
      }
   }
   *buf = 0;
   return dataSize <= outChars;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_ExitProcessAbruptly
 *
 *    On Win32, terminate the process and all of its threads, without
 *    calling any of the DLL termination handlers.
 *
 *    On Linux, call _exit().
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
Util_ExitProcessAbruptly(int code) // IN
{
#if defined(_WIN32)
   TerminateProcess(GetCurrentProcess(), code);
#else
   _exit(code);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Util_ExitThread --
 *
 *    Terminate the running thread.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

void
Util_ExitThread(int code) // IN
{
#if defined(_WIN32)
   ExitThread(code);
#else
   exit(code);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Util_CompareDotted --
 *
 *      Compares two version numbers encoded as dotted strings.
 *
 * Results:
 *      0 if equal, -1 if s1 is less than s2, else 1.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

int
Util_CompareDotted(const char *s1, const char *s2)
{
   int i, x[5], y[5];

   for (i = 0; i < 5; i++) {
      x[i] = 0;
      y[i] = 0;
   }

   if (sscanf(s1, "%d.%d.%d.%d.%d", &x[0], &x[1], &x[2], &x[3], &x[4]) < 1) {
      x[0] = 1;
   }
   if (sscanf(s2, "%d.%d.%d.%d.%d", &y[0], &y[1], &y[2], &y[3], &y[4]) < 1) {
      y[0] = 1;
   }

   for (i = 0; i < 5; i++) {
      if (x[i] < y[i]) {
         return -1;
      }
      if (x[i] > y[i]) {
         return 1;
      }
   }

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetOpt --
 *
 *      A wrapper around getopt_long that avoids needing separate long and
 *      short option lists.
 *
 *      To use this, the array of option structs must:
 *      * Store the short option name in the 'val' member.
 *      * Set the 'name' member to NULL if the option has a short name but no
 *        long name.
 *      * For options that have only a long name, 'val' should be set to a
 *        unique value greater than UCHAR_MAX.
 *      * Terminate the array with a sentinel value that zero-initializes both
 *        'name' and 'val'.
 *
 * Results:
 *      See getopt_long.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
Util_GetOpt(int argc,                  // IN
            char * const *argv,        // IN
            const struct option *opts, // IN
            Util_NonOptMode mode,      // IN
            Bool manualErrorHandling)  // IN: True if the caller wants to handle error reporting.

{
   int ret = -1;

   struct option *longOpts = NULL;
   char *shortOptString = NULL;

   /*
    * In the worst case, each character needs "::" to indicate that it takes
    * an optional argument.
    */
   const size_t maxCharsPerShortOption = 3;
   const size_t modePrefixSize = 2; // "[+-][:]"

   size_t n = 0;
   size_t shortOptStringSize;

   while (!(opts[n].name == NULL && opts[n].val == 0)) {
      if (UNLIKELY(n == SIZE_MAX)) {
         /*
          * Avoid integer overflow.  If you have this many options, you're
          * doing something wrong.
          */
         ASSERT(FALSE);
         goto exit;
      }
      n++;
   }

   if (UNLIKELY(n > SIZE_MAX / sizeof *longOpts - 1)) {
      /* Avoid integer overflow. */
      ASSERT(FALSE);
      goto exit;
   }
   longOpts = malloc((n + 1) * sizeof *longOpts);
   if (longOpts == NULL) {
      goto exit;
   }

   if (UNLIKELY(n > (SIZE_MAX - modePrefixSize - 1 /* NUL */) /
                maxCharsPerShortOption)) {
      /* Avoid integer overflow. */
      ASSERT(FALSE);
      goto exit;
   }
   shortOptStringSize = n * maxCharsPerShortOption + modePrefixSize + 1 /* NUL */;
   shortOptString = malloc(shortOptStringSize);
   if (shortOptString == NULL) {
      goto exit;
   } else {
      struct option empty = { 0 };

      size_t i;
      struct option *longOptOut = longOpts;
      char *shortOptOut = shortOptString;

      // How to handle non-option arguments.
      switch (mode) {
         case UTIL_NONOPT_STOP:
            *shortOptOut++ = '+';
            break;
         case UTIL_NONOPT_ALL:
            *shortOptOut++ = '-';
            break;
         default:
            break;
      }

      if (manualErrorHandling) {
         /*
          * Make getopt return ':' instead of '?' if required arguments to
          * options are missing.
          */
         *shortOptOut++ = ':';
      }

      for (i = 0; i < n; i++) {
         int val = opts[i].val;

         if (opts[i].name != NULL) {
            *longOptOut++ = opts[i];
         }

         if (val > 0 && val <= UCHAR_MAX) {
            int argSpec = opts[i].has_arg;

            *shortOptOut++ = (char) val;

            if (argSpec != no_argument) {
               *shortOptOut++ = ':';

               if (argSpec == optional_argument) {
                  *shortOptOut++ = ':';
               }
            }
         }
      }

      ASSERT(longOptOut - longOpts <= n);
      *longOptOut = empty;

      ASSERT(shortOptOut - shortOptString < shortOptStringSize);
      *shortOptOut = '\0';
   }

   ret = getopt_long(argc, argv, shortOptString, longOpts, NULL);

exit:
   free(longOpts);
   free(shortOptString);
   return ret;
}




/*
 *-----------------------------------------------------------------------------
 *
 * Util_HasAdminPriv --
 *
 *    Determine if the calling code has administrator privileges --hpreg
 *
 * Results:
 *    1 if yes
 *    0 if no
 *    <0 on error
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

int
Util_HasAdminPriv(void)
{
#if defined(_WIN32)
#if defined(VM_WIN_UWP)
   return 0;
#else
   HANDLE token = INVALID_HANDLE_VALUE;
   int ret = -1;

   /*
    * Retrieve the access token of the calling thread --hpreg
    *
    * On some machines OpenThreadToken with openAsSelf set to FALSE fails.
    * Empirically, it seems that, in the security context of another user
    * (even when the impersonation token is at SecurityImpersonation level)
    * it is not able to obtain the thread token with TOKEN_DUPLICATE access.
    * Calling OpenThreadToken to open as self is more reliable and does not
    * seem to hurt. -- vui
    */

   if (OpenThreadToken(GetCurrentThread(), TOKEN_DUPLICATE, TRUE,
          &token) == 0) {
      if (GetLastError() != ERROR_NO_TOKEN) {
         ret = -1;
         goto end;
      }

      if (OpenProcessToken(GetCurrentProcess(), TOKEN_DUPLICATE,
             &token) == 0) {
         ret = -2;
         goto end;
      }
   }

   ret = Util_TokenHasAdminPriv(token);

end:

   if (token != INVALID_HANDLE_VALUE) {
      if (CloseHandle(token) == 0 && ret >= 0) {
         ret = -13;
      }
   }

   return ret;
#endif // !VM_WIN_UWP
#else
   return Id_IsSuperUser() ? 1 : 0;
#endif
}

/*
 *-----------------------------------------------------------------------------
 *
 * Util_DeriveFileName --
 *
 *      This function is admittedly weird.  The basic idea is that we
 *      have a path to a dictionary file, and we need to make a path to
 *      a another file that's named in a similar way to that dictionary
 *      file (e.g., only difference is extension, or filename and
 *      extension).
 *
 *      This function returns a pointer to the result
 *
 * Results:
 *      Pointer to string (on success, caller should free string),
 *      otherwise NULL.
 *
 * Side effects:
 *      Allocates memory to be freed by caller.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_DeriveFileName(const char *source, // IN: path to dict file (incl filename)
                    const char *name,   // IN: what to replace filename with (optional)
                    const char *ext)    // IN: what to replace extension with (optional)
{
   char *returnResult = NULL;
   char *path = NULL;
   char *base = NULL;

   if (source == NULL || (name == NULL && ext == NULL)) {
      Warning("invalid use of function\n");
      return NULL;
   }
   File_GetPathName(source, &path, &base);

   /* If replacing name and extension */
   if (name != NULL) {
      free(base);

      /*
       * If the "name" we have to append is a relative path (i.e., not an
       * absolute path), then we need to concatenate the "name" to the
       * path of "source". If the path of "source" doesn't exist or is
       * just ".", then we don't need to bother with concatenating results
       * together.
       */

      if (!Util_IsAbsolutePath(name) && strlen(path) > 0 &&
          strcmp(path, ".") != 0) {
	 if (ext == NULL) {
	    returnResult = Str_SafeAsprintf(NULL, "%s%s%s",
                                            path, DIRSEPS, name);
	 } else {
            returnResult = Str_SafeAsprintf(NULL, "%s%s%s.%s",
                                            path, DIRSEPS, name, ext);
	 }
      } else {
	 /*
          * Path is non-existent or is just the current directory (or the
	  * result from the dictionary is an absolute path), so we
	  * just need to use the filename (using the DIRSEPS method above
          * for a non-existent path might result in something undesireable
	  * like "\foobar.vmdk")
	  */

	 if (ext == NULL) {
            returnResult = Util_SafeStrdup(name);
	 } else {
            returnResult = Str_SafeAsprintf(NULL, "%s.%s", name, ext);
	 }
      }
      free(path);
      return returnResult;
   }

   /* replacing only the file extension */

   /* strip off the existing file extension, if present */
   {
      char *p = Str_Strrchr(base, '.');
      if (p != NULL) {
	 *p = '\0';
      }
   }

   /* Combine disk path with parent path */
   if (strlen(path) > 0 && strcmp(path, ".") != 0) {
      returnResult = Str_SafeAsprintf(NULL, "%s%s%s.%s",
                                      path, DIRSEPS, base, ext);
   } else {
      /*
       * Path is non-existent or is just the current directory, so we
       * just need to use the filename (using the DIRSEPS method might
       * result in something undesireable like "\foobar.vmdk")
       */
      returnResult = Str_SafeAsprintf(NULL, "%s.%s", base, ext);
   }
   free(path);
   free(base);
   return returnResult;
}
