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

#include "compat_version.h"
#include "compat_autoconf.h"

/*
 * Between 2.6.27-rc1 and 2.6.27-rc2 ctor prototype was changed from
 * ctor(cache, ptr) to ctor(ptr).  Unfortunately there
 * is no typedef for ctor, so we have to redefine kmem_cache_create
 * to find out ctor prototype.  If prototype matches, then this is old
 * kernel.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 28)
#error "This test intentionally fails on 2.6.28 and newer kernels."
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 26)
#include <linux/slab.h>

struct kmem_cache *kmem_cache_create(const char *, size_t, size_t,
                                     unsigned long,
                                     void (*)(struct kmem_cache *, void *));
						
#endif
