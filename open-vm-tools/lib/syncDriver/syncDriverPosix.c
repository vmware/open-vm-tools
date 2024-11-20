/*********************************************************
 * Copyright (c) 2005-2019, 2023 VMware, Inc. All rights reserved.
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
  "overlay",
   "shm",
   "tmpfs",
   "btrfs",
   "autofs",
   "cifs",
   "nfs",
   "nfs4",
   "smbfs",
   "vmhgfs"
};

typedef struct {
   const char *prefix;
   size_t len;
} RemoteDevPrefix;

#define DEF_DEV_PREFIX(a) {(a), sizeof((a)) - 1}

static RemoteDevPrefix gRemoteDevPrefixes[] = {
   DEF_DEV_PREFIX("https://"),
   DEF_DEV_PREFIX("http://")
};

#undef DEF_DEV_PREFIX

/* Cached value of excludedFileSystems */
static char *gExcludedFileSystems = NULL;

/* Array of path patterns parsed and compiled from gExcludedFileSystems */
static GPtrArray *gExcludedPathPatterns = NULL;


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverIsRemoteFS  --
 *
 *    Checks whether a file system is remote or not
 *
 * Results:
 *    Returns TRUE for remote file system types or device names,
 *    otherwise FALSE.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SyncDriverIsRemoteFS(const MNTINFO *mntinfo)
{
   size_t i;

   for (i = 0; i < ARRAYSIZE(gRemoteFSTypes); i++) {
      if (Str_Strcmp(gRemoteFSTypes[i], MNTINFO_FSTYPE(mntinfo)) == 0) {
         return TRUE;
      }
   }

   for (i = 0; i < ARRAYSIZE(gRemoteDevPrefixes); i++) {
      if (Str_Strncasecmp(gRemoteDevPrefixes[i].prefix,
                          MNTINFO_NAME(mntinfo),
                          gRemoteDevPrefixes[i].len) == 0) {
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
   GHashTable *devices;
   MNTHANDLE mounts;
   DECLARE_MNTINFO(mntinfo);

   if ((mounts = OPEN_MNTFILE("r")) == NULL) {
      Warning(LGPFX "Failed to open mount point table.\n");
      return NULL;
   }

   devices = g_hash_table_new_full(g_str_hash, g_str_equal, free, free);

   while (GETNEXT_MNTINFO(mounts, mntinfo)) {
      const char *device;
      const char *path;
      const char *prevDevicePath;

      device = MNTINFO_NAME(mntinfo);
      path = MNTINFO_MNTPT(mntinfo);

      /*
       * Skip remote mounts because they are not freezable and opening them
       * could lead to hangs. See PR 1196785.
       */
      if (SyncDriverIsRemoteFS(mntinfo)) {
         Debug(LGPFX "Skipping remote file system, name=%s, mntpt=%s.\n",
               device, path);
         continue;
      }

      /*
       * Avoid adding a path to the list, if we have already got
       * a path mounting the same device path.
       */
      prevDevicePath = g_hash_table_lookup(devices, device);
      if (prevDevicePath != NULL) {
         Debug(LGPFX "Skipping duplicate file system, name=%s, mntpt=%s "
               "(existing path=%s).\n", device, path, prevDevicePath);
         continue;
      }

      g_hash_table_insert(devices, Util_SafeStrdup(device),
                          Util_SafeStrdup(path));

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
      paths = g_slist_prepend(paths, Util_SafeStrdup(path));
   }

   g_hash_table_destroy(devices);
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
 * SyncDriverUpdateExcludedFS --
 *
 *    Update the excluded file system list and compile the path patterns.
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
SyncDriverUpdateExcludedFS(const char *excludedFS)    // IN
{
   gchar **patterns = NULL;
   int i;

   ASSERT((gExcludedFileSystems == NULL && gExcludedPathPatterns == NULL) ||
          (gExcludedFileSystems != NULL && gExcludedPathPatterns != NULL));

   /*
    * Passing a NULL pointer to g_ptr_array_free appears to result in
    * a glib assert array, hence this test.  As per the above assert,
    * either both of gExcludedFileSystems and gExcludedPathPatterns
    * are NULL or both aren't.
    */
   if (gExcludedPathPatterns != NULL) {
      /*
       * Free the data but don't set the pointers to anything here because
       * they're about to get new assignments below.
       */
      g_free(gExcludedFileSystems);
      g_ptr_array_free(gExcludedPathPatterns, TRUE);
   }

   if (excludedFS == NULL) {
      Debug(LGPFX "Set the excluded file system list to (null).\n");
      gExcludedFileSystems = NULL;
      gExcludedPathPatterns = NULL;
      return;
   }

   Debug(LGPFX "Set the excluded file system list to \"%s\".\n", excludedFS);

   gExcludedFileSystems = g_strdup(excludedFS);
   gExcludedPathPatterns =
         g_ptr_array_new_with_free_func((GDestroyNotify) &g_pattern_spec_free);

   patterns = g_strsplit(gExcludedFileSystems, ",", 0);

   for (i = 0; patterns[i] != NULL; ++i) {
      if (patterns[i][0] != '\0') {
         g_ptr_array_add(gExcludedPathPatterns,
                         g_pattern_spec_new(patterns[i]));
      }
   }

   g_strfreev(patterns);
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverIsExcludedFS --
 *
 *    See whether a given path (mount point) matches any of the
 *    file systems to be excluded.
 *
 *    Assumes that the caller has verified that the excluded file
 *    system list is non-empty.
 *
 * Results:
 *    TRUE if the path matches at least one of the excluded path patterns
 *    FALSE no matches found
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static Bool
SyncDriverIsExcludedFS(const char *path)                 // IN
{
   int i;

   ASSERT(gExcludedFileSystems != NULL && gExcludedPathPatterns != NULL);
   ASSERT(path != NULL);

   for (i = 0; i < gExcludedPathPatterns->len; ++i) {
      if (g_pattern_match_string(g_ptr_array_index(gExcludedPathPatterns, i),
                                 path)) {
         return TRUE;
      }
   }
   return FALSE;
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverFilterFS
 *
 *    Remove a specified list of file systems from a list of paths.  The
 *    parameter excludedFS is a string containing a comma-separated
 *    list of patterns describing file systems that are to be excluded
 *    from the the quiescing operation.  The patterns specify the paths
 *    of file system mount points.  This routine removes from pathlist
 *    any paths that match one or more patterns specified in excludedFS.
 *
 * Results:
 *    Modified path list with all paths matching excludedFS removed.
 *
 * Side effects:
 *    Calls other routines that modify the global variables
 *    gExcludedFileSystems and gExcludedPathPatterns, which cache the
 *    information in excludedFS.
 *
 *-----------------------------------------------------------------------------
 */

GSList *
SyncDriverFilterFS(GSList *pathlist,             // IN / OUT
                   const char *excludedFS)       // IN
{
   GSList *current;

   /*
    * Update the excluded file system list if excludedFS has changed.
    */
   if (g_strcmp0(excludedFS, gExcludedFileSystems) != 0) {
      SyncDriverUpdateExcludedFS(excludedFS);
   } else {
      Debug(LGPFX "Leave the excluded file system list as \"%s\".\n",
         (excludedFS != NULL) ? excludedFS : "(null)");
   }

   /*
    * If the excluded file system list is empty, return the path list as is.
    */
   if (gExcludedFileSystems == NULL) {
      return pathlist;
   }

   /*
    * Traverse the path list, removing all file systems that should be
    * excluded.
    */
   current = pathlist;
   while (current != NULL) {
      GSList *next = g_slist_next(current);
      char *path = (char *) current->data;

      if (SyncDriverIsExcludedFS(path)) {
         Debug(LGPFX "Excluding file system, name=%s\n", path);
         pathlist = g_slist_delete_link(pathlist, current);
         free(path);
      }
      current = next;
   }

   return pathlist;
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
 *    excludedFileSystems is the value of the tools.conf setting of the same
 *    name.  If non-NULL, It's expected to be a list of patterns specifying
 *    file system mount points to be excluded from the freeze operation.
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
SyncDriver_Freeze(const char *userPaths,              // IN
                  Bool enableNullDriver,              // IN
                  SyncDriverHandle *handle,           // OUT
                  const char *excludedFileSystems,    // IN
                  Bool ignoreFrozenFS)                // IN
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

   paths = SyncDriverFilterFS(paths, excludedFileSystems);
   if (paths == NULL) {
      Warning(LGPFX "No file systems to freeze.\n");
      return FALSE;
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
      err = freezeFn(paths, handle, ignoreFrozenFS);
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


#if defined(__linux__)
/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_GetAttr --
 *
 *    Returns attributes of the backend provider for this handle.
 *    If the backend does not supply a getattr function, it's treated
 *    as non-quiescing.
 *
 * Results:
 *    No return value.
 *    Sets OUT parameters:
 *        *name:      pointer to backend provider name
 *        *quiesces:  indicates whether backend is capable of quiescing.
 *
 * Side effects:
 *   None.
 *
 *-----------------------------------------------------------------------------
 */

void
SyncDriver_GetAttr(const SyncDriverHandle handle,  // IN
                   const char **name,              // OUT
                   Bool *quiesces)                 // OUT
{

   if (handle != SYNCDRIVER_INVALID_HANDLE && handle->getattr != NULL) {
      handle->getattr(handle, name, quiesces);
   } else {
      *name = NULL;
      *quiesces = FALSE;
   }
}
#endif /* __linux__ */
