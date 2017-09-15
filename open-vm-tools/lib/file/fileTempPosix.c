/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
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

#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>

#if !defined(__FreeBSD__) && !defined(sun)
#   include <pwd.h>
#endif

#include "vmware.h"
#include "file.h"
#include "fileInt.h"
#include "util.h"
#include "su.h"
#include "vm_atomic.h"
#include "str.h"
#include "vm_product.h"
#include "random.h"
#include "userlock.h"
#include "unicodeOperations.h"
#include "err.h"
#include "posix.h"
#include "mutexRankLib.h"
#include "hostType.h"
#include "localconfig.h"

#define LOGLEVEL_MODULE util
#include "loglevel_user.h"


/*
 *----------------------------------------------------------------------
 *
 * FileTryDir --
 *
 *	Check to see if the given directory is actually a directory
 *      and is writable by us.
 *
 * Results:
 *	The expanded directory name on success, NULL on failure.
 *
 * Side effects:
 *	The result is allocated.
 *
 *----------------------------------------------------------------------
 */

static char *
FileTryDir(const char *dirName)  // IN: Is this a writable directory?
{
   char *edirName;

   if (dirName != NULL) {
      edirName = Util_ExpandString(dirName);

      if ((edirName != NULL) && FileIsWritableDir(edirName)) {
         return edirName;
      }

      free(edirName);
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * FileGetTmpDir --
 *
 *	Determine the best temporary directory. Unsafe since the
 *	returned directory is generally going to be 0777, thus all sorts
 *	of denial of service or symlink attacks are possible.
 *
 * Results:
 *	NULL if error (reported to the user).
 *
 * Side effects:
 *	The result is allocated.
 *
 *----------------------------------------------------------------------
 */

static char *
FileGetTmpDir(Bool useConf)  // IN: Use the config file?
{
   char *dirName;
   char *edirName;

   /* Make several attempts to find a good temporary directory candidate */

   if (useConf) {
      dirName = (char *)LocalConfig_GetString(NULL, "tmpDirectory");
      edirName = FileTryDir(dirName);
      free(dirName);
      if (edirName != NULL) {
         return edirName;
      }
   }

   /* Posix_Getenv string must _not_ be freed */
   edirName = FileTryDir(Posix_Getenv("TMPDIR"));
   if (edirName != NULL) {
      return edirName;
   }

   /* P_tmpdir is usually defined in <stdio.h> */
   edirName = FileTryDir(P_tmpdir);
   if (edirName != NULL) {
      return edirName;
   }

   edirName = FileTryDir("/tmp");
   if (edirName != NULL) {
      return edirName;
   }

   edirName = FileTryDir("~");
   if (edirName != NULL) {
      return edirName;
   }

   dirName = File_Cwd(NULL);

   if (dirName != NULL) {
      edirName = FileTryDir(dirName);
      free(dirName);
      if (edirName != NULL) {
         return edirName;
      }
   }

   edirName = FileTryDir("/");
   if (edirName != NULL) {
      return edirName;
   }

   Warning("%s: Couldn't get a temporary directory\n", __FUNCTION__);
   return NULL;
}

#undef HOSTINFO_TRYDIR


#if !defined(__FreeBSD__) && !defined(sun)
/*
 *-----------------------------------------------------------------------------
 *
 * FileGetUserName --
 *
 *      Retrieve the name associated with a user ID. Thread-safe
 *      version. --hpreg
 *
 * Results:
 *      The allocated name on success
 *      NULL on failure
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileGetUserName(uid_t uid)  // IN:
{
   char *memPool;
   char *userName;
   struct passwd pw;
   struct passwd *pw_p;
   long memPoolSize;

#if defined(__APPLE__)
   memPoolSize = _PASSWORD_LEN;
#else
   memPoolSize = sysconf(_SC_GETPW_R_SIZE_MAX);

   if (memPoolSize <= 0) {
      Warning("%s: sysconf(_SC_GETPW_R_SIZE_MAX) failed.\n", __FUNCTION__);

      return NULL;
   }
#endif

   memPool = malloc(memPoolSize);
   if (memPool == NULL) {
      Warning("%s: Not enough memory.\n", __FUNCTION__);

      return NULL;
   }

   if ((Posix_Getpwuid_r(uid, &pw, memPool, memPoolSize, &pw_p) != 0) ||
       pw_p == NULL) {
      free(memPool);
      Warning("%s: Unable to retrieve the username associated with "
              "user ID %u.\n", __FUNCTION__, uid);

      return NULL;
   }

   userName = strdup(pw_p->pw_name);
   free(memPool);
   if (userName == NULL) {
      Warning("%s: Not enough memory.\n", __FUNCTION__);

      return NULL;
   }

   return userName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileAcceptableSafeTmpDir --
 *
 *      Determines if the specified path is acceptable as the safe
 *      temp directory.  The directory must either be creatable
 *      with the appropriate permissions and userId or it must
 *      already exist with those settings.
 *
 * Results:
 *      TRUE if path is acceptible, FALSE otherwise
 *
 * Side effects:
 *      Directory may be created
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileAcceptableSafeTmpDir(const char *dirname,  // IN:
                         int userId)           // IN:
{
   Bool result;
   static const mode_t mode = 0700;

   result = (Posix_Mkdir(dirname, mode) == 0);
   if (!result) {
      int error = errno;

      if (EEXIST == error) {
         struct stat st;

         /*
          * The name already exists. Check that it is what we want: a
          * directory owned by the current effective user with permissions
          * 'mode'. It is crucial to use lstat() instead of stat() here,
          * because we do not want the name to be a symlink (created by
          * another user) pointing to a directory owned by the current
          * effective user with permissions 'mode'.
          */

         if (0 == Posix_Lstat(dirname, &st)) {
            /*
             * Our directory inherited S_ISGID if its parent had it. So it
             * is important to ignore that bit, and it is safe to do so
             * because that bit does not affect the owner's permissions.
             */

            if (S_ISDIR(st.st_mode) &&
                (st.st_uid == userId) &&
                ((st.st_mode & 05777) == mode)) {
               result = TRUE;
            }
         }
      }
   }

   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileFindExistingSafeTmpDir --
 *
 *      Searches the directory baseTmpDir to see if any subdirectories
 *      are suitable to use as the safe temp directory.  The safe temp
 *      directory must have the correct permissions and userId.
 *
 * Results:
 *      Path to discovered safe temp directory (must be freed).
 *      NULL returned if no suitable directory is found.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileFindExistingSafeTmpDir(uid_t userId,            // IN:
                           const char *userName,    // IN:
                           const char *baseTmpDir)  // IN:
{
   int i;
   int numFiles;
   char *pattern;
   char *tmpDir = NULL;
   char **fileList = NULL;

   /*
    * We always use the pattern PRODUCT-USER-xxxx when creating
    * alternative safe temp directories, so check for ones with
    * those names and the appropriate permissions.
    */

   pattern = Unicode_Format("%s-%s-", PRODUCT_GENERIC_NAME_LOWER, userName);
   if (pattern == NULL) {
      return NULL;
   }

   numFiles = File_ListDirectory(baseTmpDir, &fileList);

   if (numFiles == -1) {
      free(pattern);

      return NULL;
   }

   for (i = 0; i < numFiles; i++) {
       if (Unicode_StartsWith(fileList[i], pattern)) {
          char *path = Unicode_Join(baseTmpDir, DIRSEPS, fileList[i], NULL);

          if (File_IsDirectory(path) &&
              FileAcceptableSafeTmpDir(path, userId)) {
             tmpDir = path;
             break;
          }

          free(path);
       }
   }

   Util_FreeStringList(fileList, numFiles);
   free(pattern);

   return tmpDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileCreateSafeTmpDir --
 *
 *      Creates a new directory within baseTmpDir with the correct permissions
 *      and userId to ensure it is safe from symlink attacks.
 *
 * Results:
 *      Path to created safe temp directory (must be freed).
 *      NULL returned if no suitable directory could be created.
 *
 * Side effects:
 *      Directory may be created.
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileCreateSafeTmpDir(uid_t userId,            // IN:
                     const char *userName,    // IN:
                     const char *baseTmpDir)  // IN:
{
   static const int MAX_DIR_ITERS = 250;
   int curDirIter = 0;
   char *tmpDir = NULL;

   while (TRUE) {
      /*
       * We use a random number that makes it more likely that we will create
       * an unused name than if we had simply tried suffixes in numeric order.
       */

      tmpDir = Str_Asprintf(NULL, "%s%s%s-%s-%u", baseTmpDir, DIRSEPS,
                            PRODUCT_GENERIC_NAME_LOWER, userName,
                            FileSimpleRandom());

      if (!tmpDir) {
         Warning("%s: Out of memory error.\n", __FUNCTION__);
         break;
      }

      if (FileAcceptableSafeTmpDir(tmpDir, userId)) {
         break;
      }

      if (++curDirIter > MAX_DIR_ITERS) {
         Warning("%s: Failed to create a safe temporary directory, path "
                 "\"%s\". The maximum number of attempts was exceeded.\n",
                 __FUNCTION__, tmpDir);
         free(tmpDir);
         tmpDir = NULL;
         break;
      }

      free(tmpDir);
      tmpDir = NULL;
   }

   return tmpDir;
}
#endif // __linux__


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetSafeTmpDir --
 *
 *      Return a safe temporary directory (i.e. a temporary directory which
 *      is not prone to symlink attacks, because it is only writable by the
 *      current effective user).
 *
 *      Guaranteed to return the same directory every time it is
 *      called during the lifetime of the current process, for the
 *      current effective user ID. (Barring the user manually deleting
 *      or renaming the directory.)
 *
 * Results:
 *      The allocated directory path on success.
 *      NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_GetSafeTmpDir(Bool useConf)  // IN:
{
   char *tmpDir;

#if defined(__FreeBSD__) || defined(sun)
   tmpDir = FileGetTmpDir(useConf);
#else
   static Atomic_Ptr lckStorage;
   static char *safeDir;
   char *baseTmpDir = NULL;
   char *userName = NULL;
   uid_t userId;
   MXUserExclLock *lck;

   userId = geteuid();

   /* Get and take lock for our safe dir. */
   lck = MXUser_CreateSingletonExclLock(&lckStorage, "getSafeTmpDirLock",
                                        RANK_getSafeTmpDirLock);

   MXUser_AcquireExclLock(lck);

   /*
    * Check if we've created a temporary dir already and if it is still usable.
    */

   tmpDir = NULL;

   if (safeDir && FileAcceptableSafeTmpDir(safeDir, userId)) {
      tmpDir = Util_SafeStrdup(safeDir);
      goto exit;
   }

   /* We don't have a useable temporary dir, create one. */
   baseTmpDir = FileGetTmpDir(useConf);

   if (!baseTmpDir) {
      Warning("%s: FileGetTmpDir failed.\n", __FUNCTION__);
      goto exit;
   }

   userName = FileGetUserName(userId);

   if (!userName) {
      Warning("%s: FileGetUserName failed, using numeric ID "
              "as username instead.\n", __FUNCTION__);

      /* Fallback on just using the userId as the username. */
      userName = Str_Asprintf(NULL, "uid-%d", userId);

      if (!userName) {
         Warning("%s: Str_Asprintf error.\n", __FUNCTION__);
         goto exit;
      }
   }

   tmpDir = Str_Asprintf(NULL, "%s%s%s-%s", baseTmpDir, DIRSEPS,
                         PRODUCT_GENERIC_NAME_LOWER, userName);

   if (!tmpDir) {
      Warning("%s: Out of memory error.\n", __FUNCTION__);
      goto exit;
   }

   if (!FileAcceptableSafeTmpDir(tmpDir, userId)) {
      /*
       * We didn't get our first choice for the safe temp directory.
       * Search through the unsafe tmp directory to see if there is
       * an acceptable one to use.
       */

      free(tmpDir);

      tmpDir = FileFindExistingSafeTmpDir(userId, userName, baseTmpDir);

      if (!tmpDir) {
         /*
          * We didn't find any usable directories, so try to create one now.
          */

         tmpDir = FileCreateSafeTmpDir(userId, userName, baseTmpDir);
      }
   }

   if (tmpDir) {
      /*
       * We have successfully created a temporary directory, remember it for
       * future calls.
       */

      free(safeDir);
      safeDir = Util_SafeStrdup(tmpDir);
   }

  exit:
   MXUser_ReleaseExclLock(lck);
   free(baseTmpDir);
   free(userName);
#endif

   return tmpDir;
}
