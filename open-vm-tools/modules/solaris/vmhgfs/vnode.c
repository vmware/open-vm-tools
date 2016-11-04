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
 * vnode.c --
 *
 * Implementaiton of entry points for operations on files (vnodes) and
 * definition of the vnodeops structure.
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

#include "hgfsSolaris.h"
#include "hgfsState.h"
#include "hgfsBdGlue.h"
#include "vnode.h"
#include "filesystem.h"
#include "request.h"
#include "debug.h"

#include "hgfsEscape.h"         /* Escaping/unescaping buffers */
#include "cpName.h"             /* Cross-platform name conversion */
#include "hgfsUtil.h"           /* Cross-platform time conversion */
#include "sha1.h"               /* SHA-1 for Node ID calculation */

/*
 * Macros
 */
#define HGFS_ATTR_MODE_SHIFT    6

/* Sets the values of request headers properly */
#define HGFS_INIT_REQUEST_HDR(request, req, _op)        \
         do {                                           \
            request->header.id = req->id;               \
            request->header.op = _op;                   \
         } while(0)

/* Solaris times support nsecs, so only use these functions directly */
#define HGFS_SET_TIME(unixtm, nttime)                   \
         HgfsConvertFromNtTimeNsec(&unixtm, nttime)
#define HGFS_GET_TIME(unixtm)                           \
         HgfsConvertTimeSpecToNtTime(&unixtm)

/* Determine if this is the root vnode. */
#define HGFS_IS_ROOT_VNODE(sip, vp)                     \
                (sip->rootVnode == vp)
//             ((vp->v_flag & VROOT) && (vp->v_vfsp == sip->vfsp))


/*
 * Prototypes
 */

/* Local vnode functions */
static int HgfsDirOpen(HgfsSuperInfo *sip, struct vnode *vp);
static int HgfsFileOpen(HgfsSuperInfo *sip, struct vnode *vp,
                        int flag, int permissions);
static int HgfsDirClose(HgfsSuperInfo *sip, struct vnode *vp);
static int HgfsFileClose(HgfsSuperInfo *sip, struct vnode *vp);
static int HgfsGetNextDirEntry(HgfsSuperInfo *sip, HgfsHandle handle,
                               uint32_t offset, char *nameOut, Bool *done);
static int HgfsDoRead(HgfsSuperInfo *sip, HgfsHandle handle, uint64_t offset,
                      uint32_t size, uio_t *uiop, uint32_t *count);
static int HgfsDoWrite(HgfsSuperInfo *sip, HgfsHandle handle, int ioflag,
                       uint64_t offset, uint32_t size, uio_t *uiop,
                       uint32_t *count);
static int HgfsDelete(HgfsSuperInfo *sip, char *filename, HgfsOp op);

static int HgfsSubmitRequest(HgfsSuperInfo *sip, HgfsReq *req);
static int HgfsValidateReply(HgfsReq *req, uint32_t minSize);
static int HgfsStatusConvertToSolaris(HgfsStatus hgfsStatus);
static void HgfsAttrToSolaris(struct vnode *vp, const HgfsAttr *hgfsAttr,
                              struct vattr *solAttr);
static Bool HgfsSetattrCopy(struct vattr *solAttr, int flags,
                            HgfsAttr *hgfsAttr, HgfsAttrChanges *update);
static int HgfsMakeFullName(const char *path, uint32_t pathLen, const char *file,
                            char *outBuf, ssize_t bufSize);
static int HgfsGetOpenMode(uint32 flags);
static int HgfsGetOpenFlags(uint32 flags);
INLINE static void HgfsDisableSignals(k_sigset_t *oldIgnoreSet);
INLINE static void HgfsRestoreSignals(k_sigset_t *oldIgnoreSet);


/* vnode Operation prototypes */
#if HGFS_VFS_VERSION <= 3
static int HgfsOpen(struct vnode **vpp, int flag, struct cred *cr);
static int HgfsClose(struct vnode *vp, int flag, int count, offset_t offset,
                     struct cred *cr);
