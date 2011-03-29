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

#ifndef __COMPAT_NAMEI_H__
#   define __COMPAT_NAMEI_H__

#include <linux/namei.h>

/*
 * In 2.6.25-rc2, dentry and mount objects were removed from the nameidata
 * struct. They were both replaced with a struct path.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#define compat_vmw_nd_to_dentry(nd) (nd).path.dentry
#else
#define compat_vmw_nd_to_dentry(nd) (nd).dentry
#endif

/* In 2.6.25-rc2, path_release(&nd) was replaced with path_put(&nd.path). */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 25)
#define compat_path_release(nd) path_put(&(nd)->path)
#else
#define compat_path_release(nd) path_release(nd)
#endif

/* path_lookup was removed in 2.6.39 merge window VFS merge */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 38)
#define compat_path_lookup(name, flags, nd)     kern_path(name, flags, &((nd)->path))
#else
#define compat_path_lookup(name, flags, nd)     path_lookup(name, flags, nd)
#endif

#endif /* __COMPAT_NAMEI_H__ */
