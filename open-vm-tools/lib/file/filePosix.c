/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * filePosix.c --
 *
 *      Interface to Posix-specific file functions.
 */

#include <sys/types.h> /* Needed before sys/vfs.h with glibc 2.0 --hpreg */

#if defined(__FreeBSD__)
# include <sys/param.h>
#else
# if !defined(__APPLE__)
#  include <sys/vfs.h>
# endif
# include <limits.h>
# include <stdio.h>      /* Needed before sys/mnttab.h in Solaris */
# ifdef sun
#  include <sys/mnttab.h>
# elif __APPLE__
#  include <sys/mount.h>
# else
#  include <mntent.h>
# endif
#include <signal.h>
#endif
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#ifdef __linux__
#   include <pwd.h>
#endif

#include "vmware.h"
#include "posix.h"
#include "file.h"
#include "fileInt.h"
#include "msg.h"
#include "util.h"
#include "str.h"
#include "util.h"
#include "timeutil.h"
#include "dynbuf.h"
#include "localconfig.h"

#include "unicodeOperations.h"

#if !defined(__FreeBSD__) && !defined(sun)
#if !defined(__APPLE__)
static char *FilePosixLookupMountPoint(char const *canPath, Bool *bind);
#endif
static char *FilePosixNearestExistingAncestor(char const *path);

# ifdef VMX86_SERVER
#define VMFS2CONST 456
#define VMFS3CONST 256
#include "hostType.h"
/* Needed for VMFS implementation of File_GetFreeSpace() */
#  include <sys/ioctl.h>
# endif
#endif

#ifdef VMX86_SERVER
#include "fs_user.h"
#endif


/*
 * Local functions
 */

static Bool FileIsGroupsMember(gid_t gid);


/*
 *-----------------------------------------------------------------------------
 *
 * FileRemoveDirectory --
 *
 *	Delete a directory.
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *-----------------------------------------------------------------------------
 */

