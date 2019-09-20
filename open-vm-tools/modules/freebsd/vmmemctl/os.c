/*********************************************************
 * Copyright (C) 2000,2014,2018-2019 VMware, Inc. All rights reserved.
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
 *      Wrappers for FreeBSD system functions required by "vmmemctl".
 */

/*
 * Compile-Time Options
 */

#define	OS_DISABLE_UNLOAD   0
#define	OS_DEBUG            1

/*
 * Includes
 */

#include "vm_basic_types.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/rwlock.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_object.h>
#include <vm/vm_param.h>
#include <sys/vmmeter.h>

#include <machine/stdarg.h>

#include "os.h"
#include "vmballoon.h"

/*
 * Types
 */

typedef struct {
   /* system structures */
   struct callout_handle callout_handle;

   /* termination flag */
   volatile int stop;
} os_timer;

typedef struct {
   unsigned long size;       /* bitmap size in bytes */
   unsigned long *bitmap;    /* bitmap words */
   unsigned int  hint;       /* start searching from this word */
} os_pmap;

typedef struct {
   os_timer    timer;
   os_pmap     pmap;
   vm_object_t vmobject;     /* vm backing object */
} os_state;

MALLOC_DEFINE(M_VMMEMCTL, BALLOON_NAME, "vmmemctl metadata");

/*
 * FreeBSD version specific MACROS
 */
#if __FreeBSD_version >= 900000
   #define VM_PAGE_LOCK(page) vm_page_lock(page);
   #define VM_PAGE_UNLOCK(page) vm_page_unlock(page)
#else
   #define VM_PAGE_LOCK(page) vm_page_lock_queues()
   #define VM_PAGE_UNLOCK(page) vm_page_unlock_queues()
#endif

#if __FreeBSD_version > 1000029
   #define VM_OBJ_LOCK(object) VM_OBJECT_WLOCK(object)
   #define VM_OBJ_UNLOCK(object) VM_OBJECT_WUNLOCK(object);
#else
   #define VM_OBJ_LOCK(object) VM_OBJECT_LOCK(object);
   #define VM_OBJ_UNLOCK(object) VM_OBJECT_UNLOCK(object);
#endif

#if __FreeBSD_version < 1100015
   #define VM_SYS_PAGES cnt.v_page_count
#else
   #define VM_SYS_PAGES vm_cnt.v_page_count
#endif

/*
 * The kmem_malloc() and kmem_free() APIs changed at different times during
 * the FreeBSD 12.0 ALPHA snapshot releases.  The difference in the
 * __FreeBSD_version values for FreeBSD 12.0 in the following macros are
 * consistent with when each API was changed.
 */
#if __FreeBSD_version < 1000000
   #define KVA_ALLOC(size) kmem_alloc_nofault(kernel_map, size)
   #define KVA_FREE(offset, size) kmem_free(kernel_map, offset, size)
#else
   #define KVA_ALLOC(size) kva_alloc(size);
   #define KVA_FREE(offset, size) kva_free(offset, size)
#endif

#if __FreeBSD_version < 1000000
   #define KMEM_ALLOC(size) kmem_alloc(kernel_map, size)
#elif  __FreeBSD_version < 1200080
   #define KMEM_ALLOC(size) kmem_malloc(kernel_arena, size, M_WAITOK | M_ZERO)
#else
   #define KMEM_ALLOC(size) kmem_malloc(size, M_WAITOK | M_ZERO)
#endif

#if __FreeBSD_version < 1000000
   #define KMEM_FREE(offset, size) kmem_free(kernel_map, offset, size)
#elif __FreeBSD_version < 1200083
   #define KMEM_FREE(offset, size) kmem_free(kernel_arena, offset, size)
#else
   #define KMEM_FREE(offset, size) kmem_free(offset, size)
#endif

/*
 * Globals
 */

static os_state global_state;

static void vmmemctl_init_sysctl(void);
static void vmmemctl_deinit_sysctl(void);


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
   return malloc(size, M_VMMEMCTL, M_NOWAIT);
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
   free(ptr, M_VMMEMCTL);
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
   memcpy(dest, src, size);
}

