/*********************************************************
 * Copyright (C) 1998-2016 VMware, Inc. All rights reserved.
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
#include "safetime.h"
#if !defined(_WIN32)
#include <unistd.h>
#endif
#include <string.h>
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


/*
 *----------------------------------------------------------------------
 *
 * FileFirstSlashIndex --
 *
 *      Finds the first pathname slash index in a path (both slashes count
 *      for Win32, only forward slash for Unix).
 *
 * Results:
 *      As described.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

UnicodeIndex
FileFirstSlashIndex(const char *pathName,     // IN:
                    UnicodeIndex startIndex)  // IN:
{
   UnicodeIndex firstFS;
#if defined(_WIN32)
   UnicodeIndex firstBS;
#endif

   ASSERT(pathName);

   firstFS = Unicode_FindSubstrInRange(pathName, startIndex, -1,
                                       "/", 0, 1);

#if defined(_WIN32)
   firstBS = Unicode_FindSubstrInRange(pathName, startIndex, -1,
                                       "\\", 0, 1);

   if ((firstFS != UNICODE_INDEX_NOT_FOUND) &&
       (firstBS != UNICODE_INDEX_NOT_FOUND)) {
      return MIN(firstFS, firstBS);
   } else {
     return (firstFS == UNICODE_INDEX_NOT_FOUND) ? firstBS : firstFS;
   }
#else
   return firstFS;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileLastSlashIndex --
 *
 *      Finds the last pathname slash index in a path (both slashes count
 *      for Win32, only forward slash for Unix).
 *
 * Results:
 *      As described.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static UnicodeIndex
FileLastSlashIndex(const char *pathName,     // IN:
                   UnicodeIndex startIndex)  // IN:
{
   UnicodeIndex lastFS;
#if defined(_WIN32)
   UnicodeIndex lastBS;
#endif

   ASSERT(pathName);

   lastFS = Unicode_FindLastSubstrInRange(pathName, startIndex, -1,
                                          "/", 0, 1);

#if defined(_WIN32)
   lastBS = Unicode_FindLastSubstrInRange(pathName, startIndex, -1,
                                          "\\", 0, 1);

   if ((lastFS != UNICODE_INDEX_NOT_FOUND) &&
       (lastBS != UNICODE_INDEX_NOT_FOUND)) {
      return MAX(lastFS, lastBS);
   } else {
     return (lastFS == UNICODE_INDEX_NOT_FOUND) ? lastBS : lastFS;
   }
#else
   return lastFS;
#endif
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
   UnicodeIndex volEnd;
   UnicodeIndex length;
   UnicodeIndex baseBegin;
   WIN32_ONLY(UnicodeIndex pathLen);

   ASSERT(pathName);

   /*
    * Get volume.
    */

   volEnd = 0;

#if defined(_WIN32)
   pathLen = Unicode_LengthInCodePoints(pathName);
   if ((pathLen > 2) &&
       (StrUtil_StartsWith(pathName, "\\\\") ||
        StrUtil_StartsWith(pathName, "//"))) {
      /* UNC path */
      volEnd = FileFirstSlashIndex(pathName, 2);

      if (volEnd == UNICODE_INDEX_NOT_FOUND) {
         /* we have \\foo, which is just bogus */
         volEnd = 0;
      } else {
         volEnd = FileFirstSlashIndex(pathName, volEnd + 1);

         if (volEnd == UNICODE_INDEX_NOT_FOUND) {
            /* we have \\foo\bar, which is legal */
            volEnd = pathLen;
         }
      }
   } else if ((pathLen >= 2) &&
              (Unicode_FindSubstrInRange(pathName, 1, 1, ":", 0,
                                         1) != UNICODE_INDEX_NOT_FOUND)) {
      /* drive-letter path */
      volEnd = 2;
   }

   if (volEnd > 0) {
      vol = Unicode_Substr(pathName, 0, volEnd);
   } else {
      vol = Unicode_Duplicate("");
   }
#else
   vol = Unicode_Duplicate("");
