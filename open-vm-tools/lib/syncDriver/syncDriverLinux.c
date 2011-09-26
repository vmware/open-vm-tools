/*********************************************************
 * Copyright (C) 2011 VMware, Inc. All rights reserved.
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
 * @file syncDriverLinux.c
 *
 * A sync driver backend that uses the Linux "FIFREEZE" and "FITHAW" ioctls
 * to freeze and thaw file systems.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <linux/fs.h>
#include <sys/ioctl.h>
#include "debug.h"
#include "dynbuf.h"
#include "strutil.h"
#include "syncDriverInt.h"

/* Out toolchain headers are somewhat outdated and don't define these. */
#if !defined(FIFREEZE)
#  define FIFREEZE        _IOWR('X', 119, int)    /* Freeze */
#  define FITHAW          _IOWR('X', 120, int)    /* Thaw */
#endif


typedef struct LinuxDriver {
   SyncHandle  driver;
   size_t      fdCnt;
   int        *fds;
} LinuxDriver;


/*
 *******************************************************************************
 * LinuxFiThaw --                                                         */ /**
 *
 * Thaws the file systems monitored by the given handle. Tries to thaw all the
 * file systems even if an error occurs in one of them.
 *
 * @param[in] handle Handle returned by the freeze call.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

static SyncDriverErr
LinuxFiThaw(const SyncDriverHandle handle)
{
   size_t i;
   LinuxDriver *sync = (LinuxDriver *) handle;
   SyncDriverErr err = SD_SUCCESS;

   for (i = 0; i < sync->fdCnt; i++) {
      if (ioctl(sync->fds[i], FITHAW) == -1) {
         err = SD_ERROR;
      }
   }

   return err;
}


/*
 *******************************************************************************
 * LinuxFiClose --                                                        */ /**
 *
 * Closes the file descriptors used for freezing, and frees memory associated
 * with the handle.
 *
 * @param[in] handle Handle to close.
 *
 *******************************************************************************
 */

static void
LinuxFiClose(SyncDriverHandle handle)
{
   LinuxDriver *sync = (LinuxDriver *) handle;
   size_t i;

   for (i = 0; i < sync->fdCnt; i++) {
      close(sync->fds[i]);
   }
   free(sync->fds);
   free(sync);
}


/*
 *******************************************************************************
 * LinuxDriver_Freeze --                                                  */ /**
 *
 * Tries to freeze the filesystems using the Linux kernel's FIFREEZE ioctl.
 *
 * If the first attempt at using the ioctl fails, assume that it doesn't exist
 * and return SD_UNAVAILABLE, so that other means of freezing are tried.
 *
 * @param[in]  paths    Paths to freeze (colon-separated).
 * @param[out] handle   Handle to use for thawing.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

SyncDriverErr
LinuxDriver_Freeze(const char *paths,
                   SyncDriverHandle *handle)
{
   char *path;
   int fd;
   size_t count = 0;
   unsigned int index = 0;
   Bool first = TRUE;
   DynBuf fds;
   LinuxDriver *sync = NULL;
   SyncDriverErr err = SD_SUCCESS;

   DynBuf_Init(&fds);

   Debug(LGPFX "Freezing using Linux ioctls...\n");

   sync = calloc(1, sizeof *sync);
   if (sync == NULL) {
      return SD_ERROR;
   }

   sync->driver.thaw = LinuxFiThaw;
   sync->driver.close = LinuxFiClose;

   /*
    * Iterate through the requested paths. If we get an error for the first
    * path, and it's not EPERM, assume that the ioctls are not available in
    * the current kernel.
    */
   while ((path = StrUtil_GetNextToken(&index, paths, ":")) != NULL) {
      fd = open(path, O_RDONLY);
      free(path);

      if (fd == -1) {
         err = SD_ERROR;
         break;
      }

      if (ioctl(fd, FIFREEZE) == -1) {
         /*
          * If the ioctl does not exist, Linux will return ENOTTY. If it's not
          * supported on the device, we get EOPNOTSUPP. Ignore the latter,
          * since freezing does not make sense for all fs types, and some
          * Linux fs drivers may not have been hooked up in the running kernel.
          */
         close(fd);
         if (errno != EOPNOTSUPP) {
            Debug(LGPFX "ioctl failed: %d (%s)\n", errno, strerror(errno));
            err = first ? SD_UNAVAILABLE : SD_ERROR;
            break;
         }
      } else {
         if (!DynBuf_Append(&fds, &fd, sizeof fd)) {
            close(fd);
            err = SD_ERROR;
            break;
         }
         count++;
      }

      first = FALSE;
   }

   if (err != SD_SUCCESS) {
      LinuxFiThaw(&sync->driver);
      LinuxFiClose(&sync->driver);
      DynBuf_Destroy(&fds);
      free(sync);
   } else {
      sync->fds = DynBuf_Detach(&fds);
      sync->fdCnt = count;
      *handle = &sync->driver;
   }
   return err;
}

