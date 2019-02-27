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
 * file.h --
 *
 *	Interface to host file system and related utility functions.
 */

#ifndef _FILE_H_
#define _FILE_H_

#define INCLUDE_ALLOW_USERLEVEL
#define INCLUDE_ALLOW_VMCORE
#include "includeCheck.h"

#include <stdio.h>

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

#if defined(__cplusplus)
extern "C" {
#endif

#define FILE_SEARCHPATHTOKEN ";"

#define FILE_UNLINK_DEFAULT_WAIT_MS 2000

/*
 * Opaque, platform-specific stucture for supporting the directory walking API.
 */

typedef struct WalkDirContextImpl WalkDirContextImpl;
typedef WalkDirContextImpl *WalkDirContext;

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

typedef char *File_MakeTempCreateNameFunc(uint32 num,
                                          void *data);

#if defined(__APPLE__)
Bool FileMacos_IsOnSparseDmg(int fd);

Bool FileMacOS_MakeSecureLibraryCopies(const char   *inDir,
                                       const char  **dylibName,
                                       unsigned      numDylibs,
                                       char        **outDir);

#elif defined VMX86_SERVER
struct FS_PartitionListResult;

int File_GetVMFSAttributes(const char *pathName,
                           struct FS_PartitionListResult **fsAttrs);

int File_GetVMFSFSType(const char *pathName,
                       int fd,
                       uint16 *fsTypeNum);

int File_GetVMFSVersion(const char *pathName,
                        uint32 *versionNum);

int File_GetVMFSBlockSize(const char *pathName,
                          uint32 *blockSize);

int File_GetVMFSMountInfo(const char *pathName,
                          char **fsType,
                          uint32 *version,
                          char **remoteIP,
                          char **remoteMountPoint,
                          char **localMountPoint);
#endif

Bool File_SupportsZeroedThick(const char *pathName);

Bool File_SupportsMultiWriter(const char *pathName);

Bool File_SupportsOptimisticLock(const char *pathName);

Bool File_SupportsMandatoryLock(const char *pathName);

Bool File_Exists(const char *pathName);

int File_Unlink(const char *pathName);

int File_UnlinkIfExists(const char *pathName);

int File_UnlinkDelayed(const char *pathName);

int File_UnlinkNoFollow(const char *pathName);

int File_UnlinkRetry(const char *pathName,
                     uint32 maxWaitTimeMilliSec);

void File_SplitName(const char *pathName,
                    char **volume,
                    char **dir,
                    char **base);

void File_GetPathName(const char *fullPath,
                      char **pathName,
                      char **base);

char *File_StripSlashes(const char *path);

char *File_PathJoin(const char *dirName,
                    const char *baseName);

Bool File_CreateDirectory(const char *pathName);

Bool File_CreateDirectoryEx(const char *pathName,
                            int mode);

Bool File_EnsureDirectory(const char *pathName);

Bool File_EnsureDirectoryEx(const char *pathName,
                            int mode);

Bool File_DeleteEmptyDirectory(const char *pathName);

Bool File_CreateDirectoryHierarchy(const char *pathName,
                                   char **topmostCreated);

Bool File_CreateDirectoryHierarchyEx(const char *pathName,
                                     int mode,
                                     char **topmostCreated);

Bool File_DeleteDirectoryContent(const char *pathName);

Bool File_DeleteDirectoryTree(const char *pathName);

int File_ListDirectory(const char *dirName,
                       char ***ids);

Bool File_IsOsfsVolumeEmpty(const char *pathName);

#ifndef _WIN32
char * File_StripFwdSlashes(const char *pathName);
#endif

/*
 * Simple file-system walk.
 */

WalkDirContext File_WalkDirectoryStart(const char *dirName);

Bool File_WalkDirectoryNext(WalkDirContext context,
                            char **fileName);

void File_WalkDirectoryEnd(WalkDirContext context);

Bool File_IsDirectory(const char *pathName);

Bool File_IsFile(const char *pathName);

Bool File_IsSymLink(const char *pathName);

Bool File_ContainSymLink(const char *pathName);

Bool File_IsCharDevice(const char *pathName);

Bool File_GetParent(char **canPath);

Bool File_IsRemote(const char *pathName);

Bool File_IsEmptyDirectory(const char *pathName);

char *File_Cwd(const char *drive); // XXX belongs to `process' module

char *File_FullPath(const char *pathName);

Bool File_IsFullPath(const char *pathName);

uint64 File_GetFreeSpace(const char *pathName,
                         Bool doNotAscend);

uint64 File_GetCapacity(const char *pathName);

#ifdef _WIN32
char *File_GetNTGlobalFinalPath(const char *pathName);
#endif

int File_MakeTempEx(const char *dir,
                    const char *pathName,
                    char **presult);

int File_MakeTempEx2(const char *dir,
                     Bool createTempFile,
                     File_MakeTempCreateNameFunc *createNameFunc,
                     void *createFuncData,
                     char **presult);

char *File_MakeSafeTempDir(const char *prefix);
char *File_MakeSafeTempSubdir(const char *safeDir, const char *subdirName);

int64 File_GetModTime(const char *pathName);

char *File_GetModTimeString(const char *pathName);

char *File_GetUniqueFileSystemID(const char *pathName);

char *File_GetMountPath(const char *pathName,
                        Bool checkEntirePath);

#ifdef _WIN32
char *File_GetVolumeGUID(const char *pathName);
#endif

Bool File_GetTimes(const char *pathName,
                   VmTimeType *createTime,
                   VmTimeType *accessTime,
                   VmTimeType *writeTime,
                   VmTimeType *attrChangeTime);

Bool File_SetTimes(const char *pathName,
                   VmTimeType createTime,
                   VmTimeType accessTime,
                   VmTimeType writeTime,
                   VmTimeType attrChangeTime);

Bool File_GetFilePermissions(const char *pathName,
                            int *mode);

Bool File_SetFilePermissions(const char *pathName,
                             int mode);

Bool File_SupportsFileSize(const char *pathName,
                           uint64 fileSize);

Bool File_GetMaxFileSize(const char *pathName,
                         uint64 *maxFileSize);

Bool File_SupportsLargeFiles(const char *pathName);

char *File_MapPathPrefix(const char *oldPath,
                         const char **oldPrefixes,
                         const char **newPrefixes,
                         size_t numPrefixes);

Bool File_CopyFromFdToFd(FileIODescriptor src,
                         FileIODescriptor dst);

Bool File_CopyFromFd(FileIODescriptor src,
                     const char *dstName,
                     Bool overwriteExisting);

Bool File_Copy(const char *srcName,
               const char *dstName,
               Bool overwriteExisting);

Bool File_MoveTree(const char *srcName,
                   const char *dstName,
                   Bool overwriteExisting,
                   Bool *asMove);

Bool File_CopyTree(const char *srcName,
                   const char *dstName,
                   Bool overwriteExisting,
                   Bool followSymlinks);

Bool File_Replace(const char *oldFile,
                  const char *newFile);

int File_Rename(const char *oldFile,
                const char *newFile);

int File_RenameRetry(const char *oldFile,
                     const char *newFile,
                     uint32 msecMaxWaitTime);

Bool File_Move(const char *oldFile,
               const char *newFile,
               Bool *asRename);

void File_Rotate(const char *pathName,
                 int n,
                 Bool noRename,
                 char **newFileName);

int File_GetFSMountInfo(const char *pathName,
                        char **fsType,
                        uint32 *version,
                        char **remoteIP,
                        char **remoteMountPoint,
                        char **localMountPoint);

/* Get size only for regular file. */
int64 File_GetSize(const char *pathName);

/* Get size for file or directory. */
int64 File_GetSizeEx(const char *pathName);

int64 File_GetSizeByPath(const char *pathName);

int64 File_GetSizeAlternate(const char *pathName);

Bool File_IsSameFile(const char *path1,
                     const char *path2);

char *File_PrependToPath(const char *searchPath,
                         const char *elem);

Bool File_FindFileInSearchPath(const char *file,
                               const char *searchPath,
                               const char *cwd,
                               char **result);

char *File_ReplaceExtension(const char *pathName,
                            const char *newExtension,
                            uint32 numExtensions,
                            ...);

char *File_RemoveExtension(const char *pathName);

Bool File_MakeCfgFileExecutable(const char *pathName);

char *File_ExpandAndCheckDir(const char *dirName);

char *File_GetSafeTmpDir(Bool useConf);

char *File_GetSafeRandomTmpDir(Bool useConf);

int File_MakeSafeTemp(const char *tag,
                      char **presult);

Bool File_DoesVolumeSupportAcls(const char *pathName);

/*
 *---------------------------------------------------------------------------
 *
 * File_IsDirsep --
 *
 *      Is the argument character a directory separator?
 *
 * Results:
 *     TRUE   Yes
 *     FALSE  No
 *
 * Side effects:
 *      None
 *
 *---------------------------------------------------------------------------
 */

static INLINE Bool
File_IsDirsep(int c)  // IN:
{
#if defined(_WIN32)
   return (c == '/') || (c == '\\');  // Until util.h dependencies work out
#else
   return c == '/';
#endif
}


#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // ifndef _FILE_H_
