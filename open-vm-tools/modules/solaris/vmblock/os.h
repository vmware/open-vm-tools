/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * The contents of this file are subject to the terms of the Common
 * Development and Distribution License (the "License") version 1.0
 * and no later version.  You may not use this file except in
 * compliance with the License.
 *
 * You can obtain a copy of the License at
 *         http://www.opensource.org/licenses/cddl1.php
 *
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 *********************************************************/


/*
 * os.h --
 *
 *      OS-specific definitions.
 */


#ifndef __OS_H__
#define __OS_H__

#include <sys/types.h>
#include <sys/kmem.h>
#include <sys/ksynch.h>
#include <sys/atomic.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/cmn_err.h>
#include <sys/varargs.h>
#include <sys/sunddi.h>

#include "vm_basic_types.h"

typedef krwlock_t os_rwlock_t;
typedef kmem_cache_t os_kmem_cache_t;
typedef struct os_completion_t {
   Bool completed;
   kmutex_t mutex;
   kcondvar_t cv;
} os_completion_t;
/*
 * Changing the os_atomic_t type requires that the os_atomic_* macros below be
 * changed as well.
 */
typedef uint_t os_atomic_t;
typedef kthread_t * os_blocker_id_t;

#define OS_UNKNOWN_BLOCKER              NULL
#define OS_ENOMEM                       ENOMEM
#define OS_ENOENT                       ENOENT
#define OS_EEXIST                       EEXIST
#define OS_PATH_MAX                     MAXPATHLEN
#define OS_KMEM_CACHE_FLAG_HWALIGN      0

#define OS_FMTTID                       "lu"
#define os_threadid                     (uintptr_t)curthread

#define os_panic(fmt, args)             vcmn_err(CE_PANIC, fmt, args)

#define os_rwlock_init(lock)            rw_init(lock, NULL, RW_DRIVER, NULL)
#define os_rwlock_destroy(lock)         rw_destroy(lock)
/*
 * rw_lock_held() returns 1 if the lock is read locked, and rw_owner() returns
 * the current lock owner if it's write locked.  In the read locked case, we
 * just have to assume we're one of the readers.
 */
#define os_rwlock_held(lock)            (rw_lock_held(lock) || \
                                         rw_owner(lock) == curthread)
#define os_read_lock(lock)              rw_enter(lock, RW_READER)
#define os_write_lock(lock)             rw_enter(lock, RW_WRITER)
#define os_read_unlock(lock)            rw_exit(lock)
#define os_write_unlock(lock)           rw_exit(lock)

#define os_kmem_cache_create(name, size, align, ctor) \
   kmem_cache_create(name, size, align, ctor, NULL, NULL, NULL, NULL, 0)
#define os_kmem_cache_destroy(cache)    kmem_cache_destroy(cache)
#define os_kmem_cache_alloc(cache)      kmem_cache_alloc(cache, KM_SLEEP)
#define os_kmem_cache_free(cache, elem) kmem_cache_free(cache, elem)

#define os_completion_init(comp)                                \
      do {                                                      \
         (comp)->completed = FALSE;                             \
         mutex_init(&(comp)->mutex, NULL, MUTEX_DRIVER, NULL);  \
         cv_init(&(comp)->cv, NULL, CV_DRIVER, NULL);           \
      } while (0)
#define os_completion_destroy(comp)                             \
      do {                                                      \
         mutex_destroy(&(comp)->mutex);                         \
         cv_destroy(&(comp)->cv);                               \
      } while (0)
/*
 * XXX The following should be made interruptible such as via
 * cv_wait_sig, and returning that function's return value.  In
 * the meantime, fake "success" by evaluating to 0.
 */
#define os_wait_for_completion(comp)                            \
({                                                              \
    mutex_enter(&(comp)->mutex);                                \
    while (!(comp)->completed) {                                \
       cv_wait(&(comp)->cv, &(comp)->mutex);                    \
    }                                                           \
    mutex_exit(&(comp)->mutex);                                 \
    0;                                                          \
})
#define os_complete_all(comp)                                   \
      do {                                                      \
         mutex_enter(&(comp)->mutex);                           \
         (comp)->completed = TRUE;                              \
         mutex_exit(&(comp)->mutex);                            \
         cv_broadcast(&(comp)->cv);                             \
      } while (0)

/* These will need to change if os_atomic_t is changed from uint_t. */
#define os_atomic_dec_and_test(atomic)  (atomic_dec_uint_nv(atomic) == 0)
#define os_atomic_dec(atomic)           atomic_dec_uint(atomic)
#define os_atomic_set(atomic, val)      atomic_swap_uint(atomic, val)
#define os_atomic_inc(atomic)           atomic_inc_uint(atomic)
#define os_atomic_read(atomic)          atomic_add_int_nv(atomic, 0)

#endif /* __OS_H__ */
