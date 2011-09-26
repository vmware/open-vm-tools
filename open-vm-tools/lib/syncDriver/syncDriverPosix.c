/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
#include "vmware.h"
#include "debug.h"
#include "dynbuf.h"
#include "str.h"
#include "syncDriverInt.h"
#include "util.h"
#include "mntinfo.h"

static SyncFreezeFn gBackends[] = {
#if defined(linux)
   LinuxDriver_Freeze,
   VmSync_Freeze,
   NullDriver_Freeze,
#endif
};


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverListMounts --
 *
 *    Returns a newly allocated string containing a list of colon-separated
 *    mount paths in the system. No filtering is done, so all paths are added.
 *    This assumes that the driver allows "unfreezable" paths to be provided
 *    to the freeze command.
 *
 *    XXX: mntinfo.h mentions Solaris and Linux, but not FreeBSD. If we ever
 *    have a FreeBSD sync driver, we should make sure this function also
 *    works there.
 *
 * Results:
 *    The list of devices to freeze, or NULL on failure.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static char *
SyncDriverListMounts(void)
{
   char *paths = NULL;
   DynBuf buf;
   MNTHANDLE mounts;
   DECLARE_MNTINFO(mntinfo);

   if ((mounts = OPEN_MNTFILE("r")) == NULL) {
      return NULL;
   }

   DynBuf_Init(&buf);

   while (GETNEXT_MNTINFO(mounts, mntinfo)) {
      /*
       * Add a separator if it's not the first path, and add the path to the
       * tail of the list.
       */
      if ((DynBuf_GetSize(&buf) != 0 && !DynBuf_Append(&buf, ":", 1))
          || !DynBuf_Append(&buf,
                            MNTINFO_MNTPT(mntinfo),
                            strlen(MNTINFO_MNTPT(mntinfo)))) {
         goto exit;
      }
   }

   if (!DynBuf_Append(&buf, "\0", 1)) {
      goto exit;
   }

   paths = DynBuf_AllocGet(&buf);
   if (paths == NULL) {
      Debug(LGPFX "Failed to allocate path list.\n");
   }

exit:
   DynBuf_Destroy(&buf);
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
 *    error.
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
                  SyncDriverHandle *handle)  // OUT
{
   char *paths = NULL;
   SyncDriverErr err = SD_UNAVAILABLE;
   size_t i = 0;

   /*
    * First, convert the given path list to something the backends will
    * understand: a colon-separated list of paths.
    */
   if (userPaths == NULL || Str_Strncmp(userPaths, "all", sizeof "all") == 0) {
      paths = SyncDriverListMounts();
      if (paths == NULL) {
         Debug(LGPFX "Failed to list mount points.\n");
         return SD_ERROR;
      }
   } else {
      /*
       * The sync driver API specifies spaces as separators, but the driver
       * uses colons as the path separator on Unix.
       */
      char *c;
      paths = Util_SafeStrdup(userPaths);
      for (c = paths; *c != '\0'; c++) {
         if (*c == ' ') {
            *c = ':';
         }
      }
   }

   while (err == SD_UNAVAILABLE && i < ARRAYSIZE(gBackends)) {
      err = gBackends[i++](paths, handle);
   }

   free(paths);
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

