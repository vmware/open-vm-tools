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
 * filesystem.c --
 *
 * Implementation of the filesystem level routines.  These include functions
 * that intialize, mount, unmount, and provide other various filesystem
 * information.
 */

#include <sys/param.h>          /* MAXNAMELEN */
#include <sys/file.h>           /* FKIOCTL */

#include "hgfsSolaris.h"
#include "hgfsState.h"
#include "filesystem.h"
#include "vnode.h"
#include "request.h"
#include "debug.h"

/*
 * Prototypes
 *
 * These are only needed because Solaris 10 requires that we create the vfsops
 * structure through their provided interface (vfs_setfsops()).
 */
#if HGFS_VFS_VERSION > 2
int HgfsMount(struct vfs *vfsp, struct vnode *vnodep,
              struct mounta *mntp, struct cred *credp);
int HgfsUnmount(struct vfs *vfsp, int mflag, struct cred *credp);
int HgfsRoot(struct vfs *vfsp, struct vnode **vnodepp);
int HgfsStatvfs(struct vfs *vfsp, struct statvfs64 *stats);
int HgfsSync(struct vfs *vfsp, short flags, struct cred *credp);
int HgfsVget(struct vfs *vfsp, struct vnode **vnodepp, struct fid *fidp);
int HgfsMountroot(struct vfs *vfsp, enum whymountroot reason);
int HgfsVnstate(vfs_t *vfsp, vnode_t *vp, vntrans_t trans);
#if HGFS_VFS_VERSION == 3
int HgfsFreevfs(struct vfs *vfsp);
#else
void HgfsFreevfs(struct vfs *vfsp);
#endif
#endif

#if HGFS_VFS_VERSION > 2
static vfsops_t *hgfsVfsOpsP;
#endif

/*
 * Fileystem type number given to us upon initialization.
 */
static int hgfsType;


#if HGFS_VFS_VERSION == 2
/*
 *----------------------------------------------------------------------------
 *
 * HgfsInit --
 *
 *    This is the filesystem initialization routine which is run when the
 *    filesystem is placed in the VFS switch table.
 *
 * Results:
 *    Returns zero on success.
 *
 * Side effects:
 *    The filesystem type (index number) is set to the provided value.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsInit(struct vfssw *vfsswp,  // IN: VFS Switch table struct
         int fstype)            // IN: Index into the vfssw table for this filesystem
{
   if (!vfsswp) {
      cmn_err(HGFS_ERROR, "HgfsInit: received NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsInit().\n");

   /*
    * Hook VFS operations into switch table and save
    * filesystem type number and pointer to vfsswp.
    */
   vfsswp->vsw_vfsops = &HgfsVfsOps;
   hgfsType = fstype;

   mutex_init(&vfsswp->vsw_lock, NULL, MUTEX_DRIVER, NULL);

   DEBUG(VM_DEBUG_LOAD, "fstype: %d\n", hgfsType);
   HgfsDebugPrintVfssw("HgfsInit()", vfsswp);

   DEBUG(VM_DEBUG_DONE, "HgfsInit() done.\n");

   return 0;
}

#else   /* Implies Solaris > 9 */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsInit --
 *
 *    This is the filesystem initialization routine for Solaris 10.  It creates
 *    an array of fs_operation_def_t for all the vfs operations, then calls
 *    vfs_setfsops() to assign them to the filesystem properly.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsInit(int fstype,    // IN: Filesystem type
         char *name)    // IN: Name of the filesystem
{
   int ret;

   /* Construct the VFS operations array to give to vfs_setfsops() */
   static fs_operation_def_t vfsOpsArr[] = {
      HGFS_VOP(VFSNAME_MOUNT, vfs_mount, HgfsMount),
      HGFS_VOP(VFSNAME_UNMOUNT, vfs_unmount, HgfsUnmount),
      HGFS_VOP(VFSNAME_ROOT, vfs_root, HgfsRoot),
      HGFS_VOP(VFSNAME_STATVFS, vfs_statvfs, HgfsStatvfs),
      HGFS_VOP(VFSNAME_VGET, vfs_vget, HgfsVget),
      HGFS_VOP(VFSNAME_MOUNTROOT, vfs_mountroot, HgfsMountroot),
      HGFS_VOP(VFSNAME_FREEVFS, vfs_freevfs, HgfsFreevfs),
      HGFS_VOP(VFSNAME_VNSTATE, vfs_vnstate, HgfsVnstate),
#if HGFS_VFS_VERSION <= 3
      HGFS_VOP(VFSNAME_SYNC,  vfs_vnstate, (fs_generic_func_p)HgfsSync),
      { NULL,               NULL                        }
#else
      HGFS_VOP(VFSNAME_SYNC, vfs_sync, HgfsSync),
      { NULL,               { NULL }},
#endif
   };

   if (!name) {
      cmn_err(HGFS_ERROR, "HgfsInit: received NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsInit: fstype=%d, name=\"%s\"\n", fstype, name);

   /* Assign VFS operations to our filesystem. */
   ret = vfs_setfsops(fstype, vfsOpsArr, &hgfsVfsOpsP);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsInit: vfs_setfsops returned %d\n", ret);
      return ret;
   }

   ret = HgfsMakeVnodeOps();
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsInit: could not register HGFS Vnode Ops.\n");
      vfs_freevfsops_by_type(fstype);
      ret = EIO;
      return ret;
   }

   /* Set our filesystem type. */
   hgfsType = fstype;
   DEBUG(VM_DEBUG_DONE, "HgfsInit: done. (fstype=%d)\n", hgfsType);

   return 0;
}

