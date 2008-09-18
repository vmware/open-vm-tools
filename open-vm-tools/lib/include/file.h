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

#ifdef N_PLAT_NLM
#define FILE_MAXPATH	256
#elif defined(_WIN32)
#define FILE_MAXPATH	MAX_PATH
#else
# ifdef __FreeBSD__
#  include <sys/param.h> // For __FreeBSD_version
# endif
# if defined(__FreeBSD__) && __FreeBSD_version >= 503000
#  include <syslimits.h>  // PATH_MAX
# else 
#  include <limits.h>  // PATH_MAX
# endif 
#define FILE_MAXPATH	PATH_MAX
#endif

#define FILE_SEARCHPATHTOKEN ";"

#if defined(__APPLE__)
typedef enum {
   FILEMACOS_UNMOUNT_SUCCESS,
   FILEMACOS_UNMOUNT_SUCCESS_ALREADY,
   FILEMACOS_UNMOUNT_ERROR,
} FileMacosUnmountStatus;

EXTERN FileMacosUnmountStatus FileMacos_UnmountDev(char const *bsdDev,
                                                   Bool wholeDev,
                                                   Bool eject);

EXTERN void FileMacos_MountDevAsyncNoResult(char const *bsdSliceDev,
                                            Bool su);

EXTERN Bool FileMacos_IsOnExternalDevice(int fd);
EXTERN Bool FileMacos_IsOnSparseDmg(int fd);

EXTERN char *FileMacos_DiskDevToDiskName(char const *bsdDiskDev);

EXTERN char *FileMacos_SliceDevToSliceUUID(char const *bsdSliceDev);
EXTERN char *FileMacos_SliceUUIDToSliceDev(char const *uuid);
#endif

EXTERN Bool File_Exists(ConstUnicode pathName);

EXTERN int File_Unlink(ConstUnicode pathName);

EXTERN int File_UnlinkIfExists(ConstUnicode pathName);

EXTERN int File_UnlinkDelayed(ConstUnicode pathName);

EXTERN int File_UnlinkNoFollow(ConstUnicode pathName);

EXTERN void File_SplitName(ConstUnicode pathName,
                           Unicode *volume, 
                           Unicode *dir, 
                           Unicode *base);

EXTERN void File_GetPathName(ConstUnicode fullPath, 
                             Unicode *pathName, 
                             Unicode *base);

EXTERN Unicode File_StripSlashes(ConstUnicode path);

EXTERN Bool File_CreateDirectory(ConstUnicode pathName);
EXTERN Bool File_EnsureDirectory(ConstUnicode pathName);

EXTERN Bool File_DeleteEmptyDirectory(ConstUnicode pathName);

EXTERN Bool File_CreateDirectoryHierarchy(ConstUnicode pathName);

EXTERN Bool File_DeleteDirectoryTree(ConstUnicode pathName);

EXTERN int File_ListDirectory(ConstUnicode pathName,
                              Unicode **ids);

EXTERN Bool File_IsWritableDir(ConstUnicode dirName);

EXTERN Bool File_IsDirectory(ConstUnicode pathName);

EXTERN Bool File_IsFile(ConstUnicode pathName);

EXTERN Bool File_IsSymLink(ConstUnicode fileName);

EXTERN Bool File_IsCharDevice(ConstUnicode pathName);

EXTERN Bool File_IsRemote(ConstUnicode pathName);

EXTERN Bool File_IsEmptyDirectory(ConstUnicode pathName);

EXTERN Unicode File_Cwd(ConstUnicode drive); // XXX belongs to `process' module

EXTERN Unicode File_FullPath(ConstUnicode pathName);

EXTERN Bool File_IsFullPath(ConstUnicode pathName);

EXTERN uint64 File_GetFreeSpace(ConstUnicode pathName);

EXTERN uint64 File_GetCapacity(ConstUnicode pathName);

/* Deprecated; use Util_GetSafeTmpDir if you can */
EXTERN char *File_GetTmpDir(Bool useConf);