#else
static int HgfsOpen(struct vnode **vpp, int flag, struct cred *cr, caller_context_t *ctx);
static int HgfsClose(struct vnode *vp, int flag, int count, offset_t offset,
                     struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static int HgfsRead(struct vnode *vp, struct uio *uiop, int ioflag,
                    struct cred *cr);
static int HgfsWrite(struct vnode *vp, struct uio *uiop, int ioflag,
                     struct cred *cr);
#else
static int HgfsRead(struct vnode *vp, struct uio *uiop, int ioflag,
                    struct cred *cr, caller_context_t *ctx);
static int HgfsWrite(struct vnode *vp, struct uio *uiop, int ioflag,
                     struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION <= 3
static int HgfsIoctl(struct vnode *vp, int cmd, intptr_t arg, int flag,
                     struct cred *cr, int *rvalp);
static int HgfsGetattr(struct vnode *vp, struct vattr *vap, int flags,
                       struct cred *cr);
#else
static int HgfsIoctl(struct vnode *vp, int cmd, intptr_t arg, int flag,
                     struct cred *cr, int *rvalp, caller_context_t *ctx);
static int HgfsGetattr(struct vnode *vp, struct vattr *vap, int flags,
                       struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static int HgfsSetattr(struct vnode *vp, struct vattr *vap, int flags,
                       struct cred *cr);
#else
static int HgfsSetattr(struct vnode *vp, struct vattr *vap, int flags,
                       struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION <= 3
static int HgfsAccess(struct vnode *vp, int mode, int flags,
                      struct cred *cr);
static int HgfsLookup(struct vnode *dvp, char *nm, struct vnode **vpp,
                      struct pathname *pnp, int flags,
                      struct vnode *rdir, struct cred *cr);
static int HgfsCreate(struct vnode *dvp, char *name, struct vattr *vap,
                      vcexcl_t excl, int mode, struct vnode **vpp,
                      struct cred *cr, int flag);
static int HgfsRemove(struct vnode *vp, char *nm, struct cred *cr);
static int HgfsLink(struct vnode *tdvp, struct vnode *svp, char *tnm,
                    struct cred *cr);
static int HgfsRename(struct vnode *sdvp, char *snm, struct vnode *tdvp,
                      char *tnm, struct cred *cr);
static int HgfsMkdir(struct vnode *dvp, char *dirname, struct vattr *vap,
                     struct vnode **vpp, struct cred *cr);
static int HgfsRmdir(struct vnode *vp, char *nm, struct vnode *cdir,
                     struct cred *cr);
static int HgfsReaddir(struct vnode *vp, struct uio *uiop, struct cred *cr,
                       int *eofp);
static int HgfsSymlink(struct vnode *dvp, char *linkname, struct vattr *vap,
                       char *target, struct cred *cr);
static int HgfsReadlink(struct vnode *vp, struct uio *uiop, struct cred *cr);
static int HgfsFsync(struct vnode *vp, int syncflag, struct cred *cr);
static void HgfsInactive(struct vnode *vp, struct cred *cr);
static int HgfsFid(struct vnode *vp, struct fid *fidp);
#else
static int HgfsAccess(struct vnode *vp, int mode, int flags,
                      struct cred *cr, caller_context_t *ctx);
static int HgfsLookup(struct vnode *dvp, char *nm, struct vnode **vpp,
                      struct pathname *pnp, int flags,
                      struct vnode *rdir, struct cred *cr, caller_context_t *ctx,
                      int *direntflags, pathname_t *realpnp);
static int HgfsCreate(struct vnode *dvp, char *name, struct vattr *vap,
                      vcexcl_t excl, int mode, struct vnode **vpp,
                      struct cred *cr, int flag, caller_context_t *ctx,
                      vsecattr_t *vsecp);
static int HgfsRemove(struct vnode *vp, char *nm, struct cred *cr,
                      caller_context_t *ctx, int flags);
static int HgfsLink(struct vnode *tdvp, struct vnode *svp, char *tnm,
                    struct cred *cr, caller_context_t *ctx, int flags);
static int HgfsRename(struct vnode *sdvp, char *snm, struct vnode *tdvp,
                      char *tnm, struct cred *cr, caller_context_t *ctx, int flags);
static int HgfsMkdir(struct vnode *dvp, char *dirname, struct vattr *vap,
                     struct vnode **vpp, struct cred *cr, caller_context_t *ctx,
                     int flags, vsecattr_t *vsecp);
static int HgfsRmdir(struct vnode *vp, char *nm, struct vnode *cdir,
                     struct cred *cr, caller_context_t *ctx, int flags);
static int HgfsReaddir(struct vnode *vp, struct uio *uiop, struct cred *cr,
                       int *eofp, caller_context_t *ctx, int flags);
static int HgfsSymlink(struct vnode *dvp, char *linkname, struct vattr *vap,
                       char *target, struct cred *cr, caller_context_t *ctx,
                       int flags);
static int HgfsReadlink(struct vnode *vp, struct uio *uiop, struct cred *cr,
                        caller_context_t *ctx);
static int HgfsFsync(struct vnode *vp, int syncflag, struct cred *cr,
                     caller_context_t *ctx);
static void HgfsInactive(struct vnode *vp, struct cred *cr, caller_context_t *ctx);
static int HgfsFid(struct vnode *vp, struct fid *fidp, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static void HgfsRwlock(struct vnode *vp, int write_lock);
#elif HGFS_VFS_VERSION == 3
static void HgfsRwlock(struct vnode *vp, int write_lock, caller_context_t *ctx);
#else
static int HgfsRwlock(struct vnode *vp, int write_lock, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static void HgfsRwunlock(struct vnode *vp, int write_lock);
#else
static void HgfsRwunlock(struct vnode *vp, int write_lock, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION <= 3
static int HgfsSeek(struct vnode *vp, offset_t ooff, offset_t *noffp);
static int HgfsCmp(struct vnode *vp1, struct vnode *vp2);
static int HgfsFrlock(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
                      offset_t offset, struct flk_callback *, struct cred *cr);
#else
static int HgfsSeek(struct vnode *vp, offset_t ooff, offset_t *noffp,
                    caller_context_t *ctx);
static int HgfsCmp(struct vnode *vp1, struct vnode *vp2, caller_context_t *ctx);
static int HgfsFrlock(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
                      offset_t offset, struct flk_callback *, struct cred *cr,
                      caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static int HgfsSpace(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
                     offset_t offset, struct cred *cr);
#else
static int HgfsSpace(struct vnode *vp, int cmd, struct flock64 *bfp, int flag,
                     offset_t offset, struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION <= 3
static int HgfsRealvp(struct vnode *vp, struct vnode **vpp);
static int HgfsGetpage(struct vnode *vp, offset_t off, size_t len, uint_t *protp,
                       struct page **plarr, size_t plsz, struct seg *seg,
                       caddr_t addr, enum seg_rw rw, struct cred *cr);
static int HgfsPutpage(struct vnode *vp, offset_t off, size_t len, int flags,
                       struct cred *cr);
static int HgfsMap(struct vnode *vp, offset_t off, struct as *as,
                   caddr_t *addrp, size_t len, uchar_t prot,
                   uchar_t maxprot, uint_t flags, struct cred *cr);
static int HgfsAddmap(struct vnode *vp, offset_t off, struct as *as,
                      caddr_t addr, size_t len, uchar_t prot,
                      uchar_t maxprot, uint_t flags, struct cred *cr);
static int HgfsDelmap(struct vnode *vp, offset_t off, struct as *as,
                      caddr_t addr, size_t len, uint_t prot,
                      uint_t maxprot, uint_t flags, struct cred *cr);
static int HgfsDump(struct vnode *vp, caddr_t addr, int lbdn, int dblks);
static int HgfsPathconf(struct vnode *vp, int cmd, ulong_t *valp,
                        struct cred *cr);
static int HgfsPageio(struct vnode *vp, struct page *pp, u_offset_t io_off,
                      size_t io_len, int flags, struct cred *cr);
static int HgfsDumpctl(struct vnode *vp, int action, int *blkp);
static void HgfsDispose(struct vnode *vp, struct page *pp, int flag, int dn,
                        struct cred *cr);
static int HgfsSetsecattr(struct vnode *vp, vsecattr_t *vsap, int flag,
                          struct cred *cr);
#else
static int HgfsRealvp(struct vnode *vp, struct vnode **vpp, caller_context_t *ctx);
static int HgfsGetpage(struct vnode *vp, offset_t off, size_t len, uint_t *protp,
                       struct page **plarr, size_t plsz, struct seg *seg,
                       caddr_t addr, enum seg_rw rw, struct cred *cr,
                       caller_context_t *ctx);
static int HgfsPutpage(struct vnode *vp, offset_t off, size_t len, int flags,
                       struct cred *cr, caller_context_t *ctx);
static int HgfsMap(struct vnode *vp, offset_t off, struct as *as,
                   caddr_t *addrp, size_t len, uchar_t prot,
                   uchar_t maxprot, uint_t flags, struct cred *cr, caller_context_t *ctx);
static int HgfsAddmap(struct vnode *vp, offset_t off, struct as *as,
                      caddr_t addr, size_t len, uchar_t prot,
                      uchar_t maxprot, uint_t flags, struct cred *cr,
                      caller_context_t *ctx);
static int HgfsDelmap(struct vnode *vp, offset_t off, struct as *as,
                      caddr_t addr, size_t len, uint_t prot,
                      uint_t maxprot, uint_t flags, struct cred *cr,
                      caller_context_t *ctx);
static int HgfsDump(struct vnode *vp, caddr_t addr, offset_t lbdn, offset_t dblks,
                    caller_context_t *ctx);
static int HgfsPathconf(struct vnode *vp, int cmd, ulong_t *valp,
                        struct cred *cr, caller_context_t *ctx);
static int HgfsPageio(struct vnode *vp, struct page *pp, u_offset_t io_off,
                      size_t io_len, int flags, struct cred *cr, caller_context_t *ctx);
static int HgfsDumpctl(struct vnode *vp, int action, offset_t *blkp,
                       caller_context_t *ctx);
static void HgfsDispose(struct vnode *vp, struct page *pp, int flag, int dn,
                        struct cred *cr, caller_context_t *ctx);
static int HgfsSetsecattr(struct vnode *vp, vsecattr_t *vsap, int flag,
                          struct cred *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 2
static int HgfsShrlock(struct vnode *vp, int cmd, struct shrlock *chr,
                       int flag);
#elif HGFS_VFS_VERSION == 3
static int HgfsShrlock(struct vnode *vp, int cmd, struct shrlock *chr,
                       int flag, cred_t *cr);
#else /* SOL11 */
static int HgfsShrlock(struct vnode *vp, int cmd, struct shrlock *chr,
                       int flag, cred_t *cr, caller_context_t *ctx);
#endif

#if HGFS_VFS_VERSION == 3
static int HgfsVnevent(struct vnode *vp, vnevent_t event);
#elif HGFS_VFS_VERSION == 5
static int HgfsVnevent(struct vnode *vp, vnevent_t event, vnode_t *dvp,
                       char *fnm, caller_context_t *ctx);
#endif


#if HGFS_VFS_VERSION == 2
/* vnode Operations Structure */
struct vnodeops hgfsVnodeOps = {
   HgfsOpen,            /* vop_open() */
   HgfsClose,           /* vop_close() */
   HgfsRead,            /* vop_read() */
   HgfsWrite,           /* vop_write() */
   HgfsIoctl,           /* vop_ioctl() */
   fs_setfl,            /* vop_setfl() */
   HgfsGetattr,         /* vop_getattr() */
   HgfsSetattr,         /* vop_setattr() */
   HgfsAccess,          /* vop_access() */
   HgfsLookup,          /* vop_lookup() */
   HgfsCreate,          /* vop_create() */
   HgfsRemove,          /* vop_remove() */
   HgfsLink,            /* vop_link() */
   HgfsRename,          /* vop_rename() */
   HgfsMkdir,           /* vop_mkdir() */
   HgfsRmdir,           /* vop_rmdir() */
   HgfsReaddir,         /* vop_readdir() */
   HgfsSymlink,         /* vop_symlink() */
   HgfsReadlink,        /* vop_readlink() */
   HgfsFsync,           /* vop_fsync() */
   HgfsInactive,        /* vop_inactive() */
   HgfsFid,             /* vop_fid() */
   HgfsRwlock,          /* vop_rwlock() */
   HgfsRwunlock,        /* vop_rwunlock() */
   HgfsSeek,            /* vop_seek() */
   HgfsCmp,             /* vop_cmp() */
   HgfsFrlock,          /* vop_frlock() */
   HgfsSpace,           /* vop_space() */
   HgfsRealvp,          /* vop_realvp() */
   HgfsGetpage,         /* vop_getpage() */
   HgfsPutpage,         /* vop_putpage() */
   HgfsMap,             /* vop_map() */
   HgfsAddmap,          /* vop_addmap() */
   HgfsDelmap,          /* vop_delmap() */
   fs_poll,             /* vop_poll() */
   HgfsDump,            /* vop_dump() */
   HgfsPathconf,        /* vop_pathconf() */
   HgfsPageio,          /* vop_pageio() */
   HgfsDumpctl,         /* vop_dumpctl() */
   HgfsDispose,         /* vop_dispose() */
   HgfsSetsecattr,      /* vop_setsecattr() */
   fs_fab_acl,          /* vop_getsecattr() */
   HgfsShrlock          /* vop_shrlock() */
};

#else

/* Will be set up during HGFS initialization (see HgfsInit) */
static struct vnodeops *hgfsVnodeOpsP;

#endif

static HgfsSuperInfo hgfsSuperInfo;


/*
 * Vnode Entry Points
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsOpen --
 *
 *    Invoked when open(2) is called on a file in our filesystem.  Sends an
 *    OPEN request to the Hgfs server with the filename of this vnode.
 *
 *    "Opens a file referenced by the supplied vnode.  The open() system call
 *    has already done a vop_lookup() on the path name, which returned a vnode
 *    pointer and then calls to vop_open().  This function typically does very
 *    little since most of the real work was performed by vop_lookup()."
 *    (Solaris Internals, p537)
 *
 * Results:
 *    Returns 0 on success and an error code on error.
 *
 * Side effects:
 *    The HgfsOpenFile for this file is given a handle that can be used on
 *    future read and write requests.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsOpen(struct vnode **vpp,    // IN: Vnode for file to open
         int flag,              // IN: Open flags
         struct cred *cr)       // IN: Credentials of caller
#else
static int
HgfsOpen(struct vnode **vpp,    // IN: Vnode for file to open
         int flag,              // IN: Open flags
         struct cred *cr,       // IN: Credentials of caller
         caller_context_t *ctx) // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;

   if (!vpp || !cr) {
      cmn_err(HGFS_ERROR, "HgfsOpen: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsOpen().\n");


   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   /* Make sure we know the filename. */
   ASSERT(HGFS_KNOW_FILENAME(*vpp));

   /*
    * Make sure the handle is not already set.  If it is, this means the file
    * has already been opened so we'll need to create a new vnode since we keep
    * a vnode for each open instance of a file.  This ensures that the handle
    * we'll create now won't clobber the other one's open-file state.
    */
   if (HgfsHandleIsSet(*vpp)) {
      int ret;
      struct vnode *origVp = *vpp;

      ret = HgfsVnodeDup(vpp, origVp, sip, &sip->fileHashTable);
      if (ret) {
         return EIO;
      }
   }

   switch((*vpp)->v_type) {
   case VDIR:
      DEBUG(VM_DEBUG_COMM, "HgfsOpen: opening a directory\n");
      return HgfsDirOpen(sip, *vpp);

   case VREG:
      {
         HgfsMode mode = 0;

         /*
          * If HgfsCreate() was called prior to this, this fills in the mode we
          * saved there.  It's okay if this fails since often HgfsCreate()
          * won't have been called.
          */
         HgfsGetOpenFileMode(*vpp, &mode);

         DEBUG(VM_DEBUG_COMM, "HgfsOpen: opening a file with flag %x\n", flag);
         return HgfsFileOpen(sip, *vpp, flag, mode);
      }

   default:
      DEBUG(VM_DEBUG_FAIL,
            "HgfsOpen: unrecognized file of type %d.\n", (*vpp)->v_type);
      return EINVAL;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsClose --
 *
 *    Invoked when a user calls close(2) on a file in our filesystem.  Sends
 *    a CLOSE request to the Hgfs server with the filename of this vnode.
 *
 *    "Closes the file given by the supplied vnode.  When this is the last
 *    close, some filesystems use vop_close() to initiate a writeback of
 *    outstanding dirty pages by checking the reference cound in the vnode."
 *    (Solaris Internals, p536)
 *
 * Results:
 *    Returns 0 on success and an error code on error.
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsClose(struct vnode *vp,     // IN: Vnode of file that is being closed
          int flag,             // IN: Flags file was opened with
          int count,            // IN: Reference count on this vnode
          offset_t offset,      // IN: XXX
          struct cred *cr)      // IN: Credentials of caller
#else
static int
HgfsClose(struct vnode *vp,        // IN: Vnode of file that is being closed
          int flag,                // IN: Flags file was opened with
          int count,               // IN: Reference count on this vnode
          offset_t offset,         // IN: XXX
          struct cred *cr,         // IN: Credentials of caller
          caller_context_t *ctx)   // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;

   if (!vp) {
      cmn_err(HGFS_ERROR, "HgfsClose: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsClose(). (vp=%p)\n", vp);
   DEBUG(VM_DEBUG_INFO, "HgfsClose: flag=%x, count=%x, offset=%lld\n",
         flag, count, offset);

   /*
    * Solaris calls this function with a count greater than one at certain
    * times.  We only want to actually close it on the last close.
    */
   if (count > 1) {
      return 0;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   if ( !HGFS_KNOW_FILENAME(vp) ) {
      DEBUG(VM_DEBUG_FAIL, "HgfsClose: we don't know the filename of:\n");
      HgfsDebugPrintVnode(VM_DEBUG_STRUCT, "HgfsClose", vp, TRUE);
      return EINVAL;
   }

   /*
    * If we are closing a directory we need to send a SEARCH_CLOSE request,
    * but if we are closing a regular file we need to send a CLOSE request.
    * Other file types are not supported by the Hgfs protocol.
    */

   switch (vp->v_type) {
   case VDIR:
      return HgfsDirClose(sip, vp);

   case VREG:
      return HgfsFileClose(sip, vp);

   default:
      DEBUG(VM_DEBUG_FAIL, "HgfsClose: unsupported filetype %d.\n", vp->v_type);
      return EINVAL;
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRead --
 *
 *    Invoked when a user calls read(2) on a file in our filesystem.
 *
 *    We call HgfsDoRead() to fill the user's buffer until the request is met
 *    or the file has no more data.  This is done since we can only transfer
 *    HGFS_IO_MAX bytes in any one request.
 *
 *    "Reads the range supplied for the given vnode.  vop_read() typically
 *    maps the requested range of a file into kernel memory and then uses
 *    vop_getpage() to do the real work." (Solaris Internals, p537)
 *
 * Results:
 *    Returns zero on success and an error code on failure.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static int
HgfsRead(struct vnode *vp,      // IN: Vnode of file to read
         struct uio *uiop,      // IN: User's read request
         int ioflag,            // IN: XXX
         struct cred *cr)       // IN: Credentials of caller
#else
static int
HgfsRead(struct vnode *vp,              // IN: Vnode of file to read
         struct uio *uiop,              // IN: User's read request
         int ioflag,                    // IN: XXX
         struct cred *cr,               // IN: Credentials of caller
         caller_context_t *ctx)         // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;
   HgfsHandle handle;
   uint64_t offset;
   int ret;

   if (!vp || !uiop) {
      cmn_err(HGFS_ERROR, "HgfsRead: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsRead: entry.\n");

   /* We can't read from directories, that's what readdir() is for. */
   if (vp->v_type == VDIR) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRead: cannot read directories.\n");
      return EISDIR;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   /* This is where the user wants to start reading from in the file. */
   offset = uiop->uio_loffset;

   /*
    * We need to get the handle for the requests sent to the Hgfs server.  Note
    * that this is guaranteed to not change until a close(2) is called on this
    * vnode, so it's safe and correct to acquire it outside the loop below.
    */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRead: could not get handle.\n");
      return EINVAL;
   }

   /*
    * Here we loop around HgfsDoRead with requests less than or equal to
    * HGFS_IO_MAX until one of the following conditions is met:
    *  (1) All the requested data has been read
    *  (2) The file has no more data
    *  (3) An error occurred
    *
    * Since HgfsDoRead() calls uiomove(9F), we know condition (1) is met when
    * the uio structure's uio_resid is decremented to zero.  If HgfsDoRead()
    * returns 0 we know condition (2) was met, and if it returns less than 0 we
    * know condtion (3) was met.
    */
   do {
      uint32_t size;
      uint32_t count;

      DEBUG(VM_DEBUG_INFO, "%s: offset=%"FMT64"d, uio_loffset=%lld\n",
            __func__, offset, uiop->uio_loffset);
      DEBUG(VM_DEBUG_HANDLE, "%s: ** handle=%d, file=%s\n",
            __func__, handle, HGFS_VP_TO_FILENAME(vp));

      /* Request at most HGFS_IO_MAX bytes */
      size = (uiop->uio_resid > HGFS_IO_MAX) ? HGFS_IO_MAX : uiop->uio_resid;

      /* Send one read request. */
      ret = HgfsDoRead(sip, handle, offset, size, uiop, &count);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "%s: HgfsDoRead() failed.\n", __func__);
         return ret;
      }

      if (count == 0) {
         /* On end of file we return success */
         DEBUG(VM_DEBUG_DONE, "%s: end of file reached.\n", __func__);
         return 0;
      }

      /* Bump the offset past where we have already read. */
      offset += count;
   } while (uiop->uio_resid);

   /* We fulfilled the user's read request, so return success. */
   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsWrite --
 *
 *    This is invoked when a user calls write(2) on a file in our filesystem.
 *
 *    We call HgfsDoWrite() once with requests less than or equal to
 *    HGFS_IO_MAX bytes until the user's write request has completed.
 *
 *    "Writes the range supplied for the given vnode.  The write system call
 *    typically maps the requested range of a file into kernel memory and then
 *    uses vop_putpage() to do the real work." (Solaris Internals, p538)
 *
 * Results:
 *    Returns 0 on success and error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION < 3
static int
HgfsWrite(struct vnode *vp,     // IN: Vnode of file to write to
          struct uio *uiop,     // IN: User's write request
          int ioflag,           // IN: XXX
          struct cred *cr)      // IN: Credentials of caller
#else
static int
HgfsWrite(struct vnode *vp,             // IN: Vnode of file to write to
          struct uio *uiop,             // IN: User's write request
          int ioflag,                   // IN: XXX
          struct cred *cr,              // IN: Credentials of caller
          caller_context_t *ctx)        // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;
   HgfsHandle handle;
   uint64_t offset;
   int ret;

   if (!vp || !uiop) {
      cmn_err(HGFS_ERROR, "HgfsWrite: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsWrite: entry. (vp=%p)\n", vp);
   DEBUG(VM_DEBUG_INFO, "HgfsWrite: ***ioflag=%x, uio_resid=%ld\n",
         ioflag, uiop->uio_resid);

   /* Skip write requests for 0 bytes. */
   if (uiop->uio_resid == 0) {
      DEBUG(VM_DEBUG_INFO, "HgfsWrite: write of 0 bytes requested.\n");
      return 0;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   DEBUG(VM_DEBUG_INFO, "HgfsWrite: file is %s\n", HGFS_VP_TO_FILENAME(vp));

   /* This is where the user will begin writing into the file. */
   offset = uiop->uio_loffset;

   /* Get the handle we need to supply the Hgfs server. */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsWrite: could not get handle.\n");
      return EINVAL;
   }

   /*
    * We loop around calls to HgfsDoWrite() until either (1) we have written all
    * of our data or (2) an error has occurred.  uiop->uio_resid is decremented
    * by uiomove(9F) inside HgfsDoWrite(), so condition (1) is met when it
    * reaches zero.  Condition (2) occurs when HgfsDoWrite() returns less than
    * zero.
    */
   do {
      uint32_t size;
      uint32_t count;

      DEBUG(VM_DEBUG_INFO, "HgfsWrite: ** offset=%"FMT64"d, uio_loffset=%lld\n",
            offset, uiop->uio_loffset);
      DEBUG(VM_DEBUG_HANDLE, "HgfsWrite: ** handle=%d, file=%s\n",
            handle, HGFS_VP_TO_FILENAME(vp));

      /* Write at most HGFS_IO_MAX bytes. */
      size = (uiop->uio_resid > HGFS_IO_MAX) ? HGFS_IO_MAX : uiop->uio_resid;

      /* Send one write request. */
      ret = HgfsDoWrite(sip, handle, ioflag, offset, size, uiop, &count);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "%s: HgfsDoRead() failed.\n", __func__);
         return ret;
      }

      /* Increment the offest by the amount already written. */
      offset += count;
   } while (uiop->uio_resid);

   /* We have completed the user's write request, so return success. */
   DEBUG(VM_DEBUG_DONE, "HgfsWrite: done.\n");
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsIoctl --
 *
 *    Invoked when a user calls ioctl(2) on a file in our filesystem.
 *    Performs a specified operation on the file.
 *
 * Results:
 *    ENOTSUP
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsIoctl(struct vnode *vp,     // IN: Vnode of file to operate on
          int cmd,              // IN: Requested command from user
          intptr_t arg,         // IN: Arguments for command
          int flag,             // IN: XXX
          struct cred *cr,      // IN: Credentials of caller
          int *rvalp)           //XXX
#else
static int
HgfsIoctl(struct vnode *vp,        // IN: Vnode of file to operate on
          int cmd,                 // IN: Requested command from user
          intptr_t arg,            // IN: Arguments for command
          int flag,                // IN: XXX
          struct cred *cr,         // IN: Credentials of caller
          int *rvalp,              //XXX
          caller_context_t *ctx)   // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsIoctl() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetfl --
 *
 *    "Sets file locks on the supplied vnode." (Solaris Internals, p538)
 *
 *    Use fs_setfl from <sys/fs_subr.h>?  Do we need this?
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */
/*
int
HgfsSetfl(struct vnode *vp,
          int oflags,
          int nflags,
          struct cred *cr)
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsSetfl() NOTSUP.\n");

   return ENOTSUP;
}*/


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetattr --
 *
 *    "Gets the attributes for the supplied vnode." (Solaris Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsGetattr(struct vnode *vp,   // IN: Vnode of file to get attributes for
            struct vattr *vap,  // OUT: Filled in with attributes of file
            int flags,          // IN: XXX
            struct cred *cr)    // IN: Credentials of caller
#else
static int
HgfsGetattr(struct vnode *vp,      // IN: Vnode of file to get attributes for
            struct vattr *vap,     // OUT: Filled in with attributes of file
            int flags,             // IN: XXX
            struct cred *cr,       // IN: Credentials of caller
            caller_context_t *ctx) // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;
   HgfsReq *req;
   HgfsRequestGetattr *request;
   HgfsReplyGetattr *reply;
   int ret;

   if (!vp || !vap) {
      cmn_err(HGFS_ERROR, "HgfsGetattr: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsGetattr().\n");

   /*
    * Here we should send a Getattr request then examine vap->va_mask to
    * retun the values the user asked for.  HgfsAttrToSolaris() handles filling
    * in the Solaris structure with the correct values based on the Hgfs type.
    */

   sip = HgfsGetSuperInfo();
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetattr() couldn't get superinfo.\n");
      return EIO;
   }

   ASSERT(HGFS_KNOW_FILENAME(vp));

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   request = (HgfsRequestGetattr *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_GETATTR);

   /*
    * Now we need to convert the filename to cross-platform and unescaped
    * format.
    */
   ret = CPName_ConvertTo(HGFS_VP_TO_FILENAME(vp), MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetattr: CPName_ConvertTo failed.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request's state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->fileName.length = ret;

   req->packetSize = sizeof *request + request->fileName.length;

   /*
    * Now submit request and wait for reply.  The request's state will be
    * properly set to COMPLETED, ERROR, or ABANDONED after calling
    * HgfsSubmitRequest()
    */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplyGetattr *)req->packet;

   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetattr: reply not valid.\n");
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
   } else {
      /* Make sure we got all of the attributes */
      if (req->packetSize != sizeof *reply) {
         DEBUG(VM_DEBUG_FAIL, "%s: packet too small.\n", __func__);
         ret = EIO;
      } else {

         DEBUG(VM_DEBUG_COMM, "%s: received reply for ID %d\n",
               __func__, reply->header.id);
         DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n",
               reply->header.status);
         DEBUG(VM_DEBUG_COMM, " file type: %d\n", reply->attr.type);
         DEBUG(VM_DEBUG_COMM, " file size: %"FMT64"u\n", reply->attr.size);
         DEBUG(VM_DEBUG_COMM, " permissions: %o\n", reply->attr.permissions);
         DEBUG(VM_DEBUG_COMM, "%s: filename %s\n", __func__, HGFS_VP_TO_FILENAME(vp));

         /* Map the Hgfs attributes into the Solaris attributes */
         HgfsAttrToSolaris(vp, &reply->attr, vap);

         DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);
      }
   }

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetattr --
 *
 *    Maps the Solaris attributes to Hgfs attributes (by calling
 *    HgfsSetattrCopy()) and sends a set attribute request to the Hgfs server.
 *
 *    "Sets the attributes for the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *    The file on the host will have new attributes.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static int
HgfsSetattr(struct vnode *vp,   // IN: Vnode of file to get attributes for
            struct vattr *vap,  // IN: New attributes for the file
            int flags,          // IN: Flags for this call (from sys/vnode.h)
            struct cred *cr)    // IN: Credentials of caller
#else
static int
HgfsSetattr(struct vnode *vp,       // IN: Vnode of file to get attributes for
            struct vattr *vap,      // IN: New attributes for the file
            int flags,              // IN: Flags for this call (from sys/vnode.h)
            struct cred *cr,        // IN: Credentials of caller
            caller_context_t *ctx)  // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;
   HgfsReq *req;
   HgfsRequestSetattr *request;
   HgfsReplySetattr *reply;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsSetattr().\n");

   if (!vp || !vap) {
      cmn_err(HGFS_ERROR, "HgfsSetattr: NULL input from Kernel.\n");
      return EINVAL;
   }

   if ( !HGFS_KNOW_FILENAME(vp) ) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsSetattr: we don't know filename to set attributes for.\n");
      return EINVAL;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   request = (HgfsRequestSetattr *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_SETATTR);

   /*
    * Fill the attributes and update fields of the request.  If no updates are
    * needed then we will just return success without sending the request.
    */
   if (HgfsSetattrCopy(vap, flags, &request->attr, &request->update) == FALSE) {
      DEBUG(VM_DEBUG_DONE, "HgfsSetattr: don't need to update attributes.\n");
      ret = 0;
      /* We need to set the request state to completed before destroying. */
      req->state = HGFS_REQ_COMPLETED;
      goto out;
   }

   /* Convert the filename to cross platform and escape its buffer. */
   ret = CPName_ConvertTo(HGFS_VP_TO_FILENAME(vp), MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsSetattr: CPName_ConvertTo failed.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->fileName.length = ret;

   /* The request's size includes the request and filename. */
   req->packetSize = sizeof *request + request->fileName.length;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplySetattr *)req->packet;

   if (HgfsValidateReply(req, sizeof *reply) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsSetattr: invalid reply received.\n");
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
   } else {
      DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);
   }

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAccess --
 *
 *    This function is invoked when the user calls access(2) on a file in our
 *    filesystem.  It checks to ensure the user has the specified type of
 *    access to the file.
 *
 *    We send a GET_ATTRIBUTE request by calling HgfsGetattr() to get the mode
 *    (permissions) for the provided vnode.
 *
 * Results:
 *    Returns 0 if access is allowed and a non-zero error code otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsAccess(struct vnode *vp,    // IN: Vnode of file to check access for
           int mode,            // IN: Mode of access
           int flags,           // IN: XXX
           struct cred *cr)     // IN: Credentials of caller
#else
static int
HgfsAccess(struct vnode *vp,       // IN: Vnode of file to check access for
           int mode,               // IN: Mode of access
           int flags,              // IN: XXX
           struct cred *cr,        // IN: Credentials of caller
           caller_context_t *ctx)  // IN: Context of caller
#endif
{
   int ret;
   struct vattr vap;

   if (!vp | !cr) {
      cmn_err(HGFS_ERROR, "HgfsAccess: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsAccess(). (vp=%p, mode=%o, flags=%x)\n",
         vp, mode, flags);

   /* We only care about the file's mode (permissions).  That is, not the owner */
   vap.va_mask = AT_MODE;

   /* Get the attributes for this file from the Hgfs server. */
#if HGFS_VFS_VERSION <= 3
   ret = HgfsGetattr(vp, &vap, flags, cr);
#else
   ret = HgfsGetattr(vp, &vap, flags, cr, NULL);
#endif
   if (ret) {
      return ret;
   }

   DEBUG(VM_DEBUG_INFO, "HgfsAccess: vp's mode: %o\n", vap.va_mode);

   /*
    * mode is the desired access from the caller, and is composed of S_IREAD,
    * S_IWRITE, and S_IEXEC from <sys/stat.h>.  Since the mode of the file is
    * guaranteed to only contain owner permissions (by the Hgfs server), we
    * don't need to shift any bits.
    */
   if ((mode & S_IREAD) && !(vap.va_mode & S_IREAD)) {
      DEBUG(VM_DEBUG_FAIL, "HgfsAccess: read access not allowed (%s).\n",
            HGFS_VP_TO_FILENAME(vp));
      return EPERM;
   }

   if ((mode & S_IWRITE) && !(vap.va_mode & S_IWRITE)) {
      DEBUG(VM_DEBUG_FAIL, "HgfsAccess: write access not allowed (%s).\n",
            HGFS_VP_TO_FILENAME(vp));
      return EPERM;
   }

   if ((mode & S_IEXEC) && !(vap.va_mode & S_IEXEC)) {
      DEBUG(VM_DEBUG_FAIL, "HgfsAccess: execute access not allowed (%s).\n",
            HGFS_VP_TO_FILENAME(vp));
      return EPERM;
   }

   /* Success */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsLookup --
 *
 *    Looks in the provided directory for the specified filename.  If we cannot
 *    determine the vnode locally (i.e, the vnode is not the root vnode of the
 *    filesystem or the provided dvp), we send a getattr request to the server
 *    and allocate a vnode and internal filesystem state for this file.
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

#if HGFS_VFS_VERSION <= 3
static int
HgfsLookup(struct vnode *dvp,   // IN:  Directory to look in
           char *nm,            // IN:  Name to lookup in directory
           struct vnode **vpp,  // OUT: Pointer to vnode representing found file
           struct pathname *pnp,// IN:  XXX
           int flags,           // IN:  XXX
           struct vnode *rdir,  // IN:  XXX
           struct cred *cr)     // IN:  Credentials of caller

#else
static int
HgfsLookup(struct vnode *dvp,      // IN:  Directory to look in
           char *nm,               // IN:  Name to lookup in directory
           struct vnode **vpp,     // OUT: Pointer to vnode representing found file
           struct pathname *pnp,   // IN:  XXX
           int flags,              // IN:  XXX
           struct vnode *rdir,     // IN:  XXX
           struct cred *cr,        // IN: Credentials of caller
           caller_context_t *ctx,  // IN: Context of caller
           int *direntflags,       // IN: XXX
           pathname_t *realpnp)    // IN: XXX
#endif
{
   int ret;
   HgfsReq *req;
   HgfsRequestGetattr *request;
   HgfsReplyGetattr *reply;
   HgfsSuperInfo *sip;
   char path[MAXPATHLEN + 1];   /* Temporary buffer for full path */

   if (!dvp || !nm || !vpp || !cr) {
      cmn_err(HGFS_ERROR, "HgfsLookup: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsLookup(). (nm=%s)\n", nm);
   //DEBUG(VM_DEBUG_ENTRY, "HgfsLookup: pnp=%x, rdir=%x\n", pnp, rdir);

   /* First ensure that we are looking in a directory. */
   if (dvp->v_type != VDIR) {
      return ENOTDIR;
   }

   DEBUG(VM_DEBUG_COMM, " looking up \"%s\"\n", nm);

   /*
    * Get pointer to the superinfo.  If the device is not attached,
    * hgfsInstance will not be valid and we immediately return an error.
    */
   sip = HgfsGetSuperInfo();
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsLookup: couldn't acquire superinfo "
                           "(hgfsInstance=%x).\n", hgfsInstance);
      return EIO;
   }

   /* Construct the full path for this lookup. */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),             // Path to this file
                          HGFS_VP_TO_FILENAME_LENGTH(dvp),      // Length of path
                          nm,                                   // File's name
                          path,                                 // Destination buffer
                          sizeof path);                         // Size of dest buffer
   if (ret < 0) {
      return EINVAL;
   }

   DEBUG(VM_DEBUG_LOAD, "HgfsLookup: full path is \"%s\"\n", path);

   /* See if the lookup is really for the root vnode. */
   if (strcmp(path, "/") == 0) {
      DEBUG(VM_DEBUG_INFO, "HgfsLookup: returning the root vnode.\n");
      *vpp = HGFS_ROOT_VNODE(sip);
      /*
       * Note that this is the only vnode we maintain a reference count on; all
       * others are per-open-file and should only be given to the Kernel once.
       */
      VN_HOLD(*vpp);
      return  0;
   }

   /*
    * Now that we know the full filename, we can check our hash table for this
    * file to prevent having to send a request to the Hgfs Server.  If we do
    * find this file in the hash table, this function will correctly create
    * a vnode and other per-open state for us.
    *
    * On an 'ls -l', this saves sending two requests for each file in the
    * directory.
    *
    * XXX
    * Note that this optimization leaves open the possibility that a file that
    * has been removed on the host will not be noticed as promptly by the
    * filesystem.  This shouldn't cause any problems, though, because as far
    * as we can tell this function is invoked internally by the kernel before
    * other operations.  That is, this function is called implicitly for path
    * traversal when user applications issue other system calls.  The operation
    * next performed on the vnode we create here should happen prior to
    * returning to the user application, so if that next operation fails
    * because the file has been deleted, the user won't see different behavior
    * than if this optimization was not included.  Nonetheless, the #if 1 below
    * is provided to make it easy to turn off.
    */
#if 1
   ret = HgfsFileNameToVnode(path, vpp, sip, sip->vfsp, &sip->fileHashTable);
   if (ret == 0) {
      /*
       * The filename was in our hash table and we successfully created new
       * per-open state for it.
       */
      DEBUG(VM_DEBUG_DONE, "HgfsLookup: created per-open state from filename.\n");
      return 0;
   }
#endif

   /*
    * We don't have any reference to this vnode, so we must send a get
    * attribute request to see if the file exists and create one.
    */
   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   /* Fill in the header of this request. */
   request = (HgfsRequestGetattr *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_GETATTR);

   /* Fill in the filename portion of the request. */
   ret = CPName_ConvertTo(path, MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsLookup: CPName_ConvertTo failed.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }
   request->fileName.length = ret;

   /* Packet size includes the request and its payload. */
   req->packetSize = request->fileName.length + sizeof *request;

   DEBUG(VM_DEBUG_COMM, "HgfsLookup: sending getattr request for ID %d\n",
         request->header.id);
   DEBUG(VM_DEBUG_COMM, " fileName.length: %d\n", request->fileName.length);
   DEBUG(VM_DEBUG_COMM, " fileName.name: \"%s\"\n", request->fileName.name);

   /*
    * Submit the request and wait for the reply.
    */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   /* The reply is in the request's packet */
   reply = (HgfsReplyGetattr *)req->packet;

   /* Validate the reply was COMPLETED and at least contains a header */
   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsLookup(): invalid reply received for ID %d "
            "with status %d.\n", reply->header.id, reply->header.status);
      ret = EPROTO;
      goto out;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsLookup: received reply for ID %d\n", reply->header.id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", reply->header.status);
   DEBUG(VM_DEBUG_COMM, " file type: %d\n", reply->attr.type);
   DEBUG(VM_DEBUG_COMM, " file size: %"FMT64"u\n", reply->attr.size);
   DEBUG(VM_DEBUG_COMM, " permissions: %o\n", reply->attr.permissions);

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed for [%s] with error %d.\n",
            __func__, nm, ret);
      goto out;
   }

   /* Ensure packet contains correct amount of data */
   if (req->packetSize != sizeof *reply) {
      DEBUG(VM_DEBUG_COMM, "%s: invalid packet size received for [%s].\n",
            __func__, nm);
      ret = EIO;
      goto out;
   }

   /*
    * We need to create a vnode for this found file to give back to the Kernel.
    * Note that v_vfsp of the filesystem's root vnode was set properly in
    * HgfsMount(), so that value (dvp->v_vfsp) propagates down to each vnode.
    *
    */
   ret = HgfsVnodeGet(vpp,                      // Location to write vnode's address
                      sip,                      // Superinfo
                      dvp->v_vfsp,              // VFS for our filesystem
                      path,                     // Full name of the file
                      reply->attr.type,         // Type of file
                      &sip->fileHashTable);     // File hash table

   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsLookup: couldn't create vnode for \"%s\".\n", path);
      ret = EIO;
      goto out;
   }

   /* HgfsVnodeGet guarantees this. */
   ASSERT(*vpp);

   DEBUG(VM_DEBUG_LOAD, "HgfsLookup: assigned vnode %p to %s\n", *vpp, path);

   ret = 0;     /* Return success */

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCreate --
 *
 *    This entry point is invoked when a user calls open(2) with the O_CREAT
 *    flag specified.  The kernel calls our open entry point (HgfsOpen()) after
 *    calling this function, so here all we do is consruct the vnode and
 *    save the filename and permission bits for the file to be created within
 *    our filesystem internal state.
 *
 *    "Creates the supplied pathname." (Solaris Internals, p536)
 *
 * Results:
 *    Returns zero on success and an appropriate error code on error.
 *
 * Side effects:
 *    If the file exists, the vnode is duplicated since they are kepy per-open.
 *    If the file doesn't exist, a vnode will be created.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsCreate(struct vnode *dvp,   // IN: Directory to create file in
           char *name,          // IN: Name of file to create
           struct vattr *vap,   // IN: Attributes for file
           vcexcl_t excl,       // IN: Exclusive creation flag: either NONEXCL or EXCL
           int mode,            // IN: Permissions to create with
           struct vnode **vpp,  // OUT: Vnode of file to create
           struct cred *cr,     // IN: Credentials of caller
           int flag)            // IN: XXX
#else
static int
HgfsCreate(struct vnode *dvp,     // IN: Directory to create file in
           char *name,            // IN: Name of file to create
           struct vattr *vap,     // IN: Attributes for file
           vcexcl_t excl,         // IN: Exclusive creation flag: either NONEXCL or EXCL
           int mode,              // IN: Permissions to create with
           struct vnode **vpp,    // OUT: Vnode of file to create
           struct cred *cr,       // IN: Credentials of caller
           int flag,              // IN: XXX
           caller_context_t *ctx, // IN: Context of caller
           vsecattr_t *vsecp)     // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   int ret = 0;

   DEBUG(VM_DEBUG_ENTRY, "HgfsCreate(): entry for \"%s\"\n", name);

   if (!dvp || !name || !vap || !vpp || !cr) {
      cmn_err(HGFS_ERROR, "HgfsCreate: NULL input from Kernel.\n");
      return EINVAL;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   if (dvp->v_type != VDIR) {
      DEBUG(VM_DEBUG_FAIL, "HgfsCreate: files must be created in directories.\n");
      return ENOTDIR;
   }

   /*
    * There are two cases: either the file already exists or it doesn't.  If
    * the file exists already then *vpp points to its vnode that was allocated
    * in HgfsLookup().  In both cases we need to create a new vnode (since our
    * vnodes are per-open-file, not per-file), but we don't need to construct
    * the full name again if we already have it in the existing vnode.
    */
   if ( !(*vpp) ) {
      char fullname[MAXPATHLEN + 1];

      ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp), // Name of directory to create in
                             HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of name
                             name,                    // Name of file to create
                             fullname,                // Buffer to write full name
                             sizeof fullname);        // Size of this buffer

      if (ret < 0) {
         DEBUG(VM_DEBUG_FAIL, "HgfsCreate: couldn't create full path name.\n");
         return ENAMETOOLONG;
      }

      /* Create the vnode for this file. */
      ret = HgfsVnodeGet(vpp, sip, dvp->v_vfsp, fullname,
                         HGFS_FILE_TYPE_REGULAR, &sip->fileHashTable);
      if (ret) {
         return EIO;
      }
   } else {
      struct vnode *origVp = *vpp;

      ASSERT(origVp->v_type != VDIR);   /* HgfsMkdir() should have been invoked */

      ret = HgfsVnodeDup(vpp, origVp, sip, &sip->fileHashTable);
      if (ret) {
         return EIO;
      }

      /* These cannot be the same. */
      ASSERT(*vpp != origVp);
   }

   /* HgfsVnodeGet() guarantees this. */
   ASSERT(*vpp);

   /* Save the mode so when open is called we can reference it. */
   HgfsSetOpenFileMode(*vpp, vap->va_mode);

   /* Solaris automatically calls open after this, so our work is done. */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRemove --
 *
 *    Composes the full pathname of this file and sends a DELETE_FILE request
 *    by calling HgfsDelete().
 *
 *    "Removes the file for the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *    Returns 0 on success or a non-zero error code on error.
 *
 * Side effects:
 *    If successful, the file specified will be deleted from the host's
 *    filesystem.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsRemove(struct vnode *vp,    // IN: Directory containing file to remove
           char *nm,            // IN: Name of file to remove
           struct cred *cr)     // IN: Credentials of caller
#else
static int
HgfsRemove(struct vnode *vp,       // IN: Directory containing file to remove
           char *nm,               // IN: Name of file to remove
           struct cred *cr,        // IN: Credentials of caller
           caller_context_t *ctx,  // IN: Context of caller
           int flags)              // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   char fullpath[MAXPATHLEN + 1];
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsRemove().\n");

   if (!vp || !nm) {
      cmn_err(HGFS_ERROR, "HgfsRemove: NULL input from Kernel.\n");
      return EINVAL;
   }

   /* Ensure parent is a directory. */
   if (vp->v_type != VDIR) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsRemove: provided parent is a file, not a directory.\n");
      return ENOTDIR;
   }

   /* Ensure we know the name of the parent. */
   ASSERT(HGFS_KNOW_FILENAME(vp));

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   /*
    * We must construct the full name of the file to remove then call
    * HgfsDelete() to send the deletion request.
    */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(vp), // Name of directory to create in
                          HGFS_VP_TO_FILENAME_LENGTH(vp), // Length of name
                          nm,                      // Name of file to create
                          fullpath,                // Buffer to write full name
                          sizeof fullpath);        // Size of this buffer
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRemove: could not construct full name.\n");
      return ENAMETOOLONG;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsRemove: removing \"%s\".\n", fullpath);

   /* We can now send the delete request. */
   return HgfsDelete(sip, fullpath, HGFS_OP_DELETE_FILE);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsLink --
 *
 *    "Creates a hard link to the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsLink(struct vnode *tdvp,    //XXX: Vnode of directory to create link in
         struct vnode *svp,     //XXX: Vnode of file to link to
         char *tnm,             //XXX: Name of link in directory
         struct cred *cr)       // IN: Credentials of caller