int
FileRemoveDirectory(ConstUnicode pathName)  // IN:
{
   return (Posix_Rmdir(pathName) == -1) ? errno : 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileRename --
 *
 *	Rename a file.
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *-----------------------------------------------------------------------------
 */

int
FileRename(ConstUnicode oldName,  // IN:
           ConstUnicode newName)  // IN:
{
   return (Posix_Rename(oldName, newName) == -1) ? errno : 0;
}


/*
 *----------------------------------------------------------------------
 *
 *  FileDeletion --
 *	Delete the specified file
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *----------------------------------------------------------------------
 */

int
FileDeletion(ConstUnicode pathName,   // IN:
             const Bool handleLink)   // IN:
{
   int err;
   char *linkPath = NULL;
   char *primaryPath = Unicode_GetAllocBytes(pathName,
                                             STRING_ENCODING_DEFAULT);

   if (primaryPath == NULL && pathName != NULL) {
      Log(LGPFX" %s: failed to convert \"%s\" to current encoding\n",
          __FUNCTION__, UTF8(pathName));
      return UNICODE_CONVERSION_ERRNO;
   }

   if (handleLink) {
      struct stat statbuf;

      if (lstat(primaryPath, &statbuf) == -1) {
         err = errno;
         goto bail;
      }

      if (S_ISLNK(statbuf.st_mode)) {
         linkPath = Util_SafeMalloc(statbuf.st_size + 1);

         if (readlink(primaryPath, linkPath,
                      statbuf.st_size) != statbuf.st_size) {
            err = errno;
            goto bail;
         }

         linkPath[statbuf.st_size] = '\0';

         if (unlink(linkPath) == -1) {
            if (errno != ENOENT) {
               err = errno;
               goto bail;
            }
         }
      }
   }

   err = (unlink(primaryPath) == -1) ? errno : 0;

bail:
   free(primaryPath);
   free(linkPath);
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_UnlinkDelayed --
 *
 *    Same as File_Unlink for POSIX systems since we can unlink anytime.
 *
 * Results:
 *    Return 0 if the unlink is successful.   Otherwise, returns -1.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

int
File_UnlinkDelayed(ConstUnicode pathName)  // IN:
{
   return (FileDeletion(pathName, TRUE) == 0) ? 0 : -1;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileAttributes --
 *
 *	Return the attributes of a file. Time units are in OS native time.
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
FileAttributes(ConstUnicode pathName,  // IN:
               FileData *fileData)     // OUT:
{
   int err;
   struct stat statbuf;

   if (Posix_Stat(pathName, &statbuf) == -1) {
      err = errno;
   } else {
      if (fileData != NULL) {
         fileData->fileCreationTime = statbuf.st_ctime;
         fileData->fileModificationTime = statbuf.st_mtime;
         fileData->fileAccessTime = statbuf.st_atime;
         fileData->fileSize = statbuf.st_size;

         switch (statbuf.st_mode & S_IFMT) {
         case S_IFREG:
            fileData->fileType = FILE_TYPE_REGULAR;
            break;

         case S_IFDIR:
            fileData->fileType = FILE_TYPE_DIRECTORY;
            break;

         case S_IFBLK:
            fileData->fileType = FILE_TYPE_BLOCKDEVICE;
            break;

         case S_IFCHR:
            fileData->fileType = FILE_TYPE_CHARDEVICE;
            break;

         case S_IFLNK:
            fileData->fileType = FILE_TYPE_SYMLINK;
            break;

         default:
            fileData->fileType = FILE_TYPE_UNCERTAIN;
            break;
         }

         fileData->fileMode = statbuf.st_mode;
         fileData->fileOwner = statbuf.st_uid;
         fileData->fileGroup = statbuf.st_gid;
      }

      err = 0;
   }

   return err;
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsRemote --
 *
 *      Determine whether a file is on a remote filesystem.
 *      In case of an error be conservative and assume that 
 *      the file is a remote file.
 *
 * Results:
 *      The answer.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

#if !defined(__FreeBSD__) && !defined(sun)
Bool
File_IsRemote(ConstUnicode pathName)  // IN: Path name
{
   struct statfs sfbuf;

#if defined(VMX86_SERVER)
   /*
    * On ESX, statfs() will always return VMFS_MAGIC for files on VMFS so this
    * function is only correct for files on COS, otherwise it always returns
    * FALSE.
    * On VMvisor, statfs() could return VMFS_NFS_MAGIC but it is very slow.
    * Since there is no COS for VMvisor, just be on par with ESX and return
    * FALSE directly.
    * XXX See PR 158284. It is not clear what the side-effects are of this
    * function being incorrect for VMFS files.
    */

   if (HostType_OSIsPureVMK()) {
      return FALSE;
   }
#endif

   if (Posix_Statfs(pathName, &sfbuf) == -1) {
      Log(LGPFX" %s: statfs(%s) failed: %s\n", __func__, UTF8(pathName),
          strerror(errno));
      return TRUE;
   }
#if defined(__APPLE__)
   return sfbuf.f_flags & MNT_LOCAL ? FALSE : TRUE;
#else
   if (NFS_SUPER_MAGIC == sfbuf.f_type) {
      return TRUE;
   }
   if (SMB_SUPER_MAGIC == sfbuf.f_type) {
      return TRUE;
   }
   return FALSE;
#endif
}
#endif /* !FreeBSD && !sun */


/*
 *----------------------------------------------------------------------
 *
 * File_IsSymLink --
 *
 *      Check if the specified file is a symbolic link or not
 *
 * Results:
 *      Bool - TRUE -> is a symlink, FALSE -> not a symlink or error
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsSymLink(ConstUnicode pathName)  // IN:
{
   struct stat statbuf;

   return (Posix_Lstat(pathName, &statbuf) == 0) &&
           S_ISLNK(statbuf.st_mode);
}


/*
 *----------------------------------------------------------------------
 *
 * File_Cwd --
 *
 *      Find the current directory on drive DRIVE.
 *      DRIVE is either NULL (current drive) or a string
 *      starting with [A-Za-z].
 *
 * Results:
 *      NULL if error.
 *
 * Side effects:
 *      The result is allocated
 *
 *----------------------------------------------------------------------
 */

Unicode
File_Cwd(ConstUnicode drive)  // IN:
{
   char buffer[FILE_MAXPATH];

   if ((drive != NULL) && !Unicode_IsEmpty(drive)) {
      Warning(LGPFX" %s: Drive letter %s on Linux?\n", __FUNCTION__,
              UTF8(drive));
   }

   if (getcwd(buffer, FILE_MAXPATH) == NULL) {
      Msg_Append(MSGID(filePosix.getcwd)
                 "Unable to retrieve the current working directory: %s. "
                 "Please check if the directory has been deleted or "
                 "unmounted.\n",
                 Msg_ErrString());
      Warning(LGPFX" %s: getcwd() failed: %s\n", __FUNCTION__,
              Msg_ErrString());

      return NULL;
   };

   return Unicode_Alloc(buffer, STRING_ENCODING_DEFAULT);
}


/*
 *----------------------------------------------------------------------
 *
 * FileStripFwdSlashes --
 *
 *      Returns a new string with the extraneous forward slashes ("/") removed.
 *
 * Results:
 *      As documented.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static Unicode
FileStripFwdSlashes(ConstUnicode pathName)  // IN:
{
   char *ptr;
   char *path;
   char *cptr;
   char *prev;
   Unicode result;

   ASSERT(pathName);

   path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_UTF8);
   ASSERT(path != NULL);

   ptr = path;
   cptr = path;
   prev = NULL;

   /*
    * Copy over if not DIRSEPC. If yes, copy over only if previous
    * character was not DIRSEPC.
    */

   while (*ptr != '\0') {
      if (*ptr == DIRSEPC) {
         if (prev != ptr - 1) {
            *cptr++ = *ptr;
         }
         prev = ptr;
      } else {
         *cptr++ = *ptr;
      }
      ptr++;
   }

   *cptr = '\0';

   result = Unicode_AllocWithUTF8(path);

   free(path);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_FullPath --
 *
 *      Compute the full path of a file. If the file if NULL or "", the
 *      current directory is returned
 *
 * Results:
 *      NULL if error (reported to the user)
 *
 * Side effects:
 *      The result is allocated
 *
 *----------------------------------------------------------------------
 */

Unicode
File_FullPath(ConstUnicode pathName)  // IN:
{
   Unicode cwd;
   Unicode ret;
   Unicode temp;

   if ((pathName != NULL) && File_IsFullPath(pathName)) {
      cwd = NULL;
   } else {
      cwd = File_Cwd(NULL);
      if (cwd == NULL) {
         return NULL;
      }
   }

   if ((pathName == NULL) || Unicode_IsEmpty(pathName)) {
      temp = Unicode_Duplicate(cwd);
   } else if (File_IsFullPath(pathName)) {
      temp = Unicode_Duplicate(pathName);
   } else {
      Unicode path;

      path = Unicode_Join(cwd, DIRSEPS, pathName, NULL);

      temp = Posix_RealPath(path);

      if (temp == NULL) {
         temp = path;
      } else {
         Unicode_Free(path);
      }
   }

   ret = FileStripFwdSlashes(temp);

   Unicode_Free(temp);
   Unicode_Free(cwd);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsFullPath --
 *
 *      Is this a full path?
 *
 * Results:
 *      TRUE if full path.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsFullPath(ConstUnicode pathName)  // IN:
{
   /* start with a slash? */
   return (pathName == NULL) ? FALSE :
                               Unicode_StartsWith(pathName, DIRSEPS);
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetTimes --
 *
 *      Get the date and time that a file was created, last accessed,
 *      last modified and last attribute changed.
 *
 * Results:
 *      TRUE if succeed or FALSE if error.
 *
 * Side effects:
 *      If a particular time is not available, -1 will be returned for
 *      that time.
 *
 *----------------------------------------------------------------------
 */

Bool
File_GetTimes(ConstUnicode pathName,      // IN:
              VmTimeType *createTime,     // OUT: Windows NT time format
              VmTimeType *accessTime,     // OUT: Windows NT time format
              VmTimeType *writeTime,      // OUT: Windows NT time format
              VmTimeType *attrChangeTime) // OUT: Windows NT time format
{
   struct stat statBuf;

   ASSERT(createTime && accessTime && writeTime && attrChangeTime);

   *createTime     = -1;
   *accessTime     = -1;
   *writeTime      = -1;
   *attrChangeTime = -1;

   if (Posix_Lstat(pathName, &statBuf) == -1) {
      Log(LGPFX" %s: error stating file \"%s\": %s\n", __FUNCTION__,
          UTF8(pathName), strerror(errno));
      return FALSE;
   }

   /*
    * XXX We should probably use the MIN of all Unix times for the creation
    *     time, so that at least times are never inconsistent in the
    *     cross-platform format. Maybe atime is always that MIN. We should
    *     check and change the code if it is not.
    *
    * XXX atime is almost always MAX.
    */

#if defined(__FreeBSD__)
   /*
    * FreeBSD: All supported versions have timestamps with nanosecond
    * resolution. FreeBSD 5+ has also file creation time.
    */
#   if defined(__FreeBSD_version) && __FreeBSD_version >= 500043
   *createTime     = TimeUtil_UnixTimeToNtTime(statBuf.st_birthtimespec);
#   endif
   *accessTime     = TimeUtil_UnixTimeToNtTime(statBuf.st_atimespec);
   *writeTime      = TimeUtil_UnixTimeToNtTime(statBuf.st_mtimespec);
   *attrChangeTime = TimeUtil_UnixTimeToNtTime(statBuf.st_ctimespec);
#elif defined(linux)
   /*
    * Linux: Glibc 2.3+ has st_Xtim.  Glibc 2.1/2.2 has st_Xtime/__unusedX on
    *        same place (see below).  We do not support Glibc 2.0 or older.
    */

#   if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3)
   {
      /*
       * stat structure is same between glibc 2.3 and older glibcs, just
       * these __unused fields are always zero. If we'll use __unused*
       * instead of zeroes, we get automatically nanosecond timestamps
       * when running on host which provides them.
       */

      struct timespec timeBuf;

      timeBuf.tv_sec  = statBuf.st_atime;
      timeBuf.tv_nsec = statBuf.__unused1;
      *accessTime     = TimeUtil_UnixTimeToNtTime(timeBuf);


      timeBuf.tv_sec  = statBuf.st_mtime;
      timeBuf.tv_nsec = statBuf.__unused2;
      *writeTime      = TimeUtil_UnixTimeToNtTime(timeBuf);

      timeBuf.tv_sec  = statBuf.st_ctime;
      timeBuf.tv_nsec = statBuf.__unused3;
      *attrChangeTime = TimeUtil_UnixTimeToNtTime(timeBuf);
   }
#   else
   *accessTime     = TimeUtil_UnixTimeToNtTime(statBuf.st_atim);
   *writeTime      = TimeUtil_UnixTimeToNtTime(statBuf.st_mtim);
   *attrChangeTime = TimeUtil_UnixTimeToNtTime(statBuf.st_ctim);
#   endif
#elif defined(__APPLE__)
   /* Mac: No file create timestamp. */
   *accessTime     = TimeUtil_UnixTimeToNtTime(statBuf.st_atimespec);
   *writeTime      = TimeUtil_UnixTimeToNtTime(statBuf.st_mtimespec);
   *attrChangeTime = TimeUtil_UnixTimeToNtTime(statBuf.st_ctimespec);
#else
   {
      /* Solaris: No nanosecond timestamps, no file create timestamp. */
      struct timespec timeBuf;

      timeBuf.tv_nsec = 0;

      timeBuf.tv_sec  = statBuf.st_atime;
      *accessTime     = TimeUtil_UnixTimeToNtTime(timeBuf);

      timeBuf.tv_sec  = statBuf.st_mtime;
      *writeTime      = TimeUtil_UnixTimeToNtTime(timeBuf);

      timeBuf.tv_sec  = statBuf.st_ctime;
      *attrChangeTime = TimeUtil_UnixTimeToNtTime(timeBuf);
   }
#endif

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * File_SetTimes --
 *
 *      Set the date and time that a file was created, last accessed, or
 *      last modified.
 *
 * Results:
 *      TRUE if succeed or FALSE if error.
 *
 * Side effects:
 *      If fileName is a symlink, target's timestamps will be updated.
 *      Symlink itself's timestamps will not be changed.
 *
 *----------------------------------------------------------------------
 */

Bool
File_SetTimes(ConstUnicode pathName,      // IN:
              VmTimeType createTime,      // IN: ignored
              VmTimeType accessTime,      // IN: Windows NT time format
              VmTimeType writeTime,       // IN: Windows NT time format
              VmTimeType attrChangeTime)  // IN: ignored
{
   struct timeval times[2];
   struct timeval *aTime, *wTime;
   struct stat statBuf;
   char *path;
   int err;

   if (pathName == NULL) {
      return FALSE;
   }

   path = Unicode_GetAllocBytes(pathName, STRING_ENCODING_DEFAULT);
   if (path == NULL) {
      Log(LGPFX" %s: failed to convert \"%s\" to current encoding\n",
          __FUNCTION__, UTF8(pathName));
      return FALSE;
   }

   err = (lstat(path, &statBuf) == -1) ? errno : 0;

   if (err != 0) {
      Log(LGPFX" %s: error stating file \"%s\": %s\n", __FUNCTION__,
          UTF8(pathName), strerror(err));
      free(path);
      return FALSE;
   }

   aTime = &times[0];
   wTime = &times[1];

   /*
    * Preserve old times if new time <= 0.
    * XXX Need a better implementation to preserve tv_usec.
    */

   aTime->tv_sec = statBuf.st_atime;
   aTime->tv_usec = 0;
   wTime->tv_sec = statBuf.st_mtime;
   wTime->tv_usec = 0;

   if (accessTime > 0) {
      struct timespec ts;
      TimeUtil_NtTimeToUnixTime(&ts, accessTime);
      aTime->tv_sec = ts.tv_sec;
      aTime->tv_usec = ts.tv_nsec / 1000;
   }

   if (writeTime > 0) {
      struct timespec ts;
      TimeUtil_NtTimeToUnixTime(&ts, writeTime);
      wTime->tv_sec = ts.tv_sec;
      wTime->tv_usec = ts.tv_nsec / 1000;
   }

   err = (utimes(path, times) == -1) ? errno : 0;

   free(path);

   if (err != 0) {
      Log(LGPFX" %s: utimes error on file \"%s\": %s\n", __FUNCTION__,
          UTF8(pathName), strerror(err));
      return FALSE;
   }

   return TRUE;
}


#if !defined(__FreeBSD__) && !defined(sun)
/*
 *-----------------------------------------------------------------------------
 *
 * FilePosixGetParent --
 *
 *      The input buffer is a canonical file path. Change it in place to the
 *      canonical file path of its parent directory.
 *
 *      Although this code is quite simple, we encapsulate it in a function
 *      because it is easy to get it wrong.
 *
 * Results:
 *      TRUE if the input buffer was (and remains) the root directory.
 *      FALSE if the input buffer was not the root directory and was changed in
 *            place to its parent directory.
 *
 *      Example: "/foo/bar" -> "/foo" FALSE
 *               "/foo"     -> "/"    FALSE
 *               "/"        -> "/"    TRUE
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FilePosixGetParent(Unicode *canPath)  // IN/OUT: Canonical file path
{
   Unicode pathName;
   Unicode baseName;

   ASSERT(File_IsFullPath(*canPath));

   if (Unicode_Compare(*canPath, DIRSEPS) == 0) {
      return TRUE;
   }

   File_GetPathName(*canPath, &pathName, &baseName);

   Unicode_Free(baseName);
   Unicode_Free(*canPath);

   if (Unicode_IsEmpty(pathName)) {
      /* empty string which denotes "/" */
      Unicode_Free(pathName);
      *canPath = Unicode_Duplicate("/");
   } else {
      *canPath = pathName;
   }

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * FileGetStats --
 *
 *      Calls statfs on a full path (eg. something returned from File_FullPath)
 *
 * Results:
 *      -1 if error
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static Bool
FileGetStats(ConstUnicode pathName,      // IN:
             struct statfs *pstatfsbuf)  // OUT:
{
   Bool retval = TRUE;
   Unicode dupPath = NULL;

   while (Posix_Statfs(dupPath ? dupPath : pathName,
                             pstatfsbuf) == -1) {
      if (errno != ENOENT) {
         retval = FALSE;
         break;
      }

      if (dupPath == NULL) {
         /* Dup fullPath, so as not to modify input parameters */
         dupPath = Unicode_Duplicate(pathName);
      }

      FilePosixGetParent(&dupPath);
   }

   Unicode_Free(dupPath);

   return retval;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetFreeSpace --
 *
 *      Return the free space (in bytes) available to the user on a disk where
 *      a file is or would be
 *
 * Results:
 *      -1 if error (reported to the user)
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint64
File_GetFreeSpace(ConstUnicode pathName)  // IN: File name
{
   uint64 ret;
   Unicode fullPath;
   struct statfs statfsbuf;

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      ret = -1;
      goto end;
   }

   if (!FileGetStats(fullPath, &statfsbuf)) {
      Warning("%s: Couldn't statfs\n", __func__);
      ret = -1;
      goto end;
   }

   ret = (uint64)statfsbuf.f_bavail * statfsbuf.f_bsize;

#if defined(VMX86_SERVER)
   /*
    * The following test is never true on VMvisor but we do not care as
    * this is only intended for callers going through vmkfs. Direct callers
    * as we are always get the right answer from statfs above.
    */

   if (statfsbuf.f_type == VMFS_MAGIC_NUMBER) {
      int fd;
      FS_FreeSpaceArgs args = { 0 };
      Unicode directory = NULL;

      File_SplitName(fullPath, NULL, &directory, NULL);
      /* Must use an ioctl() to get free space for a VMFS file. */
      ret = -1;
      fd = Posix_Open(directory, O_RDONLY, 0);
      if (fd == -1) {
         Warning(LGPFX" %s: open of %s failed with: %s\n", __func__,
                 UTF8(directory), Msg_ErrString());
      } else {
         if (ioctl(fd, IOCTLCMD_VMFS_GET_FREE_SPACE, &args) == -1) {
            Warning(LGPFX" %s: ioctl on %s failed with: %s\n", __func__,
                    UTF8(fullPath), Msg_ErrString());
         } else {
            ret = args.bytesFree;
         }
         close(fd);
      }

      Unicode_Free(directory);
   }
#endif

end:
   Unicode_Free(fullPath);

   return ret;
}


#if defined(VMX86_SERVER)
/*
 *----------------------------------------------------------------------
 *
 * File_GetVMFSAttributes --
 *
 *      Acquire the attributes for a given file on a VMFS volume.
 *
 * Results:
 *      Integer return value and populated FS_PartitionListResult
 *
 * Side effects:
 *      Will fail if file is not on VMFS or not enough memory for partition
 *      query results
 *
 *----------------------------------------------------------------------
 */

static int
File_GetVMFSAttributes(ConstUnicode pathName,             // IN: File to test
                       FS_PartitionListResult **fsAttrs)  // IN/OUT: VMFS Info
{
   int fd;
   int ret;
   Unicode fullPath;

   Unicode parentPath = NULL;

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      ret = -1;
      goto bail;
   }

   File_SplitName(fullPath, NULL, &parentPath, NULL);

   if (!File_OnVMFS(pathName)) {
      Log(LGPFX" %s: File %s not on VMFS volume\n", __func__,
          UTF8(pathName));
      ret = -1;
      goto bail;
   }

   *fsAttrs = Util_SafeMalloc(FS_PARTITION_ARR_SIZE(FS_PLIST_DEF_MAX_PARTITIONS));

   if (*fsAttrs == NULL) {
      Log(LGPFX" %s: failed to allocate memory\n", __func__);
      ret = -1;
      goto bail;
   }

   memset(*fsAttrs, 0, FS_PARTITION_ARR_SIZE(FS_PLIST_DEF_MAX_PARTITIONS));

   (*fsAttrs)->ioctlAttr.maxPartitions = FS_PLIST_DEF_MAX_PARTITIONS;
   (*fsAttrs)->ioctlAttr.getAttrSpec = FS_ATTR_SPEC_BASIC;

   fd = Posix_Open(parentPath, O_RDONLY, 0);

   if (fd == -1) {
      Log(LGPFX" %s: could not open %s.\n", __func__, UTF8(pathName));
      ret = -1;
      goto bail;
   }

   ret = ioctl(fd, IOCTLCMD_VMFS_FS_GET_ATTR, (char *) *fsAttrs);

   close(fd);
   if (ret == -1) {
      Log(LGPFX" %s: Could not get volume attributes (ret = %d)\n", __func__,
          ret);
   }

bail:
   Unicode_Free(fullPath);
   Unicode_Free(parentPath);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetVMFSVersion --
 *
 *      Acquire the version number for a given file on a VMFS file system.
 *
 * Results:
 *      Integer return value and version number
 *
 * Side effects:
 *      Will fail if file is not on VMFS or not enough memory for partition
 *      query results
 *
 *----------------------------------------------------------------------
 */

static int
File_GetVMFSVersion(ConstUnicode pathName,  // IN: Filename to test
                    uint32 *version)        // IN/OUT: version number of VMFS
{
   int ret = -1;
   FS_PartitionListResult *fsAttrs = NULL;

   ret = File_GetVMFSAttributes(pathName, &fsAttrs);
   if (ret < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
      goto done;
   }

   *version = fsAttrs->versionNumber;

done:
   if (fsAttrs) {
      free(fsAttrs);
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetVMFSBlockSize --
 *
 *      Acquire the blocksize for a given file on a VMFS file system.
 *
 * Results:
 *      Integer return value and block size
 *
 * Side effects:
 *      Will fail if file is not on VMFS or not enough memory for partition
 *      query results
 *
 *----------------------------------------------------------------------
 */

static int
File_GetVMFSBlockSize(ConstUnicode pathName,  // IN: File name to test
                      uint32 *blockSize)      // IN/OUT: VMFS block size
{
   int ret = -1;
   FS_PartitionListResult *fsAttrs = NULL;

   ret = File_GetVMFSAttributes(pathName, &fsAttrs);
   if (ret < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
      goto done;
   }

   *blockSize = fsAttrs->fileBlockSize;

done:
   if (fsAttrs) {
      free(fsAttrs);
   }
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetVMFSfsType --
 *
 *      Acquire the fsType for a given file on a VMFS.
 *
 * Results:
 *      Integer return value and fs type
 *
 * Side effects:
 *      Will fail if file is not on VMFS or not enough memory for partition
 *      query results
 *
 *----------------------------------------------------------------------
 */

static int
File_GetVMFSfsType(ConstUnicode pathName,  // IN: File name to test
                   char **fsType)          // IN/OUT: VMFS fsType
{
   int ret = -1;
   FS_PartitionListResult *fsAttrs = NULL;

   ret = File_GetVMFSAttributes(pathName, &fsAttrs);
   if (ret < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
      goto done;
   }

   *fsType = Util_SafeMalloc(sizeof(char) * FS_PLIST_DEF_MAX_FSTYPE_LEN);
   memcpy(*fsType, fsAttrs->fsType, FS_PLIST_DEF_MAX_FSTYPE_LEN);

done:
   if (fsAttrs) {
      free(fsAttrs);
   }
   return ret;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * File_OnVMFS --
 *
 *      Return TRUE if file is on a VMFS file system.
 *
 * Results:
 *      Boolean
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_OnVMFS(ConstUnicode pathName)
{
#if defined(VMX86_SERVER)
   Bool ret;
   struct statfs statfsbuf;

   /* XXX See Vmfs_IsVMFSDir. Same caveat about fs exclusion. */
   if (HostType_OSIsPureVMK()) {
      return TRUE;
   }

   /*
    * Do a quick statfs() for best performance in the case that the file
    * exists.  If file doesn't exist, then get the full path and do a
    * FileGetStats() to check each of the parent directories.
    */

   if (Posix_Statfs(pathName, &statfsbuf) == -1) {
      int err;
      Unicode fullPath;

      fullPath = File_FullPath(pathName);
      if (fullPath == NULL) {
         ret = FALSE;
         goto end;
      }

      err = FileGetStats(fullPath, &statfsbuf);

      Unicode_Free(fullPath);

      if (err == -1) {
         Warning(LGPFX" %s: Couldn't statfs\n", __FUNCTION__);
         ret = FALSE;
         goto end;
      }
   }

   ret = (statfsbuf.f_type == VMFS_MAGIC_NUMBER);

end:

   return ret;
#else
   return FALSE;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetCapacity --
 *
 *      Return the total capacity (in bytes) available to the user on a disk
 *      where a file is or would be
 *
 * Results:
 *      -1 if error (reported to the user)
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint64
File_GetCapacity(ConstUnicode pathName)  // IN: Path name
{
   uint64 ret;
   Unicode fullPath;
   struct statfs statfsbuf;

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      ret = -1;
      goto end;
   }

   if (!FileGetStats(fullPath, &statfsbuf)) {
      Warning(LGPFX" %s: Couldn't statfs\n", __func__);
      ret = -1;
      goto end;
   }

   ret = (uint64)statfsbuf.f_blocks * statfsbuf.f_bsize;

end:
   Unicode_Free(fullPath);
   return ret;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetUniqueFileSystemID --
 *
 *      Returns a string which uniquely identifies the underlying filesystem
 *      for a given path.
 *
 *      'path' can be relative (including empty) or absolute, and any number of
 *      non-existing components at the end of 'path' are simply ignored.
 *
 *      XXX: On Posix systems, we choose the underlying device's name as the
 *           unique ID. I make no claim that this is 100% unique so if you need
 *           this functionality to be 100% perfect, I suggest you think about
 *           it more deeply than I did. -meccleston
 *
 * Results:
 *      On success: Allocated and NUL-terminated filesystem ID.
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
File_GetUniqueFileSystemID(char const *path) // IN: File path
{
#if defined(VMX86_SERVER)
   char *canPath;
   char *existPath;

   existPath = FilePosixNearestExistingAncestor(path);
   canPath = Posix_RealPath(existPath);
   free(existPath);

   if (canPath == NULL) {
      return NULL;
   }

   /*
    * VCFS doesn't have real mount points, so the mount point lookup below
    * returns "/vmfs", instead of the VCFS mount point.
    *
    * See bug 61646 for why we care.
    */

   if (strncmp(canPath, VCFS_MOUNT_POINT, strlen(VCFS_MOUNT_POINT)) == 0) {
      char vmfsVolumeName[FILE_MAXPATH];

      if (sscanf(canPath, VCFS_MOUNT_PATH "%[^/]%*s", vmfsVolumeName) == 1) {
         free(canPath);

         return Str_SafeAsprintf(NULL, "%s/%s", VCFS_MOUNT_POINT,
                                 vmfsVolumeName);
      }
   }

   free(canPath);
#endif

   return FilePosixGetBlockDevice(path);
}


#if !defined(__APPLE__)
/*
 *-----------------------------------------------------------------------------
 *
 * FilePosixLookupMountPoint --
 *
 *      Looks up passed in canonical file path in list of mount points.
 *      If there is a match, it returns the underlying device name of the
 *      mount point along with a flag indicating whether the mount point is
 *      mounted with the "--[r]bind" option.
 *
 * Results:
 *      On success: The allocated, NUL-terminated mounted "device".
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FilePosixLookupMountPoint(char const *canPath, // IN: Canonical file path
                          Bool *bind)          // OUT: Mounted with --[r]bind?
{
   FILE *f;
   struct mntent *mnt;

   ASSERT(canPath);
   ASSERT(bind);

   f = setmntent(MOUNTED, "r");
   if (f == NULL) {
      return NULL;
   }

   /* XXX getmntent() is not thread-safe. Use getmntent_r() instead. */
   while ((mnt = getmntent(f)) != NULL) {
      /*
       * NB: A call to realpath is not needed as getmntent() already
       *     returns it in canonical form.  Additionally, it is bad
       *     to call realpath() as often a mount point is down, and
       *     realpath calls stat which can block trying to stat
       *     a filesystem that the caller of the function is not at
       *     all expecting.
       */

      if (strcmp(mnt->mnt_dir, canPath) == 0) {
         endmntent(f);

         /*
          * The --bind and --rbind options behave differently. See 
          * FilePosixGetBlockDevice() for details.
          *
          * Sadly (I blame a bug in 'mount'), there is no way to tell them
          * apart in /etc/mtab: the option recorded there is, in both cases,
          * always "bind".
          */

         *bind = strstr(mnt->mnt_opts, "bind") != NULL;

         return Util_SafeStrdup(mnt->mnt_fsname);
      }
   }

   // 'canPath' is not a mount point.
   endmntent(f);
   return NULL;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * FilePosixGetBlockDevice --
 *
 *      Retrieve the block device that backs file path 'path'.
 *
 *      'path' can be relative (including empty) or absolute, and any number of
 *      non-existing components at the end of 'path' are simply ignored.
 *
 * Results:
 *      On success: The allocated, NUL-terminated block device absolute path.
 *      On failure: NULL.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

char *
FilePosixGetBlockDevice(char const *path) // IN: File path
{
   char *existPath;
   Bool failed;
#if defined(__APPLE__)
   struct statfs buf;
#else
   char canPath[FILE_MAXPATH];
   char canPath2[FILE_MAXPATH];
   unsigned int retries = 0;
   char *realPath;
#endif

   existPath = FilePosixNearestExistingAncestor(path);

#if defined(__APPLE__)
   failed = statfs(existPath, &buf) == -1;
   free(existPath);
   if (failed) {
      return NULL;
   }

   return Util_SafeStrdup(buf.f_mntfromname);
#else
   realPath = Posix_RealPath(existPath);
   free(existPath);

   if (realPath == NULL) {
      return NULL;
   }
   Str_Strcpy(canPath, realPath, sizeof canPath);
   free(realPath);

retry:
   Str_Strcpy(canPath2, canPath, sizeof canPath2);

   /* Find the nearest ancestor of 'canPath' that is a mount point. */
   for (;;) {
      char *x;
      Bool bind;
      char *ptr;

      ptr = FilePosixLookupMountPoint(canPath, &bind);
      if (ptr) {

         if (bind) {
            /*
             * 'canPath' is a mount point mounted with --[r]bind. This is the
             * mount equivalent of a hard link. Follow the rabbit...
             *
             * --bind and --rbind behave differently. Consider this mount
             * table:
             *
             *    /dev/sda1              /             ext3
             *    exit14:/vol/vol0/home  /exit14/home  nfs
             *    /                      /bind         (mounted with --bind)
             *    /                      /rbind        (mounted with --rbind)
             *
             * then what we _should_ return for these paths is:
             *
             *    /bind/exit14/home -> /dev/sda1
             *    /rbind/exit14/home -> exit14:/vol/vol0/home
             *
             * XXX but currently because we cannot easily tell the difference,
             *     we always assume --rbind and we return:
             *
             *    /bind/exit14/home -> exit14:/vol/vol0/home
             *    /rbind/exit14/home -> exit14:/vol/vol0/home
             */

            Bool rbind = TRUE;

            if (rbind) {
               /*
                * Compute 'canPath = ptr + (canPath2 - canPath)' using and
                * preserving the structural properties of all canonical
                * paths involved in the expression.
                */

               size_t canPathLen = strlen(canPath);
               char const *diff = canPath2 + (canPathLen > 1 ? canPathLen : 0);

               if (*diff != '\0') {
                  Str_Sprintf(canPath, sizeof canPath, "%s%s",
                     strlen(ptr) > 1 ? ptr : "", diff);
               } else {
                  Str_Strcpy(canPath, ptr, sizeof canPath);
               }
            } else {
               Str_Strcpy(canPath, ptr, sizeof canPath);
            }

            free(ptr);

            /*
             * There could be a series of these chained together.  It is
             * possible for the mounts to get into a loop, so limit the total
             * number of retries to something reasonable like 10.
             */

            retries++;
            if (retries > 10) {
               Warning(LGPFX" %s: The --[r]bind mount count exceeds %u. Giving "
                       "up.\n", __func__, 10);
               return NULL;
            }

            goto retry;
         }

         return ptr;
      }

/* XXX temporary work-around until this function is Unicoded. */
      x = Util_SafeStrdup(canPath);
      failed = FilePosixGetParent(&x);
      Str_Strcpy(canPath, x, sizeof canPath);
      free(x);

      /*
       * Prevent an infinite loop in case FilePosixLookupMountPoint() even
       * fails on "/".
       */

      if (failed) {
         return NULL;
      }
   }
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * FilePosixNearestExistingAncestor --
 *
 *      Find the nearest existing ancestor of 'path'.
 *
 *      'path' can be relative (including empty) or absolute, and 'path' can
 *      have any number of non-existing components at its end.
 *
 * Results:
 *      The allocated, NUL-terminated, non-empty path of the
 *      nearest existing ancestor.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FilePosixNearestExistingAncestor(char const *path) // IN: File path
{
   size_t resultSize;
   char *result;

   resultSize = MAX(strlen(path), 1) + 1;
   result = Util_SafeMalloc(resultSize);

   Str_Strcpy(result, path, resultSize);
   for (;;) {
      char *ptr;

      if (*result == '\0') {
         Str_Strcpy(result, *path == DIRSEPC ? "/" : ".", resultSize);
         break;
      }

      if (File_Exists(result)) {
         break;
      }

      ptr = strrchr(result, DIRSEPC);
      if (!ptr) {
         ptr = result;
      }
      *ptr = '\0';
   }

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * File_IsSameFile --
 *
 *      Determine whether both paths point to the same file.
 *
 *      Caveats - While local files are matched based on inode and device 
 *      ID, some older versions of NFS return buggy device IDs, so the
 *      determination cannot be done with 100% confidence across NFS.
 *      Paths that traverse NFS mounts are matched based on device, inode
 *      and all of the fields of the stat structure except for times.
 *      This introduces a race condition in that if the target files are not
 *      locked, they can change out from underneath this function yielding
 *      false negative results.  Cloned files sytems mounted across an old
 *      version of NFS may yield a false positive.  
 *
 * Results:
 *      TRUE if both paths point to the same file, FALSE otherwise.
 *
 * Side effects:
 *      Changes errno, maybe.
 *
 *----------------------------------------------------------------------------
 */

Bool
File_IsSameFile(ConstUnicode path1,  // IN:
                ConstUnicode path2)  // IN:
{
   struct stat st1;
   struct stat st2;
   struct statfs stfs1;
   struct statfs stfs2;

   ASSERT(path1);
   ASSERT(path2);

#if defined(VMX86_SERVER)
   {
      Unicode fs1;
      Unicode fs2;

      fs1 = Posix_RealPath(path1);
      fs2 = Posix_RealPath(path2);

      /*
       * ESX doesn't have real inodes for VMFS disks in User Worlds. So only
       * way to check if a file is the same is using real path. So said Satyam.
       */

      if (fs1 && Unicode_StartsWith(fs1, VCFS_MOUNT_POINT)) {
         Bool res;

         res = (!fs2 || Unicode_Compare(fs1, fs2) != 0) ? FALSE : TRUE;

         Unicode_Free(fs1);
         Unicode_Free(fs2);

         return res;
      }

      Unicode_Free(fs1);
      Unicode_Free(fs2);
   }
#endif

   /*
    * First take care of the easy checks.  If the paths are identical, or if
    * the inode numbers don't match, we're done.
    */

   if (Unicode_Compare(path1, path2) == 0) {
      return TRUE;
   }

   if (Posix_Stat(path1, &st1) == -1) {
      return FALSE;
   }

   if (Posix_Stat(path2, &st2) == -1) {
      return FALSE;
   }

   if (st1.st_ino != st2.st_ino) {
      return FALSE;
   }

   if (Posix_Statfs(path1, &stfs1) != 0) {
      return FALSE;
   }

   if (Posix_Statfs(path2, &stfs2) != 0) {
      return FALSE;
   }

#if defined(__APPLE__)
   if ((stfs1.f_flags & MNT_LOCAL) && (stfs2.f_flags & MNT_LOCAL)) {
      return st1.st_dev == st2.st_dev;
   }
#else
   if ((stfs1.f_type != NFS_SUPER_MAGIC) &&
       (stfs2.f_type != NFS_SUPER_MAGIC)) {
      return st1.st_dev == st2.st_dev;
   }
#endif

   /*
    * At least one of the paths traverses NFS and some older NFS
    * implementations can set st_dev incorrectly. Do some extra checks of the
    * stat structure to increase our confidence. Since the st_ino numbers had
    * to match to get this far, the overwhelming odds are the two files are
    * the same.  
    *
    * If another process was actively writing or otherwise modifying the file
    * while we stat'd it, then the following test could fail and we could
    * return a false negative.  On the other hand, if NFS lies about st_dev
    * and the paths point to a cloned file system, then the we will return a
    * false positive.
    */

   if ((st1.st_dev == st2.st_dev) &&
       (st1.st_mode == st2.st_mode) &&
       (st1.st_nlink == st2.st_nlink) &&
       (st1.st_uid == st2.st_uid) &&
       (st1.st_gid == st2.st_gid) &&
       (st1.st_rdev == st2.st_rdev) &&
       (st1.st_size == st2.st_size) &&
       (st1.st_blksize == st2.st_blksize) &&
       (st1.st_blocks == st2.st_blocks)) {
      return TRUE;
   }
   return FALSE;
}


#endif /* !FreeBSD && !sun */


/*
 *-----------------------------------------------------------------------------
 *
 * File_Replace --
 *
 *      Replace old file with new file, and attempt to reproduce
 *      file permissions.
 *
 * Results:
 *      TRUE on success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_Replace(ConstUnicode oldName,  // IN: old file
             ConstUnicode newName)  // IN: new file
{
   int status;
   Bool result = FALSE;
   char *newPath = NULL;
   char *oldPath = NULL;
   struct stat st;

   newPath = Unicode_GetAllocBytes(newName, STRING_ENCODING_DEFAULT);
   if (newPath == NULL && newName != NULL) {
      status = UNICODE_CONVERSION_ERRNO;
      Msg_Append(MSGID(filePosix.replaceConversionFailed)
                 "Failed to convert file path \"%s\" to current encoding\n",
                 newName);
      goto bail;
   }
   oldPath = Unicode_GetAllocBytes(oldName, STRING_ENCODING_DEFAULT);
   if (oldPath == NULL && oldName != NULL) {
      status = UNICODE_CONVERSION_ERRNO;
      Msg_Append(MSGID(filePosix.replaceConversionFailed)
                 "Failed to convert file path \"%s\" to current encoding\n",
                 oldName);
      goto bail;
   }

   if ((stat(oldPath, &st) == 0) && (chmod(newPath, st.st_mode) == -1)) {
      status = errno;
      Msg_Append(MSGID(filePosix.replaceChmodFailed)
                 "Failed to duplicate file permissions from "
                 "\"%s\" to \"%s\": %s\n",
                 oldName, newName, Msg_ErrString());
      goto bail;
   }

   status = (rename(newPath, oldPath) == -1) ? errno : 0;
   if (status != 0) {
      Msg_Append(MSGID(filePosix.replaceRenameFailed)
                 "Failed to rename \"%s\" to \"%s\": %s\n",
                 newName, oldName, Msg_ErrString());
      goto bail;
   }

   result = TRUE;

bail:
   free(newPath);
   free(oldPath);

   errno = status;
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIsVMFS --
 *
 *      Determine whether specified file lives on VMFS filesystem.
 *      Only Linux host can have VMFS, so skip it on Solaris
 *      and FreeBSD.
 *
 * Results:
 *      TRUE if specified file lives on VMFS
 *      FALSE if file is not on VMFS or does not exist
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
FileIsVMFS(ConstUnicode pathName)  // IN: file name to test
{
#if defined(linux)
   struct statfs statfsbuf;

#if defined(VMX86_SERVER)
   /* XXX See Vmfs_IsVMFSFile. Same caveat about fs exclusion. */
   if (HostType_OSIsPureVMK()) {
      return TRUE;
   }
#endif

   if (Posix_Statfs(pathName, &statfsbuf) == 0) {
      return statfsbuf.f_type == VMFS_SUPER_MAGIC;
   }

#endif

   return FALSE;
}


/*
 *----------------------------------------------------------------------
 *
 * FilePosixCreateTestFileSize --
 *
 *      See if the given directory is on a file system that supports
 *      large files.  We just create an empty file and pass it to the
 *      FileIO_SupportsFileSize which does actual job of determining
 *      file size support.
 *
 * Results:
 *      TRUE if FS supports files of specified size
 *      FALSE otherwise (no support, invalid path, ...)
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
FilePosixCreateTestFileSize(ConstUnicode dirName, // IN: directory to create large file
                            uint64 fileSize)      // IN: test file size
{
   Bool retVal;
   int posixFD;
   Unicode temp;
   Unicode path;
   FileIODescriptor fd;

   temp = Unicode_Append(dirName, "/.vmBigFileTest");
   posixFD = File_MakeTemp(temp, &path);
   Unicode_Free(temp);

   if (posixFD == -1) {
      return FALSE;
   }

   fd = FileIO_CreateFDPosix(posixFD, O_RDWR);

   retVal = FileIO_SupportsFileSize(&fd, fileSize);
   /* Eventually perform destructive tests here... */

   FileIO_Close(&fd);
   File_Unlink(path);
   Unicode_Free(path);

   return retVal;
}

/*
 *----------------------------------------------------------------------
 *
 * File_VMFSSupportsFileSize --
 *
 *      Check if the given file is on a VMFS supports such a file size
 *
 *      In the case of VMFS2, the largest supported file size is
 *         456 * 1024 * B bytes
 *
 *      In the case of VMFS3/4, the largest supported file size is
 *         256 * 1024 * B bytes
 *
 *      where B represents the blocksize in bytes
 *
 *
 * Results:
 *      TRUE if VMFS supports such file size
 *      FALSE otherwise (file size not supported)
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static Bool
File_VMFSSupportsFileSize(ConstUnicode pathName,  // IN:
                          uint64 fileSize)        // IN:
{
#if defined(VMX86_SERVER)
   uint32 version = -1;
   uint32 blockSize = -1;
   uint64 maxFileSize = -1;
   Bool supported;
   char *fsType = NULL;

   if (File_GetVMFSVersion(pathName, &version) < 0) {
      Log(LGPFX" %s: File_GetVMFSVersion Failed\n", __func__);
      return FALSE;
   }
   if (File_GetVMFSBlockSize(pathName, &blockSize) < 0) {
      Log(LGPFX" %s: File_GetVMFSBlockSize Failed\n", __func__);
      return FALSE;
   }
   if (File_GetVMFSfsType(pathName, &fsType) < 0) {
      Log(LGPFX" %s: File_GetVMFSfsType Failed\n", __func__);
      return FALSE;
   }

   if (strcmp(fsType, "VMFS") == 0) {
      if (version == 2) {
         maxFileSize = (VMFS2CONST * (uint64) blockSize * 1024);
      } else if (version >= 3) {
         /* Get ready for VMFS4 and perform sanity check on version */
         ASSERT(version == 3 || version == 4);

         maxFileSize = (VMFS3CONST * (uint64) blockSize * 1024);
      } 

      if (fileSize <= maxFileSize && maxFileSize != -1) {
         free(fsType);
         return TRUE;
      } else {
         Log(LGPFX" %s: Requested file size (%"FMT64"d) larger than maximum "
             "supported filesystem file size (%"FMT64"d)\n", __FUNCTION__,
             fileSize, maxFileSize);
         free(fsType);
         return FALSE;
      }
   } else {
      Unicode fullPath;
      Unicode parentPath;

      fullPath = File_FullPath(pathName);

      if (fullPath == NULL) {
         Log(LGPFX" %s: Error acquiring full path\n", __func__);
         free(fsType);
         return FALSE;
      }

      File_GetPathName(fullPath, &parentPath, NULL);

      supported = FilePosixCreateTestFileSize(parentPath, fileSize);

      free(fsType);
      Unicode_Free(fullPath);
      Unicode_Free(parentPath);

      return supported;
   }

#endif
   Log(LGPFX" %s: did not execute properly\n", __func__);
   return FALSE; /* happy compiler */
}

/*
 *----------------------------------------------------------------------
 *
 * File_SupportsFileSize --
 *
 *      Check if the given file is on an FS that supports such file size
 *
 * Results:
 *      TRUE if FS supports such file size
 *      FALSE otherwise (file size not supported, invalid path, read-only, ...)
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_SupportsFileSize(ConstUnicode pathName,  // IN:
                      uint64 fileSize)        // IN:
{
   Unicode fullPath;

   Bool supported = FALSE;
   Unicode folderPath = NULL;

   /* All supported filesystems can hold at least 2GB - 1 files. */
   if (fileSize <= 0x7FFFFFFF) {
      return TRUE;
   }

   /* 
    * We acquire the full path name for testing in 
    * FilePosixCreateTestFileSize().  This is also done in the event that
    * a user tries to create a virtual disk in the directory that they want
    * a vmdk created in (setting filePath only to the disk name, not the entire
    * path.
    */

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      Log(LGPFX" %s: Error acquiring full path\n", __func__);
      goto out;
   }

   /* 
    * This function expects a filename. If given one, truncate the name to point
    * to the parent directory so we can get accurate results from FileIsVMFS.
    * If handed a directory directly, no truncation is necessary.
    */

   if (File_IsDirectory(pathName)) {
      folderPath = Unicode_Duplicate(fullPath);
   } else {
      File_SplitName(fullPath, NULL, &folderPath, NULL);
   }

   /* 
    * We know that VMFS supports large files - But they have limitations
    * See function File_VMFSSupportsFileSize() - PR 146965
    */

   if (FileIsVMFS(folderPath)) {
      supported = File_VMFSSupportsFileSize(pathName, fileSize);
      goto out;
   }

   if (File_IsFile(pathName)) {
      FileIOResult res;
      FileIODescriptor fd;

      FileIO_Invalidate(&fd);
      res = FileIO_Open(&fd, pathName, FILEIO_OPEN_ACCESS_READ, FILEIO_OPEN);
      if (FileIO_IsSuccess(res)) {
         supported = FileIO_SupportsFileSize(&fd, fileSize);
         FileIO_Close(&fd);
         goto out;
      }
   }

   /*
    * On unknown filesystems create temporary file and use it as a test.
    */

   supported = FilePosixCreateTestFileSize(folderPath, fileSize);

out:
   Unicode_Free(fullPath);
   Unicode_Free(folderPath);

   return supported;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileCreateDirectory --
 *
 *	Create a directory. The umask is honored.
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      May change the host file system.
 *
 *-----------------------------------------------------------------------------
 */

int
FileCreateDirectory(ConstUnicode pathName)  // IN:
{
   int err;

   if (pathName == NULL) {
      err = errno = EFAULT;
   } else {
      err = (Posix_Mkdir(pathName,
                         S_IRWXU | S_IRWXG | S_IRWXO) == -1) ? errno : 0;
   }

   return err;
}


/*
 *----------------------------------------------------------------------
 *
 * File_ListDirectory --
 *
 *      Gets the list of files (and directories) in a directory.
 *
 * Results:
 *      Returns the number of files returned or -1 on failure.
 *
 * Side effects:
 *      If ids is provided and the function succeeds, memory is
 *      allocated for both the unicode strings and the array itself
 *      and must be freed.  (See Unicode_FreeList.)
 *      The memory allocated for the array may be larger than necessary.
 *      The caller may trim it with realloc() if it cares.
 *
 *----------------------------------------------------------------------
 */

int
File_ListDirectory(ConstUnicode pathName,  // IN:
                   Unicode **ids)          // OUT: relative paths
{
   int err;
   DIR *dir;
   DynBuf b;
   int count;

   ASSERT(pathName != NULL);

   dir = Posix_OpenDir(pathName);

   if (dir == (DIR *) NULL) {
      // errno is preserved
      return -1;
   }

   DynBuf_Init(&b);
   count = 0;

   while (TRUE) {
      struct dirent *entry;

      errno = 0;
      entry = readdir(dir);
      if (entry == (struct dirent *) NULL) {
         err = errno;
         break;
      }

      /* Strip out undesirable paths.  No one ever cares about these. */
      if ((strcmp(entry->d_name, ".") == 0) ||
          (strcmp(entry->d_name, "..") == 0)) {
         continue;
      }

      /* Don't create the file list if we aren't providing it to the caller. */
      if (ids) {
         Unicode id = Unicode_Alloc(entry->d_name, STRING_ENCODING_DEFAULT);
         DynBuf_Append(&b, &id, sizeof id);
      }

      count++;
   }

   closedir(dir);

   if (ids && (err == 0)) {
      *ids = DynBuf_Detach(&b);
   }
   DynBuf_Destroy(&b);

   return (errno = err) == 0 ? count : -1;
}


/*
 *----------------------------------------------------------------------
 *
 * File_IsWritableDir --
 *
 *	Determine in a non-intrusive way if the user can create a file in a
 *	directory
 *
 * Results:
 *	FALSE if error (reported to the user)
 *
 * Side effects:
 *	None
 *
 * Bug:
 *	It would be cleaner to use the POSIX access(2), which deals well
 *	with read-only filesystems. Unfortunately, access(2) doesn't deal with
 *	the effective [u|g]ids.
 *
 *----------------------------------------------------------------------
 */

Bool
File_IsWritableDir(ConstUnicode dirName)  // IN:
{
   int err;
   uid_t euid;
   FileData fileData;

   if (dirName == NULL) {
      errno = EFAULT;
      return FALSE;
   }

   err = FileAttributes(dirName, &fileData);

   if ((err != 0) || (fileData.fileType != FILE_TYPE_DIRECTORY)) {
      return FALSE;
   }

   euid = geteuid();
   if (euid == 0) {
      /* Root can read or write any file. Well... This is not completely true
         because of read-only filesystems and NFS root squashing... What a
         nightmare --hpreg */

      return TRUE;
   }

   if (fileData.fileOwner == euid) {
      fileData.fileMode >>= 6;
   } else if (FileIsGroupsMember(fileData.fileGroup)) {
      fileData.fileMode >>= 3;
   }

   /* Check for Read and Execute permissions */
   return (fileData.fileMode & 3) == 3;
}


/*
 *----------------------------------------------------------------------
 *
 * FileTryDir --
 *
 *	Check to see if the given directory is actually a directory
 *      and is writable by us.
 *
 * Results:
 *	The expanded directory name on success, NULL on failure.
 *
 * Side effects:
 *	The result is allocated.
 *
 *----------------------------------------------------------------------
 */

static char *
FileTryDir(const char *dirName) // IN: Is this a writable directory?
{
   char *edirName;

   if (dirName == NULL) {
      return NULL;
   }

   edirName = Util_ExpandString(dirName);
   if (edirName != NULL && File_IsWritableDir(edirName)) {
      return edirName;
   }
   free(edirName);

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetTmpDir --
 *
 *	Determine the best temporary directory. Unsafe since the
 *	returned directory is generally going to be 0777, thus all sorts
 *	of denial of service or symlink attacks are possible.  Please
 *	use Util_GetSafeTmpDir if your dependencies permit it.
 *
 * Results:
 *	NULL if error (reported to the user).
 *
 * Side effects:
 *	The result is allocated.
 *
 *----------------------------------------------------------------------
 */

char *
File_GetTmpDir(Bool useConf) // IN: Use the config file?
{
   char *dirName;
   char *edirName;

   /* Make several attempts to find a good temporary directory candidate */

   if (useConf) {
      dirName = (char *)LocalConfig_GetString(NULL, "tmpDirectory");
      edirName = FileTryDir(dirName);
      free(dirName);
      if (edirName != NULL) {
         return edirName;
      }
   }

   /* getenv string must _not_ be freed */
   edirName = FileTryDir(getenv("TMPDIR"));
   if (edirName != NULL) {
      return edirName;
   }

   /* P_tmpdir is usually defined in <stdio.h> */
   edirName = FileTryDir(P_tmpdir);
   if (edirName != NULL) {
      return edirName;
   }

   edirName = FileTryDir("/tmp");
   if (edirName != NULL) {
      return edirName;
   }

   edirName = FileTryDir("~");
   if (edirName != NULL) {
      return edirName;
   }

   dirName = File_Cwd(NULL);

   if (dirName != NULL) {
      edirName = FileTryDir(dirName);
      free(dirName);
      if (edirName != NULL) {
         return edirName;
      }
   }

   edirName = FileTryDir("/");
   if (edirName != NULL) {
      return edirName;
   }

   Warning("%s: Couldn't get a temporary directory\n", __FUNCTION__);
   return NULL;
}

#undef HOSTINFO_TRYDIR


/*
 *----------------------------------------------------------------------
 *
 * FileIsGroupsMember --
 *
 *	Determine if a gid is in the gid list of the current process
 *
 * Results:
 *	FALSE if error (reported to the user)
 *
 * Side effects:
 *	None
 *
 *----------------------------------------------------------------------
 */

static Bool
FileIsGroupsMember(gid_t gid)
{
   int nr_members;
   gid_t *members;
   int res;
   int ret;

   members = NULL;
   nr_members = 0;
   for (;;) {
      gid_t *new;

      res = getgroups(nr_members, members);
      if (res == -1) {
         Warning(LGPFX" %s: Couldn't getgroups\n", __FUNCTION__);
         ret = FALSE;
         goto end;
      }

      if (res == nr_members) {
         break;
      }

      /* Was bug 17760 --hpreg */
      new = realloc(members, res * sizeof *members);
      if (new == NULL) {
         Warning(LGPFX" %s: Couldn't realloc\n", __FUNCTION__);
         ret = FALSE;
         goto end;
      }

      members = new;
      nr_members = res;
   }

   for (res = 0; res < nr_members; res++) {
      if (members[res] == gid) {
         ret = TRUE;
         goto end;
      }
   }
   ret = FALSE;

end:
   free(members);

   return ret;
}

/*
 *----------------------------------------------------------------------
 *
 * File_MakeCfgFileExecutable --
 *
 *	Make a .vmx file executable. This is sometimes necessary 
 *      to enable MKS access to the VM.
 *
 *      Owner always gets rwx.  Group/other get x where r is set.
 *
 * Results:
 *	FALSE if error
 *
 * Side effects:
 *	errno is set on error
 *
 *----------------------------------------------------------------------
 */

Bool
File_MakeCfgFileExecutable(ConstUnicode pathName)
{
   struct stat s;

   if (Posix_Stat(pathName, &s) == 0) {
      mode_t newMode = s.st_mode;

      newMode |= S_IRUSR | S_IWUSR | S_IXUSR;

      ASSERT_ON_COMPILE(S_IRGRP >> 2 == S_IXGRP && S_IROTH >> 2 == S_IXOTH);
      newMode |= ((newMode & (S_IRGRP | S_IROTH)) >> 2);

      return newMode == s.st_mode || Posix_Chmod(pathName, newMode);
   }
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * File_GetSizeAlternate --
 *
 *      An alternate way to determine the filesize. Useful for finding problems
 *      with files on remote fileservers, such as described in bug 19036.
 *      However, in Linux we do not have an alternate way, yet, to determine the
 *      problem, so we call back into the regular getSize function.
 *
 * Results:
 *      Size of file or -1.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

int64
File_GetSizeAlternate(ConstUnicode pathName)  // IN:
{
   return File_GetSize(pathName);
}


/*
 *----------------------------------------------------------------------------
 *
 * File_IsCharDevice --
 *
 *      This function checks whether the given file is a char device
 *      and return TRUE in such case. This is often useful on Windows
 *      where files like COM?, LPT? must be differentiated from "normal"
 *      disk files.
 *
 * Results:
 *      TRUE    is a character device
 *      FALSE   is not a character device or error
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

Bool
File_IsCharDevice(ConstUnicode pathName)  // IN:
{
   FileData fileData;

   return (FileAttributes(pathName, &fileData) == 0) &&
           (fileData.fileType == FILE_TYPE_CHARDEVICE);
}
