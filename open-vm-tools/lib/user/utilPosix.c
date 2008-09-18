/*********************************************************
 * Copyright (C) 2004 VMware, Inc. All rights reserved.
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
 * utilPosix.c --
 *
 *	Posix misc util functions.
 */

#include <string.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <dirent.h>

#if !__FreeBSD__ && !sun
#   include <pwd.h>
#endif

#ifdef sun
#   include <procfs.h>
#endif

#include "vmware.h"
#include "file.h"
#include "util.h"
#include "su.h"
#include "vm_atomic.h"
#include "str.h"
#include "vm_version.h"
#include "random.h"
#include "hostinfo.h"
#include "syncMutex.h"
#include "escape.h"
#include "unicodeOperations.h"
#include "err.h"
#include "posix.h"


/* For Util_GetProcessName() */
#ifdef sun
#   define PROCFILE  "psinfo"
#else
#   define PROCFILE  "status"
    /*
     * Note that the limit of what is output for the process name in
     * /proc/<pid>/status is 15 characters (30 with '\' -> '\\' escaping) on
     * Linux, and 19 characters (76 with ' ' -> '\040' escaping) on FreeBSD.
     * Reading in at most 128 gives us a bit of extra room in case these fields
     * grow in the future.  The 129 in our buffer is for the NUL terminator.
     */
#   define PSINFOSZ  129
#   ifdef __linux__
#      define PRE  "Name:   "
#      define POST "\n"
#      define PSINFOFMT "%128[^\n]"
#   else /* FreeBSD */
#      define PRE
#      define POST " "
#      define PSINFOFMT "%128s"
#   endif
#endif



#if !__FreeBSD__ && !sun

/*
 *-----------------------------------------------------------------------------
 *
 * Util_BumpNoFds --
 *
 *      Bump the number of file descriptor this process can open to 2048. On
 *      failure sets *cur to the number of fds we can currently open, and sets
 *      *wanted to what we want.
 *
 * Results:
 *      0 on success, errno from the failing call to setrlimit(2) otherwise.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
Util_BumpNoFds(uint32 *cur,     // OUT
               uint32 *wanted)  // OUT
{
   struct rlimit lim;
   int err;

   /*
    * Check for minimum file descriptor limit. The number 2048 is
    * somewhat arbitrary. Trying to do multiple snapshots of a split
    * disk can rapidly consume descriptors however, so we ought to
    * have a large number. This is only pushing back the problem of
    * course. Ideally we'd have a fully scalable solution.
    */

   static const rlim_t fdsDesired = 2048;

   err = getrlimit(RLIMIT_NOFILE, &lim);
   ASSERT_NOT_IMPLEMENTED(err >= 0);

   if (cur) {
      *cur = lim.rlim_cur;
   }
   if (wanted) {
      *wanted = fdsDesired;
   }

   if (lim.rlim_cur != RLIM_INFINITY && lim.rlim_cur < fdsDesired) {
      Bool needSuperUser;

      /*
       * First attempt to raise limit ourselves.
       * If that fails, complain and make user do it.
       */
      rlim_t curFdLimit = lim.rlim_cur;
      rlim_t maxFdLimit = lim.rlim_max;

      lim.rlim_cur = fdsDesired;

      /*
       * rlim_max may need to be increased as well.
       */

      needSuperUser = lim.rlim_max != RLIM_INFINITY && lim.rlim_max < fdsDesired;

      if (needSuperUser) {
	 lim.rlim_max = fdsDesired;
      } else { 
         err = setrlimit(RLIMIT_NOFILE, &lim) < 0 ? errno : 0;
      }

      /*
       * Set euid to root for the FD limit increase. Note we don't need root
       * unless rlim_max is being increased.  Revert to non-root immediately
       * after.
       */

      if (err == EPERM || needSuperUser) {
         Bool wasSuper = IsSuperUser();
         SuperUser(TRUE);
         err = setrlimit(RLIMIT_NOFILE, &lim) < 0 ? errno : 0;
         SuperUser(wasSuper);
      }

      /*
       * If everything else failed, simply try using rlim_max. That might be
       * enough..
       */

      if (err != 0) {
         lim.rlim_cur = maxFdLimit;
         lim.rlim_max = maxFdLimit;
         err = setrlimit(RLIMIT_NOFILE, &lim) < 0 ? errno : 0;
         ASSERT_NOT_TESTED(err == 0);
      }

      if (err != 0) {
         Log("UTIL: Failed to set number of fds at %u, was %u: %s (%d)\n",
             (uint32)fdsDesired, (uint32)curFdLimit, Err_Errno2String(err), err); 
      }
   }
   return err;
}


