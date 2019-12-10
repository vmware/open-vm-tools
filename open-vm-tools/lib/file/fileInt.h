/*********************************************************
 * Copyright (C) 2007-2019 VMware, Inc. All rights reserved.
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
 * fileInt.h --
 *
 *     Things internal to the file library.
 */

#if !defined(__FILE_INTERNAL_H__)
#define __FILE_INTERNAL_H__

#define INCLUDE_ALLOW_USERLEVEL
#include "includeCheck.h"
#include "err.h"
#include "posix.h"
#include "file.h"
#include "fileIO.h"
#include "fileLock.h"
#include "unicodeTypes.h"
#include "memaligned.h"

/*
 * Max supported file size is 64 TB.
 */
#define MAX_SUPPORTED_FILE_SIZE CONST64U(0x400000000000)

#if defined __linux__
/*
 * These magic constants are used only for parsing Linux statfs data.
 * So they make sense only for Linux build. If you need them on other OSes,
 * think once more.
 */
#define HFSPLUS_SUPER_MAGIC   0x482B
#if !defined(__ANDROID__)
#define ADFS_SUPER_MAGIC      0xadf5
#define AFFS_SUPER_MAGIC      0xADFF
#define EXT_SUPER_MAGIC       0x137D
#define EXT2_OLD_SUPER_MAGIC  0xEF51
#define EXT2_SUPER_MAGIC      0xEF53
#define EXT3_SUPER_MAGIC      0xEF53
#define EXT4_SUPER_MAGIC      0xEF53
#define NFS_SUPER_MAGIC       0x6969
#define SMB_SUPER_MAGIC       0x517B
#define ISOFS_SUPER_MAGIC     0x9660
#define JFFS2_SUPER_MAGIC     0x72b6
#define PROC_SUPER_MAGIC      0x9fa0
#define OPENPROM_SUPER_MAGIC  0x9fa1
#define USBDEVICE_SUPER_MAGIC 0x9fa2
#define AUTOFS_SUPER_MAGIC    0x0187
#endif

#if !defined(MSDOS_SUPER_MAGIC)
#define MSDOS_SUPER_MAGIC     0x4D44
#endif

#define XENIX_SUPER_MAGIC     0x012FF7B4
#define SYSV4_SUPER_MAGIC     0x012FF7B5
#define SYSV2_SUPER_MAGIC     0x012FF7B6
#define COH_SUPER_MAGIC       0x012FF7B7
#define UFS_SUPER_MAGIC       0x00011954
#define XFS_SUPER_MAGIC       0x58465342
#define VMFS_SUPER_MAGIC      0x2fABF15E
#define TMPFS_SUPER_MAGIC     0x01021994
#define JFS_SUPER_MAGIC       0x3153464a
#define AFS_SUPER_MAGIC       0x5346414F
#define CIFS_SUPER_MAGIC      0xFF534D42

#if !defined(REISERFS_SUPER_MAGIC)
#define REISERFS_SUPER_MAGIC  0x52654973
#endif
#endif // linux

#define LGPFX "FILE:"

#define FILE_TYPE_REGULAR      0
#define FILE_TYPE_DIRECTORY    1
#define FILE_TYPE_BLOCKDEVICE  2
#define FILE_TYPE_CHARDEVICE   3
#define FILE_TYPE_SYMLINK      4
#define FILE_TYPE_FIFO         5
#define FILE_TYPE_SOCKET       6
#define FILE_TYPE_UNCERTAIN    7

typedef struct FileData {
   uint64 fileAccessTime;
   uint64 fileCreationTime;
   uint64 fileModificationTime;
   uint64 fileSize;
   int    fileType;
   int    fileMode;
   int    fileOwner;
   int    fileGroup;
} FileData;

#define FILE_MAX_WAIT_TIME_MS 2000  // maximum wait time in milliseconds

void FileIOResolveLockBits(int *access);

#if defined(_WIN32)
int FileMapErrorToErrno(const char *functionName,
                        Err_Number status);

Bool FileRetryThisError(DWORD error,
                        uint32 numCodes,
                        DWORD *codes);

int FileAttributesRetry(const char *pathName,
                        uint32 maxWaitTimeMsec,
                        FileData *fileData);

int FileDeletionRetry(const char *pathName,
                      Bool handleLink,
                      uint32 maxWaitTimeMsec);

int FileCreateDirectoryRetry(const char *pathName,
                             int mask,
                             uint32 maxWaitTimeMsec);

int FileRemoveDirectoryRetry(const char *pathName,
                             uint32 maxWaitTimeMsec);

int FileListDirectoryRetry(const char *pathName,
                           uint32 maxWaitTimeMsec,
                           char ***ids);

#define FileAttributes(a, b)       FileAttributesRetry((a), 0, (b))
#define FileDeletion(a, b)         FileDeletionRetry((a), (b), 0)
#define FileCreateDirectory(a, b)  FileCreateDirectoryRetry((a), (b), 0)
#define FileRemoveDirectory(a)     FileRemoveDirectoryRetry((a), 0)

#define FileListDirectoryRobust(a, b) \
                    FileListDirectoryRetry((a), FILE_MAX_WAIT_TIME_MS, (b))
#define FileAttributesRobust(a, b) \
                    FileAttributesRetry((a), FILE_MAX_WAIT_TIME_MS, (b))
#define FileRenameRobust(a, b) \
                    File_RenameRetry((a), (b), FILE_MAX_WAIT_TIME_MS)
