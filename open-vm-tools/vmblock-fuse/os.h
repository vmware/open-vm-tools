/*********************************************************
 * Copyright (C) 2008-2016 VMware, Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation version 2.1 and no later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the Lesser GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *********************************************************/


/*
 * os.h --
 *
 *      OS-specific definitions.
 */


#ifndef __OS_H__
#define __OS_H__

#include <limits.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>

#include "vm_assert.h"

typedef pthread_rwlock_t                os_rwlock_t;
typedef void                            os_kmem_cache_t; // Not used.
typedef struct os_completion_t {
	pthread_cond_t cv;
	pthread_mutex_t mutex;
	int completed;
}                                       os_completion_t;
typedef gint                            os_atomic_t;
typedef char *                          os_blocker_id_t;

#define OS_UNKNOWN_BLOCKER              0
#define OS_ENOMEM                       (-ENOMEM)
#define OS_ENOENT                       (-ENOENT)
#define OS_EEXIST                       (-EEXIST)
#define OS_PATH_MAX                     PATH_MAX

#define OS_FMTTID                       "u"
#define os_threadid                     ((unsigned)(pthread_self()))

#define os_panic(fmt, args)             \
({                                      \
   vfprintf(stderr, fmt, args);         \
   abort();                             \
})

#define os_rwlock_init(lock)            pthread_rwlock_init(lock, NULL)
#define os_rwlock_destroy(lock)         pthread_rwlock_destroy(lock)

/*
 * XXX I don't know of anything better for os_rwlock_held that pthreads offers.
 */

#define os_rwlock_held(lock)            TRUE
#define os_read_lock(lock)              pthread_rwlock_rdlock(lock)
#define os_write_lock(lock)             pthread_rwlock_wrlock(lock)
#define os_read_unlock(lock)            pthread_rwlock_unlock(lock)
#define os_write_unlock(lock)           pthread_rwlock_unlock(lock)

/*
 * os_kmem_cache_create can't evaluate to NULL because there's a !NULL check
 * on its result.
 */

#define os_kmem_cache_create(name, size, align, ctor)  ((void *)1)
#define os_kmem_cache_destroy(cache)
#define os_kmem_cache_alloc(cache)                     malloc(sizeof(struct BlockInfo))
#define os_kmem_cache_free(cache, elem)                free(elem)

/*
 * Completion Functions
 */

#define os_completion_init(comp)                        \
({                                                      \
   pthread_cond_init(&(comp)->cv, NULL);                \
   pthread_mutex_init(&(comp)->mutex, NULL);            \
   (comp)->completed = 0;                               \
})
#define os_completion_destroy(comp)                     \
({                                                      \
   pthread_cond_destroy(&(comp)->cv);                   \
   pthread_mutex_destroy(&(comp)->mutex);               \
})
#define os_wait_for_completion(comp)                    \
({                                                      \
    pthread_mutex_lock(&(comp)->mutex);                 \
    while ((comp)->completed == 0) {                    \
       pthread_cond_wait(&(comp)->cv, &(comp)->mutex);  \
    }                                                   \
    pthread_mutex_unlock(&(comp)->mutex);               \
    0;                                                  \
})
#define os_complete_all(comp)                           \
({                                                      \
    pthread_mutex_lock(&(comp)->mutex);                 \
    (comp)->completed = 1;                              \
    pthread_cond_broadcast(&(comp)->cv);                \
    pthread_mutex_unlock(&(comp)->mutex);               \
    0;                                                  \
})

/*
 * Atomic Value Functions
 *
 * os_atomic_dec_and_test needs to test against 1 because ReadDecInt returns
 * the value before the dec.
 */

#define os_atomic_dec_and_test(atomic)  g_atomic_int_dec_and_test(atomic)
#define os_atomic_dec(atomic)           g_atomic_int_add((atomic), -1)
#define os_atomic_inc(atomic)           g_atomic_int_inc(atomic)
#define os_atomic_set(atomic, val)      g_atomic_int_set((atomic), (val))
#define os_atomic_read(atomic)          g_atomic_int_get(atomic)

/*
 * Extra stuff fuse port needs defined (ie not in os.h for other ports).
 */

#ifdef VMX86_DEVEL
extern int LOGLEVEL_THRESHOLD;
#  define LOG(level, fmt, args...)                              \
     ((void) (LOGLEVEL_THRESHOLD >= (level) ?                   \
              fprintf(stderr, "DEBUG:  " fmt, ## args) :        \
              0)                                                \
     )
#else
#  define LOG(level, fmt, args...)
#endif
#define Warning(fmt, args...)                                   \
     fprintf(stderr, "WARNING: " fmt, ## args)

/*
 * The GNU C library doesn't include strlcpy.
 * XXX: This prototype is moving. See util.c.
 */

size_t strlcpy(char *dest, const char *src, size_t count);

#endif /* __OS_H__ */
