/*********************************************************
 * Copyright (C) 2006-2016 VMware, Inc. All rights reserved.
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
 * mntinfo.h --
 *
 *    Macros to abstract differences between structures and functions used in
 *    accessing information about mounted file systems.
 *
 */


#ifndef __MNTINFO_H__
#define __MNTINFO_H__

#ifdef sun
# include <sys/mnttab.h>
# include <libgen.h>
# include <limits.h>
#elif defined(__linux__)
# include <mntent.h>
#elif defined(__FreeBSD__)
# include <sys/mount.h>
#endif
#include "posix.h"

/*
 *----------------------------------------------------------------------------
 *
 * DECLARE_MNTINFO, MNTINFO
 * OPEN_MNTFILE, GETNEXT_MNTINFO, CLOSE_MNTFILE,
 * MNTINFO_NAME, MNTINFO_FSTYPE, MNTINFO_MNTPT --
 *
 *    Cross-platform macros for accessing information about the mounted file
 *    systems.  This is necessary since the interfaces for getmntent(3) are
 *    slightly different on Linux and Solaris.
 *
 *    DECLARE_MNTINFO() is used to declare the variable used when invoking
 *    GETNEXT_MNTINFO().  MNTINFO is the type that can be used when passing
 *    between functions.
 *
 *    OPEN_MNTFILE() and CLOSE_MNTFILE() must be called before and after
 *    a series of GETNEXT_MNTINFO() calls, respectively.  GETNEXT_MNTINFO() is
 *    called successively to retrieve information about the next mounted file
 *    system.
 *
 *    MNTINFO_NAME, MNTINFO_FSTYPE, and MNTINFO_MNTPT retrieve the name, file
 *    system type, and mount point of the provided MNTINFO, respectively.
 *
 *    MNTFILE is a string with the name of the file containing mount
 *    information.
 *
 * Results:
 *    OPEN_MNTFILE:    MNTHANDLE on success, NULL on failure
 *    GETNEXT_MNTINFO: on success, TRUE and mnt is filled with file system's
 *                     information; FALSE when no mounts left or on failure
 *    CLOSE_MNTFILE:   TRUE on success, FALSE on failure
 *
 *    MNTINFO_NAME:    mount's name on success, NULL on failure
 *    MNTINFO_FSTYPE:  mount's file system type on success, NULL on failure
 *    MNTINFO_MNTPT:   mount's mount point on success, NULL on failure
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#ifdef sun
# define MNTFILE                        MNTTAB
# define MNTHANDLE                      FILE *
# define MNTINFO                        struct mnttab
# define DECLARE_MNTINFO(name)          struct mnttab __ ## name; \
                                        struct mnttab *name = &__ ## name
# define OPEN_MNTFILE(mode)             Posix_Fopen(MNTFILE, mode)
# define GETNEXT_MNTINFO(fp, mnt)       (Posix_Getmntent(fp, mnt) == 0)
# define CLOSE_MNTFILE(fp)              (fclose(fp) == 0)
# define MNTINFO_NAME(mnt)              mnt->mnt_special
# define MNTINFO_FSTYPE(mnt)            mnt->mnt_fstype
# define MNTINFO_MNTPT(mnt)             mnt->mnt_mountp
# define MNTINFO_MNT_IS_RO(mnt)         (hasmntopt((mnt), "rw") == NULL)
#elif defined(__linux__)
# define MNTFILE                        MOUNTED
# define MNTHANDLE                      FILE *
# define MNTINFO                        struct mntent
# define DECLARE_MNTINFO(name)          struct mntent *name
# define OPEN_MNTFILE(mode)             Posix_Setmntent(MNTFILE, mode)
# define GETNEXT_MNTINFO(fp, mnt)       ((mnt = Posix_Getmntent(fp)) != NULL)
# define CLOSE_MNTFILE(fp)              (endmntent(fp) == 1)
# define MNTINFO_NAME(mnt)              mnt->mnt_fsname
# define MNTINFO_FSTYPE(mnt)            mnt->mnt_type
# define MNTINFO_MNTPT(mnt)             mnt->mnt_dir
# define MNTINFO_MNT_IS_RO(mnt)         (hasmntopt((mnt), "rw") == NULL)
#elif defined(__FreeBSD__) || defined(__APPLE__)
struct mntHandle {
   struct statfs *mountPoints;  // array of mountpoints per getmntinfo(3)
   int numMountPoints;          // number of elements in mntArray
   int mountIndex;              // current location within mountPoints array
};
# define MNTFILE                        _PATH_FSTAB
# define MNTHANDLE                      struct mntHandle *
# define MNTINFO                        struct statfs
# define DECLARE_MNTINFO(name)          struct statfs __ ## name; \
                                        struct statfs *name = &__ ## name

# define OPEN_MNTFILE(mode)                                             \
({                                                                      \
   MNTHANDLE mntHandle;                                                 \
   mntHandle = malloc(sizeof *mntHandle);                               \
   if (mntHandle != NULL) {                                             \
      mntHandle->numMountPoints = getmntinfo(&mntHandle->mountPoints,   \
                                             MNT_NOWAIT);               \
      mntHandle->mountIndex = 0;                                        \
   }                                                                    \
   mntHandle;                                                           \
})

# define GETNEXT_MNTINFO(mntHandle, mnt)                                \
({                                                                      \
   /* Avoid multiple evaluations/expansions. */                         \
   MNTHANDLE thisHandle = (mntHandle);                                  \
   MNTINFO *thisMnt = (mnt);                                            \
   Bool boolVal = FALSE;                                                \
   ASSERT(thisHandle);                                                  \
   if (thisHandle->mountIndex < thisHandle->numMountPoints) {           \
      memcpy(thisMnt,                                                   \
             &thisHandle->mountPoints[thisHandle->mountIndex],          \
             sizeof *thisMnt);                                          \
      ++thisHandle->mountIndex;                                         \
      boolVal = TRUE;                                                   \
   }                                                                    \
   boolVal;                                                             \
})

# define CLOSE_MNTFILE(mntHandle)                                       \
({                                                                      \
   free(mntHandle);                                                     \
   TRUE;                                                                \
})
# define MNTINFO_NAME(mnt)              mnt->f_mntfromname
# define MNTINFO_FSTYPE(mnt)            mnt->f_fstypename
# define MNTINFO_MNTPT(mnt)             mnt->f_mntonname
# define MNTINFO_MNT_IS_RO(mnt)         ((mnt)->f_flags & MNT_RDONLY)
#else
# error "Define mount information macros for your OS type"
#endif

#endif /* __MNTINFO_H__ */