#endif

/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreeVfsOps --
 *
 *    Free VFS Ops created when we initialized the filesystem.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    Resets hgfsVfsOpsP back to NULL.
 *
 *----------------------------------------------------------------------------
 */


void
HgfsFreeVfsOps(void)
{
#if HGFS_VFS_VERSION > 2
   if (hgfsVfsOpsP) {
      vfs_freevfsops_by_type(hgfsType);
   }
#endif
}


/*
 * VFS Entry Points
 */

/*
 *----------------------------------------------------------------------------
 *
 * HgfsMount --
 *
 *   This function is invoked when mount(2) is called on our filesystem.
 *   The filesystem is mounted on the supplied vnode.
 *
 * Results:
 *   Returns zero on success and an appropriate error code on error.
 *
 * Side effects:
 *   The filesystem is mounted on top of vnodep.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMount(struct vfs *vfsp,     // IN: Filesystem to mount
          struct vnode *vnodep, // IN: Vnode that we are mounting on
          struct mounta *mntp,  // IN: Arguments to mount(2) from user
          struct cred *credp)   // IN: Credentials of caller
{
   HgfsSuperInfo *sip;
   HgfsMountData *mountData;
   int ret;
   dev_t dev;

   if (!vfsp || !vnodep || !mntp || !credp) {
      cmn_err(HGFS_ERROR, "HgfsMount: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsMount().\n");

   //HgfsDebugPrintVfs("HgfsMount", vfsp);
   //HgfsDebugPrintVnode(VM_DEBUG_STRUCT, "HgfsMount", vnodep, FALSE);
   //HgfsDebugPrintMounta("HgfsMount", mntp);
   //HgfsDebugPrintCred("HgfsMount", credp);

   if (!HgfsSuser(credp)) {
      return EPERM;
   }

   if (mntp->datalen != sizeof *mountData) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: bad data size (%lu vs %lu).\n",
            (unsigned long) mntp->datalen,
            (unsigned long) sizeof *mountData);
      return EINVAL;
   }

   mountData = kmem_zalloc(sizeof *mountData, HGFS_ALLOC_FLAG);
   if (!mountData) {
      return ENOMEM;
   }

   if (ddi_copyin(mntp->dataptr, mountData, sizeof *mountData,
                  mntp->flags & MS_SYSSPACE ? FKIOCTL : 0) == -1) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: couldn't copy mount data.\n");
      ret = EFAULT;
      goto out;
   }

   /*
    * Make sure mount data matches what mount program will send us.
    */
   if (mountData->magic != HGFS_MAGIC) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: received invalid magic value: %x\n",
            mountData->magic);
      ret = EINVAL;
      goto out;
   }

   if (mountData->size != sizeof *mountData) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: received invalid size value: %x\n",
            mountData->magic);
      ret = EINVAL;
      goto out;
   }

   /* We support only one instance of hgfs, at least for now */
   if (HgfsGetSuperInfo()) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: HGFS is already mounted somewhere\n");
      ret = EBUSY;
      goto out;
   }

   /*
    * We need to find a unique device number for this VFS that will be used to
    * construct the filesystem id.
    */
   if ((dev = getudev()) == -1) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount(): getudev() failed.\n");
      ret = ENXIO;
      goto out;
   }

   DEBUG(VM_DEBUG_LOAD, "HgfsMount: dev=%lu\n", dev);

   if (vfs_devismounted(dev)) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsMount(): dev is not unique. We should loop on this.\n");
      ret = ENXIO;
      goto out;
   }

   /*
    * Fill in values of the VFS structure for the kernel.
    *
    * There are several important values that must be set.  In particular, we
    * need to create a chain of pointers so the Kernel can easily move between
    * the various filesystems mounted on the system.
    *
    *  o Each filesystem must set its vfs_vnodecovered to the vnode of the
    *    directory it is mounted upon.
    *  o Each directory that is a mount point must set v_vfsmountedhere to
    *    point to the vfs struct of the filesystem mounted there.
    *  o The root vnode of each filesystem must have the VROOT flag set in its
    *    vnode's v_flag so that the Kernel knows to consult the two previously
    *    mentioned pointers.
    */
   vfsp->vfs_vnodecovered = vnodep;
   vfsp->vfs_flag &= ~VFS_UNMOUNTED;
   vfsp->vfs_flag |= HGFS_VFS_FLAGS;
   vfsp->vfs_bsize = HGFS_VFS_BSIZE;
   vfsp->vfs_fstype = hgfsType;
   vfsp->vfs_bcount = 0;
   /* If we had mount options, we'd call vfs_setmntopt with vfsp->vfs_mntopts */

   vfsp->vfs_dev = dev;
   vfs_make_fsid(&vfsp->vfs_fsid, vfsp->vfs_dev, hgfsType);

   /*
    * Fill in value(s) of the vnode structure we are mounted on top of.  We
    * aren't allowed to modify this ourselves in Solaris 10.
    */
