/* **********************************************************
 * Copyright (C) 2007 VMware, Inc.  All Rights Reserved.
 * **********************************************************/

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
 *	@(#)null.h	8.3 (Berkeley) 8/20/94
 *
 * $FreeBSD: src/sys/fs/nullfs/null.h,v 1.23 2005/03/15 13:49:33 jeff Exp $
 */

/*
 * vmblock_k.h --
 *
 *      Defnitions for entire vmblock module.
 */

#ifndef _VMBLOCK_K_H_
#define _VMBLOCK_K_H_

#ifdef _KERNEL

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/vnode.h>

#include "vm_basic_types.h"
#include "vm_assert.h"
#include "block.h"


/*
 * Macros
 */

#ifdef VMBLOCKFS_DEBUG
# define VMBLOCKFSDEBUG(format, args...)       printf(format ,## args)
#else
# define VMBLOCKFSDEBUG(format, args...)
#endif /* VMBLOCKFS_DEBUG */

#define MNTTOVMBLOCKMNT(mp)    ((struct VMBlockMount *)((mp)->mnt_data))
#define VPTOVMB(vp)            ((struct VMBlockNode *)(vp)->v_data)
#define VMBTOVP(xp)            ((xp)->backVnode)

/*
 * Debug logging
 */
#define VMBLOCK_DEBUG           LOG_DEBUG
#define VMBLOCK_ERROR           LOG_WARNING
#define VMBLOCK_ENTRY_LOGLEVEL  LOG_DEBUG
#define Warning(fmt, args...)   log(VMBLOCK_ERROR, fmt, ##args)

#ifdef VMX86_DEVEL
#   define LOG(level, fmt, args...)     printf(fmt, ##args)
#   define VMBLOCKDEBUG(fmt, args...)   log(VMBLOCK_DEBUG, fmt, ##args)
#   define Debug(level, fmt, args...)   log(VMBLOCK_DEBUG, fmt, ##args)
#else
#   define LOG(level, fmt, args...)
#   define VMBLOCKDEBUG(fmt, args...)
#   define Debug(level, fmt, args...)
#endif


/*
 * Describes a single mount instance
 */

typedef struct VMBlockMount {
   struct mount	*mountVFS;      /* Reference to mount parameters */
   struct vnode	*rootVnode;     /* Reference to root vnode */
} VMBlockMount;


/*
 * A cache of vnode references
 */

typedef struct VMBlockNode {
   LIST_ENTRY(VMBlockNode) hashEntry;   /* Hash chain element (contains ptr to
                                           next node, etc.) */
   struct vnode *lowerVnode;            /* VREFed once */
   struct vnode *backVnode;             /* Back pointer */
   char          *name;                 /* Looked up path to vnode */
} VMBlockNode;


/*
 * Global variables
 */

extern struct vop_vector VMBlockVnodeOps;
extern uma_zone_t VMBlockPathnameZone;


/*
 * Global functions
 */

int VMBlockInit(struct vfsconf *vfsp);
int VMBlockUninit(struct vfsconf *vfsp);
int VMBlockNodeGet(struct mount *mp, struct vnode *target, struct vnode **vpp,
                   char *pathname);
void VMBlockHashRem(struct VMBlockNode *xp);
void VMBlockSetupFileOps(void);
int VMBlockVopBypass(struct vop_generic_args *ap);

#ifdef DIAGNOSTIC
struct vnode *VMBlockCheckVp(struct vnode *vp, char *fil, int lno);
# define VMBVPTOLOWERVP(vp)     VMBlockCheckVp((vp), __FILE__, __LINE__)
#else
# define VMBVPTOLOWERVP(vp)     (VPTOVMB(vp)->lowerVnode)
#endif

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_VMBLOCKFSNODE);
#endif

#endif /* _KERNEL */
#endif /* _VMBLOCK_K_H_ */
