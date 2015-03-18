/*********************************************************
 * Copyright (C) 2007 VMware, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *********************************************************/


/*
 * os.h --
 *
 *      FreeBSD-specific definitions.
 */


#ifndef __OS_H__
#define __OS_H__

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sx.h>
#include <sys/proc.h>
#include <sys/condvar.h>
#include <vm/uma.h>
#include <machine/stdarg.h>

#include "vm_basic_types.h"

typedef struct sx os_rwlock_t;

typedef struct uma_zone os_kmem_cache_t;
typedef struct os_completion_t {
   Bool completed;
   struct mtx mutex;
   struct cv cv;
} os_completion_t;
/*
 * Changing the os_atomic_t type requires that the os_atomic_* macros below be
 * changed as well.
 */
typedef unsigned int os_atomic_t;
typedef struct file * os_blocker_id_t;

#define OS_UNKNOWN_BLOCKER              NULL
#define OS_ENOMEM                       ENOMEM
#define OS_ENOENT                       ENOENT
#define OS_EEXIST                       EEXIST
#define OS_PATH_MAX                     MAXPATHLEN
#define OS_KMEM_CACHE_FLAG_HWALIGN      0       // unused

#define OS_FMTTID                       "p"
#define os_threadid                     curthread

extern NORETURN void os_panic(const char *fmt, va_list args);

#define os_rwlock_init(lock)            sx_init(lock, "vmblock-sx")
#define os_rwlock_destroy(lock)         sx_destroy(lock)
#define os_assert_rwlock_held(lock)     sx_assert(lock, SX_LOCKED)
#define os_read_lock(lock)              sx_slock(lock)
#define os_write_lock(lock)             sx_xlock(lock)
#define os_read_unlock(lock)            sx_sunlock(lock)
#define os_write_unlock(lock)           sx_xunlock(lock)

/*
 * XXX linux/os.h requests alignment on HW cache lines.  Is this of
 * serious concern?  We can do that by using UMA_ALIGN_CACHE as the
 * 'align' parameter to uma_zalloc, but with slightly different
 * semantics, it sort of changes the name of the game.
 */
#define os_kmem_cache_create(name, size, align, ctor) \
   uma_zcreate(name, size, ctor, NULL, NULL, NULL, align, 0)
#define os_kmem_cache_destroy(cache)    uma_zdestroy(cache)
#define os_kmem_cache_alloc(cache)      uma_zalloc(cache, M_WAITOK)
#define os_kmem_cache_free(cache, elem) uma_zfree(cache, elem)

#define os_completion_init(comp)                                \
      do {                                                      \
         (comp)->completed = FALSE;                             \
         mtx_init(&(comp)->mutex, "vmblock-mtx", "vmblock-mtx", MTX_DEF);         \
         cv_init(&(comp)->cv, "vmblock-cv");                       \
      } while (0)
#define os_completion_destroy(comp)                             \
      do {                                                      \
         mtx_destroy(&(comp)->mutex);                           \
         cv_destroy(&(comp)->cv);                               \
      } while (0)
/*
 * This macro evaluates to non-zero only if cv_wait_sig is interrupted.
 */
#define os_wait_for_completion(comp)                            \
({                                                              \
         int error = 0;                                         \
         mtx_lock(&(comp)->mutex);                              \
         while (!(comp)->completed && !error) {                 \
            error = cv_wait_sig(&(comp)->cv, &(comp)->mutex);   \
         }                                                      \
         mtx_unlock(&(comp)->mutex);                            \
         error;                                                 \
})
#define os_complete_all(comp)                                   \
      do {                                                      \
         mtx_lock(&(comp)->mutex);                              \
         (comp)->completed = TRUE;                              \
         cv_broadcast(&(comp)->cv);                             \
         mtx_unlock(&(comp)->mutex);                            \
      } while (0)

/* atomic_fetchadd_int returns the value of atomic before addition. */
#define os_atomic_dec_and_test(atomic)  (atomic_fetchadd_int(atomic, -1) == 1)
#define os_atomic_dec(atomic)           atomic_subtract_int(atomic, 1)
#define os_atomic_inc(atomic)           atomic_add_int(atomic, 1)
#define os_atomic_set(atomic, val)      atomic_store_rel_int(atomic, val)
#define os_atomic_read(atomic)          atomic_load_acq_int(atomic)

#endif /* __OS_H__ */
