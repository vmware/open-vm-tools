/*********************************************************
 * Copyright (C) 2011-2020 VMware, Inc. All rights reserved.
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

#if defined(_WIN32)
#include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#include "vmware.h"
#include "log.h"
#include "file.h"
#include "fileInt.h"
#include "util.h"
#include "unicodeOperations.h"
#include "posix.h"

#if !defined(O_BINARY)
#define O_BINARY 0
#endif


/*
 *----------------------------------------------------------------------
 *
 *  FileTempNum --
 *
 *      Compute a number to be used as an attachment to a base name.
 *      In order to avoid race conditions, files and directories are
 *      kept separate via enforced odd (file) and even (directory)
 *      numberings.
 *
 *      Regardless of the input value of *var, the output value will
 *      be even or odd as determined by createTempFile.
 *
 * Results:
 *      An odd number if createTempFile is TRUE.
 *      An even number if createTempFile is FALSE.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static void
FileTempNum(Bool createTempFile,  // IN:
            uint32 *var)          // IN/OUT:
{
   ASSERT(var != NULL);

   *var += (FileSimpleRandom() >> 8) & 0xFF;
   *var = (*var & ~0x1) | (createTempFile ? 1 : 0);
}


/*
 *----------------------------------------------------------------------
 *
 *  FileMakeTempEx2Work --
 *
 *      Create a temporary file or a directory.
 *
 *      If a temporary file is created successfully, then return an open file
 *      descriptor to that file.
 *
 *      'dir' specifies the directory in which to create the object. It
 *      must not end in a slash.
 *
 *      'createTempFile', if TRUE, then a temporary file will be created. If
 *      FALSE, then a temporary directory will be created.
 *
 *      'makeSubdirSafe', if TRUE, then if a directory is requested,
 *      the directory will be made "safe". This also requires that
 *      'dir' already be safe (the code will check this).
 *
 *      'createNameFunc' specifies the user-specified callback function that
 *      will be called to construct a fileName. 'createNameFuncData' will be
 *      passed everytime 'createNameFunc' is called. 'createNameFunc'
 *      should return the dynamically allocated, proper fileName.
 *
 *      Check the documentation for File_MakeTempHelperFunc.
 *
 * Results:
 *      if a temporary file is created, then Open file descriptor or -1;
 *      if a temporary directory is created, then 0 or -1;
 *      If successful then presult points to a dynamically allocated
 *      string with the pathname of the temp object created.
 *
 * Side effects:
 *      Creates the requested object when successful. Errno is set on error
 *
 *      Files and directories are effectively in separate name spaces;
 *      the numerical value attached via createNameFunc is odd for files
 *      and even for directories.
 *
 *----------------------------------------------------------------------
 */