/* find first zero bit */
static __inline__ unsigned long os_ffz(unsigned long word)
{
#ifdef __x86_64__
   __asm__("bsfq %1,%0"
           :"=r" (word)
           :"r" (~word));
#else
   __asm__("bsfl %1,%0"
           :"=r" (word)
           :"r" (~word));
#endif
   return word;
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageGetLimit --
 *
 *      Predict the maximum achievable balloon size.
 *
 * Results:
 *      Total memory pages.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

unsigned long
OS_ReservedPageGetLimit(void)
{
   return VM_SYS_PAGES;
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
   return (((vm_page_t)handle)->phys_addr);
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
 *-----------------------------------------------------------------------------
 */

PageHandle
OS_ReservedPageGetHandle(PA64 pa)     // IN
{
   return (PageHandle)PHYS_TO_VM_PAGE(pa);
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
 *-----------------------------------------------------------------------------
 */

Mapping
OS_MapPageHandle(PageHandle handle)     // IN
{
   vm_offset_t res = KVA_ALLOC(PAGE_SIZE);

   vm_page_t page = (vm_page_t)handle;

   if (!res) {
      return MAPPING_INVALID;
   }

   pmap_qenter(res, &page, 1);

   return (Mapping)res;
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
 *-----------------------------------------------------------------------------
 */

void *
OS_Mapping2Addr(Mapping mapping)        // IN
{
   return (void *)mapping;
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
 *-----------------------------------------------------------------------------
 */

void
OS_UnmapPage(Mapping mapping)           // IN
{
   pmap_qremove((vm_offset_t)mapping, 1);
   KVA_FREE((vm_offset_t)mapping, PAGE_SIZE);
}


static void
os_pmap_alloc(os_pmap *p) // IN
{
   /* number of pages (div. 8) */
   p->size = (VM_SYS_PAGES + 7) / 8;

   /*
    * expand to nearest word boundary
    * XXX: bitmap can be greater than total number of pages in system
    */
   p->size = (p->size + sizeof(unsigned long) - 1) &
                         ~(sizeof(unsigned long) - 1);

   p->bitmap = (unsigned long *)KMEM_ALLOC(p->size);
}


static void
os_pmap_free(os_pmap *p) // IN
{
   KMEM_FREE((vm_offset_t)p->bitmap, p->size);
   p->size = 0;
   p->bitmap = NULL;
}


static void
os_pmap_init(os_pmap *p) // IN
{
   /* alloc bitmap for pages in system */
   os_pmap_alloc(p);
   if (!p->bitmap) {
      p->size = 0;
      p->bitmap = NULL;
      return;
   }

   /* clear bitmap */
   bzero(p->bitmap, p->size);
   p->hint = 0;
}


static vm_pindex_t
os_pmap_getindex(os_pmap *p) // IN
{
   int i;
   unsigned long bitidx, wordidx;

   /* start scanning from hint */
   wordidx = p->hint;

   /* scan bitmap for unset bit */
   for (i=0; i < p->size/sizeof (unsigned long); i++) {

      if (!p->bitmap[wordidx]) {
         p->bitmap[wordidx] = 1;
         p->hint = wordidx;
         return (wordidx * sizeof(unsigned long) * 8);
      }
      else if (p->bitmap[wordidx] != ~0UL) {

         /* find first zero bit */
         bitidx = os_ffz(p->bitmap[wordidx]);
         p->bitmap[wordidx] |= (1<<bitidx);
         p->hint = wordidx;
         return (wordidx * sizeof(unsigned long) * 8) + bitidx;
      }

      wordidx = (wordidx+1) % (p->size/sizeof (unsigned long));
   }

   /* failed */
   return (vm_pindex_t)-1;
}


static void
os_pmap_putindex(os_pmap *p,         // IN
                 vm_pindex_t pindex) // IN
{
   /* unset bit */
   p->bitmap[pindex / (8*sizeof(unsigned long))] &=
                             ~(1<<(pindex % (8*sizeof(unsigned long))));
}


static void
os_kmem_free(vm_page_t page) // IN
{
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;

   VM_OBJ_LOCK(state->vmobject);
   if (vm_page_lookup(state->vmobject, page->pindex)) {
      os_pmap_putindex(pmap, page->pindex);
      VM_PAGE_LOCK(page);
      vm_page_free(page);
      VM_PAGE_UNLOCK(page);
   }
   VM_OBJ_UNLOCK(state->vmobject);
}


static vm_page_t
os_kmem_alloc(int alloc_normal_failed) // IN
{
   vm_page_t page;
   vm_pindex_t pindex;
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;

   VM_OBJ_LOCK(state->vmobject);

   pindex = os_pmap_getindex(pmap);
   if (pindex == (vm_pindex_t)-1) {
      VM_OBJ_UNLOCK(state->vmobject);
      return NULL;
   }

   /*
    * BSD's page allocator does not support flags that are similar to
    * KM_NOSLEEP and KM_SLEEP in Solaris. The main page allocation function
    * vm_page_alloc() does not sleep ever. It just returns NULL when it
    * cannot find a free (or cached-and-clean) page. Therefore, we use
    * VM_ALLOC_NORMAL and VM_ALLOC_SYSTEM loosely to mean KM_NOSLEEP
    * and KM_SLEEP respectively.
    */
   if (alloc_normal_failed) {
      page = vm_page_alloc(state->vmobject, pindex, VM_ALLOC_SYSTEM);
   } else {
      page = vm_page_alloc(state->vmobject, pindex, VM_ALLOC_NORMAL);
   }

   if (!page) {
      os_pmap_putindex(pmap, pindex);
   }
   VM_OBJ_UNLOCK(state->vmobject);

   return page;
}


static void
os_balloonobject_delete(void)
{
   vm_object_deallocate(global_state.vmobject);
}


static void
os_balloonobject_create(void)
{
   global_state.vmobject = vm_object_allocate(OBJT_DEFAULT,
                  OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS));
}


/*
 *-----------------------------------------------------------------------------
 *
 * OS_ReservedPageAlloc --
 *
 *      Reserve a physical page for the exclusive use of this driver.
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
   vm_page_t page;

   ASSERT(!isLargePage);

   page = os_kmem_alloc(canSleep);
   if (page == NULL) {
      return PAGE_HANDLE_INVALID;
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
   ASSERT(!isLargePage);

   os_kmem_free((vm_page_t)handle);
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
 * vmmemctl_poll -
 *
 *      Calls Balloon_QueryAndExecute() to perform ballooning tasks and
 *      then reschedules itself to be executed in BALLOON_POLL_PERIOD
 *      seconds.
 *
 * Results:
 *      None.
 *
 * Side effects:
 *      None.
 */

static void
vmmemctl_poll(void *data) // IN
{
   os_timer *t = data;

   if (!t->stop) {
      /* invoke registered handler, rearm timer */
      Balloon_QueryAndExecute();
      t->callout_handle = timeout(vmmemctl_poll, t, BALLOON_POLL_PERIOD * hz);
   }
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_init --
 *
 *      Called at driver startup, initializes the balloon state and structures.
 *
 * Results:
 *      On success: 0
 *      On failure: standard error code
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

static int
vmmemctl_init(void)
{
   os_state *state = &global_state;
   os_timer *t = &state->timer;
   os_pmap *pmap = &state->pmap;

   if (!Balloon_Init(BALLOON_GUEST_BSD)) {
      return EIO;
   }

   /* initialize timer state */
   callout_handle_init(&state->timer.callout_handle);

   os_pmap_init(pmap);
   os_balloonobject_create();

   /* Set up and start polling */
   callout_handle_init(&t->callout_handle);
   t->stop = FALSE;
   t->callout_handle = timeout(vmmemctl_poll, t, BALLOON_POLL_PERIOD * hz);

   vmmemctl_init_sysctl();

   /* log device load */
   printf(BALLOON_NAME_VERBOSE " initialized\n");
   return 0;
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_cleanup --
 *
 *      Called when the driver is terminating, cleanup initialized structures.
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
vmmemctl_cleanup(void)
{
   os_state *state = &global_state;
   os_timer *t = &state->timer;
   os_pmap *pmap = &state->pmap;

   vmmemctl_deinit_sysctl();

   Balloon_Cleanup();

   /* Stop polling */
   t->stop = TRUE;
   untimeout(vmmemctl_poll, t, t->callout_handle);

   os_balloonobject_delete();
   os_pmap_free(pmap);

   /* log device unload */
   printf(BALLOON_NAME_VERBOSE " unloaded\n");
}


/*
 * Module Load/Unload Operations
 */

static int
vmmemctl_load(module_t mod, // IN: Unused
              int cmd,      // IN
              void *arg)    // IN: Unused
{
   int err = 0;

   switch (cmd) {
   case MOD_LOAD:
      err = vmmemctl_init();
      break;

    case MOD_UNLOAD:
       if (OS_DISABLE_UNLOAD) {
          /* prevent module unload */
          err = EBUSY;
       } else {
          vmmemctl_cleanup();
       }
       break;

   default:
      err = EINVAL;
      break;
   }

   return err;
}


static struct sysctl_oid *oid;

/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_sysctl --
 *
 *      This gets called to provide the sysctl output when requested.
 *
 * Results:
 *      Error, if any
 *
 * Side effects:
 *      Data is written into user-provided buffer
 *
 *-----------------------------------------------------------------------------
 */

static int
vmmemctl_sysctl(SYSCTL_HANDLER_ARGS)
{
   char buf[PAGE_SIZE];
   size_t len = 0;
   const BalloonStats *stats = Balloon_GetStats();

   /* format size info */
   len += snprintf(buf + len, sizeof(buf) - len,
                   "target:             %8"FMT64"u pages\n"
                   "current:            %8"FMT64"u pages\n",
                   stats->nPagesTarget,
                   stats->nPages);

   /* format rate info */
   len += snprintf(buf + len, sizeof(buf) - len,
                   "rateNoSleepAlloc:   %8d pages/sec\n"
                   "rateSleepAlloc:     %8d pages/sec\n"
                   "rateFree:           %8d pages/sec\n",
                   stats->rateNoSleepAlloc,
                   stats->rateAlloc,
                   stats->rateFree);

   len += snprintf(buf + len, sizeof(buf) - len,
                   "\n"
                   "timer:              %8u\n"
                   "start:              %8u (%4u failed)\n"
                   "guestType:          %8u (%4u failed)\n"
                   "lock:               %8u (%4u failed)\n"
                   "unlock:             %8u (%4u failed)\n"
                   "target:             %8u (%4u failed)\n"
                   "primNoSleepAlloc:   %8u (%4u failed)\n"
                   "primCanSleepAlloc:  %8u (%4u failed)\n"
                   "primFree:           %8u\n"
                   "errAlloc:           %8u\n"
                   "errFree:            %8u\n",
                   stats->timer,
                   stats->start, stats->startFail,
                   stats->guestType, stats->guestTypeFail,
                   stats->lock[FALSE],  stats->lockFail[FALSE],
                   stats->unlock[FALSE], stats->unlockFail[FALSE],
                   stats->target, stats->targetFail,
                   stats->primAlloc[BALLOON_PAGE_ALLOC_NOSLEEP],
                   stats->primAllocFail[BALLOON_PAGE_ALLOC_NOSLEEP],
                   stats->primAlloc[BALLOON_PAGE_ALLOC_CANSLEEP],
                   stats->primAllocFail[BALLOON_PAGE_ALLOC_CANSLEEP],
                   stats->primFree[FALSE],
                   stats->primErrorPageAlloc[FALSE],
                   stats->primErrorPageFree[FALSE]);

   return SYSCTL_OUT(req, buf, len + 1);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_init_sysctl --
 *
 *      Init out sysctl, to be used for providing driver state.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      Sn OID for a sysctl is registered
 *
 *-----------------------------------------------------------------------------
 */

static void
vmmemctl_init_sysctl(void)
{
   oid =  SYSCTL_ADD_OID(NULL, SYSCTL_STATIC_CHILDREN(_vm), OID_AUTO,
                         BALLOON_NAME, CTLTYPE_STRING | CTLFLAG_RD,
                         0, 0, vmmemctl_sysctl, "A",
                         BALLOON_NAME_VERBOSE);
}


/*
 *-----------------------------------------------------------------------------
 *
 * vmmemctl_deinit_sysctl --
 *
 *      Undo vmmemctl_init_sysctl(). Remove the sysctl we installed.
 *
 * Results:
 *      none
 *
 * Side effects:
 *      Sn OID for a sysctl is unregistered
 *
 *-----------------------------------------------------------------------------
 */

static void
vmmemctl_deinit_sysctl(void)
{
   if (oid) {
      sysctl_remove_oid(oid,1,0);
   }
}

DEV_MODULE(vmmemctl, vmmemctl_load, NULL);

