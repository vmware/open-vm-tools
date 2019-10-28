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
 * fileStandAlone.c --
 *
 * This file contains lib/file routines which are unentangled - they do
 * not depend on other libraries besides lib/misc and its dependencies.
 */

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "strutil.h"
#include "posix.h"
#include "file.h"

#include "unicodeOperations.h"


/*
 *----------------------------------------------------------------------
 *
 * File_GetModTime --
 *
 *      Get the last modification time of a file and return it. The time
 *      unit is seconds since the POSIX/UNIX/Linux epoch.
 *
 * Results:
 *      Last modification time of file or -1 if error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int64
File_GetModTime(const char *pathName)  // IN:
{
   int64 theTime;
   struct stat statbuf;

   if (Posix_Stat(pathName, &statbuf) == 0) {
      theTime = statbuf.st_mtime;
   } else {
      theTime = -1;
   }

   return theTime;
}


#if defined(_WIN32)
/*
 *---------------------------------------------------------------------------
 *
 * FileFindFirstDirsep --
 *
 *      Return a pointer to the first directory separator.
 *
 * Results:
 *      NULL  No directory separator found
 *     !NULL  Pointer to the last directory separator
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileFindFirstDirsep(const char *pathName)  // IN:
{
   char *p;

   ASSERT(pathName != NULL);

   p = (char *) pathName;

   while (*p != '\0') {
      if (File_IsDirsep(*p)) {
         return p;
      }

      p++;
   }

   return NULL;
}
#endif


/*
 *---------------------------------------------------------------------------
 *
 * FileFindLastDirsep --
 *
 *      Return a pointer to the last directory separator.
 *
 * Results:
 *      NULL  No directory separator found
 *     !NULL  Pointer to the last directory separator
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static char *
FileFindLastDirsep(const char *pathName,  // IN:
                   size_t len)            // IN:
{
   char *p;

   ASSERT(pathName != NULL);

   p = (char *) pathName + len;

   while (p-- != pathName) {
      if (File_IsDirsep(*p)) {
         return p;
      }
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * File_SplitName --
 *
 *      Split a file name into three components: VOLUME, DIRECTORY,
 *      BASE.  The return values must be freed.
 *
 *      VOLUME is empty for an empty string or a UNIX-style path, the
 *      drive letter and colon for a Win32 drive-letter path, or the
 *      construction "\\server\share" for a Win32 UNC path.
 *
 *      BASE is the longest string at the end that begins after the
 *      volume string and after the last directory separator.
 *
 *      DIRECTORY is everything in-between VOLUME and BASE.
 *
 *      The concatenation of VOLUME, DIRECTORY, and BASE produces the
 *      original string, so any of those strings may be empty.
 *
 *      A NULL pointer may be passed for one or more OUT parameters, in
 *      which case that parameter is not returned.
 *
 *      Able to handle both UNC and drive-letter paths on Windows.
 *
 * Results:
 *      As described.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
File_SplitName(const char *pathName,  // IN:
               char **volume,         // OUT/OPT:
               char **directory,      // OUT/OPT:
               char **base)           // OUT/OPT:
{
   char *vol;
   char *dir;
   char *bas;
   size_t len;
   char *baseBegin;
   char *volEnd;
   int volLen, dirLen;

   ASSERT(pathName != NULL);

   len = strlen(pathName);

   /*
    * Get volume.
    */

   volEnd = (char *) pathName;

#ifdef WIN32
   if ((len > 2) &&
       (!Str_Strncmp("\\\\", pathName, 2) ||
        !Str_Strncmp("//", pathName, 2))) {
      /* UNC path */
      volEnd = FileFindFirstDirsep(volEnd + 2);

      if (volEnd != NULL) {
         volEnd = FileFindFirstDirsep(volEnd + 1);

         if (volEnd == NULL) {
            /* We have \\foo\bar, which is legal */
            volEnd = (char *) pathName + len;
         }

      } else {
         /* We have \\foo, which is just bogus */
         volEnd = (char *) pathName;
      }
   } else if ((len >= 2) && (pathName[1] == ':')) {
      // drive-letter path
      volEnd = (char *) pathName + 2;
   }
