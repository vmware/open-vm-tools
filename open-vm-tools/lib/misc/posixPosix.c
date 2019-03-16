/*********************************************************
 * Copyright (C) 2008-2019 VMware, Inc. All rights reserved.
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

#define UNICODE_BUILDING_POSIX_WRAPPERS

#if __linux__
#define _GNU_SOURCE // Needed to get euidaccess()
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include "su.h"
#include <utime.h>
#include <sys/time.h>
#include <stdarg.h>

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
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#include <spawn.h>
extern char **environ;
#endif
#elif defined(__FreeBSD__)
#include <sys/param.h>
#include <sys/mount.h>
#elif defined(sun)
#include <alloca.h>
#include <sys/mnttab.h>
#else
#include <sys/statfs.h>
#include <sys/mount.h>
#include <mntent.h>
#endif

#if (!defined(__FreeBSD__) || __FreeBSD_release >= 503001) && !defined __ANDROID__
#define VM_SYSTEM_HAS_GETPWNAM_R 1
#define VM_SYSTEM_HAS_GETPWUID_R 1
#define VM_SYSTEM_HAS_GETGRNAM_R 1
#endif

# if defined(__FreeBSD__)
#  include <sys/syslimits.h>  // PATH_MAX
# else
#  include <limits.h>  // PATH_MAX
# endif

#include "vmware.h"
#include "posixInt.h"
#if defined(sun)
#include "hashTable.h" // For setenv emulation
#endif

#include "vm_basic_defs.h"

#if defined __ANDROID__
/*
 * Android doesn't support getmntent_r() or setmntent().
 */
#define NO_GETMNTENT_R
#define NO_SETMNTENT

EXTERN int truncate(const char *, off_t);
#endif


