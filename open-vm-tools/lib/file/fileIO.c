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
 * fileIO.c --
 *
 *    Basic (non internationalized) implementation of error messages for the
 *    Files library.
 *
 *    File locking/unlocking routines.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "vmware.h"
#include "util.h"
#include "fileIO.h"
#include "fileLock.h"
#include "fileInt.h"
#include "msg.h"
#include "unicodeOperations.h"
#include "hostType.h"
#if defined(_WIN32)
#include <io.h>
#else
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#if defined(VMX86_SERVER)
#include "fs_public.h"
#endif


/*
 *----------------------------------------------------------------------
 *
 * FileIO_ErrorEnglish --
 *
 *      Return the message associated with a status code
 *
 * Results:
 *      The message
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

const char *
FileIO_ErrorEnglish(FileIOResult status)  // IN:
{
   return Msg_StripMSGID(FileIO_MsgError(status));
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_MsgError --
 *
 *      Return the message associated with a status code
 *
 * Results:
 *      The message.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
FileIO_MsgError(FileIOResult status)  // IN:
{
   const char *result = NULL;

   switch (status) {
   case FILEIO_SUCCESS:
      /*
       * Most of the time, you don't call this function with this value
       * because there is no error
       */
      result = MSGID(fileio.success) "Success";
      break;

   case FILEIO_CANCELLED:
      /*
       * Most of the time, you don't call this function with this value
       * because you don't want to display error messages after a user has
       * cancelled an operation.
       */
      result = MSGID(fileio.cancel) "The operation was cancelled by the user";
      break;

   case FILEIO_ERROR:
      /*
       * Most of the time, you don't call this function with this value
       * because you can call your native function to retrieve a more
       * accurate message.
       */
      result = MSGID(fileio.generic) "Error";
      break;

   case FILEIO_OPEN_ERROR_EXIST:
      result = MSGID(fileio.exists) "The file already exists";
      break;

   case FILEIO_LOCK_FAILED:
      result = MSGID(fileio.lock) "Failed to lock the file";
      break;

   case FILEIO_READ_ERROR_EOF:
      result = MSGID(fileio.eof) "Tried to read beyond the end of the file";
      break;

   case FILEIO_FILE_NOT_FOUND:
      result = MSGID(fileio.notfound) "Could not find the file";
      break;

   case FILEIO_NO_PERMISSION:
      result = MSGID(fileio.noPerm) "Insufficient permission to access the file";
      break;

   case FILEIO_FILE_NAME_TOO_LONG:
      result = MSGID(fileio.namelong) "The file name is too long";
      break;

   case FILEIO_WRITE_ERROR_FBIG:
      result = MSGID(fileio.fBig) "The file is too large";
      break;

   case FILEIO_WRITE_ERROR_NOSPC:
      result = MSGID(fileio.noSpc) "There is no space left on the device";
      break;

   case FILEIO_WRITE_ERROR_DQUOT:
      result = MSGID(fileio.dQuot) "There is no space left on the device";
      break;

   case FILEIO_ERROR_LAST:
      NOT_IMPLEMENTED();
      break;

      /*
       * We do not provide a default case on purpose, so that the compiler can
       * detect changes in the error set and reminds us to implement the
       * associated messages --hpreg
       */
   }

   if (!result) {
      Warning("%s: bad code %d\n", __FUNCTION__, status);
      ASSERT(0);
      result = MSGID(fileio.unknown) "Unknown error";
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Init --
 *
 *      Initialize invalid FileIODescriptor.  Expects that caller
 *	prepared structure with FileIO_Invalidate.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
FileIO_Init(FileIODescriptor *fd,  // IN/OUT:
            const char *pathName)  // IN:
{
   ASSERT(fd != NULL);
   ASSERT(pathName != NULL);

   fd->fileName = Unicode_Duplicate(pathName);
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Cleanup --
 *
 *      Undo resource allocation done by FileIO_Init.  You do not want to
 *	call this function directly, you most probably want FileIO_Close.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
FileIO_Cleanup(FileIODescriptor *fd)  // IN/OUT:
{
   ASSERT(fd != NULL);

   if (fd->fileName) {
      Posix_Free(fd->fileName);
      fd->fileName = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIOResolveLockBits --
 *
 *      Resolve the multitude of lock bits from historical public names
 *      to newer internal names.
 *
 *      Input flags: FILEIO_OPEN_LOCKED a.k.a. FILEIO_OPEN_LOCK_BEST,
 *                   FILEIO_OPEN_EXCLUSIVE_LOCK
 *      Output flags: FILEIO_OPEN_LOCK_MANDATORY, FILEIO_OPEN_LOCK_ADVISORY
 *
 * Results:
 *      None
 *
 * Side effects:
 *      Only output flags are set in *access.
 *
 *----------------------------------------------------------------------
 */

void
FileIOResolveLockBits(int *access)  // IN/OUT: FILEIO_OPEN_* bits
{
   /*
    * Lock types:
    *    none: no locking at all
    *    advisory: open() ignores lock, FileIO_ respects lock.
    *    mandatory: open() and FileIO_ respect lock.
    *    "best": downgrades to advisory or mandatory based on OS support
    */
   if ((*access & FILEIO_OPEN_EXCLUSIVE_LOCK) != 0) {
      *access &= ~FILEIO_OPEN_EXCLUSIVE_LOCK;
      *access |= FILEIO_OPEN_LOCK_MANDATORY;
   }
   if ((*access & FILEIO_OPEN_LOCK_BEST) != 0) {
      /* "Best effort" bit: mandatory if OS supports, advisory otherwise */
      *access &= ~FILEIO_OPEN_LOCK_BEST;
      if (HostType_OSIsVMK()) {
         *access |= FILEIO_OPEN_LOCK_MANDATORY;
      } else {
         *access |= FILEIO_OPEN_LOCK_ADVISORY;
      }
   }

   /* Only one lock type (or none at all) allowed */
   ASSERT(((*access & FILEIO_OPEN_LOCK_ADVISORY) == 0) ||
          ((*access & FILEIO_OPEN_LOCK_MANDATORY) == 0));
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Lock --
 *
 *      Call the FileLock module to lock the given file.
 *
 * Results:
 *      FILEIO_ERROR               A serious error occured.
 *      FILEIO_SUCCESS             All is well
 *      FILEIO_LOCK_FAILED         Requested lock on file was not acquired
 *      FILEIO_FILE_NOT_FOUND      Unable to find the specified file
 *      FILEIO_NO_PERMISSION       Permissions issues
 *      FILEIO_FILE_NAME_TOO_LONG  The path name is too long
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Lock(FileIODescriptor *file,  // IN/OUT:
            int access)              // IN:
{
   FileIOResult ret = FILEIO_SUCCESS;

   /*
    * Lock the file if necessary.
    */

   ASSERT(file != NULL);
   ASSERT(file->lockToken == NULL);

   FileIOResolveLockBits(&access);
   ASSERT((access & FILEIO_OPEN_LOCKED) == 0);

#if !defined(__FreeBSD__) && !defined(sun)
   if ((access & FILEIO_OPEN_LOCK_MANDATORY) != 0) {
      /* Mandatory file locks are available only when opening a file */
      ret = FILEIO_LOCK_FAILED;
   } else if ((access & FILEIO_OPEN_LOCK_ADVISORY) != 0) {
      int err = 0;

      file->lockToken = FileLock_Lock(file->fileName,
                                      (access & FILEIO_OPEN_ACCESS_WRITE) == 0,
                                      FILELOCK_DEFAULT_WAIT,
                                      &err,
                                      NULL);

      if (file->lockToken == NULL) {
         /* Describe the lock not acquired situation in detail */
         Warning(LGPFX" %s on '%s' failed: %s\n",
                 __FUNCTION__, file->fileName,
                 (err == 0) ? "Lock timed out" : Err_Errno2String(err));

         /* Return a serious failure status if the locking code did */
         switch (err) {
         case 0:             // File is currently locked
         case EROFS:         // Attempt to lock for write on RO FS
            ret = FILEIO_LOCK_FAILED;
            break;
         case ENAMETOOLONG:  // Path is too long
            ret = FILEIO_FILE_NAME_TOO_LONG;
            break;
         case ENOENT:        // No such file or directory
            ret = FILEIO_FILE_NOT_FOUND;
            break;
         case EACCES:       // Permissions issues
            ret = FILEIO_NO_PERMISSION;
            break;
         default:            // Some sort of locking error
            ret = FILEIO_ERROR;
         }
      }
   }
#endif // !__FreeBSD__ && !sun

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_UnLock --
 *
 *      Call the FileLock module to unlock the given file.
 *
 * Results:
 *      FILEIO_SUCCESS  All is well
 *      FILEIO_ERROR    A serious error occured.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Unlock(FileIODescriptor *file)  // IN/OUT:
{
   FileIOResult ret = FILEIO_SUCCESS;

   ASSERT(file != NULL);

#if !defined(__FreeBSD__) && !defined(sun)
   if (file->lockToken != NULL) {
      int err = 0;

      if (!FileLock_Unlock(file->lockToken, &err, NULL)) {
         Warning(LGPFX" %s on '%s' failed: %s\n",
                 __FUNCTION__, file->fileName, Err_Errno2String(err));

         ret = FILEIO_ERROR;
      }

      file->lockToken = NULL;
   }
#else
   ASSERT(file->lockToken == NULL);
#endif // !__FreeBSD__ && !sun

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetSize --
 *
 *      Get size of file.
 *
 * Results:
 *      Size of file or -1.
 *
 * Side effects:
 *      errno is set on error.
 *
 *----------------------------------------------------------------------
 */

int64
FileIO_GetSize(const FileIODescriptor *fd)  // IN:
{
   int64 logicalBytes;

   return (FileIO_GetAllocSize(fd, &logicalBytes, NULL) != FILEIO_SUCCESS) ?
      -1 : logicalBytes;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetSizeByPath --
 *
 *      Get size of a file specified by path.
 *
 * Results:
 *      Size of file or -1.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int64
FileIO_GetSizeByPath(const char *pathName)  // IN:
{
   int64 logicalBytes;

   return (FileIO_GetAllocSizeByPath(pathName, &logicalBytes, NULL) !=
      FILEIO_SUCCESS) ? -1 : logicalBytes;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Filename --
 *
 *      Returns the filename that was used to open a FileIODescriptor
 *
 * Results:
 *      Filename. You DON'T own the memory - use Unicode_Duplicate if
 *      you want to keep it for yourself. In particular, if the file
 *      gets closed the string will almost certainly become invalid.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

const char *
FileIO_Filename(FileIODescriptor *fd)  // IN:
{
   ASSERT(fd != NULL);

   return fd->fileName;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_CloseAndUnlink
 *
 *      Closes and unlinks the file associated with a FileIODescriptor.
 *
 * Results:
 *      FILEIO_SUCCESS: The file was closed and unlinked. The FileIODescriptor
 *                      is no longer valid.
 *      FILEIO_ERROR: An error occurred.
 *
 * Side effects:
 *      File is probably closed and unlinked.
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_CloseAndUnlink(FileIODescriptor *fd)  // IN:
{
   char *path;
   FileIOResult ret;

   ASSERT(fd != NULL);
   ASSERT(FileIO_IsValid(fd));

   path = Unicode_Duplicate(fd->fileName);

   ret = FileIO_Close(fd);
   if ((File_UnlinkIfExists(path) == -1) && FileIO_IsSuccess(ret)) {
      ret = FILEIO_ERROR;
   }

   Posix_Free(path);

   return ret;
}


#if defined(_WIN32) || defined(__linux__) || defined(__APPLE__) || \
    defined(__FreeBSD__) || defined(sun)
/*
 *----------------------------------------------------------------------
 *
 * FileIO_Pread --
 *
 *      Reads from a file starting at a specified offset.
 *
 *      Note: This function may update the file pointer so you will need to
 *      call FileIO_Seek before calling FileIO_Read/Write afterwards.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Pread(FileIODescriptor *fd,  // IN: File descriptor
             void *buf,             // IN: Buffer to read into
             size_t len,            // IN: Length of the buffer
             uint64 offset)         // IN: Offset to start reading
{
   struct iovec iov;

   ASSERT(fd != NULL);

   iov.iov_base = buf;
   iov.iov_len = len;

   return FileIO_Preadv(fd, &iov, 1, offset, len, NULL);
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Pwrite --
 *
 *      Writes to a file starting at a specified offset.
 *
 *      Note: This function may update the file pointer so you will need to
 *      call FileIO_Seek before calling FileIO_Read/Write afterwards.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Pwrite(FileIODescriptor *fd,  // IN: File descriptor
              void const *buf,       // IN: Buffer to write from
              size_t len,            // IN: Length of the buffer
              uint64 offset)         // IN: Offset to start writing
{
   struct iovec iov;

   ASSERT(fd != NULL);

   /* The cast is safe because FileIO_Pwritev() will not write to '*buf'. */
   iov.iov_base = (void *)buf;
   iov.iov_len = len;

   return FileIO_Pwritev(fd, &iov, 1, offset, len, NULL);
}
#endif


#if defined(sun) && __GNUC__ < 3
/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_IsSuccess --
 *
 *      XXX: See comment in fileIO.h.  For reasonable compilers, this
 *      function is implemented as "static inline" in fileIO.h; for
 *      unreasonable compilers, it can't be static so we implement it here.
 *
 * Results:
 *      TRUE if the input indicates success.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileIO_IsSuccess(FileIOResult res)  // IN:
{
   return res == FILEIO_SUCCESS;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_AtomicTempPath
 *
 *      Return a temp path name in the same directory as the argument path.
 *      The path is the full path of the source file with a '~' appended.
 *      The caller must free the path when done.
 *
 * Results:
 *      UTF8 path if successful, NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
FileIO_AtomicTempPath(const char *path)  // IN:
{
   char *srcPath;
   char *retPath;

   srcPath = File_FullPath(path);
   if (srcPath == NULL) {
      Log("%s: File_FullPath of '%s' failed.\n", __FUNCTION__, path);
      return NULL;
   }
   retPath = Unicode_Join(srcPath, "~", NULL);
   Posix_Free(srcPath);

   return retPath;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_AtomicTempFile
 *
 *      Create a temp file in the same directory as the argument file.
 *      On non-Windows attempts to create the temp file with the same
 *      permissions and owner/group as the argument file.
 *
 * Results:
 *      FileIOResult of call that failed or FILEIO_SUCCESS
 *
 * Side effects:
 *      Creates a new file.
 *
 *-----------------------------------------------------------------------------
 */

FileIOResult
FileIO_AtomicTempFile(FileIODescriptor *fileFD,  // IN:
                      FileIODescriptor *tempFD)  // OUT:
{
   char *tempPath = NULL;
   int permissions;
   FileIOResult status;
#if !defined(_WIN32)
   int ret;
   struct stat stbuf;
#endif

   ASSERT(FileIO_IsValid(fileFD));
   ASSERT(tempFD && !FileIO_IsValid(tempFD));

   tempPath = FileIO_AtomicTempPath(FileIO_Filename(fileFD));
   if (tempPath == NULL) {
      status = FILEIO_ERROR;
      goto bail;
   }

#if defined(_WIN32)
   permissions = 0;
   File_UnlinkIfExists(tempPath);
#else
   if (fstat(fileFD->posix, &stbuf)) {
      Log("%s: Failed to fstat '%s', errno: %d.\n", __FUNCTION__,
          FileIO_Filename(fileFD), errno);
      status = FILEIO_ERROR;
      goto bail;
   }
   permissions = stbuf.st_mode;

   /* Clean up a previously created temp file; if one exists. */
   ret = Posix_Unlink(tempPath);
   if (ret != 0 && errno != ENOENT) {
      Log("%s: Failed to unlink temporary file, errno: %d\n",
          __FUNCTION__, errno);
      /* Fall through; FileIO_Create will report the actual error. */
   }
#endif

   status = FileIO_Create(tempFD, tempPath,
                          FILEIO_ACCESS_READ | FILEIO_ACCESS_WRITE,
                          FILEIO_OPEN_CREATE_SAFE, permissions);
   if (!FileIO_IsSuccess(status)) {
      Log("%s: Failed to create temporary file, %s (%d). errno: %d\n",
          __FUNCTION__, FileIO_ErrorEnglish(status), status, Err_Errno());
      goto bail;
   }

#if !defined(_WIN32)
   /*
    * On ESX we always use the vmkernel atomic file swap primitive, so
    * there's no need to set the permissions and owner of the temp file.
    *
    * XXX this comment is not true for NFS on ESX -- we use rename rather
    * than "vmkernel atomic file swap primitive" -- but we do not care
    * because files are always owned by root.  Sigh.  Bug 839283.
    */

   if (!HostType_OSIsVMK()) {
      if (fchmod(tempFD->posix, stbuf.st_mode)) {
         Log("%s: Failed to chmod temporary file, errno: %d\n",
             __FUNCTION__, errno);
         status = FILEIO_ERROR;
         goto bail;
      }
      if (fchown(tempFD->posix, stbuf.st_uid, stbuf.st_gid)) {
         Log("%s: Failed to chown temporary file, errno: %d\n",
             __FUNCTION__, errno);
         status = FILEIO_ERROR;
         goto bail;
      }
   }
#endif

   Posix_Free(tempPath);
   return FILEIO_SUCCESS;

bail:
   ASSERT(!FileIO_IsSuccess(status));
   if (FileIO_IsValid(tempFD)) {
      FileIO_Close(tempFD);
#if defined(_WIN32)
      File_UnlinkIfExists(tempPath);
#else
      ret = Posix_Unlink(tempPath);
      if (ret != 0) {
         Log("%s: Failed to clean up temporary file, errno: %d\n",
             __FUNCTION__, errno);
      }
      ASSERT(ret == 0);
#endif
   }
   Posix_Free(tempPath);
   return status;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_AtomicUpdateEx --
 *
 *      On ESX when the target files reside on vmfs, exchanges the contents
 *      of two files using code modeled from VmkfsLib_SwapFiles.  Both "curr"
 *      and "new" are left open.
 *
 *      On hosted products, uses rename to swap files, so "new" becomes "curr",
 *      and path to "new" no longer exists on success.
 *
 *      On ESX on NFS:
 *
 *      If renameOnNFS is TRUE, use rename, like on hosted.
 *
 *      If renameOnNFS is FALSE, returns -1 rather than trying to use
 *      rename, to avoid various bugs in the vmkernel client... (PR
 *      839283, PR 1671787, etc).
 *
 *      On success the caller must call FileIO_IsValid on newFD to verify it
 *      is still open before using it again.
 *
 * Results:
 *      1 if successful, 0 on failure, -1 if not supported on this filesystem.
 *      errno is preserved.
 *
 * Side effects:
 *      Disk I/O.
 *
 *-----------------------------------------------------------------------------
 */

int
FileIO_AtomicUpdateEx(FileIODescriptor *newFD,   // IN/OUT: file IO descriptor
                      FileIODescriptor *currFD,  // IN/OUT: file IO descriptor
                      Bool renameOnNFS)          // IN: fall back to rename on NFS
{
   char *currPath = NULL;
   char *newPath = NULL;
#if defined(_WIN32)
   uint32 currAccess;
   uint32 newAccess;
   FileIOResult status;
   FileIODescriptor tmpFD;
#else
   int fd;
#endif
   int savedErrno = 0;
   int ret = 0;

   ASSERT(FileIO_IsValid(newFD));
   ASSERT(FileIO_IsValid(currFD));

   if (HostType_OSIsVMK()) {
#if defined(VMX86_SERVER)
      FS_SwapFilesArgsUW args = { 0 };
      char *dirName = NULL;
      char *fileName = NULL;
      char *dstDirName = NULL;
      char *dstFileName = NULL;
      Bool isSame;

      currPath = File_FullPath(FileIO_Filename(currFD));
      if (currPath == NULL) {
         savedErrno = errno;
         Log("%s: File_FullPath of '%s' failed.\n", __FUNCTION__,
             FileIO_Filename(currFD));
         goto swapdone;
      }

      newPath = File_FullPath(FileIO_Filename(newFD));
      if (newPath == NULL) {
         savedErrno = errno;
         Log("%s: File_FullPath of '%s' failed.\n", __FUNCTION__,
             FileIO_Filename(newFD));
         goto swapdone;
      }

      File_GetPathName(newPath, &dirName, &fileName);
      File_GetPathName(currPath, &dstDirName, &dstFileName);

      ASSERT(dirName != NULL);
      ASSERT(fileName != NULL && *fileName != '\0');
      ASSERT(dstDirName != NULL);
      ASSERT(dstFileName != NULL && *dstFileName != '\0');

      errno = 0;
      isSame = File_IsSameFile(dirName, dstDirName);
      if (errno == 0) {
         ASSERT(isSame);
      } else {
         savedErrno = errno;
         Log("%s: File_IsSameFile of ('%s', '%s') failed: %d\n", __FUNCTION__,
             dirName, dstDirName, errno);
         goto swapdone;
      }

      args.fd = currFD->posix;
      if (ioctl(newFD->posix, IOCTLCMD_VMFS_SWAP_FILES, &args) != 0) {
         savedErrno = errno;
         if (errno != ENOSYS && errno != ENOTTY) {
            Log("%s: ioctl failed %d.\n", __FUNCTION__, errno);
            ASSERT(errno != EBUSY);   /* #615124. */
         }
      } else {
         ret = 1;
      }

      /*
       * Did we fail because we are on a file system that does not
       * support the IOCTLCMD_VMFS_SWAP_FILES ioctl? If so fallback to
       * using rename.
       *
       * Check for both ENOSYS and ENOTTY. PR 957695
       */
      if (savedErrno == ENOSYS || savedErrno == ENOTTY) {
         if (renameOnNFS) {
            /*
             * NFS allows renames of locked files, even if both files
             * are locked.  The file lock follows the file handle, not
             * the name, so after the rename we can swap the underlying
             * file descriptors instead of closing and reopening the
             * target file.
             *
             * This is different than the hosted path below because
             * ESX uses native file locks and hosted does not.
             *
             * We assume that all ESX file systems that support rename
             * have the same file lock semantics as NFS.
             */

            if (File_Rename(newPath, currPath)) {
               Log("%s: rename of '%s' to '%s' failed %d.\n",
                   __FUNCTION__, newPath, currPath, errno);
               savedErrno = errno;
               goto swapdone;
            }
            ret = 1;
            fd = newFD->posix;
            newFD->posix = currFD->posix;
            currFD->posix = fd;
            FileIO_Close(newFD);
         } else {
            ret = -1;
         }
      }

swapdone:
      Posix_Free(dirName);
      Posix_Free(fileName);
      Posix_Free(dstDirName);
      Posix_Free(dstFileName);
      Posix_Free(currPath);
      Posix_Free(newPath);

      errno = savedErrno;
      return ret;
#else
      NOT_REACHED();
#endif
   }
#if defined(_WIN32)
   currPath = Unicode_Duplicate(FileIO_Filename(currFD));
   newPath = Unicode_Duplicate(FileIO_Filename(newFD));

   newAccess = newFD->flags;
   currAccess = currFD->flags;

   FileIO_Close(newFD);

   /*
    * The current file needs to be closed and reopened,
    * but we don't want to drop the file lock by calling
    * FileIO_Close() on it.  Instead, use native close primitives.
    * We'll reopen it later with FileIO_Open.  Set the
    * descriptor/handle to an invalid value while we're in the
    * middle of transferring ownership.
    */

   CloseHandle(currFD->win32);
   currFD->win32 = INVALID_HANDLE_VALUE;
   if (File_RenameRetry(newPath, currPath, 10) == 0) {
      ret = TRUE;
   } else {
      savedErrno = errno;
      ASSERT(!ret);
   }

   FileIO_Invalidate(&tmpFD);

   /*
    * Clear the locking bits from the requested access so that reopening
    * the file ignores the advisory lock.
    */

   ASSERT((currAccess & FILEIO_OPEN_LOCK_MANDATORY) == 0);
   currAccess &= ~(FILEIO_OPEN_LOCK_MANDATORY | FILEIO_OPEN_LOCK_ADVISORY |
                   FILEIO_OPEN_LOCK_BEST | FILEIO_OPEN_LOCKED);
   status = FileIO_Open(&tmpFD, currPath, currAccess, FILEIO_OPEN);
   if (!FileIO_IsSuccess(status)) {
      Panic("Failed to reopen dictionary after renaming "
            "\"%s\" to \"%s\": %s (%d)\n", newPath, currPath,
            FileIO_ErrorEnglish(status), status);
   }
   ASSERT(tmpFD.lockToken == NULL);

   currFD->win32 = tmpFD.win32;

   FileIO_Cleanup(&tmpFD);
   Posix_Free(currPath);
   Posix_Free(newPath);
   errno = savedErrno;

   return ret;
#else
   currPath = (char *)FileIO_Filename(currFD);
   newPath = (char *)FileIO_Filename(newFD);

   if (File_Rename(newPath, currPath)) {
      Log("%s: rename of '%s' to '%s' failed %d.\n",
          __FUNCTION__, newPath, currPath, errno);
          savedErrno = errno;
   } else {
      ret = TRUE;
      fd = newFD->posix;
      newFD->posix = currFD->posix;
      currFD->posix = fd;
      FileIO_Close(newFD);
   }

   errno = savedErrno;

   return ret;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_AtomicUpdate --
 *
 *      Wrapper around FileIO_AtomicUpdateEx that defaults 'renameOnNFS' to
 *      TRUE.
 *
 * Results:
 *      TRUE if successful, FALSE otherwise.
 *
 * Side effects:
 *      See FileIO_AtomicUpdateEx.
 *
 *-----------------------------------------------------------------------------
 */

Bool
FileIO_AtomicUpdate(FileIODescriptor *newFD,   // IN/OUT: file IO descriptor
                    FileIODescriptor *currFD)  // IN/OUT: file IO descriptor
{
   return FileIO_AtomicUpdateEx(newFD, currFD, TRUE) == 1;
}
