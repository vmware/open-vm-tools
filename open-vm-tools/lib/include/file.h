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
 * file.h --
 *
 *	Interface to host file system and related utility functions.
 */

#ifndef _FILE_H_
#define _FILE_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include "fileIO.h"
#include "unicodeTypes.h"
#include "err.h"

#if defined(_WIN32)
#define FILE_MAXPATH	MAX_PATH
#else
# ifdef __FreeBSD__
#  include <sys/param.h> // For __FreeBSD_version
# endif
# if defined(__FreeBSD__)
#  include <sys/syslimits.h>  // PATH_MAX
# else
#  include <limits.h>  // PATH_MAX
# endif
#define FILE_MAXPATH	PATH_MAX
#endif

#define FILE_SEARCHPATHTOKEN ";"


/*
 * Opaque, platform-specific stucture for supporting the directory walking API.
 */

typedef struct WalkDirContextImpl WalkDirContextImpl;
typedef const WalkDirContextImpl *WalkDirContext;

/*
 * When File_MakeTempEx2 is called, it creates a temporary file or a directory
 * in a specified directory. File_MakeTempEx2 calls a user-specified callback
 * function to get the filename. Callback function should be of type
 * File_MakeTempCreateNameFunc.
 *
 * 'num' specifies nth time this function is called.
 *
 * 'data' specifies the payload that the user specified when executing
 * File_MakeTempEx2 function.
 *
 * If successful, this function should return a dynamically allocated string
 * with the filename.
 *
 * File_MakeTempEx2 frees the pathName after a successful call to this
 * function.
 *
 */

typedef Unicode File_MakeTempCreateNameFunc(int num,
                                            void *data);

#if defined(__APPLE__)
typedef enum {
   FILEMACOS_UNMOUNT_SUCCESS,
   FILEMACOS_UNMOUNT_SUCCESS_ALREADY,
   FILEMACOS_UNMOUNT_ERROR,
} FileMacosUnmountStatus;

FileMacosUnmountStatus FileMacos_UnmountDev(char const *bsdDev,
                                            Bool wholeDev,
                                            Bool eject,
                                            Bool su);

void FileMacos_MountDevAsyncNoResult(char const *bsdDev,
                                     Bool su);

Bool FileMacos_IsOnExternalDevice(int fd);
Bool FileMacos_IsOnSparseDmg(int fd);
Bool FileMacos_IsSliceDevice(char const *bsdDev);

char *FileMacos_DiskDevToUserFriendlyName(char const *bsdDiskDev);

char *FileMacos_DiskDeviceToUniqueID(char const *bsdPath);
char *FileMacos_UniqueIDToDiskDevice(char const *identifier);

#elif defined VMX86_SERVER
int File_GetVMFSVersion(ConstUnicode pathName,
                        uint32 *versionNum);
int File_GetVMFSBlockSize(ConstUnicode pathName,
                          uint32 *blockSize);

int File_GetVMFSMountInfo(ConstUnicode pathName,
                          char **fsType,
                          uint32 *version,
                          char **remoteIP,
                          char **remoteMountPoint,
                          char **localMountPoint);
#endif

Bool File_SupportsZeroedThick(ConstUnicode pathName);

Bool File_SupportsMultiWriter(ConstUnicode pathName);

Bool File_Exists(ConstUnicode pathName);

int File_Unlink(ConstUnicode pathName);

int File_UnlinkIfExists(ConstUnicode pathName);

int File_UnlinkDelayed(ConstUnicode pathName);

int File_UnlinkNoFollow(ConstUnicode pathName);

void File_SplitName(ConstUnicode pathName,
                    Unicode *volume,
                    Unicode *dir,
                    Unicode *base);

void File_GetPathName(ConstUnicode fullPath,
                      Unicode *pathName,
                      Unicode *base);

Unicode File_StripSlashes(ConstUnicode path);

Unicode File_PathJoin(ConstUnicode dirName,
                      ConstUnicode baseName);

Bool File_CreateDirectory(ConstUnicode pathName);
Bool File_EnsureDirectory(ConstUnicode pathName);

Bool File_DeleteEmptyDirectory(ConstUnicode pathName);

Bool File_CreateDirectoryHierarchy(ConstUnicode pathName);

Bool File_DeleteDirectoryTree(ConstUnicode pathName);

int File_ListDirectory(ConstUnicode pathName,
                       Unicode **ids);

/*
 * Simple file-system walk.
 */

WalkDirContext File_WalkDirectoryStart(ConstUnicode parentPath);

Bool File_WalkDirectoryNext(WalkDirContext context,
                            Unicode *path);

void File_WalkDirectoryEnd(WalkDirContext context);

Bool File_IsDirectory(ConstUnicode pathName);

Bool File_IsFile(ConstUnicode pathName);

Bool File_IsSymLink(ConstUnicode pathName);

Bool File_IsCharDevice(ConstUnicode pathName);

Bool File_IsRemote(ConstUnicode pathName);

Bool File_IsEmptyDirectory(ConstUnicode pathName);

Unicode File_Cwd(ConstUnicode drive); // XXX belongs to `process' module

Unicode File_FullPath(ConstUnicode pathName);

Bool File_IsFullPath(ConstUnicode pathName);

