/* **********************************************************
 * Copyright (C) 1998-2020 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * codeset.c --
 *
 *    Character set and encoding conversion functions, using ICU.
 *
 *
 *      Some definitions borrow from header files from the ICU 1.8.1
 *      library.
 *
 *      ICU 1.8.1 license follows:
 *
 *      ICU License - ICU 1.8.1 and later
 *
 *      COPYRIGHT AND PERMISSION NOTICE
 *
 *      Copyright (c) 1995-2006 International Business Machines Corporation
 *      and others
 *
 *      All rights reserved.
 *
 *           Permission is hereby granted, free of charge, to any
 *      person obtaining a copy of this software and associated
 *      documentation files (the "Software"), to deal in the Software
 *      without restriction, including without limitation the rights
 *      to use, copy, modify, merge, publish, distribute, and/or sell
 *      copies of the Software, and to permit persons to whom the
 *      Software is furnished to do so, provided that the above
 *      copyright notice(s) and this permission notice appear in all
 *      copies of the Software and that both the above copyright
 *      notice(s) and this permission notice appear in supporting
 *      documentation.
 *
 *           THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY
 *      KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 *      WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 *      PURPOSE AND NONINFRINGEMENT OF THIRD PARTY RIGHTS. IN NO EVENT
 *      SHALL THE COPYRIGHT HOLDER OR HOLDERS INCLUDED IN THIS NOTICE
 *      BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL INDIRECT OR
 *      CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 *      FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 *      CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *      OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *           Except as contained in this notice, the name of a
 *      copyright holder shall not be used in advertising or otherwise
 *      to promote the sale, use or other dealings in this Software
 *      without prior written authorization of the copyright holder.
 */

#if defined(_WIN32)
#   include <windows.h>
#   include <malloc.h>
#   include <str.h>
#   include "windowsUtil.h"
#else
#   define _GNU_SOURCE
#   include <string.h>
#   include <stdlib.h>
#   include <errno.h>
#   include <su.h>
#   include <sys/stat.h>
#   include "hostinfo.h"
#   include "hostType.h"
#endif

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

#include <stdio.h>
#include "vmware.h"
#include "vm_product.h"
#include "vm_atomic.h"
#if !defined(NO_ICU)
#  include "unicode/ucnv.h"
#  include "unicode/udata.h"
#  include "unicode/putil.h"
#endif
#include "file.h"
#include "util.h"
#include "codeset.h"
#include "codesetOld.h"
#include "str.h"
#if defined __APPLE__
#   define LOCATION_WEAK
#   include "location.h"
#endif

/*
 * Macros
 */

#define CODESET_CAN_FALLBACK_ON_NON_ICU TRUE

#if defined __APPLE__
#   define POSIX_ICU_DIR DEFAULT_LIBDIRECTORY
#elif !defined _WIN32
#   if defined(VMX86_TOOLS)
#      define POSIX_ICU_DIR "/etc/vmware-tools"
#   else
#      define POSIX_ICU_DIR "/etc/vmware"
#   endif
#endif

/*
 * XXX These should be passed in from the build system,
 * but I don't have time to deal with bora-vmsoft.  -- edward
 */

#define ICU_DATA_FILE "icudt44l.dat"
#ifdef _WIN32
#define ICU_DATA_ITEM "icudt44l"
#define ICU_DATA_FILE_W XCONC(L, ICU_DATA_FILE)
#endif

#ifdef VMX86_DEVEL
#ifdef _WIN32
#define ICU_DATA_FILE_DIR "%TCROOT%/noarch/icu-data-4.4-1"
#define ICU_DATA_FILE_DIR_W XCONC(L, ICU_DATA_FILE_DIR)
#define ICU_DATA_FILE_PATH ICU_DATA_FILE_DIR_W DIRSEPS_W ICU_DATA_FILE_W
#else
#define ICU_DATA_FILE_DIR "/build/toolchain/noarch/icu-data-4.4-1"
#define ICU_DATA_FILE_PATH ICU_DATA_FILE_DIR DIRSEPS ICU_DATA_FILE
#endif
#endif

/*
 * Variables
 */

static Bool dontUseIcu = TRUE;


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GetCurrentCodeSet --
 *
 *    Return native code set name. Always calls down to
 *    CodeSetOld_GetCurrentCodeSet. See there for more details.
 *
 * Results:
 *    See CodeSetOld_GetCurrentCodeSet.
 *
 * Side effects:
 *    See CodeSetOld_GetCurrentCodeSet.
 *
 *-----------------------------------------------------------------------------
 */

