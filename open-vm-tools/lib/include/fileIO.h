/*********************************************************
 * Copyright (C) 1998-2018,2021 VMware, Inc. All rights reserved.
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
 * fileIO.h --
 *
 *	Host-independent wrapper for low-level fileIO functions.
 *
 */

/*
 * Note:
 *
 *  . FileIO_[Read|Write]() [read|write]s exactly the number of bytes requested
 *    unless an error occurs
 *  . FileIO_Seek() supports files larger than 2 GB
 *  . If a function returns a generic error, you can call your native function
 *    to retrieve the last error code
 */

#ifndef _FILEIO_H_
#define _FILEIO_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdio.h>
#include <stdlib.h>
#if !defined(_WIN32)
#include <sys/types.h>
#include <dirent.h>
#endif

#include "vm_basic_types.h"
#include "unicodeTypes.h"

#include "iovector.h"        // for struct iovec

#if defined(__cplusplus)
extern "C" {
#endif

struct FileLockToken;

#if defined(_WIN32)

# include <windows.h>

typedef struct FileIODescriptor {
   HANDLE                win32;
   uint32                flags;
   char                 *fileName;
   struct FileLockToken *lockToken;
} FileIODescriptor;

#else

typedef struct FileIODescriptor {
   int                   posix;
   int                   flags;
   char                 *fileName;
   struct FileLockToken *lockToken;
} FileIODescriptor;

#endif

typedef enum {
   /* distance is relative to the beginning of the file */
   FILEIO_SEEK_BEGIN,
   /* distance is relative to the current position in the file */
   FILEIO_SEEK_CURRENT,
   /* distance is relative to the end of the file */
   FILEIO_SEEK_END,
} FileIOSeekOrigin;

#define FILEIO_OPEN_ACCESS_READ  (1 << 0)
#define FILEIO_OPEN_ACCESS_WRITE (1 << 1)
/*
 * Use synchronous writes (no lazy buffer cache flush)
 */
#define FILEIO_OPEN_SYNC         (1 << 2)
/*
 * Delete the file as soon as possible (i.e. when nobody uses it anymore)
 */
#define FILEIO_OPEN_DELETE_ASAP  (1 << 3)
#define FILEIO_OPEN_UNBUFFERED   (1 << 4)
/*
 * Lock the file on open
 */
#define FILEIO_OPEN_LOCKED       (1 << 5)
/*
 * Asynchronous file I/O
 */
#define FILEIO_ASYNCHRONOUS      (1 << 6)
/*
 * Open non-blocking mode
 */
#define FILEIO_OPEN_NONBLOCK     (1 << 7)
/*
 * Open with more privileges
 */
#define FILEIO_OPEN_PRIVILEGED   (1 << 8)
/*
 * Open exclusive.
 * On Windows host it doesn't pass the flag FILE_SHARE_(READ|WRITE) to
 * CreateFile. Right now, EXCLUSIVE_READ is not used and EXCLUSIVE_WRITE
 * is only used by the cdrom code to implement the exclusive option.
 * On Linux hosts, it passes O_EXCL if both are set.
 * By default, we share r/w. -Maxime
 */
#define FILEIO_OPEN_EXCLUSIVE_READ     (1 << 9)
#define FILEIO_OPEN_EXCLUSIVE_WRITE    (1 << 10)
/*
 * Open sequential.
 * This flag only changes the behavior on Windows host. It is off by default.
 */
#define  FILEIO_OPEN_SEQUENTIAL_SCAN   (1 << 11)
/*
 * Make IOCTL be run by root.  This flag only changes the behavior on Linux
 * host. It is off by default.
 *
 * XXX: This has nothing to do with fileIO, but since AIOMgr shares the flags
 * with fileIO, I had to add it here.  In some future it would be nice to
 * unshare the flags between the two at which point this could be fixed.
 * --Tommy
 */
#define  FILEIO_OPEN_PRIVILEGED_IOCTL    (1 << 12)
/*
 * Lock the file on open with a exclusive leased lock that can be broken
 * (supported on ESX file systems)
 */
#define FILEIO_OPEN_EXCLUSIVE_LOCK       (1 << 13)
/*
 * Lock the file on open with a multiwriter leased lock that can be broken
 * (supported on ESX file systems)
 */
#define FILEIO_OPEN_MULTIWRITER_LOCK     (1 << 14)
/*
 * Lock the file on open with an SWMR leased lock that can be broken
 * (supported on ESX file systems)
 */
#define FILEIO_OPEN_SWMR_LOCK            (1 << 15)
/*
 * Valid only for MacOS. It eventually results into O_EXLOCK flag passed to open
 * system call.
 *
 * O_EXLOCK, O_SHLOCK behavior is tested on Mac OS X Server 10.6, kernel 10.0.0.
 *
 * |                      | Block devices      | Regular files
 * |----------------------|--------------------|----------------
 * | Locking behavior     | mandatory          | advisory
 * |                      |                    |
 * | If O_NONBLOCK absent | open doesn't block | open blocks
 * |                      | on conflicts       | on conflicts
 */
#define FILEIO_OPEN_EXCLUSIVE_LOCK_MACOS (1 << 16)
/*
 * Open file in APPEND-only mode.  All writes go to the current end of file,
 * not to the current file pointer location.
 */
#define FILEIO_OPEN_APPEND               (1 << 17)
/*
 * Valid only on POSIXen. Don't follow a symbolic link.
 */
#define FILEIO_OPEN_ACCESS_NOFOLLOW      (1 << 18)
/*
 * Valid only on Windows. Set FILE_SHARE_DELETE.
 */
#define FILEIO_OPEN_SHARE_DELETE         (1 << 19)
/*
 * Strengths of file lock.
 * Advisory:
 *   Must use FileIO plus lock flags to get locking.
 *   Never uses kernel/fs-level lock, so naked open() bypasses locking.
 * Mandatory:
 *   Requires kernel/fs-level, so naked open() respects lock.
 *   Kernel/fs-level locks are available on ESX but not hosted.
 * Best:
 *   Adaptively picks between mandatory and advisory.
 * Almost all cases should use the "best" lock.
 */
#define FILEIO_OPEN_LOCK_BEST            FILEIO_OPEN_LOCKED /* historical */
#define FILEIO_OPEN_LOCK_ADVISORY        (1 << 20)
#define FILEIO_OPEN_LOCK_MANDATORY       (1 << 21)

/*
 * Flag passed to open() to enable use of swmr-reader locks on VMFS.  This
 * definition must match USEROBJ_OPEN_SWMR_LOCK in user_vsiTypes.h.
 */
#define O_SWMR_LOCK                      (1 << 21)  // 0x00200000

/*
 * OPTIMISTIC is an alternative to EXCLUSIVE and MANDATORY. It applies
 * only on ESX, and gives VMkernel permission to use a type of lock
 * called "optimistic" to speed up opens. Rule-of-thumb is to use it
 * only for read-only opens of small files (< 1KB).
 */
#define FILEIO_OPEN_OPTIMISTIC_LOCK      (1 << 22)  // 0x00400000

/*
 * Flag passed to open() to enable use of oplocks on VMFS.  This definition
 * must match USEROBJ_OPEN_OPTIMISTIC_LOCK in user_vsiTypes.h.
 */
#define O_OPTIMISTIC_LOCK                (1 << 22)  // 0x00400000

/*
 * POSIX specific close the file descriptor when the program uses a variant
 * of the exec system call capability. This is useful in fork/exec scenarios.
 */
#define FILEIO_OPEN_CLOSE_ON_EXEC        (1 << 23)  // 0x00800000

/*
 * Flag passed to open() to not attempt to get the LUN attributes as part of
 * the open operation. Applicable only to opening of SCSI devices. This
 * definition must match the definition of USEROBJ_OPEN_NOATTR in
 * user_vsiTypes.h and FS_OPEN_NOATTR in fs_public.h
 */
#define O_NOATTR                         (1 << 26)  // 0x04000000

/*
 * Flag passed to open() to get multiwriter VMFS lock.  This definition must
 * match USEROBJ_OPEN_MULTIWRITER_LOCK in user_vsiTypes.h.
 */

#define O_MULTIWRITER_LOCK               (1 << 27)  // 0x08000000

/*
 * Flag passed to open() to get exclusive VMFS lock.  This definition must
 * match USEROBJ_OPEN_EXCLUSIVE_LOCK in user_vsiTypes.h.
 */

#define O_EXCLUSIVE_LOCK                 (1 << 28)  // 0x10000000

/* File Access check args */
#define FILEIO_ACCESS_READ       (1 << 0)
#define FILEIO_ACCESS_WRITE      (1 << 1)
#define FILEIO_ACCESS_EXEC       (1 << 2)
#define FILEIO_ACCESS_EXISTS     (1 << 3)

typedef enum {               //  File doesn't exist   File exists
   FILEIO_OPEN,              //  error
   FILEIO_OPEN_EMPTY,        //  error               size = 0
   FILEIO_OPEN_CREATE,       //  create
   FILEIO_OPEN_CREATE_SAFE,  //  create              error
   FILEIO_OPEN_CREATE_EMPTY, //  create              size = 0
} FileIOOpenAction;

typedef enum {
   /*
    * Generic status codes
    */

   /* No error */
   FILEIO_SUCCESS,
   /* The user cancelled the operation */
   FILEIO_CANCELLED,
   /* Generic error */
   FILEIO_ERROR,

   /*
    * Status codes specific to FileIO_Open()
    */

   /* FILEIO_OPEN_CREATE_SAFE was used and the file already existed */
   FILEIO_OPEN_ERROR_EXIST,

   /* Couldn't obtain the requested lock */
   FILEIO_LOCK_FAILED,

   /* Status codes specific to FileIO_Read() */

   /* Tried to read beyond the end of a file */
   FILEIO_READ_ERROR_EOF,

   /* Couldnt locate file */
   FILEIO_FILE_NOT_FOUND,

   /* Insufficient Permissions */
   FILEIO_NO_PERMISSION,

   /* File name too long */
   FILEIO_FILE_NAME_TOO_LONG,
   /*
    * Status codes specific for FileIO_Write()
    */

   /* Attempts to write  file that exceeds maximum file size */
   FILEIO_WRITE_ERROR_FBIG,

   /* The device containint the file has no room for the data */
   FILEIO_WRITE_ERROR_NOSPC,

   /* Attempts to write file that exceeds user's disk quota */
   FILEIO_WRITE_ERROR_DQUOT,

   /*
    * NB: Until disklib error handling is changed, there must be no more
    *     than 16 total error codes here.
    */
   FILEIO_ERROR_LAST,  /* Must be last! */

} FileIOResult;

#if defined(__APPLE__)
typedef int (FileIOPrivilegedOpener)(const char *path,
                                     int flags);
#endif


const char *FileIO_MsgError(FileIOResult status);

void FileIO_Invalidate(FileIODescriptor *file);

Bool FileIO_IsValid(const FileIODescriptor *fd);

FileIOResult FileIO_Create(FileIODescriptor *file,
                           const char *pathName,
                           int access,
                           FileIOOpenAction action,
                           int mode);

FileIOResult FileIO_CreateRetry(FileIODescriptor *file,
                               const char *pathName,
                               int access,
                               FileIOOpenAction action,
                               int mode,
                               uint32 maxWaitTimeMsec);

FileIOResult FileIO_Open(FileIODescriptor *file,
                         const char *pathName,
                         int access,
                         FileIOOpenAction action);

FileIOResult FileIO_OpenRetry(FileIODescriptor *file,
                              const char *pathName,
                              int access,
                              FileIOOpenAction action,
                              uint32 maxWaitTimeMsec);

uint64 FileIO_Seek(const FileIODescriptor *file,
                   int64 distance,
                   FileIOSeekOrigin origin);

FileIOResult FileIO_Read(FileIODescriptor *file,
                         void *buf,
                         size_t requested,
                         size_t *actual);

FileIOResult FileIO_Write(FileIODescriptor *file,
                          const void *buf,
                          size_t requested,
                          size_t *actual);

char *FileIO_AtomicTempPath(const char *path);

FileIOResult FileIO_AtomicTempFile(FileIODescriptor *fileFD,
                                   FileIODescriptor *tempFD);

Bool FileIO_AtomicUpdate(FileIODescriptor *newFD,
                         FileIODescriptor *currFD);

int FileIO_AtomicUpdateEx(FileIODescriptor *newFD,
                          FileIODescriptor *currFD,
                          Bool renameOnNFS);

#if !defined(VMX86_TOOLS) || !defined(__FreeBSD__)

FileIOResult FileIO_Readv(FileIODescriptor *fd,
                          struct iovec const *v,
                          int count,
                          size_t totalSize,
                          size_t *bytesRead);

FileIOResult FileIO_Writev(FileIODescriptor *fd,
                           struct iovec const *v,
                           int count,
                           size_t totalSize,
                           size_t *bytesWritten);
#endif

FileIOResult FileIO_Preadv(
               FileIODescriptor *fd,        // IN: File descriptor
               struct iovec const *entries, // IN: Vector to read into
               int numEntries,              // IN: Number of vector entries
               uint64 offset,               // IN: Offset to start reading
               size_t totalSize,            // IN: totalSize (bytes) in entries
               size_t *actual);             // OUT: number of bytes read

FileIOResult FileIO_Pwritev(
              FileIODescriptor *fd,        // IN: File descriptor
              struct iovec const *entries, // IN: Vector to write from
              int numEntries,              // IN: Number of vector entries
              uint64 offset,               // IN: Offset to start writing
              size_t totalSize,            // IN: Total size (bytes) in entries
              size_t *actual);             // OUT: number of bytes written

FileIOResult FileIO_Pread(
                        FileIODescriptor *fd,    // IN: File descriptor
                        void *buf,               // IN: Buffer to read into
                        size_t len,              // IN: Length of the buffer
                        uint64 offset);          // IN: Offset to start reading

FileIOResult FileIO_Pwrite(
                         FileIODescriptor *fd,   // IN: File descriptor
                         void const *buf,        // IN: Buffer to write from
                         size_t len,             // IN: Length of the buffer
                         uint64 offset);         // IN: Offset to start writing

FileIOResult FileIO_Access(const char *pathName,
                           int accessMode);

Bool    FileIO_Truncate(FileIODescriptor *file,
                        uint64 newSize);

FileIOResult  FileIO_Sync(const FileIODescriptor *file);

FileIOResult FileIO_GetAllocSize(const FileIODescriptor *fd,
                                 uint64 *logicalBytes,
                                 uint64 *allocedBytes);

int64   FileIO_GetSize(const FileIODescriptor *fd);

Bool    FileIO_SetAllocSize(const FileIODescriptor *fd,
                            uint64 size);

FileIOResult FileIO_GetAllocSizeByPath(const char *pathName,
                                       uint64 *logicalBytes,
                                       uint64 *allocedBytes);

int64   FileIO_GetSizeByPath(const char *pathName);

FileIOResult    FileIO_Close(FileIODescriptor *file);

FileIOResult    FileIO_CloseAndUnlink(FileIODescriptor *file);

uint32  FileIO_GetFlags(FileIODescriptor *file);

#if defined(_WIN32)
Bool    FileIO_GetVolumeSectorSize(const char *name,
                                   uint32 *sectorSize);
#endif

Bool    FileIO_SupportsFileSize(const FileIODescriptor *file,
                                uint64 testSize);

int64   FileIO_GetModTime(const FileIODescriptor *fd);

FileIOResult FileIO_Lock(FileIODescriptor *file,
                         int access);

FileIOResult FileIO_Unlock(FileIODescriptor *file);

/* Only users not using FileIO_Open should use these two */
void FileIO_Init(FileIODescriptor *fd,
                 const char *pathName);

void FileIO_Cleanup(FileIODescriptor *fd);

const char *FileIO_ErrorEnglish(FileIOResult status);

void FileIO_OptionalSafeInitialize(void);

#if defined(_WIN32)
FileIODescriptor FileIO_CreateFDWin32(HANDLE win32,
                                      DWORD access,
                                      DWORD attributes);
#else
FileIODescriptor FileIO_CreateFDPosix(int posix,
                                      int flags);

int FileIO_PrivilegedPosixOpen(const char *pathName,
                               int flags);
#endif

FILE *FileIO_DescriptorToStream(FileIODescriptor *fd,
                                Bool textMode);

const char *FileIO_Filename(FileIODescriptor *fd);

#ifdef VMX86_SERVER
FileIOResult FileIO_CreateDeviceFileNoPrompt(FileIODescriptor *fd,
                                             const char *pathName,
                                             int openMode,
                                             FileIOOpenAction action,
                                             int perms,
                                             const char *device);
#endif

/*
 *-------------------------------------------------------------------------
 *
 * FileIO_IsSuccess --
 *
 *      Returns TRUE if the error code is success.
 *
 * Result:
 *      TRUE/FALSE.
 *
 * Side effects:
 *      None.
 *
 *-------------------------------------------------------------------------
 */

static INLINE Bool
FileIO_IsSuccess(FileIOResult res)      // IN
{
   return res == FILEIO_SUCCESS;
}

Bool FileIO_SupportsPrealloc(const char *pathName,
                             Bool fsCheck);

#if defined(__APPLE__)
void FileIO_SetPrivilegedOpener(FileIOPrivilegedOpener *opener);
#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _FILEIO_H_
