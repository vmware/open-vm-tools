/*********************************************************
 * Copyright (C) 2006-2008 VMware, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of VMware Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission of VMware Inc.
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

/*
 * vmblock.h --
 *
 *   User-level interface to the vmblock device.
 *
 *   VMBLOCK_DEVICE should be opened with VMBLOCK_DEVICE_MODE mode. Then
 *   VMBLOCK_CONTROL should be called to perform blocking operations.
 *   The links which can be blocked are in the directory VMBLOCK_MOUNT_POINT.
 *
 *   VMBLOCK_CONTROL takes the file descriptor of the VMBLOCK_DEVICE, an
 *   operation, and the path of the target of the file being operated on (if
 *   applicable).
 *
 *   The operation should be one of:
 *   VMBLOCK_ADD_FILEBLOCK
 *   VMBLOCK_DEL_FILEBLOCK
 *   VMBLOCK_LIST_FILEBLOCKS
 *
 *   path should be something in /tmp/VMwareDnD/ rather than in
 *   VMBLOCK_MOUNT_POINT.
 *
 *   VMBLOCK_CONTROL returns 0 on success or returns -1 and sets errno on
 *   failure.
 */

#ifndef _VMBLOCK_H_
#define _VMBLOCK_H_

#if defined(sun) || defined(__FreeBSD__)
# include <sys/ioccom.h>
#endif

#if defined(__FreeBSD__)
# include <sys/param.h>
#endif

#define VMBLOCK_FS_NAME                "vmblock"

/* Commands for the control half of vmblock driver */
#if defined(vmblock_fuse)
# include <unistd.h>
# include <limits.h>
# include <string.h>
# include <errno.h>
# include "vm_basic_types.h"
# define VMBLOCK_ADD_FILEBLOCK        'a'
# define VMBLOCK_DEL_FILEBLOCK        'd'
# ifdef VMX86_DEVEL
#  define VMBLOCK_LIST_FILEBLOCKS     'l'
# endif /* VMX86_DEVEL */
/*
 * Some of the following names don't actually make much sense on their own.
 * They're used for consistency with the other ports. See the file header for
 * explanations of what they're used for.
 */
# define VMBLOCK_DEVICE_NAME          "dev"
# define VMBLOCK_CONTROL_MOUNTPOINT   "blockdir"
# define VMBLOCK_DEVICE               "/tmp/vmblock/" VMBLOCK_DEVICE_NAME
# define VMBLOCK_DEVICE_MODE          O_WRONLY
# define VMBLOCK_MOUNT_POINT          "/tmp/vmblock/" VMBLOCK_CONTROL_MOUNTPOINT
static INLINE ssize_t
         VMBLOCK_CONTROL(int fd, char op, const char *path)
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

#elif defined(linux)
# define VMBLOCK_ADD_FILEBLOCK          98
# define VMBLOCK_DEL_FILEBLOCK          99
# ifdef VMX86_DEVEL
#  define VMBLOCK_LIST_FILEBLOCKS       100
# endif
# define VMBLOCK_CONTROL_DIRNAME        VMBLOCK_FS_NAME
# define VMBLOCK_CONTROL_DEVNAME        "dev"
# define VMBLOCK_CONTROL_MOUNTPOINT     "mountPoint"
# define VMBLOCK_CONTROL_PROC_DIRNAME	"fs/" VMBLOCK_CONTROL_DIRNAME

# define VMBLOCK_MOUNT_POINT            "/proc/" VMBLOCK_CONTROL_PROC_DIRNAME   \
                                       "/" VMBLOCK_CONTROL_MOUNTPOINT
# define VMBLOCK_DEVICE                 "/proc/" VMBLOCK_CONTROL_PROC_DIRNAME   \
                                       "/" VMBLOCK_CONTROL_DEVNAME
# define VMBLOCK_DEVICE_MODE            O_WRONLY
# define VMBLOCK_CONTROL(fd, op, path)  write(fd, path, op)

#elif defined(sun) || defined(__FreeBSD__)
# define VMBLOCK_MOUNT_POINT            "/var/run/" VMBLOCK_FS_NAME
# define VMBLOCK_DEVICE                 VMBLOCK_MOUNT_POINT
# define VMBLOCK_DEVICE_MODE            O_RDONLY
# if defined(sun)                       /* if (sun) { */
   /*
    * Construct ioctl(2) commands for blocks.  _IO() is a helper macro to
    * construct unique command values more easily.  I chose 'v' because I
    * didn't see it being used elsewhere, and the command numbers begin at one.
    */
#  define VMBLOCK_ADD_FILEBLOCK          _IO('v', 1)
#  define VMBLOCK_DEL_FILEBLOCK          _IO('v', 2)
#  ifdef VMX86_DEVEL
#   define VMBLOCK_LIST_FILEBLOCKS       _IO('v', 3)
#  endif
#  define VMBLOCK_CONTROL(fd, op, path)  ioctl(fd, op, path)

# elif defined(__FreeBSD__)              /* } else if (FreeBSD) { */
   /*
    * Similar to Solaris, construct ioctl(2) commands for block operations.
    * Since the FreeBSD implementation does not change the user's passed-in
    * data (pathname), we use the _IOW macro to define commands which write
    * to the kernel.  (As opposed to _IOR or _IOWR.)  Groups 'v' and 'V'
    * are taken by terminal drivers, so I opted for group 'Z'.
    */
#  define VMBLOCK_ADD_FILEBLOCK          _IOW('Z', 1, char[MAXPATHLEN] )
#  define VMBLOCK_DEL_FILEBLOCK          _IOW('Z', 2, char[MAXPATHLEN] )
#  ifdef VMX86_DEVEL
#   define VMBLOCK_LIST_FILEBLOCKS       _IO('Z', 3)
#   define VMBLOCK_PURGE_FILEBLOCKS      _IO('Z', 4)
#  endif
   /*
    * FreeBSD's ioctl data parameters must be of fixed size.  Guarantee a safe
    * buffer of size MAXPATHLEN by copying the user's string to one of our own.
    */
#  define VMBLOCK_CONTROL(fd, cmd, path)                                \
({                                                                      \
   char tpath[MAXPATHLEN];                                              \
   if (path != NULL) {                                                  \
      strlcpy(tpath, path, MAXPATHLEN);                                 \
   }                                                                    \
   ioctl((fd), (cmd), tpath);                                           \
})
# endif                                 /* } */
#else
# error "Unknown platform for vmblock."
#endif

#endif /* _VMBLOCK_H_ */
