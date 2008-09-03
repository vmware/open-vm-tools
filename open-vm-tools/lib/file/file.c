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
 * file.c --
 *
 *        Interface to host file system.  See also filePosix.c,
 *        fileWin32.c, etc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include "safetime.h"
#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "msg.h"
#include "uuid.h"
#include "config.h"
#include "posix.h"
#include "file.h"
#include "fileIO.h"
#include "fileInt.h"
#include "stats_file.h"
#include "dynbuf.h"
#include "base64.h"
#include "timeutil.h"
#include "hostinfo.h"
#if !defined(N_PLAT_NLM)
#include "vm_atomic.h"
#endif

#include "unicodeOperations.h"

#define SETUP_DEFINE_VARS
#include "stats_user_setup.h"

#if !defined(O_BINARY)
#define O_BINARY 0
#endif

/*
 *----------------------------------------------------------------------
 *
 * File_Exists --
 *
 *      Check if a file exists.
 *
 * Results:
 *      TRUE    file exists
 *      FALSE   file doesn't exist or an error occured
 *      
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_Exists(ConstUnicode pathName)  // IN:
{
   return FileIO_IsSuccess(FileIO_Access(pathName, FILEIO_ACCESS_EXISTS));
}


/*
 *----------------------------------------------------------------------
 *
 * File_UnlinkIfExists --
 *
 *      If the given file exists, unlink it. 
 *
 * Results:
 *      Return 0 if the unlink is successful or if the file did not exist.   
 *      Otherwise return -1.
 *
 * Side effects:
 *      May unlink the file.
 *      
 *----------------------------------------------------------------------
 */

