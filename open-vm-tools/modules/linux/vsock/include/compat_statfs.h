/*********************************************************
 * Copyright (C) 2006 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_STATFS_H__
#   define __COMPAT_STATFS_H__

/* vfs.h simply include statfs.h, but it knows what directory statfs.h is in. */
#include <linux/vfs.h>

/* 2.5.74 renamed struct statfs to kstatfs. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 74)
#define compat_kstatfs kstatfs
#else
#define compat_kstatfs statfs
#endif

#endif /* __COMPAT_STATFS_H__ */
