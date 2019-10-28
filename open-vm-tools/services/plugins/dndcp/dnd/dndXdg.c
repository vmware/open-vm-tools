/*********************************************************
 * Copyright (C) 2014-2019 VMware, Inc. All rights reserved.
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
 * dndXdg.c --
 *
 *      Drag and drop routines for X Desktop Group (XDG) / freedesktop.org
 *      platforms.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <stdlib.h>

#if defined __FreeBSD__
#include <unistd.h>
#endif

#include "dndInt.h"
#include "dnd.h"

#include "err.h"
#include "file.h"
#include "log.h"
#include "posix.h"
#include "str.h"
#include "strutil.h"
#include "su.h"
#include "unicode.h"
#include "vm_atomic.h"


static const char *CreateRealRootDirectory(void);
static const char *CreateApparentRootDirectory(void);
static char *FindSuitableExistingDirectory(const char *realRoot,
                                           const char *apparentRoot);
static char *CreateStagingDirectory(const char *realRoot,
                                    const char *apparentRoot);


/*
 *-----------------------------------------------------------------------------
 *
 * Xdg_GetCacheHome --
 *
 *      Determine path appropriate for "user-specific non-essential (cached)
 *      data"[1].
 *
 *      1. <http://standards.freedesktop.org/basedir-spec/basedir-spec-latest.html>
 *
 * Results:
 *      Returns a pointer to a valid path on success or NULL on failure.
 *
 * Side effects:
 *      First call may allocate memory which remains allocated for the life
 *      of the program.
 *
 *      NOT thread-safe.
 *
 * TODO:
 *      Move bora-vmsoft/lib/xdg to bora/lib, then move this to bora/lib/xdg.
 *      Unit tests.  (There is no good home for these at the moment.)
 *
 *-----------------------------------------------------------------------------
 */

