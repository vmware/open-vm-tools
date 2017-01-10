/*********************************************************
 * Copyright (C) 2005,2014 VMware, Inc. All rights reserved.
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
 * os.c --
 *
 *      Wrappers for Solaris system functions required by "vmmemctl".
 */

#include <sys/types.h>
#include <sys/cred.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/conf.h>
#include <sys/ddi.h>
#include <sys/sunddi.h>
#include <sys/modctl.h>
#include <sys/stat.h>
#include <sys/kstat.h>
#include <sys/id_space.h>
#include <sys/vnode.h>
#include <sys/proc.h>
#include <sys/disp.h>
#include <sys/ksynch.h>

#include "os.h"
#include "vmballoon.h"
#include "balloon_def.h"
#include "vm_assert.h"
#include "vmballoon_kstats.h"
#include "buildNumber.h"

extern void memscrub_disable(void);

/*
 * Constants
 */

#define ONE_SECOND_IN_MICROSECONDS 1000000

/*
 * Types
 */

typedef struct {
   timeout_id_t id;

   /* Worker thread ID */
   kt_did_t thread_id;

   /* termination flag */
   volatile int stop;

   /* synchronization with worker thread */
   kmutex_t lock;
   kcondvar_t cv;

   /* registered state */
   void *data;
   int period;
} os_timer;

/*
 * Keep track of offset here rather than peeking inside the page_t to
 * avoid dependencies on the page structure layout (which changes from
 * release to release).
 */
typedef struct {
   page_t *pp;
   u_offset_t offset;
} os_page;

typedef struct {
   os_timer	timer;
   kstat_t	*kstats;
   id_space_t	*id_space;
   vnode_t	vnode;
} os_state;

/*
 * Globals
 */

