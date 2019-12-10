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

#if defined(_WIN32)
#include <winsock2.h> // also includes windows.h
#include <io.h>
#include <process.h>
#define getpid() _getpid()
#endif

#include "vm_ctype.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <stddef.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>

#if !defined(_WIN32)
#  include <unistd.h>
#  include <pwd.h>
#  include <sys/socket.h>    // for AF_INET[6]
#  include <arpa/inet.h>     // for inet_pton
#  include <netinet/in.h>    // for INET6_ADDRSTRLEN
#endif

#if defined(__APPLE__)
#include <sys/stat.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#include "vmware.h"
#include "msg.h"
#include "util.h"
#include "str.h"
#include "su.h"
#include "escape.h"
#include "posix.h"
#include "unicode.h"

#if defined(_WIN32)
#include "windowsu.h"
#include "windowsUtil.h"
#endif
#if defined(__APPLE__)
#include "utilMacos.h"
#endif

/*
 * ESX with userworld VMX
 */
#if defined(VMX86_SERVER)
#include "hostType.h"
#include "user_layout.h"
#endif


char *gHomeDirOverride = NULL;


/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetCanonicalPath --
 *
 *      Canonicalizes a path name.
 *
 * Results:
 *      A freshly allocated canonicalized path name.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_GetCanonicalPath(const char *path)  // IN:
{
   char *canonicalPath = NULL;

#if defined(__linux__) || defined(__APPLE__)
   canonicalPath = Posix_RealPath(path);
#elif defined(_WIN32)
   char driveSpec[4];
   Bool remoteDrive = FALSE;

   if (!path || !*path) {
      return NULL;
   }

   memcpy(driveSpec, path, 3);
   driveSpec[3] = '\0';

   if (strchr(VALID_DIRSEPS, driveSpec[0]) &&
       strchr(VALID_DIRSEPS, driveSpec[1])) {
      remoteDrive = TRUE;
   } else {
#if defined( VM_WIN_UWP)
      /* Don't need remote path for UWP until now*/
      remoteDrive = FALSE;
#else
      remoteDrive = (GetDriveTypeA(driveSpec) == DRIVE_REMOTE);
#endif // !VM_WIN_UWP
   }

   /*
    * If the path is *potentially* a path to remote share, we do not
    * call GetLongPathName, because if the remote server is unreachable,
    * that function could hang. We sacrifice two things by doing so:
    * 1. The UNC path could refer to the local host and we incorrectly
    *    assume remote.
    * 2. We do not resolve 8.3 names for remote paths.
    */

   if (remoteDrive) {
      canonicalPath = Util_SafeStrdup(path);
   } else {
      canonicalPath = W32Util_RobustGetLongPath(path);
   }

#else
   NOT_IMPLEMENTED();
#endif
   return canonicalPath;
}