/* Deprecated; use Util_MakeSafeTemp if you can */
EXTERN int File_MakeTemp(ConstUnicode tag,
                         Unicode *presult);

EXTERN int File_MakeTempEx(ConstUnicode dir,
                           ConstUnicode fileName,
                           Unicode *presult);

EXTERN int64 File_GetModTime(ConstUnicode pathName);

EXTERN char *File_GetModTimeString(ConstUnicode pathName);

EXTERN char *File_GetUniqueFileSystemID(const char *fileName);

EXTERN Bool File_GetTimes(ConstUnicode pathName,
                          VmTimeType *createTime,
                          VmTimeType *accessTime,
                          VmTimeType *writeTime,
                          VmTimeType *attrChangeTime);

EXTERN Bool File_SetTimes(ConstUnicode pathName,
                          VmTimeType createTime,
                          VmTimeType accessTime,
                          VmTimeType writeTime,
                          VmTimeType attrChangeTime);

EXTERN Bool File_SupportsFileSize(ConstUnicode pathName,
                                  uint64 fileSize);

EXTERN Bool File_SupportsLargeFiles(ConstUnicode pathName);

EXTERN Bool File_CopyFromFdToFd(FileIODescriptor src, 
                                FileIODescriptor dst);

EXTERN FileIOResult File_CreatePrompt(FileIODescriptor *file, 
                                      ConstUnicode pathName, 
                                      int access, 
                                      int prompt);

EXTERN Bool File_CopyFromFd(FileIODescriptor src, 
                            ConstUnicode dstName, 
                            Bool overwriteExisting);

EXTERN Bool File_Copy(ConstUnicode srcName, 
                      ConstUnicode dstName, 
                      Bool overwriteExisting);

EXTERN Bool File_CopyFromFdToName(FileIODescriptor src, 
                                  ConstUnicode dstName, 
                                  int dstDispose);

EXTERN Bool File_CopyFromNameToName(ConstUnicode srcName, 
                                    ConstUnicode dstName, 
                                    int dstDispose);

EXTERN Bool File_Replace(ConstUnicode oldFile, 
                         ConstUnicode newFile);

EXTERN Bool File_Rename(ConstUnicode oldFile, 
                        ConstUnicode newFile);

EXTERN int64 File_GetSize(ConstUnicode pathName);

EXTERN int64 File_GetSizeByPath(ConstUnicode pathName);

EXTERN int64 File_GetSizeAlternate(ConstUnicode pathName);

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

EXTERN void File_PollInit(PollTimeout pt, 
                          PollRemoveTimeout prt);

EXTERN void File_PollExit(void);

EXTERN void File_PollImpersonateOnCheck(Bool check);

EXTERN Bool File_PollAddFile(ConstUnicode pathName, 
                             uint32 pollPeriod, 
                             NotifyCallback callback, 
                             void *data, 
                             Bool fPeriodic);

EXTERN Bool File_PollAddDirFile(ConstUnicode pathName,
                                uint32 pollPeriod, 
                                NotifyCallback callback,
                                void *data, 
                                Bool fPeriodic);

EXTERN Bool File_PollRemoveFile(ConstUnicode pathName, 
                                uint32 pollPeriod,
                                NotifyCallback callback);

EXTERN Bool File_IsSameFile(ConstUnicode path1,
                            ConstUnicode path2);

EXTERN char *File_PrependToPath(const char *searchPath,
                                const char *elem);

EXTERN Bool File_FindFileInSearchPath(const char *file,
                                      const char *searchPath,
                                      const char *cwd,
                                      char **result);

EXTERN Unicode File_ReplaceExtension(ConstUnicode pathName,
                                     ConstUnicode newExtension,
                                     uint32 numExtensions,
                                     ...);

EXTERN Bool File_OnVMFS(ConstUnicode pathName);

EXTERN Bool File_MakeCfgFileExecutable(ConstUnicode pathName);

EXTERN char *File_ExpandAndCheckDir(const char *dirName);

#ifdef __cplusplus
} // extern "C" {
#endif

#endif // ifndef _FILE_H_
