/*********************************************************
 * Copyright (C) 2011-2016 VMware, Inc. All rights reserved.
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

/**
 * @file vmSyncDriver.c
 *
 * A sync driver backend that uses VMware's "vmsync" driver.
 */

#include <fcntl.h>
#include <sys/ioctl.h>
#include <glib.h>
#include "debug.h"
#include "syncDriverInt.h"
#include "syncDriverIoc.h"
#include "strutil.h"
#include "util.h"

#define SYNC_PROC_PATH "/proc/driver/vmware-sync"

typedef struct VmSyncDriver {
   SyncHandle     driver;
   int            fd;
} VmSyncDriver;


/*
 *******************************************************************************
 * VmSyncThaw --                                                          */ /**
 *
 * Thaws filesystems previously frozen.
 *
 * @param[in] handle Handle returned by the freeze operation.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

static SyncDriverErr
VmSyncThaw(const SyncDriverHandle handle)
{
   const VmSyncDriver *sync = (VmSyncDriver *) handle;
   ASSERT(handle != SYNCDRIVER_INVALID_HANDLE);
   return ioctl(sync->fd, SYNC_IOC_THAW) != -1 ? SD_SUCCESS : SD_ERROR;
}


/*
 *******************************************************************************
 * VmSyncClose --                                                         */ /**
 *
 * Closes the descriptor used to talk to the vmsync driver and frees memory
 * associated with it.
 *
 * @param[in] handle Handle to close.
 *
 *******************************************************************************
 */

static void
VmSyncClose(SyncDriverHandle handle)
{
   VmSyncDriver *sync = (VmSyncDriver *) handle;
   close(sync->fd);
   free(sync);
}


/*
 *******************************************************************************
 * VmSync_Freeze --                                                       */ /**
 *
 * Tries to freeze the requested filesystems with the vmsync driver.
 *
 * Opens a description to the driver's proc node, and if successful, send an
 * ioctl to freeze the requested filesystems.
 *
 * @param[in]  paths    List of paths to freeze.
 * @param[out] handle   Where to store the handle to use for thawing.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

SyncDriverErr
VmSync_Freeze(const GSList *paths,
              SyncDriverHandle *handle)
{
   int file;
   Bool first = TRUE;
   char *allPaths = NULL;
   VmSyncDriver *sync = NULL;

   Debug(LGPFX "Freezing using vmsync driver...\n");

   file = open(SYNC_PROC_PATH, O_RDONLY);
   if (file == -1) {
      return SD_UNAVAILABLE;
   }

   /*
    * Ensure we did not get an empty list
    */
   VERIFY(paths != NULL);

   /*
    * Concatenate all paths in the list into one string
    */
   while (paths != NULL) {
      if (!first) {
         /*
          * Append the separator (':')
          */
         StrUtil_SafeStrcat(&allPaths, ":");
      }
      StrUtil_SafeStrcat(&allPaths, paths->data);
      first = FALSE;
      paths = g_slist_next(paths);
   }

   sync = calloc(1, sizeof *sync);
   if (sync == NULL) {
      goto exit;
   }

   sync->driver.thaw = VmSyncThaw;
   sync->driver.close = VmSyncClose;
   sync->fd = file;

   Debug(LGPFX "Freezing %s using vmsync driver...\n", allPaths);

   if (ioctl(file, SYNC_IOC_FREEZE, allPaths) == -1) {
      free(sync);
      sync = NULL;
   }

exit:
   if (sync == NULL) {
      if (file != -1) {
         close(file);
      }
   } else {
      *handle = &sync->driver;
   }
   free(allPaths);
   return sync != NULL ? SD_SUCCESS : SD_ERROR;
}

