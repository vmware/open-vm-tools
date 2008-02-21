/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation version 2 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 *
 *********************************************************/

/*
 * vmblock.h --
 *
 *   User-level interface to the vmblock device.
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
#if defined(linux)
# define VMBLOCK_ADD_FILEBLOCK          98
# define VMBLOCK_DEL_FILEBLOCK          99
# ifdef VMX86_DEVEL
#  define VMBLOCK_LIST_FILEBLOCKS       100
# endif
# define VMBLOCK_CONTROL_DIRNAME        VMBLOCK_FS_NAME
# define VMBLOCK_CONTROL_DEVNAME        "dev"
# define VMBLOCK_CONTROL_MOUNTPOINT     "mountPoint"

# define VMBLOCK_MOUNT_POINT            "/proc/fs/" VMBLOCK_CONTROL_DIRNAME   \
                                       "/" VMBLOCK_CONTROL_MOUNTPOINT
# define VMBLOCK_DEVICE                 "/proc/fs/" VMBLOCK_CONTROL_DIRNAME   \
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