static int
FileMakeTempEx2Work(const char *dir,                              // IN:
                    Bool createTempFile,                          // IN:
                    Bool makeSubdirSafe,                          // IN:
                    File_MakeTempCreateNameFunc *createNameFunc,  // IN:
                    void *createNameFuncData,                     // IN:
                    char **presult)                               // OUT:
{
   uint32 i;

   int fd;
   uint32 var = 0;

   ASSERT(presult != NULL);

   if ((dir == NULL) || (createNameFunc == NULL)) {
      errno = EFAULT;

      return -1;
   }

   *presult = NULL;

   for (i = 0; i < (MAX_INT32 / 2); i++) {
      char *objName;
      char *pathName;

      /*
       * Files and directories are kept separate (odd and even respectfully).
       * This way the available exclusion mechanisms work properly - O_EXCL
       * on files, mkdir on directories - and races are avoided.
       *
       * Not attempting an open on a directory is a good thing...
       */

      FileTempNum(createTempFile, &var);

      objName = (*createNameFunc)(var, createNameFuncData);
      ASSERT(objName != NULL);

      if (createTempFile) {
         pathName = File_PathJoin(dir, objName);
         fd = Posix_Open(pathName, O_CREAT | O_EXCL | O_BINARY | O_RDWR, 0600);
      } else {
         if (makeSubdirSafe) {
            pathName = File_MakeSafeTempSubdir(dir, objName);
            fd = (pathName == NULL) ? -1 : 0;
         } else {
            pathName = File_PathJoin(dir, objName);
            fd = Posix_Mkdir(pathName, 0700);
         }
      }

      if (fd != -1) {
         *presult = pathName;

         Posix_Free(objName);
         break;
      }

      Posix_Free(pathName);

      if (errno != EEXIST) {
         Log(LGPFX" Failed to create temporary %s; dir \"%s\", "
             "objName \"%s\", errno %d\n",
             createTempFile ? "file" : "directory",
             dir, objName, errno);

         Posix_Free(objName);
         goto exit;
      }

      Posix_Free(objName);
   }

   if (fd == -1) {
      Warning(LGPFX" Failed to create temporary %s: "
              "The name space is full.\n",
              createTempFile ? "file" : "directory");

      errno = EAGAIN;
   }

  exit:

   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 *  File_MakeTempEx2 --
 *
 *      Same as FileMakeTempEx2Int, defaulting 'makeSubdirSafe' to
 *      FALSE.
 *
 * Results:
 *      See FileMakeTempEx2Int.
 *
 * Side effects:
 *      See FileMakeTempEx2Int.
 *
 *----------------------------------------------------------------------
 */

int
File_MakeTempEx2(const char *dir,                              // IN:
                 Bool createTempFile,                          // IN:
                 File_MakeTempCreateNameFunc *createNameFunc,  // IN:
                 void *createNameFuncData,                     // IN:
                 char **presult)                               // OUT:
{
   return FileMakeTempEx2Work(dir, createTempFile, FALSE, createNameFunc,
                              createNameFuncData, presult);
}


/*
 *----------------------------------------------------------------------------
 *
 * FileMakeTempExCreateNameFunc --
 *
 *      This is a helper function designed for File_MakeTempEx function.
 *      Everytime this function is called, this creates a fileName with the
 *      format <num><fileName> and returns back to the caller.
 *
 *      'num' specifies the nth time this function is called.
 *
 *      'data' specifies the payload that is specified when File_MakeTempEx2()
 *      function is called. This points to a UTF8 string.
 *
 * Results:
 *      if successful, a dynamically allocated string with the basename of
 *      the temp file. NULL otherwise.
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------------
 */

static char *
FileMakeTempExCreateNameFunc(uint32 num,  // IN:
                             void *data)  // IN:
{
   if (data == NULL) {
      return NULL;
   }

   return Unicode_Format("%s%u", (char *) data, num);
}


/*
 *----------------------------------------------------------------------
 *
 *  File_MakeTempEx --
 *
 *      Create a temporary file and, if successful, return an open file
 *      descriptor to that file.
 *
 *      'dir' specifies the directory in which to create the file. It
 *      must not end in a slash.
 *
 *      'fileName' specifies the base filename of the created file.
 *
 * Results:
 *      Open file descriptor or -1; if successful then presult points
 *      to a dynamically allocated string with the pathname of the temp
 *      file.
 *
 * Side effects:
 *      Creates a file if successful. Errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
File_MakeTempEx(const char *dir,       // IN:
                const char *fileName,  // IN:
                char **presult)        // OUT:
{
   return File_MakeTempEx2(dir, TRUE, FileMakeTempExCreateNameFunc,
                           (void *) fileName, presult);
}


/*
 *----------------------------------------------------------------------
 *
 *  File_MakeSafeTempDir --
 *
 *      Create a temporary directory in a safe area.
 *
 *      Optional argument 'prefix' specifies the name prefix of the
 *      created directory. When not provided a default will be provided.
 *
 * Results:
 *      NULL  Failure
 *     !NULL  Path name of created directory
 *
 * Side effects:
 *      Creates a directory if successful. Errno is set on error
 *
 *----------------------------------------------------------------------
 */

char *
File_MakeSafeTempDir(const char *prefix)  // IN:
{
   char *result = NULL;
   char *dir = File_GetSafeTmpDir(TRUE);

   if (dir != NULL) {
      const char *effectivePrefix = (prefix == NULL) ? "safeDir" : prefix;

      FileMakeTempEx2Work(dir, FALSE, TRUE, FileMakeTempExCreateNameFunc,
                          (void *) effectivePrefix, &result);

      Posix_Free(dir);
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * File_MakeSafeTemp --
 *
 *      Exactly the same as File_MakeTempEx except uses a safe directory
 *      as the default temporary directory.
 *
 * Results:
 *      Open file descriptor or -1
 *
 * Side effects:
 *      Creates a file if successful.
 *
 *----------------------------------------------------------------------
 */

int
File_MakeSafeTemp(const char *tag,  // IN (OPT):
                  char **presult)   // OUT:
{
   int fd;
   char *dir = NULL;
   char *fileName = NULL;

   ASSERT(presult);

   *presult = NULL;

   if (tag && File_IsFullPath(tag)) {
      File_GetPathName(tag, &dir, &fileName);
   } else {
      dir = File_GetSafeTmpDir(TRUE);
      fileName = Unicode_Duplicate(tag ? tag : "vmware");
   }

   fd = File_MakeTempEx(dir, fileName, presult);

   Posix_Free(dir);
   Posix_Free(fileName);

   return fd;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_DoesVolumeSupportAcls --
 *
 *    Determines if the volume that the pathname resides on supports
 *    ACLs.
 *
 * Results:
 *    TRUE   it does
 *    FALSE  it doesn't
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

Bool
File_DoesVolumeSupportAcls(const char *path)  // IN:
{
   Bool succeeded = FALSE;

#if defined(_WIN32)
   Bool res;
   char *vol;
   char *vol2;
   const utf16_t *vol2W;
   DWORD fsFlags;

   ASSERT(path);

   File_SplitName(path, &vol, NULL, NULL);
   vol2 = Unicode_Append(vol, DIRSEPS);

   vol2W = UNICODE_GET_UTF16(vol2);
   res = GetVolumeInformationW(vol2W, NULL, 0, NULL, NULL, &fsFlags, NULL, 0);
   UNICODE_RELEASE_UTF16(vol2W);

   if (res) {
      if ((fsFlags & FS_PERSISTENT_ACLS) == 0) {
         goto exit;
      }
   } else {
      Log(LGPFX" %s: GetVolumeInformation failed: %d\n", __FUNCTION__,
          GetLastError());
      goto exit;
   }

   succeeded = TRUE;

  exit:
   Posix_Free(vol);
   Posix_Free(vol2);
#endif

   return succeeded;
}
