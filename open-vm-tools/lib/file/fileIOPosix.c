/*********************************************************
 * Copyright (C) 1998-2021 VMware, Inc. All rights reserved.
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
 * fileIOPosix.c --
 *
 *      Implementation of the file library host specific functions for linux.
 */


#if defined(__linux__)
#  if !defined(VMX86_TOOLS) && !defined(__ANDROID__)
#     define FILEIO_SUPPORT_ODIRECT
#     define _GNU_SOURCE
#  endif
#  include <features.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#if defined __ANDROID__
#include <asm/unistd.h> // for __NR_SYSCALL_BASE
#endif
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#if defined(__linux__)
#ifdef __ANDROID__
#   include <sys/syscall.h>
#else
#   include <syscall.h>
#endif
#endif
#include <sys/stat.h>
#include "su.h"

#if defined(__APPLE__)
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/kauth.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <CoreFoundation/CoreFoundation.h>
#include <dlfcn.h>
#include <sys/xattr.h>
#else
#if defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#else
#include <sys/statfs.h>
#if !defined(__sun__)
#include <mntent.h>
#include <dlfcn.h>
#endif
#endif
#endif

/* Check for non-matching prototypes */
#include "vmware.h"
#include "str.h"
#include "err.h"
#include "posix.h"
#include "file.h"
#include "fileIO.h"
#include "fileInt.h"
#include "config.h"
#include "util.h"
#include "iovector.h"
#include "hostType.h"

#include "unicodeOperations.h"
#include "memaligned.h"
#include "userlock.h"

#include "hostinfo.h"

#if defined(__APPLE__)
#include <sys/sysctl.h>
#include <sys/fcntl.h>
#endif

/*
 * fallocate() is only supported since the glibc-2.8 and
 * linux kernel-2.6.23. Presently the glibc in our toolchain is 2.3.
 */
#if defined(__linux__)
   #if !defined(SYS_fallocate)
      #if defined(__i386__)
         #define SYS_fallocate 324
      #elif __x86_64__
         #define SYS_fallocate 285
      #elif __arm__
         #define SYS_fallocate (__NR_SYSCALL_BASE+352) // newer glibc value
      #endif
   #endif
   #if !defined(FALLOC_FL_KEEP_SIZE)
      #define FALLOC_FL_KEEP_SIZE 1
   #endif
#endif

static const unsigned int FileIO_SeekOrigins[] = {
   SEEK_SET,
   SEEK_CUR,
   SEEK_END,
};

static const int FileIO_OpenActions[] = {
   0,
   O_TRUNC,
   O_CREAT,
   O_CREAT | O_EXCL,
   O_CREAT | O_TRUNC,
};

#ifdef __APPLE__
static FileIOPrivilegedOpener *privilegedOpenerFunc = NULL;
#endif

/*
 * Options for FileCoalescing performance optimization
 */
typedef struct FilePosixOptions {
   Bool initialized;
   Bool aligned;
   Bool enabled;
   int countThreshold;
   int sizeThreshold;
   int aioNumThreads;
   ssize_t maxIOVec;
} FilePosixOptions;


static FilePosixOptions filePosixOptions;

/*
 * Data structures for FileIOAligned_* functions; only used on
 * hosted (see fileInt.h for rationale).
 */
#if !defined(VMX86_TOOLS) && !defined(VMX86_SERVER)
#define ALIGNEDPOOL_FREELIST_SIZE 30
#define ALIGNEDPOOL_BUFSZ         (1024 * 1024)
#define ALIGNEDPOOL_OLD_AGE       ((VmTimeType)1000 * 1000 * 1000) /* nanoseconds */

typedef struct AlignedPool {
   MXUserExclLock *lock;

   /*
    * list: Array of allocated buffers.
    *        0 .. numBusy-1 : busy buffers (in use by a caller).
    * numBusy .. numAlloc-1 : allocated but not busy.
    *    numAlloc .. SIZE-1 : unused.
    */
   void           *list[ALIGNEDPOOL_FREELIST_SIZE];

   /*
    * timestamp: Array of release timestamps.
    *        0 .. numBusy-1 : unused.
    * numBusy .. numAlloc-1 : last time we had N buffers outstanding.
    *    numAlloc .. SIZE-1 : unused.
    */
   VmTimeType      timestamp[ALIGNEDPOOL_FREELIST_SIZE];

   /* invariant: 0 <= numBusy <= numAlloc <= ALIGNEDPOOL_FREELIST_SIZE */
   unsigned        numAlloc;
   unsigned        numBusy;
} AlignedPool;

static Atomic_Ptr alignedPoolLockStorage;
static AlignedPool alignedPool;
#endif

/*
 * Although, support for preadv()/pwrite() first appeared in Linux 2.6.30,
 * library support was added in glibc 2.10. Hence these functions
 * are not available in any header file.
 */

#if defined(__linux__) && !defined(__ANDROID__)
   #if defined(_FILE_OFFSET_BITS) && (_FILE_OFFSET_BITS == 64)
      /*
       * We want preadv/pwritev. But due to FOB=64, the symbols are -64.
       * TODO: when the baseline bumps to XOPEN=700, link directly to
       * the symbols (and anyone building XOPEN<700 gets nothing).
       */
      extern ssize_t preadv64(int fd, const struct iovec *iov, int iovcnt,
                          __off64_t offset) __attribute__ ((weak));

      extern ssize_t pwritev64(int fd, const struct iovec *iov, int iovcnt,
                          __off64_t offset) __attribute__ ((weak));
   #else
      #error "Large file support is unavailable."
   #endif
#endif /* defined(__linux__) */

/*
 *-----------------------------------------------------------------------------
 *
 * FileIOErrno2Result --
 *
 *      Convert a POSIX errno to a FileIOResult code.
 *
 * Results:
 *      The FileIOResult corresponding to the errno, FILEIO_ERROR by default.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static FileIOResult
FileIOErrno2Result(int error)  // IN: errno to convert
{
   switch (error) {
   case EIO:
      return FILEIO_ERROR;
   case EEXIST:
      return FILEIO_OPEN_ERROR_EXIST;
   case ENOENT:
      return FILEIO_FILE_NOT_FOUND;
   case EACCES:
      return FILEIO_NO_PERMISSION;
   case ENAMETOOLONG:
      return FILEIO_FILE_NAME_TOO_LONG;
   case ENOSPC:
      return FILEIO_WRITE_ERROR_NOSPC;
   case EFBIG:
      return FILEIO_WRITE_ERROR_FBIG;
#if defined(VMX86_SERVER)
   case EBUSY:
      return FILEIO_LOCK_FAILED;
#endif
#if defined(EDQUOT)
   case EDQUOT:
      return FILEIO_WRITE_ERROR_DQUOT;
#endif
   default:
      return FILEIO_ERROR;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_OptionalSafeInitialize --
 *
 *      Initialize global state. If this module is called from a
 *      thread other than the VMX or VCPU threads, like an aioGeneric worker
 *      thread, then we cannot do things like call config. Do that sort
 *      of initialization here, which is called from a safe thread.
 *
 *      This routine is OPTIONAL if you do not call this module from a
 *      worker thread. The same initialization can be done lazily when
 *      a read/write routine is called.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

INLINE void
FileIO_OptionalSafeInitialize(void)
{
   if (!filePosixOptions.initialized) {
      filePosixOptions.enabled =
                            Config_GetBool(TRUE, "filePosix.coalesce.enable");

      /*
       * Aligned malloc starts failing to allocate memory during heavy I/O on
       * Linux. We're not sure why -- maybe we are running out of mmaps?
       * Turn it off by default for now.
       */

      filePosixOptions.aligned =
                           Config_GetBool(FALSE, "filePosix.coalesce.aligned");

      filePosixOptions.countThreshold =
                           Config_GetLong(5, "filePosix.coalesce.count");

      filePosixOptions.sizeThreshold =
                           Config_GetLong(16*1024, "filePosix.coalesce.size");

      filePosixOptions.aioNumThreads =
                           Config_GetLong(0, "aiomgr.numThreads");
#if defined(__linux__)
      filePosixOptions.maxIOVec = sysconf(_SC_IOV_MAX);

      /* Assume unlimited unless sysconf says otherwise. */
      if (filePosixOptions.maxIOVec < 0) {
         filePosixOptions.maxIOVec = MAX_INT32;
      }
#elif defined(__APPLE__)
      /*
       * There appears to be no way to determine the iovec size limit at
       * runtime.  If Apple ever changes this, we lose binary compatibility.
       * On the bright side, Apple has not changed this value for at least as
       * long as they've produced Intel Macs.
       */

      filePosixOptions.maxIOVec = 1024;
#else
      filePosixOptions.maxIOVec = MAX_INT32;
