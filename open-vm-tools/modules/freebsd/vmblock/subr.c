/* **********************************************************
 * Copyright 2007-2014 VMware, Inc.  All rights reserved.
 * **********************************************************/

/*
 * subr.c --
 *
 *	Subroutines for the VMBlock filesystem on FreeBSD.
 */


/*-
 * Copyright (c) 1992, 1993
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
 *	@(#)null_subr.c	8.7 (Berkeley) 5/14/95
 *
 * $FreeBSD: src/sys/fs/nullfs/null_subr.c,v 1.48.2.1 2006/03/13 03:05:17 jeff Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/file.h>

#include "compat_freebsd.h"

#include "vmblock_k.h"
#include "block.h"

#define LOG2_SIZEVNODE  8		/* log2(sizeof struct vnode) */
#define	NVMBLOCKCACHE   16              /* Number of hash buckets/chains */


/*
 * Local data
 */

/*
 * VMBlock layer cache:
 *    Each cache entry holds a reference to the lower vnode along with a
 *    pointer to the alias vnode.  When an entry is added the lower vnode
 *    is VREF'd.  When the alias is removed the lower vnode is vrele'd.
 */

#define	VMBLOCK_NHASH(vp) \
	(&nodeHashTable[(((uintptr_t)vp)>>LOG2_SIZEVNODE) & nodeHashMask])

/*
 * See hashinit(9).
 */
static LIST_HEAD(nodeHashHead, VMBlockNode) *nodeHashTable;
static u_long nodeHashMask;
static struct mtx hashMutex;

static MALLOC_DEFINE(M_VMBLOCKFSHASH, "VMBlockFS hash", "VMBlockFS hash table");
MALLOC_DEFINE(M_VMBLOCKFSNODE, "VMBlockFS node", "VMBlockFS vnode private part");

/* Defined for quick access to temporary pathname buffers. */
uma_zone_t VMBlockPathnameZone;


/*
 * Local functions
 */

