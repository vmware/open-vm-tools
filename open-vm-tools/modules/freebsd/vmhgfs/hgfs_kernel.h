/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * hgfs_kernel.h --
 *
 *	Declarations for the FreeBSD Hgfs client kernel module.  All
 *	FreeBSD-specifc source files will include this.
 */

#ifndef _HGFSKERNEL_H_
#define _HGFSKERNEL_H_

/*
 * Intended for the Hgfs client kernel module only.
 */
#define INCLUDE_ALLOW_MODULE
#include "includeCheck.h"

/*
 * System includes
 */

#include <sys/param.h>          // for <everything>
#include <sys/vnode.h>          // for struct vnode

/*
 * VMware includes
 */

#include "dbllnklst.h"

#include "request.h"
#include "state.h"
#include "hgfs.h"
#include "hgfsProto.h"

#include "vm_basic_types.h"
#include "vm_assert.h"


/*
 * Macros
 */

#define HGFS_PAYLOAD_MAX(size)         (HGFS_PACKET_MAX - size)
#define HGFS_FS_NAME                    "vmhgfs"
#define HGFS_FS_NAME_LONG               "VMware Hgfs client"
/*
 * NB: Used only to provide a value for struct vattr::va_blocksize, "blocksize
 * preferred for i/o".
 */
#define HGFS_BLOCKSIZE                  1024

/* Internal error code(s) */
#define HGFS_ERR                        (-1)
#define HGFS_ERR_NULL_INPUT             (-50)
#define HGFS_ERR_NODEV                  (-51)
#define HGFS_ERR_INVAL                  (-52)

#if defined __FreeBSD__
#  define HGFS_MP_TO_MNTFLAGS(mp)                               \
                ((mp)->mnt_flag)
#  define HGFS_MP_SET_SIP(mp, sip)                              \
                ((mp)->mnt_data = (sip))
#  define HGFS_VP_TO_MP(vp) ((vp)->v_mount)
/* Return a pointer to mnt_stat to preserve the interface between Mac OS and FreeBSD. */
#  define HGFS_MP_TO_STATFS(mp) (&(mp)->mnt_stat)
   /* Getting to sip via any vnode */
#  define HGFS_VP_TO_SIP(vp)                                    \
                ((HgfsSuperInfo*)HGFS_VP_TO_MP(vp)->mnt_data)

#  define HGFS_VP_VI_LOCK(vp)                                     \
                (VI_LOCK(vp))
#  define HGFS_VP_VI_UNLOCK(vp)                                   \
                (VI_UNLOCK(vp))
#  define HGFS_VP_ISINUSE(vp, usecount)                           \
                ((vp)->v_usecount > usecount)
#  define HGFS_MP_IS_FORCEUNMOUNT(mp)                             \
                (mp->mnt_kern_flag & MNTK_UNMOUNTF)
#elif defined __APPLE__
#  define HGFS_MP_TO_MNTFLAGS(mp)                               \
                (vfs_flags(mp))
#  define HGFS_MP_SET_SIP(mp, sip)                              \
                (vfs_setfsprivate(mp, sip))
#  define HGFS_VP_TO_MP(vp)      (vnode_mount(vp))
#  define HGFS_MP_TO_STATFS(mp)  (vfs_statfs(mp))
#  define HGFS_VP_TO_SIP(vp)                                    \
                ((HgfsSuperInfo*)vfs_fsprivate(HGFS_VP_TO_MP(vp)))

/*
 * No concept of vnode locks are exposed to the Mac OS VFS layer, so do nothing here for
 * VI_LOCK AND VI_UNLOCK. However, make sure to call the lock functions before using
 * HGFS_VP_ISINUSE to preserve compatability with FreeBSD.
 */
#  define HGFS_VP_VI_LOCK(vp)
#  define HGFS_VP_VI_UNLOCK(vp)
#  define HGFS_VP_ISINUSE(vp, usecount)                           \
                  (vnode_isinuse(vp, usecount))
#  define HGFS_MP_IS_FORCEUNMOUNT(mp)                             \
                  (vfs_isforce(mp))
#endif

#define HGFS_VP_TO_STATFS(vp)  (HGFS_MP_TO_STATFS(HGFS_VP_TO_MP(vp)))

/*
 * Types
 */



/* We call them *Header in the kernel code for clarity. */
typedef HgfsReply       HgfsReplyHeader;
typedef HgfsRequest     HgfsRequestHeader;

/*
 * The global state structure for a single filesystem mount.  This is allocated
 * in HgfsVfsMount() and destroyed in HgfsVfsUnmount().
 */
typedef struct HgfsSuperInfo {
   Bool uidSet;
   uid_t uid;
   Bool gidSet;
   gid_t gid;
   /* Request container */
   HgfsKReqContainerHandle reqs;        /* See request.h. */
   /* For filesystem */
   struct mount *vfsp;                  /* Our filesystem structure */
   struct vnode *rootVnode;             /* Root vnode of the filesystem */
   HgfsFileHashTable fileHashTable;     /* File hash table */
   char volumeName[MAXPATHLEN];         /* Name of the volume or share. */
} HgfsSuperInfo;


/*
 * Global variables
 */

/*
 * The vnode attributes between Mac OS and FreeBSD are very similar but not exactly the
 * same. Fields have names have changed. However, only HgfsAttrToBSD and
 * HgfsSetattrCopy care about the differences so we mash the types together to enable
 * single function signatures.
 */
#if defined __FreeBSD__
   typedef struct vattr HgfsVnodeAttr;
#elif defined __APPLE__
   typedef struct vnode_attr HgfsVnodeAttr;
#endif

#if defined __FreeBSD__
  /* Defined in vnops.c. */
  extern struct vop_vector HgfsVnodeOps;
#elif defined __APPLE__
  /* Export vnops.c file operations. */
  extern errno_t (**HgfsVnodeOps)(void *);
  extern struct vnodeopv_desc *HgfsVnodeOperationVectorDescList[1];
#endif

#endif // ifndef _HGFSKERNEL_H_
