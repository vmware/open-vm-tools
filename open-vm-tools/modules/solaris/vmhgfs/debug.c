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
 * debug.c --
 *
 * Routines for debugging Solaris kernel module.
 *
 */


#include "debug.h"
#include "filesystem.h"
#ifndef SOL9
#include <sys/cred_impl.h>
#endif


/*
 * Functions
 */


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintVfssw --
 *
 *    Prints the provided VFS Switch structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintVfssw(char *str, struct vfssw *vfsswp)
{
   ASSERT(str);
   ASSERT(vfsswp);

   DEBUG(VM_DEBUG_STRUCT, "struct vfssw from %s\n", str);
   DEBUG(VM_DEBUG_STRUCT, " vsw_name    : %s\n",
	 (vfsswp->vsw_name) ? vfsswp->vsw_name : "NULL");
   DEBUG(VM_DEBUG_STRUCT, " vsw_init    : %p\n", vfsswp->vsw_init);
   DEBUG(VM_DEBUG_STRUCT, " vsw_flag    : %x\n", vfsswp->vsw_flag);
#  ifdef SOL9
   DEBUG(VM_DEBUG_STRUCT, " vsw_vfsops  : %x\n", vfsswp->vsw_vfsops);
   DEBUG(VM_DEBUG_STRUCT, " vsw_optproto: %x\n", vfsswp->vsw_optproto);
#  endif
   DEBUG(VM_DEBUG_STRUCT, " vsw_count   : %d\n", vfsswp->vsw_count);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintVfs --
 *
 *    Prints the provided VFS structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintVfs(char *str, struct vfs *vfsp)
{
   ASSERT(str);
   ASSERT(vfsp);

   DEBUG(VM_DEBUG_STRUCT, "struct vfs from %s\n", str);
   DEBUG(VM_DEBUG_STRUCT, " vfs_next        : %p\n", vfsp->vfs_next);
   DEBUG(VM_DEBUG_STRUCT, " vfs_op          : %p\n", vfsp->vfs_op);
   DEBUG(VM_DEBUG_STRUCT, " vfs_vnodecovered: %p\n", vfsp->vfs_vnodecovered);
   DEBUG(VM_DEBUG_STRUCT, " vfs_flag        : %d\n", vfsp->vfs_flag);
   DEBUG(VM_DEBUG_STRUCT, " vfs_bsize       : %d\n", vfsp->vfs_bsize);
   DEBUG(VM_DEBUG_STRUCT, " vfs_fstype      : %d\n", vfsp->vfs_fstype);
#  ifdef SOL9
   DEBUG(VM_DEBUG_STRUCT, " vfs_fsid        : %d\n", vfsp->vfs_fsid);
#  else
   DEBUG(VM_DEBUG_STRUCT, " vfs_fsid.val[0] : %d\n", vfsp->vfs_fsid.val[0]);
   DEBUG(VM_DEBUG_STRUCT, " vfs_fsid.val[1] : %d\n", vfsp->vfs_fsid.val[1]);
#  endif
   DEBUG(VM_DEBUG_STRUCT, " vfs_vadata      : %p\n", vfsp->vfs_data);
   DEBUG(VM_DEBUG_STRUCT, " vfs_dev         : %lu\n", vfsp->vfs_dev);
   DEBUG(VM_DEBUG_STRUCT, " vfs_bcount      : %lu\n", vfsp->vfs_bcount);
#  ifdef SOL9
   DEBUG(VM_DEBUG_STRUCT, " vfs_nsubmounts  : %d\n", vfsp->vfs_nsubmounts);
#  endif
   DEBUG(VM_DEBUG_STRUCT, " vfs_list        : %p\n", vfsp->vfs_list);
   DEBUG(VM_DEBUG_STRUCT, " vfs_hash        : %p\n", vfsp->vfs_hash);
   DEBUG(VM_DEBUG_STRUCT, " vfs_reflock     : %p\n", &vfsp->vfs_reflock);
   DEBUG(VM_DEBUG_STRUCT, " vfs_count       : %d\n", vfsp->vfs_count);
#  ifdef SOL9
   DEBUG(VM_DEBUG_STRUCT, " vfs_mntopts     : %x\n", vfsp->vfs_mntopts);
   DEBUG(VM_DEBUG_STRUCT, " vfs_resource    : %s\n",
	 (vfsp->vfs_resource) ? vfsp->vfs_resource : "NULL");
#  endif
   DEBUG(VM_DEBUG_STRUCT, " vfs_mtime       : %ld\n", vfsp->vfs_mtime);

}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintVnode --
 *
 *    Prints the provided vnode structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintVnode(uint32 level, char *str,
                    struct vnode *vnodep, Bool printFileName)
{
   ASSERT(str);
   ASSERT(vnodep);

   DEBUG(level, "struct vnode from %s located at %p\n", str, vnodep);
   DEBUG(level, " v_lock          : %p\n", &vnodep->v_lock);
   DEBUG(level, " v_flag          : %d\n", vnodep->v_flag);
   DEBUG(level, " v_count         : %d\n", vnodep->v_count);
   DEBUG(level, " v_vfsmountedhere: %p\n", vnodep->v_vfsmountedhere);
   DEBUG(level, " v_op            : %p\n", vnodep->v_op);
   DEBUG(level, " v_vfsp          : %p\n", vnodep->v_vfsp);
   DEBUG(level, " v_stream        : %p\n", vnodep->v_stream);
   DEBUG(level, " v_pages         : %p\n", vnodep->v_pages);
#  ifdef SOL9
   DEBUG(level, " v_next          : %p\n", vnodep->v_next);
   DEBUG(level, " v_prev          : %p\n", vnodep->v_prev);
#  endif
   DEBUG(level, " v_type          : %d\n", vnodep->v_type);
   DEBUG(level, " v_rdev          : %lu\n", vnodep->v_rdev);
   DEBUG(level, " v_data          : %p\n", vnodep->v_data);
   DEBUG(level, " v_filocks       : %p\n", vnodep->v_filocks);
   DEBUG(level, " v_shrlocks      : %p\n", vnodep->v_shrlocks);
   DEBUG(level, " v_cv            : %p\n", &vnodep->v_cv);
   DEBUG(level, " v_locality      : %p\n", vnodep->v_locality);
   DEBUG(level, " v_nbllock       : %p\n", &vnodep->v_nbllock);

   if (printFileName && HGFS_VP_TO_OFP(vnodep) && HGFS_VP_TO_FP(vnodep)) {
      DEBUG(level, " filename        : %s\n", HGFS_VP_TO_FILENAME(vnodep));
   }
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintCred --
 *
 *    Prints the provided cred structure the describes the credentials of the
 *    caller.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintCred(char *str, struct cred *credp)
{
   DEBUG(VM_DEBUG_STRUCT, "struct cred from %s\n", str);
   DEBUG(VM_DEBUG_STRUCT, " cr_ref    : %d\n", credp->cr_ref);
   DEBUG(VM_DEBUG_STRUCT, " cr_uid    : %d\n", credp->cr_uid);
   DEBUG(VM_DEBUG_STRUCT, " cr_gid    : %d\n", credp->cr_gid);
   DEBUG(VM_DEBUG_STRUCT, " cr_ruid   : %d\n", credp->cr_ruid);
   DEBUG(VM_DEBUG_STRUCT, " cr_rgid   : %d\n", credp->cr_rgid);
   DEBUG(VM_DEBUG_STRUCT, " cr_suid   : %d\n", credp->cr_suid);
   DEBUG(VM_DEBUG_STRUCT, " cr_sgid   : %d\n", credp->cr_sgid);
   DEBUG(VM_DEBUG_STRUCT, " cr_ngroups: %d\n", credp->cr_ngroups);
   DEBUG(VM_DEBUG_STRUCT, " cr_groups : %p\n", credp->cr_groups);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugMounta --
 *
 *    Prints the provided mounta structure that describes the arguments
 *    provided to users.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintMounta(char *str, struct mounta *mntp)
{
   ASSERT(str);
   ASSERT(mntp);

   DEBUG(VM_DEBUG_STRUCT, "struct mounta from %s\n", str);
   DEBUG(VM_DEBUG_STRUCT, " spec    : %s\n",
	 (mntp->spec) ? mntp->spec : "NULL");
   DEBUG(VM_DEBUG_STRUCT, " dir     : %s\n",
	 (mntp->dir) ? mntp->dir : "NULL");
   DEBUG(VM_DEBUG_STRUCT, " flags   : %x\n", mntp->flags);
   DEBUG(VM_DEBUG_STRUCT, " fstype  : %s\n",
	 (mntp->fstype) ? mntp->fstype : "NULL");
   DEBUG(VM_DEBUG_STRUCT, " dataptr : %p\n", mntp->dataptr);
   DEBUG(VM_DEBUG_STRUCT, " datalen : %d\n", mntp->datalen);
   DEBUG(VM_DEBUG_STRUCT, " optptr  : %p\n", mntp->optptr);
   DEBUG(VM_DEBUG_STRUCT, " optlen  : %d\n", mntp->optlen);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintVattr --
 *
 *    Prints the contents of an attributes structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *----------------------------------------------------------------------------
 */

INLINE void
HgfsDebugPrintVattr(const struct vattr *vap)
{
   DEBUG(VM_DEBUG_STRUCT, " va_mask: %x\n", vap->va_mask);
   DEBUG(VM_DEBUG_STRUCT, " va_type: %d\n", vap->va_type);
   DEBUG(VM_DEBUG_STRUCT, " va_mode: %x\n", vap->va_mode);
   DEBUG(VM_DEBUG_STRUCT, " va_uid:  %u\n", vap->va_uid);
   DEBUG(VM_DEBUG_STRUCT, " va_gid: %u\n", vap->va_gid);
   DEBUG(VM_DEBUG_STRUCT, " va_fsid: %lu\n", vap->va_fsid);
   DEBUG(VM_DEBUG_STRUCT, " va_nodeid: %llu\n", vap->va_nodeid);
   DEBUG(VM_DEBUG_STRUCT, " va_nlink: %x\n", vap->va_nlink);
   DEBUG(VM_DEBUG_STRUCT, " va_size: %llu\n", vap->va_size);
   DEBUG(VM_DEBUG_STRUCT, " va_atime.tv_sec: %ld\n", vap->va_atime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_atime.tv_nsec: %ld\n", vap->va_atime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_mtime.tv_sec: %ld\n", vap->va_mtime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_mtime.tv_nsec: %ld\n", vap->va_mtime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_ctime.tv_sec: %ld\n", vap->va_ctime.tv_sec);
   DEBUG(VM_DEBUG_STRUCT, " va_ctime.tv_nsec: %ld\n", vap->va_ctime.tv_nsec);
   DEBUG(VM_DEBUG_STRUCT, " va_rdev: %lu\n", vap->va_rdev);
   DEBUG(VM_DEBUG_STRUCT, " va_blksize: %u\n", vap->va_blksize);
   DEBUG(VM_DEBUG_STRUCT, " va_nblocks: %llu\n", vap->va_nblocks);
#ifdef SOL9
   DEBUG(VM_DEBUG_STRUCT, " va_vcode: %u\n", vap->va_vcode);
#else
   DEBUG(VM_DEBUG_STRUCT, " va_seq: %u\n", vap->va_seq);
#endif
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintReqList --
 *
 *    For debugging.  Prints out the request list for the provided list
 *    anchor.
 *    Note: Assumes called with the list lock held.
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
HgfsDebugPrintReqList(DblLnkLst_Links *listAnchor)   // IN: Anchor of list to print
{
   DblLnkLst_Links *currNode;
   HgfsReq *currReq;

   ASSERT(listAnchor);

   DEBUG(VM_DEBUG_STRUCT, "Request List:\n");
   DEBUG(VM_DEBUG_STRUCT, " anchor: %p\n", listAnchor);

   for (currNode = listAnchor->next; currNode != listAnchor; currNode = currNode->next)
   {
      currReq = DblLnkLst_Container(currNode, HgfsReq, listNode);
      DEBUG(VM_DEBUG_STRUCT, " address: %p (id=%d)\n",
            currReq, currReq->id);
   }

   DEBUG(VM_DEBUG_STRUCT, "--DONE--\n");
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintReq --
 *
 *    Prints the relevant portions of the provided HgfsReq structure.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrintReq(const char *str,
                  HgfsReq *req)
{
   ASSERT(str);
   ASSERT(req);

   DEBUG(VM_DEBUG_STRUCT, "struct HgfsReq from %s\n", str);
   DEBUG(VM_DEBUG_STRUCT, " id: %d\n", req->id);
   DEBUG(VM_DEBUG_STRUCT, " listNode: %p\n", &req->listNode);
   DEBUG(VM_DEBUG_STRUCT, "  next=%p\n", req->listNode.next);
   DEBUG(VM_DEBUG_STRUCT, "  prev=%p\n", req->listNode.prev);
   DEBUG(VM_DEBUG_STRUCT, " packetSize: %d\n", req->packetSize);
   DEBUG(VM_DEBUG_STRUCT, " state: %d (see hgfsSolaris.h)\n", req->state);
}


/*
 *----------------------------------------------------------------------------
 *
 * HgfsDebugPrintReqPool --
 *
 *    Prints the contents if the request pool.
 *
 * Results:
 *    None.
 *
 * Side effects:
 *    None.
 *
 *
 *----------------------------------------------------------------------------
 */

void
HgfsDebugPrintReqPool(const char *str)
{
   int i;

   ASSERT(str);

   DEBUG(VM_DEBUG_STRUCT, "Request pool from %s\n", str);

   for (i = 0; i < ARRAYSIZE(requestPool); i++) {
      DEBUG(VM_DEBUG_STRUCT, " Index: %d, ID: %d\n", i, requestPool[i].id);
      DEBUG(VM_DEBUG_STRUCT, " listNode: %p\n", &requestPool[i].listNode);
      DEBUG(VM_DEBUG_STRUCT, "  next=%p\n", requestPool[i].listNode.next);
      DEBUG(VM_DEBUG_STRUCT, "  prev=%p\n", requestPool[i].listNode.prev);
      DEBUG(VM_DEBUG_STRUCT, " packetSize: %d\n", requestPool[i].packetSize);
      DEBUG(VM_DEBUG_STRUCT, " state: %d (see hgfsSolaris.h)\n", requestPool[i].state);
   }

   DEBUG(VM_DEBUG_STRUCT, "--request pool done--\n");
}


/*
 * There is a problem in Solaris 9's header files when using the va_start
 * and va_end macros, so we manually do what the preprocessor would have
 * done here.
 *
 * Note, the line using __builtin_next_arg is equivalent to:
 * args = ((char *)(&fmt) + sizeof (char *));
 *
 * That is, it just provides a pointer to the unnamed first variable
 * argument.
 */
#ifdef SOL9
# define compat_va_start(arg, fmt) arg = ((char *)__builtin_next_arg(fmt))
# define compat_va_end(arg)
#else
# define compat_va_start(arg, fmt) va_start(arg, fmt)
# define compat_va_end(arg)        va_end(arg)
#endif

static void
vLog(const char *fmt,
     va_list args)
{
#ifdef VM_DEBUG_LEV
   char buffer[1024];

   /*
    * We check this here to avoid unnecessarily manipulating buffer if we
    * aren't even going to print the log.
    */
   if (VM_DEBUG_LOG & VM_DEBUG_LEV) {
      vsprintf(buffer, fmt, args);
      cmn_err(HGFS_DEBUG, "%s", buffer);
   }
#endif
}


/*
 * For compatibility with existing code.
 */

void
Log(const char *fmt, ...)     // IN: format string, etc
{
   va_list args;

   compat_va_start(args, fmt);
   vLog(fmt, args);
   compat_va_end(args);
}


/*
 * For compatibility with existing code.
 */

void
Debug(const char *fmt, ...)   // IN: format string, etc.
{
   va_list args;

   compat_va_start(args, fmt);
   vLog(fmt, args);
   compat_va_end(args);
}


#undef compat_va_list
#undef compat_va_start
#undef compat_va_end