const char *
Xdg_GetCacheHome(void)
{
   static char *result = NULL;

   if (result == NULL) {
      do {
         struct passwd *pw;

         if (!Id_IsSetUGid()) {
            const char *base = NULL;

            /*
             * Paranoia:  Avoid environment variables if running in a sensitive
             *            context.  sudo or other loader should've sanitized the
             *            environment, but, well, we're paranoid, remember?
             */

            // 1. $XDG_CACHE_HOME
            base = Posix_Getenv("XDG_CACHE_HOME");
            if (Util_IsAbsolutePath(base)) {
               result = Util_SafeStrdup(base);
               break;
            }

            // 2. $HOME/.cache
            base = Posix_Getenv("HOME");
            if (Util_IsAbsolutePath(base)) {
               result = Util_SafeStrdup(base);
               StrUtil_SafeStrcat(&result, "/.cache");
               break;
            }
         }

         // 3. <pw_dir>/.cache
         pw = Posix_Getpwuid(geteuid());

         if (pw != NULL && Util_IsAbsolutePath(pw->pw_dir)) {
            result = Str_Asprintf(NULL, "%s/.cache", pw->pw_dir);
         }
      } while(0);
   }

   VERIFY(result == NULL || result[0] == '/');
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * DnD_CreateStagingDirectory --
 *
 *    See dndCommon.c.
 *
 * Results:
 *    A string containing the newly created name, or NULL on failure.
 *
 * Side effects:
 *    A directory is created
 *
 *    Caller is responsible for freeing returned pointer.
 *
 *    NOT thread-safe.
 *
 *-----------------------------------------------------------------------------
 */

char *
DnD_CreateStagingDirectory(void)
{
   const char *realRoot;
   const char *apparentRoot;
   char *existingDir;

   /*
    * On XDG platforms, there are two roots:
    *
    * 1. Per-user real root ($HOME/.cache/vmware/drag_and_drop)
    *
    *    Files are stored here, leaving cleanup to discretion of users and
    *    adminisitrators, and may count against users' storage quotas.  Most
    *    importantly, however, is that it means avoiding tmpfs-backed /tmp
    *    which may be too small for large files.
    *
    * 2. Apparent root - /tmp/VMwareDnD
    *
    *    Path known to vmblock implementations.  Contains only symlinks to
    *    users' real root.
    *
    * Therefore DnD targets may access paths via
    *    /var/run/vmblock (vmblock file system) ->
    *    /tmp/VMwareDnD   (apparent root) ->
    *    $HOME/.cache/vmware/drag_and_drop (real root).
    */


   /*
    * Lookup or create real root.
    *
    * We don't worry about cleaning up or removing this directory in case
    * of failure later in this function.
    */

   realRoot = CreateRealRootDirectory();
   if (realRoot == NULL) {
      return NULL;
   }


   /*
    * Lookup or create apparent root.
    */

   apparentRoot = CreateApparentRootDirectory();
   if (apparentRoot == NULL) {
      return NULL;
   }


   /*
    * Search real root for empty directory.  If found, (re)use it.
    */

   existingDir = FindSuitableExistingDirectory(realRoot, apparentRoot);
   if (existingDir != NULL) {
      return existingDir;
   }


   /*
    * Generate new temporary directory.
    *    - Attempt to symlink $apparent/XXXXXX to $real/XXXXXX.
    *    - mkdir -p $real/XXXXXX.
    */

   return CreateStagingDirectory(realRoot, apparentRoot);
}


/*
 *-----------------------------------------------------------------------------
 *
 * DetermineRealRootDirectory --
 * CreateRealRootDirectory --
 *
 *      Produces the path of the real staging directory root (e.g.
 *      $HOME/.cache/vmware/drag_and_drop).
 *
 * Results:
 *      !NULL   Success. Points to real staging root path.
 *      NULL    Failure.
 *
 * Side effects:
 *      May create the .cache/vmware/drag_and_drop hierarchy.
 *
 *-----------------------------------------------------------------------------
 */

static const char *
DetermineRealRootDirectory(void)
{
   static char *completePath;
   const char *cachePath;

   if (completePath != NULL) {
      return completePath;
   }

   cachePath = Xdg_GetCacheHome();
   if (cachePath != NULL) {
      const char dndSuffix[] = "/vmware/drag_and_drop/";

      completePath = Unicode_Duplicate(cachePath);
      StrUtil_SafeStrcat(&completePath, dndSuffix);
      VERIFY(strlen(completePath) < PATH_MAX);

      Log_Trivia("dnd: will stage to %s\n", completePath);
      return completePath;
   }

   Log_Trivia("dnd: failed to determine path\n");
   return NULL;
}

static const char *
CreateRealRootDirectory(void)
{
   const char *realRoot = DetermineRealRootDirectory();

   if (   realRoot != NULL
       && (   File_IsDirectory(realRoot)
           || File_CreateDirectoryHierarchyEx(realRoot, 0700, NULL))) {
      return realRoot;
   }

   return NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * CreateApparentRootDirectory --
 *
 *    Checks if the root staging directory exists with the correct permissions,
 *    or creates it if necessary.
 *
 * Results:
 *    !NULL     Success. String containing apparent root directory path.
 *    NULL      failure.
 *
 * Side effects:
 *    May create apparent root (/tmp/VMwareDnD) and/or change its permissions.
 *
 *----------------------------------------------------------------------------
 */

static const char *
CreateApparentRootDirectory(void)
{
   const char *root;

   /*
    * DnD_GetFileRoot() gives us a pointer to a static string, so there's no
    * need to free anything.
    *
    * XXX On XDG platforms this path ("/tmp/VMwareDnD") is created by an
    *     init script, so we could remove some of the code below and just bail
    *     if the user deletes it.
    */

   root = DnD_GetFileRoot();
   if (!root) {
      return NULL;
   }

   if (File_Exists(root)) {
      if (!DnDRootDirUsable(root)) {
         /*
          * The directory already exists and its permissions are wrong.
          */
         Log_Trivia("dnd: The root dir is not usable.\n");
         return NULL;
      }
   } else {
      if (   !File_CreateDirectory(root)
          || !DnDSetPermissionsOnRootDir(root)) {
         /* We couldn't create the directory or set the permissions. */
         return NULL;
      }
   }

   return root;
}


/*
 *-----------------------------------------------------------------------------
 *
 * FindSuitableExistingDirectory --
 *
 *      Given the pair of <realRoot, apparentRoot>, search for an existing
 *      directory within `realRoot` satisfying the following conditions:
 *
 *         1. it's empty;
 *         2. it's pointed to by a symlink of the same name from apparentRoot.
 *
 * Results:
 *      !NULL   Success.  Caller must free result.
 *      NULL    Failure.
 *
 * Side effects:
 *      May create a symlink.
 *
 *-----------------------------------------------------------------------------
 */

static char *
FindSuitableExistingDirectory(
   const char *realRoot,       // IN: e.g. $HOME/.cache/vmware/drag_and_drop/
   const char *apparentRoot)   // IN: e.g. /tmp/VMwareDnD/
{
   char *result = NULL;

   char **stagingDirList;
   int numStagingDirs = File_ListDirectory(realRoot, &stagingDirList);
   int i;

   for (i = 0; i < numStagingDirs && result == NULL; i++) {
      char *stagingDir = Unicode_Append(realRoot, stagingDirList[i]);
      char *apparentStagingDir = Unicode_Append(apparentRoot, stagingDirList[i]);
      char *temp = NULL;
      struct stat sb;

      if (   File_IsEmptyDirectory(stagingDir)
          && (   Posix_Symlink(stagingDir, apparentStagingDir) == 0
              || (   Posix_Lstat(apparentStagingDir, &sb) == 0
                  && sb.st_uid == getuid()
                  && (temp = Posix_ReadLink(apparentStagingDir)) != NULL
                  && strcmp(stagingDir, temp) == 0))) {
         result = apparentStagingDir;
         apparentStagingDir = NULL;
      }

      free(stagingDir);
      free(apparentStagingDir);
      free(temp);
   }

   Util_FreeStringList(stagingDirList, numStagingDirs);
   return result;
}


/*
 *-----------------------------------------------------------------------------
 *
 * CreateStagingDirectory --
 *
 *      Creates a fresh staging directory within `realRoot` and creates a
 *      symlink to it within `apparentRoot`.
 *
 * Results:
 *      !NULL   Success.  Caller must free result.
 *      NULL    Failure.
 *
 * Side effects:
 *      Creates directories and stuff.
 *
 *-----------------------------------------------------------------------------
 */

static char *
CreateStagingDirectory(
   const char *realRoot,       // IN: e.g. $HOME/.cache/vmware/drag_and_drop/
   const char *apparentRoot)   // IN: e.g. /tmp/VMwareDnD/
{
   char *result = NULL;
   int i;

   for (i = 0; i < 10 && result == NULL; i++) {
      char *apparentStagingDir = NULL;
      // Reminder: mkdtemp updates its arg in-place.
      char *realStagingDir = Str_SafeAsprintf(NULL, "%sXXXXXX", realRoot);

      if (mkdtemp(realStagingDir) != NULL) {
         char *randomPart = strrchr(realStagingDir, '/') + 1;
         VERIFY(*randomPart != '\0');

         apparentStagingDir = Unicode_Append(apparentRoot, randomPart);

         if (Posix_Symlink(realStagingDir, apparentStagingDir) == 0) {
            // Transfer ownership to caller.
            result = apparentStagingDir;
            apparentStagingDir = NULL;
         } else {
            Warning("dnd: symlink(%s): %s", apparentStagingDir, Err_ErrString());
            Posix_Rmdir(realStagingDir);
         }
      } else {
         Warning("dnd: mkdtemp(%s): %s", realStagingDir, Err_ErrString());
      }

      free(realStagingDir);
      free(apparentStagingDir);
   }

   return result;
}
