/*********************************************************
 * Copyright (C) 2008-2017 VMware, Inc. All rights reserved.
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

#define _GNU_SOURCE
#define UNICODE_BUILDING_POSIX_WRAPPERS
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#if !defined(_WIN32)
#include <dlfcn.h>
#include <sys/stat.h>
#endif

#include "vmware.h"
#include "posixInt.h"


#if defined(__linux__) && !defined(VMX86_SERVER) && !defined(VMX86_DEVEL)

void *dlopen(const char *pathName, int flag) __attribute__((alias("Root_Dlopen")));

static void *(*realDlopen)(const char *filename, int flag);

/*
 *----------------------------------------------------------------------
 *
 * Root_Dlopen --
 *
 *      PR 1817345: only allow dlopen a library that meets the following:
 *         - file is owned by the root
 *         - directory is owned by the root, and not others-writable
 *
 * Results:
 *      NULL    doesn't meet the above constraints or dlopen failure
 *      !NULL   Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

void *
Root_Dlopen(const char *pathName,  // IN:
            int flag)              // IN:

{
   struct stat sb;
   char *realName, *p;
   void *handle = NULL;
   char savec;

   if (UNLIKELY(realDlopen == NULL)) {
      realDlopen = dlsym(RTLD_NEXT, "dlopen");
      VERIFY(realDlopen != NULL);
   }

   if (pathName == NULL || strchr(pathName, '/') == NULL) {
      return realDlopen(pathName, flag);
   }

   realName = Posix_RealPath(pathName);
   if (!realName) {
      Log("Fail to realpath: %s, errno=%d\n", pathName, errno);
      goto out;
   }

   /* verify the file */
   if (Posix_Stat(realName, &sb) != 0) {
      Log("Fail to stat file: %s, errno=%d\n", realName, errno);
      goto err;
   }
   if (sb.st_uid != 0) {
      Log("File not root-owned: %s, id=%d\n", realName, sb.st_uid);
      goto err;
   }

   /* verify the directory */
   if ((p = strrchr(realName, '/')) == NULL) {
      Log("Fail to find dir: %s\n", realName);
      goto err;
   }
   savec = *(p + 1);
   *(p + 1) = '\0';

   if (Posix_Stat(realName, &sb) != 0) {
      Log("Fail to stat dir: %s, errno=%d\n", realName, errno);
      goto err;
   }

   if (sb.st_uid != 0 || (sb.st_mode & S_IWOTH)) {
      Log("Dir not root-owned or others-writable: %s, id=%d mode=0x%x\n",
          realName, sb.st_uid, sb.st_mode);
      goto err;
   }

   *(p + 1) = savec;  /* restore file realName */
   handle = (*realDlopen)(realName, flag);
   free(realName);
   return handle;
err:
   free(realName);
out:
   Log("Denied library: %s\n", pathName);
   return realDlopen("/dev/null/dlopen/denied", 0);
}
#endif


#if !defined(_WIN32)
/*
 *----------------------------------------------------------------------
 *
 * Posix_Dlopen --
 *
 *      POSIX dlopen()
 *
 * Results:
 *      NULL	Error
 *      !NULL	Success
 *
 * Side effects:
 *      errno is set on error
 *
 *----------------------------------------------------------------------
 */

void *
Posix_Dlopen(const char *pathName,  // IN:
             int flag)              // IN:
{
   char *path;
   void *ret;

   if (!PosixConvertToCurrent(pathName, &path)) {
      return NULL;
   }

   ret = dlopen(path, flag);

   Posix_Free(path);
   return ret;
}
#endif