#else
static int
HgfsLink(struct vnode *tdvp,     //XXX: Vnode of directory to create link in
         struct vnode *svp,      //XXX: Vnode of file to link to
         char *tnm,              //XXX: Name of link in directory
         struct cred *cr,        // IN: Credentials of caller
         caller_context_t *ctx,  // IN: Context of caller
         int flags)              // IN: XXX
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsLink() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRename --
 *
 *    Renames the provided source name in the source directory with the
 *    destination name in the destination directory.  A RENAME request is sent
 *    to the Hgfs server.
 *
 * Results:
 *    Returns 0 on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsRename(struct vnode *sdvp,  // IN: Source directory that contains file to move
           char *snm,           // IN: File to move
           struct vnode *tdvp,  // IN: Destination directory of file
           char *tnm,           // IN: Destination name of file
           struct cred *cr)     // IN: Credentials of caller
#else
static int
HgfsRename(struct vnode *sdvp,     // IN: Source directory that contains file to move
           char *snm,              // IN: File to move
           struct vnode *tdvp,     // IN: Destination directory of file
           char *tnm,              // IN: Destination name of file
           struct cred *cr,        // IN: Credentials of caller
           caller_context_t *ctx,  // IN: Context of caller
           int flags)              // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   HgfsReq *req;
   HgfsRequestRename *request;
   HgfsReplyRename *reply;
   HgfsFileName *newNameP;
   char srcFullPath[MAXPATHLEN + 1];
   char dstFullPath[MAXPATHLEN + 1];
   int ret;

   if (!sdvp || !snm || !tdvp || !tnm) {
      cmn_err(HGFS_ERROR, "HgfsRename: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsRename().\n");

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   /* Make sure we know the names of both parent directories. */
   ASSERT(HGFS_KNOW_FILENAME(sdvp) && HGFS_KNOW_FILENAME(tdvp));

   /* Make the full path of the source. */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(sdvp), HGFS_VP_TO_FILENAME_LENGTH(sdvp),
                          snm,
                          srcFullPath, sizeof srcFullPath);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRename: could not construct full path of source.\n");
      return ENAMETOOLONG;
   }

   /* Make the full path of the destination. */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(tdvp), HGFS_VP_TO_FILENAME_LENGTH(tdvp),
                          tnm,
                          dstFullPath, sizeof dstFullPath);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRename: could not construct full path of dest.\n");
      return ENAMETOOLONG;
   }

   /* Ensure both names will fit in one request. */
   if ((sizeof *request + strlen(srcFullPath) + strlen(dstFullPath))
       > HGFS_PACKET_MAX) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRename: names too big for one request.\n");
      return EPROTO;
   }

   /*
    * Now we can prepare and send the request.
    */
   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   request = (HgfsRequestRename *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_RENAME);

   /* Convert the source to cross platform and unescape its buffer. */
   ret = CPName_ConvertTo(srcFullPath, MAXPATHLEN, request->oldName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsRename: couldn't convert source to cross platform name.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->oldName.length = ret;

   /*
    * The new name is placed directly after the old name in the packet and we
    * access it through this pointer.
    */
   newNameP = (HgfsFileName *)((char *)&request->oldName +
                               sizeof request->oldName +
                               request->oldName.length);

   /* Convert the destination to cross platform and unescape its buffer. */
   ret = CPName_ConvertTo(dstFullPath, MAXPATHLEN, newNameP->name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL,
            "HgfsRename: couldn't convert destination to cross platform name.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   newNameP->length = ret;

   /* The request's size includes the request and both filenames. */
   req->packetSize = sizeof *request + request->oldName.length + newNameP->length;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplyRename *)req->packet;

   /* Validate the reply's state and size. */
   if (HgfsValidateReply(req, sizeof *reply) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRename: invalid reply received.\n");
      ret = EPROTO;
      goto out;
   }

   /* Return appropriate value. */
   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
   } else {
      DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);
   }

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMkdir --
 *
 *    Makes a directory named dirname in the directory specified by the dvp
 *    vnode by sending a CREATE_DIR request, then allocates a vnode for this
 *    new directory and writes its address into vpp.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    If successful, a directory is created on the host's filesystem.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsMkdir(struct vnode *dvp,    // IN: Vnode of directory to create directory in
          char *dirname,        // IN: Name of directory to create
          struct vattr *vap,    // IN: Attributes of new directory
          struct vnode **vpp,   // OUT: Set to point to vnode for new directory
          struct cred *cr)      // IN: Credentials of caller
