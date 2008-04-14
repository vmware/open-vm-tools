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
 * codeset.c --
 *
 *    Character set and encoding conversion functions --hpreg
 */


/*
 * Windows have their own logic for conversion.  On Posix systems with iconv
 * available we use iconv.  On systems without iconv use simple 1:1 translation
 * for UTF8 <-> Current.  We also use 1:1 translation also for MacOS's conversion
 * between 'current' and 'UTF-8', as MacOS is UTF-8 only.
 */

#if defined(_WIN32)
#   include <windows.h>
#   include <malloc.h>
#   include <str.h>
#else
#   include <string.h>
#   include <stdlib.h>
#   include <errno.h>
#   if defined(__FreeBSD__) || (defined(__linux__) && !defined(GLIBC_VERSION_21))
#      define CURRENT_IS_UTF8
#   else
#      define USE_ICONV
#      include <iconv.h>
#      include <langinfo.h>
#      ifdef __linux__
#         include "str.h"
#      endif
#      if defined(__APPLE__)
#         define CURRENT_IS_UTF8
#         include <CoreFoundation/CoreFoundation.h> /* for CFString */
#      endif
#   endif
#endif

#include "vm_assert.h"
#include "codeset.h"

#define CSGTG_NORMAL    0x0000  /* Without any information loss. */
#define CSGTG_TRANSLIT  0x0001  /* Transliterate unknown characters. */
#define CSGTG_IGNORE    0x0002  /* Skip over untranslatable characters. */

#if defined(__FreeBSD__) || defined(sun)
static const char nul[] = {'\0', '\0'};
#else
static const wchar_t nul = L'\0';
#endif