#endif /* WIN32 */

   volLen = volEnd - pathName;
   vol = Util_SafeMalloc(volLen + 1);
   memcpy(vol, pathName, volLen);
   vol[volLen] = '\0';

   /*
    * Get base.
    */

   baseBegin = FileFindLastDirsep(pathName, len);
   baseBegin = (baseBegin == NULL) ? (char *) pathName : baseBegin + 1;

   if (baseBegin < volEnd) {
      baseBegin = (char *) pathName + len;
   }

   bas = Util_SafeStrdup(baseBegin);

   /*
    * Get dir.
    */

   dirLen = baseBegin - volEnd;
   dir = Util_SafeMalloc(dirLen + 1);
   memcpy(dir, volEnd, dirLen);
   dir[dirLen] = '\0';

   /*
    * Return what needs to be returned.
    */

   if (volume == NULL) {
      free(vol);
   } else {
      *volume = vol;
   }

   if (directory == NULL) {
      free(dir);
   } else {
      *directory = dir;
   }

   if (base == NULL) {
      free(bas);
   } else {
      *base = bas;
   }
}


/*
 *---------------------------------------------------------------------------
 *
 * File_PathJoin --
 *
 *      Join the dirName and baseName together to create a (full) path.
 *
 *      This code concatenates two strings together and omits a redundant
 *      directory separator between the two.
 *
 *      On Windows, the 'baseName' argument may not be a fully qualified path.
 *      That is, it may not be an absolute path containing a drive letter nor
 *      may it be a UNC path.
 *
 * Examples:
 *      File_PathJoin("", "b")            -> "/b"
 *      File_PathJoin("/", "b")           -> "/b"
 *      File_PathJoin("a", "b")           -> "a/b"
 *      File_PathJoin("a/", "b")          -> "a/b"
 *      File_PathJoin("a/////", "b")      -> "a/b"
 *      File_PathJoin("a", "")            -> "a/"
 *      File_PathJoin("a", "/")           -> "a/"
 *      File_PathJoin("a", "/b")          -> "a/b"
 *      File_PathJoin("a", "/////b")      -> "a/b" (only posix)
 *      File_PathJoin("a/", "/b")         -> "a/b"
 *      File_PathJoin("a/////", "/////b") -> "a/b" (only posix)
 *
 * Results:
 *      The constructed path which must be freed by the caller.
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

char *
File_PathJoin(const char *dirName,   // IN:
              const char *baseName)  // IN: See above.
{
   char *result;
   char *newDir = NULL;

   ASSERT(dirName);
   ASSERT(baseName);

   /*
    * Remove ALL directory separators from baseName begin.
    */
#if defined(_WIN32)
   {
      const char *oldBaseName = baseName;

      /*
       * Reject drive letters in baseName.
       */
      ASSERT(Unicode_LengthInCodePoints(baseName) < 2 ||
             Unicode_FindSubstrInRange(baseName, 1, 1, ":", 0, 1) ==
             UNICODE_INDEX_NOT_FOUND);

      while (*baseName == '/' || *baseName == '\\') {
         baseName++;
      }

      /*
       * Reject UNC paths for baseName.
       */
      ASSERT(baseName - oldBaseName < 2);
   }
#else
   while (*baseName == '/') {
      baseName++;
   }
#endif

   /*
    * Remove ALL directory separators from dirName end.
    */
   newDir = File_StripSlashes(dirName);

   result = Unicode_Join(newDir, DIRSEPS, baseName, NULL);
   Posix_Free(newDir);

   return result;
}


/*
 *---------------------------------------------------------------------------
 *
 * File_GetPathName --
 *
 *      Behaves like File_SplitName by splitting the fullpath into
 *      pathname & filename components.
 *
 *      The trailing directory separator [\|/] is stripped off the
 *      pathname component. This in turn means that on Linux the root
 *      directory will be returned as the empty string "". On Windows
 *      it will be returned as X: where X is the drive letter. It is
 *      important that callers of this functions are aware that the ""
 *      on Linux means root "/".
 *
 *      A NULL pointer may be passed for one or more OUT parameters,
 *      in which case that parameter is not returned.
 *
 * Results:
 *      As described.
 *
 * Side effects:
 *      The return values must be freed.
 *
 *---------------------------------------------------------------------------
 */

