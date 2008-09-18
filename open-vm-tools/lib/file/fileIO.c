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
FileIO_ErrorEnglish(FileIOResult status) // IN
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
FileIO_MsgError(FileIOResult status) // IN
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
      result = MSGID(fileio.generic) "Generic error";
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
      result = MSGID(fileio.noPerm) "Insufficient permissions to access the file";
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
      Warning("FileIO_MsgError was passed bad code %d\n", status);
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
FileIO_Init(FileIODescriptor *fd,   // IN/OUT:
            ConstUnicode pathName)  // IN:
{
   ASSERT(fd);
   ASSERT(pathName);

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
   ASSERT(fd);

   if (fd->fileName) {
      Unicode_Free(fd->fileName);
      fd->fileName = NULL;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Lock --
 *
 *      Call the FileLock module to lock the given file.
 *
 * Results:
 *      FILEIO_SUCCESS      All is well
 *      FILEIO_LOCK_FAILED  Requested lock on file was not acquired
 *      FILEIO_ERROR        A serious error occured.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Lock(FileIODescriptor *file, // IN/OUT:
            int access)             // IN:
{
   FileIOResult ret = FILEIO_SUCCESS;

   /*
    * Lock the file if necessary.
    */

   ASSERT(file);

#if !defined(__FreeBSD__) && !defined(sun)
   if (access & FILEIO_OPEN_LOCKED) {
      int err;

      ASSERT(file->lockToken == NULL);

      file->lockToken = FileLock_Lock(file->fileName,
                                      (access & FILEIO_OPEN_ACCESS_WRITE) == 0,
                                      FILELOCK_DEFAULT_WAIT,
                                      &err);

      if (file->lockToken == NULL) {
         /* Describe the lock not acquired situation in detail */
         Warning(LGPFX" %s on '%s' failed: %s\n",
                 __FUNCTION__, UTF8(file->fileName),
                 (err == 0) ? "Lock timed out" : strerror(err));

         /* Return a serious failure status if the locking code did */
         switch (err) {
         case 0:             // file is currently locked
            ret = FILEIO_LOCK_FAILED;
            break;
         case ENAMETOOLONG:  // path is too long
            ret = FILEIO_FILE_NAME_TOO_LONG;
            break;
         default:            // some sort of locking error
            ret = FILEIO_ERROR;
         }
      }
   }
#else
   ASSERT(file->lockToken == NULL);
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
FileIO_Unlock(FileIODescriptor *file)     // IN/OUT:
{
   FileIOResult ret = FILEIO_SUCCESS;

   ASSERT(file);

#if !defined(__FreeBSD__) && !defined(sun)
   if (file->lockToken != NULL) {
      int err;

      err = FileLock_Unlock(file->fileName, file->lockToken);

      if (err != 0) {
         Warning(LGPFX" %s on '%s' failed: %s\n",
                 __FUNCTION__, UTF8(file->fileName), strerror(err));

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
 * FileIO_StatsInit --
 *
 *      Initialize the stat structure in the FileIODescriptor.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
FileIO_StatsInit(FileIODescriptor *fd)  // IN:
{
   /* zero out the stat counters */

   ASSERT(fd);

#if defined(VMX86_STATS)
   fd->readIn = 0; fd->writeIn = 0;
   fd->readvIn = 0; fd->writevIn = 0;
   fd->preadvIn = 0; fd->pwritevIn = 0;
   fd->readDirect = 0; fd->writeDirect = 0;
   fd->readvDirect = 0; fd->writevDirect = 0;
   fd->preadDirect = 0; fd->pwriteDirect = 0;
   fd->bytesRead = 0; fd->bytesWritten = 0;
   fd->numReadCoalesced = 0; fd->numWriteCoalesced = 0;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_StatsLog --
 *
 *      Dump statistics about file access in the log file.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      Logs.
 *
 *----------------------------------------------------------------------
 */

void
FileIO_StatsLog(FileIODescriptor *fd)  // IN:
{
   ASSERT(fd);

#if defined(VMX86_STATS)
   if (fd->bytesRead + fd->bytesWritten == 0) {
      /* No activity --> no interesting stats */
      return;
   }

   if (fd->readIn + fd->writeIn + fd->readvIn + fd->writevIn +
       fd->preadvIn + fd->pwritevIn < 100) {
      /*
       * Less than 100 operations is insufficient to be interesting and this
       * way we don't get spew everytime a file (generally a disk) is opened
       * temporarily.
       */
      return;
   }

   Log("FILEIOSTATS | \"%s\" %d %d %d %d %d %d %d %d %d %d %d %d %d %d %"FMT64"d %"FMT64"d\n",
       fd->fileName ? UTF8(fd->fileName) : "",
       fd->readIn, fd->readDirect, fd->writeIn, fd->writeDirect,
       fd->readvIn, fd->readvDirect, fd->writevIn, fd->writevDirect,
       fd->preadvIn, fd->preadDirect, fd->pwritevIn, fd->pwriteDirect,
       fd->numReadCoalesced, fd->numWriteCoalesced,
       fd->bytesRead, fd->bytesWritten);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_StatsExit --
 *
 *      Release resources allocated for statistics.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

void
FileIO_StatsExit(const FileIODescriptor *fd)  // IN:
{
   ASSERT(fd);
}


#if defined(_WIN32) || defined(GLIBC_VERSION_21) || defined(__APPLE__) || \
    defined(__FreeBSD__)
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
FileIO_Pread(FileIODescriptor *fd,    // IN: File descriptor
             void *buf,               // IN: Buffer to read into
             size_t len,              // IN: Length of the buffer
             uint64 offset)           // IN: Offset to start reading
{
   struct iovec iov;

   ASSERT(fd);

   iov.iov_base = buf;
   iov.iov_len = len;

   return FileIO_Preadv(fd, &iov, 1, offset, len);
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
FileIO_Pwrite(FileIODescriptor *fd,   // IN: File descriptor
              void const *buf,        // IN: Buffer to write from
              size_t len,             // IN: Length of the buffer
              uint64 offset)          // IN: Offset to start writing
{
   struct iovec iov;

   ASSERT(fd);

   /* The cast is safe because FileIO_Pwritev() will not write to '*buf'. */
   iov.iov_base = (void *)buf;
   iov.iov_len = len;

   return FileIO_Pwritev(fd, &iov, 1, offset, len);
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
