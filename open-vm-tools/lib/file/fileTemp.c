/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 *  File_MakeTempEx2 --
 *
 *      Create a temporary file or a directory.
 *      If a temporary file is created successfully, then return an open file
 *      descriptor to that file.
 *
 *      'dir' specifies the directory in which to create the file. It
 *      must not end in a slash.
 *
 *      'createTempFile', if TRUE, then a temporary file will be created. If
 *      FALSE, then a temporary directory will be created.
 *
 *      'createNameFunc' specifies the user-specified callback function that
 *      will be called to construct the fileName. 'createNameFuncData' will be
 *      passed everytime 'createNameFunc' is called. 'createNameFunc'
 *      should return the proper fileName.
 *
 *      Check the documentation for File_MakeTempHelperFunc.
 *
 * Results:
 *      if a temporary file is created, then Open file descriptor or -1;
 *      if a temporary directory is created, then 0 or -1;
 *      If successful then presult points to a dynamically allocated
 *      string with the pathname of the temp file.
 *
 * Side effects:
 *      Creates a file if successful. Errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
File_MakeTempEx2(ConstUnicode dir,                             // IN:
                 Bool createTempFile,                          // IN:
                 File_MakeTempCreateNameFunc *createNameFunc,  // IN:
                 void *createNameFuncData,                     // IN:
                 Unicode *presult)                             // OUT:
{
   int fd = -1;
   int var;
   uint32 i;

   Unicode path = NULL;

   if ((dir == NULL) || (createNameFunc == NULL)) {
      errno = EFAULT;
      return -1;
   }

   ASSERT(presult);

   *presult = NULL;

   for (i = 0, var = 0; i < 0xFFFFFFFF;
        i++, var += (FileSimpleRandom() >> 8) & 0xFF) {
      Unicode fileName;

      /* construct suffixed pathname to use */
      Unicode_Free(path);
      path = NULL;

      fileName = (*createNameFunc)(var, createNameFuncData);
      ASSERT(fileName);

      /* construct base full pathname to use */
      path = Unicode_Join(dir, DIRSEPS, fileName, NULL);

      Unicode_Free(fileName);

      if (createTempFile) {
         fd = Posix_Open(path, O_CREAT | O_EXCL | O_BINARY | O_RDWR, 0600);
#if defined(_WIN32)
         /*
          * On Windows, Posix_Open() fails with EACCES if there is any
          * access violation while creating the file. Also, EACCES is returned
          * if a directory already exists with the same name. In such case,
          * we need to check if a file already exists and ignore EACCES error.
          */

         if ((fd == -1) && (errno == EACCES) && (File_Exists(path))) {
            continue;
         }
#endif
      } else {
         fd = Posix_Mkdir(path, 0600);
      }

      if (fd != -1) {
         *presult = path;
         path = NULL;
         break;
      }

      if (errno != EEXIST) {
         Log(LGPFX" Failed to create temporary %s \"%s\": %s.\n",
             createTempFile ? "file" : "directory",
             UTF8(path), strerror(errno));
         goto exit;
      }
   }

   if (fd == -1) {
      Warning(LGPFX" Failed to create temporary %s \"%s\": "
              "The name space is full.\n",
              createTempFile ? "file" : "directory", UTF8(path));

      errno = EAGAIN;
   }

  exit:
   Unicode_Free(path);

   return fd;
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
 *      function is called. This points to a Unicode string.
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

static Unicode
FileMakeTempExCreateNameFunc(int num,     // IN:
                             void *data)  // IN:
{
   Unicode filePath;

   if (data == NULL) {
      return NULL;
   }

   filePath = Unicode_Format("%s%d", (Unicode) data, num);

   return filePath;
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
File_MakeTempEx(ConstUnicode dir,       // IN:
                ConstUnicode fileName,  // IN:
                Unicode *presult)       // OUT:
{
   return File_MakeTempEx2(dir, TRUE, FileMakeTempExCreateNameFunc,
                           (void *) fileName, presult);
}


/*
 *----------------------------------------------------------------------
 *
 * File_MakeSafeTemp --
 *
 *      Exactly the same as File_MakeTemp except uses a safe directory
 *      as the default temporary directory.
 *
 * Results:
 *      Open file descriptor or -1
 *
 * Side effects:
 *      Creates a file if successful.
 *----------------------------------------------------------------------
 */

int
File_MakeSafeTemp(ConstUnicode tag,  // IN (OPT):
                  Unicode *presult)  // OUT:
{
   int fd;
   Unicode dir = NULL;
   Unicode fileName = NULL;

   ASSERT(presult);

   *presult = NULL;

   if (tag && File_IsFullPath(tag)) {
      File_GetPathName(tag, &dir, &fileName);
   } else {
      dir = File_GetSafeTmpDir(TRUE);
      fileName = Unicode_Duplicate(tag ? tag : "vmware");
   }

   fd = File_MakeTempEx(dir, fileName, presult);

   Unicode_Free(dir);
   Unicode_Free(fileName);

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
File_DoesVolumeSupportAcls(ConstUnicode path)  // IN:
{
   Bool succeeded = FALSE;

#if defined(_WIN32)
   Bool res;
   Unicode vol, vol2;
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
   Unicode_Free(vol);
   Unicode_Free(vol2);
#endif

   return succeeded;
}