int
File_UnlinkIfExists(ConstUnicode pathName)  // IN:
{
   int ret;

   ret = FileDeletion(pathName, TRUE);

   if (ret != 0) {
      ret = (ret == ENOENT) ? 0 : -1;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsDirectory --
 *
 *      Check if specified file is a directory or not.
 *
 * Results:
 *      TRUE    is a directory
 *      FALSE   is not a directory or an error occured
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsDirectory(ConstUnicode pathName)  // IN:
{
   FileData fileData;

   return (FileAttributes(pathName, &fileData) == 0) &&
           (fileData.fileType == FILE_TYPE_DIRECTORY);
}


/*
 *----------------------------------------------------------------------
 *
 * File_Unlink --
 *
 *      Unlink the file.
 *
 *      POSIX: If name is a symbolic link, then unlink the the file the link
 *      refers to as well as the link itself.  Only one level of links are
 *      followed.
 *      WINDOWS: No symbolic links so no link following.
 *
 * Results:
 *      Return 0 if the unlink is successful. Otherwise, returns -1.
 *
 * Side effects:
 *      The file is removed.
 *
 *----------------------------------------------------------------------
 */

int
File_Unlink(ConstUnicode pathName)  // IN:
{
   return (FileDeletion(pathName, TRUE) == 0) ? 0 : -1;
}

/*
 *----------------------------------------------------------------------
 *
 * File_UnlinkNoFollow --
 *
 *      Unlink the file (do not follow symbolic links).
 *      On Windows, there are no symbolic links so this is the same as
 *      File_Unlink
 *
 * Results:
 *      Return 0 if the unlink is successful. Otherwise, returns -1.
 *
 * Side effects:
 *      The file is removed.
 *
 *----------------------------------------------------------------------
 */

int
File_UnlinkNoFollow(ConstUnicode pathName)  // IN:
{
   return (FileDeletion(pathName, FALSE) == 0) ? 0 : -1;
}




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
File_GetModTime(ConstUnicode pathName)  // IN:
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
 * File_CreateDirectory --
 *
 *      Creates the specified directory.
 *
 * Results:
 *      True if the directory is successfully created, false otherwise.
 *
 * Side effects:
 *      Creates the directory on disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_CreateDirectory(ConstUnicode pathName)  // IN:
{
   return (FileCreateDirectory(pathName) == 0) ? TRUE : FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * File_EnsureDirectory --
 *
 *      If the directory doesn't exist, creates it. If the directory
 *      already exists, do nothing and succeed.
 *
 * Results:
 *      See above.
 *
 * Side effects:
 *      May create a directory on disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_EnsureDirectory(ConstUnicode pathName)  // IN:
{
   int res = FileCreateDirectory(pathName);
   return ((0 == res) || (EEXIST == res));
}


/*
 *----------------------------------------------------------------------
 *
 * File_DeleteEmptyDirectory --
 *
 *      Deletes the specified directory if it is empty.
 *
 * Results:
 *      True if the directory is successfully deleted, false otherwise.
 *
 * Side effects:
 *      Deletes the directory from disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_DeleteEmptyDirectory(ConstUnicode pathName)  // IN:
{
   return (FileRemoveDirectory(pathName) == 0) ? TRUE : FALSE;
}


#if !defined(N_PLAT_NLM)
/*
 *----------------------------------------------------------------------
 *
 * GetOldMachineID --
 *
 *      Return the old machineID, the one based on Hostinfo_MachineID.
 *
 * Results:
 *      The machineID is returned.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static const char *
GetOldMachineID(void)
{
   static Atomic_Ptr atomic; /* Implicitly initialized to NULL. --mbellon */
   const char        *machineID;

   machineID = Atomic_ReadPtr(&atomic);

   if (machineID == NULL) {
      char *p;
      uint32 hashValue;
      uint64 hardwareID;
      char encodedMachineID[16 + 1];
      char rawMachineID[sizeof hashValue + sizeof hardwareID];

      Hostinfo_MachineID(&hashValue, &hardwareID);

      // Build the raw machineID
      memcpy(rawMachineID, &hashValue, sizeof hashValue);
      memcpy(&rawMachineID[sizeof hashValue], &hardwareID,
             sizeof hardwareID);

      // Base 64 encode the binary data to obtain printable characters
      Base64_Encode(rawMachineID, sizeof rawMachineID, encodedMachineID,
                    sizeof encodedMachineID, NULL);

      // remove any '/' from the encoding; no problem using it for a file name
      for (p = encodedMachineID; *p; p++) {
         if (*p == '/') {
            *p = '-';
         }
      }

      p = Util_SafeStrdup(encodedMachineID);

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         free(p);
      }

      machineID = Atomic_ReadPtr(&atomic);
      ASSERT(machineID);
   }

   return machineID;
}


/*
 *----------------------------------------------------------------------
 *
 * FileLockGetMachineID --
 *
 *      Return the machineID, a "universally unique" identification of
 *      of the system that calls this routine.
 *
 *      An attempt is first made to use the host machine's UUID. If that
 *      fails drop back to the older machineID method.
 *
 * Results:
 *      The machineID is returned.
 *
 * Side effects:
 *      Memory allocated for the machineID is never freed, however the
 *      memory is cached - there is no memory leak.
 *
 *----------------------------------------------------------------------
 */

const char *
FileLockGetMachineID(void)
{
   static Atomic_Ptr atomic; /* Implicitly initialized to NULL. --mbellon */
   const char        *machineID;

   machineID = Atomic_ReadPtr(&atomic);

   if (machineID == NULL) {
      char *p;
      char *q;

      /*
       * UUID_GetHostRealUUID is fine on Windows.
       *
       * UUID_GetHostUUID is fine on Macs because the UUID can't be found
       * in /dev/mem even if it can be accessed. Macs always use the MAC
       * address from en0 to provide a UUID.
       *
       * UUID_GetHostUUID is problematic on Linux so it is not acceptable for
       * locking purposes - it accesses /dev/mem to obtain the SMBIOS UUID
       * and that can fail when the calling process is not priviledged.
       *
       */

#if defined(_WIN32)
      q = UUID_GetRealHostUUID();
#elif defined(__APPLE__) || defined(VMX86_SERVER)
      q = UUID_GetHostUUID();
#else
      q = NULL;
#endif

      if (q == NULL) {
         p = (char *) GetOldMachineID();
      } else {
         p = Str_SafeAsprintf(NULL, "uuid=%s", q);
         free(q);

         /* Surpress any whitespace. */
         for (q = p; *q; q++) {
            if (isspace((int) *q)) {
               *q = '-';
            }
         }
      }

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         free(p);
      }

      machineID = Atomic_ReadPtr(&atomic);
      ASSERT(machineID);
   }

   return machineID;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OldMachineIDMatch --
 *
 *	Do the old-style MachineIDs match?
 *
 * Results:
 *      TRUE     Yes
 *      FALSE    No
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
OldMachineIDMatch(const char *first,  // IN:
                  const char *second) // IN:
{
#if defined(__APPLE__) || defined(linux)
   /* Ignore the host name hash */
   char *p;
   char *q;
   size_t len;
   Bool result;
   uint8 rawMachineID_1[12];
   uint8 rawMachineID_2[12];

   for (p = Util_SafeStrdup(first), q = p; *p; p++) {
      if (*p == '-') {
         *p = '/';
      }
   }
   result = Base64_Decode(q, rawMachineID_1, sizeof rawMachineID_1, &len);
   free(q);

   if ((result == FALSE) || (len != 12)) {
      Warning("%s: unexpected decode problem #1 (%s)\n", __FUNCTION__,
              first);
      return FALSE;
   }

   for (p = Util_SafeStrdup(second), q = p; *p; p++) {
      if (*p == '-') {
         *p = '/';
      }
   }
   result = Base64_Decode(q, rawMachineID_2, sizeof rawMachineID_2, &len);
   free(q);

   if ((result == FALSE) || (len != 12)) {
      Warning("%s: unexpected decode problem #2 (%s)\n", __FUNCTION__,
              second);
      return FALSE;
   }

   return memcmp(&rawMachineID_1[4],
                 &rawMachineID_2[4], 8) == 0 ? TRUE : FALSE;
#else
   return strcmp(first, second) == 0 ? TRUE : FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileLockMachineIDMatch --
 *
 *	Do the MachineIDs match?
 *
 * Results:
 *      TRUE     Yes
 *      FALSE    No
 *
 * Side effects:
 *	None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileLockMachineIDMatch(char *hostMachineID,  // IN:
                       char *otherMachineID) // IN:
{
   if (strncmp(hostMachineID, "uuid=", 5) == 0) {
      if (strncmp(otherMachineID, "uuid=", 5) == 0) {
         return strcmp(hostMachineID + 5,
                       otherMachineID + 5) == 0 ? TRUE : FALSE;
      } else {
         return OldMachineIDMatch(GetOldMachineID(), otherMachineID);
      }
   } else {
      if (strncmp(otherMachineID, "uuid=", 5) == 0) {
         return FALSE;
      } else {
         return strcmp(hostMachineID, otherMachineID) == 0 ? TRUE : FALSE;
      }
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * File_IsEmptyDirectory --
 *
 *      Check if specified file is a directory and contains no files.
 *
 * Results:
 *      Bool - TRUE -> is an empty directory, FALSE -> not an empty directory
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
File_IsEmptyDirectory(ConstUnicode pathName)  // IN:
{
   int numFiles;

   if (!File_IsDirectory(pathName)) {
      return FALSE;
   }

   numFiles = File_ListDirectory(pathName, NULL);
   if (numFiles < 0) {
      return FALSE;
   }

   return numFiles == 0;
}
#endif /* N_PLAT_NLM */


/*
 *----------------------------------------------------------------------
 *
 * File_IsFile --
 *
 *      Check if specified file is a regular file.
 *
 * Results:
 *      TRUE	is a regular file
 *      FALSE	is not a regular file or an error occured.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsFile(ConstUnicode pathName)  // IN:
{
   FileData fileData;

   return (FileAttributes(pathName, &fileData) == 0) &&
           (fileData.fileType == FILE_TYPE_REGULAR);
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

static UnicodeIndex
FileFirstSlashIndex(ConstUnicode pathName,    // IN:
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
FileLastSlashIndex(ConstUnicode pathName,    // IN:
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
File_SplitName(ConstUnicode pathName,  // IN:
               Unicode *volume,        // OUT (OPT):
               Unicode *directory,     // OUT (OPT):
               Unicode *base)          // OUT (OPT):
{
   Unicode vol;
   Unicode dir;
   Unicode bas;
   UnicodeIndex volEnd;
   UnicodeIndex length;
   UnicodeIndex pathLen;
   UnicodeIndex baseBegin;

   ASSERT(pathName);

   pathLen = Unicode_LengthInCodeUnits(pathName);

   /*
    * Get volume.
    */

   volEnd = 0;

#if defined(_WIN32)
   if ((pathLen > 2) &&
       (Unicode_StartsWith(pathName, "\\\\") ||
        Unicode_StartsWith(pathName, "//"))) {
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
      Unicode_Free(vol);
   }

   if (directory) {
      *directory = dir;
   } else {
      Unicode_Free(dir);
   }

   if (base) {
      *base = bas;
   } else {
      Unicode_Free(bas);
   }
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
File_GetPathName(ConstUnicode fullPath,  // IN:
                 Unicode *pathName,      // OUT (OPT):
                 Unicode *baseName)      // OUT (OPT):
{
   Unicode volume;
   UnicodeIndex len;
   UnicodeIndex curLen;

   File_SplitName(fullPath, &volume, pathName, baseName);

   if (pathName == NULL) {
      Unicode_Free(volume);
      return;
   }

   /*
    * The volume component may be empty.
    */
   if (Unicode_LengthInCodeUnits(volume) > 0) {
      Unicode temp;

      temp = Unicode_Append(volume, *pathName);
      Unicode_Free(*pathName);
      *pathName = temp;
   }
   Unicode_Free(volume);

   /*
    * Remove any trailing directory separator characters.
    */

   len = Unicode_LengthInCodeUnits(*pathName);

   curLen = len;

   while ((curLen > 0) &&
          (FileFirstSlashIndex(*pathName, curLen - 1) == curLen - 1)) {
      curLen--;
   }

   if (curLen < len) {
      Unicode temp;

      temp = Unicode_Substr(*pathName, 0, curLen);
      Unicode_Free(*pathName);
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

Unicode
File_StripSlashes(ConstUnicode path) // IN
{
   Unicode result, volume, dir, base;

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
#ifdef _WIN32
      while ((i > 1) && (('/' == dir2[i - 1]) ||
                         ('\\' == dir2[i - 1]))) {
#else
      while ((i > 0) && ('/' == dir2[i - 1])) {
#endif
         i--;
      }

      Unicode_Free(dir);
      dir = Unicode_AllocWithLength(dir2, i, STRING_ENCODING_UTF8);
      free(dir2);
   }

   result = Unicode_Join(volume, dir, base, NULL);

   Unicode_Free(volume);
   Unicode_Free(dir);
   Unicode_Free(base);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 *  File_MakeTempEx --
 *
 *      Create a temporary file and, if successful, return an open file
 *      descriptor to that file.
 *
 *      'dir' specifies the directory in which to create the file. It
 *      must not end in a slash.
 *
 *      'fileName' specifies the base filename of the created file.
 *
 * Results:
 *      Open file descriptor or -1; if successful then presult points
 *      to a dynamically allocated string with the pathname of the temp
 *      file.
 *
 * Side effects:
 *      Creates a file if successful. Errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
File_MakeTempEx(ConstUnicode dir,       // IN:
                ConstUnicode fileName,  // IN:
                Unicode *presult)       // OUT:
{
   int fd;
   int err;
   uint32 var;

   Unicode path = NULL;
   Unicode basePath = NULL;

   if ((dir == NULL) || (fileName == NULL)) {
      errno = EFAULT;
      return -1;
   }

   ASSERT(presult);

   *presult = NULL;

   /* construct base full pathname to use */
   basePath = Unicode_Join(dir, DIRSEPS, fileName, NULL);

   for (var = 0; var < 0xFFFFFFFF; var++) {
      Unicode temp;

      /* construct suffixed pathname to use */
      Unicode_Free(path);

      temp = Unicode_Format("%d", var);
      ASSERT_MEM_ALLOC(temp);
      path = Unicode_Append(basePath, temp);
      Unicode_Free(temp);

      fd = Posix_Open(path, O_CREAT | O_EXCL | O_BINARY | O_RDWR, 0600);

      if (fd != -1) {
         *presult = path;
         path = NULL;
         break;
      }

      if (errno != EEXIST) {
         err = errno;
         Msg_Append(MSGID(file.maketemp.openFailed)
                 "Failed to create temporary file \"%s\": %s.\n",
                 UTF8(path), Msg_ErrString());
         errno = err;
         goto exit;
      }
   }

   if (fd == -1) {
      Msg_Append(MSGID(file.maketemp.fullNamespace)
                 "Failed to create temporary file \"%s\": The name space is "
                 "full.\n", UTF8(path));

      errno = EAGAIN;
   }

  exit:
   err = errno;
   Unicode_Free(basePath);
   Unicode_Free(path);
   errno = err;

   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 *  File_MakeTemp --
 *
 *      Create a temporary file and, if successful, return an open file
 *      descriptor to the file.
 *
 *      'tag' can either be a full pathname, a string, or NULL.
 *
 *      If 'tag' is a full pathname, that path will be used as the root
 *      path for the file.
 *
 *      If 'tag' is a string, the created file's filename will begin
 *      with 'tag' and will be created in the default temp directory.
 *
 *      If 'tag' is NULL, then 'tag' is assumed to be "vmware" and the
 *      above case applies.
 *
 *      This API is technically unsafe if you allow this function to use
 *      the default temp directory since it's not guaranteed on Windows
 *      that when the file is closed it is not readable by other users
 *      (no matter what we specify as the mode to open, the new file
 *      will inherit DACLs from the parent, and certain temp directories
 *      on Windows give all Power Users read&write access). Please use
 *      Util_MakeSafeTemp if your dependencies permit it.
 *
 * Results:
 *      Open file descriptor or -1; if successful then filename points
 *      to a dynamically allocated string with the pathname of the temp
 *      file.
 *
 * Side effects:
 *      Creates a file if successful. Errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
File_MakeTemp(ConstUnicode tag,  // IN (OPT):
              Unicode *presult)  // OUT:
{
   int fd;
   int err;
   Unicode dir;
   Unicode fileName;

   if (tag && File_IsFullPath(tag)) {
      File_GetPathName(tag, &dir, &fileName);
   } else {
      dir = File_GetTmpDir(TRUE);
      fileName = Unicode_Duplicate(tag ? tag : "vmware");
   }

   fd = File_MakeTempEx(dir, fileName, presult);
   err = errno;

   Unicode_Free(dir);
   Unicode_Free(fileName);

   errno = err;

   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CopyFromFdToFd --
 *
 *      Write all data between the current position in the 'src' file and the
 *      end of the 'src' file to the current position in the 'dst' file
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure
 *
 * Side effects:
 *      The current position in the 'src' file and the 'dst' file are modified
 *
 *----------------------------------------------------------------------
 */

Bool
File_CopyFromFdToFd(FileIODescriptor src,  // IN:
                    FileIODescriptor dst)  // IN:
{
   FileIOResult fretR;

   do {
      unsigned char buf[1024];
      size_t actual;
      FileIOResult fretW;

      fretR = FileIO_Read(&src, buf, sizeof(buf), &actual);
      if (!FileIO_IsSuccess(fretR) && (fretR != FILEIO_READ_ERROR_EOF)) {
         Msg_Append(MSGID(File.CopyFromFdToFd.read.failure)
                               "Read error: %s.\n\n", FileIO_MsgError(fretR));
         return FALSE;
      }

      fretW = FileIO_Write(&dst, buf, actual, NULL);
      if (!FileIO_IsSuccess(fretW)) {
         Msg_Append(MSGID(File.CopyFromFdToFd.write.failure)
                              "Write error: %s.\n\n", FileIO_MsgError(fretW));
         return FALSE;
      }
   } while (fretR != FILEIO_READ_ERROR_EOF);

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CopyFromFdToName --
 *
 *      Copy the 'src' file to 'dstName'.
 *      If the 'dstName' file already exists,
 *      If 'dstDispose' is -1, the user is prompted for proper action.
 *      If 'dstDispose' is 0, retry until success (dangerous).
 *      If 'dstDispose' is 1, overwrite the file.
 *      If 'dstDispose' is 2, return the error.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure: if the user cancelled the operation, no message is
 *                        appended. Otherwise messages are appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_CopyFromFdToName(FileIODescriptor src,  // IN:
                      ConstUnicode dstName,  // IN:
                      int dstDispose)        // IN:
{
   FileIODescriptor dst;
   FileIOResult fret;
   Bool result;

   ASSERT(dstName);

   FileIO_Invalidate(&dst);

   fret = File_CreatePrompt(&dst, dstName, 0, dstDispose);
   if (!FileIO_IsSuccess(fret)) {
      if (fret != FILEIO_CANCELLED) {
         Msg_Append(MSGID(File.CopyFromFdToName.create.failure)
                    "Unable to create a new '%s' file: %s.\n\n",
                    UTF8(dstName), FileIO_MsgError(fret));
      }

      return FALSE;
   }

   result = File_CopyFromFdToFd(src, dst);

   if (FileIO_Close(&dst) != 0) {
      Msg_Append(MSGID(File.CopyFromFdToName.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", UTF8(dstName),
                 Msg_ErrString());
      result = FALSE;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CreatePrompt --
 *
 *      Create the 'name' file for write access or 'access' access.
 *      If the 'name' file already exists,
 *      If 'prompt' is not -1, it is the automatic answer to the question that
 *      would be asked to the user if it was -1.
 *
 * Results:
 *      FILEIO_CANCELLED if the operation was cancelled by the user, otherwise
 *      as FileIO_Open()
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
File_CreatePrompt(FileIODescriptor *file,  // OUT:
                  ConstUnicode pathName,   // IN:
                  int access,              // IN:
                  int prompt)              // IN:
{
   FileIOOpenAction action;
   FileIOResult fret;

   ASSERT(file);
   ASSERT(pathName);

   action = FILEIO_OPEN_CREATE_SAFE;

   while ((fret = FileIO_Open(file, pathName, FILEIO_OPEN_ACCESS_WRITE | access,
                             action)) == FILEIO_OPEN_ERROR_EXIST) {
      static Msg_String const buttons[] = {
         {BUTTONID(file.create.retry) "Retry"},
         {BUTTONID(file.create.overwrite) "Overwrite"},
         {BUTTONID(cancel) "Cancel"},
         {NULL}
      };
      int answer;

      answer = (prompt != -1) ? prompt : Msg_Question(buttons, 2,
         MSGID(File.CreatePrompt.question)
         "The file '%s' already exists.\n"
         "To overwrite the content of the file, select Overwrite.\n"
         "To retry the operation after you have moved the file "
         "to another location, select Retry.\n"
         "To cancel the operation, select Cancel.\n",
         UTF8(pathName));
      if (answer == 2) {
         fret = FILEIO_CANCELLED;
         break;
      }
      if (answer == 1) {
         action = FILEIO_OPEN_CREATE_EMPTY;
      }
   }

   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CopyFromNameToName --
 *
 *      Copy the 'srcName' file to 'dstName'.
 *      If 'srcName' doesn't exist, an error is reported
 *      If the 'dstName' file already exists,
 *      If 'dstDispose' is -1, the user is prompted for proper action.
 *      If 'dstDispose' is 0, retry until success (dangerous).
 *      If 'dstDispose' is 1, overwrite the file.
 *      If 'dstDispose' is 2, return the error.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure: if the user cancelled the operation, no message is
 *                        appended. Otherwise messages are appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_CopyFromNameToName(ConstUnicode srcName,  // IN:
                        ConstUnicode dstName,  // IN:
                        int dstDispose)        // IN:
{
   FileIODescriptor src;
   FileIOResult fret;
   Bool result;

   ASSERT(srcName);
   ASSERT(dstName);

   FileIO_Invalidate(&src);

   fret = FileIO_Open(&src, srcName, FILEIO_OPEN_ACCESS_READ, FILEIO_OPEN);
   if (!FileIO_IsSuccess(fret)) {
      Msg_Append(MSGID(File.CopyFromNameToName.open.failure)
                 "Unable to open the '%s' file for read access: %s.\n\n",
                 UTF8(srcName), FileIO_MsgError(fret));
      return FALSE;
   }

   result = File_CopyFromFdToName(src, dstName, dstDispose);
   
   if (FileIO_Close(&src) != 0) {
      Msg_Append(MSGID(File.CopyFromNameToName.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", UTF8(srcName),
                 Msg_ErrString());
      result = FALSE;
   }

   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * File_CopyFromFd --
 *
 *      Copy the 'src' file to 'dstName'.
 *      If the 'dstName' file already exists, 'overwriteExisting'
 *      decides whether to overwrite the existing file or not.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure: Messages are appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_CopyFromFd(FileIODescriptor src,     // IN:
                ConstUnicode dstName,     // IN:
                Bool overwriteExisting)   // IN:
{
   FileIODescriptor dst;
   FileIOOpenAction action;
   FileIOResult fret;
   Bool result;

   ASSERT(dstName);

   FileIO_Invalidate(&dst);

   action = overwriteExisting ? FILEIO_OPEN_CREATE_EMPTY :
                                FILEIO_OPEN_CREATE_SAFE;

   fret = FileIO_Open(&dst, dstName, FILEIO_OPEN_ACCESS_WRITE, action);
   if (!FileIO_IsSuccess(fret)) {
      Msg_Append(MSGID(File.CopyFromFdToName.create.failure)
                 "Unable to create a new '%s' file: %s.\n\n", UTF8(dstName),
                 FileIO_MsgError(fret));
      return FALSE;
   }

   result = File_CopyFromFdToFd(src, dst);

   if (FileIO_Close(&dst) != 0) {
      Msg_Append(MSGID(File.CopyFromFdToName.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", UTF8(dstName),
                 Msg_ErrString());
      result = FALSE;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_Copy --
 *
 *      Copy the 'srcName' file to 'dstName'.
 *      If 'srcName' doesn't exist, an error is reported
 *      If the 'dstName' file already exists, 'overwriteExisting'
 *      decides whether to overwrite the existing file or not.
 *
 * Results:
 *      TRUE on success
 *      FALSE on failure: Messages are appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_Copy(ConstUnicode srcName,    // IN:
          ConstUnicode dstName,    // IN:
          Bool overwriteExisting)  // IN:
{
   FileIODescriptor src;
   FileIOResult fret;
   Bool result;

   ASSERT(srcName);
   ASSERT(dstName);

   FileIO_Invalidate(&src);

   fret = FileIO_Open(&src, srcName, FILEIO_OPEN_ACCESS_READ, FILEIO_OPEN);
   if (!FileIO_IsSuccess(fret)) {
      Msg_Append(MSGID(File.Copy.open.failure)
                 "Unable to open the '%s' file for read access: %s.\n\n",
                 UTF8(srcName), FileIO_MsgError(fret));
      return FALSE;
   }

   result = File_CopyFromFd(src, dstName, overwriteExisting);
   
   if (FileIO_Close(&src) != 0) {
      Msg_Append(MSGID(File.Copy.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", UTF8(srcName),
                 Msg_ErrString());
      result = FALSE;
   }

   return result;
}

/*
 *----------------------------------------------------------------------
 *
 * File_Rename --
 *
 *      Renames a source to a destination file.
 *      Will copy the file if necessary
 *
 * Results:
 *      TRUE if succeeded FALSE otherwise
 *      
 * Side effects:
 *      src file is no more, but dst file exists
 *
 *----------------------------------------------------------------------
 */

Bool 
File_Rename(ConstUnicode oldFile,  // IN:
            ConstUnicode newFile)  // IN:
{
   Bool ret = TRUE;

   if (FileRename(oldFile, newFile) != 0) {
      /* overwrite the file if it exists */
      if (File_Copy(oldFile, newFile, TRUE)) {
         File_Unlink(oldFile);
      } else {
         ret = FALSE;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetModTimeString --
 *
 *      Returns a human-readable string denoting the last modification 
 *      time of a file.
 *      ctime() returns string terminated with newline, which we replace
 *      with a '\0'.
 *
 * Results:
 *      Last modification time string on success, NULL on error.
 *      
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

char *
File_GetModTimeString(ConstUnicode pathName)  // IN:
{
   int64 modTime;

   modTime = File_GetModTime(pathName);

   return (modTime == -1) ? NULL : TimeUtil_GetTimeFormat(modTime, TRUE, TRUE);
}


/*
 *----------------------------------------------------------------------------
 *
 * File_GetSize --
 *
 *      Get size of file.
 *
 * Results:
 *      Size of file or -1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int64
File_GetSize(ConstUnicode pathName)  // IN:
{
   int64 ret;

   if (pathName == NULL) {
      ret = -1;
   } else {
      FileIODescriptor fd;
      FileIOResult res;

      FileIO_Invalidate(&fd);
      res = FileIO_Open(&fd, pathName, FILEIO_OPEN_ACCESS_READ, FILEIO_OPEN);

      if (FileIO_IsSuccess(res)) {
         ret = FileIO_GetSize(&fd);
         FileIO_Close(&fd);
      } else {
         ret = -1;
      }
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_SupportsLargeFiles --
 *
 *      Check if the given file is on an FS that supports 4GB files.
 *      Require 4GB support so we rule out FAT filesystems, which
 *      support 4GB-1 on both Linux and Windows.
 *
 * Results:
 *      TRUE if FS supports files over 4GB.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_SupportsLargeFiles(ConstUnicode pathName)  // IN:
{
   return File_SupportsFileSize(pathName, CONST64U(0x100000000));
}


#if !defined(N_PLAT_NLM)
/*
 *----------------------------------------------------------------------------
 *
 * File_GetSizeByPath --
 *
 *      Get size of a file without opening it.
 *
 * Results:
 *      Size of file or -1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int64
File_GetSizeByPath(ConstUnicode pathName)
{
   return (pathName == NULL) ? -1 : FileIO_GetSizeByPath(pathName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_CreateDirectoryHierarchy --
 *
 *      Create a directory including any parents that don't already exist.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 * Side effects:
 *      Only the obvious.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_CreateDirectoryHierarchy(ConstUnicode pathName)
{
   Unicode volume;
   UnicodeIndex index;
   UnicodeIndex length;

   if (pathName == NULL) {
      return TRUE;
   }

   length = Unicode_LengthInCodeUnits(pathName);

   if (length == 0) {
      return TRUE;
   }

   /*
    * Skip past any volume/share.
    */

   File_SplitName(pathName, &volume, NULL, NULL);

   index = Unicode_LengthInCodeUnits(volume);

   Unicode_Free(volume);

   if (index >= length) {
      return File_IsDirectory(pathName);
   }

   /*
    * Iterate parent directories, splitting on appropriate dir separators.
    */

   while (TRUE) {
      Bool failed;
      Unicode temp;

      index = FileFirstSlashIndex(pathName, index + 1);

      if (index == UNICODE_INDEX_NOT_FOUND) {
         break;
      }

      temp = Unicode_Substr(pathName, 0, index);

      failed = !File_IsDirectory(temp) && !File_CreateDirectory(temp);

      Unicode_Free(temp);

      if (failed) {
         return FALSE;
      }
   }

   return File_IsDirectory(pathName) || File_CreateDirectory(pathName);
}


/*
 *----------------------------------------------------------------------
 *
 * File_DeleteDirectoryTree --
 *
 *      Deletes the specified directory tree. If filesystem errors are
 *      encountered along the way, the function will continue to delete what it
 *      can but will return FALSE.
 *
 * Results:
 *      TRUE if the entire tree was deleted or didn't exist, FALSE otherwise.
 *      
 * Side effects:
 *      Deletes the directory tree from disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_DeleteDirectoryTree(ConstUnicode pathName)  // IN: directory to delete
{
   int i;
   int numFiles;
   Unicode base;

   Unicode *fileList = NULL;
   Bool sawFileError = FALSE;

   if (!File_Exists(pathName)) {
      return TRUE;
   }

   /* get list of files in current directory */
   numFiles = File_ListDirectory(pathName, &fileList);

   if (numFiles == -1) {
      return FALSE;
   }

   /* delete everything in the directory */
   base = Unicode_Append(pathName, DIRSEPS);

   for (i = 0; i < numFiles; i++) {
      Unicode curPath;

      curPath = Unicode_Append(base, fileList[i]);

      if (File_IsDirectory(curPath)) {
         /* is dir, recurse */
         if (!File_DeleteDirectoryTree(curPath)) {
            sawFileError = TRUE;
         }
      } else {
         /* is file, delete */
         if (File_Unlink(curPath) == -1) {
            sawFileError = TRUE;
         }
      }

      Unicode_Free(curPath);
   }

   Unicode_Free(base);

   /* delete the now-empty directory */
   if (!File_DeleteEmptyDirectory(pathName)) {
      sawFileError = TRUE;
   }

   for (i = 0; i < numFiles; i++) {
      Unicode_Free(fileList[i]);
   }

   free(fileList);

   return !sawFileError;
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
File_PrependToPath(const char *searchPath,   // IN
                   const char *elem)         // IN
{
   const char sep = FILE_SEARCHPATHTOKEN[0];
   char *newPath;
   char *path;
   size_t n;

   ASSERT(searchPath);
   ASSERT(elem);

   newPath = Str_SafeAsprintf(NULL, "%s" FILE_SEARCHPATHTOKEN "%s",
                          elem, searchPath);

   n = strlen(elem);
   path = newPath + n + 1;

   for (;;) {
      char *next = Str_Strchr(path, sep);
      size_t len = next ? next - path : strlen(path);

      if (len == n && Str_Strncmp(path, elem, len) == 0) {
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
 * File_FindFileInSearchPath --
 *
 *      Search all the directories in searchPath for a filename.
 *      If searchPath has a relative path take it with respect to cwd.
 *      searchPath must be ';' delimited.
 *
 * Results:
 *      TRUE if a file is found. FALSE otherwise.
 *
 * Side effects:
 *      If result is non Null allocate a string for the filename found.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_FindFileInSearchPath(const char *fileIn,       // IN
                          const char *searchPath,   // IN
                          const char *cwd,          // IN
                          char **result)            // OUT
{
   Bool found = FALSE;
   char *cur = NULL;
   char *sp = NULL;
   char *file = NULL;
   char *tok;

   ASSERT(fileIn);
   ASSERT(cwd);
   ASSERT(searchPath);

   /*
    * First check the usual places, the fullpath, and the cwd.
    */

   if (File_IsFullPath(fileIn)) {
      cur = Util_SafeStrdup(fileIn);
   } else {
      cur = Str_SafeAsprintf(NULL, "%s"DIRSEPS"%s", cwd, fileIn);
   }

   if (File_Exists(cur)) {
      goto found;
   }
   free(cur);

   /*
    * Didn't find it in the usual places so strip it to its bare minimum and
    * start searching.
    */
   File_GetPathName(fileIn, NULL, &file);

   sp = Util_SafeStrdup(searchPath);
   tok = strtok(sp, FILE_SEARCHPATHTOKEN);

   while (tok) {
      if (File_IsFullPath(tok)) {
         /* Fully Qualified Path. Use it. */
         cur = Str_SafeAsprintf(NULL, "%s%s%s", tok, DIRSEPS, file);
      } else {
         /* Relative Path.  Prepend the cwd. */
         if (Str_Strcasecmp(tok, ".") == 0) {
            /* Don't append "." */
            cur = Str_SafeAsprintf(NULL, "%s"DIRSEPS"%s", cwd, file);
         } else {
            cur = Str_SafeAsprintf(NULL, "%s"DIRSEPS"%s"DIRSEPS"%s", cwd,
                                   tok, file);
         }
      }

      if (File_Exists(cur)) {
         goto found;
      }
      free(cur);
      tok = strtok(NULL, FILE_SEARCHPATHTOKEN);
   }

  exit:
   free(sp);
   free(file);
   return found;

  found:
   ASSERT(cur);
   found = TRUE;
   if (result) {
      *result = File_FullPath(cur);

      if (*result == NULL) {
         found = FALSE;
      }
   }
   free(cur);
   goto exit;
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

Unicode
File_ReplaceExtension(ConstUnicode pathName,      // IN:
                      ConstUnicode newExtension,  // IN:
                      uint32 numExtensions,       // IN:
                      ...)                        // IN:
{
   Unicode path;
   Unicode base;
   Unicode result;
   va_list arguments;
   UnicodeIndex index;
   
   ASSERT(pathName);
   ASSERT(newExtension);
   ASSERT(Unicode_StartsWith(newExtension, "."));

   File_GetPathName(pathName, &path, &base);

   index = Unicode_FindLast(base, ".");

   if (index != UNICODE_INDEX_NOT_FOUND) {
      Unicode oldBase = base;

      if (numExtensions) {
         uint32 i;

         /*
          * Only truncate the old extension from the base if it exists in
          * in the valid extensions list.
          */

         va_start(arguments, numExtensions);

         for (i = 0; i < numExtensions ; i++) {
            Unicode oldExtension = va_arg(arguments, Unicode);

            ASSERT(Unicode_StartsWith(oldExtension, "."));

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
         Unicode_Free(oldBase);
      }
   }

   if (Unicode_IsEmpty(path)) {
      result = Unicode_Append(base, newExtension);
   } else {
      result = Unicode_Join(path, DIRSEPS, base, newExtension, NULL);
   }

   Unicode_Free(path);
   Unicode_Free(base);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_ExpandAndCheckDir --
 *
 *	Expand any environment variables in the given path and check that
 *	the named directory is writeable.
 *
 * Results:
 *	NULL if error, the expanded path otherwise.
 *
 * Side effects:
 *	The result is allocated.
 *
 *----------------------------------------------------------------------
 */

char *
File_ExpandAndCheckDir(const char *dirName)
{
   char *edirName;

   if (dirName != NULL) {
      edirName = Util_ExpandString(dirName);
      if (edirName != NULL) {
	 if (File_IsWritableDir(edirName) == TRUE) {
            if (edirName[strlen(edirName) - 1] == DIRSEPC) {
               edirName[strlen(edirName) - 1] = '\0';
            }
	    return edirName;
	 }
	 free(edirName);
      }
   }
   return NULL;
}

#endif // N_PLAT_NLM
