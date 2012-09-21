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
# include <sys/mount.h>
#else
# if !defined(__APPLE__)
#  include <sys/vfs.h>
# endif
# include <limits.h>
# include <stdio.h>      /* Needed before sys/mnttab.h in Solaris */
# if defined(sun)
#  include <sys/mnttab.h>
# elif __APPLE__
#  include <sys/mount.h>
# else
#  include <mntent.h>
#  if defined __ANDROID__
#   include <paths.h> /* for _PATH_MOUNTED */
#  endif
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
#if defined(__linux__)
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
#include "hostType.h"
#include "vmfs.h"

#include "unicodeOperations.h"

#if !defined(__FreeBSD__) && !defined(sun)
#if !defined(__APPLE__)
static char *FilePosixLookupMountPoint(char const *canPath, Bool *bind);
#endif
static char *FilePosixNearestExistingAncestor(char const *path);

#if defined(VMX86_SERVER)
#define VMFS3CONST 256
#include "hostType.h"
/* Needed for VMFS implementation of File_GetFreeSpace() */
#  include <sys/ioctl.h>
# endif
#endif

#if defined(VMX86_SERVER)
#include "fs_user.h"
#endif


/*
 * XXX
 * FTS is not available on all posix platforms that we care about.
 * We depend on FTS for a simple pre-order file traversal. For the Windows
 * implementation we need to write our own traversal code anyway. When that
 * happens the prosix version should be updated to use the generic code.
 */

#if defined(__USE_FILE_OFFSET64) || defined(sun)
# define CAN_USE_FTS 0
#else
# define CAN_USE_FTS 1
#endif

#if CAN_USE_FTS
# include <fts.h>

struct WalkDirContextImpl
{
   FTS *fts;
};

#endif

/* A string for NFS on ESX file system type */
#define FS_NFS_ON_ESX "NFS"
/* A string for VMFS on ESX file system type */
#define FS_VMFS_ON_ESX "VMFS"


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
 * File_Rename --
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
File_Rename(ConstUnicode oldName,  // IN:
            ConstUnicode newName)  // IN:
{
   return (Posix_Rename(oldName, newName) == -1) ? errno : 0;
}


int
File_RenameRetry(ConstUnicode oldFile,    // IN:
                 ConstUnicode newFile,    // IN:
                 uint32 msecMaxWaitTime)  // IN: Unused.
{
   return File_Rename(oldFile, newFile);
}


/*
 *----------------------------------------------------------------------
 *
 *  FileDeletion --
 *      Delete the specified file.  A NULL pathName will result in an error
 *      and errno will be set to EFAULT.
 *
 * Results:
 *	0	success
 *	> 0	failure (errno)
 *
 * Side effects:
 *      May change the host file system.  errno may be set.
 *
 *----------------------------------------------------------------------
 */

