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
 * vfsops.c --
 *
 *	VFS operations for the FreeBSD Hgfs client.
 */


#include <sys/param.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#if __FreeBSD_version >= 700000
#include <sys/priv.h>
#endif

#include "hgfs_kernel.h"
#include "request.h"
#include "debug.h"
#include "hgfsDevLinux.h"
#include "os.h"
#include "compat_freebsd.h"
#include "vfsopscommon.h"

/*
 * Local functions (prototypes)
 */

static vfs_mount_t      HgfsVfsMount;
static vfs_unmount_t    HgfsVfsUnmount;
static vfs_root_t       HgfsVfsRoot;
static vfs_statfs_t     HgfsVfsStatfs;
static vfs_init_t       HgfsVfsInit;
static vfs_uninit_t     HgfsVfsUninit;


/*
 * Global data
 */

/*
 * Hgfs VFS operations vector
 */
static struct vfsops HgfsVfsOps = {
   .vfs_mount           = HgfsVfsMount,
   .vfs_unmount         = HgfsVfsUnmount,
   .vfs_root            = HgfsVfsRoot,
   .vfs_quotactl        = vfs_stdquotactl,
   .vfs_statfs          = HgfsVfsStatfs,
   .vfs_sync            = vfs_stdsync,
   .vfs_vget            = vfs_stdvget,
   .vfs_fhtovp          = vfs_stdfhtovp,
   .vfs_checkexp        = vfs_stdcheckexp,
#if __FreeBSD_version < 700000
   .vfs_vptofh          = vfs_stdvptofh,
#endif
   .vfs_init            = HgfsVfsInit,
   .vfs_uninit          = HgfsVfsUninit,
   .vfs_extattrctl      = vfs_stdextattrctl,
   .vfs_sysctl          = vfs_stdsysctl,
};

/*
 * Kernel module glue to execute init/uninit at load/unload.
 */
VFS_SET(HgfsVfsOps, vmhgfs, 0);


/*
 * Local functions (definitions)
 */


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsMount
 *
 *      "The VFS_MOUNT() macro mounts a file system into the system's
 *      namespace or updates the attributes of an already mounted file
 *      system."  (VFS_MOUNT(9).)
 *
 * Results:
 *      Zero on success, an appropriate system error on failure.
 *
 * Side effects:
 *      Done.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsMount(struct mount *mp,  // IN: structure representing the file system
             struct thread *td) // IN: thread which is mounting the file system
{
   HgfsSuperInfo *sip;
   struct vnode *vp;
   int ret = 0;
   char *target;
   int error;
   int size;
   Bool *uidSet = NULL;
   int *uid = NULL;
   Bool *gidSet = NULL;
   int *gid = NULL;

   /*
    * - Examine/validate mount flags from userland.
    * - Grab mount options from userland, validate.  (Paths, etc.)
    * - Allocate HgfsSuperInfo, root vnode; bind the two.
    * - Update mnt_flag/mnt_kern_flags (ex: MPSAFE?)
    * - vfs_getnewfsid
    * - vfs_mountedfrom
    */

   /*
    * We do not support any of the user's mount options, so fail any mount
    * attempt with a non-zero mnt_flag.  (It'd be quite a shock to find out
    * that a share successfully mounted read-only is really writeable!)
    */
   if (mp->mnt_flag != 0) {
      return EOPNOTSUPP;
   }

   /*
    * Since Hgfs requires the caller to be root, only allow mount attempts made
    * by the superuser.
    */
   if ((ret = suser(td)) != 0) {
      return ret;
   }

   /*
    * Allocate a new HgfsSuperInfo structure.  This is the super structure
    * maintained for each file system.  (With M_WAITOK, this call cannot fail.)
    */
   sip = os_malloc(sizeof *sip, M_WAITOK | M_ZERO);
   mp->mnt_data = sip;

   error = HgfsInitFileHashTable(&sip->fileHashTable);
   if (error) {
      goto out;
   }