#endif /* _WIN32 */

   /*
    * Get base.
    */

   baseBegin = FileLastSlashIndex(pathName, 0);
   baseBegin = (baseBegin == UNICODE_INDEX_NOT_FOUND) ? 0 : baseBegin + 1;

   if (baseBegin >= volEnd) {
      bas = Unicode_Substr(pathName, baseBegin, -1);
   } else {
      bas = Unicode_Duplicate("");
   }

   /*
    * Get dir.
    */

   length = baseBegin - volEnd;

   if (length > 0) {
      dir = Unicode_Substr(pathName, volEnd, length);
   } else {
      dir = Unicode_Duplicate("");
   }

   /*
    * Return what needs to be returned.
    */

   if (volume) {
      *volume = vol;
   } else {
      free(vol);
   }

   if (directory) {
      *directory = dir;
   } else {
      free(dir);
   }

   if (base) {
      *base = bas;
   } else {
      free(bas);
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
   free(newDir);

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
   char *volume;
   UnicodeIndex len;
   UnicodeIndex curLen;

   File_SplitName(fullPath, &volume, pathName, baseName);

   if (pathName == NULL) {
      free(volume);
      return;
   }

   /*
    * The volume component may be empty.
    */

   if (!Unicode_IsEmpty(volume)) {
      char *temp = Unicode_Append(volume, *pathName);

      free(*pathName);
      *pathName = temp;
   }
   free(volume);

   /*
    * Remove any trailing directory separator characters.
    */

   len = Unicode_LengthInCodePoints(*pathName);

   curLen = len;

   while ((curLen > 0) &&
          (FileFirstSlashIndex(*pathName, curLen - 1) == curLen - 1)) {
      curLen--;
   }

   if (curLen < len) {
      char *temp = Unicode_Substr(*pathName, 0, curLen);

      free(*pathName);
      *pathName = temp;
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
      while ((i > 1) && (('/' == dir2[i - 1]) ||
                         ('\\' == dir2[i - 1]))) {
#else
      while ((i > 0) && ('/' == dir2[i - 1])) {
#endif
         i--;
      }

      free(dir);
      dir = Unicode_AllocWithLength(dir2, i, STRING_ENCODING_UTF8);
      free(dir2);
   }

   result = Unicode_Join(volume, dir, base, NULL);

   free(volume);
   free(dir);
   free(base);

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
      char *newPath;
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

         free(oldPrefix);
         free(newPrefix);

         return newPath;
      }
      free(oldPrefix);
      free(newPrefix);
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
   char *path;
   char *base;
   char *result;
   va_list arguments;
   UnicodeIndex index;

   ASSERT(pathName);
   ASSERT(newExtension);
   ASSERT(*newExtension == '.');

   File_GetPathName(pathName, &path, &base);

   index = Unicode_FindLast(base, ".");

   if (index != UNICODE_INDEX_NOT_FOUND) {
      char *oldBase = base;

      if (numExtensions) {
         uint32 i;

         /*
          * Only truncate the old extension from the base if it exists in
          * in the valid extensions list.
          */

         va_start(arguments, numExtensions);

         for (i = 0; i < numExtensions ; i++) {
            char *oldExtension = va_arg(arguments, char *);

            ASSERT(*oldExtension == '.');

            if (Unicode_CompareRange(base, index, -1,
                                     oldExtension, 0, -1, FALSE) == 0) {
               base = Unicode_Truncate(oldBase, index); // remove '.'
               break;
            }
         }

         va_end(arguments);
      } else {
         /* Always truncate the old extension if extension list is empty . */
         base = Unicode_Truncate(oldBase, index); // remove '.'
      }

      if (oldBase != base) {
         free(oldBase);
      }
   }

   if (Unicode_IsEmpty(path)) {
      result = Unicode_Append(base, newExtension);
   } else {
      result = Unicode_Join(path, DIRSEPS, base, newExtension, NULL);
   }

   free(path);
   free(base);

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
   UnicodeIndex index;

   ASSERT(pathName);

   index = Unicode_FindLast(pathName, ".");
   ASSERT(index != UNICODE_INDEX_NOT_FOUND);

   return Unicode_Truncate(pathName, index);
}
