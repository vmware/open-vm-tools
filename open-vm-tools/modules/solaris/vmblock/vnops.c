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
 * vnops.c --
 *
 *      vnode operations for vmblock file system.
 *
 */

#include <sys/types.h>
#include <sys/dirent.h>
#include <sys/sysmacros.h>
#include <sys/file.h>           /* FREAD, FWRITE, etc flags */
#include <sys/fs_subr.h>        /* fs_fab_acl */
#include <sys/stat.h>           /* S_IRWXU and friends */
#include <sys/dnlc.h>           /* Directory name lookup cache */
#include <sys/signal.h>         /* k_sigset_t and signal macros */
#include <sys/proc.h>           /* practive (the active process) */
#include <sys/user.h>           /* u macro for current user area */
#include <sys/thread.h>         /* curthread and curproc macros */
#include <vm/seg_vn.h>          /* segment vnode mapping for mmap() */
#include <sys/vmsystm.h>        /* VM address mapping functions */
#include <sys/systm.h>          /* strlen() */

#include "vmblock.h"
#include "module.h"
#include "block.h"


extern int VMBlockVnodePut(struct vnode *vp);
extern int VMBlockVnodeGet(struct vnode **vpp, struct vnode *realVp,
                           const char *name, size_t nameLen,
                           struct vnode *dvp, struct vfs *vfsp, Bool isRoot);

/*
 * Vnode Entry Points
 */