   /*
    * Allocate the root vnode, then record it and the file system information
    * in our superinfo.
    */
   error = HgfsVnodeGetRoot(&vp, sip, mp, "/",
			    HGFS_FILE_TYPE_DIRECTORY, &sip->fileHashTable);
   if (error) {
      HgfsDestroyFileHashTable(&sip->fileHashTable);
      goto out;
   }

   sip->vfsp = mp;
   sip->rootVnode = vp;

   /* We're finished with the root vnode, so unlock it. */
   COMPAT_VOP_UNLOCK(vp, 0, td);

   /*
    * Initialize this file system's Hgfs requests container.
    */
   sip->reqs = HgfsKReq_AllocateContainer();

   /*
    * Since this implementation supports fine-grained locking, inform the kernel
    * that we're MPSAFE.  (This is in the context of protecting its own data
    * structures, not oplocks/leases with the VM's host.)
    */
   MNT_ILOCK(mp);
   mp->mnt_kern_flag |= MNTK_MPSAFE;
   MNT_IUNLOCK(mp);

   /* Get a new unique filesystem ID */
   vfs_getnewfsid(mp);

   error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target, &size);
   if (error || target[size - 1] != '\0') {
      target = "host:hgfs";
   }

   /* Get uidSet */
   error = vfs_getopt(mp->mnt_optnew, "uidSet", (void**)&uidSet, &size);

   if (!error && size == sizeof(Bool) && uidSet) {
      sip->uidSet = *uidSet;
   } else {
      sip->uidSet = FALSE;
   }

   /* Get uid */
   error = vfs_getopt(mp->mnt_optnew, "uid", (void**)&uid, &size);

   if (!error && size == sizeof(int) && uid) {
      sip->uid = *uid;
   } else {
      sip->uidSet = FALSE;
   }

   /* Get gidSet */
   error = vfs_getopt(mp->mnt_optnew, "gidSet", (void**)&gidSet, &size);

   if (!error && size == sizeof(Bool) && gidSet) {
      sip->gidSet = *gidSet;
   } else {
      sip->gidSet = FALSE;
   }

   /* Get gid */
   error = vfs_getopt(mp->mnt_optnew, "gid", (void**)&gid, &size);

   if (!error && size == sizeof(int) && gid) {
      sip->gid = *gid;
   } else {
      sip->gidSet = FALSE;
   }

   vfs_mountedfrom(mp, target);

   /*
    * Fill in the statfs structure. Note that even if HgfsStatfsInt
    * fails, we shall just log the error and move on, since it is
    * not a critical operation.
    */
   error = HgfsStatfsInt(vp, &mp->mnt_stat);
   if (error) {
      DEBUG(VM_DEBUG_FAIL, "HgfsStatfsInt failed with ret = %d\n", ret);
      error = 0;
   }

   DEBUG(VM_DEBUG_LOAD, "Exit\n");

