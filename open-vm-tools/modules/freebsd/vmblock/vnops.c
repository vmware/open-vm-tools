/* **********************************************************
 * Copyright 2007-2014, 2020, 2023 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * vnops.c --
 *
 *      Vnode operations for the vmblock filesystem on FreeBSD.
 */

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * John Heidemann of the UCLA Ficus project.
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
 *	@(#)null_vnops.c	8.6 (Berkeley) 5/27/95
 *
 * Ancestors:
 *	@(#)lofs_vnops.c	1.2 (Berkeley) 6/18/92
 *	...and...
 *	@(#)nullfs_vnops.c 1.20 92/07/07 UCLA Ficus project
 *
 * $FreeBSD: src/sys/fs/nullfs/null_vnops.c,v 1.87.2.3 2006/03/13 03:05:26 jeff Exp $
 */

/*
 * NB:  The following is the introductory commentary from FreeBSD's
 * null_vnops.c, which VMBlockFS is heavily based on.  It was kept intact in
 * order to provide a better conceptual introduction to FreeBSD's approach
 * to stackable file systems.
 */

/*
 * Null Layer
 *
 * (See mount_nullfs(8) for more information.)
 *
 * The null layer duplicates a portion of the filesystem name space under
 * a new name.  In this respect, it is similar to the loopback filesystem.
 * It differs from the loopback fs in two respects:  it is implemented
 * using a stackable layers techniques, and its "null-node"s stack above
 * all lower-layer vnodes, not just over directory vnodes.
 *
 * The null layer has two purposes.  First, it serves as a demonstration
 * of layering by proving a layer which does nothing.  (It actually
 * does everything the loopback filesystem does, which is slightly more
 * than nothing.)  Second, the null layer can serve as a prototype layer.
 * Since it provides all necessary layer framework, new filesystem layers
 * can be created very easily be starting with a null layer.
 *
 * The remainder of this man page examines the null layer as a basis
 * for constructing new layers.
 *
 *
 * INSTANTIATING NEW NULL LAYERS
 *
 * New null layers are created with mount_nullfs(8).  Mount_nullfs(8)
 * takes two arguments, the pathname of the lower vfs (target-pn)
 * and the pathname where the null layer will appear in the namespace
 * (alias-pn).  After the null layer is put into place, the contents of
 * target-pn subtree will be aliased under alias-pn.
 *
 *
 * OPERATION OF A NULL LAYER
 *
 * The null layer is the minimum filesystem layer, simply bypassing
 * all possible operations to the lower layer for processing there.
 * The majority of its activity centers on the bypass routine, through
 * which nearly all vnode operations pass.
 *
 * The bypass routine accepts arbitrary vnode operations for handling by
 * the lower layer.  It begins by examing vnode operation arguments and
 * replacing any null-nodes by their lower-layer equivlants.  It then
 * invokes the operation on the lower layer.  Finally, it replaces the
 * null-nodes in the arguments and, if a vnode is return by the operation,
 * stacks a null-node on top of the returned vnode.
 *
 * Although bypass handles most operations, vop_getattr, vop_lock,
 * vop_unlock, vop_inactive, vop_reclaim, and vop_print are
 * not bypassed. Vop_getattr must change the fsid being returned.
 * Vop_lock and vop_unlock must handle any locking for the current
 * vnode as well as pass the lock request down.  Vop_inactive and
 * vop_reclaim are not bypassed so that they can handle freeing null-layer
 * specific data. Vop_print is not bypassed to avoid excessive debugging
 * information.  Also, certain vnode operations change the locking state
 * within the operation (create, mknod, remove, link, rename, mkdir,
 * rmdir, and symlink). Ideally these operations should not change the
 * lock state, but should be changed to let the caller of the function
 * unlock them. Otherwise all intermediate vnode layers (such as union,
 * umapfs, etc) must catch these functions to do the necessary locking
 * at their layer.
 *
 *
 * INSTANTIATING VNODE STACKS
 *
 * Mounting associates the null layer with a lower layer, effect
 * stacking two VFSes.  Vnode stacks are instead created on demand as
 * files are accessed.
 *
 * The initial mount creates a single vnode stack for the root of the
 * new null layer.  All other vnode stacks are created as a result of
 * vnode operations on this or other null vnode stacks.
 *
 * New vnode stacks come into existance as a result of an operation
 * which returns a vnode.  The bypass routine stacks a null-node above
 * the new vnode before returning it to the caller.
 *
 * For example, imagine mounting a null layer with "mount_nullfs
 * /usr/include /dev/layer/null".  Changing directory to /dev/layer/null
 * will assign the root null-node (which was created when the null layer
 * was mounted).  Now consider opening "sys".  A vop_lookup would be
 * done on the root null-node.  This operation would bypass through
 * to the lower layer which would return a vnode representing the UFS
 * "sys".  Null_bypass then builds a null-node aliasing the UFS "sys"
 * and returns this to the caller.  Later operations on the null-node
 * "sys" will repeat this process when constructing other vnode stacks.
 *
 *
 * CREATING OTHER FILE SYSTEM LAYERS
 *
 * One of the easiest ways to construct new filesystem layers is to make a
 * copy of the null layer, rename all files and variables, and then begin
 * modifing the copy.  Sed can be used to easily rename all variables.
 *
 * The umap layer is an example of a layer descended from the null layer.
 *
 *
 * INVOKING OPERATIONS ON LOWER LAYERS
 *
 * There are two techniques to invoke operations on a lower layer when the
 * operation cannot be completely bypassed.  Each method is appropriate
 * in different situations.  In both cases, it is the responsibility of
 * the aliasing layer to make the operation arguments "correct" for the
 * lower layer by mapping a vnode arguments to the lower layer.
 *
 * The first approach is to call the aliasing layer's bypass routine.
 * This method is most suitable when you wish to invoke the operation
 * currently being handled on the lower layer.  It has the advantage that
 * the bypass routine already must do argument mapping.  An example of
 * this is vop_getattr in the null layer.
 *
 * A second approach is to directly invoke vnode operations on the
 * lower layer with the VOP_OPERATIONNAME interface.  The advantage
 * of this method is that it is easy to invoke arbitrary operations on
 * the lower layer.  The disadvantage is that vnode arguments must be
 * manualy mapped.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/filedesc.h>
#include <sys/kdb.h>
#include "compat_freebsd.h"
#include <sys/priv.h>

#include "vmblock_k.h"
#include "vmblock.h"
#include "block.h"

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vnode_pager.h>


/*
 * Local functions
 */

static fo_ioctl_t               VMBlockFileIoctl;
static fo_close_t               VMBlockFileClose;
static vop_access_t             VMBlockVopAccess;
static vop_getattr_t            VMBlockVopGetAttr;
static vop_getwritemount_t      VMBlockVopGetWriteMount;
static vop_inactive_t           VMBlockVopInactive;
static vop_ioctl_t              VMBlockVopIoctl;
static vop_islocked_t           VMBlockVopIsLocked;
static compat_vop_lock_t        VMBlockVopLock;
static vop_lookup_t             VMBlockVopLookup;
static vop_open_t               VMBlockVopOpen;
static vop_print_t              VMBlockVopPrint;
static vop_reclaim_t            VMBlockVopReclaim;
static vop_rename_t             VMBlockVopRename;
static vop_setattr_t            VMBlockVopSetAttr;
static vop_unlock_t             VMBlockVopUnlock;


/*
 * Local data
 */

/*
 * The following is an ioctl(2) argument wrapper for VMBlockVopIoctl.
 * See the VMBlockFileOps blurb below for details.
 */
struct VMBlockIoctlArgs {
   struct file  *fileDesc;      // file descriptor receiving ioctl request
   void         *data;          // user's ioctl argument
};

typedef struct VMBlockIoctlArgs VMBlockIoctlArgs;

/*
 * VMBlockFS vnode operations vector --
 *   Following are the file system's entry points via VFS nodes (vnodes).
 *   See vnode(9) and sys/vnode.h for more information.  For details on
 *   the locking protocol[1], have a look at kern/vnode_if.src.
 *
 * 1.  Describes for which operations a vnode should be locked before 
 *     the operation is called or after it returns.
 */

struct vop_vector VMBlockVnodeOps = {
   .vop_bypass =                  VMBlockVopBypass,
   .vop_access =                  VMBlockVopAccess,
   .vop_advlockpurge =            vop_stdadvlockpurge,
   .vop_bmap =                    VOP_EOPNOTSUPP,
   .vop_getattr =                 VMBlockVopGetAttr,
   .vop_getwritemount =           VMBlockVopGetWriteMount,
   .vop_inactive =                VMBlockVopInactive,
   .vop_ioctl =                   VMBlockVopIoctl,
   .vop_islocked =                VMBlockVopIsLocked,
   .COMPAT_VOP_LOCK_OP_ELEMENT =  VMBlockVopLock,
   .vop_lookup =                  VMBlockVopLookup,
   .vop_open =                    VMBlockVopOpen,
   .vop_print =                   VMBlockVopPrint,
   .vop_reclaim =                 VMBlockVopReclaim,
   .vop_rename =                  VMBlockVopRename,
   .vop_setattr =                 VMBlockVopSetAttr,
   .vop_strategy =                VOP_EOPNOTSUPP,
   .vop_unlock =                  VMBlockVopUnlock,
};



/*
 * VMBlockFS file descriptor operations vector --
 *   There are a few special cases where we need to control behavior beyond
 *   the file system layer.  For this we define our own fdesc op vector,
 *   install our own handlers for these special cases, and fall back to the
 *   badfileops vnode ops for everything else.
 *
 *   VMBlock instances are keyed on/indexed by the file descriptor that received
 *   the ioctl request[1].  Since the relationship between file descriptors and
 *   vnodes is N:1, we need to intercept ioctl requests at the file descriptor
 *   level, rather than at the vnode level, in order to have a record of which
 *   descriptor received the request.  Similarly, we need to remove VMBlocks
 *   issued on a file descriptor when said descriptor is closed.
 *
 *   NOTICE --
 *     This applies -only- when a user opens the FS mount point directly.  All
 *     other files'/directories' file descriptor operations vectors are left
 *     untouched.
 *
 * 1.  Keying on thread ID/process ID doesn't work because file descriptors
 *     may be shared between threads/processes.  Clients may find blocks
 *     removed unintentionally when the original issuing thread or process
 *     dies, even though the same descriptor is open.
 */

static struct fileops VMBlockFileOps;

/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockSetupFileOps --
 *
 *      Sets up secial file operations vector used for root vnode _only_
 *      (see the comment for VMBlockFileOps above).
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
VMBlockSetupFileOps(void)
{
   VMBlockFileOps = badfileops;
   VMBlockFileOps.fo_stat = vnops.fo_stat;
   VMBlockFileOps.fo_flags = vnops.fo_flags;
   VMBlockFileOps.fo_ioctl = VMBlockFileIoctl;
   VMBlockFileOps.fo_close = VMBlockFileClose;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockFileIoctl --
 *
 *      Wrapper for VMBlockVopIoctl.  This is done to provide VMBlockVopIoctl
 *      with information about the file descriptor which received the user's
 *      ioctl request.
 *
 * Results:
 *      Zero on success, otherwise an appropriate system error.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockFileIoctl(struct file *fp,       // IN: user's file descriptor
                 u_long command,        // IN: encoded ioctl command
                 void *data,            // IN: opaque data argument
                 struct ucred *cred,    // IN: caller's credentials
                 struct thread *td)     // IN: caller's thread context
{
   VMBlockIoctlArgs args;
   args.fileDesc = fp;
   args.data = data;
   return vnops.fo_ioctl(fp, command, &args, cred, td);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockFileClose --
 *
 *      Called when a file descriptor is closed.  Destroy all blocks opened
 *      on this descriptor, then pass off to vn_closefile to handle any other
 *      cleanup.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *      (See vn_closefile for more information.)
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockFileClose(struct file *fp,       // IN: user's file descriptor
                 struct thread *td)     // IN: caller's thread context
{
   struct vnode *vp;
   int removed = 0;

   vp = MNTTOVMBLOCKMNT(fp->f_vnode->v_mount)->rootVnode;
   removed = BlockRemoveAllBlocks(fp);

   VI_LOCK(vp);
   vp->v_usecount -= removed;
   VI_UNLOCK(vp);

   return vnops.fo_close(fp, td);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopBypass --
 *
 *      Default routine for bypassing the VMBlockFS file system layer.
 *
 * Results:
 *      Zero on success, or an appropriate system error otherwise.
 *
 * Side effects:
 *      Parameters passed in via ap->a_desc may be modified by the lower
 *      layer's routines.
 *
 * Original function comment:
 *	This is the 10-Apr-92 bypass routine.
 *	   This version has been optimized for speed, throwing away some
 *	safety checks.  It should still always work, but it's not as
 *	robust to programmer errors.
 *
 *	In general, we map all vnodes going down and unmap them on the way back.
 *	As an exception to this, vnodes can be marked "unmapped" by setting
 *	the Nth bit in operation's vdesc_flags.
 *
 *	Also, some BSD vnode operations have the side effect of vrele'ing
 *	their arguments.  With stacking, the reference counts are held
 *	by the upper node, not the lower one, so we must handle these
 *	side-effects here.  This is not of concern in Sun-derived systems
 *	since there are no such side-effects.
 *
 *	This makes the following assumptions:
 *	- only one returned vpp
 *	- no INOUT vpp's (Sun's vop_open has one of these)
 *	- the vnode operation vector of the first vnode should be used
 *	  to determine what implementation of the op should be invoked
 *	- all mapped vnodes are of our vnode-type (NEEDSWORK:
 *	  problems on rmdir'ing mount points and renaming?)
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockVopBypass(struct vop_generic_args *ap)
/*
struct vop_generic_args {
   struct vnodeop_desc *a_desc; // IN/OUT: Vnode operation description; incl.
                                //         pointers to operand vnode(s), user
                                //         credentials, calling thread context,
                                //         etc.
};
*/
{
   struct vnode **this_vp_p;
   int error;
   struct vnode *old_vps[VDESC_MAX_VPS];
   struct vnode **vps_p[VDESC_MAX_VPS];
   struct vnode ***vppp;
   struct vnodeop_desc *descp = ap->a_desc;
   int reles, i;

#ifdef DIAGNOSTIC
   /*
    * We require at least one vp.
    */
   if (descp->vdesc_vp_offsets == NULL ||
       descp->vdesc_vp_offsets[0] == VDESC_NO_OFFSET) {
      panic ("VMBlockVopBypass: no vp's in map");
   }
#endif

   /*
    * Map the vnodes going in.  Later, we'll invoke the operation based on
    * the first mapped vnode's operation vector.
    */
   reles = descp->vdesc_flags;
   for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
      if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET) {
         break;   /* bail out at end of list */
      }

      vps_p[i] = this_vp_p =
         VOPARG_OFFSETTO(struct vnode**,descp->vdesc_vp_offsets[i],ap);

      /*
       * We're not guaranteed that any but the first vnode are of our type.
       * Check for and don't map any that aren't.  (We must always map first
       * vp or vclean fails.)
       */
      if (i && (*this_vp_p == NULLVP ||
          (*this_vp_p)->v_op != &VMBlockVnodeOps)) {
         old_vps[i] = NULLVP;
      } else {
         old_vps[i] = *this_vp_p;
         *(vps_p[i]) = VMBVPTOLOWERVP(*this_vp_p);
         /*
          * XXX - Several operations have the side effect of vrele'ing their
          * vp's.  We must account for that.  (This should go away in the
          * future.)
          */
         if (reles & VDESC_VP0_WILLRELE) {
            VREF(*this_vp_p);
         }
      }
   }

   /*
    * Call the operation on the lower layer with the modified argument
    * structure.
    */
   if (vps_p[0] && *vps_p[0]) {
      error = VCALL(ap);
   } else {
      printf("VMBlockVopBypass: no map for %s\n", descp->vdesc_name);
      error = EINVAL;
   }

   /*
    * Maintain the illusion of call-by-value by restoring vnodes in the
    * argument structure to their original value.
    */
   reles = descp->vdesc_flags;
   for (i = 0; i < VDESC_MAX_VPS; reles >>= 1, i++) {
      if (descp->vdesc_vp_offsets[i] == VDESC_NO_OFFSET) {
         break;   /* bail out at end of list */
      }
      if (old_vps[i]) {
         *(vps_p[i]) = old_vps[i];
         if (reles & VDESC_VP0_WILLRELE) {
            vrele(*(vps_p[i]));
         }
      }
   }

   /*
    * Map the possible out-going vpp (Assumes that the lower layer always
    * returns a VREF'ed vpp unless it gets an error.)
    */
   if (descp->vdesc_vpp_offset != VDESC_NO_OFFSET && !error) {
      vppp = VOPARG_OFFSETTO(struct vnode***, descp->vdesc_vpp_offset,ap);
      if (*vppp) {
         /* FIXME: set proper name for the vnode */
         error = VMBlockNodeGet(old_vps[0]->v_mount, **vppp, *vppp, NULL);
      }
   }

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopLookup --
 *
 *      "VOP_LOOKUP(9) -- lookup a component of a pathname"
 *
 * Results:
 *      Zero if the component name is found.  EJUSTRETURN if the namei
 *      operation is CREATE or RENAME, we're looking up the final component
 *      name, and said operation would succeed.  Otherwise returns an
 *      appropriate system error.
 *
 * Side effects:
 *      Requested vnode is locked and returned in *ap->a_vpp.
 *
 * Original function comment:
 *	We have to carry on the locking protocol on the null layer vnodes
 *	as we progress through the tree. We also have to enforce read-only
 *	if this layer is mounted read-only.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopLookup(struct vop_lookup_args *ap)
/*
struct vop_lookup_args {
   struct vnode *dvp;           // IN: pointer to searched directory
   struct vnode **vpp;          // OUT: if found, points to requested
                                //      vnode; else NULL
   struct componentname *cnp;   // IN: directory search context
};
*/
{
   struct componentname *cnp = ap->a_cnp;
   COMPAT_THREAD_VAR(td, cnp->cn_thread);
   struct vnode *dvp = ap->a_dvp;
   struct vnode *vp, *ldvp, *lvp;
   BlockHandle blockCookie;
   int flags = cnp->cn_flags;
   int error = 0;
   char *pathname;
   size_t pathname_len;

   /*
    * Fail attempts to modify a read-only filesystem w/o bothering with a
    * lower-layer lookup.
    */
   if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
       (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
      return EROFS;
   }

   /*
    * Before looking in the lower layer, determine whether the search path
    * should be blocked.  If so, do the following:
    *   1.  Make a copy of the block pathname.  (BlockWaitOnFile may make
    *       use of this, and our VMBlockNode may be destroyed while asleep
    *       if user forcibly unmounts file system.)
    *   2.  Bump up hold counts of current VMBlock directory vnode and its
    *       lower layer counterpart.  This makes sure that at least they
    *       aren't purged from memory while we sleep.
    *   3.  Unlock & relock directory vnodes around sleeping.  This prevents
    *       a cascading file system lookup deadlock.  (E.g., we have dvp locked,
    *       but another thread trying to look up dvp will block, holding /its/
    *       dvp's (dvp2) lock, and yet another thread would block looking up
    *       dvp2 while holding its dvp (dvp3), etc.
    *
    * If we find we were forcibly unmounted, fail with EIO.
    */

   pathname = uma_zalloc(VMBlockPathnameZone, M_WAITOK);
   if (pathname == NULL) {
      return ENOMEM;
   }

   /*
    * FIXME: we need to ensure that vnode always has name set up.
    * Currently VMBlockVopBypass() may produce vnodes without a name.
    */
   pathname_len = strlcpy(pathname,
                          VPTOVMB(dvp)->name ? VPTOVMB(dvp)->name : ".",
                          MAXPATHLEN);
   /*
    * Make sure we have room in the buffer to add our component.
    * + 1 is for separator (slash).
    */
   if (pathname_len + 1 + cnp->cn_namelen >= MAXPATHLEN) {
      error = ENAMETOOLONG;
      goto out;
   }

   if ((blockCookie = BlockLookup(pathname, OS_UNKNOWN_BLOCKER)) != NULL) {
      int lkflags = compat_lockstatus(dvp->v_vnlock, td) & LK_TYPE_MASK;
      lvp = VPTOVMB(dvp)->lowerVnode;
      vhold(dvp);
      vhold(lvp);
      COMPAT_VOP_UNLOCK(dvp, 0, td);

      error = BlockWaitOnFile(pathname, blockCookie);

      COMPAT_VOP_LOCK(dvp, lkflags, td);
      vdrop(lvp);
      vdrop(dvp);
      if (dvp->v_op != &VMBlockVnodeOps) {
         Debug("%s: vmblockfs forcibly unmounted?\n", __func__);
         error = EIO;
      }

      if (error) {
         goto out;
      }
   }

   /* We already verified that buffer is big enough. */
   pathname[pathname_len] = '/';
   bcopy(cnp->cn_nameptr, &pathname[pathname_len + 1], cnp->cn_namelen);
   pathname[pathname_len + 1 + cnp->cn_namelen] = 0;

   /*
    * Although it is possible to call VMBlockVopBypass(), we'll do a direct
    * call to reduce overhead
    */
   ldvp = VMBVPTOLOWERVP(dvp);
   vp = lvp = NULL;

   error = VOP_LOOKUP(ldvp, &lvp, cnp);
   if (error == EJUSTRETURN && (flags & ISLASTCN) &&
       (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
       (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME)) {
      error = EROFS;
   }

   if ((error == 0 || error == EJUSTRETURN) && lvp != NULL) {
      /*
       * Per VOP_LOOKUP(9), if looking up the current directory ("."), we bump
       * our vnode's refcount.
       */
      if (ldvp == lvp) {
         *ap->a_vpp = dvp;
         VREF(dvp);
         vrele(lvp);
      } else {
         error = VMBlockNodeGet(dvp->v_mount, lvp, &vp, pathname);
         if (error) {
            /* XXX Cleanup needed... */
            panic("VMBlockNodeGet failed");
         }
         *ap->a_vpp = vp;
         /* The vnode now owns pathname so don't try to free it below. */
         pathname = NULL;
      }
   }

out:
   if (pathname) {
      uma_zfree(VMBlockPathnameZone, pathname);
   }
   return error;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVopOpen --
 *
 *      "The VOP_OPEN() entry point is called before a file is accessed by a
 *      process..." - VOP_OPEN(9).  If the vnode in question is the file
 *      system's root vnode, allow access only to the superuser.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:     
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockVopOpen(struct vop_open_args *ap)
/*
struct vop_open_args {
   struct vnode *vp;    // IN: vnode which ioctl issued upon
   int fflag;           // IN: fdesc flags (see fcntl(2))
   struct ucred *cred;  // IN: caller's credentials (usually real uid vs euid)
   struct thread *td;   // IN: calling thread's context      
   FreeBSD <= 6 --
      int fdidx;        // IN: file descriptor number alloc'd to this open()
   FreeBSD >= 7 --
      struct file *fp   // IN: struct associated with this particular open()
};
*/
{
   VMBlockMount *mp;
   struct vnode *vp, *ldvp;
   struct file *fp;
   int retval;

   vp = ap->a_vp;
   ldvp = VMBVPTOLOWERVP(vp);

   mp = MNTTOVMBLOCKMNT(ap->a_vp->v_mount);
   if (ap->a_vp == mp->rootVnode) {
      /*
       * Opening the mount point is a special case.  First, only allow this
       * access to the superuser.  Next, we install a custom fileops vector in
       * order to trap the ioctl() and close() operations.  (See the *FileOps'
       * descriptions for more details.)
       *
       * NB:  Allowing only the superuser to open this directory breaks
       *      readdir() of the filesystem root for non-privileged users.
       *
       * Also, on FreeBSD 8.0 and newer we check for a specific module priv
       * because none of the existing privs seemed to match very well.
       */
      if ((retval = compat_priv_check(ap->a_td, PRIV_DRIVER)) == 0) {
         fp = ap->a_fp;
         fp->f_ops = &VMBlockFileOps;
      }
   } else {
      /*
       * Pass off to the lower layer.  If lower layer mapped a VM object, copy
       * its reference.
       */
      retval = VMBlockVopBypass(&ap->a_gen);
      if (retval == 0) {
         vp->v_object = ldvp->v_object;
      }
   }

   return retval;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopSetAttr --
 *
 *      "VOP_SETATTR(9) -- set attributes on a file or directory."
 *
 *      This version is simpler than the original null_setattr as it only
 *      tests whether the user is attempting an operation in a read-only
 *      file system.  Beyond that, it defers judgment about the validity of
 *      the request to the lower layer via vop_bypass.
 *
 * Results:
 *      Zero on success, else an appropriate system error.
 *
 * Side effects:
 *      None.
 *
 * Original function comment:
 *      Setattr call. Disallow write attempts if the layer is mounted read-
 *      only.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopSetAttr(struct vop_setattr_args *ap)
/*
struct vop_setattr_args {
   struct vnode *vp;    // IN: vnode operand
   struct vattr *vap;   // IN: attributes
   struct ucred *cred;  // IN: caller's credentials
   struct thread *td;   // IN: caller's thread context
};
*/
{
   struct vnode *vp = ap->a_vp;

   if (vp->v_mount->mnt_flag & MNT_RDONLY) {
      return EROFS;
   }

   return VMBlockVopBypass((struct vop_generic_args *)ap);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVopIoctl --
 *
 *      Handle ioctl(2) requests to add and remove file blocks.  Ioctl
 *      commands are defined in public/vmblock.h.  
 *
 * Results:
 *      Zero on success, otherwise an appropriate error is returned.
 *
 * Side effects
 *      A block may be placed on or removed from a file.  The root vnode's
 *      reference count will be incremented when a block is successfully added,
 *      and it will be decremented when a block is removed.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockVopIoctl(struct vop_ioctl_args *ap)      // IN/OUT: ioctl parameters
/*
struct vop_ioctl_args {
   struct vnode *vp;    // IN: vnode which ioctl issued upon
   u_long command;      // IN: ioctl command
   caddr_t data;        // IN: ioctl parameter (e.g., pathname)
   int fflag;           // IN: fcntl style flags (no-op?)
   struct ucred *cred;  // IN: caller's credentials (usually real uid vs euid)
   struct thread *td;   // IN: calling thread's context      
};
*/
{
   VMBlockIoctlArgs *ioctlArgs = (VMBlockIoctlArgs *)ap->a_data;
   VMBlockMount *mp;
   COMPAT_THREAD_VAR(td, ap->a_td);
   struct vnode *vp = ap->a_vp;
   char *pathbuf = NULL;
   int ret = 0, pathlen;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockVopIoctl: entry\n");

   /*
    * The operand vnode is passed in unlocked, so test a few things before
    * proceeding.
    *   1.  Make sure we're still dealing with a VMBlock vnode.  Note
    *       that this test -must- come before the next one.  Otherwise v_mount
    *       may be invalid.
    *   2.  Make sure the filesystem isn't being unmounted.
    */
   COMPAT_VOP_LOCK(vp, LK_EXCLUSIVE|LK_RETRY, td);
   if (vp->v_op != &VMBlockVnodeOps ||
       vp->v_mount->mnt_kern_flag & MNTK_UNMOUNT) {
      COMPAT_VOP_UNLOCK(vp, 0, td);
      return EBADF;
   }

   /*
    * At this layer/in this file system, only the root vnode handles ioctls,
    * and only the superuser may open the root vnode.  If we're not given
    * the root vnode, simply bypass to the next lower layer.
    */
   mp = MNTTOVMBLOCKMNT(vp->v_mount);
   if (vp != mp->rootVnode) {
      /*
       * VMBlockFileIoctl wraps the user's data in a special structure which
       * includes the user's file descriptor, so we must unwrap the data
       * argument before passing to the lower layer.
       */
      ap->a_data = ioctlArgs->data;
      COMPAT_VOP_UNLOCK(vp, 0, td);
      return VMBlockVopBypass((struct vop_generic_args *)ap);
   }

   pathbuf = uma_zalloc(VMBlockPathnameZone, M_WAITOK);

   switch (ap->a_command) {
   case VMBLOCK_ADD_FILEBLOCK:
   case VMBLOCK_DEL_FILEBLOCK:
   {
      /*
       * Trim trailing slashes
       */
      pathlen = strlcpy(pathbuf, ioctlArgs->data, MAXPATHLEN);
      pathlen = MIN(pathlen, MAXPATHLEN);
      while (pathlen > 0 && pathbuf[pathlen - 1] == '/') {
         pathbuf[pathlen - 1] = '\0';
         pathlen--;
      }

      VMBLOCKDEBUG("%s: %s on %s\n", __func__,
                   (ap->a_command == VMBLOCK_ADD_FILEBLOCK) ? "add" : "del",
                   pathbuf);

      /*
       * Don't block the mount point!
       */
      if (!strcmp(VPTOVMB(vp)->name, pathbuf)) {
         ret = EINVAL;
      } else {
         ret = (ap->a_command == VMBLOCK_ADD_FILEBLOCK) ?
                  BlockAddFileBlock(pathbuf, ioctlArgs->fileDesc) :
                  BlockRemoveFileBlock(pathbuf, ioctlArgs->fileDesc);

         /*
          * When adding a block, bump the reference count on the root vnode.  If
          * removing a block, decrement the reference count.  Of course, only do
          * so if the action succeeds!
          */
         if (ret == 0) {
            VI_LOCK(vp);
            vp->v_usecount += (ap->a_command == VMBLOCK_ADD_FILEBLOCK) ? 1 : -1;
            VI_UNLOCK(vp);
         }
      }
      break;
   }
#ifdef VMX86_DEVEL
   case VMBLOCK_LIST_FILEBLOCKS:
      BlockListFileBlocks();
      ret = 0;
      break;
   case VMBLOCK_PURGE_FILEBLOCKS:
      {
         int removed = 0;
         removed = BlockRemoveAllBlocks(OS_UNKNOWN_BLOCKER);
         VI_LOCK(vp);
         vp->v_usecount -= removed;
         VI_UNLOCK(vp);
      }
      ret = 0;
      break;
#endif
   default:
      Warning("VMBlockVopIoctl: unknown command (%lu) received.\n", ap->a_command);
      ret = EOPNOTSUPP;
   }

   COMPAT_VOP_UNLOCK(vp, 0, td);
   if (pathbuf) {
      uma_zfree(VMBlockPathnameZone, pathbuf);
   }
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVopGetAttr --
 *
 *      Query the underlying filesystem for file/directory information.
 *      Also fixup fsid to be ours rather than that of the underlying fs.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockVopGetAttr(struct vop_getattr_args *ap)
/*
struct vop_getattr_args {
   struct vnode *vp;    // IN: operand vnode
   struct vattr *vap;   // OUT: vnode's parameters
   struct ucred *ucred; // IN: caller's credentials
   struct thread *td;   // IN: caller's thread context
};
*/
{
   int error;

   if ((error = VMBlockVopBypass((struct vop_generic_args *)ap)) != 0) {
      return error;
   }

   ap->a_vap->va_fsid = ap->a_vp->v_mount->mnt_stat.f_fsid.val[0];
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockVopAccess --
 *
 *      "VOP_ACCESS(9) -- check access permissions of a file or Unix domain
 *      socket."  We handle this to disallow write access if our layer is,
 *      for whatever reason, mounted read-only.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockVopAccess(struct vop_access_args *ap)
/*
struct vop_access_args {
   struct vnode *vp;    // IN: operand vnode
   int mode;            // IN: access(2) flags
   struct vattr *vap;   // OUT: vnode's parameters
   struct ucred *ucred; // IN: caller's credentials
   struct thread *td;   // IN: caller's thread context
};
*/
{
   struct vnode *vp = ap->a_vp;
   compat_accmode_t mode = ap->compat_a_accmode;

   /*
    * Disallow write attempts on read-only layers; unless the file is a
    * socket, fifo, or a block or character device resident on the filesystem.
    */
   if (mode & VWRITE) {
      switch (vp->v_type) {
      case VDIR:
      case VLNK:
      case VREG:
         if (vp->v_mount->mnt_flag & MNT_RDONLY) {
            return EROFS;
         }
         break;
      default:
         break;
      }
   }
   return VMBlockVopBypass((struct vop_generic_args *)ap);
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockRename --
 *
 *      "VOP_RENAME(9) -- rename a file."
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 * Original function comment:
 *      We handle this to eliminate null FS to lower FS file moving. Don't
 *      know why we don't allow this, possibly we should.
 *----------------------------------------------------------------------------
 */

static int
VMBlockVopRename(struct vop_rename_args *ap)
/*
struct vop_rename_args {
   struct vnode *fdvp;          // IN: source directory
   struct vnode *fvp;           // IN: source file
   struct componentname *fcnp;  // IN: source's path lookup state
   struct vnode *tdvp;          // IN: destination directory
   struct vnode *tvp;           // IN: destination file
   struct componentname *tcnp;  // IN: destination's path lookup state
};
*/
{
   struct vnode *tdvp = ap->a_tdvp;
   struct vnode *fvp = ap->a_fvp;
   struct vnode *fdvp = ap->a_fdvp;
   struct vnode *tvp = ap->a_tvp;

   /* Check for cross-device rename. */
   if ((fvp->v_mount != tdvp->v_mount) ||
       (tvp && (fvp->v_mount != tvp->v_mount))) {
      if (tdvp == tvp) {
         vrele(tdvp);
      } else {
         vput(tdvp);
      }
      if (tvp) {
         vput(tvp);
      }
      vrele(fdvp);
      vrele(fvp);
      return EXDEV;
   }

   return VMBlockVopBypass((struct vop_generic_args *)ap);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopLock --
 *
 *      Acquire a vnode lock.
 *
 * Results:
 *      Zero on success, otherwise an error is returned.
 *
 * Side effects:
 *      Upper & lower layers share a lock, so both vnodes will be considered
 *      locked.
 *
 * Original function comment:
 *      We need to process our own vnode lock and then clear the interlock flag
 *      as it applies only to our vnode, not the vnodes below us on the stack.
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopLock(compat_vop_lock_args *ap)
/*
struct vop_lock_args {
   struct vnode *vp;    // IN: vnode operand
   int flags;           // IN: lockmgr(9) flags
   struct thread *td;   // IN: calling thread's context
};
*/
{
   struct vnode *vp = ap->a_vp;
   int flags = ap->a_flags;
   COMPAT_THREAD_VAR(td, ap->a_td);
   struct VMBlockNode *nn;
   struct vnode *lvp;
   int error;


   if ((flags & LK_INTERLOCK) == 0) {
      VI_LOCK(vp);
      ap->a_flags = flags |= LK_INTERLOCK;
   }
   nn = VPTOVMB(vp);
   /*
    * If we're still active we must ask the lower layer to lock as ffs
    * has special lock considerations in it's vop lock. -- FreeBSD
    */
   if (nn != NULL && (lvp = VMBVPTOLOWERVP(vp)) != NULL) {
      VI_LOCK_FLAGS(lvp, MTX_DUPOK);
      VI_UNLOCK(vp);
      /*
       * We have to hold the vnode here to solve a potential reclaim race.
       * If we're forcibly vgone'd while we still have refs, a thread
       * could be sleeping inside the lowervp's vop_lock routine.  When we
       * vgone we will drop our last ref to the lowervp, which would
       * allow it to be reclaimed.  The lowervp could then be recycled,
       * in which case it is not legal to be sleeping in it's VOP.
       * We prevent it from being recycled by holding the vnode here.
       */
      vholdl(lvp);
      error = COMPAT_VOP_LOCK(lvp, flags, td);

      /*
       * We might have slept to get the lock and someone might have clean
       * our vnode already, switching vnode lock from one in lowervp
       * to v_lock in our own vnode structure.  Handle this case by
       * reacquiring correct lock in requested mode.
       */
      if (VPTOVMB(vp) == NULL && error == 0) {
         ap->a_flags &= ~(LK_TYPE_MASK | LK_INTERLOCK);
         switch (flags & LK_TYPE_MASK) {
         case LK_SHARED:
            ap->a_flags |= LK_SHARED;
            break;
         case LK_UPGRADE:
         case LK_EXCLUSIVE:
            ap->a_flags |= LK_EXCLUSIVE;
            break;
         default:
            panic("Unsupported lock request %d\n",
                ap->a_flags);
         }
         COMPAT_VOP_UNLOCK(lvp, 0, td);
         error = vop_stdlock(ap);
      }
      vdrop(lvp);
   } else {
      error = vop_stdlock(ap);
   }

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopUnlock --
 *
 *      Release a vnode lock.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 * Original function comment:
 *      We need to process our own vnode unlock and then clear the interlock
 *      flag as it applies only to our vnode, not the vnodes below us on
 *      the stack.
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopUnlock(struct vop_unlock_args *ap)
/*
struct vop_unlock_args {
   struct vnode *vp;    // IN: vnode operand
   int flags;           // IN: lock request flags (see lockmgr(9))
   struct thread *td;   // IN: calling thread's context
};
*/
{
   struct vnode *vp = ap->a_vp;
#if __FreeBSD_version < 1300074
   int flags = ap->a_flags;
#endif
   COMPAT_THREAD_VAR(td, ap->a_td);
   struct VMBlockNode *nn;
   struct vnode *lvp;
   int error;

#if __FreeBSD_version < 1300074
   /*
    * If caller already holds interlock, drop it.  (Per VOP_UNLOCK() API.)
    * Also strip LK_INTERLOCK from flags passed to lower layer.
    */
   if ((flags & LK_INTERLOCK) != 0) {
      VI_UNLOCK(vp);
      ap->a_flags = flags &= ~LK_INTERLOCK;
   }
#endif
   nn = VPTOVMB(vp);
   if (nn != NULL && (lvp = VMBVPTOLOWERVP(vp)) != NULL) {
#if __FreeBSD_version < 1300074
      error = COMPAT_VOP_UNLOCK(lvp, flags, td);
#else
      error = COMPAT_VOP_UNLOCK(lvp, 0, td);
#endif
   } else {
      error = vop_stdunlock(ap);
   }

   return error;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopIsLocked --
 *
 *      Test whether a vnode is locked.
 *
 * Results:
 *      Zero if locked, non-zero otherwise.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopIsLocked(struct vop_islocked_args *ap)
/*
struct vop_islocked_args {
   struct vnode *vp;    // IN: vnode operand 
   struct thread *td;   // IN: calling thread's context
};
*/
{
   struct vnode *vp = ap->a_vp;
   COMPAT_THREAD_VAR(td, ap->a_td);

   return compat_lockstatus(vp->v_vnlock, td);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopInactive --
 *
 *      "VOP_INACTIVE() is called when the kernel is no longer using the vnode.
 *      This may be because the reference count reaches zero or it may be that
 *      the file system is being forcibly unmounted while there are open files.
 *      It can be used to reclaim space for `open but deleted' files."
 *
 * Results:
 *      Zero.
 *
 * Side effects:
 *      If this vnode's reference is zero, vrecycle() will handle induce
 *      cleanup.
 *
 * Original function comment:
 *	There is no way to tell that someone issued remove/rmdir operation
 *	on the underlying filesystem. For now we just have to release lowevrp
 *	as soon as possible.
 *
 *	Note, we can't release any resources nor remove vnode from hash before
 *	appropriate VXLOCK stuff is is done because other process can find
 *	this vnode in hash during inactivation and may be sitting in vget()
 *	and waiting for VMBlockVopInactive to unlock vnode. Thus we will do
 *	all those in VOP_RECLAIM.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopInactive(struct vop_inactive_args *ap)
/*
struct vop_inactive_args {
   struct vnode *vp;    // IN: vnode operand 
   struct thread *td;   // IN: calling thread's context
};
*/
{
   struct vnode *vp = ap->a_vp;

   vp->v_object = NULL;

   /*
    * If this is the last reference, then free up the vnode so as not to
    * tie up the lower vnode.
    */
   vrecycle(vp);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopReclaim --
 *
 *      "VOP_RECLAIM() is called when a vnode is being reused for a
 *      different file system.  Any file system specific resources
 *      associated with the vnode should be freed."
 *
 * Results:
 *      Returns zero.
 *
 * Side effects:
 *      If node is an associate VMBlockNode, it's removed from
 *      the VMBlockNode hash and freed.  Reference to the lower vnode, if
 *      it exists, is also dropped.      
 *
 * Original function comment:
 *      Now, the VXLOCK is in force and we're free to destroy the null vnode.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopReclaim(struct vop_reclaim_args *ap)
/*
struct vop_reclaim_args {
   struct vnode *vp;    // IN: vnode operand 
   struct thread *td;   // IN: calling thread's context
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct VMBlockNode *xp = VPTOVMB(vp);
   struct vnode *lowervp = xp->lowerVnode;

   KASSERT(lowervp != NULL, ("reclaiming node with no lower vnode"));

   VMBlockHashRem(xp);

   /*
    * Use the interlock to protect the clearing of v_data to
    * prevent faults in VMBlockVopLock().
    */
   VI_LOCK(vp);
   vp->v_data = NULL;
   vp->v_object = NULL;

   /*
    * Reassign lock pointer to this vnode's lock.  (Originally assigned
    * to the lower layer's lock.)
    */
   vp->v_vnlock = &vp->v_lock;
   compat_lockmgr(vp->v_vnlock, LK_EXCLUSIVE|LK_INTERLOCK, VI_MTX(vp), curthread);
   vput(lowervp);

   /*
    * Clean up VMBlockNode attachment.
    */
   if (xp->name) {
      uma_zfree(VMBlockPathnameZone, xp->name);
   }
   free(xp, M_VMBLOCKFSNODE);

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopPrint --
 *
 *      "VOP_PRINT -- print debugging information"
 *
 * Results:
 *      Zero.  Always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static int
VMBlockVopPrint(struct vop_print_args *ap)
/*
struct vop_print_args {
   struct vnode *vp;    // IN: vnode operand
};
*/
{
   struct vnode *vp = ap->a_vp;
   printf("\tvp=%p, lowervp=%p\n", vp, VMBVPTOLOWERVP(vp));
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockVopGetWriteMount --
 *
 *      When the caller wishes to begin a write operation, we need to bump
 *      the count of write operations on the destination file system.  This
 *      routine passes the request down.  "Real" file systems will usually
 *      call vop_stdgetwritemount().
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
VMBlockVopGetWriteMount(struct vop_getwritemount_args *ap)
/*
struct vop_getwritemount_args {
   struct vnode *vp;    // IN: vnode operand
   struct mount **mpp;  // OUT: pointer to filesystem where write operation
                        //      will actually occur
};
*/
{
   struct VMBlockNode *xp;
   struct vnode *lowervp;
   struct vnode *vp;

   vp = ap->a_vp;
   VI_LOCK(vp);
   xp = VPTOVMB(vp);
   if (xp && (lowervp = xp->lowerVnode)) {
      VI_LOCK_FLAGS(lowervp, MTX_DUPOK);
      VI_UNLOCK(vp);
      vholdl(lowervp);
      VI_UNLOCK(lowervp);
      VOP_GETWRITEMOUNT(lowervp, ap->a_mpp);
      vdrop(lowervp);
   } else {
      VI_UNLOCK(vp);
      *(ap->a_mpp) = NULL;
   }
   return 0;
}

