/*********************************************************
 * Copyright (C) 2005 VMware, Inc. All rights reserved.
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
 * 	Wrappers for Solaris system functions required by "vmmemctl".
 */

/*
 * Includes
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
#include <sys/taskq.h>
#include <sys/disp.h>
#include <sys/ksynch.h>

#include "os.h"
#include "balloon_def.h"
#include "vmballoon_kstats.h"
#include "vmmemctl.h"
#include "buildNumber.h"

#if defined(SOL9)
extern unsigned disable_memscrub;
#else
extern void memscrub_disable(void);
#endif

/*
 * Constants
 */

#define ONE_SECOND_IN_MICROSECONDS	1000000

/*
 * Types
 */

typedef struct {
   timeout_id_t id;

   /* termination flag */
   volatile int stop;

   /* synchronization with worker thread */
   kmutex_t lock;
   kcondvar_t cv;

   /* registered state */
   os_timer_handler handler;
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
   const char	*name;
   const char	*name_verbose;
   os_timer	timer;
   kstat_t	*kstats;
   id_space_t	*id_space;
   vnode_t	vnode;
} os_state;

/*
 * Globals
 */

static os_state global_state;
static dev_info_t *vmmemctl_dip;	/* only one instance */

/*
 * Simple Wrappers
 */

void *os_kmalloc_nosleep(unsigned int size)
{
   return (kmem_alloc(size, KM_NOSLEEP));
}

void os_kfree(void *obj, unsigned int size)
{
   kmem_free(obj, size);
}

void os_bzero(void *b, unsigned int len)
{
   bzero(b, len);
}

void os_memcpy(void *dest, const void *src, unsigned int size)
{
   bcopy(src, dest, size);
}

int os_sprintf(char *str, const char *format, ...)
{
   ASSERT(0);
   /*
    * XXX disabled because the varargs header file doesn't seem to
    * work in the current (gcc 2.95.3) cross-compiler environment.
    * Not used for Solaris anyway.
    */
   return 0;
}

/*
 * System-Dependent Operations
 */

char *os_identity(void)
{
   return "solaris";
}

/*
 * Predict the maximum achievable balloon size.
 *
 * Currently we just return the total memory pages.
 */
unsigned int os_predict_max_balloon_pages(void)
{
   return(maxmem);
}

