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
 * vnops.c --
 *
 *	Vnode operations for FreeBSD HGFS client.
 */

#include <sys/param.h>          // for everything
#include <sys/vnode.h>          // for struct vnode
#include <sys/mount.h>          // for struct mount
#include <sys/namei.h>          // for name lookup goodness
#include <sys/libkern.h>        // for string & other functions
#include <sys/fcntl.h>          // for in-kernel file access flags (FREAD, etc)
#include <sys/stat.h>           // for file flag bitmasks (S_IRWXU, etc)
#include <sys/uio.h>            // for uiomove
#include <sys/dirent.h>         // for struct dirent

#include "cpName.h"
#include "staticEscape.h"

#include "hgfsUtil.h"

#include "hgfs_kernel.h"
#include "request.h"
#include "state.h"
#include "debug.h"
#include "fsutil.h"
#include "vnopscommon.h"

/*
 * Macros
 */

/*
 * Hgfs permissions are similar to Unix permissions in that they both include
 * bits for read vs. write vs. execute permissions.  However, Hgfs is only
 * concerned with file owners, meaning no "group" or "other" bits, so we need to
 * translate between Hgfs and Unix permissions with a simple bitshift.  The
 * shift value corresponds to omitting the "group" and "other" bits.
 */
#define HGFS_ATTR_MODE_SHIFT    6

/* Sets the values of request headers properly */
#define HGFS_INIT_REQUEST_HDR(request, req, _op)                \
         do {                                                   \
            request->header.id = HgfsKReq_GetId(req);           \
            request->header.op = _op;                           \
         } while(0)

/* FreeBSD times support nsecs, so only use these functions directly */
#define HGFS_SET_TIME(unixtm, nttime)                   \
         HgfsConvertFromNtTimeNsec(&unixtm, nttime)
#define HGFS_GET_TIME(unixtm)                           \
         HgfsConvertTimeSpecToNtTime(&unixtm)

/* Determine if this is the root vnode. */
#define HGFS_IS_ROOT_VNODE(sip, vp)                     \
                (sip->rootVnode == vp)


/*
 * Local functions (prototypes)
 */

static vop_lookup_t     HgfsVopLookup;
static vop_create_t	HgfsVopCreate;
static vop_open_t	HgfsVopOpen;
static vop_close_t	HgfsVopClose;
static vop_access_t	HgfsVopAccess;
static vop_getattr_t	HgfsVopGetattr;
static vop_setattr_t	HgfsVopSetattr;
static vop_read_t	HgfsVopRead;
static vop_write_t	HgfsVopWrite;
static vop_remove_t	HgfsVopRemove;
static vop_rename_t	HgfsVopRename;
static vop_mkdir_t	HgfsVopMkdir;
static vop_rmdir_t	HgfsVopRmdir;
static vop_readdir_t	HgfsVopReaddir;
static vop_inactive_t   HgfsVopInactive;
static vop_reclaim_t	HgfsVopReclaim;
static vop_print_t	HgfsVopPrint;

/*
 * Global data
 */

/*
 * HGFS vnode operations vector
 */
struct vop_vector HgfsVnodeOps = {
   .vop_lookup          = HgfsVopLookup,
   .vop_create          = HgfsVopCreate,
   .vop_open            = HgfsVopOpen,
   .vop_close           = HgfsVopClose,
   .vop_access          = HgfsVopAccess,
   .vop_getattr         = HgfsVopGetattr,
   .vop_setattr         = HgfsVopSetattr,
   .vop_read            = HgfsVopRead,
   .vop_write           = HgfsVopWrite,
   .vop_remove          = HgfsVopRemove,
   .vop_rename          = HgfsVopRename,
   .vop_mkdir           = HgfsVopMkdir,
   .vop_rmdir           = HgfsVopRmdir,
   .vop_readdir         = HgfsVopReaddir,
   .vop_inactive        = HgfsVopInactive,
   .vop_reclaim         = HgfsVopReclaim,
   .vop_print           = HgfsVopPrint,

