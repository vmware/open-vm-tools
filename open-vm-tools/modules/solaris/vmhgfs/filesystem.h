/*********************************************************
 * Copyright (C) 2004-2016 VMware, Inc. All rights reserved.
 *
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
 * filesystem.h --
 *
 * Includes and definitions for filesystem code.
 *
 */



#ifndef __FILESYSTEM_H_
#define __FILESYSTEM_H_


/*
 * Solaris includes
 */
#include <sys/mntent.h>		/* mount flags */
#include <sys/mount.h>

/*
 * Kernel only includes
 */
#ifdef _KERNEL

#include <sys/modctl.h>         /* mod_fsops, ... */
#include <sys/cmn_err.h>        /* cmn_err() */
#include <sys/ddi.h>            /* cmn_err() */
#include <sys/sunddi.h>         /* cmn_err() */
#include <sys/vfs.h>            /* struct vfs, ... */
#include <sys/vnode.h>          /* struct vnode, ... */

#if SOL11
#include <sys/vfs_opreg.h>      /* fs_operation_def_t, ... */
#endif

#endif /* _KERNEL */

#include "hgfsSolaris.h"

/*
 * Macros
 *
 * XXX - Must place these into a common header file that can be
 * used here and by the Linux client and OS X clients (user and kernel)
 * components.
 */
#define HGFS_MAGIC           (0xbacbacbc)
#define HGFS_FSTYPE          HGFS_FS_NAME

/*
 * Struct passed from mount program to kernel (fs module)
 *
 * ******************* IMPORTANT ****************************
 * XXX - This must be kept compatible with the HgfsMountInfo
 * structure which is defined in hgfsDevLinux.h
 * ******************* IMPORTANT ****************************
 */
typedef struct HgfsMountData {
   uint32_t magic;
   uint32_t size;
   uint32_t version;
   uint32_t fd;
   uint32_t flags;
} HgfsMountData;


#ifdef _KERNEL
/*
 * Macros
 */
#ifdef SOL9
 #define HGFS_VFS_FLAGS         VFS_NOSUID
#else
 #define HGFS_VFS_FLAGS         VFS_NOSETUID
#endif

#ifdef SOL9
 #define HGFS_VFS_VERSION       2
#elif defined SOL10
 #define HGFS_VFS_VERSION       3
#else
 #define HGFS_VFS_VERSION       5
#endif

#define HGFS_VFS_BSIZE          HGFS_PACKET_MAX
#define HGFS_COPYIN_FLAGS	(0)
#define HGFS_VFS_TO_SI(vfsp)	((HgfsSuperInfo *)vfsp->vfs_data)

/* This macro is used for both vnode ops and vfs ops. */
#if defined SOL9 || defined SOL10
#define HGFS_VOP(vopName, vopFn, hgfsFn) { vopName, hgfsFn }
#else
#define HGFS_VOP(vopName, vopFn, hgfsFn) { vopName, { .vopFn = hgfsFn } }
#endif

/*
 * Functions
 */
/* To abstract Solaris 9 and 10 differences in suser() calls */
int HgfsSuser(struct cred *cr);

void HgfsFreeVfsOps(void);

/*
 * Extern variables
 */
#ifdef SOL9
EXTERN struct vfsops HgfsVfsOps;
#endif

#endif /* _KERNEL */


#endif /* __FILESYSTEM_H_ */
