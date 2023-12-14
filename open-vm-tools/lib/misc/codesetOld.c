/*********************************************************
 * Copyright (C) 1998-2017,2019,2021-2023 VMware, Inc. All rights reserved.
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
 * codesetOld.c --
 *
 *    The old codeset implementation that depends on system libraries.
 *    Used for fallback if ICU isn't available.
 */


#if defined(_WIN32)
#   include <windows.h>
#   include <malloc.h>
#   include <str.h>
#else
#   if defined(__linux__)
#      define _GNU_SOURCE   // see nl_langinfo_l explanation below
#   endif
#   include <string.h>
#   include <stdlib.h>
#   include <unistd.h>
#   include <errno.h>
#endif

#if defined(__APPLE__)
#   include <CoreFoundation/CoreFoundation.h> /* for CFString */
#endif

#include "vmware.h"
#include "codeset.h"
#include "codesetOld.h"
#include "unicodeTypes.h"
#include "util.h"
#include "str.h"

#if defined(USE_ICONV)
   /*
    * Use nl_langinfo_l on Linux.  To get the nl_langinfo_l
    * related definitions in local.h, we need to define _GNU_SOURCE,
    * which has to be done above because one of the other standard include
    * files sucks it in.
    */
   #include <locale.h>
   #if defined(__linux__) && !defined(LC_CTYPE_MASK)  // Prior to glibc 2.3
      #define LC_CTYPE_MASK (1 << LC_CTYPE)
      #define locale_t __locale_t
      #define newlocale __newlocale
      #define freelocale __freelocale
      #define nl_langinfo_l __nl_langinfo_l
   #endif
   #include <dlfcn.h>
   #include <iconv.h>
   #include <langinfo.h>
#endif


#if defined(__FreeBSD__) || defined(sun)
static const char nul[] = {'\0', '\0'};
#else
static const wchar_t nul = L'\0';
#endif

#ifndef USE_ICONV
static Bool CodeSetOldIso88591ToUtf8Db(char const *bufIn, size_t sizeIn,
                                       unsigned int flags, DynBuf *db);
#endif

#if defined __ANDROID__
/*
 * This function will be called when buffer overflows. It will panic the app.
 * It should be in libc.a. But new Android ndk does not have it.
 */
void __stack_chk_fail(void);
void __stack_chk_fail_local (void)
{
   __stack_chk_fail();
}
#endif