#else
static int
HgfsMkdir(struct vnode *dvp,      // IN: Vnode of directory to create directory in
          char *dirname,          // IN: Name of directory to create
          struct vattr *vap,      // IN: Attributes of new directory
          struct vnode **vpp,     // OUT: Set to point to vnode for new directory
          struct cred *cr,        // IN: Credentials of caller
          caller_context_t *ctx,  // IN: Context of caller
          int flags,              // IN: XXX
          vsecattr_t *vsecp)      // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   HgfsReq *req;
   HgfsRequestCreateDir *request;
   HgfsReplyCreateDir *reply;
   char fullname[MAXPATHLEN + 1];
   int ret;

   if (!dvp || !dirname || !vap || !vpp) {
      cmn_err(HGFS_ERROR, "HgfsMkdir: NULL input from Kernel.\n");
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsMkdir: dvp=%p (%s), dirname=%s, vap=%p, vpp=%p\n",
                         dvp, HGFS_VP_TO_FILENAME(dvp), dirname, vap,
                         *vpp);

   /*
    * We need to construct the full path of the directory to create then send
    * a CREATE_DIR request.  If successful we will create a vnode and fill in
    * vpp with a pointer to it.
    *
    * Note that unlike in HgfsCreate(), *vpp is always NULL.
    */

   if (dvp->v_type != VDIR) {
        DEBUG(VM_DEBUG_FAIL, "HgfsMkdir: must create directory in directory.\n");
        return ENOTDIR;
   }

   /* Construct the complete path of the directory to create. */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(dvp),// Name of directory to create in
                          HGFS_VP_TO_FILENAME_LENGTH(dvp), // Length of name
                          dirname,                 // Name of file to create
                          fullname,                // Buffer to write full name
                          sizeof fullname);        // Size of this buffer

   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsCreate: couldn't create full path name.\n");
      return ENAMETOOLONG;
   }

   /* Get pointer to our Superinfo */
   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   /* Initialize the request's contents. */
   request = (HgfsRequestCreateDir *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_CREATE_DIR);

   request->permissions = (vap->va_mode & S_IRWXU) >> HGFS_ATTR_MODE_SHIFT;

   ret = CPName_ConvertTo(fullname, MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMkdir: cross-platform name is too long.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->fileName.length = ret;

   /* Set the size of this request. */
   req->packetSize = sizeof *request + request->fileName.length;

   /* Send the request. */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplyCreateDir *)req->packet;

   if (HgfsValidateReply(req, sizeof *reply) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMkdir: invalid reply received.\n");
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   /* We now create the vnode for the new directory. */
   ret = HgfsVnodeGet(vpp, sip, dvp->v_vfsp, fullname,
                      HGFS_FILE_TYPE_DIRECTORY, &sip->fileHashTable);
   if (ret) {
      ret = EIO;
      goto out;
   }

   ASSERT(*vpp);        /* HgfsIget guarantees this. */
   ret = 0;

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRmdir --
 *
 *    Removes the specified name from the provided vnode.  Sends a DELETE
 *    request by calling HgfsDelete() with the filename and correct opcode to
 *    indicate deletion of a directory.
 *
 *    "Removes the directory pointed to by the supplied vnode." (Solaris
 *    Internals, p537)
 *
 * Results:
 *    Returns 0 on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsRmdir(struct vnode *vp,     // IN: Dir containing dir to remove
          char *nm,             // IN: Name of directory to remove
          struct vnode *cdir,   // XXX?
          struct cred *cr)      // IN: Credentials of caller
#else
static int
HgfsRmdir(struct vnode *vp,       // IN: Dir containing dir to remove
          char *nm,               // IN: Name of directory to remove
          struct vnode *cdir,     // XXX?
          struct cred *cr,        // IN: Credentials of caller
          caller_context_t *ctx,  // IN: Context of caller
          int flags)              // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   char fullpath[MAXPATHLEN + 1];
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsRmdir().\n");

   if (!vp || !nm) {
      cmn_err(HGFS_ERROR, "HgfsRmdir: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsRmdir: vp=%p (%s), nm=%s, cdir=%p (%s)\n",
         vp, (HGFS_VP_TO_FP(vp)) ? HGFS_VP_TO_FILENAME(vp) : "vp->v_data null",
         nm, cdir, (HGFS_VP_TO_FP(cdir)) ? HGFS_VP_TO_FILENAME(cdir) : "cdir->v_data null");

   /* A few checks to ensure we can remove the directory. */
   if (vp->v_type != VDIR) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRmdir: provided parent is a file, not a directory.\n");
      return ENOTDIR;
   }

   ASSERT(HGFS_KNOW_FILENAME(vp));

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return EIO;
   }

   /*
    * We need to construct the full name of the directory to remove then call
    * HgfsDelete with the proper opcode.
    */
   ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(vp), // Name of directory to create in
                          HGFS_VP_TO_FILENAME_LENGTH(vp), // Length of name
                          nm,                      // Name of file to create
                          fullpath,                // Buffer to write full name
                          sizeof fullpath);        // Size of this buffer
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsRmdir: could not construct full name.\n");
      return ENAMETOOLONG;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsRmdir: removing \"%s\".\n", fullpath);

   /* We can now send the delete request. */
   return HgfsDelete(sip, fullpath, HGFS_OP_DELETE_DIR);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReaddir --
 *
 *    Reads as many entries from the directory as will fit in to the provided
 *    buffer.  Each directory entry is read by calling HgfsGetNextDirEntry().
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
 *    Returns 0 on success and a non-zero error code on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsReaddir(struct vnode *vp,   // IN: Vnode of directory to read
            struct uio *uiop,   // IN: User's read request
            struct cred *cr,    // IN: Credentials of caller
            int *eofp)          // OUT: Indicates we are done
#else
static int
HgfsReaddir(struct vnode *vp,       // IN: Vnode of directory to read
            struct uio *uiop,       // IN: User's read request
            struct cred *cr,        // IN: Credentials of caller
            int *eofp,              // OUT: Indicates we are done
            caller_context_t *ctx,  // IN: Context of caller
            int flags)              // IN: XXX
#endif
{
   HgfsSuperInfo *sip;
   HgfsHandle handle;
   struct dirent64 *dirp, *origdirp;
   ssize_t readSize;
   uint64_t offset;
   Bool done;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir().\n");

   if (!vp || !uiop || !cr || !eofp) {
      cmn_err(HGFS_ERROR, "HgfsReaddir: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir: uiop->uio_resid=%ld, "
         "uiop->uio_loffset=%lld\n",
         uiop->uio_resid, uiop->uio_loffset);


   /*
    * XXX: If would be nice if we could perform some sort of sanity check on
    * the handle here.  Perhaps make sure handle <= NUM_SEARCHES in
    * hgfsServer.c since the handle is the index number in searchArray.
    */
   if ( !HGFS_KNOW_FILENAME(vp) ) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: we don't know the filename.\n");
      return EBADF;
   }


   sip = HgfsGetSuperInfo();
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: we can't get the superinfo.\n");
      return EIO;
   }
   /*
    * In order to fill the user's buffer with directory entries, we must
    * iterate on HGFS_OP_SEARCH_READ requests until either the user's buffer is
    * full or there are no more entries.  Each call to HgfsGetNextDirEntry()
    * fills in the name and attribute structure for the next entry.  We then
    * escape that name and place it in a kernel buffer that's the same size as
    * the user's buffer.  Once there are no more entries or no more room in the
    * buffer, we copy it to user space.
    */

   /*
    * XXX
    * Note that I allocate a large buffer in kernel space so I can do only one
    * copy to user space, otherwise we would need to do a copy for each
    * directory entry.  This approach is potentially bad since readSize is
    * as big as the buffer the user called us with, and therefore in their
    * control.  (Actually, it's likely that the user can just say it has a
    * huge buffer without really having it.)  For this reason, I call
    * kmem_zalloc() with the KM_NOSLEEP flag which fails if it cannot allocate
    * memory rather than sleeping until it can (as KM_SLEEP does).
    *
    * This approach may want to be changed in the future.
    */

   readSize = uiop->uio_resid;
   origdirp = dirp = (struct dirent64 *)kmem_zalloc(readSize, KM_NOSLEEP);
   if (!origdirp) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: couldn't allocate memory.\n");
      return ENOMEM;
   }

   /*
    * We need to get the handle for this open directory to send to the Hgfs
    * server in our requests.
    */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: could not get handle.\n");
      return EINVAL;
   }


   /*
    * Loop until one of the following conditions is met:
    *  o An error occurs while reading a directory entry
    *  o There are no more directory entries to read
    *  o The buffer is full and cannot hold the next entry
    *
    * We request dentries from the Hgfs server based on their index in the
    * directory.  The offset value is initialized to the value specified in
    * the user's io request and is incremented each time through the loop.
    *
    * dirp is incremented by the record length each time through the loop and
    * is used to determine where in the kernel buffer we write to.
    */
   for (offset = uiop->uio_loffset, done = 0; /* Nothing */ ; offset++) {
      char nameBuf[MAXNAMELEN + 1];
      char escName[MAXNAMELEN + 1];
      char fullName[MAXPATHLEN + 1];

      DEBUG(VM_DEBUG_COMM,
            "HgfsReaddir: getting directory entry at offset %"FMT64"u.\n", offset);

      DEBUG(VM_DEBUG_HANDLE, "HgfsReaddir: ** handle=%d, file=%s\n",
            handle, HGFS_VP_TO_FILENAME(vp));

      ret = HgfsGetNextDirEntry(sip, handle, offset, nameBuf, &done);
      /* If the filename was too long, we skip to the next entry ... */
      if (ret == EOVERFLOW) {
         continue;
      /* ... but if another error occurred, we return that error code ... */
      } else if (ret) {
         DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: failure occurred in HgfsGetNextDirEntry\n");
         goto out;
      /*
       * ... and if there are no more entries, we set the end of file pointer
       * and break out of the loop.
       */
      } else if (done == TRUE) {
         DEBUG(VM_DEBUG_COMM, "HgfsReaddir: Done reading directory entries.\n");
         *eofp = TRUE;
         break;
      }

      /*
       * We now have the directory entry, so we sanitize the name and try to
       * put it in our buffer.
       */
      DEBUG(VM_DEBUG_COMM, "HgfsReaddir: received filename \"%s\"\n", nameBuf);

      ret = HgfsEscape_Do(nameBuf, strlen(nameBuf), sizeof escName, escName);
      /* If the escaped name didn't fit in the buffer, skip to the next entry. */
      if (ret < 0) {
         DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: HgfsEscape_Do failed.\n");
         continue;
      }

      /*
       * Make sure there is enough room in the buffer for the entire directory
       * entry.  If not, we just break out of the loop and copy what we have.
       */
      if (DIRENT64_RECLEN(ret) > (readSize - ((uintptr_t)dirp - (uintptr_t)origdirp))) {
         DEBUG(VM_DEBUG_INFO, "HgfsReaddir: ran out of room in the buffer.\n");
         break;
      }

      /* Fill in the directory entry. */
      dirp->d_reclen = DIRENT64_RECLEN(ret);
      dirp->d_off = offset;
      memcpy(dirp->d_name, escName, ret);
      dirp->d_name[ret] = '\0';

      ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(vp),           // Directorie's name
                             HGFS_VP_TO_FILENAME_LENGTH(vp),    // Length
                             dirp->d_name,                      // Name of file
                             fullName,                          // Destination buffer
                             sizeof fullName);                  // Size of this buffer
      /* Skip this entry if the full path was too long. */
      if (ret < 0) {
         continue;
      }

      /*
       * Place the node id, which serves the purpose of inode number, for this
       * filename directory entry.  As long as we are using a dirent64, this is
       * okay since ino_t is also a u_longlong_t.
       */
      HgfsNodeIdGet(&sip->fileHashTable, fullName, (uint32_t)ret,
                    &dirp->d_ino);

      /* Advance to the location for the next directory entry */
      dirp = (struct dirent64 *)((intptr_t)dirp + dirp->d_reclen);
   }

   /*
    * Now that we've filled our buffer with as many dentries as fit, we copy it
    * into the user's buffer.
    */
   ret = uiomove(origdirp,                                // Source buffer
                 ((uintptr_t)dirp - (uintptr_t)origdirp), // Size of this buffer
                 UIO_READ,                                // Read flag
                 uiop);                                   // User's request struct

   /*
    * uiomove(9F) will have incremented the uio offset by the number of bytes
    * written.  We reset it here to the fs-specific offset in our directory so
    * the next time we are called it is correct.  (Note, this does not break
    * anything and /is/ how this field is intended to be used.)
    */
   uiop->uio_loffset = offset;

   DEBUG(VM_DEBUG_DONE, "HgfsReaddir: done (ret=%d, *eofp=%d).\n", ret, *eofp);
out:
   kmem_free(origdirp, readSize);
   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir: exiting.\n");
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSymlink --
 *
 *    "Creates a symbolic link between the two pathnames" (Solaris Internals,
 *    p538)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <=3
static int
HgfsSymlink(struct vnode *dvp,  // IN: Directory to create link in
            char *linkname,     // IN: Name of link
            struct vattr *vap,  // IN: Attributes for the symlink
            char *target,       // IN: Name of target for symlink
            struct cred *cr)    // IN: Credentials of caller
#else
static int
HgfsSymlink(struct vnode *dvp,      // IN: Directory to create link in
            char *linkname,         // IN: Name of link
            struct vattr *vap,      // IN: Attributes for the symlink
            char *target,           // IN: Name of target for symlink
            struct cred *cr,        // IN: Credentials of caller
            caller_context_t *ctx,  // IN: Context of caller
            int flags)              // IN: XXX
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsSymlink() NOTSUP.\n");

   /*
    * Hgfs doesn't support links.
    */
   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsReadlink --
 *
 *    "Follows the symlink in the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsReadlink(struct vnode *vp,  // IN: Vnode for the symlink
             struct uio *uiop,  // IN: XXX User's request?
             struct cred *cr)   // IN: Credentials of caller
