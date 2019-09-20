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
 * file.c --
 *
 *        Interface to host file system.  See also filePosix.c,
 *        fileWin32.c, etc.
 *
 *        If a function can be implemented such that it has no dependencies
 *        outside of lib/misc, place the function in fileStandAlone.c, NOT
 *        here.
 */

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#if defined(_WIN32)
#include <io.h>
#define S_IXUSR    0100
#define S_IWUSR    0200
#else
#include <unistd.h>
#endif
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "vmware.h"
#include "util.h"
#include "str.h"
#include "msg.h"
#include "log.h"
#include "random.h"
#include "uuid.h"
#include "config.h"
#include "posix.h"
#include "file.h"
#include "fileIO.h"
#include "util.h"
#include "fileInt.h"
#include "dynbuf.h"
#include "base64.h"
#include "timeutil.h"
#include "hostinfo.h"
#include "hostType.h"
#include "vm_atomic.h"
#include "fileLock.h"
#include "userlock.h"

#include "unicodeOperations.h"


/*
 *----------------------------------------------------------------------
 *
 * File_Exists --
 *
 *      Check if a file is accessible with the process' real user ID
 *
 *      XXX - This function invokes access(), which uses the real uid,
 *      not the effective uid, so it probably does not do what you
 *      expect.  Instead it should use Posix_EuidAccess(), which
 *      uses the effective uid, but it's too risky to fix right now.
 *      See PR 459242.
 *
 *      Errno/GetLastError is available upon failure.
 *
 * Results:
 *      TRUE    file is accessible with the process' real uid
 *      FALSE   file doesn't exist or an error occured
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_Exists(const char *pathName)  // IN: May be NULL.
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
 *      Errno/GetLastError is available upon failure.
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
File_UnlinkIfExists(const char *pathName)  // IN:
{
   int ret = FileDeletion(pathName, TRUE);

   if (ret != 0) {
      ret = (ret == ENOENT) ? 0 : -1;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_SupportsMandatoryLock --
 *
 *      Determines if the underlying filesystem for a particular location
 *      can support mandatory locking. Mandatory locking is used within
 *      FileLock to make the advisory FileLock self-cleaning in the event
 *      of host failure.
 *
 * Results:
 *      TRUE if FILEIO_OPEN_EXCLUSIVE_LOCK will work, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_SupportsMandatoryLock(const char *pathName) // IN: file to be locked
{
   /*
    * For now, "know" that all ESX filesystems support mandatory locks
    * and no non-ESX filesystems support mandatory locks.
    */
   return HostType_OSIsVMK();
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsDirectory --
 *
 *      Check if specified file is a directory or not.
 *
 *      Errno/GetLastError is available upon failure.
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
File_IsDirectory(const char *pathName)  // IN:
{
   FileData fileData;

   return (FileAttributes(pathName, &fileData) == 0) &&
           (fileData.fileType == FILE_TYPE_DIRECTORY);
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetFilePermissions --
 *
 *      Return the read / write / execute permissions of a file.
 *
 *      Errno/GetLastError is available upon failure.
 *
 * Results:
 *      TRUE if success, FALSE otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_GetFilePermissions(const char *pathName,  // IN:
                        int *mode)             // OUT: file mode
{
   FileData fileData;

   ASSERT(mode != NULL);

   if (FileAttributes(pathName, &fileData) != 0) {
      return FALSE;
   }

   *mode = fileData.fileMode;

#if defined(_WIN32)
      /*
       * On Win32 implementation of FileAttributes does not return execution
       * bit.
       */

      if (FileIO_Access(pathName, FILEIO_ACCESS_EXEC) == FILEIO_SUCCESS) {
         *mode |= S_IXUSR;
      }
#endif

   return TRUE;
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
 *      Errno/GetLastError is available upon failure.
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
File_Unlink(const char *pathName)  // IN:
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
 *      Errno/GetLastError is available upon failure.
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
File_UnlinkNoFollow(const char *pathName)  // IN:
{
   return (FileDeletion(pathName, FALSE) == 0) ? 0 : -1;
}


/*
 *----------------------------------------------------------------------
 *
 * File_UnlinkRetry --
 *
 *      Unlink the file, retrying on EBUSY on ESX, up to given timeout.
 *
 * Results:
 *      Return 0 if the unlink is successful. Otherwise, return -1.
 *
 * Side effects:
 *      The file is removed.
 *
 *----------------------------------------------------------------------
 */

int
File_UnlinkRetry(const char *pathName,       // IN:
                 uint32 maxWaitTimeMilliSec) // IN:
{
   int ret;

   if (vmx86_server) {
      uint32 const unlinkWait = 300;
      uint32 waitMilliSec = 0;

      do {
         ret = FileDeletion(pathName, TRUE);
         if (ret != EBUSY || waitMilliSec >= maxWaitTimeMilliSec) {
            break;
         }
         Log(LGPFX" %s: %s after %u ms\n", __FUNCTION__, pathName, unlinkWait);
         Util_Usleep(unlinkWait * 1000);
         waitMilliSec += unlinkWait;
      } while (TRUE);
   } else {
      ret = FileDeletion(pathName, TRUE);
   }

   return ret == 0 ? 0 : -1;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CreateDirectoryEx --
 *
 *      Creates the specified directory with the specified permissions.
 *
 *      Errno/GetLastError is available upon failure.
 *
 * Results:
 *      TRUE   Directory was created
 *      FALSE  Directory creation failed.
 *             See File_EnsureDirectoryEx for dealing with directories that
 *             may exist.
 *
 * Side effects:
 *      Creates the directory on disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_CreateDirectoryEx(const char *pathName,  // IN:
                       int mode)              // IN:
{
   int err = FileCreateDirectory(pathName, mode);

   return err == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * File_CreateDirectory --
 *
 *      Creates the specified directory with 0777 permissions.
 *
 *      Errno/GetLastError is available upon failure.
 *
 * Results:
 *      TRUE   Directory was created
 *      FALSE  Directory creation failed.
 *             See File_EnsureDirectory for dealing with directories that
 *             may exist.
 *
 * Side effects:
 *      Creates the directory on disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_CreateDirectory(const char *pathName)  // IN:
{
   int err = FileCreateDirectory(pathName, 0777);

   return err == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * File_EnsureDirectoryEx --
 *
 *      If the directory doesn't exist, creates it. If the directory
 *      already exists, do nothing and succeed.
 *
 *      Errno/GetLastError is available upon failure.
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
File_EnsureDirectoryEx(const char *pathName,  // IN:
                       int mode)              // IN:
{
   int err = FileCreateDirectory(pathName, mode);

   if (err == EEXIST) {
      FileData fileData;

      err = FileAttributes(pathName, &fileData);

      if (err == 0) {
         if (fileData.fileType != FILE_TYPE_DIRECTORY) {
            err = ENOTDIR;
            errno = ENOTDIR;

#if defined(_WIN32)
            SetLastError(ERROR_DIRECTORY);
#endif
         }
      }
   }

   return err == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * File_EnsureDirectory --
 *
 *      If the directory doesn't exist, creates it. If the directory
 *      already exists, do nothing and succeed.
 *
 *      Errno/GetLastError is available upon failure.
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
File_EnsureDirectory(const char *pathName)  // IN:
{
   return File_EnsureDirectoryEx(pathName, 0777);
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
File_DeleteEmptyDirectory(const char *pathName)  // IN:
{
   Bool returnValue = TRUE;

   if (FileRemoveDirectory(pathName) != 0) {
#if defined(_WIN32)
      /*
       * Directory may have read-only bit set. Unset the
       * read-only bit and try deleting one more time.
       */
      if (File_SetFilePermissions(pathName, S_IWUSR)) {
         if (FileRemoveDirectory(pathName) != 0) {
            returnValue = FALSE;
         }
      } else {
         returnValue = FALSE;
      }
#else
      returnValue =  FALSE;
#endif
   }

   return returnValue;
}


/*
 *----------------------------------------------------------------------
 *
 * GetOldMachineID --
 *
 *      Return the old machineID, the one based on Hostinfo_MachineID.
 *
 * Results:
 *      The machineID is returned. It should not be freed.
 *
 * Side effects:
 *      Memory allocated for the machineID is never freed, however the
 *      memory is cached - there is no memory leak.
 *
 *----------------------------------------------------------------------
 */

static const char *
GetOldMachineID(void)
{
   static Atomic_Ptr atomic; /* Implicitly initialized to NULL. --mbellon */
   const char *machineID;

   machineID = Atomic_ReadPtr(&atomic);

   if (machineID == NULL) {
      char *p;
      uint32 hashValue;
      uint64 hardwareID;
      char encodedMachineID[16 + 1];
      char rawMachineID[sizeof hashValue + sizeof hardwareID];

      Hostinfo_MachineID(&hashValue, &hardwareID);

      /* Build the raw machineID */
      memcpy(rawMachineID, &hashValue, sizeof hashValue);
      memcpy(&rawMachineID[sizeof hashValue], &hardwareID,
             sizeof hardwareID);

      /* Base 64 encode the binary data to obtain printable characters */
      Base64_Encode(rawMachineID, sizeof rawMachineID, encodedMachineID,
                    sizeof encodedMachineID, NULL);

      /* remove '/' from the encoding; no problem using it for a file name */
      for (p = encodedMachineID; *p; p++) {
         if (*p == '/') {
            *p = '-';
         }
      }

      p = Util_SafeStrdup(encodedMachineID);

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         Posix_Free(p);
      }

      machineID = Atomic_ReadPtr(&atomic);
      ASSERT(machineID != NULL);
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
 *      The machineID is returned. It should not be freed.
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
   const char *machineID;

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
         p = Util_SafeStrdup(GetOldMachineID());
      } else {
         p = Str_SafeAsprintf(NULL, "uuid=%s", q);
         Posix_Free(q);

         /* Surpress any whitespace. */
         for (q = p; *q; q++) {
            if (isspace((int) *q)) {
               *q = '-';
            }
         }
      }

      if (Atomic_ReadIfEqualWritePtr(&atomic, NULL, p)) {
         Posix_Free(p);
      }

      machineID = Atomic_ReadPtr(&atomic);
      ASSERT(machineID != NULL);
   }

   return machineID;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OldMachineIDMatch --
 *
 *      Do the old-style MachineIDs match?
 *
 * Results:
 *      TRUE     Yes
 *      FALSE    No
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
OldMachineIDMatch(const char *first,   // IN:
                  const char *second)  // IN:
{
#if defined(__APPLE__) || defined(__linux__)
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
   Posix_Free(q);

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
   Posix_Free(q);

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
 *      Do the MachineIDs match?
 *
 * Results:
 *      TRUE     Yes
 *      FALSE    No
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileLockMachineIDMatch(const char *hostMachineID,   // IN:
                       const char *otherMachineID)  // IN:
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
File_IsEmptyDirectory(const char *pathName)  // IN:
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


/*
 *----------------------------------------------------------------------------
 *
 * File_IsOsfsVolumeEmpty --
 *
 *      Check if specified OSFS volume contains no files.
 *      This method ignore hidden .sf files. *.sf files are VMFS
 *      metadata files.
 *
 *      OSFS based volumes are considered empty even if they
 *      contain vmfs metadata files. This emptiness can not be
 *      checked by File_IsEmptyDirectory API (PR 1050328).
 *
 * Results:
 *      Bool - TRUE -> is vmfs empty directory, FALSE -> not an vmfs
 *      empty directory
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
File_IsOsfsVolumeEmpty(const char *pathName)  // IN:
{
   int i, numFiles;
   char **fileList = NULL;
   static const char vmfsSystemFilesuffix[] = ".sf";
   Bool onlyVmfsSystemFilesFound = TRUE;

   numFiles = File_ListDirectory(pathName, &fileList);
   if (numFiles == -1) {
      return FALSE;
   }

   for (i = 0; i < numFiles; i++) {
      if (!Unicode_EndsWith(fileList[i], vmfsSystemFilesuffix)) {
         onlyVmfsSystemFilesFound = FALSE;
         break;
      }
   }

   Util_FreeStringList(fileList, numFiles);

   return onlyVmfsSystemFilesFound;
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsFile --
 *
 *      Check if specified file is a regular file.
 *
 * Results:
 *      TRUE    is a regular file
 *      FALSE   is not a regular file or an error occured.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsFile(const char *pathName)  // IN:
{
   FileData fileData;

   return (FileAttributes(pathName, &fileData) == 0) &&
           (fileData.fileType == FILE_TYPE_REGULAR);
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
 *      TRUE   success
 *      FALSE  failure
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
   Err_Number err;
   FileIOResult fretR;

   do {
      unsigned char buf[8 * 1024];
      size_t actual;
      FileIOResult fretW;

      fretR = FileIO_Read(&src, buf, sizeof buf, &actual);
      if (!FileIO_IsSuccess(fretR) && (fretR != FILEIO_READ_ERROR_EOF)) {
         err = Err_Errno();

         Msg_Append(MSGID(File.CopyFromFdToFd.read.failure)
                               "Read error: %s.\n\n", FileIO_MsgError(fretR));

         Err_SetErrno(err);

         return FALSE;
      }

      fretW = FileIO_Write(&dst, buf, actual, NULL);
      if (!FileIO_IsSuccess(fretW)) {
         err = Err_Errno();

         Msg_Append(MSGID(File.CopyFromFdToFd.write.failure)
                              "Write error: %s.\n\n", FileIO_MsgError(fretW));

         Err_SetErrno(err);

         return FALSE;
      }
   } while (fretR != FILEIO_READ_ERROR_EOF);

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileCopyTree --
 *
 *      Recursively copies all files from a source path to a destination,
 *      optionally overwriting any files. This does the actual work
 *      for File_CopyTree.
 *
 * Results:
 *      TRUE   Success.
 *      FALSE  Failure. Error messages appended
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileCopyTree(const char *srcName,     // IN:
             const char *dstName,     // IN:
             Bool overwriteExisting,  // IN:
             Bool followSymlinks)     // IN:
{
   int err;
   Bool success = TRUE;
   int numFiles;
   int i;
   char **fileList = NULL;

   numFiles = File_ListDirectory(srcName, &fileList);

   if (numFiles == -1) {
      err = Err_Errno();
      Msg_Append(MSGID(File.CopyTree.walk.failure)
                 "Unable to access '%s' when copying files.\n\n",
                 srcName);
      Err_SetErrno(err);

      return FALSE;
   }

   File_EnsureDirectory(dstName);

   for (i = 0; i < numFiles && success; i++) {
      struct stat sb;
      char *srcFilename;

      srcFilename = File_PathJoin(srcName, fileList[i]);

      if (followSymlinks) {
         success = (Posix_Stat(srcFilename, &sb) == 0);
      } else {
         success = (Posix_Lstat(srcFilename, &sb) == 0);
      }

      if (success) {
         char *dstFilename = File_PathJoin(dstName, fileList[i]);

         switch (sb.st_mode & S_IFMT) {
         case S_IFDIR:
            success = FileCopyTree(srcFilename, dstFilename, overwriteExisting,
                                   followSymlinks);
            break;

#if !defined(_WIN32)
         case S_IFLNK:
            if (Posix_Symlink(Posix_ReadLink(srcFilename), dstFilename) != 0) {
               err = Err_Errno();
               Msg_Append(MSGID(File.CopyTree.symlink.failure)
                          "Unable to symlink '%s' to '%s': %s\n\n",
                          Posix_ReadLink(srcFilename),
                          dstFilename,
                          Err_Errno2String(err));
               Err_SetErrno(err);
               success = FALSE;
            }
            break;
#endif

         default:
            if (!File_Copy(srcFilename, dstFilename, overwriteExisting)) {
               err = Err_Errno();
               Msg_Append(MSGID(File.CopyTree.copy.failure)
                          "Unable to copy '%s' to '%s': %s\n\n",
                          srcFilename, dstFilename,
                          Err_Errno2String(err));
               Err_SetErrno(err);
               success = FALSE;
            }

            break;
         }

         Posix_Free(dstFilename);
      } else {
         err = Err_Errno();
         Msg_Append(MSGID(File.CopyTree.stat.failure)
                    "Unable to get information on '%s' when copying files.\n\n",
                    srcFilename);
         Err_SetErrno(err);
      }

      Posix_Free(srcFilename);
   }

   Util_FreeStringList(fileList, numFiles);

   return success;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_CopyTree --
 *
 *      Recursively copies all files from a source path to a destination,
 *      optionally overwriting any files.
 *
 * Results:
 *      TRUE   Success.
 *      FALSE  Failure. Error messages appended
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_CopyTree(const char *srcName,     // IN:
              const char *dstName,     // IN:
              Bool overwriteExisting,  // IN:
              Bool followSymlinks)     // IN:
{
   int err;

   ASSERT(srcName != NULL);
   ASSERT(dstName != NULL);

   if (!File_IsDirectory(srcName)) {
      err = Err_Errno();
      Msg_Append(MSGID(File.CopyTree.source.notDirectory)
                 "Source path '%s' is not a directory.",
                 srcName);
      Err_SetErrno(err);
      return FALSE;
   }

   if (!File_IsDirectory(dstName)) {
      err = Err_Errno();
      Msg_Append(MSGID(File.CopyTree.dest.notDirectory)
                 "Destination path '%s' is not a directory.",
                 dstName);
      Err_SetErrno(err);
      return FALSE;
   }

   return FileCopyTree(srcName, dstName, overwriteExisting, followSymlinks);
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
 *      TRUE   Success.
 *      FALSE  Failure. Error messages appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_CopyFromFd(FileIODescriptor src,    // IN:
                const char *dstName,     // IN:
                Bool overwriteExisting)  // IN:
{
   Bool success;
   Err_Number err;
   FileIOResult fret;
   FileIODescriptor dst;
   FileIOOpenAction action;

   ASSERT(dstName != NULL);

   FileIO_Invalidate(&dst);

   action = overwriteExisting ? FILEIO_OPEN_CREATE_EMPTY :
                                FILEIO_OPEN_CREATE_SAFE;

   fret = FileIO_Open(&dst, dstName, FILEIO_OPEN_ACCESS_WRITE, action);
   if (!FileIO_IsSuccess(fret)) {
      err = Err_Errno();

      Msg_Append(MSGID(File.CopyFromFdToName.create.failure)
                 "Unable to create a new '%s' file: %s.\n\n", dstName,
                 FileIO_MsgError(fret));

      Err_SetErrno(err);

      return FALSE;
   }

   success = File_CopyFromFdToFd(src, dst);

   err = Err_Errno();

   if (!FileIO_IsSuccess(FileIO_Close(&dst))) {
      if (success) {  // Report close failure when there isn't another error
         err =  Err_Errno();
      }

      Msg_Append(MSGID(File.CopyFromFdToName.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", dstName,
                 Msg_ErrString());

      success = FALSE;
   }

   if (!success) {
      /* The copy failed: ensure the destination file is removed */
      File_Unlink(dstName);
   }

   Err_SetErrno(err);

   return success;
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
 *      TRUE   Success.
 *      FALSE  Failure. Error messages appended
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_Copy(const char *srcName,     // IN:
          const char *dstName,     // IN:
          Bool overwriteExisting)  // IN:
{
   Bool success;
   Err_Number err;
   FileIOResult fret;
   FileIODescriptor src;

   ASSERT(srcName != NULL);
   ASSERT(dstName != NULL);

   FileIO_Invalidate(&src);

   fret = FileIO_Open(&src, srcName, FILEIO_OPEN_ACCESS_READ, FILEIO_OPEN);
   if (!FileIO_IsSuccess(fret)) {
      err = Err_Errno();

      Msg_Append(MSGID(File.Copy.open.failure)
                 "Unable to open the '%s' file for read access: %s.\n\n",
                 srcName, FileIO_MsgError(fret));

      Err_SetErrno(err);

      return FALSE;
   }

   success = File_CopyFromFd(src, dstName, overwriteExisting);

   err = Err_Errno();

   if (!FileIO_IsSuccess(FileIO_Close(&src))) {
      if (success) {  // Report close failure when there isn't another error
         err =  Err_Errno();
      }

      Msg_Append(MSGID(File.Copy.close.failure)
                 "Unable to close the '%s' file: %s.\n\n", srcName,
                 Msg_ErrString());

      success = FALSE;
   }

   Err_SetErrno(err);

   return success;
}


/*
 *----------------------------------------------------------------------
 *
 * File_Move --
 *
 *      Moves a file from one place to the other as efficiently as possible.
 *      This can be used to rename a file but, since file copying may be
 *      necessary, there is no assurance of atomicity. For efficiency
 *      purposes copying only results if the native rename ability fails.
 *
 * Results:
 *      TRUE   success
 *      FALSE  otherwise
 *
 * Side effects:
 *      src file is no more, but dst file exists
 *
 *----------------------------------------------------------------------
 */

Bool
File_Move(const char *oldFile,  // IN:
          const char *newFile,  // IN:
          Bool *asRename)       // OUT/OPT: result occurred due to rename/copy
{
   Bool ret;
   Bool duringRename;

   if (File_Rename(oldFile, newFile) == 0) {
      duringRename = TRUE;
      ret = TRUE;
      Err_SetErrno(0);
   } else {
      duringRename = FALSE;

      if (File_Copy(oldFile, newFile, TRUE)) {  // Allow overwrite
         File_Unlink(oldFile);  // Explicitly ignore errors
         ret = TRUE;
         Err_SetErrno(0);
      } else {
         ret = FALSE;
      }
   }

   if (asRename) {
      *asRename = duringRename;
   }

   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_MoveTree --
 *
 *    Moves a directory from one place to the other.
 *     - If dstName indicates a path that does not exist a directory will be
 *       created with that path filled with the contents from srcName.
 *     - If dstName is an existing directory then the contents will be moved
 *       into that directory.
 *     - If dstName indicates a file then File_MoveTree fails.
 *
 *    First we'll attempt to rename the directory, failing that we copy the
 *    contents from src->destination and unlink the src.  If the copy is
 *    succesful then we will report success even if the unlink fails for some
 *    reason.  In that event we will append error messages.
 *
 * Results:
 *    TRUE   Success.
 *    FALSE  Failure. Error messages appended
 *
 * Side effects:
 *    - Deletes the originating directory
 *    - In the event of a failed copy we'll leave the new directory in a state
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_MoveTree(const char *srcName,    // IN:
              const char *dstName,    // IN:
              Bool overwriteExisting, // IN:
              Bool *asMove)           // OUT/OPT:
{
   Bool ret = FALSE;
   Bool createdDir = FALSE;

   ASSERT(srcName != NULL);
   ASSERT(dstName != NULL);

   if (asMove) {
      *asMove = FALSE;
   }

   if (!File_IsDirectory(srcName)) {
      Msg_Append(MSGID(File.MoveTree.source.notDirectory)
                 "Source path '%s' is not a directory.",
                 srcName);

      return FALSE;
   }

   if (File_Rename(srcName, dstName) == 0) {
      if (asMove) {
         *asMove = TRUE;
      }

      ret = TRUE;
   } else {
      struct stat statbuf;

      if (Posix_Stat(dstName, &statbuf) == -1) {
         int err = Err_Errno();

         if (err == ENOENT) {
            if (!File_CreateDirectoryHierarchy(dstName, NULL)) {
               Msg_Append(MSGID(File.MoveTree.dst.couldntCreate)
                          "Could not create '%s'.\n\n", dstName);

               return FALSE;
            }

            createdDir = TRUE;
         } else {
            Msg_Append(MSGID(File.MoveTree.statFailed)
                       "%d:Failed to stat destination '%s'.\n\n",
                       err, dstName);

            return FALSE;
         }
      } else {
         if (!File_IsDirectory(dstName)) {
            Msg_Append(MSGID(File.MoveTree.dest.notDirectory)
                       "The destination path '%s' is not a directory.\n\n",
                       dstName);

            return FALSE;
         }
      }

#if !defined(__FreeBSD__) && !defined(sun)
      /*
       * File_GetFreeSpace is not defined for FreeBSD
       */
      if (createdDir) {
         /*
          * Check for free space on destination filesystem.
          * We only check for free space if the destination directory
          * did not exist. In this case, we will not be overwriting any existing
          * paths, so we need as much space as srcName.
          */
         int64 srcSize;
         int64 freeSpace;
         srcSize = File_GetSizeEx(srcName);
         freeSpace = File_GetFreeSpace(dstName, TRUE);
         if (freeSpace < srcSize) {
            char *spaceStr = Msg_FormatSizeInBytes(srcSize);
            Msg_Append(MSGID(File.MoveTree.dst.insufficientSpace)
                  "There is not enough space in the file system to "
                  "move the directory tree. Free %s and try again.",
                  spaceStr);
            Posix_Free(spaceStr);
            return FALSE;
         }
      }
#endif

      if (File_CopyTree(srcName, dstName, overwriteExisting, FALSE)) {
         ret = TRUE;

         if (!File_DeleteDirectoryTree(srcName)) {
            Msg_Append(MSGID(File.MoveTree.cleanupFailed)
                       "Forced to copy '%s' into '%s' but unable to remove "
                       "source directory.\n\n",
                       srcName, dstName);
         }
      } else {
         ret = FALSE;
         Msg_Append(MSGID(File.MoveTree.copyFailed)
                    "Could not rename and failed to copy source directory "
                    "'%s'.\n\n",
                    srcName);
         if (createdDir) {
            /*
             * Only clean up if we created the directory.  Not attempting to
             * clean up partial failures.
             */
            File_DeleteDirectoryTree(dstName);
         }
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
File_GetModTimeString(const char *pathName)  // IN:
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
 *      Get size of file. Try File_GetSizeEx to get size of directory/symlink.
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
File_GetSize(const char *pathName)  // IN:
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
File_SupportsLargeFiles(const char *pathName)  // IN:
{
   return File_SupportsFileSize(pathName, CONST64U(0x100000000));
}


/*
 *----------------------------------------------------------------------------
 *
 * File_GetSizeEx --
 *
 *      Get size of file or directory or symlink. File_GetSize can only get
 *      size of file.
 *
 * Results:
 *      Size of file/directory/symlink or -1.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int64
File_GetSizeEx(const char *pathName)  // IN:
{
   int i;
   int numFiles;
   int64 totalSize = 0;
   char **fileList = NULL;

   if (pathName == NULL) {
      return -1;
   }

   if (!File_IsDirectory(pathName)) {
      return File_GetSize(pathName);
   }

   numFiles = File_ListDirectory(pathName, &fileList);
   if (numFiles == -1) {
      return -1;
   }

   for (i = 0; i < numFiles; i++) {
      char *fileName = File_PathJoin(pathName, fileList[i]);
      int64 fileSize = File_GetSizeEx(fileName);

      Posix_Free(fileName);

      if (fileSize != -1) {
         totalSize += fileSize;
      }
   }

   Util_FreeStringList(fileList, numFiles);

   return totalSize;
}


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
File_GetSizeByPath(const char *pathName)  // IN:
{
   return (pathName == NULL) ? -1 : FileIO_GetSizeByPath(pathName);
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
 *-----------------------------------------------------------------------------
 *
 * File_CreateDirectoryHierarchyEx --
 *
 *      Create a directory including any parents that don't already exist.
 *      All the created directories are tagged with the specified permission.
 *      Returns the topmost directory which was created, to allow calling code
 *      to remove it after in case later operations fail.
 *
 * Results:
 *      TRUE   Success.
 *      FALSE  Failure.
 *
 *      If topmostCreated is not NULL, it returns the result of the hierarchy
 *      creation. If no directory was created, *topmostCreated is set to NULL.
 *      Otherwise *topmostCreated is set to the topmost directory which was
 *      created. *topmostCreated is set even in case of failure.
 *
 *      The caller most free the resulting string.
 *
 * Side effects:
 *      Only the obvious.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_CreateDirectoryHierarchyEx(const char *pathName,   // IN:
                                int mode,               // IN:
                                char **topmostCreated)  // OUT/OPT:
{
   char *volume;
   UnicodeIndex index;
   UnicodeIndex length;

   if (topmostCreated != NULL) {
      *topmostCreated = NULL;
   }

   if (pathName == NULL) {
      return TRUE;
   }

   length = Unicode_LengthInCodePoints(pathName);

   if (length == 0) {
      return TRUE;
   }

   /*
    * Skip past any volume/share.
    */

   File_SplitName(pathName, &volume, NULL, NULL);

   index = Unicode_LengthInCodePoints(volume);

   Posix_Free(volume);

   if (index >= length) {
      return File_IsDirectory(pathName);
   }

   /*
    * Iterate directory path, creating directories as necessary.
    */

   while (TRUE) {
      int err;
      char *temp;

      index = FileFirstSlashIndex(pathName, index + 1);

      temp = Unicode_Substr(pathName,
                            0,
                            (index == UNICODE_INDEX_NOT_FOUND) ? -1 : index);

      /*
       * If we check if the directory already exists and then we create it,
       * there is a race between these two operations. Any failure can be
       * confusing. We avoid this by attempting to create the directory before
       * checking the type.
       */
      err = FileCreateDirectory(temp, mode);

      if (err == 0) {
         if (topmostCreated != NULL && *topmostCreated == NULL) {
            *topmostCreated = temp;
            temp = NULL;
         }
      } else {
         if (err == EEXIST) {
            FileData fileData;

            err = FileAttributes(temp, &fileData);

            if (err == 0) {
               if (fileData.fileType != FILE_TYPE_DIRECTORY) {
                  err = ENOTDIR;
                  errno = err;

#if defined(_WIN32)
                  SetLastError(ERROR_DIRECTORY);
#endif
               }
            }
         }
      }

      if (err != 0) {
         Log(LGPFX" %s: Failure on '%s'. Error = %d\n",
             __FUNCTION__, temp, err);
      }

      Posix_Free(temp);

      if (err != 0) {
         return FALSE;
      }

      if (index == UNICODE_INDEX_NOT_FOUND) {
         break;
      }
   }

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_CreateDirectoryHierarchy --
 *
 *      Create a directory including any parents that don't already exist.
 *      All the created directories are tagged with 0777 permissions.
 *      Returns the topmost directory which was created, to allow calling code
 *      to remove it after in case later operations fail.
 *
 * Results:
 *      TRUE on success, FALSE on failure.
 *
 *      If topmostCreated is not NULL, it returns the result of the hierarchy
 *      creation. If no directory was created, *topmostCreated is set to NULL.
 *      Otherwise *topmostCreated is set to the topmost directory which was
 *      created. *topmostCreated is set even in case of failure.
 *
 *      The caller most free the resulting string.
 *
 * Side effects:
 *      Only the obvious.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_CreateDirectoryHierarchy(const char *pathName,   // IN:
                              char **topmostCreated)  // OUT/OPT:
{
   return File_CreateDirectoryHierarchyEx(pathName,
                                          0777,
                                          topmostCreated);
}

/*
 *----------------------------------------------------------------------
 *
 * FileDeleteDirectoryTree --
 *
 *      Deletes the specified directory tree. If filesystem errors are
 *      encountered along the way, the function will continue to delete what
 *      it can but will return FALSE. If contentOnly is TRUE it does not
 *      delete the directory itself.
 *
 * Results:
 *      TRUE   the entire tree was deleted or didn't exist
 *      FALSE  otherwise.
 *
 * Side effects:
 *      Deletes the directory tree from disk.
 *
 *----------------------------------------------------------------------
 */

static INLINE Bool
FileDeleteDirectoryTree(const char *pathName,  // IN: directory to delete
                        Bool contentOnly)      // IN: Content only or not
{
   int i;
   int numFiles;
   int err = 0;
   char *base;

   char **fileList = NULL;
   Err_Number fileError = 0;

   if (Posix_EuidAccess(pathName, F_OK) != 0) {
      /*
       * If Posix_EuidAccess failed with errno == ENOSYS, then fall back
       * to FileAttributes.
       */
      if (errno == ENOSYS) {
         /* FileAttributes returns the error code instead of setting errno. */
         err = FileAttributes(pathName, NULL);
      } else {
         /* Use the error value that was set by Posix_EuidAccess. */
         err = errno;
      }
   }

   switch (err) {
      case ENOENT:
      case ENOTDIR:
         /* path does not exist or is inaccessible */
         return TRUE;
      default:
         break;
   }

   /* get list of files in current directory */
   numFiles = File_ListDirectory(pathName, &fileList);

   if (numFiles == -1) {
      return FALSE;
   }

   /* delete everything in the directory */
   base = Unicode_Append(pathName, DIRSEPS);

   for (i = 0; i < numFiles; i++) {
      char *curPath;
      struct stat statbuf;

      curPath = Unicode_Append(base, fileList[i]);

      if (Posix_Lstat(curPath, &statbuf) == 0) {
         switch (statbuf.st_mode & S_IFMT) {
         case S_IFDIR:
            /* Directory, recurse */
            if (!FileDeleteDirectoryTree(curPath, FALSE)) {
               fileError = Err_Errno();
            }
            break;

#if !defined(_WIN32)
         case S_IFLNK:
            /* Delete symlink, not what it points to */
            if (FileDeletion(curPath, FALSE) != 0) {
               fileError = Err_Errno();
            }
            break;
#endif

         default:
            if (FileDeletion(curPath, FALSE) != 0) {
#if defined(_WIN32)
               if (File_SetFilePermissions(curPath, S_IWUSR)) {
                  if (FileDeletion(curPath, FALSE) != 0) {
                     fileError = Err_Errno();
                  }
               } else {
                  fileError = Err_Errno();
               }
#else
               fileError = Err_Errno();
#endif
            }
            break;
         }
      } else {
         fileError = Err_Errno();
         Log(LGPFX" %s: Lstat of '%s' failed, errno = %d\n",
             __FUNCTION__, curPath, errno);
      }

      Posix_Free(curPath);
   }

   Posix_Free(base);

   if (!contentOnly) {
      /*
       * Call File_DeleteEmptyDirectory() only if there is no prior error
       * while deleting the children.
       */
      if (fileError == 0 && !File_DeleteEmptyDirectory(pathName)) {
         fileError = Err_Errno();
      }
   }

   Util_FreeStringList(fileList, numFiles);

   Err_SetErrno(fileError);

   return fileError == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * File_DeleteDirectoryContent --
 *
 *      Deletes the specified directory content. If filesystem errors are
 *      encountered along the way, the function will continue to delete what
 *      it can but will return FALSE.
 *
 * Results:
 *      TRUE   the entire contents were deleted or there were no files and the
 *             directory was empty
 *      FALSE  otherwise
 *
 * Side effects:
 *      Deletes the directory content from disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_DeleteDirectoryContent(const char *pathName)  // IN: directory to delete
{
   return FileDeleteDirectoryTree(pathName, TRUE);
}


/*
 *----------------------------------------------------------------------
 *
 * File_DeleteDirectoryTree --
 *
 *      Deletes the specified directory tree. If filesystem errors are
 *      encountered along the way, the function will continue to delete what
 *      it can but will return FALSE.
 *
 * Results:
 *      TRUE   the entire tree was deleted or didn't exist
 *      FALSE  otherwise.
 *
 * Side effects:
 *      Deletes the directory tree from disk.
 *
 *----------------------------------------------------------------------
 */

Bool
File_DeleteDirectoryTree(const char *pathName)  // IN: directory to delete
{
   return FileDeleteDirectoryTree(pathName, FALSE);
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
 *      TRUE   file was found
 *      FALSE  otherwise.
 *
 * Side effects:
 *      If result is non Null allocate a string for the filename found.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_FindFileInSearchPath(const char *fileIn,      // IN:
                          const char *searchPath,  // IN:
                          const char *cwd,         // IN:
                          char **result)           // OUT/OPT:
{
   char *cur;
   char *tok;
   Bool found;
   Bool full;
   char *saveptr = NULL;
   char *sp = NULL;
   char *dir = NULL;
   char *file = NULL;

   ASSERT(fileIn != NULL);
   ASSERT(searchPath != NULL);
   ASSERT(cwd != NULL);

   /*
    * First check the usual places - the fullpath or the cwd.
    */

   full = File_IsFullPath(fileIn);
   if (full) {
      cur = Util_SafeStrdup(fileIn);
   } else {
      cur = Str_SafeAsprintf(NULL, "%s%s%s", cwd, DIRSEPS, fileIn);
   }

   if (Posix_EuidAccess(cur, F_OK) == 0) {
      goto done;
   }
   if (errno == ENOSYS && FileAttributes(cur, NULL) == 0) {
      goto done;
   }

   Posix_Free(cur);
   cur = NULL;

   if (full) {
      goto done;
   }

   File_GetPathName(fileIn, &dir, &file);

   /*
    * Search path applies only if filename is simple basename.
    */
   if (Unicode_LengthInCodePoints(dir) != 0) {
      goto done;
   }

   /*
    * Didn't find it in the usual places so strip it to its bare minimum and
    * start searching.
    */

   sp = Util_SafeStrdup(searchPath);
   tok = strtok_r(sp, FILE_SEARCHPATHTOKEN, &saveptr);

   while (tok) {
      if (File_IsFullPath(tok)) {
         /* Fully Qualified Path. Use it. */
         cur = Str_SafeAsprintf(NULL, "%s%s%s", tok, DIRSEPS, file);
      } else {
         /* Relative Path.  Prepend the cwd. */
         if (Str_Strcasecmp(tok, ".") == 0) {
            /* Don't append "." */
            cur = Str_SafeAsprintf(NULL, "%s%s%s", cwd, DIRSEPS, file);
         } else {
            cur = Str_SafeAsprintf(NULL, "%s%s%s%s%s", cwd, DIRSEPS, tok,
                                   DIRSEPS, file);
         }
      }

      if (Posix_EuidAccess(cur, F_OK) == 0) {
         break;
      }

      if ((errno == ENOSYS) && (FileAttributes(cur, NULL) == 0)) {
         break;
      }

      Posix_Free(cur);
      cur = NULL;

      tok = strtok_r(NULL, FILE_SEARCHPATHTOKEN, &saveptr);
   }

done:
   if (cur) {
      found = TRUE;

      if (result) {
         *result = File_FullPath(cur);

         if (*result == NULL) {
            found = FALSE;
         }
      }

      Posix_Free(cur);
   } else {
      found = FALSE;
   }

   Posix_Free(sp);
   Posix_Free(dir);
   Posix_Free(file);

   return found;
}


/*
 *----------------------------------------------------------------------
 *
 * File_ExpandAndCheckDir --
 *
 *      Expand any environment variables in the given path and check that
 *      the named directory is writeable.
 *
 * Results:
 *      NULL error
 *     !NULL the expanded path otherwise.
 *
 * Side effects:
 *      The result is allocated.
 *
 *----------------------------------------------------------------------
 */

char *
File_ExpandAndCheckDir(const char *dirName)  // IN:
{
   if (dirName != NULL) {
      char *edirName = Util_ExpandString(dirName);

      if ((edirName != NULL) && FileIsWritableDir(edirName)) {
         size_t len = strlen(edirName) - 1;

         if (edirName[len] == DIRSEPC) {
            edirName[len] = '\0';
         }

         return edirName;
      }
   }

   return NULL;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileSimpleRandom --
 *
 *      Return a random number in the range of 0 and 2^32-1.
 *
 * Results:
 *      Random number is returned.
 *
 * Side Effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

uint32
FileSimpleRandom(void)
{
   static Atomic_Ptr lckStorage;
   static rqContext *context = NULL;
   uint32 result;
   MXUserExclLock *lck = MXUser_CreateSingletonExclLock(&lckStorage,
                                                        "fileSimpleRandomLock",
                                                        RANK_LEAF);

   MXUser_AcquireExclLock(lck);

   if (UNLIKELY(context == NULL)) {
      uint32 value;

#if defined(_WIN32)
      value = GetCurrentProcessId();
#else
      value = getpid();
#endif

      context = Random_QuickSeed(value);
      ASSERT(context != NULL);
   }

   result = Random_Quick(context);

   MXUser_ReleaseExclLock(lck);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * FileSleeper
 *
 *      Sleep for a random amount of time, no less than the specified minimum
 *      and no more than the specified maximum sleep time values. This often
 *      proves useful to "jitter" retries such that multiple threads don't
 *      easily get into resonance performing necessary actions.
 *
 * Results:
 *      Somnambulistic behavior; the amount of time slept is returned.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint32
FileSleeper(uint32 minSleepTimeMsec,  // IN:
            uint32 maxSleepTimeMsec)  // IN:
{
   uint32 variance;
   uint32 actualSleepTimeMsec;
#if defined(_WIN32)
   uint32 totalSleepTimeMsec;
#endif

   ASSERT(minSleepTimeMsec <= maxSleepTimeMsec);

   variance = maxSleepTimeMsec - minSleepTimeMsec;

   if (variance == 0) {
      actualSleepTimeMsec = minSleepTimeMsec;
   } else {
      float fpRand = ((float) FileSimpleRandom()) / ((float) ~((uint32) 0));

      actualSleepTimeMsec = minSleepTimeMsec + (uint32) (fpRand * variance);
   }

#if defined(_WIN32)
   /* Clamp individual sleeps to avoid Windows issues */
   totalSleepTimeMsec = actualSleepTimeMsec;

   while (totalSleepTimeMsec > 0) {
      uint32 sleepTimeMsec = (totalSleepTimeMsec > 900) ? 900 :
                                                          totalSleepTimeMsec;

      Util_Usleep(1000 * sleepTimeMsec);

      totalSleepTimeMsec -= sleepTimeMsec;
   }
#else
   Util_Usleep(1000 * actualSleepTimeMsec);
#endif

   return actualSleepTimeMsec;
}


/*
 *----------------------------------------------------------------------
 *
 * FileRotateByRename --
 *
 *      The oldest indexed file should be removed so that the consequent
 *      rename succeeds.
 *
 *      The last dst is 'fileName' and should not be deleted.
 *
 * Results:
 *      If newFileName is non-NULL: the new path is returned to *newFileName
 *      if the rotation succeeded, otherwise NULL is returned in *newFileName.
 *      The caller is responsible for freeing the string returned in
 *      *newFileName.
 *
 * Side effects:
 *      Rename backup old files kept so far.
 *
 *----------------------------------------------------------------------
 */

static void
FileRotateByRename(const char *fileName,  // IN: full path to file
                   const char *baseName,  // IN: filename w/o extension.
                   const char *ext,       // IN: extension
                   int n,                 // IN: number of old files to keep
                   char **newFileName)    // OUT/OPT: new path to file
{
   char *src = NULL;
   char *dst = NULL;
   int i;
   int result;

   if (newFileName != NULL) {
      *newFileName = NULL;
   }

   for (i = n; i >= 0; i--) {
      src = (i == 0) ? (char *) fileName :
                       Str_SafeAsprintf(NULL, "%s-%d%s", baseName, i - 1, ext);

      if (dst == NULL) {
         result = File_UnlinkIfExists(src);

         if (result == -1) {
            Log(LGPFX" %s: failed to remove %s: %s\n", __FUNCTION__,
                src, Msg_ErrString());
         }
      } else {
         result = Posix_Rename(src, dst);

         if (result == -1) {
            int error = Err_Errno();

            if (error != ENOENT) {
               Log(LGPFX" %s: failed to rename %s -> %s: %s\n", src, dst,
                   __FUNCTION__, Err_Errno2String(error));
            }
         }
      }

      if ((src == fileName) && (newFileName != NULL) && (result == 0)) {
         *newFileName = Util_SafeStrdup(dst);
      }

      ASSERT(dst != fileName);
      Posix_Free(dst);
      dst = src;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileNumberCompare --
 *
 *      Helper function for comparing the contents of two
 *      uint32 pointers a and b, suitable for use by qsort
 *      to order an array of file numbers.
 *
 * Results:
 *      The contents of 'a' minus the contents of 'b'.
 *
 * Side effects:
 *      None.
 */

static int
FileNumberCompare(const void *a,  // IN:
                  const void *b)  // IN:
{
   return *(uint32 *) a - *(uint32 *) b;
}


/*
 *----------------------------------------------------------------------
 *
 * FileRotateByRenumber --
 *
 *      File rotation scheme optimized for vmfs:
 *        1) find highest numbered file (maxNr)
 *        2) rename <base>.<ext> to <base>-<maxNr + 1>.<ext>
 *        3) delete (nFound - numToKeep) lowest numbered files.
 *
 *        Wrap around is handled incorrectly.
 *
 * Results:
 *      If newFilePath is non-NULL: the new path is returned to *newFilePath
 *      if the rotation succeeded, otherwise NULL is returned in *newFilePath.
 *      The caller is responsible for freeing the string returned in
 *      *newFilePath.
 *
 * Side effects:
 *      Files renamed / deleted.
 *
 *----------------------------------------------------------------------
 */

static void
FileRotateByRenumber(const char *filePath,       // IN: full path to file
                     const char *filePathNoExt,  // IN: filename w/o extension.
                     const char *ext,            // IN: extension
                     int n,                      // IN: number old files to keep
                     char **newFilePath)         // OUT/OPT: new path to file
{
   char *baseDir = NULL, *fmtString = NULL, *baseName = NULL, *tmp;
   char *fullPathNoExt = NULL;
   uint32 maxNr = 0;
   int i, nrFiles, nFound = 0;
   char **fileList = NULL;
   uint32 *fileNumbers = NULL;
   int result;

   if (newFilePath != NULL) {
      *newFilePath = NULL;
   }

   fullPathNoExt = File_FullPath(filePathNoExt);
   if (fullPathNoExt == NULL) {
      Log(LGPFX" %s: failed to get full path for '%s'.\n", __FUNCTION__,
          filePathNoExt);
      goto cleanup;
   }

   File_GetPathName(fullPathNoExt, &baseDir, &baseName);

   if ((baseDir == NULL) || (*baseDir == '\0')) {
      free(baseDir);
      baseDir = Unicode_Duplicate(DIRSEPS);
   }

   if ((baseName == NULL) || (*baseName == '\0')) {
      Log(LGPFX" %s: failed to get base name for path '%s'.\n", __FUNCTION__,
          filePathNoExt);
      goto cleanup;
   }

   fmtString = Str_SafeAsprintf(NULL, "%s-%%d%s%%n", baseName, ext);

   nrFiles = File_ListDirectory(baseDir, &fileList);
   if (nrFiles == -1) {
      Log(LGPFX" %s: failed to read the directory '%s'.\n", __FUNCTION__,
          baseDir);
      goto cleanup;
   }

   fileNumbers = Util_SafeCalloc(nrFiles, sizeof(uint32));

   for (i = 0; i < nrFiles; i++) {
      uint32 curNr;
      int bytesProcessed = 0;

      /*
       * Make sure the whole file name matched what we expect for the file.
       */

      if ((sscanf(fileList[i], fmtString, &curNr, &bytesProcessed) >= 1) &&
          (bytesProcessed == strlen(fileList[i]))) {
         fileNumbers[nFound++] = curNr;
      }

      Posix_Free(fileList[i]);
   }

   if (nFound > 0) {
      qsort(fileNumbers, nFound, sizeof(uint32), FileNumberCompare);
      maxNr = fileNumbers[nFound - 1];
   }

   /* rename the existing file to the next number */
   tmp = Str_SafeAsprintf(NULL, "%s/%s-%d%s", baseDir, baseName,
                          maxNr + 1, ext);

   result = Posix_Rename(filePath, tmp);

   if (result == -1) {
      int error = Err_Errno();

      if (error != ENOENT) {
         Log(LGPFX" %s: failed to rename %s -> %s failed: %s\n", __FUNCTION__,
             filePath, tmp, Err_Errno2String(error));
      }
   }

   if (newFilePath == NULL || result == -1) {
      Posix_Free(tmp);
   } else {
      *newFilePath = tmp;
   }

   if (nFound >= n) {
      /* Delete the extra files. */
      for (i = 0; i <= nFound - n; i++) {
         tmp = Str_SafeAsprintf(NULL, "%s/%s-%d%s", baseDir, baseName,
                                fileNumbers[i], ext);

         if (Posix_Unlink(tmp) == -1) {
            Log(LGPFX" %s: failed to remove %s: %s\n", __FUNCTION__, tmp,
                Msg_ErrString());
         }
         Posix_Free(tmp);
      }
   }

  cleanup:
   Posix_Free(fileNumbers);
   Posix_Free(fileList);
   Posix_Free(fmtString);
   Posix_Free(baseDir);
   Posix_Free(baseName);
   Posix_Free(fullPathNoExt);
}


/*
 *----------------------------------------------------------------------
 *
 * File_Rotate --
 *
 *      Rotate old files. The 'noRename' option is useful for filesystems
 *      where rename is hideously expensive (*cough* vmfs).
 *
 * Results:
 *      If newFileName is non-NULL: the new path is returned to
 *      *newFileName if the rotation succeeded, otherwise NULL
 *      is returned in *newFileName.  The caller is responsible
 *      for freeing the string returned in *newFileName.
 *
 * Side effects:
 *      Files are renamed / deleted.
 *
 *----------------------------------------------------------------------
 */

void
File_Rotate(const char *fileName,  // IN: original file
            int n,                 // IN: number of backup files
            Bool noRename,         // IN: don't rename all files
            char **newFileName)    // OUT/OPT: new path to file
{
   const char *ext;
   size_t baseLen;
   char *baseName;

   ASSERT(fileName != NULL);

   if ((ext = Str_Strrchr(fileName, '.')) == NULL) {
      ext = fileName + strlen(fileName);
   }
   baseLen = ext - fileName;

   /*
    * Backup base of file name.
    *
    * Since the Str_Asprintf(...) doesn't like format of %.*s and crashes
    * in Windows 2000. (Daniel Liu)
    */

   baseName = Util_SafeStrdup(fileName);
   baseName[baseLen] = '\0';

   if (noRename) {
      FileRotateByRenumber(fileName, baseName, ext, n, newFileName);
   } else {
      FileRotateByRename(fileName, baseName, ext, n, newFileName);
   }

   Posix_Free(baseName);
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetFSMountInfo --
 *
 *      Platform-independent wrapper around File_GetVMFSMountInfo
 *
 * Results:
 *      On failure return -1.  Otherwise, return fsType, version,
 *      remoteIP, remoteMountPoint, and localMountPoint.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
File_GetFSMountInfo(const char *pathName,
                    char **fsType,
                    uint32 *version,
                    char **remoteIP,
                    char **remoteMountPoint,
                    char **localMountPoint)
{
#if defined VMX86_SERVER
   return  File_GetVMFSMountInfo(pathName, fsType, version,
                                 remoteIP, remoteMountPoint,
                                 localMountPoint);
#else
   return -1;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * File_ContainSymLink --
 *
 *      Check if the specified file path contains symbolic link.
 *
 * Results:
 *      TRUE   pathName contains a symlink,
 *      FALSE  pathName is not a symlink nor contains a symlink, or error.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_ContainSymLink(const char *pathName)  // IN:
{
   char *path = NULL;
   char *base = NULL;
   Bool retValue = FALSE;

   if (File_IsSymLink(pathName)) {
      return TRUE;
   }

   File_GetPathName(pathName, &path, &base);

   if ((path != NULL) &&
       (base != NULL) &&
       (strcmp(path, "") != 0) &&
       (strcmp(base, "") != 0)) {
      if (File_ContainSymLink(path)) {
         retValue = TRUE;
      }
   }

   Posix_Free(path);
   Posix_Free(base);

   return retValue;
}