/*
 *----------------------------------------------------------------------------
 *
 * VMBlockOpen --
 *
 *    Invoked when open(2) is called on a file in our filesystem.
 *
 *    "Opens a file referenced by the supplied vnode.  The open() system call
 *    has already done a vop_lookup() on the path name, which returned a vnode
 *    pointer and then calls to vop_open().  This function typically does very
 *    little since most of the real work was performed by vop_lookup()."
 *    (Solaris Internals, p537)
 *
 * Results:
 *    Zero on success, error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockOpen(struct vnode **vpp,    // IN: Vnode for file to open
            int flag,              // IN: Open flags
            struct cred *cr        // IN: Credentials of caller
#if OS_VFS_VERSION >= 5
          , caller_context_t *ctx  // IN: Caller's context
#endif
           )
{
   VMBlockMountInfo *mip;
   Bool isRoot = TRUE;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockOpen: entry\n");

   /*
    * The opened vnode is held for us, so we don't need to do anything here
    * except make sure only root opens the mount point.
    */
   mip = VPTOMIP(*vpp);
   if (mip->root == *vpp) {
      isRoot = crgetuid(cr) == 0;
   }

   return isRoot ? 0 : EACCES;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockClose --
 *
 *    Invoked when a user calls close(2) on a file in our filesystem.
 *
 *    "Closes the file given by the supplied vnode.  When this is the last
 *    close, some filesystems use vop_close() to initiate a writeback of
 *    outstanding dirty pages by checking the reference cound in the vnode."
 *    (Solaris Internals, p536)
 *
 * Results:
 *    Zero on success, error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockClose(struct vnode *vp,     // IN: Vnode of file that is being closed
             int flag,             // IN: Flags file was opened with
             int count,            // IN: Reference count on this vnode
             offset_t offset,      // IN: File offset
             struct cred *cr       // IN :Credentials of caller
#if OS_VFS_VERSION >= 5
           , caller_context_t *ctx // IN: Caller's context
#endif
            )
{
   VMBlockMountInfo *mip;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockClose: entry\n");

   /*
    * If someone is closing the root of our file system (the mount point), then
    * we need to remove all blocks that were added by this thread.  Note that
    * Solaris calls close with counts greater than one, but we only want to
    * actually close the file when the count reaches one.
    */
   mip = VPTOMIP(vp);
   if (count == 1 && vp == mip->root) {
      BlockRemoveAllBlocks(curthread);
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockIoctl --
 *
 *    Invoked when a user calls ioctl(2) on a file in our filesystem.
 *    Performs a specified operation on the file.
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockIoctl(struct vnode *vp,     // IN:  Vnode of file to operate on
             int cmd,              // IN:  Requested command from user
             intptr_t arg,         // IN:  Arguments for command
             int flag,             // IN:  File pointer flags and data model
             struct cred *cr,      // IN:  Credentials of caller
             int *rvalp            // OUT: Return value on success
#if OS_VFS_VERSION >= 5
           , caller_context_t *ctx // IN: Caller's context
#endif
            )
{
   VMBlockMountInfo *mip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockIoctl: entry\n");

   mip = VPTOMIP(vp);
   if (vp != mip->root) {
      return ENOTSUP;
   }

   if (rvalp) {
      *rvalp = 0;
   }

   switch (cmd) {
   case VMBLOCK_ADD_FILEBLOCK:
   case VMBLOCK_DEL_FILEBLOCK:
   {
      struct pathname pn;

      ret = pn_get((char *)arg, UIO_USERSPACE, &pn);
      if (ret) {
         goto out;
      }

      /* Remove all trailing path separators. */
      while (pn.pn_pathlen > 0 && pn.pn_path[pn.pn_pathlen - 1] == '/') {
         pn.pn_path[pn.pn_pathlen - 1] = '\0';
         pn.pn_pathlen--;
      }

      ret = cmd == VMBLOCK_ADD_FILEBLOCK ?
               BlockAddFileBlock(pn.pn_path, curthread) :
               BlockRemoveFileBlock(pn.pn_path, curthread);
      pn_free(&pn);
      break;
   }
#ifdef VMX86_DEVEL
   case VMBLOCK_LIST_FILEBLOCKS:
      BlockListFileBlocks();
      ret = 0;
      break;
#endif
   default:
      Warning("VMBlockIoctl: unknown command (%d) received.\n", cmd);
      return ENOTSUP;
   }

out:
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockGetattr --
 *
 *    "Gets the attributes for the supplied vnode." (Solaris Internals, p536)
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockGetattr(struct vnode *vp,       // IN:  Vnode of file to get attributes for
               struct vattr *vap,      // OUT: Filled in with attributes of file
               int flags,              // IN:  Getattr flags (see ATTR_ in vnode.h)
               struct cred *cr         // IN:  Credentials of caller
#if OS_VFS_VERSION >= 5
             , caller_context_t *ctx   // IN: Caller's context
#endif
              )
{
   VMBlockMountInfo *mip;
   VMBlockVnodeInfo *vip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockGetattr: entry\n");

   mip = VPTOMIP(vp);
   vip = VPTOVIP(vp);

   ASSERT(mip);
   ASSERT(vip);

   ret = VOP_GETATTR(vip->realVnode, vap, flags, cr
#if OS_VFS_VERSION >= 5
                     , ctx
#endif
                    );
   if (ret) {
      return ret;
   }

   if (vp == mip->root) {
      vap->va_type = VDIR;
   } else {
      vap->va_type = VLNK;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockAccess --
 *
 *    This function is invoked when the user calls access(2) on a file in our
 *    filesystem.  It checks to ensure the user has the specified type of
 *    access to the file.
 *
 * Results:
 *    Zero if access is allowed, error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockAccess(struct vnode *vp,     // IN: Vnode of file to check access for
              int mode,             // IN: Mode of access
              int flags,            // IN: Flags
              struct cred *cr       // IN: Credentials of caller
#if OS_VFS_VERSION >= 5
            , caller_context_t *ctx // IN: Caller's context
#endif
              )
{
   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockAccess: entry\n");

   /* Success */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockLookup --
 *
 *    Looks in the provided directory for the specified filename.  Only
 *    succeeds and creates a vmblock vnode if nm exists in the redirect path.
 *
 *    "Looks up the path name for the supplied vnode.  The vop_lookup() does
 *    file-name translation for the open, stat system calls." (Solaris
 *    Internals, p537)
 *
 * Results:
 *   Returns zero on success and ENOENT if the file cannot be found
 *   If file is found, a vnode representing the file is returned in vpp.
 *
 * Side effects:
 *   None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockLookup(struct vnode *dvp,   // IN:  Directory to look in
              char *nm,            // IN:  Name of component to lookup in directory
              struct vnode **vpp,  // OUT: Pointer to vnode representing found file
              struct pathname *pnp,// IN:  Full pathname being looked up
              int flags,           // IN:  Lookup flags (see vnode.h)
              struct vnode *rdir,  // IN:  Vnode of root device
              struct cred *cr          // IN:  Credentials of caller
#if OS_VFS_VERSION >= 5
           ,  caller_context_t *ctx    // IN: Caller's context
           ,  int *direntflags         // IN:
           ,  struct pathname *rpnp    // IN:
#endif
            )
{
   struct vnode *realVp;
   VMBlockMountInfo *mip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMblockLookup: entry\n");

   /* First ensure that we are looking in a directory. */
   if (dvp->v_type != VDIR) {
      return ENOTDIR;
   }

   /* Don't invoke lookup for ourself. */
   if (nm[0] == '\0' || (nm[0] == '.' && nm[1] == '\0')) {
      VN_HOLD(dvp);
      *vpp = dvp;
      return 0;
   }

   *vpp = NULL;

   /* Make sure nm exists before creating our link to it. */
   mip = VPTOMIP(dvp);
   ret = VOP_LOOKUP(mip->redirectVnode, nm, &realVp, pnp, flags, rdir, cr
#if OS_VFS_VERSION >= 5
                    , ctx, direntflags, rpnp
#endif
                   );
   if (ret) {
      return ret;
   }

   ret = VMBlockVnodeGet(vpp, realVp, nm, strlen(nm), dvp, dvp->v_vfsp, FALSE);
   if (ret) {
      VN_RELE(realVp);
      return ret;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockReaddir --
 *
 *    Reads as many entries from the directory as will fit in to the provided
 *    buffer.
 *
 *    "The vop_readdir() method reads chunks of the directory into a uio
 *    structure.  Each chunk can contain as many entries as will fit within
 *    the size supplied by the uio structure.  The uio_resid structure member
 *    shows the size of the getdents request in bytes, which is divided by the
 *    size of the directory entry made by the vop_readdir() method to
 *    calculate how many directory entries to return." (Solaris Internals,
 *    p555)
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockReaddir(struct vnode *vp,       // IN: Vnode of directory to read
               struct uio *uiop,       // IN: User's read request
               struct cred *cr,        // IN: Credentials of caller
               int *eofp               // OUT: Indicates we are done
#if OS_VFS_VERSION >= 5
             , caller_context_t *ctx   // IN: Caller's context
             , int flags               // IN: flags
#endif
               )
{
   VMBlockMountInfo *mip;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockReaddir: entry\n");

   mip = (VMBlockMountInfo *)vp->v_vfsp->vfs_data;
   return VOP_READDIR(mip->redirectVnode, uiop, cr, eofp
#if OS_VFS_VERSION >= 5
                     , ctx, flags
#endif
                     );
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockReadlink --
 *
 *    "Follows the symlink in the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *    Zero on success, error code on failure.
 *
 * Side effects:
 *    Blocks if a block has been placed on this file.
 *
 *----------------------------------------------------------------------------
 */

static int
VMBlockReadlink(struct vnode *vp,      // IN: Vnode for the symlink
                struct uio *uiop,      // IN: IO request structure
                struct cred *cr        // IN: Credentials of caller
#if OS_VFS_VERSION >= 5
              , caller_context_t *ctx  // IN: Caller's context
#endif
               )
{
   VMBlockMountInfo *mip;
   VMBlockVnodeInfo *vip;
   int ret;

   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockReadlink: entry\n");

   mip = VPTOMIP(vp);
   vip = VPTOVIP(vp);

   if (vip->nameLen + 1 >= uiop->uio_resid) {
      Warning("VMBlockReadlink: name is too long for provided buffer\n");
      return ENAMETOOLONG;
   }

   BlockWaitOnFile(vip->name, NULL);

   /* Copy path to user space. */
   ASSERT(vip->name[vip->nameLen] == '\0');
   ret = uiomove(vip->name, vip->nameLen + 1, UIO_READ, uiop);
   if (ret) {
      return ret;
   }

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * VMBlockInactive --
 *
 *    Frees a vnode that is no longer referenced.
 *
 *    "Free resources and releases the supplied vnode.  The file system can
 *    choose to destroy the vnode or put it onto an inactive list, which is
 *    managed by the file system implementation." (Solaris Internals, p536)
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static void
VMBlockInactive(struct vnode *vp,      // IN: Vnode to operate on
                struct cred *cr        // IN: Credentials of the caller
#if OS_VFS_VERSION >= 5
              , caller_context_t *ctx  // IN: Caller's context
#endif
               )
{
   Debug(VMBLOCK_ENTRY_LOGLEVEL, "VMBlockInactive: entry\n");

   VMBlockVnodePut(vp);
}

const fs_operation_def_t vnodeOpsArr[] = {
   VMBLOCK_VOP(VOPNAME_OPEN, vop_open, VMBlockOpen),
   VMBLOCK_VOP(VOPNAME_CLOSE, vop_close, VMBlockClose),
   VMBLOCK_VOP(VOPNAME_IOCTL, vop_ioctl, VMBlockIoctl),
   VMBLOCK_VOP(VOPNAME_GETATTR, vop_getattr, VMBlockGetattr),
   VMBLOCK_VOP(VOPNAME_ACCESS, vop_access, VMBlockAccess),
   VMBLOCK_VOP(VOPNAME_LOOKUP, vop_lookup, VMBlockLookup),
   VMBLOCK_VOP(VOPNAME_READDIR, vop_readdir, VMBlockReaddir),
   VMBLOCK_VOP(VOPNAME_READLINK, vop_readlink, VMBlockReadlink),
#if OS_VFS_VERSION <=3
   VMBLOCK_VOP(VOPNAME_INACTIVE, vop_inactive,
               (fs_generic_func_p)VMBlockInactive),
#else
   VMBLOCK_VOP(VOPNAME_INACTIVE, vop_inactive, VMBlockInactive),
#endif
   { NULL }
};

