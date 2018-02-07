/*********************************************************
 * Copyright (C) 2011-2018 VMware, Inc. All rights reserved.
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
#include <sys/types.h>
#include <sys/stat.h>
#include "debug.h"
#include "dynbuf.h"
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

   /*
    * Thaw in the reverse order of freeze
    */
   for (i = sync->fdCnt; i > 0; i--) {
      Debug(LGPFX "Thawing fd=%d.\n", sync->fds[i-1]);
      if (ioctl(sync->fds[i-1], FITHAW) == -1) {
         Debug(LGPFX "Thaw failed for fd=%d.\n", sync->fds[i-1]);
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

   /*
    * Close in the reverse order of open
    */
   for (i = sync->fdCnt; i > 0; i--) {
      Debug(LGPFX "Closing fd=%d.\n", sync->fds[i-1]);
      close(sync->fds[i-1]);
   }
   free(sync->fds);
   free(sync);
}


/*
 *******************************************************************************
 * LinuxFiGetAttr --                                                      */ /**
 *
 * Return some attributes of the backend provider
 *
 * @param[out] name       name of backend provider
 * @param[out] quiesces   TRUE (FALSE) if provider is (is not) capable
 *                        of quiescing.
 *
 *******************************************************************************
 */

static void
LinuxFiGetAttr(const SyncDriverHandle handle,   // IN (ignored)
               const char **name,               // OUT
               Bool *quiesces)                  // OUT
{
   *name = "fifreeze";
   *quiesces = TRUE;
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
 * NOTE: This function performs two system calls open() and ioctl(). We have
 * seen open() being slow with NFS mount points at times and ioctl() being
 * slow when guest is performing significant IO. Therefore, caller should
 * consider running this function in a separate thread.
 *
 * @param[in]  paths    List of paths to freeze.
 * @param[out] handle   Handle to use for thawing.
 *
 * @return A SyncDriverErr.
 *
 *******************************************************************************
 */

SyncDriverErr
LinuxDriver_Freeze(const GSList *paths,
                   SyncDriverHandle *handle)
{
   ssize_t count = 0;
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
   sync->driver.getattr = LinuxFiGetAttr;

   /*
    * Ensure we did not get an empty list
    */
   VERIFY(paths != NULL);

   /*
    * Iterate through the requested paths. If we get an error for the first
    * path, and it's not EPERM, assume that the ioctls are not available in
    * the current kernel.
    */
   while (paths != NULL) {
      int fd;
      struct stat sbuf;
      const char *path = paths->data;
      Debug(LGPFX "opening path '%s'.\n", path);
      paths = g_slist_next(paths);
      fd = open(path, O_RDONLY);
      if (fd == -1) {
         switch (errno) {
         case ENOENT:
            /*
             * We sometimes get stale mountpoints or special mountpoints
             * created by the docker engine.
             */
            Debug(LGPFX "cannot find the directory '%s'.\n", path);
            continue;

         case EACCES:
            /*
             * We sometimes get access errors to virtual filesystems mounted
             * as users with permission 700, so just ignore these.
             */
            Debug(LGPFX "cannot access mounted directory '%s'.\n", path);
            continue;

         case ENXIO:
            /*
             * A bind-mounted file, such as a mount of /dev/log for a
             * chrooted application, will land us here.  Just skip it.
             */
            Debug(LGPFX "no such device or address '%s'.\n", path);
            continue;

         case EIO:
            /*
             * A mounted HGFS filesystem with the backend disabled will give
             * us these; probably could use a better way to detect HFGS, but
             * this should be enough. Just skip.
             */
            Debug(LGPFX "I/O error reading directory '%s'.\n", path);
            continue;

         default:
            Debug(LGPFX "failed to open '%s': %d (%s)\n",
                  path, errno, strerror(errno));
            err = SD_ERROR;
            goto exit;
         }
      }

      if (fstat(fd, &sbuf) == -1) {
         close(fd);
         Debug(LGPFX "failed to stat '%s': %d (%s)\n",
               path, errno, strerror(errno));
         err = SD_ERROR;
         goto exit;
      }

      if (!S_ISDIR(sbuf.st_mode)) {
         close(fd);
         Debug(LGPFX "Skipping a non-directory path '%s'.\n", path);
         continue;
      }

      Debug(LGPFX "freezing path '%s' (fd=%d).\n", path, fd);
      if (ioctl(fd, FIFREEZE) == -1) {
         int ioctlerr = errno;
         /*
          * If the ioctl does not exist, Linux will return ENOTTY. If it's not
          * supported on the device, we get EOPNOTSUPP. Ignore the latter,
          * since freezing does not make sense for all fs types, and some
          * Linux fs drivers may not have been hooked up in the running kernel.
          *
          * Also ignore EBUSY since we may try to freeze the same superblock
          * more than once depending on the OS configuration (e.g., usage of
          * bind mounts).
          */
         close(fd);
         Debug(LGPFX "freeze on '%s' returned: %d (%s)\n",
               path, ioctlerr, strerror(ioctlerr));
         if (ioctlerr != EBUSY && ioctlerr != EOPNOTSUPP) {
            Debug(LGPFX "failed to freeze '%s': %d (%s)\n",
                  path, ioctlerr, strerror(ioctlerr));
            err = first && ioctlerr == ENOTTY ? SD_UNAVAILABLE : SD_ERROR;
            break;
         }
      } else {
         Debug(LGPFX "successfully froze '%s' (fd=%d).\n", path, fd);
         if (!DynBuf_Append(&fds, &fd, sizeof fd)) {
            if (ioctl(fd, FITHAW) == -1) {
               Warning(LGPFX "failed to thaw '%s': %d (%s)\n",
                       path, errno, strerror(errno));
            }
            close(fd);
            err = SD_ERROR;
            break;
         }
         count++;
      }

      first = FALSE;
   }

exit:
   sync->fds = DynBuf_Detach(&fds);
   sync->fdCnt = count;

   if (err != SD_SUCCESS) {
      LinuxFiThaw(&sync->driver);
      LinuxFiClose(&sync->driver);
   } else {
      *handle = &sync->driver;
   }
   return err;
}

