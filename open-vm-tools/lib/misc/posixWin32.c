/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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

#include <direct.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <process.h>
#include <stdarg.h>

#include "vmware.h"
#include "posixInt.h"
#include "win32u.h"


/*
 *----------------------------------------------------------------------
 *
 * Posix_Open --
 *
 *      Open a file via POSIX open().
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Open(ConstUnicode pathName,  // IN:
           int flags,              // IN:
           ...)                    // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   mode_t mode = 0;
   int fd;

   WIN32U_CHECK_LONGPATH(path, fd = -1, exit);

   if ((flags & O_CREAT) != 0) {
      va_list a;
      va_start(a, flags);
      mode = va_arg(a, mode_t);
      va_end(a);
   }

   fd = _wopen(path, flags, mode);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Creat --
 *
 *      Create a file via POSIX creat().
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Creat(ConstUnicode pathName,  // IN:
            mode_t mode)            // IN:
{
   return Posix_Open(pathName, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Fopen --
 *
 *      Open a file via POSIX fopen().
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Fopen(ConstUnicode pathName,  // IN:
            const char *mode)       // IN:
{
   const utf16_t *path = NULL;
   const utf16_t *modeString = NULL;
   FILE *stream;

   ASSERT(mode);

   path = UNICODE_GET_UTF16(pathName);
   WIN32U_CHECK_LONGPATH(path, stream = NULL, exit);
   modeString = UNICODE_GET_UTF16(mode);

   stream = _wfopen(path, modeString);

  exit:
   UNICODE_RELEASE_UTF16(path);
   UNICODE_RELEASE_UTF16(modeString);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Popen --
 *
 *      Open a file via POSIX popen().
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Popen(ConstUnicode pathName,  // IN:
            const char *mode)       // IN:
{
   const utf16_t *path = NULL;
   const utf16_t *modeString = NULL;
   FILE *stream;

   ASSERT(mode);

   path = UNICODE_GET_UTF16(pathName);
   WIN32U_CHECK_LONGPATH(path, stream = NULL, exit);
   modeString = UNICODE_GET_UTF16(mode);

   stream = _wpopen(path, modeString);

  exit:
   UNICODE_RELEASE_UTF16(path);
   UNICODE_RELEASE_UTF16(modeString);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chdir --
 *
 *      POSIX chdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Chdir(ConstUnicode pathName)  // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int result;

   WIN32U_CHECK_LONGPATH(path, result = -1, exit);
   result = _wchdir(path);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mkdir --
 *
 *      POSIX mkdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Mkdir(ConstUnicode pathName,  // IN:
            mode_t mode)            // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int result;

   WIN32U_CHECK_LONGPATH(path, result = -1, exit);
   result = _wmkdir(path);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Stat --
 *
 *      POSIX stat().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Stat(ConstUnicode pathName,  // IN:
           struct stat *statbuf)   // IN:
{
   struct _stat _statbuf;
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int ret;

   WIN32U_CHECK_LONGPATH(path, ret = -1, exit);
   ret = _wstat(path, &_statbuf);

  exit:
   UNICODE_RELEASE_UTF16(path);

   if (ret == 0) {
      statbuf->st_dev     = _statbuf.st_dev;
      statbuf->st_ino     = _statbuf.st_ino;
      statbuf->st_mode    = _statbuf.st_mode;
      statbuf->st_nlink   = _statbuf.st_nlink;
      statbuf->st_uid     = _statbuf.st_uid;
      statbuf->st_gid     = _statbuf.st_gid;
      statbuf->st_rdev    = _statbuf.st_rdev;
      statbuf->st_size    = _statbuf.st_size;
      statbuf->st_atime   = _statbuf.st_atime;
      statbuf->st_mtime   = _statbuf.st_mtime;
      statbuf->st_ctime   = _statbuf.st_ctime;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Rename --
 *
 *      POSIX rename().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Rename(ConstUnicode fromPathName,  // IN:
             ConstUnicode toPathName)    // IN:
{
   utf16_t *fromPath = UNICODE_GET_UTF16(fromPathName);
   utf16_t *toPath = UNICODE_GET_UTF16(toPathName);
   int result;

   WIN32U_CHECK_LONGPATH(fromPath, result = -1, exit);
   WIN32U_CHECK_LONGPATH(toPath, result = -1, exit);

   result =  _wrename(fromPath, toPath);

  exit:
   UNICODE_RELEASE_UTF16(fromPath);
   UNICODE_RELEASE_UTF16(toPath);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Unlink --
 *
 *      POSIX unlink().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Unlink(ConstUnicode pathName)  // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int result;

   WIN32U_CHECK_LONGPATH(path, result = -1, exit);
   result = _wunlink(path);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Rmdir --
 *
 *      POSIX rmdir().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Rmdir(ConstUnicode pathName)  // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int result;

   WIN32U_CHECK_LONGPATH(path, result = -1, exit);
   result = _wrmdir(path);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chmod --
 *
 *      POSIX chmod().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Chmod(ConstUnicode pathName,  // IN:
            mode_t mode)            // IN:
{
   const utf16_t *path = UNICODE_GET_UTF16(pathName);
   int result;

   WIN32U_CHECK_LONGPATH(path, result = -1, exit);
   result = _wchmod(path, mode);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Freopen --
 *
 *      Open a file via POSIX freopen().
 *
 * Results:
 *      -1	Error
 *      >= 0	File descriptor (success)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Freopen(ConstUnicode pathName,  // IN:
              const char *mode,       // IN:
              FILE *input_stream)     // IN:
{
   const utf16_t *path;
   const utf16_t *modeString;
   FILE *stream;

   path = UNICODE_GET_UTF16(pathName);
   WIN32U_CHECK_LONGPATH(path, stream = NULL, exit);
   modeString = UNICODE_GET_UTF16(mode);

   stream = _wfreopen(path, modeString, input_stream);

  exit:
   UNICODE_RELEASE_UTF16(path);
   UNICODE_RELEASE_UTF16(modeString);
   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Access --
 *
 *      POSIX access().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Access(ConstUnicode pathName,  // IN:
             int mode)               // IN:
{
   utf16_t *path = UNICODE_GET_UTF16(pathName);
   int ret;

   WIN32U_CHECK_LONGPATH(path, ret = -1, exit);
   ret = _waccess(path, mode);

  exit:
   UNICODE_RELEASE_UTF16(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execl --
 *
 *      POSIX execl().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execl(ConstUnicode pathName,   // IN:
            ConstUnicode arg0, ...)  // IN:
{
   int ret = -1;
   utf16_t *path = UNICODE_GET_UTF16(pathName);
   utf16_t **argv = NULL;
   va_list vl;
   int i, count = 0;

   WIN32U_CHECK_LONGPATH(path, ret = -1, exit);

   if (arg0) {
      count = 1;
      va_start(vl, arg0);
      while(va_arg(vl, utf16_t *)) {
         count ++;
      }
      va_end(vl);
   }

   argv = (utf16_t **)malloc(sizeof(utf16_t *) * (count + 1));
   if (argv) {
      if (count > 0) {
         argv[0] = UNICODE_GET_UTF16(arg0);
         va_start(vl, arg0);
         for (i = 1; i < count; i++) {
            argv[i] = UNICODE_GET_UTF16(va_arg(vl, char *));
         }
         va_end(vl);
      }
      argv[count] = NULL;
   } else {
      errno = ENOMEM;
      goto exit;
   }

   ret = _wexecv(path, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         UNICODE_RELEASE_UTF16(argv[i]);
      }
      free(argv);
   }
   UNICODE_RELEASE_UTF16(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execv --
 *
 *      POSIX execv().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execv(ConstUnicode pathName,        // IN:
            Unicode const argVal[])       // IN:
{
   int ret = -1;
   utf16_t *path = UNICODE_GET_UTF16(pathName);
   utf16_t **argv = NULL;
   int i, count = 0;

   WIN32U_CHECK_LONGPATH(path, ret = -1, exit);

   if (argVal) {
      while (argVal[count]) {
         count++;
      }
      argv = (utf16_t **)malloc(sizeof(utf16_t *) * (count + 1));
      if (argv) {
         for (i = 0; i < count; i++) {
            argv[i] = UNICODE_GET_UTF16(argVal[i]);
         }
         argv[count] = NULL;
      } else {
         errno = ENOMEM;
         goto exit;
      }
   }

   ret = _wexecv(path, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         UNICODE_RELEASE_UTF16(argv[i]);
      }
      free(argv);
   }
   UNICODE_RELEASE_UTF16(path);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execvp --
 *
 *      POSIX execvp().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execvp(ConstUnicode fileName,        // IN:
             Unicode const argVal[])       // IN:
{
   int ret = -1;
   utf16_t *file = UNICODE_GET_UTF16(fileName);
   utf16_t **argv = NULL;
   int i, count = 0;

   WIN32U_CHECK_LONGPATH(file, ret = -1, exit);

   if (argVal) {
      while (argVal[count]) {
         count++;
      }
      argv = (utf16_t **)malloc(sizeof(utf16_t *) * (count + 1));
      if (argv) {
         for (i = 0; i < count; i++) {
            argv[i] = UNICODE_GET_UTF16(argVal[i]);
         }
         argv[count] = NULL;
      } else {
         errno = ENOMEM;
         goto exit;
      }
   }

   ret = _wexecvp(file, argv);

exit:
   if (argv) {
      for (i = 0; i < count; i++) {
         UNICODE_RELEASE_UTF16(argv[i]);
      }
      free(argv);
   }
   UNICODE_RELEASE_UTF16(file);
   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getenv --
 *
 *      POSIX getenv().
 *
 * Results:
 *      NULL    The name was not found or an error occurred
 *      !NULL   The value associated with the name in UTF8. This does not
 *              need to be freed.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

Unicode
Posix_Getenv(ConstUnicode name)  // IN:
{
   utf16_t *rawName;
   utf16_t *rawValue;

   rawName = UNICODE_GET_UTF16(name);
   rawValue = _wgetenv(rawName);
   UNICODE_RELEASE_UTF16(rawName);

   if (rawValue == NULL) {
      return NULL;
   }

   return PosixGetenvHash(name, Unicode_AllocWithUTF16(rawValue));
}