#  if HGFS_VFS_VERSION == 2
   vnodep->v_vfsmountedhere = vfsp;
#  endif

   HgfsInitSuperInfo(vfsp);

   sip = HgfsGetSuperInfo();
   vfsp->vfs_data = (caddr_t)sip;

   if (!sip->transportInit()) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: failed to start transport.\n");
      HgfsClearSuperInfo();
      ret = EIO;
      goto out;
   }

   /*
    * Now create the root vnode of the filesystem.
    *
    * Note: do not change the name from "/" here without first checking that
    * HgfsMakeFullName() in vnode.c will still do the right thing.  (See the
    * comment for the ".." special case.)
    */
   ret = HgfsVnodeGet(&sip->rootVnode,                  // vnode to fill in
                      sip,                              // Superinfo
                      vfsp,                             // This filesystem
                      "/",                              // File name
                      HGFS_FILE_TYPE_DIRECTORY,         // File type
                      &sip->fileHashTable);             // File hash table
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMount: couldn't get root vnode.\n");
      sip->transportCleanup();
      HgfsClearSuperInfo();
      ret = EIO;
      goto out;
   }

   /* We must signify that this is the root of our filesystem. */
   mutex_enter(&sip->rootVnode->v_lock);
   sip->rootVnode->v_flag |= VROOT;
   mutex_exit(&sip->rootVnode->v_lock);

   /* XXX do this? */
   VN_HOLD(vnodep);

   DEBUG(VM_DEBUG_DONE, "HgfsMount() done.\n");
   ret = 0;     /* Return success */

out:
   kmem_free(mountData, sizeof *mountData);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsUnmount --
 *
 *    This function is invoked when umount(2) is called on our filesystem.
 *
 * Results:
 *    Returns 0 on success and an error code on error.
 *
 * Side effects:
 *    The root vnode will be freed.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsUnmount(struct vfs *vfsp,   // IN: This filesystem
            int mflag,          // IN: Unmount flags
            struct cred *credp) // IN: Credentials of caller
{
   HgfsSuperInfo *sip;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsUnmount().\n");

   if (!vfsp || !credp) {
      cmn_err(HGFS_ERROR, "HgfsUnmount: NULL input from Kernel.\n");
      return EINVAL;
   }

   /*
    * Initial check to ensure caller is root.
    */
   if (!HgfsSuser(credp)) {
      return EPERM;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EINVAL;
   }

   if (vfsp != sip->vfsp) {
      DEBUG(VM_DEBUG_ALWAYS, "HgfsUnmount: vfsp != sip->vfsp.\n");
   }

   HgfsDebugPrintVnode(VM_DEBUG_STRUCT, "HgfsUnmount",
                       vfsp->vfs_vnodecovered, FALSE);

   /* Take the request lock to prevent submitting new requests */
   mutex_enter(&sip->reqMutex);

   /*
    * Make sure there are no active files (besides the root vnode which we
    * release at the end of the function).
    */
   HgfsDebugPrintFileHashTable(&sip->fileHashTable, VM_DEBUG_STATE);

