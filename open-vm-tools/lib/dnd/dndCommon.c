/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/

/*
 * dndCommon.c --
 *
 *     Implementation of bora/lib/public/dnd.h functions that are common to
 *     Linux and Windows platforms
 */


#include <stdlib.h>
#include <time.h>
#include <limits.h>

#include "dndInt.h"
#include "dnd.h"
#include "file.h"
#include "str.h"
#include "random.h"
#include "util.h"
#include "cpNameUtil.h"
#include "hgfsServerPolicy.h"
#include "hgfsVirtualDir.h"
#include "unicodeOperations.h"

#define LOGLEVEL_MODULE dnd
#include "loglevel_user.h"

#define WIN_DIRSEPC     '\\'
#define WIN_DIRSEPS     "\\"

static ConstUnicode DnDCreateRootStagingDirectory(void);
Bool DnDDataContainsIllegalCharacters(const char *data,
                                      const size_t dataSize,
                                      const char *illegalChars);
Bool DnDPrependFileRoot(const char *fileRoot, const char delimiter,
                        char **src, size_t *srcSize);


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_CreateStagingDirectory --
 *
 *    Generate a unique staging directory name, create the directory, and
 *    return the name. The caller is responsible for freeing the returned
 *    string.
 *
 *    Our staging directory structure is comprised of a "root" staging
 *    directory that itself contains multiple staging directories that are
 *    intended to be used on a per-DnD and per-user basis.  That is, each DnD
 *    by a particular user will have its own staging directory within the root.
 *    Sometimes these directories are emptied after the DnD (either because it
 *    was cancelled or the destination application told us to), and we resuse
 *    any empty directories that we can.  This function will return a directory
 *    to be reused if possible and fall back on creating a new one if
 *    necessary.
 *
 * Results:
 *    A string containing the newly created name, or NULL on failure.
 *
 * Side effects:
 *    A directory is created
 *
 *-----------------------------------------------------------------------------
 */