void
File_GetPathName(const char *fullPath,  // IN:
                 char **pathName,       // OUT/OPT:
                 char **baseName)       // OUT/OPT:
{
   char *p;
   char *pName;
   char *bName;
   ASSERT(fullPath);

   p = FileFindLastDirsep(fullPath, strlen(fullPath));

   if (p == NULL) {
      pName = Util_SafeStrdup("");
      bName = Util_SafeStrdup(fullPath);
   } else {
      bName = Util_SafeStrdup(&fullPath[p - fullPath + 1]);
      pName = Util_SafeStrdup(fullPath);
      pName[p - fullPath] = '\0';

      p = &pName[p - fullPath];

      while (p-- != pName) {
         if (File_IsDirsep(*p)) {
            *p = '\0';
         } else {
            break;
         }
      }
   }

   if (pathName == NULL) {
      free(pName);
   } else {
      *pathName = pName;
   }

   if (baseName == NULL) {
      free(bName);
   } else {
      *baseName = bName;
   }
}


/*
 *----------------------------------------------------------------------
 *
 *  File_StripSlashes --
 *
 *      Strip trailing slashes from the end of a path.
 *
 * Results:
 *      The stripped filename.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
File_StripSlashes(const char *path)  // IN:
{
   char *result;
   char *volume;
   char *dir;
   char *base;

   /*
    * SplitName handles all drive letter/UNC/whatever cases, all we
    * have to do is make sure the dir part is stripped of slashes if
    * there isn't a base part.
    */

   File_SplitName(path, &volume, &dir, &base);

   if (!Unicode_IsEmpty(dir) && Unicode_IsEmpty(base)) {
      char *dir2 = Unicode_GetAllocBytes(dir, STRING_ENCODING_UTF8);
      size_t i = strlen(dir2);

      /*
       * Don't strip first slash on Windows, since we want at least
       * one slash to trail a drive letter/colon or UNC specifier.
       */

#if defined(_WIN32)
      while ((i > 1) && File_IsDirsep(dir2[i - 1])) {
#else
      while ((i > 0) && File_IsDirsep(dir2[i - 1])) {
#endif
         i--;
      }

      Posix_Free(dir);
      dir = Unicode_AllocWithLength(dir2, i, STRING_ENCODING_UTF8);
      Posix_Free(dir2);
   }

   result = Unicode_Join(volume, dir, base, NULL);

   Posix_Free(volume);
   Posix_Free(dir);
   Posix_Free(base);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_MapPathPrefix --
 *
 *      Given a path and a newPrefix -> oldPrefix mapping, transform
 *      oldPath according to the mapping.
 *
 * Results:
 *      The new path, or NULL if there is no mapping.
 *
 * Side effects:
 *      The returned string is allocated, free it.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_MapPathPrefix(const char *oldPath,       // IN:
                   const char **oldPrefixes,  // IN:
                   const char **newPrefixes,  // IN:
                   size_t numPrefixes)        // IN:
{
   int i;
   size_t oldPathLen = strlen(oldPath);

   for (i = 0; i < numPrefixes; i++) {
      char *oldPrefix;
      char *newPrefix;
      size_t oldPrefixLen;

      oldPrefix = File_StripSlashes(oldPrefixes[i]);
      newPrefix = File_StripSlashes(newPrefixes[i]);
      oldPrefixLen = strlen(oldPrefix);

      /*
       * If the prefix matches on a DIRSEPS boundary, or the prefix is the
       * whole string, replace it.
       *
       * If we don't insist on matching a whole directory name, we could
       * mess things of if one directory is a substring of another.
       *
       * Perform a case-insensitive compare on Windows. (There are
       * case-insensitive filesystems on MacOS also, but the problem
       * is more acute with Windows because of frequent drive-letter
       * case mismatches. So in lieu of actually asking the
       * filesystem, let's just go with a simple ifdef for now.)
       */

      if ((oldPathLen >= oldPrefixLen) &&
#ifdef _WIN32
          (Str_Strncasecmp(oldPath, oldPrefix, oldPrefixLen) == 0) &&
#else
          (Str_Strncmp(oldPath, oldPrefix, oldPrefixLen) == 0) &&
#endif
          (strchr(VALID_DIRSEPS, oldPath[oldPrefixLen]) ||
              (oldPath[oldPrefixLen] == '\0'))) {
         size_t newPrefixLen = strlen(newPrefix);
         size_t newPathLen = (oldPathLen - oldPrefixLen) + newPrefixLen;
         char *newPath;

         ASSERT(newPathLen > 0);
         ASSERT(oldPathLen >= oldPrefixLen);

         newPath = Util_SafeMalloc((newPathLen + 1) * sizeof(char));
         memcpy(newPath, newPrefix, newPrefixLen);
         memcpy(newPath + newPrefixLen, oldPath + oldPrefixLen,
                oldPathLen - oldPrefixLen + 1);
         /*
          * It should only match once.  Weird self-referencing mappings
          * aren't allowed.
          */

         Posix_Free(oldPrefix);
         Posix_Free(newPrefix);

         return newPath;
      }
      Posix_Free(oldPrefix);
      Posix_Free(newPrefix);
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_PrependToPath --
 *
 *      This function checks if the elem is already present in the
 *      searchPath, if it is then it is moved forward in the search path.
 *      Otherwise it is prepended to the searchPath.
 *
 * Results:
 *      Return file search path with elem in front.
 *
 * Side effects:
 *      Caller must free returned string.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_PrependToPath(const char *searchPath,  // IN:
                   const char *elem)        // IN:
{
   const char sep = FILE_SEARCHPATHTOKEN[0];
   char *newPath;
   char *path;
   size_t n;

   ASSERT(searchPath);
   ASSERT(elem);

   newPath = Str_SafeAsprintf(NULL, "%s%s%s", elem, FILE_SEARCHPATHTOKEN,
                              searchPath);

   n = strlen(elem);
   path = newPath + n + 1;

   for (;;) {
      char *next = Str_Strchr(path, sep);
      size_t len = next ? next - path : strlen(path);

      if ((len == n) && (Str_Strncmp(path, elem, len) == 0)) {
         if (next) {
            memmove(path, next + 1, strlen(next + 1) + 1);
         } else {
            *--path = '\0';
         }
         break;
      }

      if (!next) {
         break;
      }
      path = next + 1;
   }

   return newPath;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_ReplaceExtension --
 *
 *      Replaces the extension in input with newExtension.
 *
 *      If the old extension exists in the list of extensions specified in ...,
 *      truncate it before appending the new extension.
 *
 *      If the extension is not found in the list, the newExtension is
 *      just appended.
 *
 *      If there isn't a list of extensions specified (numExtensions == 0),
 *      truncate the old extension unconditionally.
 *
 *      NB: newExtension and the extension list must have .'s.
 *
 * Results:
 *      The name with newExtension added to it. The caller is responsible to
 *      free it when they are done with it.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_ReplaceExtension(const char *pathName,      // IN:
                      const char *newExtension,  // IN:
                      uint32 numExtensions,      // IN:
                      ...)                       // IN:
{
   char *p;
   char *place;
   char *result;
   size_t newExtLen;
   size_t resultLen;
   size_t pathNameLen;

   ASSERT(pathName);
   ASSERT(newExtension);
   ASSERT(*newExtension == '.');

   pathNameLen = strlen(pathName);
   newExtLen = strlen(newExtension);
   resultLen = pathNameLen + newExtLen + 1;
   result = Util_SafeMalloc(resultLen);

   memcpy(result, pathName, pathNameLen + 1);

   p = FileFindLastDirsep(result, pathNameLen);
   if (p == NULL) {
       p = strrchr(result, '.');
   } else {
       p = strrchr(p, '.');
   }

   if (p == NULL) {
      /* No extension... just append */
      place = &result[pathNameLen];  // The NUL
   } else if (numExtensions == 0) {
      /* Always truncate the old extension if extension list is empty. */
      place = p;  // The '.'
   } else {
      uint32 i;
      va_list arguments;

      /*
       * Only truncate the old extension if it exists in the valid
       * extensions list.
       */

      place = &result[pathNameLen];  // The NUL

      va_start(arguments, numExtensions);

      for (i = 0; i < numExtensions ; i++) {
         const char *oldExtension = va_arg(arguments, const char *);

         ASSERT(*oldExtension == '.');

         if (strcmp(p, oldExtension) == 0) {
            place = p;  // The '.'
            break;
         }
      }

      va_end(arguments);
   }

   /* Add the new extension - in the appropriate place - to pathName */
   memcpy(place, newExtension, newExtLen + 1);

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_RemoveExtension --
 *
 *      Return a copy of the given path name with the extension
 *      removed. We ASSERT that the given path does have an extension.
 *
 * Results:
 *      A newly allocated buffer with the modified string. The caller
 *      is responsible to free it when they are done with it.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_RemoveExtension(const char *pathName)  // IN:
{
   char *p;
   char *result;

   ASSERT(pathName != NULL);

   result = Util_SafeStrdup(pathName);

   p = FileFindLastDirsep(result, strlen(pathName));
   if (p == NULL) {
       p = strrchr(result, '.');
   } else {
       p = strrchr(p, '.');
   }

   ASSERT(p != NULL);

   if (p != NULL) {
      *p = '\0';
   }

   return result;
}
