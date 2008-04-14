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

#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "debug.h"
#include "dynbuf.h"
#include "str.h"
#include "strutil.h"
#include "syncDriver.h"
#include "util.h"

#if defined(linux)
#include "syncDriverIoc.h"
#include "mntinfo.h"
#endif


#define SYNC_PROC_PATH "/proc/driver/vmware-sync"


#if defined(linux)
/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriverDebug --
 *
 *    Calls Debug(), preserving the value of errno.
 *
 * Results:
 *    None
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

static INLINE void
SyncDriverDebug(const char *msg)
{
   int savedErrno = errno;
   Debug("SyncDriver: %s (%d: %s)\n", msg, errno, strerror(errno));
   errno = savedErrno;
}



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
   FILE *mounts;
   DECLARE_MNTINFO(mntinfo);

   if ((mounts = OPEN_MNTFILE("r")) == NULL) {
      SyncDriverDebug("error opening mtab file");
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
         Debug("SyncDriver: failed to append to buffer\n");
         goto exit;
      }
   }

   if (!DynBuf_Append(&buf, "\0", 1)) {
      Debug("SyncDriver: failed to append to buffer\n");
      goto exit;
   }

   paths = DynBuf_AllocGet(&buf);
   if (paths == NULL) {
      Debug("SyncDriver: failed to allocate path list.\n");
   }

exit:
   DynBuf_Destroy(&buf);
   (void) CLOSE_MNTFILE(mounts);
   return paths;
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_DrivesAreFrozen --
 *
 *    Report whether any drives are currently frozen. The handle can be
 *    invalid, in which case the function will try to open the driver
 *    proc node for querying and close it afterwards.
 *
 * Results:
 *    Whether there are frozen devices.
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncDriver_DrivesAreFrozen(void)
{
#if defined(linux)
   int ret;
   int file;
   int32 active = 0;

   file = open(SYNC_PROC_PATH, O_RDONLY);
   if (file == -1) {
      return FALSE;
   }

   ret = ioctl(file, SYNC_IOC_QUERY, &active);
   if (ret == -1) {
      active = 0;
   }

   close(file);
   return (active > 0);
#else
   return FALSE;
#endif
}


/*
 *-----------------------------------------------------------------------------
 *
 * SyncDriver_Init --
 *
 *    Checks whether the sync driver is loaded.
 *
 * Results:
 *    TRUE if driver is loaded (proc node exists)
 *    FALSE otherwise
 *
 * Side effects:
 *    None
 *
 *-----------------------------------------------------------------------------
 */

Bool
SyncDriver_Init(void)
{
#if defined(linux)
   struct stat info;

   if (stat(SYNC_PROC_PATH, &info) == 0) {
      return S_ISREG(info.st_mode);
   } else {
      return FALSE;
   }
#else
   return FALSE;
#endif
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
#if defined(linux)
   int ret = -1;
   int file;
   char *paths = NULL;

   file = open(SYNC_PROC_PATH, O_RDONLY);
   if (file == -1) {
      goto exit;
   }

   if (userPaths == NULL || Str_Strncmp(userPaths, "all", sizeof "all") == 0) {
      paths = SyncDriverListMounts();
      if (paths == NULL) {
         goto exit;
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

   ret = ioctl(file, SYNC_IOC_FREEZE, paths);

exit:
   if (ret == -1) {
      SyncDriverDebug("SYNC_IOC_FREEZE failed");
      if (file != -1) {
         close(file);
         file = -1;
      }
   }
   free(paths);
   *handle = file;
   return (ret != -1);
#else
   NOT_IMPLEMENTED();
#endif
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
#if defined(linux)
   int ret;
   ASSERT(handle != SYNCDRIVER_INVALID_HANDLE);

   ret = ioctl(handle, SYNC_IOC_THAW);
   if (ret == -1) {
      int _errno = errno;
      SyncDriverDebug("SYNC_IOC_THAW ioctl failed");
      errno = _errno;
   }

   return (ret != -1);
#else
   NOT_IMPLEMENTED();
#endif
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
   ASSERT(handle != NULL);
   if (*handle != SYNCDRIVER_INVALID_HANDLE) {
      close(*handle);
      *handle = SYNCDRIVER_INVALID_HANDLE;
   }
}

