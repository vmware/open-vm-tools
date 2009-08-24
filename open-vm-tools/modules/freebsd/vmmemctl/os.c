/*********************************************************
 * Copyright (C) 2000 VMware, Inc. All rights reserved.
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

#define	OS_DISABLE_UNLOAD	(0)
#define	OS_DEBUG		(1)

/*
 * Includes
 */

#include "vm_basic_types.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/conf.h>
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

/*
 * Constants
 */

/*
 * Types
 */

typedef struct {
   /* system structures */
   struct callout_handle callout_handle;

   /* termination flag */
   volatile int stop;

   /* registered state */
   OSTimerHandler *handler;
   void *data;
   int period;
} os_timer;

typedef struct {
   /* registered state */
   OSStatusHandler *handler;
   const char *name_verbose;
   const char *name;
} os_status;

typedef struct {
   unsigned long size;       /* bitmap size in bytes */
   unsigned long *bitmap;    /* bitmap words */
   unsigned int  hint;       /* start searching from this word */
} os_pmap;

typedef struct {
   os_status   status;
   os_timer    timer;
   os_pmap     pmap;
   vm_object_t vmobject;     /* vm backing object */
} os_state;

MALLOC_DEFINE(M_VMMEMCTL, "vmmemctl", "vmmemctl metadata"); 

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
   return(malloc(size, M_VMMEMCTL, M_NOWAIT));
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
 * OS_Snprintf --
 *
 *      Print a string into a bounded memory location.
 *
 * Results:
 *      Number of character printed including trailing \0.
 *
 * Side effects:
 *      None
 *
 *-----------------------------------------------------------------------------
 */

int
OS_Snprintf(char *buf,          // OUT
            size_t size,        // IN
            const char *format, // IN
            ...)                // IN
{
   int result;
   va_list args;

   va_start(args, format);
   result = vsnprintf(buf, size, format, args);
   va_end(args);

   return result;
}

/*
 * System-Dependent Operations
 */

const char *
OS_Identity(void)
{
   return("bsd");
}

/*
 * Predict the maximum achievable balloon size.
 *
 * Currently we just return the total memory pages.
 */
unsigned int
OS_PredictMaxReservedPages(void)
{
   return(cnt.v_page_count);
}

unsigned long
OS_AddrToPPN(unsigned long addr)
{
   return (((vm_page_t)addr)->phys_addr) >> PAGE_SHIFT;
}

static void os_pmap_alloc(os_pmap *p)
{
   /* number of pages (div. 8) */
   p->size = (cnt.v_page_count + 7) / 8;

   /* 
    * expand to nearest word boundary 
    * XXX: bitmap can be greater than total number of pages in system 
    */
   p->size = (p->size + sizeof(unsigned long) - 1) & 
                         ~(sizeof(unsigned long) - 1);

   p->bitmap = (unsigned long *)kmem_alloc(kernel_map, p->size);
}

static void os_pmap_free(os_pmap *p)
{
   kmem_free(kernel_map, (vm_offset_t)p->bitmap, p->size);
   p->size = 0;
   p->bitmap = NULL;
}

static void os_pmap_init(os_pmap *p)
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

static vm_pindex_t os_pmap_getindex(os_pmap *p)
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

static void os_pmap_putindex(os_pmap *p, vm_pindex_t pindex)
{
   /* unset bit */
   p->bitmap[pindex / (8*sizeof(unsigned long))] &= 
                             ~(1<<(pindex % (8*sizeof(unsigned long))));
}

static void os_kmem_free(vm_page_t page)
{
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;

   if ( !vm_page_lookup(state->vmobject, page->pindex) ) {
      return;
   }

   os_pmap_putindex(pmap, page->pindex);
   vm_page_free(page);
}

