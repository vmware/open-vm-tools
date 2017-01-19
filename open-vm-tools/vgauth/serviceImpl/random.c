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
 * @file random.c --
 *
 *    Random byte generation.
 */

#include "serviceInt.h"
#include "VGAuthLog.h"

#ifdef _WIN32
#include <wincrypt.h>
#else
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#define GENERIC_RANDOM_DEVICE "/dev/urandom"
#endif

/*
 ******************************************************************************
 * ServiceRandomBytes --                                                 */ /**
 *
 * Fills a buffer with random bytes.
 * Taken with minor changes from bora/lib/misc/random.c
 *
 * @param[in]  size      The size of the buffer.
 * @param[out] buffer    The buffer to fill.
 *
 * @return VGAUTH_E_OK on success, VGAuthError on failure
 *
 ******************************************************************************
 */

VGAuthError
ServiceRandomBytes(int size,
                   guchar *buffer)
{
#if defined(_WIN32)
   HCRYPTPROV csp;

   if (CryptAcquireContext(&csp, NULL, NULL, PROV_RSA_FULL,
                           CRYPT_VERIFYCONTEXT) == FALSE) {
      VGAUTH_LOG_ERR_WIN("CryptAcquireContext failed\n");
      return VGAUTH_E_FAIL;
   }

   if (CryptGenRandom(csp, size, buffer) == FALSE) {
      CryptReleaseContext(csp, 0);
      VGAUTH_LOG_ERR_WIN("CryptGenRandom failed\n");
      return VGAUTH_E_FAIL;
   }

   if (CryptReleaseContext(csp, 0) == FALSE) {
      VGAUTH_LOG_ERR_WIN("CryptReleaseContext failed\n");
      return VGAUTH_E_FAIL;
   }
#else
   int fd;

   /*
    * We use /dev/urandom and not /dev/random because it is good enough and
    * because it cannot block. --hpreg
    */

   fd = open(GENERIC_RANDOM_DEVICE, O_RDONLY);
   if (fd == -1) {
      Warning("%s: Failed to open random device: %d\n", __FUNCTION__, errno);

      return VGAUTH_E_FAIL;
   }

   /* Although /dev/urandom does not block, it can return short reads. */
   while (size > 0) {
      gsize bytesRead = read(fd, buffer, size);

      if (bytesRead == 0 || (bytesRead == -1 && errno != EINTR)) {
         int error = errno;

         close(fd);
         Warning("%s: read error: %d\n", __FUNCTION__, error);

         return VGAUTH_E_FAIL;
      }
      if (bytesRead > 0) {
         size -= bytesRead;
         buffer = ((guchar *) buffer) + bytesRead;
      }
   }

   /*
    * Note we don't retry the close() on an EINTR:
    * http://linux.derkeiler.com/Mailing-Lists/Kernel/2005-09/3000.html
    *
    * This'll probably turn out to be in conflict with some other flavor
    * of *ix.
    */
   if (close(fd) == -1) {
      Warning("%s: Failed to close: %d\n", __FUNCTION__, errno);

      return VGAUTH_E_FAIL;
   }
#endif

   return VGAUTH_E_OK;
}