/*
 *-----------------------------------------------------------------------------
 *
 * UtilGetUserName --
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
UtilGetUserName(uid_t uid) // IN
{
   long memPoolSize;
   char *memPool;
   struct passwd pw;
   struct passwd *pw_p;
   char *userName;

#if __APPLE__
   memPoolSize = _PASSWORD_LEN;
#else
   memPoolSize = sysconf(_SC_GETPW_R_SIZE_MAX);
   if (memPoolSize <= 0) {
      Warning("UtilGetUserName: sysconf(_SC_GETPW_R_SIZE_MAX) failed.\n");
      return NULL;
   }
#endif

   memPool = malloc(memPoolSize);
   if (memPool == NULL) {
      Warning("UtilGetUserName: Not enough memory.\n");
      return NULL;
   }

   if (Posix_Getpwuid_r(uid, &pw, memPool, memPoolSize, &pw_p) != 0) {
      free(memPool);
      Warning("UtilGetUserName: Unable to retrieve the username associated "
              "with user ID %u.\n",
          uid);
      return NULL;
   }

   userName = strdup(pw_p->pw_name);
   free(memPool);
   if (userName == NULL) {
      Warning("UtilGetUserName: Not enough memory.\n");
      return NULL;
   }

   return userName;
}


/*
 *----------------------------------------------------------------------
 *
 *  Util_MakeSafeTemp --
 *
 *      Exactly the same as File_MakeTemp except uses a safe directory
 *      as the default temporary directory.
 *
 *      This code is duplicated in identical form between utilPosix.c
 *      and utilWin32.c, but can't go in util.c because it depends on
 *      Util_GetSafeTmpDir whose Win32 implementation implies extra
 *      dependencies that some people who link just to util.c can't
 *      stomach.
 *
 * Results:
 *      Open file descriptor or -1
 *
 * Side effects:
 *      Creates a file if successful.
 *----------------------------------------------------------------------
 */