static vm_page_t os_kmem_alloc(int alloc_normal_failed)
{
   vm_page_t page;
   vm_pindex_t pindex;
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;

   pindex = os_pmap_getindex(pmap);
   if (pindex == (vm_pindex_t)-1) {
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

   return page;
}

static void os_balloonobject_delete(void)
{
   vm_object_deallocate(global_state.vmobject);
}

static void os_balloonobject_create(void)
{
   global_state.vmobject = vm_object_allocate(OBJT_DEFAULT,
                  OFF_TO_IDX(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS));
}

unsigned long
OS_AllocReservedPage(int canSleep)
{
   return (unsigned long)os_kmem_alloc(canSleep);
}

void
OS_FreeReservedPage(unsigned long page)
{
   os_kmem_free((vm_page_t)page);
}

static void os_timer_internal(void *data)
{
   os_timer *t = (os_timer *) data;

   if (!t->stop) {
      /* invoke registered handler, rearm timer */
      (void) (*(t->handler))(t->data);
      t->callout_handle = timeout(os_timer_internal, t, t->period);
   }
}

void
OS_TimerInit(OSTimerHandler *handler, // IN
             void *clientData,        // IN
             int period)              // IN
{
   os_timer *t = &global_state.timer;

   callout_handle_init(&t->callout_handle);
   t->handler = handler;
   t->data = clientData;
   t->period = period;
   t->stop = 0;
}

void
OS_TimerStart(void)
{
   os_timer *t = &global_state.timer;

   /* clear termination flag */
   t->stop = 0;

   /* scheduler timer handler */
   t->callout_handle = timeout(os_timer_internal, t, t->period);
}

void
OS_TimerStop(void)
{
   os_timer *t = &global_state.timer;

   /* set termination flag */
   t->stop = 1;

   /* deschedule timer handler */
   untimeout(os_timer_internal, t, t->callout_handle);
}

unsigned int
OS_TimerHz(void)
{
   return hz;
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

void
OS_Init(const char *name,
        const char *name_verbose,
        OSStatusHandler *handler)
{
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;
   static int initialized = 0;

   /* initialize only once */
   if (initialized++) {
      return;
   }

   /* zero global state */
   bzero(state, sizeof(global_state));

   /* initialize timer state */
   callout_handle_init(&state->timer.callout_handle);

   /* initialize status state */
   state->status.handler = handler;
   state->status.name = name;
   state->status.name_verbose = name_verbose;

   os_pmap_init(pmap);
   os_balloonobject_create();

   vmmemctl_init_sysctl();

   /* log device load */
   printf("%s initialized\n", state->status.name_verbose);
}

void
OS_Cleanup(void)
{
   os_state *state = &global_state;
   os_pmap *pmap = &state->pmap;
   os_status *status = &state->status;

   vmmemctl_deinit_sysctl();

   os_balloonobject_delete();
   os_pmap_free(pmap);

   /* log device unload */
   printf("%s unloaded\n", status->name_verbose);
}

/*
 * Module Load/Unload Operations
 */

extern int  init_module(void);
extern void cleanup_module(void);

static int vmmemctl_load(module_t mod, int cmd, void *arg)
{
   int err = 0;

   switch (cmd) {
   case MOD_LOAD:
      (void) init_module();
      break;

    case MOD_UNLOAD:
       if (OS_DISABLE_UNLOAD) {
          /* prevent moudle unload */
          err = EBUSY;
       } else {
          cleanup_module();
       }
       break;

   default:
      err = EINVAL;
      break;
   }

   return(err);
}

/* All these interfaces got added in 4.x, so we support 5.0 and above with them */
#if __FreeBSD_version >= 500000

static struct sysctl_oid *oid = NULL;


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
   char stats[PAGE_SIZE];
   size_t len;

   len = 1 + global_state.status.handler(stats, PAGE_SIZE);

   return SYSCTL_OUT(req, stats, len);
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
   oid =  sysctl_add_oid(NULL, SYSCTL_STATIC_CHILDREN(_vm), OID_AUTO,
                         global_state.status.name, CTLTYPE_STRING | CTLFLAG_RD,
                         0, 0, vmmemctl_sysctl, "A",
                         global_state.status.name_verbose);
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

#else

static void
vmmemctl_init_sysctl(void)
{
   printf("Not providing sysctl for FreeBSD below 5.0\n");
}

static void
vmmemctl_deinit_sysctl(void)
{
   printf("Not uninstalling sysctl for FreeBSD below 5.0\n");
}

#endif

/*
 * FreeBSD 3.2 does not have DEV_MODULE
 */
#ifndef DEV_MODULE
#define DEV_MODULE(name, evh, arg)                                      \
static moduledata_t name##_mod = {                                      \
    #name,                                                              \
    evh,                                                                \
    arg                                                                 \
};                                                                      \
DECLARE_MODULE(name, name##_mod, SI_SUB_DRIVERS, SI_ORDER_MIDDLE)
#endif
	    
DEV_MODULE(vmmemctl, vmmemctl_load, NULL);