static os_state global_state;


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Malloc --
 *
 *      Allocates kernel memory.
 *
 * Results:
 *      On success: Pointer to allocated memory
 *      On failure: NULL
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void *
OS_Malloc(size_t size) // IN
{
   return (kmem_alloc(size, KM_NOSLEEP));
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Free --
 *
 *      Free allocated kernel memory.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_Free(void *ptr,   // IN
        size_t size) // IN
{
   kmem_free(ptr, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_MemZero --
 *
 *      Fill a memory location with 0s.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_MemZero(void *ptr,   // OUT
           size_t size) // IN
{
   bzero(ptr, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_MemCopy --
 *
 *      Copy a memory portion into another location.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_MemCopy(void *dest,      // OUT
           const void *src, // IN
           size_t size)     // IN
{
   bcopy(src, dest, size);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetLimit --
 *
 *      Predict the maximum achievable balloon size.
 *
 * Results:
 *      Currently we just return the total memory pages.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
OS_ReservedPageGetLimit(void)
{
   return maxmem;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetPA --
 *
 *      Convert a page handle (of a physical page previously reserved with
 *      OS_ReservedPageAlloc()) to a pa.
 *
 * Results:
 *      The pa.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

PA64
OS_ReservedPageGetPA(PageHandle handle) // IN: A valid page handle
{
   return ptob(page_pptonum(((os_page *)handle)->pp));
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetHandle --
 *
 *      Convert a pa (of a physical page previously reserved with
 *      OS_ReservedPageAlloc()) to a page handle.
 *
 * Results:
 *      The page handle.
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Currently not implemented on Solaris.
 *
 *-----------------------------------------------------------------------------
 */

PageHandle
OS_ReservedPageGetHandle(PA64 pa)     // IN
{
   // Solaris does not use batched commands.
   NOT_IMPLEMENTED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_MapPageHandle --
 *
 *      Map a page handle into kernel address space, and return the
 *      mapping to that page handle.
 *
 * Results:
 *      The mapping.
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Currently not implemented on Solaris.
 *
 *-----------------------------------------------------------------------------
 */

Mapping
OS_MapPageHandle(PageHandle handle)     // IN
{
   // Solaris does not use batched commands.
   NOT_IMPLEMENTED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Mapping2Addr --
 *
 *      Return the address of a previously mapped page handle (with
 *      OS_MapPageHandle).
 *
 * Results:
 *      The mapping address.
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Currently not implemented on Solaris.
 *
 *-----------------------------------------------------------------------------
 */

void *
OS_Mapping2Addr(Mapping mapping)        // IN
{
   // Solaris does not use batched commands.
   NOT_IMPLEMENTED();
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_UnmapPage --
 *
 *      Unmap a previously mapped page handle.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 * Note:
 *      Currently not implemented on Solaris.
 *
 *-----------------------------------------------------------------------------
 */

void
OS_UnmapPage(Mapping mapping)   // IN
{
   // Solaris does not use protocol v3.
   NOT_IMPLEMENTED();
}

/*
 * NOTE: cast id before shifting to avoid overflow (id_t is 32 bits,
 * u_offset_t is 64 bits).  Also, can't use ptob because it will
 * overflow in a 32-bit kernel (since ptob returns a ulong_t, and the
 * physical address may be larger than 2^32).
 */
#define idtooff(id)	((u_offset_t)(id) << PAGESHIFT)
#define offtoid(off)	((id_t)((off) >> PAGESHIFT))


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageAlloc --
 *
 *      Reserve a physical page for the exclusive use of this driver.
 *
 *      This is a bit ugly.  In order to allocate a page, we need a vnode to
 *      hang it from and a unique offset within that vnode.  We do this by
 *      using our own vnode (used only to hang pages from) and allocating
 *      offsets by use of the id space allocator.  The id allocator hands
 *      us back unique integers between 0 and INT_MAX; we can then use those
 *      as page indices into our fake vnode space.
 *
 *      Future versions of Solaris will have a devmap_pmem_alloc/free
 *      interface for allocating physical pages that may allow us to
 *      eliminate some of this.
 *
 * Results:
 *      On success: A valid page handle that can be passed to OS_ReservedPageGetPA()
 *                  or OS_ReservedPageFree().
 *      On failure: PAGE_HANDLE_INVALID
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

PageHandle
OS_ReservedPageAlloc(int canSleep,    // IN
                     int isLargePage) // IN
{
   os_state *state = &global_state;
   page_t *pp;
   u_offset_t off;
   struct seg kseg;
   os_page *page;
   id_space_t *idp = state->id_space;
   vnode_t *vp = &state->vnode;
   uint_t flags;

   ASSERT(!isLargePage);

   /*
    * Reserve space for the page.
    */
   flags = canSleep ? KM_SLEEP : KM_NOSLEEP;
   if (!page_resv(1, flags))
      return PAGE_HANDLE_INVALID; /* no space! */

   /*
    * Allocating space for os_page early simplifies error handling.
    */
   if ((page = kmem_alloc(sizeof (os_page), flags)) == NULL) {
      page_unresv(1);
      return PAGE_HANDLE_INVALID;
   }

   /*
    * Construct an offset for page_create.
    */
   off = idtooff(id_alloc(idp));

   /*
    * Allocate the page itself.  Note that this can fail.
    */
   kseg.s_as = &kas;
   flags = canSleep ? PG_EXCL | PG_WAIT : PG_EXCL;
   pp = page_create_va(vp, off, PAGESIZE, flags, &kseg,
		       (caddr_t)(ulong_t)off);
   if (pp != NULL) {
      /*
       * We got a page. We keep the PG_EXCL lock to prohibit
       * anyone (swrand, memscrubber) touching the page. Return the
       * pointer to structure describing page.
       */
      page_io_unlock(pp);
      page_hashout(pp, NULL);
      page->pp = pp;
      page->offset = off;
   } else {
      /*
       * Oops, didn't get a page.  Undo everything and return.
       */
      id_free(idp, offtoid(off));
      kmem_free(page, sizeof (os_page));
      page_unresv(1);
      page = NULL;
   }

   return (PageHandle)page;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageFree --
 *
 *      Unreserve a physical page previously reserved with OS_ReservedPageAlloc().
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 *
 *-----------------------------------------------------------------------------
 */

void
OS_ReservedPageFree(PageHandle handle, // IN: A valid page handle
                    int isLargePage)   // IN
{
   os_state *state = &global_state;
   os_page *page = (os_page *)handle;
   page_t *pp = page->pp;
   u_offset_t off = page->offset;
   id_space_t *idp = state->id_space;

   ASSERT(!isLargePage);

   page_free(pp, 1);
   page_unresv(1);
   id_free(idp, offtoid(off));
   kmem_free(page, sizeof (os_page));
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmememctl_poll_worker --
 *
 *      Worker thread that periodically calls the timer handler.  This is
 *      executed by a user context thread so that it can block waiting for
 *      memory without fear of deadlock.
 *
 * Results:
 *      On success: 0
 *      On failure: error code
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
vmmemctl_poll_worker(os_timer *t) // IN
{
   clock_t timeout;

   mutex_enter(&t->lock);

   while (!t->stop) {
      mutex_exit(&t->lock);

      Balloon_QueryAndExecute();

      mutex_enter(&t->lock);
      /* check again whether we should stop */
      if (t->stop)
         break;

      /* wait for timeout */
      (void) drv_getparm(LBOLT, &timeout);
      timeout += t->period;
      cv_timedwait_sig(&t->cv, &t->lock, timeout);
   }

   mutex_exit(&t->lock);

   thread_exit();
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_TimerStart --
 *
 *      Setup the timer callback function, then start it.
 *
 * Results:
 *      Always TRUE, cannot fail.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
vmmemctl_poll_start(void)
{
   os_timer *t = &global_state.timer;
   kthread_t *tp;

   /* setup the timer structure */
   t->id = 0;
   t->stop = 0;
   t->period = drv_usectohz(BALLOON_POLL_PERIOD * ONE_SECOND_IN_MICROSECONDS);

   mutex_init(&t->lock, NULL, MUTEX_DRIVER, NULL);
   cv_init(&t->cv, NULL, CV_DRIVER, NULL);

   /*
    * All Solaris drivers that I checked assume that thread_create() will
    * succeed, let's follow the suit.
    */
   tp = thread_create(NULL, 0, vmmemctl_poll_worker, (void *)t,
                      0, &p0, TS_RUN, minclsyspri);
   t->thread_id = tp->t_did;

   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_poll_stop --
 *
 *      Signal polling thread to stop and wait till it exists.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static void
vmmemctl_poll_stop(void)
{
   os_timer *t = &global_state.timer;

   mutex_enter(&t->lock);

   /* Set termination flag. */
   t->stop = 1;

   /* Wake up worker thread so it can exit. */
   cv_signal(&t->cv);

   mutex_exit(&t->lock);

   /* Wait for the worker thread to complete. */
   if (t->thread_id != 0) {
      thread_join(t->thread_id);
      t->thread_id = 0;
   }

   mutex_destroy(&t->lock);
   cv_destroy(&t->cv);
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_Yield --
 *
 *      Yield the CPU, if needed.
 *
 * Results:
 *      None
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

void
OS_Yield(void)
{
   /* Do nothing. */
}


/*
 * Module linkage
 */

static struct modldrv vmmodldrv = {
   &mod_miscops,
   "VMware Memory Control b" BUILD_NUMBER_NUMERIC_STRING,
};

static struct modlinkage vmmodlinkage = {
   MODREV_1,
   { &vmmodldrv, NULL }
};


int
_init(void)
{
   os_state *state = &global_state;
   int error;

   if (!Balloon_Init(BALLOON_GUEST_SOLARIS)) {
      return EIO;
   }

   state->kstats = BalloonKstatCreate();
   state->id_space = id_space_create(BALLOON_NAME, 0, INT_MAX);

   /* disable memscrubber */
   memscrub_disable();

   error = vmmemctl_poll_start();
   if (error) {
      goto err_do_cleanup;
   }

   error = mod_install(&vmmodlinkage);
   if (error) {
      goto err_stop_poll;
   }

   cmn_err(CE_CONT, "!%s initialized\n", BALLOON_NAME_VERBOSE);
   return 0;

err_stop_poll:
   vmmemctl_poll_stop();

err_do_cleanup:
   Balloon_Cleanup();
   id_space_destroy(state->id_space);
   BalloonKstatDelete(state->kstats);

   return error;
}


int
_info(struct modinfo *modinfop) // IN
{
   return mod_info(&vmmodlinkage, modinfop);
}


int
_fini(void)
{
   os_state *state = &global_state;
   int error;

   /*
    * Check if the module is busy before cleaning up.
    */
   error = mod_remove(&vmmodlinkage);
   if (error) {
      return error;
   }

   vmmemctl_poll_stop();
   Balloon_Cleanup();
   id_space_destroy(state->id_space);
   BalloonKstatDelete(state->kstats);

   cmn_err(CE_CONT, "!%s unloaded\n", BALLOON_NAME_VERBOSE);

   return 0;
}