int
FileDeletion(ConstUnicode pathName,  // IN:
             const Bool handleLink)  // IN:
{
   int err;
   char *linkPath = NULL;
   char *primaryPath;

   if (pathName == NULL) {
      errno = EFAULT;

      return errno;
   } else if ((primaryPath = Unicode_GetAllocBytes(pathName,
                               STRING_ENCODING_DEFAULT)) == NULL) {
      Log(LGPFX" %s: failed to convert \"%s\" to current encoding\n",
          __FUNCTION__, UTF8(pathName));
      errno = UNICODE_CONVERSION_ERRNO;

      return errno;
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
 *
 *      On ESX all files are treated as local files, as all
 *      callers of this function wants to do is to post message
 *      that performance will be degraded on remote filesystems.
 *      On ESX (a) performance should be acceptable with remote
 *      files, and (b) even if it is not, we should not ask users
 *      whether they are aware that it is poor.  ESX has
 *      performance monitoring which can notify user if something
 *      is wrong.
 *
 *      On hosted platform we report remote files as faithfully
 *      as we can because having mainmem file on NFS is known
 *      to badly affect VM consistency when NFS filesystem gets
 *      reconnected.  Due to that we are conservative, and report
 *      filesystem as remote if there was some problem with
 *      determining file remoteness.
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
   if (HostType_OSIsVMK()) {
      /*
       * All files and file systems are treated as "directly attached"
       * on ESX.  See bug 158284.
       */

      return FALSE;
   } else {
      struct statfs sfbuf;

      if (Posix_Statfs(pathName, &sfbuf) == -1) {
         Log(LGPFX" %s: statfs(%s) failed: %s\n", __func__, UTF8(pathName),
             Err_Errno2String(errno));

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

      if (CIFS_SUPER_MAGIC == sfbuf.f_type) {
         return TRUE;
      }

      return FALSE;
#endif
   }
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
                 "Check if the directory has been deleted or "
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

   if ((pathName != NULL) && File_IsFullPath(pathName)) {
      cwd = NULL;
   } else {
      cwd = File_Cwd(NULL);
      if (cwd == NULL) {
         return NULL;
      }
   }

   if ((pathName == NULL) || Unicode_IsEmpty(pathName)) {
      ret = Unicode_Duplicate(cwd);
   } else if (File_IsFullPath(pathName)) {
       ret = Posix_RealPath(pathName);
       if (ret == NULL) {
          ret = FileStripFwdSlashes(pathName);
       }
   } else {
      Unicode path = Unicode_Join(cwd, DIRSEPS, pathName, NULL);

      ret = Posix_RealPath(path);

      if (ret == NULL) {
         ret = FileStripFwdSlashes(path);
      } 
      Unicode_Free(path);
   }

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
File_GetTimes(ConstUnicode pathName,       // IN:
              VmTimeType *createTime,      // OUT: Windows NT time format
              VmTimeType *accessTime,      // OUT: Windows NT time format
              VmTimeType *writeTime,       // OUT: Windows NT time format
              VmTimeType *attrChangeTime)  // OUT: Windows NT time format
{
   struct stat statBuf;

   ASSERT(createTime && accessTime && writeTime && attrChangeTime);

   *createTime     = -1;
   *accessTime     = -1;
   *writeTime      = -1;
   *attrChangeTime = -1;

   if (Posix_Lstat(pathName, &statBuf) == -1) {
      Log(LGPFX" %s: error stating file \"%s\": %s\n", __FUNCTION__,
          UTF8(pathName), Err_Errno2String(errno));

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

#   if (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 3) && !defined(__UCLIBC__)
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
          UTF8(pathName), Err_Errno2String(err));
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
          UTF8(pathName), Err_Errno2String(err));

      return FALSE;
   }

   return TRUE;
}


/*
 *----------------------------------------------------------------------
 *
 * File_SetFilePermissions --
 *
 *      Set file permissions.
 *
 * Results:
 *      TRUE if succeed or FALSE if error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
File_SetFilePermissions(ConstUnicode pathName,  // IN:
                        int perms)              // IN: permissions
{
   ASSERT(pathName);

   if (Posix_Chmod(pathName, perms) == -1) {
      /* The error is not critical, just log it. */
      Log(LGPFX" %s: failed to change permissions on file \"%s\": %s\n",
          __FUNCTION__, UTF8(pathName), Err_Errno2String(errno));

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
 *      Calls statfs on a full path (eg. something returned from File_FullPath).
 *      If doNotAscend is FALSE, climb up the directory chain and call statfs
 *      on each level until it succeeds.
 *
 * Results:
 *      TRUE	statfs succeeded
 *	FALSE	unable to statfs anything along the path
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static Bool
FileGetStats(ConstUnicode pathName,      // IN:
             Bool doNotAscend,           // IN:
             struct statfs *pstatfsbuf)  // OUT:
{
   Bool retval = TRUE;
   Unicode dupPath = NULL;

   while (Posix_Statfs(dupPath ? dupPath : pathName,
                             pstatfsbuf) == -1) {
      if (errno != ENOENT || doNotAscend) {
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
 *      a file is or would be.  If doNotAscend is FALSE, the helper function
 *      ascends the directory chain on system call errors in order to obtain
 *      the file system information.
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
File_GetFreeSpace(ConstUnicode pathName,  // IN: File name
                  Bool doNotAscend)       // IN: Do not ascend dir chain
{
   uint64 ret;
   Unicode fullPath;
   struct statfs statfsbuf;

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      return -1;
   }

   if (FileGetStats(fullPath, doNotAscend, &statfsbuf)) {
      ret = (uint64) statfsbuf.f_bavail * statfsbuf.f_bsize;
   } else {
      Warning("%s: Couldn't statfs %s\n", __func__, fullPath);
      ret = -1;
   }

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

int
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

   if (!HostType_OSIsVMK()) {
      Log(LGPFX" %s: File %s not on VMFS volume\n", __func__, UTF8(pathName));
      ret = -1;
      goto bail;
   }

   *fsAttrs = Util_SafeMalloc(FS_PARTITION_ARR_SIZE(FS_PLIST_DEF_MAX_PARTITIONS));

   memset(*fsAttrs, 0, FS_PARTITION_ARR_SIZE(FS_PLIST_DEF_MAX_PARTITIONS));

   (*fsAttrs)->ioctlAttr.maxPartitions = FS_PLIST_DEF_MAX_PARTITIONS;
   (*fsAttrs)->ioctlAttr.getAttrSpec = FS_ATTR_SPEC_BASIC;

   fd = Posix_Open(parentPath, O_RDONLY, 0);

   if (fd == -1) {
      Log(LGPFX" %s: could not open %s: %s\n", __func__, UTF8(pathName),
          Err_Errno2String(errno));
      ret = -1;
      goto bail;
   }

   ret = ioctl(fd, IOCTLCMD_VMFS_FS_GET_ATTR, (char *) *fsAttrs);

   close(fd);
   if (ret == -1) {
      Log(LGPFX" %s: Could not get volume attributes (ret = %d): %s\n",
          __func__, ret, Err_Errno2String(errno));
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
 *      Get the version number of the VMFS file system on which the
 *      given file resides.
 *
 * Results:
 *      Integer return value and version number.
 *
 * Side effects:
 *      Will fail if file is not on VMFS or not enough memory for partition
 *      query results.
 *
 *----------------------------------------------------------------------
 */

int
File_GetVMFSVersion(ConstUnicode pathName,  // IN: File name to test
                    uint32 *versionNum)     // OUT: Version number
{
   int ret = -1;
   FS_PartitionListResult *fsAttrs = NULL;

   if (!versionNum) {
      errno = EINVAL;
      goto exit;
   }
   
   ret = File_GetVMFSAttributes(pathName, &fsAttrs);

   if (ret < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
   } else {
      *versionNum = fsAttrs->versionNumber;
   }

   if (fsAttrs) {
      free(fsAttrs);
   }

exit:
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

int
File_GetVMFSBlockSize(ConstUnicode pathName,  // IN: File name to test
                      uint32 *blockSize)      // IN/OUT: VMFS block size
{
   int ret = -1;
   FS_PartitionListResult *fsAttrs = NULL;

   if (!blockSize) {
      errno = EINVAL;
      goto exit;
   }

   ret = File_GetVMFSAttributes(pathName, &fsAttrs);

   if (ret < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
   } else {
      *blockSize = fsAttrs->fileBlockSize;
   }

   if (fsAttrs) {
      free(fsAttrs);
   }

exit:
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * File_GetVMFSMountInfo --
 *
 *      Acquire the FS mount point info such as fsType, major version,
 *      local mount point (/vmfs/volumes/xyz), and for NFS,
 *      remote IP and remote mount point for a given file.     
 *
 * Results:
 *      Integer return value and allocated data
 *
 * Side effects:
 *      Only implemented on ESX. Will fail on other platforms. 
 *      remoteIP and remoteMountPoint are only populated for files on NFS.   
 *
 *----------------------------------------------------------------------
 */

int 
File_GetVMFSMountInfo(ConstUnicode pathName,   // IN:
                     char **fsType,            // OUT:
                     uint32 *version,          // OUT:
                     char **remoteIP,          // OUT:
                     char **remoteMountPoint,  // OUT:
                     char **localMountPoint)   // OUT:
{
   int ret;
   int len;
   FS_PartitionListResult *fsAttrs;

   *localMountPoint = File_GetUniqueFileSystemID(pathName);

   if (*localMountPoint == NULL) {
      return -1;
   }

   // Get file IP and mount point
   ret = File_GetVMFSAttributes(pathName, &fsAttrs);  
   if (ret >= 0 && fsAttrs) { 
      *version = fsAttrs->versionNumber;
      *fsType = Util_SafeStrdup(fsAttrs->fsType);
 
      if (strncmp(fsAttrs->fsType, FS_NFS_ON_ESX, sizeof(FS_NFS_ON_ESX)) == 0) {
         len = strlen(fsAttrs->logicalDevice);
         *remoteIP = Util_SafeMalloc(len);
         *remoteMountPoint = Util_SafeMalloc(len);
         sscanf(fsAttrs->logicalDevice, "%s %s", *remoteIP, *remoteMountPoint);
      } else {
         *remoteIP = NULL;
         *remoteMountPoint = NULL;   
      }  
   }

   free(fsAttrs);

   return ret;
}
#endif 


/*
 *----------------------------------------------------------------------
 *
 * FileIsVMFS --
 *
 *      Is the given file on a filesystem that supports vmfs-specific
 *      features like zeroed-thick and multiwriter files?
 *
 * Results:
 *      TRUE if we're on VMFS.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
FileIsVMFS(ConstUnicode pathName)  // IN:
{
   Bool result = FALSE;

#if defined(VMX86_SERVER)
   /* Right now only VMFS supports zeroedThick and multiWriter. */
   FS_PartitionListResult *fsAttrs = NULL;

   if (File_GetVMFSAttributes(pathName, &fsAttrs) >= 0) {
      result = strncmp(fsAttrs->fsType, FS_VMFS_ON_ESX,
                       sizeof(FS_VMFS_ON_ESX)) == 0;
   } else {
      Log(LGPFX" %s: File_GetVMFSAttributes failed\n", __func__);
   }

   if (fsAttrs) {
      free(fsAttrs);
   }
#endif
 
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_SupportsZeroedThick --
 *
 *      Check if the given file is on an FS supports creation of
 *      the zeroed-thick files.
 *      Currently only VMFS on ESX does support zeroed-thick files, but
 *      this may change in the future.
 *
 * Results:
 *      TRUE if FS supports creation of the zeroed-thick files.
 *
 * Side effects:
 *       None
 *
 *----------------------------------------------------------------------
 */

Bool
File_SupportsZeroedThick(ConstUnicode pathName)  // IN:
{
   return FileIsVMFS(pathName);
}


/*
 *----------------------------------------------------------------------
 *
 * File_SupportsMultiWriter --
 *  
 *      Check if the given file is on an FS supports opening files
 *      in multi-writer mode.
 *      Currently only VMFS on ESX supports multi-writer mode, but
 *      this may change in the future.
 *
 * Results:
 *      TRUE if FS supports opening files in multi-writer mode.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
File_SupportsMultiWriter(ConstUnicode pathName)  // IN:
{
   return FileIsVMFS(pathName);
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
      return -1;
   }

   if (FileGetStats(fullPath, FALSE, &statfsbuf)) {
      ret = (uint64) statfsbuf.f_blocks * statfsbuf.f_bsize;
   } else {
      Warning(LGPFX" %s: Couldn't statfs\n", __func__);
      ret = -1;
   }

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
 *      'path' can be relative (including empty) or absolute, and any number
 *      of non-existing components at the end of 'path' are simply ignored.
 *
 *      XXX: On Posix systems, we choose the underlying device's name as the
 *           unique ID. I make no claim that this is 100% unique so if you
 *           need this functionality to be 100% perfect, I suggest you think
 *           about it more deeply than I did. -meccleston
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
File_GetUniqueFileSystemID(char const *path)  // IN: File path
{
   if (HostType_OSIsVMK()) {
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

         if (sscanf(canPath, VCFS_MOUNT_PATH "%[^/]%*s",
                    vmfsVolumeName) == 1) {
            free(canPath);

            return Str_SafeAsprintf(NULL, "%s/%s", VCFS_MOUNT_POINT,
                                    vmfsVolumeName);
         }
      }

      free(canPath);
   }

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
FilePosixLookupMountPoint(char const *canPath,  // IN: Canonical file path
                          Bool *bind)           // OUT: Mounted with --[r]bind?
{
   FILE *f;
   struct mntent mnt;
   char *buf;
   size_t size;
   size_t used;
   char *ret = NULL;

   ASSERT(canPath);
   ASSERT(bind);

   size = 4 * FILE_MAXPATH;  // Should suffice for most locales

retry:
#if defined __ANDROID__
   /*
    * Android supports neither setmntent() nor MOUNTED.
    * The code below is a workaround.
    */
   NOT_TESTED();
   f = fopen(_PATH_MOUNTED, "r");
#else
   f = setmntent(MOUNTED, "r");
#endif
   if (f == NULL) {
      return NULL;
   }

   buf = Util_SafeMalloc(size);

   while (Posix_Getmntent_r(f, &mnt, buf, size) != NULL) {

      /*
       * Our Posix_Getmntent_r graciously sets errno when the buffer 
       * is too small, but on UTF-8 based platforms Posix_Getmntent_r
       * is #defined to the system's getmntent_r, which can simply 
       * truncate the strings with no other indication.  See how much 
       * space it used and increase the buffer size if needed.  Note
       * that if some of the strings are empty, they may share a
       * common nul in the buffer, and the resulting size calculation 
       * will be a little over-zealous.
       */

      used = 0;  
      if (mnt.mnt_fsname) {
         used += strlen(mnt.mnt_fsname) + 1;
      } 
      if (mnt.mnt_dir) {
         used += strlen(mnt.mnt_dir) + 1;
      } 
      if (mnt.mnt_type) {
         used += strlen(mnt.mnt_type) + 1;
      } 
      if (mnt.mnt_opts) {
         used += strlen(mnt.mnt_opts) + 1;
      } 
      if (used >= size || !mnt.mnt_fsname || !mnt.mnt_dir || 
          !mnt.mnt_type || !mnt.mnt_opts) {
         size += 4 * FILE_MAXPATH;
         ASSERT(size <= 32 * FILE_MAXPATH);
         free(buf);
         endmntent(f);
         goto retry;
      }

      /*
       * NB: A call to realpath is not needed as getmntent() already
       *     returns it in canonical form.  Additionally, it is bad
       *     to call realpath() as often a mount point is down, and
       *     realpath calls stat which can block trying to stat
       *     a filesystem that the caller of the function is not at
       *     all expecting.
       */

      if (strcmp(mnt.mnt_dir, canPath) == 0) {
         /*
          * The --bind and --rbind options behave differently. See 
          * FilePosixGetBlockDevice() for details.
          *
          * Sadly (I blame a bug in 'mount'), there is no way to tell them
          * apart in /etc/mtab: the option recorded there is, in both cases,
          * always "bind".
          */

         *bind = strstr(mnt.mnt_opts, "bind") != NULL;

         ret = Util_SafeStrdup(mnt.mnt_fsname);

	 break;
      }
   }

   // 'canPath' is not a mount point.
   endmntent(f);

   free(buf);

   return ret;
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
FilePosixGetBlockDevice(char const *path)  // IN: File path
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
FilePosixNearestExistingAncestor(char const *path)  // IN: File path
{
   size_t resultSize;
   char *result;
   struct stat statbuf;

   resultSize = MAX(strlen(path), 1) + 1;
   result = Util_SafeMalloc(resultSize);
   Str_Strcpy(result, path, resultSize);

   for (;;) {
      char *ptr;

      if (*result == '\0') {
         Str_Strcpy(result, *path == DIRSEPC ? "/" : ".", resultSize);
         break;
      }

      if (Posix_Stat(result, &statbuf) == 0) {
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


#endif /* !FreeBSD && !sun */


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
#if !defined(sun)  // Solaris does not have statfs
   struct statfs stfs1;
   struct statfs stfs2;
#endif

   ASSERT(path1);
   ASSERT(path2);

   /*
    * First take care of the easy checks.  If the paths are identical, or if
    * the inode numbers or resident devices don't match, we're done.
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

   if (st1.st_dev != st2.st_dev) {
      return FALSE;
   }

   if (HostType_OSIsPureVMK()) {
      /*
       * On ESX, post change 1074635 the st_dev field of the stat structure
       * is valid and differentiates between resident devices or NFS file
       * systems - no need to use statfs to obtain file system information.
       */

      return TRUE;
   }

#if !defined(sun)  // Solaris does not have statfs
   if (Posix_Statfs(path1, &stfs1) != 0) {
      return FALSE;
   }

   if (Posix_Statfs(path2, &stfs2) != 0) {
      return FALSE;
   }

#if defined(__APPLE__) || defined(__FreeBSD__) 
   if ((stfs1.f_flags & MNT_LOCAL) && (stfs2.f_flags & MNT_LOCAL)) {
      return TRUE;
   }
#else
   if ((stfs1.f_type != NFS_SUPER_MAGIC) &&
       (stfs2.f_type != NFS_SUPER_MAGIC)) {
      return TRUE;
   }
#endif
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

   if ((st1.st_mode == st2.st_mode) &&
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


/*
 *-----------------------------------------------------------------------------
 *
 * File_Replace --
 *
 *      Replace old file with new file, and attempt to reproduce
 *      file permissions.  A NULL value for either the oldName or
 *      newName will result in failure and errno will be set to EFAULT.
 *
 * Results:
 *      TRUE on success.
 *
 * Side effects:
 *      errno may be set.
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

   if (newName == NULL) {
      status = EFAULT;
      goto bail;
   } else if ((newPath = Unicode_GetAllocBytes(newName,
                           STRING_ENCODING_DEFAULT)) == NULL) {
      status = UNICODE_CONVERSION_ERRNO;
      Msg_Append(MSGID(filePosix.replaceConversionFailed)
                 "Failed to convert file path \"%s\" to current encoding\n",
                 newName);
      goto bail;
   }
   if (oldName == NULL) {
      status = EFAULT;
      goto bail;
   } else if ((oldPath = Unicode_GetAllocBytes(oldName,
                           STRING_ENCODING_DEFAULT)) == NULL) {
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
FilePosixCreateTestFileSize(ConstUnicode dirName,  // IN: test directory
                            uint64 fileSize)       // IN: test file size
{
   Bool retVal;
   int posixFD;
   Unicode temp;
   Unicode path;
   FileIODescriptor fd;

   temp = Unicode_Append(dirName, "/.vmBigFileTest");
   posixFD = File_MakeSafeTemp(temp, &path);
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
 *      In the case of VMFS3, the largest supported file size is
 *         256 * 1024 * B bytes
 *      VMFS5 supports larger file sizes.
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
   uint64 maxFileSize = -1;
   Bool supported;
   FS_PartitionListResult *fsAttrs = NULL;

   if (File_GetVMFSAttributes(pathName, &fsAttrs) < 0) {
      Log(LGPFX" %s: File_GetVMFSAttributes Failed\n", __func__);

      return FALSE;
   }

   if (strcmp(fsAttrs->fsType, FS_VMFS_ON_ESX) == 0) {
      if (fsAttrs->versionNumber == 3) {
         maxFileSize = (VMFS3CONST * (uint64) fsAttrs->fileBlockSize * 1024);
      } else if (fsAttrs->versionNumber >= 5) {
         /* Get ready for 64 TB on VMFS5 and perform sanity check on version */
         maxFileSize = (uint64) 0x400000000000ULL;
      } else {
         Log(LGPFX" %s: Unsupported filesystem version, %u\n", __func__,
             fsAttrs->versionNumber);
         free(fsAttrs);

         return FALSE;
      }

      if (fileSize <= maxFileSize && maxFileSize != -1) {
         free(fsAttrs);

         return TRUE;
      } else {
         Log(LGPFX" %s: Requested file size (%"FMT64"d) larger than maximum "
             "supported filesystem file size (%"FMT64"d)\n", __FUNCTION__,
             fileSize, maxFileSize);
         free(fsAttrs);

         return FALSE;
      }
   } else {
      Unicode fullPath;
      Unicode parentPath;

      fullPath = File_FullPath(pathName);

      if (fullPath == NULL) {
         Log(LGPFX" %s: Error acquiring full path\n", __func__);
         free(fsAttrs);

         return FALSE;
      }

      File_GetPathName(fullPath, &parentPath, NULL);

      supported = FilePosixCreateTestFileSize(parentPath, fileSize);

      free(fsAttrs);
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
   Unicode folderPath;

   Bool supported = FALSE;

   /* All supported filesystems can hold at least 2GB - 1 files. */
   if (fileSize <= 0x7FFFFFFF) {
      return TRUE;
   }

   /* 
    * We acquire the full path name for testing in 
    * FilePosixCreateTestFileSize().  This is also done in the event that
    * a user tries to create a virtual disk in the directory that they want
    * a vmdk created in (setting filePath only to the disk name, not the
    * entire path.
    */

   fullPath = File_FullPath(pathName);
   if (fullPath == NULL) {
      Log(LGPFX" %s: Error acquiring full path\n", __func__);
      goto out;
   }

   /* 
    * We know that VMFS supports large files - But they have limitations
    * See function File_VMFSSupportsFileSize() - PR 146965
    */

   if (HostType_OSIsVMK()) {
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
    * On unknown filesystems create a temporary file in the argument file's
    * parent directory and use it as a test.
    */

   if (File_IsDirectory(pathName)) {
      folderPath = Unicode_Duplicate(fullPath);
   } else {
      folderPath = NULL;
      File_SplitName(fullPath, NULL, &folderPath, NULL);
   }

   supported = FilePosixCreateTestFileSize(folderPath, fileSize);
   Unicode_Free(folderPath);

out:
   Unicode_Free(fullPath);
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
FileCreateDirectory(ConstUnicode pathName,  // IN:
                    int mask)               // IN:
{
   int err;

   if (pathName == NULL) {
      err = errno = EFAULT;
   } else {
      err = (Posix_Mkdir(pathName, mask) == -1) ? errno : 0;
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
 *      A file name that cannot be represented in the default encoding
 *      will appear as a string of three UTF8 sustitution characters.
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
         Unicode id;

         if (Unicode_IsBufferValid(entry->d_name, -1,
                                   STRING_ENCODING_DEFAULT)) {
            id = Unicode_Alloc(entry->d_name, STRING_ENCODING_DEFAULT);
         } else {
            id = Unicode_EscapeBuffer(entry->d_name, -1,
                                     STRING_ENCODING_DEFAULT);

            Warning("%s: file '%s' in directory '%s' cannot be converted to "
                    "UTF8\n", __FUNCTION__, pathName, id);

            Unicode_Free(id);

            id = Unicode_Duplicate(UNICODE_SUBSTITUTION_CHAR
                                   UNICODE_SUBSTITUTION_CHAR
                                   UNICODE_SUBSTITUTION_CHAR);
         }

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


#if CAN_USE_FTS

/*
 *-----------------------------------------------------------------------------
 *
 * File_WalkDirectoryStart --
 *
 *      Start a directory tree walk at 'parentPath'.
 *
 *      To read each entry, repeatedly pass the returned context to
 *      File_WalkDirectoryNext() until that function returns FALSE.
 *
 *      When done, pass the returned context to File_WalkDirectoryEnd().
 *
 *      A pre-order, logical traversal will be completed; hard links and
 *      symbolic links that do not cause a cycle are followed in the directory
 *      traversal.
 *
 *      We assume no thread will change the working directory between the calls
 *      to File_WalkDirectoryStart and File_WalkDirectoryEnd.
 *
 * Results:
 *      A context used in subsequent calls to File_WalkDirectoryNext() or NULL
 *      if an error is encountered.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

WalkDirContext
File_WalkDirectoryStart(ConstUnicode parentPath)  // IN:
{
   WalkDirContextImpl *context;
   char * const traversalRoots[] =
      { Unicode_GetAllocBytes(parentPath, STRING_ENCODING_DEFAULT), NULL };

   context = malloc(sizeof *context);
   if (!context) {
      return NULL;
   }

   context->fts = fts_open(traversalRoots, FTS_LOGICAL|FTS_NOSTAT|FTS_NOCHDIR,
                           NULL);
   if (!context->fts) {
      free(context);
      context = NULL;
   }

   free(traversalRoots[0]);

   return context;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_WalkDirectoryNext --
 *
 *      Get the next entry in a directory traversal started with
 *      File_WalkDirectoryStart.
 *
 * Results:
 *      TRUE iff the traversal hasn't completed.
 *
 *      If TRUE, *path holds an allocated string prefixed by parentPath that
 *      the caller must free (see Unicode_Free).
 *
 *      If FALSE, errno is 0 iff the walk completed sucessfully.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_WalkDirectoryNext(WalkDirContext context,  // IN:
                       Unicode *path)           // OUT:
{
   FTSENT *nextEntry;

   ASSERT(context);
   ASSERT(context->fts);
   ASSERT(path);

   do {
      nextEntry = fts_read(context->fts);

      /*
       * We'll skip any entries that cannot be read, are errors, or
       * are the second traversal (post-order) of a directory.
       */

      if (nextEntry &&
          nextEntry->fts_info != FTS_DNR &&
          nextEntry->fts_info != FTS_ERR &&
          nextEntry->fts_info != FTS_DP) {
         *path = Unicode_AllocWithLength(nextEntry->fts_path,
                                         nextEntry->fts_pathlen,
                                         STRING_ENCODING_DEFAULT);

         return TRUE;
      }
   } while (nextEntry);

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_WalkDirectoryEnd --
 *
 *      End the directory traversal.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      The context is now invalid.
 *
 *-----------------------------------------------------------------------------
 */

void
File_WalkDirectoryEnd(WalkDirContext context)  // IN:
{
   ASSERT(context);
   ASSERT(context->fts);

   if (fts_close(context->fts) == -1) {
      Log(LGPFX" %s: failed to close fts: %p\n", __FUNCTION__, context->fts);
   }
   free((WalkDirContextImpl *)context);
}

#else


/*
 *-----------------------------------------------------------------------------
 *
 * File_WalkDirectoryStart --
 * File_WalkDirectoryNext --
 * File_WalkDirectoryEnd --
 *
 *      XXX FTS is not supported on this posix variant. See above.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      ASSERTs.
 *
 *-----------------------------------------------------------------------------
 */

WalkDirContext
File_WalkDirectoryStart(ConstUnicode parentPath)  // IN:
{
   NOT_IMPLEMENTED();
}


Bool
File_WalkDirectoryNext(WalkDirContext context,  // IN:
                       Unicode *path)           // OUT:
{
   NOT_IMPLEMENTED();
}


void
File_WalkDirectoryEnd(WalkDirContext context)  // IN:
{
   NOT_IMPLEMENTED();
}


#endif // CAN_USE_FTS


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
FileIsGroupsMember(gid_t gid)  // IN:
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
 * FileIsWritableDir --
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
FileIsWritableDir(ConstUnicode dirName)  // IN:
{
   int err;
   uid_t euid;
   FileData fileData;

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
File_MakeCfgFileExecutable(ConstUnicode pathName)  // IN:
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
 *      An alternate way to determine the filesize. Useful for finding
 *      problems with files on remote fileservers, such as described in bug
 *      19036. However, in Linux we do not have an alternate way, yet, to
 *      determine the problem, so we call back into the regular getSize
 *      function.
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