#endif

      filePosixOptions.initialized = TRUE;
      FileIOAligned_PoolInit();
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Invalidate --
 *
 *      Initialize a FileIODescriptor with an invalid value
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
FileIO_Invalidate(FileIODescriptor *fd)  // OUT:
{
   ASSERT(fd != NULL);

   (memset)(fd, 0, sizeof *fd);
   fd->posix = -1;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_IsValid --
 *
 *      Check whether a FileIODescriptor is valid.
 *
 * Results:
 *      True if valid.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
FileIO_IsValid(const FileIODescriptor *fd)  // IN:
{
   ASSERT(fd != NULL);

   return fd->posix != -1;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_CreateFDPosix --
 *
 *      This function is for specific needs: for example, when you need
 *      to create a FileIODescriptor from an already open fd. Use only
 *      FileIO_* library functions on the FileIODescriptor from that point on.
 *
 *      Because FileIODescriptor struct is different on two platforms,
 *      this function is the only one in the file library that's
 *      platform-specific.
 *
 * Results:
 *      FileIODescriptor
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIODescriptor
FileIO_CreateFDPosix(int posix,  // IN: UNIX file descriptor
                     int flags)  // IN: UNIX access flags
{
   FileIODescriptor fd;

   FileIO_Invalidate(&fd);

   switch (flags & O_ACCMODE) {
   case O_RDWR:
      fd.flags |= (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE);
      break;
   case O_WRONLY:
      fd.flags |= FILEIO_OPEN_ACCESS_WRITE;
      break;
   default:
      ASSERT(FALSE);
      /* FALLTHRU */
   case O_RDONLY:
      fd.flags |= FILEIO_OPEN_ACCESS_READ;
      break;
   }

#if defined(O_SYNC) // Not available in FreeBSD tools build
   if (flags & O_SYNC) {
      fd.flags |= FILEIO_OPEN_SYNC;
   }
#endif
   if (flags & O_APPEND) {
      fd.flags |= FILEIO_OPEN_APPEND;
   }

#if defined(__linux__) && defined(O_CLOEXEC)
   if (flags & O_CLOEXEC) {
      fd.flags |= FILEIO_OPEN_CLOSE_ON_EXEC;
   }
#endif

   fd.posix = posix;

   return fd;
}


#if defined(__APPLE__)
/*
 *----------------------------------------------------------------------
 *
 * FileIO_SetPrivilegedOpener --
 *
 *      Set the function to be used when opening files with privilege,
 *      overriding the default behavior. See FileIO_PrivilegedPosixOpen.
 *
 *      Setting the privileged opener to NULL will restore default
 *      behavior.
 *
 *      This function is not thread safe.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

void
FileIO_SetPrivilegedOpener(FileIOPrivilegedOpener *opener) // IN
{
   ASSERT(privilegedOpenerFunc == NULL || opener == NULL);
   privilegedOpenerFunc = opener;
}
#endif


/*
 *----------------------------------------------------------------------
 *
 * FileIOCreateRetry --
 *
 *      Open/create a file; specify creation mode
 *      May perform retries to deal with certain OS conditions
 *
 * Results:
 *      FILEIO_SUCCESS on success: 'file' is set
 *      FILEIO_OPEN_ERROR_EXIST if the file already exists
 *      FILEIO_FILE_NOT_FOUND if the file is not present
 *      FILEIO_ERROR for other errors
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIOCreateRetry(FileIODescriptor *file,   // OUT:
                  const char *pathName,     // IN:
                  int access,               // IN:
                  FileIOOpenAction action,  // IN:
                  int mode,                 // IN: mode_t for creation
                  uint32 maxWaitTimeMsec)   // IN: Ignored
{
   int fd = -1;
   int flags = 0;
   int error;
   FileIOResult ret;

   ASSERT(file != NULL);

   if (pathName == NULL) {
      errno = EFAULT;

      return FILEIO_ERROR;
   }

   ASSERT(!FileIO_IsValid(file));
   ASSERT(file->lockToken == NULL);
   ASSERT_ON_COMPILE(FILEIO_ERROR_LAST < 16); /* See comment in fileIO.h */

   FileIOResolveLockBits(&access);
   ASSERT((access & FILEIO_OPEN_LOCKED) == 0 &&
          (access & FILEIO_OPEN_EXCLUSIVE_LOCK) == 0);
   /* Only ESX implements mandatory locking */
   ASSERT((access & FILEIO_OPEN_LOCK_MANDATORY) == 0 ||
          File_SupportsMandatoryLock(pathName));

#if defined(__APPLE__)
   if (access & FILEIO_OPEN_EXCLUSIVE_LOCK_MACOS) {
      flags |= O_EXLOCK;
   }
#elif defined(__linux__)
   if (HostType_OSIsVMK()) {
      if ((access & FILEIO_OPEN_SWMR_LOCK) != 0) {
         flags |= O_SWMR_LOCK;
      } else if ((access & FILEIO_OPEN_MULTIWRITER_LOCK) != 0) {
         flags |= O_MULTIWRITER_LOCK;
      } else if ((access & FILEIO_OPEN_LOCK_MANDATORY) != 0) {
         flags |= O_EXCLUSIVE_LOCK;
      } else if ((access & FILEIO_OPEN_OPTIMISTIC_LOCK) != 0) {
         flags |= O_OPTIMISTIC_LOCK;
      }
   }
#endif

   /*
    * Locking implementation note: this can be recursive. On ESX:
    * FileIOCreateRetry("foo", ...ADVISORY...)
    *  -> FileIO_Lock("foo", ...ADVISORY...)
    *     -> FileLock_Lock("foo", ...ADVISORY...)
    *        -> FileIOCreateRetry("foo.lck", ...MANDATORY...)
    *           -> open("foo.lck", ...O_EXCLUSIVE_LOCK...)
    */

   FileIO_Init(file, pathName);
   /* Mandatory file locks are only available at open() itself */
   if ((access & FILEIO_OPEN_LOCK_ADVISORY) != 0) {
      ret = FileIO_Lock(file, access);
      if (!FileIO_IsSuccess(ret)) {
         goto error;
      }
   }

   if ((access & (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE)) ==
       (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE)) {
      flags |= O_RDWR;
   } else if (access & FILEIO_OPEN_ACCESS_WRITE) {
      flags |= O_WRONLY;
   } else if (access & FILEIO_OPEN_ACCESS_READ) {
      flags |= O_RDONLY;
   }

   if (access & FILEIO_OPEN_EXCLUSIVE_READ &&
       access & FILEIO_OPEN_EXCLUSIVE_WRITE) {
      flags |= O_EXCL;
   }

   if (access & FILEIO_OPEN_UNBUFFERED) {
#if defined(FILEIO_SUPPORT_ODIRECT)
      flags |= O_DIRECT;
#elif !defined(__APPLE__) // Mac hosts need this access flag after opening.
      access &= ~FILEIO_OPEN_UNBUFFERED;
      LOG_ONCE(LGPFX" %s reverting to buffered IO on %s.\n",
               __FUNCTION__, pathName);
#endif
   }

   if (access & FILEIO_OPEN_NONBLOCK) {
      flags |= O_NONBLOCK;
   }

   if (access & FILEIO_OPEN_APPEND) {
      flags |= O_APPEND;
   }

#if defined(O_NOFOLLOW)
   if (access & FILEIO_OPEN_ACCESS_NOFOLLOW) {
      flags |= O_NOFOLLOW;
   }
#endif

#if defined(__linux__)
   if (access & FILEIO_OPEN_SYNC) {
      flags |= O_SYNC;
   }
#endif

#if defined(O_NOFOLLOW)
   if (access & FILEIO_OPEN_ACCESS_NOFOLLOW) {
      flags |= O_NOFOLLOW;
   }
#endif

#if defined(__linux__) && defined(O_CLOEXEC)
   if (access & FILEIO_OPEN_CLOSE_ON_EXEC) {
      flags |= O_CLOEXEC;
   }
#endif

   flags |= FileIO_OpenActions[action];

   file->flags = access;

#if defined(__APPLE__)
   if (access & FILEIO_OPEN_PRIVILEGED) {
      // We only support privileged opens, not creates or truncations.
      if ((flags & (O_CREAT | O_TRUNC)) != 0) {
         fd = -1;
         errno = EACCES;
      } else {
         fd = FileIO_PrivilegedPosixOpen(pathName, flags);
      }
   } else {
      fd = Posix_Open(pathName, flags, mode);
   }
#else
   {
      uid_t uid = -1;

      if (access & FILEIO_OPEN_PRIVILEGED) {
         uid = Id_BeginSuperUser();
      }

      fd = Posix_Open(pathName, flags, mode);

      error = errno;

      if (access & FILEIO_OPEN_PRIVILEGED) {
         Id_EndSuperUser(uid);
      }

      errno = error;
   }
#endif

   if (fd == -1) {
      ret = FileIOErrno2Result(errno);
      if (ret == FILEIO_ERROR) {
         Log(LGPFX "open error on %s: %s\n", pathName,
             Err_Errno2String(errno));
      }
      goto error;
   }

#if defined(__APPLE__)
   if (access & (FILEIO_OPEN_UNBUFFERED | FILEIO_OPEN_SYNC)) {
      error = fcntl(fd, F_NOCACHE, 1);
      if (error == -1) {
         ret = FileIOErrno2Result(errno);
         if (ret == FILEIO_ERROR) {
            Log(LGPFX "fcntl error on %s: %s\n", pathName,
                Err_Errno2String(errno));
         }
         goto error;
      }

      if (!(access & FILEIO_OPEN_SYNC)) {
         error = fcntl(fd, F_NODIRECT, 1);
         if (error == -1) {
            ret = FileIOErrno2Result(errno);
            if (ret == FILEIO_ERROR) {
               Log(LGPFX "fcntl error on %s: %s\n", pathName,
                   Err_Errno2String(errno));
            }
            goto error;
         }
      }
   }
#endif

   if (access & FILEIO_OPEN_DELETE_ASAP) {
      /*
       * Remove the name from the name space. The file remains laid out on the
       * disk and accessible through the file descriptor until it is closed.
       */

      if (Posix_Unlink(pathName) == -1) {
         ret = FileIOErrno2Result(errno);
         if (ret == FILEIO_ERROR) {
            Log(LGPFX "unlink error on %s: %s\n", pathName,
                Err_Errno2String(errno));
         }
         goto error;
      }
   }

   file->posix = fd;

   return FILEIO_SUCCESS;

error:
   error = errno;

   if (fd != -1) {
      close(fd);
   }
   FileIO_Unlock(file);
   FileIO_Cleanup(file);
   FileIO_Invalidate(file);
   errno = error;

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_CreateRetry --
 *
 *      Open/create a file; specify creation mode
 *
 * Results:
 *      FILEIO_SUCCESS on success: 'file' is set
 *      FILEIO_OPEN_ERROR_EXIST if the file already exists
 *      FILEIO_FILE_NOT_FOUND if the file is not present
 *      FILEIO_ERROR for other errors
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_CreateRetry(FileIODescriptor *file,   // OUT:
                   const char *pathName,     // IN:
                   int access,               // IN:
                   FileIOOpenAction action,  // IN:
                   int mode,                 // IN: mode_t for creation
                   uint32 maxWaitTimeMsec)   // IN:
{
   return FileIOCreateRetry(file, pathName, access, action, mode,
                            maxWaitTimeMsec);
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Create --
 *
 *      Open/create a file; specify creation mode
 *
 * Results:
 *      FILEIO_SUCCESS on success: 'file' is set
 *      FILEIO_OPEN_ERROR_EXIST if the file already exists
 *      FILEIO_FILE_NOT_FOUND if the file is not present
 *      FILEIO_ERROR for other errors
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Create(FileIODescriptor *file,   // OUT:
              const char *pathName,     // IN:
              int access,               // IN:
              FileIOOpenAction action,  // IN:
              int mode)                 // IN: mode_t for creation
{
   return FileIOCreateRetry(file, pathName, access, action, mode, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_OpenRetry --
 *
 *      Open/create a file.
 *      May perform retries to deal with certain OS conditions.
 *
 * Results:
 *      FILEIO_SUCCESS on success: 'file' is set
 *      FILEIO_OPEN_ERROR_EXIST if the file already exists
 *      FILEIO_FILE_NOT_FOUND if the file is not present
 *      FILEIO_ERROR for other errors
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_OpenRetry(FileIODescriptor *file,   // OUT:
                 const char *pathName,     // IN:
                 int access,               // IN:
                 FileIOOpenAction action,  // IN:
                 uint32 maxWaitTimeMsec)   // IN:
{
#if defined(VMX86_SERVER)
   FileIOResult res;
   uint32 waitTimeMsec = 0;
   uint32 maxLoopTimeMsec = 3000;  // 3 seconds

   /*
    * Workaround the ESX NFS client bug as seen in PR 1341775.
    * Since ESX NFS client can sometimes *wrongly* return ESTALE for a
    * legitimate file open case, we retry for some time in hopes that the
    * problem will resolve itself.
    */

   while (TRUE) {
      res = FileIOCreateRetry(file, pathName, access, action,
                              S_IRUSR | S_IWUSR, maxWaitTimeMsec);

      if (res == FILEIO_ERROR && Err_Errno() == ESTALE &&
          waitTimeMsec < maxLoopTimeMsec) {
         Log(LGPFX "FileIOCreateRetry (%s) failed with ESTALE, retrying.\n",
             pathName);

         waitTimeMsec += FileSleeper(100, 300);
      } else {
         break;
      }
   }

   return res;
#else
   return FileIOCreateRetry(file, pathName, access, action,
                            S_IRUSR | S_IWUSR, maxWaitTimeMsec);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Open --
 *
 *      Open/create a file.
 *
 * Results:
 *      FILEIO_SUCCESS on success: 'file' is set
 *      FILEIO_OPEN_ERROR_EXIST if the file already exists
 *      FILEIO_FILE_NOT_FOUND if the file is not present
 *      FILEIO_ERROR for other errors
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Open(FileIODescriptor *file,   // OUT:
            const char *pathName,     // IN:
            int access,               // IN:
            FileIOOpenAction action)  // IN:
{
   return FileIO_OpenRetry(file, pathName, access, action, 0);
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Seek --
 *
 *      Change the current position in a file
 *
 * Results:
 *      On success: the new current position in bytes from the beginning of
 *                  the file
 *
 *      On failure: -1
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint64
FileIO_Seek(const FileIODescriptor *file,  // IN:
            int64 distance,                // IN:
            FileIOSeekOrigin origin)       // IN:
{
   ASSERT(file != NULL);

#if defined(__ANDROID__)
   /*
    * Android doesn't implement _FILE_OFFSET_BITS=64, but always has lseek64.
    */
   return lseek64(file->posix, distance, FileIO_SeekOrigins[origin]);
#else
   /*
    * Require 64-bit file API support via _FILE_OFFSET_BITS=64 or
    * operating system default.
    */
   ASSERT_ON_COMPILE(sizeof(off_t) == 8);

   return lseek(file->posix, distance, FileIO_SeekOrigins[origin]);
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Write --
 *
 *      Write to a file
 *
 * Results:
 *      FILEIO_SUCCESS on success: '*actual_count' = 'requested' bytes have
 *       been written.
 *      FILEIO_WRITE_ERROR_FBIG for the attempt to write file that exceeds
 *       maximum file size.
 *      FILEIO_WRITE_ERROR_NOSPC when the device containing the file has no
 *       room for the data.
 *      FILEIO_WRITE_ERROR_DQUOT for attempts to write file that exceeds
 *       user's disk quota.
 *      FILEIO_ERROR for other errors: only '*actual_count' bytes have been
 *       written for sure.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Write(FileIODescriptor *fd,  // IN:
             const void *bufIn,     // IN:
             size_t requested,      // IN:
             size_t *actual)        // OUT:
{
   const uint8 *buf = (const uint8 *)bufIn;
   size_t initial_requested;
   FileIOResult fret = FILEIO_SUCCESS;

   ASSERT(fd != NULL);

   VERIFY(requested < 0x80000000);

   initial_requested = requested;
   while (requested > 0) {
      ssize_t res;

      res = write(fd->posix, buf, requested);

      if (res == -1) {
         int error = errno;

         if (error == EINTR) {
            continue;
         }
         fret = FileIOErrno2Result(error);
         break;
      }

      buf += res;
      requested -= res;
   }

   if (actual) {
      *actual = initial_requested - requested;
   }
   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Read --
 *
 *      Read from a file
 *
 * Results:
 *      FILEIO_SUCCESS on success: '*actual_count' = 'requested' bytes have
 *       been read.
 *      FILEIO_READ_ERROR_EOF if the end of the file was reached: only
 *       '*actual_count' bytes have been read for sure.
 *      FILEIO_ERROR for other errors: only '*actual_count' bytes have been
 *       read for sure.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Read(FileIODescriptor *fd,  // IN:
            void *bufIn,           // OUT:
            size_t requested,      // IN:
            size_t *actual)        // OUT:
{
   uint8 *buf = (uint8 *) bufIn;
   size_t initial_requested;
   FileIOResult fret = FILEIO_SUCCESS;

   ASSERT(fd != NULL);

   VERIFY(requested < 0x80000000);

   initial_requested = requested;
   while (requested > 0) {
      ssize_t res;

      res = read(fd->posix, buf, requested);
      if (res == -1) {
         if (errno == EINTR) {
            continue;
         }
         fret = FileIOErrno2Result(errno);
         break;
      }

      if (res == 0) {
         fret = FILEIO_READ_ERROR_EOF;
         break;
      }

      buf += res;
      requested -= res;
   }

   if (actual) {
      *actual = initial_requested - requested;
   }
   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Truncate --
 *
 *      Truncates file to a given length
 *
 * Results:
 *      Bool - TRUE on success, FALSE on failure
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

Bool
FileIO_Truncate(FileIODescriptor *file,  // IN:
                uint64 newLength)        // IN:
{
   ASSERT(file != NULL);

   return ftruncate(file->posix, newLength) == 0;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Close --
 *
 *      Close a file
 *
 * Results:
 *      FILEIO_SUCCESS: The file was closed and unlinked. The FileIODescriptor
 *                      is no longer valid.
 *      FILEIO_ERROR: An error occurred.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Close(FileIODescriptor *file)  // IN:
{
   int err;

   ASSERT(file != NULL);

   err = (close(file->posix) == -1) ? errno : 0;

   /* Unlock the file if it was locked */
   FileIO_Unlock(file);
   FileIO_Cleanup(file);
   FileIO_Invalidate(file);

   if (err) {
      errno = err;
   }

   return (err == 0) ? FILEIO_SUCCESS : FILEIO_ERROR;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Sync --
 *
 *      Synchronize the disk state of a file with its memory state
 *
 * Results:
 *      On success: FILEIO_SUCCESS
 *      On failure: FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Sync(const FileIODescriptor *file)  // IN:
{
   ASSERT(file != NULL);

   return (fsync(file->posix) == -1) ? FILEIO_ERROR : FILEIO_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIOCoalesce --
 *
 *      Linux 2.2 does a fairly braindead thing with ioVec's.  It simply issues
 *      reads and writes internal to the kernel in serial
 *      (linux/fs/read_write.c:do_readv_writev()).  We optimize here for the
 *      case of many small chunks.  The cost of the extra copy in this case
 *      is made up for by the decreased number of separate I/Os the kernel
 *      issues internally. Note that linux 2.4 seems to be smarter with respect
 *      to this problem.
 *
 * Results:
 *      Bool - Whether or not coalescing was done.  If it was done,
 *             FileIODecoalesce *MUST* be called.
 *
 * Side effects:
 *      FileIOCoalesce will malloc *outVec if coalescing is performed
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileIOCoalesce(
           struct iovec const *inVec, // IN:  Vector to coalesce from
           int inCount,               // IN:  count for inVec
           size_t inTotalSize,        // IN:  totalSize (bytes) in inVec
           Bool isWrite,              // IN:  coalesce for writing (or reading)
           Bool forceCoalesce,        // IN:  if TRUE always coalesce
           int flags,                 // IN: fileIO open flags
           struct iovec *outVec)      // OUT: Coalesced (1-entry) iovec
{
   uint8 *cBuf;

   ASSERT(inVec != NULL);
   ASSERT(outVec != NULL);

   FileIO_OptionalSafeInitialize();

   /* simple case: no need to coalesce */
   if (inCount == 1) {
      return FALSE;
   }

   /*
    * Only coalesce when the number of entries is above our count threshold
    * and the average size of an entry is less than our size threshold
    */

   if (!forceCoalesce &&
       (!filePosixOptions.enabled ||
       inCount <= filePosixOptions.countThreshold ||
       inTotalSize / inCount >= filePosixOptions.sizeThreshold)) {
      return FALSE;
   }

   // XXX: Wouldn't it be nice if we could log from here!
   //LOG(5, "FILE: Coalescing %s of %d elements and %d size\n",
   //    isWrite ? "write" : "read", inCount, inTotalSize);

   if (filePosixOptions.aligned || flags & FILEIO_OPEN_UNBUFFERED) {
      cBuf = FileIOAligned_Malloc(sizeof(uint8) * inTotalSize);
   } else {
      cBuf = Util_SafeMalloc(sizeof(uint8) * inTotalSize);
   }
   if (!cBuf) {
      return FALSE;
   }

  if (isWrite) {
      IOV_WriteIovToBuf(inVec, inCount, cBuf, inTotalSize);
   }

   outVec->iov_base = cBuf;
   outVec->iov_len = inTotalSize;

   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIODecoalesce --
 *
 *      Inverse of the coalesce optimization.  For writes, its a NOOP, but
 *      for reads, it copies the data back into the original buffer.
 *      It also frees the memory allocated by FileIOCoalesce.
 *
 * Results:
 *      void
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
FileIODecoalesce(
        struct iovec *coVec,         // IN: Coalesced (1-entry) vector
        struct iovec const *origVec, // IN: Original vector
        int origVecCount,            // IN: count for origVec
        size_t actualSize,           // IN: # bytes to transfer back to origVec
        Bool isWrite,                // IN: decoalesce for writing (or reading)
        int flags)                   // IN: fileIO open flags
{
   ASSERT(coVec != NULL);
   ASSERT(origVec != NULL);

   ASSERT(actualSize <= coVec->iov_len);
   ASSERT_NOT_TESTED(actualSize == coVec->iov_len);

   if (!isWrite) {
      IOV_WriteBufToIov(coVec->iov_base, actualSize, origVec, origVecCount);
   }

   if (filePosixOptions.aligned || flags & FILEIO_OPEN_UNBUFFERED) {
      FileIOAligned_Free(coVec->iov_base);
   } else {
      Posix_Free(coVec->iov_base);
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Readv --
 *
 *      Wrapper for readv. In linux, we can issue a readv directly.
 *      But the readv is not atomic, i.e, the read can succeed
 *      on the first N vectors, and return a positive value in spite
 *      of the fact that there was an error on the N+1st vector. There
 *      is no way to query the exact error that happened. So, we retry
 *      in a loop (for a max of MAX_RWV_RETRIES).
 *      XXX: If we retried MAX_RWV_RETRIES times and gave up, we will
 *      return FILEIO_ERROR even if errno is undefined.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR, FILEIO_READ_ERROR_EOF
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Readv(FileIODescriptor *fd,  // IN:
             struct iovec const *v, // IN:
             int numEntries,        // IN:
             size_t totalSize,      // IN:
             size_t *actual)        // OUT:
{
   size_t bytesRead = 0, sum = 0;
   FileIOResult fret = FILEIO_ERROR;
   int nRetries = 0, maxRetries = numEntries;
   struct iovec coV;
   struct iovec const *vPtr;
   Bool didCoalesce;
   int numVec;

   ASSERT(fd != NULL);

   didCoalesce = FileIOCoalesce(v, numEntries, totalSize, FALSE,
                                FALSE, fd->flags, &coV);

   VERIFY(totalSize < 0x80000000);

   numVec = didCoalesce ? 1 : numEntries;
   vPtr = didCoalesce ? &coV : v;

   while (nRetries < maxRetries) {
      ssize_t retval;
      int tempVec = MIN(filePosixOptions.maxIOVec, numVec);

      ASSERT(tempVec > 0);
      retval = readv(fd->posix, vPtr, tempVec);

      if (retval == -1) {
         if (errno == EINTR) {
            continue;
         }
         fret = FileIOErrno2Result(errno);
         break;
      }
      bytesRead += retval;
      if (bytesRead == totalSize) {
         fret = FILEIO_SUCCESS;
         break;
      }
      if (retval == 0) {
         fret = FILEIO_READ_ERROR_EOF;
         break;
      }

      /*
       * Ambigous case. Stupid Linux. If the bytesRead matches an
       * exact iovector boundary, we need to retry from the next
       * iovec. 2) If it does not match, EOF is the only error possible.
       * NOTE: If Linux Readv implementation changes, this
       * ambiguity handling may need to change.
       * --Ganesh, 08/15/2001.
       */

      for (; sum < bytesRead; vPtr++, numVec--) {
         sum += vPtr->iov_len;

         /*
          * In each syscall, we will process atleast one iovec
          * or get an error back. We will therefore retry atmost
          * count times. If multiple iovecs were processed before
          * an error hit, we will retry a lesser number of times.
          */

         nRetries++;
      }
      if (sum > bytesRead) {
         // A partially filled iovec can ONLY mean EOF
         fret = FILEIO_READ_ERROR_EOF;
         break;
      }
   }

   if (didCoalesce) {
      FileIODecoalesce(&coV, v, numEntries, bytesRead, FALSE, fd->flags);
   }

   if (actual) {
      *actual = bytesRead;
   }

   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Writev --
 *
 *      Wrapper for writev. In linux, we can issue a writev directly.
 *      But the writev is not atomic, i.e, the write can succeed
 *      on the first N vectors, and return a positive value in spite
 *      of the fact that there was an error on the N+1st vector. There
 *      is no way to query the exact error that happened. So, we retry
 *      in a loop (for a max of MAX_RWV_RETRIES).
 *      XXX: If we retried MAX_RWV_RETRIES times and gave up, we will
 *      return FILEIO_ERROR even if errno is undefined.
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
FileIO_Writev(FileIODescriptor *fd,  // IN:
              struct iovec const *v, // IN:
              int numEntries,        // IN:
              size_t totalSize,      // IN:
              size_t *actual)        // OUT:
{
   size_t bytesWritten = 0, sum = 0;
   FileIOResult fret = FILEIO_ERROR;
   int nRetries = 0, maxRetries = numEntries;
   struct iovec coV;
   struct iovec const *vPtr;
   Bool didCoalesce;
   int numVec;

   ASSERT(fd != NULL);

   didCoalesce = FileIOCoalesce(v, numEntries, totalSize, TRUE,
                                FALSE, fd->flags, &coV);

   VERIFY(totalSize < 0x80000000);

   numVec = didCoalesce ? 1 : numEntries;
   vPtr = didCoalesce ? &coV : v;

   while (nRetries < maxRetries) {
      ssize_t retval;
      int tempVec = MIN(filePosixOptions.maxIOVec, numVec);

      ASSERT(tempVec > 0);
      retval = writev(fd->posix, vPtr, tempVec);

      if (retval == -1) {
         if (errno == EINTR) {
            continue;
         }
         fret = FileIOErrno2Result(errno);
         break;
      }

      bytesWritten += retval;
      if (bytesWritten == totalSize) {
         fret = FILEIO_SUCCESS;
         break;
      }
      for (; sum < bytesWritten; vPtr++, numVec--) {
         sum += vPtr->iov_len;
         nRetries++;
      }

      /*
       * writev only seems to produce a partial iovec when the disk is
       * out of space.  Just call it an error. --probin
       */

      if (sum != bytesWritten) {
         fret = FILEIO_WRITE_ERROR_NOSPC;
         break;
      }
   }

   if (didCoalesce) {
      FileIODecoalesce(&coV, v, numEntries, bytesWritten, TRUE, fd->flags);
   }

   if (actual) {
      *actual = bytesWritten;
   }

   return fret;
}


#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||\
    defined(__sun__)

/*
 *----------------------------------------------------------------------
 *
 * FileIOPreadvCoalesced --
 *
 *      This function implements vector pread for platforms that do not
 *      support the preadv system call. The incoming vectors are
 *      coalesced to a single buffer to issue only one pread()
 *      system call which reads from a specified offset. The
 *      vectors are then decoalesced before return.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static FileIOResult
FileIOPreadvCoalesced(
                FileIODescriptor *fd,        // IN: File descriptor
                struct iovec const *entries, // IN: Vector to read into
                int numEntries,              // IN: Number of vector entries
                uint64 offset,               // IN: Offset to start reading
                size_t totalSize,            // IN: totalSize(bytes) in entries
                size_t *actual)              // OUT: number of bytes read
{
   struct iovec const *vPtr;
   struct iovec coV;
   int count;
   uint64 fileOffset;
   FileIOResult fret;
   Bool didCoalesce;
   size_t sum = 0;

   didCoalesce = FileIOCoalesce(entries, numEntries, totalSize, FALSE,
                                TRUE /* force coalescing */, fd->flags, &coV);

   count = didCoalesce ? 1 : numEntries;
   vPtr = didCoalesce ? &coV : entries;

   fileOffset = offset;
   while (count > 0) {
      size_t leftToRead = vPtr->iov_len;
      uint8 *buf = (uint8 *) vPtr->iov_base;

      while (leftToRead > 0) {
         ssize_t retval = pread(fd->posix, buf, leftToRead, fileOffset);

         if (retval == -1) {
            if (errno == EINTR) {
               continue;
            }
            fret = FileIOErrno2Result(errno);
            goto exit;
         }

         if (retval == 0) {
            fret = FILEIO_READ_ERROR_EOF;
            goto exit;
         }

         buf += retval;
         leftToRead -= retval;
         sum += retval;
         fileOffset += retval;
      }

      count--;
      vPtr++;
   }
   fret = FILEIO_SUCCESS;

exit:
   if (didCoalesce) {
      FileIODecoalesce(&coV, entries, numEntries, sum, FALSE, fd->flags);
   }
   if (actual) {
      *actual = sum;
   }

   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIOPwritevCoalesced --
 *
 *      This function implements vector pwrite for platforms that do not
 *      support the pwritev system call. The incoming vectors are
 *      coalesced to a single buffer to issue only one pwrite()
 *      system call which writes from a specified offset. The
 *      vectors are then decoalesced before return.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static FileIOResult
FileIOPwritevCoalesced(
                   FileIODescriptor *fd,        // IN: File descriptor
                   struct iovec const *entries, // IN: Vector to write from
                   int numEntries,              // IN: Number of vector entries
                   uint64 offset,               // IN: Offset to start writing
                   size_t totalSize,            // IN: Total size(bytes)
                   size_t *actual)              // OUT: number of bytes written
{
   struct iovec coV;
   Bool didCoalesce;
   struct iovec const *vPtr;
   int count;
   uint64 fileOffset;
   FileIOResult fret;
   size_t sum = 0;

   didCoalesce = FileIOCoalesce(entries, numEntries, totalSize, TRUE,
                                TRUE /* force coalescing */, fd->flags, &coV);

   count = didCoalesce ? 1 : numEntries;
   vPtr = didCoalesce ? &coV : entries;

   fileOffset = offset;
   while (count > 0) {
      size_t leftToWrite = vPtr->iov_len;
      uint8 *buf = (uint8 *)vPtr->iov_base;

      while (leftToWrite > 0) {
         ssize_t retval = pwrite(fd->posix, buf, leftToWrite, fileOffset);

         if (retval == -1) {
            if (errno == EINTR) {
               continue;
            }
            fret = FileIOErrno2Result(errno);
            goto exit;
         }
         if (retval == 0) {
            NOT_TESTED();
            fret = FILEIO_WRITE_ERROR_NOSPC;
            goto exit;
         }
         if (retval < leftToWrite) {
            /*
             * Using %zd on Android generated a warning about
             * expecting a "signed size_t" argument; casting retval to
             * "signed size_t" generated an error, though. We've
             * already checked for retval == -1 above, so the cast
             * below should be OK. Refer to bug 817761.
             */
            LOG_ONCE(LGPFX" %s wrote %"FMTSZ"u out of %"FMTSZ"u bytes.\n",
                     __FUNCTION__, (size_t)retval, leftToWrite);
         }

         buf += retval;
         leftToWrite -= retval;
         sum += retval;
         fileOffset += retval;
      }

      count--;
      vPtr++;
   }

   fret = FILEIO_SUCCESS;
exit:
   if (didCoalesce) {
      FileIODecoalesce(&coV, entries, numEntries, sum, TRUE, fd->flags);
   }
   if (actual) {
      *actual = sum;
   }
   return fret;
}

#endif /* defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||
          defined(__sun__) */


#if defined(__linux__) && !defined(__ANDROID__)

/*
 *----------------------------------------------------------------------
 *
 * FileIOPreadvInternal --
 *
 *      This function implements vector pread for linux builds. Although,
 *      support for preadv() first appeared in Linux 2.6.30,
 *      library support was added in glibc 2.10.
 *      Hence using weak linkage technique, we try to call the more
 *      optimized preadv system call. If the system does not support
 *      this, we fall back to earlier unoptimized technique.
 *
 *      Note that in linux, preadv can succeed on the first N vectors,
 *      and return a positive value in spite of the fact that there was
 *      an error on the N+1st vector. There is no way to query the exact
 *      error that happened. So, we retry in a loop (for a max of
 *      MAX_RWV_RETRIES), same as FileIO_Readv().
 *
 *      XXX: If we retried MAX_RWV_RETRIES times and gave up, we will
 *      return FILEIO_ERROR even if errno is undefined.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static FileIOResult
FileIOPreadvInternal(
                FileIODescriptor *fd,        // IN: File descriptor
                struct iovec const *entries, // IN: Vector to read into
                int numEntries,              // IN: Number of vector entries
                uint64 offset,               // IN: Offset to start reading
                size_t totalSize,            // IN: totalSize(bytes) in entries
                size_t *actual)              // OUT: number of bytes read
{
   struct iovec const *vPtr;
   int numVec;
   size_t partialBytes = 0;
   size_t bytesRead = 0;
   size_t sum = 0;
   int nRetries = 0;
   int maxRetries = numEntries;
   FileIOResult fret = FILEIO_ERROR;

   FileIO_OptionalSafeInitialize();
   numVec = numEntries;
   vPtr = entries;

   while (nRetries < maxRetries) {
      ssize_t retval = 0;

      ASSERT(numVec > 0);

      /*
       * This is needed to deal with old libraries.  Once we're over
       * the library horizon this can go away.
       */
      /* coverity[func_conv] */
      if (preadv64 == NULL) {
         fret = FileIOPreadvCoalesced(fd, entries, numEntries, offset,
                                      totalSize, &bytesRead);
         break;
      } else {
         int tempVec = MIN(filePosixOptions.maxIOVec, numVec);
         retval = preadv64(fd->posix, vPtr, tempVec, offset);
      }
      if (retval == -1) {
         if (errno == EINTR) {
            continue;
         }
         if (errno == ENOSYS || errno == EINVAL || errno == ENOMEM) {
            /*
             * ENOSYS is returned when the kernel does not support preadv and
             * will be returned the first time we ever call preadv. So, we
             * can assume that we are not reading partial requests here.
             * ENOMEM can be due to system call failure and EINVAL is when file
             * was opened with the O_DIRECT flag and memory is not suitably
             * aligned. Let's try to read the remaining vectors using the
             * unoptimized function and hope we don't encounter another error.
             */
            size_t remSize = totalSize - bytesRead;
            uint64 tempOffset =  offset + bytesRead;
            partialBytes = 0;
            fret = FileIOPreadvCoalesced(fd, vPtr, numVec, tempOffset, remSize,
                                         &partialBytes);
            break;
         }
         fret = FileIOErrno2Result(errno);
         break;
      }
      bytesRead += retval;
      if (bytesRead == totalSize) {
         fret = FILEIO_SUCCESS;
         break;
      }
      if (retval == 0) {
         fret = FILEIO_READ_ERROR_EOF;
         break;
      }

      /*
       * This is an ambiguous case in linux preadv implementation.
       * If the bytesRead matches an exact iovector boundary, we need
       * to retry from the next iovec. However, if it does not match,
       * EOF is the only error possible. Linux 3.4.4 continues to have
       * this behaviour.
       * NOTE: If Linux preadv implementation changes, this
       * ambiguity handling may need to change.
       */

      for (; sum < bytesRead; vPtr++, numVec--) {
         sum += vPtr->iov_len;
         offset += vPtr->iov_len;

         /*
          * In each syscall, we will process atleast one iovec
          * or get an error back. We will therefore retry at most
          * count times. If multiple iovecs were processed before
          * an error hit, we will retry a lesser number of times.
          */

         nRetries++;
      }
      if (sum > bytesRead) {
         // A partially filled iovec can ONLY mean EOF
         fret = FILEIO_READ_ERROR_EOF;
         break;
      }
   }
   if (actual) {
      *actual = bytesRead + partialBytes;
   }

   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIOPwritevInternal --
 *
 *      This function implements vector pwrite for linux builds. Although,
 *      support for pwritev() first appeared in Linux 2.6.30, library
 *      support was added in glibc 2.10.
 *      Hence using weak linkage technique, we try to call the more
 *      optimized pwritev system call. If the system does not support
 *      this, we fall back to earlier unoptimized technique.
 *
 *      Note that in linux, pwritev can succeed on the first N vectors,
 *      and return a positive value in spite of the fact that there was
 *      an error on the N+1st vector. There is no way to query the exact
 *      error that happened. So, we retry in a loop (for a max of
 *      MAX_RWV_RETRIES), same as FileIO_Writev().
 *
 *      XXX: If we retried MAX_RWV_RETRIES times and gave up, we will
 *      return FILEIO_ERROR even if errno is undefined.
 *
 * Results:
 *      FILEIO_SUCCESS, FILEIO_ERROR
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static FileIOResult
FileIOPwritevInternal(
                FileIODescriptor *fd,        // IN: File descriptor
                struct iovec const *entries, // IN: Vector to write from
                int numEntries,              // IN: Number of vector entries
                uint64 offset,               // IN: Offset to start writing
                size_t totalSize,            // IN: Total size(bytes)in entries
                size_t *actual)              // OUT: number of bytes written
{
   struct iovec const *vPtr;
   int numVec;
   size_t partialBytes = 0;
   size_t bytesWritten = 0;
   size_t sum = 0;
   int nRetries = 0;
   int maxRetries = numEntries;
   FileIOResult fret = FILEIO_ERROR;

   FileIO_OptionalSafeInitialize();
   numVec = numEntries;
   vPtr = entries;

   while (nRetries < maxRetries) {
      ssize_t retval = 0;

      ASSERT(numVec > 0);

      /*
       * This is needed to deal with old libraries.  Once we're over
       * the library horizon this can go away.
       */
      /* coverity[func_conv] */
      if (pwritev64 == NULL) {
         fret = FileIOPwritevCoalesced(fd, entries, numEntries, offset,
                                       totalSize, &bytesWritten);
         break;
      } else {
         int tempVec = MIN(filePosixOptions.maxIOVec, numVec);
         retval = pwritev64(fd->posix, vPtr, tempVec, offset);
      }
      if (retval == -1) {
         if (errno == EINTR) {
            continue;
         }
         if (errno == ENOSYS || errno == EINVAL || errno == ENOMEM) {
            /*
             * ENOSYS is returned when the kernel does not support pwritev and
             * will be returned the first time we ever call pwritev. So, we
             * can assume that we are not writing partial requests here.
             * ENOMEM can be due to system call failure and EINVAL is when file
             * was opened with the O_DIRECT flag and memory is not suitably
             * aligned. Let's try writing the remaining vectors using the
             * unoptimized function and hope we don't encounter another error.
             */
            size_t remSize = totalSize - bytesWritten;
            uint64 tempOffset =  offset + bytesWritten;
            partialBytes = 0;
            fret = FileIOPwritevCoalesced(fd, vPtr, numVec, tempOffset, remSize,
                                          &partialBytes);
            break;
         }

         fret = FileIOErrno2Result(errno);
         break;
      }

      bytesWritten += retval;
      if (bytesWritten == totalSize) {
         fret = FILEIO_SUCCESS;
         break;
      }
      for (; sum < bytesWritten; vPtr++, numVec--) {
         sum += vPtr->iov_len;
         offset += vPtr->iov_len;
         nRetries++;
      }

      /*
       * pwritev produces a partial iovec when the disk is
       * out of space.  Just call it an error.
       */

      if (sum != bytesWritten) {
         fret = FILEIO_WRITE_ERROR_NOSPC;
         break;
      }
   }
   if (actual) {
      *actual = bytesWritten + partialBytes;
   }
   return fret;
}

#endif /* defined(__linux__) && !defined(__ANDROID__) */


#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||\
    defined(__sun__)

/*
 *----------------------------------------------------------------------
 *
 * FileIO_Preadv --
 *
 *      Implementation of vector pread. The function checks for the support
 *      of system call preadv with the version of glibc and calls the
 *      optimized system call. If the system call is not supported,
 *      we fall back to the earlier technique of coalescing the vectors
 *      and calling a single pread and decoalescing again.
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
FileIO_Preadv(FileIODescriptor *fd,        // IN: File descriptor
              struct iovec const *entries, // IN: Vector to read into
              int numEntries,              // IN: Number of vector entries
              uint64 offset,               // IN: Offset to start reading
              size_t totalSize,            // IN: totalSize (bytes) in entries
              size_t *actual)              // OUT: number of bytes read
{
   FileIOResult fret;

   ASSERT(fd != NULL);
   ASSERT(entries != NULL);
   ASSERT(!(fd->flags & FILEIO_ASYNCHRONOUS));
   VERIFY(totalSize < 0x80000000);

#if defined(__linux__) && !defined(__ANDROID__)
   fret = FileIOPreadvInternal(fd, entries, numEntries, offset, totalSize,
                               actual);
#else
   fret = FileIOPreadvCoalesced(fd, entries, numEntries, offset, totalSize,
                                actual);
#endif /* defined(__linux__ ) && !defined(__ANDROID__) */
   return fret;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Pwritev --
 *
 *      Implementation of vector pwrite. The function checks for the support
 *      of system call pwritev with the version of glibc and calls the
 *      optimized system call. If the system call is not supported,
 *      we fall back to the earlier technique of coalescing the vectors and
 *      calling a single pread and decoalescing again.
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
FileIO_Pwritev(FileIODescriptor *fd,        // IN: File descriptor
               struct iovec const *entries, // IN: Vector to write from
               int numEntries,              // IN: Number of vector entries
               uint64 offset,               // IN: Offset to start writing
               size_t totalSize,            // IN: Total size (bytes) in entries
               size_t *actual)              // OUT: number of bytes written
{
   FileIOResult fret;

   ASSERT(fd != NULL);
   ASSERT(entries != NULL);
   ASSERT(!(fd->flags & FILEIO_ASYNCHRONOUS));
   VERIFY(totalSize < 0x80000000);

#if defined(__linux__) && !defined(__ANDROID__)
   fret = FileIOPwritevInternal(fd, entries, numEntries, offset, totalSize,
                                actual);
#else
   fret = FileIOPwritevCoalesced(fd, entries, numEntries, offset, totalSize,
                                 actual);
#endif /* defined(__linux__ ) && !defined(__ANDROID__) */
   return fret;
}

#endif /* defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__) ||
          defined(__sun__) */


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetAllocSize --
 *
 *      Get logcial and alloced size of a file.
 *
 * Results:
 *      FileIOResult.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_GetAllocSize(const FileIODescriptor *fd,  // IN:
                    uint64 *logicalBytes,        // OUT:
                    uint64 *allocedBytes)        // OUT:
{
   struct stat statBuf;

   ASSERT(fd != NULL);

   if (fstat(fd->posix, &statBuf) == -1) {
      return FileIOErrno2Result(errno);
   }

   if (logicalBytes) {
      *logicalBytes = statBuf.st_size;
   }

   if (allocedBytes) {
     /*
      * If you don't like the magic number 512, yell at the people
      * who wrote sys/stat.h and tell them to add a #define for it.
      */
      *allocedBytes = statBuf.st_blocks * 512;
   }

   return FILEIO_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_SetAllocSize --
 *
 *      Set allocated size of file, allocating new blocks if needed.
 *      It is an error for size to be less than the current size.
 *
 * Results:
 *      TRUE on success.  Sets errno on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
FileIO_SetAllocSize(const FileIODescriptor *fd,  // IN:
                    uint64 size)                 // IN:
{

#if defined(__APPLE__) || defined(__linux__)
   FileIOResult fret;
   uint64 curSize;
   uint64 preallocLen;
#if defined(__APPLE__)
   fstore_t prealloc;
#endif

   fret = FileIO_GetAllocSize(fd, NULL, &curSize);
   if (!FileIO_IsSuccess(fret)) {
      return FALSE;
   }

   if (curSize > size) {
      errno = EINVAL;

      return FALSE;
   }
   preallocLen = size - curSize;

#if defined(__APPLE__)
   prealloc.fst_flags = 0;
   prealloc.fst_posmode = F_PEOFPOSMODE;
   prealloc.fst_offset = 0;
   prealloc.fst_length = preallocLen;
   prealloc.fst_bytesalloc = 0;

   return fcntl(fd->posix, F_PREALLOCATE, &prealloc) != -1;
#elif __linux__
   return syscall(SYS_fallocate, fd->posix, FALLOC_FL_KEEP_SIZE,
                  curSize, preallocLen) == 0;
#endif

#else
   errno = ENOSYS;

   return FALSE;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetAllocSizeByPath --
 *
 *      Get the logcial and alloced size of a file specified by path.
 *
 * Results:
 *      FileIOResult.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_GetAllocSizeByPath(const char *pathName,  // IN:
                          uint64 *logicalBytes,  // OUT:
                          uint64 *allocedBytes)  // OUT:
{
   struct stat statBuf;

   if (Posix_Stat(pathName, &statBuf) == -1) {
      return FileIOErrno2Result(errno);
   }

   if (logicalBytes) {
      *logicalBytes = statBuf.st_size;
   }

   if (allocedBytes) {
     /*
      * If you don't like the magic number 512, yell at the people
      * who wrote sys/stat.h and tell them to add a #define for it.
      */
      *allocedBytes = statBuf.st_blocks * 512;
   }

   return FILEIO_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_Access --
 *
 *      Wrapper for access syscall. We return FILEIO_SUCCESS if the file
 *      is accessible with the specified mode. If not, we will return
 *      FILEIO_ERROR.
 *
 * Results:
 *      FILEIO_SUCCESS or FILEIO_ERROR.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FileIOResult
FileIO_Access(const char *pathName,  // IN: Path name. May be NULL.
              int accessMode)        // IN: Access modes to be asserted
{
   int mode = 0;

   if (pathName == NULL) {
      errno = EFAULT;

      return FILEIO_ERROR;
   }

   if (accessMode & FILEIO_ACCESS_READ) {
      mode |= R_OK;
   }
   if (accessMode & FILEIO_ACCESS_WRITE) {
      mode |= W_OK;
   }
   if (accessMode & FILEIO_ACCESS_EXEC) {
      mode |= X_OK;
   }
   if (accessMode & FILEIO_ACCESS_EXISTS) {
      mode |= F_OK;
   }

   return (Posix_Access(pathName, mode) == -1) ? FILEIO_ERROR : FILEIO_SUCCESS;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetFlags --
 *
 *      Accessor for fd->flags;
 *
 * Results:
 *      fd->flags
 *
 * Side Effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

uint32
FileIO_GetFlags(FileIODescriptor *fd)  // IN:
{
   ASSERT(fd != NULL);
   ASSERT(FileIO_IsValid(fd));

   return fd->flags;
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_SupportsFileSize --
 *
 *      Test whether underlying filesystem supports specified file size.
 *
 * Results:
 *      Return TRUE if such file size is supported, FALSE otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

Bool
FileIO_SupportsFileSize(const FileIODescriptor *fd,  // IN:
                        uint64 requestedSize)        // IN:
{
#if defined(__linux__)
   /*
    * Linux makes test on seek(), so we can do simple non-intrusive test.
    * Verified to work on 2.2.x, 2.4.x and 2.6.x, with ext2, ext3, smbfs,
    * cifs, nfs and ncpfs.  Always got some reasonable value.
    */
   Bool supported = FALSE;
   uint64 oldPos;

   ASSERT(FileIO_IsValid(fd));

   oldPos = FileIO_Seek(fd, 0, FILEIO_SEEK_CURRENT);
   if (oldPos != (uint64)-1) {
      uint64 newPos;

      if (FileIO_Seek(fd, requestedSize, FILEIO_SEEK_BEGIN) == requestedSize) {
         supported = TRUE;
      }
      newPos = FileIO_Seek(fd, oldPos, FILEIO_SEEK_BEGIN);
      VERIFY(oldPos == newPos);
   }

   return supported;
#elif defined(__APPLE__)
   struct statfs buf;

   if (fstatfs(fd->posix, &buf) == -1) {
      Log(LGPFX" %s fstatfs failure: %s\n", __FUNCTION__,
          Err_Errno2String(errno));
      /* Be optimistic despite failure */
      return TRUE;
   }

   /* Check for FAT and UFS file systems */
   if ((Str_Strcasecmp(buf.f_fstypename, "msdos") == 0) ||
       (Str_Strcasecmp(buf.f_fstypename, "ufs") == 0)) {
      /* 4 GB limit */
      return requestedSize > CONST64U(0xFFFFFFFF) ? FALSE : TRUE;
   }

   /* Be optimistic... */
   return TRUE;
#else
   /* Be optimistic on FreeBSD and Solaris... */
   return TRUE;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_GetModTime --
 *
 *      Retrieve last modification time.
 *
 * Results:
 *      Return POSIX epoch time or -1 on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int64
FileIO_GetModTime(const FileIODescriptor *fd)  // IN:
{
   struct stat statbuf;

   if (fstat(fd->posix, &statbuf) == 0) {
      return statbuf.st_mtime;
   } else {
      return -1;
   }
}


/*
 *----------------------------------------------------------------------
 *
 * FileIO_PrivilegedPosixOpen --
 *
 *      Opens file with elevated privileges.
 *
 * Results:
 *      Opened file descriptor, or -1 on failure (errno contains error code).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
FileIO_PrivilegedPosixOpen(const char *pathName,  // IN:
                           int flags)             // IN:
{
   int fd;

   if (pathName == NULL) {
      errno = EFAULT;

      return -1;
   }

   /*
    * I've said *opens*.  I want you really think twice before creating files
    * with elevated privileges, so for them you have to use Id_BeginSuperUser()
    * yourself.
    */

   ASSERT((flags & (O_CREAT | O_TRUNC)) == 0);

#if defined(__APPLE__)
   if (privilegedOpenerFunc != NULL) {
      fd = (*privilegedOpenerFunc)(pathName, flags);
   } else
#endif
   {
      Bool suDone;
      uid_t uid = -1;

      if (Id_IsSuperUser()) {
         suDone = FALSE;
      } else {
         uid = Id_BeginSuperUser();
         suDone = TRUE;
      }

      fd = Posix_Open(pathName, flags, 0);

      if (suDone) {
         int error = errno;

         Id_EndSuperUser(uid);
         errno = error;
      }
   }

   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileIO_DescriptorToStream
 *
 *      Return a FILE * stream equivalent to the given FileIODescriptor.
 *      This is the logical equivalent of Posix dup() then fdopen().
 *
 *      Caller should fclose the returned descriptor when finished.
 *
 * Results:
 *      !NULL  A FILE * associated with the file associated with the fd
 *      NULL   Failure
 *
 * Side effects:
 *      New fd allocated.
 *
 *-----------------------------------------------------------------------------
 */

FILE *
FileIO_DescriptorToStream(FileIODescriptor *fdesc,  // IN:
                          Bool textMode)            // IN: unused
{
   int dupFD;
   const char *mode;
   int tmpFlags;
   FILE *stream;

   dupFD = dup(fdesc->posix);
   if (dupFD == -1) {
      return NULL;
   }

   /* The file you pass us should be valid and opened for *something* */
   ASSERT(FileIO_IsValid(fdesc));
   ASSERT((fdesc->flags & (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE)) != 0);
   tmpFlags = fdesc->flags & (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE);

   if (tmpFlags == (FILEIO_OPEN_ACCESS_READ | FILEIO_OPEN_ACCESS_WRITE)) {
      mode = "r+";
   } else if (tmpFlags == FILEIO_OPEN_ACCESS_WRITE) {
      mode = "w";
   } else {  /* therefore (tmpFlags == FILEIO_OPEN_ACCESS_READ) */
      mode = "r";
   }

   stream = fdopen(dupFD, mode);

   if (stream == NULL) {
      close(dupFD);
   }

   return stream;
}


#if defined(__APPLE__)
/*
 *----------------------------------------------------------------------
 *
 * HostSupportsPrealloc --
 *
 *      Returns TRUE if the host OS is new enough to support F_PREALLOCATE
 *      without data loss bugs.  On OSX, this has been verified fixed
 *      on 10.6 build with kern.osrelease 10.0.0d6.
 *
 * Results:
 *      TRUE if the current host OS is new enough.
 *      FALSE if it is not or we can't tell because of an error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
HostSupportsPrealloc(void)
{
   static Atomic_uint32 supported = { 0 };
   enum { PREALLOC_UNKNOWN = 0, PREALLOC_YES, PREALLOC_NO } val;
   char curRel[32];
   char type;
   unsigned static const int req[] = { 10, 0, 0, 6 };
   unsigned int cur[4], i;
   int num;
   size_t len = sizeof curRel;
   Bool ret = FALSE;

   val = Atomic_Read(&supported);
   if (val != PREALLOC_UNKNOWN) {
      return val == PREALLOC_YES;
   }

   if (sysctlbyname("kern.osrelease", (void *) &curRel, &len, NULL, 0) == -1) {
      goto exit;
   }

   curRel[31] = '\0';
   Log("Current OS Release is %s\n", curRel);

   /*
    * Apple's osversion is in the format X.Y.Z which maps to the public
    * OSX version 10.X-4.Y, and Z is incremented for each publicly
    * released build.  The Z part is of the form A<type>B, where a and
    * B are version numbers and <type> is either d (devel), a (alpha),
    * b (beta), rc, or fc.  If the <type>B is missing, then its a GA build.
    *
    * Since we're checking for 10.0.0d6, we can just say anything without
    * a type or with a type other than d is higher.  For d, we compare
    * the last number.
    */

   num = sscanf(curRel, "%u.%u.%u%c%u", &cur[0], &cur[1], &cur[2], &type,
                &cur[3]);

   if (num < 3) {
      goto exit;
   }

   for (i = 0; i < 3; i++) {
      if (req[i] > cur[i]) {
         goto exit;
      } else if (req[i] < cur[i]) {
         ret = TRUE;
         goto exit;
      }
   }
   if (num == 5 && type == 'd') {
      ret = req[3] <= cur[3];
      goto exit;
   }

   /*
    * If we get a type with no letter (num == 4), thats odd.
    * Consider it mal-formatted and fail.
    */

   ret = num != 4;

exit:
   if (!ret && filePosixOptions.initialized &&
       filePosixOptions.aioNumThreads == 1) {
      ret = TRUE;
   }

   Atomic_Write(&supported, ret ? PREALLOC_YES : PREALLOC_NO);

   return ret;
}

#else

/*
 *----------------------------------------------------------------------
 *
 * HostSupportsPrealloc --
 *
 *      fallocate() is supported for ext4 and xfs since 2.6.23 kernels
 *
 * Results:
 *      TRUE if the current host is linux and kernel is >= 2.6.23.
 *      FALSE if it is not .
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

static Bool
HostSupportsPrealloc(void)
{
#if  (defined(__linux__ ) && !defined(VMX86_SERVER))
    if (Hostinfo_OSVersion(0) >=2 && Hostinfo_OSVersion(1) >=6 &&
        Hostinfo_OSVersion(2) >=23) {
       return TRUE;
    }
#endif
    return FALSE;
}

#endif


/*
 *----------------------------------------------------------------------------
 *
 * FileIO_SupportsPrealloc --
 *
 *      Checks if the HostOS/filesystem supports preallocation.
 *
 * Results:
 *      TRUE if supported by the Host OS/filesystem.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
FileIO_SupportsPrealloc(const char *pathName,  // IN:
                        Bool fsCheck)          // IN:
{
   Bool ret = TRUE;

   if (!HostSupportsPrealloc()) {
      return FALSE;
   }

   if (!fsCheck) {
      return ret;
   }

#if (defined( __linux__) && !defined(VMX86_SERVER))
   {
      struct statfs statBuf;
      char *fullPath;

      ret = FALSE;
      if (!pathName) {
         return ret;
      }

      fullPath = File_FullPath(pathName);
      if (!fullPath) {
         return ret;
      }

      if (Posix_Statfs(fullPath, &statBuf) == 0 &&
         statBuf.f_type == EXT4_SUPER_MAGIC) {
         ret = TRUE;
      }
      Posix_Free(fullPath);
   }
#endif

   return ret;
}


/*
 * The FileIOAligned_* functions are only used on
 * hosted (see fileInt.h for rationale).
 */
#if !defined(VMX86_TOOLS) && !defined(VMX86_SERVER)

/*
 *---------------------------------------------------------------------------
 *
 * FileIOAligned_PoolInit --
 *
 *      Initialize alignedPool. Must be called before FileIOAligned_PoolMalloc.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
FileIOAligned_PoolInit(void)
{
   alignedPool.lock = MXUser_CreateSingletonExclLock(&alignedPoolLockStorage,
                                                     "alignedPoolLock",
                                                     RANK_LEAF);
}


/*
 *---------------------------------------------------------------------------
 *
 * FileIOAligned_PoolExit --
 *
 *      Tear down alignedPool.  Afterwards, PoolInit can be called again if
 *      desired.
 *
 * Result:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void
FileIOAligned_PoolExit(void)
{
   if (!alignedPool.lock) {
      LOG_ONCE("%s called without FileIOAligned_Pool lock\n", __FUNCTION__);
      return;
   }

   MXUser_AcquireExclLock(alignedPool.lock);

   if (alignedPool.numBusy > 0) {
      LOG_ONCE("%s: %d busy buffers!  Proceeding with trepidation.\n",
               __FUNCTION__, alignedPool.numBusy);
   }
   while (alignedPool.numAlloc > 0) {
      alignedPool.numAlloc--;
      Aligned_Free(alignedPool.list[alignedPool.numAlloc]);
   }

   MXUser_ReleaseExclLock(alignedPool.lock);
   MXUser_DestroyExclLock(alignedPool.lock);

   memset(&alignedPool, 0, sizeof alignedPool);
}


/*
 *---------------------------------------------------------------------------
 *
 * FileIOAligned_PoolMalloc --
 *
 *      Alloc a chunk of memory aligned on a page boundary using a memory
 *      pool.  Result needs to be freed with FileIOAligned_PoolFree.  Returns
 *      NULL if the pool is full or the requested size cannot be satisfied from
 *      the pool.
 *
 * Result:
 *      A pointer.  NULL if requested size is too large, or on out of memory
 *      condition.
 *
 * Side effects:
 *      None.
 *
 *---------------------------------------------------------------------------
 */

void *
FileIOAligned_PoolMalloc(size_t size)  // IN:
{
   void *buf = NULL;

   if (!alignedPool.lock) {
      LOG_ONCE("%s called without FileIOAligned_Pool lock\n", __FUNCTION__);
      return NULL;
   }

   if (size > ALIGNEDPOOL_BUFSZ) {
      return NULL;
   }

   MXUser_AcquireExclLock(alignedPool.lock);

   ASSERT(alignedPool.numBusy <= ARRAYSIZE(alignedPool.list));
   ASSERT(alignedPool.numAlloc <= ARRAYSIZE(alignedPool.list));
   ASSERT(alignedPool.numBusy <= alignedPool.numAlloc);

   if (alignedPool.numBusy == ARRAYSIZE(alignedPool.list)) {
      goto done;
   }
   if (alignedPool.numBusy == alignedPool.numAlloc) {
      buf = Aligned_UnsafeMalloc(ALIGNEDPOOL_BUFSZ);
      /* If allocation fails, just bail. */
      if (buf) {
         alignedPool.list[alignedPool.numAlloc] = buf;
         alignedPool.numBusy = ++alignedPool.numAlloc;
      }
      goto done;
   }
   buf = alignedPool.list[alignedPool.numBusy];
   alignedPool.numBusy++;

done:
   MXUser_ReleaseExclLock(alignedPool.lock);

   return buf;
}


/*
 *---------------------------------------------------------------------------
 *
 * FileIOAligned_PoolFree --
 *
 *      Test if a pointer was allocated from alignedPool, and if so, free it.
 *
 * Result:
 *      TRUE if ptr was allocated from alignedPool.  ptr is returned to pool.
 *      FALSE otherwise.
 *
 * Side effects:
 *      Might Aligned_Free() some entries from alignedPool if the timestamp[]
 *      entries indicate that we have not needed them for a while.
 *
 *---------------------------------------------------------------------------
 */

Bool
FileIOAligned_PoolFree(void *ptr)  // IN:
{
   unsigned i;
   Bool ret = FALSE;
   VmTimeType now;

   if (!alignedPool.lock) {
      LOG_ONCE("%s called without FileIOAligned_Pool lock\n", __FUNCTION__);

      return FALSE;
   }

   MXUser_AcquireExclLock(alignedPool.lock);

   ASSERT(alignedPool.numBusy <= ARRAYSIZE(alignedPool.list));
   ASSERT(alignedPool.numAlloc <= ARRAYSIZE(alignedPool.list));
   ASSERT(alignedPool.numBusy <= alignedPool.numAlloc);

   for (i = 0; i < alignedPool.numBusy; i++) {
      if (alignedPool.list[i] == ptr) {
         break;
      }
   }
   if (i == alignedPool.numBusy) {
      /* The pointer wasn't allocated from our pool. */
      goto done;
   }

   alignedPool.numBusy--;

   /*
    * At this point either i points to the "top" busy item, or i points to an
    * earlier busy item.  If i points to the top, we're done, and the following
    * "swap" is a noop.  If i points somewhere further down the busy list, we
    * can simply move the newly freed item to the top of the free list by
    * swapping its place with the not-freed item at list[numBusy].
    */
   alignedPool.list[i] = alignedPool.list[alignedPool.numBusy];
   alignedPool.list[alignedPool.numBusy] = ptr;

   now = Hostinfo_SystemTimerNS();
   alignedPool.timestamp[alignedPool.numBusy] = now;

   while (alignedPool.numAlloc > alignedPool.numBusy &&
          now - alignedPool.timestamp[alignedPool.numAlloc - 1] > ALIGNEDPOOL_OLD_AGE) {
      alignedPool.numAlloc--;
      Aligned_Free(alignedPool.list[alignedPool.numAlloc]);
      alignedPool.list[alignedPool.numAlloc] = NULL;
   }

   ret = TRUE;

done:
   MXUser_ReleaseExclLock(alignedPool.lock);

   return ret;
}

#endif