const char *
CodeSet_GetCurrentCodeSet(void)
{
   return CodeSetOld_GetCurrentCodeSet();
}

#if !defined NO_ICU

#ifdef _WIN32

/*
 *----------------------------------------------------------------------------
 *
 * CodeSetGetModulePath --
 *
 *      Returns the wide-character current module path. We can't use
 *      Win32U_GetModuleFileName because it invokes codeset.c conversion
 *      routines.
 *
 * Returns:
 *      NULL, or a utf16 string (free with free).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static utf16_t *
CodeSetGetModulePath(HANDLE hModule) // IN
{
   utf16_t *pathW = NULL;
   DWORD size = MAX_PATH;

   while (TRUE) {
      DWORD res;

      pathW = realloc(pathW, size * (sizeof *pathW));
      if (!pathW) {
         return NULL;
      }

      res = GetModuleFileNameW(hModule, pathW, size);

      if (res == 0) {
         /* fatal error */
         goto exit;
      } else if (res == size) {
         /*
          * The buffer is too small; double its size. The maximum path length
          * on Windows is ~64KB so doubling will not push a DWORD into
          * integer overflow before success or error due to another reason
          * will occur.
          */

         size *= 2;
      } else {
         /* success */
         break;
      }
   }

  exit:
   return pathW;
}


#elif vmx86_devel && !vmx86_server && !defined(TEST_CUSTOM_ICU_DATA_FILE) // _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetGetModulePath --
 *
 *	Retrieve the full path to the executable. Not supported under
 *	VMvisor. Based on HostInfo_GetModulePath, simplified for safe
 *	use before ICU is initialized.
 *
 * Results:
 *      On success: The allocated, NUL-terminated file path.
 *         Note: This path can be a symbolic or hard link; it's just one
 *         possible path to access the executable.
 *
 *      On failure: NULL.
 *
 * Side effects:
 *	None
 *
 *-----------------------------------------------------------------------------
 */

static char *
CodeSetGetModulePath(uint32 priv)
{
   char path[PATH_MAX];
   Bool ret = FALSE;
#if defined(__APPLE__)
   uint32_t size;
#else
   ssize_t size;
   uid_t uid = -1;
#endif

   if ((priv != HGMP_PRIVILEGE) && (priv != HGMP_NO_PRIVILEGE)) {
      return NULL;
   }

#if defined(__APPLE__)
   size = sizeof path;
   if (_NSGetExecutablePath(path, &size)) {
      goto exit;
   }

#else
#if defined(VMX86_SERVER)
   if (HostType_OSIsVMK()) {
      goto exit;
   }
#endif

   if (priv == HGMP_PRIVILEGE) {
      uid = Id_BeginSuperUser();
   }

   size = readlink("/proc/self/exe", path, sizeof path - 1);
   if (size == -1) {
      if (priv == HGMP_PRIVILEGE) {
         Id_EndSuperUser(uid);
      }
      goto exit;
   }

   path[size] = '\0';

   if (priv == HGMP_PRIVILEGE) {
      Id_EndSuperUser(uid);
   }
#endif

   ret = TRUE;

  exit:
   if (ret) {
      return strdup(path);
   } else {
      return NULL;
   }
}

#endif // _WIN32

#endif // !NO_ICU


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GetAltPathName --
 *
 *	The Unicode path is probably not compatible in the current encoding.
 *	Try to convert the path into a short name so it may work with
 *      local encoding.
 *
 *      XXX -- this function is a temporary fix. It should be removed once
 *             we fix the 3rd party library pathname issue.
 *
 * Results:
 *      NULL, or a char string (free with free).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