Unicode
DnD_CreateStagingDirectory(void)
{
   ConstUnicode root;
   Unicode *stagingDirList;
   int numStagingDirs;
   int i;
   Unicode ret = NULL;
   Bool found = FALSE;

   /*
    * Make sure the root staging directory is created with the correct
    * permissions.
    */
   root = DnDCreateRootStagingDirectory();
   if (!root) {
      return NULL;
   }

   /* Look for an existing, empty staging directory */
   numStagingDirs = File_ListDirectory(root, &stagingDirList);
   if (numStagingDirs < 0) {
      goto exit;
   }

   for (i = 0; i < numStagingDirs; i++) {
      if (!found) {
         Unicode stagingDir;

         stagingDir = Unicode_Append(root, stagingDirList[i]);

         if (File_IsEmptyDirectory(stagingDir) &&
             DnDStagingDirectoryUsable(stagingDir)) {
               ret = Unicode_Append(stagingDir, U(DIRSEPS));
               /*
                * We can use this directory.  Make sure to continue to loop
                * so we don't leak the remaining stagindDirList[i]s.
                */
               found = TRUE;
         }

         Unicode_Free(stagingDir);
      }

      Unicode_Free(stagingDirList[i]);
   }

   free(stagingDirList);

   /* Only create a directory if we didn't find one above. */
   if (!found) {
      void *p;

      p = Random_QuickSeed((unsigned)time(NULL));

      for (i = 0; i < 10; i++) {
         Unicode temp;
         char string[16];

         /* Each staging directory is given a random name. */
         Unicode_Free(ret);
         Str_Sprintf(string, sizeof string, "%08x%c", Random_Quick(p),
                     DIRSEPC);
         temp = Unicode_Alloc(string, STRING_ENCODING_US_ASCII);
         ret = Unicode_Append(root, temp);
         Unicode_Free(temp);

         if (File_CreateDirectory(ret) &&
             DnDSetPermissionsOnStagingDir(ret)) {
            found = TRUE;
            break;
         }
      }

      free(p);
   }

exit:
   if (!found && ret != NULL) {
      Unicode_Free(ret);
      ret = NULL;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDCreateRootStagingDirectory --
 *
 *    Checks if the root staging directory exists with the correct permissions,
 *    or creates it if necessary.
 *
 * Results:
 *    The path of the root directory on success, NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static ConstUnicode
DnDCreateRootStagingDirectory(void)
{
   ConstUnicode root;

   /*
    * DnD_GetFileRoot() gives us a pointer to a static string, so there's no
    * need to free anything.
    */
   root = DnD_GetFileRoot();
   if (!root) {
      return NULL;
   }

   if (File_Exists(root)) {
      if (!DnDRootDirUsable(root) &&
          !DnDSetPermissionsOnRootDir(root)) {
         /*
          * The directory already exists and its permissions are wrong and
          * cannot be set, so there's not much we can do.
          */
         return NULL;
      }
   } else {
      if (!File_CreateDirectory(root) ||
          !DnDSetPermissionsOnRootDir(root)) {
         /* We couldn't create the directory or set the permissions. */
         return NULL;
      }
   }

   return root;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDDataContainsIllegalCharacters --
 *
 *    Determines whether the data buffer contains any of the illegal
 *    characters.  This is the common code used by platform-dependent
 *    implementations of DnD_DataContainsIllegalCharacters().
 *
 * Results:
 *    TRUE if data contains any illegalChars, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDDataContainsIllegalCharacters(const char *data,         // IN: buffer
                                 const size_t dataSize,    // IN: size of buffer
                                 const char *illegalChars) // IN: chars to look for
{
   size_t i;

   ASSERT(data);
   ASSERT(illegalChars);

   /* We don't just call strchr(3) here since data may contain NUL characters. */
   for (i = 0; i < dataSize; i++) {
      const char *currIllegalChar = illegalChars;

      while (*currIllegalChar != '\0') {
         if (data[i] == *currIllegalChar) {
            LOG(1, ("DnDDataContainsIllegalCharacters: found illegal character \'%c\'\n",
                    data[i]));
            return TRUE;
         }
         currIllegalChar++;
      }
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnDPrependFileRoot --
 *
 *    Given a buffer of '\0' delimited filenames, this prepends the file root
 *    to each one and uses delimiter for delimiting the output buffer.  The
 *    buffer pointed to by *src will be freed and *src will point to a new
 *    buffer containing the results.  *srcSize is set to the size of the new
 *    buffer, not including the NUL-terminator.
 *
 *    We can't simply use Str_Sprintf here because it calls FormatMessage on
 *    Win32 which doesn't play well with Unicode strings.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    *src will be freed, and a new buffer will be allocated. This buffer must
 *    be freed by the caller.
 *
 *----------------------------------------------------------------------------
 */

Bool
DnDPrependFileRoot(const char *fileRoot,  // IN    : file root to append
                   const char delimiter,  // IN    : delimiter for output buffer
                   char **src,            // IN/OUT: NUL-delimited list of paths
                   size_t *srcSize)       // IN/OUT: size of list
{
   char *newData = NULL;
   size_t newDataLen = 0;
   Bool firstPass = TRUE;
   const char *begin;
   const char *end;
   const char *next;
   size_t rootLen;
   int len;

   ASSERT(fileRoot);
   ASSERT(src);
   ASSERT(*src);
   ASSERT(srcSize);

   rootLen = strlen(fileRoot);

   /*
    * To prevent CPName_GetComponentGeneric() errors, we set begin to the first
    * Non-NUL character in *src, and end to the last NUL character in *src.  We
    * assume that the components are delimited with single NUL characters; if
    * that is not true, CPName_GetComponentGeneric() will fail.
    */
   for (begin = *src; *begin == '\0'; begin++)
      ;
   end = CPNameUtil_Strrchr(*src, *srcSize, '\0');

   /* Get the length of this component, and a pointer to the next */
   while ((len = CPName_GetComponentGeneric(begin, end, "", &next)) != 0) {
      size_t origNewDataLen = newDataLen;

      if (len < 0) {
         Log("DnDPrependFileRoot: error getting next component\n");
         if (!firstPass) {
            free(newData);
         }
         return FALSE;
      }

      /*
       * Append this component to our list: allocate one more for NUL on first
       * pass and delimiter on all other passes.
       */
      newDataLen += rootLen + len + 1;
      newData = (char *)Util_SafeRealloc(newData, newDataLen);

      if (!firstPass) {
         ASSERT(origNewDataLen > 0);
         newData[origNewDataLen - 1] = delimiter;
      }
      memcpy(newData + origNewDataLen, fileRoot, rootLen);
      memcpy(newData + origNewDataLen + rootLen, begin, len);
      newData[newDataLen - 1] = '\0';

      firstPass = FALSE;
      begin = next;
   }

   free(*src);
   *src = newData;
   /* Not including NUL terminator */
   *srcSize = newDataLen - 1;
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_UTF8Asprintf --
 *
 *    Str_Asprintf should not be used with UTF-8 strings as it uses 
 *    FormatMessage. This interprets UTF-8 strings as a string in the current
 *    locale giving wrong results. This function is otherwise funcationally
 *    equivalent to Str_Asprintf. The caller must first compute how large the
 *    output buffer needs to be and pass it in as outBufSize.
 *
 * Results:
 *    The allocated string on success.
 *    NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

char *
DnD_UTF8Asprintf(unsigned int outBufSize, // IN: size of output buffer
                 const char *format,      // IN
                 ...)                     // IN
{
   va_list arguments;
   char *buffer = NULL;

   ASSERT(format);
   va_start(arguments, format);

   if (!(buffer = (char *)malloc(outBufSize))) {
      Log("DnD_UTF8Asprintf: Error creating string.\n");
      goto exit;
   }

   if (Str_Vsnprintf(buffer, outBufSize, format, arguments) < 0) {
      Log("DnD_UTF8Asprintf: Error writing to string.\n");
      free (buffer);
      buffer = NULL;
   }

exit:
   va_end(arguments);
   return buffer;
}


/*
 *----------------------------------------------------------------------------
 *
 * DnD_LegacyConvertToCPName --
 *
 *    Converts paths received from older tools that do not send data in CPName
 *    format across the backdoor.  Older tools send paths in Windows format so
 *    this implementation must always convert from Windows path to CPName path,
 *    regardless of the platform we are running on.
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
DnD_LegacyConvertToCPName(const char *nameIn,   // IN:  Buffer to convert
                          size_t bufOutSize,    // IN:  Size of output buffer
                          char *bufOut)         // OUT: Output buffer
{
   const char partialName[] = HGFS_SERVER_POLICY_ROOT_SHARE_NAME;
   const size_t partialNameLen = HGFS_STR_LEN(HGFS_SERVER_POLICY_ROOT_SHARE_NAME);
   const char *partialNameSuffix = "";
   size_t partialNameSuffixLen;
   char *fullName;
   size_t fullNameSize;
   size_t nameSize;
   int result;

   ASSERT(nameIn);
   ASSERT(bufOut);

   /*
    * Create the full name. Note that Str_Asprintf should not be
    * used here as it uses FormatMessages which interprets 'data', a UTF-8
    * string, as a string in the current locale giving wrong results.
    */

   /*
    * Is this file path a UNC path?
    */
   if (nameIn[0] == WIN_DIRSEPC && nameIn[1] == WIN_DIRSEPC) {
      partialNameSuffix    = WIN_DIRSEPS HGFS_UNC_DIR_NAME WIN_DIRSEPS;
      partialNameSuffixLen = HGFS_STR_LEN(WIN_DIRSEPS) +
                             HGFS_STR_LEN(HGFS_UNC_DIR_NAME) +
                             HGFS_STR_LEN(WIN_DIRSEPS);
   } else {
      partialNameSuffix    = WIN_DIRSEPS HGFS_DRIVE_DIR_NAME WIN_DIRSEPS;
      partialNameSuffixLen = HGFS_STR_LEN(WIN_DIRSEPS) +
                             HGFS_STR_LEN(HGFS_DRIVE_DIR_NAME) +
                             HGFS_STR_LEN(WIN_DIRSEPS);
   }

   /* Skip any path separators at the beginning of the input string */
   while (*nameIn == WIN_DIRSEPC) {
      nameIn++;
   }

   nameSize = strlen(nameIn);
   fullNameSize = partialNameLen + partialNameSuffixLen + nameSize;
   fullName = (char *)Util_SafeMalloc(fullNameSize + 1);

   memcpy(fullName, partialName, partialNameLen);
   memcpy(fullName + partialNameLen, partialNameSuffix, partialNameSuffixLen);
   memcpy(fullName + partialNameLen + partialNameSuffixLen, nameIn, nameSize);
   fullName[fullNameSize] = '\0';

   LOG(4, ("DnD_LegacyConvertToCPName: generated name is \"%s\"\n", fullName));

   /*
    * CPName_ConvertTo implementation is performed here without calling any
    * CPName_ functions.  This is safer since those functions might change, but
    * the legacy behavior we are special casing here will not.
    */
   {
      char const *winNameIn = fullName;
      char const *origOut = bufOut;
      char const *endOut = bufOut + bufOutSize;
      char const pathSep = WIN_DIRSEPC;
      char *ignores = ":";

      /* Skip any path separators at the beginning of the input string */
      while (*winNameIn == pathSep) {
         winNameIn++;
      }

      /*
       * Copy the string to the output buf, converting all path separators into
       * '\0' and ignoring the specified characters.
       */
      for (; *winNameIn != '\0' && bufOut < endOut; winNameIn++) {
         if (ignores) {
            char *currIgnore = ignores;
            Bool ignore = FALSE;

            while (*currIgnore != '\0') {
               if (*winNameIn == *currIgnore) {
                  ignore = TRUE;
                  break;
               }
               currIgnore++;
            }

            if (!ignore) {
               *bufOut = (*winNameIn == pathSep) ? '\0' : *winNameIn;
               bufOut++;
            }
         } else {
            *bufOut = (*winNameIn == pathSep) ? '\0' : *winNameIn;
            bufOut++;
         }
      }

      /*
       * NUL terminate. XXX This should go away.
       *
       * When we get rid of NUL termination here, this test should
       * also change to "if (*winNameIn != '\0')".
       */
      if (bufOut == endOut) {
         result = -1;
         goto out;
      }
      *bufOut = '\0';

      /* Path name size should not require more than 4 bytes. */
      ASSERT((bufOut - origOut) <= 0xFFFFFFFF);

      /* If there were any trailing path separators, dont count them [krishnan] */
      result = (int)(bufOut - origOut);
      while ((result >= 1) && (origOut[result - 1] == 0)) {
         result--;
      }

      /* Make exception and call CPName_Print() here, since it's only for logging */
      LOG(4, ("DnD_LegacyConvertToCPName: CPName is \"%s\"\n",
              CPName_Print(origOut, result)));
   }

out:
   free(fullName);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_GetLastDirName --
 *
 *    Try to get last directory name from a full path name.
 *
 * Results:
 *    size of dirName if success, 0 otherwise.
 *
 * Side effects:
 *    Memory may allocated for dirName if success.
 *
 *-----------------------------------------------------------------------------
 */

size_t
DnD_GetLastDirName(const char *str,   // IN: can be UTF-8
                   size_t strSize,    // IN
                   char **dirName)    // OUT
{
   size_t end = strSize;
   size_t start;
   size_t res = 0;

   if (end != 0 && DIRSEPC == str[end - 1]) {
      end--;
   }

   if (end == 0) {
      return 0;
   }

   start = end;

   while (start && DIRSEPC != str[start - 1]) {
      start--;
   }

   /* There should be at lease 1 DIRSEPC before end. */
   if (start == 0) {
      return 0;
   }

   res = end - start;
   *dirName = (char *)Util_SafeMalloc(res + 1);
   memcpy(*dirName, str + start, res);
   (*dirName)[res] = '\0';
   return res;
}