unsigned long os_addr_to_ppn(unsigned long addr)
{
   return (ulong_t)page_pptonum(((os_page *)addr)->pp);
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
 * This is a bit ugly.  In order to allocate a page, we need a vnode to
 * hang it from and a unique offset within that vnode.  We do this by
 * using our own vnode (used only to hang pages from) and allocating
 * offsets by use of the id space allocator.  The id allocator hands
 * us back unique integers between 0 and INT_MAX; we can then use those
 * as page indices into our fake vnode space.
 *
 * Future versions of Solaris will have a devmap_pmem_alloc/free
 * interface for allocating physical pages that may allow us to
 * eliminate some of this.
 */
unsigned long os_alloc_reserved_page(int can_sleep)
{
   os_state *state = &global_state;
   page_t *pp;
   u_offset_t off;
   struct seg kseg;
   os_page *page;
   id_space_t *idp = state->id_space;
   vnode_t *vp = &state->vnode;
   uint_t flags;

   /*
    * Reserve space for the page.
    */
   flags = can_sleep ? KM_SLEEP : KM_NOSLEEP;
   if (!page_resv(1, flags))
      return 0;		/* no space! */

   /*
    * Allocating space for os_page early simplifies error handling. 
    */
   if ((page = kmem_alloc(sizeof (os_page), flags)) == NULL) {
      page_unresv(1);
      return 0;
   }

   /* 
    * Construct an offset for page_create.
    */
   off = idtooff(id_alloc(idp));

   /*
    * Allocate the page itself.  Note that this can fail.
    */
   kseg.s_as = &kas;
   flags = can_sleep ? PG_EXCL | PG_WAIT : PG_EXCL;
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

   return (unsigned long)page;
}

void os_free_reserved_page(unsigned long addr)
{
   os_state *state = &global_state;
   os_page *page = (os_page *)addr;
   page_t *pp = page->pp;
   u_offset_t off = page->offset;
   id_space_t *idp = state->id_space;

   page_free(pp, 1);
   page_unresv(1);
   id_free(idp, offtoid(off));
   kmem_free(page, sizeof (os_page));
}

/*
 * Worker thread that periodically calls the timer handler.  This is
 * executed by a user context thread so that it can block waiting for
 * memory without fear of deadlock.
 */
static int os_worker(void)
{
   os_timer *t = &global_state.timer;
   clock_t timeout;

   mutex_enter(&t->lock);
   while (!t->stop) {
      /* invoke registered handler */
      mutex_exit(&t->lock);
      (void) (*(t->handler))(t->data);
      mutex_enter(&t->lock);

      /* check again whether we should stop */
      if (t->stop)
	 break;

      /* wait for timeout */
      (void) drv_getparm(LBOLT, &timeout);
      timeout += t->period;
      if (cv_timedwait_sig(&t->cv, &t->lock, timeout) == 0) {
	 mutex_exit(&t->lock);
	 return EINTR;		/* took a signal, return to user level */
      }
   }
   mutex_exit(&t->lock);
   ASSERT(t->stop);
   return 0;			/* normal termination */
}

/*
 * Initialize timer data.
 */
void os_timer_init(os_timer_handler handler, void *data, int period)
{
   os_timer *t = &global_state.timer;

   t->id = 0;
   t->handler = handler;
   t->data = data;
   t->period = period;
   t->stop = 0;

   mutex_init(&t->lock, NULL, MUTEX_DRIVER, NULL);
   cv_init(&t->cv, NULL, CV_DRIVER, NULL);
}

void os_timer_start(void)
{
   os_timer *t = &global_state.timer;

   t->stop = 0;
}

void os_timer_stop(void)
{
   os_timer *t = &global_state.timer;

   mutex_enter(&t->lock);

   /* set termination flag */
   t->stop = 1;

   /* wake up worker thread so it can exit */
   cv_signal(&t->cv);

   mutex_exit(&t->lock);
}

static void os_timer_cleanup(void)
{
   os_timer *timer = &global_state.timer;

   mutex_destroy(&timer->lock);
   cv_destroy(&timer->cv);
}

unsigned int os_timer_hz(void)
{
   return drv_usectohz(ONE_SECOND_IN_MICROSECONDS);
}

void os_yield(void)
{
   /* Do nothing. */
}

void os_init(const char *name,
             const char *name_verbose,
             os_status_handler handler)
{
   os_state *state = &global_state;
   static int initialized = 0;

   /* initialize only once */
   if (initialized++) {
      return;
   }

   /* zero global state */
   bzero(state, sizeof(global_state));

   state->kstats = BalloonKstatCreate();
   state->id_space = id_space_create("vmmemctl", 0, INT_MAX);
   state->name = name;
   state->name_verbose = name_verbose;

   /* disable memscrubber */
#if defined(SOL9)
   disable_memscrub = 1;
#else
   memscrub_disable();
#endif

   /* log device load */
   cmn_err(CE_CONT, "!%s initialized\n", name_verbose);
}

void os_cleanup(void)
{
   os_state *state = &global_state;

   os_timer_cleanup();
   BalloonKstatDelete(state->kstats);
   id_space_destroy(state->id_space);

   /* log device unload */
   cmn_err(CE_CONT, "!%s unloaded\n", state->name_verbose);
}

/*
 * Module Load/Unload Operations
 */

extern int  init_module(void);
extern void cleanup_module(void);

/*
 * Device configuration entry points
 */
static int vmmemctl_attach(dev_info_t *dip, ddi_attach_cmd_t cmd)
{
   switch (cmd) {
   case DDI_ATTACH:
      vmmemctl_dip = dip;
      if (ddi_create_minor_node(dip, "0", S_IFCHR, ddi_get_instance(dip),
				DDI_PSEUDO,0) != DDI_SUCCESS) {
	 return DDI_FAILURE;
      } else {
	 return DDI_SUCCESS;
      }
   default:
      return DDI_FAILURE;
   }
}

static int vmmemctl_detach(dev_info_t *dip, ddi_detach_cmd_t cmd)
{
   switch (cmd) {
   case DDI_DETACH:
      vmmemctl_dip = 0;
      ddi_remove_minor_node(dip, NULL);
      return DDI_SUCCESS;
   default:
      return DDI_FAILURE;
   }
}

/*
 * Commands used by the user level daemon to control the driver.
 * Since the daemon is single threaded, we use a simple monitor to
 * make sure that only one thread is executing here at a time.
 */
static int vmmemctl_ioctl(dev_t dev, int cmd, intptr_t arg, int mode,
			  cred_t *cred, int *rvalp)
{
   int error = 0;
   static int busy = 0;		/* set when a thread is in this function */
   static kmutex_t lock;	/* lock to protect busy count */

   if (drv_priv(cred) != 0)
      return EPERM;

   mutex_enter(&lock);
   if (busy) {
      /*
       * Only one thread at a time.
       */
      mutex_exit(&lock);
      return EBUSY;
   }
   busy = 1;
   mutex_exit(&lock);

   switch (cmd) {
   case VMMIOCWORK:
      error = os_worker();
      break;

   default:
      error = ENXIO;
      break;
   }

   mutex_enter(&lock);
   ASSERT(busy);
   busy = 0;
   mutex_exit(&lock);

   return error;
}

/*
 * Module linkage
 */

static struct cb_ops vmmemctl_cb_ops = {
   nulldev,		/* open */
   nulldev,		/* close */
   nodev,		/* strategy */
   nodev,		/* print */
   nodev,		/* dump */
   nodev,		/* read */
   nodev,		/* write */
   vmmemctl_ioctl,
   nodev,		/* devmap */
   nodev,		/* mmap */
   nodev,		/* segmap */
   nochpoll,		/* poll */
   ddi_prop_op,		/* prop_op */
   0,			/* streamtab */
   D_NEW | D_MP
};

static struct dev_ops vmmemctl_dev_ops = {
   DEVO_REV,
   0,
   ddi_no_info,		/* getinfo */
   nulldev,		/* identify */
   nulldev,		/* probe */
   vmmemctl_attach,
   vmmemctl_detach,
   nodev,		/* reset */
   &vmmemctl_cb_ops,	/* cb_ops */
   NULL,		/* bus_ops */
   nodev		/* power */
};

static struct modldrv vmmodldrv = {
   &mod_driverops,
   "VMware Memory Control b" BUILD_NUMBER_NUMERIC_STRING,
   &vmmemctl_dev_ops
};

static struct modlinkage vmmodlinkage = {
   MODREV_1,
   {&vmmodldrv, NULL}
};

int _init(void)
{
   int error;

   if (init_module() != BALLOON_SUCCESS)
      return EINVAL;
   if ((error = mod_install(&vmmodlinkage)) != 0)
      cleanup_module();
   return error;
}

int _info(struct modinfo *modinfop)
{
   return mod_info(&vmmodlinkage, modinfop);
}

int _fini(void)
{
   int error;

   /*
    * Check if the module is busy (i.e., there's a worker thread active)
    * before cleaning up.
    */
   if ((error = mod_remove(&vmmodlinkage)) == 0)
      cleanup_module();
   return error;
}


