/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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
 * vfsops.c --
 *
 *      VFS operations for vmblock file system.
 */

#include <sys/types.h>
#include <sys/kmem.h>      /* kmem_zalloc() */
#include <sys/errno.h>     /* error codes */
#include <sys/mount.h>     /* MS_OVERLAY */
#include <sys/sysmacros.h> /* makedevice macro */
#include <sys/systm.h>     /* cmpldev() */
#include <sys/policy.h>    /* secpolicy_fs_mount() */

#include "module.h"


/*
 * Variables
 */
vfsops_t *vmblockVfsOps;

static major_t vmblockMajor;
static minor_t vmblockMinor;
static kmutex_t vmblockMutex;


/*
 * Prototypes
 */
int VMBlockVnodeGet(struct vnode **vpp, struct vnode *realVp,
                    const char *name, size_t nameLen,
                    struct vnode *dvp, struct vfs *vfsp, Bool isRoot);
int VMBlockVnodePut(struct vnode *vp);
static int VMBlockMount(struct vfs *vfsp, struct vnode *vnodep,
                        struct mounta *mntp, struct cred *credp);
static int VMBlockUnmount(struct vfs *vfsp, int mflag, struct cred *credp);
static int VMBlockRoot(struct vfs *vfsp, struct vnode **vnodepp);
static int VMBlockStatvfs(struct vfs *vfsp, struct statvfs64 *stats);



