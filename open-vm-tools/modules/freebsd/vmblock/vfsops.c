/* **********************************************************
 * Copyright 2007-2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vfsops.c --
 *
 *      VFS operations for VMBlock file system on FreeBSD.
 */

/*-
 * Copyright (c) 1992, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software donated to Berkeley by
 * Jan-Simon Pendry.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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
 *	@(#)null_vfsops.c	8.2 (Berkeley) 1/21/94
 *
 * @(#)lofs_vfsops.c	1.2 (Berkeley) 6/18/92
 * $FreeBSD: src/sys/fs/nullfs/null_vfsops.c,v 1.72.2.5 2006/10/09 19:47:14 tegge Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/fcntl.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/vnode.h>

#include "vmblock_k.h"
#include "compat_freebsd.h"

static MALLOC_DEFINE(M_VMBLOCKFSMNT, "VMBlockFS mount", "VMBlockFS mount structure");


/*
 * Local data
 */

static vfs_mount_t	VMBlockVFSMount;
static vfs_root_t	VMBlockVFSRoot;
static vfs_sync_t	VMBlockVFSSync;
static vfs_statfs_t	VMBlockVFSStatFS;
static vfs_unmount_t	VMBlockVFSUnmount;
static vfs_vget_t	VMBlockVFSVGet;

/*
 * VFS operations vector
 */
static struct vfsops VMBlockVFSOps = {
   .vfs_init            = VMBlockInit,
   .vfs_uninit          = VMBlockUninit,
   .vfs_mount           = VMBlockVFSMount,
   .vfs_root            = VMBlockVFSRoot,
   .vfs_statfs          = VMBlockVFSStatFS,
   .vfs_sync            = VMBlockVFSSync,
   .vfs_unmount         = VMBlockVFSUnmount,
   .vfs_vget            = VMBlockVFSVGet,
};

/*
 * The following generates a struct vfsconf for our filesystem & grafts us into
 * the kernel's list of known filesystems at module load.
 */
VFS_SET(VMBlockVFSOps, vmblock, VFCF_LOOPBACK);


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSMount --
 *
 *      Mount the vmblock file system.
 *
 * Results:
 *      Zero on success, otherwise an appropriate system error.  (See
 *      VFS_MOUNT(9).)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
#if __FreeBSD_version >= 800011
VMBlockVFSMount(struct mount *mp)        // IN: mount(2) parameters
#else
VMBlockVFSMount(struct mount *mp,        // IN: mount(2) parameters
                struct thread *td)       // IN: caller's thread context
