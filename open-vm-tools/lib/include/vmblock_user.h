/*********************************************************
 * Copyright (C) 2009-2017 VMware, Inc. All rights reserved.
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

/*********************************************************
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/

/*********************************************************
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/

/*
 * vmblock_user.h --
 *
 *   Provides interfaces that allow user level programs to talk to the
 *   vmblock fs.
 */

#ifndef _VMBLOCK_USER_H_
#define _VMBLOCK_USER_H_

#include "vm_basic_types.h"
#include "vmblock.h"

#if defined(__cplusplus)
extern "C" {
#endif

static INLINE int
VMBLOCK_CONTROL_FUSE(int fd,            // IN
                     char op,           // IN
                     const char *path)  // IN
{
   /*
    * buffer needs room for an operation character and a string with max length
    * PATH_MAX - 1.
    */

   char buffer[PATH_MAX];
   size_t pathLength;

   pathLength = strlen(path);
   if (pathLength >= PATH_MAX) {
      errno = ENAMETOOLONG;
      return -1;
   }

   buffer[0] = op;
   memcpy(buffer + 1, path, pathLength);

   /*
    * The lseek is only to prevent the file pointer from overflowing;
    * vmblock-fuse ignores the file pointer / offset. Overflowing the file
    * pointer causes write to fail:
    * http://article.gmane.org/gmane.comp.file-systems.fuse.devel/6648
    * There's also a race condition here where many threads all calling
    * VMBLOCK_CONTROL at the same time could have all their seeks executed one
    * after the other, followed by all the writes. Again, it's not a problem
    * unless the file pointer overflows which is very unlikely with 32 bit
    * offsets and practically impossible with 64 bit offsets.
    */

   if (lseek(fd, 0, SEEK_SET) < 0) {
      return -1;
   }

   if (write(fd, buffer, pathLength + 1) < 0) {
      return -1;
   }

   return 0;
}

#if defined(vmblock_fuse)

#define VMBLOCK_CONTROL(fd, op, path) VMBLOCK_CONTROL_FUSE(fd, op, path)

#elif defined(__linux__)

static INLINE int
VMBLOCK_CONTROL(int fd, int op, const char *path)
{
   return write(fd, path, op);
}

#elif defined(__FreeBSD__)

static INLINE int
VMBLOCK_CONTROL(int fd, int cmd, const char *path)
{
   char tpath[MAXPATHLEN];
   size_t pathSize;

   if (path == NULL) {
      return -1;
   }

  /*
   * FreeBSD's ioctl data parameters must be of fixed size.  Guarantee a safe
   * buffer of size MAXPATHLEN by copying the user's string to one of our own.
   */
   pathSize = strlcpy(tpath, path, MAXPATHLEN);
   if (pathSize >= sizeof tpath) {
      return -1;
   }

   return ioctl(fd, cmd, tpath);
}

#elif defined(sun)

static INLINE int
VMBLOCK_CONTROL(int fd, int cmd, const char *path)
{
   return ioctl(fd, cmd, path);
}

#endif

#if defined(__cplusplus)
}  // extern "C"
#endif

#endif // _VMBLOCK_USER_H_
