/*********************************************************
 * Copyright (C) 2013 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_DCACHE_H__
#   define __COMPAT_DCACHE_H__

#include <linux/dcache.h>

/*
 * per-dentry locking was born in 2.5.62.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 62)
#define compat_lock_dentry(dentry) spin_lock(&dentry->d_lock)
#define compat_unlock_dentry(dentry) spin_unlock(&dentry->d_lock)
#else
#define compat_lock_dentry(dentry) do {} while (0)
#define compat_unlock_dentry(dentry) do {} while (0)
#endif

/*
 * d_alloc_name was born in 2.6.10.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 10)
#define compat_d_alloc_name(parent, s) d_alloc_name(parent, s)
#else
#define compat_d_alloc_name(parent, s)                                        \
({                                                                            \
   struct qstr q;                                                             \
   q.name = s;                                                                \
   q.len = strlen(s);                                                         \
   q.hash = full_name_hash(q.name, q.len);                                    \
   d_alloc(parent, &q);                                                       \
})
#endif

#endif /* __COMPAT_DCACHE_H__ */
