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
 * os.c --
 *
 *      FreeBSD specific implementations of the hgfs memory allocation
 *      and thread synchronization routines.
 */

#if !defined _KERNEL
#  error "This os.c file can only be compiled for the FreeBSD kernel."
#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/lock.h>         // for struct mtx
#include <sys/kernel.h>
#include <vm/uma.h>           // for uma_zone_t
#include <sys/kthread.h>      // for kthread_create()
#include <vm/vm.h>
#include <vm/vm_extern.h>     // for vnode_pager_setsize

#include "vm_basic_types.h"
#include "os.h"
#include "debug.h"
#include "channel.h"
#include "compat_freebsd.h"

/*
 * Malloc tag for statistics, debugging, etc.
 */
MALLOC_DEFINE(M_HGFS, HGFS_FS_NAME, HGFS_FS_NAME_LONG);

/*
 * Since FreeBSD provides a zone allocator, just store a pointer to the FreeBSD
 * zone allocator.
 */
typedef struct os_zone_struct {
   struct uma_zone *umaZone;
} os_zone_struct;

/*
 *----------------------------------------------------------------------------
 *
 * os_init --
 *
 *      Initialize the global memory allocation variables needed by other
 *      functions in this file. Must be called before any other functions in this
 *      file.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
os_init(void)
{
   /* NOP */
   return 0;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_cleanup --
 *
 *      Cleanup the global variables that were created in os_init.
 *      Must be called if os_init was called. Other functions in this
 *      file cannot be called after os_cleanup is called.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_cleanup(void)
{
   /* NOP */
}


/*
 *----------------------------------------------------------------------------
 *
 * os_zone_create --
 *
 *      Creates a new zone (OS_ZONE_T) from which memory allocations can
 *      be made.
 *
 * Results:
 *      Either an uma zone or NULL (if no memory was available).
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

OS_ZONE_T *
os_zone_create(char *zoneName,       // IN
	       size_t objectSize,    // IN
	       os_zone_ctor ctor,    // IN
	       os_zone_dtor dtor,    // IN
	       os_zone_init init,    // IN
	       os_zone_finit finit,  // IN
               int align,            // IN
               uint32 flags)         // IN
{
   OS_ZONE_T *zone;

   zone = os_malloc(sizeof *zone, M_WAITOK);
   zone->umaZone = uma_zcreate(zoneName,
			       objectSize, ctor, dtor, init, finit, align, flags);
   return zone;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_zone_destroy --
 *
 *      _destroys a zone created with os_zone_create.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_zone_destroy(OS_ZONE_T *zone) // IN
{
   ASSERT(zone);

   uma_zdestroy(zone->umaZone);
   os_free(zone, sizeof *zone);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_zone_alloc --
 *
 *      Allocates an object from the specified zone and calls the the zone
 *      initializer and constructor.
 *
 * Results:
 *      Either an allocated and initizalized object or NULL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void *
os_zone_alloc(OS_ZONE_T *zone, // IN
              int flags)       // IN
{
   void *mem;
   HgfsTransportChannel *channel = gHgfsChannel;
   HgfsKReqObject *req;
   ASSERT(zone);
   ASSERT(zone->umaZone);

   mem = uma_zalloc(zone->umaZone, flags | M_ZERO);
   if (mem) {
      req = (HgfsKReqObject *)mem;
      req->channel = channel;
   }
   return mem;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_zone_free --
 *
 *      Calls the zone destructor and final initialization routine on the
 *      specified object and then frees the object.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_zone_free(OS_ZONE_T *zone, // IN
             void *mem)       // IN
{
   ASSERT(zone);
   ASSERT(zone->umaZone);

   uma_zfree(zone->umaZone, mem);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_malloc --
 *
 *      Malloc some memory in a FreeBSD / Mac OS kernel independent manner.
 *      This just calls the internal kernel malloc function. According to the
 *      FreeBSD commments, if M_WAITOK is passed to the flags, malloc will never
 *      return NULL.
 *
 * Results:
 *      A pointer to allocated memory or NULL.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void *
os_malloc(size_t size, // IN
	  int flags)   // IN
{
   return malloc(size, M_HGFS, flags);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_free --
 *
 *      Free some memory in a FreeBSD / Mac OS kernel independent manner.
 *      This just calls the internal kernel free function.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The memory (mem) is freed.
 *
 *----------------------------------------------------------------------------
 */

