/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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

#ifndef __COMPAT_SLAB_H__
#   define __COMPAT_SLAB_H__


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 2, 0)
#   include <linux/slab.h>
#else
#   include <linux/malloc.h>
#endif

/*
 * Before 2.6.20, kmem_cache_t was the accepted way to refer to a kmem_cache
 * structure.  Prior to 2.6.15, this structure was called kmem_cache_s, and
 * afterwards it was renamed to kmem_cache.  Here we keep things simple and use
 * the accepted typedef until it became deprecated, at which point we switch
 * over to the kmem_cache name.
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
#   define compat_kmem_cache struct kmem_cache
#else
#   define compat_kmem_cache kmem_cache_t
#endif

/*
 * Up to 2.6.22 kmem_cache_create has 6 arguments - name, size, alignment, flags,
 * constructor, and destructor.  Then for some time kernel was asserting that
 * destructor is NULL, and since 2.6.23-pre1 kmem_cache_create takes only 5
 * arguments - destructor is gone.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 22) || defined(VMW_KMEMCR_HAS_DTOR)
#define compat_kmem_cache_create(name, size, align, flags, ctor) \
		kmem_cache_create(name, size, align, flags, ctor, NULL)
#else
#define compat_kmem_cache_create(name, size, align, flags, ctor) \
		kmem_cache_create(name, size, align, flags, ctor)
#endif

/*
 * Up to 2.6.23 kmem_cache constructor has three arguments - pointer to block to
 * prepare (aka "this"), from which cache it came, and some unused flags.  After
 * 2.6.23 flags were removed, and order of "this" and cache parameters was swapped...
 * Since 2.6.27-rc2 everything is different again, and ctor has only one argument.
 *
 * HAS_3_ARGS has precedence over HAS_2_ARGS if both are defined.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 23) && !defined(VMW_KMEMCR_CTOR_HAS_3_ARGS)
#  define VMW_KMEMCR_CTOR_HAS_3_ARGS
#endif
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 26) && !defined(VMW_KMEMCR_CTOR_HAS_2_ARGS)
#  define VMW_KMEMCR_CTOR_HAS_2_ARGS
#endif

#if defined(VMW_KMEMCR_CTOR_HAS_3_ARGS)
typedef void compat_kmem_cache_ctor(void *, compat_kmem_cache *, unsigned long);
#define COMPAT_KMEM_CACHE_CTOR_ARGS(arg) void *arg, \
                                         compat_kmem_cache *cache, \
                                         unsigned long flags
#elif defined(VMW_KMEMCR_CTOR_HAS_2_ARGS)
typedef void compat_kmem_cache_ctor(compat_kmem_cache *, void *);
#define COMPAT_KMEM_CACHE_CTOR_ARGS(arg) compat_kmem_cache *cache, \
                                         void *arg
#else
typedef void compat_kmem_cache_ctor(void *);
#define COMPAT_KMEM_CACHE_CTOR_ARGS(arg) void *arg
#endif

#endif /* __COMPAT_SLAB_H__ */