out:
   if (error) {
      os_free(sip, sizeof *sip);
   }

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsUnmount --
 *
 *      "VFS_UNMOUNT -- unmount a file system", VFS_UNMOUNT(9).
 *
 * Results:
 *      Zero if filesystem unmounted, otherwise errno.
 *
 * Side effects:
 *      This call may fail if the filesystem is busy & MNT_FORCE not set.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsUnmount(struct mount *mp, int mntflags, struct thread *td)
{
   HgfsSuperInfo *sip;
   int ret = 0;
   int flags = 0;

   sip = (HgfsSuperInfo *)mp->mnt_data;

   ASSERT(sip);

   /*
    * If there are pending requests & we're not being forced out, tell the user
    * that we're still busy.
    */
   if (((mntflags & MNT_FORCE) == 0) &&
       ((HgfsKReq_ContainerIsEmpty(sip->reqs) == FALSE) ||
        (HgfsFileHashTableIsEmpty(sip, &sip->fileHashTable) == FALSE))) {
      return EBUSY;
   }

   /*
    * If the user wants us out, cancel all pending Hgfs requests and fail all
    * existing vnode operations.
    */
   if (mntflags & MNT_FORCE) {
      HgfsKReq_CancelRequests(sip->reqs);
      flags |= FORCECLOSE;
   }

   /*
    * Vflush will wait until all pending vnode operations are complete.
    */
   ret = vflush(mp, 1, flags, td);
   if (ret != 0) {
      return ret;
   }

   HgfsDestroyFileHashTable(&sip->fileHashTable);

   /*
    * Now we can throw away our superinfo.  Let's reclaim everything allocated
    * during HgfsVfsMount.
    */
   HgfsKReq_FreeContainer(sip->reqs);

   mp->mnt_data = NULL;
   os_free(sip, sizeof *sip);

   DEBUG(VM_DEBUG_LOAD, "Exit\n");

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsStatvs --
 *
 *      "VFS_STATFS(9) - return file system status."
 *
 * Results:
 *      Zero on success and non-zero error code on failure.
 *
 * Side effects:
 *      Caller's statfs structure is populated.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsStatfs(struct mount *mp, struct statfs *sbp, struct thread *td)
{
   int ret = 0;
   struct vnode *vp;

   /* We always want HGFS_BLOCKSIZE to be a power of two */
   ASSERT_ON_COMPILE(HGFS_IS_POWER_OF_TWO(HGFS_BLOCKSIZE));

   /* 
    * This fills in file system ID and the type number that
    * we got from a call to vfs_getnewfsid() in HgfsVfsMount()
    */
   bcopy(&mp->mnt_stat, sbp, sizeof mp->mnt_stat);

   ret = HgfsVfsRoot(mp, LK_SHARED, &vp, td);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsVfsRoot failed\n");
      return ret;
   }

   ret = HgfsStatfsInt(vp, sbp);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsStatfsInt failed with ret = %d\n", ret);
      goto out;
   }

out:
   /* Drop the reference and shared lock that we acquired in HgfsVfsRoot */
   vput(vp);
   return ret;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsRoot --
 *
 *      Retrieves the root Vnode of the specified filesystem.
 *
 * Results:
 *      Zero if successful, non-zero otherwise.
 *
 * Side effects:
 *      Sets *vpp.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsRoot(struct mount *mp,   // IN: Filesystem structure
            int flags,          // IN: Flags to vget
            struct vnode **vpp, // OUT: Address of root vnode
            struct thread *td)  // IN: Thread structure
{
   HgfsSuperInfo *sip = (HgfsSuperInfo *)mp->mnt_data;
   int ret = 0;

   *vpp = NULL;

   ret = vget(sip->rootVnode, flags, td);
   if (ret == 0) {
      *vpp = sip->rootVnode;
   }

   return ret;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsInit --
 *
 *      Initializes the Hgfs filesystem implementation
 *
 * Results:
 *      Zero if successful, an errno-type value otherwise.
 *
 * Side effects:
 *      Initializes the hgfs request processing system.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsInit(struct vfsconf *vfsconf)
{
   int ret = 0;

   /* Initialize the os memory allocation and thread synchronization subsystem. */
   if ((ret = os_init()) != 0) {
      return ret;
   }

   ret = HgfsKReq_SysInit();

   DEBUG(VM_DEBUG_LOAD, "Hgfs filesystem loaded");

   return ret;
}

/*
 *-----------------------------------------------------------------------------
 *
 * HgfsVfsUninit --
 *
 *      Tears down the Hgfs filesystem module state
 *
 * Results:
 *      Zero if successful, an errno-type value otherwise.
 *
 * Side effects:
 *      Can no longer use any hgfs file systems.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsVfsUninit(struct vfsconf *vfsconf)
{
   int ret = 0;

   ret = HgfsKReq_SysFini();
   os_cleanup();

   DEBUG(VM_DEBUG_LOAD, "Hgfs filesystem unloaded");
   return ret;
}