#else
static int
HgfsReadlink(struct vnode *vp,       // IN: Vnode for the symlink
             struct uio *uiop,       // IN: XXX User's request?
             struct cred *cr,        // IN: Credentials of caller
             caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsReadlink() NOTSUP.\n");

   /*
    * Hgfs doesn't support links
    */
   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFsync --
 *
 *    We don't map any memory so we can safely return success.
 *
 *    "Flushes out any dirty pages for the supplied vnode." (Solaris
 *    Internals, p536)
 *
 * Results:
 *    Returns 0 on success and non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsFsync(struct vnode *vp,     // IN: Vnode for the file to sync
          int syncflag,         // IN: XXX?
          struct cred *cr)      // IN: Credentials of the caller
#else
static int
HgfsFsync(struct vnode *vp,       // IN: Vnode for the file to sync
          int syncflag,           // IN: XXX?
          struct cred *cr,        // IN: Credentials of the caller
          caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsFsync().\n");

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInactive --
 *
 *    Frees a vnode that is no longer referenced.  This is done by calling
 *    HgfsVnodePut() from hgfsState.c, which also cleans up our internal
 *    filesystem state.
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

#if HGFS_VFS_VERSION <= 3
static void
HgfsInactive(struct vnode *vp,  // IN: Vnode to operate on
             struct cred *cr)   // IN: Credentials of the caller
#else
static void
HgfsInactive(struct vnode *vp,       // IN: Vnode to operate on
             struct cred *cr,        // IN: Credentials of the caller
             caller_context_t *ctx)  // IN: Context of caller
#endif
{
   HgfsSuperInfo *sip;

   if (!vp) {
      cmn_err(HGFS_ERROR, "HgfsInactive: NULL input from Kernel.\n");
      return;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsInactive().\n");

   sip = HgfsGetSuperInfo();
   if (!sip) {
      return;
   }

   /* We need the check and decrement of v_count to be atomic */
   mutex_enter(&vp->v_lock);

   if (vp->v_count > 1) {
      vp->v_count--;
      mutex_exit(&vp->v_lock);

      DEBUG(VM_DEBUG_LOAD, "--> decremented count of vnode %p to %d\n",
            vp, vp->v_count);

      /*
       * XXX This should only ever happen for the root vnode with our new state
       * organization.
       */
      if (vp != sip->rootVnode) {
         DEBUG(VM_DEBUG_ALWAYS, "HgfsInactive: v_count of vnode for %s too high!\n",
               HGFS_VP_TO_FILENAME(vp));
      }
      ASSERT(vp == sip->rootVnode);

   } else {
      mutex_exit(&vp->v_lock);

      DEBUG(VM_DEBUG_LOAD, "--> freeing vnode %p - \"%s\"\n",
            vp, HGFS_VP_TO_FILENAME(vp));

      /* Deallocate this vnode. */
      HgfsVnodePut(vp, &sip->fileHashTable);

   }

}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFid --
 *
 *    Provide a unique file identifier for this vnode.  Note that I have never
 *    seen this function called by the Kernel.
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsFid(struct vnode *vp,       // IN: File to generate file identifier for
        struct fid *fidp)       // XXX: IN/OUT?: File identifier
#else
static int
HgfsFid(struct vnode *vp,       // IN: File to generate file identifier for
        struct fid *fidp,       // XXX: IN/OUT?: File identifier
        caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsFid().\n");

   if (!vp || !fidp) {
      cmn_err(HGFS_ERROR, "HgfsFid: NULL input from Kernel.\n");
   }

   /*
    * Make sure we can fit our node id in the provided structure.  This allows
    * us to call memcpy() with the sizeof the source below.
    */
   if (sizeof fidp->fid_data < sizeof HGFS_VP_TO_NODEID(vp)) {
      return EOVERFLOW;
   }

   memset(fidp, 0, sizeof *fidp);
   memcpy(&fidp->fid_data, &HGFS_VP_TO_NODEID(vp), sizeof HGFS_VP_TO_NODEID(vp));
   fidp->fid_len  = sizeof HGFS_VP_TO_NODEID(vp);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRwlock --
 *
 *    Acquires either a readers or writers lock.
 *
 *    "Holds the reader/writer lock for the supplied vnode.  This method is
 *    called for each vnode, with the rwflag set to 0 inside a read() system
 *    call and the rwflag set to 1 inside a write() at a time.  Some file
 *    system implementations have opetions to ignore the writer lock inside
 *    vop_rwlock()." (Solaris Internals, p537)
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The file's readers/writers lock is held after this function.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static void
HgfsRwlock(struct vnode *vp,    // IN: Vnode to get lock for
           int write_lock)      // IN: Set if wants a write lock, cleared for read lock
#elif HGFS_VFS_VERSION == 3
static void
HgfsRwlock(struct vnode *vp,            // IN: Vnode to get lock for
           int write_lock,              // IN: Set for write lock, cleared for read lock
           caller_context_t *context)   // IN: Context of caller
#else /* SOL11 */
static int
HgfsRwlock(struct vnode *vp,            // IN: Vnode to get lock for
           int write_lock,              // IN: Set for write lock, cleared for read lock
           caller_context_t *context)   // IN: Context of caller
#endif
{
   if (write_lock) {
      rw_enter(HGFS_VP_TO_RWLOCKP(vp), RW_WRITER);
   } else {
      rw_enter(HGFS_VP_TO_RWLOCKP(vp), RW_READER);
   }

#if HGFS_VFS_VERSION == 5
   return write_lock ? V_WRITELOCK_TRUE : V_WRITELOCK_FALSE;
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRwunlock --
 *
 *    "Releases the reader/writer lock for the supplied vnode." (Solaris
 *    Internals, p537)
 *
 * Results:
 *    Void.
 *
 * Side effects:
 *    This file's readers/writer lock is unlocked.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static void
HgfsRwunlock(struct vnode *vp,  // IN: Vnode to release lock for
             int write_lock)    // IN: Set for write lock, cleared for read lock
#else
static void
HgfsRwunlock(struct vnode *vp,          // IN: Vnode to release lock for
             int write_lock,            // IN: Set for write lock, cleared for read lock
             caller_context_t *context) // IN: Context of caller
#endif
{
   //DEBUG(VM_DEBUG_ENTRY, "HgfsRwunlock().\n");

   if (!vp) {
      cmn_err(HGFS_ERROR, "HgfsRwunlock: NULL input from Kernel.\n");
      return;
   }

   rw_exit(HGFS_VP_TO_RWLOCKP(vp));
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSeek --
 *
 *    Checks to ensure that the specified offset is valid.  Actual manipulation
 *    of the file position is handled by the Kernel.
 *
 *    "Seeks within the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *    Returns zero if this offset is valid and EINVAL if it isn't.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsSeek(struct vnode *vp,      // IN:     Vnode to seek within
         offset_t ooff,         // IN:     Current offset in this vnode
         offset_t *noffp)       // IN/OUT: Requested new offset within file
#else
static int
HgfsSeek(struct vnode *vp,      // IN:     Vnode to seek within
         offset_t ooff,         // IN:     Current offset in this vnode
         offset_t *noffp,       // IN/OUT: Requested new offset within file
         caller_context_t *ctx) // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsSeek().\n");

   if (!noffp) {
      DEBUG(VM_DEBUG_FAIL, "HgfsSeek: noffp is NULL\n");
      return EINVAL;
   }

   if (*noffp < 0) {
      return EINVAL;
   }

   DEBUG(VM_DEBUG_INFO, "HgfsSeek: file   %s\n", HGFS_VP_TO_FILENAME(vp));
   DEBUG(VM_DEBUG_INFO, "HgfsSeek: ooff   %llu\n", ooff);
   DEBUG(VM_DEBUG_INFO, "HgfsSeek: *noffp %llu\n", *noffp);

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsCmp --
 *
 *    Compares two vnodes to see if they are for the same file.  Our
 *    filesystem-specific check is to compare the filenames, file type, and
 *    file flags.  Since we keep vnodes per-open-file, rather than per-file,
 *    this function has significance.
 *
 *    This function is invoked by the VN_CMP macro only if the two given
 *    pointers are different and each has the same operations (v_op).
 *
 * Results:
 *    TRUE if vnodes are the same, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsCmp(struct vnode *vp1,      // IN: First vnode
        struct vnode *vp2)      // IN: Second vnode
#else
static int
HgfsCmp(struct vnode *vp1,      // IN: First vnode
        struct vnode *vp2,      // IN: Second vnode
        caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsCmp: vp1=%p, vp2=%p.\n", vp1, vp2);

   /*
    * This function is only called if:
    * ((vp1 != vp2) && (vp1->v_op == vp2->v_op))
    *
    * We also care if the filenames are the same.
    */

   if (vp1->v_type != vp2->v_type) {
      DEBUG(VM_DEBUG_FAIL, "HgfsCmp: %s != %s",
            (vp1->v_type == VDIR) ? "VDIR" : "VREG",
            (vp2->v_type == VDIR) ? "VDIR" : "VREG");
      return FALSE;
   }

   if (vp1->v_flag != vp2->v_flag) {
      DEBUG(VM_DEBUG_FAIL, "HgfsCmp: flags: %x != %x\n", vp1->v_flag, vp2->v_flag);
      return FALSE;
   }

   if (strcmp(HGFS_VP_TO_FILENAME(vp1), HGFS_VP_TO_FILENAME(vp2)) != 0) {
      return FALSE;
   }

   DEBUG(VM_DEBUG_DONE, "HgfsCmp: for \"%s\", vp1 == vp2\n", HGFS_VP_TO_FILENAME(vp1));
   return TRUE;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFrlock --
 *
 *    "Does file and record locking for the supplied vnode." (Solaris
 *    Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsFrlock(struct vnode *vp,                    // XXX
           int cmd,                             // IN: Command to carry out
           struct flock64 *bfp,                 // XXX
           int flag,                            // XXX
           offset_t offset,                     // XXX
           struct flk_callback *flk_callbackp,  // XXX
           struct cred *cr)                     // IN: Credentials of caller
#else
static int
HgfsFrlock(struct vnode *vp,                    // XXX
           int cmd,                             // IN: Command to carry out
           struct flock64 *bfp,                 // XXX
           int flag,                            // XXX
           offset_t offset,                     // XXX
           struct flk_callback *flk_callbackp,  // XXX
           struct cred *cr,                     // IN: Credentials of caller
           caller_context_t *ctx)               // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsFrlock() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSpace --
 *
 *    "Frees space for the supplied vnode." (Solaris Internals, p538)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static int
HgfsSpace(struct vnode *vp,     // IN: Vnode to free space for
          int cmd,              // IN: XXX Command
          struct flock64 *bfp,  // IN: XXX
          int flag,             // IN: XXX
          offset_t offset,      // IN: XXX
          struct cred *cr)      // IN: Credentials of caller
#else
static int
HgfsSpace(struct vnode *vp,             // IN: Vnode to free space for
          int cmd,                      // IN: XXX Command
          struct flock64 *bfp,          // IN: XXX
          int flag,                     // IN: XXX
          offset_t offset,              // IN: XXX
          struct cred *cr,              // IN: Credentials of caller
          caller_context_t *ctx)        // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsSpace() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRealvp --
 *
 *    "Gets the real vnode from the supplied vnode." (Solaris Internals, p537)
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.  On success, vpp
 *    is given the value of the real vnode.  Currently this always returns
 *    success.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsRealvp(struct vnode *vp,    // IN: Vnode to find real vnode for
           struct vnode **vpp)  // OUT: Set to point to real vnode
#else
static int
HgfsRealvp(struct vnode *vp,    // IN: Vnode to find real vnode for
           struct vnode **vpp,  // OUT: Set to point to real vnode
           caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsRealvp().\n");

   DEBUG(VM_DEBUG_ENTRY, "HgfsRealvp: vp=%p\n", vp);
   DEBUG(VM_DEBUG_ENTRY, "HgfsRealvp: vp's name=%s\n", HGFS_VP_TO_FILENAME(vp));

   /*
    * Here we just supply the vnode we were provided.  This behavior is correct
    * since we maintain vnodes per-open-file rather than per-file.  The "real"
    * vnode /is/ the provided one since any other one belongs to a different
    * "open" file.
    */
   *vpp = vp;

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetpage --
 *
 *    HgfsRead() does not map file data into the Kernel's address space, so we
 *    shouldn't need to support this (that is, page faults will never occur).
 *
 *    "Gets pages in the range offset and length for the vnode from the
 *    backing store of the file system.  Does the real work of reading a
 *    vnode.  This method is often called as a result of read(), which causes
 *    a page fault in seg_map, which calls vop_getpage()." (Solaris Internals,
 *    p536)
 *
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsGetpage(struct vnode *vp,           // IN: Vnode of file to get page for
            offset_t off,               // IN: Offset in file/page to retrieve
            size_t len,                 // IN: Length of range to retrieve
            uint_t *protp,              // XXX
            struct page **plarr,        // XXX
            size_t plsz,                // IN: XXX
            struct seg *seg,            // XXX: Segment
            caddr_t addr,               // IN: Address of XXX
            enum seg_rw rw,             // IN: XXX
            struct cred *cr)            //IN Credentials of caller
#else
static int
HgfsGetpage(struct vnode *vp,           // IN: Vnode of file to get page for
            offset_t off,               // IN: Offset in file/page to retrieve
            size_t len,                 // IN: Length of range to retrieve
            uint_t *protp,              // XXX
            struct page **plarr,        // XXX
            size_t plsz,                // IN: XXX
            struct seg *seg,            // XXX: Segment
            caddr_t addr,               // IN: Address of XXX
            enum seg_rw rw,             // IN: XXX
            struct cred *cr,            //IN Credentials of caller
            caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsGetpage() NOTSUP.\n");

   /* We don't currently need this; see the comment above. */

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsPutpage --
 *
 *    HgfsWrite() does not map file data into the Kernel's address space, so we
 *    shouldn't need to support this (that is, page faults will never occur).
 *
 *    "Writes pages in the range offset and length for the vnode to the
 *    backing store of the file system.  Does the real work of reading a
 *    vnode."  (Solaris Internals, p537)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsPutpage(struct vnode *vp,   // IN: Vnode of file to put page for
            offset_t off,       // IN: Offset to begin writing
            size_t len,         // IN: Amount of data to write
            int flags,          // IN: XXX
            struct cred *cr)    // IN: Credentials of caller
#else
static int
HgfsPutpage(struct vnode *vp,      // IN: Vnode of file to put page for
            offset_t off,          // IN: Offset to begin writing
            size_t len,            // IN: Amount of data to write
            int flags,             // IN: XXX
            struct cred *cr,       // IN: Credentials of caller
            caller_context_t *ctx) // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsPutpage() NOTSUP.\n");

   /* We don't currently need this; see the comment above. */

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMap --
 *
 *    Each file has its VNOMAP flag set so this shouldn't be invoked.  Most
 *    applications seem to handle this so, if this becomes a problem this
 *    function will need to be implemented.
 *
 *    "Maps a range of pages into an address space by doing the appropriate
 *    checks and calline as_map()" (Solaris Internals, p537)
 *
 *
 * Results:
 *    Returns 0 on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsMap(struct vnode *vp,       // IN: Vnode of file to map page for
        offset_t off,           // IN: XXX
        struct as *as,          // IN: Address space
        caddr_t *addrp,         // IN: XXX: Address in this space?
        size_t len,             // IN: XXX:Length of area to map?
        uchar_t prot,           // IN: XXX
        uchar_t maxprot,        // IN: XXX
        uint_t flags,           // IN: XXX
        struct cred *cr)        // IN: Credentials of caller
#else
static int
HgfsMap(struct vnode *vp,       // IN: Vnode of file to map page for
        offset_t off,           // IN: XXX
        struct as *as,          // IN: Address space
        caddr_t *addrp,         // IN: XXX: Address in this space?
        size_t len,             // IN: XXX:Length of area to map?
        uchar_t prot,           // IN: XXX
        uchar_t maxprot,        // IN: XXX
        uint_t flags,           // IN: XXX
        struct cred *cr,        // IN: Credentials of caller
        caller_context_t *ctx)  // IN: Context of caller
#endif
{
   /* We specify VNOMAP for each file, so this shouldn't be called. */
   DEBUG(VM_DEBUG_NOTSUP, "HgfsMap() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAddmap --
 *
 *    Since HgfsMap() above is ENOTSUP, this is not needed.
 *
 *    "Increments the map count." (Solaris Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsAddmap(struct vnode *vp,    // IN: Vnode to increment map count for
           offset_t off,        //XXX
           struct as *as,       // IN: Address space
           caddr_t addrp,       // IN: XXX: Address in this space?
           size_t len,          // IN: XXX: Length of this mapping?
           uchar_t prot,        // IN: XXX
           uchar_t maxprot,     // IN: XXX
           uint_t flags,        // IN: XXX
           struct cred *cr)     // IN: Credentials of caller
#else
static int
HgfsAddmap(struct vnode *vp,       // IN: Vnode to increment map count for
           offset_t off,           //XXX
           struct as *as,          // IN: Address space
           caddr_t addrp,          // IN: XXX: Address in this space?
           size_t len,             // IN: XXX: Length of this mapping?
           uchar_t prot,           // IN: XXX
           uchar_t maxprot,        // IN: XXX
           uint_t flags,           // IN: XXX
           struct cred *cr,        // IN: Credentials of caller
           caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsAddmap() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDelmap --
 *
 *    Since HgfsMap() above is ENOTSUP, this is not needed.
 *
 *    "Decrements the map count." (Solaris Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsDelmap(struct vnode *vp,    // IN: Vnode of file to decrement map count for
           offset_t off,        // XXX
           struct as *as,       // IN: Address space
           caddr_t addr,        // IN: XXX: Address in this space?
           size_t len,          // IN: XXX: Length of this mapping?
           uint_t prot,         // IN: XXX
           uint_t maxprot,      // IN: XXX
           uint_t flags,        // IN: XXX
           struct cred *cr)     // IN: Credentials of caller
#else
static int
HgfsDelmap(struct vnode *vp,    // IN: Vnode of file to decrement map count for
           offset_t off,        // XXX
           struct as *as,       // IN: Address space
           caddr_t addr,        // IN: XXX: Address in this space?
           size_t len,          // IN: XXX: Length of this mapping?
           uint_t prot,         // IN: XXX
           uint_t maxprot,      // IN: XXX
           uint_t flags,        // IN: XXX
           struct cred *cr,     // IN: Credentials of caller
           caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsDelmap() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsPoll --
 *
 *    We are using fs_poll() instead of this, which seems acceptable so far.
 *
 *    Invoked when user calls poll(2) on a file in our filesystem.
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */
/*
int
HgfsPoll(struct vnode *vp,      // IN: Vnode of file to poll
         short ev,              // IN: Requested events
         int any,               // IN: Whether other file descriptors have had events
         short *revp,           // OUT: Filled in with events that have occurred
         struct pollhead **phpp)// OUT: Set to a pollhead if necessary
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsPoll() NOTSUP.\n");

   return ENOTSUP;
}*/


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDump --
 *
 *    "Dumps data when the kernel is in a frozen state." (Solaris Internals,
 *    p536)
 *
 * Results:
 *
 * Side effects:
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsDump(struct vnode *vp,      // IN: Vnode of XXX
         caddr_t addr,          // IN: XXX: Location to dump to?
         int lbdn,              // IN: XXX
         int dblks)             // IN: XXX
#else
static int
HgfsDump(struct vnode *vp,         // IN: Vnode of XXX
         caddr_t addr,             // IN: XXX: Location to dump to?
         offset_t lbdn,            // IN: XXX
         offset_t dblks,           // IN: XXX
         caller_context_t *ctx)    // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsDump() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsPathconf --
 *
 *    "Establishes file system parameters with the pathconf system call."
 *    (Solaris Internals, p537)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsPathconf(struct vnode *vp,  // IN: Vnode of file to establish parameters for
             int cmd,           // IN: Command
             ulong_t *valp,     // OUT: XXX: Returned value?
             struct cred *cr)   // IN: Credentials of caller
#else
static int
HgfsPathconf(struct vnode *vp,       // IN: Vnode of file to establish parameters for
             int cmd,                // IN: Command
             ulong_t *valp,          // OUT: XXX: Returned value?
             struct cred *cr,        // IN: Credentials of caller
             caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsPathconf() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsPageio --
 *
 *    "Paged I/O supprt for file system swap files." (Solaris Internals, p537)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsPageio(struct vnode *vp,
           struct page *pp,
           u_offset_t io_off,
           size_t io_len,
           int flags,
           struct cred *cr)
#else
static int
HgfsPageio(struct vnode *vp,
           struct page *pp,
           u_offset_t io_off,
           size_t io_len,
           int flags,
           struct cred *cr,
           caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsPageio() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDumpctl --
 *
 *    "Prepares the file system before and after a dump" (Solaris Internals,
 *    p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsDumpctl(struct vnode *vp,
            int action,
            int *blkp)
#else
static int
HgfsDumpctl(struct vnode *vp,
            int action,
            offset_t *blkp,
            caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsDumpctl() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDispose --
 *
 *    Since we don't map any parts of files to pages, this isn't needed.
 *
 *    "Frees the given page from the vnode." (Solaris Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static void
HgfsDispose(struct vnode *vp,   // IN: Vnode to free page for
            struct page *pp,    // IN: Page to free
            int flag,           // IN: XXX
            int dn,             // IN: XXX
            struct cred *cr)    // IN: Credentials of caller
#else
static void
HgfsDispose(struct vnode *vp,       // IN: Vnode to free page for
            struct page *pp,        // IN: Page to free
            int flag,               // IN: XXX
            int dn,                 // IN: XXX
            struct cred *cr,        // IN: Credentials of caller
            caller_context_t *ctx)  // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_ENTRY, "HgfsDispose().\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetsecattr --
 *
 *    "Sets security access control list attributes." (Solaris Internals,
 *    p538)
 *
 *    We almost certainly won't support this.
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION <= 3
static int
HgfsSetsecattr(struct vnode *vp,
               vsecattr_t *vsap,
               int flag,
               struct cred *cr)
#else
static int
HgfsSetsecattr(struct vnode *vp,
               vsecattr_t *vsap,
               int flag,
               struct cred *cr,
               caller_context_t *ctx)
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsSetsecattr() NOTSUP.\n");

   return ENOTSUP;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetsecattr --
 *
 *    We are using fs_fab_acl() instead of this, which seems to do the right
 *    thing.
 *
 *    "Gets security access control list attributes" (Solaris Internals, p536)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */
/*
int
HgfsGetsecattr(struct vnode *vp,
               vsecattr_t *vsap,
               int flag,
               struct cred *cr)
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsGetsecattr() NOTSUP.\n");

   return ENOTSUP;
}*/


/*
 *----------------------------------------------------------------------------
 *
 * HgfsShrlock --
 *
 *    "ONC shared lock support." (Solaris Internals, p538)
 *
 * Results:
 *
 * Side effects:
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION == 2
static int
HgfsShrlock(struct vnode *vp,           // IN: Vnode of file to lock
            int cmd,                    // IN: Command
            struct shrlock *chr,        // IN: Lock
            int flag)                   // IN: Flags for XXX
#elif HGFS_VFS_VERSION == 3
static int
HgfsShrlock(struct vnode *vp,           // IN: Vnode of file to lock
            int cmd,                    // IN: Command
            struct shrlock *chr,        // IN: Lock
            int flag,                   // IN: Flags for XXX
            cred_t *cr)                 // IN: Credentials of caller
#else
static int
HgfsShrlock(struct vnode *vp,           // IN: Vnode of file to lock
            int cmd,                    // IN: Command
            struct shrlock *chr,        // IN: Lock
            int flag,                   // IN: Flags for XXX
            cred_t *cr,                 // IN: Credentials of caller
            caller_context_t *ctx)      // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsShrlock() NOTSUP.\n");

   return ENOTSUP;
}



/*
 *----------------------------------------------------------------------------
 *
 * HgfsVnevent --
 *
 *    Handles an event for the provided vnode.
 *
 *    Events can be VE_SUPPORT, VE_RENAME_SRC, VE_RENAME_DEST, VE_REMOVE,
 *    VE_RMDIR.
 *
 *    Note that this function showed up at some point after Build 52 (02/2004)
 *    of Solaris 10 but before (or at) Build 58 (06/2004).  We only compile
 *    this in if the driver is being built for Builds greater than 52.
 *    XXX We should find out what build this function first showed up on.
 *
 * Results:
 *    Returns zero on success and a non-zero error code on error.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

#if HGFS_VFS_VERSION > 2
#if HGFS_VFS_VERSION == 3
static int
HgfsVnevent(struct vnode *vp,        // IN: Vnode the event is occuring to
            vnevent_t event)         // IN: Event that has occurred
#else
static int
HgfsVnevent(struct vnode *vp,        // IN: Vnode the event is occuring to
            vnevent_t event,         // IN: Event that has occurred
            vnode_t *dvp,            // IN: XXX
            char *fnm,               // IN: XXX
            caller_context_t *ctx)   // IN: Context of caller
#endif
{
   DEBUG(VM_DEBUG_NOTSUP, "HgfsVnevent: ENOTSUP\n");

   return ENOTSUP;
}
#endif


/*
 * Local vnode functions.
 *
 * (The rest of the functions in this file are only invoked by our code so they
 *  ASSERT() their pointer arguments.)
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDirOpen --
 *
 *    Invoked when HgfsOpen() is called with a vnode of type VDIR.
 *
 *    Sends a SEARCH_OPEN request to the Hgfs server.
 *
 * Results:
 *    Returns zero on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDirOpen(HgfsSuperInfo *sip, // IN: Superinfo pointer
            struct vnode *vp)   // IN: Vnode of directory to open
{
   int ret;
   HgfsReq *req;
   HgfsRequestSearchOpen *request;
   HgfsReplySearchOpen *reply;

   ASSERT(sip);
   ASSERT(vp);

   DEBUG(VM_DEBUG_ENTRY, "HgfsDirOpen: opening \"%s\"\n", HGFS_VP_TO_FILENAME(vp));

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   /* Set the correct header values */
   request = (HgfsRequestSearchOpen *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_SEARCH_OPEN);

   /*
    * Convert name to cross-platform and unescape.  If the vnode is the root of
    * our filesystem the Hgfs server expects an empty string.
    */
   ret = CPName_ConvertTo((HGFS_IS_ROOT_VNODE(sip, vp)) ? "" : HGFS_VP_TO_FILENAME(vp),
                          MAXPATHLEN, request->dirName.name);
   if (ret < 0) {
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->dirName.length = ret;

   req->packetSize = request->dirName.length + sizeof *request;

   /* Submit the request to the Hgfs server */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   /* Our reply is in the request packet */
   reply = (HgfsReplySearchOpen *)req->packet;

   /* Perform basic validation of packet transfer */
   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
         DEBUG(VM_DEBUG_FAIL, "HgfsDirOpen(): invalid reply received.\n");
         ret = EPROTO;
         goto out;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsDirOpen: received reply for ID %d\n", reply->header.id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", reply->header.status);
   DEBUG(VM_DEBUG_COMM, " handle: %d\n", reply->search);

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed for [%s] with error %d.\n",
            __func__, HGFS_VP_TO_FILENAME(vp), ret);
      goto out;
   }

   if (req->packetSize != sizeof *reply) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDirOpen: incorrect packet size.\n");
      ret = EIO;
      goto out;
   }

   /* Set the search open handle for use in HgfsReaddir() */
   ret = HgfsSetOpenFileHandle(vp, reply->search);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDirOpen: couldn't assign handle=%d to %s\n",
         reply->search, HGFS_VP_TO_FILENAME(vp));
      req->state = HGFS_REQ_ERROR;
      ret = EINVAL;
      goto out;
   }

   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);

out:
   /* Make sure we put the request back on the list */
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileOpen --
 *
 *    Invoked when HgfsOpen() is called with a vnode of type VREG.  Sends
 *    a OPEN request to the Hgfs server.
 *
 *    Note that this function doesn't need to handle creations since the
 *    HgfsCreate() entry point is called by the kernel for that.
 *
 * Results:
 *    Returns zero on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsFileOpen(HgfsSuperInfo *sip,        // IN: Superinfo pointer
             struct vnode *vp,        // IN: Vnode of file to open
             int flag,                  // IN: Flags of open
             int permissions)           // IN: Permissions of open (only when creating)
{
   HgfsReq *req;
   HgfsRequestOpen *request;
   HgfsReplyOpen *reply;
   int ret;

   ASSERT(sip);
   ASSERT(vp);

   DEBUG(VM_DEBUG_ENTRY, "HgfsFileOpen: opening \"%s\"\n", HGFS_VP_TO_FILENAME(vp));

   req = HgfsGetNewReq(sip);
   if (!req) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: HgfsGetNewReq failed.\n");
      return EIO;
   }

   request = (HgfsRequestOpen *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_OPEN);

   /* Convert Solaris modes to Hgfs modes */
   ret = HgfsGetOpenMode((uint32_t)flag);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: HgfsGetOpenMode failed.\n");
      ret = EINVAL;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->mode = ret;
   DEBUG(VM_DEBUG_COMM, "HgfsFileOpen: open mode is %x\n", request->mode);

   /* Convert Solaris flags to Hgfs flags */
   ret = HgfsGetOpenFlags((uint32_t)flag);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: HgfsGetOpenFlags failed.\n");
      ret = EINVAL;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->flags = ret;
   DEBUG(VM_DEBUG_COMM, "HgfsFileOpen: open flags are %x\n", request->flags);

   request->permissions = (permissions & S_IRWXU) >> HGFS_ATTR_MODE_SHIFT;
   DEBUG(VM_DEBUG_COMM, "HgfsFileOpen: permissions are %o\n", request->permissions);

   /* Convert the file name to cross platform format. */
   ret = CPName_ConvertTo(HGFS_VP_TO_FILENAME(vp), MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: CPName_ConvertTo failed.\n");
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }
   request->fileName.length = ret;

   /* Packet size includes the request and its payload. */
   req->packetSize = request->fileName.length + sizeof *request;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: could not submit request.\n");
      goto out;
   }

   reply = (HgfsReplyOpen *)req->packet;

   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: request not valid.\n");
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed for [%s] with error %d.\n",
            __func__, HGFS_VP_TO_FILENAME(vp), ret);
      goto out;
   }

   if (req->packetSize != sizeof *reply) {
      DEBUG(VM_DEBUG_FAIL, "%s: size of reply is incorrect.\n", __func__);
      ret = EIO;
      goto out;
   }

   /*
    * We successfully received a reply, so we need to save the handle in
    * this file's HgfsOpenFile and return success.
    */
   ret = HgfsSetOpenFileHandle(vp, reply->file);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileOpen: couldn't assign handle %d (%s)\n",
            reply->file, HGFS_VP_TO_FILENAME(vp));
      req->state = HGFS_REQ_ERROR;
      ret = EINVAL;
      goto out;
   }

   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDirClose --
 *
 *    Invoked when HgfsClose() is called with a vnode of type VDIR.
 *
 *    Sends an SEARCH_CLOSE request to the Hgfs server.
 *
 * Results:
 *    Returns zero on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDirClose(HgfsSuperInfo *sip,        // IN: Superinfo pointer
             struct vnode *vp)          // IN: Vnode of directory to close
{
   HgfsReq *req;
   HgfsRequestSearchClose *request;
   HgfsReplySearchClose *reply;
   int ret;

   ASSERT(sip);
   ASSERT(vp);

   req = HgfsGetNewReq(sip);
   if (!sip) {
      return EIO;
   }

   /*
    * Prepare the request structure.  Of note here is that the request is
    * always the same size so we just set the packetSize to that.
    */
   request = (HgfsRequestSearchClose *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_SEARCH_CLOSE);

   /* Get this open file's handle, since that is what we want to close. */
   ret = HgfsGetOpenFileHandle(vp, &request->search);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDirClose: couldn't get handle for %s\n",
            HGFS_VP_TO_FILENAME(vp));
      ret = EINVAL;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }
   req->packetSize = sizeof *request;

   /* Submit the request to the Hgfs server */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplySearchClose *)req->packet;

   /* Ensure reply was received correctly and is necessary size. */
   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDirClose: invalid reply received.\n");
      ret = EPROTO;
      goto out;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsDirClose: received reply for ID %d\n", reply->header.id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", reply->header.status);

   /* Ensure server was able to close directory. */
   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   /* Now clear this open file's handle for future use. */
   ret = HgfsClearOpenFileHandle(vp);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: couldn't clear handle.\n", __func__);
      ret = EINVAL;
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   /* The directory was closed successfully so we return success. */
   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFileClose --
 *
 *    Invoked when HgfsClose() is called with a vnode of type VREG.
 *
 *    Sends a CLOSE request to the Hgfs server.
 *
 * Results:
 *    Returns zero on success and an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsFileClose(HgfsSuperInfo *sip,       // IN: Superinfo pointer
              struct vnode *vp)         // IN: Vnode of file to close
{
   HgfsReq *req;
   HgfsRequestClose *request;
   HgfsReplyClose *reply;
   int ret;

   ASSERT(sip);
   ASSERT(vp);

   req = HgfsGetNewReq(sip);
   if (!req) {
      ret = EIO;
      goto out;
   }

   request = (HgfsRequestClose *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_CLOSE);

   /* Tell the Hgfs server which handle to close */
   ret = HgfsGetOpenFileHandle(vp, &request->file);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileClose: couldn't get handle.\n");
      ret = EINVAL;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   req->packetSize = sizeof *request;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileClose: submit request failed.\n");
      goto out;
   }

   if (HgfsValidateReply(req, sizeof *reply) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsFileClose: reply was invalid.\n");
      ret = EPROTO;
      goto out;
   }

   reply = (HgfsReplyClose *)req->packet;

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   /*
    * We already verified the size of the reply above since this reply type
    * only contains a header, so we just clear the handle and return success.
    */
   ret = HgfsClearOpenFileHandle(vp);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: couldn't clear handle.\n", __func__);
      ret = EINVAL;
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);

out:
   HgfsDestroyReq(sip, req);
   DEBUG(VM_DEBUG_DONE, "HgfsFileClose: returning %d\n", ret);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetNextDirEntry --
 *
 *    Writes the name of the directory entry matching the handle and offset to
 *    nameOut.  This requires sending a SEARCH_READ request.
 *
 * Results:
 *    Returns zero on success and an error code on error.  The done value is
 *    set if there are no more directory entries.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsGetNextDirEntry(HgfsSuperInfo *sip,         // IN: Superinfo pointer
                    HgfsHandle handle,          // IN: Handle for request
                    uint32_t offset,            // IN: Offset
                    char *nameOut,              // OUT: Location to write name
                    Bool *done)                 // OUT: Whether there are any more
{
   HgfsReq *req;
   HgfsRequestSearchRead *request;
   HgfsReplySearchRead *reply;
   int ret;

   DEBUG(VM_DEBUG_ENTRY,
         "HgfsGetNextDirEntry: handle=%d, offset=%d.\n", handle, offset);

   ASSERT(sip);
   ASSERT(nameOut);
   ASSERT(done);

   req = HgfsGetNewReq(sip);
   if (!req) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetNextDirEntry: couldn't get req.\n");
      return EIO;
   }

   /*
    * Fill out the search read request that will return a single directory
    * entry for the provided handle at the given offset.
    */
   request = (HgfsRequestSearchRead *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_SEARCH_READ);

   request->search = handle;
   request->offset = offset;

   req->packetSize = sizeof *request;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetNextDirEntry: HgfsSubmitRequest failed.\n");
      goto out;
   }

   reply = (HgfsReplySearchRead *)req->packet;

   /* Validate the request state and ensure we have at least a header */
   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsGetNextDirEntry: reply not valid.\n");
      ret = EPROTO;
      goto out;
   }

   DEBUG(VM_DEBUG_COMM, "HgfsGetNextDirEntry: received reply for ID %d\n",
         reply->header.id);
   DEBUG(VM_DEBUG_COMM, " status: %d (see hgfsProto.h)\n", reply->header.status);

   /* Now ensure the server didn't have an error */
   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   /* Make sure we got an entire reply (excluding filename) */
   if (req->packetSize < sizeof *reply) {
      DEBUG(VM_DEBUG_FAIL, "%s: server didn't provide entire reply.\n",
            __func__);
      ret = EIO;
      goto out;
   }

   /* See if there are no more filenames to read */
   if (reply->fileName.length <= 0) {
      DEBUG(VM_DEBUG_DONE, "%s: no more directory entries.\n", __func__);
      *done = TRUE;
      ret = 0;         /* return success */
      goto out;
   }

   /* Make sure filename isn't too long */
   if ((reply->fileName.length > MAXNAMELEN) ||
       (reply->fileName.length > HGFS_PAYLOAD_MAX(reply)) ) {
      DEBUG(VM_DEBUG_FAIL, "%s: filename is too long.\n", __func__);
      ret = EOVERFLOW;
      goto out;
   }

   /*
    * Everything is all right, copy filename to caller's buffer.  Note that
    * Solaris directory entries don't need the attribute information in the
    * reply.
    */
   memcpy(nameOut, reply->fileName.name, reply->fileName.length);
   nameOut[reply->fileName.length] = '\0';
   ret = 0;

   DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoRead --
 *
 *    Sends a single READ request to the Hgfs server and writes the contents
 *    into the user's buffer if successful.
 *
 *    This function is called repeatedly by HgfsRead() with requests of size
 *    less than or equal to HGFS_IO_MAX.
 *
 * Results:
 *   Returns 0 on success and a positive value on error.
 *
 * Side effects:
 *   On success, up to 'size' bytes are written into the user's buffer.
 *   Actual number of bytes written passed back in 'count' argument.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoRead(HgfsSuperInfo *sip,  // IN: Superinfo pointer
           HgfsHandle handle,   // IN: Server's handle to read from
           uint64_t offset,     // IN: File offset to read at
           uint32_t size,       // IN: Number of bytes to read
           uio_t *uiop,         // IN: Defines user's read request
           uint32_t *count)     // OUT: Number of bytes read
{
   HgfsReq *req;
   HgfsRequestRead *request;
   HgfsReplyRead *reply;
   int ret;

   ASSERT(sip);
   ASSERT(uiop);
   ASSERT(size <= HGFS_IO_MAX); // HgfsRead() should guarantee this
   ASSERT(count);

   DEBUG(VM_DEBUG_ENTRY, "%s: entry.\n", __func__);

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   request = (HgfsRequestRead *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_READ);

   /* Indicate which file, where in the file, and how much to read. */
   request->file = handle;
   request->offset = offset;
   request->requiredSize = size;

   req->packetSize = sizeof *request;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: HgfsSubmitRequest failed.\n", __func__);
      goto out;
   }

   reply = (HgfsReplyRead *)req->packet;

   /* Ensure we got an entire header. */
   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "%s: invalid reply received.\n", __func__);
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   /*
    * Now perform checks on the actualSize.  There are three cases:
    *  o actualSize is less than or equal to size, which indicates success
    *  o actualSize is zero, which indicates the end of the file (and success)
    *  o actualSize is greater than size, which indicates a server error
    */
   if (reply->actualSize > size) {
      /* We got too much data: server error. */
      DEBUG(VM_DEBUG_FAIL, "%s: received too much data in payload.\n",
            __func__);
      ret = EPROTO;
      goto out;
   }

   /* Perform the copy to the user if we have something to copy */
   if (reply->actualSize > 0) {
      ret = uiomove(reply->payload, reply->actualSize, UIO_READ, uiop);
      if (ret) {
         DEBUG(VM_DEBUG_FAIL, "%s: uiomove failed, rc: %d\n.",
               __func__, ret);
         goto out;
      }
   }

   *count = reply->actualSize;
   DEBUG(VM_DEBUG_DONE, "%s: successfully read %d bytes to user.\n",
         __func__, *count);

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDoWrite --
 *
 *    Sends a single WRITE request to the Hgfs server with the contents of
 *    the user's buffer.
 *
 *    This function is called repeatedly by HgfsWrite() with requests of size
 *    less than or equal to HGFS_IO_MAX.
 *
 * Results:
 *   Returns number 0 on success and a positive value on error.
 *
 * Side effects:
 *   On success, up to 'size' bytes are written to the file specified by the
 *   handle. Actual number of bytes written passed back in 'count' argument.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDoWrite(HgfsSuperInfo *sip, // IN: Superinfo pointer
            HgfsHandle handle,  // IN: Handle representing file to write to
            int ioflag,         // IN: Flags for write
            uint64_t offset,    // IN: Where in the file to begin writing
            uint32_t size,      // IN: How much data to write
            uio_t *uiop,        // IN: Describes user's write request
            uint32_t *count)    // OUT: number of bytes written
{
   HgfsReq *req;
   HgfsRequestWrite *request;
   HgfsReplyWrite *reply;
   int ret;

   ASSERT(sip);
   ASSERT(uiop);
   ASSERT(size <= HGFS_IO_MAX); // HgfsWrite() guarantees this
   ASSERT(count);

   DEBUG(VM_DEBUG_ENTRY, "%s: entry.\n", __func__);

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   request = (HgfsRequestWrite *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, HGFS_OP_WRITE);

   request->file = handle;
   request->flags = 0;
   request->offset = offset;
   request->requiredSize = size;

   if (ioflag & FAPPEND) {
      DEBUG(VM_DEBUG_COMM, "%s: writing in append mode.\n", __func__);
      request->flags |= HGFS_WRITE_APPEND;
   }

   DEBUG(VM_DEBUG_COMM, "%s: requesting write of %d bytes.\n",
         __func__, size);

   /* Copy the data the user wants to write into the payload. */
   ret = uiomove(request->payload, request->requiredSize, UIO_WRITE, uiop);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL,
            "%s: uiomove(9F) failed copying data from user.\n", __func__);
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   /* We subtract one so request's 'char payload[1]' member isn't double counted. */
   req->packetSize = sizeof *request + request->requiredSize - 1;

   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: HgfsSubmitRequest failed.\n", __func__);
      goto out;
   }

   reply = (HgfsReplyWrite *)req->packet;

   if (HgfsValidateReply(req, sizeof reply->header) != 0) {
      DEBUG(VM_DEBUG_FAIL, "%s: invalid reply received.\n", __func__);
      ret = EPROTO;
      goto out;
   }

   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
      goto out;
   }

   if (req->packetSize != sizeof *reply) {
      DEBUG(VM_DEBUG_FAIL, "%s: invalid size of reply on successful reply.\n",
            __func__);
      ret = EPROTO;
      goto out;
   }

   /* The write was completed successfully, so return the amount written. */
   *count = reply->actualSize;
   DEBUG(VM_DEBUG_DONE, "%s: wrote %d bytes.\n", __func__, *count);

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDelete --
 *
 *    Sends a request to delete a file or directory.
 *
 * Results:
 *    Returns 0 on success or an error code on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsDelete(HgfsSuperInfo *sip,          // IN: Superinfo
           char *filename,              // IN: Full name of file to remove
           HgfsOp op)                   // IN: Hgfs operation this delete is for
{
   HgfsReq *req;
   HgfsRequestDelete *request;
   HgfsReplyDelete *reply;
   int ret;

   ASSERT(sip);
   ASSERT(filename);
   ASSERT((op == HGFS_OP_DELETE_FILE) || (op == HGFS_OP_DELETE_DIR));

   DEBUG(VM_DEBUG_ENTRY, "HgfsDelete().\n");

   req = HgfsGetNewReq(sip);
   if (!req) {
      return EIO;
   }

   /* Initialize the request's contents. */
   request = (HgfsRequestDelete *)req->packet;
   HGFS_INIT_REQUEST_HDR(request, req, op);

   /* Convert filename to cross platform and unescape. */
   ret = CPName_ConvertTo(filename, MAXPATHLEN, request->fileName.name);
   if (ret < 0) {
      ret = ENAMETOOLONG;
      /* We need to set the request state to error before destroying. */
      req->state = HGFS_REQ_ERROR;
      goto out;
   }

   request->fileName.length = ret;

   /* Set the size of our request. (XXX should this be - 1 for char[1]?) */
   req->packetSize = sizeof *request + request->fileName.length;

   DEBUG(VM_DEBUG_COMM, "HgfsDelete: deleting \"%s\"\n", filename);

   /* Submit our request. */
   ret = HgfsSubmitRequest(sip, req);
   if (ret) {
      goto out;
   }

   reply = (HgfsReplyDelete *)req->packet;

   /* Check the request status and size of reply. */
   if (HgfsValidateReply(req, sizeof *reply) != 0) {
      DEBUG(VM_DEBUG_FAIL, "HgfsDelete: invalid reply received.\n");
      ret = EPROTO;
      goto out;
   }

   /* Return the appropriate value. */
   ret = HgfsStatusConvertToSolaris(reply->header.status);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "%s: failed with error %d.\n",
            __func__, ret);
   } else {
      DEBUG(VM_DEBUG_DONE, "%s: done.\n", __func__);
   }