/*
 *----------------------------------------------------------------------
 *
 * Posix_Open --
 *
 *      Open a file using POSIX open.
 *
 * Results:
 *      -1	error
 *      >= 0	success (file descriptor)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Open(const char *pathName,  // IN:
           int flags,             // IN:
           ...)                   // IN:
{
   char *path;
   mode_t mode = 0;
   int fd;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   if ((flags & O_CREAT) != 0) {
      va_list a;

      /*
       * The FreeBSD tools compiler
       * (toolchain/lin32/gcc-4.1.2-5/bin/i686-freebsd5.0-gcc)
       * wants us to use va_arg(a, int) instead of va_arg(a, mode_t),
       * so oblige.  -- edward
       */

      va_start(a, flags);
      ASSERT_ON_COMPILE(sizeof (int) >= sizeof(mode_t));
      mode = va_arg(a, int);
      va_end(a);
   }

   fd = open(path, flags, mode);

   Posix_Free(path);

   return fd;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Creat --
 *
 *      Create a file via POSIX creat()
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
Posix_Creat(const char *pathName,  // IN:
            mode_t mode)           // IN:
{
   return Posix_Open(pathName, O_CREAT | O_WRONLY | O_TRUNC, mode);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Fopen --
 *
 *      Open a file via POSIX fopen()
 *
 * Results:
 *      A file pointer, or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Fopen(const char *pathName,  // IN:
            const char *mode)      // IN:
{
   char *path;
   FILE *stream;

   ASSERT(mode);

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   stream = fopen(path, mode);

   Posix_Free(path);

   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Stat --
 *
 *      POSIX stat()
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
Posix_Stat(const char *pathName,  // IN:
           struct stat *statbuf)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = stat(path, statbuf);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chmod --
 *
 *      POSIX chmod()
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
Posix_Chmod(const char *pathName,  // IN:
            mode_t mode)           // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = chmod(path, mode);

   Posix_Free(path);
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
Posix_Rename(const char *fromPathName,  // IN:
             const char *toPathName)    // IN:
{
   char *toPath;
   char *fromPath;
   int result;

   if (!PosixConvertToCurrent(fromPathName, &fromPath)) {
      return -1;
   }
   if (!PosixConvertToCurrent(toPathName, &toPath)) {
      Posix_Free(fromPath);
      return -1;
   }

   result = rename(fromPath, toPath);

   Posix_Free(toPath);
   Posix_Free(fromPath);
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
Posix_Unlink(const char *pathName)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = unlink(path);

   Posix_Free(path);
   return ret;
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
Posix_Rmdir(const char *pathName)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = rmdir(path);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Freopen --
 *
 *      Open a file via POSIX freopen()
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
Posix_Freopen(const char *pathName,  // IN:
              const char *mode,      // IN:
              FILE *input_stream)    // IN:
{
   char *path;
   FILE *stream;

   ASSERT(mode);

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   stream = freopen(path, mode, input_stream);

   Posix_Free(path);
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
Posix_Access(const char *pathName,  // IN:
             int mode)              // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

#if defined(VMX86_SERVER)
   /*
    * ESX can return EINTR making retries a necessity. This is a bug in
    * ESX that is being worked around here - POSIX says access cannot return
    * EINTR.
    */

   do {
      ret = access(path, mode);
   } while ((ret == -1) && (errno == EINTR));
#else
   ret = access(path, mode);
#endif

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_EuidAccess --
 *
 *      POSIX euidaccess().
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
Posix_EuidAccess(const char *pathName,  // IN:
                 int mode)              // IN:
{
#if defined(__GLIBC__)
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = euidaccess(path, mode);

   Posix_Free(path);
   return ret;
#else
   errno = ENOSYS;
   return -1;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Utime --
 *
 *      POSIX utime().
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
Posix_Utime(const char *pathName,         // IN:
            const struct utimbuf *times)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = utime(path, times);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Perror --
 *
 *      POSIX perror()
 *
 * Results:
 *      Appends error message corresponding to errno to the passed in string,
 *      and puts the result on stderr.
 *
 * Side effects:
 *      Message printed to stderr.
 *
 *----------------------------------------------------------------------
 */

void
Posix_Perror(const char *str)  // IN:
{
   char *tmpstr = Unicode_GetAllocBytes(str, STRING_ENCODING_DEFAULT);

   // ignore conversion error silently
   perror(tmpstr);

   Posix_Free(tmpstr);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Pathconf --
 *
 *      POSIX pathconf()
 *
 * Results:
 *      Returns the limit, -1 if limit doesn't exist or on error
 *
 * Side effects:
 *      errno is set on error.
 *
 *----------------------------------------------------------------------
 */

long
Posix_Pathconf(const char *pathName,  // IN:
               int name)              // IN:
{
   char *path;
   long ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = pathconf(path, name);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Popen --
 *
 *      Open a file using POSIX popen().
 *
 * Results:
 *      Returns a non-NULL FILE* on success, NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Popen(const char *pathName,  // IN:
            const char *mode)      // IN:
{
   char *path;
   FILE *stream;

   ASSERT(mode);

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   stream = popen(path, mode);

   Posix_Free(path);

   return stream;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mknod --
 *
 *      POSIX mknod().
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
Posix_Mknod(const char *pathName,  // IN:
            mode_t mode,           // IN:
            dev_t dev)             // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = mknod(path, mode, dev);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Chown --
 *
 *      POSIX chown().
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
Posix_Chown(const char *pathName,  // IN:
            uid_t owner,           // IN:
            gid_t group)           // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = chown(path, owner, group);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Lchown --
 *
 *      POSIX lchown().
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
Posix_Lchown(const char *pathName,  // IN:
             uid_t owner,           // IN:
             gid_t group)           // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = lchown(path, owner, group);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Link --
 *
 *      POSIX link().
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
Posix_Link(const char *uOldPath,  // IN:
           const char *uNewPath)  // IN:
{
   char *oldPath;
   char *newPath;
   int ret;

   if (!PosixConvertToCurrent(uOldPath, &oldPath)) {
      return -1;
   }
   if (!PosixConvertToCurrent(uNewPath, &newPath)) {
      Posix_Free(oldPath);
      return -1;
   }

   ret = link(oldPath, newPath);

   Posix_Free(oldPath);
   Posix_Free(newPath);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Symlink --
 *
 *      POSIX symlink().
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
Posix_Symlink(const char *uOldPath,  // IN:
              const char *uNewPath)  // IN:
{
   char *oldPath;
   char *newPath;
   int ret;

   if (!PosixConvertToCurrent(uOldPath, &oldPath)) {
      return -1;
   }
   if (!PosixConvertToCurrent(uNewPath, &newPath)) {
      Posix_Free(oldPath);
      return -1;
   }

   ret = symlink(oldPath, newPath);

   Posix_Free(oldPath);
   Posix_Free(newPath);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Mkfifo --
 *
 *      POSIX mkfifo().
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
Posix_Mkfifo(const char *pathName,  // IN:
             mode_t mode)           // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = mkfifo(path, mode);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Truncate --
 *
 *      POSIX truncate().
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
Posix_Truncate(const char *pathName,  // IN:
               off_t length)          // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = truncate(path, length);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Utimes --
 *
 *      POSIX utimes().
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
Posix_Utimes(const char *pathName,         // IN:
             const struct timeval *times)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = utimes(path, times);

   Posix_Free(path);

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
 *      -1      Error
 *      0       Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execl(const char *pathName,  // IN:
            const char *arg0,      // IN:
            ...)                   // IN:
{
   int ret = -1;
   char *path;
   va_list vl;
   char **argv = NULL;
   int i, count = 0;

   if (!PosixConvertToCurrent(pathName, &path)) {
      goto exit;
   }

   if (arg0) {
      count = 1;
      va_start(vl, arg0);
      while (va_arg(vl, char *)) {
         count ++;
      }
      va_end(vl);
   }

   argv = (char **) malloc(sizeof(char *) * (count + 1));
   if (argv == NULL) {
      errno = ENOMEM;
      goto exit;
   }
   if (argv) {
      errno = 0;
      if (count > 0) {
         if (!PosixConvertToCurrent(arg0, &argv[0])) {
            goto exit;
         }
         va_start(vl, arg0);
         for (i = 1; i < count; i++) {
            if (!PosixConvertToCurrent(va_arg(vl, char *), &argv[i])) {
               va_end(vl);
               goto exit;
            }
         }
         va_end(vl);
      }
      argv[count] = NULL;
      if (errno != 0) {
         goto exit;
      }
   }

   ret = execv(path, argv);

exit:
   Util_FreeStringList(argv, -1);
   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execlp --
 *
 *      POSIX execlp().
 *
 * Results:
 *      -1      Error
 *      0       Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

int
Posix_Execlp(const char *fileName,  // IN:
             const char *arg0,      // IN:
             ...)                   // IN:
{
   int ret = -1;
   char *file;
   va_list vl;
   char **argv = NULL;
   int i, count = 0;

   if (!PosixConvertToCurrent(fileName, &file)) {
      goto exit;
   }

   if (arg0) {
      count = 1;
      va_start(vl, arg0);
      while (va_arg(vl, char *)) {
         count ++;
      }
      va_end(vl);
   }

   argv = (char **) malloc(sizeof(char *) * (count + 1));
   if (argv == NULL) {
      errno = ENOMEM;
      goto exit;
   }
   if (argv) {
      errno = 0;
      if (count > 0) {
         if (!PosixConvertToCurrent(arg0, &argv[0])) {
            goto exit;
         }
         va_start(vl, arg0);
         for (i = 1; i < count; i++) {
            if (!PosixConvertToCurrent(va_arg(vl, char *), &argv[i])) {
               va_end(vl);
               goto exit;
            }
         }
         va_end(vl);
      }
      argv[count] = NULL;
      if (errno != 0) {
         goto exit;
      }
   }

   ret = execvp(file, argv);

exit:
   Util_FreeStringList(argv, -1);
   Posix_Free(file);

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
Posix_Execv(const char *pathName,  // IN:
            char *const argVal[])  // IN:
{
   int ret = -1;
   char *path;
   char **argv = NULL;

   if (!PosixConvertToCurrent(pathName, &path)) {
      goto exit;
   }
   if (!PosixConvertToCurrentList(argVal, &argv)) {
      goto exit;
   }

   ret = execv(path, argv);

exit:
   Util_FreeStringList(argv, -1);
   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Execve --
 *
 *      POSIX execve().
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
Posix_Execve(const char *pathName,  // IN:
             char *const argVal[],  // IN:
             char *const envPtr[])  // IN:
{
   int ret = -1;
   char *path;
   char **argv = NULL;
   char **envp = NULL;

   if (!PosixConvertToCurrent(pathName, &path)) {
      goto exit;
   }
   if (!PosixConvertToCurrentList(argVal, &argv)) {
      goto exit;
   }
   if (!PosixConvertToCurrentList(envPtr, &envp)) {
      goto exit;
   }

   ret = execve(path, argv, envp);

exit:
   Util_FreeStringList(argv, -1);
   Util_FreeStringList(envp, -1);
   Posix_Free(path);

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
Posix_Execvp(const char *fileName,   // IN:
             char *const argVal[])  // IN:
{
   int ret = -1;
   char *file;
   char **argv = NULL;

   if (!PosixConvertToCurrent(fileName, &file)) {
      goto exit;
   }
   if (!PosixConvertToCurrentList(argVal, &argv)) {
      goto exit;
   }

   ret = execvp(file, argv);

exit:
   Util_FreeStringList(argv, -1);
   Posix_Free(file);

   return ret;
}


#if TARGET_OS_IPHONE

/*
 *----------------------------------------------------------------------
 *
 * Posix_SplitCommands --
 *
 *      Split the commands into arguments.
 *
 * Results:
 *      The length of the commands and the arguments.
 *
 * Side effects:
 *      NO
 *
 *----------------------------------------------------------------------
 */

int
Posix_SplitCommands(char *command,  // IN
                    char **argv)    // OUT
{
   char *token;
   char *savePtr;
   char *str;
   int count;
   for (count = 0, str = command; ; str = NULL, count++) {
      token = strtok_r(str, " ", &savePtr);
      if (token == NULL) {
         break;
      }
      if (argv != NULL) {
         *argv = token;
         argv++;
      }
   }
   return count;
}

#endif


/*
 *----------------------------------------------------------------------
 *
 * Posix_System --
 *
 *      POSIX system()
 *
 * Results:
 *      Returns the status of command, or -1 on failure.
 *
 * Side effects:
 *      errno is set on error.
 *
 *----------------------------------------------------------------------
 */

int
Posix_System(const char *command)  // IN:
{
   char *tmpcommand;
   int ret;

   if (!PosixConvertToCurrent(command, &tmpcommand)) {
      return -1;
   }

#if TARGET_OS_IPHONE
   pid_t pid;
   int count;
   char **argv;
   char *commandCopy = strdup(command);
   count = Posix_SplitCommands(commandCopy, NULL);
   free(commandCopy);
   if (!count) {
      return -1;
   }
   argv = malloc(count * sizeof(char *));
   commandCopy = strdup(command);
   Posix_SplitCommands(commandCopy, argv);
   ret = posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
   free(argv);
   free(commandCopy);
#else
   ret = system(tmpcommand);
#endif

   Posix_Free(tmpcommand);

   return ret;
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
Posix_Mkdir(const char *pathName,  // IN:
            mode_t mode)           // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = mkdir(path, mode);

   Posix_Free(path);

   return ret;
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
Posix_Chdir(const char *pathName)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = chdir(path);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_RealPath --
 *
 *      POSIX realpath().
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success (result must be freed by the caller)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

char *
Posix_RealPath(const char *pathName)  // IN:
{
   char *path;
   char rpath[PATH_MAX];
   char *p;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   p = realpath(path, rpath);

   Posix_Free(path);

   return p == NULL ? NULL : Unicode_Alloc(rpath, STRING_ENCODING_DEFAULT);
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_ReadLink --
 *
 *      POSIX readlink().
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success (result must be freed by the caller)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

char *
Posix_ReadLink(const char *pathName)  // IN:
{
   char *path = NULL;
   char *result = NULL;

   if (PosixConvertToCurrent(pathName, &path)) {
      size_t size = 2 * 1024;

      while (TRUE) {
         char *linkPath = Util_SafeMalloc(size);
         ssize_t len = readlink(path, linkPath, size);

         if (len == -1) {
            Posix_Free(linkPath);
            break;
         }

         if (len < size) {
            linkPath[len] = '\0'; // Add the missing NUL to path
            result = Unicode_Alloc(linkPath, STRING_ENCODING_DEFAULT);
            Posix_Free(linkPath);
            break;
         }
         Posix_Free(linkPath);

         size += 1024;
      }
   }

   Posix_Free(path);

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Lstat --
 *
 *      POSIX lstat()
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
Posix_Lstat(const char *pathName,  // IN:
            struct stat *statbuf)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = lstat(path, statbuf);

   Posix_Free(path);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_OpenDir --
 *
 *      POSIX opendir()
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

DIR *
Posix_OpenDir(const char *pathName)  // IN:
{
   char *path;
   DIR *ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   ret = opendir(path);

   Posix_Free(path);

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
 *      None
 *
 *----------------------------------------------------------------------
 */

char *
Posix_Getenv(const char *name)  // IN:
{
   char *rawName;
   char *rawValue;

   if (!PosixConvertToCurrent(name, &rawName)) {
      return NULL;
   }
   rawValue = getenv(rawName);
   Posix_Free(rawName);

   if (rawValue == NULL) {
      return NULL;
   }

   return PosixGetenvHash(name, Unicode_Alloc(rawValue,
                                              STRING_ENCODING_DEFAULT));
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Putenv --
 *
 *      POSIX putenv().  This wrapper will only assert the string is ASCII.
 *                       putenv() should not be used.
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      Environment may be changed.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Putenv(char *name)  // IN:
{
   ASSERT(Unicode_IsBufferValid(name, -1, STRING_ENCODING_US_ASCII));

   return putenv(name);
}


#if !defined(sun) // {

/*
 *----------------------------------------------------------------------
 *
 * Posix_Statfs --
 *
 *      POSIX statfs()
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
Posix_Statfs(const char *pathName,      // IN:
             struct statfs *statfsbuf)  // IN:
{
   char *path;
   int ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return -1;
   }

   ret = statfs(path, statfsbuf);

   Posix_Free(path);

   return ret;
}
#endif // } !defined(sun)


/*
 *----------------------------------------------------------------------
 *
 * Posix_Setenv --
 *
 *      POSIX setenv().
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      Environment may be changed.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Setenv(const char *name,   // IN:
             const char *value,  // IN:
             int overWrite)      // IN:
{
   int ret = -1;
   char *rawName = NULL;
   char *rawValue = NULL;

   if (!PosixConvertToCurrent(name, &rawName)) {
      goto exit;
   }
   if (!PosixConvertToCurrent(value, &rawValue)) {
      goto exit;
   }

#if defined(sun)
   if (overWrite || !getenv(rawName)) {
      static HashTable *trackEnv = NULL; // Tracks values to avoid leaks.
      char *keyStr;
      char *fullStr;
      int fslen;
      int rawNameLen;
      int rawValueLen;

      if (!trackEnv) {
         trackEnv = HashTable_Alloc(16, HASH_STRING_KEY, free);
      }

      /*
       * In order to keep memory management and hash table manipulation simple,
       * each env var is stored as a memory block containing the NUL-terminated
       * environment variable name, followed immediately in memory by the
       * full argument to putenv ('varname=value').
       */

      rawNameLen = strlen(rawName) + 1;
      rawValueLen = strlen(rawValue) + 1;
      fslen = rawNameLen + rawValueLen + 1; // 1 is for '=' sign
      keyStr = malloc(rawNameLen + fslen);
      fullStr = keyStr + rawNameLen;

      /*
       * Use memcpy because Str_Snprintf() doesn't play well with non-UTF8
       * strings.
       */

      memcpy(keyStr, rawName, rawNameLen);
      memcpy(fullStr, rawName, rawNameLen);
      fullStr[rawNameLen - 1] = '=';
      memcpy(fullStr + rawNameLen, rawValue, rawValueLen);

      ret = putenv(fullStr);
      HashTable_Insert(trackEnv, keyStr, keyStr); // Any old value will be freed
   } else {
      ret = 0;
   }
#else
   ret = setenv(rawName, rawValue, overWrite);
#endif

exit:
   Posix_Free(rawName);
   Posix_Free(rawValue);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Unsetenv --
 *
 *      POSIX unsetenv().
 *
 * Results:
 *      0 on success. -1 on error.
 *
 * Side effects:
 *      Environment may be changed.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Unsetenv(const char *name)  // IN:
{
   char *rawName;
   int ret;

   if (!PosixConvertToCurrent(name, &rawName)) {
      return -1;
   }

#if defined(sun)
   ret = putenv(rawName);
#elif defined(__FreeBSD__)
   /*
    * Our tools build appears to use an old enough libc version that returns
    * void.
    */
   unsetenv(rawName);
   ret = 0;
#else
   ret = unsetenv(rawName);
#endif
   Posix_Free(rawName);

   return ret;
}


#if !defined(sun) // {

#if !defined(__APPLE__) && !defined(__FreeBSD__) // {
/*
 *----------------------------------------------------------------------
 *
 * Posix_Mount --
 *
 *      POSIX mount()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error. On success, filesystem is mounted.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Mount(const char *source,          // IN:
            const char *target,          // IN:
            const char *filesystemtype,  // IN:
            unsigned long mountflags,    // IN:
            const void *data)            // IN:
{
   int ret = -1;
   char *tmpsource = NULL;
   char *tmptarget = NULL;

   if (!PosixConvertToCurrent(source, &tmpsource)) {
      goto exit;
   }
   if (!PosixConvertToCurrent(target, &tmptarget)) {
      goto exit;
   }

   ret = mount(tmpsource, tmptarget, filesystemtype, mountflags, data);

exit:
   Posix_Free(tmpsource);
   Posix_Free(tmptarget);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Umount --
 *
 *      POSIX umount()
 *
 * Results:
 *      -1	Error
 *      0	Success
 *
 * Side effects:
 *      errno is set on error. On success, filesystem is unmounted.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Umount(const char *target)  // IN:
{
   char *tmptarget;
   int ret;

   if (!PosixConvertToCurrent(target, &tmptarget)) {
      return -1;
   }

   ret = umount(tmptarget);

   Posix_Free(tmptarget);

   return ret;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Setmntent --
 *
 *      Open a file via POSIX setmntent()
 *
 * Results:
 *      NULL	Error
 *      !NULL	File stream
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

FILE *
Posix_Setmntent(const char *pathName,  // IN:
                const char *mode)      // IN:
{
#if defined NO_SETMNTENT
   NOT_IMPLEMENTED();
   errno = ENOSYS;
   return NULL;
#else
   char *path;
   FILE *stream;

   ASSERT(mode != NULL);

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }
   stream = setmntent(path, mode);
   Posix_Free(path);

   return stream;
#endif
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getmntent --
 *
 *      POSIX getmntent()
 *
 * Results:
 *      Pointer to updated mntent struct or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct mntent *
Posix_Getmntent(FILE *fp)  // IN:
{
   int ret;
   struct mntent *m;
   static struct mntent sm = {0};

   m = getmntent(fp);
   if (!m) {
      return NULL;
   }

   /* Free static structure string pointers before reuse. */
   Posix_Free(sm.mnt_fsname);
   sm.mnt_fsname = NULL;
   Posix_Free(sm.mnt_dir);
   sm.mnt_dir = NULL;
   Posix_Free(sm.mnt_type);
   sm.mnt_type = NULL;
   Posix_Free(sm.mnt_opts);
   sm.mnt_opts = NULL;

   /* Fill out structure with new values. */
   sm.mnt_freq = m->mnt_freq;
   sm.mnt_passno = m->mnt_passno;

   ret = ENOMEM;
   if (m->mnt_fsname &&
       (sm.mnt_fsname = Unicode_Alloc(m->mnt_fsname,
                                      STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_dir &&
       (sm.mnt_dir = Unicode_Alloc(m->mnt_dir,
                                   STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_type &&
       (sm.mnt_type = Unicode_Alloc(m->mnt_type,
                                    STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_opts &&
       (sm.mnt_opts = Unicode_Alloc(m->mnt_opts,
                                    STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   ret = 0;

exit:
   if (ret != 0) {
      errno = ret;
      return NULL;
   }

   return &sm;
}


/*
 *----------------------------------------------------------------------
 *
 * Posix_Getmntent_r --
 *
 *      POSIX getmntent_r()
 *
 * Results:
 *      Pointer to updated mntent struct or NULL on error.
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

struct mntent *
Posix_Getmntent_r(FILE *fp,          // IN:
                  struct mntent *m,  // IN:
                  char *buf,         // IN:
                  int size)          // IN:
{
#if defined NO_GETMNTENT_R
   NOT_IMPLEMENTED();
   errno = ENOSYS;
   return NULL;
#else
   int ret;
   char *fsname = NULL;
   char *dir = NULL;
   char *type = NULL;
   char *opts = NULL;
   size_t n;

   if (!getmntent_r(fp, m, buf, size)) {
      return NULL;
   }

   /*
    * Convert strings to UTF-8
    */

   ret = ENOMEM;
   if (m->mnt_fsname &&
       (fsname = Unicode_Alloc(m->mnt_fsname,
                               STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_dir &&
       (dir = Unicode_Alloc(m->mnt_dir,
                            STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_type &&
       (type = Unicode_Alloc(m->mnt_type,
                             STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }
   if (m->mnt_opts &&
       (opts = Unicode_Alloc(m->mnt_opts, STRING_ENCODING_DEFAULT)) == NULL) {
      goto exit;
   }

   /*
    * Put UTF-8 strings into the structure.
    */

   ret = ERANGE;
   n = 0;

   if (fsname) {
      size_t len = strlen(fsname) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      m->mnt_fsname = memcpy(buf + n, fsname, len);
      n += len;
   }

   if (dir != NULL) {
      size_t len = strlen(dir) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      m->mnt_dir = memcpy(buf + n, dir, len);
      n += len;
   }

   if (type) {
      size_t len = strlen(type) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      m->mnt_type = memcpy(buf + n, type, len);
      n += len;
   }

   if (opts) {
      size_t len = strlen(opts) + 1;

      if (n + len > size || n + len < n) {
         goto exit;
      }
      m->mnt_opts = memcpy(buf + n, opts, len);
   }
   ret = 0;

exit:

   Posix_Free(fsname);
   Posix_Free(dir);
   Posix_Free(type);
   Posix_Free(opts);

   if (ret != 0) {
      errno = ret;

      return NULL;
   }

   return m;
#endif // NO_GETMNTENT_R
}


/*
 *----------------------------------------------------------------------------
 *
 * Posix_Printf --
 *
 *      POSIX printf.
 *
 * Returns:
 *      Returns the number of characters printed out or a negative value on
 *      failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
Posix_Printf(const char *format,  // IN:
             ...)                 // IN:
{
   va_list args;
   char *output;
   char *outCurr;
   int numChars;

   va_start(args, format);
   output = Str_Vasprintf(NULL, format, args);
   va_end(args);

   if (!PosixConvertToCurrent(output, &outCurr)) {
      return -1;
   }
   numChars = printf("%s", outCurr);

   Posix_Free(output);
   Posix_Free(outCurr);

   return numChars;
}


/*
 *----------------------------------------------------------------------------
 *
 * Posix_Fprintf --
 *
 *      POSIX fprintf.
 *
 * Returns:
 *      Returns the number of characters printed out or a negative value on
 *      failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
Posix_Fprintf(FILE *stream,        // IN:
              const char *format,  // IN:
              ...)                 // IN:
{
   va_list args;
   char *output;
   char *outCurr;
   int nOutput;

   va_start(args, format);
   output = Str_Vasprintf(NULL, format, args);
   va_end(args);

   if (!PosixConvertToCurrent(output, &outCurr)) {
      return -1;
   }
   nOutput = fprintf(stream, "%s", outCurr);

   Posix_Free(output);
   Posix_Free(outCurr);

   return nOutput;
}


#endif // } !defined(__APPLE__) && !defined(__FreeBSD)


#else  // } !defined(sun) {
/*
 *----------------------------------------------------------------------
 *
 * Posix_Getmntent --
 *
 *      POSIX getmntent() for Solaris
 *
 * Results:
 *      -1  EOF
 *      0   Success
 *      >0  Error
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------
 */

int
Posix_Getmntent(FILE *fp,           // IN:
                struct mnttab *mp)  // IN:
{
   int ret;
   static struct mnttab m = {0};

   ret = getmntent(fp, mp);
   if (ret == 0) {
      Posix_Free(m.mnt_special);
      Posix_Free(m.mnt_mountp);
      Posix_Free(m.mnt_fstype);
      Posix_Free(m.mnt_mntopts);
      Posix_Free(m.mnt_time);
      m.mnt_special = Unicode_Alloc(mp->mnt_special, STRING_ENCODING_DEFAULT);
      m.mnt_mountp = Unicode_Alloc(mp->mnt_mountp, STRING_ENCODING_DEFAULT);
      m.mnt_fstype = Unicode_Alloc(mp->mnt_fstype, STRING_ENCODING_DEFAULT);
      m.mnt_mntopts = Unicode_Alloc(mp->mnt_mntopts, STRING_ENCODING_DEFAULT);
      m.mnt_time = Unicode_Alloc(mp->mnt_time, STRING_ENCODING_DEFAULT);
      mp->mnt_special = m.mnt_special;
      mp->mnt_mountp = m.mnt_mountp;
      mp->mnt_fstype = m.mnt_fstype;
      mp->mnt_mntopts = m.mnt_mntopts;
      mp->mnt_time = m.mnt_time;
   }

   return ret;
}
#endif // } !defined(sun)


/*
 *----------------------------------------------------------------------
 *
 * Posix_MkTemp --
 *
 *      POSIX mktemp().  It is implemented via mkstemp() to avoid
 *      warning about using dangerous mktemp() API - but note that
 *      it suffers from all mktemp() problems - caller has to use
 *      O_EXCL when creating file, and retry if file already exists.
 *
 * Results:
 *      NULL    Error
 *      !NULL   Success (result must be freed by the caller)
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

char *
Posix_MkTemp(const char *pathName)  // IN:
{
   char *result = NULL;
   char *path;
   int fd;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }
   fd = mkstemp(path);
   if (fd >= 0) {
      close(fd);
      unlink(path);
      result = Unicode_Alloc(path, STRING_ENCODING_DEFAULT);
   }
   Posix_Free(path);
   return result;
}
