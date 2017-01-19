/*********************************************************
 * Copyright (C) 2005-2016 VMware, Inc. All rights reserved.
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
 * syncDriverPosix.c --
 *
 *   Interface to the Sync Driver for non-Windows guests.
 */

#include <stdio.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <glib.h>
#include "vmware.h"
#include "debug.h"
#include "str.h"
#include "syncDriverInt.h"
#include "util.h"
#include "mntinfo.h"

static SyncFreezeFn gBackends[] = {
#if defined(__linux__) && !defined(USERWORLD)
   LinuxDriver_Freeze,
   VmSync_Freeze,
   NullDriver_Freeze,
#endif
};

static const char *gRemoteFSTypes[] = {
   "autofs",
   "cifs",
   "nfs",
   "nfs4",
   "smbfs",
   "vmhgfs"
};


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverIsRemoteFSType  --
 *
 *    Checks whether a filesystem is remote or not
 *
 * Results:
 *    Returns TRUE for remote filesystem types, otherwise FALSE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SyncDriverIsRemoteFSType(const char *fsType)
{
   size_t i;

   for (i = 0; i < ARRAYSIZE(gRemoteFSTypes); i++) {
      if (Str_Strcmp(gRemoteFSTypes[i], fsType) == 0) {
         return TRUE;
      }
   }

   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverLocalMounts --
 *
 *    Returns a singly-linked list of all local disk paths mounted in the
 *    system filtering out remote file systems. There is no filtering for
 *    other mount points because we assume that the underlying driver and
 *    IOCTL can deal with "unfreezable" paths. The returned list of paths
 *    is in the reverse order of the paths returned by GETNEXT_MNTINFO.
 *    Caller must free each path and the list itself.
 *
 *    XXX: mntinfo.h mentions Solaris and Linux, but not FreeBSD. If we ever
 *    have a FreeBSD sync driver, we should make sure this function also
 *    works there.
 *
 * Results:
 *    GSList* on success, NULL on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static GSList *
SyncDriverLocalMounts(void)
{
   GSList *paths = NULL;
   MNTHANDLE mounts;
   DECLARE_MNTINFO(mntinfo);

   if ((mounts = OPEN_MNTFILE("r")) == NULL) {
      Warning(LGPFX "Failed to open mount point table.\n");
      return NULL;
   }

   while (GETNEXT_MNTINFO(mounts, mntinfo)) {
      char *path;
      /*
       * Skip remote mounts because they are not freezable and opening them
       * could lead to hangs. See PR 1196785.
       */
      if (SyncDriverIsRemoteFSType(MNTINFO_FSTYPE(mntinfo))) {
         Debug(LGPFX "Skipping remote filesystem, name=%s, mntpt=%s.\n",
               MNTINFO_NAME(mntinfo), MNTINFO_MNTPT(mntinfo));
         continue;
      }

      path = Util_SafeStrdup(MNTINFO_MNTPT(mntinfo));

      /*
       * A mount point could depend on existence of a previous mount
       * point like a loopback. In order to avoid deadlock/hang in
       * freeze operation, a mount point needs to be frozen before
       * its dependency is frozen.
       * Typically, mount points are listed in the order they are
       * mounted by the system i.e. dependent comes after the
       * dependency. So, we need to keep them in reverse order of
       * mount points to achieve the dependency order.
       */
      paths = g_slist_prepend(paths, path);
   }

   (void) CLOSE_MNTFILE(mounts);
   return paths;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_Init --
 *
 *    Checks whether a sync backend is available.
 *
 * Results:
 *    TRUE if there are sync backends available.
 *    FALSE otherwise.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncDriver_Init(void)
{
   return ARRAYSIZE(gBackends) > 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverFreePath --
 *
 *    A GFunc for freeing path strings. It is intended for g_slist_foreach.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static void
SyncDriverFreePath(gpointer data, gpointer userData)
{
   free(data);
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_Freeze --
 *
 *    Freeze I/O on the indicated drives. "all" means all drives.
 *    Handle is set to SYNCDRIVER_INVALID_HANDLE on failure.
 *    Freeze operations are currently synchronous in POSIX systems, but
 *    clients should still call SyncDriver_QueryStatus to maintain future
 *    compatibility in case that changes.
 *
 *    This function will try different available sync implementations. It will
 *    follow the order in the "gBackends" array, and keep on trying different
 *    backends while SD_UNAVAILABLE is returned. If all backends are
 *    unavailable (unlikely given the "null" backend), the the function returns
 *    error. NullDriver will be tried only if enableNullDriver is TRUE.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    See description.
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncDriver_Freeze(const char *userPaths,     // IN
                  Bool enableNullDriver,     // IN
                  SyncDriverHandle *handle)  // OUT
{
   GSList *paths = NULL;
   SyncDriverErr err = SD_UNAVAILABLE;
   size_t i = 0;

   /*
    * NOTE: Ignore disk UUIDs. We ignore the userPaths if it does
    * not start with '/' because all paths are absolute and it is
    * possible only when we get diskUUID as userPaths. So, all
    * mount points are considered instead of the userPaths provided.
    */
   if (userPaths == NULL ||
       Str_Strncmp(userPaths, "all", sizeof "all") == 0 ||
       userPaths[0] != '/') {
      paths = SyncDriverLocalMounts();
   } else {
      /*
       * The sync driver API specifies spaces as separators.
       */
      while (*userPaths != '\0') {
         const char *c;
         char *path;

         if (*userPaths == ' ') {
            /*
             * Trim spaces from beginning
             */
            userPaths++;
            continue;
         }

         c = strchr(userPaths, ' ');
         if (c == NULL) {
            path = Util_SafeStrdup(userPaths);
            paths = g_slist_append(paths, path);
            break;
         } else {
            path = Util_SafeStrndup(userPaths, c - userPaths);
            paths = g_slist_append(paths, path);
            userPaths = c;
         }
      }
   }

   if (paths == NULL) {
      Warning(LGPFX "No paths to freeze.\n");
      return SD_ERROR;
   }

   while (err == SD_UNAVAILABLE && i < ARRAYSIZE(gBackends)) {
      SyncFreezeFn freezeFn = gBackends[i];
      Debug(LGPFX "Calling backend %d.\n", (int) i);
      i++;
#if defined(__linux__) && !defined(USERWORLD)
      if (!enableNullDriver && (freezeFn == NullDriver_Freeze)) {
         Debug(LGPFX "Skipping nullDriver backend.\n");
         continue;
      }
#endif
      err = freezeFn(paths, handle);
   }

   /*
    * g_slist_free_full requires glib >= v2.28
    */
   g_slist_foreach(paths, SyncDriverFreePath, NULL);
   g_slist_free(paths);

   return err == SD_SUCCESS;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_Thaw --
 *
 *    Thaw I/O on previously frozen volumes.
 *
 * Results:
 *    TRUE on success
 *    FALSE on failure
 *
 * Side effects:
 *    See description.
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncDriver_Thaw(const SyncDriverHandle handle) // IN
{
   if (handle->thaw != NULL) {
      return handle->thaw(handle) == SD_SUCCESS;
   }
   return TRUE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_QueryStatus --
 *
 *    Polls the handle and returns the current status of the driver.
 *
 * Results:
 *    SYNCDRIVER_IDLE, since all operations are currently synchronous.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

SyncDriverStatus
SyncDriver_QueryStatus(const SyncDriverHandle handle, // IN
                       int32 timeout)                 // IN
{
   return SYNCDRIVER_IDLE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_CloseHandle --
 *
 *    Closes the handle the sets it to SYNCDRIVER_INVALID_HANDLE.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

void
SyncDriver_CloseHandle(SyncDriverHandle *handle)   // IN/OUT
{
   if (*handle != NULL) {
      if ((*handle)->close != NULL) {
         (*handle)->close(*handle);
      }
      *handle = NULL;
   }
}