out:
   HgfsDestroyReq(sip, req);
   return ret;
}


/*
 * Function(s) exported to Solaris Hgfs code
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsGetSuperInfo --
 *
 *    Provides a pointer to the superinfo structure as long as the filesystem
 *    is mounted.
 *
 * Results:
 *    Pointer to the superinfo on success, NULL on failure.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

inline HgfsSuperInfo *
HgfsGetSuperInfo(void)
{
   return hgfsSuperInfo.vfsp ? &hgfsSuperInfo : NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsInitSuperInfo --
 *
 *    Initializes superinfo structure to indicate that filesystem has been
 *    mounted and can be used now.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsInitSuperInfo(struct vfs *vfsp) // IN: pointer to vfs being mounted
{
   hgfsSuperInfo.vfsp = vfsp;

   /* For now we are only using the backdoor transport. */
   hgfsSuperInfo.sendRequest = HgfsBackdoorSendRequest;
   hgfsSuperInfo.cancelRequest = HgfsBackdoorCancelRequest;
   hgfsSuperInfo.transportInit = HgfsBackdoorInit;
   hgfsSuperInfo.transportCleanup = HgfsBackdoorCleanup;

   HgfsInitRequestList(&hgfsSuperInfo);
   HgfsInitFileHashTable(&hgfsSuperInfo.fileHashTable);
}

/*
 *----------------------------------------------------------------------------
 *
 * HgfsClearSuperInfo --
 *
 *    Clears superinfo structure to indicate that filesystem has been
 *    unmounted.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsClearSuperInfo(void)
{
   hgfsSuperInfo.vfsp = NULL;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetVnodeOps --
 *
 *    Sets the vnode operations for the provided vnode.
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
HgfsSetVnodeOps(struct vnode *vp)       // IN: vnode for this file
{
   ASSERT(vp);

#if HGFS_VFS_VERSION == 2
   vp->v_op = &hgfsVnodeOps;
#else
   /* hgfsVnodeOpsP is set up when we mounted HGFS volume. */
   if (vn_getops(vp) == hgfsVnodeOpsP) {
      DEBUG(VM_DEBUG_INFO, "HgfsSetVnodeOps: vnode ops already set.\n");
   } else {
      DEBUG(VM_DEBUG_INFO, "HgfsSetVnodeOps: we had to set the vnode ops.\n");
      /* Set the operations for this vnode. */
      vn_setops(vp, hgfsVnodeOpsP);
   }
