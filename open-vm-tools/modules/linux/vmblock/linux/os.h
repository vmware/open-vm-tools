/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
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
 * os.h --
 *
 *      OS-specific definitions.
 */


#ifndef __OS_H__
#define __OS_H__

#include "driver-config.h"
#include <linux/completion.h>
#include <linux/limits.h>
#include "compat_slab.h"
#include <linux/sched.h>
#include <asm/atomic.h>
#include <asm/errno.h>
#include <asm/current.h>

typedef rwlock_t os_rwlock_t;
typedef compat_kmem_cache os_kmem_cache_t;
typedef struct completion os_completion_t;
typedef atomic_t os_atomic_t;
typedef struct file * os_blocker_id_t;

#define OS_UNKNOWN_BLOCKER              NULL
#define OS_ENOMEM                       (-ENOMEM)
#define OS_ENOENT                       (-ENOENT)
#define OS_EEXIST                       (-EEXIST)
#define OS_PATH_MAX                     PATH_MAX

#define OS_FMTTID                       "d"
#define os_threadid                     (current->pid)

#define os_panic(fmt, args)              \
      ({                                 \
          vprintk(fmt, args);            \
          BUG();                         \
      })

#define os_rwlock_init(lock)            rwlock_init(lock)
#define os_rwlock_destroy(lock)
/*
 * XXX We'd like to check for kernel version 2.5.34 as the patches indicate,
 * but SLES10's 2.6.16.21-0.8-i586default doesn't seem to have this defined.
 */
#if defined(rwlock_is_locked)
# define os_rwlock_held(lock)           rwlock_is_locked(lock)
#else
/* XXX Is there something we can come up with for this? */
# define os_rwlock_held(lock)           TRUE
#endif
#define os_read_lock(lock)              read_lock(lock)
#define os_write_lock(lock)             write_lock(lock)
#define os_read_unlock(lock)            read_unlock(lock)
#define os_write_unlock(lock)           write_unlock(lock)

#define os_kmem_cache_create(name, size, align, ctor) \
   compat_kmem_cache_create(name, size, align, SLAB_HWCACHE_ALIGN, ctor)
#define os_kmem_cache_destroy(cache)    kmem_cache_destroy(cache)
#define os_kmem_cache_alloc(cache)      kmem_cache_alloc(cache, GFP_KERNEL)
#define os_kmem_cache_free(cache, elem) kmem_cache_free(cache, elem)

#define os_completion_init(comp)        init_completion(comp)
#define os_completion_destroy(comp)
/*
 * XXX This should be made interruptible using
 * wait_for_completion_interruptible(), and return a proper value.  Callers
 * would need to handle interruption, of course.
 */
#define os_wait_for_completion(comp)                                    \
({                                                                      \
    wait_for_completion(comp);                                          \
    0;                                                                  \
 })
#define os_complete_all(comp)           complete_all(comp)

#define os_atomic_dec_and_test(atomic)  atomic_dec_and_test(atomic)
#define os_atomic_dec(atomic)           atomic_dec(atomic)
#define os_atomic_set(atomic, val)      atomic_set(atomic, val)
#define os_atomic_inc(atomic)           atomic_inc(atomic)
#define os_atomic_read(atomic)          atomic_read(atomic)

#endif /* __OS_H__ */