#if defined(CURRENT_IS_UTF8) || defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetDuplicateStr --
 *
 *    Duplicate input string, appending zero terminator to its end.  Only
 *    used on Windows and on platforms where current encoding is always
 *    UTF-8, on other iconv-capable platforms we just use iconv even for
 *    UTF-8 to UTF-8 translation.  Note that this function is more like
 *    memdup() than strdup(), so it can be used for duplicating UTF-16
 *    strings as well - string is always terminated by wide character NUL,
 *    which is supposed to be longest NUL character occuring on specified
 *    host.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetDuplicateStr(const char   *bufIn,    // IN: Input string
                    size_t  sizeIn,   // IN: Input string length
                    char        **bufOut,   // OUT: "Converted" string
                    size_t *sizeOut)  // OUT: Length of string
{
   char *myBufOut;

   myBufOut = malloc(sizeIn + sizeof nul);
   if (myBufOut == NULL) {
      return FALSE;
   }

   memcpy(myBufOut, bufIn, sizeIn);
   memset(myBufOut + sizeIn, 0, sizeof nul);

   *bufOut = myBufOut;
   if (sizeOut) {
      *sizeOut = sizeIn;
   }
   return TRUE;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetDynBufFinalize --
 *
 *    Append NUL terminator to the buffer, and return pointer to buffer
 *    and its data size (before appending terminator).  Destroys buffer
 *    on failure.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetDynBufFinalize(Bool          ok,       // IN: Earlier steps succeeded
                      DynBuf       *db,       // IN: Buffer with converted string
                      char        **bufOut,   // OUT: Converted string
                      size_t *sizeOut)  // OUT: Length of string in bytes
{
   if (!ok || !DynBuf_Append(db, &nul, sizeof nul) || !DynBuf_Trim(db)) {
      DynBuf_Destroy(db);
      return FALSE;
   }

   *bufOut = DynBuf_Get(db);
   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(db) - sizeof nul;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetUtf8ToUtf16le --
 *
 *    Append the content of a buffer (that uses the UTF-8 encoding) to a
 *    DynBuf (that uses the UTF-16LE encoding)
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetUtf8ToUtf16le(const char *bufIn,  // IN
                     size_t      sizeIn, // IN
                     DynBuf     *db)     // IN
{
   size_t currentSize;
   size_t allocatedSize;
   uint16 *buf;

   currentSize = DynBuf_GetSize(db);
   allocatedSize = DynBuf_GetAllocatedSize(db);
   buf = (uint16 *)((char *)DynBuf_Get(db) + currentSize);
   while (sizeIn--) {
      unsigned int uniChar = *bufIn++ & 0xFF;
      size_t neededSize;

      if (uniChar >= 0x80) {
         size_t charLen;
         size_t idx;

         if (uniChar < 0xC2) {
            /* 80-BF cannot start UTF8 sequence.  C0-C1 cannot appear in UTF8 stream at all. */
            return FALSE;
         } else if (uniChar < 0xE0) {
            uniChar -= 0xC0;
            charLen = 1;
         } else if (uniChar < 0xF0) {
            uniChar -= 0xE0;
            charLen = 2;
         } else if (uniChar < 0xF8) {
            uniChar -= 0xF0;
            charLen = 3;
         } else if (uniChar < 0xFC) {
            uniChar -= 0xF8;
            charLen = 4;
         } else if (uniChar < 0xFE) {
            uniChar -= 0xFC;
            charLen = 5;
         } else {
            return FALSE;
         }
         if (sizeIn < charLen) {
            return FALSE;
         }
         for (idx = 0; idx < charLen; idx++) {
            unsigned int nextChar;

            nextChar = *bufIn++ & 0xFF;
            if (nextChar < 0x80 || nextChar >= 0xC0) {
               return FALSE;
            }
            uniChar = (uniChar << 6) + nextChar - 0x80;
            sizeIn--;
         }

         /*
          * Shorter encoding available.  UTF-8 mandates that shortest possible encoding
          * is used, as otherwise doing UTF-8 => anything => UTF-8 could bypass some
          * important tests, like '/' for path separator or \0 for string termination.
          *
          * This test is not correct for charLen == 1, as its disallowed range is
          * 0x00-0x7F, while this test disallows only 0x00-0x3F.  But this part of
          * problem is solved by stopping when 0xC1 is encountered, as 0xC1 0xXX
          * describes just this range.  This also means that 0xC0 and 0xC1 cannot
          * ever occur in valid UTF-8 string.
          */
         if (uniChar < 1U << (charLen * 5 + 1)) {
            return FALSE;
         }
      }

      /*
       * Here we have UCS-4 character in uniChar, between 0 and 0x7FFFFFFF.
       * Let's convert it to UTF-16.
       */

      /* Non-paired surrogates are illegal in UTF-16. */
      if (uniChar >= 0xD800 && uniChar < 0xE000) {
         return FALSE;
      }
      if (uniChar < 0x10000) {
         neededSize = currentSize + sizeof *buf;
      } else if (uniChar < 0x110000) {
         neededSize = currentSize + 2 * sizeof *buf;
      } else {
         /* This character cannot be represented in UTF-16. */
         return FALSE;
      }
      if (allocatedSize < neededSize) {
         if (DynBuf_Enlarge(db, neededSize) == FALSE) {
            return FALSE;
         }
         allocatedSize = DynBuf_GetAllocatedSize(db);
         ASSERT(neededSize <= allocatedSize);
         buf = (uint16 *)((char *)DynBuf_Get(db) + currentSize);
      }
      if (uniChar < 0x10000) {
         *buf++ = uniChar;
      } else {
         *buf++ = 0xD800 + ((uniChar - 0x10000) >> 10);
         *buf++ = 0xDC00 + ((uniChar - 0x10000) & 0x3FF);
      }
      currentSize = neededSize;
   }
   /* All went fine, update buffer size. */
   DynBuf_SetSize(db, currentSize);
   return TRUE;
}


#if defined(_WIN32)

static Bool IsWin95(void);
static DWORD GetInvalidCharsFlag(void);

/*
 * Win32-specific remarks: here is my understanding of those terms as of
 * 2002/02/12:
 *
 * ANSI code page
 *    The character set used internally by Windows applications (when they are
 *    not fully Unicode).
 *
 * OEM code page
 *    The character set used by MS-DOS and stored in the FAT filesystem.
 *
 *   --hpreg
 */


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetGenericToUtf16le --
 *
 *    Append the content of a buffer (that uses the specified encoding) to a
 *    DynBuf (that uses the UTF-16LE encoding) --hpreg
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetGenericToUtf16le(UINT codeIn,         // IN
                        char const *bufIn,   // IN
                        size_t sizeIn, // IN
                        DynBuf *db)          // IN
{
   /*
    * Undocumented: calling MultiByteToWideChar() with sizeIn == 0 returns 0
    * with GetLastError() set to ERROR_INVALID_PARAMETER. Isn't this API
    * robust? --hpreg
    */
   if (sizeIn) {
      size_t initialSize;
      DWORD flags = GetInvalidCharsFlag();
      Bool invalidCharsCheck = ((flags & MB_ERR_INVALID_CHARS) != 0);

      initialSize = DynBuf_GetSize(db);
      for (;;) {
         int result;
         int resultReverse;
         DWORD error = ERROR_SUCCESS;

         if (DynBuf_Enlarge(db, sizeof(wchar_t)) == FALSE) {
            return FALSE;
         }

         /*
          * Must fail if bufIn has any invalid characters.
          * So MB_ERR_INVALID_CHARS added, otherwise can
          * lead to security issues see bug 154114.
          */
         result = MultiByteToWideChar(codeIn,
                     flags,
                     bufIn,
                     sizeIn,
                     (wchar_t *)((char *)DynBuf_Get(db) + initialSize),
                     (DynBuf_GetAllocatedSize(db) - initialSize) /
                                      sizeof(wchar_t));

         if (0 == result) {
            error = GetLastError();   // may be ERROR_NO_UNICODE_TRANSLATION
         }

         /*
          * Success if: result is > 0 and is the same number
          * of characters as the input string contains. If there
          * are any invalid characters, the Win2K SP4 or later will
          * fail, but for earlier OS versions, these invalid characters
          * will be dropped. Thus only succeed if we have no dropped
          * characters.
          * For the older platforms which don't fail for invalid characters
          * we see if the reverse conversion of the converted string
          * yields the same string size that was passed in.
          * If not, then dropped characters so fail.
          */
         if (!invalidCharsCheck) {
            resultReverse = WideCharToMultiByte(codeIn, 0,
                            (wchar_t *)((char *)DynBuf_Get(db) + initialSize),
                            result, NULL, 0, 0, 0);
         }
         if (result > 0 &&
             (invalidCharsCheck ||
             (sizeIn == resultReverse))) {
            DynBuf_SetSize(db, initialSize + result * sizeof(wchar_t));
            break;
         }

         if (result > 0 && (!invalidCharsCheck && sizeIn != resultReverse)) {
            return FALSE;
         }

         ASSERT(result == 0);

         if (error != ERROR_INSUFFICIENT_BUFFER) {
            return FALSE;
         }

         /* Need a larger buffer --hpreg */
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetUtf16leToGeneric --
 *
 *    Append the content of a buffer (that uses the UTF-16LE encoding) to a
 *    DynBuf (that uses the specified encoding) --hpreg
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetUtf16leToGeneric(char const *bufIn,   // IN
                        size_t sizeIn, // IN
                        UINT codeOut,        // IN
                        DynBuf *db)          // IN
{
   /*
    * Undocumented: calling WideCharToMultiByte() with sizeIn == 0 returns 0
    * with GetLastError() set to ERROR_INVALID_PARAMETER. Isn't this API
    * robust? --hpreg
    */
   if (sizeIn) {
      size_t initialSize;

      initialSize = DynBuf_GetSize(db);
      for (;;) {
         int result;
         DWORD error;

         if (DynBuf_Enlarge(db, 1) == FALSE) {
            return FALSE;
         }

         result = WideCharToMultiByte(codeOut,
                     0,
                     (wchar_t const *)bufIn,
                     sizeIn / sizeof(wchar_t),
                     (char *)DynBuf_Get(db) + initialSize,
                     DynBuf_GetAllocatedSize(db) - initialSize,
                     NULL,
                     /*
                      * XXX We may need to pass that argument
                      *     to know when the conversion was
                      *     not possible --hpreg
                      */
                     NULL);
         if (result > 0) {
            DynBuf_SetSize(db, initialSize + result);
            break;
         }

         ASSERT(result == 0);

         /* Must come first --hpreg */
         error = GetLastError();

         if (error != ERROR_INSUFFICIENT_BUFFER) {
            return FALSE;
         }

         /* Need a larger buffer --hpreg */
      }
   }

   /*
    * Undocumented: if the input buffer is not NUL-terminated, the output
    * buffer will not be NUL-terminated either --hpreg
    */
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetUtf16leToCurrent --
 *
 *    Convert the content of a buffer (that uses the UTF-16LE encoding) into
 *    another buffer (that uses the current encoding) --hpreg
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetUtf16leToCurrent(char const *bufIn,     // IN
                        size_t sizeIn,   // IN
                        char **bufOut,         // OUT
                        size_t *sizeOut) // OUT
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   /* XXX We should probably use CP_THREAD_ACP on Windows 2000/XP --hpreg */
   ok = CodeSetUtf16leToGeneric(bufIn, sizeIn, CP_ACP, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}
#endif


#if defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetUtf8Normalize --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the UTF-8 encoding) that is in precomposed
      (Normalization Form C) or decomposed (Normalization Form D).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains a NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    '*bufOut' contains the allocated, NUL terminated buffer.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetUtf8Normalize(char const *bufIn,     // IN
                     size_t sizeIn,         // IN
                     Bool precomposed,      // IN
                     DynBuf *db)            // OUT
{
   Bool ok = FALSE;
   CFStringRef str = NULL;
   CFMutableStringRef mutStr = NULL;
   CFIndex len, lenMut;
   size_t initialSize = DynBuf_GetSize(db);

   str = CFStringCreateWithCString(NULL,
                                   bufIn,
                                   kCFStringEncodingUTF8);
   if (str == NULL) {
      goto exit;
   }

   mutStr = CFStringCreateMutableCopy(NULL, 0, str);
   if (mutStr == NULL) {
      goto exit;
   }

   /*
    * Normalize the string, Form C - precomposed or D, not.
    */
   CFStringNormalize(mutStr, 
                     (precomposed ? kCFStringNormalizationFormC :
                     kCFStringNormalizationFormD));

   /* 
    * Get the number (in terms of UTF-16 code units) 
    * of characters in a string.
    */
   lenMut = CFStringGetLength(mutStr);

   /*
    * Retrieve the maximum number of bytes a string of a 
    * specified length (in UTF-16 code units) will take up 
    * if encoded in a specified encoding.
    */
   len = CFStringGetMaximumSizeForEncoding(lenMut,
                                           kCFStringEncodingUTF8);
   if (len + 1 > initialSize) {
      if (DynBuf_Enlarge(db, len + 1 - initialSize) == FALSE) {
         ok = FALSE;
         goto exit;
      }
   }

   /*
    * Copies the character contents of a string to a local C 
    * string buffer after converting the characters to UTF-8.
    */
   ok = CFStringGetCString(mutStr, 
                           (char *)DynBuf_Get(db),
                           len + 1, 
                           kCFStringEncodingUTF8);
   if (ok) {
      /* Remove the NUL terminator that the above includes. */
      DynBuf_SetSize(db, strlen((char *)DynBuf_Get(db)));
   }

exit:
   if (str) {
      CFRelease(str);
   }
   if (mutStr) {
      CFRelease(mutStr);
   }

   return ok;
}
#endif /* defined(__APPLE__) */


/*
 * Linux-specific remarks:
 *
 * We use UTF-16 instead of UCS-2, because Windows 2000 introduces support
 * for basic input, output, and simple sorting of surrogates.
 *
 * We use UTF-16LE instead of UTF-16 so that iconv() does not prepend a BOM.
 *
 *   --hpreg
 */


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GetCurrentCodeSet --
 *
 *    Return currently active code set - always UTF-8 on Apple (and old Linux),
 *    and reported by nl_langinfo on Linux & Solaris.
 *
 * Results:
 *    The name of the current code set on success
 *    
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

const char *
CodeSet_GetCurrentCodeSet(void)
{
#if defined(CURRENT_IS_UTF8)
   /*
    * This code is used in
    * 1. tools which need to support older glibc versions in which
    *    internationalization functions are not available.
    * 2. Mac OS which we always assume UTF-8
    * On such systems we revert to the original behaviour of this code which
    * simply copies the input buffer into the output.
    */

   return "UTF-8";
#elif defined(_WIN32)
   static char ret[20];  // max is "windows-4294967296"

   Str_Sprintf(ret, sizeof(ret), "windows-%u", GetACP());

   return ret;
#else
   return nl_langinfo(CODESET);
#endif
}


#if defined(USE_ICONV)

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetIconvOpen --
 *
 *    Open iconv translator with requested flags.  Currently only no flags,
 *    and both CSGTG_TRANSLIT and CSGTG_IGNORE together are supported, as
 *    this is only thing we need.  If translit/ignore convertor fails,
 *    then non-transliterating conversion is used.
 *
 * Results:
 *    (iconv_t)-1 on failure
 *    iconv handle on success
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static INLINE_SINGLE_CALLER iconv_t
CodeSetIconvOpen(const char  *codeIn,  // IN
                 const char  *codeOut, // IN
                 unsigned int flags)   // IN
{
#ifdef __linux__
   if (flags) {
      char *codeOutExt;

      ASSERT(flags == (CSGTG_TRANSLIT | CSGTG_IGNORE));
      /*
       * We should be using //TRANSLIT,IGNORE, but glibc versions older than
       * 2.3.4 (in particular, the version that ships with redhat linux 9.0)
       * are subtly broken when passing options with a comma, in such a way
       * that iconv_open will succeed but iconv_close can crash.  For now, we
       * only use TRANSLIT and bail out after the first non-translitible
       * character.
       */
      codeOutExt = Str_Asprintf(NULL, "%s//TRANSLIT", codeOut);
      if (codeOutExt) {
         iconv_t cd = iconv_open(codeOutExt, codeIn);
         free(codeOutExt);
         if (cd != (iconv_t)-1) {
            return cd;
         }
      }
   }
#endif
   return iconv_open(codeOut, codeIn);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetGenericToGeneric --
 *
 *    Append the content of a buffer (that uses the specified encoding) to a
 *    DynBuf (that uses the specified encoding). --hpreg
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetGenericToGeneric(char const *codeIn,  // IN
                        char const *bufIn,   // IN
                        size_t sizeIn,       // IN
                        char const *codeOut, // IN
                        DynBuf *db,          // IN/OUT
                        size_t flags)  // IN
{
   iconv_t cd;

   ASSERT(codeIn);
   ASSERT(sizeIn == 0 || bufIn);
   ASSERT(codeOut);
   ASSERT(db);

   cd = CodeSetIconvOpen(codeIn, codeOut, flags);
   if (cd == (iconv_t)-1) {
      return FALSE;
   }

   for (;;) {
      size_t size;
      char *out;
      char *outOrig;
      size_t outLeft;
      size_t status;

      /*
       * Every character we care about can occupy at most
       * 4 bytes - UCS-4 is 4 bytes, UTF-16 is 2+2 bytes,
       * and UTF-8 is also at most 4 bytes for all
       * characters under 0x1FFFFF.
       *
       * If we allocate too small buffer nothing critical
       * happens except that in //IGNORE case some
       * implementations might return EILSEQ instead of
       * E2BIG.  By having at least 4 bytes available we
       * can be sure that at least one character is
       * converted each call to iconv().
       */
      size = DynBuf_GetSize(db);
      if (DynBuf_Enlarge(db, size + 4) == FALSE) {
         goto error;
      }

      out = (int8 *)DynBuf_Get(db) + size;
      outOrig = out;
      outLeft = DynBuf_GetAllocatedSize(db) - size;

      /*
       * From glibc 2.2 onward bufIn is no longer const due to a change
       * in the standard. However, the implementation of iconv doesn't
       * change bufIn so a simple cast is safe. --plangdale
       */
#if defined(GLIBC_VERSION_22)
      status = iconv(cd, (char **)&bufIn, &sizeIn, &out, &outLeft);
#else
      status = iconv(cd, &bufIn, &sizeIn, &out, &outLeft);
#endif

      DynBuf_SetSize(db, size + out - outOrig);

      /*
       * If all input characters were consumed, we are done.
       * Otherwise if at least one character was produced by conversion
       * then just increase buffer size and try again - with //IGNORE
       * iconv() returns an error (EILSEQ) but still processes as
       * many characters as possible.  If no characters were produced,
       * then consult error code - do not consult return value in other
       * cases, it can be either random positive value (if some
       * characters were transliterated) or even -1 with errno set to
       * EILSEQ (if some characters were ignored).
       */
      if (sizeIn == 0) {
         break;
      }
      if (out == outOrig) {
         if (status != -1) {
            goto error;
         }
	 /*
	  * Some libc implementations (one on ESX3, and one on Ganesh's
	  * box) silently ignore //IGNORE.  So if caller asked for
	  * getting conversion done at any cost, just return success
	  * even if failure occured.  User will get truncated
	  * message, but that's our best.  We have no idea whether
	  * incoming encoding is 8bit, 16bit, or what, so we cannot
	  * skip over characters in input stream and recover :-(
	  */
	 if ((flags & CSGTG_IGNORE) && errno == EILSEQ) {
	    break;
	 }
         if (errno != E2BIG) {
            goto error;
         }
      }
      /* Need a larger buffer --hpreg */
   }

   if (iconv_close(cd) < 0) {
      return FALSE;
   }

   return TRUE;

error:
   iconv_close(cd);

   return FALSE;
}
#endif


/*
 * Generic remarks: here is my understanding of those terms as of 2001/12/27:
 *
 * BOM
 *    Byte-Order Mark
 *
 * BMP
 *    Basic Multilingual Plane. This plane comprises the first 2^16 code
 *    positions of ISO/IEC 10646's canonical code space
 *
 * UCS
 *    Universal Character Set. Encoding form specified by ISO/IEC 10646
 *
 * UCS-2
 *    Directly store all Unicode scalar value (code point) from U+0000 to
 *    U+FFFF on 2 bytes. Consequently, this representation can only hold
 *    characters in the BMP
 *
 * UCS-4
 *    Directly store a Unicode scalar value (code point) from U-00000000 to
 *    U-FFFFFFFF on 4 bytes
 *
 * UTF
 *    Abbreviation for Unicode (or UCS) Transformation Format
 *
 * UTF-8
 *    Unicode (or UCS) Transformation Format, 8-bit encoding form. UTF-8 is the
 *    Unicode Transformation Format that serializes a Unicode scalar value
 *    (code point) as a sequence of 1 to 6 bytes
 *
 * UTF-16
 *    UCS-2 + surrogate mechanism: allow to encode some non-BMP Unicode
 *    characters in a UCS-2 string, by using 2 2-byte units. See the Unicode
 *    standard, v2.0
 *
 * UTF-32
 *    Directly store all Unicode scalar value (code point) from U-00000000 to
 *    U-0010FFFF on 4 bytes. This is a subset of UCS-4
 *
 *   --hpreg
 */


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToCurrent --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the current encoding) --hpreg
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8ToCurrent(char const *bufIn,     // IN
                      size_t sizeIn,   // IN
                      char **bufOut,         // OUT
                      size_t *sizeOut) // OUT
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetGenericToGeneric("UTF-8", bufIn, sizeIn,
                                CodeSet_GetCurrentCodeSet(), &db, CSGTG_NORMAL);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char *buf;
   size_t size;
   Bool status;

   if (CodeSet_Utf8ToUtf16le(bufIn, sizeIn, &buf, &size) == FALSE) {
      if (IsWin95()) {
         /*
          * Win95 doesnt support UTF-8 by default. If we are here,
          * it means that the application was not linked with unicows.lib.
          * So simply copy the buffer as is.
          */
         return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
      }
      return FALSE;
   }

   status = CodeSetUtf16leToCurrent(buf, size, bufOut, sizeOut);
   free(buf);

   return status;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToCurrentTranslit --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the current encoding).  Transliterate
 *    characters which can be transliterated, and ignore characters which
 *    cannot be even transliterated.
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8ToCurrentTranslit(char const *bufIn,     // IN
                              size_t sizeIn,   // IN
                              char **bufOut,         // OUT
                              size_t *sizeOut) // OUT
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetGenericToGeneric("UTF-8", bufIn, sizeIn,
                                CodeSet_GetCurrentCodeSet(), &db,
                                CSGTG_TRANSLIT | CSGTG_IGNORE);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char *buf;
   size_t size;
   Bool status;

   if (CodeSet_Utf8ToUtf16le(bufIn, sizeIn, &buf, &size) == FALSE) {
      if (IsWin95()) {
         /*
          * Win95 doesnt support UTF-8 by default. If we are here,
          * it means that the application was not linked with unicows.lib.
          * So simply copy the buffer as is.
          */
         return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
      }
      return FALSE;
   }

   status = CodeSetUtf16leToCurrent(buf, size, bufOut, sizeOut);
   free(buf);

   return status;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_CurrentToUtf8 --
 *
 *    Convert the content of a buffer (that uses the current encoding) into
 *    another buffer (that uses the UTF-8 encoding) --hpreg
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_CurrentToUtf8(char const *bufIn,     // IN
                      size_t sizeIn,   // IN
                      char **bufOut,         // OUT
                      size_t *sizeOut) // OUT
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetGenericToGeneric(CodeSet_GetCurrentCodeSet(), bufIn, sizeIn,
                                "UTF-8", &db, CSGTG_NORMAL);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char *buf;
   size_t size;
   Bool status;

   if (CodeSet_CurrentToUtf16le(bufIn, sizeIn, &buf, &size) == FALSE) {
      return FALSE;
   }

   status = CodeSet_Utf16leToUtf8(buf, size, bufOut, sizeOut);
   free(buf);
   if (!status && IsWin95()) {
      /*
       * Win95 doesnt support UTF-8 by default. If we are here,
       * it means that the application was not linked with unicows.lib.
       * So simply copy the buffer as is.
       */
      return CodeSetDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
   }
  return status;
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16leToUtf8_Db --
 *
 *    Append the content of a buffer (that uses the UTF-16LE encoding) to a
 *    DynBuf (that uses the UTF-8 encoding). --hpreg
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf16leToUtf8_Db(char const *bufIn,   // IN
                         size_t sizeIn, // IN
                         DynBuf *db)          // IN
{
#if defined(_WIN32)
   return CodeSetUtf16leToGeneric(bufIn, sizeIn, CP_UTF8, db);
#else
   const uint16 *utf16In;
   size_t numCodeUnits;
   size_t codeUnitIndex;

   if (sizeIn % sizeof *utf16In != 0) {
      return FALSE;
   }

   utf16In = (const uint16 *)bufIn;
   numCodeUnits = sizeIn / 2;

   for (codeUnitIndex = 0; codeUnitIndex < numCodeUnits; codeUnitIndex++) {
      uint32 codePoint;
      uint8 *dbBytes;
      size_t size;

      if (   utf16In[codeUnitIndex] < 0xD800
          || utf16In[codeUnitIndex] > 0xDFFF) {
         // Non-surrogate UTF-16 code units directly represent a code point.
         codePoint = utf16In[codeUnitIndex];
      } else {
         static const uint32 SURROGATE_OFFSET =
            (0xD800 << 10UL) + 0xDC00 - 0x10000;

         uint16 surrogateLead = utf16In[codeUnitIndex];
         uint16 surrogateTrail;

         // We need one more code unit for the trailing surrogate.
         codeUnitIndex++;
         if (codeUnitIndex == numCodeUnits) {
            return FALSE;
         }

         surrogateTrail = utf16In[codeUnitIndex];

         // Ensure we have a lead surrogate followed by a trail surrogate.
         if (   surrogateLead > 0xDBFF
             || surrogateTrail < 0xDC00
             || surrogateTrail > 0xDFFF) {
            return FALSE;
         }

         /*
          * To get a code point between 0x10000 and 0x10FFFF (2^16 to
          * (2^21) - 1):
          *
          * 1) Ensure surrogateLead is in the range [0xD800, 0xDBFF]
          *
          * 2) Ensure surrogateTrail is in the range [0xDC00, 0xDFFF]
          *
          * 3) Mask off all but the low 10 bits of lead and shift that
          *    left 10 bits: ((surrogateLead << 10) - (0xD800 << 10))
          *    -> result [0, 0xFFC00]
          *
          * 4) Add to that the low 10 bits of trail: (surrogateTrail - 0xDC00)
          *    -> result [0, 0xFFFFF]
          *
          * 5) Add to that 0x10000:
          *    -> result [0x10000, 0x10FFFF]
          */
         codePoint = ((uint32)surrogateLead << 10UL) +
            (uint32)surrogateTrail - SURROGATE_OFFSET;

         ASSERT(codePoint >= 0x10000 && codePoint <= 0x10FFFF);
      }

      size = DynBuf_GetSize(db);

      // We'll need at most 4 more bytes for this code point.
      if (   DynBuf_GetAllocatedSize(db) < size + 4
          && DynBuf_Enlarge(db, size + 4) == FALSE) {
         return FALSE;
      }

      dbBytes = (uint8 *)DynBuf_Get(db) + size;

      // Convert the code point to UTF-8.
      if (codePoint <= 0x007F) {
         // U+0000 - U+007F: 1 byte of UTF-8.
         dbBytes[0] = codePoint;
         size += 1;
      } else if (codePoint <= 0x07FF) {
         // U+0080 - U+07FF: 2 bytes of UTF-8.
         dbBytes[0] = 0xC0 | (codePoint >> 6);
         dbBytes[1] = 0x80 | (codePoint & 0x3F);
         size += 2;
      } else if (codePoint <= 0xFFFF) {
         // U+0800 - U+FFFF: 3 bytes of UTF-8.
         dbBytes[0] = 0xE0 | (codePoint >> 12);
         dbBytes[1] = 0x80 | ((codePoint >> 6) & 0x3F);
         dbBytes[2] = 0x80 | (codePoint & 0x3F);
         size += 3;
      } else {
         /*
          * U+10000 - U+10FFFF: 4 bytes of UTF-8.
          *
          * See the surrogate pair handling block above for the math
          * that ensures we're in the range [0x10000, 0x10FFFF] here.
          */
         ASSERT(codePoint <= 0x10FFFF);
         dbBytes[0] = 0xF0 | (codePoint >> 18);
         dbBytes[1] = 0x80 | ((codePoint >> 12) & 0x3F);
         dbBytes[2] = 0x80 | ((codePoint >> 6) & 0x3F);
         dbBytes[3] = 0x80 | (codePoint & 0x3F);
         size += 4;
      }

      DynBuf_SetSize(db, size);
   }

   return TRUE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16leToUtf8 --
 *
 *    Convert the content of a buffer (that uses the UTF-16LE encoding) into
 *    another buffer (that uses the UTF-8 encoding). --hpreg
 *
 *    The operation is inversible (its inverse is CodeSet_Utf8ToUtf16le).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf16leToUtf8(char const *bufIn,     // IN
                      size_t sizeIn,   // IN
                      char **bufOut,         // OUT
                      size_t *sizeOut) // OUT
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSet_Utf16leToUtf8_Db(bufIn, sizeIn, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToUtf16le --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the UTF-16LE encoding). --hpreg
 *
 *    The operation is inversible (its inverse is CodeSet_Utf16leToUtf8).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8ToUtf16le(char const *bufIn,     // IN
                      size_t sizeIn,   // IN
                      char **bufOut,         // OUT
                      size_t *sizeOut) // OUT
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetUtf8ToUtf16le(bufIn, sizeIn, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8FormDToUtf8FormC  --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) 
 *    which is in normal form D (decomposed) into another buffer
 *    (that uses the UTF-8 encoding) and is normalized as
 *    precomposed (Normalization Form C).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains a NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    '*bufOut' contains the allocated, NUL terminated buffer.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8FormDToUtf8FormC(char const *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut)       // OUT
{
#if defined(__APPLE__)
   DynBuf db;
   Bool ok;
   DynBuf_Init(&db);
   ok = CodeSetUtf8Normalize(bufIn, sizeIn, TRUE, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8FormCToUtf8FormD  --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) 
 *    which is in normal form C (precomposed) into another buffer
 *    (that uses the UTF-8 encoding) and is normalized as
 *    decomposed (Normalization Form D).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains a NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    '*bufOut' contains the allocated, NUL terminated buffer.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8FormCToUtf8FormD(char const *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut)       // OUT
{
#if defined(__APPLE__)
   DynBuf db;
   Bool ok;
   DynBuf_Init(&db);
   ok = CodeSetUtf8Normalize(bufIn, sizeIn, FALSE, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_CurrentToUtf16le --
 *
 *    Convert the content of a buffer (that uses the current encoding) into
 *    another buffer (that uses the UTF-16LE encoding).
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_CurrentToUtf16le(char const *bufIn,     // IN
                        size_t sizeIn,    // IN
                        char **bufOut,          // OUT
                        size_t *sizeOut)  // OUT
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
#if defined(CURRENT_IS_UTF8)
   ok = CodeSetUtf8ToUtf16le(bufIn, sizeIn, &db);
#elif defined(USE_ICONV)
   ok = CodeSetGenericToGeneric(CodeSet_GetCurrentCodeSet(), bufIn, sizeIn,
                                "UTF-16LE", &db, CSGTG_NORMAL);
#elif defined(_WIN32)
   /* XXX We should probably use CP_THREAD_ACP on Windows 2000/XP. */
   ok = CodeSetGenericToUtf16le(CP_ACP, bufIn, sizeIn, &db);
#else
   ok = FALSE;
#endif
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16leToCurrent --
 *
 *    Convert the content of a buffer (that uses the UTF-16 little endian
 *    encoding) into another buffer (that uses the current encoding)
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf16leToCurrent(char const *bufIn,     // IN
                         size_t sizeIn,   // IN
                         char **bufOut,         // OUT
                         size_t *sizeOut) // OUT
{
#if defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetGenericToGeneric("UTF-16LE", bufIn, sizeIn,
                                CodeSet_GetCurrentCodeSet(), &db, CSGTG_NORMAL);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   return CodeSetUtf16leToCurrent(bufIn, sizeIn, bufOut, sizeOut);
#else // Not Windows or Linux or Solaris
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16beToCurrent --
 *
 *    Convert the content of a buffer (that uses the UTF-16 big endian
 *    encoding) into another buffer (that uses the current encoding)
 *
 * Results:
 *    TRUE on success: '*bufOut' contains the allocated, NUL terminated buffer.
 *                     If not NULL, '*sizeOut' contains the size of the buffer
 *                     (excluding the NUL terminator)
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf16beToCurrent(char const *bufIn,     // IN
                         size_t sizeIn,   // IN
                         char **bufOut,         // OUT
                         size_t *sizeOut) // OUT
{
#if defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetGenericToGeneric("UTF-16BE", bufIn, sizeIn,
                                CodeSet_GetCurrentCodeSet(), &db, CSGTG_NORMAL);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char c;
   char *bufIn_dup;
   int i;
   Bool status = FALSE;

   bufIn_dup = NULL;

   /* sizeIn must be even */
   ASSERT((sizeIn & 1) == 0);

   /* Make a non-const copy */
   bufIn_dup = malloc(sizeIn);
   if (bufIn_dup == NULL) {
      goto error;
   }
   memcpy(bufIn_dup, bufIn, sizeIn);

   /* Swap pairs of bytes */
   for (i = 0; i < sizeIn; i += 2) {
      c = bufIn_dup[i];
      bufIn_dup[i] = bufIn_dup[i + 1];
      bufIn_dup[i + 1] = c;
   }

   status = CodeSetUtf16leToCurrent(bufIn_dup, sizeIn, bufOut, sizeOut);

  error:
   free(bufIn_dup);
   return status;
#else // Not Windows or Linux or Solaris
   return FALSE;
#endif
}


#if defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * IsWin95 --
 *
 *      Is the current OS Windows 95?
 *
 * Results:
 *      TRUE if this is Win95
 *      FALSE if this is not Win95.
 *
 * Side effects:
 *      none
 *
 *-----------------------------------------------------------------------------
 */

static Bool
IsWin95(void)
{
   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;

   /*
    * Try calling GetVersionEx using the OSVERSIONINFOEX structure.
    * If that fails, try using the OSVERSIONINFO structure.
    */

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if(!(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *) &osvi))) {
      /* If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO. */
      osvi.dwOSVersionInfoSize = sizeof (OSVERSIONINFO);
      if (!GetVersionEx((OSVERSIONINFO *) &osvi)) {
         return FALSE;
      }
   }

   if (osvi.dwPlatformId == VER_PLATFORM_WIN32_WINDOWS) {
      if (osvi.dwMajorVersion == 4 && osvi.dwMinorVersion == 0) {
         return TRUE;
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * GetInvalidCharsFlag --
 *
 *    The flag MB_ERR_INVALID_CHARS can only be passed to MultiByteToWideChar
 *    on Win2000 SP4 or later.  If it's passed to an NT or 9x system,
 *    MultiByteToWideChar fails with ERR_INVALID_FLAGS.
 *
 * Results:
 *      returns MB_ERR_INVALID_CHARS if this flag is supported under current OS
 *      returns zero if the flag would case MultiByteToWideChar failure.
 *
 * Side effects:
 *      none
 *
 *-----------------------------------------------------------------------------
 */

static DWORD
GetInvalidCharsFlag(void)
{
   static volatile Bool bFirstCall = TRUE;
   static DWORD retval;

   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;

   if (!bFirstCall) {   // We can return a cached result for subsequent calls
      return retval;
   }

   /*
    * Try calling GetVersionEx using the OSVERSIONINFOEX structure.
    */

   ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
   osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

   if(!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *) &osvi))) {
      /*
       * If GetVersionEx failed, we are running something earlier than NT4+SP6,
       * thus we cannot use MB_ERR_INVALID_CHARS
       */
       retval = 0;
       bFirstCall = FALSE;
       return retval;
   }

   if (osvi.dwMajorVersion > 5) {
      retval = MB_ERR_INVALID_CHARS;  // Vista or later
   } else if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion > 0) {
      retval = MB_ERR_INVALID_CHARS;  // XP, 2003
   } else if (osvi.dwMajorVersion == 5
               && osvi.dwMinorVersion == 0
               && osvi.wServicePackMajor >= 4) {  // Win2000 + SP4
      retval = MB_ERR_INVALID_CHARS;
   } else {
      retval = 0;   // Do not use MB_ERR_INVALID_CHARS on this OS.
   }

   bFirstCall = FALSE;
   return retval;
}
#endif
