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
 * vfsopscommon.h --
 *
 * Common VFS vfsop implementations that are shared between both Mac OS and FreeBSD.
 */

#ifndef _HGFS_VFSOPS_COMMON_H_
#define _HGFS_VFSOPS_COMMON_H_

#include <sys/mount.h>
#include <sys/vnode.h>

/*
 * Macros
 */

#define HGFS_CONVERT_TO_BLOCKS(bytes) (bytes / HGFS_BLOCKSIZE)
#define HGFS_IS_POWER_OF_TWO(val) (val && !(val & (val - 1)))

#if defined __FreeBSD__
   typedef struct statfs HgfsStatfs;
#elif defined __APPLE__
   typedef struct vfsstatfs HgfsStatfs;
#endif


int
HgfsStatfsInt(struct vnode *vp, HgfsStatfs *stat);

#endif // _HGFS_VFSOPS_COMMON_H_