   /*
    * The following operations are not supported directly by the Hgfs module,
    * so we fall back to the kernel's default support routines.  (Most cases
    * return EOPNOTSUPP or EINVAL.
    */
   .vop_advlock         = VOP_EINVAL,
   .vop_bmap            = vop_stdbmap,
   .vop_bypass          = VOP_EOPNOTSUPP,
   .vop_fsync           = VOP_NULL,
   .vop_getpages        = vop_stdgetpages,
   .vop_getwritemount   = vop_stdgetwritemount,
   .vop_ioctl           = VOP_ENOTTY,
   .vop_islocked        = vop_stdislocked,
   .vop_kqfilter        = vop_stdkqfilter,
   .vop_lease           = VOP_NULL,
   .vop_lock            = vop_stdlock,
   .vop_pathconf        = VOP_EINVAL,
   .vop_poll            = vop_nopoll,
   .vop_putpages        = vop_stdputpages,
   .vop_readlink        = VOP_EINVAL,
   .vop_revoke          = VOP_PANIC,
   .vop_unlock          = vop_stdunlock,
};


/*
 * Local functions (definitions)
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopLookup --
 *
 *      Looks in the provided directory for the specified filename by calling
 *      HgfsLookupInt.
 *
 * Results:
 *      Returns zero on success and ENOENT if the file cannot be found
 *      If file is found, a vnode representing the file is returned in vpp.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopLookup(struct vop_lookup_args *ap)
/*
struct vop_lookup_args {
   struct vnode *dvp;           // IN    : locked vnode of search directory
   struct vnode **vpp;          // IN/OUT: addr to store located (locked) vnode
   struct componentname *cnp;   // IN    : pathname component to search for
};
 */
{
   struct vnode *dvp = ap->a_dvp;
   struct vnode **vpp = ap->a_vpp;
   struct componentname *cnp = ap->a_cnp;

   return HgfsLookupInt(dvp, vpp, cnp);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopCreate --
 *
 *      This entry point is invoked when a user calls open(2) with the O_CREAT
 *      flag specified.  We simply call HgfsCreateInt which does the file
 *      creation work in a FreeBSD / Mac OS independent way.
 *
 * Results:
 *      Returns zero on success and an appropriate error code on error.
 *
 * Side effects:
 *      If the file doesn't exist, a vnode will be created.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopCreate(struct vop_create_args *ap)
/*
struct vop_create {
   struct vnode *dvp;           // IN : locked directory vnode
   struct vnode **vp;           // OUT: location to place resultant locked vnode
   struct componentname *cnp;   // IN : pathname component created
   struct vattr *vap;           // IN : attributes to create new object with
 */
{
   struct vnode *dvp = ap->a_dvp;
   struct vnode **vpp = ap->a_vpp;
   struct componentname *cnp = ap->a_cnp;
   struct vattr *vap = ap->a_vap;

   return HgfsCreateInt(dvp, vpp, cnp, vap->va_mode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopOpen --
 *
 *      Invoked when open(2) is called on a file in our filesystem.  Sends an
 *      OPEN request to the Hgfs server with the filename of this vnode.
 *
 *      "Opens a file referenced by the supplied vnode.  The open() system call
 *      has already done a vop_lookup() on the path name, which returned a vnode
 *      pointer and then calls to vop_open().  This function typically does very
 *      little since most of the real work was performed by vop_lookup()."
 *      (Solaris Internals, p537)
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      The HgfsOpenFile for this file is given a handle that can be used on
 *      future read and write requests.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopOpen(struct vop_open_args *ap)
/*
struct vop_open_args {
   struct vnode *vp;    // IN: vnode of file to open
   int mode;            // IN: access mode requested by calling process
   struct ucred *cred;  // IN: calling process's user's credentials
   struct thread *td;   // IN: thread accessing file
   int fdidx;           // IN: file descriptor number
};
*/
{
   struct vnode *vp = ap->a_vp;
   int mode = ap->a_mode;

   return HgfsOpenInt(vp, mode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopClose --
 *
 *      Invoked when a user calls close(2) on a file in our filesystem.
 *
 *      Calls HgfsCloseInt which handles the close in a FreeBSD / Mac OS
 *      independent way.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopClose(struct vop_close_args *ap)
/*
struct vop_close_args {
   struct vnode *vp;    // IN: vnode of object to close [exclusive lock held]
   int fflag;           // IN: F* flags (FWRITE, etc) on object
   struct ucred *cred;  // IN: calling process's user's credentials
   struct thread *td;   // IN: thread accessing file
};
*/
{
   struct vnode *vp = ap->a_vp;
   int fflag = ap->a_fflag;
   struct vnode *rootVnode;
   int ret = 0;

   /*
    * According to the FreeBSD manpage, VOP_CLOSE can be called with or
    * without a lock held on vp. However, in the FreeBSD 6.2 source code,
    * the only place that VOP_CLOSE is called without a lock held is in
    * kern/vfs_subr.c::vgone1 and only if the vnode is not already doomed with the
    * VI_DOOMED flag. In addition, the VFS layer will not acquire a vnode lock
    * on a doomed vnode (kern/vfs_vnops.c::vn_lock). This means that there is no need
    * to do any locking here as this function will always be called in a serial manner.
    */

   /*
    * A problem exists where vflush (on unmount) calls close on the root vnode without
    * first having calling open.
    * Here is the problematic sequence of events:
    * 1. HgfsVfsUnmount calls vflush with 1 v_usecount ref on the rootVnode (the one from mount).
    * 2. vflush calls vgone on the root vnode because rootrefs (in FreeBSD vflush code)
    *      is > 0.
    * 3. vgone calls VOP_CLOSE because the root vnode has a v_usecount == 1.
    * The problem is that there was never an open to match the close. This means that when
    * HgfsCloseInt tries decrement the handle reference count, it will go negative (in addition
    * to sending a bad close to the hgfs server). To handle this situation, look for this
    * specific case (which only happens on FreeBSD) and do not call HgfsCloseInt.
    */

   rootVnode = HGFS_VP_TO_SIP(vp)->rootVnode;
   if ((rootVnode == vp) && (rootVnode->v_usecount == 1)) {
      DEBUG(VM_DEBUG_LOG, "Skipping final close on rootVnode\n");
      goto out;
   }

   ret = HgfsCloseInt(vp, fflag);

out:
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopAccess --
 *
 *      This function is invoked when the user calls access(2) on a file in our
 *      filesystem.  It checks to ensure the user has the specified type of
 *      access to the file.
 *
 *      We send a GET_ATTRIBUTE request by calling HgfsGetattr() to get the mode
 *      (permissions) for the provided vnode.
 *
 * Results:
 *      Returns 0 if access is allowed and a non-zero error code otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopAccess(struct vop_access_args *ap)
/*
struct vop_access_args {
   struct vnode *vp;    // IN: vnode of file to check
   int mode;            // IN: type of access required (mask of VREAD|VWRITE|VEXEC)
   struct ucred *cred;  // IN: calling process's user's credentials
   struct thread *td;   // IN: thread accessing file
};
*/
{
   struct vnode *vp = ap->a_vp;
   int mode = ap->a_mode;

   return HgfsAccessInt(vp, mode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopGetattr --
 *
 *      "Gets the attributes for the supplied vnode." (Solaris Internals, p536)
 *
 * Results:
 *      Zero if successful, an errno-type value otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopGetattr(struct vop_getattr_args *ap)
/*
struct vop_getattr_args {
   struct vnode *vp;    // IN : vnode of file
   struct vattr *vap;   // OUT: attribute container
   struct ucred *cred;  // IN : calling process's user's credentials
   struct thread *td;   // IN : thread accessing file
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct vattr *vap = ap->a_vap;

   return HgfsGetattrInt(vp, vap);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopSetattr --
 *
 *      Maps the FreeBSD attributes to Hgfs attributes (by calling
 *      HgfsSetattrCopy()) and sends a set attribute request to the Hgfs server.
 *
 *      "Sets the attributes for the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *      The file on the host will have new attributes.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopSetattr(struct vop_setattr_args *ap)
/*
struct vop_setattr_args {
   struct vnode *vp;    // IN: vnode of file
   struct vattr *vap;   // IN: attribute container
   struct ucred *cred;  // IN: calling process's user's credentials
   struct thread *td;   // IN: thread accessing file
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct vattr *vap = ap->a_vap;

   return HgfsSetattrInt(vp, vap);

}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopRead --
 *
 *      Invoked when a user calls read(2) on a file in our filesystem.
 *
 * Results:
 *      Returns zero on success and an error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopRead(struct vop_read_args *ap)
/*
struct vop_read_args {
   struct vnode *vp;    // IN   : the vnode of the file
   struct uio *uio;     // INOUT: location of data to be read
   int ioflag;          // IN   : hints & other directives
   struct ucread *cred; // IN   : caller's credentials
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct uio *uiop = ap->a_uio;

   return HgfsReadInt(vp, uiop);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopWrite --
 *
 *      This is invoked when a user calls write(2) on a file in our filesystem.
 *
 *
 * Results:
 *      Returns 0 on success and error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopWrite(struct vop_write_args *ap)
/*
struct vop_write_args {
   struct vnode *vp;    // IN   :
   struct uio *uio;     // INOUT:
   int ioflag;          // IN   :
   struct ucred *cred;  // IN   :
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct uio *uiop = ap->a_uio;
   int ioflag = ap->a_ioflag;

   return HgfsWriteInt(vp, uiop, ioflag);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopRemove --
 *
 *      Composes the full pathname of this file and sends a DELETE_FILE request
 *      by calling HgfsDelete().
 *
 *      "Removes the file for the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *      Returns 0 on success or a non-zero error code on error.
 *
 * Side effects:
 *      If successful, the file specified will be deleted from the host's
 *      filesystem.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopRemove(struct vop_remove_args *ap)
/*
struct vop_remove_args {
   struct vnode *dvp;           // IN: parent directory
   struct vnode *vp;            // IN: vnode to remove
   struct componentname *cnp;   // IN: file's pathname information
*/
{
   struct vnode *vp = ap->a_vp;

   return HgfsRemoveInt(vp);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopRename --
 *
 *      Renames the provided source name in the source directory with the
 *      destination name in the destination directory.  A RENAME request is sent
 *      to the Hgfs server.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopRename(struct vop_rename_args *ap)
/*
struct vop_rename_args {
   struct vnode *fdvp;          // IN: "from" parent directory
   struct vnode *fvp;           // IN: "from" file
   struct componentname *fcnp:  // IN: "from" pathname info
   struct vnode *tdvp;          // IN: "to" parent directory
   struct vnode *tvp;           // IN: "to" file (if it exists)
   struct componentname *tcnp:  // IN: "to" pathname info
};
*/
{
   struct vnode *fdvp = ap->a_fdvp;
   struct vnode *fvp = ap->a_fvp;
   struct vnode *tdvp = ap->a_tdvp;
   struct vnode *tvp = ap->a_tvp;
   struct componentname *tcnp = ap->a_tcnp;


   int ret;

   /*
    * Note that fvp and fdvp are not locked when called by the VFS layer. However,
    * this does not matter for the HgfsRenameInt implementaiton which does not use
    * the handle or mode from the HgfsOpenFile (the two things that can change in an
    * HgfsOpenFile struct). So while a normal VFS implementation would lock at least fvp
    * here, this one does not.
    */
   ret = HgfsRenameInt(fvp, tdvp, tvp, tcnp);

   vrele(fdvp);
   vrele(fvp);

   vput(tdvp);
   if (tvp) {
      vput(tvp);
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMkdir --
 *
 *      Calls HgfsMkdirInt which does all of the directory creation work in a
 *      FreeBSD / Mac OS independent way.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      If successful, a directory is created on the host's filesystem.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopMkdir(struct vop_mkdir_args *ap)
/*
struct vop_mkdir_args {
   struct vnode *dvp;           // IN : directory vnode
   struct vnode **vpp;          // OUT: pointer to new directory vnode
   struct componentname *cnp;   // IN : pathname component created
   struct vattr *vap;           // IN : attributes to create directory with
};
*/
{
   struct vnode *dvp = ap->a_dvp;
   struct vnode **vpp = ap->a_vpp;
   struct componentname *cnp = ap->a_cnp;
   struct vattr *vap = ap->a_vap;

   return HgfsMkdirInt(dvp, vpp, cnp, vap->va_mode);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopRmdir --
 *
 *      Removes the specified name from the provided vnode by calling
 *      HgfsRmdirInt.
 *
 * Results:
 *      Returns 0 on success and an error code on error.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopRmdir(struct vop_rmdir_args *ap)
/*
struct vop_rmdir_args {
   struct vnode *dvp;           // IN: parent directory vnode
   struct vnode *vp;            // IN: directory to remove
   struct componentname *cnp;   // IN: pathname information
};
*/
{
   struct vnode *dvp = ap->a_dvp;
   struct vnode *vp = ap->a_vp;
   struct componentname *cnp = ap->a_cnp;

   return HgfsRmdirInt(dvp, vp, cnp);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopReaddir --
 *
 *      Reads as many entries from the directory as will fit in to the provided
 *      buffer.  Each directory entry is read by calling HgfsGetNextDirEntry().
 *
 *      The funciton simply calls HgfsReaddirInt to do all of the common
 *      FreeBSD and Solaris work.
 *
 * Results:
 *      Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopReaddir(struct vop_readdir_args *ap)
/*
struct vop_readdir_args {
   struct vnode *vp;    // IN   : directory to read from
   struct uio *uio;     // INOUT: where to read contents
   struct ucred *cred;  // IN   : caller's credentials
   int *eofflag;        // INOUT: end of file status
   int *ncookies;       // OUT  : used by NFS server only; ignored
   u_long **cookies;    // INOUT: used by NFS server only; ignored
};
*/
{
   struct vnode *vp = ap->a_vp;
   struct uio *uiop = ap->a_uio;
   int *eofp = ap->a_eofflag;

   return HgfsReaddirInt(vp, uiop, eofp);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopInactive --
 *
 *      Called when vnode's use count reaches zero.
 *
 * Results:
 *      Unconditionally zero.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopInactive(struct vop_inactive_args *ap)
/*
struct vop_inactive_args {
   struct vnode *vp;    // IN: vnode to inactive
   struct thread *td;   // IN: caller's thread context
};
*/
{
   /*
    * On FreeBSD we can always call vgone because there is no possibility that the
    * vnode is being shared by more than one open file.
    */
   vgone(ap->a_vp);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopReclaim --
 *
 *      Dissociates vnode from the underlying filesystem.
 *
 * Results:
 *      Zero on success, or an appropriate system error otherwise.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopReclaim(struct vop_reclaim_args *ap)
/*
struct vop_reclaim_args {
   struct vnode *vp;    // IN: vnode to reclaim
   struct thread *td;   // IN: caller's thread context
};
*/
{
   struct vnode *vp = ap->a_vp;

   HgfsSuperInfo *sip = HGFS_VP_TO_SIP(vp);

   HgfsVnodePut(vp, &sip->fileHashTable);
   vp->v_data = NULL;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsVopPrint --
 *
 *      This function is needed to fill in the HgfsVnodeOps structure.
 *      Right now it does nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsVopPrint(struct vop_print_args *ap)
{
   return 0;
}