#if defined(_WIN32)
/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetCanonicalPathForHash --
 *
 *      Utility function to both get the canonical version of the input path
 *      and produce a unique case-insensitive version of the path suitable
 *      for use as a seed to hash functions.
 *
 * Results:
 *       Canonicalized UTF-8 pathname suitable for use in hashing.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_GetCanonicalPathForHash(const char *path)  // IN: UTF-8
{
   char *ret = NULL;
   char *cpath = Util_GetCanonicalPath(path);

   if (cpath != NULL) {
      ret = Unicode_FoldCase(cpath);
      free(cpath);
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilGetLegacyEncodedString --
 *
 *      Takes a UTF-8 string, and allocates a new string in legacy encoding.
 *      This is necessary to maintain compatibility with older versions of
 *      the product, which may have stored strings (paths) in legacy
 *      encoding.  Hence, the use of WideCharToMultiByte().
 *
 * Results:
 *      An allocated string in legacy encoding (MBCS when applicable).
 *      NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
UtilGetLegacyEncodedString(const char *path)  // IN: UTF-8
{
   char *ret = NULL;
   char *cpath = Util_GetCanonicalPath(path);

   if (cpath != NULL) {
      char *apath = NULL;
      int retlen;
      WCHAR *wcpath = Unicode_GetAllocUTF16(cpath);

      /* First get the length of multibyte string */
      int alen = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wcpath, -1,
                                     NULL, 0, NULL, NULL);
      if (alen > 0) {
         /* Now get the converted string */
         ret = Util_SafeMalloc(alen);
         retlen = WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, wcpath, -1,
                                      ret, alen, NULL, NULL);
         if (retlen != alen) {
            free(ret);
            ret = NULL;
         }
      }
      free(cpath);
      free(wcpath);
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CompatGetCanonicalPath --
 *
 *      Canonicalizes a path name (compatibility version).
 *
 * Results:
 *      A freshly allocated canonicalized path name in legacy encoding
 *      (MBCS when applicable).
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_CompatGetCanonicalPath(const char *path)  // IN: UTF-8
{
   char *cpath = Util_GetCanonicalPath(path);
   char *ret = NULL;

   if (cpath != NULL) {
      ret = UtilGetLegacyEncodedString(cpath);
      free(cpath);
   }

   return ret;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CanonicalPathsIdentical --
 *
 *      Utility function to compare two paths that have already been made
 *      canonical. This function exists to mask platform differences in
 *      path case-sensitivity.
 *
 *      XXX: This implementation makes assumptions about the host filesystem's
 *           case sensitivity without any regard to what filesystem the
 *           provided paths actually use. There are many ways to break this
 *           assumption, on any of our supported host OSes! The return value
 *           of this function cannot be trusted.
 *
 * Results:
 *      TRUE if the paths are equivalenr, FALSE if they are not.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_CanonicalPathsIdentical(const char *path1,  // IN:
                             const char *path2)  // IN:
{
   ASSERT(path1);
   ASSERT(path2);

#if defined(__linux__)
   return (strcmp(path1, path2) == 0);
#elif defined(_WIN32)
   return (_stricmp(path1, path2) == 0);
#elif defined(__APPLE__)
   return (strcasecmp(path1, path2) == 0);
#else
   NOT_IMPLEMENTED();
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * Util_IsAbsolutePath --
 *
 *      Checks if the given path is absolute.
 *
 * Results:
 *      TRUE if the path is absolute, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
Util_IsAbsolutePath(const char *path)  // IN: path to check
{
#if defined __linux__ || defined __APPLE__ || defined __FreeBSD__ || defined sun
   // path[0] is valid even for the empty string.
   return path && path[0] == DIRSEPC;
#elif defined _WIN32
   // if the length is 2, path[2] will be valid because of the null terminator.
   if (!path || strlen(path) < 2) {
      return FALSE;
   }

   // <Drive letter>:\path
   if (CType_IsAlpha(path[0]) && path[1] == ':' && path[2] == DIRSEPC) {
      return TRUE;
   }

   // UNC paths
   if (path[0] == DIRSEPC && path[1] == DIRSEPC) {
      return TRUE;
   }

   return FALSE;
#else
   NOT_IMPLEMENTED();
#endif
   NOT_REACHED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilIsAlphaOrNum --
 *
 *      Checks if character is a numeric digit or a letter of the
 *      english alphabet.
 *
 * Results:
 *      Returns TRUE if character is a digit or a letter, FALSE otherwise.
 *
 * Side effects:
 *	     None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
UtilIsAlphaOrNum(char ch) //IN
{
   if ((ch >= '0' && ch <= '9') ||
       (ch >= 'a' && ch <= 'z') ||
       (ch >= 'A' && ch <= 'Z')) {
      return TRUE;
   } else {
      return FALSE;
   }
}


#ifndef _WIN32

/*
 *-----------------------------------------------------------------------------
 *
 * UtilGetHomeDirectory --
 *
 *      Retrieves the home directory for a user, given their passwd struct.
 *
 * Results:
 *      Returns user's home directory or NULL if it fails.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
UtilGetHomeDirectory(struct passwd *pwd) // IN/OPT: user passwd struct
{
   if (pwd && pwd->pw_dir) {
      return Util_SafeStrdup(pwd->pw_dir);
   } else {
      return NULL;
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilGetLoginName --
 *
 *      Retrieves the login name for a user, given their passwd struct.
 *
 * Results:
 *      Returns user's login name or NULL if it fails.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
UtilGetLoginName(struct passwd *pwd) // IN/OPT: user passwd struct
{
   if (pwd && pwd->pw_name) {
      return Util_SafeStrdup(pwd->pw_name);
   } else {
      return NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * UtilDoTildeSubst --
 *
 *      Given a string following a tilde, this routine returns the
 *      corresponding home directory.
 *
 * Results:
 *      A string containing the home directory in native format. The
 *      returned string is a newly allocated string which may/must be
 *      freed by the caller.
 *
 * Side effects:
 *      None.
 *
 * Credit: derived from J.K.Ousterhout's Tcl
 *----------------------------------------------------------------------
 */

static char *
UtilDoTildeSubst(const char *user)  // IN: name of user
{
   char *str = NULL;
   struct passwd *pwd = NULL;

   if (gHomeDirOverride) {
      /*
       * Allow code to override the tilde expansion for things like unit tests.
       * See: Util_OverrideHomeDir
       */
      return Util_SafeStrdup(gHomeDirOverride);
   }

   if (*user == '\0') {
#if defined(__APPLE__)
      /*
       * This check mimics the checks and order of CFCopyHomeDirectoryURL(),
       * which is unfortunately not callable directly since Apple has marked it
       * as only in iOS despite the fact that they clearly ship it on macOS.
       */
      str = issetugid() ? NULL
                        : Unicode_Duplicate(Posix_Getenv("CFFIXED_USER_HOME"));

      if (str == NULL) {
         pwd = Posix_Getpwuid(getuid());
         if (pwd == NULL) {
            Log("Could not get passwd for current user.\n");
         }
      }
#endif // defined(__APPLE__)
      if (str == NULL && pwd == NULL) {
         str = Unicode_Duplicate(Posix_Getenv("HOME"));
         if (str == NULL) {
            Log("Could not expand environment variable HOME.\n");
         }
      }
   } else {
      pwd = Posix_Getpwnam(user);
      if (pwd == NULL) {
         Log("Could not get passwd for user '%s'.\n", user);
      }
   }
   if (str == NULL && pwd != NULL) {
      str = UtilGetHomeDirectory(pwd);
      Posix_Endpwent();
      if (str == NULL) {
         Log("Could not get home directory for user.\n");
      }
   }
   return str;
}

#endif // !_WIN32


/*
 *----------------------------------------------------------------------
 *
 * Util_ExpandString --
 *
 *      converts the strings by expanding "~", "~user" and environment
 *      variables
 *
 * Results:
 *
 *	Return a newly allocated string.  The caller is responsible
 *	for deallocating it.
 *
 *      Return NULL in case of error.
 *
 * Side effects:
 *      memory allocation
 *
 * Bugs:
 *      the handling of enviroment variable references is very
 *	simplistic: there can be only one in a pathname segment
 *	and it must appear last in the string
 *
 *----------------------------------------------------------------------
 */

#define UTIL_MAX_PATH_CHUNKS 100

char *
Util_ExpandString(const char *fileName) // IN  file path to expand
{
   char *copy = NULL;
   char *result = NULL;
   int nchunk = 0;
   char *chunks[UTIL_MAX_PATH_CHUNKS];
   int chunkSize[UTIL_MAX_PATH_CHUNKS];
   Bool freeChunk[UTIL_MAX_PATH_CHUNKS];
   char *cp;
   int i;

   ASSERT(fileName);

   copy = Unicode_Duplicate(fileName);

   /*
    * quick exit
    */
   if ((*fileName != '~') && (strchr(fileName, '$') == NULL)) {
      return copy;
   }

   /*
    * XXX Because the rest part of code depends pretty heavily from
    *     character pointer operations we want to leave it as-is and
    *     don't want to re-work it with using unicode library. However
    *     it's acceptable only until our Unicode type is UTF-8 and
    *     until code below works correctly with UTF-8.
    */

   /*
    * Break string into nice chunks for separate expansion.
    *
    * The rule for terminating a ~ expansion is historical.  -- edward
    */

   nchunk = 0;
   for (cp = copy; *cp;) {
      size_t len;
      if (*cp == '$') {
	 char *p;
	 for (p = cp + 1; UtilIsAlphaOrNum(*p) || *p == '_'; p++) {
	 }
	 len = p - cp;
#if !defined(_WIN32)
      } else if (cp == copy && *cp == '~') {
	 len = strcspn(cp, DIRSEPS);
#endif
      } else {
	 len = strcspn(cp, "$");
      }
      if (nchunk >= UTIL_MAX_PATH_CHUNKS) {
         Log("%s: Filename \"%s\" has too many chunks.\n", __FUNCTION__,
             fileName);
	 goto out;
      }
      chunks[nchunk] = cp;
      chunkSize[nchunk] = len;
      freeChunk[nchunk] = FALSE;
      nchunk++;
      cp += len;
   }

   /*
    * Expand leanding ~
    */

#if !defined(_WIN32)
   if (chunks[0][0] == '~') {
      char save = (cp = chunks[0])[chunkSize[0]];
      cp[chunkSize[0]] = 0;
      ASSERT(!freeChunk[0]);
      chunks[0] = UtilDoTildeSubst(chunks[0] + 1);
      cp[chunkSize[0]] = save;
      if (chunks[0] == NULL) {
         /* It could not be expanded, therefore leave as original. */
         chunks[0] = cp;
      } else {
         /* It was expanded, so adjust the chunks. */
         chunkSize[0] = strlen(chunks[0]);
         freeChunk[0] = TRUE;
      }
   }
#endif

   /*
    * Expand $
    */

   for (i = 0; i < nchunk; i++) {
      char save;
      char *expand = NULL;
      char buf[100];
#if defined(_WIN32)
      utf16_t bufW[100];
#endif
      cp = chunks[i];

      if (*cp != '$' || chunkSize[i] == 1) {

         /*
          * Skip if the chuck has only the $ character.
          * $ will be kept as a part of the pathname.
          */

	 continue;
      }

      save = cp[chunkSize[i]];
      cp[chunkSize[i]] = 0;

      /*
       * $PID and $USER are interpreted specially.
       * Others are just getenv().
       */

      expand = Unicode_Duplicate(Posix_Getenv(cp + 1));
      if (expand != NULL) {
      } else if (strcasecmp(cp + 1, "PID") == 0) {
	 Str_Snprintf(buf, sizeof buf, "%"FMTPID, getpid());
	 expand = Util_SafeStrdup(buf);
      } else if (strcasecmp(cp + 1, "USER") == 0) {
#if !defined(_WIN32)
         struct passwd *pwd = Posix_Getpwuid(getuid());
         expand = UtilGetLoginName(pwd);
         Posix_Endpwent();
#else
	 DWORD n = ARRAYSIZE(bufW);
	 if (GetUserNameW(bufW, &n)) {
	    expand = Unicode_AllocWithUTF16(bufW);
	 }
#endif
	 if (expand == NULL) {
	    expand = Unicode_Duplicate("unknown");
	 }
      } else {
         Log("Environment variable '%s' not defined in '%s'.\n",
             cp + 1, fileName);
#if !defined(_WIN32)
         /*
          * Strip off the env variable string from the pathname.
          */

	 expand = Unicode_Duplicate("");

#else    // _WIN32

         /*
          * The problem is we have really no way to distinguish the caller's
          * intention is a dollar sign ($) is used as a part of the pathname
          * or as an environment variable.
          *
          * If the token does not expand to an environment variable,
          * then assume it is a part of the pathname. Do not strip it
          * off like it is done in linux host (see above)
          *
          * XXX   We should also consider using %variable% convention instead
          *       of $variable for Windows platform.
          */

         Str_Strcpy(buf, cp, 100);
         expand = Unicode_AllocWithUTF8(buf);
#endif
      }

      cp[chunkSize[i]] = save;

      ASSERT(expand != NULL);
      ASSERT(!freeChunk[i]);
      chunks[i] = expand;
      if (chunks[i] == NULL) {
	 Log("%s: Cannot allocate memory to expand $ in \"%s\".\n",
             __FUNCTION__, fileName);
	 goto out;
      }
      chunkSize[i] = strlen(expand);
      freeChunk[i] = TRUE;
   }

   /*
    * Put all the chunks back together.
    */

   {
      int size = 1;	// 1 for the terminating null
      for (i = 0; i < nchunk; i++) {
	 size += chunkSize[i];
      }
      result = malloc(size);
   }
   if (result == NULL) {
      Log("%s: Cannot allocate memory for the expansion of \"%s\".\n",
          __FUNCTION__, fileName);
      goto out;
   }
   cp = result;
   for (i = 0; i < nchunk; i++) {
      memcpy(cp, chunks[i], chunkSize[i]);
      cp += chunkSize[i];
   }
   *cp = 0;

out:
   /*
    * Clean up and return.
    */

   for (i = 0; i < nchunk; i++) {
      if (freeChunk[i]) {
	 free(chunks[i]);
      }
   }
   free(copy);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_OverrideHomeDir --
 *
 *      This function changes the behavior of Util_ExpandPath so that
 *      it will expand "~" to the provided path rather than the current
 *      user's home directory.
 *
 *      This function is not thread safe, so a best practice is to
 *      invoke it once at the begining of program execution, much like
 *      an *_Init() function. It should also never be called more than
 *      once.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Future calls to Util_ExpandPath will substitute "~" with the
 *      given path.
 *
 *----------------------------------------------------------------------
 */
void
Util_OverrideHomeDir(const char *path) // IN
{
   ASSERT(!gHomeDirOverride);
   gHomeDirOverride = Util_SafeStrdup(path);
}


#if defined(__APPLE__)

/*
 * XXX TODO: move these to utilMacos.c after it gets split up to avoid
 * dependencies on IOKit and DiskArbitration.
 */


/*
 *-----------------------------------------------------------------------------
 *
 * Util_CFStringToUTF8CString --
 *
 *      Convert a CFString into a UTF-8 encoded C string.
 *
 *      Amazingly, CFString does not provide this functionality, so everybody
 *      (including Apple, see smb-217.18/lib/smb/charsets.c in darwinsource)
 *      ends up re-implementing it this way...
 *
 * Results:
 *      On success: Allocated, UTF8-encoded, NUL-terminated string.
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
Util_CFStringToUTF8CString(CFStringRef s) // IN
{
   static CFStringEncoding const encoding = kCFStringEncodingUTF8;
   char const *fast;
   char *result;

   ASSERT(s);

   fast = CFStringGetCStringPtr(s, encoding);
   if (fast) {
      result = strdup(fast);
   } else {
      size_t maxSize;

      maxSize = CFStringGetMaximumSizeForEncoding(CFStringGetLength(s),
                                                  encoding) + 1;
      result = malloc(maxSize);
      if (result) {
         if (CFStringGetCString(s, result, maxSize, encoding)) {
            /*
             * It is likely that less than 'maxSize' bytes have actually been
             * written into 'result'. If that becomes a problem in the future,
             * we can always trim the buffer here.
             */
         } else {
            free(result);
            result = NULL;
         }
      }
   }

   if (!result) {
      Log("Failed to get C string from CFString.\n");
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilMacos_CreateCFDictionaryWithContentsOfFile --
 *
 *      Creates and returns a dictionary with the contents of the specified
 *      property list file. The file can be either XML or binary.
 *
 *      This is equivalent to +[NSDictionary dictionaryWithContentsOfFile:],
 *      unfortunately Apple did not provide a similar CoreFoundation function.
 *
 * Results:
 *      Dictionary with contents of the property list file, or NULL on failure.
 *      Caller must release the dictionary with CFRelease.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

CFDictionaryRef
UtilMacos_CreateCFDictionaryWithContentsOfFile(const char *path) // IN
{
   struct stat s;
   CFURLRef url;
   CFReadStreamRef stream = NULL;
   CFPropertyListRef plist = NULL;
   CFDictionaryRef result = NULL;

   /*
    * Avoid creating the unnecessary CFURL and CFReadStream objects if the
    * file does not exist. Trying to read a non-existent file will only
    * fail in CFReadStreamOpen().
    */
   if (Posix_Stat(path, &s) != 0) {
      return NULL;
   }

   url = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault,
                                                 (const UInt8 *)path,
                                                 strlen(path),
                                                 false);
   if (   url != NULL
       && (stream = CFReadStreamCreateWithFile(kCFAllocatorDefault, url))
       && CFReadStreamOpen(stream)
       && (plist = CFPropertyListCreateWithStream(kCFAllocatorDefault,
                                                  stream,
                                                  0,
                                                  kCFPropertyListImmutable,
                                                  NULL,
                                                  NULL))
       && (CFGetTypeID(plist) == CFDictionaryGetTypeID())) {
      result = (CFDictionaryRef)plist;
      plist = NULL;
   }

   if (plist) {
      CFRelease(plist);
   }

   if (stream) {
      CFReadStreamClose(stream);
      CFRelease(stream);
   }

   if (url) {
      CFRelease(url);
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilMacos_ReadSystemVersion --
 *
 *      Reads the Mac OS system version from the provided dictionary, and
 *      returns one or more of the requested values.
 *
 * Results:
 *      TRUE if the all the values were read successfully, or FALSE on failure.
 *      Caller must free the values.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
UtilMacos_ReadSystemVersion(CFDictionaryRef versionDict, // IN
                            char **productName,          // OUT/OPT
                            char **productVersion,       // OUT/OPT
                            char **productBuildVersion)  // OUT/OPT
{
   struct {
      const CFStringRef key;
      char **outValue;
   } keyedValues[] = {
      { CFSTR("ProductName"),         productName         },
      { CFSTR("ProductVersion"),      productVersion      },
      { CFSTR("ProductBuildVersion"), productBuildVersion },
   };
   int i;

   for (i = 0; i < ARRAYSIZE(keyedValues); i++) {
      CFStringRef curVal;

      if (keyedValues[i].outValue == NULL) {
         continue;
      }

      curVal = CFDictionaryGetValue(versionDict, keyedValues[i].key);
      if (   curVal != NULL
          && CFGetTypeID(curVal) == CFStringGetTypeID()
          && (*keyedValues[i].outValue = Util_CFStringToUTF8CString(curVal))) {
         continue;
      }

      /*
       * Error retrieving one of the values. Clean up any previously saved
       * values and return with failure.
       */
      while (--i >= 0) {
         if (keyedValues[i].outValue != NULL) {
            free(*keyedValues[i].outValue);
            *keyedValues[i].outValue = NULL;
         }
      }

      return FALSE;
   }

   return TRUE;
}

#endif // defined(__APPLE__)

/*
 * These are generic functions which might be useful on any platform
 * (ESX/Linux/BSD/Apple/Windows). Currently the Windows version have
 * not been defined as that is not very straightforward and I do not
 * know how to fully test it. f.e  we have to handle char* to wchar_t*
 * conversion, also it appears to cause linking problems for some Windows
 * tools.
 * If you want to use these for Windows, please feel free to fix these
 * and remove the ifdef macro below and in util.h.
 */

#if !defined(_WIN32)
/*
 *----------------------------------------------------------------------
 *
 * Util_IPv4AddrValid --
 *
 * Results:
 *      TRUE if the passed-in string represents a valid IPv4 address in
 *      dotted decimal notation, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_IPv4AddrValid(const char *addr)  // IN: IPv4 address string
{
   struct in_addr dummyInAddr;

   return inet_pton(AF_INET, addr, &dummyInAddr) == 1;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_IPv6AddrValid --
 *
 * Results:
 *      TRUE if the passed-in string represents a valid IPv6 address,
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_IPv6AddrValid(const char *addr)  // IN: IPv6 address string
{
   struct in6_addr dummyInAddr;
   char   ipv6AddrStr[INET6_ADDRSTRLEN + 1];

   /*
    * IPv6 link-local addresses can have a suffix of the form
    * %{ifname}, e.g. %vmk0, %eth0.
    * We cannot pass it as-is to inet_pton(), need to strip off the
    * suffix before passing to inet_pton().
    */
   if (sscanf(addr, "%"XSTR(INET6_ADDRSTRLEN)"[^%]",
              ipv6AddrStr) != 1) {
      return FALSE;
   }
   return inet_pton(AF_INET6, ipv6AddrStr, &dummyInAddr) == 1;
}


/*
 *----------------------------------------------------------------------
 *
 * Util_IPAddrValid --
 *
 * Results:
 *      TRUE if the passed-in string represents a valid IPv4 or IPv6 address,
 *      FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
Util_IPAddrValid(const char *addr)  // IN: IPv4 or IPv6 address string
{
   return Util_IPv4AddrValid(addr) || Util_IPv6AddrValid(addr);
}

#endif   /* ! _WIN32 */