#if defined(CURRENT_IS_UTF8) || defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOldDuplicateStr --
 *
 *    Duplicate input string, appending zero terminator to its end.  Only
 *    used on Windows and on platforms where current encoding is always
 *    UTF-8, on other iconv-capable platforms we just use iconv even for
 *    UTF-8 to UTF-8 translation.
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
CodeSetOldDuplicateStr(const char *bufIn,  // IN: Input string
                       size_t  sizeIn,     // IN: Input string length
                       char **bufOut,      // OUT: "Converted" string
                       size_t *sizeOut)    // OUT/OPT: Length of string
{
   char *myBufOut;
   size_t newSize = sizeIn + sizeof *myBufOut;

   if (newSize < sizeIn) {   // Prevent integer overflow
      return FALSE;
   }

   myBufOut = malloc(newSize);
   if (myBufOut == NULL) {
      return FALSE;
   }

   memcpy(myBufOut, bufIn, sizeIn);
   myBufOut[sizeIn] = '\0';

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
 * CodeSetOldDynBufFinalize --
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
CodeSetOldDynBufFinalize(Bool ok,          // IN: Earlier steps succeeded
                         DynBuf *db,       // IN: Buffer with converted string
                         char **bufOut,    // OUT: Converted string
                         size_t *sizeOut)  // OUT/OPT: Length of string in bytes
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
 * CodeSetOldUtf8ToUtf16leDb --
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
CodeSetOldUtf8ToUtf16leDb(const char *bufIn,   // IN:
                          size_t      sizeIn,  // IN:
                          DynBuf     *db)      // IN:
{
   const char *bufEnd = bufIn + sizeIn;
   size_t currentSize;
   size_t allocatedSize;
   uint16 *buf;

   currentSize = DynBuf_GetSize(db);
   allocatedSize = DynBuf_GetAllocatedSize(db);
   buf = (uint16 *)((char *)DynBuf_Get(db) + currentSize);
   while (bufIn < bufEnd) {
      size_t neededSize;
      uint32 uniChar;
      int n = CodeSet_GetUtf8(bufIn, bufEnd, &uniChar);

      if (n <= 0) {
         return FALSE;
      }
      bufIn += n;

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


#if defined(_WIN32) // {

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
 * CodeSetOldGenericToUtf16leDb --
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
CodeSetOldGenericToUtf16leDb(UINT codeIn,        // IN:
                             char const *bufIn,  // IN:
                             size_t sizeIn,      // IN:
                             DynBuf *db)         // IN:
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
         result = MultiByteToWideChar(codeIn, flags, bufIn, sizeIn,
                     (wchar_t *)((char *)DynBuf_Get(db) + initialSize),
                     (DynBuf_GetAllocatedSize(db) - initialSize) /
                                      sizeof(wchar_t));

         if (result == 0) {
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
 * CodeSetOldUtf16leToGeneric --
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
CodeSetOldUtf16leToGeneric(char const *bufIn,  // IN:
                           size_t sizeIn,      // IN:
                           UINT codeOut,       // IN:
                           DynBuf *db)         // IN:
{
   /*
    * Undocumented: calling WideCharToMultiByte() with sizeIn == 0 returns 0
    * with GetLastError() set to ERROR_INVALID_PARAMETER. Isn't this API
    * robust? --hpreg
    */

   if (sizeIn) {
      size_t initialSize;
      Bool canHaveSubstitution = codeOut != CP_UTF8 && codeOut != CP_UTF7;

      initialSize = DynBuf_GetSize(db);
      for (;;) {
         int result;
         DWORD error;
         BOOL usedSubstitution = FALSE;

         if (DynBuf_Enlarge(db, 1) == FALSE) {
            return FALSE;
         }

         result = WideCharToMultiByte(codeOut,
                     canHaveSubstitution ? WC_NO_BEST_FIT_CHARS : 0,
                     (wchar_t const *)bufIn,
                     sizeIn / sizeof(wchar_t),
                     (char *)DynBuf_Get(db) + initialSize,
                     DynBuf_GetAllocatedSize(db) - initialSize,
                     NULL,
                     canHaveSubstitution ? &usedSubstitution : NULL);

         if (usedSubstitution) {
            return FALSE;
         }

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
 * CodeSetOldUtf16leToCurrent --
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
CodeSetOldUtf16leToCurrent(char const *bufIn,  // IN:
                           size_t sizeIn,      // IN:
                           char **bufOut,      // OUT:
                           size_t *sizeOut)    // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   /* XXX We should probably use CP_THREAD_ACP on Windows 2000/XP --hpreg */
   ok = CodeSetOldUtf16leToGeneric(bufIn, sizeIn, CP_ACP, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}

#endif // }


#if defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf8Normalize --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the UTF-8 encoding) that is in precomposed
 *    (Normalization Form C) or decomposed (Normalization Form D).
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
CodeSetOld_Utf8Normalize(char const *bufIn,  // IN:
                         size_t sizeIn,      // IN:
                         Bool precomposed,   // IN:
                         DynBuf *db)         // OUT:
{
   Bool ok = FALSE;
   CFStringRef str = NULL;
   CFMutableStringRef mutStr = NULL;
   CFIndex len, lenMut;
   size_t initialSize = DynBuf_GetSize(db);

   str = CFStringCreateWithCString(NULL, bufIn, kCFStringEncodingUTF8);
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

   CFStringNormalize(mutStr, (precomposed ? kCFStringNormalizationFormC :
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

   len = CFStringGetMaximumSizeForEncoding(lenMut, kCFStringEncodingUTF8);
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

   ok = CFStringGetCString(mutStr, (char *)DynBuf_Get(db),
                           len + 1, kCFStringEncodingUTF8);
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


#if defined(USE_ICONV) // {
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOldGetCodeSetFromLocale --
 *
 *    Extract the native code set from LC_CTYPE.  
 *
 * Results:
 *
 *    The name of the current code set on success.  The return value depends
 *    on how LC_CTYPE is set in the locale, even if setlocale has not been
 *    previously called.
 *
 * Side effects:
 *
 *    May briefly set and restore locale on some systems, which is not
 *    thread-safe.
 *
 *-----------------------------------------------------------------------------
 */

static char *
CodeSetOldGetCodeSetFromLocale(void)
{
   char *codeset;

#if defined(__linux__) || defined(__EMSCRIPTEN__)

   locale_t new = newlocale(LC_CTYPE_MASK, "", NULL);
   if (!new) {
      /*
       * If the machine is configured incorrectly (no current locale),
       * newlocale() could return NULL.  Try to fall back on the "C"
       * locale.
       */

      new = newlocale(LC_CTYPE_MASK, "C", NULL);
      ASSERT(new);
   }
   codeset = Util_SafeStrdup(nl_langinfo_l(CODESET, new));
   freelocale(new);

#elif defined(sun)

   char *locale = setlocale(LC_CTYPE, NULL);

   if (!setlocale(LC_CTYPE, "")) {
      /*
       * If the machine is configured incorrectly (no current locale),
       * setlocale() can fail.  Try to fall back on the "C" locale.
       */

      setlocale(LC_CTYPE, "C");
   }
   codeset = Util_SafeStrdup(nl_langinfo(CODESET));
   setlocale(LC_CTYPE, locale);

#else
#error
#endif

   return codeset;
}
#endif // } USE_ICONV


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_GetCurrentCodeSet --
 *
 *    Return native code set name - always "UTF-8" on Apple, FreeBSD,
 *    old Linux, and ESX. Obtained from GetACP on Windows and
 *    nl_langinfo on Linux, Solaris, etc.  On first invocation
 *    initialize a cache with the code set name.
 *
 * Results:
 *
 *    The name of the current code set on success.  On systems that
 *    use iconv the return value depends on how LC_CTYPE is set in the
 *    locale, even if setlocale has not been previously called.
 *
 * Side effects:
 *
 *    During first invocation may briefly set and restore locale on
 *    some systems, which is not thread-safe.
 *
 *-----------------------------------------------------------------------------
 */

const char *
CodeSetOld_GetCurrentCodeSet(void)
{
#if defined(CURRENT_IS_UTF8) || defined(VM_WIN_UWP)
   return "UTF-8";
#elif defined(_WIN32)
   static char ret[20];  // max is "windows-4294967296"
   if (ret[0] == '\0') {
      Str_Sprintf(ret, sizeof ret, "windows-%u", GetACP());
   }

   return ret;
#elif defined(USE_ICONV)
   static const char *cachedCodeset;

   /*
    * Mirror GLib behavior:
    *
    *    $G_FILENAME_ENCODING can have one or more encoding names
    *    in a comma separated list.
    *
    *    If the first entry in $G_FILENAME_ENCODING is set to
    *    "@locale", get the code set from the environment.
    *
    *    If the first entry in $G_FILENAME_ENCODING is not set to
    *    "@locale", then it is the encoding name.
    *
    *    If $G_FILENAME_ENCODING is not set and $G_BROKEN_FILENAMES
    *    is set, the get the code set from the environment.
    *
    *    If none of the above are met, the code set is UTF-8.
    *
    *    XXX - TODO - Support multiple encodings in the list.
    *    While G_FILENAME_ENCODING is documented to be a list,
    *    the current implementation (GLib 2.16) ignores all but
    *    the first entry when converting to/from UTF-8.
    */

   if (cachedCodeset == NULL) {
      char *gFilenameEncoding = getenv("G_FILENAME_ENCODING");

      if (gFilenameEncoding != NULL && *gFilenameEncoding != '\0') {
         char *p;

         gFilenameEncoding = Util_SafeStrdup(gFilenameEncoding);
         p = strchr(gFilenameEncoding, ',');

         if (p != NULL) {
            *p = '\0';
         }
         if (!strcmp(gFilenameEncoding, "@locale")) {
            free(gFilenameEncoding);
            cachedCodeset = CodeSetOldGetCodeSetFromLocale();

            return cachedCodeset;
         }
         cachedCodeset = gFilenameEncoding;

         return cachedCodeset;
      }

      if (getenv("G_BROKEN_FILENAMES")) {
         cachedCodeset = CodeSetOldGetCodeSetFromLocale();

         return cachedCodeset;
      }

      cachedCodeset = "UTF-8";
   }

   return cachedCodeset;
#else
#error
#endif
}

#if defined(USE_ICONV) // {

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOldIconvOpen --
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
CodeSetOldIconvOpen(const char  *codeIn,   // IN:
                    const char  *codeOut,  // IN:
                    unsigned int flags)    // IN:
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
 * CodeSetOld_GenericToGenericDb --
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

Bool
CodeSetOld_GenericToGenericDb(char const *codeIn,   // IN:
                              char const *bufIn,    // IN:
                              size_t sizeIn,        // IN:
                              char const *codeOut,  // IN:
                              unsigned int flags,   // IN:
                              DynBuf *db)           // IN/OUT:
{
   iconv_t cd;

   ASSERT(codeIn);
   ASSERT(sizeIn == 0 || bufIn);
   ASSERT(codeOut);
   ASSERT(db);

   // XXX make CodeSetOldIconvOpen happy
   if (flags != 0) {
      flags = CSGTG_TRANSLIT | CSGTG_IGNORE;
   }

   cd = CodeSetOldIconvOpen(codeIn, codeOut, flags);
   if (cd == (iconv_t)-1) {
      return FALSE;
   }

   for (;;) {
      size_t size;
      size_t newSize;
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
      newSize = size + 4;

      if (newSize < size) {  // Prevent integer overflow.
         goto error;
      }

      if (DynBuf_Enlarge(db, newSize) == FALSE) {
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

#if defined(__linux__) || defined(__EMSCRIPTEN__)
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

#else // USE_ICONV } {

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_GenericToGenericDb
 *
 *    This non-iconv version can only handle common encodings.
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

Bool
CodeSetOld_GenericToGenericDb(char const *codeIn,   // IN:
                              char const *bufIn,    // IN:
                              size_t sizeIn,        // IN:
                              char const *codeOut,  // IN:
                              unsigned int flags,   // IN:
                              DynBuf *db)           // IN/OUT:
{
   Bool ret = FALSE;
   StringEncoding encIn = Unicode_EncodingNameToEnum(codeIn);
   StringEncoding encOut = Unicode_EncodingNameToEnum(codeOut);
   StringEncoding rawCurEnc = Unicode_GetCurrentEncoding();
   char *bufOut = NULL;
   size_t sizeOut;

   /*
    * Trivial case.
    */

   if ((sizeIn == 0) || (bufIn == NULL)) {
      ret = TRUE;
      goto exit;
   }

   if (encIn == encOut) {
      if (encIn == STRING_ENCODING_UTF8) {
         if (!CodeSetOld_Utf8ToUtf16le(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (encIn == STRING_ENCODING_UTF16_LE) {
         if (!CodeSetOld_Utf16leToUtf8(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (encIn == STRING_ENCODING_UTF16_BE) {
         if (!CodeSetOld_Utf16beToUtf8(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (encIn == STRING_ENCODING_US_ASCII) {
         if (!CodeSetOld_AsciiToUtf8(bufIn, sizeIn, 0, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (encIn == rawCurEnc) {
         if (!CodeSetOld_CurrentToUtf8(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      }
      free(bufOut);
      bufOut = NULL;
      if (!DynBuf_Append(db, bufIn, sizeIn)) {
         goto exit;
      }
   } else if (rawCurEnc == encIn) {
      if (STRING_ENCODING_UTF8 == encOut) {
         if (!CodeSetOld_CurrentToUtf8(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (STRING_ENCODING_UTF16_LE == encOut) {
         if (!CodeSetOld_CurrentToUtf16le(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else if (STRING_ENCODING_UTF8 == encIn) {
      if (rawCurEnc == encOut) {
         if (!CodeSetOld_Utf8ToCurrent(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (STRING_ENCODING_UTF16_LE == encOut) {
         if (!CodeSetOldUtf8ToUtf16leDb(bufIn, sizeIn, db)) {
            goto exit;
         }
      } else if (STRING_ENCODING_US_ASCII == encOut) {
         if (!CodeSetOld_Utf8ToAsciiDb(bufIn, sizeIn, flags, db)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else if (STRING_ENCODING_UTF16_LE == encIn) {
      if (rawCurEnc == encOut) {
         if (!CodeSetOld_Utf16leToCurrent(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (STRING_ENCODING_UTF8 == encOut) {
         if (!CodeSetOld_Utf16leToUtf8Db(bufIn, sizeIn, db)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else if (STRING_ENCODING_UTF16_BE == encIn) {
      if (rawCurEnc == encOut) {
         if (!CodeSetOld_Utf16beToCurrent(bufIn, sizeIn, &bufOut, &sizeOut)) {
            goto exit;
         }
      } else if (STRING_ENCODING_UTF8 == encOut) {
         if (!CodeSetOld_Utf16beToUtf8Db(bufIn, sizeIn, db)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else if (STRING_ENCODING_US_ASCII == encIn) {
      if (STRING_ENCODING_UTF8 == encOut) {
         if (!CodeSetOld_AsciiToUtf8Db(bufIn, sizeIn, flags, db)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else if (STRING_ENCODING_ISO_8859_1 == encIn) {
      if (STRING_ENCODING_UTF8 == encOut) {
         if (!CodeSetOldIso88591ToUtf8Db(bufIn, sizeIn, flags, db)) {
            goto exit;
         }
      } else {
         goto exit;
      }
   } else {
      goto exit;
   }

   if (bufOut != NULL) {
      if (DynBuf_GetSize(db) == 0) {
         DynBuf_Attach(db, sizeOut, bufOut);
         bufOut = NULL;
      } else {
         if (!DynBuf_Append(db, bufOut, sizeOut)) {
            goto exit;
         }
      }
   }

   ret = TRUE;

  exit:
   free(bufOut);

   return ret;
}

#endif // USE_ICONV }

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_GenericToGeneric --
 *
 *    Non-db version of CodeSetOld_GenericToGenericDb.
 *
 * Results:
 *    TRUE on success, plus allocated string
 *    FALSE on failure
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_GenericToGeneric(const char *codeIn,   // IN:
                            const char *bufIn,    // IN:
                            size_t sizeIn,        // IN:
                            const char *codeOut,  // IN:
                            unsigned int flags,   // IN:
                            char **bufOut,        // OUT:
                            size_t *sizeOut)      // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb(codeIn, bufIn, sizeIn,
                                      codeOut, flags, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


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
 * CodeSetOld_Utf8ToCurrent --
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
CodeSetOld_Utf8ToCurrent(char const *bufIn,  // IN:
                         size_t sizeIn,      // IN:
                         char **bufOut,      // OUT:
                         size_t *sizeOut)    // OUT:
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetOldDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb("UTF-8", bufIn, sizeIn,
                                      CodeSetOld_GetCurrentCodeSet(),
                                      0, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char *buf;
   size_t size;
   Bool status;

   if (CodeSetOld_Utf8ToUtf16le(bufIn, sizeIn, &buf, &size) == FALSE) {
      return FALSE;
   }

   status = CodeSetOldUtf16leToCurrent(buf, size, bufOut, sizeOut);
   free(buf);

   return status;
#else
#error
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_CurrentToUtf8 --
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
CodeSetOld_CurrentToUtf8(char const *bufIn,  // IN:
                      size_t sizeIn,         // IN:
                      char **bufOut,         // OUT:
                      size_t *sizeOut)       // OUT:
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetOldDuplicateStr(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb(CodeSetOld_GetCurrentCodeSet(), bufIn,
                                      sizeIn, "UTF-8", 0, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   char *buf;
   size_t size;
   Bool status;

   if (CodeSetOld_CurrentToUtf16le(bufIn, sizeIn, &buf, &size) == FALSE) {
      return FALSE;
   }

   status = CodeSetOld_Utf16leToUtf8(buf, size, bufOut, sizeOut);
   free(buf);

   return status;
#else
#error
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16leToUtf8Db --
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
CodeSetOld_Utf16leToUtf8Db(char const *bufIn,  // IN
                           size_t sizeIn,      // IN
                           DynBuf *db)         // IN
{
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
      size_t newSize;

      if (utf16In[codeUnitIndex] < 0xD800 ||
          utf16In[codeUnitIndex] > 0xDFFF) {
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
         if (surrogateLead > 0xDBFF ||
             surrogateTrail < 0xDC00 ||
             surrogateTrail > 0xDFFF) {
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
      newSize = size + 4;

      // We'll need at most 4 more bytes for this code point.
      if ((newSize < size) ||  // Prevent integer overflow
          (DynBuf_GetAllocatedSize(db) < newSize &&
           DynBuf_Enlarge(db, newSize) == FALSE)) {
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
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16leToUtf8 --
 *
 *    Convert the content of a buffer (that uses the UTF-16LE encoding) into
 *    another buffer (that uses the UTF-8 encoding). --hpreg
 *
 *    The operation is inversible (its inverse is CodeSetOld_Utf8ToUtf16le).
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
CodeSetOld_Utf16leToUtf8(char const *bufIn,  // IN:
                         size_t sizeIn,      // IN:
                         char **bufOut,      // OUT:
                         size_t *sizeOut)    // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_Utf16leToUtf8Db(bufIn, sizeIn, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf8ToUtf16le --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the UTF-16LE encoding). --hpreg
 *
 *    The operation is inversible (its inverse is CodeSetOld_Utf16leToUtf8).
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
CodeSetOld_Utf8ToUtf16le(char const *bufIn,  // IN:
                         size_t sizeIn,      // IN:
                         char **bufOut,      // OUT:
                         size_t *sizeOut)    // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOldUtf8ToUtf16leDb(bufIn, sizeIn, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf8FormDToUtf8FormC  --
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
CodeSetOld_Utf8FormDToUtf8FormC(char const *bufIn,  // IN:
                                size_t sizeIn,      // IN:
                                char **bufOut,      // OUT:
                                size_t *sizeOut)    // OUT:
{
#if defined(__APPLE__)
   DynBuf db;
   Bool ok;
   DynBuf_Init(&db);
   ok = CodeSetOld_Utf8Normalize(bufIn, sizeIn, TRUE, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf8FormCToUtf8FormD  --
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
CodeSetOld_Utf8FormCToUtf8FormD(char const *bufIn,  // IN:
                                size_t sizeIn,      // IN:
                                char **bufOut,      // OUT:
                                size_t *sizeOut)    // OUT:
{
#if defined(__APPLE__)
   DynBuf db;
   Bool ok;
   DynBuf_Init(&db);
   ok = CodeSetOld_Utf8Normalize(bufIn, sizeIn, FALSE, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_CurrentToUtf16le --
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
CodeSetOld_CurrentToUtf16le(char const *bufIn,  // IN:
                            size_t sizeIn,      // IN:
                            char **bufOut,      // OUT:
                            size_t *sizeOut)    // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
#if defined(CURRENT_IS_UTF8)
   ok = CodeSetOldUtf8ToUtf16leDb(bufIn, sizeIn, &db);
#elif defined(USE_ICONV)
   ok = CodeSetOld_GenericToGenericDb(CodeSetOld_GetCurrentCodeSet(), bufIn,
                                      sizeIn, "UTF-16LE", 0, &db);
#elif defined(_WIN32)
   /* XXX We should probably use CP_THREAD_ACP on Windows 2000/XP. */
   ok = CodeSetOldGenericToUtf16leDb(CP_ACP, bufIn, sizeIn, &db);
#else
#error
#endif

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16leToCurrent --
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
CodeSetOld_Utf16leToCurrent(char const *bufIn,  // IN:
                            size_t sizeIn,      // IN:
                            char **bufOut,      // OUT:
                            size_t *sizeOut)    // OUT:
{
#if defined(CURRENT_IS_UTF8)
   return CodeSetOld_Utf16leToUtf8(bufIn, sizeIn, bufOut, sizeOut);
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb("UTF-16LE", bufIn, sizeIn,
                                      CodeSetOld_GetCurrentCodeSet(), 0, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
#elif defined(_WIN32)
   return CodeSetOldUtf16leToCurrent(bufIn, sizeIn, bufOut, sizeOut);
#else
#error
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16beToCurrent --
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
CodeSetOld_Utf16beToCurrent(char const *bufIn,  // IN:
                            size_t sizeIn,      // IN:
                            char **bufOut,      // OUT:
                            size_t *sizeOut)    // OUT:
{
#if defined(CURRENT_IS_UTF8)
   Bool status;
   char *temp;

   if ((temp = malloc(sizeIn)) == NULL) {
      return FALSE;
   }
   ASSERT((sizeIn & 1) == 0);
   ASSERT((ssize_t) sizeIn >= 0);
   swab(bufIn, temp, sizeIn);
   status = CodeSetOld_Utf16leToUtf8(temp, sizeIn, bufOut, sizeOut);
   free(temp);

   return status;
#elif defined(USE_ICONV)
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb("UTF-16BE", bufIn, sizeIn,
                                      CodeSetOld_GetCurrentCodeSet(), 0, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
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

   status = CodeSetOldUtf16leToCurrent(bufIn_dup, sizeIn, bufOut, sizeOut);

  error:
   free(bufIn_dup);

   return status;
#else
#error
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16beToUtf8 --
 *
 *    Convert the content of a buffer (that uses the UTF-16BE encoding) into
 *    another buffer (that uses the UTF-8 encoding). 
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
CodeSetOld_Utf16beToUtf8(char const *bufIn,  // IN:
                         size_t sizeIn,      // IN:
                         char **bufOut,      // OUT:
                         size_t *sizeOut)    // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_Utf16beToUtf8Db(bufIn, sizeIn, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Utf16beToUtf8Db --
 *
 *    Append the content of a buffer (that uses the UTF-16BE encoding) to a
 *    DynBuf (that uses the UTF-8 encoding). 
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
CodeSetOld_Utf16beToUtf8Db(char const *bufIn,  // IN:
                           size_t sizeIn,      // IN:
                           DynBuf *db)         // IN:
{
   int i;
   char *temp;
   Bool ret = FALSE;

   if ((temp = malloc(sizeIn)) == NULL) {
      return ret;
   }
   ASSERT((sizeIn & 1) == 0);
   ASSERT((ssize_t) sizeIn >= 0);

   for (i = 0; i < sizeIn; i += 2) {
      temp[i] = bufIn[i + 1];
      temp[i + 1] = bufIn[i];
   }

   ret = CodeSetOld_Utf16leToUtf8Db(temp, sizeIn, db);
   free(temp);

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_AsciiToUtf8 --
 *
 *      Convert ASCII to UTF-8
 *
 * Results:
 *      On success, TRUE and conversion result in bufOut and sizeOut.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_AsciiToUtf8(const char *bufIn,   // IN:
                       size_t sizeIn,       // IN:
                       unsigned int flags,  // IN:
                       char **bufOut,       // OUT:
                       size_t *sizeOut)     // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_AsciiToUtf8Db(bufIn, sizeIn, flags, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_AsciiToUtf8Db --
 *
 *      Convert ASCII to UTF-8
 *
 * Results:
 *      On success, TRUE and conversion result appended to db.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_AsciiToUtf8Db(char const *bufIn,   // IN:
                         size_t sizeIn,       // IN:
                         unsigned int flags,  // IN:
                         DynBuf *db)          // OUT:
{
   size_t oldSize = DynBuf_GetSize(db);
   size_t i;
   size_t last = 0;

   for (i = 0; i < sizeIn; i++) {
      if (UNLIKELY((unsigned char) bufIn[i] >= 0x80)) {
         if (flags == 0) {
            DynBuf_SetSize(db, oldSize);

            return FALSE;
         }
         DynBuf_Append(db, bufIn + last, i - last);
         if ((flags & CSGTG_TRANSLIT) != 0) {
            DynBuf_Append(db, "\xef\xbf\xbd", 3);
         }
         last = i + 1;
      }
   }
   DynBuf_Append(db, bufIn + last, i - last);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToAscii --
 *
 *      Convert UTF-8 to ASCII
 *
 * Results:
 *      On success, TRUE and conversion result in bufOut and sizeOut.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_Utf8ToAscii(const char *bufIn,   // IN:
                       size_t sizeIn,       // IN:
                       unsigned int flags,  // IN:
                       char **bufOut,       // OUT:
                       size_t *sizeOut)     // OUT:
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSetOld_Utf8ToAsciiDb(bufIn, sizeIn, flags, &db);

   return CodeSetOldDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToAsciiDb --
 *
 *      Convert UTF-8 to ASCII
 *
 * Results:
 *      On success, TRUE and conversion result appended to db.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_Utf8ToAsciiDb(char const *bufIn,   // IN:
                         size_t sizeIn,       // IN:
                         unsigned int flags,  // IN:
                         DynBuf *db)          // OUT:
{
   size_t oldSize = DynBuf_GetSize(db);
   uint8 *p = (uint8 *) bufIn;
   uint8 *end = (uint8 *) bufIn + sizeIn;
   uint8 *last = p;

   for (; p < end; p++) {
      if (UNLIKELY(*p >= 0x80)) {
         int n;

         if (flags == 0) {
            DynBuf_SetSize(db, oldSize);

            return FALSE;
         }
         DynBuf_Append(db, last, p - last);
         if ((flags & CSGTG_TRANSLIT) != 0) {
            DynBuf_Append(db, "\x1a", 1);
         }
         if ((n = CodeSet_GetUtf8((char *) p, (char *) end, NULL)) > 0) {
            p += n - 1;
         }
         last = p + 1;
      }
   }
   DynBuf_Append(db, last, p - last);

   return TRUE;
}


#ifndef USE_ICONV
/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOldIso88591ToUtf8Db --
 *
 *      Convert ISO-8859-1 to UTF-8
 *
 * Results:
 *      On success, TRUE and conversion result appended to db.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
CodeSetOldIso88591ToUtf8Db(char const *bufIn,   // IN:
                           size_t sizeIn,       // IN:
                           unsigned int flags,  // IN:
                           DynBuf *db)          // OUT:
{
   size_t i;
   size_t last = 0;

   for (i = 0; i < sizeIn; i++) {
      unsigned int c = (unsigned char)bufIn[i];

      if (UNLIKELY(c >= 0x80)) {
         unsigned char buf[2];

         buf[0] = 0xC0 | (c >> 6);
         buf[1] = 0x80 | (c & 0x3F);
         DynBuf_Append(db, bufIn + last, i - last);
         DynBuf_Append(db, buf, sizeof buf);
         last = i + 1;
      }
   }
   DynBuf_Append(db, bufIn + last, i - last);

   return TRUE;
}
#endif


#if defined(_WIN32)
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
#if defined(VM_WIN_UWP)
   return MB_ERR_INVALID_CHARS;
#else
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

   ZeroMemory(&osvi, sizeof osvi);
   osvi.dwOSVersionInfoSize = sizeof osvi;

   /*
    * Starting with msvc-12.0 / SDK v8.1 GetVersionEx is deprecated.
    * Bug 1259185 tracks switching to VerifyVersionInfo.
    */

#pragma warning(push)
#pragma warning(disable : 4996) // 'function': was declared deprecated
   if (!(bOsVersionInfoEx = GetVersionEx((OSVERSIONINFO *) &osvi))) {
      /*
       * If GetVersionEx failed, we are running something earlier than NT4+SP6,
       * thus we cannot use MB_ERR_INVALID_CHARS
       */

       retval = 0;
       bFirstCall = FALSE;

       return retval;
   }
#pragma warning(pop)

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
#endif //VM_WIN_UWP
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_IsEncodingSupported --
 *
 *    Not really from the old codeset file, but we need a non-ICU way
 *    of doing this. Asking lib/unicode to cross-reference the
 *    encoding name with its internal list is functionally equivalent
 *    to what Unicode_IsEncodingSupported used to do (and still does
 *    if no ICU support is built-in).
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
CodeSetOld_IsEncodingSupported(const char *name)  // IN:
{
   ASSERT(name);

   return (STRING_ENCODING_UNKNOWN != Unicode_EncodingNameToEnum(name));
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Validate --
 *
 *    Validate a string in the given encoding.
 *
 * Results:
 *    TRUE if string is valid,
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_Validate(const char *buf,   // IN: the string
                    size_t size,       // IN: length of string
                    const char *code)  // IN: encoding
{
   DynBuf db;
   Bool ok;

   if (size == 0) {
      return TRUE;
   }

   DynBuf_Init(&db);
   ok = CodeSetOld_GenericToGenericDb(code, buf, size, "UTF-8",
                                      CSGTG_NORMAL, &db);
   DynBuf_Destroy(&db);

   return ok;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetOld_Init --
 *
 *    No-op.
 *
 * Results:
 *    TRUE.
 *
 * Side effects:
 *    See above
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSetOld_Init(UNUSED_PARAM(const char *dataDir))  // IN:
{
   return TRUE;
}