#define FileDeletionRobust(a, b) \
                    FileDeletionRetry((a), (b), FILE_MAX_WAIT_TIME_MS)
#define FileCreateDirectoryRobust(a, b) \
                    FileCreateDirectoryRetry((a), (b), FILE_MAX_WAIT_TIME_MS)
#define FileRemoveDirectoryRobust(a) \
                    FileRemoveDirectoryRetry((a), FILE_MAX_WAIT_TIME_MS)
#else
static INLINE int
FileMapErrorToErrno(const char *functionName,
                    Err_Number status)
{
   return status;
}

char *FilePosixGetBlockDevice(char const *path);

int FileAttributes(const char *pathName,
                   FileData *fileData);

int FileDeletion(const char *pathName,
                 Bool handleLink);

int FileCreateDirectory(const char *pathName,
                       int mask);

int FileRemoveDirectory(const char *pathName);

#define FileListDirectoryRobust(a, b)    File_ListDirectory((a), (b))
#define FileAttributesRobust(a, b)       FileAttributes((a), (b))
#define FileRenameRobust(a, b)           File_Rename((a), (b))
#define FileDeletionRobust(a, b)         FileDeletion((a), (b))
#define FileCreateDirectoryRobust(a, b)  FileCreateDirectory((a), (b))
#define FileRemoveDirectoryRobust(a)     FileRemoveDirectory((a))
#endif

typedef struct active_lock
{
  struct active_lock *next;
  uint32              age;
  Bool                marked;
  char               *dirName;
} ActiveLock;

typedef struct lock_values
{
   char         *machineID;
   char         *executionID;
   char         *lockType;
   char         *locationChecksum;
   char         *memberName;
   unsigned int  lamportNumber;
   Bool          exclusivity;
   VmTimeType    startTimeMsec;
   uint32        maxWaitTimeMsec;
   ActiveLock   *lockList;
} LockValues;

#include "file_extensions.h"

#define FILELOCK_SUFFIX "." LOCK_FILE_EXTENSION

#define FILELOCK_DATA_SIZE 512

uint32 FileSleeper(uint32 minSleepTimeMsec,
                   uint32 maxSleepTimeMsec);

uint32 FileSimpleRandom(void);

const char *FileLockGetMachineID(void);

char *FileLockGetExecutionID(void);

Bool FileLockMachineIDMatch(const char *host,
                            const char *second);

int FileLockMemberValues(const char *lockDir,
                         const char *fileName,
                         char *buffer,
                         size_t size,
                         LockValues *memberValues);

FileLockToken *FileLockIntrinsic(const char *filePathName,
                                 Bool exclusivity,
                                 uint32 maxWaitTimeMsec,
                                 int *err);

int FileUnlockIntrinsic(FileLockToken *tokenPtr);

Bool FileLockIsLocked(const char *filePath,
                      int *err);

Bool FileLockValidExecutionID(const char *executionID);

Bool FileLockValidName(const char *fileName);

void FileLockAppendMessage(MsgList **msgs,
                           int err);

Bool FileIsWritableDir(const char *dirName);

FileIOResult
FileIOCreateRetry(FileIODescriptor *fd,
                  const char *pathName,
                  int access,
                  FileIOOpenAction action,
                  int mode,
                  uint32 maxWaitTimeMsec);


/*
 * FileIOAligned_* are useful on hosted platforms where malloc/memalign/valloc
 * for "large buffers" (in the range 64 KiB - 1 MiB) will generally fall
 * through to mmap/munmap, which is expensive due to page table modifications
 * and the attendant TLB flushing (which requires IPIs and possibly world
 * switches) on other threads running in the same address space.  In particular,
 * on Mac OS 10.6.6 on a Westmere-class Mac Pro, mmap + memcpy + munmap adds
 * around a millisecond of CPU time and a hundred IPIs to a 512 KiB write.  See
 * PR#685845.
 *
 * This isn't applicable to ESX because
 * 1. we don't use this path for disk IO
 * 2. we don't want to do extra large allocations
 * 3. we don't have the same alignment constraints
 * so simply define it away to nothing.
 *
 * Tools is another case, we can use this path for IO but we don't want to add
 * MXUserExclLock dependencies.
 */

#if defined(VMX86_TOOLS) || defined(VMX86_SERVER)
#define FileIOAligned_PoolInit()     /* nothing */
#define FileIOAligned_PoolExit()     /* nothing */
#define FileIOAligned_PoolMalloc(sz) NULL
#define FileIOAligned_PoolFree(ptr)  FALSE
#else
void FileIOAligned_PoolInit(void);
void FileIOAligned_PoolExit(void);
void *FileIOAligned_PoolMalloc(size_t);
Bool FileIOAligned_PoolFree(void *);
#endif

static INLINE void *
FileIOAligned_Malloc(size_t sz)  // IN:
{
   void *buf = FileIOAligned_PoolMalloc(sz);

   if (!buf) {
      buf = Aligned_Malloc(sz);
   }

   return buf;
}

static INLINE void
FileIOAligned_Free(void *ptr)  // IN:
{
   if (!FileIOAligned_PoolFree(ptr)) {
      Aligned_Free(ptr);
   }
}

#if defined(__APPLE__)
int PosixFileOpener(const char *pathName,
                    int flags,
                    mode_t mode);
#else
#define PosixFileOpener(a, b, c) Posix_Open(a, b, c);
#endif

#endif
