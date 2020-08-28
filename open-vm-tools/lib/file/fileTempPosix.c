/*********************************************************
 * Copyright (C) 2004-2019 VMware, Inc. All rights reserved.
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
 *      Check to see if the specified directory is actually a directory
 *      and is writable by us.
 *
 * Results:
 *     !NULL  The expanded directory name (must be freed)
 *      NULL  Failure
 *
 * Side effects:
 *      None
 *
 *----------------------------------------------------------------------
 */

static char *
FileTryDir(const char *dirName)  // IN: Is this a writable directory?
{
   if (dirName != NULL) {
      char *edirName = Util_ExpandString(dirName);

      if ((edirName != NULL) && FileIsWritableDir(edirName)) {
         return edirName;
      }

      Posix_Free(edirName);
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------
 *
 * FileGetTmpDir --
 *
 *      Determine the best temporary directory. Unsafe since the returned
 *      directory is generally going to be 0777, thus all sorts of denial
 *      of service or symlink attacks are possible.
 *
 * Results:
 *     !NULL  The temp directory name (must be freed)
 *      NULL  Failure (reported to the user).
 *
 * Side effects:
 *      None
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
      Posix_Free(dirName);
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
      Posix_Free(dirName);
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
 *      Retrieve the name associated with the specified UID. Thread-safe
 *      version. --hpreg
 *
 * Results:
 *     !NULL  The user name (must be freed)
 *      NULL  Failure
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
   long memPoolSize;
   struct passwd pw;
   struct passwd *pw_p;
   char *userName = NULL;

#if defined(__APPLE__)
   memPoolSize = _PASSWORD_LEN;
#else
   errno = 0;
   memPoolSize = sysconf(_SC_GETPW_R_SIZE_MAX);

   if ((errno != 0) || (memPoolSize == 0)) {
      Warning("%s: sysconf(_SC_GETPW_R_SIZE_MAX) failed.\n", __FUNCTION__);

      return NULL;
   }

   if (memPoolSize == -1) {  // Unlimited; pick something reasonable
      memPoolSize = 16 * 1024;
   }
#endif

   memPool = Util_SafeMalloc(memPoolSize);

   if ((Posix_Getpwuid_r(uid, &pw, memPool, memPoolSize, &pw_p) != 0) ||
       pw_p == NULL) {
      Warning("%s: Unable to retrieve the user name associated with UID %u.\n",
             __FUNCTION__, uid);
   } else {
      userName = Util_SafeStrdup(pw_p->pw_name);
   }

   Posix_Free(memPool);

   return userName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileGetUserIdentifier --
 *
 *      Attempt to obtain an user identification string.
 *
 * Results:
 *     A dynamically allocated string containing the user identifier.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileGetUserIdentifier(uid_t uid,    // IN:
                      Bool addPid)  // IN:
{
   char *userName = FileGetUserName(uid);

   if (userName == NULL) {
      Warning("%s: Failed to get user name, using UID.\n", __FUNCTION__);

      /* Fallback on just using the uid as the user name. */
      userName = Str_SafeAsprintf(NULL, "uid_%u", uid);
   }

   if (addPid) {
      char *pidToo = Str_SafeAsprintf(NULL, "%s_%u", userName, getpid());

      Posix_Free(userName);
      userName = pidToo;
   }

   return userName;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileAcceptableSafeTmpDir --
 *
 *      Determines if the specified path is acceptable as a safe temp
 *      directory. The directory must either be creatable with the appropriate
 *      permissions and UID or it must already exist with those settings.
 *
 * Results:
 *      TRUE   Path is acceptible
 *      FALSE  Otherwise
 *
 * Side effects:
 *      Directory may be created
 *
 *-----------------------------------------------------------------------------
 */

static Bool
FileAcceptableSafeTmpDir(const char *dirName,  // IN:
                         uid_t uid)            // IN:
{
   Bool acceptable;
   static const mode_t mode = 0700;

   acceptable = (Posix_Mkdir(dirName, mode) == 0);

   if (!acceptable) {
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

         if (Posix_Lstat(dirName, &st) == 0) {
            /*
             * Our directory inherited S_ISGID if its parent had it. So it is
             * important to ignore that bit, and it is safe to do so because
             * that bit does not affect the owner's permissions.
             */

            if (S_ISDIR(st.st_mode) &&
                (st.st_uid == uid) &&
                ((st.st_mode & 05777) == mode)) {
               acceptable = TRUE;
            }
         }
      }
   }

   return acceptable;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileFindExistingSafeTmpDir --
 *
 *      Searches baseTmpDir to see if any subdirectories are suitable to use
 *      as a safe temp directory. The safe temp directory must have the correct
 *      permissions and UID.
 *
 * Results:
 *     !NULL  Path to discovered safe temp directory (must be freed)
 *      NULL  No suitable directory was found
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileFindExistingSafeTmpDir(const char *baseTmpDir,  // IN:
                           const char *userName,    // IN:
                           uid_t uid)               // IN:
{
   int i;
   int numFiles;
   char *pattern;
   char *tmpDir = NULL;
   char **fileList = NULL;

   /*
    * We always use the pattern PRODUCT-USER-xxxx when creating alternative
    * safe temp directories, so check for ones with those names and the
    * appropriate permissions.
    */

   pattern = Unicode_Format("%s-%s-", PRODUCT_GENERIC_NAME_LOWER, userName);
   if (pattern == NULL) {
      return NULL;
   }

   numFiles = File_ListDirectory(baseTmpDir, &fileList);

   if (numFiles == -1) {
      Posix_Free(pattern);

      return NULL;
   }

   for (i = 0; i < numFiles; i++) {
       if (Unicode_StartsWith(fileList[i], pattern)) {
          char *path = Unicode_Join(baseTmpDir, DIRSEPS, fileList[i], NULL);

          if (File_IsDirectory(path) &&
              FileAcceptableSafeTmpDir(path, uid)) {
             tmpDir = path;
             break;
          }

          Posix_Free(path);
       }
   }

   Util_FreeStringList(fileList, numFiles);
   Posix_Free(pattern);

   return tmpDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FileCreateSafeTmpDir --
 *
 *      Creates a new directory within baseTmpDir with the correct permissions
 *      and UID to ensure it is safe from symlink attacks.
 *
 * Results:
 *     !NULL  Path to created safe temp directory (must be freed)
 *      NULL  No suitable directory was found
 *
 * Side effects:
 *      Directory may be created.
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileCreateSafeTmpDir(const char *baseTmpDir,  // IN:
                     const char *userName,    // IN:
                     uid_t uid)               // IN:
{
   int curDirIter = 0;
   char *tmpDir = NULL;
   static const int MAX_DIR_ITERS = 250;

   while (TRUE) {
      /*
       * We use a random number that makes it more likely that we will create
       * an unused name than if we had simply tried suffixes in numeric order.
       */

      tmpDir = Str_SafeAsprintf(NULL, "%s%s%s-%s-%u", baseTmpDir, DIRSEPS,
                                PRODUCT_GENERIC_NAME_LOWER, userName,
                                FileSimpleRandom());

      if (FileAcceptableSafeTmpDir(tmpDir, uid)) {
         break;
      }

      if (++curDirIter > MAX_DIR_ITERS) {
         Warning("%s: Failed to create a safe temporary directory, path "
                 "\"%s\". The maximum number of attempts was exceeded.\n",
                 __FUNCTION__, tmpDir);
         Posix_Free(tmpDir);
         tmpDir = NULL;
         break;
      }

      Posix_Free(tmpDir);
   }

   return tmpDir;
}
#endif // __linux__


/*
 *-----------------------------------------------------------------------------
 *
 * FileGetSafeTmpDir --
 *
 *      Return a safe temporary directory (i.e. a temporary directory which
 *      is not prone to symlink attacks, because it is only writable with the
 *      current set of credentials (EUID).
 *
 *      Guaranteed to return the same directory for any EUID every time it is
 *      called during the lifetime of the current process. (Barring the user
 *      manually deleting or renaming the directory.)
 *
 *      Optionally, add the PID to the user identifier for the cases where
 *      the EUID may change during the lifetime of the calling process.
 *
 * Results:
 *     !NULL  Path to safe temp directory (must be freed)
 *      NULL  No suitable directory was found
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static char *
FileGetSafeTmpDir(Bool useConf,  // IN: Use configuration variables?
                  Bool addPid)   // IN: Add PID to userName?
{
   char *tmpDir = NULL;

#if defined(__FreeBSD__) || defined(sun)
   tmpDir = FileGetTmpDir(useConf);
#else
   static Atomic_Ptr lckStorage;
   static char *cachedDir;
   static uid_t cachedEuid;
   static char *cachedPidDir;
   uid_t euid;
   char *testSafeDir;
   MXUserExclLock *lck;
   char *userName = NULL;
   char *baseTmpDir = NULL;

   /* Get and take lock for our safe dir. */
   lck = MXUser_CreateSingletonExclLock(&lckStorage, "getSafeTmpDirLock",
                                        RANK_getSafeTmpDirLock);

   MXUser_AcquireExclLock(lck);

   /*
    * If a suitable temporary directory was cached for this EUID, use it...
    * as long as it is still acceptable.
    */

   euid = geteuid();

   testSafeDir = addPid ? cachedPidDir : cachedDir;

   /*
    * Detecting an EUID change without resorting to I/Os is an nice performance
    * improvement... particularly on ESXi where the operations are expensive.
    */

   if ((euid == cachedEuid) &&
       (testSafeDir != NULL) &&
       FileAcceptableSafeTmpDir(testSafeDir, euid)) {
      tmpDir = Util_SafeStrdup(testSafeDir);
      goto exit;
   }

   /* We don't have a useable temporary dir, create one. */
   baseTmpDir = FileGetTmpDir(useConf);

   if (baseTmpDir == NULL) {
      goto exit;
   }

   userName = FileGetUserIdentifier(euid, addPid);

   tmpDir = Str_SafeAsprintf(NULL, "%s%s%s-%s", baseTmpDir, DIRSEPS,
                             PRODUCT_GENERIC_NAME_LOWER, userName);

   if (addPid || !FileAcceptableSafeTmpDir(tmpDir, euid)) {
      /*
       * Either we want a truely random temp directory or we didn't get our
       * first choice for the safe temp directory. Search through the unsafe
       * tmp directory to see if there is an acceptable one to use.
       */

      Posix_Free(tmpDir);

      tmpDir = FileFindExistingSafeTmpDir(baseTmpDir, userName, euid);

      if (tmpDir == NULL) {
         /*
          * We didn't find any usable directories, so try to create one now.
          */

         tmpDir = FileCreateSafeTmpDir(baseTmpDir, userName, euid);
      }
   }

   if (tmpDir != NULL) {
      char *newDir = Util_SafeStrdup(tmpDir);

      if (euid == cachedEuid) {
         if (addPid) {
            Posix_Free(cachedPidDir);
            cachedPidDir = newDir;
         } else {
            Posix_Free(cachedDir);
            cachedDir = newDir;
         }
      } else {
         Posix_Free(cachedPidDir);
         Posix_Free(cachedDir);

         if (addPid) {
            cachedPidDir = newDir;
            cachedDir = NULL;
         } else {
            cachedDir = newDir;
            cachedPidDir = NULL;
         }

         cachedEuid = euid;
      }
   }

exit:

   MXUser_ReleaseExclLock(lck);
   Posix_Free(baseTmpDir);
   Posix_Free(userName);
#endif

   return tmpDir;
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetSafeTmpDir --
 *
 *      Return a safe temporary directory (i.e. a temporary directory which
 *      is not prone to symlink attacks, because it is only writable with the
 *      current set of credentials (EUID).
 *
 *      Guaranteed to return the same directory for any EUID every time it is
 *      called during the lifetime of the current process. (Barring the user
 *      manually deleting or renaming the directory.)
 *
 * Results:
 *     !NULL  Path to safe temp directory (must be freed)
 *      NULL  No suitable directory was found
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_GetSafeTmpDir(Bool useConf)  // IN:
{
   return FileGetSafeTmpDir(useConf, FALSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_GetSafeRandomTmpDir --
 *
 *      Return a safe, random temporary directory (i.e. a temporary directory
 *      is not prone to symlink attacks, because it is only writable with the
 *      current set of credentials (EUID).
 *
 *      Guaranteed to return the same directory for any EUID every time it is
 *      called during the lifetime of the current process. (Barring the user
 *      manually deleting or renaming the directory.)
 *
 * Results:
 *     !NULL  Path to safe temp directory (must be freed).
 *      NULL  No suitable directory was found.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_GetSafeRandomTmpDir(Bool useConf)  // IN:
{
   return FileGetSafeTmpDir(useConf, TRUE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * File_MakeSafeTempSubdir --
 *
 *      Given an existing safe directory, create a safe subdir of
 *      the specified name in that directory.
 *
 * Results:
 *      The allocated subdir path on success.
 *      NULL on failure.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

char *
File_MakeSafeTempSubdir(const char *safeDir,     // IN
                        const char *subdirName)  // IN
{
#if defined(__FreeBSD__) || defined(sun)
   if (!File_Exists(safeDir)) {
      return NULL;
   }

   return File_PathJoin(safeDir, subdirName);
#else
   uid_t userId = geteuid();
   char *fullSafeSubdir;

   if (!File_Exists(safeDir) ||
       !FileAcceptableSafeTmpDir(safeDir, userId)) {
      return NULL;
   }

   fullSafeSubdir = File_PathJoin(safeDir, subdirName);
   if (!FileAcceptableSafeTmpDir(fullSafeSubdir, userId)) {
      free(fullSafeSubdir);
      return NULL;
   }

   return fullSafeSubdir;
#endif
}