#endif

   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMakeVnodeOps --
 *
 *    Registers our vnode operations with the kernel.  After this function
 *    completes, all calls to vn_alloc() for our filesystem should return vnodes
 *    with the correct operations.
 *
 * Results:
 *    Return 0 on success and non-zero on failure.
 *
 * Side effects:
 *    The kernel allocates memory for our operations structure.
 *
 *----------------------------------------------------------------------------
 */

int
HgfsMakeVnodeOps(void)
{
#if HGFS_VFS_VERSION > 2
   int ret;
   static fs_operation_def_t vnodeOpsArr[] = {
      HGFS_VOP(VOPNAME_OPEN, vop_open, HgfsOpen),
      HGFS_VOP(VOPNAME_CLOSE, vop_close, HgfsClose),
      HGFS_VOP(VOPNAME_READ, vop_read, HgfsRead),
      HGFS_VOP(VOPNAME_WRITE, vop_write, HgfsWrite),
      HGFS_VOP(VOPNAME_IOCTL, vop_ioctl, HgfsIoctl),
      HGFS_VOP(VOPNAME_SETFL, vop_setfl, fs_setfl),
      HGFS_VOP(VOPNAME_GETATTR, vop_getattr, HgfsGetattr),
      HGFS_VOP(VOPNAME_SETATTR, vop_setattr, HgfsSetattr),
      HGFS_VOP(VOPNAME_ACCESS, vop_access, HgfsAccess),
      HGFS_VOP(VOPNAME_LOOKUP, vop_lookup, HgfsLookup),
      HGFS_VOP(VOPNAME_CREATE, vop_create, HgfsCreate),
      HGFS_VOP(VOPNAME_REMOVE, vop_remove, HgfsRemove),
      HGFS_VOP(VOPNAME_LINK, vop_link, HgfsLink),
      HGFS_VOP(VOPNAME_RENAME, vop_rename, HgfsRename),
      HGFS_VOP(VOPNAME_MKDIR, vop_mkdir, HgfsMkdir),
      HGFS_VOP(VOPNAME_RMDIR, vop_rmdir, HgfsRmdir),
      HGFS_VOP(VOPNAME_READDIR, vop_readdir, HgfsReaddir),
      HGFS_VOP(VOPNAME_SYMLINK, vop_symlink, HgfsSymlink),
      HGFS_VOP(VOPNAME_READLINK, vop_readlink, HgfsReadlink),
      HGFS_VOP(VOPNAME_FSYNC, vop_fsync, HgfsFsync),
#if HGFS_VFS_VERSION <= 3
      HGFS_VOP(VOPNAME_INACTIVE, vop_inactive, (fs_generic_func_p)HgfsInactive),
      HGFS_VOP(VOPNAME_RWLOCK, vop_rwlock, (fs_generic_func_p)HgfsRwlock),
      HGFS_VOP(VOPNAME_RWUNLOCK, vop_rwunlock, (fs_generic_func_p)HgfsRwunlock),
      HGFS_VOP(VOPNAME_MAP, vop_map, (fs_generic_func_p)HgfsMap),
      HGFS_VOP(VOPNAME_ADDMAP, vop_addmap, (fs_generic_func_p)HgfsAddmap),
      HGFS_VOP(VOPNAME_POLL, vop_poll, (fs_generic_func_p)fs_poll),
      HGFS_VOP(VOPNAME_DISPOSE, vop_dispose, (fs_generic_func_p)HgfsDispose),
#else
      HGFS_VOP(VOPNAME_INACTIVE, vop_inactive, HgfsInactive),
      HGFS_VOP(VOPNAME_RWLOCK, vop_rwlock, HgfsRwlock),
      HGFS_VOP(VOPNAME_RWUNLOCK, vop_rwunlock, HgfsRwunlock),
      HGFS_VOP(VOPNAME_MAP, vop_map, HgfsMap),
      HGFS_VOP(VOPNAME_ADDMAP, vop_addmap, HgfsAddmap),
      HGFS_VOP(VOPNAME_POLL, vop_poll, fs_poll),
      HGFS_VOP(VOPNAME_DISPOSE, vop_dispose, HgfsDispose),
#endif
      HGFS_VOP(VOPNAME_FID, vop_fid, HgfsFid),
      HGFS_VOP(VOPNAME_SEEK, vop_seek, HgfsSeek),
      HGFS_VOP(VOPNAME_CMP, vop_cmp, HgfsCmp),
      HGFS_VOP(VOPNAME_FRLOCK, vop_frlock, HgfsFrlock),
      HGFS_VOP(VOPNAME_SPACE, vop_space, HgfsSpace),
      HGFS_VOP(VOPNAME_REALVP, vop_realvp, HgfsRealvp),
      HGFS_VOP(VOPNAME_GETPAGE, vop_getpage, HgfsGetpage),
      HGFS_VOP(VOPNAME_PUTPAGE, vop_putpage, HgfsPutpage),
      HGFS_VOP(VOPNAME_DELMAP, vop_delmap, HgfsDelmap),
      HGFS_VOP(VOPNAME_DUMP, vop_dump, HgfsDump),
      HGFS_VOP(VOPNAME_PATHCONF, vop_pathconf, HgfsPathconf),
      HGFS_VOP(VOPNAME_PAGEIO, vop_pageio, HgfsPageio),
      HGFS_VOP(VOPNAME_DUMPCTL, vop_dumpctl, HgfsDumpctl),
      HGFS_VOP(VOPNAME_GETSECATTR, vop_getsecattr, fs_fab_acl),
      HGFS_VOP(VOPNAME_SETSECATTR, vop_setsecattr, HgfsSetsecattr),
      HGFS_VOP(VOPNAME_SHRLOCK, vop_shrlock, HgfsShrlock),
      HGFS_VOP(VOPNAME_VNEVENT, vop_vnevent, HgfsVnevent),
#if HGFS_VFS_VERSION <= 3
      { NULL, NULL }
#else
      { NULL, { NULL }                             }
#endif
   };

   DEBUG(VM_DEBUG_ENTRY, "HgfsMakeVnodeOps: making vnode ops.\n");

   /*
    * Create a vnodeops structure and register it with the kernel.
    * We save the operations structure so it can be assigned in the
    * future.
    */
   ret = vn_make_ops(HGFS_FS_NAME, vnodeOpsArr, &hgfsVnodeOpsP);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsMakeVnodeOps: vn_make_ops returned %d\n", ret);
      return ret;
   }

   DEBUG(VM_DEBUG_DONE, "HgfsMakeVnodeOps: hgfsVnodeOpsP=%p\n", hgfsVnodeOpsP);

#endif
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsFreeVnodeOps --
 *
 *    Unregisters vnode operations from the kernel.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The kernel frees memory allocated for our operations structure.
 *
 *----------------------------------------------------------------------------
 */

void
HgfsFreeVnodeOps(void)
{
#if HGFS_VFS_VERSION > 2
   if (hgfsVnodeOpsP)
      vn_freevnodeops(hgfsVnodeOpsP);
#endif
}


/*
 * Local utility functions.
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSubmitRequest --
 *
 *    Sends request through the transport channel, then waits for
 *    the response.
 *
 *    Both submitting request and waiting for reply are in this function
 *    because the signaling of the request list's condition variable and
 *    waiting on the request's condition variable must be atomic.
 *
 * Results:
 *    Returns zero on success, and an appropriate error code on error.
 *    Note: EINTR is returned if cv_wait_sig() is interrupted.
 *
 * Side effects:
 *    The request list's condition variable is signaled.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsSubmitRequest(HgfsSuperInfo *sip,   // IN: Superinfo containing request list,
                                        //     condition variable, and mutex
                  HgfsReq *req)         // IN: Request to submit
{
   int ret = 0;

   ASSERT(sip);
   ASSERT(req);

   mutex_enter(&sip->reqMutex);

   if (sip->vfsp->vfs_flag & VFS_UNMOUNTED) {
      DEBUG(VM_DEBUG_REQUEST, "HgfsSubmitRequest(): filesystem not mounted.\n");
      ret = ENODEV;
      goto out;
   }

   ret = HgfsSendRequest(sip, req);
   if (ret) {
      DEBUG(VM_DEBUG_REQUEST, "HgfsSubmitRequest(): transport failed.\n");
      goto out;
   }

   /*
    * If we are using synchronous transport we should have the result right
    * here and status will not be equal HGFS_REQ_SUBMITTED. If we are using
    * async transport we'll sleep till somebody wakes us up.
    */

   while (req->state == HGFS_REQ_SUBMITTED) {
      k_sigset_t oldIgnoreSet;

      //DEBUG(VM_DEBUG_SIG, "HgfsSubmitRequest: currproc is %s.\n", u.u_comm);

      HgfsDisableSignals(&oldIgnoreSet);

      if (cv_wait_sig(&req->condVar, &sip->reqMutex) == 0) {
         /*
          * We received a system signal (e.g., SIGKILL) while waiting for the
          * reply.
          *
          * Since we gave up the mutex while waiting on the condition
          * variable, we must make sure the reply didn't come /after/ we were
          * signaled but /before/ we reacquired the mutex.  We do this by
          * checking the state to make sure it is still SUBMITTED.  (Note that
          * this case should be quite rare, but is possible.)
          *
          * If the reply has come, we ignore it (since we were interrupted) and
          * clean up the request.  Otherwise we set the state to ABANDONED so
          * the device half knows we are no longer waiting for the reply and it
          * can clean up for us.
          */
         HgfsRestoreSignals(&oldIgnoreSet);

         DEBUG(VM_DEBUG_SIG, "HgfsSubmitRequest(): interrupted while waiting for reply.\n");

         if (req->state != HGFS_REQ_SUBMITTED) {
            /* It it's not SUBMITTED, it must be COMPLETED or ERROR */
            ASSERT(req->state == HGFS_REQ_COMPLETED ||
                   req->state == HGFS_REQ_ERROR);
            DEBUG(VM_DEBUG_REQUEST, "HgfsSubmitRequest(): request not in submitted status.\n");
         } else {
            DEBUG(VM_DEBUG_REQUEST, "HgfsSubmitRequest(): setting request state to abandoned.\n");
            req->state = HGFS_REQ_ABANDONED;
         }

         ret = EINTR;
         goto out;
      }

      HgfsRestoreSignals(&oldIgnoreSet);
   }

   /* The reply should now be in req->packet. */
   DEBUG(VM_DEBUG_SIG, "HgfsSubmitRequest(): awoken because reply received.\n");

 out:
   mutex_exit(&sip->reqMutex);
   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsValidateReply --
 *
 *    Validates a reply to ensure that its state is set appropriately and the
 *    reply is at least the minimum expected size and not greater than the
 *    maximum allowed packet size.
 *
 * Results:
 *    Returns zero on success, and a non-zero on error.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsValidateReply(HgfsReq *req,         // IN: Request that contains reply data
                  uint32_t minSize)     // IN: Minimum size expected for the reply
{
   ASSERT(req);
   ASSERT(minSize <= HGFS_PACKET_MAX);  /* we want to know if this fails */

   switch (req->state) {
   case HGFS_REQ_ERROR:
      DEBUG(VM_DEBUG_FAIL, "HgfsValidateReply(): received reply with error.\n");
      return -1;

   case HGFS_REQ_COMPLETED:
      if ((req->packetSize < minSize) || (req->packetSize > HGFS_PACKET_MAX)) {
         DEBUG(VM_DEBUG_FAIL, "HgfsValidateReply(): successfully "
               "completed reply is too small/big: !(%d < %d < %d).\n",
               minSize, req->packetSize, HGFS_PACKET_MAX);
         return -1;
      } else {
         return 0;
      }
   /*
    * If we get here then there is a programming error in this module:
    *  HGFS_REQ_UNUSED should be for requests in the free list
    *  HGFS_REQ_SUBMITTED should be for requests only that are awaiting
    *                     a response
    *  HGFS_REQ_ABANDONED should have returned an error to the client
    */
   default:
      NOT_REACHED();
      return -1;        /* avoid compiler warning */
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * HgfsStatusConvertToSolaris --
 *
 *    Convert a cross-platform HGFS status code to its kernel specific
 *    counterpart.
 *
 *    Rather than encapsulate the status codes within an array indexed by the
 *    various HGFS status codes, we explicitly enumerate them in a switch
 *    statement, saving the reader some time when matching HGFS status codes
 *    against Solaris status codes.
 *
 * Results:
 *    Zero if the converted status code represents success, positive error
 *    otherwise. Unknown status codes are converted to EPROTO.
 *
 * Side effects:
 *    None.
 *
 *-----------------------------------------------------------------------------
 */

static int
HgfsStatusConvertToSolaris(HgfsStatus hgfsStatus) // IN: Status code to convert
{
   switch (hgfsStatus) {
   case HGFS_STATUS_SUCCESS:
      return 0;

   case HGFS_STATUS_NO_SUCH_FILE_OR_DIR:
   case HGFS_STATUS_INVALID_NAME:
      return ENOENT;

   case HGFS_STATUS_INVALID_HANDLE:
      return EBADF;

   case HGFS_STATUS_OPERATION_NOT_PERMITTED:
      return EPERM;

   case HGFS_STATUS_FILE_EXISTS:
      return EEXIST;

   case HGFS_STATUS_NOT_DIRECTORY:
      return ENOTDIR;

   case HGFS_STATUS_DIR_NOT_EMPTY:
      return ENOTEMPTY;

   case HGFS_STATUS_PROTOCOL_ERROR:
      return EPROTO;

   case HGFS_STATUS_ACCESS_DENIED:
   case HGFS_STATUS_SHARING_VIOLATION:
      return EACCES;

   case HGFS_STATUS_NO_SPACE:
      return ENOSPC;

   case HGFS_STATUS_OPERATION_NOT_SUPPORTED:
      return EOPNOTSUPP;

   case HGFS_STATUS_NAME_TOO_LONG:
      return ENAMETOOLONG;

   case HGFS_STATUS_GENERIC_ERROR:
      return EIO;

   default:
      DEBUG(VM_DEBUG_LOG,
            "VMware hgfs: %s: unknown error: %u\n", __func__, hgfsStatus);
      return EPROTO;
   }
}


/*
 * XXX
 * These were taken and slightly modified from hgfs/driver/linux/driver.c.
 * Should we move them into a hgfs/driver/posix/driver.c?
 */


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenMode --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which open mode (access type) to request from
 *    the server.
 *
 * Results:
 *    Returns the correct HgfsOpenMode enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsGetOpenMode(uint32 flags) // IN: Open flags
{
#ifdef sun
   /*
    * Sun uses different values in the kernel.  These are defined in
    * <sys/file.h>.
    */
   #undef O_RDONLY
   #undef O_WRONLY
   #undef O_RDWR

   #define O_RDONLY     FREAD
   #define O_WRONLY     FWRITE
   #define O_RDWR       (FREAD | FWRITE)
#endif

   uint32 mask = O_RDONLY|O_WRONLY|O_RDWR;
   int result = -1;

#ifndef sun
   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenMode: entered\n"));
#else
   DEBUG(VM_DEBUG_LOG, "HgfsGetOpenMode: entered\n");
#endif

   /*
    * Mask the flags to only look at the access type.
    */
   flags &= mask;

   /* Pick the correct HgfsOpenMode. */
   switch (flags) {

   case O_RDONLY:
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenMode: O_RDONLY\n");
      result = HGFS_OPEN_MODE_READ_ONLY;
      break;

   case O_WRONLY:
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenMode: O_WRONLY\n");
      result = HGFS_OPEN_MODE_WRITE_ONLY;
      break;

   case O_RDWR:
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenMode: O_RDWR\n");
      result = HGFS_OPEN_MODE_READ_WRITE;
      break;

   default:
      /* This should never happen. */
      NOT_REACHED();
#ifndef sun
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenMode: invalid open flags %o\n",
             flags));
#else
      DEBUG(VM_DEBUG_LOG, "HgfsGetOpenMode: invalid open flags %o\n", flags);
#endif
      result = -1;
      break;
   }

   return result;
}


/*
 *----------------------------------------------------------------------
 *
 * HgfsGetOpenFlags --
 *
 *    Based on the flags requested by the process making the open()
 *    syscall, determine which flags to send to the server to open the
 *    file.
 *
 * Results:
 *    Returns the correct HgfsOpenFlags enumeration to send to the
 *    server, or -1 on failure.
 *
 * Side effects:
 *    None
 *
 *----------------------------------------------------------------------
 */