void
os_free(void *mem,   // IN
	size_t size) // IN
{
   free(mem, M_HGFS);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_mutex_alloc_init --
 *
 *      Allocate and initialize a FreeBSD mutex in an OS independent way.
 *      Mtx_name is not used on Mac OS.
 *
 * Results:
 *      A new mtx which has been allocated and is ready for use.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

OS_MUTEX_T *
os_mutex_alloc_init(const char *mtxName) // IN
{
   OS_MUTEX_T *mtx;
   mtx = os_malloc(sizeof *mtx,
		   M_ZERO | M_WAITOK);
   mtx_init(mtx, mtxName, NULL, MTX_DEF);
   return mtx;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_mutex_free --
 *
 *      Frees a FreeBSD mutex in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The mutex (mtx) is destroyed.
 *
 *----------------------------------------------------------------------------
 */

void
os_mutex_free(OS_MUTEX_T *mtx) // IN
{
   mtx_destroy(mtx);
   os_free(mtx, sizeof *mtx);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_mutex_lock --
 *
 *      Lock a FreeBSD mutex in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_mutex_lock(OS_MUTEX_T *mtx) // IN
{
   mtx_lock(mtx);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_mutex_unlock --
 *
 *      Unlock a FreeBSD mutex in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_mutex_unlock(OS_MUTEX_T *mtx) // IN
{
   mtx_unlock(mtx);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_alloc_init --
 *
 *      Allocate and initialize a FreeBSD rwlock in an OS independent way.
 *
 * Results:
 *      A new lock which has been allocated and is ready for use.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

OS_RWLOCK_T *
os_rw_lock_alloc_init(const char *lckName) // IN
{
   OS_RWLOCK_T *lck;
   lck = os_malloc(sizeof *lck,
		   M_ZERO | M_WAITOK);
   sx_init(lck, lckName);
   return lck;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_free --
 *
 *      Frees a FreeBSD rwlock in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      The rwlock (lck) is destroyed.
 *
 *----------------------------------------------------------------------------
 */

void
os_rw_lock_free(OS_RWLOCK_T *lck) // IN
{
   sx_destroy(lck);
   os_free(lck, sizeof *lck);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_lock_shared --
 *
 *      Lock a FreeBSD rwlock for reads in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_rw_lock_lock_shared(OS_RWLOCK_T *lck) // IN
{
   sx_slock(lck);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_lock_exclusive --
 *
 *      Lock a FreeBSD rwlock for writes in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_rw_lock_lock_exclusive(OS_RWLOCK_T *lck) // IN
{
   sx_xlock(lck);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_unlock_shared --
 *
 *      Unlock FreeBSD rwlock in an OS independent way. Results are
 *      undefined if the function caller has an exculsive lock on lck.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_rw_lock_unlock_shared(OS_RWLOCK_T *lck) // IN
{
   sx_sunlock(lck);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_rw_lock_unlock_exclusive --
 *
 *      Unlock FreeBSD rwlock in an OS independent way. Results are
 *      undefined if the function caller has an exculsive lock on lck.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_rw_lock_unlock_exclusive(OS_RWLOCK_T *lck) // IN
{
   sx_xunlock(lck);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_cv_init --
 *
 *      Initialize a cv under FreeBSD. Under Mac OS, we are actually passed an
 *      object address we will use in place of a cv in later functions. Here
 *      we simply do nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_cv_init(OS_CV_T *cv,      // IN
	   const char *name) // IN
{
   cv_init(cv, name);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_cv_destroy --
 *
 *      Destroy a cv under FreeBSD. Under Mac OS, we are actually passed an
 *      object address we will use in place of a cv in later functions. Here
 *      we simply do nothing.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_cv_destroy(OS_CV_T *cv) // IN
{
   cv_destroy(cv);
}

/*
 *----------------------------------------------------------------------------
 *
 * os_cv_signal --
 *
 *      Signal a thread to wakeup in a FreeBSD/Mac OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_cv_signal(OS_CV_T *cv) // IN
{
   cv_signal(cv);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_cv_wait --
 *
 *      Have an XNU or FreeBSD kernel thread wait until the specified condition
 *      is signaled. This function unlocks the mutex (mtx) before it goes to
 *      sleep and reacquires it after the thread wakes up. Under FreeBSD it is a
 *      standard condition variable. os_cv_wait will return immediately if the
 *      thread was interrupted. It is the callers responsibility to determine
 *      if a signal was delivered or the dependent condition actually occurred.
 *      Under FreeBSD, it is illegal to sleep while holding a lock. Callers of
 *      this function should not hold any locks other than the mutex (mtx) that
 *      is passed into the function.
 *
 * Results:
 *      Zero on success. On FreeBSD errno if interrupted.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
os_cv_wait(OS_CV_T *cv,      // IN
	   OS_MUTEX_T *mtx)  // IN
{
   return cv_wait_sig(cv, mtx);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_thread_create --
 *
 *      Create an Mac OS or FreeBSD kernel thread in an OS independent way.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
os_thread_create(void *function,           // IN
		 void *parameter,          // IN
		 const char *threadName,   // IN
		 OS_THREAD_T *newThread)   // OUT
{
   return compat_kthread_create(function, parameter,
                                newThread, 0, 0, threadName);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_thread_join --
 *
 *      Wait until the specified kernel thread exits and then return. Mtx must
 *      be held by the calling code and the thread (thread) is not allowed to
 *      exit while mtx is held. This prevents (thread) from exiting before
 *      the caller goes to sleep.
 *
 * Results:
 *      Zero on success.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_thread_join(OS_THREAD_T thread, // IN
               OS_MUTEX_T *mtx)    // IN
{
   ASSERT(mtx);

   msleep(thread, mtx, PDROP, NULL, 0);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_thread_release --
 *
 *      Release the OS_THREAD_T reference that was acquired in os_thread_create.
 *      This is a nop on FreeBSD.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_thread_release(OS_THREAD_T thread) // IN
{
   /* NOP */
}


/*
 *----------------------------------------------------------------------------
 *
 * Hgfsthreadexit --
 *
 *      Called when a thread is exiting. ErrorCode is returned as the thread exit code
 *      under FreeBSD and ignored under Mac OS.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

void
os_thread_exit(int errorCode) // IN
{
   compat_kthread_exit(errorCode);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_add_atomic --
 *
 *      Atomically increment an integer at a given location (address) by a given
 *      value (amount).
 *
 * Results:
 *      The value before the addition..
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
os_add_atomic(unsigned *address, // IN
	      int amount)        // IN
{
   return atomic_fetchadd_int(address, amount);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_utf8_conversion_needed --
 *
 *      It returns result depending upon whether a particular operating
 *      system expects utf8 strings in a format (decomposed utf8)
 *      different from wire format (precomposed utf8) or not. Since FreeBSD
 *      does not expect decomposed utf8, we return FALSE.
 *
 * Results:
 *      FALSE if conversion is not needed, TRUE if needed.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

Bool
os_utf8_conversion_needed(void)
{
   return FALSE;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_component_to_utf8_decomposed --
 *
 *      Converts an input component into decomposed form and writes it into
 *      output buffer. It simply returns OS_ERR for FreeBSD.
 *
 * Results:
 *      0 on success or OS_ERR on failure. Since this function is not
 *      implemented, it always returns OS_ERR.
 *
 * Side effects:
 *      None.
 *
 *----------------------------------------------------------------------------
 */

int
os_component_to_utf8_decomposed(char const *bufIn,  // IN
                                uint32 bufSizeIn,   // IN
                                char *bufOut,       // OUT
                                size_t *sizeOut,    // OUT
                                uint32 bufSizeOut)  // IN
{
   NOT_IMPLEMENTED();
   return OS_ERR;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_component_to_utf8_precomposed --
 *
 *      Converts an input component into precomposed form and writes it into
 *      output buffer. It simply returns OS_ERR for FreeBSD.
 *
 * Results:
 *      0 on success or OS_ERR on failure. Since this function is not 
 *      implemented, it always returns OS_ERR.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
os_component_to_utf8_precomposed(char const *bufIn,  // IN
                                     uint32 bufSizeIn,   // IN
                                     char *bufOut,       // OUT
                                     size_t *sizeOut,    // OUT
                                     uint32 bufSizeOut)  // IN
{
   NOT_IMPLEMENTED();
   return OS_ERR;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_path_to_utf8_precomposed  --
 *
 *      Converts an input path into precomposed form and writes it into output
 *      buffer. It simply returns OS_ERR for FreeBSD.
 *
 * Results:
 *      0 on success or OS_ERR on failure. Since this function is not
 *      implemented, it always returns OS_ERR.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
os_path_to_utf8_precomposed(char const *bufIn,  // IN
                            uint32 bufSizeIn,   // IN
                            char *bufOut,       // OUT
                            uint32 bufSizeOut)  // IN
{
   NOT_IMPLEMENTED();
   return OS_ERR;
}


/*
 *----------------------------------------------------------------------------
 *
 * os_SetSize  --
 *
 *      Notifies memory management system that file size has been changed.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

void
os_SetSize(struct vnode* vp,          // IN: vnode which size has changed
           off_t newSize)             // IN: new file size
{
   vnode_pager_setsize(vp, newSize);
}


/*
 *----------------------------------------------------------------------------
 *
 * os_FlushRange  --
 *
 *      Flushes dirty pages associated with the file.
 *
 * Results:
 *      Always retun 0 (success) for now since it is NOOP.
 *
 * Side effects:
 *      None.
 *----------------------------------------------------------------------------
 */

int
os_FlushRange(struct vnode *vp,    // IN: vnode which data needs flushing
              off_t start,         // IN: starting offset in the file to flush
              uint32_t length)     // IN: length of data to flush
{
   /*
    * XXX: NOOP for now. This routine is needed to maintain coherence
    *      between memory mapped data and data for read/write operations.
    *      Will need to implement when adding support for memory mapped files to HGFS
    *      for FreeBsd.
    */
   return 0;
}