/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVnodeGet --
 *
 *    Creates a vnode.
 *
 *    Note that realVp is assumed to be held (see the comment in the function
 *    for further explanation).
 *
 * Results:
 *    Returns zero on success and a non-zero error code on failure.  On
 *    success, vpp is filled in with a new, held vnode.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockVnodeGet(struct vnode **vpp,        // OUT: Filled with address of new vnode
                struct vnode *realVp,      // IN:  Real vnode (assumed held)
                const char *name,          // IN:  Relative name of the file
                size_t nameLen,            // IN:  Size of name
                struct vnode *dvp,         // IN:  Parent directory's vnode
                struct vfs *vfsp,          // IN:  Filesystem structure
                Bool isRoot)               // IN:  If is root directory of fs
{
   VMBlockVnodeInfo *vip;
   struct vnode *vp;
   char *curr;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockVnodeGet: entry\n");

   ASSERT(vpp);
   ASSERT(realVp);
   ASSERT(vfsp);
   ASSERT(name);
   ASSERT(dvp || isRoot);

   vp = vn_alloc(KM_SLEEP);
   if (!vp) {
      return ENOMEM;
   }

   vip = kmem_zalloc(sizeof *vip, KM_SLEEP);
   vp->v_data = (void *)vip;

   /*
    * Store the path that this file redirects to.  For the root vnode we just
    * store the provided path, but for all others we first copy in the parent
    * directory's path.
    */
   curr = vip->name;

   if (!isRoot) {
      VMBlockVnodeInfo *dvip = VPTOVIP(dvp);
      if (dvip->nameLen + 1 + nameLen + 1 >= sizeof vip->name) {
         ret = ENAMETOOLONG;
         goto error;
      }

      memcpy(vip->name, dvip->name, dvip->nameLen);
      vip->name[dvip->nameLen] = '/';
      curr = vip->name + dvip->nameLen + 1;
   }

   if (nameLen + 1 > (sizeof vip->name - (curr - vip->name))) {
      ret = ENAMETOOLONG;
      goto error;
   }

   memcpy(curr, name, nameLen);
   curr[nameLen] = '\0';
   vip->nameLen = nameLen + (curr - vip->name);

   /*
    * We require the caller to have held realVp so we don't need VN_HOLD() it
    * here here even though we VN_RELE() this vnode in VMBlockVnodePut().
    * Despite seeming awkward, this is more natural since the function that our
    * caller obtained realVp from provided a held vnode.
    */
   vip->realVnode = realVp;

   /*
    * Now we'll initialize the vnode.  We need to set the file type, vnode
    * operations, flags, filesystem pointer, reference count, and device.
    */
   /* The root directory is our only directory; the rest are symlinks. */
   vp->v_type = isRoot ? VDIR : VLNK;

   vn_setops(vp, vmblockVnodeOps);

   vp->v_flag  = VNOMAP | VNOMOUNT | VNOSWAP | isRoot ? VROOT : 0;
   vp->v_vfsp  = vfsp;
   vp->v_rdev  = NODEV;

   /* Fill in the provided address with the new vnode. */
   *vpp = vp;

   return 0;

error:
   kmem_free(vip, sizeof *vip);
   vn_free(vp);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVnodePut --
 *
 *    Frees state associated with provided vnode.
 *
 * Results:
 *    Zero on success, non-zero error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockVnodePut(struct vnode *vp)
{
   VMBlockVnodeInfo *vip;
   struct vnode *realVnode;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockVnodePut: entry (%p)\n", vp);

   mutex_enter(&vp->v_lock);
   if (vp->v_count > 1) {
      vp->v_count--;
      mutex_exit(&vp->v_lock);
      return 0;
   }
   mutex_exit(&vp->v_lock);

   vip = (VMBlockVnodeInfo *)vp->v_data;
   realVnode = vip->realVnode;

   kmem_free(vip, sizeof *vip);
   vn_free(vp);
   /*
    * VMBlockVnodeGet() doesn't VN_HOLD() the real vnode, but all callers of it
    * will have the vnode held, so we need to VN_RELE() here.
    */
   VN_RELE(realVnode);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInit --
 *
 *    This is the file system initialization routine.  It creates an array of
 *    fs_operation_def_t for all the vfs operations, then calls vfs_makefsops()
 *    and vfs_setfsops() to assign them to the file system properly.
 *
 * Results:
 *    Returns zero on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockInit(int fstype,    // IN: file system type
            char *name)    // IN: Name of the file system
{
   int ret;
   static const fs_operation_def_t vfsOpsArr[] = {
      VMBLOCK_VOP(VFSNAME_MOUNT, vfs_mount, VMBlockMount),
      VMBLOCK_VOP(VFSNAME_UNMOUNT, vfs_unmount, VMBlockUnmount),
      VMBLOCK_VOP(VFSNAME_ROOT, vfs_root, VMBlockRoot),
      VMBLOCK_VOP(VFSNAME_STATVFS, vfs_statvfs, VMBlockStatvfs),
      { NULL }
   };

   if (!name) {
      Warning("VMBlockInit: received NULL input from kernel.\n");
      return EINVAL;
   }

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockInit: fstype=%d, name=\"%s\"\n", fstype, name);

   /*
    * Set our file system type and the vfs operations in the kernel's VFS
    * switch table.
    */
   vmblockType = fstype;

   ret = vfs_setfsops(vmblockType, vfsOpsArr, &vmblockVfsOps);
   if (ret) {
      Warning("VMBlockInit: could not set vfs operations.\n");
      return ret;
   }

   ret = vn_make_ops(name, vnodeOpsArr, &vmblockVnodeOps);
   if (ret) {
      Warning("VMBlockInit: could not create vnode operations.\n");
      /*
       * It's important not to call vfs_freevfsops() here; that's only for
       * freeing ops created with vfs_makefsops().
       */
      vfs_freevfsops_by_type(vmblockType);
      return ret;
   }

   /*
    * We need to find a unique device number for this instance of the module;
    * it will be used at each mount to secure a unique device number and file
    * system identifier.  If one cannot be located, we'll just use zero like
    * other Solaris file systems.
    */
   if ((vmblockMajor = getudev()) == (major_t)-1) {
        Warning("VMBlockInit: could not obtain unique device.\n");
        vmblockMajor = 0;
   }
   vmblockMinor = 0;
   mutex_init(&vmblockMutex, NULL, MUTEX_DEFAULT, NULL);

   return 0;
}


/*
 * VFS Entry Points
 */

/*
 *----------------------------------------------------------------------------
 *
 * VMBlockMount --
 *
 *   This function is invoked when mount(2) is called on our file system.
 *   The file system is mounted on the supplied vnode.
 *
 * Results:
 *   Returns zero on success and an appropriate error code on error.
 *
 * Side effects:
 *   The file system is mounted on top of vnodep.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockMount(struct vfs *vfsp,     // IN: file system to mount
             struct vnode *vnodep, // IN: Vnode that we are mounting on
             struct mounta *mntp,  // IN: Arguments to mount(2) from user
             struct cred *credp)   // IN: Credentials of caller
{
   VMBlockMountInfo *mip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockMount: entry\n");

   /*
    * These next few checks are done by all other Solaris file systems, so
    * let's follow their lead.
    */
   ret = secpolicy_fs_mount(credp, vnodep, vfsp);
   if (ret) {
      Warning("VMBlockMount: mounting security check failed.\n");
      return ret;
   }

   if (vnodep->v_type != VDIR) {
      Warning("VMBlockMount: not mounting on a directory.\n");
      return ENOTDIR;
   }

   mutex_enter(&vnodep->v_lock);
   if ((mntp->flags & MS_OVERLAY) == 0 &&
       (vnodep->v_count != 1 || (vnodep->v_flag & VROOT))) {
      mutex_exit(&vnodep->v_lock);
      Warning("VMBlockMount: cannot allow unrequested overlay mount.\n");
      return EBUSY;
   }
   mutex_exit(&vnodep->v_lock);

   /*
    * The directory we are redirecting to is specified as the special file
    * since we have no actual device to mount on.  We store that path in the
    * mount information structure (note that there's another allocation inside
    * pn_get() so we must pn_free() that path at unmount time). KM_SLEEP
    * guarantees our memory allocation will succeed (pn_get() uses this flag
    * too).
    */
   mip = kmem_zalloc(sizeof *mip, KM_SLEEP);
   ret = pn_get(mntp->spec,
                (mntp->flags & MS_SYSSPACE) ? UIO_SYSSPACE : UIO_USERSPACE,
                &mip->redirectPath);
   if (ret) {
      Warning("VMBlockMount: could not obtain redirecting directory.\n");
      kmem_free(mip, sizeof *mip);
      return ret;
   }

   /* Do a lookup on the specified path. */
   ret = lookupname(mntp->spec,
                    (mntp->flags & MS_SYSSPACE) ? UIO_SYSSPACE : UIO_USERSPACE,
                    FOLLOW,
                    NULLVPP,
                    &mip->redirectVnode);
   if (ret) {
      Warning("VMBlockMount: could not obtain redirecting directory.\n");
      goto error_lookup;
   }

   if (mip->redirectVnode->v_type != VDIR) {
      Warning("VMBlockMount: not redirecting to a directory.\n");
      ret = ENOTDIR;
      goto error;
   }

   /*
    * Initialize our vfs structure.
    */
   vfsp->vfs_vnodecovered = vnodep;
   vfsp->vfs_flag &= ~VFS_UNMOUNTED;
   vfsp->vfs_flag |= VMBLOCK_VFS_FLAGS;
   vfsp->vfs_bsize = PAGESIZE;
   vfsp->vfs_fstype = vmblockType;
   vfsp->vfs_bcount = 0;
   /* If we had mount options, we'd call vfs_setmntopt with vfsp->vfs_mntopts */

   /* Locate a unique device minor number for this mount. */
   mutex_enter(&vmblockMutex);
   do {
      vfsp->vfs_dev = makedevice(vmblockMajor, vmblockMinor);
      vmblockMinor = (vmblockMinor + 1) & L_MAXMIN32;
   } while (vfs_devismounted(vfsp->vfs_dev));
   mutex_exit(&vmblockMutex);

   vfs_make_fsid(&vfsp->vfs_fsid, vfsp->vfs_dev, vmblockType);
   vfsp->vfs_data = (caddr_t)mip;

   /*
    * Now create the root vnode of the file system.
    */
   ret = VMBlockVnodeGet(&mip->root, mip->redirectVnode,
                         mip->redirectPath.pn_path,
                         mip->redirectPath.pn_pathlen,
                         NULL, vfsp, TRUE);
   if (ret) {
      Warning("VMBlockMount: couldn't create root vnode.\n");
      ret = EFAULT;
      goto error;
   }

   VN_HOLD(vfsp->vfs_vnodecovered);
   return 0;

error:
   /* lookupname() provides a held vnode. */
   VN_RELE(mip->redirectVnode);
error_lookup:
   pn_free(&mip->redirectPath);
   kmem_free(mip, sizeof *mip);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockUnmount --
 *
 *    This function is invoked when umount(2) is called on our file system.
 *
 * Results:
 *    Returns zero on success and an error code on error.
 *
 * Side effects:
 *    The root vnode will be freed.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockUnmount(struct vfs *vfsp,   // IN: This file system
               int flag,           // IN: Unmount flags
               struct cred *credp) // IN: Credentials of caller
{
   VMBlockMountInfo *mip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockUnmount: entry\n");

   ret = secpolicy_fs_unmount(credp, vfsp);
   if (ret) {
      return ret;
   }

   mip = (VMBlockMountInfo *)vfsp->vfs_data;

   mutex_enter(&mip->root->v_lock);
   if (mip->root->v_count > 1) {
      mutex_exit(&mip->root->v_lock);
      return EBUSY;
   }
   mutex_exit(&mip->root->v_lock);

   VN_RELE(vfsp->vfs_vnodecovered);
   /*
    * We don't need to VN_RELE() mip->redirectVnode since it's the realVnode
    * for mip->root.  That means when we VN_RELE() mip->root and
    * VMBlockInactive() is called, VMBlockVnodePut() will VN_RELE()
    * mip->redirectVnode for us.  It's like magic, but better.
    */
   VN_RELE(mip->root);

   pn_free(&mip->redirectPath);
   kmem_free(mip, sizeof *mip);

   vfsp->vfs_flag |= VFS_UNMOUNTED;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockRoot --
 *
 *    This supplies the root vnode for the file system.
 *
 * Results:
 *    Returns zero on success and an error code on error.  On success vnodepp
 *    is set to the pointer of the root vnode.
 *
 * Side effects:
 *    The root vnode's reference count is incremented by one.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockRoot(struct vfs *vfsp,       // IN: file system to find root vnode of
            struct vnode **vnodepp) // OUT: Set to pointer to root vnode of this fs
{
   VMBlockMountInfo *mip;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockRoot: entry\n");

   mip = (VMBlockMountInfo *)vfsp->vfs_data;

   VN_HOLD(mip->root);
   *vnodepp = mip->root;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockStatvfs --
 *
 *    Provides statistics for the provided file system.  The values provided
 *    by this function are fake.
 *
 * Results:
 *    Returns zero on success and a non-zero error code on exit.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
VMBlockStatvfs(struct vfs *vfsp,         // IN: file system to get statistics for
               struct statvfs64 *stats)  // OUT: Statistics are placed into this struct
{
   dev32_t dev32;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockStatvfs: entry\n");

   /* Clear stats struct, then fill it in with our values. */
   memset(stats, 0, sizeof *stats);

   /*
    * Macros in case we need these elsewhere later.
    *
    * Since vmblock does not provide any actual storage, we use zero so that
    * the output of df(1) is pleasant for users.
    */
   #define VMBLOCK_BLOCKSIZE       PAGESIZE
   #define VMBLOCK_BLOCKS_TOTAL    0
   #define VMBLOCK_BLOCKS_FREE     0
   #define VMBLOCK_BLOCKS_AVAIL    0
   #define VMBLOCK_FILES_TOTAL     0
   #define VMBLOCK_FILES_FREE      0
   #define VMBLOCK_FILES_AVAIL     0

   /* Compress the device number to 32-bits for consistency on 64-bit systems. */
   cmpldev(&dev32, vfsp->vfs_dev);

   stats->f_bsize   = VMBLOCK_BLOCKSIZE;        /* Preferred fs block size */
   stats->f_frsize  = VMBLOCK_BLOCKSIZE;        /* Fundamental fs block size */
   /* Next six are u_longlong_t */
   stats->f_blocks  = VMBLOCK_BLOCKS_TOTAL;     /* Total blocks on fs */
   stats->f_bfree   = VMBLOCK_BLOCKS_FREE;      /* Total free blocks */
   stats->f_bavail  = VMBLOCK_BLOCKS_AVAIL;     /* Total blocks avail to non-root */
   stats->f_files   = VMBLOCK_FILES_TOTAL;      /* Total files (inodes) */
   stats->f_ffree   = VMBLOCK_FILES_FREE;       /* Total files free */
   stats->f_favail  = VMBLOCK_FILES_AVAIL;      /* Total files avail to non-root */
   stats->f_fsid    = dev32;                    /* file system id */
   stats->f_flag   &= ST_NOSUID;                /* Flags: we don't support setuid. */
   stats->f_namemax = MAXNAMELEN;               /* Max filename; use Solaris default. */

   /* Memset above and -1 of array size as n below ensure NUL termination. */
   strncpy(stats->f_basetype, VMBLOCK_FS_NAME, sizeof stats->f_basetype - 1);
   strncpy(stats->f_fstr, VMBLOCK_FS_NAME, sizeof stats->f_fstr - 1);

   return 0;
}
