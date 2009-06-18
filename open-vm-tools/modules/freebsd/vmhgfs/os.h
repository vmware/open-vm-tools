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


/*
 * os.h --
 *
 *   Wrappers for OS specific functions that are different between Mac OS and FreeBsd.
 *   1. Implementation Mac OS / FreeBSD independent memory allocation and
 *   thread synchronization routines.
 *   2. Interaction with memory manager/pager.
 */

#ifndef _OS_H_
#define _OS_H_

#if defined __FreeBSD__
#  include <sys/param.h>          // for <everything>
#  include <sys/proc.h>
#  include <sys/condvar.h>
#  include <sys/lock.h>           // for struct mtx
#  include <sys/mutex.h>          // for struct mtx
#  include <sys/sx.h>
#elif defined __APPLE__
#  include <kern/thread.h>
#  include <kern/locks.h>
#endif

#include <sys/malloc.h>
#include "vm_basic_types.h"

#if defined __FreeBSD__
   typedef struct proc *OS_THREAD_T;
   typedef struct mtx OS_MUTEX_T;
   typedef struct sx OS_RWLOCK_T;
   typedef struct cv OS_CV_T;
#elif defined __APPLE__
   typedef thread_t OS_THREAD_T;
   typedef lck_mtx_t OS_MUTEX_T;
   typedef lck_rw_t OS_RWLOCK_T;
   /*
    * In Mac OS, a kernel thread waits on a 32-bit integer. To avoid collision,
    * Apple recommends that threads wait on the address of an object.
    */
   typedef void *OS_CV_T;
#endif

/* OS_ERR is the error code returned by os_* functions on error. */
#define OS_ERR (-1)

int os_init(void);
void os_cleanup(void);

/*
 * There does not seem to be a public zone allocator exposed in Mac OS. We create
 * a zone wrapper around the FreeBSD zone allocator so that we can keep the
 * FreeBSD zone allocator and support Mac OS at the same time.
 */

struct os_zone_struct;
typedef struct os_zone_struct OS_ZONE_T;

/*
 * Provide zone allocator function prototypes with the same signature as
 * the ones used in the FreeBSD kernel. This way they can be used both with
 * the FreeBSD uma allocator and the custom Mac OS allocation functions.
 */
typedef int (*os_zone_ctor)(void *mem, int size, void *arg, int flags);
typedef void (*os_zone_dtor)(void *mem, int size, void *arg);
typedef int (*os_zone_init)(void *mem, int size, int flags);
typedef void (*os_zone_finit)(void *mem, int size);

OS_ZONE_T *os_zone_create(char *zoneName, size_t objectSize,
			    os_zone_ctor ctor, os_zone_dtor dtor,
			    os_zone_init init, os_zone_finit finit,
			    int align, uint32 flags);
void os_zone_destroy(OS_ZONE_T *zone);
void *os_zone_alloc(OS_ZONE_T *zone, int flags);
void os_zone_free(OS_ZONE_T *zone, void *mem);

void *os_malloc(size_t size, int flags);
void os_free(void *mem, size_t size);

extern OS_MUTEX_T *os_mutex_alloc_init(const char *mtxName);
extern void os_mutex_free(OS_MUTEX_T *mtx);
extern void os_mutex_lock(OS_MUTEX_T *mtx);
extern void os_mutex_unlock(OS_MUTEX_T *mtx);

extern OS_RWLOCK_T *os_rw_lock_alloc_init(const char *lckName);
extern void os_rw_lock_free(OS_RWLOCK_T *lck);
extern void os_rw_lock_lock_shared(OS_RWLOCK_T *lck);
extern void os_rw_lock_lock_exclusive(OS_RWLOCK_T *lck);
extern void os_rw_lock_unlock_shared(OS_RWLOCK_T *lck);
extern void os_rw_lock_unlock_exclusive(OS_RWLOCK_T *lck);

extern void os_cv_init(OS_CV_T *cv, const char *name);
extern void os_cv_destroy(OS_CV_T *cv);
extern void os_cv_signal(OS_CV_T *cv);
extern int  os_cv_wait(OS_CV_T *cv, OS_MUTEX_T *mtx);

extern int os_thread_create(void *function, void *parameter,
			    const char *threadName, OS_THREAD_T *newThread);
extern void os_thread_join(OS_THREAD_T thread, OS_MUTEX_T *mtx);
extern void os_thread_release(OS_THREAD_T thread);
extern void os_thread_exit(int errorCode);

extern int os_add_atomic(unsigned int *address, int amount);
extern int os_component_to_utf8_decomposed(const char *bufIn, uint32 bufInSize, char *bufOut,
                                           size_t *sizeOut, uint32 bufOutSize);
extern int os_component_to_utf8_precomposed(const char *bufIn, uint32 bufInSize, char *bufOut,
                                            size_t *sizeOut, uint32 bufOutSize);
extern int os_path_to_utf8_precomposed(const char *bufIn, uint32 bufInSize, char *bufOut,
                                       uint32 bufOutSize);
extern Bool os_utf8_conversion_needed(void);

// Memory manager/pager functions
void os_SetSize(struct vnode *vp, off_t newSize);
int os_FlushRange(struct vnode *vp, off_t start, uint32_t length);

#endif // ifndef _OS_H_
