/*********************************************************
 * Copyright (C) 2008 VMware, Inc. All rights reserved.
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
 * vnopscommon.h --
 *
 * Common VFS vnop implementations that are shared between both Mac OS and FreeBSD.
 */

#ifndef _HGFS_VNOPS_COMMON_H_
#define _HGFS_VNOPS_COMMON_H_

#include "hgfs_kernel.h"

/*
 * Macros
 */

/* Access uio struct information in a Mac OS / FreeBSD independent manner. */
#if defined __FreeBSD__
#define HGFS_UIOP_TO_RESID(uiop)                                \
                ((uiop)->uio_resid)
#define HGFS_UIOP_TO_OFFSET(uiop)                               \
                ((uiop)->uio_offset)
#define HGFS_UIOP_SET_OFFSET(uiop, offset)                      \
                ((uiop)->uio_offset = (offset))
#elif defined __APPLE__
#define HGFS_UIOP_TO_RESID(uiop)                                \
                (uio_resid(uiop))
#define HGFS_UIOP_TO_OFFSET(uiop)                               \
		(uio_offset(uiop))
#define HGFS_UIOP_SET_OFFSET(uiop, offset)                      \
                (uio_setoffset(uiop, offset))
#endif

/* Access vnode struct information in a Mac OS / FreeBSD independent manner. */
#if defined __FreeBSD__
#define HGFS_VP_TO_VTYPE(vp)                                    \
                (vp->v_type)
#define HGFS_VPP_GET_IOCOUNT(vpp)                                   \
                (vref(*vpp)) 
#elif defined __APPLE__
#define HGFS_VP_TO_VTYPE(vp)                                    \
                (vnode_vtype(vp))
#define HGFS_VPP_GET_IOCOUNT(vpp)                                 \
                (vnode_get(*vpp))
#endif

/* Internal Vnops functions used by both FreeBSD and Mac OS */
int HgfsReaddirInt(struct vnode *vp, struct uio *uiop, int *eofp);
int HgfsSetattrInt(struct vnode *vp, HgfsVnodeAttr *vap);
int HgfsGetattrInt(struct vnode *vp, HgfsVnodeAttr *vap);
int HgfsRmdirInt(struct vnode *dvp, struct vnode *vp,
		 struct componentname *cnp);
int HgfsRemoveInt(struct vnode *vp);
int HgfsCloseInt(struct vnode *vp, int mode);
int HgfsOpenInt(struct vnode *vp, int fflag, HgfsOpenType openType);
int HgfsLookupInt(struct vnode *dvp, struct vnode **vpp,
		  struct componentname *cnp);
int HgfsCreateInt(struct vnode *dvp, struct vnode **vpp,
		  struct componentname *cnp, int mode);
int HgfsReadInt(struct vnode *vp, struct uio *uiop, Bool pagingIo);
int HgfsReadlinkInt(struct vnode *vp, struct uio *uiop);
int HgfsWriteInt(struct vnode *vp, struct uio *uiop,
                 int ioflag, Bool pagingIo);
int HgfsMkdirInt(struct vnode *dvp, struct vnode **vpp,
		 struct componentname *cnp, int mode);
int HgfsRenameInt(struct vnode *fvp,
		  struct vnode *tdvp, struct vnode *tvp,
		  struct componentname *tcnp);
int HgfsAccessInt(struct vnode *vp, HgfsAccessMode mode);
int HgfsSymlinkInt(struct vnode *dvp,
                   struct vnode **vpp,
                   struct componentname *cnp,
                   char *targetName);
int HgfsMmapInt(struct vnode *vp, int accessMode);
int HgfsMnomapInt(struct vnode *vp);

#endif // _HGFS_VNOPS_COMMON_H_