int
Util_MakeSafeTemp(ConstUnicode tag,  // IN (OPT):
                  Unicode *presult)  // OUT:
{
   int fd = -1;
   Unicode dir = NULL;
   Unicode fileName = NULL;

   *presult = NULL;

   if (tag && File_IsFullPath(tag)) {
      File_GetPathName(tag, &dir, &fileName);
   } else {
      dir = Util_GetSafeTmpDir(TRUE);
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
 * UtilAcceptableSafeTmpDir --
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
UtilAcceptableSafeTmpDir(const char *dirname,  // IN
                         int userId)           // IN
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
          * directory owned by the current effective user with
          * permissions 'mode'. It is crucial to use lstat() instead of
          * stat() here, because we do not want the name to be a symlink
          * (created by another user) pointing to a directory owned by
          * the current effective user with permissions 'mode'.
          */

         if (0 == Posix_Lstat(dirname, &st)) {
            /*
             * Our directory inherited S_ISGID if its parent had it. So it
             * is important to ignore that bit, and it is safe to do so
             * because that bit does not affect the owner's
             * permissions.
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
 * UtilFindExistingSafeTmpDir --
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

static Unicode
UtilFindExistingSafeTmpDir(uid_t userId,             // IN
                           const char * userName,    // IN
                           const char * baseTmpDir)  // IN
{
   int i;
   int numFiles;
   Unicode pattern;
   Unicode tmpDir = NULL;
   Unicode *fileList = NULL;
   
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
      Unicode_Free(pattern);
      return NULL;
   }

   for (i = 0; i < numFiles; i++) {
       if (Unicode_StartsWith(fileList[i], pattern)) {
          Unicode path = Unicode_Join(baseTmpDir, DIRSEPS, fileList[i],
                                      NULL);

          if (File_IsDirectory(path) &&
              UtilAcceptableSafeTmpDir(path, userId)) {
             tmpDir = path;
             break;
          }

          Unicode_Free(path);
       }
   }

   Unicode_FreeList(fileList, numFiles);
   Unicode_Free(pattern);

   return tmpDir;
}

/*
 *-----------------------------------------------------------------------------
 *
 * UtilCreateSafeTmpDir --
 *
 *      Creates a new directory within baseTmpDir with the correct 
 *      permissions and userId to ensure it is safe from symlink attacks.
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
UtilCreateSafeTmpDir(uid_t userId,             // IN
                     const char * userName,    // IN
                     const char * baseTmpDir)  // IN
{
   static const int MAX_DIR_ITERS = 250;
   char *tmpDir = NULL;
   int curDirIter;

   curDirIter = 0;
   while (TRUE) {
      unsigned int suffix;
      
      /* 
       * We use a crypographically strong random number which is
       * overkill for this purpose but makes it slightly more likely
       * that we will create an unused name than if we had simply tried
       * suffixes in numeric order.
       */

      if (!Random_Crypto(sizeof(suffix), &suffix)) {
         Warning("%s: Call to Random_Crypto failed.\n", __FUNCTION__);
         break;
      }
      
      tmpDir = Str_Asprintf(NULL, "%s"DIRSEPS"%s-%s-%u", baseTmpDir,
                            PRODUCT_GENERIC_NAME_LOWER, userName, suffix);
      
      if (!tmpDir) {
         Warning("%s: Out of memory error.\n", __FUNCTION__);
         break;
      }
      
      if (UtilAcceptableSafeTmpDir(tmpDir, userId)) {
         break;
      }
      
      if (++curDirIter > MAX_DIR_ITERS) {
         Warning("%s: Failed to create a safe temporary "
               "directory, path \"%s\". The maximum number of attempts was "
               "exceeded.\n", __FUNCTION__, tmpDir);
         free(tmpDir);
         tmpDir = NULL;
         break;
      }
      
      free(tmpDir);
      tmpDir = NULL;
   }

   return tmpDir;
}

/*
 *-----------------------------------------------------------------------------
 *
 * Util_GetSafeTmpDir --
 *
 *      Return a safe temporary directory (i.e. a temporary directory
 *      which is not prone to symlink attacks, because it is only
 *      writable by the current effective user). Guaranteed to return
 *      the same directory every time it is called during the lifetime
 *      of the current process (unless that directory is deleted while
 *      the process is running).
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
Util_GetSafeTmpDir(Bool useConf) // IN
{
   static Atomic_Ptr lckStorage;
   static char *safeDir;
   char *tmpDir = NULL;
   char *baseTmpDir = NULL;
   char *userName = NULL;
   uid_t userId;
   SyncMutex *lck;

   userId = geteuid();

   /* Get and take lock for our safe dir. */
   lck = SyncMutex_CreateSingleton(&lckStorage);
   SyncMutex_Lock(lck);

   /*
    * Check if we've created a temporary dir already and if it is
    * still usable.
    */
   if (safeDir && UtilAcceptableSafeTmpDir(safeDir, userId)) {
      tmpDir = Util_SafeStrdup(safeDir);
      goto exit;
   }

   /* We don't have a useable temporary dir, create one. */
   baseTmpDir = File_GetTmpDir(useConf);
   
   if (!baseTmpDir) {
      Warning("%s: File_GetTmpDir failed.\n", __FUNCTION__);
      goto exit;
   }
   
   userName = UtilGetUserName(userId);
   
   if (!userName) {
      Warning("%s: UtilGetUserName failed, using numeric ID "
              "as username instead.\n", __FUNCTION__);
      
      /* Fallback on just using the userId as the username. */
      userName = Str_Asprintf(NULL, "uid-%d", userId);
      
      if (!userName) {
         Warning("%s: Str_Asprintf error.\n", __FUNCTION__);
         goto exit;
      }
   }
   
   tmpDir = Str_Asprintf(NULL, "%s"DIRSEPS"%s-%s", baseTmpDir,
                         PRODUCT_GENERIC_NAME_LOWER, userName);
   
   if (!tmpDir) {
      Warning("%s: Out of memory error.\n", __FUNCTION__);
      goto exit;
   }

   if (!UtilAcceptableSafeTmpDir(tmpDir, userId)) {
      /*
       * We didn't get our first choice for the safe temp directory.
       * Search through the unsafe tmp directory to see if there is
       * an acceptable one to use.
       */

      free(tmpDir);

      tmpDir = UtilFindExistingSafeTmpDir(userId, userName, baseTmpDir);

      if (!tmpDir) {
         /*
          * We didn't find any usable directories, so try to create one
          * now.
          */
         tmpDir = UtilCreateSafeTmpDir(userId, userName, baseTmpDir);
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
   SyncMutex_Unlock(lck);
   free(baseTmpDir);
   free(userName);

   return tmpDir;
}

#endif // __linux__


#if defined(__linux__) || defined(__FreeBSD__) || defined(sun)
/*
 *----------------------------------------------------------------------------
 *
 * Util_GetProcessName --
 *
 *    Tries to locate the process name of the specified process id.  The
 *    process' name is placed in bufOut.
 *
 * Results:
 *    TRUE on success, FALSE on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

Bool
Util_GetProcessName(pid_t pid,         // IN : process id
                    char *bufOut,      // OUT: output buffer
                    size_t bufOutSize) // IN : buffer size
{
   int fd;
   int nread;
   char *psname;
   size_t psnameLen;
   char fileName[MAXPATHLEN];
#ifdef sun
   char buf[sizeof (psinfo_t)];
#else
   char buf[PSINFOSZ];
   char psinfo[PSINFOSZ];
#endif

   ASSERT(bufOut);

   /*
    * Open up /proc/<pid>/status on Linux/FreeBSD, and /proc/<pid>/psinfo on
    * Solaris.
    */
   Str_Sprintf(fileName, sizeof fileName, "/proc/%"FMTPID"/" PROCFILE, pid);

   fd = Posix_Open(fileName, O_RDONLY);
   if (fd < 0) {
      Log("Util_GetProcessName: Error: cannot open %s\n", fileName);
      return FALSE;
   }

   nread = read(fd, buf, sizeof buf);
#ifdef sun
   if (nread != sizeof buf) {
#else
   if (nread < 0) {
#endif
      Log("Util_GetProcessName: Error: could not read %s\n", fileName);
      close(fd);
      return FALSE;
   }

   close(fd);

#ifdef sun
   psname = ((psinfo_t *)buf)->pr_fname;
#else /* Linux & FreeBSD */
   ASSERT(nread <= sizeof buf);
   buf[nread == sizeof buf ? nread - 1 : nread] = '\0';
   /*
    * Parse the plain text formatting of the status file.  Note that PSINFOFMT
    * contains a format modifier to ensure psinfo is not overrun.
    */
   if (sscanf(buf, PRE PSINFOFMT POST, psinfo) != 1) {
      Log("Util_GetProcessName: Error, could not parse contents of %s\n", fileName);
      return FALSE;
   }

   Escape_UnescapeCString(psinfo);

   psname = psinfo;
#endif /* sun */

   psnameLen = strlen(psname);
   if (psnameLen + 1 > bufOutSize) {
      Log("Util_GetProcessName: Error, process name (%"FMTSZ"u bytes) is larger "
          "than output buffer\n", psnameLen);
      return FALSE;
   }

   memcpy(bufOut, psname, psnameLen + 1);
   return TRUE;
}
#endif