static int
HgfsGetOpenFlags(uint32 flags) // IN: Open flags
{
#ifdef sun
   /*
    * Sun uses different values inside the kernel.  These are defined in
    * <sys/file.h>.
    */
   #undef O_CREAT       // Must undef b/c included <sys/fs_subr.h>
   #undef O_TRUNC
   #undef O_EXCL

   #define O_CREAT      FCREAT
   #define O_TRUNC      FTRUNC
   #define O_EXCL       FEXCL
#endif

   uint32 mask = O_CREAT | O_TRUNC | O_EXCL;
   int result = -1;

#ifndef sun
   LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenFlags: entered\n"));
#else
   DEBUG(VM_DEBUG_INFO, "HgfsGetOpenFlags: entered\n");
#endif

   /*
    * Mask the flags to only look at O_CREAT, O_EXCL, and O_TRUNC.
    */

   flags &= mask;

   /* O_EXCL has no meaning if O_CREAT is not set. */
   if (!(flags & O_CREAT)) {
      flags &= ~O_EXCL;
   }

   /* Pick the right HgfsOpenFlags. */
   switch (flags) {

   case 0:
      /* Regular open; fails if file nonexistant. */
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenFlags: 0\n");
      result = HGFS_OPEN;
      break;

   case O_CREAT:
      /* Create file; if it exists already just open it. */
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenFlags: O_CREAT\n");
      result = HGFS_OPEN_CREATE;
      break;

   case O_TRUNC:
      /* Truncate existing file; fails if nonexistant. */
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenFlags: O_TRUNC\n");
      result = HGFS_OPEN_EMPTY;
      break;

   case (O_CREAT | O_EXCL):
      /* Create file; fail if it exists already. */
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenFlags: O_CREAT | O_EXCL\n");
      result = HGFS_OPEN_CREATE_SAFE;
      break;

   case (O_CREAT | O_TRUNC):
      /* Create file; if it exists already, truncate it. */
      DEBUG(VM_DEBUG_COMM, "HgfsGetOpenFlags: O_CREAT | O_TRUNC\n");
      result = HGFS_OPEN_CREATE_EMPTY;
      break;

   default:
      /*
       * This can only happen if all three flags are set, which
       * conceptually makes no sense because O_EXCL and O_TRUNC are
       * mutually exclusive if O_CREAT is set.
       *
       * However, the open(2) man page doesn't say you can't set all
       * three flags, and certain apps (*cough* Nautilus *cough*) do
       * so. To be friendly to those apps, we just silenty drop the
       * O_TRUNC flag on the assumption that it's safer to honor
       * O_EXCL.
       */
#ifndef sun
      LOG(4, (KERN_DEBUG "VMware hgfs: HgfsGetOpenFlags: invalid open "
              "flags %o. Ignoring the O_TRUNC flag.\n", flags));
#else
      DEBUG(VM_DEBUG_INFO, "HgfsGetOpenFlags: invalid open flags %o.  "
            "Ignoring the O_TRUNC flag.\n", flags);
#endif
      result = HGFS_OPEN_CREATE_SAFE;
      break;
   }

   return result;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsAttrToSolaris --
 *
 *    Maps Hgfs attributes to Solaris attributes, filling the provided Solaris
 *    attribute structure appropriately.
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
HgfsAttrToSolaris(struct vnode *vp,             // IN:  The vnode for this file
                  const HgfsAttr *hgfsAttr,     // IN:  Hgfs attributes to copy from
                  struct vattr *solAttr)        // OUT: Solaris attributes to fill
{
   ASSERT(vp);
   ASSERT(hgfsAttr);
   ASSERT(solAttr);

   DEBUG(VM_DEBUG_ENTRY, "HgfsAttrToSolaris: %p -> %p", hgfsAttr, solAttr);

   /*
    * We only fill in those fields that va_mask tells us to.
    */

   if (solAttr->va_mask & AT_TYPE) {
      /* Set the file type. */
      switch (hgfsAttr->type) {
      case HGFS_FILE_TYPE_REGULAR:
         solAttr->va_type = VREG;
         DEBUG(VM_DEBUG_ATTR, " Type: VREG\n");
         break;

      case HGFS_FILE_TYPE_DIRECTORY:
         solAttr->va_type = VDIR;
         DEBUG(VM_DEBUG_ATTR, " Type: VDIR\n");
         break;

      default:
         /*
          * There are only the above two filetypes.  If there is an error
          * elsewhere that provides another value, we set the Solaris type to
          * none and ASSERT in devel builds.
          */
         solAttr->va_type = VNON;
         DEBUG(VM_DEBUG_FAIL, "HgfsAttrToSolaris: invalid HgfsFileType provided.\n");
         ASSERT(0);
      }
   }

   if (solAttr->va_mask & AT_MODE) {
      /* We only have permissions for owners. */
      solAttr->va_mode = (hgfsAttr->permissions << HGFS_ATTR_MODE_SHIFT);
      DEBUG(VM_DEBUG_ATTR, " Owner's permissions: %o\n",
            solAttr->va_mode >> HGFS_ATTR_MODE_SHIFT);
   }

   if (solAttr->va_mask & AT_UID) {
      DEBUG(VM_DEBUG_ATTR, " Setting uid\n");
      solAttr->va_uid = 0;                 /* XXX root? */
   }

   if (solAttr->va_mask & AT_GID) {
      DEBUG(VM_DEBUG_ATTR, " Setting gid\n");
      solAttr->va_gid = 0;                 /* XXX root? */
   }

   if (solAttr->va_mask & AT_FSID) {
      DEBUG(VM_DEBUG_ATTR, " Setting fsid\n");
      solAttr->va_fsid = vp->v_vfsp->vfs_dev;
   }

   if (solAttr->va_mask & AT_NODEID) {
      /* Get the node id calculated for this file in HgfsVnodeGet() */
      solAttr->va_nodeid = HGFS_VP_TO_NODEID(vp);
      DEBUG(VM_DEBUG_ATTR, "*HgfsAttrToSolaris: fileName %s\n",
            HGFS_VP_TO_FILENAME(vp));
      DEBUG(VM_DEBUG_ATTR, " Node ID: %llu\n", solAttr->va_nodeid);
   }

   if (solAttr->va_mask & AT_NLINK) {
      DEBUG(VM_DEBUG_ATTR, " Setting nlink\n");
      solAttr->va_nlink = 1;               /* fake */
   }

   if (solAttr->va_mask & AT_SIZE) {
      DEBUG(VM_DEBUG_ATTR, " Setting size\n");
      solAttr->va_size = hgfsAttr->size;
   }

   if (solAttr->va_mask & AT_ATIME) {
      DEBUG(VM_DEBUG_ATTR, " Setting atime\n");
      HGFS_SET_TIME(solAttr->va_atime, hgfsAttr->accessTime);
   }

   if (solAttr->va_mask & AT_MTIME) {
      DEBUG(VM_DEBUG_ATTR, " Setting mtime\n");
      HGFS_SET_TIME(solAttr->va_mtime, hgfsAttr->writeTime);
   }

   if (solAttr->va_mask & AT_CTIME) {
      DEBUG(VM_DEBUG_ATTR, " Setting ctime\n");
      /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
      if (HGFS_SET_TIME(solAttr->va_ctime, hgfsAttr->attrChangeTime)) {
         solAttr->va_ctime = solAttr->va_mtime;
      }
   }

   if (solAttr->va_mask & AT_RDEV) {
      DEBUG(VM_DEBUG_ATTR, " Setting rdev\n");
      /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
      solAttr->va_rdev = 0;                /* devices aren't allowed in Hgfs */
   }

   if (solAttr->va_mask & AT_BLKSIZE) {
      DEBUG(VM_DEBUG_ATTR, " Setting blksize\n");
      /* Since Windows doesn't keep ctime, we may need to use mtime instead. */
      solAttr->va_blksize = HGFS_BLOCKSIZE;
   }

   if (solAttr->va_mask & AT_NBLOCKS) {
      DEBUG(VM_DEBUG_ATTR, " Setting nblocks\n");
      solAttr->va_nblocks = (solAttr->va_size / HGFS_BLOCKSIZE) + 1;
   }

#if HGFS_VFS_VERSION == 2
   if (solAttr->va_mask & AT_VCODE) {
      DEBUG(VM_DEBUG_ATTR, " Setting vcode\n");
      solAttr->va_vcode = 0;               /* fake */
   }
#else
   if (solAttr->va_mask & AT_SEQ) {
      DEBUG(VM_DEBUG_ATTR, " Setting seq\n");
      solAttr->va_seq = 0;                /* fake */
   }
#endif

   HgfsDebugPrintVattr(solAttr);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsSetattrCopy --
 *
 *    Sets the Hgfs attributes that need to be modified based on the provided
 *    Solaris attribute structure.
 *
 * Results:
 *    Returns TRUE if changes need to be made, FALSE otherwise.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static Bool
HgfsSetattrCopy(struct vattr *solAttr,          // IN:  Attributes to change to
                int flags,                      // IN:  Context of HgfsSetattr call
                HgfsAttr *hgfsAttr,             // OUT: Hgfs attributes to fill in
                HgfsAttrChanges *update)        // OUT: Hgfs attribute changes to make
{
   uint32_t mask;
   Bool ret = FALSE;

   ASSERT(solAttr);
   ASSERT(hgfsAttr);
   ASSERT(update);

   memset(hgfsAttr, 0, sizeof *hgfsAttr);
   memset(update, 0, sizeof *update);

   /* This is the mask of attributes to change. */
   mask = solAttr->va_mask;

   /*
    * Hgfs supports changing these attributes:
    * o mode bits (permissions)
    * o size
    * o access/write times
    */

   if (mask & AT_MODE) {
      DEBUG(VM_DEBUG_COMM, "HgfsSetattrCopy: updating permissions.\n");
      *update |= HGFS_ATTR_PERMISSIONS;
      hgfsAttr->permissions = (solAttr->va_mode & S_IRWXU) >> HGFS_ATTR_MODE_SHIFT;
      ret = TRUE;
   }

   if (mask & AT_SIZE) {
      DEBUG(VM_DEBUG_COMM, "HgfsSetattrCopy: updating size.\n");
      *update |= HGFS_ATTR_SIZE;
      hgfsAttr->size = solAttr->va_size;
      ret = TRUE;
   }

   if (mask & AT_ATIME) {
      DEBUG(VM_DEBUG_COMM, "HgfsSetattrCopy: updating access time.\n");
      *update |= HGFS_ATTR_ACCESS_TIME |
                 ((flags & ATTR_UTIME) ? HGFS_ATTR_ACCESS_TIME_SET : 0);
      hgfsAttr->accessTime = HGFS_GET_TIME(solAttr->va_atime);
      ret = TRUE;
   }

   if (mask & AT_MTIME) {
      DEBUG(VM_DEBUG_COMM, "HgfsSetattrCopy: updating write time.\n");
      *update |= HGFS_ATTR_WRITE_TIME |
                 ((flags & ATTR_UTIME) ? HGFS_ATTR_WRITE_TIME_SET : 0);
      hgfsAttr->writeTime = HGFS_GET_TIME(solAttr->va_mtime);
      ret = TRUE;
   }

   return ret;
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsMakeFullName --
 *
 *    Concatenates the path and filename to construct the full path.  This
 *    handles the special cases of . and .. filenames so the Hgfs server
 *    doesn't return an error.
 *
 * Results:
 *    Returns the length of the full path on success, and a negative value on
 *    error.  The full pathname is placed in outBuf.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

static int
HgfsMakeFullName(const char *path,      // IN:  Path of directory containing file
                 uint32_t pathLen,      // IN:  Length of path
                 const char *file,      // IN:  Name of file
                 char *outBuf,          // OUT: Location to write full path
                 ssize_t bufSize)       // IN:  Size of the out buffer
{

   ASSERT(path);
   ASSERT(file);
   ASSERT(outBuf);


   DEBUG(VM_DEBUG_INFO, "HgfsMakeFullName:\n"
         " path: \"%s\" (%d)\n"
         " file: \"%s\" (%ld)\n",
         path, pathLen, file, (long) strlen(file));

   /*
    * Here there are three possibilities:
    *  o file is ".", in which case we just place path in outBuf
    *  o file is "..", in which case we strip the last component from path and
    *    put that in outBuf
    *  o for all other cases, we concatenate path, a path separator, file, and
    *    a NUL terminator and place it in outBuf
    */

   /* Make sure that the path and a NUL terminator will fit. */
   if (bufSize < pathLen + 1) {
      return HGFS_ERR_INVAL;
   }


   /* Copy path for this file into the caller's buffer. */
   memset(outBuf, 0, bufSize);
   memcpy(outBuf, path, pathLen);

   /* Handle three cases. */
   if (strcmp(file, ".") == 0) {
      /* NUL terminate and return provided length. */
      outBuf[pathLen] = '\0';
      return pathLen;

   } else if (strcmp(file, "..") == 0) {
      /*
       * Replace the last path separator with a NUL terminator, then return the
       * size of the buffer.
       */
      char *newEnd = strrchr(outBuf, '/');
      if (!newEnd) {
         /*
          * We should never get here since we name the root vnode "/" in
          * HgfsMount().
          */
         return HGFS_ERR_INVAL;
      }

      *newEnd = '\0';
      return ((uintptr_t)newEnd - (uintptr_t)outBuf);

   } else {

      /*
       * The full path consists of path, the path separator, file, plus a NUL
       * terminator.  Make sure it will all fit.
       */
      int fileLen = strlen(file);
      if (bufSize < pathLen + 1 + fileLen + 1) {
         return HGFS_ERR_INVAL;
      }

      /*
       * The CPName_ConvertTo function handles multiple path separators
       * at the beginning of the filename, so we skip the checks to limit
       * them to one.  This also enables clobbering newEnd above to work
       * properly on base shares (named "//sharename") that need to turn into
       * "/".
       */
      outBuf[pathLen] = '/';

      /* Now append the filename and NUL terminator. */
      memcpy(outBuf + pathLen + 1, file, fileLen);
      outBuf[pathLen + 1 + fileLen] = '\0';

      return pathLen + 1 + fileLen;
   }
}


/*
 * Process signal mask manipulation
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDisableSignals --
 *
 *    Disables signals of current thread by calling sigintr().
 *
 * Results:
 *    Returns the old set of signals this process ignores.
 *
 * Side effects:
 *    This process is now only delivered SIGKILL sinals.
 *
 *----------------------------------------------------------------------------
 */

INLINE static void
HgfsDisableSignals(k_sigset_t *oldIgnoreSet)    // OUT: Current thread's ignore set
{
   ASSERT(oldIgnoreSet);

   /*
    * Passing sigintr() a 1 ensures that SIGINT will not be blocked.
    */
   sigintr(oldIgnoreSet, 1);

   /*
    * Note that the following alone works for Netscape ...
    * sigaddset(&curthread->t_hold, SIGALRM);
    */
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsRestoreSignals --
 *
 *    Restores the current process' set of signals to ignore to the provided
 *    signal set.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    The process will now be delivered signals as dictated by the oldSigSet.
 *
 *----------------------------------------------------------------------------
 */

INLINE static void
HgfsRestoreSignals(k_sigset_t *oldIgnoreSet)
{
   ASSERT(oldIgnoreSet);

   /*
    * sigunintr() will replace the current thread's blocked signals with the
    * provided signal set.
    */
   sigunintr(oldIgnoreSet);

   /*
    * Note that the following alone works for Netscape ...
    * sigdelset(&curthread->t_hold, SIGALRM);
    */
}


/*
 * This is a less-tested, alternate implementation of HgfsReaddir().  The
 * difference is that this one copies each entry individually so it doesn't
 * have to a malloc() a buffer of size readSize (see the XXX comment in the
 * HgfsReaddir() implementation above).  The thinking is that this approach is
 * likely safer, but has the potential to be slower.  Initial tests show that
 * this implementation "feels" the same speed as the other one.
 */
#if 0
static int
HgfsReaddir(struct vnode *vp,   // IN: Vnode of directory to read
            struct uio *uiop,   // IN: User's read request
            struct cred *cr,    // IN: Credentials of caller
            int *eofp)          // OUT: Indicates we are done
{
   HgfsSuperInfo *sip;
   HgfsHandle handle;
   struct dirent64 *dirp;
   char buf[sizeof *dirp + MAXNAMELEN];
   ssize_t readSize;
   uint64_t offset;
   Bool done;
   int ret;

   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir().\n");

   if (!vp || !uiop || !cr || !eofp) {
      cmn_err(HGFS_ERROR, "HgfsReaddir: NULL input from Kernel.\n");
      return EINVAL;
   }

   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir: uiop->uio_resid=%d, uiop->uio_loffset=%d\n",
         uiop->uio_resid, uiop->uio_loffset);


   /*
    * XXX: If would be nice if we could perform some sort of sanity check on
    * the handle here.  Perhaps make sure handle <= NUM_SEARCHES in
    * hgfsServer.c since the handle is the index number in searchArray.
    */
   if ( !HGFS_KNOW_FILENAME(vp) ) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: we don't know the filename.\n");
      return EBADF;
   }

   sip = HgfsGetSuperInfo();
   if (!sip) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: we can't get the superinfo.\n");
      return EIO;
   }

   /*
    * In order to fill the user's buffer with directory entries, we must
    * iterate on HGFS_OP_SEARCH_READ requests until either the user's buffer is
    * full or there are no more entries.  Each call to HgfsGetNextDirEntry()
    * fills in the name and attribute structure for the next entry.  We then
    * escape the name, create the directory entry in our temporary buf, and
    * copy the entry to the user's buffer.
    */

   readSize = uiop->uio_resid;
   dirp = (struct dirent64 *)buf;

   /*
    * We need to get the handle for this open directory to send to the Hgfs
    * server in our requests.
    */
   ret = HgfsGetOpenFileHandle(vp, &handle);
   if (ret) {
      DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: could not get handle.\n");
      return EINVAL;
   }

   /*
    * Loop until one of the following conditions is met:
    *  o An error occurs while reading a directory entry
    *  o There are no more directory entries to read
    *  o The buffer is full and cannot hold the next entry
    *
    * We request dentries from the Hgfs server based on their index in the
    * directory.  The offset value is initialized to the value specified in
    * the user's io request and is incremented each time through the loop.
    *
    * We decrement readSize by the size of the directory entry each time we
    * successfully copy one into the user's buffer.
    */
   for (offset = uiop->uio_loffset, done = 0; /* Nothing */ ; offset++) {
      char nameBuf[MAXNAMELEN + 1];
      char escName[MAXNAMELEN + 1];
      char fullName[MAXPATHLEN + 1];

      DEBUG(VM_DEBUG_COMM,
            "HgfsReaddir: getting directory entry at offset %d.\n", offset);

      memset(nameBuf, 0, sizeof nameBuf);
      memset(buf, 0, sizeof buf);

      ret = HgfsGetNextDirEntry(sip, handle, offset, nameBuf, &done);
      /* If the filename was too long, we skip to the next entry ... */
      if (ret == EOVERFLOW) {
         continue;
      /* ... but if another error occurred, we return that error code ... */
      } else if (ret) {
         DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: failure occurred in HgfsGetNextDirEntry\n");
         goto out;
      /*
       * ... and if there are no more entries, we set the end of file pointer
       * and break out of the loop.
       */
      } else if (done == TRUE) {
         DEBUG(VM_DEBUG_COMM, "HgfsReaddir: Done reading directory entries.\n");
         *eofp = TRUE;
         break;
      }

      /*
       * We now have the directory entry, so we sanitize the name and try to
       * put it in our buffer.
       */
      DEBUG(VM_DEBUG_COMM, "HgfsReaddir: received filename \"%s\"\n", nameBuf);

      memset(escName, 0, sizeof escName);

      ret = HgfsEscape_Do(nameBuf, strlen(nameBuf), MAXNAMELEN, escName);
      /* If the escaped name didn't fit in the buffer, skip to the next entry. */
      if (ret < 0 || ret > MAXNAMELEN) {
         DEBUG(VM_DEBUG_FAIL, "HgfsReaddir: HgfsEscape_Do failed.\n");
         continue;
      }

      /*
       * Make sure there is enough room in the buffer for the entire directory
       * entry.  If not, we just break out of the loop and copy what we have.
       */
      if (DIRENT64_RECLEN(ret) > readSize) {
         DEBUG(VM_DEBUG_INFO, "HgfsReaddir: ran out of room in the buffer.\n");
         break;
      }

      /* Fill in the directory entry. */
      dirp->d_reclen = DIRENT64_RECLEN(ret);
      dirp->d_off = offset;
      memcpy(dirp->d_name, escName, ret);
      dirp->d_name[ret] = '\0';

      ret = HgfsMakeFullName(HGFS_VP_TO_FILENAME(vp),           // Directorie's name
                             HGFS_VP_TO_FILENAME_LENGTH(vp),    // Length
                             dirp->d_name,                      // Name of file
                             fullName,                          // Destination buffer
                             sizeof fullName);                  // Size of this buffer
      /* Skip this entry if the full path was too long. */
      if (ret < 0) {
         continue;
      }

      /*
       * Place the node id, which serves the purpose of inode number, for this
       * filename directory entry.  As long as we are using a dirent64, this is
       * okay since ino_t is also a u_longlong_t.
       */
      HgfsNodeIdGet(&sip->fileHashTable, fullName, (uint32_t)ret,
                    (u_longlong_t *)&dirp->d_ino);

      /*
       * Now that we've filled our buffer with as many dentries as fit, we copy it
       * into the user's buffer.
       */
      ret = uiomove(dirp,               // Source buffer
                    dirp->d_reclen,     // Size of this buffer
                    UIO_READ,           // Read flag
                    uiop);              // User's request struct

      /* Break the loop if we can't copy this dentry into the user's buffer. */
      if (ret) {
         goto out;
      }

      /* Decrement the number of bytes copied on success */
      readSize -= dirp->d_reclen;
   }

   /* Return success */
   ret = 0;

out:
   /*
    * uiomove(9F) will have incremented the uio offset by the number of bytes
    * written.  We reset it here to the fs-specific offset in our directory so
    * the next time we are called it is correct.  (Note, this does not break
    * anything and /is/ how this field is intended to be used.)
    */
   uiop->uio_loffset = offset;  // XXX ok to do this on error too?

   DEBUG(VM_DEBUG_DONE, "HgfsReaddir: done (ret=%d, *eofp=%d).\n", ret, *eofp);
   DEBUG(VM_DEBUG_ENTRY, "HgfsReaddir: exiting.\n");
   return ret;
}
#endif
