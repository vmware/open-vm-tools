/*********************************************************
 * Copyright (C) 2012 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_HIGHMEM_H__
#   define __COMPAT_HIGHMEM_H__

#include <linux/highmem.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 5, 0)
#   define compat_kmap_atomic(_page)   kmap_atomic(_page)
#   define compat_kunmap_atomic(_page) kunmap_atomic(_page)
#else
#   define compat_kmap_atomic(_page)   kmap_atomic((_page), KM_USER0)
#   define compat_kunmap_atomic(_page) kunmap_atomic((_page), KM_USER0)
#endif

#endif /* __COMPAT_HIGHMEM_H__ */