uint64 File_GetFreeSpace(ConstUnicode pathName,
                         Bool doNotAscend);

uint64 File_GetCapacity(ConstUnicode pathName);

int File_MakeTempEx(ConstUnicode dir,
                    ConstUnicode pathName,
                    Unicode *presult);

int File_MakeTempEx2(ConstUnicode dir,
                     Bool createTempFile,
                     File_MakeTempCreateNameFunc *createNameFunc,
                     void *createFuncData,
                     Unicode *presult);

int64 File_GetModTime(ConstUnicode pathName);

char *File_GetModTimeString(ConstUnicode pathName);

char *File_GetUniqueFileSystemID(const char *pathName);

Bool File_GetTimes(ConstUnicode pathName,
                   VmTimeType *createTime,
                   VmTimeType *accessTime,
                   VmTimeType *writeTime,
                   VmTimeType *attrChangeTime);

Bool File_SetTimes(ConstUnicode pathName,
                   VmTimeType createTime,
                   VmTimeType accessTime,
                   VmTimeType writeTime,
                   VmTimeType attrChangeTime);

Bool File_GetFilePermissions(ConstUnicode pathName,
                            int *mode);

Bool File_SetFilePermissions(ConstUnicode pathName,
                             int mode);

Bool File_SupportsFileSize(ConstUnicode pathName,
                           uint64 fileSize);

Bool File_SupportsLargeFiles(ConstUnicode pathName);

char *File_MapPathPrefix(const char *oldPath,
                         const char **oldPrefixes,
                         const char **newPrefixes,
                         size_t numPrefixes);

Bool File_CopyFromFdToFd(FileIODescriptor src,
                         FileIODescriptor dst);

FileIOResult File_CreatePrompt(FileIODescriptor *file,
                               ConstUnicode pathName,
                               int access,
                               int prompt);

Bool File_CopyFromFd(FileIODescriptor src,
                     ConstUnicode dstName,
                     Bool overwriteExisting);

Bool File_Copy(ConstUnicode srcName,
               ConstUnicode dstName,
               Bool overwriteExisting);

Bool File_CopyFromFdToName(FileIODescriptor src,
                           ConstUnicode dstName,
                           int dstDispose);

Bool File_CopyFromNameToName(ConstUnicode srcName,
                             ConstUnicode dstName,
                             int dstDispose);

Bool File_MoveTree(ConstUnicode srcName,
                   ConstUnicode dstName,
                   Bool overwriteExisting);

Bool File_CopyTree(ConstUnicode srcName,
                   ConstUnicode dstName,
                   Bool overwriteExisting,
                   Bool followSymlinks);

Bool File_Replace(ConstUnicode oldFile,
                  ConstUnicode newFile);

Bool File_Move(ConstUnicode oldFile,
               ConstUnicode newFile,
               Bool *asRename);

void File_Rotate(const char *pathName,
                 int n,
                 Bool noRename,
                 char **newFileName);

/* Get size only for regular file. */
int64 File_GetSize(ConstUnicode pathName);

/* Get size for file or directory. */
int64 File_GetSizeEx(ConstUnicode pathName);

int64 File_GetSizeByPath(ConstUnicode pathName);

int64 File_GetSizeAlternate(ConstUnicode pathName);

/* file change notification module */
typedef void (*CbFunction)(void *clientData);

typedef void (*NotifyCallback)(ConstUnicode pathName,
                               int err,
                               void *data);

typedef void (*PollTimeout) (CbFunction f,
                             void *clientData,
                             int delay);

typedef void (*PollRemoveTimeout) (CbFunction f,
                                   void *clientData);

void File_PollInit(PollTimeout pt,
                   PollRemoveTimeout prt);

void File_PollExit(void);

void File_PollImpersonateOnCheck(Bool check);

Bool File_PollAddFile(ConstUnicode pathName,
                      uint32 pollPeriod,
                      NotifyCallback callback,
                      void *data,
                      Bool fPeriodic);

Bool File_PollAddDirFile(ConstUnicode pathName,
                         uint32 pollPeriod,
                         NotifyCallback callback,
                         void *data,
                         Bool fPeriodic);

Bool File_PollRemoveFile(ConstUnicode pathName,
                         uint32 pollPeriod,
                         NotifyCallback callback);

Bool File_IsSameFile(ConstUnicode path1,
                     ConstUnicode path2);

char *File_PrependToPath(const char *searchPath,
                         const char *elem);

Bool File_FindFileInSearchPath(const char *file,
                               const char *searchPath,
                               const char *cwd,
                               char **result);

Unicode File_ReplaceExtension(ConstUnicode pathName,
                              ConstUnicode newExtension,
                              uint32 numExtensions,
                              ...);

Unicode File_RemoveExtension(ConstUnicode pathName);

Bool File_MakeCfgFileExecutable(ConstUnicode pathName);

char *File_ExpandAndCheckDir(const char *dirName);

char *File_GetSafeTmpDir(Bool useConf);

int File_MakeSafeTemp(ConstUnicode tag,
                      Unicode *presult);

Bool File_DoesVolumeSupportAcls(ConstUnicode pathName);

#ifdef __cplusplus
} // extern "C" {
#endif

#endif // ifndef _FILE_H_