CodeSet_GetAltPathName(const utf16_t *pathW) // IN
{
   char *path = NULL;
#if defined(_WIN32) && !defined(VM_WIN_UWP)
   DWORD res;
   utf16_t shortPathW[_MAX_PATH];

   ASSERT(pathW);

   res = GetShortPathNameW(pathW, shortPathW, ARRAYSIZE(shortPathW));

   if ((res == 0) || (res >= ARRAYSIZE(shortPathW))) {
      goto exit;
   }

   if (!CodeSetOld_Utf16leToCurrent((const char *)shortPathW,
                                    wcslen(shortPathW) * sizeof *shortPathW,
                                    &path, NULL)) {
      goto exit;
   }

  exit:
#endif // _WIN32

   return path;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_DontUseIcu --
 *
 *    Tell codeset not to load or use ICU (or stop using it if it's
 *    already loaded). Codeset will fall back on codesetOld.c, which
 *    relies on system internationalization APIs (and may have more
 *    limited functionality). Not all APIs have a fallback, however
 *    (namely GenericToGeneric).
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    See above
 *
 *-----------------------------------------------------------------------------
 */

void
CodeSet_DontUseIcu(void)
{
   dontUseIcu = TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Init --
 *
 *    Looks for ICU's data file in some platform-specific
 *    directory. If present, inits ICU by feeding it the path to that
 *    directory. If already inited, returns the current state (init
 *    failed/succeeded).
 *
 *    For Windows, CodeSet_Init is thread-safe (with critical section).
 *    For Linux/Apple, call while single-threaded.
 *
 *    *********** WARNING ***********
 *    Do not call CodeSet_Init directly, it is called already by
 *    Unicode_Init. Lots of code depends on codeset. Please call
 *    Unicode_Init as early as possible.
 *    *******************************
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    See above
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Init(const char *icuDataDir) // IN: ICU data file location in Current code page.
                                     //     Default is used if NULL.
{
#ifdef NO_ICU
   /* Nothing required if not using ICU. */
   return TRUE;
#else // NO_ICU
   DynBuf dbpath;
#ifdef _WIN32
   DWORD attribs;
   utf16_t *modPath = NULL;
   utf16_t *lastSlash;
   utf16_t *wpath;
   HANDLE hFile = INVALID_HANDLE_VALUE;
   HANDLE hMapping = NULL;
   void *memMappedData = NULL;
#else
   struct stat finfo;
#endif
   char *path = NULL;
   Bool ret = FALSE;

   DynBuf_Init(&dbpath);

#ifdef USE_ICU
   /*
    * We're using system ICU, which finds its own data. So nothing to
    * do here.
    */
   dontUseIcu = FALSE;
   ret = TRUE;
   goto exit;
#endif

  /*
   * ********************* WARNING
   * Must avoid recursive calls into the codeset library here, hence
   * the idiotic hoop-jumping. DO NOT change any of these calls to
   * wrapper equivalents or call any other functions that may perform
   * string conversion.
   * ********************* WARNING
   */

#ifdef _WIN32 // {

#if vmx86_devel && !defined(TEST_CUSTOM_ICU_DATA_FILE)
   /*
    * Devel builds use toolchain directory first.
    */
   {
      WCHAR icuFilePath[MAX_PATH] = { 0 };
      DWORD n = ExpandEnvironmentStringsW(ICU_DATA_FILE_PATH,
                                          icuFilePath, ARRAYSIZE(icuFilePath));
      if (n > 0 && n < ARRAYSIZE(icuFilePath)) {
         attribs = GetFileAttributesW(icuFilePath);
         if ((INVALID_FILE_ATTRIBUTES != attribs) ||
             (attribs & FILE_ATTRIBUTE_DIRECTORY) == 0) {
            if (!CodeSetOld_Utf16leToCurrent((const char *) icuFilePath,
                                             n * sizeof *icuFilePath,
                                             &path, NULL)) {
               goto exit;
            }
            goto found;
         }
      }
   }
#endif

   if (icuDataDir) {
      /*
       * Data file must be in the specified directory.
       */
      size_t length = strlen(icuDataDir);

      if (!DynBuf_Append(&dbpath, icuDataDir, length)) {
         goto exit;
      }
      if (length && icuDataDir[length - 1] != DIRSEPC) {
         if (!DynBuf_Append(&dbpath, DIRSEPS, strlen(DIRSEPS))) {
            goto exit;
         }
      }
      if (!DynBuf_Append(&dbpath, ICU_DATA_FILE, strlen(ICU_DATA_FILE)) ||
          !DynBuf_Append(&dbpath, "\0", 1)) {
         goto exit;
      }

      /*
       * Check for file existence.
       */
      attribs = GetFileAttributesA(DynBuf_Get(&dbpath));

      if ((INVALID_FILE_ATTRIBUTES == attribs) ||
          (attribs & FILE_ATTRIBUTE_DIRECTORY)) {
         goto exit;
      }

      path = (char *) DynBuf_Detach(&dbpath);
   } else {
      /*
       * Data file must be in the directory of the current module
       * (i.e. the module that contains CodeSet_Init()).
       */
      HMODULE hModule = W32Util_GetModuleByAddress((void *) CodeSet_Init);
      if (!hModule) {
         goto exit;
      }

      modPath = CodeSetGetModulePath(hModule);
      if (!modPath) {
         goto exit;
      }

      lastSlash = wcsrchr(modPath, DIRSEPC_W);
      if (!lastSlash) {
         goto exit;
      }

      *lastSlash = L'\0';

      if (!DynBuf_Append(&dbpath, modPath,
                         wcslen(modPath) * sizeof(utf16_t)) ||
          !DynBuf_Append(&dbpath, DIRSEPS_W,
                         wcslen(DIRSEPS_W) * sizeof(utf16_t)) ||
          !DynBuf_Append(&dbpath, ICU_DATA_FILE_W,
                         wcslen(ICU_DATA_FILE_W) * sizeof(utf16_t)) ||
          !DynBuf_Append(&dbpath, L"\0", 2)) {
         goto exit;
      }

      /*
       * Since u_setDataDirectory can't handle UTF-16, we would have to
       * now convert this path to local encoding. But that fails when
       * the module is in a path containing characters not in the
       * local encoding (see 282524). So we'll memory-map the file
       * instead and call udata_setCommonData() below.
       */
      wpath = (utf16_t *) DynBuf_Get(&dbpath);
      hFile = CreateFileW(wpath, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0,
                          NULL);
      if (INVALID_HANDLE_VALUE == hFile) {
         goto exit;
      }
      hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
      if (hMapping == NULL) {
         goto exit;
      }
      memMappedData = MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
      if (memMappedData == NULL) {
         goto exit;
      }
   }

#else // } _WIN32 {

#if vmx86_devel && !vmx86_server && !defined(TEST_CUSTOM_ICU_DATA_FILE)
   {
      char *modPath;
      char *lastSlash;

      /*
       * Devel builds use toolchain directory first.
       */
      if (stat(ICU_DATA_FILE_PATH, &finfo) >= 0 && !S_ISDIR(finfo.st_mode)) {
         if ((path = strdup(ICU_DATA_FILE_PATH)) == NULL) {
            goto exit;
         }
         goto found;
      }

      /*
       * Then we try module directory, if we can get it.
       */
      modPath = CodeSetGetModulePath(HGMP_PRIVILEGE);
      if (modPath) {
         lastSlash = strrchr(modPath, DIRSEPC);
         if (lastSlash) {
            *lastSlash = '\0';

            if (DynBuf_Append(&dbpath, modPath, strlen(modPath)) &&
                DynBuf_Append(&dbpath, DIRSEPS, strlen(DIRSEPS)) &&
                DynBuf_Append(&dbpath, ICU_DATA_FILE,
                              strlen(ICU_DATA_FILE)) &&
                DynBuf_Append(&dbpath, "\0", 1)) {

               if ((stat((const char *) DynBuf_Get(&dbpath), &finfo) >= 0) &&
                   !S_ISDIR(finfo.st_mode)) {
                  free(modPath);
                  path = DynBuf_Detach(&dbpath);
                  goto found;
               } else {
                  DynBuf_SetSize(&dbpath, 0);
               }
            }
         }

         free(modPath);
      }
   }
#endif // vmx86_devel

   if (icuDataDir) {
      /* Use the caller-specified ICU data dir. */
      if (!DynBuf_Append(&dbpath, icuDataDir, strlen(icuDataDir))) {
         goto exit;
      }
   } else {
      /* Use a default ICU data dir. */
#   if defined __APPLE__
      Location_Get_Type *Location_Get = Location_Get_Addr();

      if (Location_Get) {
         char *icuDir = Location_Get("icuDir");
         Bool success =    icuDir
                        && DynBuf_Append(&dbpath, icuDir, strlen(icuDir));

         free(icuDir);
         if (!success) {
            goto exit;
         }
      } else
#   endif

      {
         if (!DynBuf_Append(&dbpath, POSIX_ICU_DIR, strlen(POSIX_ICU_DIR)) ||
             !DynBuf_Append(&dbpath, "/icu", strlen("/icu"))) {
            goto exit;
         }
      }
   }
   if (!DynBuf_Append(&dbpath, DIRSEPS, strlen(DIRSEPS)) ||
       !DynBuf_Append(&dbpath, ICU_DATA_FILE, strlen(ICU_DATA_FILE)) ||
       !DynBuf_Append(&dbpath, "\0", 1)) {
      goto exit;
   }

   /*
    * Check for file existence. (DO NOT CHANGE TO 'stat' WRAPPER).
    */
   path = (char *) DynBuf_Detach(&dbpath);
   if (stat(path, &finfo) < 0 || S_ISDIR(finfo.st_mode)) {
      goto exit;
   }

#endif // } _WIN32

#if vmx86_devel && !vmx86_server && !defined(TEST_CUSTOM_ICU_DATA_FILE)
found:
#endif

#ifdef _WIN32
   if (memMappedData) {
      /*
       * Tell ICU to use this mapped data.
       */
      UErrorCode uerr = U_ZERO_ERROR;
      ASSERT(memMappedData);

      udata_setCommonData(memMappedData, &uerr);
      if (U_FAILURE(uerr)) {
         UnmapViewOfFile(memMappedData);
         goto exit;
      }
      udata_setAppData(ICU_DATA_ITEM, memMappedData, &uerr);
      if (U_FAILURE(uerr)) {
         UnmapViewOfFile(memMappedData);
         goto exit;
      }
   } else {
#endif
      /*
       * Tell ICU to use this directory.
       */
      u_setDataDirectory(path);
#ifdef _WIN32
   }
#endif

   dontUseIcu = FALSE;
   ret = TRUE;

  exit:
   if (!ret) {
      /*
       * There was an error initing ICU, but if we can fall back on
       * non-ICU (old CodeSet) then things are OK.
       */
      if (CODESET_CAN_FALLBACK_ON_NON_ICU) {
         ret = TRUE;
         dontUseIcu = TRUE;

#ifdef _WIN32
         OutputDebugStringW(L"CodeSet_Init: no ICU\n");
#endif
      }
   }

#ifdef _WIN32
   free(modPath);
   if (hMapping) {
      CloseHandle(hMapping);
   }
   if (hFile != INVALID_HANDLE_VALUE) {
      CloseHandle(hFile);
   }
#endif
   free(path);
   DynBuf_Destroy(&dbpath);

   return ret;
#endif
}


#if defined(CURRENT_IS_UTF8)

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetDuplicateUtf8Str --
 *
 *    Duplicate UTF-8 string, appending zero terminator to its end.
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
CodeSetDuplicateUtf8Str(const char *bufIn,  // IN: Input string
                        size_t sizeIn,      // IN: Input string length
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

#endif // defined(CURRENT_IS_UTF8)


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSetDynBufFinalize --
 *
 *    Append NUL terminator to the buffer, and return pointer to
 *    buffer and its data size (before appending terminator). Destroys
 *    buffer on failure.
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
CodeSetDynBufFinalize(Bool ok,          // IN: Earlier steps succeeded
                      DynBuf *db,       // IN: Buffer with converted string
                      char **bufOut,    // OUT: Converted string
                      size_t *sizeOut)  // OUT/OPT: Length of string in bytes
{
   /*
    * NUL can be as long as 4 bytes if UTF-32, make no assumptions.
    */
   if (!ok || !DynBuf_Append(db, "\0\0\0\0", 4) || !DynBuf_Trim(db)) {
      DynBuf_Destroy(db);
      return FALSE;
   }

   *bufOut = DynBuf_Get(db);
   if (sizeOut) {
      *sizeOut = DynBuf_GetSize(db) - 4;
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
                     size_t sizeIn,      // IN
                     DynBuf *db)         // IN
{
   return CodeSet_GenericToGenericDb("UTF-8", bufIn, sizeIn, "UTF-16LE", 0,
                                     db);
}


#if defined(__APPLE__)

/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8Normalize --
 *
 *    Calls down to CodeSetOld_Utf8Normalize.
 *
 * Results:
 *    See CodeSetOld_Utf8Normalize.
 *
 * Side effects:
 *    See CodeSetOld_Utf8Normalize.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_Utf8Normalize(const char *bufIn,     // IN
                      size_t sizeIn,         // IN
                      Bool precomposed,      // IN
                      DynBuf *db)            // OUT
{
   return CodeSetOld_Utf8Normalize(bufIn, sizeIn, precomposed, db);
}

#endif /* defined(__APPLE__) */


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GenericToGenericDb --
 *
 *    Append the content of a buffer (that uses the specified encoding) to a
 *    DynBuf (that uses the specified encoding).
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    String (sans NUL-termination) is appended to db.
 *
 *-----------------------------------------------------------------------------
 */

Bool
CodeSet_GenericToGenericDb(const char *codeIn,  // IN
                           const char *bufIn,   // IN
                           size_t sizeIn,       // IN
                           const char *codeOut, // IN
                           unsigned int flags,  // IN
                           DynBuf *db)          // IN/OUT
{
#if defined(NO_ICU)
   return CodeSetOld_GenericToGenericDb(codeIn, bufIn, sizeIn, codeOut,
                                        flags, db);
#else
   Bool result = FALSE;
   UErrorCode uerr;
   const char *bufInCur;
   const char *bufInEnd;
   UChar bufPiv[1024];
   UChar *bufPivSource;
   UChar *bufPivTarget;
   UChar *bufPivEnd;
   char *bufOut;
   char *bufOutCur;
   char *bufOutEnd;
   size_t newSize;
   size_t bufOutSize;
   size_t bufOutOffset;
   UConverter *cvin = NULL;
   UConverter *cvout = NULL;
   UConverterToUCallback toUCb;
   UConverterFromUCallback fromUCb;

   ASSERT(codeIn);
   ASSERT(sizeIn == 0 || bufIn);
   ASSERT(codeOut);
   ASSERT(db);
   ASSERT((CSGTG_NORMAL == flags) || (CSGTG_TRANSLIT == flags) ||
          (CSGTG_IGNORE == flags));

   if (dontUseIcu) {
      // fall back
      return CodeSetOld_GenericToGenericDb(codeIn, bufIn, sizeIn, codeOut,
                                           flags, db);
   }

   /*
    * Trivial case.
    */

   if ((sizeIn == 0) || (bufIn == NULL)) {
      result = TRUE;
      goto exit;
   }

   /*
    * Open converters.
    */

   uerr = U_ZERO_ERROR;
   cvin = ucnv_open(codeIn, &uerr);
   if (!cvin) {
      goto exit;
   }

   uerr = U_ZERO_ERROR;
   cvout = ucnv_open(codeOut, &uerr);
   if (!cvout) {
      goto exit;
   }

   /*
    * Set callbacks according to flags.
    */

   switch (flags) {
   case CSGTG_NORMAL:
      toUCb = UCNV_TO_U_CALLBACK_STOP;
      fromUCb = UCNV_FROM_U_CALLBACK_STOP;
      break;

   case CSGTG_TRANSLIT:
      toUCb = UCNV_TO_U_CALLBACK_SUBSTITUTE;
      fromUCb = UCNV_FROM_U_CALLBACK_SUBSTITUTE;
      break;

   case CSGTG_IGNORE:
      toUCb = UCNV_TO_U_CALLBACK_SKIP;
      fromUCb = UCNV_FROM_U_CALLBACK_SKIP;
      break;

   default:
      NOT_IMPLEMENTED();
      break;
   }

   uerr = U_ZERO_ERROR;
   ucnv_setToUCallBack(cvin, toUCb, NULL, NULL, NULL, &uerr);
   if (U_FAILURE(uerr)) {
      goto exit;
   }

   uerr = U_ZERO_ERROR;
   ucnv_setFromUCallBack(cvout, fromUCb, NULL, NULL, NULL, &uerr);
   if (U_FAILURE(uerr)) {
      goto exit;
   }

   /*
    * Convert using ucnv_convertEx().
    * As a starting guess, make the output buffer the same size as
    * the input string (with a fudge constant added in to avoid degen
    * cases).
    */

   bufInCur = bufIn;
   bufInEnd = bufIn + sizeIn;
   newSize = sizeIn + 4;
   if (newSize < sizeIn) {  // Prevent integer overflow
      goto exit;
   }
   bufOutSize = newSize;
   bufOutOffset = 0;
   bufPivSource = bufPiv;
   bufPivTarget = bufPiv;
   bufPivEnd = bufPiv + ARRAYSIZE(bufPiv);

   for (;;) {
      if (!DynBuf_Enlarge(db, bufOutSize)) {
         goto exit;
      }
      bufOut = DynBuf_Get(db);
      bufOutCur = bufOut + bufOutOffset;
      bufOutSize = DynBuf_GetAllocatedSize(db);
      bufOutEnd = bufOut + bufOutSize;

      uerr = U_ZERO_ERROR;
      ucnv_convertEx(cvout, cvin, &bufOutCur, bufOutEnd,
		     &bufInCur, bufInEnd,
		     bufPiv, &bufPivSource, &bufPivTarget, bufPivEnd,
		     FALSE, TRUE, &uerr);

      if (U_SUCCESS(uerr)) {
         /*
          * "This was a triumph. I'm making a note here: HUGE SUCCESS. It's
          * hard to overstate my satisfaction."
          */

         break;
      }

      if (U_BUFFER_OVERFLOW_ERROR != uerr) {
	 // failure
         goto exit;
      }

      /*
       * Our guess at 'bufOutSize' was obviously wrong, just double it.
       * We'll be reallocating bufOut, so will need to recompute bufOutCur
       * based on bufOutOffset.
       */

      newSize = 2 * bufOutSize;

      /*
       * Prevent integer overflow. We can use this form of checking
       * specifically because a multiple by 2 is used. This type of checking
       * does not work in the general case.
       */

      if (newSize < bufOutSize) {
         goto exit;
      }

      bufOutSize = newSize;
      bufOutOffset = bufOutCur - bufOut;
   }

   /*
    * Set final size and return.
    */

   DynBuf_SetSize(db, bufOutCur - bufOut);

   result = TRUE;

  exit:
   if (cvin) {
      ucnv_close(cvin);
   }

   if (cvout) {
      ucnv_close(cvout);
   }

   return result;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_GenericToGeneric --
 *
 *    Non-db version of CodeSet_GenericToGenericDb.
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
CodeSet_GenericToGeneric(const char *codeIn,  // IN
                         const char *bufIn,   // IN
                         size_t sizeIn,       // IN
                         const char *codeOut, // IN
                         unsigned int flags,  // IN
                         char **bufOut,       // OUT
                         size_t *sizeOut)     // OUT/OPT
{
   DynBuf db;
   Bool ok;

   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb(codeIn, bufIn, sizeIn, codeOut, flags, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
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
 * CodeSet_Utf8ToCurrent --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the current encoding).
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
CodeSet_Utf8ToCurrent(const char *bufIn,  // IN
                      size_t sizeIn,      // IN
                      char **bufOut,      // OUT
                      size_t *sizeOut)    // OUT/OPT
{
#if !defined(CURRENT_IS_UTF8)
   DynBuf db;
   Bool ok;
#endif

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf8ToCurrent(bufIn, sizeIn, bufOut, sizeOut);
   }

#if defined(CURRENT_IS_UTF8)
   return CodeSetDuplicateUtf8Str(bufIn, sizeIn, bufOut, sizeOut);
#else
   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb("UTF-8", bufIn, sizeIn,
                                   CodeSet_GetCurrentCodeSet(), 0, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_CurrentToUtf8 --
 *
 *    Convert the content of a buffer (that uses the current encoding) into
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
CodeSet_CurrentToUtf8(const char *bufIn,  // IN
                      size_t sizeIn,      // IN
                      char **bufOut,      // OUT
                      size_t *sizeOut)    // OUT/OPT
{
#if !defined(CURRENT_IS_UTF8)
   DynBuf db;
   Bool ok;
#endif

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_CurrentToUtf8(bufIn, sizeIn, bufOut, sizeOut);
   }

#if defined(CURRENT_IS_UTF8)
   return CodeSetDuplicateUtf8Str(bufIn, sizeIn, bufOut, sizeOut);
#else
   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb(CodeSet_GetCurrentCodeSet(), bufIn, sizeIn,
                                   "UTF-8", 0, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16leToUtf8Db --
 *
 *    Append the content of a buffer (that uses the UTF-16LE encoding) to a
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
CodeSet_Utf16leToUtf8Db(const char *bufIn, // IN
                        size_t sizeIn,     // IN
                        DynBuf *db)        // IN
{
   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf16leToUtf8Db(bufIn, sizeIn, db);
   }

   return CodeSet_GenericToGenericDb("UTF-16LE", bufIn, sizeIn, "UTF-8", 0,
                                     db);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf16leToUtf8 --
 *
 *    Convert the content of a buffer (that uses the UTF-16LE encoding) into
 *    another buffer (that uses the UTF-8 encoding).
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
CodeSet_Utf16leToUtf8(const char *bufIn,  // IN
                      size_t sizeIn,      // IN
                      char **bufOut,      // OUT
                      size_t *sizeOut)    // OUT/OPT
{
   DynBuf db;
   Bool ok;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf16leToUtf8(bufIn, sizeIn, bufOut, sizeOut);
   }

   DynBuf_Init(&db);
   ok = CodeSet_Utf16leToUtf8Db(bufIn, sizeIn, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Utf8ToUtf16le --
 *
 *    Convert the content of a buffer (that uses the UTF-8 encoding) into
 *    another buffer (that uses the UTF-16LE encoding).
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
CodeSet_Utf8ToUtf16le(const char *bufIn,  // IN
                      size_t sizeIn,      // IN
                      char **bufOut,      // OUT
                      size_t *sizeOut)    // OUT/OPT
{
   DynBuf db;
   Bool ok;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf8ToUtf16le(bufIn, sizeIn, bufOut, sizeOut);
   }

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
CodeSet_Utf8FormDToUtf8FormC(const char *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut)       // OUT/OPT
{
   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf8FormDToUtf8FormC(bufIn, sizeIn, bufOut, sizeOut);
   }

#if defined(__APPLE__)
   {
      DynBuf db;
      Bool ok;
      DynBuf_Init(&db);
      ok = CodeSet_Utf8Normalize(bufIn, sizeIn, TRUE, &db);
      return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
   }
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
CodeSet_Utf8FormCToUtf8FormD(const char *bufIn,     // IN
                             size_t sizeIn,         // IN
                             char **bufOut,         // OUT
                             size_t *sizeOut)       // OUT/OPT
{
   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf8FormCToUtf8FormD(bufIn, sizeIn, bufOut, sizeOut);
   }

#if defined(__APPLE__)
   {
      DynBuf db;
      Bool ok;
      DynBuf_Init(&db);
      ok = CodeSet_Utf8Normalize(bufIn, sizeIn, FALSE, &db);
      return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
   }
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
CodeSet_CurrentToUtf16le(const char *bufIn, // IN
                         size_t sizeIn,     // IN
                         char **bufOut,     // OUT
                         size_t *sizeOut)   // OUT/OPT
{
   DynBuf db;
   Bool ok;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_CurrentToUtf16le(bufIn, sizeIn, bufOut, sizeOut);
   }

   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb(CodeSet_GetCurrentCodeSet(), bufIn, sizeIn,
                                   "UTF-16LE", 0, &db);
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
CodeSet_Utf16leToCurrent(const char *bufIn,  // IN
                         size_t sizeIn,      // IN
                         char **bufOut,      // OUT
                         size_t *sizeOut)    // OUT/OPT
{
   DynBuf db;
   Bool ok;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf16leToCurrent(bufIn, sizeIn, bufOut, sizeOut);
   }

   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb("UTF-16LE", bufIn, sizeIn,
                                   CodeSet_GetCurrentCodeSet(), 0, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
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
CodeSet_Utf16beToCurrent(const char *bufIn,  // IN
                         size_t sizeIn,      // IN
                         char **bufOut,      // OUT
                         size_t *sizeOut)    // OUT/OPT
{
   DynBuf db;
   Bool ok;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_Utf16beToCurrent(bufIn, sizeIn, bufOut, sizeOut);
   }

   DynBuf_Init(&db);
   ok = CodeSet_GenericToGenericDb("UTF-16BE", bufIn, sizeIn,
                                   CodeSet_GetCurrentCodeSet(), 0, &db);
   return CodeSetDynBufFinalize(ok, &db, bufOut, sizeOut);
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_IsEncodingSupported --
 *
 *    Ask ICU if it supports the specific encoding.
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
CodeSet_IsEncodingSupported(const char *name) // IN
{
#if defined(NO_ICU)
   return CodeSetOld_IsEncodingSupported(name);
#else
   UConverter *cv;
   UErrorCode uerr;

   /*
    * Fallback if necessary.
    */
   if (dontUseIcu) {
      return CodeSetOld_IsEncodingSupported(name);
   }

   /*
    * Try to open the encoding.
    */
   uerr = U_ZERO_ERROR;
   cv = ucnv_open(name, &uerr);
   if (cv) {
      ucnv_close(cv);

      return TRUE;
   }

   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * CodeSet_Validate --
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
CodeSet_Validate(const char *buf,   // IN: the string
                 size_t size,       // IN: length of string
                 const char *code)  // IN: encoding
{
#if defined(NO_ICU)
   return CodeSetOld_Validate(buf, size, code);
#else
   UConverter *cv;
   UErrorCode uerr;

   // ucnv_toUChars takes 32-bit int size
   VERIFY(size <= (size_t) MAX_INT32);

   if (size == 0) {
      return TRUE;
   }

   /*
    * Fallback if necessary.
    */

   if (dontUseIcu) {
      return CodeSetOld_Validate(buf, size, code);
   }

   /*
    * Calling ucnv_toUChars() this way is the idiom to precompute
    * the length of the output.  (See preflighting in the ICU User Guide.)
    * So if the error is not U_BUFFER_OVERFLOW_ERROR, then the input
    * is bad.
    */

   uerr = U_ZERO_ERROR;
   cv = ucnv_open(code, &uerr);
   VERIFY(U_SUCCESS(uerr));
   ucnv_setToUCallBack(cv, UCNV_TO_U_CALLBACK_STOP, NULL, NULL, NULL, &uerr);
   VERIFY(U_SUCCESS(uerr));
   ucnv_toUChars(cv, NULL, 0, buf, size, &uerr);
   ucnv_close(cv);

   return uerr == U_BUFFER_OVERFLOW_ERROR;
#endif
}