   if (!HgfsFileHashTableIsEmpty(sip, &sip->fileHashTable) &&
       !(mflag & MS_FORCE)) {
      DEBUG(VM_DEBUG_FAIL, "HgfsUnmount: there are still active files.\n");
      ret = EBUSY;
      goto out;
   }

   HgfsCancelAllRequests(sip);

   /*
    * Set unmounted flag in vfs structure.
    */
   vfsp->vfs_flag |= VFS_UNMOUNTED;

   /*
    * Close transport channel, we should not be gettign any more requests.
    */
   sip->transportCleanup();

   /*
    * Clean up fields in vnode structure of mount point and release hold on
    * vnodes for mount.
    */
#  if HGFS_VFS_VERSION == 2
   vfsp->vfs_vnodecovered->v_vfsmountedhere = NULL;
#  endif
   VN_RELE(vfsp->vfs_vnodecovered);
   VN_RELE(HGFS_ROOT_VNODE(sip));

   /*
    * Signify to the device half that the filesystem has been unmounted.
    */
   HGFS_ROOT_VNODE(sip) = NULL;
   HgfsClearSuperInfo();

   ret = 0;

out:
   mutex_exit(&sip->reqMutex);

   DEBUG(VM_DEBUG_DONE, "HgfsUnmount() done.\n");
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRoot --
 *
 *    This supplies the root vnode for the filesystem.
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

int
HgfsRoot(struct vfs *vfsp,              // IN: Filesystem to find root vnode of
         struct vnode **vnodepp)        // OUT: Set to pointer to root vnode of this fs
{
   HgfsSuperInfo *sip;

   if (!vfsp || !vnodepp) {
      cmn_err(HGFS_ERROR, "HgfsRoot: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsRoot().\n");

   /*
    * Get the root vnode from the superinfo structure.
    */
   sip = HgfsGetSuperInfo();
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRoot() failed to find superinfo.\n");
      return EIO;
   }

   if (vfsp != sip->vfsp) {
      DEBUG(VM_DEBUG_ALWAYS, "HgfsRoot: vfsp != sip->vfsp.\n");
   }

   VN_HOLD( HGFS_ROOT_VNODE(sip) );
   *vnodepp = HGFS_ROOT_VNODE(sip);

   DEBUG(VM_DEBUG_LOAD, " rootvnode=%p", HGFS_ROOT_VNODE(sip));