#endif
{
   struct VMBlockMount *xmp;
   struct nameidata nd, *ndp = &nd;
   struct vnode *lowerrootvp, *vp;
   char *target;
   char *pathname;
   int len, error = 0;

   VMBLOCKDEBUG("VMBlockVFSMount(mp = %p)\n", (void *)mp);

   /*
    * TODO:  Strip out extraneous export & other misc cruft.
    */

   /*
    * Disallow the following:
    *   1.  Mounting over the system root.
    *   2.  Mount updates/remounts.  (Reconsider for rw->ro, ro->rw?)
    *   3.  Mounting VMBlock on top of a VMBlock.
    */
   if ((mp->mnt_flag & MNT_ROOTFS) ||
       (mp->mnt_flag & MNT_UPDATE) ||
       (mp->mnt_vnodecovered->v_op == &VMBlockVnodeOps)) {
      return EOPNOTSUPP;
   }

   /*
    * XXX Should only be unlocked if mnt_flag & MNT_UPDATE.
    */
   ASSERT_VOP_UNLOCKED(mp->mnt_vnodecovered, "Covered vnode already locked!");

   /*
    * Look up path to lower layer (VMBlock source / DnD staging area).
    * (E.g., in the command "mount /tmp/VMwareDnD /var/run/vmblock",
    * /tmp/VMwareDnD is the staging area.)
    */
   error = vfs_getopt(mp->mnt_optnew, "target", (void **)&target, &len);
   if (error || target[len - 1] != '\0') {
      return EINVAL;
   }

   pathname = uma_zalloc(VMBlockPathnameZone, M_WAITOK);
   if (pathname == NULL) {
      return ENOMEM;
   }

   if (strlcpy(pathname, target, MAXPATHLEN) >= MAXPATHLEN) {
      uma_zfree(VMBlockPathnameZone, pathname);
      return ENAMETOOLONG;
   }

   /*
    * Find lower node and lock if not already locked.
    */

   NDINIT(ndp, LOOKUP, FOLLOW|LOCKLEAF, UIO_SYSSPACE, target, compat_td);
   error = namei(ndp);
   if (error) {
      NDFREE(ndp, 0);
      uma_zfree(VMBlockPathnameZone, pathname);
      return error;
   }
   NDFREE(ndp, NDF_ONLY_PNBUF);

   /*
    * Check multi VMBlock mount to avoid `lock against myself' panic.
    */
   lowerrootvp = ndp->ni_vp;
   if (lowerrootvp == VPTOVMB(mp->mnt_vnodecovered)->lowerVnode) {
      VMBLOCKDEBUG("VMBlockVFSMount: multi vmblock mount?\n");
      vput(lowerrootvp);
      uma_zfree(VMBlockPathnameZone, pathname);
      return EDEADLK;
   }

   xmp = malloc(sizeof *xmp, M_VMBLOCKFSMNT, M_WAITOK);

   /*
    * Record pointer (mountVFS) to the staging area's file system.  Follow up
    * by grabbing a VMBlockNode for our layer's root.
    */
   xmp->mountVFS = lowerrootvp->v_mount;
   error = VMBlockNodeGet(mp, lowerrootvp, &vp, pathname);

   /*
    * Make sure the node alias worked
    */
   if (error) {
      COMPAT_VOP_UNLOCK(vp, 0, compat_td);
      vrele(lowerrootvp);
      free(xmp, M_VMBLOCKFSMNT);   /* XXX */
      uma_zfree(VMBlockPathnameZone, pathname);
      return error;
   }

   /*
    * Record a reference to the new filesystem's root vnode & mark it as such.
    */
   xmp->rootVnode = vp;
   xmp->rootVnode->v_vflag |= VV_ROOT;

   /*
    * Unlock the node (either the lower or the alias)
    */
   COMPAT_VOP_UNLOCK(vp, 0, compat_td);

   /*
    * If the staging area is a local filesystem, reflect that here, too.  (We
    * could potentially allow NFS staging areas.)
    */
   MNT_ILOCK(mp);
   mp->mnt_flag |= lowerrootvp->v_mount->mnt_flag & MNT_LOCAL;
#if __FreeBSD_version >= 600000 && __FreeBSD_version < 1000000
   mp->mnt_kern_flag |= lowerrootvp->v_mount->mnt_kern_flag & MNTK_MPSAFE;
#endif
   MNT_IUNLOCK(mp);

   mp->mnt_data = (qaddr_t) xmp;

   vfs_getnewfsid(mp);
   vfs_mountedfrom(mp, target);

   VMBLOCKDEBUG("VMBlockVFSMount: lower %s, alias at %s\n",
      mp->mnt_stat.f_mntfromname, mp->mnt_stat.f_mntonname);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSUnmount --
 *
 *      "VFS_UNMOUNT(9) -- unmount a filesystem."
 *
 * Results:
 *      Zero on success, else an appropriate system error.
 *
 * Side effects:
 *      VMBlocks on all filesystems are removed.  (Expecting vmblockfs to
 *      be mounted only once.)
 *
 *-----------------------------------------------------------------------------
 */

static int
#if __FreeBSD_version >= 800011
VMBlockVFSUnmount(struct mount *mp,    // IN: filesystem to unmount
                  int mntflags)        // IN: unmount(2) flags (ex: MNT_FORCE)
#else
VMBlockVFSUnmount(struct mount *mp,    // IN: filesystem to unmount
                  int mntflags,        // IN: unmount(2) flags (ex: MNT_FORCE)
                  struct thread *td)   // IN: caller's kernel thread context
#endif
{
   struct VMBlockMount *xmp;
   struct vnode *vp;
   void *mntdata;
   int error;
   int flags = 0, removed = 0;

   VMBLOCKDEBUG("VMBlockVFSUnmount: mp = %p\n", (void *)mp);

   xmp = MNTTOVMBLOCKMNT(mp);
   vp = xmp->rootVnode;

   VI_LOCK(vp);

   /*
    * VMBlocks reference the root vnode.  This check returns EBUSY if
    * VMBlocks still exist & the user isn't forcing us out.
    */
   if ((vp->v_usecount > 1) && !(mntflags & MNT_FORCE)) {
      VI_UNLOCK(vp);
      return EBUSY;
   }

   /*
    * FreeBSD forbids acquiring sleepable locks (ex: sx locks) while holding
    * non-sleepable locks (ex: mutexes).  The vnode interlock acquired above
    * is a mutex, and the Block* routines involve sx locks, so we need to
    * yield the interlock.
    *
    * In order to do this safely, we trade up to locking the entire vnode,
    * and indicate to the lock routine that we hold the interlock.  The lock
    * transfer will happen atomically.  (Er, at least within the scope of
    * the vnode subsystem.)
    */
   COMPAT_VOP_LOCK(vp, LK_EXCLUSIVE|LK_RETRY|LK_INTERLOCK, compat_td);

   removed = BlockRemoveAllBlocks(OS_UNKNOWN_BLOCKER);

   VI_LOCK(vp);
   vp->v_usecount -= removed;
   VI_UNLOCK(vp);
   COMPAT_VOP_UNLOCK(vp, 0, compat_td);

   if (mntflags & MNT_FORCE) {
      flags |= FORCECLOSE;
   }

   /* There is 1 extra root vnode reference (xmp->rootVnode). */
   error = vflush(mp, 1, flags, compat_td);
   if (error) {
      return error;
   }

   /*
    * Finally, throw away the VMBlockMount structure
    */
   mntdata = mp->mnt_data;
   mp->mnt_data = 0;
   free(mntdata, M_VMBLOCKFSMNT);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSRoot --
 *
 *      "VFS_ROOT -- return the root vnode of a file system."
 *
 * Results:
 *      Zero.
 *
 * Side effects:
 *      Root vnode is locked.
 *
 *-----------------------------------------------------------------------------
 */

static int
#if __FreeBSD_version >= 800011
VMBlockVFSRoot(struct mount *mp,        // IN: vmblock file system
               int flags,               // IN: lockmgr(9) flags
               struct vnode **vpp)      // OUT: root vnode
#else
VMBlockVFSRoot(struct mount *mp,        // IN: vmblock file system
               int flags,               // IN: lockmgr(9) flags
               struct vnode **vpp,      // OUT: root vnode
               struct thread *td)       // IN: caller's thread context
#endif
{
   struct vnode *vp;

   /*
    * Return locked reference to root.
    */
   vp = MNTTOVMBLOCKMNT(mp)->rootVnode;
   VREF(vp);
   compat_vn_lock(vp, flags | LK_RETRY, compat_td);
   *vpp = vp;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSStatFS --
 *
 *      "VFS_STATFS -- return file system status."  We pass the request to the
 *      lower layer, but return only the "interesting" bits.  (E.g., fs type,
 *      sizes, usage, etc.)
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
#if __FreeBSD_version >= 800011
VMBlockVFSStatFS(struct mount *mp,      // IN: vmblock file system
                 struct statfs *sbp)    // OUT: statfs(2) arg container
#else
VMBlockVFSStatFS(struct mount *mp,      // IN: vmblock file system
                 struct statfs *sbp,    // OUT: statfs(2) arg container
                 struct thread *td)     // IN: caller's thread context
#endif
{
   int error;
   struct statfs mstat;

   VMBLOCKDEBUG("VMBlockVFSStatFS(mp = %p, vp = %p->%p)\n", (void *)mp,
       (void *)MNTTOVMBLOCKMNT(mp)->rootVnode,
       (void *)VMBVPTOLOWERVP(MNTTOVMBLOCKMNT(mp)->rootVnode));

   bzero(&mstat, sizeof mstat);

   error = COMPAT_VFS_STATFS(MNTTOVMBLOCKMNT(mp)->mountVFS, &mstat, compat_td);
   if (error) {
      return error;
   }

   /* now copy across the "interesting" information and fake the rest */
   sbp->f_type = mstat.f_type;
   sbp->f_flags = mstat.f_flags;
   sbp->f_bsize = mstat.f_bsize;
   sbp->f_iosize = mstat.f_iosize;
   sbp->f_blocks = mstat.f_blocks;
   sbp->f_bfree = mstat.f_bfree;
   sbp->f_bavail = mstat.f_bavail;
   sbp->f_files = mstat.f_files;
   sbp->f_ffree = mstat.f_ffree;
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSSync --
 *
 *      "VFS_SYNC -- flush unwritten data."  Since there's no caching at our
 *      layer, this is a no-op.
 *
 * Results:
 *      Zero.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
#if __FreeBSD_version >= 800011
VMBlockVFSSync(struct mount *mp,        // Ignored
               int waitfor)             // Ignored
#else
VMBlockVFSSync(struct mount *mp,        // Ignored
               int waitfor,             // Ignored
               struct thread *td)       // Ignored
#endif
{
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVFSVGet --
 *
 *      "VFS_VGET -- convert an inode number to a vnode."
 *
 * Results:
 *      Zero on success, otherwise an appropriate system error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVFSVGet(struct mount *mp,        // IN: vmblock file system
               ino_t ino,               // IN: requested inode number
               int flags,               // IN: vget(9) locking flags
               struct vnode **vpp)      // OUT: located vnode
{
   int error;
   error = VFS_VGET(MNTTOVMBLOCKMNT(mp)->mountVFS, ino, flags, vpp);
   if (error) {
      return error;
   }

   return VMBlockNodeGet(mp, *vpp, vpp, NULL);
}