static struct vnode * VMBlockHashGet(struct mount *, struct vnode *);
static struct vnode * VMBlockHashInsert(struct mount *, struct VMBlockNode *);


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockInit --
 *
 *      Initialize VMBlock file system.  Called when module first loaded into
 *      the kernel.
 *
 * Results:
 *      Zero.
 *
 * Side effects:
 *      None.
 *
 * Original comments:
 *      Initialise cache headers
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockInit(struct vfsconf *vfsp)       // ignored
{
   VMBLOCKDEBUG("VMBlockInit\n");      /* printed during system boot */
   nodeHashTable = hashinit(NVMBLOCKCACHE, M_VMBLOCKFSHASH, &nodeHashMask);
   mtx_init(&hashMutex, "vmblock-hs", NULL, MTX_DEF);
   VMBlockPathnameZone = uma_zcreate("VMBlock", MAXPATHLEN, NULL, NULL, NULL,
                                     NULL, UMA_ALIGN_PTR, 0);
   VMBlockSetupFileOps();
   BlockInit();
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockUninit --
 *
 *      Clean up when module is unloaded.
 *
 * Results:
 *      Zero always.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockUninit(struct vfsconf *vfsp)     // ignored
{
   mtx_destroy(&hashMutex);
   free(nodeHashTable, M_VMBLOCKFSHASH);
   BlockCleanup();
   uma_zdestroy(VMBlockPathnameZone);
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockHashGet --
 *
 *      "Return a VREF'ed alias for lower vnode if already exists, else 0.
 *      Lower vnode should be locked on entry and will be left locked on exit."
 *
 * Results:
 *      Pointer to upper layer/alias vnode if lowervp found, otherwise NULL.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static struct vnode *
VMBlockHashGet(struct mount *mp,        // IN: vmblock file system information
               struct vnode *lowervp)   // IN: lower vnode to search for
{
   struct nodeHashHead *hd;
   struct VMBlockNode *a;
   struct vnode *vp;

   ASSERT_VOP_LOCKED(lowervp, "hashEntryget");

   /*
    * Find hash base, and then search the (two-way) linked list looking
    * for a VMBlockNode structure which is referencing the lower vnode.
    * If found, the increment the VMBlockNode reference count (but NOT the
    * lower vnode's VREF counter).
    */
   hd = VMBLOCK_NHASH(lowervp);
   mtx_lock(&hashMutex);
   LIST_FOREACH(a, hd, hashEntry) {
      if (a->lowerVnode == lowervp && VMBTOVP(a)->v_mount == mp) {
         /*
          * Since we have the lower node locked the nullfs node can not be
          * in the process of recycling.  If it had been recycled before we
          * grabed the lower lock it would not have been found on the hash.
          */
         vp = VMBTOVP(a);
         vref(vp);
         mtx_unlock(&hashMutex);
         return vp;
      }
   }
   mtx_unlock(&hashMutex);
   return NULLVP;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockHashInsert --
 *
 *      "Act like VMBlockHashGet, but add passed VMBlockNode to hash if no
 *      existing node found."
 *
 * Results:
 *      Referenced, locked alias vnode if entry already in hash.  Otherwise
 *      NULLVP.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

static struct vnode *
VMBlockHashInsert(struct mount *mp,             // IN: VMBlock file system info
                  struct VMBlockNode *xp)       // IN: node to insert into hash
{
   struct nodeHashHead *hd;
   struct VMBlockNode *oxp;
   struct vnode *ovp;

   hd = VMBLOCK_NHASH(xp->lowerVnode);
   mtx_lock(&hashMutex);
   LIST_FOREACH(oxp, hd, hashEntry) {
      if (oxp->lowerVnode == xp->lowerVnode && VMBTOVP(oxp)->v_mount == mp) {
         /*
          * See hashEntryget for a description of this
          * operation.
          */
         ovp = VMBTOVP(oxp);
         vref(ovp);
         mtx_unlock(&hashMutex);
         return ovp;
      }
   }
   LIST_INSERT_HEAD(hd, xp, hashEntry);
   mtx_unlock(&hashMutex);
   return NULLVP;
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockHashRem --
 *
 *      Remove a VMBlockNode from the hash.
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
VMBlockHashRem(struct VMBlockNode *xp)  // IN: node to remove
{
   mtx_lock(&hashMutex);
   LIST_REMOVE(xp, hashEntry);
   mtx_unlock(&hashMutex);
}


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockInsMntQueDtr --
 *
 *      Do filesystem specific cleanup when recycling a vnode on a failed
 *      insmntque1 call.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

#if __FreeBSD_version >= 700055
static void
VMBlockInsMntQueDtr(struct vnode *vp, // IN: node to cleanup
		    void *xp)         // IN: FS private data
{
   vp->v_data = NULL;
   vp->v_vnlock = &vp->v_lock;
   free(xp, M_VMBLOCKFSNODE);
   vp->v_op = &dead_vnodeops;
   (void) compat_vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, curthread);
   vgone(vp);
   vput(vp);
}
#endif


/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockNodeGet --
 *
 *      Return a VMBlockNode mapped to the given lower layer vnode.
 *
 * Results:
 *      Zero on success, an appropriate system error otherwise.
 *
 * Side effects:
 *      In case of success takes over ownership of pathname thus caller
 *      has to dispose of it only if error was signalled.
 *
 * Original function comment:
 *
 *	Make a new or get existing nullfs node.  Vp is the alias vnode,
 *	lowervp is the lower vnode.
 *
 *	The lowervp assumed to be locked and having "spare" reference. This
 *	routine vrele lowervp if nullfs node was taken from hash. Otherwise it
 *	"transfers" the caller's "spare" reference to created nullfs vnode.
 *
 *-----------------------------------------------------------------------------
 */

int
VMBlockNodeGet(struct mount *mp,        // IN: VMBlock fs info
               struct vnode *lowervp,   // IN: lower layer vnode
               struct vnode **vpp,      // OUT: upper layer/alias vnode
               char         *pathname)  // IN: Pointer to the path we took to
                                        //     reach this vnode
{
   struct VMBlockNode *xp;
   struct vnode *vp;
   int error;

   /* Lookup the hash firstly */
   *vpp = VMBlockHashGet(mp, lowervp);
   if (*vpp != NULL) {
      vrele(lowervp);
      return 0;
   }

   /*
    * We do not serialize vnode creation, instead we will check for duplicates
    * later, when adding new vnode to hash.
    *
    * Note that duplicate can only appear in hash if the lowervp is locked
    * LK_SHARED.
    */

   /*
    * Do the malloc before the getnewvnode since doing so afterward might
    * cause a bogus v_data pointer to get dereferenced elsewhere if malloc
    * should block.
    */
   xp = malloc(sizeof *xp, M_VMBLOCKFSNODE, M_WAITOK|M_ZERO);

   error = getnewvnode("vmblock", mp, &VMBlockVnodeOps, &vp);
   if (error) {
      free(xp, M_VMBLOCKFSNODE);
      return error;
   }

   xp->name = pathname;
   xp->backVnode = vp;
   xp->lowerVnode = lowervp;
   vp->v_type = lowervp->v_type;
   vp->v_data = xp;
   vp->v_vnlock = lowervp->v_vnlock;
   if (vp->v_vnlock == NULL) {
      panic("VMBlockNodeGet: Passed a NULL vnlock.\n");
   }

   /* Before FreeBSD 7, insmntque was called by getnewvnode. */
#if __FreeBSD_version >= 700055
   error = insmntque1(vp, mp, VMBlockInsMntQueDtr, xp);
   if (error != 0) {
      return error;
   }
#endif

   /*
    * Atomically insert our new node into the hash or vget existing if
    * someone else has beaten us to it.
    *
    * ETA:  If a hash entry already exists, we'll be stuck with an orphaned
    * vnode and associated VMBlockNode.  By vrele'ng this vp, it'll be reclaimed
    * by the OS later.  That same process will take care of freeing the
    * VMBlockNode, too.
    */
   *vpp = VMBlockHashInsert(mp, xp);
   if (*vpp != NULL) {
      vrele(lowervp);
      vp->v_vnlock = &vp->v_lock;
      xp->lowerVnode = NULL;
      vrele(vp);
   } else {
      *vpp = vp;
   }

   return 0;
}


#ifdef DIAGNOSTIC                               /* if (DIAGNOSTIC) { */

/*
 *-----------------------------------------------------------------------------
 *
 * VMBlockCheckVp --
 *
 *      Sanity-checking intermediary used for debugging.  When module is
 *      compiled with FreeBSD macro "DIAGNOSTIC", every instance of
 *      VMBVPTOLOWERVP() calls this function to test vnodes' and VMBlockNodes'
 *      values, printing diagnostic information before panicing.  If the kernel
 *      debugger (KDB) is enabled, then this function will break to the debugger
 *      before a panic.
 *
 * Results:
 *      Valid pointer to a VMBlockNode's lower vnode.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

struct vnode *
VMBlockCheckVp(vp, fil, lno)
	struct vnode *vp;
	char *fil;
	int lno;
{
   struct VMBlockNode *a = VPTOVMB(vp);
#ifdef notyet
   /*
    * Can't do this check because vop_reclaim runs
    * with a funny vop vector.
    */
   if (vp->v_op != null_vnodeop_p) {
      printf ("VMBlockCheckVp: on non-null-node\n");
      panic("VMBlockCheckVp");
   };
#endif
   if (a->lowerVnode == NULLVP) {
      /* Should never happen */
      int i; u_long *p;
      printf("vp = %p, ZERO ptr\n", (void *)vp);
      for (p = (u_long *) a, i = 0; i < 8; i++) {
         printf(" %lx", p[i]);
      }
      printf("\n");
      panic("VMBlockCheckVp");
   }
   if (vrefcnt(a->lowerVnode) < 1) {
      int i; u_long *p;
      printf("vp = %p, unref'ed lowervp\n", (void *)vp);
      for (p = (u_long *) a, i = 0; i < 8; i++) {
         printf(" %lx", p[i]);
      }
      printf("\n");
      panic ("null with unref'ed lowervp");
   };
#ifdef notyet
   printf("null %x/%d -> %x/%d [%s, %d]\n", VMBTOVP(a), vrefcnt(VMBTOVP(a)),
      a->lowerVnode, vrefcnt(a->lowerVnode), fil, lno);
#endif
   return a->lowerVnode;
}
#endif                                          /* } [DIAGNOSTIC] */