   DEBUG(VM_DEBUG_DONE, "HgfsRoot() done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsStatvfs --
 *
 *    Provides statistics for the provided filesystem.  The values provided
 *    by this function are fake.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on exit.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsStatvfs(struct vfs *vfsp,           // IN: Filesystem to get statistics for
            struct statvfs64 *stats)    // OUT: Statistics are placed into this struct
{
   dev32_t dev32;

   DEBUG(VM_DEBUG_ENTRY, "HgfsStatvfs().\n");

   if (!stats) {
      cmn_err(HGFS_ERROR, "HgfsStatvfs: NULL input from Kernel.\n");
      return EINVAL;
   }

   /* Clear stats struct, then fill it in with our values. */
   memset(stats, 0, sizeof *stats);

   /*
    * Macros in case we need these elsewhere later.
    *
    * These were selected pretty randomly: the numbers should be large enough
    * so a user can attempt to create any reasonably sized file, but small
    * enough to be so the Kernel doesn't give callers who are using statvfs32
    * an EOVERFLOW.
    */
   #define HGFS_BLOCKS_TOTAL    0x00ffffff
   #define HGFS_BLOCKS_FREE     0x00ffefff
   #define HGFS_BLOCKS_AVAIL    0x00ffef00
   #define HGFS_FILES_TOTAL     0x00ffffff
   #define HGFS_FILES_FREE      0x00ffefff
   #define HGFS_FILES_AVAIL     0x00ffef00

   /* Compress the device number to 32-bits for consistency on 64-bit systems. */
   cmpldev(&dev32, vfsp->vfs_dev);

   stats->f_bsize   = HGFS_BLOCKSIZE;           /* Preferred fs block size */
   stats->f_frsize  = HGFS_BLOCKSIZE;           /* Fundamental fs block size */
   /* Next six are u_longlong_t */
   stats->f_blocks  = HGFS_BLOCKS_TOTAL;        /* Total blocks on fs */
   stats->f_bfree   = HGFS_BLOCKS_FREE;         /* Total free blocks */
   stats->f_bavail  = HGFS_BLOCKS_AVAIL;        /* Total blocks avail to non-root */
   stats->f_files   = HGFS_FILES_TOTAL;         /* Total files (inodes) */
   stats->f_ffree   = HGFS_FILES_FREE;          /* Total files free */
   stats->f_favail  = HGFS_FILES_AVAIL;         /* Total files avail to non-root */
   stats->f_fsid    = dev32;                    /* Filesystem id */
   stats->f_flag   &= ST_NOSUID;                /* Flags: we don't support setuid. */
   stats->f_namemax = MAXNAMELEN;               /* Max filename; use Solaris default. */

   /* Memset above and -1 of array size as n below ensure NUL termination. */
   strncpy(stats->f_basetype, HGFS_FS_NAME, sizeof stats->f_basetype - 1);
   strncpy(stats->f_fstr, HGFS_FS_NAME, sizeof stats->f_fstr - 1);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSync --
 *
 *    Flushes the filesystem cache.
 *
 * Results:
 *    Returns zero on success and an error code on failure.  Currently this
 *    always succeeds.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSync(struct vfs *vfsp,      // IN: Filesystem to flush
         short flags,           // XXX: ?
         struct cred *credp)    // IN: Credentials of caller
{
   //DEBUG(VM_DEBUG_ENTRY, "HgfsSync().\n");

   /*
    * We just return success and hope the host OS calls its filesystem sync
    * operation periodically as well.
    */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVget --
 *
 *    Finds the vnode that matches the unique file identifier.
 *
 *    XXX: Come back to this when figure out how/if to store fidp to vnode
 *    mappings.
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVget(struct vfs *vfsp,              // IN: Filesystem to operate on
         struct vnode **vnodepp,        // OUT: Set to pointer of found vnode
         struct fid *fidp)              // IN: Unique file identifier for vnode
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsVget() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMountroot --
 *
 *    Mounts the file system on the root directory.
 *
 *    XXX: Still need to figure out when this is invoked.
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMountroot(struct vfs *vfsp,         // IN: Filesystem to mount
              enum whymountroot reason) // IN: Reason why mounting on root
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsMountroot() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReserved --
 *
 *    XXX: Is this a placeholder function in the struct?
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

int
HgfsReserved(struct vfs *vfsp,
             struct vnode **vnodepp,
             char *charp)
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsReserved() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreevfs --
 *
 *    Called when a filesystem is unmounted to free the resources held by the
 *    filesystem.
 *
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 3
int HgfsFreevfs(vfs_t *vfsp)
#else
void HgfsFreevfs(vfs_t *vfsp)
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsFreevfs().\n");

   /*
    * The only allocation to undo here is call mutex_destroy() on the vsw_lock
    * for our filesystem's struct vfssw.  Doing this causes a system crash,
    * from a call to a mutex free function within the Kernel (that is, not from
    * our code), so we are assured that the Kernel cleans this up for us.
    *
    * In Solaris 10 it seemed that we needed to free the vnode and vfs
    * operations we had made earlier (vn_freevnodeops() and
    * vfs_freevfsops_by_type()), but this is not so.  Freeing these prevents
    * 1) multiple mounts without first reloading the module, and 2) unloading
    * the module from the Kernel.  The combination of these two meant that
    * the guest would have to be rebooted to remount the filesystem.  Because
    * of all this, the assumption is made that the Kernel takes care of
    * removing these structures for us.
    */

#if HGFS_VFS_VERSION == 3
   return 0;
#endif
}


#if HGFS_VFS_VERSION > 2
/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnstate --
 *
 *    Performs the necessary operations on the provided vnode given the state
 *    transfer that has occurred.
 *
 *    The possible transfers are VNTRANS_EXISTS, VNTRANS_IDLED,
 *    VNTRANS_RECLAIMED, and VNTRANS_DESTROYED (see <sys/vfs.h>).
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsVnstate(vfs_t *vfsp,        // IN: Pointer to our filesystem
            vnode_t *vp,        // IN: Vnode to change state of
            vntrans_t trans)    // IN: Type of state transfer
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsVnstate: entry.\n");

   return ENOTSUP;
}
#endif


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSuser --
 *
 *    Correctly implements the superuser check depending on the version of
 *    Solaris.
 *
 * Results:
 *    Returns zero if this user is not superuser, returns non-zero otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsSuser(struct cred *cr)      // IN: Credentials of the caller
{
   ASSERT(cr);
#if HGFS_VFS_VERSION == 2
   return suser(cr);
#else
   /*
    * I am assuming the crgetuid() is the effective uid, since the other two
    * related functions are crgetruid() and crgetsuid().
    */
   return (crgetuid(cr) == 0);
#endif
}

